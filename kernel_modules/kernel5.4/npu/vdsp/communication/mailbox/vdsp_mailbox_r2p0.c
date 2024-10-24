
/*
 * Copyright (C) 2021 Unisoc Communications Inc.
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
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>
#include "vdsp_hw.h"
#include "vdsp_mailbox_r2p0.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: mboxr2p0 %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

/* mbox local staruct global var define */
static struct regmap *mailbox_gpr;
static unsigned long sprd_inbox_base;
static unsigned long sprd_outbox_base;
static unsigned long sprd_global_base;

static struct mbox_fifo_data_tag mbox_fifo[MBOX_V2_OUTBOX_FIFO_SIZE];
static struct mbox_fifo_data_tag mbox_fifo_bak[MAX_SMSG_BAK];

static int mbox_fifo_bak_len;
static u8 g_one_time_recv_cnt;

																																																													      /* static u8 g_inbox_send; *//*for debug */
static u32 g_inbox_irq_mask;	/*for debug , in init */

static unsigned long max_total_irq_proc_time;
static unsigned long max_total_irq_cnt;

																																																																		/* static int g_restore_cnt; *//*for debug */
static int g_inbox_block_cnt;
static int g_outbox_full_cnt;

static unsigned int g_recv_cnt[MBOX_MAX_CORE_CNT];
static unsigned int g_send_cnt[MBOX_MAX_CORE_CNT];

static struct mbox_chn_tag mbox_chns[MBOX_MAX_CORE_CNT];
static struct mbox_cfg_tag mbox_cfg;

static inline void reg_relaxed_write32(void *addr, u32 v)
{
	writel_relaxed(v, (void __iomem *)(addr));
}

static inline u32 reg_relaxed_read32(void *addr)
{
	return readl_relaxed((void __iomem *)(addr));
}

static inline void reg_relaxed_setbit(void *addr, u32 v)
{
	u32 tmpvalue;

	tmpvalue = readl_relaxed((void __iomem *)(addr));
	tmpvalue |= v;
	writel_relaxed(tmpvalue, (void __iomem *)(addr));
}

static inline void reg_relaxed_clearbit(void *addr, u32 v)
{
	u32 tmpvalue;

	tmpvalue = readl_relaxed((void __iomem *)(addr));
	tmpvalue &= v;
	writel_relaxed(tmpvalue, (void __iomem *)(addr));
}

/* mbox local feature define */

/* redefine debug function, only can open it in mbox bringup phase */

/*#define MBOX_REDEFINE_DEBUG_FUNCTION*/

/* remove printk, because Mbox was used so frequently, remove
 * debug info can improve the system performance
 */
#define MBOX_REMOVE_PR_DEBUG

/* remove the same msg in fifo, It also can improve the system performance */

/* #define MBOX_REMOVE_THE_SAME_MSG */
#ifdef MBOX_REMOVE_THE_SAME_MSG
static int g_skip_msg;
#endif

/* mbox test feature, will cteate a device "dev/sprd_mbox",
 * echo '1' to start,  echo '1' to stop, but we only can
 * open it in mbox bringup phase and don't care dsp state
 */

extern int sipc_get_wakeup_flag(void);

static void mbox_raw_recv(struct mbox_fifo_data_tag *fifo)
{
	u64 msg_l, msg_h;
	int target_id;

	fifo->msg = 0;
	fifo->core_id = MBOX_MAX_CORE_MASK;
	msg_l = reg_relaxed_read32((void *)(sprd_outbox_base + MBOX_MSG_L));
	msg_h = reg_relaxed_read32((void *)(sprd_outbox_base + MBOX_MSG_H));
	target_id = reg_relaxed_read32((void *)(sprd_outbox_base + MBOX_ID));
	pr_debug("id =%d, msg_l = 0x%x, msg_h = 0x%x\n",
		 target_id, (unsigned int)msg_l, (unsigned int)msg_h);

	fifo->msg = (msg_h << 32) | msg_l;
	fifo->core_id = target_id & MBOX_MAX_CORE_MASK;
	g_recv_cnt[fifo->core_id]++;
	reg_relaxed_write32((void *)(sprd_outbox_base + MBOX_TRI), TRIGGER);
}

