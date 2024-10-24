
/*****************************************************************************
 *
 * Copyright (c) Imagination Technologies Ltd.
 *
 * The contents of this file are subject to the MIT license as set out below.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the terms of the
 * GNU General Public License Version 2 ("GPL")in which case the provisions of
 * GPL are applicable instead of those above.
 *
 * If you wish to allow use of your version of this file only under the terms
 * of GPL, and not to allow others to use your version of this file under the
 * terms of the MIT license, indicate your decision by deleting the provisions
 * above and replace them with the notice and other provisions required by GPL
 * as set out in the file called "GPLHEADER" included in this distribution. If
 * you do not delete the provisions above, a recipient may use your version of
 * this file under the terms of either the MIT license or GPL.
 *
 * This License is also included in this distribution in the file called
 * "MIT_COPYING".
 *
 *****************************************************************************/
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/dma-buf.h>
#include <linux/gfp.h>
#include <linux/vmalloc.h>
#include <linux/genalloc.h>
#include <linux/version.h>

#include <asm/cacheflush.h>

#include "sprd_vdsp_mem_core.h"
#include "sprd_vdsp_mem_core_priv.h"
#include <asm-generic/bug.h>

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: [mem_heap]: %d %s: "\
        fmt, current->pid, __func__

/* Default allocation order */
#define POOL_ALLOC_ORDER_BASE PAGE_SHIFT

struct heap_data {
	struct gen_pool *pool;
};

struct buffer_data {
	unsigned long addr;	/* addr returned by genalloc */
	uint64_t *addrs;	/* array of physical addresses, upcast to 64-bit */
	enum sprd_vdsp_mem_attr mattr;	/* memory attributes */
	struct vm_area_struct *mapped_vma;	/* Needed for cache manipulation */
	/* exporter via dmabuf */
	struct sg_table *sgt;
	bool exported;
	struct dma_buf *dma_buf;
	dma_addr_t dma_base;
	unsigned int dma_size;
	enum dma_data_direction dma_dir;
};

static int trace_physical_pages = 0;
static int trace_mmap_fault = 0;

/*
 * dmabuf wrapper ops
 */
static struct sg_table *carveout_map_dmabuf(struct dma_buf_attachment *attach,
					    enum dma_data_direction dir)
{
	struct buffer_data *buffer_data = attach->dmabuf->priv;

	if (!buffer_data)
		return NULL;

	sg_dma_address(buffer_data->sgt->sgl) = buffer_data->dma_base;
	sg_dma_len(buffer_data->sgt->sgl) = buffer_data->dma_size;

	return buffer_data->sgt;
}

static void carveout_unmap_dmabuf(struct dma_buf_attachment *attach,
				  struct sg_table *sgt,
				  enum dma_data_direction dir)
{
	struct buffer_data *buffer_data = attach->dmabuf->priv;

	if (!buffer_data)
		return;

	sg_dma_address(buffer_data->sgt->sgl) = (~(dma_addr_t) 0);
	sg_dma_len(buffer_data->sgt->sgl) = 0;
}

/* Called when when ref counter reaches zero! */
static void carveout_release_dmabuf(struct dma_buf *buf)
{
	struct buffer_data *buffer_data = buf->priv;

	if (!buffer_data)
		return;

	buffer_data->exported = false;
}

/* Called on file descriptor mmap */
static int carveout_mmap_dmabuf(struct dma_buf *buf, struct vm_area_struct *vma)
{
	struct buffer_data *buffer_data = buf->priv;

	if (!buffer_data)
		return -EINVAL;

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	return remap_pfn_range(vma, vma->vm_start,
			       page_to_pfn(sg_page(buffer_data->sgt->sgl)),
			       buffer_data->sgt->sgl->length,
			       vma->vm_page_prot);
}

static void *carveout_kmap_dmabuf(struct dma_buf *buf, unsigned long page)
{
	pr_err("not supported\n");
	return NULL;
}

static const struct dma_buf_ops carveout_dmabuf_ops = {
	.map_dma_buf = carveout_map_dmabuf,
	.unmap_dma_buf = carveout_unmap_dmabuf,
	.release = carveout_release_dmabuf,
	.mmap = carveout_mmap_dmabuf,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,12,0)
	.kmap_atomic = carveout_kmap_dmabuf,
	.kmap = carveout_kmap_dmabuf
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,19,0)
	.map_atomic = carveout_kmap_dmabuf,
#endif
	.map = carveout_kmap_dmabuf,
#endif
};

