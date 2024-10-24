#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include "vdsp_debugfs.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: debugfs %d: %d %s:" \
	fmt, current->pid, __LINE__, __func__

static struct dentry *root_d;

int vdsp_debugfs_init(struct xrp_debug_para *para)
{
	pr_debug("vdsp_debugfs: initialing...");
	root_d = debugfs_create_dir("sprd_vdsp", NULL);
	if (!root_d) {
		pr_err("vdsp_debugfs: error create root dir");
		return -ENOMEM;
	}
	if (!para) {
		pr_err("vdsp_debugfs: invalid para");
		return -EINVAL;
	}
	debugfs_create_u32("log_mode", 0664, root_d, &para->log_mode);
	debugfs_create_u32("log_level", 0664, root_d, &para->log_level);
	return 0;
}

void vdsp_debugfs_exit(void)
{
	pr_debug("vdsp_debugfs: exiting...");
	if (root_d)
		debugfs_remove_recursive(root_d);
}
