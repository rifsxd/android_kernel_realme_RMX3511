// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 * MyungJoo Ham <myungjoo.ham@samsung.com>
 *
 * This driver enables to monitor battery health and control charger
 * during suspend-to-mem.
 * Charger manager depends on other devices. Register this later than
 * the depending devices.
 *
**/

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/io.h>
#include <linux/firmware.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/power/sprd_battery_info.h>
#include <linux/reboot.h>
#include <linux/regulator/consumer.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/iio/consumer.h>
#include <linux/hardware_info.h>
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/printk.h>
#include <linux/pinctrl/consumer.h>
#include <linux/gpio/consumer.h>
/*
 * Default temperature threshold for charging.
 * Every temperature units are in tenth of centigrade.
 */
#define CM_DEFAULT_RECHARGE_TEMP_DIFF		50
#define CM_DEFAULT_CHARGE_TEMP_MAX		500
#define CM_CAP_CYCLE_TRACK_TIME			5
#define CM_UVLO_OFFSET				50000
#define CM_FORCE_SET_FUEL_CAP_FULL		1000
#define CM_LOW_TEMP_REGION			100
#define CM_UVLO_CALIBRATION_VOLTAGE_THRESHOLD	3400000
#define CM_UVLO_CALIBRATION_CNT_THRESHOLD	3
#define CM_LOW_TEMP_SHUTDOWN_VALTAGE		3400000

#define CM_CAP_ONE_PERCENT			10
#define CM_HCAP_DECREASE_STEP			8
#ifdef VENDOR_KERNEL
#define CM_HCAP_THRESHOLD			985
#else
#define CM_HCAP_THRESHOLD			1001
#endif //defined VENDOR_KERNEL
#define CM_CAP_FULL_PERCENT			1000
#define CM_MAGIC_NUM				0x5A5AA5A5
#define CM_CAPACITY_LEVEL_CRITICAL		0
#define CM_CAPACITY_LEVEL_LOW			15
#define CM_CAPACITY_LEVEL_NORMAL		85
#define CM_CAPACITY_LEVEL_FULL			100
#define CM_CAPACITY_LEVEL_CRITICAL_VOLTAGE	3300000
#define CM_FAST_CHARGE_ENABLE_BATTERY_VOLTAGE	3400000
#define CM_FAST_CHARGE_ENABLE_CURRENT		1200000
#define CM_FAST_CHARGE_ENABLE_THERMAL_CURRENT	1000000
#define CM_FAST_CHARGE_DISABLE_BATTERY_VOLTAGE	3400000
#define CM_FAST_CHARGE_DISABLE_CURRENT		500000
#define CM_FAST_CHARGE_TRANSITION_CURRENT_1P5A	1500000
#define CM_FAST_CHARGE_CURRENT_2A		2000000
#define CM_FAST_CHARGE_VOLTAGE_9V		9000000
#define CM_FAST_CHARGE_VOLTAGE_5V		5000000
#define CM_FAST_CHARGE_START_VOLTAGE_LTHRESHOLD	3520000
#define CM_FAST_CHARGE_START_VOLTAGE_HTHRESHOLD	4200000
#ifdef VENDOR_KERNEL
#define CM_FAST_CHARGE_ENABLE_COUNT		3
#endif //defined VENDOR_KERNEL
#define CM_FAST_CHARGE_DISABLE_COUNT		2

#define CM_CP_VSTEP				20000
#define CM_CP_ISTEP				50000
#define CM_CP_PRIMARY_CHARGER_DIS_TIMEOUT	20
#define CM_CP_IBAT_UCP_THRESHOLD		8
#define CM_CP_ADJUST_VOLTAGE_THRESHOLD		(5 * 1000 / CM_CP_WORK_TIME_MS)
#define CM_CP_ACC_VBAT_HTHRESHOLD		3850000
#define CM_CP_VBAT_STEP1			300000
#define CM_CP_VBAT_STEP2			150000
#define CM_CP_VBAT_STEP3			50000
#define CM_CP_IBAT_STEP1			2000000
#define CM_CP_IBAT_STEP2			1000000
#define CM_CP_IBAT_STEP3			100000
#define CM_CP_VBUS_STEP1			2000000
#define CM_CP_VBUS_STEP2			1000000
#define CM_CP_VBUS_STEP3			50000
#define CM_CP_IBUS_STEP1			1000000
#define CM_CP_IBUS_STEP2			500000
#define CM_CP_IBUS_STEP3			100000

#define CM_CAPACITY_CAP_ONE_TIME_30S	30
#define CM_CAPACITY_CAP_ONE_TIME_20S	14
#define CM_CAPACITY_CAP_CYCLE_15S	15
#define CM_CAPACITY_CAP_CYCLE_10S	7

#define CM_IR_COMPENSATION_TIME			3

#define CM_CP_WORK_TIME_MS			500
#define BATTERY_VOLTAGE_MAX     4550000
#define BATTERY_RECHARGE_VOLTAGE     4370000

#define CM_ATO_CHARGE_STATUS                (1 << 4)
#define JEITA_HIGHEST_TEMPE         530
#define JEITA_LOWEST_TEMPE         -100
#define NTC_DISCONNECT_TEMP		-200
#define CM_BAT_VOL_SHAKE_COUNT                  3
#define CM_RESTORE_BATT_VOL_SHAKE           4000000
#ifndef VENDOR_KERNEL
#define CM_FAST_CHARGE_ENABLE_COUNT			4
#define CM_FAST_CHARGE_ENABLE_CAP			900
#define CM_CC_CV_VOLTAGE					4435000
#define CM_FAST_CHARGE_ENABLE_BATTERY_VOLTAGE_MIN   3500000
#define CM_FAST_CHARGE_ENABLE_BATTERY_VOLTAGE_MAX   4550000
#define CM_FAST_CHARGE_ENABLE_TEMP_MAX      420
#define CM_FAST_CHARGE_ENABLE_TEMP_MIN      0
#endif //not defined VENDOR_KERNEL
#define CM_TRY_DIS_FCHG_WORK_MS			100
#ifdef VENDOR_KERNEL
#define CM_HIGH_TEMP_STATUS      9//��ֵ��Ҫ��Ӧ46��jeita״̬ dcp-jeita-temp-table
#else
#define CM_HIGH_TEMP_STATUS      6
#endif //defined VENDOR_KERNEL
#define CM_HIGH_TEMP_STATUS_CV      4070000
#define HW_BAT_FULL_VOLT 50000
enum {
	BAT_OVP_SET = 0,
	FRAMEWORK_SET,
	MMI_SET,
	AGING_SET,
	ATO_SET,
	OVER_JEITA_TEMP_LIMIT,
	VBUS_OVP,
	BAT_POOR_CONTACT,
	BELOW_JEITA_TEMP_LIMIT,
	SAFETY_TIMER,
	HIZ_SET,
	HIGH_LOW_TEMP_SET,
	OTHERS_MAX,
};
static const char * const cm_cp_state_names[] = {
	[CM_CP_STATE_UNKNOWN] = "Charge pump state: UNKNOWN",
	[CM_CP_STATE_RECOVERY] = "Charge pump state: RECOVERY",
	[CM_CP_STATE_ENTRY] = "Charge pump state: ENTRY",
	[CM_CP_STATE_CHECK_VBUS] = "Charge pump state: CHECK VBUS",
	[CM_CP_STATE_TUNE] = "Charge pump state: TUNE",
	[CM_CP_STATE_EXIT] = "Charge pump state: EXIT",
};

static char *charger_manager_supplied_to[] = {
	"audio-ldo",
};

/*
 * Regard CM_JIFFIES_SMALL jiffies is small enough to ignore for
 * delayed works so that we can run delayed works with CM_JIFFIES_SMALL
 * without any delays.
 */
#define	CM_JIFFIES_SMALL	(2)

/* If y is valid (> 0) and smaller than x, do x = y */
#define CM_MIN_VALID(x, y)	x = (((y > 0) && ((x) > (y))) ? (y) : (x))

/*
 * Regard CM_RTC_SMALL (sec) is small enough to ignore error in invoking
 * rtc alarm. It should be 2 or larger
 */
#define CM_RTC_SMALL		(2)
int g_temp_debug = 0;
#define CM_EVENT_TYPE_NUM	6
static LIST_HEAD(cm_list);
static DEFINE_MUTEX(cm_list_mtx);

/* About in-suspend (suspend-again) monitoring */
static struct alarm *cm_timer;

static bool cm_suspended;
static bool cm_timer_set;
static unsigned long cm_suspend_duration_ms;
static int cm_event_num;
static enum cm_event_types cm_event_type[CM_EVENT_TYPE_NUM];
static char *cm_event_msg[CM_EVENT_TYPE_NUM];

/* About normal (not suspended) monitoring */
static unsigned long polling_jiffy = ULONG_MAX; /* ULONG_MAX: no polling */
static unsigned long next_polling; /* Next appointed polling time */
static struct workqueue_struct *cm_wq; /* init at driver add */
static struct delayed_work cm_monitor_work; /* init at driver add */

/*odm.hq.bsp.luowenjiang@huaqin.com 2021.11.12 modify for usb temp protection start*/
#ifdef CONFIG_HQ_USB_TEMP_CHECK
static struct charger_manager *my_cm;
static void cm_wake_up_usbtemp_thread(void);
#endif
/*odm.hq.bsp.luowenjiang@huaqin.com 2021.11.12 modify for usb temp protection end*/

static bool allow_charger_enable;
static bool is_charger_mode;
static void cm_notify_type_handle(struct charger_manager *cm, enum cm_event_types type, char *msg);
static bool cm_manager_adjust_current(struct charger_manager *cm, int jeita_status);
static void cm_update_charger_type_status(struct charger_manager *cm);
static int cm_manager_get_jeita_status(struct charger_manager *cm, int cur_temp);
static bool cm_charger_is_support_fchg(struct charger_manager *cm);
static int cm_manager_step_charge_monitor(struct charger_manager *cm);

#ifdef CONFIG_HQ_USB_TEMP_CHECK
extern struct sc27xx_typec *psc ;
extern int sc27xx_typec_set_sink(struct sc27xx_typec *sc);
extern int sc27xx_typec_set_enable(struct sc27xx_typec *sc);
#endif //defined CONFIG_HQ_USB_TEMP_CHECK
int cm_ntc_get_current_temp(struct charger_manager *cm);
#ifndef VENDOR_KERNEL
#ifdef CONFIG_HQ_USB_TEMP_CHECK
/*odm.hq.bsp.luowenjiang@huaqin.com 2022.01.08 modify for cc short to vbus start*/
extern bool sc27xx_typec_cc1_cc2_voltage_detect(struct sc27xx_typec *sc);
/*odm.hq.bsp.luowenjiang@huaqin.com 2022.01.08 modify for cc short to vbus start*/
#endif //defined CONFIG_HQ_USB_TEMP_CHECK
static int otg_switch_state;
#endif //not defined VENDOR_KERNEL
unsigned int notify_code = 0;
static bool bat_ovp_flag = false;
static bool g_bat_vol_shake = false;
int i_ovp = 0;
static bool g_is_fast_charge = false;
int prj_name = 0;
int runin_stop = 0;//1--stop charging 0--star charging
EXPORT_SYMBOL_GPL(prj_name);
EXPORT_SYMBOL_GPL(runin_stop);
EXPORT_SYMBOL_GPL(i_ovp);

#define OVER_HIGH_TEMP         850
static int charge_stop = 0;//1--stop charging 0--star charging
int ship_mode = 0;
int g_cool_down = 0;
int g_factory_mode = 0;
static int charger_full_count = 0,hw_full_counts = 0;
static bool is_full_warm = false;
static int  last_temp = -100;

/*odm.hq.bsp.luowenjiang@huaqin.com 2021.12.30 modify for error full charge status start*/
static bool error_full_status = false;
/*odm.hq.bsp.luowenjiang@huaqin.com 2021.12.30 modify for error full charge status end*/
#define ATL_BATTERY_VOLTAGE_MIN_5000  630
#ifdef CONFIG_HQ_USB_TEMP_CHECK
/*odm.hq.bsp.luowenjiang@huaqin.com 2022.01.08 modify for cc short to vbus start*/
static bool g_cc_vbus_status = false;
/*odm.hq.bsp.luowenjiang@huaqin.com 2022.01.08 modify for cc short to vbus end*/
#endif //defined CONFIG_HQ_USB_TEMP_CHECK
#define ATL_BATTERY_VOLTAGE_MAX_5000  810
#define GUANYU_BATTERY_VOLTAGE_MAX_5000    510
#define GUANYU_BATTERY_VOLTAGE_MIN_5000    310
static int batid_volt = 0;
int sc27xx_fgu_bat_id = 0;
EXPORT_SYMBOL_GPL(sc27xx_fgu_bat_id);
static char cm_prj_name[10] = "unknow";
bool bbat_mode = false;

#ifdef CONFIG_HQ_USB_TEMP_CHECK
extern int g_cc_polarity;
#endif
static int cm_get_battery_temperature_by_psy(struct charger_manager *cm, int *temp);
static int get_prj_name_setup(void)
{

	struct device_node *cmdline_node;
	const char *cmd_line, *temp_name;
	int rc = 0;

	cmdline_node = of_find_node_by_path("/chosen");
	rc = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
	if (!rc) {
		temp_name = strstr(cmd_line, "prj_name=");
		if (temp_name) {
			sscanf(temp_name, "prj_name=%s", cm_prj_name);
			if (!(strncmp(cm_prj_name, "21660", 5) && strncmp(cm_prj_name, "21661", 5) && strncmp(cm_prj_name, "21662", 5) && 
				strncmp(cm_prj_name, "216DA", 5) && strncmp(cm_prj_name, "216DB", 5) && strncmp(cm_prj_name, "21735", 5) &&
				strncmp(cm_prj_name, "216DC", 5) && strncmp(cm_prj_name, "216DE", 5) && strncmp(cm_prj_name, "21663", 5) &&
				strncmp(cm_prj_name, "2173B", 5) && strncmp(cm_prj_name, "2173C", 5) && strncmp(cm_prj_name, "2173E", 5) &&
				strncmp(cm_prj_name, "2173F", 5)))
				prj_name = 1;//NICO,NICO_B,,NICO_C
			else if (!(strncmp(cm_prj_name, "2171A", 5) && strncmp(cm_prj_name, "2171C", 5) && strncmp(cm_prj_name, "2171B", 5) && 
				strncmp(cm_prj_name, "2171D", 5) && strncmp(cm_prj_name, "2171E", 5) && strncmp(cm_prj_name, "21724", 5)))
				prj_name = 2;//NICKY
			else if (!(strncmp(cm_prj_name, "2171F", 5) && strncmp(cm_prj_name, "21721", 5) && strncmp(cm_prj_name, "21720", 5) && 
				strncmp(cm_prj_name, "21722", 5) && strncmp(cm_prj_name, "21723", 5)))
				prj_name = 3;//NICKY-A
				pr_err("%s: cm_prj_name=%s  prj_name=%d\n", __func__, cm_prj_name, prj_name);
		} else {
			pr_err("%s: cm_prj_name read error", __func__);
		}
	}

	return rc;
}

int cm_get_bat_id(struct charger_manager *cm)
{
	int ret, id_vol;

	ret = iio_read_channel_processed(cm->bat_id_cha, &id_vol);
	if (ret < 0){
		dev_err(cm->dev, "bat-id iio_read failed\n");
		return ret;
	}else{
		dev_err(cm->dev, "bat-id iio_read vol:%d\n",id_vol);
	}

	batid_volt = id_vol;
	if ((id_vol >= ATL_BATTERY_VOLTAGE_MIN_5000)&&(id_vol <= ATL_BATTERY_VOLTAGE_MAX_5000)) {
#ifndef VENDOR_KERNEL // VENDOR_KERNEL：for beidou_T
		get_hardware_info_data(HWID_BATERY_ID,"ATL 4.45V");
#endif
		sc27xx_fgu_bat_id = 1;
	} else if((id_vol >= GUANYU_BATTERY_VOLTAGE_MIN_5000)&&(id_vol <= GUANYU_BATTERY_VOLTAGE_MAX_5000)){
#ifndef VENDOR_KERNEL // VENDOR_KERNEL：for beidou_T
		get_hardware_info_data(HWID_BATERY_ID,"GUANYU 4.45V");
#endif
		sc27xx_fgu_bat_id = 2;
	}else{
		sc27xx_fgu_bat_id = 3;
#ifndef VENDOR_KERNEL // VENDOR_KERNEL：for beidou_T
		get_hardware_info_data(HWID_BATERY_ID,"OTHERS");
#endif
        }
	pr_err("**********sc27xx_fgu_bat_id=%d", sc27xx_fgu_bat_id);

	return sc27xx_fgu_bat_id;
}

EXPORT_SYMBOL_GPL(cm_get_bat_id);
static int cm_battery_status = 0;
static void set_batt_charge_status(int user, int status, int enable)
{
	if (enable) {
		cm_battery_status &= ~status;
	} else {
		cm_battery_status |= status;
	}
}
static int set_hiz_mode(struct charger_manager *cm,bool hiz_en)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret,i;

	for (i = 0; desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			 desc->psy_charger_stat[i]);
			 continue;
		}

		if (hiz_en)
			val.intval = CM_HIZ_ENABLE_CMD;
		else
			val.intval = CM_HIZ_DISABLE_CMD;

		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
		power_supply_put(psy);
		if (ret) {
			dev_err(cm->dev,"set hiz mode fail\n");
			return ret;
		}
	}
	return 0;
}

static int sprd_pin_set_gpio(char *pinctrlName);
static int pull_gpio(int num,int value);

static void cm_cap_remap_init_boundary(struct charger_desc *desc, int index, struct device *dev)
{

	if (index == 0) {
		desc->cap_remap_table[index].lb = (desc->cap_remap_table[index].lcap) * 1000;
		desc->cap_remap_total_cnt = desc->cap_remap_table[index].lcap;
	} else {
		desc->cap_remap_table[index].lb = desc->cap_remap_table[index - 1].hb +
			(desc->cap_remap_table[index].lcap -
			 desc->cap_remap_table[index - 1].hcap) * 1000;
		desc->cap_remap_total_cnt += (desc->cap_remap_table[index].lcap -
					      desc->cap_remap_table[index - 1].hcap);
	}

	desc->cap_remap_table[index].hb = desc->cap_remap_table[index].lb +
		(desc->cap_remap_table[index].hcap - desc->cap_remap_table[index].lcap) *
		desc->cap_remap_table[index].cnt * 1000;

	desc->cap_remap_total_cnt +=
		(desc->cap_remap_table[index].hcap - desc->cap_remap_table[index].lcap) *
		desc->cap_remap_table[index].cnt;

	dev_info(dev, "%s, cap_remap_table[%d].lb =%d,cap_remap_table[%d].hb = %d\n",
		 __func__, index, desc->cap_remap_table[index].lb, index,
		 desc->cap_remap_table[index].hb);
}

/*
 * cm_capacity_remap - remap fuel_cap
 * @ fuel_cap: cap from fuel gauge
 * Return the remapped cap
 */
static int cm_capacity_remap(struct charger_manager *cm, int fuel_cap)
{
	int i, temp, cap = 0;

	if (cm->desc->cap_remap_full_percent) {
		fuel_cap = fuel_cap * 100 / cm->desc->cap_remap_full_percent;
		if (fuel_cap > CM_CAP_FULL_PERCENT)
			fuel_cap  = CM_CAP_FULL_PERCENT;
	}

	if (!cm->desc->cap_remap_table)
		return fuel_cap;

	if (fuel_cap < 0) {
		fuel_cap = 0;
		return 0;
	} else if (fuel_cap >  CM_CAP_FULL_PERCENT) {
		fuel_cap  = CM_CAP_FULL_PERCENT;
		return fuel_cap;
	}

	temp = fuel_cap * cm->desc->cap_remap_total_cnt;

	for (i = 0; i < cm->desc->cap_remap_table_len; i++) {
		if (temp <= cm->desc->cap_remap_table[i].lb) {
			if (i == 0)
				cap = DIV_ROUND_CLOSEST(temp, 100);
			else
				cap = DIV_ROUND_CLOSEST((temp -
					cm->desc->cap_remap_table[i - 1].hb), 100) +
					cm->desc->cap_remap_table[i - 1].hcap * 10;
			break;
		} else if (temp <= cm->desc->cap_remap_table[i].hb) {
			cap = DIV_ROUND_CLOSEST((temp - cm->desc->cap_remap_table[i].lb),
						cm->desc->cap_remap_table[i].cnt * 100)
				+ cm->desc->cap_remap_table[i].lcap * 10;
			break;
		}

		if (i == cm->desc->cap_remap_table_len - 1 && temp > cm->desc->cap_remap_table[i].hb)
			cap = DIV_ROUND_CLOSEST((temp - cm->desc->cap_remap_table[i].hb), 100)
				+ cm->desc->cap_remap_table[i].hcap;

	}

	return cap;
}

static int cm_init_cap_remap_table(struct charger_desc *desc, struct device *dev)
{

	struct device_node *np = dev->of_node;
	const __be32 *list;
	int i, size;

	list = of_get_property(np, "cm-cap-remap-table", &size);
	if (!list || !size) {
		dev_err(dev, "%s  get cm-cap-remap-table fail\n", __func__);
		return 0;
	}
	desc->cap_remap_table_len = (u32)size / (3 * sizeof(__be32));
	desc->cap_remap_table = devm_kzalloc(dev, sizeof(struct cap_remap_table) *
				(desc->cap_remap_table_len + 1), GFP_KERNEL);
	if (!desc->cap_remap_table) {
		dev_err(dev, "%s, get cap_remap_table fail\n", __func__);
		return -ENOMEM;
	}
	for (i = 0; i < desc->cap_remap_table_len; i++) {
		desc->cap_remap_table[i].lcap = be32_to_cpu(*list++);
		desc->cap_remap_table[i].hcap = be32_to_cpu(*list++);
		desc->cap_remap_table[i].cnt = be32_to_cpu(*list++);

		cm_cap_remap_init_boundary(desc, i, dev);

		dev_info(dev, "cap_remap_table[%d].lcap= %d,cap_remap_table[%d].hcap = %d,"
			 "cap_remap_table[%d].cnt= %d\n", i, desc->cap_remap_table[i].lcap,
			 i, desc->cap_remap_table[i].hcap, i, desc->cap_remap_table[i].cnt);
	}

	if (desc->cap_remap_table[desc->cap_remap_table_len - 1].hcap != 100)
		desc->cap_remap_total_cnt +=
			(100 - desc->cap_remap_table[desc->cap_remap_table_len - 1].hcap);

	dev_info(dev, "cap_remap_total_cnt =%d, cap_remap_table_len = %d\n",
		 desc->cap_remap_total_cnt, desc->cap_remap_table_len);

	return 0;
}

/**
 * is_batt_present - See if the battery presents in place.
 * @cm: the Charger Manager representing the battery.
 */
static bool is_batt_present(struct charger_manager *cm)
{
	union power_supply_propval val;
	struct power_supply *psy;
	bool present = false;
	int i, ret;

	switch (cm->desc->battery_present) {
	case CM_BATTERY_PRESENT:
		present = true;
		break;
	case CM_NO_BATTERY:
		break;
	case CM_FUEL_GAUGE:
		psy = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
		if (!psy)
			break;

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT, &val);
		if (ret == 0 && val.intval)
			present = true;
		power_supply_put(psy);
		break;
	case CM_CHARGER_STAT:
		for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
			psy = power_supply_get_by_name(
					cm->desc->psy_charger_stat[i]);
			if (!psy) {
				dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
				continue;
			}

			ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT, &val);
			power_supply_put(psy);
			if (ret == 0 && val.intval) {
				present = true;
				break;
			}
		}
		break;
	}

	return present;
}

static bool is_ext_wl_pwr_online(struct charger_manager *cm)
{
	union power_supply_propval val;
	struct power_supply *psy;
	bool online = false;
	int i, ret;

	if (!cm->desc->psy_wl_charger_stat)
		return online;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_wl_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_wl_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_wl_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE, &val);
		power_supply_put(psy);
		if (ret == 0 && val.intval) {
			online = true;
			break;
		}
	}

	return online;
}

/**
 * is_ext_usb_pwr_online - See if an external power source is attached to charge
 * @cm: the Charger Manager representing the battery.
 *
 * Returns true if at least one of the chargers of the battery has an external
 * power source attached to charge the battery regardless of whether it is
 * actually charging or not.
 */
static bool is_ext_usb_pwr_online(struct charger_manager *cm)
{
	union power_supply_propval val;
	struct power_supply *psy;
	bool online = false;
	int i, ret;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE, &val);
		power_supply_put(psy);
		if (ret == 0 && val.intval) {
			online = true;
			break;
		}
	}

	return online;
}

static bool is_ext_pwr_online(struct charger_manager *cm)
{
	bool online = false;

	if (is_ext_usb_pwr_online(cm) || is_ext_wl_pwr_online(cm))
		online = true;

	return online;
}

/**
 * get_cp_ibat_uA - Get the charge current of the battery from charge pump
 * @cm: the Charger Manager representing the battery.
 * @uA: the current returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_cp_ibat_uA(struct charger_manager *cm, int *uA)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int i, ret = -ENODEV;

	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!cp_psy) {
			dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[i]);
			continue;
		}

		val.intval = CM_IBAT_CURRENT_NOW_CMD;
		ret = power_supply_get_property(cp_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
		power_supply_put(cp_psy);
		if (ret == 0)
			*uA += val.intval;
	}

	return ret;
}

/**
 * get_cp_vbat_uV - Get the voltage level of the battery from charge pump
 * @cm: the Charger Manager representing the battery.
 * @uV: the voltage level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_cp_vbat_uV(struct charger_manager *cm, int *uV)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int i, ret = -ENODEV;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!cp_psy) {
			dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[i]);
			continue;
		}

		ret = power_supply_get_property(cp_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
		power_supply_put(cp_psy);
		if (ret == 0) {
			*uV = val.intval;
			break;
		}
	}

	return ret;
}

/**
 * get_cp_vbus_uV - Get the voltage level of the bus from charge pump
 * @cm: the Charger Manager representing the battery.
 * @uV: the voltage level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_cp_vbus_uV(struct charger_manager *cm, int *uV)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int i, ret = -ENODEV;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!cp_psy) {
			dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[i]);
			continue;
		}

		ret = power_supply_get_property(cp_psy,
						POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, &val);
		power_supply_put(cp_psy);
		if (ret == 0) {
			*uV = val.intval;
			break;
		}
	}

	return ret;
}

 /**
  * get_cp_ibus_uA - Get the current level of the charge pump
  * @cm: the Charger Manager representing the battery.
  * @uA: the current level returned.
  *
  * Returns 0 if there is no error.
  * Returns a negative value on error.
  */
static int get_cp_ibus_uA(struct charger_manager *cm, int *cur)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int i, ret = -ENODEV;

	if (!cm->desc->psy_cp_stat)
		return 0;

	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!cp_psy) {
			dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[i]);
			continue;
		}

		val.intval = CM_IBUS_CURRENT_NOW_CMD;
		ret = power_supply_get_property(cp_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
		power_supply_put(cp_psy);
		if (ret == 0)
			*cur += val.intval;
	}

	return ret;
}

static int get_cp_ibat_uA_by_id(struct charger_manager *cm, int *cur, int id)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int ret = -ENODEV;

	*cur = 0;

	if (!cm->desc->psy_cp_stat || !cm->desc->psy_cp_stat[id])
		return 0;

	cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[id]);
	if (!cp_psy) {
		dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
			cm->desc->psy_cp_stat[id]);
		return ret;
	}

	ret = power_supply_get_property(cp_psy,
					POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	power_supply_put(cp_psy);
	if (ret == 0)
		*cur = val.intval;

	return ret;
}

 /**
  * get_ibat_avg_uA - Get the current level of the battery
  * @cm: the Charger Manager representing the battery.
  * @uA: the current level returned.
  *
  * Returns 0 if there is no error.
  * Returns a negative value on error.
  */
static int get_ibat_avg_uA(struct charger_manager *cm, int *uA)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CURRENT_AVG, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*uA = val.intval;
	return 0;
}

 /**
  * get_ibat_now_uA - Get the current level of the battery
  * @cm: the Charger Manager representing the battery.
  * @uA: the current level returned.
  *
  * Returns 0 if there is no error.
  * Returns a negative value on error.
  */
static int get_ibat_now_uA(struct charger_manager *cm, int *uA)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	val.intval = CM_IBAT_CURRENT_NOW_CMD;
	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*uA = val.intval;
	return 0;
}

/**
 *
 * get_vbat_avg_uV - Get the voltage level of the battery
 * @cm: the Charger Manager representing the battery.
 * @uV: the voltage level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_vbat_avg_uV(struct charger_manager *cm, int *uV)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_VOLTAGE_AVG, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*uV = val.intval;
	return 0;
}

/*
 * get_batt_ocv - Get the battery ocv
 * level of the battery.
 * @cm: the Charger Manager representing the battery.
 * @uV: the voltage level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_batt_ocv(struct charger_manager *cm, int *ocv)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_VOLTAGE_OCV, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*ocv = val.intval;
	return 0;
}

/*
 * get_batt_now - Get the battery voltage now
 * level of the battery.
 * @cm: the Charger Manager representing the battery.
 * @uV: the voltage level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_vbat_now_uV(struct charger_manager *cm, int *ocv)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*ocv = val.intval;
	return 0;
}

/**
 * get_batt_cap - Get the cap level of the battery
 * @cm: the Charger Manager representing the battery.
 * @uV: the cap level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_batt_cap(struct charger_manager *cm, int *cap)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	val.intval = 0;
	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CAPACITY, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*cap = val.intval;
	return 0;
}

/**
 * get_batt_total_cap - Get the total capacity level of the battery
 * @cm: the Charger Manager representing the battery.
 * @uV: the total_cap level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_batt_total_cap(struct charger_manager *cm, u32 *total_cap)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
					&val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*total_cap = val.intval;

	return 0;
}

/*
 * get_boot_cap - Get the battery boot capacity
 * of the battery.
 * @cm: the Charger Manager representing the battery.
 * @cap: the battery capacity returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_boot_cap(struct charger_manager *cm, int *cap)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	val.intval = CM_BOOT_CAPACITY;
	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CAPACITY, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*cap = val.intval;
	return 0;
}

static int cm_get_charge_cycle(struct charger_manager *cm, int *cycle)
{
	struct power_supply *fuel_gauge = NULL;
	union power_supply_propval val;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge) {
		ret = -ENODEV;
		return ret;
	}

	*cycle = 0;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CYCLE_COUNT, &val);
	if (ret)
		return ret;

	power_supply_put(fuel_gauge);
	*cycle = val.intval;

	return 0;
}

static int cm_get_usb_type(struct charger_manager *cm, u32 *type)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int ret = -EINVAL, i;

	*type = POWER_SUPPLY_USB_TYPE_UNKNOWN;

	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_USB_TYPE, &val);
		power_supply_put(psy);
		if (ret == 0) {
			*type = val.intval;
			break;
		}
	}

	return ret;
}

/**
 * get_usb_charger_type - Get the charger type
 * @cm: the Charger Manager representing the battery.
 * @type: the charger type returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_usb_charger_type(struct charger_manager *cm, u32 *type)
{
	int ret = -EINVAL;

	mutex_lock(&cm->desc->charger_type_mtx);
	if (cm->desc->is_fast_charge) {
		mutex_unlock(&cm->desc->charger_type_mtx);
		return 0;
	}

	ret = cm_get_usb_type(cm, type);

	mutex_unlock(&cm->desc->charger_type_mtx);
	return ret;
}

/**
 * get_wireless_charger_type - Get the wireless_charger type
 * @cm: the Charger Manager representing the battery.
 * @type: the wireless charger type returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_wireless_charger_type(struct charger_manager *cm, u32 *type)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int ret = -EINVAL, i;

	if (!cm->desc->psy_wl_charger_stat)
		return 0;

	mutex_lock(&cm->desc->charger_type_mtx);
	for (i = 0; cm->desc->psy_wl_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_wl_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_wl_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TYPE, &val);
		power_supply_put(psy);
		if (ret == 0) {
			*type = val.intval;
			break;
		}
	}

	mutex_unlock(&cm->desc->charger_type_mtx);

	return ret;
}

/**
 * set_batt_cap - Set the cap level of the battery
 * @cm: the Charger Manager representing the battery.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int set_batt_cap(struct charger_manager *cm, int cap)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge) {
		dev_err(cm->dev, "can not find fuel gauge device\n");
		return -ENODEV;
	}

	val.intval = cap;
	ret = power_supply_set_property(fuel_gauge, POWER_SUPPLY_PROP_CAPACITY, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		dev_err(cm->dev, "failed to save current battery capacity\n");

	return ret;
}
/**
 * get_charger_voltage - Get the charging voltage from fgu
 * @cm: the Charger Manager representing the battery.
 * @cur: the charging input voltage returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_charger_voltage(struct charger_manager *cm, int *vol)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret = -ENODEV;

	if (!is_ext_pwr_online(cm))
		return 0;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge) {
		dev_err(cm->dev, "Cannot find power supply  %s\n",
			cm->desc->psy_fuel_gauge);
		return	ret;
	}

	ret = power_supply_get_property(fuel_gauge,
					POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, &val);
	power_supply_put(fuel_gauge);
	if (ret == 0)
		*vol = val.intval;

	return ret;
}

/**
 * adjust_fuel_cap - Adjust the fuel cap level
 * @cm: the Charger Manager representing the battery.
 * @cap: the adjust fuel cap level.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int adjust_fuel_cap(struct charger_manager *cm, int cap)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	val.intval = cap;
	ret = power_supply_set_property(fuel_gauge,
					POWER_SUPPLY_PROP_CALIBRATE, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		dev_err(cm->dev, "failed to adjust fuel cap\n");

	return ret;
}

/**
 * get_constant_charge_current - Get the charging current from charging ic
 * @cm: the Charger Manager representing the battery.
 * @cur: the charging current returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_constant_charge_current(struct charger_manager *cm, int *cur)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int i, ret = -ENODEV;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy,
						POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, &val);
		power_supply_put(psy);
		if (ret == 0) {
			*cur += val.intval;
		}
	}

	return ret;
}

/**
 * get_input_current_limit - Get the input current limit from charging ic
 * @cm: the Charger Manager representing the battery.
 * @cur: the charging input limit current returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_input_current_limit(struct charger_manager *cm, int *cur)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int i, ret = -ENODEV;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy,
						POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
						&val);
		power_supply_put(psy);
		if (ret == 0)
			*cur += val.intval;
	}

	return ret;
}

static int get_charger_input_current(struct charger_manager *cm, int *cur)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int i, ret = -ENODEV;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		val.intval = CM_IBUS_CURRENT_NOW_CMD;
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
		power_supply_put(psy);
		if (ret == 0)
			*cur += val.intval;
	}

	return ret;
}

static bool cm_reset_basp_parameters(struct charger_manager *cm, int volt_uv)
{
	struct sprd_battery_jeita_table *table;
	int i, j, size;
	bool is_need_update = false;

	if (cm->desc->constant_charge_voltage_max_uv == volt_uv) {
		dev_warn(cm->dev, "BASP does not reset: volt_uv == constant charge voltage\n");
		return is_need_update;
	}

	cm->desc->ir_comp.us = volt_uv;
	cm->desc->cp.cp_target_vbat = volt_uv;
	cm->desc->constant_charge_voltage_max_uv = volt_uv;
	cm->desc->fullbatt_uV = volt_uv - cm->desc->fullbatt_voltage_offset_uv;

	for (i = SPRD_BATTERY_JEITA_DCP; i < SPRD_BATTERY_JEITA_MAX; i++) {
		table = cm->desc->jeita_tab_array[i];
		size = cm->desc->jeita_size[i];

		if (!table || !size)
			continue;

		for (j = 0; j < size; j++) {
			if (table[j].term_volt > volt_uv) {
				is_need_update = true;
				dev_info(cm->dev, "%s, set table[%d] from %d to %d\n",
					 sprd_battery_jeita_type_names[i], j,
					 table[j].term_volt, volt_uv);
				table[j].term_volt = volt_uv;
			}
		}
	}

	return is_need_update;
}

static int cm_set_basp_max_volt(struct charger_manager *cm, int max_volt_uv)
{
	struct power_supply *fuel_gauge = NULL;
	union power_supply_propval val;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge) {
		ret = -ENODEV;
		return ret;
	}

	val.intval = max_volt_uv;
	ret = power_supply_set_property(fuel_gauge, POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN, &val);
	power_supply_put(fuel_gauge);

	if (ret)
		dev_err(cm->dev, "failed to set basp max voltage, ret = %d\n", ret);

	return ret;
}

static int cm_get_basp_max_volt(struct charger_manager *cm, int *max_volt_uv)
{
	struct power_supply *fuel_gauge = NULL;
	union power_supply_propval val;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge) {
		dev_err(cm->dev, "%s: Fail to get fuel_gauge\n", __func__);
		ret = -ENODEV;
		return ret;
	}

	*max_volt_uv = 0;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN, &val);
	if (ret) {
		dev_err(cm->dev, "Fail to get voltage max design, ret = %d\n", ret);
		return ret;
	}

	power_supply_put(fuel_gauge);
	*max_volt_uv = val.intval;

	return ret;
}

static bool cm_init_basp_parameter(struct charger_manager *cm)
{
	int ret;
	int max_volt_uv;

	ret = cm_get_basp_max_volt(cm, &max_volt_uv);
	if (ret)
		return false;

	if (max_volt_uv == 0 || max_volt_uv == -1)
		return false;

	return cm_reset_basp_parameters(cm, max_volt_uv);
}

static void cm_power_path_enable(struct charger_manager *cm, int cmd)
{
	int ret, i;
	union power_supply_propval val = {0,};
	struct power_supply *psy;

	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find primary power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		val.intval = cmd;
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
		power_supply_put(psy);
		if (ret) {
			dev_err(cm->dev, "Fail to set power_path[%d] of %s, ret = %d\n",
				cmd, cm->desc->psy_charger_stat[i], ret);
			continue;
		}
	}
}

static bool cm_is_power_path_enabled(struct charger_manager *cm)
{
	int ret, i;
	bool enabled = false;
	union power_supply_propval val = {0,};
	struct power_supply *psy;

	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find primary power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		val.intval = CM_POWER_PATH_ENABLE_CMD;
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
		power_supply_put(psy);
		if (!ret) {
			if (val.intval) {
				enabled = true;
				break;
			}
		}
	}

	dev_info(cm->dev, "%s: %s\n", __func__, enabled ? "enabled" : "disabled");
	return enabled;
}

/**
 * is_charging - Returns true if the battery is being charged.
 * @cm: the Charger Manager representing the battery.
 */
