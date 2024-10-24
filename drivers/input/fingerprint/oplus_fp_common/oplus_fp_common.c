// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 oplus. All rights reserved.
 */

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/of_gpio.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/device.h>

#include "../include/oplus_fp_common.h"



#define CHIP_UNKNOWN           "unknown"

static char *oplus_fp_common_dir_name = "oplus_fp_common";

static struct proc_dir_entry *oplus_fp_common_dir = NULL;
static char *fp_id_name = "fp_id";
static char fp_manu[FP_ID_MAX_LENGTH] = CHIP_UNKNOWN;
static struct proc_dir_entry *fp_id_dir = NULL;
static struct fp_data *oplus_fp_data_ptr = NULL;
static int finger_screen_status = 0;

finger_screen fingerprint_get_screen_status = NULL;
#ifdef VENDOR_KERNEL
EXPORT_SYMBOL(fingerprint_get_screen_status);
#endif

struct fp_data {
    struct device *dev;
    uint32_t fp_id[MAX_ID_AMOUNT];
    fp_vendor_t fpsensor_type;
};
static struct fp_data *fp_data_ptr = NULL;


static ssize_t fp_suspend_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
        return sprintf(buf, "%s\n",
               finger_screen_status? "true" : "false");
}

static ssize_t fp_suspend_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
     
        if ((buf[0] == '1') ) {
            finger_screen_status = 1;
            printk("oplusfp buf[0] = %c finger_screen_status = %d \n",buf[0],finger_screen_status);
            if(fingerprint_get_screen_status != NULL)
                fingerprint_get_screen_status(1);
          
        } else if ((buf[0] == '0')) {
            finger_screen_status = 0;
            printk("oplusfp buf[0] = %c finger_screen_status = %d \n",buf[0],finger_screen_status);
            if(fingerprint_get_screen_status != NULL)
                fingerprint_get_screen_status(0);
        }

        return count;
}



static DEVICE_ATTR_RW(fp_suspend);

static struct attribute *fp_debug_attrs[] = {
        &dev_attr_fp_suspend.attr,
        NULL,
};

static struct attribute_group fp_debug_attr_group = {
        .attrs = fp_debug_attrs,
};


static int fp_filesys_create(struct platform_device *pdata)
{
        int retval;
        //struct device *dev = pdata->dev;
        printk("oplusfp #### %s %d ###\n",__func__,__LINE__);

        /* create sysfs debug files     */
        retval = sysfs_create_group(&pdata->dev.kobj,
                &fp_debug_attr_group);
        
                printk("oplusfp #### %s %d ###\n",__func__,__LINE__);
        if (retval < 0) {
              //  dev_err(dev, "Fail to create debug files!");
                return -ENOMEM;
        }

        printk("oplusfp #### %s %d ###\n",__func__,__LINE__);

        /* convenient access to sysfs node */
        retval = sysfs_create_link(NULL, &pdata->dev.kobj, "fingerprintscreen");
        
        printk("oplusfp #### %s %d ###\n",__func__,__LINE__);
        if (retval < 0) {
               // dev_err(dev, "Failed to create link!");
                return -ENOMEM;
        }
        printk("oplusfp #### %s %d ###\n",__func__,__LINE__);

        return 0;

}

static void fp_filesys_remove(struct fp_data *pdata)
{

       sysfs_remove_link(NULL, "fingerprintscreen");
   //    sysfs_remove_group(&pdata->dev->kobj, &fp_debug_attr_group);
}




static ssize_t fp_id_node_read(struct file *file, char __user *buf, size_t count, loff_t *pos) {
    char page[FP_ID_MAX_LENGTH] = { 0 };
    char *p = page;
    int len = 0;

    p += snprintf(p, FP_ID_MAX_LENGTH - 1, "%s", fp_manu);
    len = p - page;
    if (len > *pos) {
        len -= *pos;
    }
    else {
        len = 0;
    }

    if (copy_to_user(buf, page, len < count ? len  : count)) {
        return -EFAULT;
    }

    *pos = *pos + (len < count ? len  : count);

    return len < count ? len  : count;
}

static ssize_t fp_id_node_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
    size_t local_count;
    if (count <= 0) {
        return 0;
    }
    strncpy(fp_manu, CHIP_UNKNOWN, FP_ID_MAX_LENGTH - 1);

    local_count = (FP_ID_MAX_LENGTH - 1) < count ? (FP_ID_MAX_LENGTH - 1) : count;
    if (copy_from_user(fp_manu , buf, local_count) != 0) {
        dev_err(fp_data_ptr->dev, "write fp manu value fail\n");
        return -EFAULT;
    }
    fp_manu[local_count] = '\0';
    dev_info(fp_data_ptr->dev, "write fp manu = %s\n", fp_manu);
    return count;
}

