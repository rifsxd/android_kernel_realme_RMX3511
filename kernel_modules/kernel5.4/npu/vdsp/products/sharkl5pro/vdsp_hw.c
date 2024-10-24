
/*
 * Copyright (C) 2017-2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/soc/sprd/hwfeature.h>
#include <asm/cacheflush.h>
#include "xrp_kernel_defs.h"
#include "vdsp_hw.h"
#include "xrp_internal.h"
#include "sprd_dvfs_vdsp.h"
#include "vdsp_dvfs_sharkl5pro.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: hw %d: %d %s:" \
	fmt, current->pid, __LINE__, __func__

static int regmap_write_vdsp_hw(struct regmap *regmap,
				uint32_t reg, uint32_t mask, uint32_t val)
{
	int ret;

	if (!(regmap)) {
		pr_err("regmap is null\n");
		return -EINVAL;
	}
	ret = regmap_update_bits(regmap, reg, mask, val);
	if (ret) {
		pr_err
		    ("regmap_update_bits failed reg:%x, mask:%x, val:%x, ret:%d\n",
		     reg, mask, val, ret);
	}
	return ret;
}

static int regmap_read_vdsp_hw(struct regmap *regmap,
			       uint32_t reg, uint32_t mask, uint32_t * val)
{
	int ret = 0;

	if ((!(regmap)) || (!val)) {
		pr_err("regmap and val invalid regmap:%p, val:%p\n", regmap,
		       val);
		return -EINVAL;
	}
	ret = regmap_read(regmap, reg, val);
	if (!ret)
		*val &= (uint32_t) mask;
	else
		pr_err("regmap_read fail reg:%x, mask:%s ret:%d\n", reg, mask,
		       ret);
	return ret;
}

static int regmap_write_vdsp_hw_raw(struct regmap *regmap,
				    uint32_t reg, uint32_t mask, uint32_t val)
{
	uint32_t temp = 0;
	uint32_t orig = 0;
	int32_t ret;

	if (!regmap) {
		pr_err("regmap is null\n");
		return -EINVAL;
	}
	ret = regmap_read(regmap, reg, &orig);
	if (ret) {
		pr_err("regmap_read fail reg:%x ret:%d\n", reg, ret);
		return -EFAULT;
	}
	temp = orig & ~mask;
	temp |= val & mask;
	ret = regmap_write(regmap, reg, temp);
	if (ret) {
		pr_err("regmap_write fail reg:%x ret:%d, temp:%x\n", reg, ret,
		       temp);
	}
	return ret;
}

static void parse_qos(void *hw_arg, void *of_node)
{
	struct device_node *qos_node = NULL;
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;

	if ((NULL == hw_arg) || (NULL == of_node)) {
		pr_err("hw_arg:%lx , of_node:%lx\n",
		       (unsigned long)hw_arg, (unsigned long)of_node);
		return;
	}
	qos_node = of_parse_phandle(of_node, "vdsp-qos", 0);
	if (qos_node) {
		if (of_property_read_u8(qos_node,
					"arqos-vdsp-msti",
					&hw->qos.ar_qos_vdsp_msti))
			hw->qos.ar_qos_vdsp_msti = 6;

		if (of_property_read_u8(qos_node,
					"awqos-vdsp-mstd",
					&hw->qos.aw_qos_vdsp_mstd))
			hw->qos.aw_qos_vdsp_mstd = 6;

		if (of_property_read_u8(qos_node,
					"arqos-vdsp-mstd",
					&hw->qos.ar_qos_vdsp_mstd))
			hw->qos.ar_qos_vdsp_mstd = 6;

		if (of_property_read_u8(qos_node,
					"arqos-vdsp-idma",
					&hw->qos.ar_qos_vdsp_idma))
			hw->qos.ar_qos_vdsp_idma = 1;

		if (of_property_read_u8(qos_node,
					"awqos-vdsp-idma",
					&hw->qos.aw_qos_vdsp_idma))
			hw->qos.aw_qos_vdsp_idma = 1;

		if (of_property_read_u8
		    (qos_node, "arqos-vdma", &hw->qos.ar_qos_vdma))
			hw->qos.ar_qos_vdma = 1;

		if (of_property_read_u8
		    (qos_node, "awqos-vdma", &hw->qos.aw_qos_vdma))
			hw->qos.aw_qos_vdma = 1;

		if (of_property_read_u8(qos_node,
					"arqos-threshold",
					&hw->qos.ar_qos_threshold))
			hw->qos.ar_qos_threshold = 0x0f;

		if (of_property_read_u8(qos_node,
					"awqos-threshold",
					&hw->qos.aw_qos_threshold))
			hw->qos.aw_qos_threshold = 0x0f;

	} else {
		hw->qos.ar_qos_vdsp_msti = 6;
		hw->qos.ar_qos_vdsp_mstd = 6;
		hw->qos.aw_qos_vdsp_mstd = 6;
		hw->qos.ar_qos_vdsp_idma = 1;
		hw->qos.aw_qos_vdsp_idma = 1;
		hw->qos.ar_qos_vdma = 1;
		hw->qos.aw_qos_vdma = 1;
		hw->qos.ar_qos_threshold = 0x0f;
		hw->qos.aw_qos_threshold = 0x0f;
	}
	return;
}

static void set_qos(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;

	/*set qos threshold */

	regmap_write_vdsp_hw_raw(hw->ahb_regmap, REG_QOS_THRESHOLD,
				 (0xf << 28 | 0xf << 24),
				 ((hw->qos.ar_qos_threshold << 28)
				  | (hw->qos.aw_qos_threshold << 24)));
	/*set qos 3 */
	regmap_write_vdsp_hw_raw(hw->ahb_regmap, REG_QOS_3,
				 0xf0ffffff, ((hw->qos.ar_qos_vdsp_msti << 28)
					      | (hw->qos.ar_qos_vdsp_mstd << 20)
					      | (hw->qos.aw_qos_vdsp_mstd << 16)
					      | (hw->qos.ar_qos_vdsp_idma << 12)
					      | (hw->qos.aw_qos_vdsp_idma << 8)
					      | (hw->qos.ar_qos_vdma << 4)
					      | (hw->qos.aw_qos_vdma)));

	/*set qos sel 3 */
	regmap_write_vdsp_hw_raw(hw->ahb_regmap, REG_QOS_SEL3, 0x7f, 0x7f);
}