static bool is_charging(struct charger_manager *cm)
{
	bool charging = false, wl_online = false;
	struct power_supply *psy;
	union power_supply_propval val;
	int i, ret;

	/* If there is no battery, it cannot be charged */
	if (!is_batt_present(cm) || (bat_ovp_flag == true))
		return false;

	if (is_ext_wl_pwr_online(cm))
		wl_online = true;

	/* If at least one of the charger is charging, return yes */
	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		/* 1. The charger sholuld not be DISABLED */
		if (cm->emergency_stop)
			continue;
		if (!cm->charger_enabled)
			continue;

		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
			continue;
		}

		/* 2. The charger should be online (ext-power) */
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret) {
			dev_warn(cm->dev, "Cannot read ONLINE value from %s\n",
				 cm->desc->psy_charger_stat[i]);
			power_supply_put(psy);
			continue;
		}

		if (val.intval == 0 && !wl_online) {
			power_supply_put(psy);
			continue;
		}

		/*
		 * 3. The charger should not be FULL, DISCHARGING,
		 * or NOT_CHARGING.
		 */
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS,
				&val);
		power_supply_put(psy);
		if (ret) {
			dev_warn(cm->dev, "Cannot read STATUS value from %s\n",
				 cm->desc->psy_charger_stat[i]);
			continue;
		}
		if (val.intval == POWER_SUPPLY_STATUS_FULL ||
		    val.intval == POWER_SUPPLY_STATUS_DISCHARGING ||
		    val.intval == POWER_SUPPLY_STATUS_NOT_CHARGING)
			continue;

		/* Then, this is charging. */
		charging = true;
		break;
	}

	return charging;
}

static bool cm_primary_charger_enable(struct charger_manager *cm, bool enable)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int ret;

	if (!cm->desc->psy_charger_stat || !cm->desc->psy_charger_stat[0])
		return false;

	psy = power_supply_get_by_name(cm->desc->psy_charger_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			cm->desc->psy_charger_stat[0]);
		return false;
	}

	val.intval = enable;
	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_CALIBRATE, &val);
	power_supply_put(psy);
	if (ret) {
		dev_err(cm->dev, "failed to %s primary charger, ret = %d\n",
			enable ? "enable" : "disable", ret);
		return false;
	}

	return true;
}
//#define FIX_FULL_BAT_SOC_UA 10000
//#define FIX_FULL_BAT_SOC_MIN 990
/**
 * is_full_charged - Returns true if the battery is fully charged.
 * @cm: the Charger Manager representing the battery.
 */
static bool is_full_charged(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	bool is_full = false;
	int ret = 0;
	int uV, uA;
	int cur_jeita_status;

	/* If there is no battery, it cannot be charged */
	if (!is_batt_present(cm))
		return false;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return false;

	if (desc->fullbatt_full_capacity > 0) {
		val.intval = 0;

		/* Not full if capacity of fuel gauge isn't full */
		ret = power_supply_get_property(fuel_gauge,
						POWER_SUPPLY_PROP_CHARGE_FULL, &val);
		if (!ret && val.intval > desc->fullbatt_full_capacity) {
			is_full = true;
			goto out;
		}
	}

	cur_jeita_status = cm_manager_get_jeita_status(cm, desc->temperature);

	/* Full, if it's over the fullbatt voltage */
	if (desc->fullbatt_uV > 0 && desc->fullbatt_uA > 0) {
		ret = get_vbat_now_uV(cm, &uV);
		if (ret)
			goto out;

		ret = get_ibat_now_uA(cm, &uA);
		if (ret)
			goto out;

		/* Battery is already full, checks voltage drop. */
		if ((cm->battery_status == POWER_SUPPLY_STATUS_FULL || is_full_warm == true)&& desc->fullbatt_vchkdrop_uV) {
			int batt_ocv;

			ret = get_batt_ocv(cm, &batt_ocv);
			if (ret)
				goto out;

			if (uV > (cm->desc->fullbatt_uV - cm->desc->fullbatt_vchkdrop_uV)){
			//dev_info(cm->dev, "VBATT %duV after full-batt:%d\n", uV,desc->fullbatt_vchkdrop_uV);
				if(cm->battery_status == POWER_SUPPLY_STATUS_FULL){
					is_full = true;
				}
				if(is_full_warm){
					is_full_warm = true;
				}
			}else{
				is_full_warm = false;
			}
			goto out;
		}

		if (desc->first_fullbatt_uA > 0 && uV >= desc->fullbatt_uV &&
		    uA > desc->fullbatt_uA && uA <= desc->first_fullbatt_uA && uA >= 0) {
			if (++desc->first_trigger_cnt > 1)
				cm->desc->force_set_full = true;
		} else {
			desc->first_trigger_cnt = 0;
		}
#if 0
		dev_err(cm->dev, "%s vbat_now_uV = %d, ibat_now_uA %d, trigger_cnt %d\n", __func__, uV, uA, desc->trigger_cnt);
		if (uV >= desc->fullbatt_uV && uA <= (desc->fullbatt_uA + FIX_FULL_BAT_SOC_UA) && uA >= 0 && cm->desc->cap > FIX_FULL_BAT_SOC_MIN) {
			dev_err(cm->dev, "normal full,fix soc to FULL\n");
			adjust_fuel_cap(cm, CM_FORCE_SET_FUEL_CAP_FULL);
		}else if(cur_jeita_status == CM_HIGH_TEMP_STATUS && uV >= CM_HIGH_TEMP_STATUS_CV && uA <= (desc->fullbatt_uA + FIX_FULL_BAT_SOC_UA) && uA >= 0){
			dev_err(cm->dev, "warm full,fix soc to FULL\n");
			adjust_fuel_cap(cm, CM_FORCE_SET_FUEL_CAP_FULL);
		}
#endif
		if (uV >= desc->fullbatt_uV && uA <= desc->fullbatt_uA && uA >= 0) {
			if (++desc->trigger_cnt > 1) {
				dev_info(cm->dev,"%s up full,uv:%d, ua:%d, cap:%d, cnt:%d\n",__func__,uV,uA,cm->desc->cap,desc->trigger_cnt);
				if (cm->desc->cap >= CM_CAP_FULL_PERCENT) {
					if (desc->trigger_cnt >= 2){
						dev_info(cm->dev,"%s isfull,uv:%d, ua:%d, cap:%d cnt:%d\n",__func__,uV,uA,cm->desc->cap,desc->trigger_cnt);
						adjust_fuel_cap(cm, CM_FORCE_SET_FUEL_CAP_FULL);
						is_full = true;
						}
				} else {
					is_full = false;
					adjust_fuel_cap(cm, CM_FORCE_SET_FUEL_CAP_FULL);
					if (desc->trigger_cnt == 2)
						cm_primary_charger_enable(cm, false);
				}
				cm->desc->force_set_full = true;
			} else {
				is_full = false;
			}
			goto out;
		}else if(cur_jeita_status == CM_HIGH_TEMP_STATUS && uV >= CM_HIGH_TEMP_STATUS_CV && uA <= desc->fullbatt_uA && uA >= 0){
			charger_full_count++;
			dev_info(cm->dev,"%s high temp up full,uv:%d, ua:%d, cap:%d, cnt:%d\n",__func__,uV,uA,cm->desc->cap,charger_full_count);
			if(charger_full_count > 3){
				is_full_warm = true;
				charger_full_count = 0;
			}
			goto out;
		} else if(uV >= (desc->fullbatt_uV + HW_BAT_FULL_VOLT)){
			hw_full_counts++;
			if(hw_full_counts > 3){
					hw_full_counts = 0;
					adjust_fuel_cap(cm, CM_FORCE_SET_FUEL_CAP_FULL);
					cm->desc->force_set_full = true;
					is_full = true;
					dev_info(cm->dev,"%s ,hw charge full uV:%d fullbatt_uV:%d uA:%d fullbatt_uA:%d",__func__,uV,desc->fullbatt_uV,uA,desc->fullbatt_uA);
			}
			goto out;
		}else {
			//dev_info(cm->dev,"charge full reset");
			is_full_warm = false;
			hw_full_counts = 0;
			charger_full_count = 0;
			is_full = false;
			desc->trigger_cnt = 0;
			goto out;
		}
	}

	/* Full, if the capacity is more than fullbatt_soc */
	if (desc->fullbatt_soc > 0) {
		val.intval = 0;

		ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CAPACITY, &val);
		if (!ret && val.intval >= desc->fullbatt_soc) {
			is_full = true;
			goto out;
		}
	}

out:
	//dev_err(cm->dev, "%s is_full = %d, force_set_full %d, is_full_warm %d\n", __func__, is_full, cm->desc->force_set_full, is_full_warm);
	power_supply_put(fuel_gauge);
	return is_full;
}

/**
 * is_polling_required - Return true if need to continue polling for this CM.
 * @cm: the Charger Manager representing the battery.
 */
static bool is_polling_required(struct charger_manager *cm)
{
	switch (cm->desc->polling_mode) {
	case CM_POLL_DISABLE:
		return false;
	case CM_POLL_ALWAYS:
		return true;
	case CM_POLL_EXTERNAL_POWER_ONLY:
		return is_ext_pwr_online(cm);
	case CM_POLL_CHARGING_ONLY:
		return is_charging(cm);
	default:
		dev_warn(cm->dev, "Incorrect polling_mode (%d)\n",
			 cm->desc->polling_mode);
	}

	return false;
}

static void cm_update_current_jeita_status(struct charger_manager *cm)
{
	int cur_jeita_status;

	/**
	 * Note that it need to vote for ibat before the caller of this function
	 * if does not define jeita table
	 */
	if (cm->desc->jeita_tab_size && !cm->charging_status) {
		cur_jeita_status = cm_manager_get_jeita_status(cm, cm->desc->temperature);
		if (cm->desc->jeita_disabled)
			cur_jeita_status = cm->desc->force_jeita_status;
		cm_manager_adjust_current(cm, cur_jeita_status);
	}
}

static void cm_update_charge_info(struct charger_manager *cm, int cmd)
{
	struct charger_desc *desc = cm->desc;
	struct cm_thermal_info *thm_info = &cm->desc->thm_info;

	mutex_lock(&cm->desc->charge_info_mtx);
	switch (desc->charger_type) {
	case POWER_SUPPLY_USB_CHARGER_TYPE_DCP:
		desc->charge_limit_cur = desc->cur.dcp_cur;
		desc->input_limit_cur = desc->cur.dcp_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_size[SPRD_BATTERY_JEITA_DCP]) {
			desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_DCP];
			desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_DCP];
		}
		if (desc->normal_charge_voltage_max)
			desc->charge_voltage_max = desc->normal_charge_voltage_max;
		if (desc->normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->normal_charge_voltage_drop;
		break;
	case POWER_SUPPLY_USB_CHARGER_TYPE_SDP:
		desc->charge_limit_cur = desc->cur.sdp_cur;
		desc->input_limit_cur = desc->cur.sdp_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_size[SPRD_BATTERY_JEITA_SDP]) {
			desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_SDP];
			desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_SDP];
		}
		if (desc->normal_charge_voltage_max)
			desc->charge_voltage_max = desc->normal_charge_voltage_max;
		if (desc->normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->normal_charge_voltage_drop;
		break;
	case POWER_SUPPLY_USB_CHARGER_TYPE_CDP:
		desc->charge_limit_cur = desc->cur.cdp_cur;
		desc->input_limit_cur = desc->cur.cdp_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_size[SPRD_BATTERY_JEITA_CDP]) {
			desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_CDP];
			desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_CDP];
		}
		if (desc->normal_charge_voltage_max)
			desc->charge_voltage_max = desc->normal_charge_voltage_max;
		if (desc->normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->normal_charge_voltage_drop;
		break;
	case POWER_SUPPLY_USB_CHARGER_TYPE_PD:
	case POWER_SUPPLY_USB_CHARGER_TYPE_SFCP_1P0:
		if (desc->enable_fast_charge) {
			desc->charge_limit_cur = desc->cur.fchg_cur;
			desc->input_limit_cur = desc->cur.fchg_limit;
			thm_info->adapter_default_charge_vol = 9;
			if (desc->jeita_size[SPRD_BATTERY_JEITA_FCHG]) {
				desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_FCHG];
				desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_FCHG];
			}
			if (desc->fast_charge_voltage_max)
				desc->charge_voltage_max = desc->fast_charge_voltage_max;
			if (desc->fast_charge_voltage_drop)
				desc->charge_voltage_drop = desc->fast_charge_voltage_drop;
			break;
		}
		desc->charge_limit_cur = desc->cur.dcp_cur;
		desc->input_limit_cur = desc->cur.dcp_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_size[SPRD_BATTERY_JEITA_DCP]) {
			desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_DCP];
			desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_DCP];
		}
		if (desc->normal_charge_voltage_max)
			desc->charge_voltage_max = desc->normal_charge_voltage_max;
		if (desc->normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->normal_charge_voltage_drop;
		break;
/*
	case POWER_SUPPLY_USB_CHARGER_TYPE_PD_PPS:
	case POWER_SUPPLY_USB_CHARGER_TYPE_SFCP_2P0:
		if (desc->cp.cp_running && !desc->cp.recovery) {
			desc->charge_limit_cur = desc->cur.flash_cur;
			desc->input_limit_cur = desc->cur.flash_limit;
			thm_info->adapter_default_charge_vol = 11;
			if (desc->jeita_size[SPRD_BATTERY_JEITA_FLASH]) {
				desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_FLASH];
				desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_FLASH];
			}
			if (desc->flash_charge_voltage_max)
				desc->charge_voltage_max = desc->flash_charge_voltage_max;
			if (desc->flash_charge_voltage_drop)
				desc->charge_voltage_drop = desc->flash_charge_voltage_drop;
			break;
		}
		desc->charge_limit_cur = desc->cur.dcp_cur;
		desc->input_limit_cur = desc->cur.dcp_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_size[SPRD_BATTERY_JEITA_DCP]) {
			desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_DCP];
			desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_DCP];
		}
		if (desc->normal_charge_voltage_max)
			desc->charge_voltage_max = desc->normal_charge_voltage_max;
		if (desc->normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->normal_charge_voltage_drop;
		break;
	case POWER_SUPPLY_WIRELESS_CHARGER_TYPE_BPP:
		desc->charge_limit_cur = desc->cur.wl_bpp_cur;
		desc->input_limit_cur = desc->cur.wl_bpp_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_size[SPRD_BATTERY_JEITA_WL_BPP]) {
			desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_WL_BPP];
			desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_WL_BPP];
		}
		if (desc->wireless_normal_charge_voltage_max)
			desc->charge_voltage_max = desc->wireless_normal_charge_voltage_max;
		if (desc->wireless_normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->wireless_normal_charge_voltage_drop;
		break;
	case POWER_SUPPLY_WIRELESS_CHARGER_TYPE_EPP:
		desc->charge_limit_cur = desc->cur.wl_epp_cur;
		desc->input_limit_cur = desc->cur.wl_epp_limit;
		thm_info->adapter_default_charge_vol = 11;
		if (desc->jeita_size[SPRD_BATTERY_JEITA_WL_EPP]) {
			desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_WL_EPP];
			desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_WL_EPP];
		}
		if (desc->wireless_fast_charge_voltage_max)
			desc->charge_voltage_max = desc->wireless_fast_charge_voltage_max;
		if (desc->wireless_fast_charge_voltage_drop)
			desc->charge_voltage_drop = desc->wireless_fast_charge_voltage_drop;
		break;
*/
	default:
		desc->charge_limit_cur = desc->cur.unknown_cur;
		desc->input_limit_cur = desc->cur.unknown_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_size[SPRD_BATTERY_JEITA_UNKNOWN]) {
			desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_UNKNOWN];
			desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_UNKNOWN];
		}
		if (desc->normal_charge_voltage_max)
			desc->charge_voltage_max = desc->normal_charge_voltage_max;
		if (desc->normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->normal_charge_voltage_drop;
		break;
	}

	mutex_unlock(&cm->desc->charge_info_mtx);

	if (thm_info->thm_pwr && thm_info->adapter_default_charge_vol)
		thm_info->thm_adjust_cur = (int)(thm_info->thm_pwr /
			thm_info->adapter_default_charge_vol) * 1000;

	dev_info(cm->dev, "%s, chgr type = %d, fchg_en = %d, cp_running = %d, cp_recovery = %d"
		 " max chg_lmt_cur = %duA, max inpt_lmt_cur = %duA, max chg_volt = %duV,"
		 " chg_volt_drop = %d, adapter_chg_volt = %dmV, thm_cur = %d, chg_info_cmd = 0x%x,"
		 " jeita_size = %d\n",
		 __func__, desc->charger_type, desc->enable_fast_charge, desc->cp.cp_running,
		 desc->cp.recovery, desc->charge_limit_cur, desc->input_limit_cur,
		 desc->charge_voltage_max, desc->charge_voltage_drop,
		 thm_info->adapter_default_charge_vol * 1000, thm_info->thm_adjust_cur, cmd,
		 desc->jeita_tab_size);

	if (!cm->cm_charge_vote || !cm->cm_charge_vote->vote) {
		dev_err(cm->dev, "%s: cm_charge_vote is null\n", __func__);
		return;
	}

	if (cmd & CM_CHARGE_INFO_CHARGE_LIMIT)
		cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBAT,
					 SPRD_VOTE_TYPE_IBAT_ID_CHARGER_TYPE,
					 SPRD_VOTE_CMD_MIN, desc->charge_limit_cur, cm);
	if (cmd & CM_CHARGE_INFO_INPUT_LIMIT)
		cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBUS,
					 SPRD_VOTE_TYPE_IBUS_ID_CHARGER_TYPE,
					 SPRD_VOTE_CMD_MIN, desc->input_limit_cur, cm);
	if (cmd & CM_CHARGE_INFO_THERMAL_LIMIT && thm_info->thm_adjust_cur > 0) {
		/* The ChargerIC with linear charging cannot set Ibus, only Ibat. */
		if (cm->desc->thm_info.need_calib_charge_lmt)
			cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBAT,
					 SPRD_VOTE_TYPE_IBAT_ID_CHARGE_CONTROL_LIMIT,
					 SPRD_VOTE_CMD_MIN,
					 cm->desc->thm_info.thm_adjust_cur, cm);
		else
			cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
						 SPRD_VOTE_TYPE_IBUS,
						 SPRD_VOTE_TYPE_IBUS_ID_CHARGE_CONTROL_LIMIT,
						 SPRD_VOTE_CMD_MIN,
						 cm->desc->thm_info.thm_adjust_cur, cm);
	}
	if (cmd & CM_CHARGE_INFO_JEITA_LIMIT)
		cm_update_current_jeita_status(cm);
}

#ifndef VENDOR_KERNEL
static void cm_vote_property_unparadll(struct charger_manager *cm, int target_cur,
				       const char **name, enum power_supply_property psp)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int i, ret;
	int cur[2];

	if (!name) {
		dev_err(cm->dev, "psy name is null!!!\n");
		return;
	}

	cur[0] = target_cur*6/10;
	cur[1] = target_cur*4/10;

	dev_info(cm->dev, "target_cur=%d, cur[0]=%d, cur[1]=%d\n",
		 target_cur, cur[0], cur[1]);
	for (i = 0; name[i]; i++) {
		psy = power_supply_get_by_name(name[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				name[i]);
			continue;
		}
		val.intval = cur[i];
		ret = power_supply_set_property(psy, psp, &val);
		power_supply_put(psy);
		if (ret) {
			dev_err(cm->dev, "failed to %s set power_supply_property[%d], ret = %d\n",
				name[i], ret, psp);
			continue;
		}
	}
}
#endif //not defined VENDOR_KERNEL

static void cm_vote_property(struct charger_manager *cm, int target_val,
			     const char **name, enum power_supply_property psp)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int i, ret;

	if (!name) {
		dev_err(cm->dev, "psy name is null!!!\n");
		return;
	}

	for (i = 0; name[i]; i++) {
		psy = power_supply_get_by_name(name[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n", name[i]);
			continue;
		}

		val.intval = target_val;
		ret = power_supply_set_property(psy, psp, &val);
		power_supply_put(psy);
		if (ret)
			dev_err(cm->dev, "failed to %s set power_supply_property[%d], ret = %d\n",
				name[i], psp, ret);
	}
}

#ifdef VENDOR_KERNEL // VENDOR_KERNEL：for beidou_T
static int cm_check_parallel_charger(struct charger_manager *cm, int cur)
{
	if (cm->desc->enable_fast_charge && cm->desc->psy_charger_stat[1])
		cur /= 2;

	return cur;
}
#endif //defined VENDOR_KERNEL

static void cm_sprd_vote_callback(struct sprd_vote *vote_gov, int vote_type,
				  int value, void *data)
{
	struct charger_manager *cm = (struct charger_manager *)data;
	const char **psy_charger_name;

	dev_info(cm->dev, "%s, %s[%d]\n", __func__, vote_type_names[vote_type], value);
	switch (vote_type) {
	case SPRD_VOTE_TYPE_IBAT:
		psy_charger_name = cm->desc->psy_charger_stat;
#ifndef VENDOR_KERNEL
		if (cm->desc->enable_fast_charge) {
			cm_vote_property_unparadll(cm, value, psy_charger_name,
						   POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT);
		} else 
#endif //not defined VENDOR_KERNEL
		{
#ifdef VENDOR_KERNEL // VENDOR_KERNEL：for beidou_T
			value = cm_check_parallel_charger(cm, value);
#endif //defined VENDOR_KERNEL
			cm_vote_property(cm, value, psy_charger_name,
					 POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT);
		}
		break;
	case SPRD_VOTE_TYPE_IBUS:
		psy_charger_name = cm->desc->psy_charger_stat;
#ifndef VENDOR_KERNEL
		if (cm->desc->enable_fast_charge) {
			cm_vote_property_unparadll(cm, value, psy_charger_name,
						   POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT);
		} else
#endif //not defined VENDOR_KERNEL
		{
#ifdef VENDOR_KERNEL // VENDOR_KERNEL：for beidou_T
			value = cm_check_parallel_charger(cm, value);
#endif //defined VENDOR_KERNEL
			cm_vote_property(cm, value, psy_charger_name,
					 POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT);
		}
		break;
	case SPRD_VOTE_TYPE_CCCV:
		psy_charger_name = cm->desc->psy_charger_stat;
		if (cm->desc->cp.cp_running)
			psy_charger_name = cm->desc->psy_cp_stat;
		cm_vote_property(cm, value, psy_charger_name,
				 POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX);
		break;
	default:
		dev_err(cm->dev, "vote_gov: vote_type[%d] error!!!\n", vote_type);
		break;
	}
}

static int cm_get_adapter_max_voltage(struct charger_manager *cm, int *max_vol)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	*max_vol = 0;
	psy = power_supply_get_by_name(desc->psy_fast_charger_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			desc->psy_fast_charger_stat[0]);
		return -ENODEV;
	}

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
	power_supply_put(psy);
	if (ret) {
		dev_err(cm->dev,
			"failed to get max voltage\n");
		return ret;
	}

	*max_vol = val.intval;

	return 0;
}

static int cm_get_adapter_max_current(struct charger_manager *cm, int *max_cur)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	*max_cur = 0;
	psy = power_supply_get_by_name(desc->psy_fast_charger_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			desc->psy_fast_charger_stat[0]);
		return -ENODEV;
	}

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_MAX, &val);
	power_supply_put(psy);
	if (ret) {
		dev_err(cm->dev,
			"failed to get max current\n");
		return ret;
	}

	*max_cur = val.intval;

	return 0;
}

static int cm_set_charger_ovp(struct charger_manager *cm, int cmd)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret, i;

	if (!desc->psy_charger_stat) {
		dev_err(cm->dev, "psy_charger_stat is null!!!\n");
		return -ENODEV;
	}

	/*
	 * make the psy_charger_stat[0] to be main charger,
	 * set the main charger charge current and limit current
	 * in 9V/5V fast charge status.
	 */
	for (i = 0; desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_charger_stat[i]);
			return -ENODEV;
		}

		val.intval = cmd;
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
		power_supply_put(psy);
		if (ret) {
			dev_err(cm->dev, "failed to set \"%s\" ovp cmd = %d, ret = %d\n",
				desc->psy_charger_stat[i], cmd, ret);
			return ret;
		}
	}

	return 0;
}

static int cm_enable_second_charger(struct charger_manager *cm, bool enable)
{

	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	if (!desc->psy_charger_stat[1])
		return 0;

	psy = power_supply_get_by_name(desc->psy_charger_stat[1]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			desc->psy_charger_stat[1]);
		power_supply_put(psy);
		return -ENODEV;
	}

	/*
	 * enable/disable the second charger to start/stop charge
	 */
	val.intval = enable;
	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
	power_supply_put(psy);
	if (ret) {
		dev_err(cm->dev,
			"failed to %s second charger \n", enable ? "enable" : "disable");
		return ret;
	}

	return 0;
}

static int cm_adjust_fast_charge_voltage(struct charger_manager *cm, int vol)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(desc->psy_fast_charger_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			desc->psy_fast_charger_stat[0]);
		return -ENODEV;
	}

	val.intval = vol;
	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
	power_supply_put(psy);
	if (ret) {
		dev_err(cm->dev,
			"failed to adjust fast charger voltage vol = %d\n", vol);
		return ret;
	}

	return 0;
}

static bool cm_is_reach_fchg_threshold(struct charger_manager *cm)
{
	int batt_ocv, batt_uA, fchg_ocv_threshold, thm_cur;
	int cur_jeita_status, target_cur;
#ifndef VENDOR_KERNEL
	int batt_uV = 0, cur_temp = 0, fast_charge_enable_count = 0;
#endif //not defined VENDOR_KERNEL

	if (get_batt_ocv(cm, &batt_ocv)) {
		dev_err(cm->dev, "get_batt_ocv error.\n");
		return false;
	}

	if (get_ibat_now_uA(cm, &batt_uA)) {
		dev_err(cm->dev, "get_ibat_now_uA error.\n");
		return false;
	}

#ifndef VENDOR_KERNEL
	if (get_vbat_now_uV(cm, &batt_uV)) {
		dev_err(cm->dev, "get_vbat_now_uV error\n");
		return false;
	}

	if (cm_get_battery_temperature_by_psy(cm, &cur_temp)) {
		dev_err(cm->dev, "get_cur_temp error.\n");
		return false;
	}
#endif //not defined VENDOR_KERNEL

	target_cur = batt_uA;
	if (cm->desc->jeita_tab_size) {
		cur_jeita_status = cm_manager_get_jeita_status(cm, cm->desc->temperature);
		if (cm->desc->jeita_disabled)
			cur_jeita_status = cm->desc->force_jeita_status;

		target_cur = 0;
		if (cur_jeita_status != cm->desc->jeita_tab_size)
			target_cur = cm->desc->jeita_tab[cur_jeita_status].current_ua;
	}

	fchg_ocv_threshold = CM_FAST_CHARGE_START_VOLTAGE_HTHRESHOLD;
	if (cm->desc->fchg_ocv_threshold > 0)
		fchg_ocv_threshold = cm->desc->fchg_ocv_threshold;

	thm_cur = CM_FAST_CHARGE_ENABLE_THERMAL_CURRENT;
	if (cm->desc->thm_info.thm_adjust_cur > 0)
		thm_cur = cm->desc->thm_info.thm_adjust_cur;

#ifdef VENDOR_KERNEL
	if (target_cur >= CM_FAST_CHARGE_ENABLE_CURRENT &&
	    thm_cur >= CM_FAST_CHARGE_ENABLE_THERMAL_CURRENT &&
	    batt_ocv >= CM_FAST_CHARGE_START_VOLTAGE_LTHRESHOLD &&
	    batt_ocv < fchg_ocv_threshold)
		return true;
	else if (batt_ocv >= CM_FAST_CHARGE_START_VOLTAGE_LTHRESHOLD &&
		 batt_uA >= CM_FAST_CHARGE_ENABLE_CURRENT)
		return true;

	return false;
#else //not defined VENDOR_KERNEL
	fast_charge_enable_count = 0;

	/* Ibat > 1A */
	if (batt_uA  > CM_FAST_CHARGE_ENABLE_CURRENT)
		fast_charge_enable_count++;

	/* 3.5v < Vbat < 4.55v */
	if (batt_uV < CM_FAST_CHARGE_ENABLE_BATTERY_VOLTAGE_MAX &&
			batt_uV > CM_FAST_CHARGE_ENABLE_BATTERY_VOLTAGE_MIN)
		fast_charge_enable_count++;

	/* 0 <= bat_temp < 42 */
	if (cur_temp >= CM_FAST_CHARGE_ENABLE_TEMP_MIN &&
			cur_temp < CM_FAST_CHARGE_ENABLE_TEMP_MAX)
		fast_charge_enable_count++;

	/* cap < 90% */
	if (cm->desc->cap < CM_FAST_CHARGE_ENABLE_CAP && batt_uV <= CM_CC_CV_VOLTAGE)
		fast_charge_enable_count++;

#ifdef CONFIG_HQ_USB_TEMP_CHECK
	if (fast_charge_enable_count < CM_FAST_CHARGE_ENABLE_COUNT || (g_factory_mode == 0 && g_cool_down == 1)
				|| g_cc_vbus_status)
		return false;

	dev_info(cm->dev, "batt_uA = %d, batt_uV = %d, cur_temp = %d, cap = %d, cool_down = %d g_cc_vbus_status = %d\n",
			batt_uA, batt_uV, cur_temp, cm->desc->cap, g_cool_down,g_cc_vbus_status);
#else
        if (fast_charge_enable_count < CM_FAST_CHARGE_ENABLE_COUNT || (g_factory_mode == 0 && g_cool_down == 1))
                return false;

        dev_info(cm->dev, "batt_uA = %d, batt_uV = %d, cur_temp = %d, cap = %d, cool_down = %d\n",
                        batt_uA, batt_uV, cur_temp, cm->desc->cap, g_cool_down);
#endif
	return true;
#endif //defined VENDOR_KERNEL
}

static int cm_fixed_fchg_enable(struct charger_manager *cm)
{
	int ret, adapter_max_vbus;

	/*
	 * if it occurs emergency event, don't enable fast charge.
	 */
	if (cm->emergency_stop)
		return -EAGAIN;

	if (!cm->desc) {
		dev_err(cm->dev, "cm->desc is a null pointer!!!\n");
		return 0;
	}

	/*
	 * if it don't define cm-fast-chargers in dts,
	 * we think that it don't plan to use fast charge.
	 */
	if (!cm->desc->psy_fast_charger_stat || !cm->desc->psy_fast_charger_stat[0])
		return 0;

	if (!cm->desc->is_fast_charge || cm->desc->enable_fast_charge)
		return 0;

	if (!cm->cm_charge_vote || !cm->cm_charge_vote->vote) {
		dev_err(cm->dev, "%s: cm_charge_vote is null\n", __func__);
		return 0;
	}

	/*
	 * cm->desc->enable_fast_charge should be set to true when the transient
	 * current is voting, otherwise the current of the parallel charging
	 * scheme cannot be halved.
	 *
	 * In the normal fast charge voltage regulation process, add the normal
	 * fast charge transition current to prevent overload and other abnormal
	 * situations.
	 */
	cm->desc->enable_fast_charge = true;
	cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
				 SPRD_VOTE_TYPE_IBUS,
				 SPRD_VOTE_TYPE_IBUS_ID_FCHG_FIXED_TRANSITION,
				 SPRD_VOTE_CMD_MIN, CM_FAST_CHARGE_TRANSITION_CURRENT_1P5A, cm);

	pr_err("cm_fixed_fchg_enable fastcharger true\n");
	cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
				   CM_CHARGE_INFO_INPUT_LIMIT |
				   CM_CHARGE_INFO_THERMAL_LIMIT |
				   CM_CHARGE_INFO_JEITA_LIMIT));

	/*
	 * adjust over voltage protection in 9V
	 */
	ret = cm_set_charger_ovp(cm, CM_FAST_CHARGE_OVP_ENABLE_CMD);
	if (ret) {
		dev_err(cm->dev, "failed to enable fchg ovp\n");
		/*
		 * if it failed to set fast charge ovp, reset to DCP setting
		 * first so that the charging ovp can reach the condition again.
		 */
		goto tran_cur_err;
	}

	/*
	 * adjust fast charger output voltage from 5V to 9V
	 */
	ret = cm_get_adapter_max_voltage(cm, &adapter_max_vbus);
	if (ret) {
		dev_err(cm->dev, "failed to obtain the adapter max voltage\n");
		goto ovp_err;
	}

	if (adapter_max_vbus >= CM_FAST_CHARGE_VOLTAGE_9V)
		adapter_max_vbus = CM_FAST_CHARGE_VOLTAGE_9V;

	ret = cm_adjust_fast_charge_voltage(cm, adapter_max_vbus);
	if (ret) {
		dev_err(cm->dev, "failed to adjust fast charger voltage\n");
		goto ovp_err;
	}

	ret = cm_enable_second_charger(cm, true);
	if (ret) {
		dev_err(cm->dev, "failed to enable second charger\n");
		goto adj_vol_err;
	}

	goto out;

adj_vol_err:
	cm_adjust_fast_charge_voltage(cm, CM_FAST_CHARGE_VOLTAGE_5V);

ovp_err:
	cm_set_charger_ovp(cm, CM_FAST_CHARGE_OVP_DISABLE_CMD);

tran_cur_err:
	cm->desc->enable_fast_charge = false;
	cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
				   CM_CHARGE_INFO_INPUT_LIMIT |
				   CM_CHARGE_INFO_THERMAL_LIMIT |
				   CM_CHARGE_INFO_JEITA_LIMIT));

out:
	cm->cm_charge_vote->vote(cm->cm_charge_vote, false,
				 SPRD_VOTE_TYPE_IBUS,
				 SPRD_VOTE_TYPE_IBUS_ID_FCHG_FIXED_TRANSITION,
				 SPRD_VOTE_CMD_MIN, CM_FAST_CHARGE_TRANSITION_CURRENT_1P5A, cm);
	return ret;
}

static int cm_fixed_fchg_disable(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	int ret, charge_vol;

	if (!desc->enable_fast_charge)
		return 0;

	if (!cm->cm_charge_vote || !cm->cm_charge_vote->vote) {
		dev_err(cm->dev, "%s: cm_charge_vote is null\n", __func__);
		return 0;
	}

	cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
				 SPRD_VOTE_TYPE_IBUS,
				 SPRD_VOTE_TYPE_IBUS_ID_FCHG_FIXED_TRANSITION,
				 SPRD_VOTE_CMD_MIN, CM_FAST_CHARGE_TRANSITION_CURRENT_1P5A, cm);

	/*
	 * If defined psy_charger_stat[1], then disable the second
	 * charger first.
	 */
	ret = cm_enable_second_charger(cm, false);
	if (ret) {
		dev_err(cm->dev, "failed to disable second charger\n");
		goto out;
	}

	/*
	 * Adjust fast charger output voltage from 9V to 5V
	 */
	if (!desc->wait_vbus_stable &&
	    cm_adjust_fast_charge_voltage(cm, CM_FAST_CHARGE_VOLTAGE_5V)) {
		dev_err(cm->dev, "%s, failed to adjust 5V fast charger voltage\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	/*
	 * Waiting for the charger to step down to prevent the occurrence
	 * of Vbus overvoltage.
	 * Reason: It takes a certain time for the charger to switch from
	 *         9V to 5V. At this time, if the OVP is directly set to
	 *         6.5V, there is a small probability that Vbus overvoltage
	 *         will occur.
	 */
	ret = get_charger_voltage(cm, &charge_vol);
	if (ret) {
		dev_err(cm->dev, "%s, fail to get charge vol\n", __func__);
		goto out;
	}

	if (charge_vol > desc->normal_charge_voltage_max) {
		dev_err(cm->dev, "%s, waiting for the charger to step down\n", __func__);
		desc->wait_vbus_stable = true;
		ret = -EINVAL;
		goto out;
	}

	desc->wait_vbus_stable = false;

	ret = cm_set_charger_ovp(cm, CM_FAST_CHARGE_OVP_DISABLE_CMD);
	if (ret) {
		dev_err(cm->dev, "%s, failed to disable fchg ovp\n", __func__);
		goto out;
	}

	desc->enable_fast_charge = false;
	/*
	 * Adjust over voltage protection in 5V
	 */
	cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
				   CM_CHARGE_INFO_INPUT_LIMIT |
				   CM_CHARGE_INFO_THERMAL_LIMIT |
				   CM_CHARGE_INFO_JEITA_LIMIT));

out:
	cm->cm_charge_vote->vote(cm->cm_charge_vote, false,
				 SPRD_VOTE_TYPE_IBUS,
				 SPRD_VOTE_TYPE_IBUS_ID_FCHG_FIXED_TRANSITION,
				 SPRD_VOTE_CMD_MIN, CM_FAST_CHARGE_TRANSITION_CURRENT_1P5A, cm);
	return ret;
}

static bool cm_is_disable_fixed_fchg_check(struct charger_manager *cm)
{
#ifdef VENDOR_KERNEL
	int batt_uV, batt_uA, ret;
#else //not defined VENDOR_KERNEL
	int batt_uV = 0, batt_uA = 0, ret = 0, cur_temp = 0, vbus_uV = 0;
#endif //defined VENDOR_KERNEL

	if (!cm->desc->enable_fast_charge)
		return true;

	ret = get_vbat_now_uV(cm, &batt_uV);
	if (ret) {
		dev_err(cm->dev, "%s, failed to get batt uV, ret=%d\n", __func__, ret);
		return false;
	}

	ret = get_ibat_now_uA(cm, &batt_uA);
	if (ret) {
		dev_err(cm->dev, "%s, failed to get batt uA, ret=%d\n", __func__, ret);
		return false;
	}

#ifdef VENDOR_KERNEL      /* sprd 原生disable方案 */
	if (batt_uV < CM_FAST_CHARGE_DISABLE_BATTERY_VOLTAGE ||
	    batt_uA < CM_FAST_CHARGE_DISABLE_CURRENT)
		cm->desc->fast_charge_disable_count++;
	else
		cm->desc->fast_charge_disable_count = 0;

	if (cm->desc->fast_charge_disable_count < CM_FAST_CHARGE_DISABLE_COUNT)
		return false;
#else //not defined VENDOR_KERNEL
	ret = get_charger_voltage(cm, &vbus_uV);
	if (ret) {
		dev_err(cm->dev, "failed to get vbus uV\n");
		return false;
	}

	ret = cm_get_battery_temperature_by_psy(cm, &cur_temp);
	if (ret) {
		dev_err(cm->dev, "failed to get battery temperature\n");
		return false;
	}

	cm->desc->fast_charge_disable_count = 0;

	/* Ibat >= 500mA */
	if (batt_uA >= CM_FAST_CHARGE_DISABLE_CURRENT)
		cm->desc->fast_charge_disable_count++;

	/* 3.5v < Vbat < 4.55v */
	if (batt_uV < CM_FAST_CHARGE_ENABLE_BATTERY_VOLTAGE_MAX &&
			batt_uV > CM_FAST_CHARGE_ENABLE_BATTERY_VOLTAGE_MIN)
		cm->desc->fast_charge_disable_count++;

	/* 0 <= bat_temp < 42 */
	if ((cur_temp < CM_FAST_CHARGE_ENABLE_TEMP_MAX) &&
			(cur_temp >= CM_FAST_CHARGE_ENABLE_TEMP_MIN))
		cm->desc->fast_charge_disable_count++;

	/* cap < 90% */
	if (cm->desc->cap < CM_FAST_CHARGE_ENABLE_CAP && batt_uV < CM_CC_CV_VOLTAGE)
		cm->desc->fast_charge_disable_count++;



#ifdef CONFIG_HQ_USB_TEMP_CHECK
	if (cm->desc->fast_charge_disable_count == CM_FAST_CHARGE_ENABLE_COUNT &&
			(g_cool_down == 0 || g_factory_mode == 1 )&& vbus_uV >= 7000000 && !g_cc_vbus_status)
		return 0;

	dev_info(cm->dev, "batt_uA = %d, batt_uV = %d, vbus_uV = %d, cur_temp = %d, cap = %d, cool_down = %d g_cc_vbus_status = %d\n",
			batt_uA, batt_uV, vbus_uV, cur_temp, cm->desc->cap, g_cool_down,g_cc_vbus_status);
#else
        if (cm->desc->fast_charge_disable_count == CM_FAST_CHARGE_ENABLE_COUNT &&
                        (g_cool_down == 0 || g_factory_mode == 1 )&& vbus_uV >= 7000000)
                return 0;

        dev_info(cm->dev, "batt_uA = %d, batt_uV = %d, vbus_uV = %d, cur_temp = %d, cap = %d, cool_down = %d\n",
                        batt_uA, batt_uV, vbus_uV, cur_temp, cm->desc->cap, g_cool_down);
#endif
#endif //not defined VENDOR_KERNEL

	cm->desc->fast_charge_disable_count = 0;

	return true;
}

