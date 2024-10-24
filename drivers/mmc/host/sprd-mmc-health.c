/*
 * create in 2021/1/7.
 * create emmc node in  /proc/bootdevice
 */

#include <linux/mmc/sprd-mmc-health.h>
#include "../core/mmc_ops.h"
#define PROC_MODE 0444

struct __mmchealthdata mmchealthdata;

void set_mmchealth_data(u8 *data)
{
	memcpy(&mmchealthdata.buf[0], data, 512);
}

/*********************  FORESEE_32G_eMMC  *****************************************/
//Read reclaim tlc
static int sprd_Read_reclaim_tlc_show(struct seq_file *m, void *v)
{
	u16 temp = ((mmchealthdata.buf[51]  << 8) & 0xff00) |
				(mmchealthdata.buf[50] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Read_reclaim_tlc_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Read_reclaim_tlc_show, inode->i_private);
}

static const struct file_operations Read_reclaim_tlc_fops = {
	.open = sprd_Read_reclaim_tlc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Factory bad block count
static int sprd_factory_bad_block_count_show(struct seq_file *m, void *v)
{
	u16 temp = mmchealthdata.buf[32] & 0xff;

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_factory_bad_block_count_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_factory_bad_block_count_show, inode->i_private);
}

static const struct file_operations factory_bad_block_count_fops = {
	.open = sprd_factory_bad_block_count_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Vcc drop count
static int sprd_vcc_drop_count_show(struct seq_file *m, void *v)
{
	u16 temp = ((mmchealthdata.buf[53]  << 8) & 0xff00) |
				(mmchealthdata.buf[52] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_vcc_drop_count_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_vcc_drop_count_show, inode->i_private);
}

static const struct file_operations vcc_drop_count_fops = {
	.open = sprd_vcc_drop_count_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Min_EC_Num_TLC
static int sprd_Min_EC_Num_TLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[15] << 24) & 0xff000000) |
			((mmchealthdata.buf[14] << 16) & 0xff0000) |
			((mmchealthdata.buf[13] << 8) & 0xff00) |
			(mmchealthdata.buf[12] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Min_EC_Num_TLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Min_EC_Num_TLC_show, inode->i_private);
}

static const struct file_operations Min_EC_Num_TLC_fops = {
	.open = sprd_Min_EC_Num_TLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Max_EC_Num_TLC
static int sprd_Max_EC_Num_TLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[19] << 24) & 0xff000000) |
			((mmchealthdata.buf[18] << 16) & 0xff0000) |
			((mmchealthdata.buf[17] << 8) & 0xff00) |
			(mmchealthdata.buf[16] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Max_EC_Num_TLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Max_EC_Num_TLC_show, inode->i_private);
}

static const struct file_operations Max_EC_Num_TLC_fops = {
	.open = sprd_Max_EC_Num_TLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Ave_EC_Num_TLC
static int sprd_Ave_EC_Num_TLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[23] << 24) & 0xff000000) |
			((mmchealthdata.buf[22] << 16) & 0xff0000) |
			((mmchealthdata.buf[21] << 8) & 0xff00) |
			(mmchealthdata.buf[20] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Ave_EC_Num_TLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Ave_EC_Num_TLC_show, inode->i_private);
}

static const struct file_operations Ave_EC_Num_TLC_fops = {
	.open = sprd_Ave_EC_Num_TLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Min_EC_Num_SLC
static int sprd_Min_EC_Num_SLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[3] << 24) & 0xff000000) |
			((mmchealthdata.buf[2] << 16) & 0xff0000) |
			((mmchealthdata.buf[1] << 8) & 0xff00) |
			(mmchealthdata.buf[0] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Min_EC_Num_SLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Min_EC_Num_SLC_show, inode->i_private);
}

static const struct file_operations Min_EC_Num_SLC_fops = {
	.open = sprd_Min_EC_Num_SLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Max_EC_Num_SLC
static int sprd_Max_EC_Num_SLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[7] << 24) & 0xff000000) |
			((mmchealthdata.buf[6] << 16) & 0xff0000) |
			((mmchealthdata.buf[5] << 8) & 0xff00) |
			(mmchealthdata.buf[4] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Max_EC_Num_SLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Max_EC_Num_SLC_show, inode->i_private);
}

static const struct file_operations Max_EC_Num_SLC_fops = {
	.open = sprd_Max_EC_Num_SLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Ave_EC_Num_SLC
static int sprd_Ave_EC_Num_SLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[11] << 24) & 0xff000000) |
			((mmchealthdata.buf[10] << 16) & 0xff0000) |
			((mmchealthdata.buf[9] << 8) & 0xff00) |
			(mmchealthdata.buf[8] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Ave_EC_Num_SLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Ave_EC_Num_SLC_show, inode->i_private);
}

