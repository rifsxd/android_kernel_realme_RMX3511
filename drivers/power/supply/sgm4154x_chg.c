/*
 * SGM SGM4154x charger driver
 *
 * Copyright (C) 2021 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/acpi.h>
#include <linux/alarmtimer.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power/charger-manager.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/types.h>
#include <linux/usb/phy.h>
#include <uapi/linux/usb/charger.h>
#include <linux/power/sprd_battery_info.h>
/*odm.hq.bsp.luowenjiang@huaqin.com 2021.12.03 modify for usb temp protection start*/
#include <linux/power/charger-manager.h>
/*odm.hq.bsp.luowenjiang@huaqin.com 2021.12.03 modify for usb temp protection end*/


//#define __SGM41511_CHARGE_DEV__
//#define __SGM41512_CHARGE_DEV__


#define SGM4154x_REG_00				0x00
#define SGM4154x_REG_01				0x01
#define SGM4154x_REG_02				0x02
#define SGM4154x_REG_03				0x03
#define SGM4154x_REG_04				0x04
#define SGM4154x_REG_05				0x05
#define SGM4154x_REG_06				0x06
#define SGM4154x_REG_07				0x07
#define SGM4154x_REG_08				0x08
#define SGM4154x_REG_09				0x09
#define SGM4154x_REG_0A				0x0a
#define SGM4154x_REG_0B				0x0b
#define SGM4154x_REG_0C				0x0c
#define SGM4154x_REG_0D				0x0d
#define SGM4154x_REG_0E				0x0e
#define SGM4154x_REG_0F				0x0f
#define SGM4154x_REG_NUM			16

/* Register 0x00 */
#define REG00_ENHIZ_MASK			GENMASK(7, 7)
#define REG00_ENHIZ_SHIFT			7
#define REG00_HIZ_DISABLE			0
#define REG00_HIZ_ENABLE			1
#define REG00_IINLIM_MASK			GENMASK(4, 0)
#define REG00_IINLIM_SHIFT			0
#define REG00_IINLIM_OFFSET			100
#define REG00_IINLIM_STEP			100
#define REG00_IINLIM_MIN			100
#define REG00_IINLIM_MAX			3800


/* Register 0x01*/
#define REG01_WDT_RESET_MASK			GENMASK(6, 6)
#define REG01_WDT_RESET_SHIFT			6
#define REG01_WDT_RESET				    1
#define REG01_OTG_CONFIG_MASK			GENMASK(5, 5)
#define REG01_OTG_CONFIG_SHIFT			5
#define REG01_OTG_DISABLE			    0
#define REG01_OTG_ENABLE			    1
#define REG01_CHG_CONFIG_MASK			GENMASK(4, 4)
#define REG01_CHG_CONFIG_SHIFT			4
#define REG01_CHG_DISABLE			    0
#define REG01_CHG_ENABLE			    1
#define REG01_SYS_MINV_MASK			    GENMASK(3, 1)
#define REG01_SYS_MINV_SHIFT			1
#define REG01_SYSV_OFFSET			    2600
#define REG01_SYSV_MIN				    2600
#define REG01_SYSV_MAX				    3700

/* Register 0x02*/
#define REG02_BOOSTV_LIM_1200MA		0
#define REG02_BOOSTV_LIM_2000MA		1
#define REG02_ICHG_MASK				GENMASK(5, 0)
#define REG02_BOOSTV_LIM_MASK          GENMASK(7, 7)
#define REG02_BOOSTV_LIM_SHIFT         7

#define REG02_ICHG_SHIFT			0
#define REG02_ICHG_OFFSET			0
#define REG02_ICHG_STEP				60
#define REG02_ICHG_MIN				0
#define REG02_ICHG_MAX				3780

/* Register 0x03 */
#define REG03_IPRECHG_MASK			GENMASK(7, 4)
#define REG03_IPRECHG_SHIFT			4
#define REG03_IPRECHG_OFFSET		60
#define REG03_IPRECHG_STEP			60
#define REG03_IPRECHG_MIN			60
#define REG03_IPRECHG_MAX			780
#define REG03_ITERM_MASK			GENMASK(3, 0)
#define REG03_ITERM_SHIFT			0
#define REG03_ITERM_OFFSET			60
#define REG03_ITERM_STEP			60
#define REG03_ITERM_MIN				60
#define REG03_ITERM_MAX				960

/* Register 0x04*/
#define REG04_VREG_MASK				GENMASK(7, 3)
#define REG04_VREG_SHIFT			3
#define REG04_VREG_STEP				32
#define REG04_VREG_MIN				3856
#define REG04_VREG_OFFSET			3856
#define REG04_VREG_MAX				4624
#define REG04_VRECHG_MASK			GENMASK(0, 0)
#define REG04_VRECHG_SHIFT			0
#define REG04_VRECHG_100MV			0
#define REG04_VRECHG_200MV			1

/* Register 0x05*/
#define REG05_EN_TERM_MASK			GENMASK(7, 7)
#define REG05_EN_TERM_SHIFT			7
#define REG05_TERM_DISABLE			0
#define REG05_TERM_ENABLE			1
#define REG05_WDT_MASK				GENMASK(5, 4)
#define REG05_WDT_SHIFT				4
#define REG05_WDT_DISABLE			0
#define REG05_WDT_40S				1
#define REG05_WDT_80S				2
#define REG05_WDT_160S				3
#define REG05_EN_TIMER_MASK			GENMASK(3, 3)
#define REG05_EN_TIMER_SHIFT		3
#define REG05_CHG_TIMER_DISABLE			0
#define REG05_CHG_TIMER_ENABLE			1
#define REG05_CHG_TIMER_MASK		GENMASK(2, 2)
#define REG05_CHG_TIMER_SHIFT		2
#define REG05_CHG_TIMER_5HOURS			0
#define REG05_CHG_TIMER_10HOURS			1
#define REG05_TREG_MASK				GENMASK(1, 1)
#define REG05_TREG_SHIFT			1
#define REG05_TREG_80				0
#define REG05_TREG_120				1
#define REG05_JEITA_ISET_MASK		GENMASK(0, 0)
#define REG05_JEITA_ISET_SHIFT		0
#define REG05_JEITA_ISET_50PCT			0
#define REG05_JEITA_ISET_20PCT			1
/* Register 0x06*/
#define REG06_BOOSTV_MASK			GENMASK(5, 4)
#define REG06_BOOSTV_SHIFT			4
#define REG06_BOOSTV_OFFSET			4850
#define REG06_BOOSTV_STEP			150
#define REG06_BOOSTV_MIN			4850
#define REG06_BOOSTV_MAX			5300
#define REG06_VINDPM_MASK		    GENMASK(3, 0)
#define REG06_VINDPM_SHIFT		    0
#define REG06_VINDPM_STEP			100
#define REG06_VINDPM_MIN			3900
#define REG06_VINDPM_MAX			12000
#define REG06_OVP_MASK			GENMASK(7, 6)
#define REG06_OVP_SHIFT			6

/* Register 0x07*/
#define REG07_IINDET_EN_MASK			GENMASK(7, 7)
#define REG07_IINDET_EN_SHIFT		7
#define REG07_TMR2X_EN_MASK			GENMASK(6, 6)
#define REG07_TMR2X_EN_SHIFT		6
#define REG07_TMR2X_EN_DISABLE			0
#define REG07_TMR2X_EN_ENABLE			1
#define REG07_BATFET_DIS_MASK		GENMASK(5, 5)
#define REG07_BATFET_DIS_SHIFT		5
#define REG07_BATFET_DIS_DISABLE		0
#define REG07_BATFET_DIS_ENABLE			1
#define REG07_JEITA_VSET_MASK		GENMASK(4, 4)
#define REG07_JEITA_VSET_SHIFT		4
#define REG07_JEITA_VSET_DISABLE		0
#define REG07_JEITA_VSET_ENABLE			1
#define REG07_BATFET_DLY_MASK		GENMASK(3, 3)
#define REG07_BATFET_DLY_SHIFT		3
#define REG07_BATFET_DLY_DISABLE		0
#define REG07_BATFET_DLY_ENABLE			1
#define REG07_BATFET_RST_EN_MASK	GENMASK(2, 2)
#define REG07_BATFET_RST_EN_SHIFT	2
#define REG07_BATFET_RST_EN_DISABLE		0
#define REG07_BATFET_RST_EN_ENABLE		1
#define REG07_VDPM_BAT_TRACK_MASK   GENMASK(1, 0)

/* Register 0x08*/
#define REG08_VBUS_STAT_MASK		GENMASK(7, 5)
#define REG08_VBUS_STAT_SHIFT		5
#define REG08_VBUS_TYPE_USB_SDP			1
#define REG08_VBUS_TYPE_USB_CDP			2
#define REG08_VBUS_TYPE_USB_DCP			3
//#define REG08_VBUS_TYPE_DCP			4
#define REG08_VBUS_TYPE_UNKNOWN			5
#define REG08_VBUS_TYPE_ADAPTER			6
#define REG08_VBUS_TYPE_OTG			7
#define REG08_CHRG_STAT_MASK		GENMASK(4, 3)
#define REG08_CHRG_STAT_SHIFT		3
#define REG08_CHRG_STAT_IDLE			0
#define REG08_CHRG_STAT_PRECHG			1
#define REG08_CHRG_STAT_FASTCHG			2
#define REG08_CHRG_STAT_CHGDONE			3
#define REG08_PG_STAT_MASK			GENMASK(2, 2)
#define REG08_PG_STAT_SHIFT			2
#define REG08_POWER_NOT_GOOD			0
#define REG08_POWER_GOOD			1
#define REG08_VSYS_STAT_MASK		GENMASK(0, 0)
#define REG08_VSYS_STAT_SHIFT		0
#define REG08_NOT_IN_VSYS_STAT			0
#define REG08_IN_VSYS_STAT			1
/* Register 0x09*/
#define REG09_FAULT_WDT_MASK			0x80
#define REG09_FAULT_WDT_SHIFT			7
#define REG09_FAULT_BOOST_MASK			0x40
#define REG09_FAULT_BOOST_SHIFT			6
#define REG09_FAULT_CHRG_MASK			0x30
#define REG09_FAULT_CHRG_SHIFT			4
#define REG09_FAULT_BAT_MASK			0x08
#define REG09_FAULT_BAT_SHIFT			3
#define REG09_FAULT_NTC_MASK			0x07
#define REG09_FAULT_NTC_SHIFT			0
#define REG09_FAULT_WDT				1
#define REG09_FAULT_BOOST			1
#define REG09_FAULT_CHRG_NORMAL			0
#define REG09_FAULT_CHRG_INPUT			1
#define REG09_FAULT_CHRG_THERMAL		2
#define REG09_FAULT_CHRG_TIMER			3
#define REG09_FAULT_BAT_OVP			1
#define REG09_FAULT_NTC_NORMAL			0
#define REG09_FAULT_NTC_WARM			2
#define REG09_FAULT_NTC_COOL			3
#define REG09_FAULT_NTC_COLD			5
#define REG09_FAULT_NTC_HOT			6
/* Register 0x0A*/
#define REG0A_VBUS_GD_MASK			0x80
#define REG0A_VBUS_GD_SHIFT			7
#define REG0A_VDPM_STAT_MASK		GENMASK(6, 6)
#define REG0A_VDPM_STAT_SHIFT		6
#define REG0A_IDPM_STAT_MASK		GENMASK(5, 5)
#define REG0A_IDPM_STAT_SHIFT		5
#define REG0A_VBUS_GD				1
#define REG0A_VDPM_STAT				1
#define REG0A_IDPM_STAT				1
/* Register 0x0B*/
#define REG0B_REG_RESET_MASK		0x80
#define REG0B_REG_RESET_SHIFT		7
#define REG0B_PN_MASK				GENMASK(6, 3)
#define REG0B_PN_SHIFT				3
#define REG0B_DEV_REV_MASK			GENMASK(1, 0)
#define REG0B_DEV_REV_SHIFT			0
#define REG0B_REG_RESET				1
/* Register 0x0D*/
/* DP DM SEL  */
#define REG0D_DP_VSEL_MASK       GENMASK(4, 3)
#define REG0D_DM_VSEL_MASK       GENMASK(2, 1)

