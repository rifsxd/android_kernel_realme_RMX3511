/*
 * SGM RT9471 charger driver
 *
 * Copyright (C) 2015 Intel Corporation
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

enum rt9471_regs {
	RT9471_REG_OTGCFG = 0x00,
	RT9471_REG_TOP,
	RT9471_REG_FUNCTION,
	RT9471_REG_IBUS,
	RT9471_REG_VBUS,
	RT9471_REG_PRECHG,
	RT9471_REG_REGU,
	RT9471_REG_VCHG,
	RT9471_REG_ICHG,
	RT9471_REG_CHGTIMER,
	RT9471_REG_EOC,
	RT9471_REG_INFO,
	RT9471_REG_JEITA,
	RT9471_REG_DPDMDET = 0x0E,
	RT9471_REG_STATUS,
	RT9471_REG_STAT0,
	RT9471_REG_STAT1,
	RT9471_REG_STAT2,
	RT9471_REG_STAT3,
	RT9471_REG_IRQ0 = 0x20,
	RT9471_REG_IRQ1,
	RT9471_REG_IRQ2,
	RT9471_REG_IRQ3,
	RT9471_REG_MASK0 = 0x30,
	RT9471_REG_MASK1,
	RT9471_REG_MASK2,
	RT9471_REG_MASK3,
	RT9471_REG_HIDDEN_0 = 0x40,
	RT9471_REG_TOP_HDEN = 0x43,
	RT9471_REG_BUCK_HDEN1 = 0x45,
	RT9471_REG_BUCK_HDEN2 = 0x46,
	RT9471_REG_BUCK_HDEN3 = 0x54,
	RT9471_REG_BUCK_HDEN4,
	RT9471_REG_OTG_HDEN2 = 0x58,
	RT9471_REG_BUCK_HDEN5,
};

static const u8 rt9471_reg_addr[] = {
	RT9471_REG_OTGCFG,
	RT9471_REG_TOP,
	RT9471_REG_FUNCTION,
	RT9471_REG_IBUS,
	RT9471_REG_VBUS,
	RT9471_REG_PRECHG,
	RT9471_REG_REGU,
	RT9471_REG_VCHG,
	RT9471_REG_ICHG,
	RT9471_REG_CHGTIMER,
	RT9471_REG_EOC,
	RT9471_REG_INFO,
	RT9471_REG_JEITA,
	RT9471_REG_DPDMDET,
	RT9471_REG_STATUS,
	RT9471_REG_STAT0,
	RT9471_REG_STAT1,
	RT9471_REG_STAT2,
	RT9471_REG_STAT3,
	/* Skip IRQs to prevent reading clear while dumping registers */
	RT9471_REG_MASK0,
	RT9471_REG_MASK1,
	RT9471_REG_MASK2,
	RT9471_REG_MASK3,
};

/* ========== OTGCFG 0x00 ============ */
#define RT9471_OTGCC_SHIFT	0
#define RT9471_OTGCC_MASK	GENMASK(0, 0)

/* ========== TOP 0x01 ============ */
#define RT9471_WDT_SHIFT	0
#define RT9471_WDT_MASK		0x03
#define RT9471_WDTCNTRST_MASK	GENMASK(2, 2)
#define RT9471_DISI2CTO_MASK	GENMASK(3, 3)
#define RT9471_QONRST_MASK	GENMASK(7, 7)

/* ========== FUNCTION 0x02 ============ */
#define RT9471_CHG_EN_SHIFT	0
#define RT9471_CHG_EN_MASK	GENMASK(0, 0)
#define RT9471_OTG_EN_SHIFT	1
#define RT9471_OTG_EN_MASK	GENMASK(1, 1)
#define RT9471_HZ_SHIFT		5
#define RT9471_HZ_MASK		GENMASK(5, 5)
#define RT9471_BATFETDIS_SHIFT	7
#define RT9471_BATFETDIS_MASK	GENMASK(7, 7)

/* ========== IBUS 0x03 ============ */
#define RT9471_AICR_SHIFT	0
#define RT9471_AICR_MASK	0x3F
#define RT9471_AICR_MIN		50000
#define RT9471_AICR_MAX		3200000
#define RT9471_AICR_STEP	50000
#define RT9471_AUTOAICR_MASK	GENMASK(6, 6)

/* ========== VBUS 0x04 ============ */
#define RT9471_MIVR_SHIFT	0
#define RT9471_MIVR_MASK	0x0F
#define RT9471_MIVR_MIN		3900000
#define RT9471_MIVR_MAX		5400000
#define RT9471_MIVR_STEP	100000
#define RT9471_MIVRTRACK_SHIFT	4
#define RT9471_MIVRTRACK_MASK	0x30

/* ========== VCHG 0x07 ============ */
#define RT9471_VRECHG_MASK	GENMASK(7, 7)
#define RT9471_CV_SHIFT		0
#define RT9471_CV_MASK		0x7F
#define RT9471_CV_MIN		3900000
#define RT9471_CV_MAX		4700000
#define RT9471_CV_STEP		10000

/* ========== ICHG 0x08 ============ */
#define RT9471_ICHG_SHIFT	0
#define RT9471_ICHG_MASK	0x3F
#define RT9471_ICHG_MIN		0
#define RT9471_ICHG_MAX		3150000
#define RT9471_ICHG_STEP	50000

/* ========== CHGTIMER 0x09 ============ */
#define RT9471_SAFETMR_EN_SHIFT	7
#define RT9471_SAFETMR_EN_MASK	GENMASK(7, 7)
#define RT9471_SAFETMR_SHIFT	4
#define RT9471_SAFETMR_MASK	0x30
#define RT9471_SAFETMR_MIN	5
#define RT9471_SAFETMR_MAX	20
#define RT9471_SAFETMR_STEP	5

/* ========== EOC 0x0A ============ */
#define RT9471_IEOC_SHIFT	4
#define RT9471_IEOC_MASK	0xF0
#define RT9471_IEOC_MIN		50000
#define RT9471_IEOC_MAX		800000
#define RT9471_IEOC_STEP	50000
#define RT9471_TE_MASK		GENMASK(1, 1)
#define RT9471_EOC_RST_SHIFT	0
#define RT9471_EOC_RST_MASK	GENMASK(0, 0)

/* ========== INFO 0x0B ============ */
#define RT9471_REGRST_MASK	GENMASK(7, 7)
#define RT9471_DEVID_SHIFT	3
#define RT9471_DEVID_MASK	0x78
#define RT9471_DEVREV_SHIFT	0
#define RT9471_DEVREV_MASK	0x03

/* ========== JEITA 0x0C ============ */
#define RT9471_JEITA_EN_MASK	GENMASK(7, 7)