static const struct file_operations Ave_EC_Num_SLC_fops = {
	.open = sprd_Ave_EC_Num_SLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Total_write_size(MB)/100
static int sprd_Total_write_size_show(struct seq_file *m, void *v)
{
	u32 temp_L = 0;
	u64 temp_M = 0;
	u64 temp_T = 0;

	temp_M = ((mmchealthdata.buf[103] << 24) & 0xff000000) |
					((mmchealthdata.buf[102] << 16) & 0xff0000) |
					((mmchealthdata.buf[101] << 8) & 0xff00) |
					(mmchealthdata.buf[100] & 0xff);

	temp_L = ((mmchealthdata.buf[99] << 24) & 0xff000000) |
			((mmchealthdata.buf[98] << 16) & 0xff0000) |
			((mmchealthdata.buf[97] << 8) & 0xff00) |
			(mmchealthdata.buf[96] & 0xff);
			
	temp_T = ((temp_M << 32) | temp_L) * 512 / (1024 * 1024 * 100);

	seq_printf(m, "%d\n", temp_T);

	return 0;
}

static int sprd_Total_write_size_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Total_write_size_show, inode->i_private);
}

static const struct file_operations Total_write_size_fops = {
	.open = sprd_Total_write_size_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//FORESEE eMMC health data
static int sprd_health_data_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < 32; i++)
		seq_printf(m, "%02x", mmchealthdata.buf[i]);

	return 0;
}

static int sprd_health_data_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_health_data_show, inode->i_private);
}

static const struct file_operations health_data_fops = {
	.open = sprd_health_data_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*****************************************************************/
static const struct file_operations *proc_fops_list1[] = {
	&Read_reclaim_tlc_fops,	
    	&factory_bad_block_count_fops,
	&vcc_drop_count_fops,
    	&Min_EC_Num_TLC_fops,
    	&Max_EC_Num_TLC_fops,
    	&Ave_EC_Num_TLC_fops,
    	&Min_EC_Num_SLC_fops,
    	&Max_EC_Num_SLC_fops,
    	&Ave_EC_Num_SLC_fops,
    	&Total_write_size_fops,
	&health_data_fops,
};

static char * const sprd_emmc_node_info1[] = {
	"Read_reclaim_tlc",
	"factory_bad_block_count",
	"vcc_drop_count",
    	"Min_EC_Num_TLC",
    	"Max_EC_Num_TLC",
    	"Ave_EC_Num_TLC",
    	"Min_EC_Num_SLC",
    	"Max_EC_Num_SLC",
    	"Ave_EC_Num_SLC",
    	"Total_write_size",
	"health_data",
};

int sprd_create_mmc_health_init1(void)
{
	struct proc_dir_entry *mmchealthdir;
	struct proc_dir_entry *prEntry;
	int i, node;

	mmchealthdir = proc_mkdir("mmchealth", NULL);
	if (!mmchealthdir) {
		pr_err("%s: failed to create /proc/mmchealth\n",
			__func__);
		return -1;
	}

	node = ARRAY_SIZE(sprd_emmc_node_info1);
	for (i = 0; i < node; i++) {
		prEntry = proc_create(sprd_emmc_node_info1[i], PROC_MODE,
				      mmchealthdir, proc_fops_list1[i]);
		if (!prEntry) {
			pr_err("%s,failed to create node: /proc/mmchealth/%s\n",
				__func__, sprd_emmc_node_info1[i]);
			return -1;
		}
	}

	return 0;
}


//Factory bad block count
static int sprd_bad_block_count_show(struct seq_file *m, void *v)
{
	u16 temp = ((mmchealthdata.buf[0] << 8 ) & 0xff00) |
				(mmchealthdata.buf[1] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_bad_block_count_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_bad_block_count_show, inode->i_private);
}

static const struct file_operations bad_block_count_fops = {
	.open = sprd_bad_block_count_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Runtime Bad Block
static int sprd_runtime_bad_block_count_show(struct seq_file *m, void *v)
{
	u16 temp = ((mmchealthdata.buf[2] << 8 ) & 0xff00) |
				(mmchealthdata.buf[3] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_runtime_bad_block_count_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_runtime_bad_block_count_show, inode->i_private);
}

static const struct file_operations runtime_bad_block_count_fops = {
	.open = sprd_runtime_bad_block_count_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//reserved block num
static int sprd_reserved_block_num_show(struct seq_file *m, void *v)
{
	u16 temp = ((mmchealthdata.buf[4] << 8 ) & 0xff00) |
				(mmchealthdata.buf[5] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_reserved_block_num_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_reserved_block_num_show, inode->i_private);
}

static const struct file_operations reserved_block_num_fops = {
	.open = sprd_reserved_block_num_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//read_reclaim_count
static int sprd_read_reclaim_count_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[87] << 24) & 0xff000000) |
			((mmchealthdata.buf[86] << 16) & 0xff0000) |
			((mmchealthdata.buf[85] << 8) & 0xff00) |
			(mmchealthdata.buf[84] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_read_reclaim_count_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_read_reclaim_count_show, inode->i_private);
}

static const struct file_operations read_reclaim_count_fops = {
	.open = sprd_read_reclaim_count_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Min_EC_Cnt_TLC
static int sprd_Min_EC_Cnt_TLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[16] << 24) & 0xff000000) |
			((mmchealthdata.buf[17] << 16) & 0xff0000) |
			((mmchealthdata.buf[18] << 8) & 0xff00) |
			(mmchealthdata.buf[19] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Min_EC_Cnt_TLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Min_EC_Cnt_TLC_show, inode->i_private);
}

