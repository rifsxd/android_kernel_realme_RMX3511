#ifndef _SPRD_VDSP_IOMMU_H_
#define _SPRD_VDSP_IOMMU_H_

#include <linux/of.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include "sprd_vdsp_iommu_dev.h"
#include "sprd_vdsp_iova.h"

#ifdef BSP_DTB_SHARKL5PRO
enum sprd_vdsp_iommu_id {	//NOTE:This order must be consistent with the order in dts。
	SPRD_VDSP_IOMMU_EPP = 0,
	SPRD_VDSP_IOMMU_EDP,
	SPRD_VDSP_IOMMU_IDMA,
	SPRD_VDSP_IOMMU_VDMA,
	SPRD_VDSP_IOMMU_MAX,
};

#define VDSP_IOMMU_VERSION 12	//for iova version
#endif
#ifdef BSP_DTB_QOGIRN6PRO
enum sprd_vdsp_iommu_id {
	SPRD_VDSP_IOMMU_EPP = 0,
	SPRD_VDSP_IOMMU_IDMA,
	SPRD_VDSP_IOMMU_VDMA,
	SPRD_VDSP_IOMMU_MAX,
};

#define VDSP_IOMMU_VERSION 12	//for iova version
#endif
enum VDSP_IOMMU_ID {		// For compatibility，use in cll.c cand dll.h file
	VDSP_IOMMU_VAUL5P_EPP,
	VDSP_IOMMU_VAUL5P_EDP,
	VDSP_IOMMU_VAUL5P_IDMA,
	VDSP_IOMMU_VAUL5P_VDMA,
	VDSP_IOMMU_MAX,
};

enum sprd_vdsp_iommu_type {
	SPRD_IOMMUVAU_SHARKL5P,
	SPRD_IOMMU_NOT_SUPPORT,
};

struct sprd_vdsp_iommus {
	struct device *dev;
	struct sprd_vdsp_iommu_dev *iommu_devs[SPRD_VDSP_IOMMU_MAX];
	struct sprd_vdsp_iommus_ops *ops;
	struct mutex mutex;	// for all iommu_dev
#ifdef  VDSP_IOMMU_USE_SIGNAL_IOVA
	unsigned long iova_base;	// iova base addr
	size_t iova_size;	// iova range size
	struct sprd_vdsp_iommu_iova *iova_dev;	// iommu iova manager
	struct sprd_vdsp_iommu_map_record *record_dev;	// iommu map record manager
#endif
};

struct sprd_vdsp_iommus_ops {
	int (*init) (struct sprd_vdsp_iommus * iommus,
		     struct device_node * of_node, struct device * dev);
	void (*release) (struct sprd_vdsp_iommus * iommus);
	int (*map_all) (struct sprd_vdsp_iommus * iommus,
			struct sprd_vdsp_iommu_map_conf * map_conf);
	int (*unmap_all) (struct sprd_vdsp_iommus * iommus,
			  struct sprd_vdsp_iommu_unmap_conf * unmap_conf);
	//map special iommu
	int (*map_idx) (struct sprd_vdsp_iommus * iommus,
			struct sprd_vdsp_iommu_map_conf * map_conf,
			int iommu_id);
	int (*unmap_idx) (struct sprd_vdsp_iommus * iommus,
			  struct sprd_vdsp_iommu_unmap_conf * unmap_conf,
			  int iommu_id);
	int (*reserve_init)(struct sprd_vdsp_iommus * iommus,
			  struct iova_reserve *reserve_data,unsigned int reserve_num);
	void (*reserve_release)(struct sprd_vdsp_iommus * iommus);
	//power and clock
	int (*suspend) (struct sprd_vdsp_iommus * iommus);
	int (*resume) (struct sprd_vdsp_iommus * iommus);
	int (*restore) (struct sprd_vdsp_iommus * iommus);
};

extern struct sprd_vdsp_iommus_ops iommus_ops;
#endif //_SPRD_VDSP_IOMMU_H_