/* ========== DPDMDET 0x0E ============ */
#define RT9471_BC12_EN_MASK	GENMASK(7, 7)

/* ========== STATUS 0x0F ============ */
#define RT9471_ICSTAT_SHIFT	0
#define RT9471_ICSTAT_MASK	0x0F
#define RT9471_PORTSTAT_SHIFT	4
#define RT9471_PORTSTAT_MASK	0xF0

/* ========== STAT0 0x10 ============ */
#define RT9471_ST_VBUSGD_SHIFT		7
#define RT9471_ST_VBUSGD_MASK		GENMASK(7, 7)
#define RT9471_ST_CHGRDY_SHIFT		6
#define RT9471_ST_CHGRDY_MASK		GENMASK(6, 6)
#define RT9471_ST_CHGDONE_SHIFT		3
#define RT9471_ST_BC12_DONE_SHIFT	0
#define RT9471_ST_BC12_DONE_MASK	GENMASK(0, 0)

/* ========== STAT1 0x11 ============ */
#define RT9471_ST_MIVR_SHIFT	7
#define RT9471_ST_MIVR_MASK	GENMASK(7, 7)
#define RT9471_ST_AICR_MASK	GENMASK(6, 6)
#define RT9471_ST_THREG_MASK	GENMASK(5, 5)
#define RT9471_ST_BUSUV_MASK	GENMASK(4, 4)
#define RT9471_ST_TOUT_MASK	GENMASK(3, 3)
#define RT9471_ST_SYSOV_MASK	GENMASK(2, 2)
#define RT9471_ST_BATOV_MASK	GENMASK(1, 1)

/* ========== STAT3 0x13 ============ */
#define RT9471_ST_OTP_MASK	GENMASK(7, 7)
#define RT9471_ST_VACOV_SHIFT	6
#define RT9471_ST_VACOV_MASK	GENMASK(6, 6)
#define RT9471_ST_WDT_MASK	GENMASK(5, 5)

/* ========== HIDDEN_0 0x40 ============ */
#define RT9471_CHIP_REV_SHIFT	5
#define RT9471_CHIP_REV_MASK	0xE0

/* ========== TOP_HDEN 0x43 ============ */
#define RT9471_FORCE_EN_VBUS_SINK_SHIFT	4
#define RT9471_FORCE_EN_VBUS_SINK_MASK	GENMASK(4, 4)

/* ========== OTG_HDEN2 0x58 ============ */
#define RT9471_REG_OTG_RES_COMP_SHIFT	4
#define RT9471_REG_OTG_RES_COMP_MASK	0x30

/* Other Realted Definition*/
#define RT9471_BATTERY_NAME			"sc27xx-fgu"

#define BIT_DP_DM_BC_ENB			GENMASK(0, 0)

#define RT9471_WDT_VALID_MS			50

#define RT9471_OTG_ALARM_TIMER_MS		15000
#define RT9471_OTG_VALID_MS			500
#define RT9471_OTG_RETRY_TIMES			10

#define RT9471_DISABLE_PIN_MASK		GENMASK(0, 0)
#define RT9471_DISABLE_PIN_MASK_2721		BIT(15)

#define RT9471_FAST_CHG_VOL_MAX		10500000
#define RT9471_NORMAL_CHG_VOL_MAX		6500000

#define RT9471_WAKE_UP_MS			2000
#define SET_DELAY_CHECK_LIMIT_INPUT_CURRENT

extern int get_now_battery_id(void);
struct rt9471_charger_sysfs {
	char *name;
	struct attribute_group attr_g;
	struct device_attribute attr_rt9471_dump_reg;
	struct device_attribute attr_rt9471_lookup_reg;
	struct device_attribute attr_rt9471_sel_reg_id;
	struct device_attribute attr_rt9471_reg_val;
	struct attribute *attrs[5];

	struct rt9471_charger_info *info;
};

struct power_supply_charge_current {
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
struct rt9471_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	struct power_supply *psy_usb;
	struct power_supply_charge_current cur;
	struct work_struct work;
	struct mutex lock;
	bool charging;
	u32 limit;
	struct delayed_work otg_work;
	struct delayed_work wdt_work;
	#ifdef SET_DELAY_CHECK_LIMIT_INPUT_CURRENT
	struct delayed_work check_limit_current_work;
	unsigned char check_limit_current_cnt;
	#endif
	struct regmap *pmic;
	u32 charger_detect;
	u32 charger_pd;
	u32 charger_pd_mask;
	struct gpio_desc *gpiod;
	struct extcon_dev *edev;
	u32 last_limit_current;
	u32 role;
	bool need_disable_Q1;
	int termination_cur;
	int vol_max_mv;
	u32 actual_limit_current;
	bool otg_enable;
	struct alarm otg_timer;
	struct rt9471_charger_sysfs *sysfs;
	int reg_id;
};

struct rt9471_charger_reg_tab {
	int id;
	u32 addr;
	char *name;
};

static struct rt9471_charger_reg_tab reg_tab[12 + 1] = {
	{0, RT9471_REG_OTGCFG, "reg0"},
	{1, RT9471_REG_TOP, "reg1"},
	{2, RT9471_REG_FUNCTION, "reg2"},
	{3, RT9471_REG_IBUS, "reg3"},
	{4, RT9471_REG_VBUS, "reg4"},
	{5, RT9471_REG_PRECHG, "reg5"},
	{6, RT9471_REG_REGU, "reg6"},
	{7, RT9471_REG_VCHG, "reg7"},
	{8, RT9471_REG_ICHG, "reg8"},
	{9, RT9471_REG_CHGTIMER, "reg9"},
	{10, RT9471_REG_EOC, "reg10"},
	{11, RT9471_REG_INFO, "reg11"},	
	{12, 0, "null"},
};


extern int sprd_charger_parse_charger_id(void);
static int charger_id = 0xFF;
static bool ccali_mode = false;

static int get_boot_mode(void)
{
	struct device_node *cmdline_node;
	const char *cmd_line;
	int ret;

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
	if (ret)
		return ret;

	if (strstr(cmd_line, "androidboot.mode=cali") ||
	    strstr(cmd_line, "androidboot.mode=autotest"))
		ccali_mode = true;

	return 0;
}

static int rt9471_charger_set_limit_current(struct rt9471_charger_info *info,
					     u32 limit_cur);