/* PUMPX SET  */
#define REG0D_EN_PUMPX           BIT(7)
#define REG0D_PUMPX_UP           BIT(6)
#define REG0D_PUMPX_DN           BIT(5)
/* Register 0x0E*/
#define REG0E_INPUT_DET_DONE     BIT(7)

/* Register 0x0F*/
#define REG0F_VINDPM_OS_MASK     GENMASK(1, 0)
/* Other Realted Definition*/
#define SGM4154x_BATTERY_NAME			"sc27xx-fgu"

#define BIT_DP_DM_BC_ENB			BIT(0)

#define SGM4154x_WDT_VALID_MS			50

#define SGM4154x_OTG_ALARM_TIMER_MS		15000
#define SGM4154x_OTG_VALID_MS			500
#define SGM4154x_OTG_RETRY_TIMES			10

#define SGM4154x_DISABLE_PIN_MASK		BIT(0)
#define SGM4154x_DISABLE_PIN_MASK_2721		BIT(15)

#define SGM4154x_FAST_CHG_VOL_MAX		10500000
#define SGM4154x_NORMAL_CHG_VOL_MAX		6500000
#define SGM4154x_FCHG_OVP_6V			6000
#define SGM4154x_FCHG_OVP_9V			10500

#define SGM4154x_WAKE_UP_MS			2000

#define SGM4154x_FAST_CHARGER_VOLTAGE		9000000
#define SGM4154x_NORMAL_CHARGER_VOLTAGE		5000000

extern int runin_stop;
extern int prj_name;
extern int sc27xx_fgu_bat_id;


struct sgm4154x_charger_sysfs {
	char *name;
	struct attribute_group attr_g;
	struct device_attribute attr_sgm4154x_dump_reg;
	struct device_attribute attr_sgm4154x_lookup_reg;
	struct device_attribute attr_sgm4154x_sel_reg_id;
	struct device_attribute attr_sgm4154x_reg_val;
	struct attribute *attrs[5];

	struct sgm4154x_charger_info *info;
};

struct sgm4154x_charge_current {
	int sdp_limit;
	int sdp_cur;
	int dcp_limit;
	int dcp_cur;
	int cdp_limit;
	int cdp_cur;
	int unknown_limit;
	int unknown_cur;
	int fchg_limit;
	int fchg_cur;
};

struct sgm4154x_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	struct power_supply *psy_usb;
	struct sgm4154x_charge_current cur;
	struct work_struct work;
	struct mutex lock;
	bool charging;
	u32 limit;
	struct delayed_work otg_work;
	struct delayed_work wdt_work;
	struct delayed_work recharge_detect_work;
	struct work_struct qc_detection_work;
	struct regmap *pmic;
	u32 charger_detect;
	u32 charger_pd;
	u32 charger_pd_mask;
	struct gpio_desc *gpiod;
	struct extcon_dev *edev;
	u32 last_limit_current;
	u32 role;
	u32 state;
	bool need_disable_Q1;
	bool hvdcp_checked;
	int qc_dete_count;
	int redetect_count;
	int termination_cur;
	int vol_max_mv;
	u32 actual_limit_current;
	bool otg_enable;
	struct alarm otg_timer;
	struct sgm4154x_charger_sysfs *sysfs;
	int reg_id;
#ifndef VENDOR_KERNEL
	struct workqueue_struct *charge_wq;
	struct work_struct qc_voltage_adjust_work;
	u32 qc_voltage;
#endif //not defined VENDOR_KERNEL
};

struct sgm4154x_charger_reg_tab {
	int id;
	u32 addr;
	char *name;
};

enum SGM4154x_VINDPM_OS {
	VINDPM_OS_3900mV,
	VINDPM_OS_5900mV,
	VINDPM_OS_7500mV,
	VINDPM_OS_10500mV,
};

static struct sgm4154x_charger_reg_tab reg_tab[SGM4154x_REG_NUM + 1] = {
	{0, SGM4154x_REG_00, "Setting Input Limit Current reg"},
	{1, SGM4154x_REG_01, "Related Function Enable reg"},
	{2, SGM4154x_REG_02, "Setting Charge Limit Current reg"},
	{3, SGM4154x_REG_03, "Setting Terminal Current reg"},
	{4, SGM4154x_REG_04, "Setting Charge Limit Voltage reg"},
	{5, SGM4154x_REG_05, "Related Function Config reg"},
	{6, SGM4154x_REG_06, "Setting input Limit Voltage reg"},
	{7, SGM4154x_REG_07, "Related Function Config reg"},
	{8, SGM4154x_REG_08, "Status reg"},
	{9, SGM4154x_REG_09, "Fault reg"},
	{10, SGM4154x_REG_0A, "Stat reg"},
	{11, SGM4154x_REG_0B, "Setting rst reg"},
	{12, SGM4154x_REG_0C, "Jeita Config reg"},
	{13, SGM4154x_REG_0D, "config qc pe reg"},
	{14, SGM4154x_REG_0E, "input done flag reg"},
	{15, SGM4154x_REG_0F, "Related Function Config reg"},
	{16, 0, "null"},
};

int g_adp_support_hvdcp = 0;

struct sgm4154x_charger_info *pinfo;
EXPORT_SYMBOL_GPL(pinfo);

extern void cm_wake_up_usbtemp_thread(void);

static int sgm4154x_charger_set_limit_current(struct sgm4154x_charger_info *info,
					     u32 limit_cur);
static int sgm4154x_force_dpdm(struct sgm4154x_charger_info *info);
enum usb_charger_type sgm4154x_charger_redetect(void);


