/*
 * Copyright (C) 2015-2016 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/module.h>
#include "sprd_img.h"
#include "flash_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "FLASH_AW3641: %d %d %s : " fmt, current->pid, __LINE__, __func__

#define FLASH_DRIVER_NAME "flash-aw3641"
#define FLASH_GPIO_MAX 2
#define FLASH_MAX_PWM 16
#define FLASH_MAX_LEVEL 7
#define FLASH_MIN_LEVEL 0

#define AW3641_SOFTWARE_TIMEOUT   500 /* : ms  */
enum {
	GPIO_FLASH_TORCH_MODE,	// 132
	GPIO_CHIP_EN,		// 133
};

struct flash_driver_data {
	int gpio_tab[FLASH_GPIO_MAX];
	u32 torch_led_index;
};

static const char *const flash_gpio_names[FLASH_GPIO_MAX] = {
	"flash-torch-en-gpios",	/* 132  for torch/flash mode */
	"flash-en-gpios",	/* 133  for enable ic pin */
};

static int g_high_level = 0;
static int highlight_opened = 0;
spinlock_t flash_lock;
//+Bug696101 xuyichen01.wt, MODIFY, 2022/1/19, flash temperature rise protection
#include <linux/hrtimer.h>
#include <linux/ktime.h>
static int sprd_flash_aw3641_close_highlight(void *drvd, uint8_t idx);
static struct flash_driver_data *drv_data_tmp = NULL;

static struct hrtimer g_timeOutTimer;
static struct work_struct workTimeOut;
static int g_timer_flag = 0;
static void work_timeOutFunc(struct work_struct *data);
static enum hrtimer_restart ledTimeOutCallback(struct hrtimer *timer);
static int sprd_flash_aw3641_time_close(void *drvd)
{
    int ret = 0;
	int gpio_id = 0;
	uint8_t idx = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;
	pr_info("sprd_flash_aw3641_time_close\n");

	if (!drv_data)
		return -EFAULT;
	idx = drv_data->torch_led_index;
		pr_info("torch_led_index:%d, idx:%d\n", drv_data->torch_led_index, idx);

	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[GPIO_CHIP_EN];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
			if (ret)
				goto exit;
		}
	}

	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[GPIO_FLASH_TORCH_MODE];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
			if (ret)
				goto exit;
		}
	}
exit:
	return ret;
}

static void work_timeOutFunc(struct work_struct *data)
{
	if(drv_data_tmp != NULL)
	{
		sprd_flash_aw3641_time_close(drv_data_tmp);
	}
	pr_info("aw3641 ledTimeOut_callback\n");
}
static enum hrtimer_restart ledTimeOutCallback(struct hrtimer *timer)
{
	schedule_work(&workTimeOut);
	return HRTIMER_NORESTART;
}
static void timerInit(void)
{
	static int init_flag;

	if (init_flag == 0) {
		init_flag = 1;
		INIT_WORK(&workTimeOut, work_timeOutFunc);
		hrtimer_init(&g_timeOutTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		g_timeOutTimer.function = ledTimeOutCallback;
	}
}
//-Bug696101 xuyichen01.wt, MODIFY, 2022/1/19, flash temperature rise protection

static int sprd_flash_aw3641_open_pwm(void *drvd, uint8_t idx, int level)
{
	int ret = 0;
	int i = 0;
	int gpio_id = 0;
	unsigned long flags;

	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;
	if (!drv_data)
		return -EFAULT;

	if (level > FLASH_MAX_LEVEL)
		level = FLASH_MAX_LEVEL;

	if (level < FLASH_MIN_LEVEL)
		level = FLASH_MIN_LEVEL;

	idx = drv_data->torch_led_index;
	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[GPIO_FLASH_TORCH_MODE];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_ON);
		}
	}

	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[GPIO_CHIP_EN];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
			udelay(550);
			spin_lock_irqsave(&flash_lock, flags);
			for (i = 0; i < FLASH_MAX_PWM - level; i++) {
				pr_info("open_pwm:%d\n", i);
				ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
				udelay(1);
				ret = gpio_direction_output(gpio_id, SPRD_FLASH_ON);
				udelay(1);
			}
			spin_unlock_irqrestore(&flash_lock, flags);
		}
	}
	return ret;
}

static int sprd_flash_aw3641_init(void *drvd)
{
	int ret = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	return ret;
}

static int sprd_flash_aw3641_deinit(void *drvd)
{
	int ret = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	return ret;
}

static int sprd_flash_aw3641_open_torch(void *drvd, uint8_t idx)
{
	int ret = 0;
	int gpio_id = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	pr_info("torch_led_index:%d, idx:%d\n", drv_data->torch_led_index, idx);
	idx = drv_data->torch_led_index;
	idx |= SPRD_FLASH_LED0;
	if (SPRD_FLASH_LED0 & idx ) {
		gpio_id = drv_data->gpio_tab[GPIO_CHIP_EN];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_ON);
			if (ret) {
				goto exit;
			}
		}
	}