static u8 mbox_read_all_fifo_msg()
{
	u32 fifo_sts;
	u8 fifo_depth;
	u8 rd, wt, cnt, i;

	fifo_sts = reg_relaxed_read32((void *)
				      (sprd_outbox_base +
				       MBOX_FIFO_INBOX_STS_1));
	wt = MBOX_GET_FIFO_WR_PTR(fifo_sts);
	rd = MBOX_GET_FIFO_RD_PTR(fifo_sts);
	pr_debug("debug fifo_sts:0x%x", fifo_sts);
	/* if fifo is full or empty, when the read ptr == write ptr */
	if (rd == wt) {
		if (fifo_sts & FIFO_FULL_FLAG) {
			g_outbox_full_cnt++;
			cnt = mbox_cfg.outbox_fifo_size;
		} else {
			cnt = 0;
		}
	} else {
		if (wt > rd)
			cnt = wt - rd;
		else
			cnt = mbox_cfg.outbox_fifo_size - rd + wt;
	}

	if (cnt == 0 || cnt > mbox_cfg.outbox_fifo_size)
		pr_err("[error]rd = %d, wt = %d, cnt = %d\n", rd, wt, cnt);

	pr_debug("rd = %d, wt = %d, cnt = %d\n", rd, wt, cnt);

	fifo_depth = 0;
	for (i = 0; i < cnt; i++) {
		fifo_depth++;
		mbox_raw_recv(&mbox_fifo[i]);
	}

	g_one_time_recv_cnt = cnt;
	return fifo_depth;
}

static irqreturn_t mbox_recv_irq(int irq_num, void *dev)
{
	u32 irq_status;
	int i = 0;
	void *priv_data;
	u8 target_id;
	u8 fifo_len;
	unsigned long jiff, jiff_total;
	struct vdsp_hw *hw = (struct vdsp_hw *)dev;
	struct vdsp_mbox_ctx_desc *ctx = hw->vdsp_mbox_desc;
	unsigned long flags = 0;

	jiff_total = jiffies;
	spin_lock_irqsave(&ctx->mbox_spinlock, flags);
	if (!ctx->mbox_active) {
		pr_err("mbox_recv_irqhandle mbox is not active\n");
		spin_unlock_irqrestore(&ctx->mbox_spinlock, flags);
		return IRQ_HANDLED;
	}
	/* get fifo status */
	irq_status =
	    reg_relaxed_read32((void *)(sprd_outbox_base + MBOX_IRQ_STS));
	irq_status = irq_status & (OUTBOX_CLR_IRQ_BIT | FIFO_CLR_WR_IRQ_BIT);

	pr_debug("irq_status =0x%08x\n", irq_status);

	fifo_len = mbox_read_all_fifo_msg();	/* mail save in mbox_fifo */

	/* clear irq mask & irq after read all msg, if clear before read,
	 * it will produce a irq again  */
	reg_relaxed_write32((void *)(sprd_outbox_base + MBOX_IRQ_STS),
			    irq_status);

	/* print the id of the fist mail to know who wake up ap */
	if (sipc_get_wakeup_flag())
		pr_debug("mbox: wake up by id = %d\n", mbox_fifo[0].core_id);

	for (i = 0; i < fifo_len; i++) {
		target_id = mbox_fifo[i].core_id;

		if (target_id >= mbox_cfg.core_cnt) {
			pr_err("target_id >= mbox_cfg.core_cnt\n");
			spin_unlock_irqrestore(&ctx->mbox_spinlock, flags);
			return IRQ_NONE;
		}

		if (mbox_chns[target_id].mbox_smsg_handler) {
			pr_debug("msg handle,index = %d, id = %d\n", i,
				 target_id);
			priv_data = mbox_chns[target_id].mbox_priv_data;
			/* get the jiffs before irq proc */
			jiff = jiffies;
			mbox_chns[target_id].mbox_smsg_handler(&mbox_fifo[i].
							       msg, priv_data);
			/* update the max jiff time */
			jiff = jiffies - jiff;
			if (jiff > mbox_chns[target_id].max_irq_proc_time)
				mbox_chns[target_id].max_irq_proc_time = jiff;
		} else if (mbox_fifo_bak_len < MAX_SMSG_BAK) {
			pr_debug("msg bak here,index =%d, id = %d\n", i,
				 target_id);
			memcpy(&mbox_fifo_bak[mbox_fifo_bak_len], &mbox_fifo[i],
			       sizeof(struct mbox_fifo_data_tag));
			mbox_fifo_bak_len++;
		} else {
			pr_err("msg drop here,index =%d, id = %d\n", i,
			       target_id);
		}
	}

	/* update the max total irq time */
	jiff_total = jiffies - jiff_total;
	if (jiff_total > max_total_irq_proc_time)
		max_total_irq_proc_time = jiff_total;

	max_total_irq_cnt++;
	spin_unlock_irqrestore(&ctx->mbox_spinlock, flags);
	return IRQ_HANDLED;
}

