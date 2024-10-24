// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef SPRD_VDSP_MEM_MAN_H
#define SPRD_VDSP_MEM_MAN_H

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
#define KERNEL_DMA_FENCE_SUPPORT
#endif

#include <linux/mm.h>
#include <linux/types.h>
#include <linux/device.h>
#ifdef KERNEL_DMA_FENCE_SUPPORT
#include <linux/dma-fence.h>
#endif
#include "sprd_vdsp_mem_core_uapi.h"

union heap_options {
	struct {
		gfp_t gfp_type;	/* pool and flags for buffer allocations */
		int min_order;	/* minimum page allocation order */
		int max_order;	/* maximum page allocation order */
	} unified;

	struct {
		struct ion_client *client;	/* must be provided by platform */
	} ion;

	struct {
		void *(*get_kptr) (phys_addr_t addr,
				   size_t size, enum sprd_vdsp_mem_attr mattr);
		int (*put_kptr) (void *);
		phys_addr_t phys;	/* physical address start of memory */
		size_t size;	/* size of memory */
		unsigned long offs;	/* optional offset of the start
					   of memory as seen from device,
					   zero by default */
		int pool_order;	/* allocation order */
	} carveout;

	struct {
		gfp_t gfp_flags;	/* for buffer allocations */
	} coherent;

	struct {
		phys_addr_t phys;	/* physical address start of memory */
		size_t size;	/* size of memory */
	} ocm;
};

struct heap_config {
	enum sprd_vdsp_mem_heap_type type;
	union heap_options options;
	/* (optional) functions to convert a physical address as seen from
	   the CPU to the physical address as seen from the vha device and
	   vice versa. When not implemented,
	   it is assumed that physical addresses are the
	   same regardless of viewpoint */
	 phys_addr_t(*to_dev_addr) (union heap_options * opts,
				    phys_addr_t addr);
	 phys_addr_t(*to_host_addr) (union heap_options * opts,
				     phys_addr_t addr);
	/* Cache attribute,
	 * could be platform specific if provided - overwrites the global cache policy */
	enum sprd_vdsp_mem_attr cache_attr;
};

struct mem_ctx;

int sprd_vdsp_mem_core_init(void);
void sprd_vdsp_mem_core_exit(void);

int sprd_vdsp_mem_add_heap(const struct heap_config *heap_cfg, int *heap_id);
void sprd_vdsp_mem_del_heap(int heap_id);
int sprd_vdsp_mem_get_heap_info(int heap_id, uint8_t * type, uint32_t * attrs);
int sprd_vdsp_mem_get_heap_id(enum sprd_vdsp_mem_heap_type type);

/*
*  related to process context (contains SYSMEM heap's functionality in general)
*/

int sprd_vdsp_mem_create_proc_ctx(struct mem_ctx **ctx);
void sprd_vdsp_mem_destroy_proc_ctx(struct mem_ctx *ctx);

int sprd_vdsp_mem_alloc(struct device *device, struct mem_ctx *ctx, int heap_id,
			size_t size, enum sprd_vdsp_mem_attr attributes,
			int *buf_id);
int sprd_vdsp_mem_import(struct device *device, struct mem_ctx *ctx,
			 int heap_id, size_t size,
			 enum sprd_vdsp_mem_attr attributes, uint64_t buf_hnd,
			 int *buf_id);
int sprd_vdsp_mem_export(struct device *device, struct mem_ctx *ctx, int buf_id,
			 size_t size, enum sprd_vdsp_mem_attr attributes,
			 uint64_t * buf_hnd);
void sprd_vdsp_mem_free(struct mem_ctx *ctx, int buf_id);

int sprd_vdsp_mem_map_um(struct mem_ctx *ctx, int buf_id,
			 struct vm_area_struct *vma);
int sprd_vdsp_mem_unmap_um(struct mem_ctx *ctx, int buf_id);
int sprd_vdsp_mem_map_km(struct mem_ctx *ctx, int buf_id);
int sprd_vdsp_mem_unmap_km(struct mem_ctx *ctx, int buf_id);
void *sprd_vdsp_mem_get_kptr(struct mem_ctx *ctx, int buf_id);
uint64_t *sprd_vdsp_mem_get_page_array(struct mem_ctx *mem_ctx, int buf_id);
uint64_t sprd_vdsp_mem_get_single_page(struct mem_ctx *mem_ctx, int buf_id,
				       unsigned int offset);
phys_addr_t sprd_vdsp_mem_get_dev_addr(struct mem_ctx *mem_ctx, int buf_id);
size_t sprd_vdsp_mem_get_size(struct mem_ctx *mem_ctx, int buf_id);
phys_addr_t sprd_vdsp_mem_get_phy_addr(struct mem_ctx *mem_ctx, int buf_id);

int sprd_vdsp_mem_sync_cpu_to_device(struct mem_ctx *ctx, int buf_id);
int sprd_vdsp_mem_sync_device_to_cpu(struct mem_ctx *ctx, int buf_id);

size_t sprd_vdsp_mem_get_usage_max(const struct mem_ctx *ctx);
size_t sprd_vdsp_mem_get_usage_current(const struct mem_ctx *ctx);

#ifdef KERNEL_DMA_FENCE_SUPPORT
struct dma_fence *sprd_vdsp_mem_add_fence(struct mem_ctx *ctx, int buf_id);
void sprd_vdsp_mem_remove_fence(struct mem_ctx *ctx, int buf_id);
int sprd_vdsp_mem_signal_fence(struct mem_ctx *ctx, int buf_id);
#endif

/*
* related to stream MMU context (constains IMGMMU functionality in general)
*/
int sprd_vdsp_mem_map_iova(struct mem_ctx *mem_ctx, int buf_id,int isfixed,unsigned long fixed_data);
int sprd_vdsp_mem_unmap_iova(struct mem_ctx *mem_ctx, int buf_id);
#endif /* SPRD_VDSP_MEM_MAN_H */
