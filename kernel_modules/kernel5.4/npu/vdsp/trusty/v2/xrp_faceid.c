
/*
 * xrp_firmware: firmware manipulation for the XRP
 *
 * Copyright (c) 2015 - 2017 Cadence Design Systems, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Alternatively you can use and distribute this file under the terms of
 * the GNU General Public License version 2 or later.
 */

#include <linux/dma-mapping.h>
#include <linux/elf.h>
#include <linux/firmware.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/sprd_ion.h>
#include <linux/slab.h>
#include "vdsp_hw.h"
#include "xrp_internal.h"
#include "xrp_kernel_dsp_interface.h"
#include "xrp_faceid.h"
#include "vdsp_trusty.h"

#define SIGN_HEAD_SIZE (512)
#define SIGN_TAIL_SIZE (512)

//#define PAGE_ALIGN(addr) (((addr)+PAGE_SIZE-1) & (~(PAGE_SIZE-1)))

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: faceid %d %d %s : "\
        fmt, current->pid, __LINE__, __func__

int sprd_cam_pw_on(void);
int sprd_cam_pw_off(void);
int sprd_cam_domain_eb(void);
int sprd_cam_domain_disable(void);

static int sprd_alloc_faceid_weights_buffer(struct xvp *xvp,
					    struct ion_buf *ion_buf,
					    size_t size)
{
	int align_size = PAGE_ALIGN(size);

	ion_buf->addr_k =
	    xvp->ion_faceid_fw.addr_k + xvp->ion_faceid_addr_offset;

	ion_buf->addr_p =
	    xvp->ion_faceid_fw.addr_p + xvp->ion_faceid_addr_offset;

	xvp->ion_faceid_addr_offset += align_size;

	ion_buf->dev = xvp->dev;
	pr_debug("faceid alloc addr_p %lx vaddr:%lx size %ld\n",
		 ion_buf->addr_p, ion_buf->addr_k, align_size);
	return 0;

}

static int sprd_free_faceid_weights_buffer(struct xvp *xvp,
					   struct ion_buf *ion_buf)
{
	ion_buf->addr_k = 0;
	ion_buf->addr_p = 0;
	return 0;
}

int sprd_faceid_request_algo_mem(struct xvp *xvp)
{
	int ret = sprd_alloc_faceid_weights_buffer(xvp,
						   &xvp->faceid_pool.
						   ion_fd_mem_pool,
						   FACEID_FD_MEM_SIZE);

	if (unlikely(ret < 0)) {
		pr_err("request fd mem fail\n");
		return ret;
	}

	return 0;
}

int sprd_faceid_release_algo_mem(struct xvp *xvp)
{
	return sprd_free_faceid_weights_buffer(xvp,
					       &xvp->faceid_pool.
					       ion_fd_mem_pool);
}

int sprd_request_weights(struct xvp *xvp, char *name, struct ion_buf *coeff_ion)
{
	unsigned long dst = 0;
	int ret = request_firmware(&xvp->faceid_fw,
				   name, xvp->dev);

	if (unlikely(ret < 0)) {
		pr_info("request %s weights fail\n", name);
		return ret;
	}

	ret = sprd_alloc_faceid_weights_buffer(xvp,
					       coeff_ion, xvp->faceid_fw->size);

	if (unlikely(ret < 0)) {
		pr_err("alloc %s weights fail\n", name);
		return ret;
	}

	dst = coeff_ion->addr_k;
	memcpy((void *)dst, xvp->faceid_fw->data, xvp->faceid_fw->size);

	release_firmware(xvp->faceid_fw);

	return ret;
}

