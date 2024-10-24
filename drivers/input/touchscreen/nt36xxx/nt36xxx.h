/*
 * Copyright (C) 2010 - 2021 Novatek, Inc.
 *
 * $Revision: 85753 $
 * $Date: 2021-07-27 17:21:08 +0800 (週二, 27 七月 2021) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#ifndef 	_LINUX_NVT_TOUCH_H
#define		_LINUX_NVT_TOUCH_H

#include <linux/delay.h>
#include <linux/input.h>
#include <linux/of.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include "linux/hardware_info.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "nt36xxx_mem_map.h"

#ifdef CONFIG_MTK_SPI
/* Please copy mt_spi.h file under mtk spi driver folder */
#include "mt_spi.h"
#endif

#ifdef CONFIG_SPI_MT65XX
#include <linux/platform_data/spi-mt65xx.h>
#endif

#define NVT_DEBUG 1

//---GPIO number---
#define NVTTOUCH_RST_PIN 980
#define NVTTOUCH_INT_PIN 943


//---INT trigger mode---
//#define IRQ_TYPE_EDGE_RISING 1
//#define IRQ_TYPE_EDGE_FALLING 2
#define INT_TRIGGER_TYPE IRQ_TYPE_EDGE_FALLING


//---SPI driver info.---
#define NVT_SPI_NAME "NVT-ts"

#if NVT_DEBUG
#define NVT_LOG(fmt, args...)    pr_err("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)
#else
#define NVT_LOG(fmt, args...)    pr_info("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)
#endif
#define NVT_ERR(fmt, args...)    pr_err("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)

//---Input device info.---
#define NVT_TS_NAME "NVTCapacitiveTouchScreen"
#define NVT_PEN_NAME "NVTCapacitivePen"

//---Touch info.---
#define TOUCH_DEFAULT_MAX_WIDTH  (1080)
#define TOUCH_DEFAULT_MAX_HEIGHT (2408)
#define TOUCH_MAX_FINGER_NUM 10
#define TOUCH_KEY_NUM 0
#if TOUCH_KEY_NUM > 0
extern const uint16_t touch_key_array[TOUCH_KEY_NUM];
#endif
#define TOUCH_FORCE_NUM 1000
//---for Pen---
#define PEN_PRESSURE_MAX (4095)
#define PEN_DISTANCE_MAX (1)
#define PEN_TILT_MIN (-60)
#define PEN_TILT_MAX (60)

/* Enable only when module have tp reset pin and connected to host */
#define NVT_TOUCH_SUPPORT_HW_RST 0

//---Customerized func.---
#define NVT_TOUCH_PROC 1
#define NVT_TOUCH_EXT_PROC 1
#define NVT_TOUCH_MP 1
#define MT_PROTOCOL_B 1
#define NVT_TOUCH_FW_DEBUG_INFO 0
#define WAKEUP_GESTURE 1
#if WAKEUP_GESTURE
//extern const uint16_t gesture_key_array[];
#endif
struct coordinate {
	uint16_t x;
	uint16_t y;
};

struct gesture_info {
	uint8_t gesture_type;
	uint8_t clockwise;
	struct coordinate Point_start;
	struct coordinate Point_end;
	struct coordinate Point_1st;
	struct coordinate Point_2nd;
	struct coordinate Point_3rd;
	struct coordinate Point_4th;
};

struct oppo_debug_info {
	struct coordinate coordinate[10];
};

struct oppo_debug_gesture_record_info {
	struct coordinate coordinate[1024];
};

#define FIRMWARE_NAME_LENGTH_MAX 64
#define BOOT_UPDATE_FIRMWARE 1
#define BOOT_UPDATE_FIRMWARE_NAME "novatek_ts_fw.bin"
#define MP_UPDATE_FIRMWARE_NAME   "novatek_ts_mp.bin"
/**********************************************************/
//TM
#define TM_BOOT_UPDATE_FIRMWARE_NAME "novatek_tm_fw.bin"
#define TM_MP_UPDATE_FIRMWARE_NAME   "novatek_tm_mp.bin"

//CSOT
#define CSOT_BOOT_UPDATE_FIRMWARE_NAME "novatek_cs_fw.bin"
#define CSOT_MP_UPDATE_FIRMWARE_NAME "novatek_cs_mp.bin"
/**********************************************************/

#define POINT_DATA_CHECKSUM 1
#define POINT_DATA_CHECKSUM_LEN 65
#define POINT_DATA_LEN 120
#define NVT_PM_WAIT_SPI_I2C_RESUME_COMPLETE 1

//---ESD Protect.---
#define NVT_TOUCH_ESD_PROTECT 0
#define NVT_TOUCH_ESD_CHECK_PERIOD 2000	/* ms */
#define NVT_TOUCH_WDT_RECOVERY 1