static int rt9471_read(struct rt9471_charger_info *info, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int rt9471_write(struct rt9471_charger_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int rt9471_update_bits(struct rt9471_charger_info *info, u8 reg,
			       u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = rt9471_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return rt9471_write(info, reg, v);
}

static void rt9471_charger_dump_register(struct rt9471_charger_info *info)
{
	int i, ret, len, idx = 0;
	u8 reg_val;
	char buf[512];

	memset(buf, '\0', sizeof(buf));
	for (i = 0; i < ARRAY_SIZE(rt9471_reg_addr); i++) {
		ret = rt9471_read(info, rt9471_reg_addr[i], &reg_val);
		if (ret == 0) {
			len = snprintf(buf + idx, sizeof(buf) - idx,
				       "[REG_0x%.2x]=0x%.2x; ", rt9471_reg_addr[i],
				       reg_val);
			idx += len;
		}
	}

	dev_info(info->dev, "%s: %s", __func__, buf);
}

static bool rt9471_charger_is_bat_present(struct rt9471_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	bool present = false;
	int ret;

	psy = power_supply_get_by_name(RT9471_BATTERY_NAME);
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

static int rt9471_charger_is_fgu_present(struct rt9471_charger_info *info)
{
	struct power_supply *psy;

	psy = power_supply_get_by_name(RT9471_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to find psy of sc27xx_fgu\n");
		return -ENODEV;
	}
	power_supply_put(psy);

	return 0;
}

static u32 rt9471_closest_value(u32 min, u32 max, u32 step, u8 regval)
{
	u32 val = 0;

	val = min + regval * step;
	if (val > max)
		val = max;

	return val;
}

static u8 rt9471_closest_reg(u32 min, u32 max, u32 step, u32 target)
{
	if (target < min)
		return 0;

	if (target >= max)
		return (max - min) / step;

	return (target - min) / step;
}

static int rt9471_charger_set_vindpm(struct rt9471_charger_info *info, u32 vol)
{
	u8 reg_val;
	
	reg_val = rt9471_closest_reg(RT9471_MIVR_MIN, RT9471_MIVR_MAX,
				    RT9471_MIVR_STEP, vol * 1000);
dev_info(info->dev, "rt9471_charger_set_vindpm:%d  %d\n", vol, reg_val);


	return rt9471_update_bits(info, RT9471_REG_VBUS,
				   RT9471_MIVR_MASK, reg_val << RT9471_MIVR_SHIFT);
}

static int rt9471_charger_set_termina_vol(struct rt9471_charger_info *info, u32 vol)
{
	u8 reg_val;

	//
	rt9471_update_bits(info, RT9471_REG_VCHG, RT9471_VRECHG_MASK,
				   1 << 7);

	reg_val = rt9471_closest_reg(RT9471_CV_MIN, RT9471_CV_MAX,
				    RT9471_CV_STEP, vol * 1000);
    dev_info(info->dev, "rt9471_charger_set_termina_vol:%d   %d\n", vol, reg_val);
    


	return rt9471_update_bits(info, RT9471_REG_VCHG, RT9471_CV_MASK,
				   reg_val << RT9471_CV_SHIFT);
}

static int rt9471_charger_set_termina_cur(struct rt9471_charger_info *info, u32 cur)
{
	u8 reg_val;
	
	reg_val = rt9471_closest_reg(RT9471_IEOC_MIN, RT9471_IEOC_MAX,
				    RT9471_IEOC_STEP, cur * 1000);
    dev_info(info->dev, "rt9471_charger_set_termina_cur:%d  %d\n", cur, reg_val);
	

	return rt9471_update_bits(info, RT9471_REG_EOC, RT9471_IEOC_MASK, reg_val << RT9471_IEOC_SHIFT);
}

static int rt9471_enable_safetytimer(struct rt9471_charger_info *info,bool en)
{

	int ret = 0;
	if (en)
		ret = rt9471_update_bits(info, RT9471_REG_CHGTIMER,
		RT9471_SAFETMR_EN_MASK, 1 << RT9471_SAFETMR_EN_SHIFT);
	else
		ret = rt9471_update_bits(info, RT9471_REG_CHGTIMER,
		RT9471_SAFETMR_EN_MASK, 0 << RT9471_SAFETMR_EN_SHIFT);

	return ret;
}

int rt9471_set_prechrg_curr(struct rt9471_charger_info *info,int pre_current)
{
	int reg_val;
	int offset = 50;
	int ret = 0;

	if (pre_current < 50)
		pre_current = 50;
	else if (pre_current > 800)
		pre_current = 80;
	reg_val = (pre_current - offset) / 50;

	printk("chg_rt9471_set_prechrg_curr:%d\n",reg_val);

	ret = rt9471_update_bits(info, RT9471_REG_PRECHG, 0x0F, reg_val);
	if (ret)
		dev_err(info->dev, "set rt9471 pre cur failed\n");

	return ret;
}
static int rt9471_charger_hw_init(struct rt9471_charger_info *info)
{
	struct sprd_battery_info bat_info;
	int ret;

	bat_info.bat_id = get_now_battery_id();

	ret = sprd_battery_get_battery_info(info->psy_usb, &bat_info, bat_info.bat_id);
	if (ret) {
		dev_warn(info->dev, "rt9471_charger_hw_init:no battery information is supplied\n");

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
	dev_warn(info->dev, "rt9471_charger_hw_init:have battery information is supplied\n");
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
#if 0
		ret = rt9471_update_bits(info, RT9471_REG_0B, REG0B_REG_RESET_MASK,
					  REG0B_REG_RESET << REG0B_REG_RESET_SHIFT);
		if (ret) {
			dev_err(info->dev, "reset rt9471 failed\n");
			return ret;
		}
#endif
		ret = rt9471_charger_set_vindpm(info, 4500);
		if (ret) {
			dev_err(info->dev, "set rt9471 vindpm vol failed\n");
			return ret;
		}

		ret = rt9471_charger_set_termina_vol(info,
						      info->vol_max_mv);
		if (ret) {
			dev_err(info->dev, "set rt9471 terminal vol failed\n");
			return ret;
		}

		ret = rt9471_charger_set_termina_cur(info, info->termination_cur);
		if (ret) {
			dev_err(info->dev, "set rt9471 terminal cur failed\n");
			return ret;
		}

		ret = rt9471_charger_set_limit_current(info,
							info->cur.unknown_cur);
		if (ret)
			dev_err(info->dev, "set rt9471 limit current failed\n");

		ret = rt9471_enable_safetytimer(info,false);
		if (ret)
			dev_err(info->dev, "set rt9471 set safetytimer failed\n");

		ret = rt9471_set_prechrg_curr(info,300);
		if (ret)
			dev_err(info->dev, "rt9471 set prechrg failed\n");

		// disable auto set AICR
	    ret = rt9471_update_bits(info, RT9471_REG_IBUS, RT9471_AUTOAICR_MASK, 0 << 6);
		if (ret)
			dev_err(info->dev, "rt9471 disable auto set AICR failed\n");

		// disable JEITA
	    ret = rt9471_update_bits(info, RT9471_REG_JEITA, RT9471_JEITA_EN_MASK, 0 << 7);
		if (ret)
			dev_err(info->dev, "rt9471 disable JEITA failed\n");

		// disable WDT
	    ret = rt9471_update_bits(info, RT9471_REG_TOP, RT9471_WDT_MASK, 0 << RT9471_WDT_SHIFT);
		if (ret)
			dev_err(info->dev, "set rt9471 disable WDT failed\n");


	}

	return ret;
}

/*
static int rt9471_charger_get_charge_voltage(struct rt9471_charger_info *info,
					      u32 *charge_vol)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(RT9471_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "failed to get RT9471_BATTERY_NAME\n");
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
*/

static int rt9471_charger_start_charge(struct rt9471_charger_info *info)
{
	int ret;

	dev_info(info->dev, "rt9471_charger_start_charge\n");
	ret = rt9471_update_bits(info, RT9471_REG_FUNCTION,
				  RT9471_HZ_MASK, 0 << RT9471_HZ_SHIFT);
	if (ret)
		dev_err(info->dev, "disable HIZ mode failed\n");

	ret = rt9471_update_bits(info, RT9471_REG_FUNCTION,
				  RT9471_CHG_EN_MASK, 1 << RT9471_CHG_EN_SHIFT);

//	ret = rt9471_update_bits(info, RT9471_REG_05, REG05_WDT_MASK,
//				  REG05_WDT_40S << REG05_WDT_SHIFT);
//    ret = rt9471_update_bits(info, RT9471_REG_05, REG05_WDT_MASK,
//				  REG05_WDT_DISABLE);
				  
	if (ret) {
		dev_err(info->dev, "Failed to enable rt9471 watchdog\n");
		return ret;
	}

	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask, 0);
	if (ret) {
		dev_err(info->dev, "enable rt9471 charge failed\n");
			return ret;
		}

	ret = rt9471_charger_set_limit_current(info,
						info->last_limit_current);
	if (ret) {
		dev_err(info->dev, "failed to set limit current\n");
		return ret;
	}

	ret = rt9471_charger_set_termina_cur(info, info->termination_cur);
	if (ret)
		dev_err(info->dev, "set rt9471 terminal cur failed\n");

    dev_info(info->dev, "rt9471 aquire charger wakelock\n");
    pm_stay_awake(info->dev);
	return ret;
}

static void rt9471_charger_stop_charge(struct rt9471_charger_info *info)
{
	int ret;
    dev_info(info->dev, "rt9471_charger_stop_charge rt9471 release charger wakelock\n");
    pm_relax(info->dev);

	if (info->need_disable_Q1) {
		ret = rt9471_update_bits(info, RT9471_REG_FUNCTION,
				  RT9471_HZ_MASK, 1 << RT9471_HZ_SHIFT);

		if (ret)
			dev_err(info->dev, "enable HIZ mode failed\n");
		info->need_disable_Q1 = false;
	}

	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask,
				 info->charger_pd_mask);
	if (ret)
		dev_err(info->dev, "disable rt9471 charge failed\n");

//	ret = rt9471_update_bits(info, RT9471_REG_05, REG05_WDT_MASK,
//				  REG05_WDT_DISABLE);
	if (ret)
		dev_err(info->dev, "Failed to disable rt9471 watchdog\n");

}

static int rt9471_charger_set_current(struct rt9471_charger_info *info,
				       u32 cur)
{
	u8 reg_val;
	int ret;

	reg_val = rt9471_closest_reg(RT9471_ICHG_MIN, RT9471_ICHG_MAX,
				    RT9471_ICHG_STEP, cur);
    dev_info(info->dev, "rt9471_charger_set_current :%d   %d\n", cur, reg_val);

	ret = rt9471_update_bits(info, RT9471_REG_ICHG, RT9471_ICHG_MASK, reg_val << RT9471_ICHG_SHIFT);
	
	return ret;
}

static int rt9471_charger_get_current(struct rt9471_charger_info *info,
				       u32 *cur)
{
	u8 reg_val;
	int ret;

	ret = rt9471_read(info, RT9471_REG_ICHG, &reg_val);
	if (ret < 0)
		return ret;

    reg_val = (reg_val & RT9471_ICHG_MASK) >> RT9471_ICHG_SHIFT;
	*cur = rt9471_closest_value(RT9471_ICHG_MIN, RT9471_ICHG_MAX,
				     RT9471_ICHG_STEP, reg_val);

    dev_info(info->dev, "rt9471_charger_get_current :%d\n", *cur);
	return 0;
}

static int rt9471_charger_set_limit_current(struct rt9471_charger_info *info,
					     u32 limit_cur)
{
	u8 reg_val;
	int ret;	

    info->last_limit_current = limit_cur;
	reg_val = rt9471_closest_reg(RT9471_AICR_MIN, RT9471_AICR_MAX,
				    RT9471_AICR_STEP, limit_cur);
	/* 0 & 1 are both 50mA */
	if (limit_cur < RT9471_AICR_MAX)
		reg_val += 1;
    dev_info(info->dev, "rt9471_charger_set_limit_current :%d   %d\n", limit_cur, reg_val);
	
	ret = rt9471_update_bits(info, RT9471_REG_IBUS, RT9471_AICR_MASK, reg_val << RT9471_AICR_SHIFT);
    if (ret)
		dev_err(info->dev, "set rt9471 limit cur failed\n");

	info->actual_limit_current = limit_cur;

	return ret;
}

static u32 rt9471_charger_get_limit_current(struct rt9471_charger_info *info,
					     u32 *limit_cur)
{
	u8 reg_val;
	int ret;

	ret = rt9471_read(info, RT9471_REG_IBUS, &reg_val);
	if (ret < 0)
		return ret;

	reg_val = (reg_val & RT9471_AICR_MASK) >> RT9471_AICR_SHIFT;
	*limit_cur = rt9471_closest_value(RT9471_AICR_MIN, RT9471_AICR_MAX,
				     RT9471_AICR_STEP, reg_val);

	if (*limit_cur > RT9471_AICR_MIN && *limit_cur < RT9471_AICR_MAX)
		*limit_cur -= RT9471_AICR_STEP;
    dev_info(info->dev, "rt9471_charger_get_limit_current[%d] info->actual_limit_current[%d]\n", *limit_cur, info->actual_limit_current);

	return 0;
}

static inline int rt9471_charger_get_health(struct rt9471_charger_info *info,
				      u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static inline int rt9471_charger_get_online(struct rt9471_charger_info *info,
				      u32 *online)
{
	if (info->limit)
		*online = true;
	else
		*online = false;

	return 0;
}

static int rt9471_charger_feed_watchdog(struct rt9471_charger_info *info,
					 u32 val)
{
	int ret;
	u32 limit_cur = 0;
dev_err(info->dev, "rt9471_charger_feed_watchdog In..\n");

	ret = rt9471_update_bits(info, RT9471_REG_TOP, RT9471_WDTCNTRST_MASK,
				  1 << 2);
	if (ret) {
		dev_err(info->dev, "reset rt9471 failed\n");
		return ret;
	}

	ret = rt9471_charger_get_limit_current(info, &limit_cur);
	if (ret) {
		dev_err(info->dev, "get limit cur failed\n");
		return ret;
	}

	if (info->actual_limit_current == limit_cur)
		return 0;

	ret = rt9471_charger_set_limit_current(info, info->actual_limit_current);
	if (ret) {
		dev_err(info->dev, "set limit cur failed\n");
		return ret;
	}

	return 0;
}

static inline int rt9471_charger_get_status(struct rt9471_charger_info *info)
{
	if (info->charging)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static int rt9471_charger_set_status(struct rt9471_charger_info *info,
				      int val)
{
	int ret = 0;
//	u32 input_vol;

dev_err(info->dev, "rt9471_charger_set_status  val=%d\n", val);

	if (!val && info->charging) {
		dev_info(info->dev,"rt9471_charger_set_status set status stop charging");
		rt9471_charger_stop_charge(info);
		info->charging = false;
	} else if (val && !info->charging) {
		dev_info(info->dev,"rt9471_charger_set_status set status start charging");
		ret = rt9471_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}

static void rt9471_charger_work(struct work_struct *data)
{
	struct rt9471_charger_info *info =
		container_of(data, struct rt9471_charger_info, work);
	bool present;

	present = rt9471_charger_is_bat_present(info);


	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	if (info->limit)
		schedule_delayed_work(&info->wdt_work, 0);
	else
		cancel_delayed_work_sync(&info->wdt_work);

#ifdef SET_DELAY_CHECK_LIMIT_INPUT_CURRENT
    if (info->limit) {
		schedule_delayed_work(&info->check_limit_current_work, HZ * 2);
	}
		
	else {
		cancel_delayed_work_sync(&info->check_limit_current_work);
	}	
	info->check_limit_current_cnt = 0;
#endif	

	dev_info(info->dev, "rt9471_charger_work: battery present = %d, charger type = %d info->limit = %d\n",
		 present, info->usb_phy->chg_type, info->limit);
	cm_notify_event(info->psy_usb, CM_EVENT_CHG_START_STOP, NULL);
}

static ssize_t rt9471_reg_val_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct rt9471_charger_sysfs *rt9471_sysfs =
		container_of(attr, struct rt9471_charger_sysfs,
			     attr_rt9471_reg_val);
	struct rt9471_charger_info *info = rt9471_sysfs->info;
	u8 val;
	int ret;

	if (!info)
		return sprintf(buf, "%s rt9471_sysfs->info is null\n", __func__);

	ret = rt9471_read(info, reg_tab[info->reg_id].addr, &val);
	if (ret) {
		dev_err(info->dev, "fail to get rt9471_REG_0x%.2x value, ret = %d\n",
			reg_tab[info->reg_id].addr, ret);
		return sprintf(buf, "fail to get rt9471_REG_0x%.2x value\n",
			       reg_tab[info->reg_id].addr);
	}

	return sprintf(buf, "rt9471_REG_0x%.2x = 0x%.2x\n",
		       reg_tab[info->reg_id].addr, val);
}

static ssize_t rt9471_reg_val_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	struct rt9471_charger_sysfs *rt9471_sysfs =
		container_of(attr, struct rt9471_charger_sysfs,
			     attr_rt9471_reg_val);
	struct rt9471_charger_info *info = rt9471_sysfs->info;
	u8 val;
	int ret;

	if (!info) {
		dev_err(dev, "%s rt9471_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtou8(buf, 16, &val);
	if (ret) {
		dev_err(info->dev, "fail to get addr, ret = %d\n", ret);
		return count;
	}

	ret = rt9471_write(info, reg_tab[info->reg_id].addr, val);
	if (ret) {
		dev_err(info->dev, "fail to wite 0x%.2x to REG_0x%.2x, ret = %d\n",
				val, reg_tab[info->reg_id].addr, ret);
		return count;
	}

	dev_info(info->dev, "wite 0x%.2x to REG_0x%.2x success\n", val,
		 reg_tab[info->reg_id].addr);
	return count;
}

static ssize_t rt9471_reg_id_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct rt9471_charger_sysfs *rt9471_sysfs =
		container_of(attr, struct rt9471_charger_sysfs,
			     attr_rt9471_sel_reg_id);
	struct rt9471_charger_info *info = rt9471_sysfs->info;
	int ret, id;

	if (!info) {
		dev_err(dev, "%s rt9471_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtoint(buf, 10, &id);
	if (ret) {
		dev_err(info->dev, "%s store register id fail\n", rt9471_sysfs->name);
		return count;
	}

	if (id < 0 || id >= 12) {
		dev_err(info->dev, "%s store register id fail, id = %d is out of range\n",
			rt9471_sysfs->name, id);
		return count;
	}

	info->reg_id = id;

	dev_info(info->dev, "%s store register id = %d success\n", rt9471_sysfs->name, id);
	return count;
}

static ssize_t rt9471_reg_id_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct rt9471_charger_sysfs *rt9471_sysfs =
		container_of(attr, struct rt9471_charger_sysfs,
			     attr_rt9471_sel_reg_id);
	struct rt9471_charger_info *info = rt9471_sysfs->info;

	if (!info)
		return sprintf(buf, "%s rt9471_sysfs->info is null\n", __func__);

	return sprintf(buf, "Cuurent register id = %d\n", info->reg_id);
}

static ssize_t rt9471_reg_table_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct rt9471_charger_sysfs *rt9471_sysfs =
		container_of(attr, struct rt9471_charger_sysfs,
			     attr_rt9471_lookup_reg);
	struct rt9471_charger_info *info = rt9471_sysfs->info;
	int i, len, idx = 0;
	char reg_tab_buf[2048];

	if (!info)
		return sprintf(buf, "%s rt9471_sysfs->info is null\n", __func__);

	memset(reg_tab_buf, '\0', sizeof(reg_tab_buf));
	len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
		       "Format: [id] [addr] [desc]\n");
	idx += len;

	for (i = 0; i < 12; i++) {
		len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
			       "[%d] [REG_0x%.2x] [%s]; \n",
			       reg_tab[i].id, reg_tab[i].addr, reg_tab[i].name);
		idx += len;
	}

	return sprintf(buf, "%s\n", reg_tab_buf);
}

static ssize_t rt9471_dump_reg_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct rt9471_charger_sysfs *rt9471_sysfs =
		container_of(attr, struct rt9471_charger_sysfs,
			     attr_rt9471_dump_reg);
	struct rt9471_charger_info *info = rt9471_sysfs->info;

	if (!info)
		return sprintf(buf, "%s rt9471_sysfs->info is null\n", __func__);

	rt9471_charger_dump_register(info);

	return sprintf(buf, "%s\n", rt9471_sysfs->name);
}