static void _dma_map(struct buffer *buffer)
{
	struct buffer_data *buffer_data = buffer->priv;
	struct sg_table *sgt = buffer_data->sgt;
	int ret = 0;

	if (buffer_data->dma_dir == DMA_NONE)
		buffer_data->dma_dir = DMA_BIDIRECTIONAL;

	ret = dma_map_sg(buffer->device, sgt->sgl, sgt->orig_nents,
			 buffer_data->dma_dir);
	if (ret <= 0) {
		pr_err("dma_map_sg failed!\n");
		buffer_data->dma_dir = DMA_NONE;
		return;
	}

	sgt->nents = ret;
}

static void _dma_unmap(struct buffer *buffer)
{
	struct buffer_data *buffer_data = buffer->priv;
	struct sg_table *sgt = buffer_data->sgt;

	if (buffer_data->dma_dir == DMA_NONE)
		return;

	dma_unmap_sg(buffer->device, sgt->sgl,
		     sgt->orig_nents, buffer_data->dma_dir);
	buffer_data->dma_dir = DMA_NONE;	// DMA_NONE =3

}

static int carveout_heap_export(struct device *device, struct heap *heap,
				size_t size, enum sprd_vdsp_mem_attr attr,
				struct buffer *buffer, uint64_t * buf_hnd)
{
	struct buffer_data *buffer_data = buffer->priv;
	struct dma_buf *dma_buf;
	int ret, fd;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0)
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
#endif

	if (!buffer_data)
		/* Nothing to export ? */
		return -ENOMEM;

	if (buffer_data->exported) {
		pr_err("already exported!\n");
		return -EBUSY;
	}

	if (!buffer_data->sgt) {
		/* Create for the very first time */
		buffer_data->sgt = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
		if (!buffer_data->sgt) {
			pr_err("failed to allocate sg_table\n");
			return -ENOMEM;
		}

		ret = sg_alloc_table(buffer_data->sgt, 1, GFP_KERNEL);
		if (ret) {
			pr_err("sg_alloc_table failed\n");
			goto free_sgt_mem;
		}
		sg_set_page(buffer_data->sgt->sgl,
			    pfn_to_page(PFN_DOWN(buffer_data->addr)),
			    PAGE_ALIGN(size), 0);
		/* Store dma info */
		if (heap->to_dev_addr)
			buffer_data->dma_base =
			    heap->to_dev_addr(&heap->options,
					      buffer_data->addr);
		else
			buffer_data->dma_base = buffer_data->addr;

		buffer_data->dma_size = PAGE_ALIGN(size);
		/* No mapping yet */
		sg_dma_address(buffer_data->sgt->sgl) = (~(dma_addr_t) 0);
		sg_dma_len(buffer_data->sgt->sgl) = 0;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,17,0)
	dma_buf =
	    dma_buf_export(buffer_data, &carveout_dmabuf_ops, size, O_RDWR);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4,1,0)
	dma_buf = dma_buf_export(buffer_data, &carveout_dmabuf_ops,
				 size, O_RDWR, NULL);
#else
	exp_info.ops = &carveout_dmabuf_ops;
	exp_info.size = size;
	exp_info.flags = O_RDWR;
	exp_info.priv = buffer_data;
	exp_info.resv = NULL;
	dma_buf = dma_buf_export(&exp_info);
#endif
	if (IS_ERR(dma_buf)) {
		pr_err("dma_buf_export failed\n");
		ret = PTR_ERR(dma_buf);
		return ret;
	}

	get_dma_buf(dma_buf);
	fd = dma_buf_fd(dma_buf, 0);
	if (fd < 0) {
		pr_err("dma_buf_fd failed\n");
		dma_buf_put(dma_buf);
		return -EFAULT;
	}
	buffer_data->dma_buf = dma_buf;
	buffer_data->exported = true;
	*buf_hnd = (uint64_t) fd;

	return 0;

free_sgt_mem:
	kfree(buffer_data->sgt);
	buffer_data->sgt = NULL;

	return ret;
}

static int carveout_heap_alloc(struct device *device, struct heap *heap,
			       size_t size, enum sprd_vdsp_mem_attr attr,
			       struct buffer *buffer)
{
	struct heap_data *heap_data = heap->priv;
	struct buffer_data *buffer_data;
	phys_addr_t phys_addr;
	size_t num_pages, p;
	struct sg_table *sgt;
	struct scatterlist *sgl;
	int ret;
	int i;

	buffer_data = kzalloc(sizeof(struct buffer_data), GFP_KERNEL);
	if (!buffer_data)
		return -ENOMEM;

	num_pages = PAGE_ALIGN(size) >> PAGE_SHIFT;

