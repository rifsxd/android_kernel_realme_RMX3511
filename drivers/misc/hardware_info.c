/***********************************************************
** Copyright (C), 2008-2016, OPPO Mobile Comm Corp., Ltd.
** OPLUS_EDIT
** File: - hardware_info.c
** Description: source  for hardware infomation
**
** Version: 1.0
** Date : 2018/08/11
** Author: Jinfan.Hu@BSP.Kernel.Boot
**
** ------------------------------- Revision History: -------------------------------
**  	<author>		<data> 	   <version >	       <desc>
**      Jinfan.Hu       2018/08/11     1.0               source  for hardware infomation
**
****************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/proc_fs.h>

#include <linux/hardware_info_wt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <soc/oppo/device_info.h>

typedef struct hardwareinfo_get{
	void (*get)(void *driver_data);
	void *driver_data;
} hardwareinfo_get;

static hardwareinfo_get hardwareinfo_get_tp;
char Ctp_name[HARDWARE_MAX_ITEM_LONGTH];
EXPORT_SYMBOL_GPL(Ctp_name);
void hardwareinfo_tp_register(void (*fn)(void *), void *driver_data)
{
	hardwareinfo_get_tp.get = fn;
	hardwareinfo_get_tp.driver_data = driver_data;
}
char Lcm_name2[HARDWARE_MAX_ITEM_LONGTH];

char sub_match[HARDWARE_MAX_ITEM_LONGTH];	//wt_chenxu,20220315,add wt sub_match
char board_id[HARDWARE_MAX_ITEM_LONGTH];
char hardware_id[HARDWARE_MAX_ITEM_LONGTH];
static char hardwareinfo_name[HARDWARE_MAX_ITEM][HARDWARE_MAX_ITEM_LONGTH];

char* hardwareinfo_items[HARDWARE_MAX_ITEM] =
{
	"LCD",
	"TP",
	"MEMORY",
	"CAM_FRONT",
	"CAM_BACK",
	"BT",
	"WIFI",
	"GSENSOR",
	"PLSENSOR",
	"GYROSENSOR",
	"MSENSOR",
	"SAR",
	"GPS",
        "NFC",
	"FM",
	"BATTERY",
	"CAM_M_BACK",
	"CAM_M_FRONT",
	"BOARD_ID"
	"HARDWARE_ID"
};

#define FP_PROC_NAME	"hwinfo"

static char hdware_info_fp[100] = "unknow";

const char *cmd_line;
int get_lcd_name_from_cmdline(void)
{
	struct device_node *cmdline_node;
	int rc;

	cmdline_node = of_find_node_by_path("/chosen");
	rc = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
	if (!rc) {
		printk(" parse bootargs property\n");
	} else {
		printk("can't not parse bootargs property\n");
		return rc;
	}
	return 0;
}

static ssize_t fp_hwinfo_proc_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	size_t min;

	min = min(count, (size_t)(ARRAY_SIZE(hdware_info_fp) - 1));
	memset(hdware_info_fp, 0, ARRAY_SIZE(hdware_info_fp));
	if (copy_from_user(hdware_info_fp, buf, min)) {
		pr_err("%s: copy from user error.", __func__);
		return -1;
	}

	return min;
}

static ssize_t fp_hwinfo_proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	size_t min;
	char *p;
	size_t size = ARRAY_SIZE(hdware_info_fp) + strlen("Fingerprint: \n");

	if(*ppos)
		return 0;
	*ppos += count;

	min = min(count, size);
	p = kzalloc(size, GFP_KERNEL);
	if (!p) {
		pr_err("%s: out of memory.", __func__);
		return -1;
	}
	snprintf(p, size, "Fingerprint: %s\n", hdware_info_fp);

	if (copy_to_user(buf, p, min)) {
		pr_err("%s: copy to user error.", __func__);
		kfree(p);
		return -1;
	}
	kfree(p);

	return min;
}

static const struct file_operations fp_hwinfo_fops =
{
	.write = fp_hwinfo_proc_write,
	.read  = fp_hwinfo_proc_read,
	.owner = THIS_MODULE,
};

/*
* This function will create a node named FP_PROC_NAME for user to read the fingerprint
* infomation.Just cat /proc/FP_PROC_NAME.
*/
void fp_hwinfo_proc_create(void)
{
	struct proc_dir_entry *fp_hwinfo_proc_dir;

	fp_hwinfo_proc_dir = proc_create(FP_PROC_NAME, 0666, NULL, &fp_hwinfo_fops);
	if (fp_hwinfo_proc_dir == NULL)
		pr_err("[HW_INFO][ERR]: create fp_hwinfo_proc fail\n");
}


