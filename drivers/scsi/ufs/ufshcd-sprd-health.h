/*
 * spreadtrum create in 2021/11/19
 *
 * ufs health report for vendor & create ufs node in /proc/ufs_health
 *
 */

#ifndef __UFS_SPRD_HEALTH_H__
#define __UFS_SPRD_HEALTH_H__

void get_ufshealth_data(u8 *data, int buffer_len);

u32 get_sprd_u32_buf(int data);
int sprd_fbbc_show(int data);
int sprd_rb_num_show(int data);
int sprd_rtbb_esf_show(int data);
int sprd_rtbb_psf_show(int data);
int sprd_rtbb_uecc_show(int data);
int sprd_uecc_count_show(int data);
int sprd_read_reclaim_slc_show(int data);
int sprd_read_reclaim_tlc_show(int data);
int sprd_vdt_vccq_show(int data);
int sprd_vdt_vcc_show(int data);
int sprd_sudden_power_off_recovery_success_show(int data);
int sprd_sudden_power_off_recovery_fail_show(int data);
int sprd_min_ec_num_slc_show(int data);
int sprd_max_ec_num_slc_show(int data);
int sprd_ave_ec_num_slc_show(int data);
int sprd_min_ec_num_tlc_show(int data);
int sprd_max_ec_num_tlc_show(int data);
int sprd_ave_ec_num_tlc_show(int data);
int sprd_cumulative_host_read_show(int data);
int sprd_cumulative_host_write_show(int data);
int sprd_cumulative_initialization_count_show(int data);
int sprd_waf_total_show(int data);
int sprd_history_min_nand_temp_show(int data);
int sprd_history_max_nand_temp_show(int data);
int sprd_slc_used_life_show(int data);
int sprd_tlc_used_life_show(int data);
int sprd_ffu_success_cnt_show(int data);
int sprd_ffu_fail_cnt_show(int data);
int sprd_spare_slc_block_num_show(int data);
int sprd_spare_tlc_block_num_show(int data);
int sprd_max_temperature_counter_over_85c_show(int data);
int sprd_max_temperature_counter_over_125c_show(int data);
int sprd_rtbb_slc_show(int data);
int sprd_rtbb_tlc_show(int data);
int sprd_health_data_show(int data);

int ufs_get_health_report(struct ufs_hba *hba);

#endif