int sprd_faceid_request_weights(struct xvp *xvp)
{
	int i = 0, ret;

	char *coeff_name[FACEID_COEFF_NUM] = { "network_coeff_fa.bin",
		"network_coeff_fp.bin",
		"network_coeff_foc.bin"
	};

	struct ion_buf *coeff_ion[FACEID_COEFF_NUM] = {
		&xvp->faceid_pool.ion_fa_weights,
		&xvp->faceid_pool.ion_fp_weights,
		&xvp->faceid_pool.ion_foc_weights,
	};

	for (; i < FACEID_COEFF_NUM; i++) {
		ret = sprd_request_weights(xvp, coeff_name[i], coeff_ion[i]);
		if (ret < 0)
			return ret;
	}
	sprd_faceid_request_algo_mem(xvp);

	return 0;
}

void sprd_faceid_release_weights(struct xvp *xvp)
{
	int i = 0;

	struct ion_buf *coeff_ion[FACEID_COEFF_NUM] = {
		&xvp->faceid_pool.ion_fa_weights,
		&xvp->faceid_pool.ion_fp_weights,
		&xvp->faceid_pool.ion_foc_weights,
	};

	for (; i < FACEID_COEFF_NUM; i++) {
		sprd_free_faceid_weights_buffer(xvp, coeff_ion[i]);
	}
	sprd_faceid_release_algo_mem(xvp);
}

int sprd_alloc_faceid_combuffer(struct xvp *xvp)
{
	xvp->ion_faceid_comm.addr_k =
	    xvp->ion_faceid_fw.addr_k + xvp->ion_faceid_addr_offset;
	xvp->ion_faceid_comm.addr_p =
	    xvp->ion_faceid_fw.addr_p + xvp->ion_faceid_addr_offset;

	xvp->ion_faceid_comm.dev = xvp->dev;

	pr_debug("faceid com phyaddr %llX, vaddr %llx\n",
		 xvp->ion_faceid_comm.addr_p, xvp->ion_faceid_comm.addr_k);
	xvp->ion_faceid_addr_offset += PAGE_SIZE;
	return 0;
}

int sprd_free_faceid_combuffer(struct xvp *xvp)
{

	xvp->ion_faceid_comm.addr_k = 0;
	xvp->ion_faceid_comm.addr_p = 0;

	return 0;
}

static int sprd_alloc_faceid_fwbuffer(struct xvp *xvp)
{
	int ret;
	struct vdsp_hw *hw = (struct vdsp_hw *)xvp->hw_arg;

	xvp->ion_faceid_addr_offset = 0;

	ret = request_firmware(&xvp->firmware2_sign, FACEID_FIRMWARE, xvp->dev);

	if (unlikely(ret < 0)) {
		pr_err("request firmware failed ret:%d\n", ret);
		return ret;
	}

	pr_info("request signed fw size 0x%X\n", xvp->firmware2_sign->size);

	ret = xvp->vdsp_mem_desc->ops->mem_alloc(xvp->vdsp_mem_desc,
						 &xvp->ion_faceid_fw_sign,
						 ION_HEAP_ID_MASK_VDSP,
						 VDSP_FACEID_FIRMWIRE_SIZE / 2);
	if (unlikely(0 != ret)) {
		pr_err("alloc sign fw buffer failed,ret %d\n", ret);
		goto mem_alloc_fw_sign;
	}

	ret = xvp->vdsp_mem_desc->ops->mem_kmap(xvp->vdsp_mem_desc,
						&xvp->ion_faceid_fw_sign);
	if (unlikely(0 != ret)) {
		pr_err("kmap fw sign buffer failed,ret %d\n", ret);
		goto mem_kmap_fw_sign;
	}
	xvp->ion_faceid_fw_sign.dev = xvp->dev;
	pr_debug("ion_faceid_fw_sign phyaddr %llX\n",
		 xvp->ion_faceid_fw_sign.addr_p);

	xvp->ion_faceid_fw.addr_p = 0;
	ret = xvp->vdsp_mem_desc->ops->mem_alloc(xvp->vdsp_mem_desc,
						 &xvp->ion_faceid_fw,
						 ION_HEAP_ID_MASK_VDSP,
						 hw->vdsp_reserved_mem_size -
						 (VDSP_FACEID_FIRMWIRE_SIZE /
						  2));
	if (unlikely(0 != ret)) {
		pr_err("alloc fw buffer failed,ret %d\n", ret);
		goto mem_alloc_fw;
	}

	ret = xvp->vdsp_mem_desc->ops->mem_kmap(xvp->vdsp_mem_desc,
						&xvp->ion_faceid_fw);
	if (unlikely(0 != ret)) {
		pr_err("kmap fw buffer failed,ret %d\n", ret);
		goto mem_kmap_fw;
	}

	xvp->ion_faceid_fw.dev = xvp->dev;
	xvp->ion_faceid_addr_offset += VDSP_FACEID_FIRMWIRE_SIZE;
	pr_debug("ion_faceid_fw phyaddr %llX\n", xvp->ion_faceid_fw.addr_p);

	return 0;

mem_kmap_fw:
	xvp->vdsp_mem_desc->ops->mem_free(xvp->vdsp_mem_desc,
					  &xvp->ion_faceid_fw);
mem_alloc_fw:
	xvp->vdsp_mem_desc->ops->mem_kunmap(xvp->vdsp_mem_desc,
					    &xvp->ion_faceid_fw_sign);
mem_kmap_fw_sign:
	xvp->vdsp_mem_desc->ops->mem_free(xvp->vdsp_mem_desc,
					  &xvp->ion_faceid_fw_sign);
mem_alloc_fw_sign:
	release_firmware(xvp->firmware2_sign);

	return ret;
}

