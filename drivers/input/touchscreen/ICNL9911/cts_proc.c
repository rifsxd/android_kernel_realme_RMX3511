#define LOG_TAG         "Proc"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_test.h"
#include "cts_proc.h"
#include "cts_strerror.h"
#include "cts_oem.h"

#define TP_DIRECTION_PROC_FILENAME    "oppo_tp_limit_enable"

struct cts_proc_data {
	struct proc_dir_entry *tp_direction_proc_entry;
	struct chipone_ts_data *cts_data;
};

struct cts_proc_data *proc_data;

static ssize_t cts_set_tp_direction_switch(struct cts_device *cts_dev, uint8_t tp_direction_switch)
{
	int ret;

	cts_info("set tp direction swtich: %d", tp_direction_switch);

	msleep(30);

	ret = cts_fw_reg_writeb_retry(&proc_data->cts_data->cts_dev, CTS_DEVICE_TP_DIRECTION_SWITCH, tp_direction_switch, 3, 0);
	if (ret) {
		cts_err("set tp direction switch failed %d(%s)",
			ret, cts_strerror(ret));
		return ret;
	}
	
	return ret;
}

static int32_t cts_get_tp_direction_switch(struct cts_device *cts_dev, uint8_t *tp_direction_switch)
{
	int ret = 0;
	
	cts_info("get tp direction swtich");
	
	msleep(30);

	ret = cts_fw_reg_readb_retry(&proc_data->cts_data->cts_dev, CTS_DEVICE_TP_DIRECTION_SWITCH, tp_direction_switch, 3, 0);
	if (ret) {
		cts_err("set tp direction switch failed %d(%s)",
			ret, cts_strerror(ret));
		return ret;
	}

	cts_info("tp_direction_switch = %d", *tp_direction_switch);

	return ret;	
}

static ssize_t cts_tp_direction_proc_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) 
{
	struct cts_device *cts_dev = &proc_data->cts_data->cts_dev;

	static int finished;
	int ret = 0, cnt = 0, len = 0;
	uint8_t tp_direction_switch = 0;
	char tmp_buf[64];

	cts_info("tp direction read");

	if (finished) {
		cts_info("read end!");
		finished = 0;
		return 0;
	}
	finished = 1;

	cts_lock_device(cts_dev);

	cts_get_tp_direction_switch(cts_dev, &tp_direction_switch);

	cts_unlock_device(cts_dev);

	cnt = snprintf(tmp_buf, sizeof(tmp_buf), "tp_direction_switch: %d\n", tp_direction_switch);
	ret = copy_to_user(buf, tmp_buf, sizeof(tmp_buf));
	if (ret) {
		cts_err("copy_to_user() failed %d", ret);
		return ret;
	}
	buf += cnt;
	len += cnt;

	return len;
}

static ssize_t cts_tp_direction_proc_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) 
{
	struct cts_device *cts_dev = &proc_data->cts_data->cts_dev;

	int ret = 0, tmp = 0;
	uint8_t tp_direction_switch = 0;
	char *tmp_buf = NULL;

	cts_info("tp direction write");

	if (count == 0 || count > 2) {
		cts_err("Invalid value! count = %zu", count);
		ret = -EINVAL;
		goto tp_direction_err;
	}

	tmp_buf = kzalloc((count + 1), GFP_KERNEL);
	if (!tmp_buf) {
		cts_err("alloc tmp_buf failed");
		ret = -ENOMEM;
		goto tp_direction_err;
	}

	ret = copy_from_user(tmp_buf, buf, count);
	if (ret) {
		cts_err("copy_from_user() failed %d(%s)", ret, cts_strerror(ret));
		goto tp_direction_err;
	}

	ret = sscanf(tmp_buf, "%d", &tmp);
	if (ret != 1) {
		cts_err("Invalid value! tmp = %d", tmp);
		ret = -EINVAL;
		goto tp_direction_err;
	}

	if (tmp < 0 || tmp > 2) {
		cts_err("Invalid value! tmp = %d; Need echo value: 0 or 1 or 2", tmp);
		ret = -EINVAL;
		goto tp_direction_err;
	}

	tp_direction_switch = tmp;
	cts_info("tp_direction_switch = %d", tp_direction_switch);

	cts_lock_device(cts_dev);

	cts_set_tp_direction_switch(cts_dev, tp_direction_switch);

	cts_unlock_device(cts_dev);

	ret = count;	

tp_direction_err:
	if (tmp_buf)
		kfree(tmp_buf);
	return ret;
}

static const struct file_operations cts_tp_direction_proc_fops = {
	.owner = THIS_MODULE,
	.read = cts_tp_direction_proc_read,
	.write = cts_tp_direction_proc_write,
};

int32_t cts_proc_init(struct chipone_ts_data *cts_data)
{
    int ret;

    if (cts_data == NULL) {
        cts_err("Init with cts_data = NULL");
        return -EINVAL;
    }
	
    cts_info("Init");

    cts_data->proc_data = NULL;
	
    proc_data = kzalloc(sizeof(*proc_data), GFP_KERNEL);
    if (proc_data == NULL) {
        cts_err("Alloc proc data failed");
        return -ENOMEM;
    }

	cts_data->proc_data = proc_data;
	proc_data->cts_data = cts_data;

    cts_info(" - Create '/proc/touchpanel/"TP_DIRECTION_PROC_FILENAME"'");
  
    proc_data->tp_direction_proc_entry =
        proc_create_data(TP_DIRECTION_PROC_FILENAME,
            S_IRUGO, CTS_proc_touchpanel_dir, &cts_tp_direction_proc_fops, cts_data);
    if (proc_data->tp_direction_proc_entry == NULL) {
        cts_err("Create '/proc/touchpanel/"TP_DIRECTION_PROC_FILENAME"' failed");
        ret = -EFAULT;
        goto free_proc_data;
    }

    return 0;

free_proc_data:
    kfree(proc_data);
    return ret;
}

int cts_proc_deinit(struct chipone_ts_data *cts_data)
{
    struct cts_proc_data *proc_data = NULL;

    if (cts_data == NULL) {
        cts_err("Deinit with cts_data = NULL");
        return -EINVAL;
    }

    if (cts_data->proc_data == NULL) {
        cts_warn("Deinit with proc_data = NULL");
        return 0;
    }

    cts_info("Deinit");

    proc_data = cts_data->proc_data;

    if (proc_data->tp_direction_proc_entry) {
        cts_info("  Remove '/proc/touchpanel/"TP_DIRECTION_PROC_FILENAME"'");
        remove_proc_entry(TP_DIRECTION_PROC_FILENAME, NULL);
    }

    kfree(cts_data->proc_data);
    cts_data->proc_data = NULL;

    return 0;
}