static int rt9471_register_sysfs(struct rt9471_charger_info *info)
{
	struct rt9471_charger_sysfs *rt9471_sysfs;
	int ret;

	rt9471_sysfs = devm_kzalloc(info->dev, sizeof(*rt9471_sysfs), GFP_KERNEL);
	if (!rt9471_sysfs)
		return -ENOMEM;

	info->sysfs = rt9471_sysfs;
	rt9471_sysfs->name = "rt9471_sysfs";
	rt9471_sysfs->info = info;
	rt9471_sysfs->attrs[0] = &rt9471_sysfs->attr_rt9471_dump_reg.attr;
	rt9471_sysfs->attrs[1] = &rt9471_sysfs->attr_rt9471_lookup_reg.attr;
	rt9471_sysfs->attrs[2] = &rt9471_sysfs->attr_rt9471_sel_reg_id.attr;
	rt9471_sysfs->attrs[3] = &rt9471_sysfs->attr_rt9471_reg_val.attr;
	rt9471_sysfs->attrs[4] = NULL;
	rt9471_sysfs->attr_g.name = "debug";
	rt9471_sysfs->attr_g.attrs = rt9471_sysfs->attrs;

	sysfs_attr_init(&rt9471_sysfs->attr_rt9471_dump_reg.attr);
	rt9471_sysfs->attr_rt9471_dump_reg.attr.name = "rt9471_dump_reg";
	rt9471_sysfs->attr_rt9471_dump_reg.attr.mode = 0444;
	rt9471_sysfs->attr_rt9471_dump_reg.show = rt9471_dump_reg_show;

	sysfs_attr_init(&rt9471_sysfs->attr_rt9471_lookup_reg.attr);
	rt9471_sysfs->attr_rt9471_lookup_reg.attr.name = "rt9471_lookup_reg";
	rt9471_sysfs->attr_rt9471_lookup_reg.attr.mode = 0444;
	rt9471_sysfs->attr_rt9471_lookup_reg.show = rt9471_reg_table_show;

	sysfs_attr_init(&rt9471_sysfs->attr_rt9471_sel_reg_id.attr);
	rt9471_sysfs->attr_rt9471_sel_reg_id.attr.name = "rt9471_sel_reg_id";
	rt9471_sysfs->attr_rt9471_sel_reg_id.attr.mode = 0644;
	rt9471_sysfs->attr_rt9471_sel_reg_id.show = rt9471_reg_id_show;
	rt9471_sysfs->attr_rt9471_sel_reg_id.store = rt9471_reg_id_store;

	sysfs_attr_init(&rt9471_sysfs->attr_rt9471_reg_val.attr);
	rt9471_sysfs->attr_rt9471_reg_val.attr.name = "rt9471_reg_val";
	rt9471_sysfs->attr_rt9471_reg_val.attr.mode = 0644;
	rt9471_sysfs->attr_rt9471_reg_val.show = rt9471_reg_val_show;
	rt9471_sysfs->attr_rt9471_reg_val.store = rt9471_reg_val_store;

	ret = sysfs_create_group(&info->psy_usb->dev.kobj, &rt9471_sysfs->attr_g);
	if (ret < 0)
		dev_err(info->dev, "Cannot create sysfs , ret = %d\n", ret);

	return ret;
}

