#include <linux/kthread.h>
#include <linux/hashtable.h>
#include "vdsp_dvfs.h"
#include "vdsp_hw.h"
#include "xrp_internal.h"
#include "xvp_main.h"

#define DVFS_MONITOR_CYCLE_TIME   (1000*1000)

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: dvfs %d: %d %s:" \
	fmt, current->pid, __LINE__, __func__

enum vdsp_powerhint_adddec_flag {
	VDSP_POWERHINT_ADD_LEVELCOUNT,
	VDSP_POWERHINT_DEC_LEVELCOUNT,
};

static void vdsp_add_dec_level_tofile(struct file *filp,
				      enum sprd_vdsp_kernel_power_level level,
				      enum vdsp_powerhint_adddec_flag flag)
{
	struct xvp_file *xvp_file = (struct xvp_file *)filp->private_data;

	if (flag == VDSP_POWERHINT_ADD_LEVELCOUNT) {
		xvp_file->powerhint_info.powerhint_count_level[level]++;
	} else if (flag == VDSP_POWERHINT_DEC_LEVELCOUNT) {
		if (unlikely
		    (xvp_file->powerhint_info.powerhint_count_level[level] ==
		     0)) {
			pr_err("err dec level count level count is 0\n");
		} else {
			xvp_file->powerhint_info.powerhint_count_level[level]--;
		}
	} else {
		pr_err("err param flag:%d\n", flag);
	}
	pr_debug("flag:%d,level:%d,count:%d\n",
		 flag, level,
		 xvp_file->powerhint_info.powerhint_count_level[level]);
}

static enum sprd_vdsp_kernel_power_level vdsp_get_current_maxlevel(struct file
								   *filp)
{
	struct xvp *xvp = ((struct xvp_file *)(filp->private_data))->xvp;
	struct xvp_file *xvpfile_temp;
	unsigned long bkt;
	struct xrp_known_file *p;
	int32_t i;
	enum sprd_vdsp_kernel_power_level max_level =
	    SPRD_VDSP_KERNEL_POWERHINT_RESTORE_DVFS;
	enum sprd_vdsp_kernel_power_level temp_level;

	/*check every file */
	mutex_lock(&xvp->xrp_known_files_lock);
	hash_for_each(xvp->xrp_known_files, bkt, p, node) {
		xvpfile_temp =
		    (struct xvp_file *)(((struct file *)(p->filp))->
					private_data);
		temp_level = SPRD_VDSP_KERNEL_POWERHINT_RESTORE_DVFS;
		for (i = SPRD_VDSP_KERNEL_POWERHINT_LEVEL_MAX - 1; i >= 0; i--) {
			if (xvpfile_temp->powerhint_info.
			    powerhint_count_level[i] != 0) {
				temp_level = i;
				break;
			}
		}
		if (temp_level > max_level) {
			max_level = temp_level;
		}
	}
	mutex_unlock(&xvp->xrp_known_files_lock);
	pr_debug("max level:%d\n", max_level);
	return max_level;
}

static enum sprd_vdsp_kernel_powerhint_acquire_release
translate_acquire_release_fromuser(uint32_t acq_rel)
{
	switch (acq_rel) {
	case 0:
		return SPRD_VDSP_KERNEL_POWERHINT_ACQUIRE;
	case 1:
		return SPRD_VDSP_KERNEL_POWERHINT_RELEASE;
	default:
		return SPRD_VDSP_KERNEL_POWERHINT_MAX;
	}
}

int32_t set_dvfs_maxminfreq(void *data, int32_t maxminflag)
{
	struct xvp *xvp = (struct xvp *)data;
	int32_t index;

	if (maxminflag)
		index = xvp->hw_ops->get_maxsupported_level(xvp);
	else
		index = SPRD_VDSP_KERNEL_POWERHINT_LEVEL_0;
	if (xvp->hw_ops->setdvfs)
		xvp->hw_ops->setdvfs(xvp->hw_arg, index);
	return 0;
}