static int cm_get_ibat_avg(struct charger_manager *cm, int *ibat)
{
	int ret, batt_uA, min, max, i, sum = 0;
	struct cm_ir_compensation *ir_sts = &cm->desc->ir_comp;

	ret = get_ibat_now_uA(cm, &batt_uA);
	if (ret) {
		dev_err(cm->dev, "get bat_uA error.\n");
		return ret;
	}

	if (ir_sts->ibat_index >= CM_IBAT_BUFF_CNT)
		ir_sts->ibat_index = 0;
	ir_sts->ibat_buf[ir_sts->ibat_index++] = batt_uA;

	if (ir_sts->ibat_buf[CM_IBAT_BUFF_CNT - 1] == CM_MAGIC_NUM)
		return -EINVAL;

	min = max = ir_sts->ibat_buf[0];
	for (i = 0; i < CM_IBAT_BUFF_CNT; i++) {
		if (max < ir_sts->ibat_buf[i])
			max = ir_sts->ibat_buf[i];
		if (min > ir_sts->ibat_buf[i])
			min = ir_sts->ibat_buf[i];
		sum += ir_sts->ibat_buf[i];
	}

	sum  = sum - min - max;

	*ibat = DIV_ROUND_CLOSEST(sum, (CM_IBAT_BUFF_CNT - 2));

	if (*ibat < 0)
		*ibat = 0;

	return ret;
}

static void cm_ir_compensation_init(struct charger_manager *cm)
{
	cm->desc->ir_comp.ibat_buf[CM_IBAT_BUFF_CNT - 1] = CM_MAGIC_NUM;
	cm->desc->ir_comp.ibat_index = 0;
	cm->desc->ir_comp.last_target_cccv = 0;
	if (cm->cm_charge_vote)
		cm->cm_charge_vote->vote(cm->cm_charge_vote, false,
					 SPRD_VOTE_TYPE_CCCV,
					 SPRD_VOTE_TYPE_CCCV_ID_IR,
					 SPRD_VOTE_CMD_MIN,
					 0, cm);
}

static void cm_ir_compensation_enable(struct charger_manager *cm, bool enable)
{
	struct cm_ir_compensation *ir_sts = &cm->desc->ir_comp;

	cm_ir_compensation_init(cm);

	if (enable) {
		if (ir_sts->rc && !ir_sts->ir_compensation_en) {
			dev_info(cm->dev, "%s enable ir compensation\n", __func__);
			ir_sts->ir_compensation_en = true;
			queue_delayed_work(system_power_efficient_wq,
					   &cm->ir_compensation_work,
					   CM_IR_COMPENSATION_TIME * HZ);
		}
		ir_sts->ir_compensation_en = true;
	} else {
		if (ir_sts->ir_compensation_en) {
			dev_info(cm->dev, "%s stop ir compensation\n", __func__);
			cancel_delayed_work_sync(&cm->ir_compensation_work);
			ir_sts->ir_compensation_en = false;
		}
	}
}

static void cm_ir_compensation(struct charger_manager *cm, enum cm_ir_comp_state state, int *target)
{
	struct cm_ir_compensation *ir_sts = &cm->desc->ir_comp;
	int ibat_avg, target_cccv;

	if (!ir_sts->rc)
		return;

	if (cm_get_ibat_avg(cm, &ibat_avg))
		return;

	target_cccv = ir_sts->us + (ibat_avg / 1000)  * ir_sts->rc;

	if (target_cccv < ir_sts->us_lower_limit)
		target_cccv = ir_sts->us_lower_limit;
	else if (target_cccv > ir_sts->us_upper_limit)
		target_cccv = ir_sts->us_upper_limit;

	*target = target_cccv;

	if ((*target / 1000) == (ir_sts->last_target_cccv / 1000))
		return;

	dev_info(cm->dev, "%s, us = %d, rc = %d, upper_limit = %d, lower_limit = %d, "
		 "target_cccv = %d, ibat_avg = %d, offset = %d\n",
		 __func__, ir_sts->us, ir_sts->rc, ir_sts->us_upper_limit,
		 ir_sts->us_lower_limit, target_cccv, ibat_avg,
		 ir_sts->cp_upper_limit_offset);

	ir_sts->last_target_cccv = *target;
	switch (state) {
	case CM_IR_COMP_STATE_CP:
		target_cccv = min(ir_sts->us_upper_limit,
				  (*target + ir_sts->cp_upper_limit_offset));
		fallthrough;
	case CM_IR_COMP_STATE_NORMAL:
		cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					SPRD_VOTE_TYPE_CCCV,
					SPRD_VOTE_TYPE_CCCV_ID_IR,
					SPRD_VOTE_CMD_MAX,
					target_cccv, cm);
		break;
	default:
		break;
	}
}

static void cm_ir_compensation_works(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
						  struct charger_manager,
						  ir_compensation_work);

	int target_cccv;

	cm_ir_compensation(cm, CM_IR_COMP_STATE_NORMAL, &target_cccv);
	queue_delayed_work(system_power_efficient_wq,
			   &cm->ir_compensation_work,
			   CM_IR_COMPENSATION_TIME * HZ);
}

static void cm_fixed_fchg_control_switch(struct charger_manager *cm, bool enable)
{
	int ret;

	dev_dbg(cm->dev, "%s enable = %d start\n", __func__, enable);

	if (!cm->desc->psy_fast_charger_stat)
		return;

	cm->desc->check_fixed_fchg_threshold = enable;
	if (!enable && cm->desc->fixed_fchg_running) {
		cancel_delayed_work_sync(&cm->fixed_fchg_work);
		ret = cm_fixed_fchg_disable(cm);
		if (ret)
			dev_err(cm->dev, "%s, failed to disable fixed fchg\n", __func__);
	}
}

static bool cm_is_need_start_fixed_fchg(struct charger_manager *cm)
{
	bool need = false;

	if (!cm->desc->psy_fast_charger_stat || cm->desc->fixed_fchg_running)
		return false;

	cm_charger_is_support_fchg(cm);
	if ((cm->desc->fast_charger_type == POWER_SUPPLY_USB_CHARGER_TYPE_PD ||
	     cm->desc->fast_charger_type == POWER_SUPPLY_USB_CHARGER_TYPE_SFCP_1P0) &&
	     cm->charger_enabled && cm->desc->check_fixed_fchg_threshold &&
	     cm_is_reach_fchg_threshold(cm))
		need = true;

	return need;
}

static void cm_start_fixed_fchg(struct charger_manager *cm, bool start)
{
	if (!cm->desc->fixed_fchg_running && start) {
		dev_info(cm->dev, "%s, reach fchg threshold, enable it\n", __func__);
		cm->desc->fixed_fchg_running = true;
		schedule_delayed_work(&cm->fixed_fchg_work, 0);
	}
}

static void cm_fixed_fchg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
						  struct charger_manager,
						  fixed_fchg_work);
	int ret, delay_work_ms = cm->desc->polling_interval_ms;

	/*
	 * Effects:
	 *   1. Prevent CM_FAST_CHARGE_ENABLE_COUNT from becoming PPS
	 *      within the time and enable the fast charge status.
	 */
	if (cm->desc->fast_charger_type != POWER_SUPPLY_USB_CHARGER_TYPE_PD &&
	    cm->desc->fast_charger_type != POWER_SUPPLY_USB_CHARGER_TYPE_SFCP_1P0)
		goto stop_fixed_fchg;

	/*
	 * The first if branch: fix the problem that the Xiaomi 65W
	 *                      charger PD2.0 and PPS follow closely.
	 */
	if (cm->desc->fast_charger_type == POWER_SUPPLY_USB_CHARGER_TYPE_PD &&
	    cm->desc->fast_charge_enable_count < CM_FAST_CHARGE_ENABLE_COUNT) {
		cm->desc->fast_charge_enable_count++;
		delay_work_ms = CM_CP_WORK_TIME_MS;
	} else if (cm->desc->enable_fast_charge) {
		if (cm_is_disable_fixed_fchg_check(cm))
			goto stop_fixed_fchg;
	} else {
		ret = cm_fixed_fchg_enable(cm);
		if (ret) {
			dev_err(cm->dev, "%s, failed to enable fixed fchg\n", __func__);
			cm->desc->fixed_fchg_running = false;
			cm->desc->fast_charge_enable_count = 0;
			return;
		}
	}

	schedule_delayed_work(&cm->fixed_fchg_work, msecs_to_jiffies(delay_work_ms));
	return;

stop_fixed_fchg:
	ret = cm_fixed_fchg_disable(cm);
	if (ret) {
		dev_err(cm->dev, "%s, failed to disable fixed fchg, try again!\n", __func__);
		schedule_delayed_work(&cm->fixed_fchg_work,
				      msecs_to_jiffies(CM_TRY_DIS_FCHG_WORK_MS));
		return;
	}
	cm->desc->fixed_fchg_running = false;
	cm->desc->fast_charge_enable_count = 0;
}

static void cm_cp_state_change(struct charger_manager *cm, int state)
{
	cm->desc->cp.cp_state = state;
	dev_dbg(cm->dev, "%s, current cp_state = %d\n", __func__, state);
}

static  bool cm_cp_master_charger_enable(struct charger_manager *cm, bool enable)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int ret;

	if (!cm->desc->psy_cp_stat)
		return false;

	cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[0]);
	if (!cp_psy) {
		dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[0]);
		return false;
	}

	val.intval = enable;
	ret = power_supply_set_property(cp_psy, POWER_SUPPLY_PROP_CALIBRATE, &val);
	power_supply_put(cp_psy);
	if (ret) {
		dev_err(cm->dev, "failed to %s master charge pump, ret = %d\n",
			enable ? "enabel" : "disable", ret);
		return false;
	}

	return true;
}

static void cm_init_cp(struct charger_manager *cm)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int i, ret = -ENODEV;

	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!cp_psy) {
			dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[i]);
			continue;
		}

		val.intval = CM_USB_PRESENT_CMD;
		ret = power_supply_set_property(cp_psy,
						POWER_SUPPLY_PROP_PRESENT,
						&val);
		power_supply_put(cp_psy);
		if (ret) {
			dev_err(cm->dev, "fail to init cp[%d], ret = %d\n", i, ret);
			break;
		}
	}
}

static int cm_adjust_fast_charge_current(struct charger_manager *cm, int cur)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(desc->psy_fast_charger_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			desc->psy_fast_charger_stat[0]);
		return -ENODEV;
	}

	val.intval = cur;
	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_CURRENT_MAX, &val);
	power_supply_put(psy);
	if (ret) {
		dev_err(cm->dev,
			"failed to adjust fast ibus = %d\n", cur);
		return ret;
	}

	return 0;
}

static int cm_fast_enable_pps(struct charger_manager *cm, bool enable)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	dev_dbg(cm->dev, "%s, pps %s\n", __func__, enable ? "enable" : "disable");
	psy = power_supply_get_by_name(desc->psy_fast_charger_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			desc->psy_fast_charger_stat[0]);
		return -ENODEV;
	}

	if (enable)
		val.intval = CM_PPS_CHARGE_ENABLE_CMD;
	else
		val.intval = CM_PPS_CHARGE_DISABLE_CMD;

	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_ONLINE, &val);
	power_supply_put(psy);
	if (ret) {
		dev_err(cm->dev,
			"failed to disable pps\n");
		return ret;
	}

	return 0;
}

static bool cm_check_primary_charger_enabled(struct charger_manager *cm)
{
	int ret;
	bool enabled = false;
	union power_supply_propval val = {0,};
	struct power_supply *psy;

	psy = power_supply_get_by_name(cm->desc->psy_charger_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find primary power supply \"%s\"\n",
			cm->desc->psy_charger_stat[0]);
		return false;
	}

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CALIBRATE, &val);
	power_supply_put(psy);
	if (!ret) {
		if (val.intval)
			enabled = true;
	}

	dev_dbg(cm->dev, "%s: %s\n", __func__, enabled ? "enabled" : "disabled");
	return enabled;
}

static bool cm_check_cp_charger_enabled(struct charger_manager *cm)
{
	int ret;
	bool enabled = false;
	union power_supply_propval val = {0,};
	struct power_supply *cp_psy;

	if (!cm->desc->psy_cp_stat)
		return false;

	cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[0]);
	if (!cp_psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			cm->desc->psy_cp_stat[0]);
		return false;
	}

	ret = power_supply_get_property(cp_psy, POWER_SUPPLY_PROP_CALIBRATE, &val);
	power_supply_put(cp_psy);
	if (!ret)
		enabled = !!val.intval;

	dev_dbg(cm->dev, "%s: %s\n", __func__, enabled ? "enabled" : "disabled");

	return enabled;
}

static void cm_cp_clear_fault_status(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	dev_info(cm->dev, "%s\n", __func__);
	cp->cp_fault_event = false;
	cp->flt.bat_ovp_fault = false;
	cp->flt.bat_ocp_fault = false;
	cp->flt.bus_ovp_fault = false;
	cp->flt.bus_ocp_fault = false;
	cp->flt.bat_therm_fault = false;
	cp->flt.bus_therm_fault = false;
	cp->flt.die_therm_fault = false;

	cp->alm.bat_ovp_alarm = false;
	cp->alm.bat_ocp_alarm = false;
	cp->alm.bus_ovp_alarm = false;
	cp->alm.bus_ocp_alarm = false;
	cp->alm.bat_therm_alarm = false;
	cp->alm.bus_therm_alarm = false;
	cp->alm.die_therm_alarm = false;
	cp->alm.bat_ucp_alarm = false;

}

static void cm_check_cp_fault_status(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	if (!cm->desc->psy_cp_stat || !cm->desc->cm_check_int)
		return;

	dev_info(cm->dev, "%s\n", __func__);

	cm->desc->cm_check_int = false;
	cp->cp_fault_event = true;

	psy = power_supply_get_by_name(cm->desc->psy_cp_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			cm->desc->psy_cp_stat[0]);
		return;
	}

	val.intval = CM_FAULT_HEALTH_CMD;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_HEALTH, &val);
	if (!ret) {
		cp->flt.bat_ovp_fault = !!(val.intval & CM_CHARGER_BAT_OVP_FAULT_MASK);
		cp->flt.bat_ocp_fault = !!(val.intval & CM_CHARGER_BAT_OCP_FAULT_MASK);
		cp->flt.bus_ovp_fault = !!(val.intval & CM_CHARGER_BUS_OVP_FAULT_MASK);
		cp->flt.bus_ocp_fault = !!(val.intval & CM_CHARGER_BUS_OCP_FAULT_MASK);
		cp->flt.bat_therm_fault = !!(val.intval & CM_CHARGER_BAT_THERM_FAULT_MASK);
		cp->flt.bus_therm_fault = !!(val.intval & CM_CHARGER_BUS_THERM_FAULT_MASK);
		cp->flt.die_therm_fault = !!(val.intval & CM_CHARGER_DIE_THERM_FAULT_MASK);
		cp->alm.bat_ovp_alarm = !!(val.intval & CM_CHARGER_BAT_OVP_ALARM_MASK);
		cp->alm.bat_ocp_alarm = !!(val.intval & CM_CHARGER_BAT_OCP_ALARM_MASK);
		cp->alm.bus_ovp_alarm = !!(val.intval & CM_CHARGER_BUS_OVP_ALARM_MASK);
		cp->alm.bus_ocp_alarm = !!(val.intval & CM_CHARGER_BUS_OCP_ALARM_MASK);
		cp->alm.bat_therm_alarm = !!(val.intval & CM_CHARGER_BAT_THERM_ALARM_MASK);
		cp->alm.bus_therm_alarm = !!(val.intval & CM_CHARGER_BUS_THERM_ALARM_MASK);
		cp->alm.die_therm_alarm = !!(val.intval & CM_CHARGER_DIE_THERM_ALARM_MASK);
		cp->alm.bat_ucp_alarm = !!(val.intval & CM_CHARGER_BAT_UCP_ALARM_MASK);
	}
}

static void cm_update_cp_charger_status(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	cp->ibus_uA = 0;
	cp->vbat_uV = 0;
	cp->vbus_uV = 0;
	cp->ibat_uA = 0;

	if (cp->cp_running) {
		if (get_cp_ibus_uA(cm, &cp->ibus_uA)) {
			cp->ibus_uA = 0;
			dev_err(cm->dev, "get ibus current error.\n");
		}

		if (get_cp_vbat_uV(cm, &cp->vbat_uV)) {
			cp->vbat_uV = 0;
			dev_err(cm->dev, "get vbatt error.\n");
		}

		if (get_cp_vbus_uV(cm, &cp->vbus_uV)) {
			cp->vbat_uV = 0;
			dev_err(cm->dev, "get vbus error.\n");
		}

		if (get_cp_ibat_uA(cm, &cp->ibat_uA)) {
			cp->ibat_uA = 0;
			dev_err(cm->dev, "get vbatt error.\n");
		}

	} else {
		if (get_charger_input_current(cm, &cp->ibus_uA)) {
			cp->ibus_uA = 0;
			dev_err(cm->dev, "get ibus current error.\n");
		}

		if (get_vbat_now_uV(cm, &cp->vbat_uV)) {
			cp->vbat_uV = 0;
			dev_err(cm->dev, "get vbatt error.\n");
		}

		if (get_charger_voltage(cm, &cp->vbus_uV)) {
			cp->vbat_uV = 0;
			dev_err(cm->dev, "get vbus error.\n");
		}


		if (get_ibat_now_uA(cm, &cp->ibat_uA)) {
			cp->ibat_uA = 0;
			dev_err(cm->dev, "get vbatt error.\n");
		}
	}

	dev_dbg(cm->dev, " %s,  %s, batt_uV = %duV, vbus_uV = %duV, batt_uA = %duA, ibus_uA = %duA\n",
	       __func__, (cp->cp_running ? "charge pump" : "Primary charger"),
	       cp->vbat_uV, cp->vbus_uV, cp->ibat_uA, cp->ibus_uA);
}

static void cm_cp_check_vbus_status(struct charger_manager *cm)
{
	struct cm_fault_status *fault = &cm->desc->cp.flt;
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int ret;

	fault->vbus_error_lo = false;
	fault->vbus_error_hi = false;

	if (!cm->desc->psy_cp_stat || !cm->desc->cp.cp_running)
		return;

	cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[0]);
	if (!cp_psy) {
		dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[0]);
		return;
	}

	val.intval = CM_BUS_ERR_HEALTH_CMD;
	ret = power_supply_get_property(cp_psy, POWER_SUPPLY_PROP_HEALTH, &val);
	power_supply_put(cp_psy);
	if (ret) {
		dev_err(cm->dev, "failed to get vbus status, ret = %d\n", ret);
		return;
	}

	fault->vbus_error_lo = !!(val.intval & CM_CHARGER_BUS_ERR_LO_MASK);
	fault->vbus_error_hi = !!(val.intval & CM_CHARGER_BUS_ERR_HI_MASK);
}

static void cm_check_target_ibus(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	int target_ibus;

	target_ibus = cp->cp_max_ibus;

	if (cp->adapter_max_ibus > 0)
		target_ibus = min(target_ibus, cp->adapter_max_ibus);

	if (cm->desc->thm_info.thm_adjust_cur > 0)
		target_ibus = min(target_ibus, cm->desc->thm_info.thm_adjust_cur);

	cp->cp_target_ibus = target_ibus;

	dev_dbg(cm->dev, "%s, adp_max_ibus = %d, cp_max_ibus = %d, thm_cur = %d, target_ibus = %d\n",
	       __func__, cp->adapter_max_ibus, cp->cp_max_ibus,
	       cm->desc->thm_info.thm_adjust_cur, cp->cp_target_ibus);
}

static void cm_check_target_vbus(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	if (cp->adapter_max_vbus > 0)
		cp->cp_target_vbus = min(cp->cp_target_vbus, cp->adapter_max_vbus);

	dev_dbg(cm->dev, "%s, adp_max_vbus = %d, target_vbus = %d\n",
	       __func__, cp->adapter_max_vbus, cp->cp_target_vbus);
}

static int cm_cp_vbat_step_algo(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	int vbat_step = 0, delta_vbat_uV;

	delta_vbat_uV = cp->cp_target_vbat - cp->vbat_uV;

	if (cp->vbat_uV > 0 && delta_vbat_uV > CM_CP_VBAT_STEP1)
		vbat_step = CM_CP_VSTEP * 3;
	else if (cp->vbat_uV > 0 && delta_vbat_uV > CM_CP_VBAT_STEP2)
		vbat_step = CM_CP_VSTEP * 2;
	else if (cp->vbat_uV > 0 && delta_vbat_uV > CM_CP_VBAT_STEP3)
		vbat_step = CM_CP_VSTEP;
	else if (cp->vbat_uV > 0 && delta_vbat_uV < 0)
		vbat_step = -CM_CP_VSTEP * 2;

	return vbat_step;
}

static int cm_cp_ibat_step_algo(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	int ibat_step = 0, delta_ibat_uA;

	delta_ibat_uA = cp->cp_target_ibat - cp->ibat_uA;

	if (cp->ibat_uA > 0 && delta_ibat_uA > CM_CP_IBAT_STEP1)
		ibat_step = CM_CP_VSTEP * 3;
	else if (cp->ibat_uA > 0 && delta_ibat_uA > CM_CP_IBAT_STEP2)
		ibat_step = CM_CP_VSTEP * 2;
	else if (cp->ibat_uA > 0 && delta_ibat_uA > CM_CP_IBAT_STEP3)
		ibat_step = CM_CP_VSTEP;
	else if (cp->ibat_uA > 0 && delta_ibat_uA < 0)
		ibat_step = -CM_CP_VSTEP * 2;

	return ibat_step;
}

static int cm_cp_vbus_step_algo(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	int vbus_step = 0, delta_vbus_uV;

	delta_vbus_uV = cp->adapter_max_vbus - cp->vbus_uV;

	if (cp->vbus_uV > 0 && delta_vbus_uV > CM_CP_VBUS_STEP1)
		vbus_step = CM_CP_VSTEP * 3;
	else if (cp->vbus_uV > 0 && delta_vbus_uV > CM_CP_VBUS_STEP2)
		vbus_step = CM_CP_VSTEP * 2;
	else if (cp->vbus_uV > 0 && delta_vbus_uV > CM_CP_VBUS_STEP3)
		vbus_step = CM_CP_VSTEP;
	else if (cp->vbus_uV > 0 && delta_vbus_uV < 0)
		vbus_step = -CM_CP_VSTEP * 2;

	return vbus_step;
}

static int cm_cp_ibus_step_algo(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	int ibus_step = 0, delta_ibus_uA;

	delta_ibus_uA = cp->cp_target_ibus - cp->ibus_uA;

	if (cp->ibus_uA > 0 && delta_ibus_uA > CM_CP_IBUS_STEP1)
		ibus_step = CM_CP_VSTEP * 3;
	else if (cp->ibus_uA > 0 && delta_ibus_uA > CM_CP_IBUS_STEP2)
		ibus_step = CM_CP_VSTEP * 2;
	else if (cp->ibus_uA > 0 && delta_ibus_uA > CM_CP_IBUS_STEP3)
		ibus_step = CM_CP_VSTEP;
	else if (cp->ibus_uA > 0 && delta_ibus_uA < 0)
		ibus_step = -CM_CP_VSTEP * 2;

	return ibus_step;
}

static bool cm_cp_tune_algo(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	int vbat_step = 0;
	int ibat_step = 0;
	int vbus_step = 0;
	int ibus_step = 0;
	int alarm_step = 0;
	bool is_taper_done = false;

	/* check taper done*/
	if (cp->vbat_uV >= cp->cp_target_vbat - 50000) {
		if (cp->ibat_uA < cp->cp_taper_current) {
			if (cp->cp_taper_trigger_cnt++ > 5) {
				is_taper_done = true;
				return is_taper_done;
			}
		} else {
			cp->cp_taper_trigger_cnt = 0;
		}
	}

	/* check battery voltage*/
	vbat_step = cm_cp_vbat_step_algo(cm);

	/* check battery current*/
	ibat_step = cm_cp_ibat_step_algo(cm);

	/* check bus voltage*/
	vbus_step = cm_cp_vbus_step_algo(cm);

	/* check bus current*/
	cm_check_target_ibus(cm);
	ibus_step = cm_cp_ibus_step_algo(cm);

	/* check alarm status*/
	if (cp->alm.bat_ovp_alarm || cp->alm.bat_ocp_alarm ||
	    cp->alm.bus_ovp_alarm || cp->alm.bus_ocp_alarm ||
	    cp->alm.bat_therm_alarm || cp->alm.bus_therm_alarm ||
	    cp->alm.die_therm_alarm) {
		dev_warn(cm->dev, "%s, bat_ovp_alarm = %d, bat_ocp_alarm = %d, bus_ovp_alarm = %d, "
			 "bus_ocp_alarm = %d, bat_therm_alarm = %d, bus_therm_alarm = %d, "
			 "die_therm_alarm = %d\n", __func__, cp->alm.bat_ovp_alarm,
			 cp->alm.bat_ocp_alarm, cp->alm.bus_ovp_alarm,
			 cp->alm.bus_ocp_alarm, cp->alm.bat_therm_alarm,
			 cp->alm.bus_therm_alarm, cp->alm.die_therm_alarm);
		alarm_step = -CM_CP_VSTEP * 2;
	} else {
		alarm_step = CM_CP_VSTEP * 3;
	}

	cp->cp_target_vbus += min(min(min(min(vbat_step, ibat_step),
					  vbus_step), ibus_step), alarm_step);
	cm_check_target_vbus(cm);

	dev_info(cm->dev, "%s vbatt = %duV, ibatt = %duA, vbus = %duV, ibus = %duA, "
		 "cp_target_vbat = %duV, cp_target_ibat = %duA, cp_target_vbus = %duV, "
		 "cp_target_ibus = %duA, cp_taper_current = %duA, taper_cnt = %d, "
		 "vbat_step = %d, ibat_step = %d, vbus_step = %d, ibus_step = %d, alarm_step = %d, "
		 "adapter_max_vbus = %duV, adapter_max_ibus = %duA, ucp_cnt = %d\n",
		 __func__, cp->vbat_uV, cp->ibat_uA, cp->vbus_uV, cp->ibus_uA,
		 cp->cp_target_vbat, cp->cp_target_ibat, cp->cp_target_vbus,
		 cp->cp_target_ibus, cp->cp_taper_current, cp->cp_taper_trigger_cnt,
		 vbat_step, ibat_step, vbus_step, ibus_step, alarm_step,
		 cp->adapter_max_vbus, cp->adapter_max_ibus, cp->cp_ibat_ucp_cnt);

	return is_taper_done;
}

static bool cm_cp_check_ibat_ucp_status(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	bool status = false;
	bool ibat_ucp_flag = false;

	if (cp->alm.bat_ucp_alarm) {
		dev_warn(cm->dev, "%s, bat_ucp_alarm = %d\n", __func__, cp->alm.bat_ucp_alarm);
		cp->cp_ibat_ucp_cnt++;
		ibat_ucp_flag = true;
	}

	if (!cp->cp_ibat_ucp_cnt)
		return status;

	if (cp->vbat_uV >= cp->cp_target_vbat - 50000) {
		cp->cp_ibat_ucp_cnt = 0;
		return status;
	}

	if (cp->ibat_uA < cp->cp_taper_current && !(ibat_ucp_flag))
		cp->cp_ibat_ucp_cnt++;
	else if (cp->ibat_uA >= cp->cp_taper_current)
		cp->cp_ibat_ucp_cnt = 0;

	if (cp->cp_ibat_ucp_cnt > CM_CP_IBAT_UCP_THRESHOLD)
		status = true;

	return status;
}

static void cm_cp_state_recovery(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	dev_info(cm->dev, "cm_cp_state_machine: state %d, %s\n",
	       cp->cp_state, cm_cp_state_names[cp->cp_state]);

	if (is_ext_pwr_online(cm) && cm_is_reach_fchg_threshold(cm)) {
		cm_cp_state_change(cm, CM_CP_STATE_ENTRY);
	} else {
		cm->desc->cp.recovery = false;
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
	}
}

static void cm_cp_state_entry(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	static int primary_charger_dis_retry;

	dev_info(cm->dev, "cm_cp_state_machine: state %d, %s\n",
	       cp->cp_state, cm_cp_state_names[cp->cp_state]);

	cm->desc->cm_check_fault = false;
	cm_fast_enable_pps(cm, false);
	if (cm_fast_enable_pps(cm, true)) {
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
		dev_err(cm->dev, "fail to enable pps\n");
		return;
	}

	cm_adjust_fast_charge_voltage(cm, CM_FAST_CHARGE_VOLTAGE_5V);
	cm_cp_master_charger_enable(cm, false);
	cm_primary_charger_enable(cm, false);
	cm_ir_compensation_enable(cm, false);

	if (cm_check_primary_charger_enabled(cm)) {
		if (primary_charger_dis_retry++ > CM_CP_PRIMARY_CHARGER_DIS_TIMEOUT) {
			cm_cp_state_change(cm, CM_CP_STATE_EXIT);
			primary_charger_dis_retry = 0;
		}
		return;
	}

	cm_get_adapter_max_current(cm, &cp->adapter_max_ibus);
	cm_get_adapter_max_voltage(cm, &cp->adapter_max_vbus);

	cm_init_cp(cm);

	cp->recovery = false;
	cm->desc->enable_fast_charge = true;

	cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
				   CM_CHARGE_INFO_INPUT_LIMIT |
				   CM_CHARGE_INFO_THERMAL_LIMIT |
				   CM_CHARGE_INFO_JEITA_LIMIT));

	cm_cp_master_charger_enable(cm, true);

	cm->desc->cp.tune_vbus_retry = 0;
	primary_charger_dis_retry = 0;
	cp->cp_ibat_ucp_cnt = 0;

	cp->cp_target_ibus = cp->cp_max_ibus;

	if (cp->vbat_uV <= CM_CP_ACC_VBAT_HTHRESHOLD)
		cp->cp_target_vbus = cp->vbat_uV * 205 / 100 + 10 * CM_CP_VSTEP;
	else
		cp->cp_target_vbus = cp->vbat_uV * 205 / 100 + 2 * CM_CP_VSTEP;


	dev_dbg(cm->dev, "%s, target_ibat = %d, cp_target_vbus = %d\n",
		 __func__, cp->cp_target_ibat, cp->cp_target_vbus);

	cm_check_target_vbus(cm);
	cm_adjust_fast_charge_voltage(cm, cp->cp_target_vbus);
	cp->cp_last_target_vbus = cp->cp_target_vbus;

	cm_check_target_ibus(cm);
	cm_adjust_fast_charge_current(cm, cp->cp_target_ibus);
	cm_cp_state_change(cm, CM_CP_STATE_CHECK_VBUS);
}

static void cm_cp_state_check_vbus(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	dev_info(cm->dev, "cm_cp_state_machine: state %d, %s\n",
		 cp->cp_state, cm_cp_state_names[cp->cp_state]);

	if (cp->flt.vbus_error_lo && cp->vbus_uV < cp->vbat_uV * 219 / 100) {
		cp->tune_vbus_retry++;
		cp->cp_target_vbus += CM_CP_VSTEP;
		cm_check_target_vbus(cm);

		if (cm_adjust_fast_charge_voltage(cm, cp->cp_target_vbus))
			cp->cp_target_vbus -= CM_CP_VSTEP;

	} else if (cp->flt.vbus_error_hi && cp->vbus_uV > cp->vbat_uV * 205 / 100) {
		cp->tune_vbus_retry++;
		cp->cp_target_vbus -= CM_CP_VSTEP;
		if (cm_adjust_fast_charge_voltage(cm, cp->cp_target_vbus))
			dev_err(cm->dev, "fail to adjust pps voltage = %duV\n",
				cp->cp_target_vbus);
	} else {
		dev_info(cm->dev, "adapter volt tune ok, retry %d times\n",
			 cp->tune_vbus_retry);
		cm_cp_state_change(cm, CM_CP_STATE_TUNE);

		if (!cm_check_cp_charger_enabled(cm))
			cm_cp_master_charger_enable(cm, true);

		cm->desc->cm_check_fault = true;
		return;
	}

	dev_info(cm->dev, " %s, target_ibat = %duA, cp_target_vbus = %duV, vbus_err_lo = %d, "
		 "vbus_err_hi = %d, retry_time = %d",
		 __func__, cp->cp_target_ibat, cp->cp_target_vbus,
		 cp->flt.vbus_error_lo, cp->flt.vbus_error_hi, cp->tune_vbus_retry);

	if (cp->tune_vbus_retry >= 50) {
		dev_info(cm->dev, "Failed to tune adapter volt into valid range,move to CM_CP_STATE_EXIT\n");
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
	}
}

static void cm_cp_state_tune(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	int target_vbat = 0;

	if (!cp->cp_state_tune_log) {
		dev_info(cm->dev, "cm_cp_state_machine: state %d, %s\n",
			 cp->cp_state, cm_cp_state_names[cp->cp_state]);
		cp->cp_state_tune_log = true;
	}

	cm_ir_compensation(cm, CM_IR_COMP_STATE_CP, &target_vbat);
	if (target_vbat > 0)
		cp->cp_target_vbat = target_vbat;

	if (cp->flt.bat_therm_fault || cp->flt.die_therm_fault ||
	    cp->flt.bus_therm_fault) {
		dev_err(cm->dev, "bat_therm_fault = %d, die_therm_fault = %d, exit cp\n",
			 cp->flt.bat_therm_fault, cp->flt.die_therm_fault);
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);

	} else if (cp->flt.bat_ocp_fault || cp->flt.bat_ovp_fault ||
		cp->flt.bus_ocp_fault || cp->flt.bus_ovp_fault) {
		dev_err(cm->dev, "bat_ocp_fault = %d, bat_ovp_fault = %d, "
			 "bus_ocp_fault = %d, bus_ovp_fault = %d, exit cp\n",
			 cp->flt.bat_ocp_fault, cp->flt.bat_ovp_fault,
			 cp->flt.bus_ocp_fault, cp->flt.bus_ovp_fault);
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);

	} else if (!cm_check_cp_charger_enabled(cm) &&
		   (cp->flt.vbus_error_hi || cp->flt.vbus_error_lo)) {
		dev_err(cm->dev, " %s some error happen, need recovery\n", __func__);
		cp->recovery = true;
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);

	} else if (!cm_check_cp_charger_enabled(cm)) {
		dev_err(cm->dev, "%s cp charger is disabled, exit cp\n", __func__);
		cp->recovery = true;
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
	} else if (cm_cp_check_ibat_ucp_status(cm)) {
		dev_err(cm->dev, "cp_ibat_ucp_cnt =%d, exit cp!\n", cp->cp_ibat_ucp_cnt);
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
	} else {
		dev_info(cm->dev, "cp is ok, fine tune\n");
		if (cm_cp_tune_algo(cm)) {
			dev_info(cm->dev, "taper done, exit cp machine\n");
			cm_cp_state_change(cm, CM_CP_STATE_EXIT);
			cp->recovery = false;
		} else {
			if (cp->cp_last_target_vbus != cp->cp_target_vbus) {
				cm_adjust_fast_charge_voltage(cm, cp->cp_target_vbus);
				cp->cp_last_target_vbus = cp->cp_target_vbus;
				cp->cp_adjust_cnt = 0;
			} else if (cp->cp_adjust_cnt++ > CM_CP_ADJUST_VOLTAGE_THRESHOLD) {
				cm_adjust_fast_charge_voltage(cm, cp->cp_target_vbus);
				cp->cp_adjust_cnt = 0;
			}
		}
	}

	if (cp->cp_fault_event)
		cm_cp_clear_fault_status(cm);
}

static void cm_cp_state_exit(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	dev_info(cm->dev, "cm_cp_state_machine: state %d, %s\n",
		 cp->cp_state, cm_cp_state_names[cp->cp_state]);

	if (!cm_cp_master_charger_enable(cm, false))
		return;

	/* Hardreset will request 5V/2A or 5V/3A default.
	 * Disable pps will request sink-pdos PDO_FIXED value.
	 * And PDO_FIXED defined in dts is 5V/2A or 5V/3A, so
	 * we does not need requeset 5V/2A or 5V/3A when exit cp
	 */
	cm_fast_enable_pps(cm, false);

	if (!cp->recovery)
		cp->cp_running = false;

	cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
				   CM_CHARGE_INFO_INPUT_LIMIT |
				   CM_CHARGE_INFO_THERMAL_LIMIT |
				   CM_CHARGE_INFO_JEITA_LIMIT));

	if (!cm->charging_status && !cm->emergency_stop) {
		cm_primary_charger_enable(cm, true);
		cm_ir_compensation_enable(cm, true);
	}

	if (cp->recovery)
		cm_cp_state_change(cm, CM_CP_STATE_RECOVERY);

	cm->desc->cm_check_fault = false;
	cm->desc->enable_fast_charge = false;
	cp->cp_fault_event = false;
	cp->cp_ibat_ucp_cnt = 0;
	cp->cp_state_tune_log = false;
}

static int cm_cp_state_machine(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	dev_dbg(cm->dev, "%s, state %d, %s\n", __func__,
	       cp->cp_state, cm_cp_state_names[cp->cp_state]);

	switch (cp->cp_state) {
	case CM_CP_STATE_RECOVERY:
		cm_cp_state_recovery(cm);
		break;
	case CM_CP_STATE_ENTRY:
		cm_cp_state_entry(cm);
		break;
	case CM_CP_STATE_CHECK_VBUS:
		cm_cp_state_check_vbus(cm);
		break;
	case CM_CP_STATE_TUNE:
		cm_cp_state_tune(cm);
		break;
	case CM_CP_STATE_EXIT:
		cm_cp_state_exit(cm);
		break;
	case CM_CP_STATE_UNKNOWN:
	default:
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
		break;
	}

	return 0;
}