static const struct file_operations Min_EC_Cnt_TLC_fops = {
	.open = sprd_Min_EC_Cnt_TLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Max_EC_Cnt_TLC
static int sprd_Max_EC_Cnt_TLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[20] << 24) & 0xff000000) |
			((mmchealthdata.buf[21] << 16) & 0xff0000) |
			((mmchealthdata.buf[22] << 8) & 0xff00) |
			(mmchealthdata.buf[23] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Max_EC_Cnt_TLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Max_EC_Cnt_TLC_show, inode->i_private);
}

static const struct file_operations Max_EC_Cnt_TLC_fops = {
	.open = sprd_Max_EC_Cnt_TLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Ave_EC_Cnt_TLC
static int sprd_Ave_EC_Cnt_TLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[24] << 24) & 0xff000000) |
			((mmchealthdata.buf[25] << 16) & 0xff0000) |
			((mmchealthdata.buf[26] << 8) & 0xff00) |
			(mmchealthdata.buf[27] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Ave_EC_Cnt_TLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Ave_EC_Cnt_TLC_show, inode->i_private);
}

static const struct file_operations Ave_EC_Cnt_TLC_fops = {
	.open = sprd_Ave_EC_Cnt_TLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Tal_EC_Cnt_TLC
static int sprd_Tal_EC_Cnt_TLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[28] << 24) & 0xff000000) |
			((mmchealthdata.buf[29] << 16) & 0xff0000) |
			((mmchealthdata.buf[30] << 8) & 0xff00) |
			(mmchealthdata.buf[31] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Tal_EC_Cnt_TLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Tal_EC_Cnt_TLC_show, inode->i_private);
}

static const struct file_operations Tal_EC_Cnt_TLC_fops = {
	.open = sprd_Tal_EC_Cnt_TLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Min_EC_Cnt_SLC
static int sprd_Min_EC_Cnt_SLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[32] << 24) & 0xff000000) |
			((mmchealthdata.buf[33] << 16) & 0xff0000) |
			((mmchealthdata.buf[34] << 8) & 0xff00) |
			(mmchealthdata.buf[35] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Min_EC_Cnt_SLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Min_EC_Cnt_SLC_show, inode->i_private);
}

static const struct file_operations Min_EC_Cnt_SLC_fops = {
	.open = sprd_Min_EC_Cnt_SLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Max_EC_Cnt_SLC
static int sprd_Max_EC_Cnt_SLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[36] << 24) & 0xff000000) |
			((mmchealthdata.buf[37] << 16) & 0xff0000) |
			((mmchealthdata.buf[38] << 8) & 0xff00) |
			(mmchealthdata.buf[39] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Max_EC_Cnt_SLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Max_EC_Cnt_SLC_show, inode->i_private);
}

static const struct file_operations Max_EC_Cnt_SLC_fops = {
	.open = sprd_Max_EC_Cnt_SLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Ave_EC_Cnt_SLC
static int sprd_Ave_EC_Cnt_SLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[40] << 24) & 0xff000000) |
			((mmchealthdata.buf[41] << 16) & 0xff0000) |
			((mmchealthdata.buf[42] << 8) & 0xff00) |
			(mmchealthdata.buf[43] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Ave_EC_Cnt_SLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Ave_EC_Cnt_SLC_show, inode->i_private);
}

