/*
 * Copyright (C) 2010 - 2018 Novatek, Inc.
 *
 * $Revision$
 * $Date$
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


#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>

#include "nt36xxx.h"
#include <asm/uaccess.h>

#if NVT_TOUCH_EXT_PROC
#define NVT_FW_VERSION "nvt_fw_version"
#define NVT_BASELINE "nvt_baseline"
#define NVT_RAW "nvt_raw"
#define NVT_DIFF "nvt_diff"
#define NVT_PEN_DIFF "nvt_pen_diff"

#define OPPO_BASELINE_TEST "baseline_test"
#define OPPO_COORDINATE "coordinate"
#define OPPO_DELTA "delta"
#define OPPO_BASELINE "baseline"
#define OPPO_MAIN_REGISTER "main_register"
#define OPPO_DEBUG_LEVEL "debug_level"
#define OPPO_GESTURE "double_tap_enable"
#define OPPO_IRQ_DEPATH "irq_depth"
#define OPPO_REGISTER_INFO "oppo_register_info"
#define OPPO_FW_UPDATE "tp_fw_update"
#define OPPO_GAME_SWITCH "game_switch_enable"
#define OPPO_TP_LIMIT_ENABLE "oppo_tp_limit_enable"
#define OPPO_DBG_MESSAGE_DIFF_ENABLE "dbg_message_diff_enable"
#define OPPO_DBG_FINGER_DOWN_DIFF "finger_down_diff"
#define OPPO_DBG_STATUS_CHANGE_DIFF "status_change_diff"
#define OPPO_DBG_GESTURE_COORD_RECORD_ENABLE "dbg_gesture_coord_record_enable"
#define OPPO_DBG_GESTURE_COORD_RECORD "dbg_gesture_coord_record"
#define OPPO_DBG_GESTURE_COORD_ENABLE "dbg_gesture_coord_enable"

static struct proc_dir_entry *oppo_coordinate;
static struct proc_dir_entry *oppo_delta;
static struct proc_dir_entry *oppo_baseline;
static struct proc_dir_entry *oppo_main_register;
static struct proc_dir_entry *oppo_debug_level;
static struct proc_dir_entry *oppo_gesture;
static struct proc_dir_entry *oppo_irq_depath;
static struct proc_dir_entry *register_info_oppo;
static struct proc_dir_entry *oppo_fw_update;
static struct proc_dir_entry *oppo_game_switch;
static struct proc_dir_entry *oppo_tp_limit_enable;
static struct proc_dir_entry *oppo_dbg_message_diff_enable_entry;
static struct proc_dir_entry *oppo_dbg_finger_down_diff_entry;
static struct proc_dir_entry *oppo_dbg_status_change_diff_entry;
static struct proc_dir_entry *oppo_dbg_gesture_coord_enable_entry;
static struct proc_dir_entry *oppo_dbg_gesture_coord_record_enable_entry;
static struct proc_dir_entry *oppo_dbg_gesture_coord_record_entry;



#define BUS_TRANSFER_LENGTH  256

#define NORMAL_MODE 0x00
#define TEST_MODE_1 0x21
#define TEST_MODE_2 0x22
#define HANDSHAKING_HOST_READY 0xBB

#define XDATA_SECTOR_SIZE   256

static uint8_t xdata_tmp[5000] = {0};
static int32_t xdata[2500] = {0};
static int32_t xdata_pen_tip_x[256] = {0};
static int32_t xdata_pen_tip_y[256] = {0};
static int32_t xdata_pen_ring_x[256] = {0};
static int32_t xdata_pen_ring_y[256] = {0};

static struct proc_dir_entry *NVT_proc_fw_version_entry;
static struct proc_dir_entry *NVT_proc_baseline_entry;
static struct proc_dir_entry *NVT_proc_raw_entry;
static struct proc_dir_entry *NVT_proc_diff_entry;
static struct proc_dir_entry *NVT_proc_pen_diff_entry;

/*******************************************************
Description:
	Novatek touchscreen change mode function.

return:
	n.a.
*******************************************************/
void nvt_change_mode(uint8_t mode)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---set mode---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = mode;
	CTP_SPI_WRITE(ts->client, buf, 2);

	if (mode == NORMAL_MODE) {
		usleep_range(20000, 20000);
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = HANDSHAKING_HOST_READY;
		CTP_SPI_WRITE(ts->client, buf, 2);
		usleep_range(20000, 20000);
	}
}