static void *get_hw_sync_data(void *hw_arg, size_t * sz, uint32_t log_addr)
{
	static const u32 irq_mode[] = {
		[XRP_IRQ_NONE] = XRP_DSP_SYNC_IRQ_MODE_NONE,
		[XRP_IRQ_LEVEL] = XRP_DSP_SYNC_IRQ_MODE_LEVEL,
		[XRP_IRQ_EDGE] = XRP_DSP_SYNC_IRQ_MODE_EDGE,
		[XRP_IRQ_EDGE_SW] = XRP_DSP_SYNC_IRQ_MODE_EDGE,
	};
	struct vdsp_hw *hw = hw_arg;
	struct vdsp_side_sync_data *hw_sync_data =
	    kmalloc(sizeof(*hw_sync_data), GFP_KERNEL);

	if (!hw_sync_data) {
		pr_err("hw_sync_data is NULL !!!\n");
		return NULL;
	}

	*hw_sync_data = (struct vdsp_side_sync_data) {
	.device_mmio_base = hw->ipi_phys,.host_irq_mode =
		    hw->host_irq_mode,.host_irq_offset =
		    hw->host_irq[0],.host_irq_bit =
		    hw->host_irq[1],.device_irq_mode =
		    irq_mode[hw->device_irq_mode],.device_irq_offset =
		    hw->device_irq[0],.device_irq_bit =
		    hw->device_irq[1],.device_irq =
		    hw->device_irq[2],.vdsp_smsg_addr =
		    (unsigned int)*sz,.vdsp_log_addr = log_addr,};
	pr_debug("device_mmio_base:%lx , (host_irq)mode:%d, offset:%d, bit:%d,"
		 "(device_irq)mode:%d, offset:%d, bit:%d, irq:%d"
		 "vdsp_smsg addr:0x%lx, vdsp_log_addr:0x%lx\n",
		 hw_sync_data->device_mmio_base, hw_sync_data->host_irq_mode,
		 hw_sync_data->host_irq_offset, hw_sync_data->host_irq_bit,
		 hw_sync_data->device_irq_mode, hw_sync_data->device_irq_offset,
		 hw_sync_data->device_irq_bit, hw_sync_data->device_irq,
		 hw_sync_data->vdsp_smsg_addr, hw_sync_data->vdsp_log_addr);

	*sz = sizeof(*hw_sync_data);

	return hw_sync_data;
}