exit:
	return ret;
}

static int sprd_flash_aw3641_close_torch(void *drvd, uint8_t idx)
{
	int ret = 0;
	int gpio_id = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;
	pr_info("torch_led_index:%d, idx:%d\n", drv_data->torch_led_index, idx);
	idx = drv_data->torch_led_index;
	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[GPIO_CHIP_EN];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
			if (ret)
				goto exit;
		}
	}

	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[GPIO_FLASH_TORCH_MODE];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
			if (ret)
				goto exit;
		}
	}
exit:
	return ret;
}

static int sprd_flash_aw3641_open_preflash(void *drvd, uint8_t idx)
{
	int ret = 0;
	int gpio_id = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;
	pr_info("torch_led_index:%d, idx:%d\n", drv_data->torch_led_index, idx);
	idx = drv_data->torch_led_index;
	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[GPIO_CHIP_EN];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_ON);
			if (ret) {
				goto exit;
			}
		}
	}

exit:
	return ret;
}

static int sprd_flash_aw3641_close_preflash(void *drvd, uint8_t idx)
{
	int ret = 0;
	int gpio_id = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;
	pr_info("torch_led_index:%d, idx:%d\n", drv_data->torch_led_index, idx);
	idx = drv_data->torch_led_index;
	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[GPIO_CHIP_EN];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
			if (ret)
				goto exit;
		}
	}

	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[GPIO_FLASH_TORCH_MODE];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
			if (ret)
				goto exit;
		}
	}
exit:
	return ret;
}

static int sprd_flash_aw3641_open_highlight(void *drvd, uint8_t idx)
{
	int ret = 0;
	int gpio_id = 0;
	ktime_t ktime;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;
	if (!drv_data)
			return -EFAULT;

	pr_info("highlight_opened:%d\n", highlight_opened);
	if(1 ==  highlight_opened) {
		gpio_id = drv_data->gpio_tab[GPIO_FLASH_TORCH_MODE];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
			if (ret)
				goto exit;
		}
		gpio_id = drv_data->gpio_tab[GPIO_CHIP_EN];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
			if (ret)
				goto exit;
		}
		udelay(550);
	}
	idx = drv_data->torch_led_index;
	if(g_high_level == 0){
		gpio_id = drv_data->gpio_tab[GPIO_FLASH_TORCH_MODE];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
			if (ret)
				goto exit;
		}
		gpio_id = drv_data->gpio_tab[GPIO_CHIP_EN];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_ON);
			if (ret) {
				goto exit;
			}
		}
	}else{
		ret = sprd_flash_aw3641_open_pwm(drv_data, idx, g_high_level-1);
	}
//+Bug696101 xuyichen01.wt, MODIFY, 2022/1/19, flash temperature rise protection
	if(1 ==  g_timer_flag){
	    hrtimer_cancel(&g_timeOutTimer);
	    g_timer_flag = 0;
	    pr_info("not close last time,cancel g_timeOutTimer\n");
	}
	timerInit();
	ktime = ktime_set(0, AW3641_SOFTWARE_TIMEOUT * 1000000);// param1:s, param2:ms
	hrtimer_start(&g_timeOutTimer, ktime, HRTIMER_MODE_REL);
	g_timer_flag = 1;
//-Bug696101 xuyichen01.wt, MODIFY, 2022/1/19, flash temperature rise protection
	if (ret)
		goto exit;
	highlight_opened = 1;
exit:
	return ret;
}

static int sprd_flash_aw3641_close_highlight(void *drvd, uint8_t idx)
{
	int ret = 0;
	int gpio_id = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;
	pr_info("torch_led_index:%d, idx:%d\n", drv_data->torch_led_index, idx);
	idx = drv_data->torch_led_index;
	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[GPIO_CHIP_EN];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
			if (ret)
				goto exit;
		}
	}

	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[GPIO_FLASH_TORCH_MODE];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
			if (ret)
				goto exit;
		}
	}
//+Bug696101 xuyichen01.wt, MODIFY, 2022/1/19, flash temperature rise protection

	if(1 ==  g_timer_flag)
	{
	    hrtimer_cancel(&g_timeOutTimer);

	    g_timer_flag = 0;
	}
//-Bug696101 xuyichen01.wt, MODIFY, 2022/1/19, flash temperature rise protection

	highlight_opened = 0;
exit:
	return ret;
}

static int sprd_flash_aw3641_cfg_value_torch(void *drvd, uint8_t idx,
					     struct sprd_flash_element *element)
{
	int ret = 0;
	int gpio_id = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;
	pr_info("torch_led_index:%d, idx:%d\n", drv_data->torch_led_index, idx);
	idx = drv_data->torch_led_index;
	idx |= SPRD_FLASH_LED0;
	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[GPIO_FLASH_TORCH_MODE];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
			if (ret)
				goto exit;
		}
	}