static int sprd_free_faceid_fwbuffer(struct xvp *xvp)
{
	release_firmware(xvp->firmware2_sign);
	xvp->vdsp_mem_desc->ops->mem_kunmap(xvp->vdsp_mem_desc,
					    &xvp->ion_faceid_fw_sign);
	xvp->vdsp_mem_desc->ops->mem_free(xvp->vdsp_mem_desc,
					  &xvp->ion_faceid_fw_sign);

	xvp->vdsp_mem_desc->ops->mem_kunmap(xvp->vdsp_mem_desc,
					    &xvp->ion_faceid_fw);
	xvp->vdsp_mem_desc->ops->mem_free(xvp->vdsp_mem_desc,
					  &xvp->ion_faceid_fw);

	xvp->ion_faceid_fw.addr_p = 0;
	return 0;
}

int sprd_faceid_iommu_map_buffer(struct xvp *xvp)
{
	int ret = -EFAULT;

	if (unlikely(xvp->ion_faceid_fw.addr_p == 0)) {
		pr_err("map faceid fw addr is NULL \n");
		return ret;
	}
	//fw
	ret = xvp->vdsp_mem_desc->ops->mem_iommu_map(xvp->vdsp_mem_desc,
						     &xvp->ion_faceid_fw,
						     IOMMU_ALL);
	if (unlikely(ret)) {
		pr_err("map faceid fw buffer failed\n");
		return ret;
	}
	xvp->dsp_firmware_addr = xvp->ion_faceid_fw.iova;

	pr_debug("fw :%lx --> %lx\n",
		 xvp->ion_faceid_fw.addr_p,
		 (unsigned long)xvp->dsp_firmware_addr);

	//comm
	xvp->ion_faceid_comm.iova = xvp->ion_faceid_fw.iova +
	    (xvp->ion_faceid_comm.addr_p - xvp->ion_faceid_fw.addr_p);

	pr_debug("comm :%lx --> %lx\n",
		 xvp->ion_faceid_comm.addr_p, xvp->ion_faceid_comm.iova);

	//coeff fa
	xvp->faceid_pool.ion_fa_weights.iova = xvp->ion_faceid_fw.iova +
	    (xvp->faceid_pool.ion_fa_weights.addr_p -
	     xvp->ion_faceid_fw.addr_p);

	pr_debug("fa :%lx --> %lx\n",
		 xvp->faceid_pool.ion_fa_weights.addr_p,
		 xvp->faceid_pool.ion_fa_weights.iova);

	//coeff fp
	xvp->faceid_pool.ion_fp_weights.iova = xvp->ion_faceid_fw.iova +
	    (xvp->faceid_pool.ion_fp_weights.addr_p -
	     xvp->ion_faceid_fw.addr_p);

	pr_debug("fp :%lx --> %lx\n",
		 xvp->faceid_pool.ion_fp_weights.addr_p,
		 xvp->faceid_pool.ion_fp_weights.iova);

	//coeff foc
	xvp->faceid_pool.ion_foc_weights.iova = xvp->ion_faceid_fw.iova +
	    (xvp->faceid_pool.ion_foc_weights.addr_p -
	     xvp->ion_faceid_fw.addr_p);

	pr_debug("foc :%lx --> %lx\n",
		 xvp->faceid_pool.ion_foc_weights.addr_p,
		 xvp->faceid_pool.ion_foc_weights.iova);

	//algo mem pool
	xvp->faceid_pool.ion_fd_mem_pool.iova = xvp->ion_faceid_fw.iova +
	    (xvp->faceid_pool.ion_fd_mem_pool.addr_p -
	     xvp->ion_faceid_fw.addr_p);

	pr_debug("mem pool :%lx --> %lx\n",
		 xvp->faceid_pool.ion_fd_mem_pool.addr_p,
		 xvp->faceid_pool.ion_fd_mem_pool.iova);

	return 0;
}