static int sgm4154x_read(struct sgm4154x_charger_info *info, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int sgm4154x_write(struct sgm4154x_charger_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int sgm4154x_update_bits(struct sgm4154x_charger_info *info, u8 reg,
			       u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = sgm4154x_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return sgm4154x_write(info, reg, v);
}

void sgm4154x_charger_dump_register(struct sgm4154x_charger_info *info)
{
	int i, ret, len, idx = 0;
	u8 reg_val;
	char buf[512];

	memset(buf, '\0', sizeof(buf));
	for (i = 0; i < SGM4154x_REG_NUM; i++) {
		ret = sgm4154x_read(info, reg_tab[i].addr, &reg_val);
		if (ret == 0) {
			len = snprintf(buf + idx, sizeof(buf) - idx,
				       "[REG_0x%.2x]=0x%.2x; ", reg_tab[i].addr,
				       reg_val);
			idx += len;
		}
	}

	dev_info(info->dev, "%s: %s", __func__, buf);
}

EXPORT_SYMBOL_GPL(sgm4154x_charger_dump_register);

static bool sgm4154x_charger_is_bat_present(struct sgm4154x_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	bool present = false;
	int ret;

	psy = power_supply_get_by_name(SGM4154x_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to get psy of sc27xx_fgu\n");
		return present;
	}
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
					&val);
	if (!ret && val.intval)
		present = true;
	power_supply_put(psy);

	if (ret)
		dev_err(info->dev, "Failed to get property of present:%d\n", ret);

	return present;
}

static int sgm4154x_charger_is_fgu_present(struct sgm4154x_charger_info *info)
{
	struct power_supply *psy;

	psy = power_supply_get_by_name(SGM4154x_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to find psy of sc27xx_fgu\n");
		return -ENODEV;
	}
	power_supply_put(psy);

	return 0;
}

int sgm4154x_get_vindpm_offset_os(struct sgm4154x_charger_info *info)
{
	int ret;
	int reg_val;

	ret = regmap_read(info->pmic, SGM4154x_REG_0F, &reg_val);
	if (ret)
		return ret;

	reg_val = reg_val & REG0F_VINDPM_OS_MASK;

	return reg_val;
}

static int sgm4154x_set_vindpm_offset_os(struct sgm4154x_charger_info *info,u8 offset_os)
{
	int ret;

	ret = sgm4154x_update_bits(info, SGM4154x_REG_0F,
				  REG0F_VINDPM_OS_MASK, offset_os);

	if (ret){
		pr_err("%s fail\n",__func__);
		return ret;
	}

	return ret;
}

static int sgm4154x_charger_set_vindpm(struct sgm4154x_charger_info *info, u32 vol)
{
	u8 reg_val;
	u32 offset, os_val;

	if (vol < REG06_VINDPM_MIN)
		vol = REG06_VINDPM_MIN;
	else if (vol > REG06_VINDPM_MAX)
		vol = REG06_VINDPM_MAX;

	if (vol < 5900){
		os_val = VINDPM_OS_3900mV;
		offset = 3900;
	}
	else if (vol >= 5900 && vol < 7500){
		os_val = VINDPM_OS_5900mV;
		offset = 5900; //mv
	}
	else if (vol >= 7500 && vol < 10500){
		os_val = VINDPM_OS_7500mV;
		offset = 7500; //mv
	}
	else{
		os_val = VINDPM_OS_10500mV;
		offset = 10500; //mv
	}

	sgm4154x_set_vindpm_offset_os(info, os_val);
	reg_val = (vol - offset) / REG06_VINDPM_STEP;

	return sgm4154x_update_bits(info, SGM4154x_REG_06,
				   REG06_VINDPM_MASK, reg_val);
}

static int sgm4154x_charger_set_termina_vol(struct sgm4154x_charger_info *info, u32 vol)
{
	u8 reg_val;

	if (vol < REG04_VREG_MIN)
		vol = REG04_VREG_MIN;
	else if (vol >= REG04_VREG_MAX)
		vol = REG04_VREG_MAX;
	reg_val = (vol - REG04_VREG_OFFSET) / REG04_VREG_STEP;

	return sgm4154x_update_bits(info, SGM4154x_REG_04, REG04_VREG_MASK,
				   reg_val << REG04_VREG_SHIFT);
}

static int sgm4154x_charger_set_termina_cur(struct sgm4154x_charger_info *info, u32 cur)
{
	u8 reg_val;

	if (cur <= REG03_ITERM_MIN)
		cur = REG03_ITERM_MIN;
	else if (cur >= REG03_ITERM_MAX)
		cur = REG03_ITERM_MAX;
	reg_val = (cur - REG03_ITERM_OFFSET) / REG03_ITERM_STEP;

	return sgm4154x_update_bits(info, SGM4154x_REG_03, REG03_ITERM_MASK, reg_val);
}

static int sgm4154x_enable_safetytimer(struct sgm4154x_charger_info *info,bool en)
{

	int ret = 0;
	if (en)
		ret = sgm4154x_update_bits(info, SGM4154x_REG_05,
		REG05_EN_TIMER_MASK, REG05_CHG_TIMER_ENABLE);
	else
		ret = sgm4154x_update_bits(info, SGM4154x_REG_05,
		REG05_EN_TIMER_MASK, REG05_CHG_TIMER_DISABLE);

	return ret;
}

static int sgm4154x_charger_set_current(struct sgm4154x_charger_info *info,
				       u32 cur)
{
	u8 reg_val;
	int ret;

	cur = cur / 1000;
	if (cur <= REG02_ICHG_MIN)
		cur = REG02_ICHG_MIN;
	else if (cur >= REG02_ICHG_MAX)
		cur = REG02_ICHG_MAX;
	reg_val = cur / REG02_ICHG_STEP;

	ret = sgm4154x_update_bits(info, SGM4154x_REG_02, REG02_ICHG_MASK, reg_val);

	return ret;
}

static bool sgm4154x_charger_dpdm_detect_is_done(struct sgm4154x_charger_info *info)
{
	u8 reg_val;
	int ret;

	ret = sgm4154x_read(info, SGM4154x_REG_0E, &reg_val);
	if (ret < 0)
		return ret;

	return (reg_val & REG0E_INPUT_DET_DONE) ? true : false;
}

static int sgm4154x_charger_get_type(struct sgm4154x_charger_info *info)
{
    u8 reg_val;
    int ret;

    ret = sgm4154x_read(info, SGM4154x_REG_08, &reg_val);
    if (ret < 0)
        return ret;

    return reg_val;
}

static int sgm4154x_charger_get_current(struct sgm4154x_charger_info *info,
				       u32 *cur)
{
	u8 reg_val;
	int ret;

	ret = sgm4154x_read(info, SGM4154x_REG_02, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG02_ICHG_MASK;
	*cur = reg_val * REG02_ICHG_STEP * 1000;

	return 0;
}

int sgm4154x_get_main_charger_current(void)
{
	int cur = 0;
	if (NULL != pinfo && pinfo->charging) {
		sgm4154x_charger_get_current(pinfo, &cur);
	}

	return cur;
}

static int sgm4154x_charger_set_limit_current(struct sgm4154x_charger_info *info,
					     u32 limit_cur)
{
	u8 reg_val;
	int ret;

	limit_cur = limit_cur / 1000;
	if (limit_cur >= REG00_IINLIM_MAX)
		limit_cur = REG00_IINLIM_MAX;
	else if (limit_cur <= REG00_IINLIM_MIN)
		limit_cur = REG00_IINLIM_MIN;
	else if (limit_cur > 3100 && limit_cur < REG00_IINLIM_MAX)
		limit_cur = 3100;

	info->last_limit_current = limit_cur * 1000;
	reg_val = (limit_cur - REG00_IINLIM_OFFSET) / REG00_IINLIM_STEP;

	ret = sgm4154x_update_bits(info, SGM4154x_REG_00, REG00_IINLIM_MASK, reg_val);
	if (ret)
		dev_err(info->dev, "set sgm4154x limit cur failed\n");

	info->actual_limit_current =
		(reg_val * REG00_IINLIM_STEP + REG00_IINLIM_OFFSET) * 1000;

	return ret;
}

#ifndef VENDOR_KERNEL
static int sgm4154x_charger_force_limit_current_to_100ma(struct sgm4154x_charger_info *info,
					     u32 limit_cur)
{
	u8 reg_val;
	int ret;


	limit_cur = 100;

	info->last_limit_current = limit_cur * 1000;
	reg_val = (limit_cur - REG00_IINLIM_OFFSET) / REG00_IINLIM_STEP;

	ret = sgm4154x_update_bits(info, SGM4154x_REG_00, REG00_IINLIM_MASK, reg_val);
	if (ret)
		dev_err(info->dev, "set sgm4154x limit cur failed\n");

	info->actual_limit_current =
		(reg_val * REG00_IINLIM_STEP + REG00_IINLIM_OFFSET) * 1000;
	dev_err(info->dev, "sgm4154x_charger_force_limit_current_to_100ma\n");

	return ret;
}
#endif //not defined VENDOR_KERNEL

static u32 sgm4154x_charger_get_limit_current(struct sgm4154x_charger_info *info,
					     u32 *limit_cur)
{
	u8 reg_val;
	int ret;

	ret = sgm4154x_read(info, SGM4154x_REG_00, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG00_IINLIM_MASK;

	if (reg_val == REG00_IINLIM_MASK)
		*limit_cur = REG00_IINLIM_MAX * 1000;
	else
		*limit_cur = (reg_val * REG00_IINLIM_STEP + REG00_IINLIM_OFFSET)* 1000;

	return 0;
}

int sgm4154x_enable_qc20_hvdcp_9v(struct sgm4154x_charger_info *info)
{
	int ret, val, dp_val;

	/*dp and dm connected,dp 0.6V dm 0.6V*/
    dp_val = 0x2<<3;
    ret = sgm4154x_update_bits(info, SGM4154x_REG_0D,
				  REG0D_DP_VSEL_MASK, dp_val); //dp 0.6V
    if (ret)
		return ret;

    msleep(2500);

	val = 0xe << 1;
	ret = sgm4154x_update_bits(info, SGM4154x_REG_0D,
				  GENMASK(4,1), val); //dp 3.3v dm 0.6v
#ifndef VENDOR_KERNEL
	sgm4154x_charger_force_limit_current_to_100ma(info, 100);
#endif //not defined VENDOR_KERNEL

	return ret;
}

int sgm4154x_enable_qc20_hvdcp_12v(struct sgm4154x_charger_info *info)
{
	int ret;
	int dp_val, dm_val;

	return 0;

	/*dp and dm connected,dp 0.6V dm 0.6V*/
    dp_val = 0x2<<3;
    ret = sgm4154x_update_bits(info, SGM4154x_REG_0D,
				  REG0D_DP_VSEL_MASK, dp_val); //dp 0.6V
    if (ret)
	{
		return ret;
	}

	dm_val = 0x2<<1;

	ret = sgm4154x_update_bits(info, SGM4154x_REG_0D,
				  REG0D_DM_VSEL_MASK, dm_val); //dm 0.6V
    if (ret)
        return ret;
    mdelay(1000);

    dm_val = 0x2;
	ret = sgm4154x_update_bits(info, SGM4154x_REG_0D,
				  REG0D_DM_VSEL_MASK, dm_val); //dm 0V
	mdelay(1);
	/* dp 0.6v and dm 0.6v out 12V */
	dp_val = 0x2<<3;
	ret = sgm4154x_update_bits(info, SGM4154x_REG_0D,
				  REG0D_DP_VSEL_MASK, dp_val); //dp 0.6v
	if (ret)
		return ret;
	//mdelay(1250);

	dm_val = 0x2<<1;
	ret = sgm4154x_update_bits(info, SGM4154x_REG_0D,
				  REG0D_DM_VSEL_MASK, dm_val); //dm 0.6v

	return ret;
}

int sgm4154x_disable_hvdcp(struct sgm4154x_charger_info *info)
{
	int ret = 0, val = 0;

	dev_info(info->dev, "disable hvdcp\n");

	/*dp and dm connected,dp 0.6V dm Hiz*/
	val = 0x8 << 1;
    ret = sgm4154x_update_bits(info, SGM4154x_REG_0D,
				  GENMASK(4,1), val);

	return ret;
}

void sc27xx_force_dpdm(void)
{
	int val;

	if (NULL == pinfo)
		return;

    val = 0;
    sgm4154x_update_bits(pinfo, SGM4154x_REG_0D,
				  GENMASK(4,1), val); //dp/dm Hiz
}

static int sgm4154x_force_dpdm(struct sgm4154x_charger_info *info)
{
	int ret;

	ret = sgm4154x_update_bits(info, SGM4154x_REG_07,
						REG07_IINDET_EN_MASK,
						1 << REG07_IINDET_EN_SHIFT);

	return ret;

}

static int sgm4154x_charger_get_vbus(struct sgm4154x_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(SGM4154x_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to get psy of sc27xx_fgu\n");
		return false;
	}
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
					&val);
	power_supply_put(psy);

	return val.intval;
}

bool sgm4154x_is_hvdcp(struct sgm4154x_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	bool hvdcp = false;
	int ret;

	psy = power_supply_get_by_name(SGM4154x_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to get psy of sc27xx_fgu\n");
		return hvdcp;
	}
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
					&val);
	if (!ret && val.intval > 8000 * 1000)
		hvdcp = true;
	power_supply_put(psy);

	dev_err(info->dev, "finn vbus is %d\n", val.intval);

	if (ret)
		dev_err(info->dev, "Failed to get property of present:%d\n", ret);

	return hvdcp;
}

static int sgm4154x_qc_adjust_voltage(struct sgm4154x_charger_info *info, u32 value)
{
	int i = 0, cnt = 0, ret = 0;
	int val = 0xe << 1;

	if (value == SGM4154x_NORMAL_CHARGER_VOLTAGE) {
		ret = sgm4154x_disable_hvdcp(info);
		dev_info(info->dev, "adjust 5v success\n");
		return  ret;
	} else if (value == SGM4154x_FAST_CHARGER_VOLTAGE) {
		ret = sgm4154x_update_bits(info, SGM4154x_REG_0D,
				  GENMASK(4,1), val); //dp 3.3v dm 0.6v

		for (i = 0; i < 10; i++) {
			if (sgm4154x_is_hvdcp(info))
				break;
			msleep(100);
		}

		if (!sgm4154x_is_hvdcp(info))
		{
			do {
				sc27xx_force_dpdm();
				mdelay(10);
				ret = sgm4154x_enable_qc20_hvdcp_9v(info);
				msleep(100);
			} while (!sgm4154x_is_hvdcp(info) && cnt++ < 5);
		}

		if (sgm4154x_is_hvdcp(info))
			dev_info(info->dev, "adjust 9v success\n");
		else
			dev_info(info->dev, "adjust 9v failed\n");
		return ret;
	}
	return -EINVAL;
}

#ifndef VENDOR_KERNEL
static void sgm4154x_charger_qc_adjust_voltage_work(struct work_struct *work)
{
	struct sgm4154x_charger_info *info =
		container_of(work, struct sgm4154x_charger_info, qc_voltage_adjust_work);

	dev_info(info->dev, "sgm4154x_charger_qc_adjust_voltage_work vol = %d\n", info->qc_voltage);
	sgm4154x_qc_adjust_voltage(info, info->qc_voltage);
}
#endif //not defined VENDOR_KERNEL

static void sgm4154x_charger_qc_detection_work(struct work_struct *work)
{
	struct sgm4154x_charger_info *info =
		container_of(work, struct sgm4154x_charger_info, qc_detection_work);
	bool flag = false;
	int vbus;
	int count = 0, ret = 0;

	dev_info(info->dev, "fchg %s qc start detect type  %d\n", __func__, info->usb_phy->chg_type);

	if (info->usb_phy->chg_type != DCP_TYPE)
		return;

	while (!sgm4154x_is_hvdcp(info) &&(info->usb_phy->chg_type == DCP_TYPE)) {
		vbus = sgm4154x_charger_get_vbus(info);
		if (vbus < 3000000)
			break;

		count++;
		if (count > 60)
			break;

		if (!flag) {
			dev_info(info->dev, "fchg %s count = %d\n", __func__, count);
			mdelay(20);

			sgm4154x_enable_qc20_hvdcp_9v(info);
		}

		flag = true;

		msleep(50);
	}

	if (sgm4154x_is_hvdcp(info)) {
		dev_err(info->dev, "%s qc identify success\n", __func__);
		g_adp_support_hvdcp = 1;
		info->state = POWER_SUPPLY_USB_TYPE_PD;
		power_supply_changed(info->psy_usb);
		ret = sgm4154x_charger_set_limit_current(info, info->cur.dcp_limit);
		if (ret)
			dev_info(info->dev, "%s set limit current fail\n", __func__);
		sgm4154x_disable_hvdcp(info);
	} else {
		if (info->usb_phy->chg_type == DCP_TYPE) {
			ret = sgm4154x_charger_set_limit_current(info, info->cur.dcp_limit);
			if (ret)
				dev_info(info->dev, "%s set dcp limit current fail\n", __func__);
		} else if (info->usb_phy->chg_type == UNKNOWN_TYPE) {
			ret = sgm4154x_charger_set_limit_current(info, info->cur.unknown_limit);
			if (ret)
				dev_info(info->dev, "%s set unknown limit current fail\n", __func__);
		}
		dev_info(info->dev, "fchg %s qc identify failed, try again, dete_count = %d\n", __func__, info->qc_dete_count);
		if (info->qc_dete_count < 3 && vbus > 3000000)
		{
			info->qc_dete_count++;
#ifdef VENDOR_KERNEL
			schedule_work(&info->qc_detection_work);
#else //not defined VENDOR_KERNEL
			queue_work(info->charge_wq, &info->qc_detection_work);
#endif //defined VENDOR_KERNEL
		}
	}
}

int sgm4154x_set_prechrg_curr(struct sgm4154x_charger_info *info,int pre_current)
{
	int reg_val;
	int offset = REG03_IPRECHG_OFFSET;
	int ret = 0;

	if (pre_current < REG03_IPRECHG_MIN)
		pre_current = REG03_IPRECHG_MIN;
	else if (pre_current > REG03_IPRECHG_MAX)
		pre_current = REG03_IPRECHG_MAX;
	reg_val = (pre_current - offset) / REG03_IPRECHG_STEP;

	printk("chg_sgm4154x_set_prechrg_curr:%d\n",reg_val);

	ret = sgm4154x_update_bits(info, SGM4154x_REG_03, REG03_IPRECHG_MASK, reg_val << REG03_IPRECHG_SHIFT);
	if (ret)
		dev_err(info->dev, "set sgm4154x pre cur failed\n");

	return ret;
}

EXPORT_SYMBOL_GPL(sgm4154x_set_prechrg_curr);

static int sgm4154x_vdmp_bat_track_set(struct sgm4154x_charger_info *info, u32 vol)
{
	u8 reg_val;

	reg_val = vol;
	return sgm4154x_update_bits(info, SGM4154x_REG_07, REG07_VDPM_BAT_TRACK_MASK,
                                  reg_val);
}

static int sgm4154x_charger_hw_init(struct sgm4154x_charger_info *info)
{
	struct sprd_battery_info bat_info;
	int ret;
	int num = 0;

	if (sc27xx_fgu_bat_id == 2)
		num = 1;

	ret = sprd_battery_get_battery_info(info->psy_usb, &bat_info, num);
	if (ret) {
		dev_warn(info->dev, "no battery information is supplied\n");

		/*
		 * If no battery information is supplied, we should set
		 * default charge termination current to 100 mA, and default
		 * charge termination voltage to 4.2V.
		 */
		info->cur.sdp_limit = 500000;
		info->cur.sdp_cur = 500000;
		info->cur.dcp_limit = 5000000;
		info->cur.dcp_cur = 500000;
		info->cur.cdp_limit = 5000000;
		info->cur.cdp_cur = 1500000;
		info->cur.unknown_limit = 5000000;
		info->cur.unknown_cur = 500000;
	} else {
		info->cur.sdp_limit = bat_info.cur.sdp_limit;
		info->cur.sdp_cur = bat_info.cur.sdp_cur;
		info->cur.dcp_limit = bat_info.cur.dcp_limit;
		info->cur.dcp_cur = bat_info.cur.dcp_cur;
		info->cur.cdp_limit = bat_info.cur.cdp_limit;
		info->cur.cdp_cur = bat_info.cur.cdp_cur;
		info->cur.unknown_limit = bat_info.cur.unknown_limit;
		info->cur.unknown_cur = bat_info.cur.unknown_cur;
		info->cur.fchg_limit = bat_info.cur.fchg_limit;
		info->cur.fchg_cur = bat_info.cur.fchg_cur;

		info->vol_max_mv = bat_info.constant_charge_voltage_max_uv / 1000;
		info->termination_cur = bat_info.charge_term_current_ua / 1000;
		sprd_battery_put_battery_info(info->psy_usb, &bat_info);

		ret = sgm4154x_update_bits(info, SGM4154x_REG_0B, REG0B_REG_RESET_MASK,
					  REG0B_REG_RESET << REG0B_REG_RESET_SHIFT);
		if (ret) {
			dev_err(info->dev, "reset sgm4154x failed\n");
			return ret;
		}

		sgm4154x_disable_hvdcp(info);

		ret = sgm4154x_charger_set_vindpm(info, info->vol_max_mv);
		if (ret) {
			dev_err(info->dev, "set sgm4154x vindpm vol failed\n");
			return ret;
		}

		ret = sgm4154x_charger_set_termina_vol(info,
						      info->vol_max_mv);
		if (ret) {
			dev_err(info->dev, "set sgm4154x terminal vol failed\n");
			return ret;
		}

		ret = sgm4154x_charger_set_termina_cur(info, info->termination_cur);
		if (ret) {
			dev_err(info->dev, "set sgm4154x terminal cur failed\n");
			return ret;
		}

		ret = sgm4154x_charger_set_limit_current(info,
							info->cur.unknown_cur);
		if (ret)
			dev_err(info->dev, "set sgm4154x limit current failed\n");

		ret = sgm4154x_enable_safetytimer(info,false);
		if (ret)
			dev_err(info->dev, "set sgm4154x disable safetytimer failed\n");

		ret = sgm4154x_set_prechrg_curr(info,360);
		if (ret)
			dev_err(info->dev, "set sgm4154x set prechrg failed\n");

		ret = sgm4154x_charger_set_vindpm(info,4500);
		if (ret)
           	dev_err(info->dev, "set sgm4154x set vindpm failed\n");

		ret = sgm4154x_vdmp_bat_track_set(info,1);
		if (ret)
			dev_err(info->dev, "set sgm4154x set bat tarck set failed\n");

	}

	return ret;
}

static int sgm4154x_charger_get_charge_voltage(struct sgm4154x_charger_info *info,
					      u32 *charge_vol)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(SGM4154x_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "failed to get SGM4154x_BATTERY_NAME\n");
		return -ENODEV;
	}

	ret = power_supply_get_property(psy,
					POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
					&val);
	power_supply_put(psy);
	if (ret) {
		dev_err(info->dev, "failed to get CONSTANT_CHARGE_VOLTAGE\n");
		return ret;
	}

	*charge_vol = val.intval;

	return 0;
}