static void mbox_process_bak_msg(void)
{
	int i;
	int cnt = 0;
	int target_id = 0;
	void *priv_data;

	for (i = 0; i < mbox_fifo_bak_len; i++) {
		target_id = mbox_fifo_bak[i].core_id;
		/* has been procced */
		if (target_id == MBOX_MAX_CORE_CNT) {	/*hww doubt */
			cnt++;
			continue;
		}
		if (mbox_chns[target_id].mbox_smsg_handler) {
			pr_debug("index = %d, id = %d\n", i, target_id);

			priv_data = mbox_chns[target_id].mbox_priv_data;
			mbox_chns[target_id].
			    mbox_smsg_handler(&mbox_fifo_bak[i].msg, priv_data);
			/* set a mask indicate the bak msg is been procced */
			mbox_fifo_bak[i].core_id = MBOX_MAX_CORE_CNT;
			cnt++;
		} else {
			pr_err("mbox_smsg_handler NULL,index = %d, id = %d\n",
			       i, target_id);
		}
	}

	/* reset mbox_fifo_bak_len */
	if (mbox_fifo_bak_len == cnt)
		mbox_fifo_bak_len = 0;
}

static int mbox_register_irq(u8 target_id,
			     MBOX_FUNCALL irq_handler, void *priv_data)
{
	if (target_id >= mbox_cfg.core_cnt) {
		pr_err("invalid target_id:%d", target_id);
		return -EINVAL;
	}

	mbox_chns[target_id].mbox_smsg_handler = irq_handler;
	mbox_chns[target_id].mbox_priv_data = priv_data;

	return 0;
}

static int mbox_unregister_irq(u8 target_id)
{
	if (target_id >= mbox_cfg.core_cnt) {
		pr_err("invalid target_id:%d\n", target_id);
		return -EINVAL;
	}
	if (!mbox_chns[target_id].mbox_smsg_handler) {
		pr_err("handler is NULL already\n");
		return -EINVAL;
	}
	mbox_chns[target_id].mbox_smsg_handler = NULL;
	return 0;
}

static int mbox_send(u8 core_id, u64 msg)
{
	u32 l_msg = (u32) msg;
	u32 h_msg = (u32) (msg >> 32);
	u32 fifo_sts_1, fifo_sts_2, block, recv_flag;
	unsigned long recv_flag_cnt;

	pr_debug("core_id=%d\n", (u32) core_id);

	/* wait outbox recv flag, until flag is 0
	 * (mail be send to outbox will clear it)
	 */
	recv_flag_cnt = 0;
	recv_flag = 1 << (core_id + IN_OUTBOX_RECEIVING_FLAG_SHIFT);
	do {
		recv_flag_cnt++;
		fifo_sts_1 = reg_relaxed_read32((void *)(sprd_inbox_base +
							 MBOX_FIFO_INBOX_STS_1));
		fifo_sts_2 =
		    reg_relaxed_read32((void *)(sprd_inbox_base +
						MBOX_FIFO_INBOX_STS_2));
		block =
		    ((fifo_sts_2 & IN_OUTBOX_BLOCK_FLAG) >>
		     IN_OUTBOX_BLOCK_FLAG_SHIFT);

		/* if dst bit inbox block, we dont't send it */
		/* if block, outbox recv flag will always be 1,
		 * because mail cat't be send to outbox
		 */
		if (block & (1 << core_id))
			goto block_exit;
	}
	while (fifo_sts_1 & recv_flag);

	if (mbox_chns[core_id].max_recv_flag_cnt < recv_flag_cnt)
		mbox_chns[core_id].max_recv_flag_cnt = recv_flag_cnt;

	reg_relaxed_write32((void *)(sprd_inbox_base + MBOX_MSG_L), l_msg);
	reg_relaxed_write32((void *)(sprd_inbox_base + MBOX_MSG_H), h_msg);
	reg_relaxed_write32((void *)(sprd_inbox_base + MBOX_ID), core_id);
	reg_relaxed_write32((void *)(sprd_inbox_base + MBOX_TRI), TRIGGER);

	g_send_cnt[core_id]++;
	return 0;

block_exit:
	pr_err("mbox block or receiving ,core_id = %d, sts_1=0x%x, sts2=0x%x",
	       core_id, fifo_sts_1, fifo_sts_2);
	g_inbox_block_cnt++;

	return -EBUSY;
}

