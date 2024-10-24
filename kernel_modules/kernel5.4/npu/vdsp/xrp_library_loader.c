
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/dma-mapping.h>
#include <linux/elf.h>
#include <linux/firmware.h>
#include <linux/highmem.h>
#include <linux/hashtable.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include "xrp_internal.h"
#include "xvp_main.h"
#include "xrp_library_loader.h"
#include "xt_library_loader.h"
#include "xrp_kernel_dsp_interface.h"
#include "xrp_kernel_defs.h"
#ifdef IOMMUANDMEM
#include "sprd_vdsp_mem_xvpfile.h"
#endif

#define LIBRARY_CMD_PIL_INFO_OFFSET   40
#define LIBRARY_CMD_LOAD_UNLOAD_INPUTSIZE 44

#define XRP_EXAMPLE_V3_NSID_INITIALIZER \
{0x73, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x20, 0x63, \
        0x6d, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
#define LIBRARY_LOAD_UNLOAD_NSID (unsigned char [])XRP_EXAMPLE_V3_NSID_INITIALIZER

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: library_loader %d %d %s : "\
        fmt, current->pid, __LINE__, __func__

static void *libinfo_alloc_element()
{
	struct loadlib_info *pnew = NULL;

	pnew = vmalloc(sizeof(struct loadlib_info));
	if (unlikely(pnew != NULL)) {
		memset(pnew, 0, sizeof(struct loadlib_info));
		mutex_init(&pnew->mutex);
	}

	return pnew;
}

static xt_ptr xt_lib_memcpy(xt_ptr dest, const void *src, unsigned int n,
			    void *user)
{
	memcpy(user, src, n);
	return LIB_RESULT_OK;
}

static xt_ptr xt_lib_memset(xt_ptr s, int c, unsigned int n, void *user)
{
#ifdef IOMMUANDMEM
	memset_io(user, c, n);
#else
	memset(user, c, n);
#endif
	return LIB_RESULT_OK;
}

/*
* check lib name whether load,
* 1: aready load by self xvp file.
* 2: aready load by other xvp file.
* xvp->xvp file->load_lib_list, When multiple xvp loads the same algorithm library,
* each xvp will record a loading data, but there is only one actual loading process.
*/
static uint32_t xrp_check_whether_loaded(struct file *filp, const char *libname,
					 struct loadlib_info **outlibinfo)
{
	struct loadlib_info *libinfo, *libinfo1;
	struct xvp_file *xvpfile_temp;
	unsigned long bkt;
	uint32_t find = 0;
	struct xrp_known_file *p;
	struct xvp *xvp = ((struct xvp_file *)(filp->private_data))->xvp;
	struct xvp_file *xvp_file = (struct xvp_file *)filp->private_data;

	libinfo = libinfo1 = NULL;
	*outlibinfo = NULL;

	/*check whether loaded myself */
	list_for_each_entry(libinfo, &xvp_file->load_lib_list, node_libinfo) {
		if (0 == strcmp(libinfo->libname, libname)) {
			find = 1;
			break;
		}
	}
	if (1 == find) {
		pr_debug("this filp has loaded libname:%s\n", libname);
		return 1;	/*loaded return 1 */
	}
	//look for lib name in All xvp file lib list.
	mutex_lock(&xvp->xrp_known_files_lock);
	hash_for_each(xvp->xrp_known_files, bkt, p, node) {
		if (((struct file *)(p->filp))->private_data != xvp_file) {
			xvpfile_temp =
			    (struct xvp_file *)(((struct file *)(p->filp))->
						private_data);
			find = 0;
			list_for_each_entry(libinfo1,
					    &xvpfile_temp->load_lib_list,
					    node_libinfo) {
				pr_debug("enter libname:%s, %s\n",
					 libinfo1->libname, libname);
				if (0 == strcmp(libinfo1->libname, libname)) {
					find = 1;
					*outlibinfo = libinfo1;
					break;
				}
			}
			if (1 == find) {
				break;
			}
		}
	}
	mutex_unlock(&xvp->xrp_known_files_lock);
	return find;
}

/*look for current lib whether running now in all xvp file*/
static int32_t xrp_library_checkprocessing(struct file *filp,
					   const char *libname)
{
	unsigned long bkt;
	struct xrp_known_file *p;
	struct xvp_file *xvp_file;
	struct xvp *xvp =
	    (struct xvp *)(((struct xvp_file *)(filp->private_data))->xvp);
	struct loadlib_info *libinfo = NULL;

	mutex_lock(&xvp->xrp_known_files_lock);
	hash_for_each(xvp->xrp_known_files, bkt, p, node) {
		xvp_file =
		    (struct xvp_file *)(((struct file *)(p->filp))->
					private_data);
		list_for_each_entry(libinfo, &xvp_file->load_lib_list,
				    node_libinfo) {
			if ((0 == strcmp(libinfo->libname, libname))
			    && (libinfo->lib_state ==
				XRP_LIBRARY_PROCESSING_CMD)) {
				pr_debug
				    ("xrp_library_checkprocessing processing\n");
				mutex_unlock(&xvp->xrp_known_files_lock);
				return 1;
			}
		}
	}
	mutex_unlock(&xvp->xrp_known_files_lock);
	return 0;
}

/*look for lib info through lib name in current xvp file*/
static struct loadlib_info *xrp_library_getlibinfo(struct file *filp,
						   const char *libname)
{
	struct loadlib_info *libinfo = NULL;
	struct xvp_file *xvp_file = (struct xvp_file *)filp->private_data;

	list_for_each_entry(libinfo, &xvp_file->load_lib_list, node_libinfo) {
		if (0 == strcmp(libinfo->libname, libname)) {
			return libinfo;
		}
	}
	return NULL;
}

/*
* buffer : all lib binary.
* libname: lib name
* comment: malloc and map memory to realy load the lib.
* (only one memory for each lib, so before malloc, it need check multiple)
* function name called xrp_library_request_resource maybe good.
*/
#ifdef IOMMUANDMEM
static int32_t xrp_library_load_internal(struct file *filp, const char *buffer,
					 const char *libname)
{
	int32_t ret = 0;
	struct loadlib_info *new_element;
	unsigned int result;
	struct xvp_buf *lib_buf = NULL;
	struct xvp_buf *libinfo_buf = NULL;

	struct xvp_file *xvp_file = (struct xvp_file *)(filp->private_data);

	char *name = NULL;
	uint64_t size = 0;
	uint32_t heap_type = 0;
	uint32_t attr = 0;

	/*load library to ddr */
	size = xtlib_pi_library_size((xtlib_packaged_library *) buffer);
	if (size <= 0) {
		pr_err("library size is invaild\n");
		return -EFAULT;
	}
	/*alloc lib buffer */
	name = "xvp lib_buffer";
	heap_type = SPRD_VDSP_MEM_HEAP_TYPE_UNIFIED;
	attr = SPRD_VDSP_MEM_ATTR_WRITECOMBINE;
	lib_buf =
	    xvp_buf_alloc_with_iommu(xvp_file->xvp, name, size, heap_type,
				     attr);
	if (!lib_buf) {
		pr_err("Error:xvp_buf_alloc_with_iommu faild\n");
		goto err;
	}
	if (xvp_buf_kmap(xvp_file->xvp, lib_buf)) {
		pr_err("Error: xvp_buf_kmap failed\n");
		goto err_xvp_buf_kmap_lib_buf;
	}

	/*alloc libinfo buffer */
	name = "xvp libinfo_buffer";
	size = sizeof(xtlib_pil_info);
	heap_type = SPRD_VDSP_MEM_HEAP_TYPE_UNIFIED;
	attr = SPRD_VDSP_MEM_ATTR_WRITECOMBINE;
	libinfo_buf =
	    xvp_buf_alloc_with_iommu(xvp_file->xvp, name, size, heap_type,
				     attr);
	if (!libinfo_buf) {
		pr_err("Error:xvp_buf_alloc_with_iommu faild\n");
		goto err_alloc_libinfo_buffer;
	}
	if (xvp_buf_kmap(xvp_file->xvp, libinfo_buf)) {
		pr_err("Error: xvp_buf_kmap failed\n");
		goto err_xvp_buf_kmap_libinfo_buf;
	}
	pr_debug
	    ("buffer:%p, lib_buf_iova:%lx, lib_buf_vaddr:%p, libinfo_buf_iova:%lx, libinfo_buf_vaddr:%p\n",
	     buffer, (unsigned long)lib_buf->iova, lib_buf->vaddr,
	     (unsigned long)libinfo_buf->iova, libinfo_buf->vaddr);

	result =
	    xtlib_host_load_pi_library((xtlib_packaged_library *) buffer,
				       lib_buf->iova,
				       (xtlib_pil_info *) libinfo_buf->vaddr,
				       xt_lib_memcpy, xt_lib_memset,
				       lib_buf->vaddr);
	if (unlikely(result == 0)) {
		/*free ion buffer */
		pr_err("Error: xtlib_host_load_pi_library failed\n");
		ret = -EFAULT;
		goto err_xtlib_host_load_pi_library;
	}

	new_element = (struct loadlib_info *)libinfo_alloc_element();
	if (unlikely(new_element == NULL)) {
		/*free ion buffer */
		pr_err("Error: libinfo_alloc_element failed\n");
		ret = -ENOMEM;
		goto err_libinfo_alloc_element;
	} else {
		snprintf(new_element->libname, XRP_NAMESPACE_ID_SIZE, "%s",
			 libname);
		/*may be change later */
		new_element->length = size;
		new_element->load_count = 0;
		new_element->handle = lib_buf->buf_id;
		new_element->ion_phy = lib_buf->iova;
		new_element->ion_kaddr = lib_buf->vaddr;
		new_element->pil_handle = libinfo_buf->buf_id;
		new_element->pil_info = libinfo_buf->iova;
		new_element->lib_state = XRP_LIBRARY_LOADING;
		new_element->lib_processing_count = 0;
		new_element->original_flag = 1;
	}
	pr_debug("add new libinfo xvpfile:%x, libname:%s\n", xvp_file, libname);
	pr_debug("lib_buf_id:%d, libinfo_buf_id:%d\n", lib_buf->buf_id,
		 libinfo_buf->buf_id);
	list_add_tail(&new_element->node_libinfo, &xvp_file->load_lib_list);

	return LIB_RESULT_OK;

err_libinfo_alloc_element:
err_xtlib_host_load_pi_library:
	xvp_buf_kunmap(xvp_file->xvp, libinfo_buf);
err_xvp_buf_kmap_libinfo_buf:
	xvp_buf_free_with_iommu(xvp_file->xvp, libinfo_buf);
err_alloc_libinfo_buffer:
	xvp_buf_kunmap(xvp_file->xvp, lib_buf);
err_xvp_buf_kmap_lib_buf:
	xvp_buf_free_with_iommu(xvp_file->xvp, lib_buf);
err:
	return -EFAULT;
}
#else
static int32_t xrp_library_load_internal(struct file *filp, const char *buffer,
					 const char *libname)
{
	unsigned int size;
	int32_t ret = 0;
	struct loadlib_info *new_element;
	unsigned int result;
	struct ion_buf *lib_ion_mem = NULL;	// use for store lib bin
	struct ion_buf *libinfo_ion_mem = NULL;	// use for store lib key information
	void *kvaddr = NULL;
	void *kvaddr_libinfo = NULL;
	struct xvp_file *xvp_file = (struct xvp_file *)(filp->private_data);
	struct xvp *xvp = ((struct xvp_file *)(filp->private_data))->xvp;
	phys_addr_t kpaddr, kpaddr_libinfo;

	/*load library to ddr */
	size = xtlib_pi_library_size((xtlib_packaged_library *) buffer);
	/*alloc ion buffer later */
	lib_ion_mem = vmalloc(sizeof(struct ion_buf));
	if (unlikely(lib_ion_mem == NULL)) {
		pr_err("[ERROR]vmalloc fail,lib:%p,libinfo:%p\n", lib_ion_mem);
		return -ENOMEM;
	}
	libinfo_ion_mem = vmalloc(sizeof(struct ion_buf));
	if (unlikely(libinfo_ion_mem == NULL)) {
		pr_err("[ERROR]vmalloc fail,lib:%p,libinfo:%p\n",
		       libinfo_ion_mem);
		ret = -ENOMEM;
		goto __load_internal_err0;
	}
	/*alloc lib ion buffer */
	ret =
	    xvp->vdsp_mem_desc->ops->mem_alloc(xvp->vdsp_mem_desc, lib_ion_mem,
					       ION_HEAP_ID_MASK_SYSTEM, size);
	if (unlikely(ret != 0)) {
		ret = -ENOMEM;
		pr_err("[ERROR]alloc lib_ion_mem failed\n");
		goto __load_internal_err1;
	}
	lib_ion_mem->dev = xvp->dev;
	/*alloc libinfo ion buffer */
	ret = xvp->vdsp_mem_desc->ops->mem_alloc(xvp->vdsp_mem_desc,
						 libinfo_ion_mem,
						 ION_HEAP_ID_MASK_SYSTEM,
						 sizeof(xtlib_pil_info));
	if (unlikely(ret != 0)) {
		ret = -ENOMEM;
		pr_err("[ERROR]alloc libinfo_ion_mem failed\n");
		goto __load_internal_err2;
	}
	libinfo_ion_mem->dev = xvp->dev;
	ret =
	    xvp->vdsp_mem_desc->ops->mem_kmap(xvp->vdsp_mem_desc, lib_ion_mem);
	if (unlikely(ret != 0)) {
		pr_err("[ERROR]mem_kmap lib_ion_mem failed\n");
		ret = -EFAULT;
		goto __load_internal_err3;
	}
	kvaddr = (void *)lib_ion_mem->addr_k;
	ret =
	    xvp->vdsp_mem_desc->ops->mem_iommu_map(xvp->vdsp_mem_desc,
						   lib_ion_mem, IOMMU_ALL);
	if (unlikely(ret != 0)) {
		pr_err("[ERROR] mem_iommu_map lib_ion_mem failed\n");
		ret = -EFAULT;
		goto __load_internal_err4;
	}
	kpaddr = lib_ion_mem->iova;

	ret =
	    xvp->vdsp_mem_desc->ops->mem_kmap(xvp->vdsp_mem_desc,
					      libinfo_ion_mem);
	if (unlikely(ret != 0)) {
		pr_err("[ERROR]mem_kmap libinfo_ion_mem failed\n");
		ret = -EFAULT;
		goto __load_internal_err5;
	}
	kvaddr_libinfo = (void *)libinfo_ion_mem->addr_k;
	ret =
	    xvp->vdsp_mem_desc->ops->mem_iommu_map(xvp->vdsp_mem_desc,
						   libinfo_ion_mem, IOMMU_ALL);
	if (unlikely(ret != 0)) {
		pr_err("[ERROR]mem_iommu_map libinfo_ion_mem failed\n");
		ret = -EFAULT;
		goto __load_internal_err6;
	}
	kpaddr_libinfo = libinfo_ion_mem->iova;
	pr_debug
	    ("buffer:%p, kpaddr:%lx, kvaddr:%p, (libinfo)kpaddr:%lx, kvaddr:%p\n",
	     buffer, (unsigned long)kpaddr, kvaddr,
	     (unsigned long)kpaddr_libinfo, kvaddr_libinfo);

	result =
	    xtlib_host_load_pi_library((xtlib_packaged_library *) buffer,
				       kpaddr,
				       (xtlib_pil_info *) kvaddr_libinfo,
				       xt_lib_memcpy, xt_lib_memset, kvaddr);
	if (unlikely(result == 0)) {
		/*free ion buffer */
		pr_err("[ERROR]xtlib_host_load_pi_library failed\n");
		ret = -EFAULT;
		goto __load_internal_err7;
	}

	new_element = (struct loadlib_info *)libinfo_alloc_element();
	if (unlikely(new_element == NULL)) {
		/*free ion buffer */
		pr_err("[ERROR]libinfo_alloc_element failed\n");
		ret = -ENOMEM;
		goto __load_internal_err7;
	} else {
		snprintf(new_element->libname, XRP_NAMESPACE_ID_SIZE, "%s",
			 libname);
		/*may be change later */
		new_element->length = size;
		new_element->load_count = 0;
		new_element->ionhandle = lib_ion_mem;
		new_element->ion_phy = (phys_addr_t) kpaddr;
		new_element->ion_kaddr = kvaddr;
		new_element->pil_ionhandle = libinfo_ion_mem;
		new_element->pil_info = kpaddr_libinfo;
		new_element->lib_state = XRP_LIBRARY_LOADING;
		new_element->lib_processing_count = 0;
		new_element->original_flag = 1;
	}
	pr_debug("add new libinfo xvpfile:%x, libname:%s\n", xvp_file, libname);
	list_add_tail(&new_element->node_libinfo, &xvp_file->load_lib_list);

	return LIB_RESULT_OK;

__load_internal_err7:
	xvp->vdsp_mem_desc->ops->mem_iommu_unmap(xvp->vdsp_mem_desc,
						 libinfo_ion_mem, IOMMU_ALL);
__load_internal_err6:
	xvp->vdsp_mem_desc->ops->mem_kunmap(xvp->vdsp_mem_desc,
					    libinfo_ion_mem);
__load_internal_err5:
	xvp->vdsp_mem_desc->ops->mem_iommu_unmap(xvp->vdsp_mem_desc,
						 lib_ion_mem, IOMMU_ALL);
__load_internal_err4:
	xvp->vdsp_mem_desc->ops->mem_kunmap(xvp->vdsp_mem_desc, lib_ion_mem);
__load_internal_err3:
	xvp->vdsp_mem_desc->ops->mem_free(xvp->vdsp_mem_desc, libinfo_ion_mem);
__load_internal_err2:
	xvp->vdsp_mem_desc->ops->mem_free(xvp->vdsp_mem_desc, lib_ion_mem);
__load_internal_err1:
	vfree(libinfo_ion_mem);
__load_internal_err0:
	vfree(lib_ion_mem);
	return ret;
}
#endif

/*check cmd type (common cmd/ load/unload cmd/ error)*/
enum load_unload_flag xrp_check_load_unload(struct xvp *xvp,
					    struct xrp_request *rq,
					    uint32_t krqflag)
{
	__u32 indata_size;
	enum load_unload_flag load_flag = XRP_NOT_LOAD_UNLOAD;
	__u8 *tempbuffer = NULL;
	void *tempsrc = NULL;

	indata_size = rq->ioctl_queue.in_data_size;
	if (0 == strcmp(rq->nsid, LIBRARY_LOAD_UNLOAD_NSID)) {
		if (indata_size > XRP_DSP_CMD_INLINE_DATA_SIZE)
			tempsrc = (void *)(rq->ioctl_queue.in_data_addr);
		else
			tempsrc = rq->in_data;

		if (krqflag == 1) {
			load_flag = XRP_UNLOAD_LIB_FLAG;
		} else {
			tempbuffer = vmalloc(indata_size);
			if (unlikely(tempbuffer == NULL)) {
				return -ENOMEM;
			}
			memset(tempbuffer, 0, indata_size);
			if (unlikely
			    (copy_from_user(tempbuffer, tempsrc, indata_size)))
			{
				vfree(tempbuffer);
				return -EFAULT;	//there return -EFAULT, it different from enum load_unload_flag
			}
			load_flag = *tempbuffer;
			vfree(tempbuffer);
		}
		pr_debug("load flag:%d\n", load_flag);
		return load_flag;
	} else {
		return XRP_NOT_LOAD_UNLOAD;
	}
}

int32_t xrp_library_setall_missstate(struct xvp * xvp)
{
	unsigned long bkt;
	struct xrp_known_file *p;
	struct loadlib_info *libinfo;
	struct xvp_file *xvp_file;

	mutex_lock(&xvp->xrp_known_files_lock);
	hash_for_each(xvp->xrp_known_files, bkt, p, node) {
		xvp_file =
		    (struct xvp_file *)(((struct file *)(p->filp))->
					private_data);
		list_for_each_entry(libinfo, &xvp_file->load_lib_list,
				    node_libinfo) {
			libinfo->lib_state = XRP_LIBRARY_MISSED_STATE;
		}
	}
	mutex_unlock(&xvp->xrp_known_files_lock);
	return LIB_RESULT_OK;
}

/*
* Statistics load count
* curfilecnt: current file load count
* totalcount: all load count in all files.
* filp: current file
* comment: through foreach all file to read lib name load count to Statistics
*/
static uint32_t xrp_library_get_loadcount(struct file *filp,
					  const char *libname,
					  uint32_t * curfilecnt,
					  uint32_t * totalcount)
{
	unsigned long bkt;
	struct xrp_known_file *p;
	struct loadlib_info *libinfo;
	struct xvp_file *xvp_file = (struct xvp_file *)(filp->private_data);
	struct xvp *xvp = xvp_file->xvp;

	*curfilecnt = 0;
	*totalcount = 0;

	pr_debug("[IN]check lib name:%s\n", libname);
	mutex_lock(&xvp->xrp_known_files_lock);
	hash_for_each(xvp->xrp_known_files, bkt, p, node) {
		pr_debug("check in each file\n");
		xvp_file =
		    (struct xvp_file *)(((struct file *)(p->filp))->
					private_data);
		list_for_each_entry(libinfo, &xvp_file->load_lib_list,
				    node_libinfo) {
			if (0 == strcmp(libinfo->libname, libname)) {
				pr_debug("libname:%s, loadcount:%d\n", libname,
					 libinfo->load_count);
				(*totalcount) += libinfo->load_count;
				if (p->filp == filp) {
					(*curfilecnt) += libinfo->load_count;
				}
			}
		}
	}
	mutex_unlock(&xvp->xrp_known_files_lock);
	pr_debug
	    ("[OUT]statistics lib load status, libname:%s, curfilecnt:%d, totalcount:%d\n",
	     libname, *curfilecnt, *totalcount);
	return LIB_RESULT_OK;
}

/*
* check current lib name whether store in xvp file,
* if yes-> counter++,
* else alloc one libinfo struct(from inlibinfo), add store into xvp_file list.
*/
static int32_t xrp_library_increase(struct file *filp, const char *libname,
				    struct loadlib_info *inlibinfo)
{
	struct loadlib_info *libinfo = NULL;
	struct loadlib_info *newlibinfo = NULL;
	struct xvp_file *xvptemp_file;
	struct xvp_file *xvp_file = (struct xvp_file *)(filp->private_data);
	uint32_t find = 0;
	unsigned long bkt;
	struct xvp *xvp = xvp_file->xvp;
	struct xrp_known_file *p;

	list_for_each_entry(libinfo, &xvp_file->load_lib_list, node_libinfo) {
		if (0 == strcmp(libname, libinfo->libname)) {
			libinfo->load_count++;
			find = 1;
			pr_debug
			    ("libname:%s find is 1 ,and only add load_count\n",
			     libinfo->libname);
			break;
		}
	}
	if (1 == find) {
		pr_debug("loadcount:%d, libname:%s\n", libinfo->load_count,
			 libname);
		return libinfo->load_count;
	}
	mutex_lock(&xvp->xrp_known_files_lock);
	hash_for_each(xvp->xrp_known_files, bkt, p, node) {
		find = 0;
		xvptemp_file =
		    (struct xvp_file *)(((struct file *)(p->filp))->
					private_data);
		if (xvp_file == xvptemp_file) {
			list_for_each_entry(libinfo, &xvp_file->load_lib_list,
					    node_libinfo) {
				if (0 == strcmp(libname, libinfo->libname)) {
					libinfo->load_count++;
					find = 1;
					break;
				}
			}
			if ((0 == find) && (inlibinfo != NULL)) {
				newlibinfo = libinfo_alloc_element();
				pr_debug
				    ("alloc new element xvpfile:%x, libname:%s\n",
				     xvp_file, inlibinfo->libname);
				if (newlibinfo) {
					find = 1;
					memcpy(newlibinfo, inlibinfo,
					       sizeof(struct loadlib_info));
					newlibinfo->load_count = 1;
					newlibinfo->original_flag = 0;
					newlibinfo->lib_state =
					    XRP_LIBRARY_LOADED;
					newlibinfo->lib_processing_count = 0;
					mutex_init(&newlibinfo->mutex);
					list_add_tail(&newlibinfo->node_libinfo,
						      &xvp_file->load_lib_list);
				}
			} else {
				pr_err("find is:%d , inlibinfo:%x\n", find,
				       inlibinfo);
			}
		}
	}
	mutex_unlock(&xvp->xrp_known_files_lock);
	if (0 == find) {
		return -EFAULT;
	}
	return 0;
}

/*
* check none current file load lib count,
* bellow count is only load or not, not the sum of load counter
* whether it can use one global load total counter, to record it (atomic_t counter)
*/
static uint32_t library_check_otherfile_count(struct file *filp,
					      const char *libname)
{
	struct loadlib_info *libinfo = NULL;
	struct loadlib_info *temp;
	unsigned long bkt;
	uint32_t count = 0;
	struct xrp_known_file *p;
	struct xvp_file *xvp_file;
	struct xvp *xvp = NULL;
	struct xvp_file *xvp_file_curr =
	    (struct xvp_file *)(filp->private_data);

	xvp = xvp_file_curr->xvp;
	hash_for_each(xvp->xrp_known_files, bkt, p, node) {
		xvp_file =
		    (struct xvp_file *)(((struct file *)(p->filp))->
					private_data);
		if (xvp_file_curr != xvp_file) {
			list_for_each_entry_safe(libinfo, temp,
						 &xvp_file->load_lib_list,
						 node_libinfo) {
				if (strcmp(libinfo->libname, libname) == 0) {
					count++;
				}
			}
		}
	}
	pr_debug("count is:%d\n", count);
	return count;
}

/*
* xrp_library_decrelease, name error,its name should change to decrease(increase)
* decrease one lib record in  current file.
* if no other has this lib, release this lib memory which alloc in xrp_library_load_internal
* no need return value.
*/
int32_t xrp_library_decrelease(struct file * filp, const char *libname)
{
	int32_t find = 0;
	int32_t release = 0;
	struct loadlib_info *libinfo = NULL;
	struct loadlib_info *temp = NULL;
	struct xvp_file *xvp_file = (struct xvp_file *)(filp->private_data);
	struct xvp *xvp = xvp_file->xvp;

	/*decrease load_count */
	list_for_each_entry_safe(libinfo, temp, &xvp_file->load_lib_list,
				 node_libinfo) {
		if (0 == strcmp(libinfo->libname, libname)) {
			find = 1;
			if (libinfo->load_count > 0)
				libinfo->load_count--;
			if (libinfo->load_count == 0) {
				mutex_lock(&xvp->xrp_known_files_lock);
#ifdef IOMMUANDMEM
				if (library_check_otherfile_count(filp, libname)
				    == 0) {
					xvp_buf_kunmap(xvp,
						       xvp_buf_get_by_id(xvp,
									 libinfo->
									 handle));
					xvp_buf_free_with_iommu(xvp,
								xvp_buf_get_by_id
								(xvp,
								 libinfo->
								 handle));
					xvp_buf_kunmap(xvp,
						       xvp_buf_get_by_id(xvp,
									 libinfo->
									 pil_handle));
					xvp_buf_free_with_iommu(xvp,
								xvp_buf_get_by_id
								(xvp,
								 libinfo->
								 pil_handle));
				}
#else
				if (library_check_otherfile_count(filp, libname)
				    == 0) {
					xvp->vdsp_mem_desc->ops->
					    mem_kunmap(xvp->vdsp_mem_desc,
						       libinfo->ionhandle);
					xvp->vdsp_mem_desc->ops->
					    mem_iommu_unmap(xvp->vdsp_mem_desc,
							    libinfo->ionhandle,
							    IOMMU_ALL);
					xvp->vdsp_mem_desc->ops->mem_free(xvp->
									  vdsp_mem_desc,
									  libinfo->
									  ionhandle);
					vfree(libinfo->ionhandle);
					xvp->vdsp_mem_desc->ops->
					    mem_kunmap(xvp->vdsp_mem_desc,
						       libinfo->pil_ionhandle);
					xvp->vdsp_mem_desc->ops->
					    mem_iommu_unmap(xvp->vdsp_mem_desc,
							    libinfo->
							    pil_ionhandle,
							    IOMMU_ALL);
					xvp->vdsp_mem_desc->ops->mem_free(xvp->
									  vdsp_mem_desc,
									  libinfo->
									  pil_ionhandle);
					vfree(libinfo->pil_ionhandle);
				}
#endif
				mutex_unlock(&xvp->xrp_known_files_lock);
				pr_debug("libname:%s ok original_flag:%d\n",
					 libname, libinfo->original_flag);
				/*remove this lib element */
				list_del(&libinfo->node_libinfo);
				vfree(libinfo);
				release = 1;
			} else {
				pr_debug("warning libname:%s loadcount:%d\n",
					 libname, libinfo->load_count);
			}
			break;
		}
	}
	if (1 == find && 1 == release) {
		return LIB_RESULT_OK;
	} else {
		pr_err
		    ("[ERROR]not find lib [%s] or not release, find:%d, release:%d\n",
		     libname, find, release);
		return LIB_RESULT_ERROR;
	}
}

/*
* get which lib  to unload,
* this info is stored in unload cmd->indata (byte0: flag-load/unload; byte1:nsid)
* it is no use to check flag
*/
static int32_t xrp_library_getloadunload_libname(struct xvp *xvp,
						 struct xrp_request *rq,
						 char *outlibname,
						 uint32_t krqflag)
{
	__u32 indata_size;
	int32_t ret = LIB_RESULT_OK;
	void *tempsrc = NULL;
	__u8 *tempbuffer = NULL;

	indata_size = rq->ioctl_queue.in_data_size;

	if (likely(0 == strcmp(rq->nsid, LIBRARY_LOAD_UNLOAD_NSID))) {
		/*check libname */
		if (indata_size > XRP_DSP_CMD_INLINE_DATA_SIZE)
			tempsrc = (void *)(rq->ioctl_queue.in_data_addr);
		else
			tempsrc = (void *)(rq->in_data);
		pr_debug("before copy_from_user src:%p, dst:%p\n", tempsrc,
			 tempbuffer);
		if (krqflag == 1) {
			snprintf(outlibname, XRP_NAMESPACE_ID_SIZE, "%s",
				 ((char *)(rq->ioctl_queue.in_data_addr) + 1));
		} else {
			tempbuffer = vmalloc(indata_size);
			if (unlikely(tempbuffer == NULL)) {
				return -ENOMEM;
			}
			memset(tempbuffer, 0, indata_size);
			if (unlikely
			    (copy_from_user(tempbuffer, tempsrc, indata_size)))
			{
				pr_err("[ERROR]copy from user failed\n");
				ret = -EINVAL;
			} else {
				snprintf(outlibname, XRP_NAMESPACE_ID_SIZE,
					 "%s", tempbuffer + 1);
			}
			vfree(tempbuffer);
		}
	} else {
		ret = -EINVAL;
	}
	pr_debug("outlibname:%s, ret:%d\n", outlibname, ret);

	return ret;
}

#ifdef IOMMUANDMEM
static int32_t xrp_library_unload_prepare(struct file *filp,
					  struct xrp_request *rq, char *libname,
					  uint32_t krqflag)
{
	int ret = LIB_RESULT_OK;
	__u32 indata_size;
	__u8 *inputbuffer = NULL;
	struct loadlib_info *libinfo = NULL;
	struct xvp_file *xvp_file = filp->private_data;

	pr_debug("[IN]lib unload, nsid[%s]\n", rq->nsid);
	if (krqflag == 1) {
		pr_debug("[OUT]lib unload krqflag is 1, nsid[%s]\n", rq->nsid);
		return ret;
	}
	indata_size = rq->ioctl_queue.in_data_size;
	if (likely(0 == strcmp(rq->nsid, LIBRARY_LOAD_UNLOAD_NSID)
		   && (indata_size >= LIBRARY_CMD_LOAD_UNLOAD_INPUTSIZE))) {
		libinfo = xrp_library_getlibinfo(filp, libname);
		if (likely(libinfo != NULL)) {
			ret = xvpfile_buf_kmap(xvp_file, rq->in_buf);
			if (ret) {
				pr_err("Error: xvpfile_buf_kmap failed\n");
				return -EFAULT;
			}
			inputbuffer = (__u8 *) rq->in_buf->vaddr;
			*((unsigned int *)((__u8 *) inputbuffer +
					   LIBRARY_CMD_PIL_INFO_OFFSET)) =
			    libinfo->pil_info;
			wmb();
			xvpfile_buf_kunmap(xvp_file, rq->in_buf);
		} else {
			pr_err("[ERROR]libinfo null\n");
			ret = -EINVAL;
		}
	} else {
		pr_err("[ERROR]nsid is not unload\n");
		ret = -EINVAL;
	}
	pr_debug("unload nsid[%s], ret[%d]\n", rq->nsid, ret);

	return ret;
}

/*
* load prepare
* 1. check lib whwther loaded,
* 2.xrp_library_request_resource.
* 3. read lib info to build load cmd.
* return value 0 is need load, 1 is loaded already
*/
static int32_t xrp_library_load_prepare(struct file *filp,
					struct xrp_request *rq,
					char *outlibname,
					struct loadlib_info **outlibinfo)
{
	__u8 load_flag = 0;	//it must Load flag, no check need.
	__u8 *tempbuffer = NULL;	//it is same as input_ptr, one para is enough.
	__u8 *input_ptr = NULL;	//user input data.
	__u8 *libbuffer = NULL;
	__u8 *inputbuffer = NULL;	//kernel lib input data, it should survive during the existence of lib
	char libname[XRP_NAMESPACE_ID_SIZE];

	__u32 indata_size;
	int32_t ret = LIB_RESULT_OK;
	uint32_t loaded = 0;
	void *tempsrc = NULL;
	struct loadlib_info *libinfo = NULL;
	struct xvp_file *xvp_file = filp->private_data;
	struct xvp_buf *buf = NULL;

	*outlibinfo = NULL;
	indata_size = rq->ioctl_queue.in_data_size;
	/*check whether load cmd */
	if (0 == strcmp(rq->nsid, LIBRARY_LOAD_UNLOAD_NSID)) {
		/*check libname */
		if (indata_size > XRP_DSP_CMD_INLINE_DATA_SIZE)
			tempsrc = (void *)(rq->ioctl_queue.in_data_addr);
		else
			tempsrc = (void *)(rq->in_data);

		tempbuffer = vmalloc(indata_size);
		if (unlikely(tempbuffer == NULL)) {
			pr_err("[ERROR]vmalloc failed\n");
			return -ENOMEM;
		}
		memset(tempbuffer, 0, indata_size);
		if (unlikely(copy_from_user(tempbuffer, tempsrc, indata_size))) {
			pr_err("[ERROR]copy from user failed\n");
			vfree(tempbuffer);
			return -EFAULT;
		}
		input_ptr = tempbuffer;
		/*input_vir first byte is load or unload */
		load_flag = *input_ptr;

		if (XRP_LOAD_LIB_FLAG == load_flag) {
			/*load */
			snprintf(libname, XRP_NAMESPACE_ID_SIZE, "%s",
				 input_ptr + 1);
			/*check whether loaded */
			snprintf(outlibname, XRP_NAMESPACE_ID_SIZE, "%s",
				 libname);
			loaded =
			    xrp_check_whether_loaded(filp, libname, outlibinfo);
			if (loaded) {
				pr_debug
				    ("lib[%s] already loaded, not need reload\n",
				     libname);
				ret = 1;	/*loaded */
			} else {
				/*not loaded alloc libinfo node ,load internal */
				buf =
				    xvpfile_buf_get(xvp_file,
						    rq->id_dsp_pool[0]);
				ret = xvpfile_buf_kmap(xvp_file, buf);
				if (ret) {
					vfree(tempbuffer);
					pr_err
					    ("Error: xvpfile_buf_kmap failed\n");
					return -EFAULT;
				}
				libbuffer = (__u8 *) xvpfile_buf_get_vaddr(buf);
				pr_debug
				    ("before xrp_library_load_internal libname:%s\n",
				     libname);
				ret =
				    xrp_library_load_internal(filp, libbuffer,
							      libname);
				if (unlikely(ret != 0)) {
					pr_err
					    ("[ERROR]xrp_library_load_internal ret:%d\n",
					     ret);
					xvpfile_buf_kunmap(xvp_file, buf);
					vfree(tempbuffer);
					ret = -ENOMEM;
					return ret;
				}
				xvpfile_buf_kunmap(xvp_file, buf);

				/*re edit rq for register libname, input data: input[0] load unload flag
				   input[1] ~input[32] --- libname , input[LIBRARY_CMD_PIL_INFO_OFFSET]~input[43]
				   ---- libinfo addr */
				libinfo = xrp_library_getlibinfo(filp, libname);
				if (unlikely(libinfo == NULL)) {
					pr_err
					    ("[ERROR]xrp_library_getlibinfo NULL\n");
					xrp_library_decrelease(filp, libname);
					vfree(tempbuffer);
					ret = -ENOMEM;
					return ret;
				} else {
					*((uint32_t *) (input_ptr +
							LIBRARY_CMD_PIL_INFO_OFFSET))
					    = libinfo->pil_info;
					pr_debug
					    ("nsid:%s, loadflag:%d, libname:%s,"
					     "pil_info:%x,indata_size:%d\n",
					     rq->nsid, load_flag, libname,
					     libinfo->pil_info, indata_size);
				}
				ret = xvpfile_buf_kmap(xvp_file, rq->in_buf);
				if (ret) {
					xrp_library_decrelease(filp, libname);
					vfree(tempbuffer);
					pr_err
					    ("Error: xvpfile_buf_kmap failed\n");
					return -EFAULT;
				}
				inputbuffer = (__u8 *) rq->in_buf->vaddr;
				memcpy(inputbuffer, tempbuffer, indata_size);
				wmb();
				xvpfile_buf_kunmap(xvp_file, rq->in_buf);
			}
		} else {
			pr_err("[ERROR]not load flag\n");
			ret = -EINVAL;
		}
		vfree(tempbuffer);
		return ret;
	} else {
		return 0;
	}
}

int32_t xrp_library_release_all(struct xvp * xvp)
{
	struct loadlib_info *libinfo = NULL;
	struct loadlib_info *temp;
	unsigned long bkt;
	struct xrp_known_file *p;
	struct xvp_file *xvp_file;

	mutex_lock(&xvp->xrp_known_files_lock);
	hash_for_each(xvp->xrp_known_files, bkt, p, node) {
		pr_debug("known file-p:%lx\n", (unsigned long)p);
		xvp_file =
		    (struct xvp_file *)(((struct file *)(p->filp))->
					private_data);
		list_for_each_entry_safe(libinfo, temp,
					 &xvp_file->load_lib_list,
					 node_libinfo) {
			pr_debug
			    ("list_for_each_entry libinfo:%lx, libname:%s\n",
			     (unsigned long)libinfo, libinfo->libname);
			if (likely(NULL != libinfo)) {
				if (library_check_otherfile_count
				    (p->filp, libinfo->libname) == 0) {
					xvp_buf_kunmap(xvp,
						       xvp_buf_get_by_id
						       (xvp_file->xvp,
							libinfo->handle));
					xvp_buf_free_with_iommu(xvp_file->xvp,
								xvp_buf_get_by_id
								(xvp_file->xvp,
								 libinfo->
								 handle));
					xvp_buf_kunmap(xvp,
						       xvp_buf_get_by_id
						       (xvp_file->xvp,
							libinfo->pil_handle));
					xvp_buf_free_with_iommu(xvp_file->xvp,
								xvp_buf_get_by_id
								(xvp_file->xvp,
								 libinfo->
								 pil_handle));
				}
				pr_debug
				    ("release ion handle, pil handle current:%p, original_flag:%d\n",
				     get_current(), libinfo->original_flag);
				list_del(&libinfo->node_libinfo);
				vfree(libinfo);
			}
		}
	}
	mutex_unlock(&xvp->xrp_known_files_lock);

	return LIB_RESULT_OK;
}
#else
static int32_t xrp_library_unload_prepare(struct file *filp,
					  struct xrp_request *rq, char *libname,
					  uint32_t krqflag)
{
	int ret = LIB_RESULT_OK;
	__u32 indata_size;
	__u8 *inputbuffer = NULL;
	struct loadlib_info *libinfo = NULL;
	struct xvp *xvp = ((struct xvp_file *)(filp->private_data))->xvp;

	pr_debug("[IN]lib unload, nsid[%s]\n", rq->nsid);
	if (krqflag == 1) {
		pr_debug("[OUT]lib unload krqflag is 1, nsid[%s]\n", rq->nsid);
		return ret;
	}
	indata_size = rq->ioctl_queue.in_data_size;
	if (likely(0 == strcmp(rq->nsid, LIBRARY_LOAD_UNLOAD_NSID)
		   && (indata_size >= LIBRARY_CMD_LOAD_UNLOAD_INPUTSIZE))) {
		libinfo = xrp_library_getlibinfo(filp, libname);
		if (likely(libinfo != NULL)) {
			ret =
			    xvp->vdsp_mem_desc->ops->mem_kmap_userbuf(&rq->
								      ion_in_buf);
			if (unlikely(ret != 0)) {
				pr_err("[ERROR]kmap failed ret:%d\n", ret);
				return -EFAULT;
			}
			inputbuffer =
			    (__u8 *) ((struct ion_buf *)(&rq->ion_in_buf))->
			    addr_k;
			*((unsigned int *)((__u8 *) inputbuffer +
					   LIBRARY_CMD_PIL_INFO_OFFSET)) =
			    libinfo->pil_info;
			xvp->vdsp_mem_desc->ops->mem_kunmap_userbuf(&rq->
								    ion_in_buf);
			wmb();
		} else {
			pr_err("[ERROR]libinfo null\n");
			ret = -EINVAL;
		}
	} else {
		pr_err("[ERROR]nsid is not unload\n");
		ret = -EINVAL;
	}
	pr_debug("unload nsid[%s], ret[%d]\n", rq->nsid, ret);

	return ret;
}

/*
* load prepare
* 1. check lib whwther loaded,
* 2.xrp_library_request_resource.
* 3. read lib info to build load cmd.
* return value 0 is need load, 1 is loaded already
*/
static int32_t xrp_library_load_prepare(struct file *filp,
					struct xrp_request *rq,
					char *outlibname,
					struct loadlib_info **outlibinfo)
{
	__u8 load_flag = 0;	//it must Load flag, no check need.
	__u8 *tempbuffer = NULL;	//it is same as input_ptr, one para is enough.
	__u8 *input_ptr = NULL;	//user input data.
	__u8 *libbuffer = NULL;
	__u8 *inputbuffer = NULL;	//kernel lib input data, it should survive during the existence of lib
	char libname[XRP_NAMESPACE_ID_SIZE];

	__u32 indata_size;
	int32_t ret = LIB_RESULT_OK;
	uint32_t loaded = 0;
	void *tempsrc = NULL;
	struct loadlib_info *libinfo = NULL;
	struct xvp *xvp = ((struct xvp_file *)(filp->private_data))->xvp;	//file link xvp file->link xvp

	*outlibinfo = NULL;
	indata_size = rq->ioctl_queue.in_data_size;
	/*check whether load cmd */
	if (0 == strcmp(rq->nsid, LIBRARY_LOAD_UNLOAD_NSID)) {
		/*check libname */
		if (indata_size > XRP_DSP_CMD_INLINE_DATA_SIZE)
			tempsrc = (void *)(rq->ioctl_queue.in_data_addr);
		else
			tempsrc = (void *)(rq->in_data);

		tempbuffer = vmalloc(indata_size);
		if (unlikely(tempbuffer == NULL)) {
			pr_err("[ERROR]vmalloc failed\n");
			return -ENOMEM;
		}
		memset(tempbuffer, 0, indata_size);
		if (unlikely(copy_from_user(tempbuffer, tempsrc, indata_size))) {
			pr_err("[ERROR]copy from user failed\n");
			vfree(tempbuffer);
			return -EFAULT;
		}
		input_ptr = tempbuffer;
		/*input_vir first byte is load or unload */
		load_flag = *input_ptr;

		if (XRP_LOAD_LIB_FLAG == load_flag) {
			/*load */
			snprintf(libname, XRP_NAMESPACE_ID_SIZE, "%s",
				 input_ptr + 1);
			/*check whether loaded */
			snprintf(outlibname, XRP_NAMESPACE_ID_SIZE, "%s",
				 libname);
			loaded =
			    xrp_check_whether_loaded(filp, libname, outlibinfo);
			if (loaded) {
				pr_debug
				    ("lib[%s] already loaded, not need reload\n",
				     libname);
				ret = 1;	/*loaded */
			} else {
				/*not loaded alloc libinfo node ,load internal */
				ret =
				    xvp->vdsp_mem_desc->ops->
				    mem_kmap_userbuf(rq->ion_dsp_pool);
				if (ret != 0) {
					pr_err("[ERROR]kmap ionbuf failed\n");
					vfree(tempbuffer);
					return -EFAULT;
				}
				libbuffer = (__u8 *) (rq->ion_dsp_pool->addr_k);
				pr_debug
				    ("before xrp_library_load_internal libname:%s\n",
				     libname);
				ret =
				    xrp_library_load_internal(filp, libbuffer,
							      libname);
				if (unlikely(ret != 0)) {
					pr_err
					    ("[ERROR]xrp_library_load_internal ret:%d\n",
					     ret);
					xvp->vdsp_mem_desc->
					    ops->mem_kunmap_userbuf(rq->
								    ion_dsp_pool);
					vfree(tempbuffer);
					ret = -ENOMEM;
					return ret;
				}
				xvp->vdsp_mem_desc->ops->mem_kunmap_userbuf(rq->
									    ion_dsp_pool);
				/*re edit rq for register libname, input data: input[0] load unload flag
				   input[1] ~input[32] --- libname , input[LIBRARY_CMD_PIL_INFO_OFFSET]~input[43]
				   ---- libinfo addr */
				libinfo = xrp_library_getlibinfo(filp, libname);
				if (unlikely(libinfo == NULL)) {
					pr_err
					    ("[ERROR]xrp_library_getlibinfo NULL\n");
					xrp_library_decrelease(filp, libname);
					vfree(tempbuffer);
					ret = -ENOMEM;
					return ret;
				} else {
					*((uint32_t *) (input_ptr +
							LIBRARY_CMD_PIL_INFO_OFFSET))
					    = libinfo->pil_info;
					pr_debug
					    ("nsid:%s, loadflag:%d, libname:%s,"
					     "pil_info:%x,indata_size:%d\n",
					     rq->nsid, load_flag, libname,
					     libinfo->pil_info, indata_size);
				}
				ret =
				    xvp->vdsp_mem_desc->ops->
				    mem_kmap_userbuf(&rq->ion_in_buf);
				if (unlikely(ret != 0)) {
					pr_err("[ERROR]kmap ionbuf failed\n");
					vfree(tempbuffer);
					xrp_library_decrelease(filp, libname);
					return -EFAULT;
				}
				inputbuffer =
				    (__u8
				     *) ((struct ion_buf *)(&rq->ion_in_buf))->
				    addr_k;
				memcpy(inputbuffer, tempbuffer, indata_size);
				xvp->vdsp_mem_desc->ops->
				    mem_kunmap_userbuf(&rq->ion_in_buf);
				wmb();
			}
		} else {
			pr_err("[ERROR]not load flag\n");
			ret = -EINVAL;
		}
		vfree(tempbuffer);
		return ret;
	} else {
		return 0;
	}
}

int32_t xrp_library_release_all(struct xvp * xvp)
{
	struct loadlib_info *libinfo = NULL;
	struct loadlib_info *temp;
	unsigned long bkt;
	struct xrp_known_file *p;
	struct xvp_file *xvp_file;

	mutex_lock(&xvp->xrp_known_files_lock);
	hash_for_each(xvp->xrp_known_files, bkt, p, node) {
		pr_debug("known file-p:%lx\n", (unsigned long)p);
		xvp_file =
		    (struct xvp_file *)(((struct file *)(p->filp))->
					private_data);
		list_for_each_entry_safe(libinfo, temp,
					 &xvp_file->load_lib_list,
					 node_libinfo) {
			pr_debug
			    ("list_for_each_entry libinfo:%lx, libname:%s\n",
			     (unsigned long)libinfo, libinfo->libname);
			if (likely(NULL != libinfo)) {
				if (library_check_otherfile_count
				    (p->filp, libinfo->libname) == 0) {
					xvp->vdsp_mem_desc->ops->
					    mem_kunmap(xvp->vdsp_mem_desc,
						       libinfo->ionhandle);
					xvp->vdsp_mem_desc->ops->
					    mem_iommu_unmap(xvp->vdsp_mem_desc,
							    libinfo->ionhandle,
							    IOMMU_ALL);
					xvp->vdsp_mem_desc->ops->mem_free(xvp->
									  vdsp_mem_desc,
									  libinfo->
									  ionhandle);
					vfree(libinfo->ionhandle);
					xvp->vdsp_mem_desc->ops->
					    mem_kunmap(xvp->vdsp_mem_desc,
						       libinfo->pil_ionhandle);
					xvp->vdsp_mem_desc->ops->
					    mem_iommu_unmap(xvp->vdsp_mem_desc,
							    libinfo->
							    pil_ionhandle,
							    IOMMU_ALL);
					xvp->vdsp_mem_desc->ops->mem_free(xvp->
									  vdsp_mem_desc,
									  libinfo->
									  pil_ionhandle);
					vfree(libinfo->pil_ionhandle);
				}
				pr_debug
				    ("release ion handle, pil handle current:%p, original_flag:%d\n",
				     get_current(), libinfo->original_flag);
				list_del(&libinfo->node_libinfo);
				vfree(libinfo);
			}
		}
	}
	mutex_unlock(&xvp->xrp_known_files_lock);

	return LIB_RESULT_OK;
}
#endif

/*
* 1.load, check whether load before, if yes->add count, or add node / realy load
* 2.unload ,check whether last one load, if yes->delete node, release load, or decrease count
* 3. common cmd, record which lib running
* return value 0 is ok, other value is fail or no need process continue
*/
int32_t xrp_pre_process_request(struct file * filp,
				struct xrp_request * rq,
				enum load_unload_flag loadflag, char *libname,
				uint32_t krqflag)
{
	int32_t lib_result;
	struct loadlib_info *libinfo = NULL;
	struct xvp *xvp = ((struct xvp_file *)(filp->private_data))->xvp;

	if (loadflag == XRP_LOAD_LIB_FLAG) {
		lib_result =
		    xrp_library_load_prepare(filp, rq, libname, &libinfo);
		if (unlikely(0 != lib_result)) {
			/*has loaded needn't reload */
			if (unlikely(lib_result != 1)) {
				pr_err("[ERROR]result:%d\n", lib_result);
				return -EFAULT;
			} else {
				pr_warn
				    ("[WARN]already loaded needn't reload\n");
				/*increase */
				xrp_library_increase(filp, libname, libinfo);
				return -EEXIST;
			}
		} else {
			/*re-edit the rq for register */
			pr_debug("Load libname:%s\n", libname);
			return LIB_RESULT_OK;
		}
	} else if (loadflag == XRP_UNLOAD_LIB_FLAG) {
		lib_result =
		    xrp_library_getloadunload_libname(xvp, rq, libname,
						      krqflag);
		if (likely(lib_result == 0)) {
			uint32_t curfilecnt, totalcount;

			xrp_library_get_loadcount(filp, libname, &curfilecnt,
						  &totalcount);
			if (curfilecnt == 1) {
				if (totalcount == 1) {
					if (0 !=
					    xrp_library_checkprocessing(filp,
									libname))
					{
						pr_err
						    ("[ERROR]same lib is processing invalid\n");
						return -EINVAL;
					}
					/*if need unload may be modify libinfo addr,
					   only follow the default send cmd */
					lib_result =
					    xrp_library_unload_prepare(filp, rq,
								       libname,
								       krqflag);
					if (unlikely(lib_result != 0)) {
						pr_err
						    ("[ERROR]xrp_library_unload failed:%d\n",
						     lib_result);
						return -EINVAL;
					}
					libinfo =
					    xrp_library_getlibinfo(filp,
								   libname);
					if (likely(libinfo != NULL))
						libinfo->lib_state =
						    XRP_LIBRARY_UNLOADING;
					pr_debug("Unload libname:%s\n",
						 libname);
					return LIB_RESULT_OK;
				} else {
					pr_debug
					    ("curfile cnt is:%d total cnt:%d needn't unload\n",
					     curfilecnt, totalcount);
					xrp_library_decrelease(filp, libname);
					return -EEXIST;
				}
			} else if (curfilecnt > 1) {
				pr_debug("curfile cnt is:%d needn't unload\n",
					 curfilecnt);
				xrp_library_decrelease(filp, libname);
				return -EEXIST;
			} else {
				pr_err
				    ("curfilecnt(abnormal)load count:%d,total:%d,libname:%s\n",
				     curfilecnt, totalcount, libname);
				return -ENXIO;
			}
		} else {
			pr_err("[ERROR]get libname error, libname:%s\n",
			       libname);
			return -EINVAL;
		}
	} else {
		libinfo = xrp_library_getlibinfo(filp, rq->nsid);
		if (libinfo != NULL) {
			mutex_lock(&libinfo->mutex);
			if ((libinfo->lib_state != XRP_LIBRARY_LOADED)
			    && (libinfo->lib_state !=
				XRP_LIBRARY_PROCESSING_CMD)) {
				pr_err
				    ("[ERROR]lib:%s, libstate is:%d not XRP_LIBRARY_LOADED\n",
				     rq->nsid, libinfo->lib_state);
				return -EINVAL;
			}
			/*set processing libname */
			libinfo->lib_processing_count++;
			libinfo->lib_state = XRP_LIBRARY_PROCESSING_CMD;
			pr_debug("lib_processing_count:%d\n",
				 libinfo->lib_processing_count);
			mutex_unlock(&libinfo->mutex);
		} else {
			pr_err("libinfo null\n");
		}
		/*check whether libname unloading state, if unloading return */
		pr_debug("Command libname:%s\n", rq->nsid);
		return LIB_RESULT_OK;
	}
}

/*
* resultflag: cmd run result, ok/deliver fail/timeout busy
* load flag: load, unload, common cmd
*/
int post_process_request(struct file *filp,
			 struct xrp_request *rq,
			 const char *libname, enum load_unload_flag load_flag,
			 int32_t resultflag)
{
	struct loadlib_info *libinfo = NULL;
	int32_t ret = 0;

	pr_debug("[IN]load_flag[%d], resultflag[%d]\n", load_flag, resultflag);
	if (load_flag == XRP_LOAD_LIB_FLAG) {
		if (likely(resultflag == 0)) {
			xrp_library_increase(filp, libname, NULL);
			libinfo = xrp_library_getlibinfo(filp, libname);
			if (likely(libinfo != NULL)) {
				libinfo->lib_state = XRP_LIBRARY_LOADED;
				pr_debug
				    ("libname:%s, libstate XRP_LIBRARY_LOADED\n",
				     libname);
			}
		} else {
			/*load failedd release here */
			xrp_library_decrelease(filp, libname);
			pr_err
			    ("[ERROR]libname:%s, load failed xrp_library_decrelease\n",
			     libname);
			ret = -EFAULT;
		}
	} else if (load_flag == XRP_UNLOAD_LIB_FLAG) {
		if (likely(resultflag == 0)) {
			libinfo = xrp_library_getlibinfo(filp, libname);
			if (likely(libinfo != NULL)) {
				libinfo->lib_state = XRP_LIBRARY_IDLE;
			}
			pr_debug
			    ("libname:%s, libstate XRP_LIBRARY_IDLE libinfo:%p\n",
			     libname, libinfo);
			xrp_library_decrelease(filp, libname);
		} else {
			pr_err("[ERROR]libname:%s, unload failed\n", libname);
			ret = -EFAULT;
		}
	} else {
		/*remove processing lib */
		libinfo = xrp_library_getlibinfo(filp, rq->nsid);
		if (libinfo != NULL) {
			mutex_lock(&libinfo->mutex);
			libinfo->lib_processing_count--;
			pr_debug("post processing count:%d\n",
				 libinfo->lib_processing_count);
			if (libinfo->lib_state != XRP_LIBRARY_PROCESSING_CMD) {
				pr_err
				    ("[ERROR]lib:%s processing cmd, but state error\n",
				     rq->nsid);
				ret = -EINVAL;
			}
			/*set processing libname */
			if (libinfo->lib_processing_count == 0)
				libinfo->lib_state = XRP_LIBRARY_LOADED;
			mutex_unlock(&libinfo->mutex);
		}
		/*set processing lib state */
		pr_debug("lib:%s, process cmd over\n", rq->nsid);
	}

	return ret;
}

#ifdef IOMMUANDMEM
int32_t xrp_create_unload_cmd(struct file * filp, struct loadlib_info * libinfo,
			      struct xrp_unload_cmdinfo * info)
{
	struct xrp_request *rq;
	char *kvaddr;
	const char *libname = libinfo->libname;
	struct xvp_file *xvp_file = filp->private_data;

	char *name = NULL;
	uint64_t size = 0;
	uint32_t heap_type = 0;
	uint32_t attr = 0;
	struct xvp_buf *buf = NULL;
	int ret = 0;

	rq = vmalloc(sizeof(*rq));
	if (unlikely(rq == NULL)) {
		goto rq_failed;
	}
	name = "xvp lib_input_mem buffer";
	size = LIBRARY_CMD_LOAD_UNLOAD_INPUTSIZE;
	heap_type = SPRD_VDSP_MEM_HEAP_TYPE_UNIFIED;
	attr = SPRD_VDSP_MEM_ATTR_WRITECOMBINE;
	buf =
	    xvpfile_buf_alloc_with_iommu(xvp_file, name, size, heap_type, attr);
	if (!buf) {
		pr_err("Error: xvpfile_buf_alloc_with_iommu faild\n");
		goto alloc_lib_input_mem_failed;
	}
	ret = xvpfile_buf_kmap(xvp_file, buf);
	if (ret) {
		pr_err("Error: xvpfile_buf_kmap failed\n");
		goto error_kmap;
	}
	kvaddr = buf->vaddr;
	//set lib name
	memset(rq, 0, sizeof(*rq));
	/*nsid to load/unload nsid */
	snprintf(rq->nsid, XRP_DSP_CMD_NAMESPACE_ID_SIZE, "%s",
		 LIBRARY_LOAD_UNLOAD_NSID);
	kvaddr[0] = XRP_UNLOAD_LIB_FLAG;	/*unload */
	strncpy(kvaddr + 1, libname, 33);	/*libname */
	*((uint32_t *) (kvaddr + 40)) = libinfo->pil_info;
	info->input_kaddr = kvaddr;
	rq->ioctl_queue.flags =
	    (1 ? XRP_QUEUE_FLAG_NSID : 0) | ((2 << XRP_QUEUE_FLAG_PRIO_SHIFT) &
					     XRP_QUEUE_FLAG_PRIO);
	rq->ioctl_queue.in_data_size = LIBRARY_CMD_LOAD_UNLOAD_INPUTSIZE;
	rq->ioctl_queue.in_data_addr = (unsigned long)kvaddr;
	rq->in_buf = buf;
	info->rq = rq;
	info->input_mem_fd = buf->buf_id;
	pr_debug("create unload cmd\n");
	return LIB_RESULT_OK;
error_kmap:
	xvpfile_buf_free_with_iommu(xvp_file, buf);
alloc_lib_input_mem_failed:
	vfree(rq);
rq_failed:
	return -EFAULT;
}

int32_t xrp_free_unload_cmd(struct file * filp,
			    struct xrp_unload_cmdinfo * info)
{
	struct xvp_file *xvp_file = filp->private_data;

	xvpfile_buf_kunmap(xvp_file,
			   xvpfile_buf_get(xvp_file, info->input_mem_fd));
	xvpfile_buf_free_with_iommu(xvp_file,
				    xvpfile_buf_get(xvp_file,
						    info->input_mem_fd));

	vfree(info->rq);
	info->input_mem_fd = 0;
	pr_debug("free unload cmd\n");
	return LIB_RESULT_OK;
}
#else
int32_t xrp_create_unload_cmd(struct file * filp, struct loadlib_info * libinfo,
			      struct xrp_unload_cmdinfo * info)
{
	struct xrp_request *rq;
	struct ion_buf *lib_input_ionmem;
	int ret;
	char *kvaddr;
	const char *libname = libinfo->libname;
	struct xvp *xvp = ((struct xvp_file *)(filp->private_data))->xvp;

	rq = vmalloc(sizeof(*rq));
	if (unlikely(rq == NULL)) {
		goto rq_failed;
	}
	lib_input_ionmem = vmalloc(sizeof(struct ion_buf));
	if (unlikely(lib_input_ionmem == NULL)) {
		goto ion_buf_failed;
	}
	ret = xvp->vdsp_mem_desc->ops->mem_alloc(xvp->vdsp_mem_desc,
						 lib_input_ionmem,
						 ION_HEAP_ID_MASK_SYSTEM,
						 LIBRARY_CMD_LOAD_UNLOAD_INPUTSIZE);
	if (unlikely(ret != 0)) {
		goto alloc_lib_input_ionmem_failed;
	}
	lib_input_ionmem->dev = xvp->dev;
	/*alloc libinfo ion buffer */
	ret =
	    xvp->vdsp_mem_desc->ops->mem_kmap(xvp->vdsp_mem_desc,
					      lib_input_ionmem);
	if (unlikely(ret != 0)) {
		goto kmap_failed;
	}
	kvaddr = (void *)lib_input_ionmem->addr_k;
	ret =
	    xvp->vdsp_mem_desc->ops->mem_iommu_map(xvp->vdsp_mem_desc,
						   lib_input_ionmem, IOMMU_ALL);
	if (unlikely(ret != 0)) {
		goto iommu_map_failed;
	}
	//set lib name
	memset(rq, 0, sizeof(*rq));
	/*nsid to load/unload nsid */
	snprintf(rq->nsid, XRP_DSP_CMD_NAMESPACE_ID_SIZE, "%s",
		 LIBRARY_LOAD_UNLOAD_NSID);
	kvaddr[0] = XRP_UNLOAD_LIB_FLAG;	/*unload */
	strncpy(kvaddr + 1, libname, 33);	/*libname */
	*((uint32_t *) (kvaddr + 40)) = libinfo->pil_info;
	info->input_kaddr = kvaddr;
	rq->ioctl_queue.flags =
	    (1 ? XRP_QUEUE_FLAG_NSID : 0) | ((2 << XRP_QUEUE_FLAG_PRIO_SHIFT) &
					     XRP_QUEUE_FLAG_PRIO);
	rq->ioctl_queue.in_data_size = LIBRARY_CMD_LOAD_UNLOAD_INPUTSIZE;
	rq->ioctl_queue.in_data_addr = (unsigned long)kvaddr;
	rq->in_data_phys = lib_input_ionmem->iova;
	info->rq = rq;
	info->input_ion_handle = lib_input_ionmem;
	pr_debug("create unload cmd\n");
	return LIB_RESULT_OK;
iommu_map_failed:
	xvp->vdsp_mem_desc->ops->mem_iommu_unmap(xvp->vdsp_mem_desc,
						 lib_input_ionmem, IOMMU_ALL);
kmap_failed:
	xvp->vdsp_mem_desc->ops->mem_free(xvp->vdsp_mem_desc, lib_input_ionmem);
alloc_lib_input_ionmem_failed:
	vfree(lib_input_ionmem);
ion_buf_failed:
	vfree(rq);
rq_failed:
	return -EFAULT;
}

int32_t xrp_free_unload_cmd(struct file * filp,
			    struct xrp_unload_cmdinfo * info)
{
	struct ion_buf *lib_input_ionmem;
	struct xvp *xvp = ((struct xvp_file *)(filp->private_data))->xvp;

	lib_input_ionmem = info->input_ion_handle;
	xvp->vdsp_mem_desc->ops->mem_kunmap(xvp->vdsp_mem_desc,
					    lib_input_ionmem);
	xvp->vdsp_mem_desc->ops->mem_iommu_unmap(xvp->vdsp_mem_desc,
						 lib_input_ionmem, IOMMU_ALL);
	xvp->vdsp_mem_desc->ops->mem_free(xvp->vdsp_mem_desc, lib_input_ionmem);
	vfree(info->rq);
	vfree(info->input_ion_handle);
	pr_debug("free unload cmd\n");
	return LIB_RESULT_OK;
}
#endif