static int sgm4154x_charger_start_charge(struct sgm4154x_charger_info *info)
{
	int ret;

	dev_info(info->dev, "sgm4154x_charger_start_charge\n");
	ret = sgm4154x_update_bits(info, SGM4154x_REG_00,
				  REG00_ENHIZ_MASK, REG00_HIZ_DISABLE);
	if (ret)
		dev_err(info->dev, "disable HIZ mode failed\n");

	ret = sgm4154x_update_bits(info, SGM4154x_REG_05, REG05_WDT_MASK,
				  REG05_WDT_40S << REG05_WDT_SHIFT);
	if (ret) {
		dev_err(info->dev, "Failed to enable sgm4154x watchdog\n");
		return ret;
	}

	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask, 0);
	if (ret) {
		dev_err(info->dev, "enable sgm4154x charge failed\n");
			return ret;
		}

	ret = sgm4154x_charger_set_limit_current(info,
						info->last_limit_current);
	if (ret) {
		dev_err(info->dev, "failed to set limit current\n");
		return ret;
	}

	ret = sgm4154x_charger_set_termina_cur(info, info->termination_cur);
	if (ret)
		dev_err(info->dev, "set sgm4154x terminal cur failed\n");

	dev_info(info->dev, "sgm4154x aquire charger wakelock\n");
	pm_stay_awake(info->dev);

	return ret;
}

static void sgm4154x_charger_stop_charge(struct sgm4154x_charger_info *info, bool present)
{
	int ret;

	dev_info(info->dev, "release charger wakelock\n");
	pm_relax(info->dev);

	dev_info(info->dev, "sgm4154x_charger_stop_charge\n");
	if (!present || info->need_disable_Q1) {
		ret = sgm4154x_update_bits(info, SGM4154x_REG_00, REG00_ENHIZ_MASK,
					  REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT);
		if (ret)
			dev_err(info->dev, "enable HIZ mode failed\n");
		info->need_disable_Q1 = false;
	}

	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask,
				 info->charger_pd_mask);
	if (ret)
		dev_err(info->dev, "disable sgm4154x charge failed\n");

	ret = sgm4154x_update_bits(info, SGM4154x_REG_05, REG05_WDT_MASK,
				  REG05_WDT_DISABLE);
	if (ret)
		dev_err(info->dev, "Failed to disable sgm4154x watchdog\n");

}