	buffer_data->addrs =
	    kmalloc_array(num_pages, sizeof(struct page *), GFP_KERNEL);
	if (!buffer_data->addrs) {
		ret = -ENOMEM;
		goto alloc_addrs_array_failed;
	}

	buffer_data->mattr = attr;

	buffer_data->addr = gen_pool_alloc(heap_data->pool, size);
	if (!buffer_data->addr) {
		pr_err("gen_pool_alloc failed!\n");
		ret = -ENOMEM;
		goto alloc_pool_fail;
	}

	/* The below assigns buffer_data->addr-> 1:1 mapping */
	phys_addr = gen_pool_virt_to_phys(heap_data->pool, buffer_data->addr);

	p = 0;
	while (p < num_pages) {
		if (trace_physical_pages)
			pr_debug("phys %llx\n", (unsigned long long)phys_addr);
		buffer_data->addrs[p++] = phys_addr;
		phys_addr += PAGE_SIZE;
	};

	sgt = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!sgt) {
		ret = -ENOMEM;
		goto alloc_pages_failed;
	}

	ret = sg_alloc_table(sgt, num_pages, GFP_KERNEL);
	if (ret)
		goto sg_alloc_table_failed;

	for_each_sg(sgt->sgl, sgl, sgt->nents, i) {
		//sg_set_page(sgl, page, PAGE_SIZE, 0);
		sg_set_page(sgl,
			    pfn_to_page(PFN_DOWN
					((phys_addr_t) buffer_data->addrs[i])),
			    PAGE_SIZE, 0);

		if (~dma_get_mask(device) & sg_phys(sgl)) {
			pr_err("physical address is out of dma_mask,"
			       " and probably won't be accessible by the core!\n");
			ret = -EFAULT;
			goto dma_mask_check_failed;
		}
	}

	buffer_data->sgt = sgt;

	buffer->priv = buffer_data;
	buffer_data->dma_dir = DMA_NONE;
	buffer->paddr = (phys_addr_t) buffer_data->addrs[0];

	return 0;

dma_mask_check_failed:
	sg_free_table(sgt);
sg_alloc_table_failed:
	kfree(sgt);
alloc_pages_failed:
	gen_pool_free(heap_data->pool, buffer_data->addr, size);
alloc_pool_fail:
	kfree(buffer_data->addrs);
alloc_addrs_array_failed:
	kfree(buffer_data);

	return ret;
}

static void carveout_heap_free(struct heap *heap, struct buffer *buffer)
{
	struct heap_data *heap_data = heap->priv;
	struct buffer_data *buffer_data = buffer->priv;

	/* If forgot to unmap */
	_dma_unmap(buffer);

	if (heap->options.carveout.put_kptr && buffer->kptr)
		heap->options.carveout.put_kptr(buffer->kptr);

	if (buffer_data->dma_buf) {
		dma_buf_put(buffer_data->dma_buf);
		buffer_data->dma_buf->priv = NULL;
	}

	if (buffer_data->sgt) {
		sg_free_table(buffer_data->sgt);
		kfree(buffer_data->sgt);
		buffer_data->sgt = NULL;
	}

	if (buffer_data->mapped_vma)
		buffer_data->mapped_vma->vm_private_data = NULL;

	gen_pool_free(heap_data->pool, buffer_data->addr, buffer->actual_size);
	kfree(buffer_data->addrs);
	kfree(buffer_data);
}

static void _mmap_open(struct vm_area_struct *vma)
{
	struct buffer *buffer = vma->vm_private_data;
	struct buffer_data *buffer_data = buffer->priv;

	buffer_data->mapped_vma = vma;
}

static void _mmap_close(struct vm_area_struct *vma)
{
	struct buffer *buffer = vma->vm_private_data;
	struct buffer_data *buffer_data;

	if (!buffer)
		return;

	buffer_data = buffer->priv;

	buffer_data->mapped_vma = NULL;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
static vm_fault_t _mmap_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
#else
static int _mmap_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
#endif
	struct buffer *buffer = vma->vm_private_data;
	struct buffer_data *buffer_data = buffer->priv;
	phys_addr_t phys_addr;
	pgoff_t offset;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
	unsigned long addr = vmf->address;
#else
	unsigned long addr = (unsigned long)vmf->virtual_address;
#endif

	if (trace_mmap_fault) {
		pr_debug("buffer %d (0x%p) vma:%p\n", buffer->id, buffer, vma);
		pr_debug("vm_start %#lx vm_end %#lx total size %ld\n",
			 vma->vm_start, vma->vm_end,
			 vma->vm_end - vma->vm_start);
	}

	offset = (addr - vma->vm_start) >> PAGE_SHIFT;

	if (offset > (buffer->actual_size / PAGE_SIZE)) {
		pr_err("offs:%ld\n", offset);
		return VM_FAULT_SIGBUS;
	}

	phys_addr = buffer_data->addrs[0] + (offset * PAGE_SIZE);

	if (trace_mmap_fault)
		pr_debug("vmf pgoff %#lx vmf addr %lx offs :%ld phys:%#llx\n",
			 vmf->pgoff, addr, offset, phys_addr);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0)
	return vmf_insert_pfn(vma, addr, phys_addr >> PAGE_SHIFT);