int32_t nvt_set_pen_inband_mode_1(uint8_t freq_idx, uint8_t x_term)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 5;

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---set mode---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0xC1;
	buf[2] = 0x02;
	buf[3] = freq_idx;
	buf[4] = x_term;
	CTP_SPI_WRITE(ts->client, buf, 5);

	for (i = 0; i < retry; i++) {
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0xFF;
		CTP_SPI_READ(ts->client, buf, 2);

		if (buf[1] == 0x00)
			break;

		usleep_range(10000, 10000);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

int32_t nvt_set_pen_normal_mode(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 5;

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---set mode---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0xC1;
	buf[2] = 0x04;
	CTP_SPI_WRITE(ts->client, buf, 3);

	for (i = 0; i < retry; i++) {
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0xFF;
		CTP_SPI_READ(ts->client, buf, 2);

		if (buf[1] == 0x00)
			break;

		usleep_range(10000, 10000);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*******************************************************
Description:
	Novatek touchscreen get firmware pipe function.

return:
	Executive outcomes. 0---pipe 0. 1---pipe 1.
*******************************************************/
uint8_t nvt_get_fw_pipe(void)
{
	uint8_t buf[8]= {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

	//---read fw status---
	buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
	buf[1] = 0x00;
	CTP_SPI_READ(ts->client, buf, 2);

	//NVT_LOG("FW pipe=%d, buf[1]=0x%02X\n", (buf[1]&0x01), buf[1]);

	return (buf[1] & 0x01);
}

/*******************************************************
Description:
	Novatek touchscreen read meta data function.

return:
	n.a.
*******************************************************/
void nvt_read_mdata(uint32_t xdata_addr, uint32_t xdata_btn_addr)
{
	int32_t i = 0;
	int32_t j = 0;
	int32_t k = 0;
	uint8_t buf[BUS_TRANSFER_LENGTH + 2] = {0};
	uint32_t head_addr = 0;
	int32_t dummy_len = 0;
	int32_t data_len = 0;
	int32_t residual_len = 0;

	//---set xdata sector address & length---
	head_addr = xdata_addr - (xdata_addr % XDATA_SECTOR_SIZE);
	dummy_len = xdata_addr - head_addr;
	data_len = ts->x_num * ts->y_num * 2;
	residual_len = (head_addr + dummy_len + data_len) % XDATA_SECTOR_SIZE;

	//printk("head_addr=0x%05X, dummy_len=0x%05X, data_len=0x%05X, residual_len=0x%05X\n", head_addr, dummy_len, data_len, residual_len);

	//read xdata : step 1
	for (i = 0; i < ((dummy_len + data_len) / XDATA_SECTOR_SIZE); i++) {
		//---change xdata index---
		nvt_set_page(head_addr + XDATA_SECTOR_SIZE * i);

		//---read xdata by BUS_TRANSFER_LENGTH
		for (j = 0; j < (XDATA_SECTOR_SIZE / BUS_TRANSFER_LENGTH); j++) {
			//---read data---
			buf[0] = BUS_TRANSFER_LENGTH * j;
			CTP_SPI_READ(ts->client, buf, BUS_TRANSFER_LENGTH + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < BUS_TRANSFER_LENGTH; k++) {
				xdata_tmp[XDATA_SECTOR_SIZE * i + BUS_TRANSFER_LENGTH * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04X\n", buf[k+1], (XDATA_SECTOR_SIZE*i + BUS_TRANSFER_LENGTH*j + k));
			}
		}
		//printk("addr=0x%05X\n", (head_addr+XDATA_SECTOR_SIZE*i));
	}

	//read xdata : step2
	if (residual_len != 0) {
		//---change xdata index---
		nvt_set_page(xdata_addr + data_len - residual_len);

		//---read xdata by BUS_TRANSFER_LENGTH
		for (j = 0; j < (residual_len / BUS_TRANSFER_LENGTH + 1); j++) {
			//---read data---
			buf[0] = BUS_TRANSFER_LENGTH * j;
			CTP_SPI_READ(ts->client, buf, BUS_TRANSFER_LENGTH + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < BUS_TRANSFER_LENGTH; k++) {
				xdata_tmp[(dummy_len + data_len - residual_len) + BUS_TRANSFER_LENGTH * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04x\n", buf[k+1], ((dummy_len+data_len-residual_len) + BUS_TRANSFER_LENGTH*j + k));
			}
		}
		//printk("addr=0x%05X\n", (xdata_addr+data_len-residual_len));
	}

	//---remove dummy data and 2bytes-to-1data---
	for (i = 0; i < (data_len / 2); i++) {
		xdata[i] = (int16_t)(xdata_tmp[dummy_len + i * 2] + 256 * xdata_tmp[dummy_len + i * 2 + 1]);
	}

#if TOUCH_KEY_NUM > 0
	//read button xdata : step3
	//---change xdata index---
	nvt_set_page(xdata_btn_addr);

	//---read data---
	buf[0] = (xdata_btn_addr & 0xFF);
	CTP_SPI_READ(ts->client, buf, (TOUCH_KEY_NUM * 2 + 1));

	//---2bytes-to-1data---
	for (i = 0; i < TOUCH_KEY_NUM; i++) {
		xdata[ts->x_num * ts->y_num + i] = (int16_t)(buf[1 + i * 2] + 256 * buf[1 + i * 2 + 1]);
	}
#endif

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);
}

/*******************************************************
Description:
	Novatek touchscreen read debug meta data function.

return:
	n.a.
*******************************************************/
void nvt_read_debug_mdata(uint32_t address)
{
	int32_t i = 0;
	int32_t j = 0;
	int32_t k = 0;
	uint8_t buf[BUS_TRANSFER_LENGTH + 1] = {0};
	uint32_t head_addr = 0;
	int32_t dummy_len = 0;
	int32_t data_len = 0;
	int32_t residual_len = 0;

	//---set xdata sector address & length---
	head_addr = address - (address % XDATA_SECTOR_SIZE);
	dummy_len = address - head_addr;
	data_len = ts->x_num * ts->y_num;
	residual_len = (head_addr + dummy_len + data_len) % XDATA_SECTOR_SIZE;

	//printk("head_addr=0x%05X, dummy_len=0x%05X, data_len=0x%05X, residual_len=0x%05X\n", head_addr, dummy_len, data_len, residual_len);

	//read xdata : step 1
	for (i = 0; i < ((dummy_len + data_len) / XDATA_SECTOR_SIZE); i++) {
		//---read xdata by BUS_TRANSFER_LENGTH
		for (j = 0; j < (XDATA_SECTOR_SIZE / BUS_TRANSFER_LENGTH); j++) {
			//---change xdata index---
			nvt_set_page(head_addr + (XDATA_SECTOR_SIZE * i) + (BUS_TRANSFER_LENGTH * j));

			//---read data---
			buf[0] = BUS_TRANSFER_LENGTH * j;
			CTP_SPI_READ(ts->client, buf, BUS_TRANSFER_LENGTH + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < BUS_TRANSFER_LENGTH; k++) {
				xdata_tmp[XDATA_SECTOR_SIZE * i + BUS_TRANSFER_LENGTH * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04X\n", buf[k+1], (XDATA_SECTOR_SIZE*i + BUS_TRANSFER_LENGTH*j + k));
			}
		}
		//printk("addr=0x%05X\n", (head_addr+XDATA_SECTOR_SIZE*i));
	}

	//read xdata : step2
	if (residual_len != 0) {
		//---read xdata by BUS_TRANSFER_LENGTH
		for (j = 0; j < (residual_len / BUS_TRANSFER_LENGTH + 1); j++) {
			//---change xdata index---
			nvt_set_page(address + data_len - residual_len + (BUS_TRANSFER_LENGTH * j));

			//---read data---
			buf[0] = BUS_TRANSFER_LENGTH * j;
			CTP_SPI_READ(ts->client, buf, BUS_TRANSFER_LENGTH + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < BUS_TRANSFER_LENGTH; k++) {
				xdata_tmp[(dummy_len + data_len - residual_len) + BUS_TRANSFER_LENGTH * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04x\n", buf[k+1], ((dummy_len+data_len-residual_len) + BUS_TRANSFER_LENGTH*j + k));
			}
		}
		//printk("addr=0x%05X\n", (address+data_len-residual_len));
	}

	//---remove dummy data and scaling data (*8)---
	for (i = 0; i < data_len ; i++) {
		xdata[i] = (int16_t) (((int8_t) xdata_tmp[dummy_len + i]) * 8);
	}

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);
}

/*******************************************************
Description:
    Novatek touchscreen get meta data function.

return:
    n.a.
*******************************************************/
void nvt_get_mdata(int32_t *buf, uint8_t *m_x_num, uint8_t *m_y_num)
{
    *m_x_num = ts->x_num;
    *m_y_num = ts->y_num;
    memcpy(buf, xdata, ((ts->x_num * ts->y_num + TOUCH_KEY_NUM) * sizeof(int32_t)));
}

/*******************************************************
Description:
	Novatek touchscreen read and get number of meta data function.

return:
	n.a.
*******************************************************/
void nvt_read_get_num_mdata(uint32_t xdata_addr, int32_t *buffer, uint32_t num)
{
	int32_t i = 0;
	int32_t j = 0;
	int32_t k = 0;
	uint8_t buf[BUS_TRANSFER_LENGTH + 2] = {0};
	uint32_t head_addr = 0;
	int32_t dummy_len = 0;
	int32_t data_len = 0;
	int32_t residual_len = 0;

	//---set xdata sector address & length---
	head_addr = xdata_addr - (xdata_addr % XDATA_SECTOR_SIZE);
	dummy_len = xdata_addr - head_addr;
	data_len = num * 2;
	residual_len = (head_addr + dummy_len + data_len) % XDATA_SECTOR_SIZE;

	//printk("head_addr=0x%05X, dummy_len=0x%05X, data_len=0x%05X, residual_len=0x%05X\n", head_addr, dummy_len, data_len, residual_len);

	//read xdata : step 1
	for (i = 0; i < ((dummy_len + data_len) / XDATA_SECTOR_SIZE); i++) {
		//---change xdata index---
		nvt_set_page(head_addr + XDATA_SECTOR_SIZE * i);

		//---read xdata by BUS_TRANSFER_LENGTH
		for (j = 0; j < (XDATA_SECTOR_SIZE / BUS_TRANSFER_LENGTH); j++) {
			//---read data---
			buf[0] = BUS_TRANSFER_LENGTH * j;
			CTP_SPI_READ(ts->client, buf, BUS_TRANSFER_LENGTH + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < BUS_TRANSFER_LENGTH; k++) {
				xdata_tmp[XDATA_SECTOR_SIZE * i + BUS_TRANSFER_LENGTH * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04X\n", buf[k+1], (XDATA_SECTOR_SIZE*i + BUS_TRANSFER_LENGTH*j + k));
			}
		}
		//printk("addr=0x%05X\n", (head_addr+XDATA_SECTOR_SIZE*i));
	}

	//read xdata : step2
	if (residual_len != 0) {
		//---change xdata index---
		nvt_set_page(xdata_addr + data_len - residual_len);

		//---read xdata by BUS_TRANSFER_LENGTH
		for (j = 0; j < (residual_len / BUS_TRANSFER_LENGTH + 1); j++) {
			//---read data---
			buf[0] = BUS_TRANSFER_LENGTH * j;
			CTP_SPI_READ(ts->client, buf, BUS_TRANSFER_LENGTH + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < BUS_TRANSFER_LENGTH; k++) {
				xdata_tmp[(dummy_len + data_len - residual_len) + BUS_TRANSFER_LENGTH * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04x\n", buf[k+1], ((dummy_len+data_len-residual_len) + BUS_TRANSFER_LENGTH*j + k));
			}
		}
		//printk("addr=0x%05X\n", (xdata_addr+data_len-residual_len));
	}

	//---remove dummy data and 2bytes-to-1data---
	for (i = 0; i < (data_len / 2); i++) {
		buffer[i] = (int16_t)(xdata_tmp[dummy_len + i * 2] + 256 * xdata_tmp[dummy_len + i * 2 + 1]);
	}

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);
}

/*******************************************************
Description:
	Novatek touchscreen firmware version show function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t c_fw_version_show(struct seq_file *m, void *v)
{
	seq_printf(m, "fw_ver=%d, x_num=%d, y_num=%d, button_num=%d\n", ts->fw_ver, ts->x_num, ts->y_num, ts->max_button_num);
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print show
	function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t c_show(struct seq_file *m, void *v)
{
	int32_t i = 0;
	int32_t j = 0;

	for (i = 0; i < ts->y_num; i++) {
		for (j = 0; j < ts->x_num; j++) {
			seq_printf(m, "%5d, ", xdata[i * ts->x_num + j]);
		}
		seq_puts(m, "\n");
	}

#if TOUCH_KEY_NUM > 0
	for (i = 0; i < TOUCH_KEY_NUM; i++) {
		seq_printf(m, "%5d, ", xdata[ts->x_num * ts->y_num + i]);
	}
	seq_puts(m, "\n");
#endif

	seq_printf(m, "\n\n");
	return 0;
}

/*******************************************************
Description:
	Novatek pen 1D diff xdata sequence print show
	function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t c_pen_1d_diff_show(struct seq_file *m, void *v)
{
	int32_t i = 0;

	seq_printf(m, "Tip X:\n");
	for (i = 0; i < ts->x_num; i++) {
		seq_printf(m, "%5d, ", xdata_pen_tip_x[i]);
	}
	seq_puts(m, "\n");
	seq_printf(m, "Tip Y:\n");
	for (i = 0; i < ts->y_num; i++) {
		seq_printf(m, "%5d, ", xdata_pen_tip_y[i]);
	}
	seq_puts(m, "\n");
	seq_printf(m, "Ring X:\n");
	for (i = 0; i < ts->x_num; i++) {
		seq_printf(m, "%5d, ", xdata_pen_ring_x[i]);
	}
	seq_puts(m, "\n");
	seq_printf(m, "Ring Y:\n");
	for (i = 0; i < ts->y_num; i++) {
		seq_printf(m, "%5d, ", xdata_pen_ring_y[i]);
	}
	seq_puts(m, "\n");

	seq_printf(m, "\n\n");
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print start
	function.

return:
	Executive outcomes. 1---call next function.
	NULL---not call next function and sequence loop
	stop.
*******************************************************/
static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos < 1 ? (void *)1 : NULL;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print next
	function.

return:
	Executive outcomes. NULL---no next and call sequence
	stop function.
*******************************************************/
static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print stop
	function.

return:
	n.a.
*******************************************************/
static void c_stop(struct seq_file *m, void *v)
{
	return;
}

const struct seq_operations nvt_fw_version_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_fw_version_show
};

const struct seq_operations nvt_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_show
};

const struct seq_operations nvt_pen_diff_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_pen_1d_diff_show
};

/* oppo_dbg_message_diff_enable */
static ssize_t oppo_dbg_message_diff_enable_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *ppos)
{
	unsigned int tmp = 0;
	int32_t ret = 0;
	char cmd[128] = {0};

	if(copy_from_user(cmd, buf, count)) {
		NVT_ERR("input value error\n");
		return -EINVAL;
	}

	if (kstrtouint(cmd, 0, &tmp)) {
		NVT_ERR("kstrtouint error\n");
		return -EINVAL;
	}

	NVT_LOG("debug message enable is %d\n", tmp);

	if (tmp) {
		/* enable */
		ret = nvt_extend_cmd_store(HOST_EXT_CMD, HOST_EXT_DBG_MSG_DIFF_ON);
		if (ret < 0) {
			NVT_ERR("%s: debug message enable failed.\n", __func__);
		} else {
			ts->fw_debug_info_enable = true;
		}
	} else {
		/* disable */
		ret = nvt_extend_cmd_store(HOST_EXT_CMD, HOST_EXT_DBG_MSG_DIFF_OFF);
		if (ret < 0) {
			NVT_ERR("%s: debug message disable failed.\n", __func__);
		} else {
			ts->fw_debug_info_enable = false;
		}
	}

	return count;
};

static const struct file_operations oppo_dbg_message_diff_enable_fops =
{
	.write = oppo_dbg_message_diff_enable_write,
	.owner = THIS_MODULE,
};

#define NVT_MMAP_DEBUG_FINGER_DOWN_DIFFDATA   0x263E4
#define NVT_MMAP_DEBUG_STATUS_CHANGE_DIFFDATA 0x2666C
static int32_t oppo_dbg_finger_down_diff_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_read_debug_mdata(NVT_MMAP_DEBUG_FINGER_DOWN_DIFFDATA);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_seq_ops);
}

static const struct file_operations oppo_dbg_finger_down_diff_fops = {
	.owner = THIS_MODULE,
	.open = oppo_dbg_finger_down_diff_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static int32_t oppo_dbg_status_change_diff_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_read_debug_mdata(NVT_MMAP_DEBUG_STATUS_CHANGE_DIFFDATA);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_seq_ops);
}

