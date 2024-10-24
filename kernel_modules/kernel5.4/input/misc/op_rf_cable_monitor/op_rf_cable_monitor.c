// SPDX-License-Identifier: GPL-2.0

/*For OEM project monitor RF cable connection status,
 * and config different RF configuration
 */

#include <linux/export.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/sys_soc.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/kobject.h>
#include <linux/pm_wakeup.h>

#define RF_CABLE "op,rf_cable"
#define RF_CABLE_NAME "rf_cable"

static int rf_cable_gpio_sts;
int gpio_num;

static ssize_t cable_read_proc(struct file *file, char __user *buf, size_t 
count, loff_t *off)
{
	char page[16] = {0};
	int len = 0;

	rf_cable_gpio_sts = gpio_get_value(gpio_num);

	len += snprintf(&page[len], 16, "%d\n", rf_cable_gpio_sts);

	if (len > *off)
		len -= *off;
	else
		len = 0;

	if (copy_to_user(buf, page, (len < count ? len : count))) {
		return -EFAULT;
	}
	*off += len < count ? len : count;

	return (len < count ? len : count);
}

static const struct file_operations cable_proc_fops_cable = {
	.read = cable_read_proc,
};

static int op_rf_cable_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct proc_dir_entry *op_rf = NULL;

	pr_err("%s enter!\n", __func__);

	gpio_num = of_get_named_gpio(np, "rf,cable0-gpio", 0);
	if (gpio_is_valid(gpio_num)) {
		gpio_request(gpio_num, "rf-cable-gpio");
		gpio_direction_input(gpio_num);
	} else {
		pr_err("can't get gpio info\n");
		rf_cable_gpio_sts = 1;
	}

	op_rf = proc_mkdir("op_rf", NULL);
	if (!op_rf) {
		pr_err("can't create op_rf proc\n");
		goto exit;
	}
	proc_create("rf_cable", 0444, op_rf, &cable_proc_fops_cable);

	return rc;

exit:
	pr_err("%s: probe Fail!\n", __func__);

	return -EFAULT;
}

static const struct of_device_id rf_of_match[] = {
	{ .compatible = RF_CABLE, },
	{}
};
MODULE_DEVICE_TABLE(of, rf_of_match);

static struct platform_driver op_rf_cable_driver = {
	.driver = {
		.name       = RF_CABLE_NAME,
		.owner      = THIS_MODULE,
		.of_match_table = rf_of_match,
	},
	.probe = op_rf_cable_probe,
};

static int __init op_rf_cable_init(void)
{
	int ret;

	ret = platform_driver_register(&op_rf_cable_driver);
	if (ret)
		pr_err("rf_cable_driver register failed: %d\n", ret);

	return ret;
}

MODULE_LICENSE("GPL v2");
late_initcall(op_rf_cable_init);