#else
	{
		int err = vm_insert_pfn(vma, addr, phys_addr >> PAGE_SHIFT);

		switch (err) {
		case 0:
		case -EAGAIN:
		case -ERESTARTSYS:
		case -EINTR:
		case -EBUSY:
			return VM_FAULT_NOPAGE;
		case -ENOMEM:
			return VM_FAULT_OOM;
		}

		return VM_FAULT_SIGBUS;
	}
#endif
}

/* vma ops->fault handler is used to track user space mappings
 * (inspired by other gpu/drm drivers from the kernel source tree)
 * to properly call cache handling ops when the mapping is destroyed
 * (when user calls unmap syscall).
 * vma flags are used to choose a correct direction.
 * The above facts allows us to do automatic cache flushing/invalidation.
 *
 * Examples:
 *  mmap() -> .open -> invalidate buffer cache
 *  .. read content from buffer
 *  unmap() -> .close -> do nothing
 *
 *  mmap() -> .open -> do nothing
 *  .. write content to buffer
 *  unmap() -> .close -> flush buffer cache
 */
static struct vm_operations_struct carveout_mmap_vm_ops = {
	.open = _mmap_open,
	.close = _mmap_close,
	.fault = _mmap_fault,
};

static int carveout_heap_map_um(struct heap *heap, struct buffer *buffer,
				struct vm_area_struct *vma)
{
	struct buffer_data *buffer_data = buffer->priv;

	/* CACHED by default */
	if (buffer_data->mattr & SPRD_VDSP_MEM_ATTR_UNCACHED)
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	else if (buffer_data->mattr & SPRD_VDSP_MEM_ATTR_WRITECOMBINE)
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	vma->vm_ops = &carveout_mmap_vm_ops;
	vma->vm_flags |= VM_PFNMAP;
	vma->vm_private_data = buffer;
	vma->vm_pgoff = 0;

	_mmap_open(vma);

	return 0;
}

static int carveout_heap_map_km(struct heap *heap, struct buffer *buffer)
{
	struct buffer_data *buffer_data = buffer->priv;
	struct sg_table *sgt = buffer_data->sgt;
	struct scatterlist *sgl = sgt->sgl;
	unsigned int num_pages = sg_nents(sgl);
	struct page **pages;
	pgprot_t prot;
	int i;

	if (buffer->kptr) {
		pr_warn("called for already mapped buffer %d\n", buffer->id);
		return 0;
	}

	/*
	 * Use vmalloc to avoid limit with kmalloc
	 * where max possible allocation is 4MB,
	 * therefore the limit for the buffer that can be mapped
	 * 4194304 = number of 4k pages x sizeof(struct page *)
	 * number of 4k pages = 524288 which represents ~2.1GB.
	 * */
	pages = vmalloc(num_pages * sizeof(struct page *));
	if (!pages) {
		pr_err("failed to allocate memory for pages\n");
		return -ENOMEM;
	}

	prot = PAGE_KERNEL;
	/* CACHED by default */
	if (buffer_data->mattr & SPRD_VDSP_MEM_ATTR_WRITECOMBINE) {
		prot = pgprot_writecombine(prot);
	} else if (buffer_data->mattr & SPRD_VDSP_MEM_ATTR_UNCACHED) {
		prot = pgprot_noncached(prot);
	} else {
		pr_err("Error: unknown buffer_data->mattr \n");
		BUG_ON(1);
	}

	/* Make dma mapping before mapping into kernel */
	if (!(buffer_data->mattr & SPRD_VDSP_MEM_ATTR_UNCACHED))
		_dma_map(buffer);

	i = 0;
	while (sgl) {
		pages[i++] = sg_page(sgl);
		sgl = sg_next(sgl);
	}

	buffer->kptr = vmap(pages, num_pages, VM_MAP, prot);
	vfree(pages);
	if (!buffer->kptr) {
		pr_err("vmap failed!\n");
		return -EFAULT;
	}

	return 0;
}