static void mbox_cfg_printk(void)
{
	pr_debug("inbox_base = 0x%x, outbox_base = 0x%x\n",
		 mbox_cfg.inbox_base, mbox_cfg.outbox_base);

	pr_debug("inbox_range = 0x%x, outbox_range = 0x%x\n",
		 mbox_cfg.inbox_range, mbox_cfg.outbox_range);

	pr_debug("inbox_fifo = %d, outbox_fifo = %d\n",
		 mbox_cfg.inbox_fifo_size, mbox_cfg.outbox_fifo_size);

	pr_debug("inbox_irq_mask = 0x%x, outbox_irq_mask = 0x%x\n",
		 mbox_cfg.inbox_irq_mask, mbox_cfg.outbox_irq_mask);

	pr_debug("sensor_core = %d\n", mbox_cfg.sensor_core);

	pr_debug("rd_bit = %d, rd_mask = %d\n", mbox_cfg.rd_bit,
		 mbox_cfg.rd_mask);

	pr_debug("wr_bit = %d, wr_mask = %d\n", mbox_cfg.wr_bit,
		 mbox_cfg.wr_mask);

	pr_debug("enable_reg = 0x%x, mask_bit = 0x%x\n",
		 mbox_cfg.enable_reg, mbox_cfg.mask_bit);

	pr_debug("prior_low = %d, prior_high = %d\n",
		 mbox_cfg.prior_low, mbox_cfg.prior_high);

	pr_debug("core_cnt = %d, version = %d\n",
		 mbox_cfg.core_cnt, mbox_cfg.version);
}

static int mbox_cfg_init(struct mbox_dts_cfg_tag *mbox_dts_cfg,
			 u8 * mbox_inited)
{
	unsigned long base;

	mailbox_gpr = mbox_dts_cfg->gpr;

	/* init enable reg and mask bit */
	mbox_cfg.enable_reg = mbox_dts_cfg->enable_reg;
	mbox_cfg.mask_bit = mbox_dts_cfg->mask_bit;
	mbox_cfg.version = mbox_dts_cfg->version;

	/* init inbox base */
	mbox_cfg.inbox_base = mbox_dts_cfg->inboxres.start;
	base = (unsigned long)ioremap_nocache(mbox_dts_cfg->inboxres.start,
					      resource_size
					      (&mbox_dts_cfg->inboxres));
	if (!base) {
		pr_err("[error] fail ioremap\n");
		return -EINVAL;
	}
	pr_debug("inboxres.start is:0x%lx , size:0x%x\n",
		 base, (u32) (resource_size(&mbox_dts_cfg->inboxres)));

	sprd_inbox_base = base + MBOX_VDSPAP_INBOX_OFFSET;
	sprd_outbox_base = base + MBOX_VDSPAP_OUTBOX_OFFSET;
	sprd_global_base = base + MBOX_VDSPAP_COMMON_OFFSET;

	/* init irq */
	mbox_cfg.inbox_irq = mbox_dts_cfg->inbox_irq;
	mbox_cfg.outbox_irq = mbox_dts_cfg->outbox_irq;
	mbox_cfg.outbox_sensor_irq = mbox_dts_cfg->outbox_sensor_irq;

	/* init core cnt */
	if (mbox_dts_cfg->core_cnt > MBOX_MAX_CORE_CNT) {
		pr_err("mbox:ERR on line %d!\n", __LINE__);
		iounmap((volatile void *)base);
		return -EINVAL;
	}
	mbox_cfg.core_cnt = mbox_dts_cfg->core_cnt;

	/* init fifo size */
	mbox_cfg.inbox_fifo_size = MBOX_V2_INBOX_FIFO_SIZE;
	mbox_cfg.outbox_fifo_size = MBOX_V2_OUTBOX_FIFO_SIZE;

	/* init fifo read ptr */
	mbox_cfg.rd_bit = FIFO_RD_PTR_BIT;
	mbox_cfg.rd_mask = MBOX_V2_READ_PT_SHIFT;

	/* init fifo write ptr */
	mbox_cfg.wr_bit = FIFO_WR_PTR_BIT;
	mbox_cfg.wr_mask = MBOX_V2_WRITE_PT_SHIFT;

	/* init core range */
	mbox_cfg.inbox_range = MBOX_V2_INBOX_CORE_SIZE;
	mbox_cfg.outbox_range = MBOX_V2_OUTBOX_CORE_SIZE;

	/* init irq mask */
	mbox_cfg.inbox_irq_mask = MBOX_V2_INBOX_IRQ_MASK;
	mbox_cfg.outbox_irq_mask = MBOX_V2_OUTBOX_IRQ_MASK;

	mbox_cfg_printk();

	/* lock irq here until g_mbox_inited =1,
	 * it make sure that when irq handle come,
	 * g_mboX_inited is 1
	 */
	*mbox_inited = 1;

	return 0;
}