static void cm_cp_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
						  struct charger_manager,
						  cp_work);

	cm_update_cp_charger_status(cm);
	cm_cp_check_vbus_status(cm);

	if (cm->desc->cm_check_int && cm->desc->cm_check_fault)
		cm_check_cp_fault_status(cm);

	if (cm->desc->cp.cp_running && !cm_cp_state_machine(cm))
		schedule_delayed_work(&cm->cp_work, msecs_to_jiffies(CM_CP_WORK_TIME_MS));
}

static void cm_cp_control_switch(struct charger_manager *cm, bool enable)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	dev_dbg(cm->dev, "%s enable = %d start\n", __func__, enable);

	if (!cm->desc->psy_cp_stat)
		return;

	if (enable) {
		cp->check_cp_threshold = enable;
	} else {
		cp->check_cp_threshold = enable;
		cp->recovery = false;
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
		if (cp->cp_running) {
			cancel_delayed_work_sync(&cm->cp_work);
			cm_cp_state_machine(cm);
		}
		__pm_relax(cm->charge_ws);
	}
}

static bool cm_is_need_start_cp(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	bool need = false;
	int ret;

	if (!cm->desc->psy_cp_stat || cm->desc->cp.cp_running ||
	    cm->desc->fast_charger_type != POWER_SUPPLY_USB_CHARGER_TYPE_PD_PPS)
		return false;

	/*
	 * Before starting the cp state machine, you need to turn
	 * off fixed_fchg. If the shutdown fails, the next charging
	 * cycle will be judged again.
	 */
	if (cm->desc->fixed_fchg_running) {
		cancel_delayed_work_sync(&cm->fixed_fchg_work);
		ret = cm_fixed_fchg_disable(cm);
		if (ret) {
			dev_err(cm->dev, "%s, failed to disable fixed fchg\n", __func__);
			return false;
		}
	}

	cm_charger_is_support_fchg(cm);
	dev_info(cm->dev, "%s, check_cp_threshold = %d, pps_running = %d, fast_charger_type = %d\n",
		 __func__, cp->check_cp_threshold, cp->cp_running, cm->desc->fast_charger_type);
	if (cp->check_cp_threshold && !cp->cp_running &&
	    cm_is_reach_fchg_threshold(cm) && cm->charger_enabled)
		need = true;

	return need;
}

static void cm_start_cp_state_machine(struct charger_manager *cm, bool start)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	if (!cp->cp_running && start) {
		dev_info(cm->dev, "%s, reach pps threshold\n", __func__);
		cp->cp_running = start;
		cm->desc->cm_check_fault = false;
		__pm_stay_awake(cm->charge_ws);
		cm_cp_state_change(cm, CM_CP_STATE_ENTRY);
		schedule_delayed_work(&cm->cp_work, 0);
	}
}

static int try_charger_enable_by_psy(struct charger_manager *cm, bool enable)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	struct power_supply *psy;
	int i, err;

	for (i = 0; desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_charger_stat[i]);
			continue;
		}

		val.intval = enable;
		err = power_supply_set_property(psy, POWER_SUPPLY_PROP_STATUS,
						&val);
		power_supply_put(psy);
		if (err)
			return err;
		if (desc->psy_charger_stat[1])
			break;
	}

	return 0;
}

static int try_wireless_charger_enable_by_psy(struct charger_manager *cm, bool enable)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	struct power_supply *psy;
	int i, err;

	if (!cm->desc->psy_wl_charger_stat)
		return 0;

	for (i = 0; desc->psy_wl_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_wl_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_wl_charger_stat[i]);
			continue;
		}

		val.intval = enable;
		err = power_supply_set_property(psy, POWER_SUPPLY_PROP_CALIBRATE, &val);
		power_supply_put(psy);
		if (err)
			return err;
	}

	return 0;
}

static int try_wireless_cp_converter_enable_by_psy(struct charger_manager *cm, bool enable)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	struct power_supply *psy;
	int i, err;

	if (!cm->desc->psy_cp_converter_stat)
		return 0;

	for (i = 0; desc->psy_cp_converter_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_cp_converter_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_charger_stat[i]);
			continue;
		}

		val.intval = enable;
		err = power_supply_set_property(psy, POWER_SUPPLY_PROP_CALIBRATE, &val);
		power_supply_put(psy);
		if (err)
			return err;
	}

	return 0;
}

static int cm_set_primary_charge_wirless_type(struct charger_manager *cm, bool enable)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int ret = 0;

	psy = power_supply_get_by_name(cm->desc->psy_charger_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			cm->desc->psy_charger_stat[0]);
		return false;
	}

	if (enable)
		val.intval = cm->desc->charger_type;
	else
		val.intval = 0;

	dev_info(cm->dev, "set wirless type = %d\n", val.intval);
	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_TYPE, &val);
	power_supply_put(psy);

	return ret;
}

static int set_charger_init_info(struct charger_manager *cm)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int i, ret = -ENODEV;

//	set_charger_iterm_en(cm, 1);

	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		/* set charge iterm */
/*		ret = power_supply_set_property(psy,
						POWER_SUPPLY_PROP_ITERM,
						(const union power_supply_propval *)&val);*/
		/* set charge timer diable */
		val.intval = CM_SET_SAFFY_TIMER_DISABLE_CMD;
		ret = power_supply_set_property(psy,
						POWER_SUPPLY_PROP_STATUS,
						(const union power_supply_propval *)&val);

		/* set charge VINDPM */
/*		val.intval = 4600;
		ret = power_supply_set_property(psy,
						POWER_SUPPLY_PROP_VINDPM,
						(const union power_supply_propval *)&val);
		power_supply_put(psy);
		if (ret) {
			dev_err(cm->dev, "%s failed to set vindpm disable \n", __func__);
			return ret;
		}*/
		val.intval = CM_SET_PRE_CURRENT_CMD;
		ret = power_supply_set_property(psy,
						POWER_SUPPLY_PROP_STATUS,
						(const union power_supply_propval *)&val);
	}

	return ret;
}

static int dump_charger_reg_info(struct charger_manager *cm)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int i, ret = -ENODEV;

	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}


		val.intval = CM_DUMP_CHARGER_REGISTER_CMD;
		ret = power_supply_set_property(psy,
						POWER_SUPPLY_PROP_STATUS,
						(const union power_supply_propval *)&val);

	}

	return ret;
}

static void try_wireless_charger_enable(struct charger_manager *cm, bool enable)
{
	int ret = 0;

	ret = cm_set_primary_charge_wirless_type(cm, enable);
	if (ret) {
		dev_err(cm->dev, "set wl type to primary charge fail, ret = %d\n", ret);
		return;
	}

	ret = try_wireless_charger_enable_by_psy(cm, enable);
	if (ret) {
		dev_err(cm->dev, "enable wl charger fail, ret = %d\n", ret);
		return;
	}

	ret = try_wireless_cp_converter_enable_by_psy(cm, enable);
	if (ret)
		dev_err(cm->dev, "enable wl charger fail, ret = %d\n", ret);
}

/**
 * try_charger_enable - Enable/Disable chargers altogether
 * @cm: the Charger Manager representing the battery.
 * @enable: true: enable / false: disable
 *
 * Note that Charger Manager keeps the charger enabled regardless whether
 * the charger is charging or not (because battery is full or no external
 * power source exists) except when CM needs to disable chargers forcibly
 * because of emergency causes; when the battery is overheated or too cold.
 */
static int noplug_temperature = 0;
static int noplug_batt_volt_max = 0;
static int noplug_batt_volt_min = 0;
static int try_charger_enable(struct charger_manager *cm, bool enable)
{
	int err = 0;
	dev_info(cm->dev, "try_charger_enable,enable:%d charger_enabled:%d",enable,cm->charger_enabled);

	/* Ignore if it's redundant command */
	if (enable == cm->charger_enabled)
		return 0;
	dev_info(cm->dev, "try_charger_enable,enable:%d charger_enabled:%d",enable,cm->charger_enabled);
	if (enable) {
		if (cm->emergency_stop)
			return -EAGAIN;

		/*
		 * Enable charge is permitted in calibration mode
		 * even if use fake battery.
		 * So it will not return in calibration mode.
		 */
		if (!is_batt_present(cm) && !allow_charger_enable)
			return 0;
		set_charger_init_info(cm);
		/*
		 * Save start time of charging to limit
		 * maximum possible charging time.
		 */
		cm->charging_start_time = ktime_to_ms(ktime_get());
		cm->charging_end_time = 0;
		noplug_temperature = cm->desc->temperature;
		get_vbat_now_uV(cm, &noplug_batt_volt_max);
		get_vbat_now_uV(cm, &noplug_batt_volt_min);

		err = try_charger_enable_by_psy(cm, enable);
		cm_ir_compensation_enable(cm, enable);
		cm_fixed_fchg_control_switch(cm, enable);
		cm_cp_control_switch(cm, enable);
	} else {
		/*
		 * Save end time of charging to maintain fully charged state
		 * of battery after full-batt.
		 */
		cm->charging_start_time = 0;
		cm->charging_end_time = ktime_to_ms(ktime_get());
		cm_cp_control_switch(cm, enable);
		cm_fixed_fchg_control_switch(cm, enable);
		cm_ir_compensation_enable(cm, enable);
		err = try_charger_enable_by_psy(cm, enable);
	}

	if (!err) {
		cm->charger_enabled = enable;
		power_supply_changed(cm->charger_psy);
	}
	return err;
}

/**
 * try_charger_restart - Restart charging.
 * @cm: the Charger Manager representing the battery.
 *
 * Restart charging by turning off and on the charger.
 */
 /*
static int try_charger_restart(struct charger_manager *cm)
{
	int err;

	if (cm->emergency_stop)
		return -EAGAIN;

	err = try_charger_enable(cm, false);
	if (err)
		return err;

	return try_charger_enable(cm, true);
}
*/
static void cm_uevent_notify(struct charger_manager *cm)
{
	int ret;
	char *env[2] = { "CHGSTAT=1", NULL };

	//dev_info(cm->dev, "%s :0x%x\n", __func__, notify_code);
	ret = kobject_uevent_env(&cm->dev->kobj, KOBJ_CHANGE, env);
	if (ret) {
		dev_err(cm->dev, "%s :kobject_uevent_env fail!!\n", __func__);
	}
}

/**
 * fullbatt_vchk - Check voltage drop some times after "FULL" event.
 * @work: the work_struct appointing the function
 *
 * If a user has designated "fullbatt_vchkdrop_ms/uV" values with
 * charger_desc, Charger Manager checks voltage drop after the battery
 * "FULL" event. It checks whether the voltage has dropped more than
 * fullbatt_vchkdrop_uV by calling this function after fullbatt_vchkrop_ms.
 */

#define TEMPERATURE_LEVE_N20	-100
#define TEMPERATURE_LEVE_0		0
#define TEMPERATURE_LEVE_P5		50
#define TEMPERATURE_LEVE_P53	530
#define VRES_45     370000
#define VRES_10MV	65000
#define VRES_20MV	165000
#define VRES_30MV	265000

static void fullbatt_vchk(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
			struct charger_manager, fullbatt_vchk_work);
	struct charger_desc *desc = cm->desc;
	int batt_ocv, err, diff;
	int tbat;
	int cur_jeita_status;

	/* remove the appointment for fullbatt_vchk */
	cm->fullbatt_vchk_jiffies_at = 0;

	if (!desc->fullbatt_vchkdrop_uV || !desc->fullbatt_vchkdrop_ms)
		return;

	err = get_batt_ocv(cm, &batt_ocv);
	if (err) {
		dev_err(cm->dev, "%s: get_batt_ocV error(%d)\n", __func__, err);
		return;
	}

	tbat = cm->desc->temperature;
	cur_jeita_status = cm_manager_get_jeita_status(cm, tbat);

	if(tbat >= TEMPERATURE_LEVE_N20 && tbat < TEMPERATURE_LEVE_0) {
		desc->fullbatt_vchkdrop_uV =  VRES_30MV;
	} else if(tbat >= TEMPERATURE_LEVE_0 && tbat < TEMPERATURE_LEVE_P5) {
		desc->fullbatt_vchkdrop_uV = VRES_20MV;
	} else if(tbat >= TEMPERATURE_LEVE_P5 && tbat < TEMPERATURE_LEVE_P53) {
		if(cur_jeita_status == CM_HIGH_TEMP_STATUS){
			desc->fullbatt_vchkdrop_uV = VRES_45;
		} else {
			desc->fullbatt_vchkdrop_uV = VRES_10MV;
		}
	}

#ifdef VENDOR_KERNEL
//	if(prj_name == 1){//��Ҫȷ���Ƿ���prj 1
		desc->fullbatt_vchkdrop_uV -= 5000;
//	}
#else //not defined VENDOR_KERNEL
	if(prj_name == 1){//��Ҫȷ���Ƿ���prj 1
		desc->fullbatt_vchkdrop_uV -= 5000;
	}
#endif //defined VENDOR_KERNEL

	diff = desc->fullbatt_uV - batt_ocv;
	if (diff < 0)
		return;

	//dev_info(cm->dev, "VBATT dropped %duV after full-batt:%d\n", diff,desc->fullbatt_vchkdrop_uV);
}

/**
 * check_charging_duration - Monitor charging/discharging duration
 * @cm: the Charger Manager representing the battery.
 *
 * If whole charging duration exceed 'charging_max_duration_ms',
 * cm stop charging to prevent overcharge/overheat. If discharging
 * duration exceed 'discharging _max_duration_ms', charger cable is
 * attached, after full-batt, cm start charging to maintain fully
 * charged state for battery.
 */
//add by HQ:zhaolianghua for remove time check in aging version
#ifndef AGING_BUILD
static void check_charging_duration(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	u64 curr = ktime_to_ms(ktime_get());
	u64 duration;
	int ret = false;

	if (!desc->charging_max_duration_ms && !desc->discharging_max_duration_ms)
		return;

	if (cm->charging_status != 0 && !(cm->charging_status & CM_CHARGE_DURATION_ABNORMAL))
		return;

	if (cm->charger_enabled) {
		int batt_ocv, diff;

		ret = get_batt_ocv(cm, &batt_ocv);
		if (ret) {
			dev_err(cm->dev, "failed to get battery OCV\n");
			return;
		}

		diff = desc->fullbatt_uV - batt_ocv;
		duration = curr - cm->charging_start_time;

		if (duration > desc->charging_max_duration_ms) {
			dev_info(cm->dev, "Charging duration exceed %ums\n",
				 desc->charging_max_duration_ms);
			cm->charging_status |= CM_CHARGE_DURATION_ABNORMAL;
			notify_code |= CHG_BAT_TIMEOUT_STATUS;
			cm_uevent_notify(cm);
			try_charger_enable(cm, false);
		}
	}/* else if (!cm->charger_enabled  && (cm->charging_status & CM_CHARGE_DURATION_ABNORMAL)) {
		duration = curr - cm->charging_end_time;

		if (duration > desc->discharging_max_duration_ms) {
			dev_info(cm->dev, "Discharging duration exceed %ums\n",
				 desc->discharging_max_duration_ms);
			cm->charging_status &= ~CM_CHARGE_DURATION_ABNORMAL;
		}
	}*/

	return;
}
#endif

static int cm_get_battery_temperature_by_psy(struct charger_manager *cm, int *temp)
{
	struct power_supply *fuel_gauge;
	int ret;
	int64_t temp_val;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge,
				POWER_SUPPLY_PROP_TEMP,
				(union power_supply_propval *)&temp_val);
	power_supply_put(fuel_gauge);

	*temp = (int)temp_val;
	return ret;
}

static int cm_get_battery_temperature(struct charger_manager *cm, int *temp)
{
	int ret = 0;

	if (!cm->desc->measure_battery_temp)
		return -ENODEV;

#if IS_ENABLED(CONFIG_THERMAL)
	if (cm->tzd_batt) {
		ret = thermal_zone_get_temp(cm->tzd_batt, temp);
		if (!ret)
			/* Calibrate temperature unit */
			*temp /= 100;
	} else
#endif
	{
		/* if-else continued from CONFIG_THERMAL */
		*temp = cm->desc->temperature;
	}

	return ret;
}

static int cm_check_thermal_status(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	int temp, upper_limit, lower_limit;
	int ret = 0;

	ret = cm_get_battery_temperature(cm, &temp);
	if (ret) {
		/* FIXME:
		 * No information of battery temperature might
		 * occur hazardous result. We have to handle it
		 * depending on battery type.
		 */
		dev_err(cm->dev, "Failed to get battery temperature\n");
		return 0;
	}

	upper_limit = desc->temp_max;
	lower_limit = desc->temp_min;

	if (cm->emergency_stop) {
		upper_limit -= desc->temp_diff;
		lower_limit += desc->temp_diff;
	}

	if (temp > upper_limit)
		ret = CM_EVENT_BATT_OVERHEAT;
	else if (temp < lower_limit)
		ret = CM_EVENT_BATT_COLD;

	cm->emergency_stop = ret;

	return ret;
}

static void cm_check_charge_voltage(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	int ret, charge_vol;

	if (!desc->charge_voltage_max)
		return;

	if (cm->charging_status != 0 && !(cm->charging_status & CM_CHARGE_VOLTAGE_ABNORMAL))
		return;

	mutex_lock(&cm->desc->charge_info_mtx);
	ret = get_charger_voltage(cm, &charge_vol);
	if (ret) {
		mutex_unlock(&cm->desc->charge_info_mtx);
		dev_warn(cm->dev, "Fail to get charge vol, ret = %d.\n", ret);
		return;
	}
	//dev_info(cm->dev, "vbus= %d ovp cnt = %d\n",charge_vol, i_ovp);
	if (cm->charger_enabled && charge_vol > desc->charge_voltage_max) {
		dev_info(cm->dev, "Charging voltage %d is larger than %d\n",
			 charge_vol, desc->charge_voltage_max);
		i_ovp ++;
		dev_info(cm->dev, "vbus ovp cnt = %d\n", i_ovp);
		if (i_ovp > 5) {
			i_ovp = 6;
			cm->charging_status |= CM_CHARGE_VOLTAGE_ABNORMAL;
			mutex_unlock(&cm->desc->charge_info_mtx);
			try_charger_enable(cm, false);
			notify_code |= CHG_VBUS_OV_STATUS;
			if(bbat_mode == false)
			power_supply_changed(cm->charger_psy);
			cm_uevent_notify(cm);
			dev_info(cm->dev, "Charging voltage is larger than %d\n",
				desc->charge_voltage_max);
		} else {
			mutex_unlock(&cm->desc->charge_info_mtx);
		}
	} else if (!cm->charger_enabled &&
		   charge_vol <= (desc->charge_voltage_max - desc->charge_voltage_drop) &&
		   (cm->charging_status & CM_CHARGE_VOLTAGE_ABNORMAL)) {
		dev_info(cm->dev, "Charging voltage %d less than %d, recharging\n",
			 charge_vol, desc->charge_voltage_max - desc->charge_voltage_drop);
		i_ovp = 0;
		mutex_unlock(&cm->desc->charge_info_mtx);
		cm->charging_status &= ~CM_CHARGE_VOLTAGE_ABNORMAL;
		notify_code &= ~CHG_VBUS_OV_STATUS;
		if(bbat_mode == false)
		power_supply_changed(cm->charger_psy);
	} else {
		mutex_unlock(&cm->desc->charge_info_mtx);
		//dev_info(cm->dev, "*******mutex_unlock cnt = %d\n", i_ovp);
	}
}

static void cm_check_charge_health(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int health = POWER_SUPPLY_HEALTH_UNKNOWN;
	int ret, i;

	if (cm->charging_status != 0 &&
	    !(cm->charging_status & CM_CHARGE_HEALTH_ABNORMAL))
		return;

	for (i = 0; desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_HEALTH, &val);
		power_supply_put(psy);
		if (ret)
			return;
		health = val.intval;
	}

	if (health == POWER_SUPPLY_HEALTH_UNKNOWN)
		return;

	if (cm->charger_enabled && health != POWER_SUPPLY_HEALTH_GOOD) {
		dev_info(cm->dev, "Charging health is not good\n");
		cm->charging_status |= CM_CHARGE_HEALTH_ABNORMAL;
		try_charger_enable(cm, false);
	} else if (!cm->charger_enabled && health == POWER_SUPPLY_HEALTH_GOOD &&
		   (cm->charging_status & CM_CHARGE_HEALTH_ABNORMAL)) {
		dev_info(cm->dev, "Charging health is recover good\n");
		cm->charging_status &= ~CM_CHARGE_HEALTH_ABNORMAL;
	}
}

static void cm_bat_id_check(struct charger_manager *cm)
{
	int bat_id;

	bat_id = sc27xx_fgu_bat_id;
	if (!is_batt_present(cm) || (bat_id == 3)) {
		notify_code |= CHG_BAT_ID_STATUS;
		try_charger_enable(cm, false);
		cm_uevent_notify(cm);
		if(bbat_mode == false)
		power_supply_changed(cm->charger_psy);
		dev_err(cm->dev, "battery id:others bat_id:%d \n", bat_id);
	} else if(notify_code & CHG_BAT_ID_STATUS){
		notify_code &= ~CHG_BAT_ID_STATUS;
		try_charger_enable(cm, true);
		dev_err(cm->dev, "battery id:standard bat_id:%d \n", bat_id);
	}
}

static void cm_bat_ovp_check(struct charger_manager *cm)
{
	int ret, batt_uV;
	ret = get_vbat_now_uV(cm, &batt_uV);
	if (ret) {
		dev_err(cm->dev, "get_batt_uV error.\n");
		return;
	}

	if (batt_uV >= BATTERY_VOLTAGE_MAX) {
		bat_ovp_flag = true;
		notify_code |= CHG_BAT_OV_STATUS;
		try_charger_enable(cm, false);
		cm_uevent_notify(cm);
		if(bbat_mode == false)
		power_supply_changed(cm->charger_psy);
		dev_err(cm->dev, "%s: bat volt:%d Discharge!\n", __func__, batt_uV);
	} else if (batt_uV < BATTERY_RECHARGE_VOLTAGE) {
		notify_code &= ~CHG_BAT_OV_STATUS;
		if(bbat_mode == false)
		power_supply_changed(cm->charger_psy);
		//dev_err(cm->dev, "%s: bat volt:%d Charge!\n", __func__, batt_uV);
	}
}

#ifndef VENDOR_KERNEL
static int cm_feed_watchdog(struct charger_manager *cm)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int err, i;

	if (!cm->desc->wdt_interval)
		return 0;

	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);

			continue;
		}
		val.intval = CM_FEED_WATCHDOG_CMD;
		err = power_supply_set_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
		power_supply_put(psy);
		if (err)
			return err;

	}

	return 0;
}
#endif //not defined VENDOR_KERNEL
static bool cm_manager_adjust_current(struct charger_manager *cm, int jeita_status)
{
	struct charger_desc *desc = cm->desc;
	int term_volt, target_cur;
	int vbat,ret;

	if (cm->charging_status != 0 &&
	    !(cm->charging_status & (CM_CHARGE_TEMP_OVERHEAT | CM_CHARGE_TEMP_COLD)))
		return true;

	if (jeita_status > desc->jeita_tab_size)
		jeita_status = desc->jeita_tab_size;

	if (jeita_status == 0 || jeita_status == desc->jeita_tab_size) {
		dev_warn(cm->dev,
			 "stop charging due to battery overheat or cold\n");
		try_charger_enable(cm, false);

		if (jeita_status == 0) {
			cm->charging_status &= ~CM_CHARGE_TEMP_OVERHEAT;
			cm->charging_status |= CM_CHARGE_TEMP_COLD;
		} else {
			cm->charging_status &= ~CM_CHARGE_TEMP_COLD;
			cm->charging_status |= CM_CHARGE_TEMP_OVERHEAT;
		}
		return false;
	}

	term_volt = desc->jeita_tab[jeita_status].term_volt;
	target_cur = desc->jeita_tab[jeita_status].current_ua;

	cm->desc->ir_comp.us = term_volt;
	cm->desc->ir_comp.us_lower_limit = term_volt;

	/*odm.hq.bsp.luowenjiang@huaqin.com 2021.12.30 modify for error full charge status start*/
	ret = get_vbat_now_uV(cm, &vbat);
	if (ret)
		dev_err(cm->dev, "get_vbat_now_uV error.\n");

	/*odm.hq.bsp.luowenjiang@huaqin.com 2021.12.30 modify for error full charge status start*/
	if (cm->desc->cp.cp_running && !cm_check_primary_charger_enabled(cm)) {
		dev_info(cm->dev, "cp target terminate voltage = %d, target current = %d\n",
			 term_volt, target_cur);
		cm->desc->cp.cp_target_ibat = target_cur;
		goto exit;
	}

	dev_info(cm->dev, "target terminate voltage = %d, target current = %d\n",
		 term_volt, target_cur);

	cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
				 SPRD_VOTE_TYPE_IBAT,
				 SPRD_VOTE_TYPE_IBAT_ID_JEITA,
				 SPRD_VOTE_CMD_MIN,
				 target_cur, cm);

/*odm.hq.bsp.luowenjiang@huaqin.com 2021.12.30 modify for error full charge status start*/
	if(vbat < term_volt) {
	cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
				 SPRD_VOTE_TYPE_CCCV,
				 SPRD_VOTE_TYPE_CCCV_ID_JEITA,
				 SPRD_VOTE_CMD_MIN,
				 term_volt, cm);
	dev_info(cm->dev, "target terminate voltage = %d, target current = %d\n",
		 term_volt, target_cur);
	} else {
		error_full_status = true;
		try_charger_enable(cm, false);
		dev_info(cm->dev,"stop_charge by error full status\n");
	}
/*odm.hq.bsp.luowenjiang@huaqin.com 2021.12.30 modify for error full charge status end*/

exit:
	cm->charging_status &= ~(CM_CHARGE_TEMP_OVERHEAT | CM_CHARGE_TEMP_COLD);
	return true;
}

static void cm_jeita_temp_goes_down(struct charger_desc *desc, int status,
				    int recovery_status, int *jeita_status)
{
	if (recovery_status == desc->jeita_tab_size) {
		if (*jeita_status >= recovery_status)
			*jeita_status = recovery_status;
		return;
	}

	if (desc->jeita_tab[recovery_status].temp > desc->jeita_tab[recovery_status].recovery_temp) {
		if (*jeita_status >= recovery_status)
			*jeita_status = recovery_status;
		return;
	}

	if (*jeita_status >= status)
	*jeita_status = status;
}

static void cm_jeita_temp_goes_up(struct charger_desc *desc, int status,
				  int recovery_status, int *jeita_status)
{
	if (recovery_status == desc->jeita_tab_size) {
		if (*jeita_status <= status)
		*jeita_status = status;
		return;
	}

	if (desc->jeita_tab[recovery_status].temp < desc->jeita_tab[recovery_status].recovery_temp) {
		if (*jeita_status <= recovery_status)
			*jeita_status = recovery_status;
		return;
	}

	if (*jeita_status <= status)
	*jeita_status = status;
}

static int cm_manager_get_jeita_status(struct charger_manager *cm, int cur_temp)
{
	struct charger_desc *desc = cm->desc;
	static int jeita_status;
	int i, temp_status, recovery_temp_status = -1;

	for (i = desc->jeita_tab_size - 1; i >= 0; i--) {
		if ((cur_temp >= desc->jeita_tab[i].temp && i > 0) ||
		    (cur_temp > desc->jeita_tab[i].temp && i == 0)) {
			break;
		}
	}

	temp_status = i + 1;

	if (temp_status == desc->jeita_tab_size) {
		jeita_status = desc->jeita_tab_size;
		goto out;
	} else if (temp_status == 0) {
		jeita_status = 0;
		goto out;
	}

	for (i = desc->jeita_tab_size - 1; i >= 0; i--) {
		if ((cur_temp >= desc->jeita_tab[i].recovery_temp && i > 0) ||
		    (cur_temp > desc->jeita_tab[i].recovery_temp && i == 0)) {
			break;
		}
	}

	recovery_temp_status = i + 1;

	/* temperature goes down */
	if (last_temp > cur_temp)
		cm_jeita_temp_goes_down(desc, temp_status, recovery_temp_status, &jeita_status);
	/* temperature goes up */
	else
		cm_jeita_temp_goes_up(desc, temp_status, recovery_temp_status, &jeita_status);

out:
	last_temp = cur_temp;
	//dev_info(cm->dev, "%s: jeita status:(%d) %d %d, temperature:%d, jeita_size:%d\n",
	//	 __func__, jeita_status, temp_status, recovery_temp_status,
	//	 cur_temp, desc->jeita_tab_size);

	return jeita_status;
}

static int cm_manager_jeita_current_monitor(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	static int last_jeita_status = -1, temp_up_trigger, temp_down_trigger;
	int cur_jeita_status;
	static bool is_normal = true;
	int vbat,ret;
	if (!desc->jeita_tab_size)
		return 0;

	if (!is_ext_pwr_online(cm)) {
		if (last_jeita_status != -1)
			last_jeita_status = -1;

		return 0;
	}

	if (desc->jeita_disabled) {
		if (last_jeita_status != cm->desc->force_jeita_status) {
			dev_info(cm->dev, "Disable jeita and force jeita state to force_jeita_status\n");
			last_jeita_status = cm->desc->force_jeita_status;
			desc->thm_info.thm_adjust_cur = -EINVAL;
			cm_manager_adjust_current(cm, last_jeita_status);
		}

		return 0;
	}

	cur_jeita_status = cm_manager_get_jeita_status(cm, desc->temperature);
	if(cur_jeita_status != last_jeita_status){
		dev_info(cm->dev, "current-last jeita status: %d-%d, current temperature: %d\n",
			cur_jeita_status, last_jeita_status, desc->temperature);
	}
	ret = get_vbat_now_uV(cm, &vbat);
	if (ret)
		dev_err(cm->dev, "get_vbat_now_uV error.\n");

	if(error_full_status && vbat < desc->jeita_tab[cur_jeita_status].term_volt)
		error_full_status = false;

	/*
	 * We should give a initial jeita status with adjusting the charging
	 * current when pluging in the cabel.
	 */
	if (last_jeita_status == -1) {
		is_normal = cm_manager_adjust_current(cm, cur_jeita_status);
		last_jeita_status = cur_jeita_status;
		goto out;
	}

	if (cur_jeita_status > last_jeita_status) {
		temp_down_trigger = 0;

		if (++temp_up_trigger > 2) {
			is_normal = cm_manager_adjust_current(cm,
							      cur_jeita_status);
			last_jeita_status = cur_jeita_status;
		}
	} else if (cur_jeita_status < last_jeita_status) {
		temp_up_trigger = 0;

		if (++temp_down_trigger > 2) {
			is_normal = cm_manager_adjust_current(cm,
							      cur_jeita_status);
			last_jeita_status = cur_jeita_status;
		}
	} else {
		temp_up_trigger = 0;
		temp_down_trigger = 0;
	}

out:
	if (!is_normal)
		return -EAGAIN;

	return 0;
}

/**
 * cm_get_target_status - Check current status and get next target status.
 * @cm: the Charger Manager representing the battery.
 */
static int cm_manager_step_charge_monitor(struct charger_manager *cm)
{
	int step_chg_cur = 0, step_chg_volt = 0, batt_uV = 0, ret = 0;
	static int bat_vol_shake_enter = 0;
	static int bat_vol_shake_exit = 0;
	int cur_jeita_status = 0;

	if (!cm->desc->jeita_tab_size)
		return 0;

	if (!is_ext_pwr_online(cm))
		return 0;

	cur_jeita_status = cm_manager_get_jeita_status(cm, cm->desc->temperature);

	if (cm->desc->jeita_disabled) {
		if (cur_jeita_status != cm->desc->force_jeita_status) {
			dev_info(cm->dev, "%s Disable jeita and force jeita state to status:%d\n",
					 __func__, cm->desc->force_jeita_status);
			cur_jeita_status = cm->desc->force_jeita_status;
		}
	}

	ret = get_vbat_now_uV(cm, &batt_uV);
	if (ret) {
		dev_err(cm->dev, "get_batt_ocV error.\n");
		return ret;
	}

	step_chg_cur = cm->desc->jeita_tab[cur_jeita_status].step_chg_cur;
	step_chg_volt = cm->desc->jeita_tab[cur_jeita_status].step_chg_volt;

	//dev_info(cm->dev, "jeita_status = %d batt_uV = %d step_chg_volt = %d\n",
	//	cur_jeita_status, batt_uV, step_chg_volt);

	if (batt_uV > step_chg_volt) {
		bat_vol_shake_exit = 0;
		bat_vol_shake_enter++;
		if (bat_vol_shake_enter >= CM_BAT_VOL_SHAKE_COUNT) {
			bat_vol_shake_enter = 0;
			g_bat_vol_shake = true;
		}
	} else if (g_bat_vol_shake && (batt_uV < CM_RESTORE_BATT_VOL_SHAKE)) {
		bat_vol_shake_enter = 0;
		bat_vol_shake_exit++;
		if (bat_vol_shake_exit >= CM_BAT_VOL_SHAKE_COUNT) {
			bat_vol_shake_exit = 0;
			g_bat_vol_shake = false;
		}
	} else {
		bat_vol_shake_exit = 0;
		bat_vol_shake_enter = 0;
	}


	if (g_bat_vol_shake)
		cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
				 SPRD_VOTE_TYPE_IBAT,
				 SPRD_VOTE_TYPE_IBAT_ID_JEITA,
				 SPRD_VOTE_CMD_MIN,
				 step_chg_cur, cm);
	else
		cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
				 SPRD_VOTE_TYPE_IBAT,
				 SPRD_VOTE_TYPE_IBAT_ID_JEITA,
				 SPRD_VOTE_CMD_MIN,
				 cm->desc->jeita_tab[cur_jeita_status].current_ua, cm);

	return ret;
}

#ifndef VENDOR_KERNEL
void cm_set_otg_switch_status(bool value)
{
#ifdef CONFIG_HQ_USB_TEMP_CHECK
	if(psc != NULL){
		printk(KERN_ERR "[%s]: otg switch[%d]\n", __func__, value);
		if(value){
			sc27xx_typec_set_enable(psc);
		}else{
			sc27xx_typec_set_sink(psc);
		}
	}
#endif
}
#endif //not defined VENDOR_KERNEL