#define CHECK_PEN_DATA_CHECKSUM 0

//---OPPO Customized Info---
#define OPPO_TOUCHPANEL_NAME "touchpanel"
#define OPPO_DEBUG_INFO "debug_info"

struct nvt_fw_debug_info {
	uint8_t rek_info;
	uint8_t rst_info;
	uint8_t hopping;
	uint8_t esd;
	uint8_t palm;
	uint8_t bending;
	uint8_t water;
	uint8_t gnd;
	uint8_t er;
	uint8_t fog;
	uint8_t film;
	uint8_t notch;
};

struct nvt_ts_data {
	struct spi_device *client;
	struct input_dev *input_dev;
	struct delayed_work nvt_fwu_work;
	uint16_t addr;
	int8_t phys[32];
#if defined(CONFIG_FB)
#if defined(CONFIG_DRM_PANEL)
	struct notifier_block drm_panel_notif;
#elif defined(_MSM_DRM_NOTIFY_H_)
	struct notifier_block drm_notif;
#else
	struct notifier_block fb_notif;
#endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
	uint8_t fw_ver;
	uint8_t x_num;
	uint8_t y_num;
	uint16_t abs_x_max;
	uint16_t abs_y_max;
	uint8_t max_touch_num;
	uint8_t max_button_num;
	uint32_t int_trigger_type;
	int32_t irq_gpio;
	uint32_t irq_flags;
	int32_t reset_gpio;
	uint32_t reset_flags;
	struct mutex lock;
	const struct nvt_ts_mem_map *mmap;
	uint8_t hw_crc;
	uint16_t nvt_pid;
	uint8_t *rbuf;
	uint8_t *xbuf;
	struct mutex xbuf_lock;
	bool irq_enabled;
	bool pen_support;
	bool stylus_resol_double;
	uint8_t x_gang_num;
	uint8_t y_gang_num;
	struct input_dev *pen_input_dev;
	int8_t pen_phys[32];
#ifdef CONFIG_MTK_SPI
	struct mt_chip_conf spi_ctrl;
#endif
#ifdef CONFIG_SPI_MT65XX
    struct mtk_chip_config spi_ctrl;
#endif
	char lcd_name_for_tp[50];
	char *tp_fw_name;
	char *mp_fw_name;
	char *mp_criteria;
	uint8_t is_suspended;
	uint8_t limit_edge;
	uint8_t game_mode;
	uint8_t is_usb_checked;
	uint8_t is_headset_checked;
	uint8_t debug_level;
	struct gesture_info gesture;
	uint8_t gesture_enable_by_suspend;
	uint8_t gesture_enable;
	uint8_t fw_debug_info_enable;
	struct oppo_debug_info oppo_debug_info;
	struct oppo_debug_gesture_record_info oppo_debug_gesture_record_info;
	struct nvt_fw_debug_info nvt_fw_debug_info;
    struct proc_dir_entry *oppo_touchpanel_proc;
	struct proc_dir_entry *debug_info;
#if NVT_PM_WAIT_SPI_I2C_RESUME_COMPLETE
	bool dev_pm_suspend;
	struct completion dev_pm_resume_completion;
#endif
};

#if NVT_TOUCH_PROC
struct nvt_flash_data{
	rwlock_t lock;
};
#endif

typedef enum {
	RESET_STATE_INIT = 0xA0,// IC reset
	RESET_STATE_REK,		// ReK baseline
	RESET_STATE_REK_FINISH,	// baseline is ready
	RESET_STATE_NORMAL_RUN,	// normal run
	RESET_STATE_MAX  = 0xAF
} RST_COMPLETE_STATE;

typedef enum {
    EVENT_MAP_HOST_CMD                      = 0x50,
    EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE   = 0x51,
    EVENT_MAP_RESET_COMPLETE                = 0x60,
    EVENT_MAP_FWINFO                        = 0x78,
    EVENT_MAP_PROJECTID                     = 0x9A,
} SPI_EVENT_MAP;

typedef enum {
	MODE_EDGE = 0,
	MODE_CHARGE,
	MODE_GAME,
	MODE_HEADSET,
	MODE_HOPPING_POLLING,
	MODE_HOPPING_FIX_FREQ,
	MODE_WATER_POLLING
} NVT_CUSTOMIZED_MODE;

typedef enum {
    EDGE_REJECT_L = 0,
    EDGE_REJECT_H = 1,
    PWR_FLAG = 2,
	HOPPING_FIX_FREQ_FLAG = 3,
	HOPPING_POLLING_FLAG = 4,
	JITTER_FLAG = 6,
	HEADSET_FLAG = 7,
} CMD_OFFSET;