static void reset(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;

	pr_debug("arg:%p ,offset:%x, value:0\n", hw->ahb_regmap, REG_RESET);
	regmap_write_vdsp_hw(hw->ahb_regmap, REG_RESET, (0x3 << 9), (0x3 << 9));
	regmap_write_vdsp_hw(hw->ahb_regmap, REG_RESET, (0x3 << 9), 0);
}

static void halt(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;

	pr_debug("arg:%p ,offset:%x, value:1\n", hw->ahb_regmap, REG_RUNSTALL);
	regmap_write_vdsp_hw_raw(hw->ahb_regmap, REG_RUNSTALL, (0x1 << 2),
				 (0x1 << 2));
}

static void release(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;

	pr_debug("arg:%p ,offset:%x, value:0\n", hw->ahb_regmap, REG_RUNSTALL);
	regmap_write_vdsp_hw_raw(hw->ahb_regmap, REG_RUNSTALL, (0x1 << 2), 0);
}

static void get_max_freq(uint32_t * max_freq)
{
	struct device_node *hwf;
	const char *chip_type;

	hwf = of_find_node_by_path("/hwfeature/auto");
	if (!hwf) {
		*max_freq = T610_MAX_FREQ;
		goto efuse_out;
	}
	chip_type = of_get_property(hwf, "efuse", NULL);
	pr_info("efuse was %s\n", chip_type);

	if (!strcmp(chip_type, "T618")) {
		*max_freq = T618_MAX_FREQ;
	} else {
		*max_freq = T610_MAX_FREQ;
	}
efuse_out:
	pr_debug("get max_freq:%d\n", *max_freq);
}

static int enable(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;
	uint32_t rdata = 1;
	int ret = 0;

	/*pd_ap_vdsp_force_shutdown bit */
	regmap_write_vdsp_hw(hw->pmu_regmap, REG_PD_AP_VDSP_CORE_INT_DISABLE,
			     (0x1 << 0), 0);
	regmap_write_vdsp_hw(hw->pmu_regmap, REG_PD_AP_VDSP_CFG, (0x1 << 25),
			     0);
	regmap_write_vdsp_hw(hw->pmu_regmap, REG_PD_AP_VDSP_DLSP_ENA,
			     (0x1 << 0), 0);
	/*vdsp_stop_en */
	regmap_write_vdsp_hw_raw(hw->ahb_regmap, REG_LP_CTL, 0xC, 0x8);
	/*isppll open for 936M */
	regmap_write_vdsp_hw(hw->pmu_regmap, REG_ISPPLL_REL_CFG, (0x1 << 0),
			     0x1);
	/* loop PD_AD_VDSP_STATE */
	do {
		if (regmap_read_vdsp_hw
		    (hw->pmu_regmap, 0xbc, 0xff000000, &rdata)) {
			pr_err("[error] get vdsp power states");
			ret = -ENXIO;
		}
	}
	while (rdata);

	/* IPI &vdma enable */
	regmap_write_vdsp_hw(hw->ahb_regmap, 0x0, (0x1 << 6) | (0x1 << 3),
			     (0x1 << 6) | (0x1 << 3));
	/*vdsp_all_int_mask = 0 */
	regmap_write_vdsp_hw_raw(hw->ahb_regmap, REG_VDSP_INT_CTL, (0x1 << 13),
				 0);

	return ret;
}