static int cm_get_target_status(struct charger_manager *cm)
{
	int ret;

	/*
	 * Adjust the charging current according to current battery
	 * temperature jeita table.
	 */
	ret = cm_manager_jeita_current_monitor(cm);
	if (ret)
		dev_warn(cm->dev, "Errors orrurs when adjusting charging current\n");

	ret = cm_manager_step_charge_monitor(cm);
	if (ret)
		dev_warn(cm->dev,"Errors orrurs when adjusting step charging current\n");

	if (!is_ext_pwr_online(cm) || (!is_batt_present(cm) && !allow_charger_enable))
		return POWER_SUPPLY_STATUS_DISCHARGING;

	if (cm_check_thermal_status(cm))
		return POWER_SUPPLY_STATUS_NOT_CHARGING;

	if (cm->charging_status & (CM_CHARGE_TEMP_OVERHEAT | CM_CHARGE_TEMP_COLD)) {
		dev_warn(cm->dev, "battery overheat or cold is still abnormal\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	cm_check_charge_health(cm);
	if (cm->charging_status & CM_CHARGE_HEALTH_ABNORMAL) {
		dev_warn(cm->dev, "Charging health is still abnormal\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	cm_check_charge_voltage(cm);
	if (cm->charging_status & CM_CHARGE_VOLTAGE_ABNORMAL) {
		dev_warn(cm->dev, "Charging voltage is still abnormal\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

//add by HQ:zhaolianghua for remove time check in aging version
#ifndef AGING_BUILD
	check_charging_duration(cm);
	if (cm->charging_status & CM_CHARGE_DURATION_ABNORMAL) {
		dev_warn(cm->dev, "Charging duration is still abnormal\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}
#endif

	if(notify_code & CHG_BAT_ID_STATUS){
		dev_warn(cm->dev, "battery id is still abnormal\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	if(bat_ovp_flag == true){
		dev_warn(cm->dev, "battery ovp is still abnormal\n");
		return POWER_SUPPLY_STATUS_CHARGING;
	}
#ifdef WT_COMPILE_FACTORY_VERSION
	if (cm_battery_status & CM_ATO_CHARGE_STATUS){
		dev_warn(cm->dev, "ato charge still abnormal\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}
#endif /* WT_COMPILE_FACTORY_VERSION */

	if(charge_stop == 1 ||  runin_stop == 1){
		dev_warn(cm->dev, "set charge stop\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	if(is_ext_pwr_online(cm))
		fullbatt_vchk(&cm->fullbatt_vchk_work.work);

	if (is_full_charged(cm)){
		cm->charging_status &= ~CM_CHARGE_DURATION_ABNORMAL;
		notify_code &= ~CHG_BAT_TIMEOUT_STATUS;
		return POWER_SUPPLY_STATUS_FULL;
	}

	if(is_full_warm == true){
		dev_warn(cm->dev, "temp warm full\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

/*odm.hq.bsp.luowenjiang@huaqin.com 2021.12.30 modify for error full charge status start*/
	if(error_full_status == true) {
		dev_warn(cm->dev, "error full status \n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}
/*odm.hq.bsp.luowenjiang@huaqin.com 2021.12.30 modify for error full charge status start*/

	/* Charging is allowed. */
	return POWER_SUPPLY_STATUS_CHARGING;
}
#ifdef WT_COMPILE_FACTORY_VERSION
static void cm_ato_control_charge(struct charger_manager *cm, int fuel_cap)
{
    dev_info(cm->dev, "[%s] fuel_cap = %d\n", __func__, fuel_cap);
    if (fuel_cap >= 680) {
            set_batt_charge_status(ATO_SET, CM_ATO_CHARGE_STATUS, 0);
            dev_info(cm->dev, "[%s] cap >= 68 stop charger\n", __func__);
            try_charger_enable(cm, false);
            power_supply_changed(cm->charger_psy);
    } else if ((fuel_cap <= 650) && (cm_battery_status & CM_ATO_CHARGE_STATUS)) {
            set_batt_charge_status(ATO_SET, CM_ATO_CHARGE_STATUS, 1);
            dev_info(cm->dev, "[%s] cap <= 65 start charger\n", __func__);
            try_charger_enable(cm, true);
            power_supply_changed(cm->charger_psy);
    }
}
#endif /* WT_COMPILE_FACTORY_VERSION */
/**
 * _cm_monitor - Monitor the temperature and return true for exceptions.
 * @cm: the Charger Manager representing the battery.
 *
 * Returns true if there is an event to notify for the battery.
 * (True if the status of "emergency_stop" changes)
 */
static bool _cm_monitor(struct charger_manager *cm)
{
#ifdef VENDOR_KERNEL
	int i;
#else //not defined VENDOR_KERNEL
	int i, ret;
#endif //defined VENDOR_KERNEL

	static int last_target = -1;

	#ifdef CONFIG_HQ_USB_TEMP_CHECK
	int vbus;
	#endif
#ifndef VENDOR_KERNEL
	/* Feed the charger watchdog if necessary */
	ret = cm_feed_watchdog(cm);
	if (ret) {
		dev_warn(cm->dev, "Failed to feed charger watchdog\n");
		last_target = -1;
		return false;
	}
#endif //not defined VENDOR_KERNEL

	for (i = 0; i < cm->desc->num_charger_regulators; i++) {
		if (cm->desc->charger_regulators[i].externally_control) {
			dev_info(cm->dev, "Charger has been controlled externally, so no need monitoring\n");
			last_target = -1;
			return false;
		}
	}

	/*odm.hq.bsp.luowenjiang@huaqin.com 2021.12.30 modify for usb temp protection start*/
	#ifdef CONFIG_HQ_USB_TEMP_CHECK
	if (false == bbat_mode && (prj_name == 2 || prj_name == 3)) {
		ret = get_charger_voltage(cm, &vbus);
		if (ret)
			dev_warn(cm->dev, "get chg_vol error.\n");
		if(vbus >= 3000000 && !cm->vbus_status) {
			dev_info(cm->dev,"[ADC] vbus rised, start usb temp monitor\n");
			cm_wake_up_usbtemp_thread();
		}
	}
	#endif
	/*odm.hq.bsp.luowenjiang@huaqin.com 2021.12.30 modify for usb temp protection end*/

	cm->battery_status = cm_get_target_status(cm);

	if (cm->battery_status == POWER_SUPPLY_STATUS_CHARGING) {
		cm->emergency_stop = 0;
		cm->charging_status = 0;
		try_charger_enable(cm, true);

		if (!cm->desc->cp.cp_running && !cm_check_primary_charger_enabled(cm)
		    && !cm->desc->force_set_full && !is_full_warm) {
			dev_info(cm->dev, "%s, primary charger does not enable,enable it\n", __func__);
			cm_primary_charger_enable(cm, true);
		}

		if (cm_is_need_start_cp(cm))
			cm_start_cp_state_machine(cm, true);
		else if (!cm->desc->cp.cp_running && cm_is_need_start_fixed_fchg(cm))
			cm_start_fixed_fchg(cm, true);
	} else {
		try_charger_enable(cm, false);
	}

	if (last_target != cm->battery_status) {
		last_target = cm->battery_status;
		power_supply_changed(cm->charger_psy);
	}

	//dev_info(cm->dev, "battery_status %d, charging_status %d\n",
	//	 cm->battery_status, cm->charging_status);
	return (cm->battery_status == POWER_SUPPLY_STATUS_NOT_CHARGING);
}

/**
 * cm_monitor - Monitor every battery.
 *
 * Returns true if there is an event to notify from any of the batteries.
 * (True if the status of "emergency_stop" changes)
 */
static bool cm_monitor(void)
{
	bool stop = false;
	struct charger_manager *cm;

	mutex_lock(&cm_list_mtx);

	list_for_each_entry(cm, &cm_list, entry) {
		if (_cm_monitor(cm))
			stop = true;
	}

	mutex_unlock(&cm_list_mtx);

	return stop;
}

/**
 * _setup_polling - Setup the next instance of polling.
 * @work: work_struct of the function _setup_polling.
 */
static void _setup_polling(struct work_struct *work)
{
	unsigned long min = ULONG_MAX;
	struct charger_manager *cm;
	bool keep_polling = false;
	unsigned long _next_polling;

	mutex_lock(&cm_list_mtx);

	list_for_each_entry(cm, &cm_list, entry) {
		if (is_polling_required(cm) && cm->desc->polling_interval_ms) {
			keep_polling = true;

			if (min > cm->desc->polling_interval_ms)
				min = cm->desc->polling_interval_ms;
		}
	}

	polling_jiffy = msecs_to_jiffies(min);
	if (polling_jiffy <= CM_JIFFIES_SMALL)
		polling_jiffy = CM_JIFFIES_SMALL + 1;

	if (!keep_polling)
		polling_jiffy = ULONG_MAX;
	if (polling_jiffy == ULONG_MAX)
		goto out;

	WARN(cm_wq == NULL, "charger-manager: workqueue not initialized"
			    ". try it later. %s\n", __func__);

	/*
	 * Use mod_delayed_work() iff the next polling interval should
	 * occur before the currently scheduled one.  If @cm_monitor_work
	 * isn't active, the end result is the same, so no need to worry
	 * about stale @next_polling.
	 */
	_next_polling = jiffies + polling_jiffy;

	if (time_before(_next_polling, next_polling)) {
		mod_delayed_work(cm_wq, &cm_monitor_work, polling_jiffy);
		next_polling = _next_polling;
	} else {
		if (queue_delayed_work(cm_wq, &cm_monitor_work, polling_jiffy))
			next_polling = _next_polling;
	}
out:
	mutex_unlock(&cm_list_mtx);
}
static DECLARE_WORK(setup_polling, _setup_polling);

/**
 * cm_monitor_poller - The Monitor / Poller.
 * @work: work_struct of the function cm_monitor_poller
 *
 * During non-suspended state, cm_monitor_poller is used to poll and monitor
 * the batteries.
 */
static void cm_monitor_poller(struct work_struct *work)
{
	cm_monitor();
	schedule_work(&setup_polling);
}

/**
 * fullbatt_handler - Event handler for CM_EVENT_BATT_FULL
 * @cm: the Charger Manager representing the battery.
 */
static void fullbatt_handler(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;

	if (!desc->fullbatt_vchkdrop_uV || !desc->fullbatt_vchkdrop_ms)
		goto out;

	if (cm_suspended)
		device_set_wakeup_capable(cm->dev, true);

	mod_delayed_work(cm_wq, &cm->fullbatt_vchk_work,
			 msecs_to_jiffies(desc->fullbatt_vchkdrop_ms));
	cm->fullbatt_vchk_jiffies_at = jiffies + msecs_to_jiffies(
				       desc->fullbatt_vchkdrop_ms);

	if (cm->fullbatt_vchk_jiffies_at == 0)
		cm->fullbatt_vchk_jiffies_at = 1;

out:
	dev_info(cm->dev, "EVENT_HANDLE: Battery Fully Charged\n");
}

/**
 * battout_handler - Event handler for CM_EVENT_BATT_OUT
 * @cm: the Charger Manager representing the battery.
 */
static void battout_handler(struct charger_manager *cm)
{
	if (cm_suspended)
		device_set_wakeup_capable(cm->dev, true);

	if (!is_batt_present(cm)) {
		dev_emerg(cm->dev, "Battery Pulled Out!\n");
		try_charger_enable(cm, false);
	} else {
		dev_emerg(cm->dev, "Battery Pulled in!\n");

		if (cm->charging_status) {
			dev_emerg(cm->dev, "Charger status abnormal, stop charge!\n");
			try_charger_enable(cm, false);
		} else {
			try_charger_enable(cm, true);
		}
	}
}

static bool cm_charger_is_support_fchg(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret, i;

	if (!desc->psy_fast_charger_stat)
		return false;

	for (i = 0; desc->psy_fast_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_fast_charger_stat[i]);

		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_fast_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_USB_TYPE, &val);
		power_supply_put(psy);
		if (!ret) {
			if (val.intval == POWER_SUPPLY_USB_TYPE_PD ||
			    val.intval == POWER_SUPPLY_USB_TYPE_PD_PPS) {
				mutex_lock(&cm->desc->charger_type_mtx);
				desc->is_fast_charge = true;
				g_is_fast_charge = true;
				if (!desc->psy_cp_stat &&
				    val.intval == POWER_SUPPLY_USB_TYPE_PD_PPS)
					val.intval = POWER_SUPPLY_USB_TYPE_PD;
				desc->fast_charger_type = val.intval;
				desc->charger_type = val.intval;
				mutex_unlock(&cm->desc->charger_type_mtx);
				return true;
			} else {
				return false;
			}
		}
	}

	return false;
}

static void cm_charger_int_handler(struct charger_manager *cm)
{
	dev_info(cm->dev, "%s\n", __func__);
	cm->desc->cm_check_int = true;
}


/**
 * fast_charge_handler - Event handler for CM_EVENT_FAST_CHARGE
 * @cm: the Charger Manager representing the battery.
 */
static void fast_charge_handler(struct charger_manager *cm)
{
	bool ext_pwr_online;

	if (cm_suspended)
		device_set_wakeup_capable(cm->dev, true);

	cm_charger_is_support_fchg(cm);
	ext_pwr_online = is_ext_pwr_online(cm);

	dev_info(cm->dev, "%s, fast_charger_type = %d, cp_running = %d, "
		 "charger_enabled = %d, ext_pwr_online = %d\n",
		 __func__, cm->desc->fast_charger_type, cm->desc->cp.cp_running,
		 cm->charger_enabled, ext_pwr_online);

	if (!ext_pwr_online)
		return;

	if (cm->desc->is_fast_charge && !cm->desc->enable_fast_charge)
		cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
					   CM_CHARGE_INFO_INPUT_LIMIT));

	/*
	 * Once the fast charge is identified, it is necessary to open
	 * the charge in the first time to avoid the fast charge to boost
	 * the voltage in the next charging cycle, especially the SFCP
	 * fast charge.
	 */
	if (cm->desc->fast_charger_type == POWER_SUPPLY_USB_TYPE_PD &&
	    cm->charger_enabled)
		_cm_monitor(cm);

	if (cm->desc->fast_charger_type == POWER_SUPPLY_USB_CHARGER_TYPE_PD_PPS &&
	    !cm->desc->cp.cp_running && cm->charger_enabled) {
		cm_cp_control_switch(cm, true);
		schedule_delayed_work(&cm_monitor_work, 0);
	}
}

/**
 * misc_event_handler - Handler for other events
 * @cm: the Charger Manager representing the battery.
 * @type: the Charger Manager representing the battery.
 */
static void misc_event_handler(struct charger_manager *cm, enum cm_event_types type)
{
	int ret;

	if (cm_suspended)
		device_set_wakeup_capable(cm->dev, true);

	if (is_ext_pwr_online(cm)) {
		bat_ovp_flag = false;
		if (is_ext_usb_pwr_online(cm) && type == CM_EVENT_WL_CHG_START_STOP) {
			dev_warn(cm->dev, "usb charging, does not need start wl charge\n");
			return;
		} else if (is_ext_usb_pwr_online(cm)) {
			if (cm->desc->wl_charge_en) {
				try_wireless_charger_enable(cm, false);
				try_charger_enable(cm, false);
				cm->desc->wl_charge_en = false;
			}

			if (!cm->desc->is_fast_charge) {
				ret = get_usb_charger_type(cm, &cm->desc->charger_type);
				if (ret)
					dev_warn(cm->dev, "Fail to get usb charger type, ret = %d", ret);
			}

			cm->desc->usb_charge_en = true;
		} else {
			if (cm->desc->usb_charge_en) {
				try_charger_enable(cm, false);
				cm->desc->is_fast_charge = false;
				g_is_fast_charge = false;
				cm->desc->enable_fast_charge = false;
				cm->desc->fast_charge_enable_count = 0;
				cm->desc->fast_charge_disable_count = 0;
				cm->desc->fixed_fchg_running = false;
				cm->desc->wait_vbus_stable = false;
				cm->desc->cp.cp_running = false;
				cm->desc->fast_charger_type = 0;
				cm->desc->cp.cp_target_vbus = 0;
				cm->desc->usb_charge_en = false;
				cm->desc->charger_type = 0;
			}

			ret = get_wireless_charger_type(cm, &cm->desc->charger_type);
			if (ret)
				dev_warn(cm->dev, "Fail to get wl charger type, ret = %d\n", ret);

			try_wireless_charger_enable(cm, true);
			cm->desc->wl_charge_en = true;
		}

		cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
					   CM_CHARGE_INFO_INPUT_LIMIT |
					   CM_CHARGE_INFO_JEITA_LIMIT));

	} else {
		cm->charging_status &= ~CM_CHARGE_DURATION_ABNORMAL;
		notify_code &= ~CHG_BAT_TIMEOUT_STATUS;

		try_wireless_charger_enable(cm, false);
		try_charger_enable(cm, false);
		cancel_delayed_work_sync(&cm_monitor_work);
		cancel_delayed_work_sync(&cm->cp_work);
		_cm_monitor(cm);

		cm->desc->is_fast_charge = false;
		g_is_fast_charge = false;
		cm->desc->ir_comp.ir_compensation_en = false;
		cm->desc->enable_fast_charge = false;
		i_ovp = 0;
		cm->desc->fast_charge_enable_count = 0;
		cm->desc->fast_charge_disable_count = 0;
		cm->desc->fixed_fchg_running = false;
		cm->desc->cp.cp_running = false;
		cm->desc->cm_check_int = false;
		cm->desc->fast_charger_type = 0;
		cm->desc->charger_type = 0;
		cm->desc->cp.cp_target_vbus = 0;
		cm->desc->force_set_full = false;
		cm->emergency_stop = 0;
		cm->charging_status = 0;
		cm->desc->thm_info.thm_adjust_cur = -EINVAL;
		cm->desc->thm_info.thm_pwr = 0;
		cm->desc->thm_info.adapter_default_charge_vol = 5;
		cm->desc->wl_charge_en = 0;
		cm->desc->usb_charge_en = 0;
		cm->cm_charge_vote->vote(cm->cm_charge_vote, false,
					 SPRD_VOTE_TYPE_ALL, 0, 0, 0, cm);
		is_full_warm = false;
	}

	cm_update_charger_type_status(cm);

	if (cm->desc->force_set_full)
		cm->desc->force_set_full = false;

	if(is_full_warm)
		is_full_warm = false;

	charge_stop = 0;

	if (is_polling_required(cm) && cm->desc->polling_interval_ms) {
		_cm_monitor(cm);
		schedule_work(&setup_polling);
	}

	power_supply_changed(cm->charger_psy);
}

static void cm_get_charging_status(struct charger_manager *cm, int *status)
{
	if (is_charging(cm)) {
		cm->battery_status = POWER_SUPPLY_STATUS_CHARGING;
	} else if (is_ext_pwr_online(cm)) {
		if (is_full_charged(cm) || bat_ovp_flag)
			cm->battery_status = POWER_SUPPLY_STATUS_FULL;
		else if(notify_code & CHG_BAT_ID_STATUS)
				cm->battery_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		else if(cm->charging_status & (CM_CHARGE_TEMP_OVERHEAT | CM_CHARGE_TEMP_COLD) || is_full_warm)
				cm->battery_status = POWER_SUPPLY_STATUS_CHARGING;
		else
			cm->battery_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	} else {
		cm->battery_status = POWER_SUPPLY_STATUS_DISCHARGING;
	}

	*status = cm->battery_status;
}

static void cm_get_charging_health_status(struct charger_manager *cm, int *status)
{
	if (cm->emergency_stop == CM_EVENT_BATT_OVERHEAT ||
	    (cm->charging_status & CM_CHARGE_TEMP_OVERHEAT))
		*status = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (cm->emergency_stop == CM_EVENT_BATT_COLD ||
		 (cm->charging_status & CM_CHARGE_TEMP_COLD))
		*status = POWER_SUPPLY_HEALTH_COLD;
	else if (cm->charging_status & CM_CHARGE_VOLTAGE_ABNORMAL)
		*status = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	else
		*status = POWER_SUPPLY_HEALTH_GOOD;
}

static int cm_get_battery_technology(struct charger_manager *cm, union power_supply_propval *val)
{
	struct power_supply *fuel_gauge = NULL;
	int ret;

	val->intval = POWER_SUPPLY_TECHNOLOGY_UNKNOWN;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_TECHNOLOGY, val);
	power_supply_put(fuel_gauge);

	return ret;
}

static void cm_get_uisoc(struct charger_manager *cm, int *uisoc)
{
	if (!is_batt_present(cm)) {
		/* There is no battery. Assume 100% */
		*uisoc = 100;
		return;
	}

	*uisoc = DIV_ROUND_CLOSEST(cm->desc->cap, 10);
	if (*uisoc > 100)
		*uisoc = 100;
	else if (*uisoc < 0)
		*uisoc = 0;
}

static int cm_get_capacity_level(struct charger_manager *cm)
{
	int level = POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;
	int uisoc, ocv_uv = 0;

	if (!is_batt_present(cm)) {
		/* There is no battery. Assume 100% */
		level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		return level;
	}

	uisoc = DIV_ROUND_CLOSEST(cm->desc->cap, 10);

	if (uisoc >= CM_CAPACITY_LEVEL_FULL)
		level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	else if (uisoc > CM_CAPACITY_LEVEL_NORMAL)
		level = POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
	else if (uisoc > CM_CAPACITY_LEVEL_LOW)
		level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	else if (uisoc > CM_CAPACITY_LEVEL_CRITICAL)
		level = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	else
		level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;


	if (get_batt_ocv(cm, &ocv_uv)) {
		dev_err(cm->dev, "%s, get_batt_ocV error.\n", __func__);
		return level;
	}

	if (level == POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL && is_charging(cm) &&
	    ocv_uv > CM_CAPACITY_LEVEL_CRITICAL_VOLTAGE)
		level = POWER_SUPPLY_CAPACITY_LEVEL_LOW;

	return level;
}

static int cm_get_charge_full_design(struct charger_manager *cm, union power_supply_propval *val)
{
	struct power_supply *fuel_gauge = NULL;
	int ret;

	val->intval = 0;
	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN, val);
	power_supply_put(fuel_gauge);

	return ret;
}

static int cm_get_charge_now(struct charger_manager *cm, int *charge_now)
{
	int total_uah;
	int ret;

	ret = get_batt_total_cap(cm, &total_uah);
	if (ret) {
		dev_err(cm->dev, "failed to get total uah.\n");
		return ret;
	}

	*charge_now = total_uah * cm->desc->cap / CM_CAP_FULL_PERCENT;

	return ret;
}

static int cm_get_charge_counter(struct charger_manager *cm, int *charge_counter)
{
	int ret;

	*charge_counter = 0;
	ret = cm_get_charge_now(cm, charge_counter);

	if (*charge_counter <= 0) {
		*charge_counter = 1;
		ret = 0;
	}

	return ret;
}

static int cm_get_charge_control_limit(struct charger_manager *cm,
				       union power_supply_propval *val)
{
	struct power_supply *psy = NULL;
	int i, ret = 0;

	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, val);
		power_supply_put(psy);
		if (!ret) {
			if (cm->desc->enable_fast_charge && cm->desc->psy_charger_stat[1])
				val->intval *= 2;

			break;
		}

		ret = power_supply_get_property(psy,
						POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
						val);
		if (!ret)
			break;
	}

	return ret;
}

static int cm_get_charge_full_uah(struct charger_manager *cm, union power_supply_propval *val)
{
	return cm_get_charge_full_design(cm, val);
}

static int cm_get_time_to_full_now(struct charger_manager *cm, int *time)
{
	unsigned int total_cap = 0;
	int chg_cur = 0;
	int ret;

	ret = get_constant_charge_current(cm, &chg_cur);
	if (ret) {
		dev_err(cm->dev, "get chg_cur error.\n");
		return ret;
	}

	chg_cur = chg_cur / 1000;

	ret = get_batt_total_cap(cm, &total_cap);
	if (ret) {
		dev_err(cm->dev, "failed to get total cap.\n");
		return ret;
	}

	total_cap = total_cap / 1000;

	*time = ((1000 - cm->desc->cap) * total_cap / 1000) * 3600 / chg_cur;

	if (*time <= 0)
		*time = 1;

	return ret;
}

static void cm_get_current_max(struct charger_manager *cm, int *current_max)
{
	int ret;

	*current_max = 0;

	if (cm->desc->cp.cp_running) {
		*current_max = cm->desc->cp.cp_target_ibat;
		return;
	}

	ret = get_constant_charge_current(cm, current_max);
	if (ret)
		dev_err(cm->dev, "Fail to get current max\n");
}

static void cm_get_voltage_max(struct charger_manager *cm, int *voltage_max)
{
	*voltage_max = cm->desc->constant_charge_voltage_max_uv;
}

static void cm_set_charge_control_limit(struct charger_manager *cm, int power)
{
	dev_info(cm->dev, "thermal set charge power limit, thm_pwr = %dmW\n", power);
	if (power > 18000)
		power = 18000;
	cm->desc->thm_info.thm_pwr = power;
	cm_update_charge_info(cm, CM_CHARGE_INFO_THERMAL_LIMIT);

	if (cm->desc->cp.cp_running)
		cm_check_target_ibus(cm);
}

static int cm_set_voltage_max_design(struct charger_manager *cm, int voltage_max)
{
	int ret;

	ret = cm_set_basp_max_volt(cm, voltage_max);
	if (ret)
		return ret;

	if (cm_init_basp_parameter(cm)) {
		if (cm->cm_charge_vote && cm->cm_charge_vote->vote)
			cm_update_charge_info(cm, CM_CHARGE_INFO_JEITA_LIMIT);
	}

	return ret;
}

static int wireless_get_property(struct power_supply *psy, enum power_supply_property
				 psp, union power_supply_propval *val)
{
	int ret = 0;
	struct wireless_data *data = container_of(psy->desc, struct  wireless_data, psd);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = data->WIRELESS_ONLINE;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ac_get_property(struct power_supply *psy, enum power_supply_property psp,
			   union power_supply_propval *val)
{
	int ret = 0;
	struct ac_data *data = container_of(psy->desc, struct ac_data, psd);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = data->AC_ONLINE;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int usb_get_property(struct power_supply *psy, enum power_supply_property psp,
			    union power_supply_propval *val)
{
	int ret = 0;
	struct usb_data *data = container_of(psy->desc, struct usb_data, psd);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = data->USB_ONLINE;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		/* no need, do nothing! */
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static bool cm_add_battery_psy_property(struct charger_manager *cm, enum power_supply_property psp)
{
	u32 i;

	for (i = 0; i < cm->charger_psy_desc.num_properties; i++)
		if (cm->charger_psy_desc.properties[i] == psp)
			break;

	if (i == cm->charger_psy_desc.num_properties) {
		cm->charger_psy_desc.properties[cm->charger_psy_desc.num_properties++] = psp;
		return true;
	}
	return false;
}

static int charger_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct charger_manager *cm = power_supply_get_drvdata(psy);
	int ret = 0;

	if (!cm)
		return -ENOMEM;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
#ifdef VENDOR_KERNEL
		if (is_charging(cm)) {
			cm->battery_status = POWER_SUPPLY_STATUS_CHARGING;
		} else if (is_ext_pwr_online(cm)) {
			if (is_full_charged(cm))
				cm->battery_status = POWER_SUPPLY_STATUS_FULL;
			else if(notify_code & (CHG_BAT_HIG_TEMP_STATUS | CHG_BAT_LOW_TEMP_STATUS))
				cm->battery_status = POWER_SUPPLY_STATUS_CHARGING;
			else
				cm->battery_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		} else {
			cm->battery_status = POWER_SUPPLY_STATUS_DISCHARGING;
		}
#endif //defined VENDOR_KERNEL
		cm_get_charging_status(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		cm_get_charging_health_status(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = is_batt_present(cm);
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		ret = get_vbat_avg_uV(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = get_vbat_now_uV(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_CURRENT_AVG:
		ret = get_ibat_avg_uA(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = get_ibat_now_uA(cm, &val->intval);
#ifdef VENDOR_KERNEL
		val->intval *= -1;
#endif //defined VENDOR_KERNEL
		break;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		ret = cm_get_battery_technology(cm, val);
		break;

	case POWER_SUPPLY_PROP_TEMP:
		val->intval = cm->desc->temperature;
		break;

	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		return cm_get_battery_temperature(cm, &val->intval);

	case POWER_SUPPLY_PROP_CAPACITY:
		cm_get_uisoc(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = cm_get_capacity_level(cm);
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = is_ext_pwr_online(cm);
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = cm_get_charge_full_uah(cm, val);
		break;

	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret = cm_get_charge_now(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = get_constant_charge_current(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = get_input_current_limit(cm,  &val->intval);
		break;

	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		ret = cm_get_charge_counter(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		ret = cm_get_charge_control_limit(cm, val);
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		ret = cm_get_charge_full_design(cm, val);
		break;

	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		ret = cm_get_time_to_full_now(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_USB_TYPE:
		ret = cm_get_usb_type(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		ret = cm_get_charge_cycle(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		ret = cm_get_basp_max_volt(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_CURRENT_MAX:
		cm_get_current_max(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		cm_get_voltage_max(cm, &val->intval);
		break;

	default:
		return -EINVAL;
	}

	return ret;
}

static int charger_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct charger_manager *cm = power_supply_get_drvdata(psy);
	int ret = 0;

	if (!cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		cm->desc->thm_info.thm_pwr = 0;
		cm->desc->thm_info.thm_adjust_cur = val->intval;
		cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBAT,
					 SPRD_VOTE_TYPE_IBAT_ID_CONSTANT_CHARGE_CURRENT,
					 SPRD_VOTE_CMD_MIN,
					 val->intval, cm);
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		/* The ChargerIC with linear charging cannot set Ibus, only Ibat. */
		if (cm->desc->thm_info.need_calib_charge_lmt) {
			cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBAT,
					 SPRD_VOTE_TYPE_IBAT_ID_INPUT_CURRENT_LIMIT,
					 SPRD_VOTE_CMD_MIN,
					 val->intval, cm);
			break;
		}

		cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBUS,
					 SPRD_VOTE_TYPE_IBUS_ID_INPUT_CURRENT_LIMIT,
					 SPRD_VOTE_CMD_MIN,
					 val->intval, cm);
		break;

	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		cm_set_charge_control_limit(cm, val->intval);
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		ret = cm_set_voltage_max_design(cm, val->intval);
		break;

	default:
		ret = -EINVAL;
	}

	return ret;
}

extern void otg_switch_mode(int value);

static int usb_set_property(struct power_supply *psy,
                     enum power_supply_property psp,
                     const union power_supply_propval *val)
{
        int ret = 0;
		if (NULL == psy || NULL == val){
			dev_info(&psy->dev, "[%s] psy value err \n",__func__);
			return -ENODEV;
		}
        switch (psp) {
        case POWER_SUPPLY_PROP_STATUS:
				dev_info(&psy->dev, "------otg_switch_mode----\n");
#ifndef VENDOR_KERNEL
				otg_switch_state = val->intval;
				if(prj_name == 2 || prj_name == 3){ //nicky typec
					if(val->intval){
						cm_set_otg_switch_status(true);
					}else{
						cm_set_otg_switch_status(false);
					}
				}else
#endif //not defined VENDOR_KERNEL
				{
					otg_switch_mode(val->intval); //nico micro usb
				}
                break;

#ifndef VENDOR_KERNEL
		case POWER_SUPPLY_PROP_MANUFACTURER:
				dev_err(&psy->dev, "to set UART value = [%d].\n", val->intval);
				if (val->intval) {
					if (sprd_pin_set_gpio("gpio_70_f3") == 0)
						pull_gpio(70,0);
					if (sprd_pin_set_gpio("gpio_71_f3") == 0)
						pull_gpio(71,0);
				} else {
					gpio_free(70);
					gpio_free(71);
					sprd_pin_set_gpio("gpio_70_f3");
					sprd_pin_set_gpio("gpio_71_f3");
				}
                break;
#endif //not defined VENDOR_KERNEL
        default:
                ret = -EINVAL;
        }

        return ret;
}

static int usb_property_is_writeable(struct power_supply *psy,
                                         enum power_supply_property psp)
{
	int ret = -1;

	switch (psp) {
        case POWER_SUPPLY_PROP_STATUS:
#ifndef VENDOR_KERNEL
        case POWER_SUPPLY_PROP_MANUFACTURER:
#endif //not defined VENDOR_KERNEL
			ret = 1;
			break;

        default:
			ret = 0;
	}

	return ret;
}

static int charger_property_is_writeable(struct power_supply *psy, enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}
#define NUM_CHARGER_PSY_OPTIONAL	(4)

static enum power_supply_property wireless_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,  // otg_switch
#ifndef VENDOR_KERNEL
	POWER_SUPPLY_PROP_MANUFACTURER,
#endif //not defined VENDOR_KERNEL
};

static enum power_supply_property default_charger_props[] = {
	/* Guaranteed to provide */
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_AMBIENT,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	/*
	 * Optional properties are:
	 * POWER_SUPPLY_PROP_CHARGE_NOW,
	 */
};

/* wireless_data initialization */
static struct wireless_data wireless_main = {
	.psd = {
		.name = "wireless",
		.type =	POWER_SUPPLY_TYPE_WIRELESS,
		.properties = wireless_props,
		.num_properties = ARRAY_SIZE(wireless_props),
		.get_property = wireless_get_property,
	},
	.WIRELESS_ONLINE = 0,
};

/* ac_data initialization */
static struct ac_data ac_main = {
	.psd = {
		.name = "ac",
		.type = POWER_SUPPLY_TYPE_MAINS,
		.properties = ac_props,
		.num_properties = ARRAY_SIZE(ac_props),
		.get_property = ac_get_property,
	},
	.AC_ONLINE = 0,
};

/* usb_data initialization */
static struct usb_data usb_main = {
	.psd = {
		.name = "usb",
		.type = POWER_SUPPLY_TYPE_USB,
		.properties = usb_props,
		.num_properties = ARRAY_SIZE(usb_props),
		.get_property = usb_get_property,
		.set_property = usb_set_property,
		.property_is_writeable  = usb_property_is_writeable,
	},
	.USB_ONLINE = 0,
};

static enum power_supply_usb_type default_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};

static const struct power_supply_desc psy_default = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = default_charger_props,
	.num_properties = ARRAY_SIZE(default_charger_props),
	.get_property = charger_get_property,
	.set_property = charger_set_property,
	.property_is_writeable	= charger_property_is_writeable,
	.usb_types		= default_usb_types,
	.num_usb_types		= ARRAY_SIZE(default_usb_types),
	.no_thermal = false,
};

static void cm_update_charger_type_status(struct charger_manager *cm)
{

	if (is_ext_usb_pwr_online(cm)) {
		pr_err("need for q work  type=%d\n", cm->desc->charger_type);
		switch (cm->desc->charger_type) {
		case POWER_SUPPLY_USB_CHARGER_TYPE_DCP:
		case POWER_SUPPLY_USB_CHARGER_TYPE_PD:
		case POWER_SUPPLY_USB_CHARGER_TYPE_PD_PPS:
		case POWER_SUPPLY_USB_CHARGER_TYPE_SFCP_1P0:
		case POWER_SUPPLY_USB_CHARGER_TYPE_SFCP_2P0:
			wireless_main.WIRELESS_ONLINE = 0;
			usb_main.USB_ONLINE = 0;
			ac_main.AC_ONLINE = 1;
			break;
		default:
			wireless_main.WIRELESS_ONLINE = 0;
			ac_main.AC_ONLINE = 0;
			usb_main.USB_ONLINE = 1;
#ifdef VENDOR_KERNEL
			if (cm->desc->charger_type == POWER_SUPPLY_CHARGER_TYPE_UNKNOWN) {
				pr_err("need for q work\r\n");
				wireless_main.WIRELESS_ONLINE = 0;
				usb_main.USB_ONLINE = 0;
				ac_main.AC_ONLINE = 1;
			}
#else //not defined VENDOR_KERNEL
			/*odm.hq.bsp modify for apple 5v2.4a oppo 5v4a charger start*/
			if ((prj_name == 2 || prj_name == 3) && cm->desc->charger_type == POWER_SUPPLY_CHARGER_TYPE_UNKNOWN) {
				pr_err("need for q work\r\n");
				wireless_main.WIRELESS_ONLINE = 0;
				usb_main.USB_ONLINE = 0;
				ac_main.AC_ONLINE = 1;
				queue_delayed_work(system_power_efficient_wq,
					&cm->get_charger_type_work, HZ/5);
			}
			/*odm.hq.bsp modify for apple 5v2.4a oppo 5v4a charger end*/
#endif //not defined VENDOR_KERNEL
			break;
		}
	} else if (is_ext_wl_pwr_online(cm)) {
		wireless_main.WIRELESS_ONLINE = 1;
		ac_main.AC_ONLINE = 0;
		usb_main.USB_ONLINE = 0;
	} else {
		wireless_main.WIRELESS_ONLINE = 0;
		ac_main.AC_ONLINE = 0;
		usb_main.USB_ONLINE = 0;
	}
}

/**
 * cm_setup_timer - For in-suspend monitoring setup wakeup alarm
 *		    for suspend_again.
 *
 * Returns true if the alarm is set for Charger Manager to use.
 * Returns false if
 *	cm_setup_timer fails to set an alarm,
 *	cm_setup_timer does not need to set an alarm for Charger Manager,
 *	or an alarm previously configured is to be used.
 */
static bool cm_setup_timer(void)
{
	struct charger_manager *cm;
	unsigned int wakeup_ms = UINT_MAX;
	int timer_req = 0;

	if (time_after(next_polling, jiffies))
		CM_MIN_VALID(wakeup_ms,
			jiffies_to_msecs(next_polling - jiffies));

	mutex_lock(&cm_list_mtx);
	list_for_each_entry(cm, &cm_list, entry) {
		unsigned int fbchk_ms = 0;

		/* fullbatt_vchk is required. setup timer for that */
		if (cm->fullbatt_vchk_jiffies_at) {
			fbchk_ms = jiffies_to_msecs(cm->fullbatt_vchk_jiffies_at
						    - jiffies);
			if (time_is_before_eq_jiffies(
				cm->fullbatt_vchk_jiffies_at) ||
				msecs_to_jiffies(fbchk_ms) < CM_JIFFIES_SMALL) {
				fullbatt_vchk(&cm->fullbatt_vchk_work.work);
				fbchk_ms = 0;
			}
		}
		CM_MIN_VALID(wakeup_ms, fbchk_ms);

		/* Skip if polling is not required for this CM */
		if (!is_polling_required(cm) && !cm->emergency_stop)
			continue;
		timer_req++;
		if (cm->desc->polling_interval_ms == 0)
			continue;
		if (cm->desc->ir_comp.ir_compensation_en)
			CM_MIN_VALID(wakeup_ms, CM_IR_COMPENSATION_TIME * 1000);
		else
			CM_MIN_VALID(wakeup_ms, cm->desc->polling_interval_ms);
	}
	mutex_unlock(&cm_list_mtx);

	if (timer_req && cm_timer) {
		ktime_t now, add;

		/*
		 * Set alarm with the polling interval (wakeup_ms)
		 * The alarm time should be NOW + CM_RTC_SMALL or later.
		 */
		if (wakeup_ms == UINT_MAX ||
			wakeup_ms < CM_RTC_SMALL * MSEC_PER_SEC)
			wakeup_ms = 2 * CM_RTC_SMALL * MSEC_PER_SEC;

		pr_info("Charger Manager wakeup timer: %u ms\n", wakeup_ms);

		now = ktime_get_boottime();
		add = ktime_set(wakeup_ms / MSEC_PER_SEC,
				(wakeup_ms % MSEC_PER_SEC) * NSEC_PER_MSEC);
		alarm_start(cm_timer, ktime_add(now, add));

		cm_suspend_duration_ms = wakeup_ms;

		return true;
	}
	return false;
}

/**
 * charger_extcon_notifier - receive the state of charger cable
 *			when registered cable is attached or detached.
 *
 * @self: the notifier block of the charger_extcon_notifier.
 * @event: the cable state.
 * @ptr: the data pointer of notifier block.
 */
static int charger_extcon_notifier(struct notifier_block *self, unsigned long event, void *ptr)
{
	struct charger_cable *cable =
		container_of(self, struct charger_cable, nb);

	/*
	 * The newly state of charger cable.
	 * If cable is attached, cable->attached is true.
	 */
	cable->attached = event;

	/*
	 * Setup monitoring to check battery state
	 * when charger cable is attached.
	 */
	if (cable->attached && is_polling_required(cable->cm)) {
		cancel_work_sync(&setup_polling);
		schedule_work(&setup_polling);
	}

	return NOTIFY_DONE;
}

/**
 * charger_extcon_init - register external connector to use it
 *			as the charger cable
 *
 * @cm: the Charger Manager representing the battery.
 * @cable: the Charger cable representing the external connector.
 */
static int charger_extcon_init(struct charger_manager *cm, struct charger_cable *cable)
{
	int ret;

	/*
	 * Charger manager use Extcon framework to identify
	 * the charger cable among various external connector
	 * cable (e.g., TA, USB, MHL, Dock).
	 */
	cable->nb.notifier_call = charger_extcon_notifier;
	ret = devm_extcon_register_notifier(cm->dev, cable->extcon_dev,
					    EXTCON_USB, &cable->nb);
	if (ret < 0)
		dev_err(cm->dev, "Cannot register extcon_dev for (cable: %s)\n",
			cable->name);

	return ret;
}

/**
 * charger_manager_register_extcon - Register extcon device to receive state
 *				     of charger cable.
 * @cm: the Charger Manager representing the battery.
 *
 * This function support EXTCON(External Connector) subsystem to detect the
 * state of charger cables for enabling or disabling charger(regulator) and
 * select the charger cable for charging among a number of external cable
 * according to policy of H/W board.
 */
static int charger_manager_register_extcon(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct charger_regulator *charger;
	int ret;
	int i;
	int j;

	for (i = 0; i < desc->num_charger_regulators; i++) {
		charger = &desc->charger_regulators[i];

		charger->consumer = regulator_get(cm->dev, charger->regulator_name);
		if (IS_ERR(charger->consumer)) {
			dev_err(cm->dev, "Cannot find charger(%s)\n",
				charger->regulator_name);
			return PTR_ERR(charger->consumer);
		}
		charger->cm = cm;

		for (j = 0; j < charger->num_cables; j++) {
			struct charger_cable *cable = &charger->cables[j];

			ret = charger_extcon_init(cm, cable);
			if (ret < 0) {
				dev_err(cm->dev, "Cannot initialize charger(%s)\n",
					charger->regulator_name);
				return ret;
			}
			cable->charger = charger;
			cable->cm = cm;
		}
	}

	return 0;
}

/* help function of sysfs node to control charger(regulator) */
static ssize_t charger_name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charger_regulator *charger =
		container_of(attr, struct charger_regulator, attr_name);

	return sprintf(buf, "%s\n", charger->regulator_name);
}

static ssize_t charger_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charger_regulator *charger =
		container_of(attr, struct charger_regulator, attr_state);
	int state = 0;

	if (!charger->externally_control)
		state = regulator_is_enabled(charger->consumer);

	return sprintf(buf, "%s\n", state ? "enabled" : "disabled");
}

static ssize_t jeita_control_show(struct device *dev,  struct device_attribute *attr, char *buf)
{
	struct charger_regulator *charger =
		container_of(attr, struct charger_regulator, attr_jeita_control);
	struct charger_desc *desc = charger->cm->desc;

	return sprintf(buf, "%d\n", !desc->jeita_disabled);
}

static ssize_t jeita_control_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret;
	struct charger_regulator *charger =
		container_of(attr, struct charger_regulator, attr_jeita_control);
	struct charger_desc *desc = charger->cm->desc;
	bool enabled;

	if (!charger || !desc) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ret =  kstrtobool(buf, &enabled);
	if (ret)
		return ret;

	desc->jeita_disabled = !enabled;

	return count;
}

static ssize_t
charge_pump_present_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charger_regulator *charger =
		container_of(attr, struct charger_regulator, attr_charge_pump_present);
	struct charger_manager *cm = charger->cm;
	bool status = false;

	if (!charger || !cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (cm_check_cp_charger_enabled(cm))
		status = true;

	return sprintf(buf, "%d\n", status);
}

static ssize_t charge_pump_present_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int ret;
	struct charger_regulator *charger =
		container_of(attr, struct charger_regulator, attr_charge_pump_present);
	struct charger_manager *cm = charger->cm;
	bool enabled;

	if (!charger || !cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ret =  kstrtobool(buf, &enabled);
	if (ret)
		return ret;

	if (enabled) {
		cm_init_cp(cm);
		cm_primary_charger_enable(cm, false);
		if (cm_check_primary_charger_enabled(cm)) {
			dev_err(cm->dev, "Fail to disable primary charger\n");
			return -EINVAL;
		}

		cm_cp_master_charger_enable(cm, true);
		if (!cm_check_cp_charger_enabled(cm))
			dev_err(cm->dev, "Fail to enable charge pump\n");
	} else {
		if (!cm_cp_master_charger_enable(cm, false))
			dev_err(cm->dev, "Fail to disable master charge pump\n");
	}

	return count;
}

static ssize_t
charge_pump_current_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charger_regulator *charger =
		container_of(attr, struct charger_regulator, attr_charge_pump_current);
	struct charger_manager *cm = charger->cm;
	int cur, ret;

	if (!charger || !cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (charger->cp_id < 0) {
		dev_err(cm->dev, "charge pump id is error!!!!!!\n");
		cur = 0;
		return sprintf(buf, "%d\n", cur);
	}

	ret = get_cp_ibat_uA_by_id(cm, &cur, charger->cp_id);
	if (ret)
		cur = 0;

	return sprintf(buf, "%d\n", cur);
}

static ssize_t charge_pump_current_id_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	int ret;
	struct charger_regulator *charger =
		container_of(attr, struct charger_regulator, attr_charge_pump_current);
	struct charger_manager *cm = charger->cm;
	int cp_id;

	if (!charger || !cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ret =  kstrtoint(buf, 10, &cp_id);
	if (ret)
		return ret;

	if (cp_id < 0) {
		dev_err(cm->dev, "charge pump id is error!!!!!!\n");
		cp_id = 0;
	}
	charger->cp_id = cp_id;

	return count;
}

static ssize_t charger_stop_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct charger_regulator *charger =
		container_of(attr, struct charger_regulator, attr_stop_charge);
	bool stop_charge;

	if (!charger) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}
	stop_charge = is_charging(charger->cm);

	return sprintf(buf, "%d\n", !stop_charge);
}

static ssize_t charger_stop_store(struct device *dev,
				  struct device_attribute *attr, const char *buf,
				  size_t count)
{
	struct charger_regulator *charger =
		container_of(attr, struct charger_regulator, attr_stop_charge);
	struct charger_manager *cm = charger->cm;
	int stop_charge, ret;

	if (!charger || !cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ret = sscanf(buf, "%d", &stop_charge);
	if (!ret)
		return -EINVAL;

	if (!is_ext_pwr_online(cm))
		return -EINVAL;

	if (!stop_charge) {
		ret = try_charger_enable(cm, true);
		if (ret) {
			dev_err(cm->dev, "failed to start charger.\n");
			return ret;
		}
		charger->externally_control = false;
	} else {
		ret = try_charger_enable(cm, false);
		if (ret) {
			dev_err(cm->dev, "failed to stop charger.\n");
			return ret;
		}
		charger->externally_control = true;
	}

	power_supply_changed(cm->charger_psy);
	return count;
}

static ssize_t charger_externally_control_show(struct device *dev,
					       struct device_attribute *attr, char *buf)
{
	struct charger_regulator *charger =
		container_of(attr, struct charger_regulator, attr_externally_control);

	if (!charger) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	return sprintf(buf, "%d\n", charger->externally_control);
}

static ssize_t charger_externally_control_store(struct device *dev,
						struct device_attribute *attr, const char *buf,
						size_t count)
{
	struct charger_regulator *charger =
		container_of(attr, struct charger_regulator, attr_externally_control);
	struct charger_manager *cm = charger->cm;
	struct charger_desc *desc = cm->desc;
	int i;
	int ret;
	int externally_control;
	int chargers_externally_control = 1;

	if (!charger || !cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ret = sscanf(buf, "%d", &externally_control);
	if (ret == 0) {
		ret = -EINVAL;
		return ret;
	}

	if (!externally_control) {
		charger->externally_control = 0;
		return count;
	}

	for (i = 0; i < desc->num_charger_regulators; i++) {
		if (&desc->charger_regulators[i] != charger &&
			!desc->charger_regulators[i].externally_control) {
			/*
			 * At least, one charger is controlled by
			 * charger-manager
			 */
			chargers_externally_control = 0;
			break;
		}
	}

	if (!chargers_externally_control) {
		if (cm->charger_enabled) {
			try_charger_enable(charger->cm, false);
			charger->externally_control = externally_control;
			try_charger_enable(charger->cm, true);
		} else {
			charger->externally_control = externally_control;
		}
	} else {
		dev_warn(cm->dev,
			 "'%s' regulator should be controlled in charger-manager because charger-manager must need at least one charger for charging\n",
			 charger->regulator_name);
	}

	return count;
}

#ifdef VENDOR_KERNEL
static ssize_t batt_param_noplug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	/*struct charger_regulator *charger =
		container_of(attr, struct charger_regulator, attr_batt_param_noplug);*/

	//pr_err("%s:line%d: noplug_temperature[%d] noplug_batt_volt_max[%d] noplug_batt_volt_min[%d]\n", __func__, __LINE__, noplug_temperature,
	//	noplug_batt_volt_max, noplug_batt_volt_min);

	return sprintf(buf, "%d %d %d", noplug_temperature,
		noplug_batt_volt_max, noplug_batt_volt_min);
}
#endif //defined VENDOR_KERNEL

static ssize_t cp_num_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charger_regulator *charger =
		container_of(attr, struct charger_regulator, attr_cp_num);
	struct charger_manager *cm = charger->cm;
	int cp_num = 0;

	if (!cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cp_num = cm->desc->cp_nums;
	return sprintf(buf, "%d\n", cp_num);
}

static ssize_t enable_power_path_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct charger_regulator *charger
		= container_of(attr, struct charger_regulator,
			       attr_enable_power_path);
	struct charger_manager *cm = charger->cm;
	bool power_path_enabled;

	power_path_enabled = cm_is_power_path_enabled(cm);

	return sprintf(buf, "%d\n", power_path_enabled);
}

static ssize_t enable_power_path_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct charger_regulator *charger
		= container_of(attr, struct charger_regulator,
			       attr_enable_power_path);
	struct charger_manager *cm = charger->cm;
	bool power_path_enabled;
	int ret;

	ret =  kstrtobool(buf, &power_path_enabled);
	if (ret)
		return ret;

	if (power_path_enabled)
		cm_power_path_enable(cm, CM_POWER_PATH_ENABLE_CMD);
	else
		cm_power_path_enable(cm, CM_POWER_PATH_DISABLE_CMD);

	power_supply_changed(cm->charger_psy);

	return count;
}

static ssize_t charger_notify_code_show(struct device *dev,
									  struct device_attribute *attr,
									  char *buf)
{
   return sprintf(buf, "%d\n", notify_code);
}

#ifndef VENDOR_KERNEL
static ssize_t charger_otg_enable_show(struct device *dev,
									struct device_attribute *attr,
									char *buf)
{
 	return sprintf(buf, "%d\n", otg_switch_state);
}
#endif //not defined VENDOR_KERNEL

static ssize_t cool_down_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	int ret;

	ret = sscanf(buf, "%d", &g_cool_down);
	if (ret == 0) {
		ret = -EINVAL;
		return ret;
	}

	return count;
}

static ssize_t cool_down_show(struct device *dev,
								   struct device_attribute *attr,
								   char *buf)
{
	return sprintf(buf, "%d\n", g_cool_down);
}

#ifndef VENDOR_KERNEL
static ssize_t usb_type_role_show(struct device *dev,
								  struct device_attribute *attr,
								  char *buf)
{
   return sprintf(buf, "%d\n", ((prj_name == 1) ? 0 : 1));
}

static ssize_t typec_cc_polarity_show(struct device *dev,
								struct device_attribute *attr,
								char *buf)
{
  #ifdef CONFIG_HQ_USB_TEMP_CHECK
	return sprintf(buf, "%d\n", g_cc_polarity);
  #else
  	return sprintf(buf, "%d\n", 0);
  #endif
}
#endif //not defined VENDOR_KERNEL

static ssize_t fast_charge_support_show(struct device *dev,
								struct device_attribute *attr,
								char *buf)
{
	int fastcharge_support = 0;
#ifndef VENDOR_KERNEL
	if(prj_name == 2 || prj_name == 3){
		fastcharge_support = 1;
	} else {
		fastcharge_support = 0;
	}
#endif
	return sprintf(buf, "%d\n", fastcharge_support);
}

static ssize_t runin_stop_show(struct device *dev,
								struct device_attribute *attr,
								char *buf)
{
	return sprintf(buf, "%d\n", charge_stop);
}

static ssize_t runin_stop_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
#ifdef VENDOR_KERNEL
	struct charger_regulator *charger
		= container_of(attr, struct charger_regulator,
			       attr_enable_power_path);
#else //not defined VENDOR_KERNEL
	struct charger_regulator *charger
		= container_of(attr, struct charger_regulator,
			       attr_runin_stop);
#endif //defined VENDOR_KERNEL
	struct charger_manager *cm = charger->cm;
	bool runinstop;
	int ret;

	ret =  kstrtobool(buf, &runinstop);
	if (ret)
		return ret;

	ret = sscanf(buf, "%d", &runinstop);
	if (runinstop) {
		charge_stop = 1;
		ret = set_hiz_mode(cm,true);
		if(ret){
			dev_err(cm->dev,"runin stop set hiz mode fail\n");
		}
		ret= try_charger_enable(cm,false);
		if(ret){
			dev_err(cm->dev,"runin stop try charger enable fail\n");
		}
		}else{
			charge_stop = 0;
			ret = set_hiz_mode(cm,false);
			if(ret){
				dev_err(cm->dev,"runin stop set hiz mode fail\n");
			}
			ret= try_charger_enable(cm,true);
			if(ret){
				dev_err(cm->dev,"runin stop try charger enable fail\n");
			}
	}

	return count;
}

static ssize_t ship_mode_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	int ret;

	ret = sscanf(buf, "%d", &ship_mode);
	if (ret == 0) {
		ret = -EINVAL;
		return ret;
	}

	return count;
}

static ssize_t ship_mode_show(struct device *dev,
								   struct device_attribute *attr,
								   char *buf)
{
	return sprintf(buf, "%d\n", ship_mode);
}

#ifndef VENDOR_KERNEL
static ssize_t is_fast_charge_show(struct device *dev,
								   struct device_attribute *attr,
								   char *buf)
{
	return sprintf(buf, "%d\n", g_is_fast_charge);
}
#endif //not defined VENDOR_KERNEL

static ssize_t batid_volt_show(struct device *dev,
								   struct device_attribute *attr,
								   char *buf)
{
	return sprintf(buf, "%d\n", batid_volt);
}

#ifndef VENDOR_KERNEL
static ssize_t main_charger_current_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
   struct charger_regulator *charger
	   = container_of(attr, struct charger_regulator,
				  attr_main_charger_current);
   struct charger_manager *cm = charger->cm;
   union power_supply_propval val;
   struct power_supply *charger_psy;
   int ret = -ENODEV;

   charger_psy = power_supply_get_by_name(cm->desc->psy_charger_stat[0]);
   if (!charger_psy) {
   	dev_err(cm->dev, "Cannot find charge power supply \"%s\"\n",
				cm->desc->psy_cp_stat[0]);
   return -ENOMEM;

   }

	val.intval = 0;
	ret = power_supply_get_property(charger_psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, &val);
	power_supply_put(charger_psy);


   return sprintf(buf, "%d\n", val.intval);
}

static ssize_t second_charger_current_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct charger_regulator *charger
	 = container_of(attr, struct charger_regulator,
				attr_second_charger_current);
	struct charger_manager *cm = charger->cm;
	union power_supply_propval val;
	struct power_supply *charger_psy;
	int ret = -ENODEV;

	if(prj_name == 2 || prj_name == 3){
		charger_psy = power_supply_get_by_name(cm->desc->psy_charger_stat[1]);
		if (!charger_psy) {
			dev_err(cm->dev, "Cannot find charge power supply \"%s\"\n",
				  cm->desc->psy_cp_stat[1]);
			return -ENOMEM;
		}

		val.intval = 0;
		ret = power_supply_get_property(charger_psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, &val);
		power_supply_put(charger_psy);


		return sprintf(buf, "%d\n", val.intval);
	} else {
		return ret;
	}

}

static ssize_t is_factory_mode_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
   int ret;

   ret = sscanf(buf, "%d", &g_is_fast_charge);
   if (ret == 0) {
		ret = -EINVAL;
		return ret;
	}

   return count;
}
#endif //not defined VENDOR_KERNEL

static ssize_t notify_code_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
   struct charger_regulator *charger
	 = container_of(attr, struct charger_regulator,
				attr_notify_code);
	struct charger_manager *cm = charger->cm;
   int ret;

   ret = sscanf(buf, "%d", &notify_code);
   if (ret == 0) {
		ret = -EINVAL;
		return ret;
	}
	cm_uevent_notify(cm);
   return count;
}

#ifdef VENDOR_KERNEL
static ssize_t temp_debug_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
   int ret;

   ret = sscanf(buf, "%d", &g_temp_debug);
   if (ret == 0) {
		ret = -EINVAL;
		return ret;
	}
   return count;
}
static ssize_t temp_debug_show(struct device *dev,
								  struct device_attribute *attr,
								  char *buf)
{
   return sprintf(buf, "%d\n", g_temp_debug);
}
#endif //defined VENDOR_KERNEL

/*************************************************************
 *                        uart_switch                       *
*************************************************************/
struct pinctrl *p = NULL;

static int sprd_pin_set_gpio(char *pinctrlName)
{
	struct pinctrl_state *pinctrl_state;
	int ret;

	pinctrl_state = pinctrl_lookup_state(p,pinctrlName);
	pr_err("---retrieves a state handle from a pinctrl handle---\n");
	ret = pinctrl_select_state(p, pinctrl_state);
	pr_err("---select a pinctrl state to HW--- \n");

	return 0;

}

static int pull_gpio(int num,int value)
{
	int ret;
	struct gpio_desc *desc;

	int gpio_num = 64 + num;
	desc = gpio_to_desc(gpio_num);
	ret = gpio_request(gpio_num, "change_to_gpio");
	ret = gpio_direction_output(gpio_num, value);

	return 0;
}

#ifdef VENDOR_KERNEL
static ssize_t uart_switch_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	pr_err("uart_switch_show");
	return sprintf(buf, "%d\n", 1);
}