void preprocess_work_piece(void *data)
{
	struct xvp *xvp = (struct xvp *)data;

	mutex_lock(&xvp->dvfs_info.dvfs_lock);
	if (unlikely(xvp->dvfs_info.dvfs_init != 1)) {
		pr_warn("dvfs init:%d\n", xvp->dvfs_info.dvfs_init);
		mutex_unlock(&xvp->dvfs_info.dvfs_lock);
		return;
	}
	mutex_lock(&xvp->dvfs_info.timepiece_lock);
	if (0 == xvp->dvfs_info.workingcount)
		xvp->dvfs_info.piece_starttime = ktime_get();

	xvp->dvfs_info.workingcount++;
	mutex_unlock(&xvp->dvfs_info.timepiece_lock);
	mutex_unlock(&xvp->dvfs_info.dvfs_lock);
}

void postprocess_work_piece(void *data)
{
	ktime_t realstarttime;
	struct xvp *xvp = (struct xvp *)data;

	mutex_lock(&xvp->dvfs_info.dvfs_lock);
	if (unlikely(xvp->dvfs_info.dvfs_init != 1)) {
		pr_warn("dvfs init:%d\n", xvp->dvfs_info.dvfs_init);
		mutex_unlock(&xvp->dvfs_info.dvfs_lock);
		return;
	}
	mutex_lock(&xvp->dvfs_info.timepiece_lock);
	xvp->dvfs_info.workingcount--;
	if (0 == xvp->dvfs_info.workingcount) {
		realstarttime =
		    (ktime_compare(xvp->dvfs_info.piece_starttime,
				   xvp->dvfs_info.starttime) > 0)
		    ? xvp->dvfs_info.piece_starttime : xvp->dvfs_info.starttime;
		xvp->dvfs_info.cycle_totaltime +=
		    ktime_sub(ktime_get(), realstarttime);
	}
	xvp->dvfs_info.piece_starttime = 0;
	pr_debug("workingcount:%d", xvp->dvfs_info.workingcount);
	mutex_unlock(&xvp->dvfs_info.timepiece_lock);
	mutex_unlock(&xvp->dvfs_info.dvfs_lock);
}

int32_t set_powerhint_flag(void *data, int32_t power, uint32_t acq_rel)
{
	struct file *filp = (struct file *)data;
	struct xvp *xvp = ((struct xvp_file *)(filp->private_data))->xvp;
	int cur_max_level;
	enum sprd_vdsp_kernel_power_level level;
	enum sprd_vdsp_kernel_powerhint_acquire_release acquire_release;

	pr_debug("enter\n");
	level = xvp->hw_ops->translate_powerlevel(power);
	if (unlikely((level >= SPRD_VDSP_KERNEL_POWERHINT_LEVEL_MAX) ||
		     (level <= SPRD_VDSP_KERNEL_POWERHINT_RESTORE_DVFS) ||
		     (level > xvp->hw_ops->get_maxsupported_level(xvp)))) {
		pr_err("level:%d is error", level);
		return -EINVAL;
	}
	acquire_release = translate_acquire_release_fromuser(acq_rel);
	if (unlikely(acquire_release == SPRD_VDSP_KERNEL_POWERHINT_MAX)) {
		pr_err("acq_rel:%d is error", acq_rel);
		return -EINVAL;
	}
	mutex_lock(&xvp->dvfs_info.dvfs_lock);
	if (unlikely(xvp->dvfs_info.dvfs_init != 1)) {
		pr_warn("dvfs not init:%d\n", xvp->dvfs_info.dvfs_init);
		mutex_unlock(&xvp->dvfs_info.dvfs_lock);
		return -ESRCH;
	}
	mutex_lock(&xvp->dvfs_info.powerhint_lock);
	if (acquire_release == SPRD_VDSP_KERNEL_POWERHINT_ACQUIRE)
		vdsp_add_dec_level_tofile(filp, level,
					  VDSP_POWERHINT_ADD_LEVELCOUNT);
	else if (SPRD_VDSP_KERNEL_POWERHINT_RELEASE == acquire_release)
		vdsp_add_dec_level_tofile(filp, level,
					  VDSP_POWERHINT_DEC_LEVELCOUNT);

	cur_max_level = vdsp_get_current_maxlevel(filp);
	pr_debug
	    ("acquire_release:%d, level:%d, curr_maxlevel:%d, last_level:%d\n",
	     acquire_release, level, cur_max_level,
	     xvp->dvfs_info.last_powerhint_level);
	if ((cur_max_level != SPRD_VDSP_KERNEL_POWERHINT_RESTORE_DVFS)
	    && (xvp->dvfs_info.last_powerhint_level != cur_max_level)) {
		if (xvp->hw_ops->setdvfs) {
			pr_debug("setdvfs index:%d\n", cur_max_level);
			xvp->hw_ops->setdvfs(xvp->hw_arg, cur_max_level);
			xvp->dvfs_info.last_powerhint_level = cur_max_level;
		}
	} else if (cur_max_level == SPRD_VDSP_KERNEL_POWERHINT_RESTORE_DVFS) {
		xvp->dvfs_info.last_dvfs_index =
		    xvp->dvfs_info.last_powerhint_level;
		xvp->dvfs_info.last_powerhint_level = cur_max_level;
	}
	mutex_unlock(&xvp->dvfs_info.powerhint_lock);
	mutex_unlock(&xvp->dvfs_info.dvfs_lock);
	pr_debug("exit\n");
	return 0;
}