int sprd_faceid_iommu_unmap_buffer(struct xvp *xvp)
{
	int ret = 0;

	if (unlikely(xvp->ion_faceid_fw.addr_p == 0)) {
		pr_err("unmap faceid fw addr is NULL\n");
		return -EFAULT;
	}

	ret = xvp->vdsp_mem_desc->ops->mem_iommu_unmap(xvp->vdsp_mem_desc,
						       &xvp->ion_faceid_fw,
						       IOMMU_ALL);
	if (unlikely(ret)) {
		pr_err("unmap faceid fw fialed, ret %d\n", ret);
		return -EFAULT;
	}

	pr_debug("unmap faceid fw :%p \n", xvp->ion_faceid_fw.addr_p);

	xvp->ion_faceid_comm.iova = 0;
	xvp->faceid_pool.ion_fa_weights.iova = 0;
	xvp->faceid_pool.ion_fp_weights.iova = 0;
	xvp->faceid_pool.ion_foc_weights.iova = 0;
	xvp->faceid_pool.ion_fd_mem_pool.iova = 0;
	return 0;
}

int sprd_faceid_iommu_map_image(struct xvp *xvp, int fd)
{
	int ret;

	pr_info("image fd %d\n", fd);

	ret = sprd_ion_get_buffer(fd,
				  NULL,
				  &xvp->ion_faceid_image.buf,
				  &xvp->ion_faceid_image.size);
	if (ret) {
		pr_err("fail to get %d ionbuf \n", fd);
		return -EINVAL;
	}

	pr_info(" %lx %d\n", xvp->ion_faceid_image.addr_p,
		xvp->ion_faceid_image.size);
	xvp->ion_faceid_image.dev = xvp->dev;
	ret = xvp->vdsp_mem_desc->ops->mem_iommu_map(xvp->vdsp_mem_desc,
						     &xvp->ion_faceid_image,
						     IOMMU_ALL);
	if (unlikely(ret)) {
		pr_err("map faceid image buffer failed\n");
		return ret;
	}
	pr_debug("iomap:%llx --> %lx\n",
		 xvp->ion_faceid_image.addr_p, xvp->ion_faceid_image.iova);

	return 0;
}