static int mbox_enable(void *ctx)
{
	int ret = 0;
	unsigned long flags = 0;
	struct vdsp_mbox_ctx_desc *context = (struct vdsp_mbox_ctx_desc *)ctx;

	if (mailbox_gpr == NULL) {
		pr_err("[error] mailbox_gpr is null, ret is %d\n", ret);
		return -EINVAL;
	}

	/*power domain on follow cam sys */
	regmap_update_bits(context->mm_ahb, 0, MM_AHB_MBOX_EB, ~((uint32_t) 0));
	pr_debug("enable mailbox clk!\n");

	reg_relaxed_write32((void *)(sprd_outbox_base + MBOX_FIFO_RST),
			    FIFO_RESET_BIT);
	pr_debug("fifo rst\n");

	g_inbox_irq_mask = mbox_cfg.inbox_irq_mask;
	reg_relaxed_write32((void *)(sprd_inbox_base + MBOX_IRQ_MSK),
			    mbox_cfg.inbox_irq_mask);
	pr_debug("fifo inbox irq msk 0x%x\n", mbox_cfg.inbox_irq_mask);

	reg_relaxed_write32((void *)(sprd_outbox_base + MBOX_IRQ_MSK),
			    mbox_cfg.outbox_irq_mask);
	pr_debug("fifo outbox irq msk 0x%x\n", mbox_cfg.outbox_irq_mask);

	reg_relaxed_write32((void *)(sprd_outbox_base + MBOX_FIFO_DEPTH),
			    mbox_cfg.outbox_fifo_size - 1);
	pr_debug("set outbox fifo depth\n");

	spin_lock_init(&context->mbox_spinlock);
	spin_lock_irqsave(&context->mbox_spinlock, flags);
	context->mbox_active = 1;
	spin_unlock_irqrestore(&context->mbox_spinlock, flags);

	return ret;

}

static int mbox_disable(void *ctx)
{
	int ret = 0;
	unsigned long flags = 0;
	struct vdsp_mbox_ctx_desc *context = (struct vdsp_mbox_ctx_desc *)ctx;

	if (mailbox_gpr) {
		spin_lock_irqsave(&context->mbox_spinlock, flags);
		context->mbox_active = 0;
		spin_unlock_irqrestore(&context->mbox_spinlock, flags);
		/*mailbox disable */
		regmap_update_bits(context->mm_ahb, 0, MM_AHB_MBOX_EB, 0);
		/*power domain off follow cam sys */
	} else {
		ret = -EINVAL;
		pr_err("[error] mailbox_gpr is null, ret is %d\n", ret);
	}
	return ret;
}

static const struct mbox_operations_tag mbox_r2p0_operation = {
	.cfg_init = mbox_cfg_init,
	.phy_register_irq_handle = mbox_register_irq,
	.phy_unregister_irq_handle = mbox_unregister_irq,
	.recv_irqhandle = mbox_recv_irq,
	.phy_send = mbox_send,
	.process_bak_msg = mbox_process_bak_msg,
	.enable = mbox_enable,
	.disable = mbox_disable,
};

static struct mbox_device_tag mbox_r2p0_device = {
	.version = 0x200,
	.max_cnt = MBOX_MAX_CORE_CNT,
	.fops = &mbox_r2p0_operation,
};

void mbox_get_phy_device(struct mbox_device_tag **mbox_dev)
{
	*mbox_dev = &mbox_r2p0_device;
}