static int rt9471_charger_usb_change(struct notifier_block *nb,
				      unsigned long limit, void *data)
{
	struct rt9471_charger_info *info =
		container_of(nb, struct rt9471_charger_info, usb_notify);

	info->limit = limit;
dev_info(info->dev, "rt9471_charger_usb_change , limit = %d  info->role= %d\n", limit, info->role);
    if (ccali_mode == true)return NOTIFY_OK;

	pm_wakeup_event(info->dev, RT9471_WAKE_UP_MS);

	schedule_work(&info->work);
	return NOTIFY_OK;
}

static int rt9471_charger_usb_get_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    union power_supply_propval *val)
{
	struct rt9471_charger_info *info = power_supply_get_drvdata(psy);
	u32 cur, online, health, enabled = 0;
	enum usb_charger_type type;
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (info->limit)
			val->intval = rt9471_charger_get_status(info);
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = rt9471_charger_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = rt9471_charger_get_limit_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		ret = rt9471_charger_get_online(info, &online);
		if (ret)
			goto out;

		val->intval = online;

		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = rt9471_charger_get_health(info, &health);
			if (ret)
				goto out;

			val->intval = health;
		}
		break;

	case POWER_SUPPLY_PROP_USB_TYPE:
		type = info->usb_phy->chg_type;

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

		break;

	case POWER_SUPPLY_PROP_CALIBRATE:
		ret = regmap_read(info->pmic, info->charger_pd, &enabled);
		if (ret) {
			dev_err(info->dev, "get rt9471 charge status failed\n");
			goto out;
		}

	//	val->intval = !enabled;
		val->intval = !(enabled & info->charger_pd_mask);
		
		pr_err("rt9471_charger_usb_get_property POWER_SUPPLY_PROP_CALIBRATE enabled[%x] val->intval = %d\n", enabled, val->intval);
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = info->otg_enable;
		break;

	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int rt9471_charger_usb_set_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    const union power_supply_propval *val)
{
	struct rt9471_charger_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = rt9471_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = rt9471_charger_set_limit_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set input current limit failed\n");
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = rt9471_charger_set_termina_vol(info, val->intval / 1000);
		if (ret < 0)
			dev_err(info->dev, "failed to set terminate voltage\n");
		break;

	case POWER_SUPPLY_PROP_CALIBRATE:
		if (val->intval == true) {
			ret = rt9471_charger_start_charge(info);
			if (ret)
				dev_err(info->dev, "start charge failed\n");
		} else if (val->intval == false) {
			rt9471_charger_stop_charge(info);
		}
		dev_info(info->dev, "POWER_SUPPLY_PROP_CHARGING_ENABLED: %s\n",
			 val->intval ? "enable" : "disable");
		break;
	
	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == CM_DUMP_CHARGER_REGISTER_CMD) {
			rt9471_charger_dump_register(info);			
			break;
		}
		ret = rt9471_charger_set_status(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");
		break;
	/*
	case POWER_SUPPLY_PROP_FEED_WATCHDOG:
		ret = rt9471_charger_feed_watchdog(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "feed charger watchdog failed\n");
		break;
	*/
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int rt9471_charger_property_is_writeable(struct power_supply *psy,
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

static enum power_supply_usb_type rt9471_charger_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};

static enum power_supply_property rt9471_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CALIBRATE,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_PRESENT,
};

