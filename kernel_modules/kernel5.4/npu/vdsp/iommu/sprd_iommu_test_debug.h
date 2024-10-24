#ifndef _IOMMU_DEV_TEST_DEBUG_
#define _IOMMU_DEV_TEST_DEBUG_

//file: sprd_iommu_dev_test_debug.c
//print struct iommu_dev
extern void debug_print_iommu_dev(struct sprd_vdsp_iommu_dev *iommu_dev);

//dump iommu_dev pagetable
extern int iommu_dump_pagetable(struct sprd_vdsp_iommu_dev *iommu_dev);

//alloc memory of size and init sg_table
extern void *alloc_sg_list(struct sg_table *sgt, size_t size);

//print all iommu_dev' map_record
extern void show_iommus_all_record(struct sprd_vdsp_iommus *iommus);

#endif //_IOMMU_DEV_TEST_DEBUG_