static const struct file_operations oppo_dbg_status_change_diff_fops = {
	.owner = THIS_MODULE,
	.open = oppo_dbg_status_change_diff_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/* oppo_dbg_gesture_coord_record_enable */
static ssize_t oppo_dbg_gesture_coord_record_enable_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *ppos)
{
	unsigned int tmp = 0;
	int32_t ret = 0;
	char cmd[128] = {0};

	if(copy_from_user(cmd, buf, count)) {
		NVT_ERR("input value error\n");
		return -EINVAL;
	}

	if (kstrtouint(cmd, 0, &tmp)) {
		NVT_ERR("kstrtouint error\n");
		return -EINVAL;
	}

	NVT_LOG("debug gesture coordinate record enable is %d\n", tmp);

	if (tmp) {
		/* enable */
		ret = nvt_extend_cmd_store(HOST_EXT_CMD, HOST_EXT_DBG_WKG_COORD_RECORD_ON);
		if (ret < 0) {
			NVT_ERR("%s: debug gesture coordinate record enable failed.\n", __func__);
		}
	} else {
		/* disable */
		ret = nvt_extend_cmd_store(HOST_EXT_CMD, HOST_EXT_DBG_WKG_COORD_RECORD_OFF);
		if (ret < 0) {
			NVT_ERR("%s: debug gesture coordinate record disable failed.\n", __func__);
		}
	}

	return count;
};