static void disable(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;
	uint32_t rdata = 0;
	uint32_t count = 0;

	/*vdma&IPI  disable */
	regmap_write_vdsp_hw(hw->ahb_regmap, 0x0, (0x1 << 3) | (0x1 << 6), 0);
	/*vdsp_stop_en = 1 */
	regmap_write_vdsp_hw_raw(hw->ahb_regmap, REG_LP_CTL, (0x1 << 2),
				 (0x1 << 2));
	/*mask all int */
	regmap_write_vdsp_hw_raw(hw->ahb_regmap, REG_VDSP_INT_CTL, 0x1ffff,
				 0x1ffff);
	/*pmu ap_vdsp_core_int_disable set 1 */
	regmap_write_vdsp_hw(hw->pmu_regmap, REG_PD_AP_VDSP_CORE_INT_DISABLE,
			     0x1, 0x1);
	udelay(1);
	/*wait for vdsp enter pwait mode */
	while (((rdata & (0x1 << 5)) == 0) && (count < 100)) {
		count++;
		if (regmap_read_vdsp_hw
		    (hw->ahb_regmap, REG_LP_CTL, 0x20, &rdata)) {
			pr_err("[error] get vdsp pwaitmode");
		}
		/*delay 1 ms */
		udelay(1000);
	}
	pr_debug("disable wait count:%d\n", count);
	if (count < 100) {
		/*pmu auto shutdown by vdsp core */
		regmap_write_vdsp_hw(hw->pmu_regmap, REG_PD_AP_VDSP_CFG, 0x9000000, 0x1000000);	/*bit 24 27 */
		regmap_write_vdsp_hw(hw->pmu_regmap, REG_PD_AP_VDSP_DLSP_ENA,
				     0x1, 0x1);
	} else {
		pr_err("timed out need force shut down\n");
		/*bit25 =1 , bit24 = 0 */
		regmap_write_vdsp_hw(hw->pmu_regmap, REG_PD_AP_VDSP_CFG,
				     0xb000000, 0x2000000);
	}
}

static uint32_t translate_dvfsindex_to_freq(uint32_t index)
{
	switch (index) {
	case 0:
		return SHARKL5PRO_VDSP_CLK256M;
	case 1:
		return SHARKL5PRO_VDSP_CLK384M;
	case 2:
		return SHARKL5PRO_VDSP_CLK512M;
	case 3:
		return SHARKL5PRO_VDSP_CLK614M4;
	case 4:
		return SHARKL5PRO_VDSP_CLK768M;
	case 5:
		return SHARKL5PRO_VDSP_CLK936M;
	default:
		return SHARKL5PRO_VDSP_CLK256M;
	}
	return 0;
}

static void setdvfs(void *hw_arg, uint32_t index)
{
	uint32_t freq = translate_dvfsindex_to_freq(index);

	pr_debug("freq:%d, index:%d\n", freq, index);
	vdsp_dvfs_notifier_call_chain(&freq);
}

static void send_irq(void *hw_arg)
{
	struct vdsp_hw *hw = hw_arg;

	hw->vdsp_ipi_desc->ops->irq_send(hw->device_irq[1]);
}

static irqreturn_t xrp_hw_irq_handler(int irq, void *dev_id)
{
	struct vdsp_hw *hw = dev_id;

	return xrp_irq_handler(irq, hw->xrp);
}

static irqreturn_t xrp_hw_log_irq_handler(int irq, void *dev_id)
{
	struct vdsp_hw *hw = dev_id;

	return vdsp_log_irq_handler(irq, hw->xrp);
}

#if 0
static void memcpy_hw_function(void __iomem * dst, const void *src, size_t sz)
{
	memcpy(dst, src, sz);
	return;
}

static void memset_hw_function(void __iomem * dst, int c, size_t sz)
{
	memset(dst, c, sz);
	return;
}
#endif
int vdsp_request_irq(void *xvp_arg, void *hw_arg)
{
	struct device *dev = (struct device *)xvp_arg;
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;
	int ret;

	//struct platform_device *pdev = container_of(dev, struct platform_device, dev);

	pr_debug("dev %p ,request irq %d handle %p hw %p\n",
		 dev, hw->client_irq, hw->vdsp_ipi_desc->ops->irq_handler, hw);
	ret = devm_request_irq(dev,
			       hw->client_irq,
			       hw->vdsp_ipi_desc->ops->irq_handler,
			       IRQF_SHARED, DRIVER_NAME, hw);
	if (ret < 0) {
		pr_err("devm_request_irq fail, ret %d", ret);
		return ret;
	}

	return ret;
}