int sgm4154x_set_hiz_en(struct sgm4154x_charger_info *info, bool hiz_en)
{
	int reg_val;
	int ret;
	dev_info(info->dev,"%s:%d", __func__, hiz_en);

	reg_val = hiz_en ? REG00_HIZ_ENABLE : REG00_HIZ_DISABLE;
	ret = sgm4154x_update_bits(info, SGM4154x_REG_00, REG00_ENHIZ_MASK,
		reg_val << REG00_ENHIZ_SHIFT);
	if (ret)
		dev_err(info->dev, "set HIZ mode failed\n");

	return ret;
}

int sgm4154x_enable_shipmode(struct sgm4154x_charger_info *info,bool en)
{

	int ret;
	dev_info(info->dev,"%s:%d", __func__, en);

	if (en)
		ret = sgm4154x_update_bits(info, SGM4154x_REG_07,
		REG07_BATFET_DIS_MASK, REG07_BATFET_DIS_ENABLE << REG07_BATFET_DIS_SHIFT);
	else
		ret = sgm4154x_update_bits(info, SGM4154x_REG_07,
		REG07_BATFET_DIS_MASK, REG07_BATFET_DIS_DISABLE << REG07_BATFET_DIS_SHIFT);

	return ret;
}


static inline int sgm4154x_charger_get_health(struct sgm4154x_charger_info *info,
				      u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static inline int sgm4154x_charger_get_online(struct sgm4154x_charger_info *info,
				      u32 *online)
{
	if (info->limit)
		*online = true;
	else
		*online = false;

	return 0;
}

static int sgm4154x_charger_feed_watchdog(struct sgm4154x_charger_info *info)
{
	int ret;
	u32 limit_cur = 0;

	ret = sgm4154x_update_bits(info, SGM4154x_REG_01, REG01_WDT_RESET_MASK,
				  REG01_WDT_RESET << REG01_WDT_RESET_SHIFT);
	if (ret) {
		dev_err(info->dev, "reset sgm4154x failed\n");
		return ret;
	}

	if (info->otg_enable)
		return 0;

	ret = sgm4154x_charger_get_limit_current(info, &limit_cur);
	if (ret) {
		dev_err(info->dev, "get limit cur failed\n");
		return ret;
	}

	if (info->actual_limit_current == limit_cur)
		return 0;

	ret = sgm4154x_charger_set_limit_current(info, info->actual_limit_current);
	if (ret) {
		dev_err(info->dev, "set limit cur failed\n");
		return ret;
	}

	return 0;
}

static inline int sgm4154x_charger_get_status(struct sgm4154x_charger_info *info)
{
	if (info->charging)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static int sgm4154x_charger_set_status(struct sgm4154x_charger_info *info,
				      int val, u32 input_vol, bool bat_present)
{
	int ret = 0;

	if (val == CM_FAST_CHARGE_OVP_ENABLE_CMD) {
		dev_err(info->dev, "maybe need to set vindpm\n");//??
	} else if (val == CM_FAST_CHARGE_OVP_DISABLE_CMD) {
		if (input_vol > SGM4154x_FAST_CHG_VOL_MAX)
			info->need_disable_Q1 = true;
	} else if (val == false) {
		if (input_vol > SGM4154x_NORMAL_CHG_VOL_MAX)
			info->need_disable_Q1 = true;
	}

	if (val > CM_FAST_CHARGE_NORMAL_CMD)
		return 0;

	if (!val && info->charging) {
		dev_info(info->dev,"set status stop charging");
		sgm4154x_charger_stop_charge(info, bat_present);
		g_adp_support_hvdcp = 0;
		info->charging = false;
	} else if (val && !info->charging) {
		dev_info(info->dev,"set status start charging");
		ret = sgm4154x_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}

					  
  static void sgm4154x_charger_redetect_work(struct work_struct *work)
  {
	  struct delayed_work *dwork = to_delayed_work(work);
	  struct sgm4154x_charger_info *info = container_of(dwork,
							   struct sgm4154x_charger_info,
							   recharge_detect_work);
	  enum usb_charger_type usb_type = UNKNOWN_TYPE;
	  if ((!info) || (!info->usb_phy))
	  	return;

	  if (info->usb_phy->chg_state != USB_CHARGER_PRESENT)
	  	return;

	  if (info->usb_phy->chg_type != UNKNOWN_TYPE) {
#ifndef VENDOR_KERNEL
		if (info->usb_phy->chg_type == DCP_TYPE)
			schedule_work(&info->work);
#endif //not defined VENDOR_KERNEL
	  	return;
	  }
	  usb_type = sgm4154x_charger_redetect();
	  pr_info("sgm4154x_charger_redetect_work usb_type %d\n", usb_type);


	  if ((usb_type != DCP_TYPE) && (info->redetect_count < 2)) {
	  	schedule_delayed_work(&info->recharge_detect_work, HZ);
		info->redetect_count ++;
	  }
	  else {
	  	info->usb_phy->chg_type = usb_type;
		schedule_work(&info->work);
	  }
  }


static void sgm4154x_charger_work(struct work_struct *data)
{
	struct sgm4154x_charger_info *info =
		container_of(data, struct sgm4154x_charger_info, work);
	bool present;
	int vbus = 0;

	present = sgm4154x_charger_is_bat_present(info);
	vbus = sgm4154x_charger_get_vbus(info);

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	if (info->limit)
		schedule_delayed_work(&info->wdt_work, 0);
	else
		cancel_delayed_work_sync(&info->wdt_work);

	dev_info(info->dev, "battery present = %d, charger type = %d, vbus = %d\n",
		 present, info->usb_phy->chg_type, vbus);
	if(runin_stop == 0 || !info->limit){
		cm_notify_event(info->psy_usb, CM_EVENT_CHG_START_STOP, NULL);

		if ((vbus > 3000000) && (info->usb_phy->chg_type == DCP_TYPE || info->usb_phy->chg_type == UNKNOWN_TYPE))
		{
			if (info->usb_phy->chg_type == DCP_TYPE) {
			  pr_info("sgm4154x_charger_work start qc detect\n");
			  info->qc_dete_count = 0;
			  mdelay(1);
#ifdef VENDOR_KERNEL
			  schedule_work(&info->qc_detection_work);
#else //not defined VENDOR_KERNEL
			  queue_work(info->charge_wq, &info->qc_detection_work);
#endif //defined VENDOR_KERNEL
			} else {
			  pr_info("sgm4154x_charger_work start redetect work\n");
              schedule_delayed_work(&info->recharge_detect_work, HZ);
			}
		}
	}
}

static ssize_t sgm4154x_reg_val_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sgm4154x_charger_sysfs *sgm4154x_sysfs =
		container_of(attr, struct sgm4154x_charger_sysfs,
			     attr_sgm4154x_reg_val);
	struct sgm4154x_charger_info *info = sgm4154x_sysfs->info;
	u8 val;
	int ret;

	if (!info)
		return sprintf(buf, "%s sgm4154x_sysfs->info is null\n", __func__);

	ret = sgm4154x_read(info, reg_tab[info->reg_id].addr, &val);
	if (ret) {
		dev_err(info->dev, "fail to get sgm4154x_REG_0x%.2x value, ret = %d\n",
			reg_tab[info->reg_id].addr, ret);
		return sprintf(buf, "fail to get sgm4154x_REG_0x%.2x value\n",
			       reg_tab[info->reg_id].addr);
	}

	return sprintf(buf, "sgm4154x_REG_0x%.2x = 0x%.2x\n",
		       reg_tab[info->reg_id].addr, val);
}

static ssize_t sgm4154x_reg_val_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	struct sgm4154x_charger_sysfs *sgm4154x_sysfs =
		container_of(attr, struct sgm4154x_charger_sysfs,
			     attr_sgm4154x_reg_val);
	struct sgm4154x_charger_info *info = sgm4154x_sysfs->info;
	u8 val;
	int ret;

	if (!info) {
		dev_err(dev, "%s sgm4154x_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtou8(buf, 16, &val);
	if (ret) {
		dev_err(info->dev, "fail to get addr, ret = %d\n", ret);
		return count;
	}

	ret = sgm4154x_write(info, reg_tab[info->reg_id].addr, val);
	if (ret) {
		dev_err(info->dev, "fail to wite 0x%.2x to REG_0x%.2x, ret = %d\n",
				val, reg_tab[info->reg_id].addr, ret);
		return count;
	}

	dev_info(info->dev, "wite 0x%.2x to REG_0x%.2x success\n", val,
		 reg_tab[info->reg_id].addr);
	return count;
}

static ssize_t sgm4154x_reg_id_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct sgm4154x_charger_sysfs *sgm4154x_sysfs =
		container_of(attr, struct sgm4154x_charger_sysfs,
			     attr_sgm4154x_sel_reg_id);
	struct sgm4154x_charger_info *info = sgm4154x_sysfs->info;
	int ret, id;

	if (!info) {
		dev_err(dev, "%s sgm4154x_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtoint(buf, 10, &id);
	if (ret) {
		dev_err(info->dev, "%s store register id fail\n", sgm4154x_sysfs->name);
		return count;
	}

	if (id < 0 || id >= SGM4154x_REG_NUM) {
		dev_err(info->dev, "%s store register id fail, id = %d is out of range\n",
			sgm4154x_sysfs->name, id);
		return count;
	}

	info->reg_id = id;

	dev_info(info->dev, "%s store register id = %d success\n", sgm4154x_sysfs->name, id);
	return count;
}

static ssize_t sgm4154x_reg_id_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sgm4154x_charger_sysfs *sgm4154x_sysfs =
		container_of(attr, struct sgm4154x_charger_sysfs,
			     attr_sgm4154x_sel_reg_id);
	struct sgm4154x_charger_info *info = sgm4154x_sysfs->info;

	if (!info)
		return sprintf(buf, "%s sgm4154x_sysfs->info is null\n", __func__);

	return sprintf(buf, "Cuurent register id = %d\n", info->reg_id);
}

static ssize_t sgm4154x_reg_table_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sgm4154x_charger_sysfs *sgm4154x_sysfs =
		container_of(attr, struct sgm4154x_charger_sysfs,
			     attr_sgm4154x_lookup_reg);
	struct sgm4154x_charger_info *info = sgm4154x_sysfs->info;
	int i, len, idx = 0;
	char reg_tab_buf[2048];

	if (!info)
		return sprintf(buf, "%s sgm4154x_sysfs->info is null\n", __func__);

	memset(reg_tab_buf, '\0', sizeof(reg_tab_buf));
	len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
		       "Format: [id] [addr] [desc]\n");
	idx += len;

	for (i = 0; i < SGM4154x_REG_NUM; i++) {
		len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
			       "[%d] [REG_0x%.2x] [%s]; \n",
			       reg_tab[i].id, reg_tab[i].addr, reg_tab[i].name);
		idx += len;
	}

	return sprintf(buf, "%s\n", reg_tab_buf);
}

static ssize_t sgm4154x_dump_reg_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct sgm4154x_charger_sysfs *sgm4154x_sysfs =
		container_of(attr, struct sgm4154x_charger_sysfs,
			     attr_sgm4154x_dump_reg);
	struct sgm4154x_charger_info *info = sgm4154x_sysfs->info;

	if (!info)
		return sprintf(buf, "%s sgm4154x_sysfs->info is null\n", __func__);

	sgm4154x_charger_dump_register(info);

	return sprintf(buf, "%s\n", sgm4154x_sysfs->name);
}