int sprd_faceid_iommu_unmap_image(struct xvp *xvp)
{
	int ret = 0;

	if (unlikely(xvp->ion_faceid_image.iova == 0)) {
		pr_err("unmap faceid image addr is NULL\n");
		return -EFAULT;
	}

	ret = xvp->vdsp_mem_desc->ops->mem_iommu_unmap(xvp->vdsp_mem_desc,
						       &xvp->ion_faceid_image,
						       IOMMU_ALL);
	if (unlikely(ret)) {
		pr_err("unmap faceid image fialed, ret %d\n", ret);
		return -EFAULT;
	}

	pr_debug("unmap faceid image :%lx \n", xvp->ion_faceid_image.iova);
	return 0;
}

int sprd_faceid_put_ion_addr(struct ion_buf *ion_buf)
{
	if (IS_ERR_OR_NULL(ion_buf->dmabuf_p)) {
		pr_err("dma buff is null\n");
		return -EINVAL;
	}
	sprd_ion_unmap_kernel(ion_buf->dmabuf_p, 0);
	ion_buf->addr_k = 0;

	dma_buf_put(ion_buf->dmabuf_p);
	return 0;
}

int sprd_faceid_get_ion_addr(struct ion_buf *ion_buf)
{
	int ret;

	ret = sprd_ion_get_phys_addr(ion_buf->mfd,
				     ion_buf->dmabuf_p, &ion_buf->addr_p,
				     &ion_buf->size);
	if (ret) {
		pr_err("fail to get %d phy_addr, ret %d\n", ion_buf->mfd, ret);
		return -EINVAL;
		//goto get_pyhs_addr_fail;
	}

	pr_debug("Get ion %d phy %llX vaddr 0x%llx\n",
		 ion_buf->mfd, ion_buf->addr_p, ion_buf->addr_k);

	return 0;
}

int sprd_faceid_sec_sign(struct xvp *xvp)
{
	bool ret;
	KBC_LOAD_TABLE_V table;
	unsigned long mem_addr_p;
	size_t img_len;

	ret = trusty_kernelbootcp_connect();
	if (!ret) {
		pr_err("bootcp connect fail\n");
		return -EACCES;
	}

	memset(&table, 0, sizeof(KBC_LOAD_TABLE_V));

	mem_addr_p = xvp->ion_faceid_fw_sign.addr_p;
	img_len = xvp->firmware2_sign->size;

	table.faceid_fw.img_addr = mem_addr_p;
	table.faceid_fw.img_len = img_len;

	pr_debug("fw sign paddr %lX size %zd\n", mem_addr_p, img_len);

	ret = kernel_bootcp_verify_vdsp(&table);
	if (!ret) {
		pr_err("bootcp verify fail\n");
		trusty_kernelbootcp_disconnect();
		return -EACCES;
	}

	trusty_kernelbootcp_disconnect();

	return 0;
}

int sprd_faceid_secboot_entry(struct xvp *xvp)
{
	bool ret;

	/*copy fw to continuous physical address */
	memcpy((void *)xvp->ion_faceid_fw_sign.addr_k,
	       (void *)xvp->firmware2_sign->data, xvp->firmware2_sign->size);

	if (xvp->tee_con) {
		struct vdsp_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ7;
		msg.msg_cmd = TA_FACEID_ENTER_SEC_MODE;
		ret = vdsp_set_sec_mode(&msg);
		if (!ret) {
			pr_err("Entry secure mode fail\n");
			return -EACCES;
		}
	} else {
		pr_err("vdsp tee connect fail\n");
		return -EACCES;
	}
	return 0;
}

int sprd_faceid_secboot_exit(struct xvp *xvp)
{
	bool ret;

	if (xvp->tee_con) {
		struct vdsp_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ7;
		msg.msg_cmd = TA_FACEID_EXIT_SEC_MODE;
		ret = vdsp_set_sec_mode(&msg);
		if (!ret) {
			pr_err("Exit secure mode fail\n");
			return -EACCES;
		}
	} else {
		pr_err("vdsp tee connect fail\n");
		return -EACCES;
	}
	return 0;
}

