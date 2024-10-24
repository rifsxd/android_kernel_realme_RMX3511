#ifndef __VDSP_DEBUGFS_H__
#define __VDSP_DEBUGFS_H__

struct xrp_debug_para {
	unsigned int log_mode;
	unsigned int log_level;
};
int vdsp_debugfs_init(struct xrp_debug_para *para);
void vdsp_debugfs_exit(void);
#endif