static const struct power_supply_desc rt9471_charger_desc = {
	.name			= "bq2560x_charger",
//	.name			= "v5000_charger",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= rt9471_usb_props,
	.num_properties		= ARRAY_SIZE(rt9471_usb_props),
	.get_property		= rt9471_charger_usb_get_property,
	.set_property		= rt9471_charger_usb_set_property,
	.property_is_writeable	= rt9471_charger_property_is_writeable,
	.usb_types		= rt9471_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(rt9471_charger_usb_types),
};

static void rt9471_charger_detect_status(struct rt9471_charger_info *info)
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
	if (ccali_mode == true)return;
	
	schedule_work(&info->work);
}

static void rt9471_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct rt9471_charger_info *info = container_of(dwork,
							 struct rt9471_charger_info,
							 wdt_work);
	int ret;

	ret = rt9471_update_bits(info, RT9471_REG_TOP, RT9471_WDTCNTRST_MASK,
				  1 << 2);
	if (ret) {
		dev_err(info->dev, "reset rt9471 failed\n");
		return;
	}
	schedule_delayed_work(&info->wdt_work, HZ * 15);
}

#ifdef SET_DELAY_CHECK_LIMIT_INPUT_CURRENT
static void rt9471_delay_check_limit_current(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct rt9471_charger_info *info = container_of(dwork,
							 struct rt9471_charger_info,
							 check_limit_current_work);
	int ret;
	u32 limit_cur = 0;
	dev_info(info->dev, "%s:line%d enter cnt[%d]\n", __func__, __LINE__, info->check_limit_current_cnt);

	ret = rt9471_charger_get_limit_current(info, &limit_cur);
	if (ret) {
		dev_err(info->dev, "get limit cur failed\n");
		goto out;
	}

	if (limit_cur != info->actual_limit_current) {
		rt9471_charger_set_limit_current(info, info->actual_limit_current);
		dev_err(info->dev, "rt9471_delay_check_limit_current:reset limit!!!\n");
	}

out:	
	if (++ info->check_limit_current_cnt < 9)schedule_delayed_work(&info->check_limit_current_work, HZ * 2);
}
#endif	