int sprd_faceid_load_firmware(struct xvp *xvp)
{
	bool ret;

	if (xvp->tee_con) {
		struct vdsp_load_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ7;
		msg.msg_cmd = TA_FACEID_LOAD_FW;

		msg.firmware_size = PAGE_ALIGN(xvp->firmware2_sign->size);	/*aligned PAGE_SIZE */

		ret = vdsp_load_fw(&msg);
		if (!ret) {
			pr_err("load fw fail\n");
			return -EACCES;
		}
	} else {
		pr_err("vdsp tee connect fail\n");
		return -EACCES;
	}
	return 0;
}

int sprd_faceid_sync_vdsp(struct xvp *xvp)
{
	bool ret;
	struct vdsp_hw *hw = (struct vdsp_hw *)xvp->hw_arg;

	if (xvp->tee_con) {
		struct vdsp_sync_msg msg;

		if (xvp->ion_faceid_fw.iova == 0) {
			pr_err("fw io addr is zero\n");
			return -EACCES;
		}
		msg.vdsp_type = TA_CADENCE_VQ7;
		msg.msg_cmd = TA_FACEID_SYNC_VDSP;

		msg.vdsp_log_addr = xvp->ion_faceid_fw.iova +
		    hw->vdsp_reserved_mem_size - VDSP_FACEID_LOG_ADDR_OFFSET;
		pr_info("vdsp log addr %lx\n", msg.vdsp_log_addr);
		ret = vdsp_sync_sec(&msg);
		if (!ret) {
			pr_err("sync vdsp fail\n");
			return -EACCES;
		}
	} else {
		pr_err("vdsp tee connect fail\n");
		return -EACCES;
	}

	return 0;
}

int sprd_faceid_halt_vdsp(struct xvp *xvp)
{
	bool ret;

	if (xvp->tee_con) {
		struct vdsp_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ7;
		msg.msg_cmd = TA_FACEID_HALT_VDSP;
		ret = vdsp_set_sec_mode(&msg);
		if (!ret) {
			pr_err("halt vdsp fail\n");
			return -EACCES;
		}
	} else {
		pr_err("vdsp tee connect fail\n");
		return -EACCES;
	}
	return 0;
}

int sprd_faceid_reset_vdsp(struct xvp *xvp)
{
	bool ret;

	if (xvp->tee_con) {
		struct vdsp_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ7;
		msg.msg_cmd = TA_FACEID_RESET_VDSP;
		ret = vdsp_set_sec_mode(&msg);
		if (!ret) {
			pr_err("reset vdsp fail\n");
			return -EACCES;
		}
	} else {
		pr_err("vdsp tee connect fail\n");
		return -EACCES;
	}
	return 0;
}

int sprd_faceid_release_vdsp(struct xvp *xvp)
{
	bool ret;

	if (xvp->tee_con) {
		struct vdsp_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ7;
		msg.msg_cmd = TA_FACEID_RELEASE_VDSP;
		ret = vdsp_set_sec_mode(&msg);
		if (!ret) {
			pr_err("release vdsp fail\n");
			return -EACCES;
		}
	} else {
		pr_err("vdsp tee connect fail\n");
		return -EACCES;
	}
	return 0;
}

int sprd_faceid_enable_vdsp(struct xvp *xvp)
{
	bool ret;
	int ret2 = 0;

	ret2 = sprd_cam_pw_on();
	if (ret2) {
		pr_err("[error]cam pw on ret:%d", ret2);
		return -EACCES;
	}
	ret2 = sprd_cam_domain_eb();
	if (ret2) {
		pr_err("[error]cam doamin eb ret:%d", ret2);
		ret2 = -EACCES;
		goto err_cam_eb;
	}

	if (xvp->tee_con) {
		struct vdsp_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ7;
		msg.msg_cmd = TA_FACEID_ENABLE_VDSP;
		ret = vdsp_set_sec_mode(&msg);
		if (!ret) {
			pr_err("enable vdsp fail\n");
			ret2 = -EACCES;
			goto err_dsp_pw_on;
		}
	} else {
		pr_err("vdsp tee connect fail\n");
		ret2 = -EACCES;
		goto err_dsp_pw_on;
	}
	return 0;

err_dsp_pw_on:
	sprd_cam_domain_disable();
err_cam_eb:
	sprd_cam_pw_off();
	return ret2;

}