static uint32_t calculate_vdsp_usage(void *data, ktime_t fromtime)
{
	int32_t percent;
	ktime_t current_time = ktime_get();
	struct xvp *xvp = (struct xvp *)data;

	mutex_lock(&xvp->dvfs_info.timepiece_lock);
	if (xvp->dvfs_info.workingcount != 0) {
		/*now some piece may executing */
		if (ktime_compare(xvp->dvfs_info.piece_starttime, fromtime) <=
		    0)
			xvp->dvfs_info.cycle_totaltime =
			    ktime_sub(current_time, fromtime);
		else
			xvp->dvfs_info.cycle_totaltime +=
			    ktime_sub(current_time,
				      xvp->dvfs_info.piece_starttime);
	}
	percent = (xvp->dvfs_info.cycle_totaltime * 100) /
	    ktime_sub(current_time, fromtime);
	pr_debug("cycle_totaltime:%d ms, timeeclapse:%d ms, percent:%d",
		 (int)(xvp->dvfs_info.cycle_totaltime / 1000000),
		 (int)(ktime_sub(current_time, fromtime) / 1000000), percent);
	xvp->dvfs_info.starttime = ktime_get();
	xvp->dvfs_info.cycle_totaltime = 0;
	mutex_unlock(&xvp->dvfs_info.timepiece_lock);
	return percent;
}

static enum sprd_vdsp_kernel_power_level dvfs_recorrect_level(struct xvp *xvp, enum
							      sprd_vdsp_kernel_power_level
							      level)
{
	enum sprd_vdsp_kernel_power_level max_level =
	    xvp->hw_ops->get_maxsupported_level(xvp);
	return (level > max_level) ? max_level : level;
}

static uint32_t calculate_dvfs_index(struct xvp *xvp, uint32_t percent)
{
	enum sprd_vdsp_kernel_power_level level =
	    SPRD_VDSP_KERNEL_POWERHINT_LEVEL_0;
	static uint32_t last_percent = 0;

	if ((last_percent > 50)) {
		if (percent > 50)
			level = xvp->hw_ops->get_maxsupported_level(xvp);
		else if ((percent <= 50) && (percent > 20))
			level =
			    dvfs_recorrect_level(xvp,
						 SPRD_VDSP_KERNEL_POWERHINT_LEVEL_3);
		else
			level =
			    dvfs_recorrect_level(xvp,
						 SPRD_VDSP_KERNEL_POWERHINT_LEVEL_2);
	} else if ((last_percent <= 50) && (last_percent > 20)) {
		if (percent > 50)
			level = xvp->hw_ops->get_maxsupported_level(xvp);
		else if ((percent <= 50) && (percent > 20))
			level =
			    dvfs_recorrect_level(xvp,
						 SPRD_VDSP_KERNEL_POWERHINT_LEVEL_3);
		else
			level =
			    dvfs_recorrect_level(xvp,
						 SPRD_VDSP_KERNEL_POWERHINT_LEVEL_2);
	} else {
		if (percent > 50)
			level = xvp->hw_ops->get_maxsupported_level(xvp);
		else if ((percent <= 50) && (percent > 20))
			level =
			    dvfs_recorrect_level(xvp,
						 SPRD_VDSP_KERNEL_POWERHINT_LEVEL_2);
		else
			level =
			    dvfs_recorrect_level(xvp,
						 SPRD_VDSP_KERNEL_POWERHINT_LEVEL_2);
	}
	last_percent = percent;
	return level;
}