int hardwareinfo_set_prop(int cmd, const char *name)
{
	if(cmd < 0 || cmd >= HARDWARE_MAX_ITEM)
		return -1;

	strcpy(hardwareinfo_name[cmd], name);

	return 0;
}
EXPORT_SYMBOL_GPL(hardwareinfo_set_prop);

//for mtk platform
static char* boardid_get(void)
{
	char *p, *q;
	char* s1="not found";

	p = strstr(cmd_line, "board_id=");
	if(!p) {
		printk("board_id not found in cmdline\n");
		return s1;
	}

	if ((p - cmd_line) > strlen(cmd_line + 1))
		return s1;
	p += strlen("board_id=");
	q = p;
	while (*q != ' ' && *q != '\0')
		q++;

	memset(board_id, 0, sizeof(board_id));
	strncpy(board_id, p, (int)(q - p));
	board_id[q - p + 1]='\0';
	s1 = board_id;
	//printk("board_id found in cmdline : %s\n", board_id);

	return s1;
}

//wt_wangjianlong, 20220607, add wt accsensor_info
static char* accsensor_info_get(void)
{
    char *s1 = "not found";

    if(!strstr(cmd_line, "sensor_name")) {
        printk("sensor_name not found in cmdline,please add sensorhub uboot support!");
        return s1;
    }

    if (strstr(cmd_line, "mc3419")) {
        s1 = "mc3419";
    } else if (strstr(cmd_line, "stk8321")) {
        s1 = "stk8321";
    }
    return s1;
}

//wt_wangjianlong, 20220607, add wt plsensor_info
static char* plsensor_info_get(void)
{
    char *s1 = "not found";

    if(!strstr(cmd_line, "sensor_name")) {
        printk("sensor_name not found in cmdline,please add sensorhub uboot support!");
        return s1;
    }
    
    if (strstr(cmd_line, "tmd2712")) {
        s1 = "tmd2712";
    }
    return s1;
}

//wt_chenxu,20220315,add wt sub_match
static char* sub_match_info_get(void)
{
	char *p, *q;
	char* s1="not found";

	p = strstr(cmd_line, "sub_match=");
	if(!p) {
		printk("sub_match not found in cmdline\n");
		return s1;
	}

	if ((p - cmd_line) > strlen(cmd_line + 1))
		return s1;
	p += strlen("sub_match=");
	q = p;
	while (*q != ' ' && *q != '\0')
		q++;

	memset(sub_match, 0, sizeof(sub_match));
	strncpy(sub_match, p, (int)(q - p));
	sub_match[q - p + 1]='\0';
	s1 = sub_match;
	//printk("sub_match found in cmdline : %s\n", sub_match);

	return s1;
}

//wt_songyiyang,20220314,Add wt hardware_id
static char* hwid_get(void)
{
	char *p, *q;
	char* s1="not found";

	p = strstr(cmd_line, "hw_id=");
	if(!p) {
		printk("hardware_id not found in cmdline\n");
		return s1;
	}

	if ((p - cmd_line) > strlen(cmd_line + 1))
		return s1;

	p += strlen("hw_id=");
	q = p;
	while (*q != ' ' && *q != '\0')
		q++;

	memset(hardware_id, 0, sizeof(hardware_id));
	strncpy(hardware_id, p, (int)(q - p));
	hardware_id[q - p + 1]='\0';
	s1=hardware_id;
	return s1;
}