static const struct file_operations Ave_EC_Cnt_SLC_fops = {
	.open = sprd_Ave_EC_Cnt_SLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Tal_EC_Cnt_SLC
static int sprd_Tal_EC_Cnt_SLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[44] << 24) & 0xff000000) |
			((mmchealthdata.buf[45] << 16) & 0xff0000) |
			((mmchealthdata.buf[46] << 8) & 0xff00) |
			(mmchealthdata.buf[47] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Tal_EC_Cnt_SLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Tal_EC_Cnt_SLC_show, inode->i_private);
}

static const struct file_operations Tal_EC_Cnt_SLC_fops = {
	.open = sprd_Tal_EC_Cnt_SLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Total write cnt
static int sprd_Tal_write_count_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[99] << 24) & 0xff000000) |
			((mmchealthdata.buf[98] << 16) & 0xff0000) |
			((mmchealthdata.buf[97] << 8) & 0xff00) |
			(mmchealthdata.buf[96] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Tal_write_count_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Tal_write_count_show, inode->i_private);
}

static const struct file_operations Tal_write_count_fops = {
	.open = sprd_Tal_write_count_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//power loss count
static int sprd_power_loss_count_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[195] << 24) & 0xff000000) |
			((mmchealthdata.buf[194] << 16) & 0xff0000) |
			((mmchealthdata.buf[193] << 8) & 0xff00) |
			(mmchealthdata.buf[192] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_power_loss_count_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_power_loss_count_show, inode->i_private);
}

static const struct file_operations power_loss_count_fops = {
	.open = sprd_power_loss_count_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Phison eMMC health data
static int sprd_Health_data_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < 32; i++)
		seq_printf(m, "%02x", mmchealthdata.buf[i]);

	return 0;
}

static int sprd_Health_data_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_Health_data_show, inode->i_private);
}

static const struct file_operations Health_data_fops = {
	.open = sprd_Health_data_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations *proc_fops_list2[] = {
	&bad_block_count_fops,
	&runtime_bad_block_count_fops,
	&reserved_block_num_fops,
	&read_reclaim_count_fops,
	&Min_EC_Cnt_TLC_fops,
	&Max_EC_Cnt_TLC_fops,
    	&Ave_EC_Cnt_TLC_fops,
	&Tal_EC_Cnt_TLC_fops,
	&Min_EC_Cnt_SLC_fops,
    	&Max_EC_Cnt_SLC_fops,
    	&Ave_EC_Cnt_SLC_fops,
	&Tal_EC_Cnt_SLC_fops,
	&Tal_write_count_fops,
	&power_loss_count_fops,
	&Health_data_fops,
};

static char * const sprd_emmc_node_info2[] = {
	"factory_bad_block_count",
	"runtime_bad_block_cout",
	"reserved_blcok_num",
	"read_reclaim_count",
	"Min_EC_Num_TLC",
    	"Max_EC_Num_TLC",
    	"Ave_EC_Num_TLC",
	"Total_EC_Num_TLC",
	"Min_EC_Num_SLC",
    	"Max_EC_Num_SLC",
    	"Ave_EC_Num_SLC",
	"Total_EC_Num_SLC",
	"Total_write_size",
	"power_loss_count", 
	"health_data",
};

int sprd_create_mmc_health_init2(void)
{
	struct proc_dir_entry *mmchealthdir;
	struct proc_dir_entry *prEntry;
	int i, node;

	mmchealthdir = proc_mkdir("mmchealth", NULL);
	if (!mmchealthdir) {
		pr_err("%s: failed to create /proc/mmchealth\n",
			__func__);
		return -1;
	}

	node = ARRAY_SIZE(sprd_emmc_node_info2);
	for (i = 0; i < node; i++) {
		prEntry = proc_create(sprd_emmc_node_info2[i], PROC_MODE,
				      mmchealthdir, proc_fops_list2[i]);
		if (!prEntry) {
			pr_err("%s,failed to create node: /proc/mmchealth/%s\n",
				__func__, sprd_emmc_node_info2[i]);
			return -1;
		}
	}

	return 0;
}

/*  最终提供的接口  */
int sprd_create_mmc_health_init(int flag)
{
	int res = 0;

  	/* FORESEE 32G eMMc */
  	if (flag == FORESEE_32G_eMMC1)
		res = sprd_create_mmc_health_init1();
	/* Phison qunlian 32G eMMC */
	else if (flag == Phison_32G_eMMC)
		res = sprd_create_mmc_health_init2();

  return res;
}