static int sgm4154x_register_sysfs(struct sgm4154x_charger_info *info)
{
	struct sgm4154x_charger_sysfs *sgm4154x_sysfs;
	int ret;

	sgm4154x_sysfs = devm_kzalloc(info->dev, sizeof(*sgm4154x_sysfs), GFP_KERNEL);
	if (!sgm4154x_sysfs)
		return -ENOMEM;

	info->sysfs = sgm4154x_sysfs;
	sgm4154x_sysfs->name = "sgm4154x_sysfs";
	sgm4154x_sysfs->info = info;
	sgm4154x_sysfs->attrs[0] = &sgm4154x_sysfs->attr_sgm4154x_dump_reg.attr;
	sgm4154x_sysfs->attrs[1] = &sgm4154x_sysfs->attr_sgm4154x_lookup_reg.attr;
	sgm4154x_sysfs->attrs[2] = &sgm4154x_sysfs->attr_sgm4154x_sel_reg_id.attr;
	sgm4154x_sysfs->attrs[3] = &sgm4154x_sysfs->attr_sgm4154x_reg_val.attr;
	sgm4154x_sysfs->attrs[4] = NULL;
	sgm4154x_sysfs->attr_g.name = "debug";
	sgm4154x_sysfs->attr_g.attrs = sgm4154x_sysfs->attrs;

	sysfs_attr_init(&sgm4154x_sysfs->attr_sgm4154x_dump_reg.attr);
	sgm4154x_sysfs->attr_sgm4154x_dump_reg.attr.name = "sgm4154x_dump_reg";
	sgm4154x_sysfs->attr_sgm4154x_dump_reg.attr.mode = 0444;
	sgm4154x_sysfs->attr_sgm4154x_dump_reg.show = sgm4154x_dump_reg_show;

	sysfs_attr_init(&sgm4154x_sysfs->attr_sgm4154x_lookup_reg.attr);
	sgm4154x_sysfs->attr_sgm4154x_lookup_reg.attr.name = "sgm4154x_lookup_reg";
	sgm4154x_sysfs->attr_sgm4154x_lookup_reg.attr.mode = 0444;
	sgm4154x_sysfs->attr_sgm4154x_lookup_reg.show = sgm4154x_reg_table_show;

	sysfs_attr_init(&sgm4154x_sysfs->attr_sgm4154x_sel_reg_id.attr);
	sgm4154x_sysfs->attr_sgm4154x_sel_reg_id.attr.name = "sgm4154x_sel_reg_id";
	sgm4154x_sysfs->attr_sgm4154x_sel_reg_id.attr.mode = 0644;
	sgm4154x_sysfs->attr_sgm4154x_sel_reg_id.show = sgm4154x_reg_id_show;
	sgm4154x_sysfs->attr_sgm4154x_sel_reg_id.store = sgm4154x_reg_id_store;

	sysfs_attr_init(&sgm4154x_sysfs->attr_sgm4154x_reg_val.attr);
	sgm4154x_sysfs->attr_sgm4154x_reg_val.attr.name = "sgm4154x_reg_val";
	sgm4154x_sysfs->attr_sgm4154x_reg_val.attr.mode = 0644;
	sgm4154x_sysfs->attr_sgm4154x_reg_val.show = sgm4154x_reg_val_show;
	sgm4154x_sysfs->attr_sgm4154x_reg_val.store = sgm4154x_reg_val_store;

	ret = sysfs_create_group(&info->psy_usb->dev.kobj, &sgm4154x_sysfs->attr_g);
	if (ret < 0)
		dev_err(info->dev, "Cannot create sysfs , ret = %d\n", ret);

	return ret;
}

static int sgm4154x_charger_usb_change(struct notifier_block *nb,
				      unsigned long limit, void *data)
{
	struct sgm4154x_charger_info *info =
		container_of(nb, struct sgm4154x_charger_info, usb_notify);
	int ret;
	info->limit = limit;

	if(runin_stop == 1){
		 ret = sgm4154x_set_hiz_en(info,true);
         if (ret < 0)
           dev_err(info->dev, "set hizmode failed\n");
	}else if(!info->limit || (runin_stop == 0)){
		pm_wakeup_event(info->dev, SGM4154x_WAKE_UP_MS);
		info->redetect_count = 0; 
		schedule_work(&info->work);
	}
	return NOTIFY_OK;
}

static int sgm4154x_charger_usb_get_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    union power_supply_propval *val)
{
	struct sgm4154x_charger_info *info = power_supply_get_drvdata(psy);
	u32 cur, online, health, enabled = 0;
	enum usb_charger_type type;
	int ret = 0;

	if (!info)
		return -ENOMEM;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		//pr_err("sgm4154x_charger POWER_SUPPLY_PROP_STATUS\n");
		if (info->limit)
			val->intval = sgm4154x_charger_get_status(info);
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		//pr_err("sgm4154x_charger POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT\n");
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = sgm4154x_charger_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		//pr_err("sgm4154x_charger POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT\n");
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = sgm4154x_charger_get_limit_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		//pr_err("sgm4154x_charger POWER_SUPPLY_PROP_ONLINE\n");

		ret = sgm4154x_charger_get_online(info, &online);
		if (ret)
			goto out;

		val->intval = online;

		break;

	case POWER_SUPPLY_PROP_HEALTH:
		//pr_err("sgm4154x_charger POWER_SUPPLY_PROP_HEALTH\n");

		if (info->charging) {
			val->intval = 0;
		} else {
			ret = sgm4154x_charger_get_health(info, &health);
			if (ret)
				goto out;

			val->intval = health;
		}
		break;

	case POWER_SUPPLY_PROP_USB_TYPE:
		//pr_err("sgm4154x_charger POWER_SUPPLY_PROP_USB_TYPE 11111\n");

		type = info->usb_phy->chg_type;
	    //pr_err("sgm4154x_charger POWER_SUPPLY_PROP_USB_TYPE 22222\n");


		switch (type) {
		case SDP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_SDP;
			break;

		case DCP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_DCP;
			break;

		case CDP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_CDP;
			break;

		default:
			val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		}

		if (g_adp_support_hvdcp)
			val->intval = POWER_SUPPLY_USB_TYPE_PD;


		break;

	case POWER_SUPPLY_PROP_CALIBRATE:
	    //pr_err("sgm4154x_charger POWER_SUPPLY_PROP_CALIBRATE\n");

		ret = regmap_read(info->pmic, info->charger_pd, &enabled);
		if (ret) {
			dev_err(info->dev, "get sgm4154x charge status failed\n");
			goto out;
		}

		val->intval = !enabled;
		break;

	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (info->state)
			 val->intval = info->state;
		else
			 val->intval = 0;
		break;
	default:
	    pr_err("sgm4154x_charger default--%d\n", psp);

		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int sgm4154x_charger_usb_set_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    const union power_supply_propval *val)
{
	struct sgm4154x_charger_info *info = power_supply_get_drvdata(psy);
	int ret = 0;
	u32 input_vol;
	bool bat_present;

	if (!info)
		return -ENOMEM;

	if (psp == POWER_SUPPLY_PROP_STATUS || psp == POWER_SUPPLY_PROP_CALIBRATE) {
		bat_present = sgm4154x_charger_is_bat_present(info);
		ret = sgm4154x_charger_get_charge_voltage(info, &input_vol);
		if (ret) {
			input_vol = 0;
			dev_err(info->dev, "failed to get charge voltage! ret = %d\n", ret);
		}
	}

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = sgm4154x_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sgm4154x_charger_set_limit_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set input current limit failed\n");
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = sgm4154x_charger_set_termina_vol(info, val->intval / 1000);
		if (ret < 0)
			dev_err(info->dev, "failed to set terminate voltage\n");
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
#ifdef VENDOR_KERNEL
		ret = sgm4154x_qc_adjust_voltage(info, val->intval);
#else //not defined VENDOR_KERNEL
		info->qc_voltage = val->intval;
		queue_work(info->charge_wq, &info->qc_voltage_adjust_work);
#endif //defined VENDOR_KERNEL
		if (ret < 0)
			dev_err(info->dev, "failed to qc adjust voltage\n");
		break;

	case POWER_SUPPLY_PROP_CALIBRATE:
		if (val->intval == true) {
			ret = sgm4154x_charger_start_charge(info);
			if (ret)
				dev_err(info->dev, "start charge failed\n");
		} else if (val->intval == false) {
			sgm4154x_charger_stop_charge(info, bat_present);
		}
		break;

	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == CM_SET_PRE_CURRENT_CMD) {
			if(prj_name == 2 || prj_name ==3){
				ret = sgm4154x_set_prechrg_curr(info,360);
			}
			break;
		} else if (val->intval == CM_SET_SAFFY_TIMER_DISABLE_CMD) {
			ret = sgm4154x_enable_safetytimer(info, false);
			break;
		} else if (val->intval == CM_DUMP_CHARGER_REGISTER_CMD) {
			if(prj_name == 2 || prj_name ==3){
				sgm4154x_charger_dump_register(info);
			}
			break;
		}
#ifndef VENDOR_KERNEL
		 else if (val->intval == CM_HIZ_ENABLE_CMD) {
			ret = sgm4154x_set_hiz_en(info,true);
			if (ret < 0)
				dev_err(info->dev, "set hizmode failed\n");
			break;
		} else if (val->intval == CM_HIZ_DISABLE_CMD) {
			ret = sgm4154x_set_hiz_en(info,false);
			if (ret < 0)
				dev_err(info->dev, "set hizmode failed\n");
			break;
		} else if (val->intval == CM_FEED_WATCHDOG_CMD) {
			ret = sgm4154x_charger_feed_watchdog(info);
			if (ret < 0)
				dev_err(info->dev, "feed charger watchdog failed\n");
			break;
		}