exit:
	return ret;
}

static int sprd_flash_aw3641_cfg_value_preflash(void *drvd, uint8_t idx, struct sprd_flash_element
						*element)
{
	int ret = 0;
	int gpio_id = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;
	pr_info("torch_led_index:%d, idx:%d\n", drv_data->torch_led_index, idx);
	idx = drv_data->torch_led_index;
	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[GPIO_FLASH_TORCH_MODE];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
			if (ret)
				goto exit;
		}
	}
exit:
	return ret;
}

static int sprd_flash_aw3641_cfg_value_highlight(void *drvd, uint8_t idx, struct sprd_flash_element
						 *element)
{
	int ret = 0;
	pr_info("element->index:%d\n", element->index);
	g_high_level = element->index;
	return ret;
}

static const struct of_device_id aw3641_flash_of_match_table[] = {
	{.compatible = "sprd,flash-aw3641"},
};

static const struct sprd_flash_driver_ops flash_gpio_ops = {
	.open_torch = sprd_flash_aw3641_open_torch,
	.close_torch = sprd_flash_aw3641_close_torch,
	.open_preflash = sprd_flash_aw3641_open_preflash,
	.close_preflash = sprd_flash_aw3641_close_preflash,
	.open_highlight = sprd_flash_aw3641_open_highlight,
	.close_highlight = sprd_flash_aw3641_close_highlight,
	.cfg_value_torch = sprd_flash_aw3641_cfg_value_torch,
	.cfg_value_preflash = sprd_flash_aw3641_cfg_value_preflash,
	.cfg_value_highlight = sprd_flash_aw3641_cfg_value_highlight,
};

static int sprd_flash_aw3641_probe(struct platform_device *pdev)
{
	int ret = 0;
	u32 gpio_node = 0;
	struct flash_driver_data *drv_data = NULL;
	int gpio[FLASH_GPIO_MAX];
	int j;

	if (IS_ERR_OR_NULL(pdev))
		return -EINVAL;

	if (!pdev->dev.of_node) {
		pr_err("no device node %s", __func__);
		return -ENODEV;
	}
	pr_info("flash-aw3641 probe\n");

	ret = of_property_read_u32(pdev->dev.of_node, "flash-ic", &gpio_node);
	if (ret || gpio_node != 3641) {
		pr_err("no gpio flash\n");
		return -ENODEV;
	}

	drv_data = devm_kzalloc(&pdev->dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data)
		return -ENOMEM;

	pdev->dev.platform_data = (void *)drv_data;

	ret = of_property_read_u32(pdev->dev.of_node,
				   "torch-led-idx", &drv_data->torch_led_index);
	if (ret)
		drv_data->torch_led_index = SPRD_FLASH_LED0;

	for (j = 0; j < FLASH_GPIO_MAX; j++) {
		gpio[j] = of_get_named_gpio(pdev->dev.of_node,
					    flash_gpio_names[j], 0);
		if (gpio_is_valid(gpio[j])) {
			ret = devm_gpio_request(&pdev->dev,
						gpio[j], flash_gpio_names[j]);

			if (ret) {
				pr_err("flash gpio err\n");
				goto exit;
			}

			ret = gpio_direction_output(gpio[j], SPRD_FLASH_OFF);

			if (ret) {
				pr_err("flash gpio output err\n");
				goto exit;
			}
		}
	}

	memcpy((void *)drv_data->gpio_tab, (void *)gpio, sizeof(gpio));

	ret = sprd_flash_aw3641_init(drv_data);
	if (ret)
		goto exit;
	ret = sprd_flash_register(&flash_gpio_ops, drv_data, SPRD_FLASH_REAR);
//+Bug696101 xuyichen01.wt, MODIFY, 2022/1/19, flash temperature rise protection
	drv_data_tmp = drv_data;
//-Bug696101 xuyichen01.wt, MODIFY, 2022/1/19, flash temperature rise protection

exit:
	return ret;
}

static int sprd_flash_aw3641_remove(struct platform_device *pdev)
{
	int ret = 0;

	ret = sprd_flash_aw3641_deinit(pdev->dev.platform_data);

	return ret;
}

static struct platform_driver sprd_flash_aw3641_driver = {
	.probe = sprd_flash_aw3641_probe,
	.remove = sprd_flash_aw3641_remove,
	.driver = {
		   .name = FLASH_DRIVER_NAME,
		   .of_match_table = of_match_ptr(aw3641_flash_of_match_table),
		   },
};

module_platform_driver(sprd_flash_aw3641_driver);
MODULE_LICENSE("GPL");