#ifdef CONFIG_REGULATOR
static bool rt9471_charger_check_otg_valid(struct rt9471_charger_info *info)
{
	int ret;
	u8 value = 0;
	bool status = false;

	ret = rt9471_read(info, RT9471_REG_FUNCTION, &value);
	if (ret) {
		dev_err(info->dev, "get rt9471 charger otg valid status failed\n");
		return status;
	}

	if (value & RT9471_OTG_EN_MASK)
		status = true;
	else
		dev_err(info->dev, "otg is not valid, REG_3 = 0x%x\n", value);

	return status;
}

static bool rt9471_charger_check_otg_fault(struct rt9471_charger_info *info)
{
//	int ret;
//	u8 value = 0;
	bool status = true;
#if 0
	ret = rt9471_read(info, RT9471_REG_09, &value);
	if (ret) {
		dev_err(info->dev, "get rt9471 charger otg fault status failed\n");
		return status;
	}

	if (!(value & REG09_FAULT_BOOST_MASK))
		status = false;
	else
		dev_err(info->dev, "boost fault occurs, REG_0C = 0x%x\n",
			value);
#endif
	return status;
}

static void rt9471_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct rt9471_charger_info *info = container_of(dwork,
			struct rt9471_charger_info, otg_work);
	bool otg_valid = rt9471_charger_check_otg_valid(info);
	bool otg_fault;
	int ret, retry = 0;

	if (otg_valid)
		goto out;

	do {
		otg_fault = rt9471_charger_check_otg_fault(info);
		if (!otg_fault) {
			ret = rt9471_update_bits(info, RT9471_REG_FUNCTION,
						  RT9471_OTG_EN_MASK,
						  1 << RT9471_OTG_EN_SHIFT);
			if (ret)
				dev_err(info->dev, "restart rt9471 charger otg failed\n");
		}

		otg_valid = rt9471_charger_check_otg_valid(info);
	} while (!otg_valid && retry++ < RT9471_OTG_RETRY_TIMES);

	if (retry >= RT9471_OTG_RETRY_TIMES) {
		dev_err(info->dev, "Restart OTG failed\n");
		return;
	}

out:
	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(1500));
}

static int rt9471_charger_enable_otg(struct regulator_dev *dev)
{
	struct rt9471_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	dev_info(info->dev, "%s:line%d enter\n", __func__, __LINE__);
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

	ret = rt9471_update_bits(info, RT9471_REG_FUNCTION,
						  RT9471_OTG_EN_MASK,
						  1 << RT9471_OTG_EN_SHIFT);			  

	if (ret) {
		dev_err(info->dev, "enable rt9471 otg failed\n");
		regmap_update_bits(info->pmic, info->charger_detect,
				   BIT_DP_DM_BC_ENB, 0);
		return ret;
	}

	info->otg_enable = true;
	schedule_delayed_work(&info->wdt_work,
			      msecs_to_jiffies(RT9471_WDT_VALID_MS));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(RT9471_OTG_VALID_MS));

	return 0;
}

static int rt9471_charger_disable_otg(struct regulator_dev *dev)
{
	struct rt9471_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	dev_info(info->dev, "%s:line%d enter\n", __func__, __LINE__);
	info->otg_enable = false;
	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);
	ret = rt9471_update_bits(info, RT9471_REG_FUNCTION,
						  RT9471_OTG_EN_MASK,
						  0 << RT9471_OTG_EN_SHIFT);
	if (ret) {
		dev_err(info->dev, "disable rt9471 otg failed\n");
		return ret;
	}

	/* Enable charger detection function to identify the charger type */
	return regmap_update_bits(info->pmic, info->charger_detect,
				  BIT_DP_DM_BC_ENB, 0);
}

static int rt9471_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct rt9471_charger_info *info = rdev_get_drvdata(dev);
	int ret;
	u8 val;

	dev_info(info->dev, "%s:line%d enter\n", __func__, __LINE__);
	ret = rt9471_read(info, RT9471_REG_FUNCTION, &val);
	if (ret) {
		dev_err(info->dev, "failed to get rt9471 otg status\n");
		return ret;
	}

	val &= RT9471_OTG_EN_MASK;
	return val;
}

static const struct regulator_ops rt9471_charger_vbus_ops = {
	.enable = rt9471_charger_enable_otg,
	.disable = rt9471_charger_disable_otg,
	.is_enabled = rt9471_charger_vbus_is_enabled,
};

static const struct regulator_desc rt9471_charger_vbus_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &rt9471_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int rt9471_charger_register_vbus_regulator(struct rt9471_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev,
				      &rt9471_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "Can't register regulator:%d\n", ret);
	}

	return ret;
}