static long hardwareinfo_ioctl(struct file *file, unsigned int cmd,unsigned long arg)
{
	int ret = 0, hardwareinfo_num = HARDWARE_MAX_ITEM;
	void __user *data = (void __user *)arg;
	//char info[HARDWARE_MAX_ITEM_LONGTH];
	switch (cmd) {
	case HARDWARE_LCD_GET:
		hardwareinfo_set_prop(HARDWARE_LCD, Lcm_name2);
		hardwareinfo_num = HARDWARE_LCD;
		break;
	case HARDWARE_TP_GET:
		hardwareinfo_num = HARDWARE_TP;
		hardwareinfo_set_prop(HARDWARE_TP, Ctp_name);
			//if (hardwareinfo_get_tp.get)
			//hardwareinfo_get_tp.get(hardwareinfo_get_tp.driver_data);
		break;
	case HARDWARE_FLASH_GET:
		hardwareinfo_num = HARDWARE_FLASH;
		break;
	case HARDWARE_FRONT_CAM_GET:
		hardwareinfo_num = HARDWARE_FRONT_CAM;
		break;
	case HARDWARE_BACK_CAM_GET:
		hardwareinfo_num = HARDWARE_BACK_CAM;
		break;
	case HARDWARE_BACK_SUBCAM_GET:
		hardwareinfo_num = HARDWARE_BACK_SUB_CAM;
		break;
	case HARDWARE_FRONT_SUBCAM_GET:
		hardwareinfo_num = HARDWARE_FRONT_SUB_CAM;
		break;
	case HARDWARE_BT_GET:
		hardwareinfo_set_prop(HARDWARE_BT, "umw2652");
		hardwareinfo_num = HARDWARE_BT;
		break;
	case HARDWARE_WIFI_GET:
		hardwareinfo_set_prop(HARDWARE_WIFI, "umw2652");
		hardwareinfo_num = HARDWARE_WIFI;
		break;
	case HARDWARE_ACCELEROMETER_GET:
        hardwareinfo_set_prop(HARDWARE_ACCELEROMETER, (const char*)accsensor_info_get());
		hardwareinfo_num = HARDWARE_ACCELEROMETER;
		break;
	case HARDWARE_ALSPS_GET:
        hardwareinfo_set_prop(HARDWARE_ALSPS, (const char*)plsensor_info_get());
		hardwareinfo_num = HARDWARE_ALSPS;
		break;
	case HARDWARE_GYROSCOPE_GET:
		hardwareinfo_num = HARDWARE_GYROSCOPE;
		break;
	case HARDWARE_MAGNETOMETER_GET:
		hardwareinfo_num = HARDWARE_MAGNETOMETER;
		break;
	case HARDWARE_SAR_GET:
        hardwareinfo_set_prop(HARDWARE_SAR, "no sar");
		hardwareinfo_num = HARDWARE_SAR;
		break;
	case HARDWARE_GPS_GET:
		hardwareinfo_set_prop(HARDWARE_GPS, "umw2652");
	    hardwareinfo_num = HARDWARE_GPS;
		break;
	case HARDWARE_FM_GET:
		hardwareinfo_set_prop(HARDWARE_FM, "NULL");
	    hardwareinfo_num = HARDWARE_FM;
		break;
	case HARDWARE_NFC_GET:
                hardwareinfo_num = HARDWARE_NFC;
                hardwareinfo_set_prop(HARDWARE_NFC, "NXP,PN557");
		break;
	case HARDWARE_BATTERY_ID_GET:
		hardwareinfo_num = HARDWARE_BATTERY_ID;
		break;
	case HARDWARE_BACK_CAM_MOUDULE_ID_GET:
		hardwareinfo_num = HARDWARE_BACK_CAM_MOUDULE_ID;
		break;
	case HARDWARE_FRONT_CAM_MODULE_ID_GET:
		hardwareinfo_num = HARDWARE_FRONT_CAM_MOUDULE_ID;
		break;
	case HARDWARE_BACK_SUBCAM_MODULEID_GET:
		hardwareinfo_num = HARDWARE_BACK_SUB_CAM_MOUDULE_ID;
		break;
	case HARDWARE_FRONT_SUBCAM_MODULEID_GET:
		hardwareinfo_num = HARDWARE_FRONT_SUB_CAM_MOUDULE_ID;
		break;
	case HARDWARE_BACK_CAM_EFUSEID_GET:
		hardwareinfo_num = HARDWARE_BACK_CAM_EFUSEID;
		break;
	case HARDWARE_BCAK_SUBCAM_EFUSEID_GET:
		hardwareinfo_num = HARDWARE_BCAK_SUBCAM_EFUSEID;
		break;
	case HARDWARE_FRONT_CAME_EFUSEID_GET:
		hardwareinfo_num = HARDWARE_FRONT_CAME_EFUSEID;
		break;
	case HARDWARE_BACK_CAM_SENSORID_GET:
		hardwareinfo_num = HARDWARE_BACK_CAM_SENSORID;
		break;
	case HARDWARE_BACK_SUBCAM_SENSORID_GET:
		hardwareinfo_num = HARDWARE_BACK_SUBCAM_SENSORID;
		break;
	case HARDWARE_FRONT_CAM_SENSORID_GET:
		hardwareinfo_num = HARDWARE_FRONT_CAM_SENSORID;
		break;
	case HARDWARE_BOARD_ID_GET:
		boardid_get();
		hardwareinfo_set_prop(HARDWARE_BOARD_ID, board_id);
		hardwareinfo_num = HARDWARE_BOARD_ID;
		break;
		//wt_chenxu,20220315,add wt sub_match
	case HARDWARE_SUB_MATCH_INFO_GET:
		sub_match_info_get();
		hardwareinfo_set_prop(HARDWARE_SUB_MATCH_INFO, sub_match);
		hardwareinfo_num = HARDWARE_SUB_MATCH_INFO;
		break;
//wt_songyiyang,20220314,Add wt hardware_id
	case HARDWARE_HARDWARE_ID_GET:
		hwid_get();
		hardwareinfo_set_prop(HARDWARE_HARDWARE_ID, hardware_id);
		hardwareinfo_num = HARDWARE_HARDWARE_ID;
		break;
	case HARDWARE_CAMERA_INFO_SET:
	{
		camera_info hardware;
		memset(&hardware, 0, sizeof(camera_info));
		if(copy_from_user(&hardware, data, sizeof(camera_info)))
		{
			pr_err("copy_from_user HARDWARE_CAMERA_INFO_SET error");
			ret =  -EINVAL;
		}
		else
		{
			//strncpy(hardwareinfo_name[hardware.param], hardware.str, hardware.lenth);
			hardwareinfo_set_prop(hardware.param, hardware.str);
			pr_info("copy_from_user HARDWARE_CAMERA_INFO_SET %d, %s", hardware.param, hardwareinfo_name[hardware.param]);
		}
		goto set_ok;
		break;
	}
	default:
		ret = -EINVAL;
		goto err_out;
	}
	//memset(data, 0, HARDWARE_MAX_ITEM_LONGTH);//clear the buffer
	if (copy_to_user(data, hardwareinfo_name[hardwareinfo_num], strlen(hardwareinfo_name[hardwareinfo_num]))){
		//printk("%s, copy to usr error\n", __func__);
		ret =  -EINVAL;
	}
set_ok:
err_out:
	return ret;
}