static struct file_operations fp_id_node_ctrl = {
    .read = fp_id_node_read,
    .write = fp_id_node_write,
};
    

static int oplus_fp_register_proc_fs(void)
{
    int ret = FP_OK;
    /*  make the proc /proc/fp_id  */
    fp_id_dir = proc_create(fp_id_name, 0666, NULL, &fp_id_node_ctrl);
    if (fp_id_dir == NULL) {
        ret = -FP_ERROR_GENERAL;
        goto exit;
    }

    return FP_OK;
exit :
    return ret;
}

fp_vendor_t get_fpsensor_type(void)
{
    fp_vendor_t vendor =  FP_UNKNOWN;
    
    if (NULL == oplus_fp_data_ptr) {
        printk("oplusfp %s no device \n", __func__);
        return FP_UNKNOWN;
    }
    vendor = oplus_fp_data_ptr->fpsensor_type;
    printk("oplusfp %s get vendor = %d.\n", __func__,vendor);

    return vendor;
}
#ifdef VENDOR_KERNEL
EXPORT_SYMBOL(get_fpsensor_type);
#endif

static int oplus_fp_common_probe(struct platform_device *fp_dev)
{
    int ret = 0;
    struct device *dev = &fp_dev->dev;
    struct fp_data *fp_data = NULL;
    int fp_gpio = 0;
    int gpio_value = -1;

    fp_data = devm_kzalloc(dev, sizeof(struct fp_data), GFP_KERNEL);
    if (fp_data == NULL) {
        printk("fp_data kzalloc failed\n");
    }
    
    printk("oplusfp ### %s %d fp_gpio = %d \n",__func__,__LINE__,fp_gpio);

    fp_data = devm_kzalloc(dev, sizeof(struct fp_data), GFP_KERNEL);
    if (fp_data == NULL) {
        dev_err(dev, "fp_data kzalloc failed\n");
        ret = -ENOMEM;
        goto exit;
    }

    oplus_fp_data_ptr = fp_data;

    fp_gpio = of_get_named_gpio(fp_dev->dev.of_node, "fp-gpio", 0);
    printk("oplusfp ### %s %d fp_gpio = %d \n",__func__,__LINE__,fp_gpio);

    ret = gpio_request(fp_gpio, "fp_id");
    ret = gpio_direction_input(fp_gpio);
    gpio_value = gpio_get_value(fp_gpio);

    printk("oplusfp ### %s %d gpio_value = %d \n",__func__,__LINE__,gpio_value);

    if(gpio_value == 0)
    {
        strcpy(fp_manu,"C_7319");
        fp_data->fpsensor_type = FP_CHIPONE_C7319;
        printk("oplusfp ### %s 0 fp_data->fpsensor_type = %d \n",__func__, fp_data->fpsensor_type);
    } else {
        strcpy(fp_manu,"F_9396");
        fp_data->fpsensor_type = FP_FOCALTECH_9396;
        printk("oplusfp ### %s 1  fp_data->fpsensor_type = %d \n",__func__, fp_data->fpsensor_type);
    }

    oplus_fp_register_proc_fs();
    printk("oplusfp #### %s %d ### \n",__func__,__LINE__);
    ret = fp_filesys_create(fp_dev);

    return FP_OK;

exit:

    if (oplus_fp_common_dir) {
        remove_proc_entry(oplus_fp_common_dir_name, NULL);
    }

    if (fp_id_dir) {
        remove_proc_entry(fp_id_name, NULL);
    }

    dev_err(dev, "fp_data probe failed ret = %d\n", ret);
    if (fp_data) {
        devm_kfree(dev, fp_data);
    }

    return ret;
}

static int oplus_fp_common_remove(struct platform_device *pdev)
{
    fp_filesys_remove(oplus_fp_data_ptr);

    return FP_OK;
}

static struct of_device_id oplus_fp_common_match_table[] = {
    {   .compatible = "oplus,fp_common", },
    {}
};

static struct platform_driver oplus_fp_common_driver = {
    .probe = oplus_fp_common_probe,
    .remove = oplus_fp_common_remove,
    .driver = {
        .name = "oplus_fp_common",
        .owner = THIS_MODULE,
        .of_match_table = oplus_fp_common_match_table,
    },
};

static int __init oplus_fp_common_init(void)
{
    printk("oplusfp ### %s %d\n",__func__,__LINE__);

    return platform_driver_register(&oplus_fp_common_driver);
}

static void __exit oplus_fp_common_exit(void)
{
    platform_driver_unregister(&oplus_fp_common_driver);
}

module_init(oplus_fp_common_init);
module_exit(oplus_fp_common_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("THUNDERSOFT");
MODULE_DESCRIPTION("oplus fingerprint sensor device driver");