#endif //not defined VENDOR_KERNEL

		ret = sgm4154x_charger_set_status(info, val->intval, input_vol, bat_present);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");

		break;
	case POWER_SUPPLY_PROP_TYPE:
		if (val->intval == POWER_SUPPLY_WIRELESS_CHARGER_TYPE_BPP) {
			ret = sgm4154x_charger_set_vindpm(info, VINDPM_OS_5900mV);
		} else if (val->intval == POWER_SUPPLY_WIRELESS_CHARGER_TYPE_EPP) {
			ret = sgm4154x_charger_set_vindpm(info, VINDPM_OS_10500mV);
		} else {
			ret = sgm4154x_charger_set_vindpm(info, VINDPM_OS_5900mV);
		}
		if (ret)
			dev_err(info->dev, "failed to set fast charge ovp\n");

		break;

	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int sgm4154x_charger_property_is_writeable(struct power_supply *psy,
						 enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_CALIBRATE:
	case POWER_SUPPLY_PROP_TYPE:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_usb_type sgm4154x_charger_usb_types[] = {
/* 	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_QC2,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID */
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,		/* Standard Downstream Port */
	POWER_SUPPLY_USB_TYPE_DCP,		/* Dedicated Charging Port */
	POWER_SUPPLY_USB_TYPE_CDP,		/* Charging Downstream Port */
	POWER_SUPPLY_USB_TYPE_ACA,		/* Accessory Charger Adapters */
	POWER_SUPPLY_USB_TYPE_C,		/* Type C Port */
	POWER_SUPPLY_USB_TYPE_PD,		/* Power Delivery Port */
	POWER_SUPPLY_USB_TYPE_PD_DRP,		/* PD Dual Role Port */
	POWER_SUPPLY_USB_TYPE_PD_PPS,		/* PD Programmable Power Supply */
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID,	/* Apple Charging Method */
	//POWER_SUPPLY_USB_TYPE_QC2,

};

static enum power_supply_property sgm4154x_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CALIBRATE,
	POWER_SUPPLY_PROP_TYPE,
};

static const struct power_supply_desc sgm4154x_charger_desc = {
	.name			= "sgm4154x_charger",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= sgm4154x_usb_props,
	.num_properties		= ARRAY_SIZE(sgm4154x_usb_props),
	.get_property		= sgm4154x_charger_usb_get_property,
	.set_property		= sgm4154x_charger_usb_set_property,
	.property_is_writeable	= sgm4154x_charger_property_is_writeable,
	.usb_types		= sgm4154x_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(sgm4154x_charger_usb_types),
};

static void sgm4154x_charger_detect_status(struct sgm4154x_charger_info *info)
{
	unsigned int min, max;

	/*
	 * If the USB charger status has been USB_CHARGER_PRESENT before
	 * registering the notifier, we should start to charge with getting
	 * the charge current.
	 */
	if (info->usb_phy->chg_state != USB_CHARGER_PRESENT)
		return;

	usb_phy_get_charger_current(info->usb_phy, &min, &max);
	info->limit = min;

	/*
	 * slave no need to start charge when vbus change.
	 * due to charging in shut down will check each psy
	 * whether online or not, so let info->limit = min.
	 */
	schedule_work(&info->work);
}

static void sgm4154x_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sgm4154x_charger_info *info = container_of(dwork,
							 struct sgm4154x_charger_info,
							 wdt_work);
	int ret;

	ret = sgm4154x_charger_feed_watchdog(info);
	if (ret)
		schedule_delayed_work(&info->wdt_work, HZ * 5);
	else
		schedule_delayed_work(&info->wdt_work, HZ * 15);
}

#ifdef CONFIG_REGULATOR
static bool sgm4154x_charger_check_otg_valid(struct sgm4154x_charger_info *info)
{
	int ret;
	u8 value = 0;
	bool status = false;

	ret = sgm4154x_read(info, SGM4154x_REG_01, &value);
	if (ret) {
		dev_err(info->dev, "get sgm4154x charger otg valid status failed\n");
		return status;
	}

	if (value & REG01_OTG_CONFIG_MASK)
		status = true;
	else
		dev_err(info->dev, "otg is not valid, REG_3 = 0x%x\n", value);

	return status;
}

static bool sgm4154x_charger_check_otg_current(struct sgm4154x_charger_info *info)
{
	int ret;
	u8 value = 0;
	bool status = true;

	ret = sgm4154x_read(info, SGM4154x_REG_02, &value);
	if (ret) {
		dev_err(info->dev, "get sgm4154x charger otg fault status failed\n");
		return status;
	}

	if (value & REG02_BOOSTV_LIM_MASK)
		status = false;

	return status;
}

static int sgm4154x_charger_set_otg_current(struct sgm4154x_charger_info *info)
{
	int ret = 0;

	ret = sgm4154x_update_bits(info, SGM4154x_REG_02, REG02_BOOSTV_LIM_MASK,
			REG02_BOOSTV_LIM_1200MA << REG02_BOOSTV_LIM_SHIFT);
	if (ret) {
		dev_err(info->dev, "Failed to set sgm4154x otg current limit\n");
		return ret;
	}

	return ret;
}

static bool sgm4154x_charger_check_otg_voltage(struct sgm4154x_charger_info *info)
{
	int ret;
	u8 value = 0;
	bool status = true;

	ret = sgm4154x_read(info, SGM4154x_REG_06, &value);
	if (ret) {
		dev_err(info->dev, "get sgm4154x charger otg fault status failed\n");
		return status;
	}

	// 5v
	if ((value & REG06_BOOSTV_MASK) >> REG06_BOOSTV_SHIFT != 0x1)
		status = false;

	return status;
}

static int sgm4154x_charger_set_otg_voltage(struct sgm4154x_charger_info *info)
{
	int ret = 0;

	// boost mode valtage set 5v
	ret = sgm4154x_update_bits(info, SGM4154x_REG_06, REG06_BOOSTV_MASK,
			0x1 << REG06_BOOSTV_SHIFT);
	if (ret) {
		dev_err(info->dev, "Failed to set sgm4154x otg voltage limit\n");
		return ret;
	}

	return ret;
}

static bool sgm4154x_charger_check_otg_fault(struct sgm4154x_charger_info *info)
{
	int ret;
	u8 value = 0;
	bool status = true;

	ret = sgm4154x_read(info, SGM4154x_REG_09, &value);
	if (ret) {
		dev_err(info->dev, "get sgm4154x charger otg fault status failed\n");
		return status;
	}

	if (!(value & REG09_FAULT_BOOST_MASK))
		status = false;
	else
		dev_err(info->dev, "boost fault occurs, REG_0C = 0x%x\n",
			value);

	return status;
}

static void sgm4154x_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sgm4154x_charger_info *info = container_of(dwork,
			struct sgm4154x_charger_info, otg_work);
	bool otg_valid = sgm4154x_charger_check_otg_valid(info);
	bool otg_fault;
	int ret, retry = 0;

	if (!sgm4154x_charger_check_otg_current(info))
		sgm4154x_charger_set_otg_current(info);

	if (!sgm4154x_charger_check_otg_voltage(info))
		sgm4154x_charger_set_otg_voltage(info);

	if (otg_valid)
		goto out;

	do {
		otg_fault = sgm4154x_charger_check_otg_fault(info);
		if (!otg_fault) {
			ret = sgm4154x_update_bits(info, SGM4154x_REG_01,
						  REG01_OTG_CONFIG_MASK,
						  REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT);
			if (ret)
				dev_err(info->dev, "restart sgm4154x charger otg failed\n");
		}

		otg_valid = sgm4154x_charger_check_otg_valid(info);
	} while (!otg_valid && retry++ < SGM4154x_OTG_RETRY_TIMES);

	if (retry >= SGM4154x_OTG_RETRY_TIMES) {
		dev_err(info->dev, "Restart OTG failed\n");
		return;
	}

out:
	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(1500));
}

static int sgm4154x_charger_enable_otg(struct regulator_dev *dev)
{
	struct sgm4154x_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	/*
	 * Disable charger detection function in case
	 * affecting the OTG timing sequence.
	 */
	ret = regmap_update_bits(info->pmic, info->charger_detect,
				 BIT_DP_DM_BC_ENB, BIT_DP_DM_BC_ENB);
	if (ret) {
		dev_err(info->dev, "failed to disable bc1.2 detect function.\n");
		return ret;
	}

	ret = sgm4154x_update_bits(info, SGM4154x_REG_02, REG02_BOOSTV_LIM_MASK,
			REG02_BOOSTV_LIM_1200MA << REG02_BOOSTV_LIM_SHIFT);
	if (ret) {
		dev_err(info->dev, "Failed to set sgm4154x otg current limit\n");
		return ret;
	}

	ret = sgm4154x_update_bits(info, SGM4154x_REG_06, REG06_BOOSTV_MASK,
			0x1 << REG06_BOOSTV_SHIFT);
	if (ret) {
		dev_err(info->dev, "Failed to set sgm4154x otg voltage limit\n");
		return ret;
	}

	ret = sgm4154x_update_bits(info, SGM4154x_REG_01, REG01_OTG_CONFIG_MASK,
				  REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT);

	if (ret) {
		dev_err(info->dev, "enable sgm4154x otg failed\n");
		regmap_update_bits(info->pmic, info->charger_detect,
				   BIT_DP_DM_BC_ENB, 0);
		return ret;
	}
	dev_err(info->dev, "%s \n",__func__);
	info->otg_enable = true;
	schedule_delayed_work(&info->wdt_work,
			      msecs_to_jiffies(SGM4154x_WDT_VALID_MS));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(SGM4154x_OTG_VALID_MS));

	return 0;
}

static int sgm4154x_charger_disable_otg(struct regulator_dev *dev)
{
	struct sgm4154x_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	info->otg_enable = false;
	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);
	ret = sgm4154x_update_bits(info, SGM4154x_REG_01,
				  REG01_OTG_CONFIG_MASK, REG01_OTG_DISABLE);
	if (ret) {
		dev_err(info->dev, "disable sgm4154x otg failed\n");
		return ret;
	}
	dev_err(info->dev, "%s \n",__func__);
	/* Enable charger detection function to identify the charger type */
	return regmap_update_bits(info->pmic, info->charger_detect,
				  BIT_DP_DM_BC_ENB, 0);
}

static int sgm4154x_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct sgm4154x_charger_info *info = rdev_get_drvdata(dev);
	int ret;
	u8 val;

	ret = sgm4154x_read(info, SGM4154x_REG_01, &val);
	if (ret) {
		dev_err(info->dev, "failed to get sgm4154x otg status\n");
		return ret;
	}

	val &= REG01_OTG_CONFIG_MASK;

	return val;
}

static const struct regulator_ops sgm4154x_charger_vbus_ops = {
	.enable = sgm4154x_charger_enable_otg,
	.disable = sgm4154x_charger_disable_otg,
	.is_enabled = sgm4154x_charger_vbus_is_enabled,
};

static const struct regulator_desc sgm4154x_charger_vbus_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &sgm4154x_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int sgm4154x_charger_register_vbus_regulator(struct sgm4154x_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev,
				      &sgm4154x_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "Can't register regulator:%d\n", ret);
	}

	return ret;
}

#else
static int sgm4154x_charger_register_vbus_regulator(struct sgm4154x_charger_info *info)
{
	return 0;
}
#endif

enum usb_charger_type sgm4154x_charger_detect(void)
{
	int type = 0;
	enum usb_charger_type chr_type = UNKNOWN_TYPE;
	u8 chrg_stat;
	int i;

	if (NULL == pinfo)
		return chr_type;

	sc27xx_force_dpdm();
	mdelay(10);
	sgm4154x_force_dpdm(pinfo);

	for (i = 0; i < 20; i++) {
		if (sgm4154x_charger_dpdm_detect_is_done(pinfo))
			break;
		mdelay(50);
	}

	chrg_stat = sgm4154x_charger_get_type(pinfo);