static const struct file_operations oppo_dbg_gesture_coord_record_enable_fops =
{
	.write = oppo_dbg_gesture_coord_record_enable_write,
	.owner = THIS_MODULE,
};

static int32_t gesture_buf[1024] = {0};
/* oppo_dbg_gesture_coord_record_open */
static int32_t c_oppo_dbg_gesture_coord_record_show(struct seq_file *m, void *v)
{
	struct oppo_debug_gesture_record_info *gesture_record = &ts->oppo_debug_gesture_record_info;
	uint32_t i = 0;
	char tmp[256] = {0};

	for (i = 0 ; i < gesture_buf[1] ; i++) {
		sprintf(tmp, "(%4d,%4d) ", gesture_record->coordinate[i].x, gesture_record->coordinate[i].y);
		seq_printf(m, "%s", tmp);
		if ((i % 10) == 9) {
			seq_printf(m, "\n");
		}
	}
	seq_printf(m, "\n");


    return 0;
}

const struct seq_operations oppo_dbg_gesture_coornidate_record_seq_ops = {
    .start  = c_start,
    .next   = c_next,
    .stop   = c_stop,
    .show   = c_oppo_dbg_gesture_coord_record_show
};

void nvt_read_debug_gesture_coordinate_buffer(uint32_t address, uint32_t xdata_len)
{
	int32_t i = 0;
	int32_t j = 0;
	int32_t k = 0;
	uint8_t buf[BUS_TRANSFER_LENGTH + 1] = {0};
	uint32_t head_addr = 0;
	int32_t dummy_len = 0;
	int32_t data_len = xdata_len;
	int32_t residual_len = 0;

	//---set xdata sector address & length---
	head_addr = address - (address % XDATA_SECTOR_SIZE);
	dummy_len = address - head_addr;
	//data_len = ts->x_num * ts->y_num;
	residual_len = (head_addr + dummy_len + data_len) % XDATA_SECTOR_SIZE;

	//read xdata : step 1
	for (i = 0; i < ((dummy_len + data_len) / XDATA_SECTOR_SIZE); i++) {
		//---read xdata by BUS_TRANSFER_LENGTH
		for (j = 0; j < (XDATA_SECTOR_SIZE / BUS_TRANSFER_LENGTH); j++) {
			//---change xdata index---
			nvt_set_page(head_addr + (XDATA_SECTOR_SIZE * i) + (BUS_TRANSFER_LENGTH * j));

			//---read data---
			buf[0] = BUS_TRANSFER_LENGTH * j;
			CTP_SPI_READ(ts->client, buf, BUS_TRANSFER_LENGTH + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < BUS_TRANSFER_LENGTH; k++) {
				xdata_tmp[XDATA_SECTOR_SIZE * i + BUS_TRANSFER_LENGTH * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04X\n", buf[k+1], (XDATA_SECTOR_SIZE*i + BUS_TRANSFER_LENGTH*j + k));
			}
		}
		//printk("addr=0x%05X\n", (head_addr+XDATA_SECTOR_SIZE*i));
	}

	//read xdata : step2
	if (residual_len != 0) {
		//---read xdata by BUS_TRANSFER_LENGTH
		for (j = 0; j < (residual_len / BUS_TRANSFER_LENGTH + 1); j++) {
			//---change xdata index---
			nvt_set_page(address + data_len - residual_len + (BUS_TRANSFER_LENGTH * j));

			//---read data---
			buf[0] = BUS_TRANSFER_LENGTH * j;
			CTP_SPI_READ(ts->client, buf, BUS_TRANSFER_LENGTH + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < BUS_TRANSFER_LENGTH; k++) {
				xdata_tmp[(dummy_len + data_len - residual_len) + BUS_TRANSFER_LENGTH * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04x\n", buf[k+1], ((dummy_len+data_len-residual_len) + BUS_TRANSFER_LENGTH*j + k));
			}
		}
		//printk("addr=0x%05X\n", (address+data_len-residual_len));
	}

	//---remove dummy data---
	for (i = 0; i < data_len ; i++) {
		xdata[i] = xdata_tmp[dummy_len + i];
	}

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);
}

static int32_t oppo_dbg_gesture_coord_record_open(struct inode *inode, struct file *file)
{
	uint32_t input_x = 0;
	uint32_t input_y = 0;
	uint32_t i;
	uint8_t points_num[2] = {0};
	uint8_t data_len[2] = {0};

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	/* 1st finger */
	nvt_read_debug_gesture_coordinate_buffer(NVT_MMAP_DEBUG_FINGER_DOWN_DIFFDATA, ts->x_num * ts->y_num);
	points_num[0] = xdata[0];
	data_len[0] = 3*points_num[0];
	memcpy(&gesture_buf[2], &xdata[1], data_len[0] * sizeof(int32_t));

	/* 2nd finger */
	nvt_read_debug_gesture_coordinate_buffer(NVT_MMAP_DEBUG_STATUS_CHANGE_DIFFDATA, ts->x_num * ts->y_num);
	points_num[1] = xdata[0];
	data_len[1] = 3*points_num[1];
	memcpy(&gesture_buf[2 + data_len[0]], &xdata[1], data_len[1] * sizeof(int32_t));

	gesture_buf[0] = 10;
	gesture_buf[1] = points_num[0] + points_num[1];

	memset(&ts->oppo_debug_gesture_record_info, 0, sizeof(struct oppo_debug_gesture_record_info));

	/* decode coordinate (2 buffers) */
	for (i = 0 ; i < gesture_buf[1] ; i++) {
		input_x = (uint32_t)(gesture_buf[i*3 + 2] << 4) + (uint32_t) (gesture_buf[i*3 + 4] >> 4);
		input_y = (uint32_t)(gesture_buf[i*3 + 3] << 4) + (uint32_t) (gesture_buf[i*3 + 4] & 0x0F);

		ts->oppo_debug_gesture_record_info.coordinate[i].x = (uint16_t) input_x;
		ts->oppo_debug_gesture_record_info.coordinate[i].y = (uint16_t) input_y;
	}

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &oppo_dbg_gesture_coornidate_record_seq_ops);
}

static const struct file_operations oppo_dbg_gesture_coord_record_fops = {
	.owner = THIS_MODULE,
	.open = oppo_dbg_gesture_coord_record_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/* oppo_dbg_gesture_coord_enable */
static ssize_t oppo_dbg_gesture_coord_enable_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *ppos)
{
	unsigned int tmp = 0;
	int32_t ret = 0;
	char cmd[128] = {0};

	if(copy_from_user(cmd, buf, count)) {
		NVT_ERR("input value error\n");
		return -EINVAL;
	}

	if (kstrtouint(cmd, 0, &tmp)) {
		NVT_ERR("kstrtouint error\n");
		return -EINVAL;
	}

	NVT_LOG("debug gesture coordinate enable is %d\n", tmp);

	if (tmp) {
		/* enable */
		ret = nvt_extend_cmd_store(HOST_EXT_CMD, HOST_EXT_DBG_WKG_COORD_ON);
		if (ret < 0) {
			NVT_ERR("%s: debug gesture coordinate enable failed.\n", __func__);
		}
	} else {
		/* disable */
		ret = nvt_extend_cmd_store(HOST_EXT_CMD, HOST_EXT_DBG_WKG_COORD_OFF);
		if (ret < 0) {
			NVT_ERR("%s: debug gesture coordinate disable failed.\n", __func__);
		}
	}

	return count;
};

