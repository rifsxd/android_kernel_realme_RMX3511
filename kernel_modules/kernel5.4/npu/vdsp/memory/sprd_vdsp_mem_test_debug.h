#ifndef _SPRD_VDSP_MEM_TEST_DEBUG_
#define _SPRD_VDSP_MEM_TEST_DEBUG_
#include "sprd_vdsp_mem_xvpfile.h"
//debug
extern void debug_xvp_buf_show_all(struct xvp *xvp);
extern void debug_xvpfile_buf_show_all(struct xvp_file *xvp_file);
extern void debug_xvp_buf_print(struct xvp_buf *buf);
extern void debug_buffer_print(struct buffer *buf);
extern void debug_check_xvp_buf_leak(struct xvp_file *xvp_file);
extern char *debug_get_ioctl_cmd_name(unsigned int cmd);
extern void debug_print_xrp_ioctl_queue(struct xrp_ioctl_queue *q);
#endif //_SPRD_VDSP_MEM_TEST_DEBUG_