#else
static int rt9471_charger_register_vbus_regulator(struct rt9471_charger_info *info)
{
	return 0;
}
#endif

static int rt9471_charger_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct rt9471_charger_info *info;
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	int ret;
	
	charger_id = sprd_charger_parse_charger_id();
	get_boot_mode();
	pr_err("%s:line%d: Probe In ...charger_id[%d] ccali_mode[%d]\n", __func__, __LINE__, charger_id, ccali_mode);

    // 1:   sy6974b
    // 2:   sgm41511
	// 3:   rt9741
	// 4:   rt9741d
    
	if (charger_id == 3) {
	}
	else if (charger_id == 4) {
	}
	else return -ENODEV;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "No support for SMBUS_BYTE_DATA\n");
		return -ENODEV;
	}

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	/* HS03 for SR-SL6215-01-178 Import multi-charger driver patch of SPCSS00872701 by gaochao at 20210720 start */
	 client->addr = 0x53;
	/* HS03 for SR-SL6215-01-178 Import multi-charger driver patch of SPCSS00872701 by gaochao at 20210720 end */
	info->client = client;
	info->dev = dev;

	alarm_init(&info->otg_timer, ALARM_BOOTTIME, NULL);

	mutex_init(&info->lock);
	INIT_WORK(&info->work, rt9471_charger_work);

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

	ret = rt9471_charger_is_fgu_present(info);
	if (ret) {
		dev_err(dev, "sc27xx_fgu not ready.\n");
		return -EPROBE_DEFER;
	}

	ret = rt9471_charger_register_vbus_regulator(info);
	if (ret) {
		dev_err(dev, "failed to register vbus regulator.\n");
			goto err_psy_usb;
	}

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np)
		regmap_np = of_find_compatible_node(NULL, NULL, "sprd,ump962x-syscon");

	if (regmap_np) {
		if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
			info->charger_pd_mask = RT9471_DISABLE_PIN_MASK_2721;
		else
			info->charger_pd_mask = RT9471_DISABLE_PIN_MASK;
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
						   &rt9471_charger_desc,
						   &charger_cfg);

	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		ret = PTR_ERR(info->psy_usb);
		goto err_mutex_lock;
	}

	ret = rt9471_charger_hw_init(info);
	if (ret) {
		dev_err(dev, "failed to rt9471_charger_hw_init\n");
		goto err_mutex_lock;
	}

	rt9471_charger_stop_charge(info);

	device_init_wakeup(info->dev, true);
	info->usb_notify.notifier_call = rt9471_charger_usb_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		dev_err(dev, "failed to register notifier:%d\n", ret);
		goto err_psy_usb;
	}

	ret = rt9471_register_sysfs(info);
	if (ret) {
		dev_err(info->dev, "register sysfs fail, ret = %d\n", ret);
		goto err_sysfs;
	}

	

//	ret = rt9471_update_bits(info, RT9471_REG_05, REG05_WDT_MASK,
//				  REG05_WDT_40S << REG05_WDT_SHIFT);
//	ret = rt9471_update_bits(info, RT9471_REG_05, REG05_WDT_MASK,
//				  REG05_WDT_DISABLE);			  
	if (ret) {
		dev_err(info->dev, "Failed to enable rt9471 watchdog\n");
		return ret;
	}

	INIT_DELAYED_WORK(&info->otg_work, rt9471_charger_otg_work);
	INIT_DELAYED_WORK(&info->wdt_work,
			  rt9471_charger_feed_watchdog_work);
	#ifdef SET_DELAY_CHECK_LIMIT_INPUT_CURRENT
	INIT_DELAYED_WORK(&info->check_limit_current_work,
			  rt9471_delay_check_limit_current);
	#endif		  
	rt9471_charger_detect_status(info);
	pr_info("[%s]line=%d: probe success ccali_mode[%d]\n", __FUNCTION__, __LINE__, ccali_mode);

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

static int rt9471_charger_remove(struct i2c_client *client)
{
	struct rt9471_charger_info *info = i2c_get_clientdata(client);

	usb_unregister_notifier(info->usb_phy, &info->usb_notify);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int rt9471_charger_suspend(struct device *dev)
{
	struct rt9471_charger_info *info = dev_get_drvdata(dev);
	ktime_t now, add;
	unsigned int wakeup_ms = RT9471_OTG_ALARM_TIMER_MS;
	int ret;


	if (info->otg_enable || info->limit)
		rt9471_charger_feed_watchdog(info, 0);
	if (!info->otg_enable)
		return 0;

	cancel_delayed_work_sync(&info->wdt_work);

	/* feed watchdog first before suspend */
	ret = rt9471_update_bits(info, RT9471_REG_TOP, RT9471_WDTCNTRST_MASK,
				  1 << 2);

	if (ret)
		dev_warn(info->dev, "reset rt9471 failed before suspend\n");

	now = ktime_get_boottime();
	add = ktime_set(wakeup_ms / MSEC_PER_SEC,
		       (wakeup_ms % MSEC_PER_SEC) * NSEC_PER_MSEC);
	alarm_start(&info->otg_timer, ktime_add(now, add));

	return 0;
}

static int rt9471_charger_resume(struct device *dev)
{
	struct rt9471_charger_info *info = dev_get_drvdata(dev);
	int ret;

	if (!info->otg_enable)
		return 0;

	alarm_cancel(&info->otg_timer);

	/* feed watchdog first after resume */
	ret = rt9471_update_bits(info, RT9471_REG_TOP, RT9471_WDTCNTRST_MASK,
				  1 << 2);
	if (ret)
		dev_warn(info->dev, "reset rt9471 failed after resume\n");

	schedule_delayed_work(&info->wdt_work, HZ * 15);

	return 0;
}
#endif

static const struct dev_pm_ops rt9471_charger_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(rt9471_charger_suspend,
				rt9471_charger_resume)
};

static const struct i2c_device_id rt9471_i2c_id[] = {
	{"bq2560x_chg", 0},
	{}
};

static const struct of_device_id rt9471_charger_of_match[] = {
	{ .compatible = "ti,bq2560x_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, rt9471_charger_of_match);

static struct i2c_driver rt9471_charger_driver = {
	.driver = {
		.name = "bq2560x-charger",
		.of_match_table = rt9471_charger_of_match,
		.pm = &rt9471_charger_pm_ops,
	},
	.probe = rt9471_charger_probe,
	.remove = rt9471_charger_remove,
	.id_table = rt9471_i2c_id,
};

module_i2c_driver(rt9471_charger_driver);

MODULE_AUTHOR("qhq <Allen.Qin@sg-micro.com>");
MODULE_DESCRIPTION("RT9471 Charger Driver");
MODULE_LICENSE("GPL");