static const struct file_operations oppo_dbg_gesture_coord_enable_fops =
{
	.write = oppo_dbg_gesture_coord_enable_write,
	.owner = THIS_MODULE,
};
/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_fw_version open
	function.

return:
	n.a.
*******************************************************/
static int32_t nvt_fw_version_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_fw_version_seq_ops);
}

static const struct file_operations nvt_fw_version_fops = {
	.owner = THIS_MODULE,
	.open = nvt_fw_version_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_baseline open function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_baseline_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_read_mdata(ts->mmap->BASELINE_ADDR, ts->mmap->BASELINE_BTN_ADDR);

	nvt_change_mode(NORMAL_MODE);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_seq_ops);
}

static const struct file_operations nvt_baseline_fops = {
	.owner = THIS_MODULE,
	.open = nvt_baseline_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_raw open function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_raw_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->RAW_PIPE0_ADDR, ts->mmap->RAW_BTN_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->RAW_PIPE1_ADDR, ts->mmap->RAW_BTN_PIPE1_ADDR);

	nvt_change_mode(NORMAL_MODE);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_seq_ops);
}

static const struct file_operations nvt_raw_fops = {
	.owner = THIS_MODULE,
	.open = nvt_raw_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_diff open function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
static int32_t nvt_diff_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->DIFF_PIPE0_ADDR, ts->mmap->DIFF_BTN_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->DIFF_PIPE1_ADDR, ts->mmap->DIFF_BTN_PIPE1_ADDR);

	nvt_change_mode(NORMAL_MODE);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_seq_ops);
}

static const struct file_operations nvt_diff_fops = {
	.owner = THIS_MODULE,
	.open = nvt_diff_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_pen_diff open function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
static int32_t nvt_pen_diff_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (nvt_set_pen_inband_mode_1(0xFF, 0x00)) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN)) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_read_get_num_mdata(ts->mmap->PEN_1D_DIFF_TIP_X_ADDR, xdata_pen_tip_x, ts->x_num);
	nvt_read_get_num_mdata(ts->mmap->PEN_1D_DIFF_TIP_Y_ADDR, xdata_pen_tip_y, ts->y_num);
	nvt_read_get_num_mdata(ts->mmap->PEN_1D_DIFF_RING_X_ADDR, xdata_pen_ring_x, ts->x_num);
	nvt_read_get_num_mdata(ts->mmap->PEN_1D_DIFF_RING_Y_ADDR, xdata_pen_ring_y, ts->y_num);

	nvt_change_mode(NORMAL_MODE);

	nvt_set_pen_normal_mode();

	nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_pen_diff_seq_ops);
}

static const struct file_operations nvt_pen_diff_fops = {
	.owner = THIS_MODULE,
	.open = nvt_pen_diff_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*******************************************************
Description:
	Novatek touchscreen extra function proc. file node
	initial function.

return:
	Executive outcomes. 0---succeed. -12---failed.
*******************************************************/
/* coordinate */
static int32_t c_oppo_coordinate_show(struct seq_file *m, void *v)
{
	struct gesture_info *gesture = &ts->gesture;
	char tmp[256] = {0};

	sprintf(tmp, "%d,%d:%d,%d:%d,%d:%d,%d:%d,%d:%d,%d:%d,%d",
		gesture->gesture_type,
		gesture->Point_start.x, gesture->Point_start.y,
		gesture->Point_end.x, gesture->Point_end.y,
		gesture->Point_1st.x, gesture->Point_1st.y,
		gesture->Point_2nd.x, gesture->Point_2nd.y,
		gesture->Point_3rd.x, gesture->Point_3rd.y,
		gesture->Point_4th.x, gesture->Point_4th.y,
		gesture->clockwise);

	/* oppo gesture formate */
	seq_printf(m, "%s\n", tmp);

	return 0;
}

const struct seq_operations oppo_coordinate_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_oppo_coordinate_show
};

static int32_t oppo_coordinate_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &oppo_coordinate_seq_ops);
}

static const struct file_operations oppo_coordinate_fops = {
	.owner = THIS_MODULE,
	.open = oppo_coordinate_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static void nvt_read_fw_history_seq(struct seq_file *m, uint32_t fw_history_addr)
{
    uint8_t i = 0;
    uint8_t buf[65];
    char str[128];

	if (fw_history_addr == 0)
		return;

    nvt_set_page(fw_history_addr);

    buf[0] = (uint8_t) (fw_history_addr & 0x7F);
    CTP_SPI_READ(ts->client, buf, 64+1);	//read 64bytes history

    //print all data
    NVT_LOG("fw history 0x%x: \n", fw_history_addr);
	seq_printf(m, "fw history 0x%x: \n", fw_history_addr);
    for (i = 0; i < 4; i++) {
        snprintf(str, sizeof(str), "%2x %2x %2x %2x %2x %2x %2x %2x    %2x %2x %2x %2x %2x %2x %2x %2x\n",
                 buf[1+i*16], buf[2+i*16], buf[3+i*16], buf[4+i*16],
                 buf[5+i*16], buf[6+i*16], buf[7+i*16], buf[8+i*16],
                 buf[9+i*16], buf[10+i*16], buf[11+i*16], buf[12+i*16],
                 buf[13+i*16], buf[14+i*16], buf[15+i*16], buf[16+i*16]);
        NVT_LOG("%s", str);
		seq_printf(m, "%s", str);
    }
}

/* main_register */
static int32_t c_main_register_show(struct seq_file *m, void *v)
{
	struct irq_desc *desc = irq_to_desc(gpio_to_irq(ts->irq_gpio));
    uint8_t buf[4] = {0};

	//read fw history
	nvt_read_fw_history_seq(m, ts->mmap->MMAP_HISTORY_EVENT0);
	nvt_read_fw_history_seq(m, ts->mmap->MMAP_HISTORY_EVENT1);

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);

	//---read cmd status---
	buf[0] = 0x5E;
	buf[1] = 0xFF;
	buf[2] = 0xFF;
	CTP_SPI_READ(ts->client, buf, 3);

	NVT_LOG("PWR_FLAG:%d\n", (buf[1]>> PWR_FLAG) & 0x01);
	seq_printf(m, "PWR_FLAG:%d\n", (buf[1]>> PWR_FLAG) & 0x01);

	NVT_LOG("EDGE_REJECT:%d\n", (buf[1]>> EDGE_REJECT_L) & 0x03);
	seq_printf(m, "EDGE_REJECT:%d\n", (buf[1]>> EDGE_REJECT_L) & 0x03);

	NVT_LOG("JITTER_FLAG:%d\n", (buf[1]>> JITTER_FLAG) & 0x01);
	seq_printf(m, "JITTER_FLAG:%d\n", (buf[1]>> JITTER_FLAG) & 0x01);

	NVT_LOG("HEADSET_FLAG:%d\n", (buf[1]>> HEADSET_FLAG) & 0x01);
	seq_printf(m, "HEADSET_FLAG:%d\n", (buf[1]>> HEADSET_FLAG) & 0x01);

	NVT_LOG("HOPPING_FIX_FREQ_FLAG:%d\n", (buf[1]>> HOPPING_FIX_FREQ_FLAG) & 0x01);
	seq_printf(m, "HOPPING_FIX_FREQ_FLAG:%d\n", (buf[1]>> HOPPING_FIX_FREQ_FLAG) & 0x01);

	NVT_LOG("HOPPING_POLLING_FLAG:%d\n", (buf[1]>> HOPPING_POLLING_FLAG) & 0x01);
	seq_printf(m, "HOPPING_POLLING_FLAG:%d\n", (buf[1]>> HOPPING_POLLING_FLAG) & 0x01);

	NVT_LOG("DEBUG_MESSAGE_FLAG:%d\n", (buf[2]>> DEBUG_MESSAGE_FLAG) & 0x01);
	seq_printf(m, "DEBUG_MESSAGE_FLAG:%d\n", (buf[2]>> DEBUG_MESSAGE_FLAG) & 0x01);

	NVT_LOG("DEBUG_GESTURE_COORDINATE_FLAG:%d\n", (buf[2]>> DEBUG_GESTURE_COORDINATE_FLAG) & 0x01);
	seq_printf(m, "DEBUG_GESTURE_COORDINATE_FLAG:%d\n", (buf[2]>> DEBUG_GESTURE_COORDINATE_FLAG) & 0x01);

	NVT_LOG("DEBUG_GESTURE_COORDINATE_RECORD_FLAG:%d\n", (buf[2]>> DEBUG_GESTURE_COORDINATE_RECORD_FLAG) & 0x01);
	seq_printf(m, "DEBUG_GESTURE_COORDINATE_RECORD_FLAG:%d\n", (buf[2]>> DEBUG_GESTURE_COORDINATE_RECORD_FLAG) & 0x01);

	NVT_ERR("IRQ_DEPTH:%d\n", desc->depth);
	seq_printf(m, "IRQ_DEPTH:%d\n", desc->depth);

	return 0;
}