static ssize_t uart_switch_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	pr_err("uart_switch_store");
	sprd_pin_set_gpio("gpio_70_f3");
	sprd_pin_set_gpio("gpio_71_f3");
	pull_gpio(70,0);
	pull_gpio(71,0);
	return count;
}

static ssize_t charger_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        char *state ="null";

        if (ac_main.AC_ONLINE == 1)
                state="AC";
	else if (usb_main.USB_ONLINE == 1)
		state="USB";
	else
		state="null";

        return sprintf(buf, "%s\n", state);
}
#else //not defined VENDOR_KERNEL
static ssize_t is_factory_mode_show(struct device *dev,
								  struct device_attribute *attr,
								  char *buf)
{
   return sprintf(buf, "%d\n", g_is_fast_charge);
}
#endif //defined VENDOR_KERNEL
/**
 * charger_manager_prepare_sysfs - Prepare sysfs entry for each charger
 * @cm: the Charger Manager representing the battery.
 *
 * This function add sysfs entry for charger(regulator) to control charger from
 * user-space. If some development board use one more chargers for charging
 * but only need one charger on specific case which is dependent on user
 * scenario or hardware restrictions, the user enter 1 or 0(zero) to '/sys/
 * class/power_supply/battery/charger.[index]/externally_control'. For example,
 * if user enter 1 to 'sys/class/power_supply/battery/charger.[index]/
 * externally_control, this charger isn't controlled from charger-manager and
 * always stay off state of regulator.
 */
static int charger_manager_prepare_sysfs(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct charger_regulator *charger;
	int chargers_externally_control = 1;
	char *name;
	int i;

	/* Create sysfs entry to control charger(regulator) */
	for (i = 0; i < desc->num_charger_regulators; i++) {
		charger = &desc->charger_regulators[i];

		name = devm_kasprintf(cm->dev, GFP_KERNEL, "charger.%d", i);
		if (!name)
			return -ENOMEM;

		charger->attrs[0] = &charger->attr_name.attr;
		charger->attrs[1] = &charger->attr_state.attr;
		charger->attrs[2] = &charger->attr_externally_control.attr;
		charger->attrs[3] = &charger->attr_stop_charge.attr;
		charger->attrs[4] = &charger->attr_jeita_control.attr;
		charger->attrs[5] = &charger->attr_cp_num.attr;
		charger->attrs[6] = &charger->attr_charge_pump_present.attr;
		charger->attrs[7] = &charger->attr_charge_pump_current.attr;
		charger->attrs[8] = &charger->attr_enable_power_path.attr;
		charger->attrs[9] = &charger->attr_notify_code.attr;
#ifdef VENDOR_KERNEL
		charger->attrs[10] = &charger->attr_batt_param_noplug.attr;
#else //not defined VENDOR_KERNEL
		charger->attrs[10] = &charger->attr_otg_enable.attr;
#endif //defined VENDOR_KERNEL
		charger->attrs[11] = &charger->attr_cool_down.attr;
#ifdef VENDOR_KERNEL
		//charger->attrs[12] = &charger->attr_usb_type_role.attr;
		//charger->attrs[13] = &charger->attr_typec_cc_polarity.attr;
		charger->attrs[12] = &charger->attr_fast_charge_support.attr;
		charger->attrs[13] = &charger->attr_runin_stop.attr;
		charger->attrs[14] = &charger->attr_ship_mode.attr;
		//charger->attrs[17] = &charger->attr_is_fast_charge.attr;
		charger->attrs[15] = &charger->attr_batid_volt.attr;
		//charger->attrs[19] = &charger->attr_main_charger_current.attr;
		//charger->attrs[20] = &charger->attr_second_charger_current.attr;
		//charger->attrs[21] = &charger->attr_is_factory_mode.attr;
		charger->attrs[16] =  &charger->attr_temp_debug.attr;
		charger->attrs[17] = &charger->attr_uart_switch.attr;
		charger->attrs[18] = &charger->attr_type.attr;
		charger->attrs[19] = NULL;
#else //not defined VENDOR_KERNEL
		charger->attrs[12] = &charger->attr_usb_type_role.attr;
		charger->attrs[13] = &charger->attr_typec_cc_polarity.attr;
		charger->attrs[14] = &charger->attr_fast_charge_support.attr;
		charger->attrs[15] = &charger->attr_runin_stop.attr;
		charger->attrs[16] = &charger->attr_ship_mode.attr;
		charger->attrs[17] = &charger->attr_is_fast_charge.attr;
		charger->attrs[18] = &charger->attr_batid_volt.attr;
		charger->attrs[19] = &charger->attr_main_charger_current.attr;
		charger->attrs[20] = &charger->attr_second_charger_current.attr;
		charger->attrs[21] = &charger->attr_is_factory_mode.attr;
		charger->attrs[22] = NULL;
#endif //defined VENDOR_KERNEL

		charger->attr_grp.name = name;
		charger->attr_grp.attrs = charger->attrs;
		desc->sysfs_groups[i] = &charger->attr_grp;

		sysfs_attr_init(&charger->attr_name.attr);
		charger->attr_name.attr.name = "name";
		charger->attr_name.attr.mode = 0444;
		charger->attr_name.show = charger_name_show;

		sysfs_attr_init(&charger->attr_state.attr);
		charger->attr_state.attr.name = "state";
		charger->attr_state.attr.mode = 0444;
		charger->attr_state.show = charger_state_show;

		sysfs_attr_init(&charger->attr_stop_charge.attr);
		charger->attr_stop_charge.attr.name = "stop_charge";
		charger->attr_stop_charge.attr.mode = 0644;
		charger->attr_stop_charge.show = charger_stop_show;
		charger->attr_stop_charge.store = charger_stop_store;

		sysfs_attr_init(&charger->attr_jeita_control.attr);
		charger->attr_jeita_control.attr.name = "jeita_control";
		charger->attr_jeita_control.attr.mode = 0644;
		charger->attr_jeita_control.show = jeita_control_show;
		charger->attr_jeita_control.store = jeita_control_store;

		sysfs_attr_init(&charger->attr_cp_num.attr);
		charger->attr_cp_num.attr.name = "cp_num";
		charger->attr_cp_num.attr.mode = 0444;
		charger->attr_cp_num.show = cp_num_show;

		sysfs_attr_init(&charger->attr_charge_pump_present.attr);
		charger->attr_charge_pump_present.attr.name = "charge_pump_present";
		charger->attr_charge_pump_present.attr.mode = 0644;
		charger->attr_charge_pump_present.show = charge_pump_present_show;
		charger->attr_charge_pump_present.store = charge_pump_present_store;

		sysfs_attr_init(&charger->attr_charge_pump_current.attr);
		charger->attr_charge_pump_current.attr.name = "charge_pump_current";
		charger->attr_charge_pump_current.attr.mode = 0644;
		charger->attr_charge_pump_current.show = charge_pump_current_show;
		charger->attr_charge_pump_current.store = charge_pump_current_id_store;

		sysfs_attr_init(&charger->attr_enable_power_path.attr);
		charger->attr_enable_power_path.attr.name = "enable_power_path";
		charger->attr_enable_power_path.attr.mode = 0644;
		charger->attr_enable_power_path.show = enable_power_path_show;
		charger->attr_enable_power_path.store = enable_power_path_store;

		sysfs_attr_init(&charger->attr_notify_code.attr);
		charger->attr_notify_code.attr.name = "notify_code";
		charger->attr_notify_code.attr.mode = 0644;
		charger->attr_notify_code.show = charger_notify_code_show;
		charger->attr_notify_code.store = notify_code_store;

#ifndef VENDOR_KERNEL
		sysfs_attr_init(&charger->attr_otg_enable.attr);
		charger->attr_otg_enable.attr.name = "otg_enable";
		charger->attr_otg_enable.attr.mode = 0444;
		charger->attr_otg_enable.show = charger_otg_enable_show;
#endif //not defined VENDOR_KERNEL

		sysfs_attr_init(&charger->attr_cool_down.attr);
		charger->attr_cool_down.attr.name = "cool_down";
		charger->attr_cool_down.attr.mode = 0644;
		charger->attr_cool_down.show = cool_down_show;
		charger->attr_cool_down.store = cool_down_store;

#ifndef VENDOR_KERNEL
		sysfs_attr_init(&charger->attr_usb_type_role.attr);
		charger->attr_usb_type_role.attr.name = "usb_type_role";
		charger->attr_usb_type_role.attr.mode = 0444;
		charger->attr_usb_type_role.show = usb_type_role_show;

		sysfs_attr_init(&charger->attr_typec_cc_polarity.attr);
		charger->attr_typec_cc_polarity.attr.name = "typec_cc_polarity";
		charger->attr_typec_cc_polarity.attr.mode = 0444;
		charger->attr_typec_cc_polarity.show = typec_cc_polarity_show;
#endif //not defined VENDOR_KERNEL

		sysfs_attr_init(&charger->attr_fast_charge_support.attr);
		charger->attr_fast_charge_support.attr.name = "fast_charge_support";
		charger->attr_fast_charge_support.attr.mode = 0444;
		charger->attr_fast_charge_support.show = fast_charge_support_show;

		sysfs_attr_init(&charger->attr_runin_stop.attr);
		charger->attr_runin_stop.attr.name = "runin_stop";
		charger->attr_runin_stop.attr.mode = 0644;
		charger->attr_runin_stop.show = runin_stop_show;
		charger->attr_runin_stop.store = runin_stop_store;

		sysfs_attr_init(&charger->attr_ship_mode.attr);
		charger->attr_ship_mode.attr.name = "ship_mode";
		charger->attr_ship_mode.attr.mode = 0644;
		charger->attr_ship_mode.show = ship_mode_show;
		charger->attr_ship_mode.store = ship_mode_store;

#ifndef VENDOR_KERNEL
		sysfs_attr_init(&charger->attr_is_fast_charge.attr);
		charger->attr_is_fast_charge.attr.name = "is_fast_charge";
		charger->attr_is_fast_charge.attr.mode = 0444;
		charger->attr_is_fast_charge.show = is_fast_charge_show;
#endif //not defined VENDOR_KERNEL

		sysfs_attr_init(&charger->attr_batid_volt.attr);
		charger->attr_batid_volt.attr.name = "batid_volt";
		charger->attr_batid_volt.attr.mode = 0444;
		charger->attr_batid_volt.show = batid_volt_show;

#ifdef VENDOR_KERNEL
		sysfs_attr_init(&charger->attr_temp_debug.attr);
		charger->attr_temp_debug.attr.name = "temp_debug";
		charger->attr_temp_debug.attr.mode = 0644;
		charger->attr_temp_debug.show = temp_debug_show;
		charger->attr_temp_debug.store = temp_debug_store;
#else //not defined VENDOR_KERNEL
		sysfs_attr_init(&charger->attr_main_charger_current.attr);
		charger->attr_main_charger_current.attr.name = "main_charger_current";
		charger->attr_main_charger_current.attr.mode = 0444;
		charger->attr_main_charger_current.show = main_charger_current_show;

		sysfs_attr_init(&charger->attr_second_charger_current.attr);
		charger->attr_second_charger_current.attr.name = "second_charger_current";
		charger->attr_second_charger_current.attr.mode = 0444;
		charger->attr_second_charger_current.show = second_charger_current_show;

		sysfs_attr_init(&charger->attr_is_factory_mode.attr);
		charger->attr_is_factory_mode.attr.name = "is_factory_mode";
		charger->attr_is_factory_mode.attr.mode = 0644;
		charger->attr_is_factory_mode.show = is_factory_mode_show;
		charger->attr_is_factory_mode.store = is_factory_mode_store;
#endif //defined VENDOR_KERNEL

		sysfs_attr_init(&charger->attr_externally_control.attr);
		charger->attr_externally_control.attr.name
				= "externally_control";
		charger->attr_externally_control.attr.mode = 0644;
		charger->attr_externally_control.show
				= charger_externally_control_show;
		charger->attr_externally_control.store
				= charger_externally_control_store;

#ifdef VENDOR_KERNEL
		sysfs_attr_init(&charger->attr_batt_param_noplug.attr);
		charger->attr_batt_param_noplug.attr.name = "batt_param_noplug";
		charger->attr_batt_param_noplug.attr.mode = 0444;
		charger->attr_batt_param_noplug.show = batt_param_noplug_show;

		sysfs_attr_init(&charger->attr_uart_switch.attr);
		charger->attr_uart_switch.attr.name = "uart_switch";
		charger->attr_uart_switch.attr.mode = 0644;
		charger->attr_uart_switch.show = uart_switch_show;
		charger->attr_uart_switch.store = uart_switch_store;

		sysfs_attr_init(&charger->attr_type.attr);
                charger->attr_type.attr.name = "type";
                charger->attr_type.attr.mode = 0444;
                charger->attr_type.show = charger_type_show;
#endif //defined VENDOR_KERNEL

		if (!desc->charger_regulators[i].externally_control ||
				!chargers_externally_control)
			chargers_externally_control = 0;

		dev_info(cm->dev, "'%s' regulator's externally_control is %d\n",
			 charger->regulator_name, charger->externally_control);
	}

	if (chargers_externally_control) {
		dev_err(cm->dev, "Cannot register regulator because charger-manager must need at least one charger for charging battery\n");
		return -EINVAL;
	}

	return 0;
}

static int cm_init_thermal_data(struct charger_manager *cm, struct power_supply *fuel_gauge)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	int ret;

	/* Verify whether fuel gauge provides battery temperature */
	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_TEMP, &val);

	if (!ret) {
		if (!cm_add_battery_psy_property(cm, POWER_SUPPLY_PROP_TEMP))
			dev_warn(cm->dev, "POWER_SUPPLY_PROP_TEMP is present\n");
		cm->desc->measure_battery_temp = true;
	}
#if IS_ENABLED(CONFIG_THERMAL)
	if (desc->thermal_zone) {
		cm->tzd_batt =
			thermal_zone_get_zone_by_name(desc->thermal_zone);
		if (IS_ERR(cm->tzd_batt))
			return PTR_ERR(cm->tzd_batt);

		/* Use external thermometer */
		if (!cm_add_battery_psy_property(cm, POWER_SUPPLY_PROP_TEMP_AMBIENT))
			dev_warn(cm->dev, "POWER_SUPPLY_PROP_TEMP_AMBIENT is present\n");
		cm->desc->measure_battery_temp = true;
		ret = 0;
	}
#endif
	if (cm->desc->measure_battery_temp) {
		/* NOTICE : Default allowable minimum charge temperature is 0 */
		if (!desc->temp_max)
			desc->temp_max = CM_DEFAULT_CHARGE_TEMP_MAX;
		if (!desc->temp_diff)
			desc->temp_diff = CM_DEFAULT_RECHARGE_TEMP_DIFF;
	}

	return ret;
}

static int cm_init_jeita_table(struct sprd_battery_info *info,
			       struct charger_desc *desc, struct device *dev)
{
	int i;

	for (i = SPRD_BATTERY_JEITA_DCP; i < SPRD_BATTERY_JEITA_MAX; i++) {
		desc->jeita_size[i] = info->sprd_battery_jeita_size[i];
		if (!desc->jeita_size[i]) {
			dev_warn(dev, "%s jeita_size is zero\n",
				 sprd_battery_jeita_type_names[i]);
			continue;
		}

		desc->jeita_tab_array[i] = devm_kmemdup(dev, info->jeita_table[i],
							desc->jeita_size[i] *
							sizeof(struct sprd_battery_jeita_table),
							GFP_KERNEL);
		if (!desc->jeita_tab_array[i]) {
			dev_warn(dev, "Fail to kmemdup %s\n",
				 sprd_battery_jeita_type_names[i]);
			return -ENOMEM;
		}
	}

	desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_UNKNOWN];
	desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_UNKNOWN];

	return 0;
}

static const struct of_device_id charger_manager_match[] = {
	{
		.compatible = "charger-manager",
	},
	{},
};
MODULE_DEVICE_TABLE(of, charger_manager_match);

static int charger_extcon_data_init(struct charger_cable *cables,
				    const struct device_node *_child)
{
	if (of_property_read_bool(_child, "extcon")) {
		struct device_node *extcon_np;

		extcon_np = of_parse_phandle(_child, "extcon", 0);
		if (!extcon_np)
			return -ENODEV;

		cables->extcon_dev = extcon_find_edev_by_node(extcon_np);
		of_node_put(extcon_np);
		if (IS_ERR(cables->extcon_dev))
			return PTR_ERR(cables->extcon_dev);

	}

	return 0;
}

static int charger_regualtors_data_init(struct charger_desc *desc, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret;

	desc->num_charger_regulators = of_get_child_count(np);
	if (desc->num_charger_regulators) {
		struct charger_regulator *chg_regs;
		struct device_node *child;

		chg_regs = devm_kcalloc(dev,
					desc->num_charger_regulators,
					sizeof(*chg_regs),
					GFP_KERNEL);
		if (!chg_regs)
			return -ENOMEM;

		desc->charger_regulators = chg_regs;

		desc->sysfs_groups = devm_kcalloc(dev,
					desc->num_charger_regulators + 1,
					sizeof(*desc->sysfs_groups),
					GFP_KERNEL);
		if (!desc->sysfs_groups)
			return -ENOMEM;

		for_each_child_of_node(np, child) {
			struct charger_cable *cables;
			struct device_node *_child;

			of_property_read_string(child, "cm-regulator-name",
					&chg_regs->regulator_name);

			/* charger cables */
			chg_regs->num_cables = of_get_child_count(child);
			if (chg_regs->num_cables) {
				cables = devm_kcalloc(dev,
						      chg_regs->num_cables,
						      sizeof(*cables),
						      GFP_KERNEL);
				if (!cables) {
					of_node_put(child);
					return -ENOMEM;
				}

				chg_regs->cables = cables;

				for_each_child_of_node(child, _child) {
					of_property_read_string(_child,
					"cm-cable-name", &cables->name);
					of_property_read_u32(_child,
					"cm-cable-min",
					&cables->min_uA);
					of_property_read_u32(_child,
					"cm-cable-max",
					&cables->max_uA);

					ret = charger_extcon_data_init(cables, _child);
					if (ret)
						return ret;

					cables++;
				}
			}
			chg_regs++;
		}
	}

	return 0;
}

static struct charger_desc *of_cm_parse_desc(struct device *dev)
{
	struct charger_desc *desc;
	struct device_node *np = dev->of_node;
	u32 poll_mode = CM_POLL_DISABLE;
	u32 battery_stat = CM_NO_BATTERY;
	int ret, i = 0, num_chgs = 0;

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return ERR_PTR(-ENOMEM);

	of_property_read_string(np, "cm-name", &desc->psy_name);

	of_property_read_u32(np, "cm-poll-mode", &poll_mode);
	desc->polling_mode = poll_mode;

	desc->uvlo_shutdown_mode = CM_SHUTDOWN_MODE_ANDROID;
	of_property_read_u32(np, "cm-uvlo-shutdown-mode", &desc->uvlo_shutdown_mode);

	of_property_read_u32(np, "cm-poll-interval",
				&desc->polling_interval_ms);

	of_property_read_u32(np, "cm-fullbatt-vchkdrop-ms",
					&desc->fullbatt_vchkdrop_ms);
	of_property_read_u32(np, "cm-fullbatt-vchkdrop-volt",
					&desc->fullbatt_vchkdrop_uV);
	of_property_read_u32(np, "cm-fullbatt-soc", &desc->fullbatt_soc);
	of_property_read_u32(np, "cm-fullbatt-capacity",
					&desc->fullbatt_full_capacity);
	of_property_read_u32(np, "cm-shutdown-voltage", &desc->shutdown_voltage);
	of_property_read_u32(np, "cm-tickle-time-out", &desc->trickle_time_out);
	of_property_read_u32(np, "cm-one-cap-time", &desc->cap_one_time);
	of_property_read_u32(np, "cm-wdt-interval", &desc->wdt_interval);

	of_property_read_u32(np, "cm-battery-stat", &battery_stat);
	desc->battery_present = battery_stat;

	/* chargers */
	num_chgs = of_property_count_strings(np, "cm-chargers");
	if (num_chgs > 0) {
		/* Allocate empty bin at the tail of array */
		desc->psy_charger_stat = devm_kcalloc(dev,
						      num_chgs + 1,
						      sizeof(char *),
						      GFP_KERNEL);
		if (!desc->psy_charger_stat)
			return ERR_PTR(-ENOMEM);

		for (i = 0; i < num_chgs; i++)
			of_property_read_string_index(np, "cm-chargers", i,
						      &desc->psy_charger_stat[i]);
	}

	/* fast chargers */
	num_chgs = of_property_count_strings(np, "cm-fast-chargers");
	if (num_chgs > 0) {
		/* Allocate empty bin at the tail of array */
		desc->psy_fast_charger_stat =
			devm_kzalloc(dev, sizeof(char *) * (u32)(num_chgs + 1), GFP_KERNEL);
		if (!desc->psy_fast_charger_stat)
			return ERR_PTR(-ENOMEM);

		for (i = 0; i < num_chgs; i++)
			of_property_read_string_index(np, "cm-fast-chargers", i,
						      &desc->psy_fast_charger_stat[i]);
	}

	/* charge pumps */
	num_chgs = of_property_count_strings(np, "cm-charge-pumps");
	if (num_chgs > 0) {
		/* Allocate empty bin at the tail of array */
		desc->cp_nums = num_chgs;
		desc->psy_cp_stat =
			devm_kzalloc(dev, sizeof(char *) * (u32)(num_chgs + 1), GFP_KERNEL);
		if (!desc->psy_cp_stat)
			return ERR_PTR(-ENOMEM);

		for (i = 0; i < num_chgs; i++)
			of_property_read_string_index(np, "cm-charge-pumps", i,
						      &desc->psy_cp_stat[i]);
	}

	/* wireless chargers */
	num_chgs = of_property_count_strings(np, "cm-wireless-chargers");
	if (num_chgs > 0) {
		/* Allocate empty bin at the tail of array */
		desc->psy_wl_charger_stat =
			devm_kzalloc(dev,  sizeof(char *) * (u32)(num_chgs + 1), GFP_KERNEL);
		if (desc->psy_wl_charger_stat) {
			for (i = 0; i < num_chgs; i++)
				of_property_read_string_index(np, "cm-wireless-chargers",
						i, &desc->psy_wl_charger_stat[i]);
		} else {
			return ERR_PTR(-ENOMEM);
		}
	}

	/* wireless charge pump converters */
	num_chgs = of_property_count_strings(np, "cm-wireless-charge-pump-converters");
	if (num_chgs > 0) {
		/* Allocate empty bin at the tail of array */
		desc->psy_cp_converter_stat =
			devm_kzalloc(dev, sizeof(char *) * (u32)(num_chgs + 1), GFP_KERNEL);
		if (desc->psy_cp_converter_stat) {
			for (i = 0; i < num_chgs; i++)
				of_property_read_string_index(np, "cm-wireless-charge-pump-converters",
						i, &desc->psy_cp_converter_stat[i]);
		} else {
			return ERR_PTR(-ENOMEM);
		}
	}

	of_property_read_string(np, "cm-fuel-gauge", &desc->psy_fuel_gauge);

	of_property_read_string(np, "cm-thermal-zone", &desc->thermal_zone);

	of_property_read_u32(np, "cm-battery-cold", &desc->temp_min);
	if (of_get_property(np, "cm-battery-cold-in-minus", NULL))
		desc->temp_min *= -1;
	of_property_read_u32(np, "cm-battery-hot", &desc->temp_max);
	of_property_read_u32(np, "cm-battery-temp-diff", &desc->temp_diff);