typedef enum {
    DEBUG_MESSAGE_FLAG = 0,
    DEBUG_GESTURE_COORDINATE_FLAG = 1,
    DEBUG_GESTURE_COORDINATE_RECORD_FLAG = 2
} CMD_EXTEND_OFFSET;

enum touch_direction {
    VERTICAL_SCREEN,
    LANDSCAPE_SCREEN_90,
    LANDSCAPE_SCREEN_270,
    LANDSCAPE_SCREEN_180,
};

enum touch_panel_vendor {
	TOUCH_TM = 0,
	TOUCH_CSOT,
	TOUCH_UNKNOW,
};

//---customized command---
#define HOST_CMD_PWR_PLUG_IN                  (0x53)
#define HOST_CMD_PWR_PLUG_OUT                 (0x51)
#define HOST_CMD_HOPPING_POLLING_ON           (0x73)
#define HOST_CMD_HOPPING_POLLING_OFF          (0x74)
#define HOST_CMD_HOPPING_FIX_FREQ_ON          (0x75)
#define HOST_CMD_HOPPING_FIX_FREQ_OFF         (0x76)
#define HOST_CMD_HEADSET_PLUG_IN              (0x77)
#define HOST_CMD_HEADSET_PLUG_OUT             (0x78)
#define HOST_CMD_EDGE_LIMIT_VERTICAL_REVERSE  (0x79)
#define HOST_CMD_EDGE_LIMIT_VERTICAL          (0x7A)
#define HOST_CMD_EDGE_LIMIT_LEFT_UP           (0x7B)
#define HOST_CMD_EDGE_LIMIT_RIGHT_UP          (0x7C)
#define HOST_CMD_JITTER_ON                    (0x7D)
#define HOST_CMD_JITTER_OFF                   (0x7E)

#define HOST_EXT_CMD                      (0x7F)
#define HOST_EXT_DBG_MSG_DIFF_ON          (0x01)
#define HOST_EXT_DBG_MSG_DIFF_OFF         (0x02)
#define HOST_EXT_DBG_WKG_COORD_ON         (0x03)
#define HOST_EXT_DBG_WKG_COORD_OFF        (0x04)
#define HOST_EXT_DBG_WKG_COORD_RECORD_ON  (0x05)
#define HOST_EXT_DBG_WKG_COORD_RECORD_OFF (0x06)
#define HOST_EXT_DBG_WATER_POLLING_ON     (0x07)
#define HOST_EXT_DBG_WATER_POLLING_OFF    (0x08)

//---SPI READ/WRITE---
#define SPI_WRITE_MASK(a)	(a | 0x80)
#define SPI_READ_MASK(a)	(a & 0x7F)

#define DUMMY_BYTES (1)
#define NVT_TRANSFER_LEN	(63*1024)
#define NVT_READ_LEN		(2*1024)
#define NVT_XBUF_LEN		(NVT_TRANSFER_LEN+1+DUMMY_BYTES)

typedef enum {
	NVTWRITE = 0,
	NVTREAD  = 1
} NVT_SPI_RW;

//---extern structures---
extern struct nvt_ts_data *ts;
extern int nvt_ic_type;


//---extern functions---
int32_t CTP_SPI_READ(struct spi_device *client, uint8_t *buf, uint16_t len);
int32_t CTP_SPI_WRITE(struct spi_device *client, uint8_t *buf, uint16_t len);
void nvt_bootloader_reset(void);
void nvt_eng_reset(void);
void nvt_sw_reset(void);
void nvt_sw_reset_idle(void);
void nvt_boot_ready(void);
void nvt_bld_crc_enable(void);
void nvt_fw_crc_enable(void);
void nvt_tx_auto_copy_mode(void);
int32_t nvt_update_firmware(char *firmware_name);
int32_t nvt_check_fw_reset_state(RST_COMPLETE_STATE check_reset_state);
int32_t nvt_get_fw_info(void);
int32_t nvt_clear_fw_status(void);
int32_t nvt_check_fw_status(void);
int32_t nvt_check_spi_dma_tx_info(void);
int32_t nvt_set_page(uint32_t addr);
int32_t nvt_write_addr(uint32_t addr, uint8_t data);
int32_t nvt_ts_resume(struct device *dev);
int32_t nvt_ts_suspend(struct device *dev);
#if NVT_TOUCH_ESD_PROTECT
extern void nvt_esd_check_enable(uint8_t enable);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

int32_t nvt_extend_cmd_store(uint8_t cmd, uint8_t subcmd);
int32_t nvt_mode_switch(NVT_CUSTOMIZED_MODE mode, uint8_t flag);
void nvt_mode_change_cmd(uint8_t cmd);
void nvt_read_mdata(uint32_t xdata_addr, uint32_t xdata_btn_addr);
void nvt_read_fw_history(uint32_t fw_history_addr);

#endif /* _LINUX_NVT_TOUCH_H */