int sprd_faceid_disable_vdsp(struct xvp *xvp)
{
	bool ret;
	int ret2;

	if (xvp->tee_con) {
		struct vdsp_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ7;
		msg.msg_cmd = TA_FACEID_DISABLE_VDSP;
		ret = vdsp_set_sec_mode(&msg);
		if (!ret) {
			pr_err("disable vdsp fail\n");
			return -EACCES;
		}
	} else {
		pr_err("vdsp tee connect fail\n");
		return -EACCES;
	}
	ret2 = sprd_cam_domain_disable();
	if (ret2)
		pr_err("[error]cam dm disable ret:%d\n", ret2);

	ret2 = sprd_cam_pw_off();	//set reference num decrease
	if (ret2)
		pr_err("[error]cam pw off ret:%d\n", ret2);

	return 0;
}

int sprd_faceid_free_irq(struct xvp *xvp)
{
	if (xvp->irq_status == IRQ_STATUS_REQUESTED) {
		if (xvp->hw_ops->vdsp_free_irq) {
			xvp->hw_ops->vdsp_free_irq(xvp->dev, xvp->hw_arg);
			xvp->irq_status = IRQ_STATUS_FREED;
		} else {
			pr_err("vdsp_free_irq ops is null \n");
			return -EACCES;
		}
	} else {
		pr_err("irq has been already freed \n");
		return -EACCES;
	}
	return 0;
}

int sprd_faceid_request_irq(struct xvp *xvp)
{
	int ret;

	if (xvp->irq_status == IRQ_STATUS_FREED) {
		if (xvp->hw_ops->vdsp_request_irq) {
			ret =
			    xvp->hw_ops->vdsp_request_irq(xvp->dev,
							  xvp->hw_arg);
			if (ret < 0) {
				pr_err("xvp_request_irq failed %d\n", ret);
				return ret;
			}
			xvp->irq_status = IRQ_STATUS_REQUESTED;
		} else {
			pr_err("vdsp_request_irq ops is null \n");
			return -EACCES;
		}
	} else {
		pr_err("irq has been already requested \n");
	}
	return 0;
}

int sprd_faceid_run_vdsp(struct xvp *xvp, uint32_t in_fd, uint32_t out_fd)
{
	bool ret;
	struct ion_buf tmp;
	struct vdsp_run_msg msg;

	if (!xvp->tee_con) {
		pr_err("vdsp tee connect fail\n");
		return -EACCES;
	}

	tmp.mfd = in_fd;
	tmp.dev = xvp->dev;
	tmp.dmabuf_p = NULL;

	ret = sprd_faceid_get_ion_addr(&tmp);
	if (0 != ret)
		return ret;

	ret = sprd_faceid_iommu_map_image(xvp, out_fd);
	if (ret != 0) {
		goto map_iommu_image_fail;
	}

	msg.vdsp_type = TA_CADENCE_VQ7;
	msg.msg_cmd = TA_FACEID_RUN_VDSP;

	msg.fa_coffe_addr = xvp->faceid_pool.ion_fa_weights.iova;
	msg.fp_coffe_addr = xvp->faceid_pool.ion_fp_weights.iova;
	msg.foc_coffe_addr = xvp->faceid_pool.ion_foc_weights.iova;
	msg.mem_pool_addr = xvp->faceid_pool.ion_fd_mem_pool.iova;

	msg.in_addr = tmp.addr_p;
	msg.out_addr = xvp->ion_faceid_image.iova;

	pr_debug("fa %X, fp %X, foc %X, mem pool %X, in %llX, out %llx\n",
		 msg.fa_coffe_addr, msg.fp_coffe_addr,
		 msg.foc_coffe_addr, msg.mem_pool_addr, msg.in_addr,
		 msg.out_addr);

	ret = vdsp_run_vdsp(&msg);
	if (!ret) {
		pr_err("run vdsp fail\n");
	}

	sprd_faceid_iommu_unmap_image(xvp);

map_iommu_image_fail:
	sprd_faceid_put_ion_addr(&tmp);
	return 0;
}

