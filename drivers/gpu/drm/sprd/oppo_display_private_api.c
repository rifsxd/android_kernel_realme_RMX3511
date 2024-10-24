/***************************************************************
** Copyright (C),  2018,  OPPO Mobile Comm Corp.,  Ltd
** VENDOR_EDIT
** File : oppo_display_private_api.h
** Description : oppo display private api implement
** Version : 1.0
** Date : 2018/03/20
** Author : Jie.Hu@PSW.MM.Display.Stability
**
** ------------------------------- Revision History: -----------
**  <author>        <data>        <version >        <desc>
**   Hu.Jie          2018/03/20        1.0           Build this moudle
******************************************************************/
#include "oppo_display_private_api.h"
#include <linux/fb.h>
#include <linux/time.h>
#include <linux/timekeeping.h>
#include <linux/of.h>

/*
 * we will create a sysfs which called /sys/kernel/oppo_display,
 * In that directory, oppo display private api can be called
 */

unsigned long CABC_mode = 0;
/* wangcheng@ODM.Multimedia.LCD  2021/04/12 solve cabc dump  start*/
extern unsigned int flag_bl;
/* wangcheng@ODM.Multimedia.LCD  2021/04/12 solve cabc dump  end*/
extern unsigned long tp_gesture;
// extern int sprd_panel_cabc_mode(unsigned int level);
extern int sprd_panel_cabc(unsigned int level);
static ssize_t cabc_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
    printk("%s CABC_mode=%ld\n", __func__, CABC_mode);
    return sprintf(buf, "%ld\n", CABC_mode);
}

static ssize_t cabc_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t num)
{
    int ret = 0;
    ret = kstrtoul(buf, 10, &CABC_mode);
    printk("%s CABC_mode=%ld\n", __func__, CABC_mode);
    if (flag_bl ==0) {
        return 0;
	}
	ret = sprd_panel_cabc((unsigned int)CABC_mode);
	return num;
}

static struct kobj_attribute dev_attr_cabc =
	__ATTR(cabc, 0664, cabc_show, cabc_store);

/*add /sys/kernel/oppo_display/tp_gesture*/
static ssize_t tp_gesture_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
    printk("%s tp_gesture=%ld\n", __func__, tp_gesture);
    return sprintf(buf, "%ld\n", tp_gesture);
}

static ssize_t tp_gesture_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t num)
{
    int ret = 0;
    ret = kstrtoul(buf, 10, &tp_gesture);
    printk("%s tp_gesture_mode=%ld\n", __func__, tp_gesture);

	return num;
}


static struct kobj_attribute dev_attr_tp_gesture =
	__ATTR(tp_gesture, 0664, tp_gesture_show, tp_gesture_store);


char lcd_name_serial_num[64];
unsigned long lcd_id;
int get_lcd_name_from_cmdline(void)
{
	struct device_node *cmdline_node;
	const char *cmd_line, *lcd_name_p;
	int rc;

	cmdline_node = of_find_node_by_path("/chosen");
	rc = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
	if (!rc) {
		lcd_name_p = strstr(cmd_line, "lcd_id=");
		if (lcd_name_p) {
			sscanf(lcd_name_p, "lcd_id=ID%s", lcd_name_serial_num);
			printk("lcd_id=ID %s\n",lcd_name_serial_num);
		}
	} else {
		printk("can't not parse bootargs property\n");
		return rc;
	}
	return 0;
}

/*add /sys/kernel/oppo_display/panel_serial_number*/
static ssize_t panel_id_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	int ret;
	get_lcd_name_from_cmdline();
	ret = kstrtoul(lcd_name_serial_num, 16, &lcd_id);
	ret = scnprintf(buf, PAGE_SIZE, "LCM ID[%x]: 0x%x 0x%x\n", 0, lcd_id, 0);

    return ret;
}

static ssize_t panel_id_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t num)
{
	return num;
}


static struct kobj_attribute dev_attr_panel_id =
	__ATTR(panel_id, 0664, panel_id_show, panel_id_store);

static struct kobject *oppo_display_kobj;

//static DEVICE_ATTR_RW(cabc);

/*
 * Create a group of attributes so that we can create and destroy them all
 * at once.
 */
static struct attribute *oppo_display_attrs[] = {
	&dev_attr_cabc.attr,
	&dev_attr_tp_gesture.attr,
	&dev_attr_panel_id.attr,
	NULL,	/* need to NULL terminate the list of attributes */
};

static struct attribute_group oppo_display_attr_group = {
	.attrs = oppo_display_attrs,
};

int oppo_display_private_api_init(void)
{
	int retval;

	oppo_display_kobj = kobject_create_and_add("oppo_display", kernel_kobj);
	if (!oppo_display_kobj)
		return -ENOMEM;

	/* Create the files associated with this kobject */
	retval = sysfs_create_group(oppo_display_kobj, &oppo_display_attr_group);
	if (retval)
		kobject_put(oppo_display_kobj);

	return retval;
}
EXPORT_SYMBOL_GPL(oppo_display_private_api_init);

void oppo_display_private_api_exit(void)
{
	kobject_put(oppo_display_kobj);
}
EXPORT_SYMBOL_GPL(oppo_display_private_api_exit);

//module_init(oppo_display_private_api_init);
//module_exit(oppo_display_private_api_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Hujie <hujie@oppo.com>");