const struct seq_operations oppo_main_register_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_main_register_show
};

static int32_t nvt_main_register_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &oppo_main_register_seq_ops);
}

static const struct file_operations oppo_main_register_fops = {
	.owner = THIS_MODULE,
	.open = nvt_main_register_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/* debug_level */
static ssize_t oppo_debug_level_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *ppos)
{
	unsigned int tmp = 0;
	char cmd[128] = {0};

	if(copy_from_user(cmd, buf, count)) {
		NVT_ERR("input value error\n");
		return -EINVAL;
	}

	if (kstrtouint(cmd, 0, &tmp)) {
		NVT_ERR("kstrtouint error\n");
		return -EINVAL;
	}

	ts->debug_level = tmp;

	NVT_LOG("debug_level is %d\n", ts->debug_level);

	if ((ts->debug_level != 0) && (ts->debug_level != 1) && (ts->debug_level != 2)) {
		NVT_ERR("debug level error %d\n", ts->debug_level);
		ts->debug_level = 0;
	}

	return count;
};

static const struct file_operations oppo_debug_level_fops =
{
	.write = oppo_debug_level_write,
	.owner = THIS_MODULE,
};

/* double_tap_enable */
/*
 *    gesture_enable = 0 : disable gesture
 *    gesture_enable = 1 : enable gesture when ps is far away
 *    gesture_enable = 2 : disable gesture when ps is near
 *    gesture_enable = 3 : enable single tap gesture when ps is far away
 */
static ssize_t oppo_gesture_write(struct file *filp, const char __user *buf,size_t count, loff_t *ppos)
{
	int value = 0;
	char cmd[128] = {0};

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	if (copy_from_user(cmd, buf, count)) {
		NVT_ERR("input value error\n");
		mutex_unlock(&ts->lock);
		return -EINVAL;
	}

    sscanf(cmd, "%d", &value);
    if (value > 3) {
        return count;
	}

	if (ts->gesture_enable != value) {
		ts->gesture_enable = value;
		NVT_LOG("%s: gesture_enable = %d, is_suspended = %d\n", __func__, ts->gesture_enable, ts->is_suspended);
		if (ts->is_suspended) {
			if ((ts->gesture_enable == 0) || (ts->gesture_enable == 2)) {
				//wakeup gesture mode to deep sleep mode
				nvt_mode_change_cmd(0x11);
			} else if ((ts->gesture_enable == 1) || (ts->gesture_enable == 3)) {
				//deep sleep to wakeup gesture mode
				nvt_update_firmware(ts->tp_fw_name);
				nvt_mode_change_cmd(0x13);
			}
		}
	}
    else {
		NVT_LOG("%s: do not do same operator :%d\n", __func__, value);
	}

	mutex_unlock(&ts->lock);

	return count;

}

static ssize_t oppo_gesture_read(struct file *file, char __user *buf,size_t count, loff_t *ppos)
{
	int ret = 0;
	int len;
	uint8_t *ptr = NULL;

    if(*ppos) {
        return 0;
    }

	ptr = kzalloc(sizeof(uint8_t)*8, GFP_KERNEL);
	if (ptr == NULL) {
		NVT_ERR("failed to allocate memory for ptr\n");
		return 0;
	}

	len = sprintf(ptr, "%d\n", ts->gesture_enable);
	ret = copy_to_user(buf, ptr, len);

	*ppos += len;

	if (ptr) {
		kfree(ptr);
		ptr = NULL;
	}

	return len;

}

static const struct file_operations oppo_gesture_fops =
{
	.write = oppo_gesture_write,
	.read = oppo_gesture_read,
	.owner = THIS_MODULE,
};

/* tp_fw_update */
static ssize_t oppo_fw_update_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *ppos)
{
	unsigned int tmp = 0;
	uint8_t update_type = 0;
	char cmd[128] = {0};

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if(copy_from_user(cmd, buf, count)) {
		NVT_ERR("input value error\n");
		mutex_unlock(&ts->lock);
		return -EINVAL;
	}

	if (kstrtouint(cmd, 0, &tmp)) {
		NVT_ERR("kstrtouint error\n");
		mutex_unlock(&ts->lock);
		return -EINVAL;
	}

	update_type = tmp;

	NVT_LOG("update_type is %d\n", update_type);
	switch (update_type) {
		case 0:	/* noflash: force update. flash: force update */
			nvt_update_firmware(ts->tp_fw_name);
			break;
		case 1: /* noflash: do nothing. flash: check fw version and update */
			NVT_ERR("update_type %d. Do nothing for noflash\n", update_type);
			break;
		default:
			NVT_ERR("update_type %d error\n", update_type);
	}

	NVT_LOG("--\n");
	mutex_unlock(&ts->lock);

	return count;
};

static const struct file_operations oppo_fw_update_fops =
{
	.write = oppo_fw_update_write,
	.owner = THIS_MODULE,
};

/* irq_depth */
static int32_t c_irq_depath_show(struct seq_file *m, void *v)
{
	struct irq_desc *desc = irq_to_desc(gpio_to_irq(ts->irq_gpio));
	NVT_ERR("depth %d\n", desc->depth);

	seq_printf(m, "%d\n", desc->depth);

	return 0;
}

const struct seq_operations oppo_irq_depath_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_irq_depath_show
};

static int32_t nvt_irq_depath_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &oppo_irq_depath_seq_ops);
}