int sprd_faceid_boot_firmware(struct xvp *xvp)
{
	int ret;
	s64 tv0, tv1, tv2;

	tv0 = ktime_to_us(ktime_get());
	ret = sprd_faceid_secboot_entry(xvp);
	if (ret < 0)
		return ret;

	sprd_faceid_halt_vdsp(xvp);
	sprd_faceid_reset_vdsp(xvp);

	ret = sprd_faceid_sec_sign(xvp);
	if (ret < 0) {
		return ret;
	}
	ret = sprd_faceid_load_firmware(xvp);
	if (ret < 0) {
		return ret;
	}

	sprd_faceid_release_vdsp(xvp);
	tv1 = ktime_to_us(ktime_get());

	ret = sprd_faceid_sync_vdsp(xvp);
	if (ret < 0) {
		sprd_faceid_halt_vdsp(xvp);
		pr_err("couldn't synchronize with the DSP core\n");
		xvp->off = true;
		return ret;
	}

	tv2 = ktime_to_us(ktime_get());
	/*request firmware - sync */
	pr_info("[TIME]request firmware:%lld(us), sync:%lld(us)\n",
		tv1 - tv0, tv2 - tv1);
	return 0;
}

int sprd_faceid_secboot_init(struct xvp *xvp)
{
	bool ret;

	if (sprd_faceid_free_irq(xvp) != 0)
		return -EACCES;

	xvp->tee_con = vdsp_ca_connect();
	if (!xvp->tee_con) {
		pr_err("vdsp_ca_connect fail\n");
		sprd_faceid_request_irq(xvp);
		return -EACCES;
	} else {
		struct vdsp_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ7;
		msg.msg_cmd = TA_FACEID_INIT;
		ret = vdsp_set_sec_mode(&msg);
		if (!ret) {
			pr_err("faceid init fail\n");
			vdsp_ca_disconnect();
			xvp->tee_con = false;
			sprd_faceid_request_irq(xvp);
			return -EACCES;
		}
	}
	xvp->secmode = true;
	return 0;
}

int sprd_faceid_secboot_deinit(struct xvp *xvp)
{
	bool ret;

	if (xvp->secmode) {
		if (xvp->tee_con) {
			struct vdsp_msg msg;

			msg.vdsp_type = TA_CADENCE_VQ7;
			msg.msg_cmd = TA_FACEID_EXIT_SEC_MODE;
			ret = vdsp_set_sec_mode(&msg);
			if (!ret)
				pr_err("sprd_faceid_sec_exit fail\n");

			vdsp_ca_disconnect();
			xvp->tee_con = false;
		}
		xvp->secmode = false;
	}

	return sprd_faceid_request_irq(xvp);
}

int sprd_faceid_init(struct xvp *xvp)
{
	int ret = 0;
	struct vdsp_hw *hw = (struct vdsp_hw *)xvp->hw_arg;

	if ((hw->vdsp_reserved_mem_addr == 0)
	    || (hw->vdsp_reserved_mem_size == 0))
		return -ENOMEM;

	ret = sprd_alloc_faceid_fwbuffer(xvp);
	if (ret < 0) {
		return ret;
	}

	sprd_alloc_faceid_combuffer(xvp);

	sprd_faceid_request_weights(xvp);
	return 0;
}

int sprd_faceid_deinit(struct xvp *xvp)
{
	sprd_free_faceid_fwbuffer(xvp);
	sprd_free_faceid_combuffer(xvp);
	sprd_faceid_release_weights(xvp);
	return 0;
}