	type = (chrg_stat & REG08_VBUS_STAT_MASK) >> REG08_VBUS_STAT_SHIFT;
	switch (type) {
	case REG08_VBUS_TYPE_USB_CDP:
		chr_type = CDP_TYPE;
		break;
	case REG08_VBUS_TYPE_USB_DCP:
		chr_type = DCP_TYPE;
		break;
	case REG08_VBUS_TYPE_USB_SDP:
		chr_type = SDP_TYPE;
		break;
	case REG08_VBUS_TYPE_UNKNOWN:
		chr_type = UNKNOWN_TYPE;
		break;
	case REG08_VBUS_TYPE_ADAPTER:
		chr_type = UNKNOWN_TYPE;
		break;
	default:
		chr_type = UNKNOWN_TYPE;
	}

	dev_err(pinfo->dev, "finn chrg_stat = %#x,type = %#x, cnt = %d\n", chrg_stat, chr_type, i);

	return chr_type;
}

EXPORT_SYMBOL_GPL(sgm4154x_charger_detect);


enum usb_charger_type sgm4154x_charger_redetect(void)
{
	enum usb_charger_type type = UNKNOWN_TYPE;
	int cnt = 3;

	if (NULL == pinfo)
		return type;

	do {
		type = sgm4154x_charger_detect();
	} while (type == UNKNOWN_TYPE && (cnt-- >= 0));

    return type;
}

EXPORT_SYMBOL_GPL(sgm4154x_charger_redetect);

static int sgm4154x_charger_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct sgm4154x_charger_info *info;
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	int ret;
	bool bat_present;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "No support for SMBUS_BYTE_DATA\n");
		return -ENODEV;
	}

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->client = client;
	info->dev = dev;

	alarm_init(&info->otg_timer, ALARM_BOOTTIME, NULL);

	bat_present = sgm4154x_charger_is_bat_present(info);
	mutex_init(&info->lock);
#ifndef VENDOR_KERNEL
	info->charge_wq = alloc_ordered_workqueue("sgm4154x_charge_wq",
  						   WQ_MEM_RECLAIM);
  	if (info->charge_wq == NULL) {
  		dev_err(info->dev, "failed to create work queue\n");
  		return -ENOMEM;
  	}
#endif //not defined VENDOR_KERNEL
	INIT_DELAYED_WORK(&info->recharge_detect_work,
			  sgm4154x_charger_redetect_work);

	INIT_WORK(&info->work, sgm4154x_charger_work);

	i2c_set_clientdata(client, info);

	info->usb_phy = devm_usb_get_phy_by_phandle(dev, "phys", 0);
	if (IS_ERR(info->usb_phy)) {
		dev_err(dev, "failed to find USB phy\n");
		return PTR_ERR(info->usb_phy);
	}

	info->edev = extcon_get_edev_by_phandle(info->dev, 0);
	if (IS_ERR(info->edev)) {
		dev_err(dev, "failed to find vbus extcon device.\n");
		return PTR_ERR(info->edev);
	}

	ret = sgm4154x_charger_is_fgu_present(info);
	if (ret) {
		dev_err(dev, "sc27xx_fgu not ready.\n");
		return -EPROBE_DEFER;
	}

	ret = sgm4154x_charger_register_vbus_regulator(info);
	if (ret) {
		dev_err(dev, "failed to register vbus regulator.\n");
		return ret;
	}

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np)
		regmap_np = of_find_compatible_node(NULL, NULL, "sprd,ump962x-syscon");

	if (regmap_np) {
		if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
			info->charger_pd_mask = SGM4154x_DISABLE_PIN_MASK_2721;
		else
			info->charger_pd_mask = SGM4154x_DISABLE_PIN_MASK;
	} else {
		dev_err(dev, "unable to get syscon node\n");
		return -ENODEV;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 1,
					 &info->charger_detect);
	if (ret) {
		dev_err(dev, "failed to get charger_detect\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 2,
					 &info->charger_pd);
	if (ret) {
		dev_err(dev, "failed to get charger_pd reg\n");
		return ret;
	}

	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		of_node_put(regmap_np);
		dev_err(dev, "unable to get syscon device\n");
		return -ENODEV;
	}

	of_node_put(regmap_np);
	info->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!info->pmic) {
		dev_err(dev, "unable to get pmic regmap device\n");
		return -ENODEV;
	}

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	info->psy_usb = devm_power_supply_register(dev,
						   &sgm4154x_charger_desc,
						   &charger_cfg);

	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		ret = PTR_ERR(info->psy_usb);
		goto err_mutex_lock;
	}
	dev_err(dev, "sgm4154x_charger devm_power_supply_register done\n");

	pinfo = info;

	ret = sgm4154x_charger_hw_init(info);
	if (ret) {
		dev_err(dev, "failed to sgm4154x_charger_hw_init\n");
		goto err_mutex_lock;
	}

	sgm4154x_charger_stop_charge(info, bat_present);

	INIT_WORK(&info->qc_detection_work, sgm4154x_charger_qc_detection_work);
#ifndef VENDOR_KERNEL
	INIT_WORK(&info->qc_voltage_adjust_work, sgm4154x_charger_qc_adjust_voltage_work);
#endif //not defined VENDOR_KERNEL

	device_init_wakeup(info->dev, true);
	info->usb_notify.notifier_call = sgm4154x_charger_usb_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		dev_err(dev, "failed to register notifier:%d\n", ret);
		goto err_psy_usb;
	}

	ret = sgm4154x_register_sysfs(info);
	if (ret) {
		dev_err(info->dev, "register sysfs fail, ret = %d\n", ret);
		goto err_sysfs;
	}

	sgm4154x_charger_detect_status(info);

	ret = sgm4154x_update_bits(info, SGM4154x_REG_02, REG02_BOOSTV_LIM_MASK,
			REG02_BOOSTV_LIM_1200MA << REG02_BOOSTV_LIM_SHIFT);
	if (ret) {
		dev_err(info->dev, "Failed to set sgm4154x otg current limit\n");
		return ret;
	}

	ret = sgm4154x_update_bits(info, SGM4154x_REG_05, REG05_WDT_MASK,
				  REG05_WDT_40S << REG05_WDT_SHIFT);
	if (ret) {
		dev_err(info->dev, "Failed to enable sgm4154x watchdog\n");
		return ret;
	}

	INIT_DELAYED_WORK(&info->otg_work, sgm4154x_charger_otg_work);
	INIT_DELAYED_WORK(&info->wdt_work,
			  sgm4154x_charger_feed_watchdog_work);


    dev_err(dev, "sgm4154x_charger probe done\n");

	return 0;

err_sysfs:
	sysfs_remove_group(&info->psy_usb->dev.kobj, &info->sysfs->attr_g);
	usb_unregister_notifier(info->usb_phy, &info->usb_notify);
err_psy_usb:
	power_supply_unregister(info->psy_usb);
err_mutex_lock:
	mutex_destroy(&info->lock);

	return ret;
}

static int sgm4154x_charger_remove(struct i2c_client *client)
{
	struct sgm4154x_charger_info *info = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&info->wdt_work);
	usb_unregister_notifier(info->usb_phy, &info->usb_notify);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sgm4154x_charger_suspend(struct device *dev)
{
	struct sgm4154x_charger_info *info = dev_get_drvdata(dev);
	ktime_t now, add;
	unsigned int wakeup_ms = SGM4154x_OTG_ALARM_TIMER_MS;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (info->otg_enable || info->limit)
		sgm4154x_charger_feed_watchdog(info);

	if (!info->otg_enable)
		return 0;

	cancel_delayed_work_sync(&info->wdt_work);

	now = ktime_get_boottime();
	add = ktime_set(wakeup_ms / MSEC_PER_SEC,
		       (wakeup_ms % MSEC_PER_SEC) * NSEC_PER_MSEC);
	alarm_start(&info->otg_timer, ktime_add(now, add));

	return 0;
}

static int sgm4154x_charger_resume(struct device *dev)
{
	struct sgm4154x_charger_info *info = dev_get_drvdata(dev);
	int ret;

	if (!info->otg_enable)
		return 0;

	alarm_cancel(&info->otg_timer);

	/* feed watchdog first after resume */
	ret = sgm4154x_update_bits(info, SGM4154x_REG_01, REG01_WDT_RESET_MASK,
				  REG01_WDT_RESET << REG01_WDT_RESET_SHIFT);
	if (ret)
		dev_warn(info->dev, "reset sgm4154x failed after resume\n");

	schedule_delayed_work(&info->wdt_work, HZ * 15);

	return 0;
}
#endif

static void sgm4154x_charger_shutdown(struct i2c_client *client)
{
    int ret = 0;
	struct sgm4154x_charger_info *info = i2c_get_clientdata(client);

	cancel_work_sync(&info->qc_detection_work);
	cancel_delayed_work_sync(&info->wdt_work);
#ifndef VENDOR_KERNEL
	cancel_work_sync(&info->qc_voltage_adjust_work);
#endif //not defined VENDOR_KERNEL
	if (info->otg_enable) {
		info->otg_enable = false;
		cancel_delayed_work_sync(&info->otg_work);
		ret = sgm4154x_update_bits(info, SGM4154x_REG_01,
				  REG01_OTG_CONFIG_MASK, REG01_OTG_DISABLE);
		if (ret)
			dev_err(info->dev, "disable sgm4154x otg failed ret = %d\n", ret);

		/* Enable charger detection function to identify the charger type */
		ret = regmap_update_bits(info->pmic, info->charger_detect,
					 BIT_DP_DM_BC_ENB, 0);
		if (ret)
			dev_err(info->dev,
				"enable charger detection function failed ret = %d\n", ret);
	}

    dev_info(info->dev,"sgm4154x_charger_shutdown\n");
}

static const struct dev_pm_ops sgm4154x_charger_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sgm4154x_charger_suspend,
				sgm4154x_charger_resume)
};

static const struct i2c_device_id sgm4154x_i2c_id[] = {
	{ "sgm41541", 0 },
	{ "sgm41542", 0 },
	{}
};

static const struct of_device_id sgm4154x_charger_of_match[] = {
	{ .compatible = "sgm,sgm41541", },
	{ .compatible = "sgm,sgm41542", },
	{ .compatible = "sgm,sgm4154x_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, sgm4154x_charger_of_match);

static struct i2c_driver sgm4154x_charger_driver = {
	.driver = {
		.name = "sgm4154x_chg",
		.of_match_table = sgm4154x_charger_of_match,
		.pm = &sgm4154x_charger_pm_ops,
	},
	.probe = sgm4154x_charger_probe,
	.remove = sgm4154x_charger_remove,
	.shutdown = sgm4154x_charger_shutdown,
	.id_table = sgm4154x_i2c_id,
};

module_i2c_driver(sgm4154x_charger_driver);

MODULE_AUTHOR("qhq <Allen.Qin@sg-micro.com>");
MODULE_DESCRIPTION("SGM4154x Charger Driver");
MODULE_LICENSE("GPL");