static const struct file_operations oppo_irq_depath_fops = {
	.owner = THIS_MODULE,
	.open = nvt_irq_depath_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/* oppo_register_info */
struct oppo_register_info {
	uint32_t addr;
	uint32_t len;
} oppo_reg;

/*
 * Example data format: echo 11A60,2 > file_node
 */
static ssize_t oppo_register_info_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *ppos)
{
	uint8_t tmp[10] = {0};
	char cmd[128] = {0};

	/* Error handler */
	if (count != 8) {
		NVT_ERR("count %ld error\n", count);
		return count;
	}

	if(copy_from_user(cmd, buf, count)) {
		NVT_ERR("input value error\n");
		return -EINVAL;
	}

	/* parsing address (Novatek address length: 5 bit) */
	sprintf(tmp, "%c%c%c%c%c", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4]);

	if (kstrtouint(tmp, 16, &oppo_reg.addr)) {
		NVT_ERR("kstrtouint error\n");
		return -EINVAL;
	}

	NVT_LOG("address: 0x%05X\n", oppo_reg.addr);

	/* parsing length */
	sprintf(tmp, "%c", cmd[6]);
	if (kstrtouint(tmp, 10, &oppo_reg.len)) {
		NVT_ERR("kstrtouint error\n");
		return -EINVAL;
	}

	NVT_LOG("len %d\n", oppo_reg.len);

	return count;
}

static ssize_t oppo_register_info_read(struct file *file, char __user *buff,size_t count, loff_t *ppos)
{
	uint8_t *buf = NULL;
	uint8_t *ptr = NULL;
	uint8_t len = 0;
	uint8_t i = 0;
	int32_t ret = 0;

	if(*ppos) {
		return 0;	/* the end */
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	if (oppo_reg.len == 0) {
		NVT_ERR("len = %d\n", oppo_reg.len);
		goto fail;
	}

	buf = (uint8_t *)kzalloc(sizeof(uint8_t)*(oppo_reg.len), GFP_KERNEL);
	if (buf == NULL) {
		NVT_ERR("failed to allocate memory for buf\n");
		goto fail;
	}

	ptr = (uint8_t *)kzalloc(sizeof(uint8_t)*(oppo_reg.len)*3+1, GFP_KERNEL);
	if (ptr == NULL) {
		NVT_ERR("failed to allocate memory for ptr\n");
		goto fail;
	}

	/* read data */
	nvt_set_page(oppo_reg.addr);
	buf[0] = oppo_reg.addr & 0x7F;
	CTP_SPI_READ(ts->client, buf, oppo_reg.len + 1);

	/* set index to EVENT_BUF_ADDR */
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);

	/* copy hex data to string */
	for (i=0 ; i<oppo_reg.len ; i++) {
		len += sprintf(ptr+len, "%02X ", buf[i+1]);
		//NVT_ERR("[%d] buf %02X\n", i, buf[i+1]);
	}

	/* new line */
	len += sprintf(ptr+len, "\n");

	ret = copy_to_user(buff,ptr,len);

	*ppos += len;

fail:
	mutex_unlock(&ts->lock);

	if (buf) {
		kfree(buf);
		buf = NULL;
	}

	if (ptr) {
		kfree(ptr);
		ptr = NULL;
	}

	return len;
}

static const struct file_operations oppo_register_info_fops =
{
	.write = oppo_register_info_write,
	.read = oppo_register_info_read,
	.owner = THIS_MODULE,
};

/* game_switch_enable */
static ssize_t oppo_game_switch_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *ppos)
{
	unsigned int tmp = 0;
	char cmd[128] = {0};

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	if(copy_from_user(cmd, buf, count)) {
		NVT_ERR("input value error\n");
		mutex_unlock(&ts->lock);
		return -EINVAL;
	}

	if (kstrtouint(cmd, 0, &tmp)) {
		NVT_ERR("kstrtouint error\n");
		mutex_unlock(&ts->lock);
		return -EINVAL;
	}

	ts->game_mode = !!tmp;
	NVT_LOG("game switch enable is %d\n", ts->game_mode);

	if(nvt_mode_switch(MODE_GAME, ts->game_mode)) {
		NVT_ERR("game switch enable fail!\n");
	}

	mutex_unlock(&ts->lock);

	return count;
};

static ssize_t oppo_game_switch_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
	uint8_t tmp[8] = {0};

    NVT_LOG("ts->game_mode = %d\n", ts->game_mode);

	snprintf(tmp, sizeof(tmp) - 1, "%d\n", ts->game_mode);

	return simple_read_from_buffer(buf, count, ppos, tmp, strlen(tmp));
}

static const struct file_operations oppo_game_switch_fops =
{
	.write = oppo_game_switch_write,
	.read = oppo_game_switch_read,
	.owner = THIS_MODULE,
};

/* oppo_tp_limit_enable */
static ssize_t oppo_tp_limit_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
	uint8_t tmp[8] = {0};

    NVT_LOG("ts->limit_edge = %d\n", ts->limit_edge);

	snprintf(tmp, sizeof(tmp) - 1, "%d\n", ts->limit_edge);

	return simple_read_from_buffer(buf, count, ppos, tmp, strlen(tmp));
}

static ssize_t oppo_tp_limit_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *ppos)
{
	unsigned int tmp = 0;
	char cmd[128] = {0};

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	if(copy_from_user(cmd, buf, count)) {
		NVT_ERR("input value error\n");
		mutex_unlock(&ts->lock);
		return -EINVAL;
	}

	if (kstrtouint(cmd, 0, &tmp)) {
		NVT_ERR("kstrtouint error\n");
		mutex_unlock(&ts->lock);
		return -EINVAL;
	}

	ts->limit_edge = tmp;
	NVT_LOG("edge reject enable is %d\n", ts->limit_edge);

	if(nvt_mode_switch(MODE_EDGE, ts->limit_edge)) {
		NVT_ERR("edge reject enable fail!\n");
	}

	mutex_unlock(&ts->lock);

	return count;
};

static const struct file_operations oppo_tp_limit_fops =
{
	.write = oppo_tp_limit_write,
	.read = oppo_tp_limit_read,
	.owner = THIS_MODULE,
};

