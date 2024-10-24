#ifndef _SPRD_VDSP_IOVA_H_
#define _SPRD_VDSP_IOVA_H_

#include <linux/types.h>
#include <linux/genalloc.h>


struct iova_reserve{
	char *name;
	struct genpool_data_fixed fixed;
	unsigned long iova_addr;
	size_t size;
	int status;
};

struct sprd_vdsp_iommu_iova {
	unsigned long iova_base;	// iova base addr
	size_t iova_size;	        // iova range size
	struct gen_pool *pool;
	struct iommu_iova_ops *ops;
	struct iova_reserve *reserve_data; // reserve data
	unsigned reserve_num;              // reserve num
};

struct iommu_iova_ops {
	int (*iova_init) (struct sprd_vdsp_iommu_iova * iova,
			  unsigned long iova_base, size_t iova_size,
			  int min_alloc_order);
	void (*iova_release) (struct sprd_vdsp_iommu_iova * iova);
	unsigned long (*iova_alloc) (struct sprd_vdsp_iommu_iova * iova,
				     size_t iova_length);
	void (*iova_free) (struct sprd_vdsp_iommu_iova * iova,
			   unsigned long iova_addr, size_t iova_length);
	int (*iova_reserve_init)(struct sprd_vdsp_iommu_iova *iova,struct iova_reserve *reserve_data,unsigned int reserve_num);
	void (*iova_reserve_relsase)(struct sprd_vdsp_iommu_iova *iova);
	int (*iova_alloc_fixed)(struct sprd_vdsp_iommu_iova *iova,unsigned long *iova_addr,size_t iova_length);
};

extern struct iommu_iova_ops version12_iova_ops;

#endif // end _SPRD_VDSP_IOVA_H_