void vdsp_free_irq(void *xvp_arg, void *hw_arg)
{
	struct device *dev = (struct device *)xvp_arg;
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;

	pr_debug("free irq %d dev %p hw %p\n", hw->client_irq, dev, hw);
	devm_free_irq(dev, hw->client_irq, hw);
}

static const struct xrp_hw_ops hw_ops = {
	.halt = halt,
	.release = release,
	.reset = reset,
	.get_hw_sync_data = get_hw_sync_data,
	.send_irq = send_irq,
	.memcpy_tohw = NULL,	//memcpy_hw_function,
	.memset_hw = NULL,	//memset_hw_function,
	.enable = enable,
	.disable = disable,
	.enable_dvfs = NULL,
	.disable_dvfs = NULL,
	.setdvfs = setdvfs,
	.set_qos = set_qos,
	.vdsp_request_irq = vdsp_request_irq,
	.vdsp_free_irq = vdsp_free_irq,
	.get_max_freq = get_max_freq,
};

static long sprd_vdsp_parse_hw_dt(struct platform_device *pdev,
				  struct vdsp_hw *hw,
				  int mem_idx, enum vdsp_init_flags *init_flags)
{
	struct resource *mem;
	long ret;
	struct device_node *np;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, mem_idx);
	if (unlikely(!mem)) {
		pr_err("get mem_idx:%d failed\n", mem_idx);
		return -ENODEV;
	}
	hw->ipi_phys = mem->start;
	hw->ipi = devm_ioremap_resource(&pdev->dev, mem);
	pr_debug("ipi = %pap/%p\n", &mem->start, hw->ipi);

	np = pdev->dev.of_node;
	if (np == NULL) {
		pr_err("np is null\n");
		return -EINVAL;
	}
	hw->ahb_regmap =
	    syscon_regmap_lookup_by_phandle(np, "sprd,syscon-ap-ahb");
	if (IS_ERR(hw->ahb_regmap)) {
		pr_err("can not get ahb_regmap regmap struct!\n");
		return -EINVAL;
	}
	hw->pmu_regmap = syscon_regmap_lookup_by_phandle(np, "sprd,syscon-pmu");
	if (IS_ERR(hw->ahb_regmap)) {
		pr_err("can not get pmu_regmap regmap struct!\n");
		return -EINVAL;
	}
	/* qos */
	parse_qos(hw, pdev->dev.of_node);
	/* irq */
	ret = of_property_read_u32_array(pdev->dev.of_node,
					 "device-irq",
					 hw->device_irq,
					 ARRAY_SIZE(hw->device_irq));
	if (ret == 0) {
		u32 device_irq_host_offset;

		ret = of_property_read_u32(pdev->dev.of_node,
					   "device-irq-host-offset",
					   &device_irq_host_offset);
		if (ret == 0) {
			hw->device_irq_host_offset = device_irq_host_offset;
		} else {
			hw->device_irq_host_offset = hw->device_irq[0];
			ret = 0;
		}
	}
	if (ret == 0) {
		u32 device_irq_mode;

		ret = of_property_read_u32(pdev->dev.of_node,
					   "device-irq-mode", &device_irq_mode);
		if (likely(device_irq_mode < XRP_IRQ_MAX))
			hw->device_irq_mode = device_irq_mode;
		else
			ret = -ENOENT;
	}
	if (ret == 0) {
		pr_debug("device IRQ MMIO host offset = 0x%08x,"
			 "offset = 0x%08x, bit = %d,"
			 "device IRQ = %d, IRQ mode = %d",
			 hw->device_irq_host_offset,
			 hw->device_irq[0], hw->device_irq[1],
			 hw->device_irq[2], hw->device_irq_mode);
	} else {
		pr_debug("using polling mode on the device side\n");
	}

	ret = of_property_read_u32_array(pdev->dev.of_node,
					 "host-irq",
					 hw->host_irq,
					 ARRAY_SIZE(hw->host_irq));
	if (ret == 0) {
		u32 host_irq_mode;

		ret = of_property_read_u32(pdev->dev.of_node,
					   "host-irq-mode", &host_irq_mode);
		if (likely(host_irq_mode < XRP_IRQ_MAX))
			hw->host_irq_mode = host_irq_mode;
		else
			ret = -ENOENT;
	}

	if (ret == 0 && hw->host_irq_mode != XRP_IRQ_NONE)
		hw->client_irq = platform_get_irq(pdev, 0);
	else
		hw->client_irq = -1;

	pr_debug("irq is:%d , ret:%ld , host_irq_mode:%d\n",
		 hw->client_irq, ret, hw->host_irq_mode);
	if (hw->client_irq >= 0) {
		pr_debug("host IRQ = %d,", hw->client_irq);
		hw->vdsp_ipi_desc = get_vdsp_ipi_ctx_desc();
		if (hw->vdsp_ipi_desc) {
			hw->vdsp_ipi_desc->base_addr = hw->ahb_regmap;
			hw->vdsp_ipi_desc->ipi_addr = hw->ipi;
			hw->vdsp_ipi_desc->irq_mode = hw->host_irq_mode;

			ret = vdsp_request_irq(&pdev->dev, hw);
			if (ret < 0) {
				pr_err("request_irq %d failed\n",
				       hw->client_irq);
				goto err;
			}

			hw->vdsp_ipi_desc->ops->irq_register(0,
							     xrp_hw_irq_handler,
							     hw);
			hw->vdsp_ipi_desc->ops->irq_register(1,
							     xrp_hw_irq_handler,
							     hw);
			hw->vdsp_ipi_desc->ops->irq_register(2,
							     xrp_hw_log_irq_handler,
							     hw);

			*init_flags |= XRP_INIT_USE_HOST_IRQ;
		}
	} else {
		pr_debug("using polling mode on the host side\n");
	}
	ret = 0;
