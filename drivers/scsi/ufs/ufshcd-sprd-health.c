/*
 * spreadtrum create in 2021/11/19
 *
 * ufs health report for vendor & create ufs node in /proc/ufs_health
 *
 */


#include "ufshcd.h"
#include "ufshcd-sprd-health.h"

struct _ufshealth_data {
	u8 buf[4096];
};
struct _ufshealth_data ufshealth_data;

void get_ufshealth_data(u8 *data, int buffer_len)
{
	memcpy(&ufshealth_data.buf[0], data, buffer_len);
}

/* FBB: Factory Bad Block show */
int sprd_fbbc_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* Reseeved_Block_Num show */
int sprd_rb_num_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* RTBB_ESF(Erase status fail) show */
int sprd_rtbb_esf_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* RTBB_PSF(Program status fail) show */
int sprd_rtbb_psf_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* RTBB_UECC(Read UECC) show */
int sprd_rtbb_uecc_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* UECC Count show */
int sprd_uecc_count_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* Read_Reclaim_SLC show */
int sprd_read_reclaim_slc_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* Read_Reclaim_TLC show */
int sprd_read_reclaim_tlc_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* VDT_Vccq show */
int sprd_vdt_vccq_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* VDT_Vcc show */
int sprd_vdt_vcc_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* Sudden power off recovery success count show */
int sprd_sudden_power_off_recovery_success_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* Sudden power off recovery fail count show */
int sprd_sudden_power_off_recovery_fail_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}
/* Min_EC_Num_SLC(EC:erase count) show */
int sprd_min_ec_num_slc_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* Max_EC_Num_SLC show */
int sprd_max_ec_num_slc_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* Ave_EC_Num_SLC show */
int sprd_ave_ec_num_slc_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* Min_EC_Num_TLC show */
int sprd_min_ec_num_tlc_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* Max_EC_Num_TLC show */
int sprd_max_ec_num_tlc_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* Ave_EC_Num_TLC show */
int sprd_ave_ec_num_tlc_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* Cumulative Host Read show */
int sprd_cumulative_host_read_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* Cumulative Host Write show */
int sprd_cumulative_host_write_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* Cumulative Initialization Count show */
int sprd_cumulative_initialization_count_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* WAF total(write Amolication Factor) show */
int sprd_waf_total_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* History min NAND temp show */
int sprd_history_min_nand_temp_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* History max NAND temp show */
int sprd_history_max_nand_temp_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* SLC Used_Life show */
int sprd_slc_used_life_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* TLC Used_Life show */
int sprd_tlc_used_life_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* FFU_Success show */
int sprd_ffu_success_cnt_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* FFU_Fail_Cnt show */
int sprd_ffu_fail_cnt_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* Spare_SLC_Block_Num show */
int sprd_spare_slc_block_num_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* Spare_TLC_Block_Num show */
int sprd_spare_tlc_block_num_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* max temperature counter(over 85C) show */
int sprd_max_temperature_counter_over_85c_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* max temperature counter(over 125C) show */
int sprd_max_temperature_counter_over_125c_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* RTBB SLC show */
int sprd_rtbb_slc_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* RTBB TLC show */
int sprd_rtbb_tlc_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

/* All health data show */
int sprd_health_data_show(int data)
{
	u32 temp = get_sprd_u32_buf(data);

	return temp;
}

u32 get_sprd_u32_buf(int data)
{
	u8 temp_0 = *((u8 *)(&ufshealth_data.buf[data]));
	u8 temp_1 = *((u8 *)(&ufshealth_data.buf[data+1]));
	u8 temp_2 = *((u8 *)(&ufshealth_data.buf[data+2]));
	u8 temp_3 = *((u8 *)(&ufshealth_data.buf[data+3]));
	u32 real_val = 0;

	real_val = temp_0 | (temp_1 << 8) |
			(temp_2 << 16) | (temp_3 << 24);
	real_val = be32_to_cpu(real_val);

	return real_val;
}

int ufs_get_health_report(struct ufs_hba *hba)
{
	struct scsi_device *sdp;
	unsigned long flags;
	int ret;
	unsigned char cmd0[10] = {0};
	unsigned char cmd1[10] = {0};
	struct scsi_sense_hdr sshdr;
	u8 *read_buffer_data = NULL;
	unsigned char write_buffer_data[44];
	unsigned int write_buf_len = 0x2c;
	unsigned int read_buf_len = 0x1000;

	spin_lock_irqsave(hba->host->host_lock, flags);
	sdp = hba->sdev_ufs_device;
	if (sdp) {
		ret = scsi_device_get(sdp);
		if (!ret && !scsi_device_online(sdp)) {
			ret = -ENODEV;
			scsi_device_put(sdp);
		}
	} else {
		ret = -ENODEV;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ret)
		return ret;

	read_buffer_data = kzalloc(4096, GFP_KERNEL);
	if (!read_buffer_data)
		return -ENOMEM;

	memset(write_buffer_data, 0, write_buf_len);

	write_buffer_data[0] = 0xBB;
	write_buffer_data[1] = 0x40;
	write_buffer_data[2] = 0x10;

	cmd0[0] = WRITE_BUFFER;	/* 0x3B: WRITE_BUFFER */
	cmd0[1] = 0xE1;			/* MODE */
	cmd0[2] = 0x0;
	cmd0[3] = 0x0;
	cmd0[4] = 0x0;
	cmd0[5] = 0x0;
	cmd0[6] = 0x0;
	cmd0[7] = 0x0;
	cmd0[8] = 0x2c;			/* PARAMETER LIST LENGTH  */
	cmd0[9] = 0x0;

	ret = scsi_execute(sdp, cmd0, DMA_TO_DEVICE,
					write_buffer_data, write_buf_len, NULL, &sshdr,
					msecs_to_jiffies(1000), 3, 0, RQF_PM, NULL);
	if (ret) {
		sdev_printk(KERN_WARNING, sdp, "WRITE_BUFFER fail.\n");
		if (driver_byte(ret) & DRIVER_SENSE)
			scsi_print_sense_hdr(sdp, NULL, &sshdr);
		goto out;
	}

	cmd1[0] = READ_BUFFER;
	cmd1[1] = 0xC1;
	cmd1[2] = 0x0;
	cmd1[3] = 0x0;
	cmd1[4] = 0x0;
	cmd1[5] = 0x0;
	cmd1[6] = 0x0;
	cmd1[7] = 0x10;
	cmd1[8] = 0x0;
	cmd1[9] = 0x0;

	ret = scsi_execute(sdp, cmd1, DMA_FROM_DEVICE,
					read_buffer_data, read_buf_len, NULL, &sshdr,
					msecs_to_jiffies(1000), 3, 0, RQF_PM, NULL);
	if (ret) {
		sdev_printk(KERN_WARNING, sdp, "READ_BUFFER fail.\n");
		if (driver_byte(ret) & DRIVER_SENSE)
			scsi_print_sense_hdr(sdp, NULL, &sshdr);
		goto out;
	}

	get_ufshealth_data(read_buffer_data, 0x1000);

out:
	scsi_device_put(sdp);
	kfree(read_buffer_data);
	return ret;
}