	of_property_read_u32(np, "cm-charging-max",
				&desc->charging_max_duration_ms);
	of_property_read_u32(np, "cm-discharging-max",
				&desc->discharging_max_duration_ms);
	of_property_read_u32(np, "cm-charge-voltage-max",
			     &desc->normal_charge_voltage_max);
	of_property_read_u32(np, "cm-charge-voltage-drop",
			     &desc->normal_charge_voltage_drop);
	of_property_read_u32(np, "cm-fast-charge-voltage-max",
			     &desc->fast_charge_voltage_max);
	of_property_read_u32(np, "cm-fast-charge-voltage-drop",
			     &desc->fast_charge_voltage_drop);
	of_property_read_u32(np, "cm-flash-charge-voltage-max",
			     &desc->flash_charge_voltage_max);
	of_property_read_u32(np, "cm-flash-charge-voltage-drop",
			     &desc->flash_charge_voltage_drop);
	of_property_read_u32(np, "cm-wireless-charge-voltage-max",
			     &desc->wireless_normal_charge_voltage_max);
	of_property_read_u32(np, "cm-wireless-charge-voltage-drop",
			     &desc->wireless_normal_charge_voltage_drop);
	of_property_read_u32(np, "cm-wireless-fast-charge-voltage-max",
			     &desc->wireless_fast_charge_voltage_max);
	of_property_read_u32(np, "cm-wireless-fast-charge-voltage-drop",
			     &desc->wireless_fast_charge_voltage_drop);
	of_property_read_u32(np, "cm-cp-taper-current",
			     &desc->cp.cp_taper_current);
	of_property_read_u32(np, "cm-cap-full-advance-percent",
			     &desc->cap_remap_full_percent);

	ret = cm_init_cap_remap_table(desc, dev);
	if (ret)
		dev_err(dev, "%s init cap remap table fail\n", __func__);

	/* battery charger regualtors */
	ret = charger_regualtors_data_init(desc, dev);
	if (ret)
		return ERR_PTR(ret);

	return desc;
}

static inline struct charger_desc *cm_get_drv_data(struct platform_device *pdev)
{
	if (pdev->dev.of_node)
		return of_cm_parse_desc(&pdev->dev);
	return dev_get_platdata(&pdev->dev);
}

static int cm_get_bat_info(struct charger_manager *cm)
{
	struct sprd_battery_info info = {};
	int ret, num = 0;

	if (sc27xx_fgu_bat_id == 2)
		num = 1;
	ret = sprd_battery_get_battery_info(cm->charger_psy, &info, num);
	if (ret) {
		dev_err(cm->dev, "failed to get battery information\n");
		sprd_battery_put_battery_info(cm->charger_psy, &info);
		return ret;
	}

	cm->desc->internal_resist = info.factory_internal_resistance_uohm / 1000;
	cm->desc->ir_comp.us = info.constant_charge_voltage_max_uv;
	cm->desc->ir_comp.us_upper_limit = info.ir.us_upper_limit_uv;
	cm->desc->ir_comp.rc = info.ir.rc_uohm / 1000;
	cm->desc->ir_comp.cp_upper_limit_offset = info.ir.cv_upper_limit_offset_uv;
	cm->desc->constant_charge_voltage_max_uv = info.constant_charge_voltage_max_uv;
	cm->desc->fullbatt_voltage_offset_uv = info.fullbatt_voltage_offset_uv;
	cm->desc->fchg_ocv_threshold = info.fast_charge_ocv_threshold_uv;
	cm->desc->cp.cp_target_vbat = info.constant_charge_voltage_max_uv;
	cm->desc->cp.cp_max_ibat = info.cur.flash_cur;
	cm->desc->cp.cp_target_ibat = info.cur.flash_cur;
	cm->desc->cp.cp_max_ibus = info.cur.flash_limit;
	cm->desc->cur.sdp_limit = info.cur.sdp_limit;
	cm->desc->cur.sdp_cur = info.cur.sdp_cur;
	cm->desc->cur.dcp_limit = info.cur.dcp_limit;
	cm->desc->cur.dcp_cur = info.cur.dcp_cur;
	cm->desc->cur.cdp_limit = info.cur.cdp_limit;
	cm->desc->cur.cdp_cur = info.cur.cdp_cur;
	cm->desc->cur.unknown_limit = info.cur.unknown_limit;
	cm->desc->cur.unknown_cur = info.cur.unknown_cur;
	cm->desc->cur.fchg_limit = info.cur.fchg_limit;
	cm->desc->cur.fchg_cur = info.cur.fchg_cur;
	cm->desc->cur.flash_limit = info.cur.flash_limit;
	cm->desc->cur.flash_cur = info.cur.flash_cur;
	cm->desc->cur.wl_bpp_limit = info.cur.wl_bpp_limit;
	cm->desc->cur.wl_bpp_cur = info.cur.wl_bpp_cur;
	cm->desc->cur.wl_epp_limit = info.cur.wl_epp_limit;
	cm->desc->cur.wl_epp_cur = info.cur.wl_epp_cur;
	cm->desc->fullbatt_uV = info.fullbatt_voltage_uv;
	cm->desc->fullbatt_uA = info.fullbatt_current_uA;
	cm->desc->first_fullbatt_uA = info.first_fullbatt_current_uA;
	cm->desc->force_jeita_status = info.force_jeita_status;

	dev_info(cm->dev, "SPRD_BATTERY_INFO: internal_resist = %d, us = %d, "
		 "constant_charge_voltage_max_uv = %d, fchg_ocv_threshold = %d, "
		 "cp_target_vbat = %d, cp_max_ibat = %d, cp_target_ibat = %d "
		 "cp_max_ibus = %d, sdp_limit = %d, sdp_cur = %d, dcp_limit = %d, "
		 "dcp_cur = %d, cdp_limit = %d, cdp_cur = %d, unknown_limit=%d, "
		 "unknown_cur = %d. fchg_limit = %d, fchg_cur = %d, flash_limit = %d, "
		 "flash_cur = %d, wl_bpp_limit = %d, wl_bpp_cur= %d, wl_epp_limit = %d, "
		 "wl_epp_cur = %d, fullbatt_uV= %d, fullbatt_uA= %d, "
		 "cm->desc->first_fullbatt_uA = %d, us_upper_limit = %d, rc = %d, "
		 "cp_upper_limit_offset = %d, force_jeita_status = %d\n",
		 cm->desc->internal_resist, cm->desc->ir_comp.us,
		 cm->desc->constant_charge_voltage_max_uv, cm->desc->fchg_ocv_threshold,
		 cm->desc->cp.cp_target_vbat, cm->desc->cp.cp_max_ibat,
		 cm->desc->cp.cp_target_ibat, cm->desc->cp.cp_max_ibus, cm->desc->cur.sdp_limit,
		 cm->desc->cur.sdp_cur, cm->desc->cur.dcp_limit, cm->desc->cur.dcp_cur,
		 cm->desc->cur.cdp_limit, cm->desc->cur.cdp_cur, cm->desc->cur.unknown_limit,
		 cm->desc->cur.unknown_cur, cm->desc->cur.fchg_limit, cm->desc->cur.fchg_cur,
		 cm->desc->cur.flash_limit, cm->desc->cur.flash_cur, cm->desc->cur.wl_bpp_limit,
		 cm->desc->cur.wl_bpp_cur, cm->desc->cur.wl_epp_limit, cm->desc->cur.wl_epp_cur,
		 cm->desc->fullbatt_uV, cm->desc->fullbatt_uA, cm->desc->first_fullbatt_uA,
		 cm->desc->ir_comp.us_upper_limit, cm->desc->ir_comp.rc,
		 cm->desc->ir_comp.cp_upper_limit_offset, cm->desc->force_jeita_status);

	ret = cm_init_jeita_table(&info, cm->desc, cm->dev);
	if (ret) {
		sprd_battery_put_battery_info(cm->charger_psy, &info);
		return ret;
	}

	if (cm->desc->force_jeita_status <= 0)
		cm->desc->force_jeita_status = (cm->desc->jeita_tab_size + 1) / 2;

	if (cm->desc->fullbatt_uV == 0)
		dev_info(cm->dev, "Ignoring full-battery voltage threshold as it is not supplied\n");

	if (cm->desc->fullbatt_uA == 0)
		dev_info(cm->dev, "Ignoring full-battery current threshold as it is not supplied\n");

	if (cm->desc->fullbatt_voltage_offset_uv == 0)
		dev_info(cm->dev, "Ignoring full-battery voltage offset as it is not supplied\n");

	sprd_battery_put_battery_info(cm->charger_psy, &info);

	return 0;
}

static void cm_uvlo_check_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
				struct charger_manager, uvlo_work);
	int batt_uV, ret;

	ret = get_vbat_now_uV(cm, &batt_uV);
	if (ret || batt_uV < 0) {
		dev_err(cm->dev, "get_vbat_now_uV error.\n");
		return;
	}

	dev_info(cm->dev, "cm_uvlo_check_work bat_uv:%d shutdown_voltage:%d.\n",batt_uV,cm->desc->shutdown_voltage);
	if ((u32)batt_uV <= cm->desc->shutdown_voltage)
		cm->desc->uvlo_trigger_cnt++;
	else
		cm->desc->uvlo_trigger_cnt = 0;

	if (cm->desc->uvlo_trigger_cnt >= CM_UVLO_CALIBRATION_CNT_THRESHOLD) {
		dev_err(cm->dev, "WARN: batt_uV less than uvlo, cap %d\n", cm->desc->cap);
		if (DIV_ROUND_CLOSEST(cm->desc->cap, 10) <= 0) {
			dev_err(cm->dev, "WARN: trigger  uvlo, will shutdown with uisoc less than 1%\n");
			set_batt_cap(cm, 0);
			switch (cm->desc->uvlo_shutdown_mode) {
			case CM_SHUTDOWN_MODE_ORDERLY:
				orderly_poweroff(true);
				break;

			case CM_SHUTDOWN_MODE_KERNEL:
				kernel_power_off();
				break;

			case CM_SHUTDOWN_MODE_ANDROID:
				cancel_delayed_work_sync(&cm->cap_update_work);
				cm->desc->cap = 0;
				power_supply_changed(cm->charger_psy);
				break;

			default:
				dev_warn(cm->dev, "Incorrect uvlo_shutdown_mode (%d)\n",
					 cm->desc->uvlo_shutdown_mode);
			}
	 	} else if (batt_uV <= cm->desc->shutdown_voltage) {
				dev_err(cm->dev, "WARN: batt_uV less than shutdown voltage, will shutdown, "
					"and force capacity to 0\n");
				adjust_fuel_cap(cm, 0);
		}
	}

	if (batt_uV < CM_UVLO_CALIBRATION_VOLTAGE_THRESHOLD)
		schedule_delayed_work(&cm->uvlo_work, msecs_to_jiffies(800));
}

static void cm_jeita_temp_check(struct charger_manager *cm)
{
	int ret, cur_temp;

	ret = cm_get_battery_temperature_by_psy(cm, &cur_temp);
	if (ret) {
		dev_err(cm->dev, "failed to get battery temperature\n");
		return;
	}

	if (cur_temp > JEITA_HIGHEST_TEMPE) {
		notify_code |= CHG_BAT_HIG_TEMP_STATUS;
		dev_err(cm->dev, "%s: batt_temp:%d over temp limit!\n", __func__, cur_temp);
	} else if (cur_temp <= NTC_DISCONNECT_TEMP) {
		if(cm_ntc_get_current_temp(cm) == 2)
		{
			notify_code |= CHG_BAT_ID_STATUS;
			dev_err(cm->dev, "%s: batt_temp:%d NTC below temp limit!\n", __func__, cur_temp);
		}
	}else if (cur_temp < JEITA_LOWEST_TEMPE) {
		if(cm_ntc_get_current_temp(cm) == 1)
		{
			notify_code |= CHG_BAT_LOW_TEMP_STATUS;
			dev_err(cm->dev, "%s: batt_temp:%d below temp limit!\n", __func__, cur_temp);
		}
	} else if (notify_code & CHG_BAT_HIG_TEMP_STATUS) {
		notify_code &= ~CHG_BAT_HIG_TEMP_STATUS;
		dev_info(cm->dev, "%s: batt_temp:%d form hot temp restore normal temp\n", __func__, cur_temp);
	} else if (notify_code & CHG_BAT_LOW_TEMP_STATUS) {
		notify_code &= ~CHG_BAT_LOW_TEMP_STATUS;
		dev_info(cm->dev, "%s: batt_temp:%d form low temp restore normal temp\n", __func__, cur_temp);
	}

	cm_uevent_notify(cm);
}
int cm_ntc_get_current_temp(struct charger_manager *cm)
{
	int ret, cur_temp;
	int count_temp;
	for(count_temp=0;count_temp < 5;count_temp++)
	{
		ret = cm_get_battery_temperature_by_psy(cm, &cur_temp);
		if (ret) 
		{
			dev_err(cm->dev, "failed to get battery temperature\n");
			return 0;
		}
	}
	dev_info(cm->dev, "%s: 111temp_check,batt_temp:%d temp\n", __func__, cur_temp);
	if(cur_temp < JEITA_LOWEST_TEMPE  && cur_temp > NTC_DISCONNECT_TEMP)
	{
		return 1;
	}
	else
	{
		return 2;
	}
}

/*odm.hq.bsp.luowenjiang@huaqin.com 2021.11.04 modify for usb temp protection start*/
#ifdef CONFIG_HQ_USB_TEMP_CHECK
struct voltage_temp {
	int voltage;
	int temp;
};

static const struct voltage_temp voltage_temp_table[] = {
	{ 11808, -40 },
	{ 11791, -39 },
	{ 11773, -38 },
	{ 11754, -37 },
	{ 11734, -36 },
	{ 11713, -35 },
	{ 11691, -34 },
	{ 11667, -33 },
	{ 11642, -32 },
	{ 11616, -31 },
	{ 11588, -30 },
	{ 11558, -29 },
	{ 11528, -28 },
	{ 11495, -27 },
	{ 11461, -26 },
	{ 11425, -25 },
	{ 11388, -24 },
	{ 11348, -23 },
	{ 11307, -22 },
	{ 11264, -21 },
	{ 11219, -20 },
	{ 11171, -19 },
	{ 11122, -18 },
	{ 11070, -17 },
	{ 11016, -16 },
	{ 10960, -15 },
	{ 10902, -14 },
	{ 10841, -13 },
	{ 10778, -12 },
	{ 10712, -11 },
	{ 10644, -10 },
	{ 10574, -9  },
	{ 10501, -8  },
	{ 10425, -7  },
	{ 10347, -6  },
	{ 10266, -5  },
	{ 10182, -4  },
	{ 10096, -3  },
	{ 10007, -2  },
	{ 9916 , -1  },
	{ 9822 , 0   },
	{ 9726 , 1   },
	{ 9627 , 2   },
	{ 9525 , 3   },
	{ 9422 , 4   },
	{ 9315 , 5   },
	{ 9207 , 6   },
	{ 9096 , 7   },
	{ 8983 , 8   },
	{ 8868 , 9   },
	{ 8752 , 10  },
	{ 8633 , 11  },
	{ 8512 , 12  },
	{ 8390 , 13  },
	{ 8266 , 14  },
	{ 8140 , 15  },
	{ 8013 , 16  },
	{ 7885 , 17  },
	{ 7756 , 18  },
	{ 7626 , 19  },
	{ 7495 , 20  },
	{ 7363 , 21  },
	{ 7231 , 22  },
	{ 7098 , 23  },
	{ 6965 , 24  },
	{ 6832 , 25  },
	{ 6699 , 26  },
	{ 6566 , 27  },
	{ 6433 , 28  },
	{ 6300 , 29  },
	{ 6168 , 30  },
	{ 6037 , 31  },
	{ 5906 , 32  },
	{ 5776 , 33  },
	{ 5647 , 34  },
	{ 5519 , 35  },
	{ 5392 , 36  },
	{ 5267 , 37  },
	{ 5143 , 38  },
	{ 5020 , 39  },
	{ 4899 , 40  },
	{ 4779 , 41  },
	{ 4661 , 42  },
	{ 4545 , 43  },
	{ 4431 , 44  },
	{ 4318 , 45  },
	{ 4207 , 46  },
	{ 4098 , 47  },
	{ 3992 , 48  },
	{ 3887 , 49  },
	{ 3784 , 50  },
	{ 3683 , 51  },
	{ 3584 , 52  },
	{ 3487 , 53  },
	{ 3393 , 54  },
	{ 3300 , 55  },
	{ 3209 , 56  },
	{ 3121 , 57  },
	{ 3034 , 58  },
	{ 2950 , 59  },
	{ 2867 , 60  },
	{ 2787 , 61  },
	{ 2708 , 62  },
	{ 2632 , 63  },
	{ 2558 , 64  },
	{ 2485 , 65  },
	{ 2414 , 66  },
	{ 2346 , 67  },
	{ 2279 , 68  },
	{ 2214 , 69  },
	{ 2150 , 70  },
	{ 2088 , 71  },
	{ 2028 , 72  },
	{ 1970 , 73  },
	{ 1913 , 74  },
	{ 1858 , 75  },
	{ 1804 , 76  },
	{ 1752 , 77  },
	{ 1702 , 78  },
	{ 1653 , 79  },
	{ 1605 , 80  },
	{ 1559 , 81  },
	{ 1514 , 82  },
	{ 1471 , 83  },
	{ 1429 , 84  },
	{ 1388 , 85  },
	{ 1348 , 86  },
	{ 1309 , 87  },
	{ 1272 , 88  },
	{ 1235 , 89  },
	{ 1200 , 90  },
	{ 1166 , 91  },
	{ 1133 , 92  },
	{ 1100 , 93  },
	{ 1069 , 94  },
	{ 1039 , 95  },
	{ 1009 , 96  },
	{ 981  , 97  },
	{ 954  , 98  },
	{ 927  , 99  },
	{ 901  , 100 },
	{ 876  , 101 },
	{ 851  , 102 },
	{ 828  , 103 },
	{ 805  , 104 },
	{ 782  , 105 },
	{ 761  , 106 },
	{ 740  , 107 },
	{ 719  , 108 },
	{ 700  , 109 },
	{ 680  , 110 },
	{ 662  , 111 },
	{ 644  , 112 },
	{ 626  , 113 },
	{ 610  , 114 },
	{ 593  , 115 },
	{ 577  , 116 },
	{ 562  , 117 },
	{ 547  , 118 },
	{ 532  , 119 },
	{ 518  , 120 },
	{ 505  , 121 },
	{ 491  , 122 },
	{ 478  , 123 },
	{ 466  , 124 },
	{ 454  , 125 },
};

#define CONVERT_FACTORT	10
#define VOL_MAX 11808
#define VOL_MIN	454

static void cm_convert_vlotage_to_temp(int voltage,int *temp)
{
	int index = 0;
	int i = 0;
	int temp_voltage = 0;

	temp_voltage = voltage * CONVERT_FACTORT;

	if(temp_voltage >= VOL_MAX ) 
		index = 0;

	if(temp_voltage <= VOL_MIN ) 
		index = ARRAY_SIZE(voltage_temp_table) - 1;

	for(i = 1;i < ARRAY_SIZE(voltage_temp_table) - 1; i++)
		if ( temp_voltage <= voltage_temp_table[i].voltage && temp_voltage > voltage_temp_table[i+1].voltage) {
			index = i;
			break;
		}

	*temp = voltage_temp_table[index].temp;
}


static int cm_get_usbs_temp(struct charger_manager *cm,int *temp1,int *temp2)
{
	int rc;
	int voltage1,voltage2;
	int t1,t2;

	rc = iio_read_channel_processed(cm->usb_conn_ntc_1,&voltage1);
	if ( rc < 0) {
		pr_err("ERROR:%s:%u Failed to get usb ADC temp,rc = %d\n",__func__,__LINE__,rc);
		return rc;
	}

	rc = iio_read_channel_processed(cm->usb_conn_ntc_2,&voltage2);
	if ( rc < 0) {
		pr_err("ERROR:%s:%u Failed to get usb ADC temp,rc = %d\n",__func__,__LINE__,rc);
		return rc;
	}

	cm_convert_vlotage_to_temp(voltage1,&t1);
	cm_convert_vlotage_to_temp(voltage2,&t2);

	*temp1 = t1;
	*temp2 = t2;

	return rc ;
}

#define GPIO_HIGH	1
#define GPIO_LOW	0
static int cm_parse_gpio(struct charger_manager *cm)
{
	int rc = 0;
	struct device_node *np = NULL;
	np = cm->dev->of_node;

	if(np == NULL) {
		pr_err("ERROR:%s:%u device_node pointer is NULL",__func__,__LINE__);
		return -EINVAL;
	}

	cm->charge_enable_gpio = of_get_named_gpio(np, "cm-charge-gpio", 0);
	if(!gpio_is_valid(cm->charge_enable_gpio)) {
		pr_err("ERROR:%s:%u Faile to get charge GPIO from tree,rc = %d \n",
			__func__,__LINE__,cm->charge_enable_gpio);
		return -EINVAL;
	}

	rc =  gpio_request(cm->charge_enable_gpio,"charge_enable");
	if (rc < 0) {
		pr_err("ERROR:%s:%u Failed to request GPIO rc = %d\n",__func__,__LINE__,rc);
		return rc;
	}

	return 0;
}

static int cm_set_charge_gpio(struct charger_manager *cm,int gpio_status)
{
	if(gpio_is_valid(cm->charge_enable_gpio))
		gpio_direction_output(cm->charge_enable_gpio, gpio_status);
	else
		return -EINVAL;

	return 0;
}

#define TEMP_27C	27
#define TEMP_30C	30
#define TEMP_40C	40
#define TEMP_50C	50
#define TEMP_55C	55
#define TEMP_57C	57
#define TEMP_M20C	-20
#define TEMP_100C	100
#define RETRY_COUNT	3
#define TEMP_OFF_SET 7
#define TEMP_USB_BATT_GAP 12
#define DELAY_5MS 5
#define DELAY_2S	2000
#define DELAY_20MS	20
#define	DELAY_50MS	50
#define	DELAY_500MS	500
#define VBUS_THRESHOLD 30
#define VBUS_STATUS_THRESHOLD 30

#define MIN_MONITOR_INTERVAL	50
#define MAX_MONITOR_INTERVAL	2000

static struct task_struct *cm_usbtemp_thread;
static DECLARE_WAIT_QUEUE_HEAD(cm_usbtemp_wq);

int cm_usb_temp_check_thread(void *data)
{
	struct charger_manager *cm = (struct charger_manager *) data;

	int rc;
	int usb_temp_1,usb_temp_2;
	int batt_temp;
	int vbus;
	int retry = 3;
	int usb_temp_1_count, usb_temp_2_count;
	static int last_usb_temp_1 = 25;
	static int last_usb_temp_2 = 25;
	static int count = 0;
	int total_count;
	int delay;
	int i = 0;
	bool cc_vbus = false;
	bool ntc_short = false;

	if (cm == NULL) {
		pr_err("ERROR:%s:%u charge_pionter null \n",__func__,__LINE__);
		return 0;
	}

	while (!kthread_should_stop()) {

		rc = get_charger_voltage(cm, &vbus);
		if (rc < 0) {
			pr_err("ERROR:%s:%u get chg_vol error\n",__func__,__LINE__);
			return 0;
		}

		vbus /= 100000;
		if (vbus <= VBUS_THRESHOLD) {
			delay = DELAY_2S;
			total_count = 10;
		} else {
			delay = DELAY_50MS;
			total_count = 30;
		}

		if(vbus >= VBUS_STATUS_THRESHOLD) {
			cm->vbus_status = true;
			/*to schedule cc short to vbus work*/
			if (!cc_vbus) {
				cc_vbus = true;
				schedule_delayed_work(&cm->cc_vbus_check_work, msecs_to_jiffies(100));
			}
		} else {
			cm->vbus_status = false;
			cc_vbus = false;
/*odm.hq.bsp.luowenjiang@huaqin.com 2021.03.03 modify for usb ntc short protection start*/
			if (ntc_short) {
				ntc_short = false;
				runin_stop = 0;
				charge_stop = 0;

				rc = set_hiz_mode(cm,false);
				if (rc) {
					dev_err(cm->dev,"runin stop set hiz mode fail\n");
				}
				rc = try_charger_enable(cm,true);
				if (rc) {
					dev_err(cm->dev,"runin stop try charger enable fail\n");
				}
			}
/*odm.hq.bsp.luowenjiang@huaqin.com 2021.03.03 modify for usb ntc short protection start*/
			goto recheck;
		}

/*odm.hq.bsp.luowenjiang@huaqin.com 2021.03.03 modify for usb ntc short protection start*/
		if ((usb_temp_1 == 125 || usb_temp_2 == 125) && !ntc_short ) {
			ntc_short = true;
			runin_stop = 1;
			charge_stop = 1;

			rc = set_hiz_mode(cm,true);
			if (rc) {
				dev_err(cm->dev,"runin stop set hiz mode fail\n");
			}
			rc= try_charger_enable(cm,false);
			if (rc) {
				dev_err(cm->dev,"runin stop try charger enable fail\n");
			}

			pr_info("%s:%u:ADC stop_charge by ntc short\n",__func__,__LINE__);
		}
/*odm.hq.bsp.luowenjiang@huaqin.com 2022.03.03 modify for usb ntc short protection end*/

		rc = cm_get_usbs_temp(cm,&usb_temp_1,&usb_temp_2);
		if(rc < 0) {
			pr_err("ERROR:%s:%u Faile to get usb temp, rc = %d\n",
											__func__, __LINE__, rc);
			return 0;
		}

		rc = cm_get_battery_temperature(cm, &batt_temp);
		if (rc < 0) {
			pr_err("Error:%s:%u Failed to get batt_temp, rc = %d\n",
											__func__,__LINE__,rc);
			return 0;
		}

		batt_temp /= 10;
		/* The first comdition*/
		if ((batt_temp < TEMP_50C && usb_temp_1 >= TEMP_57C && usb_temp_1 <= TEMP_100C ) ||
			(batt_temp < TEMP_50C && usb_temp_2 >= TEMP_57C && usb_temp_2 <= TEMP_100C ) ||
			(batt_temp >= TEMP_50C && (usb_temp_1 - batt_temp) >= TEMP_OFF_SET && usb_temp_1 <= TEMP_100C)||
			(batt_temp >= TEMP_50C && (usb_temp_2 - batt_temp) >= TEMP_OFF_SET && usb_temp_2 <= TEMP_100C)) {

			notify_code |= CHG_USB_TEMP_HIGH;
			try_charger_enable(cm,false);
			cm_set_otg_switch_status(false);
			mdelay(DELAY_20MS);
			rc = cm_set_charge_gpio(cm,GPIO_HIGH);
			if (rc < 0)
				pr_err("ERROR:%s:%u Faild to set GPIO rc = %d\n",
											__func__,__LINE__,rc);

			pr_info("[ADC]%s:%u condition NO.1 success,and stop charge,"
					"usb_temp_1 = %d usb_temp_2 = %d batt_temp = %d\n",
							__func__,__LINE__,usb_temp_1,usb_temp_2,batt_temp);
			schedule_delayed_work(&cm->usb_temp_work, msecs_to_jiffies(14700000));
			count = 0;
			last_usb_temp_1 = usb_temp_1;
			last_usb_temp_2 = usb_temp_1;
		} else if ((usb_temp_1 >= TEMP_27C && (usb_temp_1 - batt_temp) >= TEMP_USB_BATT_GAP && usb_temp_1 <= TEMP_100C )||
			(usb_temp_2 >= TEMP_27C && (usb_temp_2 - batt_temp) >= TEMP_USB_BATT_GAP && usb_temp_2 <= TEMP_100C)) {	/*The Second condition case 1*/

			if (count == 0) {
				last_usb_temp_1 = usb_temp_1;
				last_usb_temp_2 = usb_temp_2;
			}
			rc = cm_get_usbs_temp(cm, &usb_temp_1, &usb_temp_2);
			if (rc < 0) {
				pr_err("ERROR:%s:%u Faile to get usb temp, rc = %d\n",__func__, __LINE__, rc);
				return 0;
			}
			if ((usb_temp_1 - last_usb_temp_1) >= 3 || (usb_temp_2 - last_usb_temp_2) >= 3 ) {
				for (i = 1; i <= retry; i++) {
					msleep(DELAY_50MS);
					rc = cm_get_usbs_temp(cm, &usb_temp_1, &usb_temp_2);
					if (rc < 0) {
						pr_err("ERROR:%s:%u Faile to get usb temp, rc = %d\n",__func__, __LINE__, rc);
						return 0;
					}
					if ( (usb_temp_1 - last_usb_temp_1) >= 3 )
						usb_temp_1_count++;
					if ( (usb_temp_2 - last_usb_temp_2) >= 3 )
						usb_temp_2_count++;
				}

				if (usb_temp_1_count >= RETRY_COUNT || usb_temp_2_count >= RETRY_COUNT)  {
					notify_code |= CHG_USB_TEMP_HIGH;
					try_charger_enable(cm,false);
					cm_set_otg_switch_status(false);
					mdelay(DELAY_20MS);
					rc = cm_set_charge_gpio(cm,GPIO_HIGH);
					if (rc < 0)
						pr_err("ERROR:%s:%u Faild to set GPIO rc = %d\n",
													__func__,__LINE__,rc);

					pr_info("[ADC]%s:%u condition NO.2 success,and stop charge"
									"usb_temp_1 = %d usb_temp_2 = %d batt_temp = %d\n",
								__func__,__LINE__,usb_temp_1,usb_temp_2,batt_temp);

					schedule_delayed_work(&cm->usb_temp_work, msecs_to_jiffies(14400000));

				}
				usb_temp_1_count = 1;
				usb_temp_2_count = 1;
			}
			count++;
			if (count > total_count)
				count = 0;
			msleep(delay);
		}else {
				recheck:
					count = 0;
					msleep(delay);
					last_usb_temp_1 = usb_temp_1;
					last_usb_temp_2 = usb_temp_2;
					wait_event_interruptible(cm_usbtemp_wq,cm->vbus_status);
				}
	}

	return 0;
}


static void cm_usb_temp_revovery_work(struct work_struct *work) {

	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
				struct charger_manager, usb_temp_work);
	int rc;
	int usb_temp_1,usb_temp_2;

	if(cm == NULL) {
		pr_err("ERROR:%s %u cm pointer is null\n",__func__, __LINE__);
		return;
	}

	rc = cm_get_usbs_temp(cm,&usb_temp_1,&usb_temp_2);
	if(rc < 0) {
		pr_err("ERROR:%s:%u Faile to get usb temp, rc = %d\n",__func__, __LINE__, rc);
		return;
	}

	if( usb_temp_1 < TEMP_55C && usb_temp_2 < TEMP_55C ) {
		notify_code &= ~CHG_USB_TEMP_HIGH;
		rc = cm_set_charge_gpio(cm,GPIO_LOW);
		if (rc < 0)
			pr_err("ERROR:%s:%u Faild to set GPIO rc = %d\n",
										__func__,__LINE__,rc);
		try_charger_enable(cm,true);
		if (1 == otg_switch_state)
			cm_set_otg_switch_status(true);

		pr_info("[ADC] recovery for usb temperature high,start charge\n");
	}

}

static void cm_usb_temp_thread_init(void *data)
{
	if (data == NULL) {
		pr_err("ERROR:%s:%u charge_pionter null \n",__func__,__LINE__);
		return;
	}
	cm_usbtemp_thread = kthread_run(cm_usb_temp_check_thread, my_cm, "usbtemp_kthread");
	if (IS_ERR(cm_usbtemp_thread))
		pr_err("ERROR:%s:%u Failed to create cm_usbtemp_thread\n",__func__,__LINE__);

}

static void cm_wake_up_usbtemp_thread(void)
{
	if (my_cm == NULL) {
		pr_err("ERROR:%s:%u charge_manage is NULL\n",__func__,__LINE__);
		return;
	}
	my_cm->vbus_status = true;
	wake_up_interruptible(&cm_usbtemp_wq);
}

#endif
/*odm.hq.bsp.luowenjiang@huaqin.com 2021.11.04 modify for usb temp protection end*/

#ifdef CONFIG_HQ_USB_TEMP_CHECK
/*odm.hq.bsp.luowenjiang@huaqin.com 2022.01.08 modify for cc short to vbus start*/
static void cm_cc_vbus_check_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
				struct charger_manager, cc_vbus_check_work);

	bool status = false;
	int vbus,rc;
	static int count = 0;

	if ( psc == NULL || cm == NULL )   {
		pr_err("%s ERROR:psc cm pointer NULL recheck!\n",__func__);
		schedule_delayed_work(&cm->cc_vbus_check_work, msecs_to_jiffies(5000));
		return;
	}

	rc = get_charger_voltage(cm, &vbus);
	if (rc < 0) {
		pr_err("ERROR:%s:%u get chg_vol error recheck\n",__func__,__LINE__);
		schedule_delayed_work(&cm->cc_vbus_check_work, msecs_to_jiffies(5000));
		return ;
	}

	status = sc27xx_typec_cc1_cc2_voltage_detect(psc);
	if (status == true) {
		count++;
	}

	if (status == false && count == 1 ) {
		count = 0;
	}

	if (count == 2) {
		if (status && !g_cc_vbus_status) {
			g_cc_vbus_status = true;
			cm_fixed_fchg_disable(cm);
			cm_set_otg_switch_status(false);
			pr_info("[CCS]cc1 or cc2 short to vbus! stop rise 9V\n");
		}
		count = 0;
	}

	if (!status && g_cc_vbus_status) {
		g_cc_vbus_status = false;
		cm_start_fixed_fchg(cm, true);
		if (otg_switch_state == 1)
			cm_set_otg_switch_status(true);
		pr_info("[CCS]cc1 or cc2 recovery\n");
	}

	if (vbus < 3300000) {
		return;
	}

	schedule_delayed_work(&cm->cc_vbus_check_work, msecs_to_jiffies(250));

}
/*odm.hq.bsp.luowenjiang@huaqin.com 2022.01.08 modify for cc short to vbus end*/
#endif //defined CONFIG_HQ_USB_TEMP_CHECK

static void cm_batt_works(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
				struct charger_manager, cap_update_work);
	struct timespec64 cur_time;
	int batt_uV, batt_ocV, batt_uA, fuel_cap, ret;
	int period_time, flush_time, cur_temp, board_temp = 0;
	int chg_cur = 0, chg_limit_cur = 0, input_cur = 0;
	int chg_vol = 0, vbat_avg = 0, ibat_avg = 0, recharge_uv = 0;
	static int last_fuel_cap = CM_MAGIC_NUM;
	static int uvlo_check_cnt = 0;
	int work_cycle = CM_CAP_CYCLE_TRACK_TIME;
#ifdef CONFIG_HQ_USB_TEMP_CHECK
	int usb_temp_1 = 0,usb_temp_2 = 0;
#endif //not defined VENDOR_KERNEL
	cm_bat_id_check(cm);
	cm_bat_ovp_check(cm);
	cm_jeita_temp_check(cm);

	ret = get_vbat_now_uV(cm, &batt_uV);
	if (ret) {
		dev_err(cm->dev, "get_vbat_now_uV error.\n");
		goto schedule_cap_update_work;
	}

	ret = get_vbat_avg_uV(cm, &vbat_avg);
	if (ret)
		dev_err(cm->dev, "get_vbat_avg_uV error.\n");

	ret = get_batt_ocv(cm, &batt_ocV);
	if (ret) {
		dev_err(cm->dev, "get_batt_ocV error.\n");
		goto schedule_cap_update_work;
	}

	ret = get_ibat_now_uA(cm, &batt_uA);
	if (ret) {
		dev_err(cm->dev, "get batt_uA error.\n");
		goto schedule_cap_update_work;
	}

	ret = get_ibat_avg_uA(cm, &ibat_avg);
	if (ret)
		dev_err(cm->dev, "get ibat_avg_uA error.\n");

	ret = get_batt_cap(cm, &fuel_cap);
	if (ret) {
		dev_err(cm->dev, "get fuel_cap error.\n");
		goto schedule_cap_update_work;
	}
	fuel_cap = cm_capacity_remap(cm, fuel_cap);

	ret = get_constant_charge_current(cm, &chg_cur);
	if (ret)
		dev_warn(cm->dev, "get constant charge error.\n");

	ret = get_input_current_limit(cm, &chg_limit_cur);
	if (ret)
		dev_warn(cm->dev, "get chg_limit_cur error.\n");

	if (cm->desc->cp.cp_running)
		ret = get_cp_ibus_uA(cm, &input_cur);
	else
		ret = get_charger_input_current(cm, &input_cur);
	if (ret)
		dev_dbg(cm->dev, "cant not get input_cur.\n");

	ret = get_charger_voltage(cm, &chg_vol);
	if (ret)
		dev_warn(cm->dev, "get chg_vol error.\n");

	ret = cm_get_battery_temperature_by_psy(cm, &cur_temp);
	if (ret) {
		dev_err(cm->dev, "failed to get battery temperature\n");
		goto schedule_cap_update_work;
	}

	cm->desc->temperature = cur_temp;
	if (g_temp_debug)
		cm->desc->temperature = g_temp_debug;

	ret = cm_get_battery_temperature(cm, &board_temp);
	if (ret)
		dev_warn(cm->dev, "failed to get board temperature\n");
#ifndef VENDOR_KERNEL /* zhangnaifeng del for 4046920*/
	if (cur_temp <= CM_LOW_TEMP_REGION &&
	    batt_uV <= CM_LOW_TEMP_SHUTDOWN_VALTAGE) {
		if (cm->desc->low_temp_trigger_cnt++ > 1)
			fuel_cap = 0;
	} else if (cm->desc->low_temp_trigger_cnt != 0) {
		cm->desc->low_temp_trigger_cnt = 0;
	}