static int carveout_heap_unmap_km(struct heap *heap, struct buffer *buffer)
{

	if (!buffer->kptr) {
		pr_warn("called for already unmapped buffer %d\n", buffer->id);
		return -EFAULT;
	}

	_dma_unmap(buffer);

	vunmap(buffer->kptr);
	buffer->kptr = NULL;

	return 0;
}

static int carveout_heap_get_sg_table(struct heap *heap,
				      struct buffer *buffer,
				      struct sg_table **sg_table)
{
	struct buffer_data *buffer_data = buffer->priv;

	if (!buffer_data)
		return -EINVAL;

	*sg_table = buffer_data->sgt;
	return 0;
}

static void carveout_cache_update(struct vm_area_struct *vma)
{
	if (!vma)
		return;

#if !defined(CONFIG_ARM) && !defined(CONFIG_ARM64)
	/* This function is not exported for modules by ARM kernel */
	flush_cache_range(vma, vma->vm_start, vma->vm_end);
#else
	/* Tentative for the SFF, this function is exported by the kernel... */
	/* vivt_flush_cache_range(vma, vma->vm_start, vma->vm_end); */
#endif
}

static void carveout_sync_cpu_to_dev(struct heap *heap, struct buffer *buffer)
{
	struct buffer_data *buffer_data = buffer->priv;

	if (!(buffer_data->mattr & SPRD_VDSP_MEM_ATTR_UNCACHED))
		carveout_cache_update(buffer_data->mapped_vma);
}

static void carveout_sync_dev_to_cpu(struct heap *heap, struct buffer *buffer)
{
	struct buffer_data *buffer_data = buffer->priv;

	if (!(buffer_data->mattr & SPRD_VDSP_MEM_ATTR_UNCACHED))
		carveout_cache_update(buffer_data->mapped_vma);
}

static void carveout_heap_destroy(struct heap *heap)
{
	struct heap_data *heap_data = heap->priv;

	gen_pool_destroy(heap_data->pool);
	kfree(heap_data);
}

static struct heap_ops carveout_heap_ops = {
	.export = carveout_heap_export,
	.alloc = carveout_heap_alloc,
	.import = NULL,
	.free = carveout_heap_free,
	.map_um = carveout_heap_map_um,
	.unmap_um = NULL,
	.map_km = carveout_heap_map_km,
	.unmap_km = carveout_heap_unmap_km,
	.get_sg_table = carveout_heap_get_sg_table,
	.get_page_array = NULL,
	.sync_cpu_to_dev = carveout_sync_cpu_to_dev,
	.sync_dev_to_cpu = carveout_sync_dev_to_cpu,
	.destroy = carveout_heap_destroy,
};

int sprd_vdsp_mem_carveout_init(const struct heap_config *config,
				struct heap *heap)
{
	struct heap_data *heap_data;
	unsigned long virt_start;
	int ret;
	int pool_order =
	    POOL_ALLOC_ORDER_BASE + heap->options.carveout.pool_order;

	pr_debug("phys:%#llx offs:%llx order:%d\n",
		 (unsigned long long)config->options.carveout.phys,
		 (unsigned long long)config->options.carveout.offs, pool_order);

	if (config->options.carveout.phys & ((1 << pool_order) - 1)) {
		pr_err
		    ("phys addr (%#llx) is not aligned to allocation order!\n",
		     (unsigned long long)config->options.carveout.phys);
		return -EINVAL;
	}

	if (config->options.carveout.size == 0) {
		pr_err("size cannot be zero!\n");
		return -EINVAL;
	}

	heap_data = kmalloc(sizeof(struct heap_data), GFP_KERNEL);
	if (!heap_data)
		return -ENOMEM;

	heap_data->pool = gen_pool_create(pool_order, -1);
	if (!heap_data->pool) {
		pr_err("gen_pool_create failed\n");
		ret = -ENOMEM;
		goto pool_create_failed;
	}

	/* Operating in no offset mode -> virtual == phys
	 * However when physical address == 0 (unlikely) we need to distinguish
	 * if address returned from gen_pool_alloc is an error or valid address,
	 * so add a const offset.
	 */
	virt_start = (unsigned long)config->options.carveout.phys;
	if (!virt_start)
		virt_start = 1 << pool_order;

	ret = gen_pool_add_virt(heap_data->pool, virt_start,
				config->options.carveout.phys,
				config->options.carveout.size, -1);
	if (ret) {
		pr_err("gen_pool_add_virt failed\n");
		goto pool_add_failed;
	}

	heap->ops = &carveout_heap_ops;
	heap->priv = heap_data;

	return 0;

pool_add_failed:
	gen_pool_destroy(heap_data->pool);
pool_create_failed:
	kfree(heap_data);
	return ret;
}