err:
	return ret;
}

static long init_sprd(struct platform_device *pdev, struct vdsp_hw *hw)
{
	long ret;
	enum vdsp_init_flags init_flags = 0;

	ret = sprd_vdsp_parse_hw_dt(pdev, hw, 0, &init_flags);
	if (unlikely(ret < 0))
		return ret;
	return sprd_vdsp_init(pdev, init_flags, &hw_ops, hw);
}

// #ifdef CONFIG_OF
static const struct of_device_id vdsp_device_match[] = {
	{
	 .compatible = "sprd,sharkl5pro-vdsp",
	 .data = init_sprd,
	 },
	{},
};

MODULE_DEVICE_TABLE(of, vdsp_device_match);
// #endif

static int vdsp_driver_probe(struct platform_device *pdev)
{
	struct vdsp_hw *hw = devm_kzalloc(&pdev->dev, sizeof(*hw), GFP_KERNEL);
	const struct of_device_id *match;
	long (*init) (struct platform_device * pdev, struct vdsp_hw * hw);
	long ret;

	printk(" sprd-vdsp: vdsp_driver probe \n");

	if (!hw)
		return -ENOMEM;

	match = of_match_device(of_match_ptr(vdsp_device_match), &pdev->dev);
	if (!match)
		return -ENODEV;

	init = match->data;
	ret = init(pdev, hw);
	if (IS_ERR_VALUE(ret)) {
		return ret;
	} else {
		hw->xrp = ERR_PTR(ret);
		return 0;
	}

}

static int vdsp_driver_remove(struct platform_device *pdev)
{
	return sprd_vdsp_deinit(pdev);
}

static const struct dev_pm_ops vdsp_pm_ops = {
	SET_RUNTIME_PM_OPS(vdsp_runtime_suspend,
			   vdsp_runtime_resume, NULL)
};

static struct platform_driver vdsp_driver = {
	.probe = vdsp_driver_probe,
	.remove = vdsp_driver_remove,
	.driver = {
		   .name = DRIVER_NAME,
		   .of_match_table = of_match_ptr(vdsp_device_match),
		   .pm = &vdsp_pm_ops,
		   },
};

module_platform_driver(vdsp_driver);

MODULE_DESCRIPTION("Sprd VDSP Driver");
MODULE_AUTHOR("Vdsp@unisoc");
MODULE_LICENSE("GPL");