static ssize_t show_boardinfo(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i = 0;
	char temp_buffer[HARDWARE_MAX_ITEM_LONGTH];
	int buf_size = 0;

	for(i = 0; i < HARDWARE_MAX_ITEM; i++)
	{
		memset(temp_buffer, 0, HARDWARE_MAX_ITEM_LONGTH);
		if(i == HARDWARE_LCD)
		{
			sprintf(temp_buffer, "%s : %s\n", hardwareinfo_items[i], Lcm_name2);
		}
		else if(i == HARDWARE_BT || i == HARDWARE_WIFI || i == HARDWARE_GPS || i == HARDWARE_FM)
		{
			sprintf(temp_buffer, "%s : %s\n", hardwareinfo_items[i], "MTK");
		}
		else
		{
			sprintf(temp_buffer, "%s : %s\n", hardwareinfo_items[i], hardwareinfo_name[i]);
		}
		strcat(buf, temp_buffer);
		buf_size +=strlen(temp_buffer);
	}

	return buf_size;
}
static DEVICE_ATTR(boardinfo, S_IRUGO, show_boardinfo, NULL);

static int boardinfo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int rc = 0;

	printk("%s: start\n", __func__);
	
	rc = device_create_file(dev, &dev_attr_boardinfo);
	if (rc < 0)
		return rc;

	dev_info(dev, "%s: ok\n", __func__);

	return 0;
}

static int boardinfo_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	device_remove_file(dev, &dev_attr_boardinfo);
	dev_info(dev, "%s\n", __func__);
	return 0;
}


static struct file_operations hardwareinfo_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.unlocked_ioctl = hardwareinfo_ioctl,
	.compat_ioctl = hardwareinfo_ioctl,
};

static struct miscdevice hardwareinfo_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "hardwareinfo",
	.fops = &hardwareinfo_fops,
};

static const struct of_device_id boardinfo_of_match[] = {
	{ .compatible = "wt:boardinfo", },
	{},
};
MODULE_DEVICE_TABLE(of, boardinfo_of_match);


static struct platform_driver boardinfo_driver = {
	.driver = {
		.name	= "boardinfo",
		.owner	= THIS_MODULE,
		.of_match_table = boardinfo_of_match,
	},
	.probe		= boardinfo_probe,
	.remove		= boardinfo_remove,
};



static int __init hardwareinfo_init_module(void)
{
	int ret, i;
#ifdef CONFIG_OPPO_DEVICE_IFNO
	char* sub_mainboard;
#endif
	get_lcd_name_from_cmdline();
	for(i = 0; i < HARDWARE_MAX_ITEM; i++)
		strcpy(hardwareinfo_name[i], "NULL");
	pr_err("******hardwareinfo_init_module init\n");
	ret = misc_register(&hardwareinfo_device);
	if(ret < 0){
		pr_err("hardwareinfo_init_module misc failed\n");
		return -ENODEV;
	}
#ifdef CONFIG_OPPO_DEVICE_IFNO
	sub_mainboard = sub_match_info_get();
	register_device_proc("sub_mainboard", "sprd", sub_mainboard);
#endif
	ret = platform_driver_register(&boardinfo_driver);
	if(ret != 0)
	{
		pr_err("hardwareinfo_init_module register boardinfo failed\n");
		return -ENODEV;
	}

	fp_hwinfo_proc_create();

	return 0;
}

static void __exit hardwareinfo_exit_module(void)
{
	misc_deregister(&hardwareinfo_device);
}

fs_initcall(hardwareinfo_init_module);
module_exit(hardwareinfo_exit_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ming.He@OPLUS");
MODULE_DESCRIPTION("hardwareinfo");