int vdsp_dvfs_thread(void *data)
{
	uint32_t percentage;
	int32_t index;
	long ret = 0;
	int32_t firstcycle = 1;
	struct xvp *xvp = (struct xvp *)data;

	while (!kthread_should_stop()) {
		mutex_lock(&xvp->dvfs_info.powerhint_lock);
		if (SPRD_VDSP_KERNEL_POWERHINT_RESTORE_DVFS ==
		    xvp->dvfs_info.last_powerhint_level) {
			percentage =
			    calculate_vdsp_usage(xvp, xvp->dvfs_info.starttime);
			if (firstcycle == 1) {
				index =
				    xvp->hw_ops->get_maxsupported_level(xvp);
				firstcycle = 0;
			} else {
				/*dvfs set freq */
				index = calculate_dvfs_index(xvp, percentage);
			}
			pr_debug
			    ("percentage:%d, dvfs index:%d, last index:%d\n",
			     percentage, index, xvp->dvfs_info.last_dvfs_index);
			if (index != xvp->dvfs_info.last_dvfs_index) {
				if (xvp->hw_ops->setdvfs) {
					xvp->hw_ops->setdvfs(xvp->hw_arg,
							     index);
					xvp->dvfs_info.last_dvfs_index = index;
				}
			}
		}
		mutex_unlock(&xvp->dvfs_info.powerhint_lock);	//.unlock();
		ret = wait_event_interruptible_timeout(xvp->dvfs_info.wait_q,
						       kthread_should_stop(),
						       msecs_to_jiffies(1000));
		pr_debug("wait_event_interruptible_timeout ret:%ld\n", ret);
	}
	pr_debug("dvfs exit\n");
	return 0;
}

int vdsp_dvfs_init(void *data)
{
	struct xvp *xvp = (struct xvp *)data;

	mutex_lock(&(xvp->dvfs_info.dvfs_lock));
	mutex_init(&xvp->dvfs_info.timepiece_lock);
	mutex_init(&xvp->dvfs_info.powerhint_lock);
	init_waitqueue_head(&xvp->dvfs_info.wait_q);
	xvp->dvfs_info.starttime = ktime_get();
	xvp->dvfs_info.cycle_totaltime = 0;
	xvp->dvfs_info.piece_starttime = 0;
	xvp->dvfs_info.last_powerhint_level =
	    SPRD_VDSP_KERNEL_POWERHINT_RESTORE_DVFS;
	xvp->dvfs_info.last_dvfs_index = 0;
	xvp->hw_ops->get_max_freq(&xvp->dvfs_info.max_freq);
	/*when open init to max freq */
	if (xvp->hw_ops->setdvfs) {
		enum sprd_vdsp_kernel_power_level max_level;

		max_level = xvp->hw_ops->get_maxsupported_level(xvp);
		xvp->hw_ops->setdvfs(xvp->hw_arg, max_level);
		xvp->dvfs_info.last_dvfs_index = max_level;
	}
	xvp->dvfs_info.dvfs_thread = kthread_run(vdsp_dvfs_thread, xvp,
						 "vdsp_dvfs_thread");
	if (IS_ERR(xvp->dvfs_info.dvfs_thread)) {
		pr_err("kthread_run err\n");
		mutex_unlock(&(xvp->dvfs_info.dvfs_lock));
		return -1;
	}
	xvp->dvfs_info.dvfs_init = 1;

	mutex_unlock(&(xvp->dvfs_info.dvfs_lock));
	return 0;
}

void vdsp_dvfs_deinit(void *data)
{
	struct xvp *xvp = (struct xvp *)data;

	mutex_lock(&(xvp->dvfs_info.dvfs_lock));
	if (xvp->dvfs_info.dvfs_thread) {
		kthread_stop(xvp->dvfs_info.dvfs_thread);
		xvp->dvfs_info.dvfs_thread = NULL;
		pr_debug("kthread_stop\n");
	}
	if (xvp->hw_ops->setdvfs)
		xvp->hw_ops->setdvfs(xvp->hw_arg,
				     SPRD_VDSP_KERNEL_POWERHINT_LEVEL_0);
	mutex_destroy(&xvp->dvfs_info.timepiece_lock);
	mutex_destroy(&xvp->dvfs_info.powerhint_lock);
	pr_debug("dvfs deinit exit\n");
	xvp->dvfs_info.dvfs_init = 0;

	mutex_unlock(&(xvp->dvfs_info.dvfs_lock));
	return;
}