#endif //not defined VENDOR_KERNEL
	if (fuel_cap > CM_CAP_FULL_PERCENT)
		fuel_cap = CM_CAP_FULL_PERCENT;
	else if (fuel_cap < 0)
		fuel_cap = 0;

	if (last_fuel_cap == CM_MAGIC_NUM)
		last_fuel_cap = fuel_cap;

	cur_time = ktime_to_timespec64(ktime_get_boottime());

	if (is_full_charged(cm))
		cm->battery_status = POWER_SUPPLY_STATUS_FULL;
	else if (is_charging(cm))
		cm->battery_status = POWER_SUPPLY_STATUS_CHARGING;
	else if (is_ext_pwr_online(cm))
		cm->battery_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	else
		cm->battery_status = POWER_SUPPLY_STATUS_DISCHARGING;

	/*
	 * Record the charging time when battery
	 * capacity is larger than 99%.
	 */
	if (cm->battery_status == POWER_SUPPLY_STATUS_CHARGING) {
		if (cm->desc->cap >= 986) {
			cm->desc->trickle_time =
				cur_time.tv_sec - cm->desc->trickle_start_time;
		} else {
			cm->desc->trickle_start_time = cur_time.tv_sec;
			cm->desc->trickle_time = 0;
		}
	} else {
		cm->desc->trickle_start_time = cur_time.tv_sec;
		cm->desc->trickle_time = cm->desc->trickle_time_out +
				cm->desc->cap_one_time;
	}

	flush_time = cur_time.tv_sec - cm->desc->update_capacity_time;
	period_time = cur_time.tv_sec - cm->desc->last_query_time;
	cm->desc->last_query_time = cur_time.tv_sec;

	if (cm->desc->force_set_full && is_ext_pwr_online(cm))
		cm->desc->charger_status = POWER_SUPPLY_STATUS_FULL;
	else
		cm->desc->charger_status = cm->battery_status;

	/*odm.hq.bsp.luowenjiang@huaqin.com 2021.12.28 monitor usb temp start*/
	#ifdef CONFIG_HQ_USB_TEMP_CHECK
	if (false == bbat_mode && (prj_name == 2 || prj_name == 3)) {
		ret = cm_get_usbs_temp(cm,&usb_temp_1,&usb_temp_2);
		if(ret < 0) {
			pr_err("ERROR:%s:%u Faile to get usb temp, rc = %d\n",
					__func__, __LINE__, ret);
		}
	}
	dev_info(cm->dev, "vbat: %d, vbat_avg: %d, OCV: %d, ibat: %d, ibat_avg: %d, ibus: %d,"
		 " vbus: %d, msoc: %d, uisoc: %d, chg_sts: %d, frce_full: %d, chg_lmt_cur: %d,"
		 " inpt_lmt_cur: %d, chgr_type: %d, Tboard: %d, Tbatt: %d, thm_cur: %d,"
		 " thm_pwr: %d, is_fchg: %d, fchg_en: %d, tflush: %d, tperiod: %d, prj_name: %d usb_temp_1: %d usb_temp_2: %d, cool_down: %d\n",
		 batt_uV, vbat_avg, batt_ocV, batt_uA, ibat_avg, input_cur, chg_vol, fuel_cap, cm->desc->cap,
		 cm->desc->charger_status, cm->desc->force_set_full, chg_cur, chg_limit_cur,
		 cm->desc->charger_type, board_temp, cur_temp,
		 cm->desc->thm_info.thm_adjust_cur, cm->desc->thm_info.thm_pwr,
		 cm->desc->is_fast_charge, cm->desc->enable_fast_charge, flush_time, period_time, prj_name,usb_temp_1,usb_temp_2, g_cool_down);
	#else
	dev_info(cm->dev, "vbat: %d, vbat_avg: %d, OCV: %d, ibat: %d, ibat_avg: %d, ibus: %d,"
		 " vbus: %d, msoc: %d, uisoc: %d, chg_sts: %d, bat_sts: %d, force_full: %d, is_full_warm: %d,chg_lmt_cur: %d,"
		 " inpt_lmt_cur: %d, chgr_type: %d, Tboard: %d, Tbatt: %d, thm_cur: %d,"
		 " thm_pwr: %d, is_fchg: %d, fchg_en: %d, tflush: %d, tperiod: %d, notify_code 0x%x\n",
		 batt_uV, vbat_avg, batt_ocV, batt_uA, ibat_avg, input_cur, chg_vol, fuel_cap, cm->desc->cap,
		 cm->desc->charger_status, cm->battery_status, cm->desc->force_set_full, is_full_warm, chg_cur, chg_limit_cur,
		 cm->desc->charger_type, board_temp, cur_temp,
		 cm->desc->thm_info.thm_adjust_cur, cm->desc->thm_info.thm_pwr,
		 cm->desc->is_fast_charge, cm->desc->enable_fast_charge, flush_time, period_time, notify_code);
	#endif
	/*odm.hq.bsp.luowenjiang@huaqin.com 2021.12.28 monitor usb temp end*/

	ret = dump_charger_reg_info(cm);
		work_cycle = CM_CAPACITY_CAP_CYCLE_15S;
		cm->desc->cap_one_time = CM_CAPACITY_CAP_ONE_TIME_30S;
		if (cm->desc->uvlo_trigger_cnt >= CM_UVLO_CALIBRATION_CNT_THRESHOLD &&
			(cm->desc->charger_status == POWER_SUPPLY_STATUS_DISCHARGING ||
			cm->desc->charger_status == POWER_SUPPLY_STATUS_NOT_CHARGING)) {
			//if (++uvlo_check_cnt > 2) {
				work_cycle = CM_CAPACITY_CAP_CYCLE_15S;
				cm->desc->cap_one_time = CM_CAPACITY_CAP_ONE_TIME_30S;
			//}
		} else {
			uvlo_check_cnt = 0;
		}

		//dev_info(cm->dev, "work_cycle = %ds, cap_one_time = %ds\n", work_cycle, cm->desc->cap_one_time);

	switch (cm->desc->charger_status) {
	case POWER_SUPPLY_STATUS_CHARGING:
		last_fuel_cap = fuel_cap;
		if (fuel_cap < cm->desc->cap) {
			if (batt_uA >= 0) {
				fuel_cap = cm->desc->cap;
			} else {
				if (period_time < cm->desc->cap_one_time) {
					/*
					 * The percentage of electricity is not
					 * allowed to change by 1% in cm->desc->cap_one_time.
					 */
					if ((cm->desc->cap - fuel_cap) >= 5)
						fuel_cap = cm->desc->cap - 5;
					if (flush_time < cm->desc->cap_one_time &&
					    DIV_ROUND_CLOSEST(fuel_cap, 10) !=
					    DIV_ROUND_CLOSEST(cm->desc->cap, 10))
						fuel_cap = cm->desc->cap;
				} else {
					/*
					 * If wake up from long sleep mode,
					 * will make a percentage compensation based on time.
					 */
					if ((cm->desc->cap - fuel_cap) >=
					    (period_time / cm->desc->cap_one_time) * 10)
						fuel_cap = cm->desc->cap -
							(period_time / cm->desc->cap_one_time) * 10;
				}
			}
		} else if (fuel_cap > cm->desc->cap) {
			if (period_time < cm->desc->cap_one_time) {
				if ((fuel_cap - cm->desc->cap) >= 5)
					fuel_cap = cm->desc->cap + 5;
				if (flush_time < cm->desc->cap_one_time &&
				    DIV_ROUND_CLOSEST(fuel_cap, 10) !=
				    DIV_ROUND_CLOSEST(cm->desc->cap, 10))
					fuel_cap = cm->desc->cap;
			} else {
				/*
				 * If wake up from long sleep mode,
				 * will make a percentage compensation based on time.
				 */
				if ((fuel_cap - cm->desc->cap) >=
				    (period_time / cm->desc->cap_one_time) * 10)
					fuel_cap = cm->desc->cap +
						(period_time / cm->desc->cap_one_time) * 10;
			}
		}

		if (cm->desc->cap >= 985 && cm->desc->cap <= 994 &&
		    fuel_cap >= CM_CAP_FULL_PERCENT)
			fuel_cap = 994;
		/*
		 * Record 99% of the charging time.
		 * if it is greater than 1500s,
		 * it will be mandatory to display 100%,
		 * but the background is still charging.
		 */
		if (cm->desc->cap >= 995 &&
		    cm->desc->trickle_time >= cm->desc->trickle_time_out &&
		    cm->desc->trickle_time_out > 0 &&
		    batt_uA > 0)
			cm->desc->force_set_full = true;

		break;

	case POWER_SUPPLY_STATUS_NOT_CHARGING:
	case POWER_SUPPLY_STATUS_DISCHARGING:
		/*
		 * In not charging status,
		 * the cap is not allowed to increase.
		 */
		if (fuel_cap >= cm->desc->cap) {
			last_fuel_cap = fuel_cap;
			fuel_cap = cm->desc->cap;
		} else if (cm->desc->cap >= CM_HCAP_THRESHOLD) {
			if (last_fuel_cap - fuel_cap >= CM_HCAP_DECREASE_STEP) {
				if (cm->desc->cap - fuel_cap >= CM_CAP_ONE_PERCENT)
					fuel_cap = cm->desc->cap - CM_CAP_ONE_PERCENT;
				else
					fuel_cap = cm->desc->cap - CM_HCAP_DECREASE_STEP;

				last_fuel_cap -= CM_HCAP_DECREASE_STEP;
			} else {
				fuel_cap = cm->desc->cap;
			}
		} else {
			if (period_time < cm->desc->cap_one_time) {
				if ((cm->desc->cap - fuel_cap) >= 5)
					fuel_cap = cm->desc->cap - 5;
				if ((cm->desc->cap >= 10) && (cm->desc->uvlo_trigger_cnt >= CM_UVLO_CALIBRATION_CNT_THRESHOLD)) /* low 3.3V */
					fuel_cap = cm->desc->cap - 10;
				else if ((cm->desc->cap >= 5) && (cm->desc->uvlo_trigger_cnt >= CM_UVLO_CALIBRATION_CNT_THRESHOLD)) /* low 3.4V */
					fuel_cap = cm->desc->cap - 5;
				if (flush_time < cm->desc->cap_one_time &&
					(DIV_ROUND_CLOSEST(fuel_cap, 10) != DIV_ROUND_CLOSEST(cm->desc->cap, 10)) &&
					(batt_uV > cm->desc->shutdown_voltage))
					fuel_cap = cm->desc->cap;
			} else {
				/*
				 * If wake up from long sleep mode,
				 * will make a percentage compensation based on time.
				 */
				if ((cm->desc->cap - fuel_cap) >=
				    (period_time / cm->desc->cap_one_time) * 10)
					fuel_cap = cm->desc->cap -
						(period_time / cm->desc->cap_one_time) * 10;
			}
		}
		break;

	case POWER_SUPPLY_STATUS_FULL:
		last_fuel_cap = fuel_cap;
		cm->desc->update_capacity_time = cur_time.tv_sec;
		recharge_uv = cm->desc->fullbatt_uV - cm->desc->fullbatt_vchkdrop_uV;
		if (batt_ocV < recharge_uv) {
			cm->desc->force_set_full = false;
			dev_info(cm->dev, "recharge_uv = %d\n", recharge_uv);
		}

		if (is_ext_pwr_online(cm)) {
			if (fuel_cap != CM_CAP_FULL_PERCENT)
				fuel_cap = CM_CAP_FULL_PERCENT;

			if (fuel_cap > cm->desc->cap)
				fuel_cap = cm->desc->cap + 1;
		}

		break;
	default:
		break;
	}

	power_supply_changed(cm->charger_psy);

	if(cur_temp > OVER_HIGH_TEMP){
			dev_info(cm->dev, "cur_temp over high temp\n");
#ifdef VENDOR_KERNEL // for beidou_T
			orderly_poweroff(true);
#else
			kernel_power_off();
#endif
	}


	//dev_info(cm->dev, "new_uisoc = %d, old_uisoc = %d\n", fuel_cap, cm->desc->cap);
#ifdef WT_COMPILE_FACTORY_VERSION
	cm_ato_control_charge(cm, cm->desc->cap);
#endif /* WT_COMPILE_FACTORY_VERSION */
	if (fuel_cap != cm->desc->cap) {
		if (DIV_ROUND_CLOSEST(fuel_cap, 10) != DIV_ROUND_CLOSEST(cm->desc->cap, 10)) {
			cm->desc->cap = fuel_cap;
			cm->desc->update_capacity_time = cur_time.tv_sec;
			power_supply_changed(cm->charger_psy);
		}

		cm->desc->cap = fuel_cap;
		if (cm->desc->uvlo_trigger_cnt < CM_UVLO_CALIBRATION_CNT_THRESHOLD)
			set_batt_cap(cm, cm->desc->cap);
	}

	if (batt_uV < CM_UVLO_CALIBRATION_VOLTAGE_THRESHOLD) {
		dev_info(cm->dev, "batt_uV is less than UVLO calib volt\n");
		schedule_delayed_work(&cm->uvlo_work, msecs_to_jiffies(100));
	}

schedule_cap_update_work:
	queue_delayed_work(system_power_efficient_wq,
			 &cm->cap_update_work, work_cycle * HZ);
}

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
		allow_charger_enable = true;
	else if (strstr(cmd_line, "androidboot.mode=charger"))
		is_charger_mode =  true;

	return 0;
}

#ifndef VENDOR_KERNEL
static ssize_t store_runin_stop(struct device *dev, struct device_attribute *attr,\
        const char *buf, size_t count)
{
	struct charger_manager *cm = dev_get_drvdata(dev);
	int ret;
	int flag = 0;

	if (!buf){
		return -1;
	}

	ret = kstrtoint(buf, 10, &flag);
	if(ret < 0){
		dev_err(cm->dev,"stop runin stop fail\n");
	}

	dev_info(cm->dev, "store_runin_stop, runin_stop = %d\n", flag);

	if(flag){
		runin_stop = 1;
		ret = set_hiz_mode(cm,true);
		if(ret){
			dev_err(cm->dev,"runin stop set hiz mode fail\n");
		}
		ret= try_charger_enable(cm,false);
		if(ret){
			dev_err(cm->dev,"runin stop try charger enable fail\n");
		}
	}else{
		runin_stop = 0;
		ret = set_hiz_mode(cm,false);
		if(ret){
			dev_err(cm->dev,"runin stop set hiz mode fail\n");
		}
		ret= try_charger_enable(cm,true);
		if(ret){
			dev_err(cm->dev,"runin stop try charger enable fail\n");
		}
	}
    return count;
}

static ssize_t show_runin_stop(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", runin_stop);
}

static DEVICE_ATTR(runin_stop, 0644, show_runin_stop, store_runin_stop);

static struct attribute *bbat_attributes[] = {
    &dev_attr_runin_stop.attr,
    NULL
};

static struct attribute_group bbat_attribute_group = {
    .attrs = bbat_attributes
};

static void get_charger_type(struct work_struct *work)
{
	int ret = 0;
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
			struct charger_manager, get_charger_type_work);

	ret = get_usb_charger_type(cm, &cm->desc->charger_type);
	pr_err("%s:line%d: charger_type: %d\n", __func__, __LINE__, cm->desc->charger_type);
	if (ret)
		dev_warn(cm->dev, "Fail to get usb charger type, ret = %d", ret);
	if (cm->desc->charger_type == POWER_SUPPLY_CHARGER_TYPE_UNKNOWN) {
		/* odm.hq.bsp modify for apple 5v2.4a oppo 5v4a charger */
		if (!is_ext_usb_pwr_online(cm)){
			pr_info("%s:line%d: is_ext_usb_pwr_online false\n", __func__, __LINE__);
			wireless_main.WIRELESS_ONLINE = 0;
			ac_main.AC_ONLINE = 0;
			usb_main.USB_ONLINE = 0;
		} else {
			wireless_main.WIRELESS_ONLINE = 0;
			usb_main.USB_ONLINE = 0;
			ac_main.AC_ONLINE = 1;
		}
	} else if (cm->desc->charger_type == POWER_SUPPLY_USB_CHARGER_TYPE_CDP) {
		wireless_main.WIRELESS_ONLINE = 0;
		usb_main.USB_ONLINE = 1;
		ac_main.AC_ONLINE = 0;
	}
	power_supply_changed(cm->charger_psy);
}
#endif //not defined VENDOR_KERNEL

static int charger_manager_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct charger_desc *desc = cm_get_drv_data(pdev);
	struct charger_manager *cm;
	int ret, i = 0;
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	struct power_supply_config psy_cfg = {};
	struct timespec64 cur_time;

	p= devm_pinctrl_get(&pdev->dev);
	dev_err(&pdev->dev,"---obtain a pinctrl handle--- \n");

	if (IS_ERR(desc)) {
		dev_err(&pdev->dev, "No platform data (desc) found\n");
		return PTR_ERR(desc);
	}

	cm = devm_kzalloc(&pdev->dev, sizeof(*cm), GFP_KERNEL);
	if (!cm)
		return -ENOMEM;

	/* Basic Values. Unspecified are Null or 0 */
	cm->dev = &pdev->dev;
	cm->desc = desc;
	psy_cfg.drv_data = cm;

	/* Initialize alarm timer */
	if (alarmtimer_get_rtcdev()) {
		cm_timer = devm_kzalloc(cm->dev, sizeof(*cm_timer), GFP_KERNEL);
		if (!cm_timer)
			return -ENOMEM;
		alarm_init(cm_timer, ALARM_BOOTTIME, NULL);
	}

	/*
	 * Some of the following do not need to be errors.
	 * Users may intentionally ignore those features.
	 */
#ifndef VENDOR_KERNEL
	if (desc->fullbatt_uV == 0) {
		dev_info(&pdev->dev, "Ignoring full-battery voltage threshold as it is not supplied\n");
	}

	if (desc->fullbatt_uA == 0)
		dev_info(&pdev->dev, "Ignoring full-battery current threshold as it is not supplied\n");
#endif //not defined VENDOR_KERNEL

	if (!desc->fullbatt_vchkdrop_ms || !desc->fullbatt_vchkdrop_uV) {
		dev_info(&pdev->dev, "Disabling full-battery voltage drop checking mechanism as it is not supplied\n");
		desc->fullbatt_vchkdrop_ms = 0;
		desc->fullbatt_vchkdrop_uV = 0;
	}
	if (desc->fullbatt_soc == 0) {
		dev_info(&pdev->dev, "Ignoring full-battery soc(state of charge) threshold as it is not supplied\n");
	}
	if (desc->fullbatt_full_capacity == 0) {
		dev_info(&pdev->dev, "Ignoring full-battery full capacity threshold as it is not supplied\n");
	}

	if (!desc->charger_regulators || desc->num_charger_regulators < 1) {
		dev_err(&pdev->dev, "charger_regulators undefined\n");
		return -EINVAL;
	}

	if (!desc->psy_charger_stat || !desc->psy_charger_stat[0]) {
		dev_err(&pdev->dev, "No power supply defined\n");
		return -EINVAL;
	}

	if (!desc->psy_fuel_gauge) {
		dev_err(&pdev->dev, "No fuel gauge power supply defined\n");
		return -EINVAL;
	}

	ret = get_boot_mode();
	if (ret) {
		pr_err("boot_mode can't not parse bootargs property\n");
		return ret;
	}

	ret = get_prj_name_setup();
	if (ret) {
		pr_err("prj_name can't not parse bootargs property\n");
		return ret;
	}

	cm->bat_id_cha= devm_iio_channel_get(&pdev->dev, "bat-id-vol");
	if (IS_ERR(cm->bat_id_cha)) {
		dev_err(&pdev->dev, "failed to get bat-id-vol IIO channel\n");
		return PTR_ERR(cm->bat_id_cha);
	}

	cm_get_bat_id(cm);

	/* Check if charger's supplies are present at probe */
	for (i = 0; desc->psy_charger_stat[i]; i++) {
		struct power_supply *psy;

		psy = power_supply_get_by_name(desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(&pdev->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_charger_stat[i]);
			return -EPROBE_DEFER;
		}
		power_supply_put(psy);
	}

	if (cm->desc->polling_mode != CM_POLL_DISABLE &&
	    (desc->polling_interval_ms == 0 ||
	     msecs_to_jiffies(desc->polling_interval_ms) <= CM_JIFFIES_SMALL)) {
		dev_err(&pdev->dev, "polling_interval_ms is too small\n");
		return -EINVAL;
	}

	if (!desc->charging_max_duration_ms ||
			!desc->discharging_max_duration_ms) {
		dev_info(&pdev->dev, "Cannot limit charging duration checking mechanism to prevent overcharge/overheat and control discharging duration\n");
		desc->charging_max_duration_ms = 0;
		desc->discharging_max_duration_ms = 0;
	}

	if (!desc->charge_voltage_max || !desc->charge_voltage_drop) {
		dev_info(&pdev->dev, "Cannot validate charge voltage\n");
		desc->charge_voltage_max = 0;
		desc->charge_voltage_drop = 0;
	}

	platform_set_drvdata(pdev, cm);

	memcpy(&cm->charger_psy_desc, &psy_default, sizeof(psy_default));

	if (!desc->psy_name)
		strncpy(cm->psy_name_buf, psy_default.name, PSY_NAME_MAX);
	else
		strncpy(cm->psy_name_buf, desc->psy_name, PSY_NAME_MAX);
	cm->charger_psy_desc.name = cm->psy_name_buf;

	/* Allocate for psy properties because they may vary */
	cm->charger_psy_desc.properties =
		devm_kcalloc(&pdev->dev,
			     ARRAY_SIZE(default_charger_props) +
				NUM_CHARGER_PSY_OPTIONAL,
			     sizeof(enum power_supply_property), GFP_KERNEL);
	if (!cm->charger_psy_desc.properties)
		return -ENOMEM;

	memcpy(cm->charger_psy_desc.properties, default_charger_props,
		sizeof(enum power_supply_property) *
		ARRAY_SIZE(default_charger_props));
	cm->charger_psy_desc.num_properties = psy_default.num_properties;

	/* Find which optional psy-properties are available */
	fuel_gauge = power_supply_get_by_name(desc->psy_fuel_gauge);
	if (!fuel_gauge) {
		dev_err(&pdev->dev, "Cannot find power supply \"%s\"\n",
			desc->psy_fuel_gauge);
		return -EPROBE_DEFER;
	}

	if (!power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CHARGE_NOW, &val)) {
		if (!cm_add_battery_psy_property(cm, POWER_SUPPLY_PROP_CHARGE_NOW))
			dev_warn(&pdev->dev, "POWER_SUPPLY_PROP_CHARGE_NOW is present\n");
	}

	val.intval = CM_IBAT_CURRENT_NOW_CMD;
	if (!power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CURRENT_NOW, &val)) {
		if (!cm_add_battery_psy_property(cm, POWER_SUPPLY_PROP_CURRENT_NOW))
			dev_warn(&pdev->dev, "POWER_SUPPLY_PROP_CURRENT_NOW is present\n");
	}

	ret = get_boot_cap(cm, &cm->desc->cap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get initial battery capacity\n");
		return ret;
	}

	cm->desc->thm_info.thm_adjust_cur = -EINVAL;
	cm->desc->ir_comp.ibat_buf[CM_IBAT_BUFF_CNT - 1] = CM_MAGIC_NUM;
	cm->desc->ir_comp.us_lower_limit = cm->desc->ir_comp.us;

	if (device_property_read_bool(&pdev->dev, "cm-support-linear-charge"))
		cm->desc->thm_info.need_calib_charge_lmt = true;

	ret = cm_get_battery_temperature_by_psy(cm, &cm->desc->temperature);
	if (ret) {
		dev_err(cm->dev, "failed to get battery temperature\n");
		return ret;
	}

	cur_time = ktime_to_timespec64(ktime_get_boottime());
	cm->desc->update_capacity_time = cur_time.tv_sec;
	cm->desc->last_query_time = cur_time.tv_sec;

	ret = cm_init_thermal_data(cm, fuel_gauge);
	if (ret) {
		dev_err(&pdev->dev, "Failed to initialize thermal data\n");
		cm->desc->measure_battery_temp = false;
	}
	power_supply_put(fuel_gauge);

	INIT_DELAYED_WORK(&cm->fullbatt_vchk_work, fullbatt_vchk);
	INIT_DELAYED_WORK(&cm->cap_update_work, cm_batt_works);
	INIT_DELAYED_WORK(&cm->fixed_fchg_work, cm_fixed_fchg_work);
	INIT_DELAYED_WORK(&cm->cp_work, cm_cp_work);
	INIT_DELAYED_WORK(&cm->ir_compensation_work, cm_ir_compensation_works);
#ifndef VENDOR_KERNEL
	INIT_DELAYED_WORK(&cm->get_charger_type_work, get_charger_type);
#endif //not defined VENDOR_KERNEL

	/*odm.hq.bsp.luowenjiang@huaqin.com 2021.11.04 modify for usb temp protection start*/
	#ifdef CONFIG_HQ_USB_TEMP_CHECK
	if (false == bbat_mode && (prj_name == 2 || prj_name == 3)) {
		my_cm = cm;
		cm->usb_conn_ntc_1 = devm_iio_channel_get(&pdev->dev, "usb-temp-1");
		if (IS_ERR(cm->usb_conn_ntc_1)) {
			dev_err(&pdev->dev, "[ADC]failed to get usb-temp-1 IIO channel\n");
			return PTR_ERR(cm->usb_conn_ntc_1);
		}

		cm->usb_conn_ntc_2 = devm_iio_channel_get(&pdev->dev, "usb-temp-2");
		if (IS_ERR(cm->usb_conn_ntc_2)) {
			dev_err(&pdev->dev, "[ADC]failed to get usb-temp-2 IIO channel\n");
			return PTR_ERR(cm->usb_conn_ntc_2);
		}
		ret = cm_parse_gpio(cm);
		if (ret < 0) {
			dev_err(&pdev->dev, "[ADC]failed to get charge_gpio\n");
			return -EINVAL;
		}
		cm_set_charge_gpio(cm,GPIO_LOW);
		cm_usb_temp_thread_init(my_cm);
		INIT_DELAYED_WORK(&cm->usb_temp_work, cm_usb_temp_revovery_work);
		/*odm.hq.bsp.luowenjiang@huaqin.com 2022.01.08 modify for cc short to vbus start*/
		INIT_DELAYED_WORK(&cm->cc_vbus_check_work,cm_cc_vbus_check_work);
		schedule_delayed_work(&cm->cc_vbus_check_work, msecs_to_jiffies(5000));
		/*odm.hq.bsp.luowenjiang@huaqin.com 2022.01.08 modify for cc short to vbus end*/
	}
	#endif
	/*odm.hq.bsp.luowenjiang@huaqin.com 2021.11.04 modify for usb temp protection end*/

	mutex_init(&cm->desc->charge_info_mtx);

	/* Register sysfs entry for charger(regulator) */
	ret = charger_manager_prepare_sysfs(cm);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Cannot prepare sysfs entry of regulators\n");
		return ret;
	}
	psy_cfg.attr_grp = desc->sysfs_groups;
	psy_cfg.of_node = np;

	cm->charger_psy = power_supply_register(&pdev->dev,
						&cm->charger_psy_desc,
						&psy_cfg);
	if (IS_ERR(cm->charger_psy)) {
		dev_err(&pdev->dev, "Cannot register charger-manager with name \"%s\"\n",
			cm->charger_psy_desc.name);
		return PTR_ERR(cm->charger_psy);
	}
	cm->charger_psy->supplied_to = charger_manager_supplied_to;
	cm->charger_psy->num_supplicants =
		ARRAY_SIZE(charger_manager_supplied_to);

	wireless_main.psy = power_supply_register(&pdev->dev, &wireless_main.psd, NULL);
	if (IS_ERR(wireless_main.psy)) {
		dev_err(&pdev->dev, "Cannot register wireless_main.psy with name \"%s\"\n",
			wireless_main.psd.name);
		return PTR_ERR(wireless_main.psy);

	}

	ac_main.psy = power_supply_register(&pdev->dev, &ac_main.psd, NULL);
	if (IS_ERR(ac_main.psy)) {
		dev_err(&pdev->dev, "Cannot register usb_main.psy with name \"%s\"\n",
			ac_main.psd.name);
		return PTR_ERR(ac_main.psy);

	}

	usb_main.psy = power_supply_register(&pdev->dev, &usb_main.psd, NULL);
	if (IS_ERR(usb_main.psy)) {
		dev_err(&pdev->dev, "Cannot register usb_main.psy with name \"%s\"\n",
			usb_main.psd.name);
		return PTR_ERR(usb_main.psy);

	}

	/* Register extcon device for charger cable */
	ret = charger_manager_register_extcon(cm);
	if (ret < 0) {
		dev_err(&pdev->dev, "Cannot initialize extcon device\n");
		goto err_reg_extcon;
	}

#ifndef VENDOR_KERNEL
	ret = sysfs_create_group(&(pdev->dev.kobj), &bbat_attribute_group);
	if (ret < 0) {
		dev_err(&pdev->dev,"sysfs_create_group failed!\n");
	}
#endif //not defined VENDOR_KERNEL
	/* Add to the list */
	mutex_lock(&cm_list_mtx);
	list_add(&cm->entry, &cm_list);
	mutex_unlock(&cm_list_mtx);

	/*
	 * Charger-manager is capable of waking up the system from sleep
	 * when event is happened through cm_notify_event()
	 */
	device_init_wakeup(&pdev->dev, true);
	device_set_wakeup_capable(&pdev->dev, false);
	cm->charge_ws = wakeup_source_create("charger_manager_wakelock");
	wakeup_source_add(cm->charge_ws);
	mutex_init(&cm->desc->charger_type_mtx);

	ret = cm_get_bat_info(cm);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get battery information\n");
		goto err_reg_extcon;
	}

	cm->cm_charge_vote = sprd_charge_vote_register("cm_charge_vote",
						       cm_sprd_vote_callback,
						       cm,
						       &cm->charger_psy->dev);
	if (IS_ERR(cm->cm_charge_vote)) {
		dev_err(&pdev->dev, "Failed to register charge vote, ret = %d\n", ret);
		goto err_reg_extcon;
	}

	cm_init_basp_parameter(cm);

	if (cm_event_num > 0) {
		for (i = 0; i < cm_event_num; i++)
			cm_notify_type_handle(cm, cm_event_type[i], cm_event_msg[i]);
		cm_event_num = 0;
	}
	/*
	 * Charger-manager have to check the charging state right after
	 * initialization of charger-manager and then update current charging
	 * state.
	 */
	cm_monitor();

	schedule_work(&setup_polling);
	set_batt_charge_status(ATO_SET, CM_ATO_CHARGE_STATUS, 1);//znf add for err.must del

	queue_delayed_work(system_power_efficient_wq, &cm->cap_update_work, CM_CAP_CYCLE_TRACK_TIME * HZ);
	INIT_DELAYED_WORK(&cm->uvlo_work, cm_uvlo_check_work);

	return 0;

err_reg_extcon:
	for (i = 0; i < desc->num_charger_regulators; i++)
		regulator_put(desc->charger_regulators[i].consumer);

	power_supply_unregister(cm->charger_psy);
	wakeup_source_remove(cm->charge_ws);

	return ret;
}

static int charger_manager_remove(struct platform_device *pdev)
{
	struct charger_manager *cm = platform_get_drvdata(pdev);
	struct charger_desc *desc = cm->desc;
	int i = 0;

	/* Remove from the list */
	mutex_lock(&cm_list_mtx);
	list_del(&cm->entry);
	mutex_unlock(&cm_list_mtx);

	cancel_work_sync(&setup_polling);
	cancel_delayed_work_sync(&cm_monitor_work);
	cancel_delayed_work_sync(&cm->cap_update_work);
	cancel_delayed_work_sync(&cm->fullbatt_vchk_work);
	cancel_delayed_work_sync(&cm->uvlo_work);
#ifndef VENDOR_KERNEL
	cancel_delayed_work_sync(&cm->get_charger_type_work);
#endif //not defined VENDOR_KERNEL

	for (i = 0 ; i < desc->num_charger_regulators ; i++)
		regulator_put(desc->charger_regulators[i].consumer);

	power_supply_unregister(cm->charger_psy);

	try_charger_enable(cm, false);
	wakeup_source_remove(cm->charge_ws);

	return 0;
}

static void charger_manager_shutdown(struct platform_device *pdev)
{
	struct charger_manager *cm = platform_get_drvdata(pdev);

	if (cm->desc->uvlo_trigger_cnt < CM_UVLO_CALIBRATION_CNT_THRESHOLD)
		set_batt_cap(cm, cm->desc->cap);

	cancel_delayed_work_sync(&cm_monitor_work);
	cancel_delayed_work_sync(&cm->fullbatt_vchk_work);
	cancel_delayed_work_sync(&cm->cap_update_work);
#ifndef VENDOR_KERNEL
	cancel_delayed_work_sync(&cm->get_charger_type_work);
#endif //not defined VENDOR_KERNEL
	cancel_delayed_work_sync(&cm->ir_compensation_work);

}

static const struct platform_device_id charger_manager_id[] = {
	{ "charger-manager", 0 },
	{ },
};
MODULE_DEVICE_TABLE(platform, charger_manager_id);

static int cm_suspend_noirq(struct device *dev)
{
	if (device_may_wakeup(dev)) {
		device_set_wakeup_capable(dev, false);
		return -EAGAIN;
	}

	return 0;
}

static int cm_suspend_prepare(struct device *dev)
{
	struct charger_manager *cm = dev_get_drvdata(dev);

	if (!cm_suspended)
		cm_suspended = true;

	cm_timer_set = cm_setup_timer();

	if (cm_timer_set) {
		cancel_work_sync(&setup_polling);
		cancel_delayed_work_sync(&cm_monitor_work);
		cancel_delayed_work(&cm->fullbatt_vchk_work);
		cancel_delayed_work_sync(&cm->cap_update_work);
		cancel_delayed_work_sync(&cm->uvlo_work);
#ifndef VENDOR_KERNEL
		cancel_delayed_work_sync(&cm->get_charger_type_work);
#endif //not defined VENDOR_KERNEL
	}

	return 0;
}

static void cm_suspend_complete(struct device *dev)
{
	struct charger_manager *cm = dev_get_drvdata(dev);

	if (cm_suspended)
		cm_suspended = false;

	if (cm_timer_set) {
		ktime_t remain;

		alarm_cancel(cm_timer);
		cm_timer_set = false;
		remain = alarm_expires_remaining(cm_timer);
		if (remain > 0)
			cm_suspend_duration_ms -= ktime_to_ms(remain);
		schedule_work(&setup_polling);
	}

	_cm_monitor(cm);
	cm_batt_works(&cm->cap_update_work.work);

	/* Re-enqueue delayed work (fullbatt_vchk_work) */
	if (cm->fullbatt_vchk_jiffies_at) {
		unsigned long delay = 0;
		unsigned long now = jiffies + CM_JIFFIES_SMALL;

		if (time_after_eq(now, cm->fullbatt_vchk_jiffies_at)) {
			delay = (unsigned long)((long)now
				- (long)(cm->fullbatt_vchk_jiffies_at));
			delay = jiffies_to_msecs(delay);
		} else {
			delay = 0;
		}

		/*
		 * Account for cm_suspend_duration_ms with assuming that
		 * timer stops in suspend.
		 */
		if (delay > cm_suspend_duration_ms)
			delay -= cm_suspend_duration_ms;
		else
			delay = 0;

		queue_delayed_work(cm_wq, &cm->fullbatt_vchk_work,
				   msecs_to_jiffies(delay));
	}
	device_set_wakeup_capable(cm->dev, false);
}

static const struct dev_pm_ops charger_manager_pm = {
	.prepare	= cm_suspend_prepare,
	.suspend_noirq	= cm_suspend_noirq,
	.complete	= cm_suspend_complete,
};

static struct platform_driver charger_manager_driver = {
	.driver = {
		.name = "charger-manager",
		.pm = &charger_manager_pm,
		.of_match_table = charger_manager_match,
	},
	.probe = charger_manager_probe,
	.remove = charger_manager_remove,
	.shutdown = charger_manager_shutdown,
	.id_table = charger_manager_id,
};

static int __init charger_manager_init(void)
{
	cm_wq = create_freezable_workqueue("charger_manager");
	if (unlikely(!cm_wq))
		return -ENOMEM;

	INIT_DELAYED_WORK(&cm_monitor_work, cm_monitor_poller);

	return platform_driver_register(&charger_manager_driver);
}
late_initcall(charger_manager_init);

static void __exit charger_manager_cleanup(void)
{
	destroy_workqueue(cm_wq);
	cm_wq = NULL;

	platform_driver_unregister(&charger_manager_driver);
}
module_exit(charger_manager_cleanup);

/**
 * cm_notify_type_handle - charger driver handle charger event
 * @cm: the Charger Manager representing the battery
 * @type: type of charger event
 * @msg: optional message passed to uevent_notify function
 */
static void cm_notify_type_handle(struct charger_manager *cm, enum cm_event_types type, char *msg)
{
	switch (type) {
	case CM_EVENT_BATT_FULL:
		fullbatt_handler(cm);
		break;
	case CM_EVENT_BATT_IN:
	case CM_EVENT_BATT_OUT:
		battout_handler(cm);
		break;
	case CM_EVENT_WL_CHG_START_STOP:
	case CM_EVENT_EXT_PWR_IN_OUT ... CM_EVENT_CHG_START_STOP:
		misc_event_handler(cm, type);
		break;
	case CM_EVENT_FAST_CHARGE:
		fast_charge_handler(cm);
		break;
	case CM_EVENT_INT:
		cm_charger_int_handler(cm);
		break;
	case CM_EVENT_UNKNOWN:
	case CM_EVENT_OTHERS:
	default:
		dev_err(cm->dev, "%s: type not specified\n", __func__);
		break;
	}

	power_supply_changed(cm->charger_psy);

}

/**
 * cm_notify_event - charger driver notify Charger Manager of charger event
 * @psy: pointer to instance of charger's power_supply
 * @type: type of charger event
 * @msg: optional message passed to uevent_notify function
 */
void cm_notify_event(struct power_supply *psy, enum cm_event_types type,
		     char *msg)
{
	struct charger_manager *cm;
	bool found_power_supply = false;

	if (psy == NULL)
		return;

	mutex_lock(&cm_list_mtx);
	list_for_each_entry(cm, &cm_list, entry) {
		if (cm->desc->psy_charger_stat) {
			if (match_string(cm->desc->psy_charger_stat, -1,
					 psy->desc->name) >= 0) {
				found_power_supply = true;
				break;
			}
		}

		if (cm->desc->psy_fast_charger_stat) {
			if (match_string(cm->desc->psy_fast_charger_stat, -1,
					 psy->desc->name) >= 0) {
				found_power_supply = true;
				break;
			}
		}

		if (cm->desc->psy_fuel_gauge) {
			/*
			 * fgu has only one string and no null pointer at the end,
			 * only needs to compare once before exiting th loop, so 1 here and -1 elsewhere.
			 */
			if (match_string(&cm->desc->psy_fuel_gauge, 1,
					 psy->desc->name) >= 0) {
				found_power_supply = true;
				break;
			}
		}

		if (cm->desc->psy_cp_stat) {
			if (match_string(cm->desc->psy_cp_stat, -1,
					 psy->desc->name) >= 0) {
				found_power_supply = true;
				break;
			}
		}

		if (cm->desc->psy_wl_charger_stat) {
			if (match_string(cm->desc->psy_wl_charger_stat, -1,
					 psy->desc->name) >= 0) {
				found_power_supply = true;
				break;
			}
		}
	}
	mutex_unlock(&cm_list_mtx);

	if (!found_power_supply || !cm->cm_charge_vote) {
		if (cm_event_num < CM_EVENT_TYPE_NUM) {
			cm_event_msg[cm_event_num] = msg;
			cm_event_type[cm_event_num++] = type;
		} else {
			pr_err("%s: too many cm_event_num!!\n", __func__);
		}
		return;
	}

	cm_notify_type_handle(cm, type, msg);
}
EXPORT_SYMBOL_GPL(cm_notify_event);

MODULE_AUTHOR("MyungJoo Ham <myungjoo.ham@samsung.com>");
MODULE_DESCRIPTION("Charger Manager");
MODULE_LICENSE("GPL");