int32_t nvt_extra_proc_init(void)
{
	NVT_proc_fw_version_entry = proc_create(NVT_FW_VERSION, 0444, NULL, &nvt_fw_version_fops);
	if (NVT_proc_fw_version_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_FW_VERSION);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_FW_VERSION);
	}

	NVT_proc_baseline_entry = proc_create(NVT_BASELINE, 0444, NULL,&nvt_baseline_fops);
	if (NVT_proc_baseline_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_BASELINE);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_BASELINE);
	}

	NVT_proc_raw_entry = proc_create(NVT_RAW, 0444, NULL,&nvt_raw_fops);
	if (NVT_proc_raw_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_RAW);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_RAW);
	}

	NVT_proc_diff_entry = proc_create(NVT_DIFF, 0444, NULL,&nvt_diff_fops);
	if (NVT_proc_diff_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_DIFF);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_DIFF);
	}

	if (ts->pen_support) {
		NVT_proc_pen_diff_entry = proc_create(NVT_PEN_DIFF, 0444, NULL,&nvt_pen_diff_fops);
		if (NVT_proc_pen_diff_entry == NULL) {
			NVT_ERR("create proc/%s Failed!\n", NVT_PEN_DIFF);
			return -ENOMEM;
		} else {
			NVT_LOG("create proc/%s Succeeded!\n", NVT_PEN_DIFF);
		}
	}

	/* oppo customized proc node */
	if ((ts->oppo_touchpanel_proc == NULL) || (ts->debug_info == NULL)) {
		NVT_ERR("proc/touchpanel/ or proc/touchpanel/debug_info is NULL!\n");
		return -ENOMEM;
	}

	oppo_coordinate = proc_create(OPPO_COORDINATE, 0664,
			ts->oppo_touchpanel_proc, &oppo_coordinate_fops);
	if (oppo_coordinate == NULL) {
		NVT_ERR("create proc/touchpanel/coordinate Failed!\n");
		return -ENOMEM;
	}

	oppo_delta = proc_create(OPPO_DELTA, 0664,
			ts->debug_info, &nvt_diff_fops);
	if (oppo_delta == NULL) {
		NVT_ERR("create proc/touchpanel/debug_info/delta Failed!\n");
		return -ENOMEM;
	}

	oppo_baseline = proc_create(OPPO_BASELINE, 0664,
			ts->debug_info, &nvt_baseline_fops);
	if (oppo_baseline == NULL) {
		NVT_ERR("create proc/touchpanel/debug_info/baseline Failed!\n");
		return -ENOMEM;
	}

	oppo_main_register = proc_create(OPPO_MAIN_REGISTER, 0664,
			ts->debug_info, &oppo_main_register_fops);
	if (oppo_main_register == NULL) {
		NVT_ERR("create proc/touchpanel/debug_info/main_register Failed!\n");
		return -ENOMEM;
	}

	oppo_debug_level = proc_create(OPPO_DEBUG_LEVEL, 0664,
			ts->oppo_touchpanel_proc, &oppo_debug_level_fops);
	if (oppo_debug_level == NULL) {
		NVT_ERR("create proc/touchpanel/debug_level Failed!\n");
		return -ENOMEM;
	}

	oppo_gesture = proc_create(OPPO_GESTURE, 0664,
			ts->oppo_touchpanel_proc, &oppo_gesture_fops);
	if (oppo_gesture == NULL) {
		NVT_ERR("create proc/touchpanel/double_tap_enable Failed!\n");
		return -ENOMEM;
	}

	oppo_irq_depath = proc_create(OPPO_IRQ_DEPATH, 0444,
			ts->oppo_touchpanel_proc, &oppo_irq_depath_fops);
	if (oppo_irq_depath == NULL) {
		NVT_ERR("create proc/touchpanel/irq_depath Failed!\n");
		return -ENOMEM;
	}

	register_info_oppo = proc_create(OPPO_REGISTER_INFO, 0664,
			ts->oppo_touchpanel_proc, &oppo_register_info_fops);
	if (register_info_oppo == NULL) {
		NVT_ERR("create proc/touchpanel/oppo_register_info Failed!\n");
		return -ENOMEM;
	}

	oppo_fw_update = proc_create(OPPO_FW_UPDATE, 0664,
			ts->oppo_touchpanel_proc, &oppo_fw_update_fops);
	if (oppo_fw_update == NULL) {
		NVT_ERR("create proc/touchpanel/tp_fw_update Failed!\n");
		return -ENOMEM;
	}

	oppo_game_switch = proc_create(OPPO_GAME_SWITCH, 0666,
			ts->oppo_touchpanel_proc, &oppo_game_switch_fops);
	if (oppo_game_switch == NULL) {
		NVT_ERR("create proc/touchpanel/oppo_game_switch_enable Failed!\n");
		return -ENOMEM;
	}

	oppo_tp_limit_enable = proc_create(OPPO_TP_LIMIT_ENABLE, 0666,
			ts->oppo_touchpanel_proc, &oppo_tp_limit_fops);
	if (oppo_tp_limit_enable == NULL) {
		NVT_ERR("create proc/touchpanel/oppo_tp_limit_enable Failed!\n");
		return -ENOMEM;
	}

	oppo_dbg_message_diff_enable_entry = proc_create(OPPO_DBG_MESSAGE_DIFF_ENABLE, 0222,
			ts->debug_info, &oppo_dbg_message_diff_enable_fops);
	if (oppo_dbg_message_diff_enable_entry == NULL) {
		NVT_ERR("create proc/touchpanel/debug_info/oppo_dbg_message_diff_enable_entry Failed!\n");
		return -ENOMEM;
	}

	oppo_dbg_finger_down_diff_entry = proc_create(OPPO_DBG_FINGER_DOWN_DIFF, 0444,
			ts->debug_info, &oppo_dbg_finger_down_diff_fops);
	if (oppo_dbg_finger_down_diff_entry == NULL) {
		NVT_ERR("create proc/touchpanel/debug_info/nvt_dbg_finger_down_diff Failed!\n");
		return -ENOMEM;
	}

	oppo_dbg_status_change_diff_entry = proc_create(OPPO_DBG_STATUS_CHANGE_DIFF, 0444,
			ts->debug_info, &oppo_dbg_status_change_diff_fops);
	if (oppo_dbg_status_change_diff_entry == NULL) {
		NVT_ERR("create proc/touchpanel/debug_info/nvt_dbg_status_change_diff Failed!\n");
		return -ENOMEM;
	}

	oppo_dbg_gesture_coord_record_enable_entry = proc_create(OPPO_DBG_GESTURE_COORD_RECORD_ENABLE, 0222,
			ts->debug_info, &oppo_dbg_gesture_coord_record_enable_fops);
	if (oppo_dbg_gesture_coord_record_enable_entry == NULL) {
		NVT_ERR("create proc/touchpanel/debug_info/oppo_dbg_gesture_coord_record_enable Failed!\n");
		return -ENOMEM;
	}

	oppo_dbg_gesture_coord_record_entry = proc_create(OPPO_DBG_GESTURE_COORD_RECORD, 0444,
			ts->debug_info, &oppo_dbg_gesture_coord_record_fops);
	if (oppo_dbg_gesture_coord_record_entry == NULL) {
		NVT_ERR("create proc/touchpanel/debug_info/oppo_dbg_gesture_coord_record Failed!\n");
		return -ENOMEM;
	}
	oppo_dbg_gesture_coord_enable_entry = proc_create(OPPO_DBG_GESTURE_COORD_ENABLE, 0222,
			ts->debug_info, &oppo_dbg_gesture_coord_enable_fops);
	if (oppo_dbg_gesture_coord_enable_entry == NULL) {
		NVT_ERR("create proc/touchpanel/debug_info/oppo_dbg_gesture_coord_enable_entry Failed!\n");
		return -ENOMEM;
	}

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen extra function proc. file node
	deinitial function.

return:
	n.a.
*******************************************************/
void nvt_extra_proc_deinit(void)
{
	if (NVT_proc_fw_version_entry != NULL) {
		remove_proc_entry(NVT_FW_VERSION, NULL);
		NVT_proc_fw_version_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_FW_VERSION);
	}

	if (NVT_proc_baseline_entry != NULL) {
		remove_proc_entry(NVT_BASELINE, NULL);
		NVT_proc_baseline_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_BASELINE);
	}

	if (NVT_proc_raw_entry != NULL) {
		remove_proc_entry(NVT_RAW, NULL);
		NVT_proc_raw_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_RAW);
	}

	if (NVT_proc_diff_entry != NULL) {
		remove_proc_entry(NVT_DIFF, NULL);
		NVT_proc_diff_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_DIFF);
	}

	if (ts->pen_support) {
		if (NVT_proc_pen_diff_entry != NULL) {
			remove_proc_entry(NVT_PEN_DIFF, NULL);
			NVT_proc_pen_diff_entry = NULL;
			NVT_LOG("Removed /proc/%s\n", NVT_PEN_DIFF);
		}
	}
}
#endif