void vdsp_release_powerhint(void *data)
{
	struct file *filp = (struct file *)data;
	struct xvp *xvp = ((struct xvp_file *)(filp->private_data))->xvp;
	struct xvp_file *xvp_file = (struct xvp_file *)filp->private_data;
	int32_t i;
	enum sprd_vdsp_kernel_power_level max_level =
	    SPRD_VDSP_KERNEL_POWERHINT_RESTORE_DVFS;

	mutex_lock(&(xvp->dvfs_info.dvfs_lock));
	if (xvp->dvfs_info.dvfs_init != 1) {
		pr_warn("dvfs init:%d\n", xvp->dvfs_info.dvfs_init);
		mutex_unlock(&(xvp->dvfs_info.dvfs_lock));
		return;
	}
	mutex_lock(&xvp->dvfs_info.powerhint_lock);
	/*check every file */
	pr_debug("enter0\n");
	for (i = 0; i < SPRD_VDSP_KERNEL_POWERHINT_LEVEL_MAX; i++)
		xvp_file->powerhint_info.powerhint_count_level[i] = 0;
	max_level = vdsp_get_current_maxlevel(filp);
	if (max_level == SPRD_VDSP_KERNEL_POWERHINT_RESTORE_DVFS) {
		/*restore dvfs */
		xvp->dvfs_info.last_dvfs_index =
		    xvp->dvfs_info.last_powerhint_level;
		xvp->dvfs_info.last_powerhint_level =
		    SPRD_VDSP_KERNEL_POWERHINT_RESTORE_DVFS;
	} else if (max_level != xvp->dvfs_info.last_powerhint_level) {
		if (xvp->hw_ops->setdvfs != NULL) {
			pr_debug("setdvfs index:%d\n", max_level);
			xvp->hw_ops->setdvfs(xvp->hw_arg, max_level);
			xvp->dvfs_info.last_powerhint_level = max_level;
		}
	}
	mutex_unlock(&xvp->dvfs_info.powerhint_lock);
	mutex_unlock(&(xvp->dvfs_info.dvfs_lock));
	return;
}

#ifdef WORKAROUND_USING_SW_DVFS
static void __iomem *mm_clk_config;

struct vdsp_dvfs_table {
	uint32_t whole_index;
	uint32_t voltage;
	uint32_t voltage_index;
	uint32_t clk;
	uint32_t clk_index;
};

static struct vdsp_dvfs_table vdsp_table[8] = {
	{0, 600, 1, 26000000, 0},
	{1, 600, 1, 307200000, 1},
	{2, 600, 1, 512000000, 2},
	{3, 650, 2, 614400000, 3},
	{4, 700, 3, 819200000, 4},
	{5, 750, 4, 1014000000, 5},
	{6, 750, 4, 1014000000, 5},
	{7, 750, 4, 1014000000, 5},
};

static inline void reg_write32_setbit_h(void *addr, u32 v)
{
	volatile u32 value;

	value = __raw_readl(addr);
	value = v | value;
	__raw_writel(value, addr);
}

static inline void reg_write32_clearbit_h(void *addr, u32 v)
{
	volatile u32 value;

	value = __raw_readl(addr);
	value = v & value;
	__raw_writel(value, addr);
}

static int set_clk_index(uint32_t clk_index)
{
	uint32_t value = (clk_index & 0x7);

	reg_write32_setbit_h((void *)(mm_clk_config + 0x28), value);
	pr_debug("[workaround] set[0x30010028]:0x%x", value);
	return 0;
}

static int set_work_index_sw(void *data, uint32_t index)
{
	(void *)data;
	if (index > 7) {
		pr_err("Invalid argument");
		return -EINVAL;
	}
	pr_debug("[workaround] vdsp set index:%d", index);
	mm_clk_config = ioremap(0x30010000, 0x1000);
	set_clk_index(vdsp_table[index].clk_index);
	iounmap(mm_clk_config);
	return 0;
}

static struct dvfs_ops_sw vdsp_dvfs_ops = {
	.setdvfs_index = set_work_index_sw,
};

struct dvfs_ops_sw *get_dvfs_ops_sw(void)
{
	return &vdsp_dvfs_ops;
}
#endif /* #ifdef WORKAROUND_USING_SW_DVFS */
