
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

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include "xrp_kernel_defs.h"
#include "vdsp_hw.h"
#include "xrp_internal.h"
#include "mmsys_dvfs_comm.h"
#include "mm_dvfs.h"
#include "vdsp_mailbox_drv.h"
#include "vdsp_dvfs.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: hw %d: %d %s:" \
	fmt, current->pid, __LINE__, __func__

#define VDSP_RECV_NORMAL_MSG    0xF8F8F8F8F8F8F8F8ULL
#define VDSP_RECV_LOG_MSG    0xF5F5F5F5F5F5F5F5ULL
#define VDSP_SEND_NORMAL_MSG   0xABABABABABABABABULL

#define PD_VDSP_SHUTDOWN	0x7
#define PD_VDSP_WAKEUP		0x0

#define MBOXID_AP	(0x0)
#define MBOXID_CH	(0x1)
#define MBOXID_VDSP	(0x2)

int sprd_cam_pw_on(void);
int sprd_cam_pw_off(void);
int sprd_cam_domain_eb(void);
int sprd_cam_domain_disable(void);

static const char *const syscon_name[] = {
	"vdsp_force_shutdown",
	"vdsp_auto_shutdown",
	"vdsp_power_state",
	"vdsp_intr_disable",
	"vdsp_deepsleep_enable",
	"vdsp_pd_sel",
	"vdsppll_force_off"
};

enum {
	FORCE_SHUTDOWN = 0,
	AUTO_SHUTDOWN,
	PW_STATUS,
	INTR_DIS,
	DPSL_EN,
	VDSP_PD_SEL,
	VDSPPLL_FORCE_OFF
};

struct register_gpr {
	struct regmap *gpr;
	uint32_t reg;
	uint32_t mask;
};

struct vdsp_dts_info {

	struct register_gpr regs[ARRAY_SIZE(syscon_name)];
};

static struct vdsp_dts_info *dts_info;

static inline void reg_write32(struct vdsp_hw *hw, void *addr, u32 v)
{
	__raw_writel(v, addr);
}

static inline u32 reg_read32(struct vdsp_hw *hw, void *addr)
{
	return __raw_readl(addr);
}

static inline void reg_write32_setbit(struct vdsp_hw *hw, void *addr, u32 v)
{
	volatile u32 value;

	value = __raw_readl(addr);
	value = v | value;
	__raw_writel(value, addr);
}

static inline void reg_write32_clearbit(struct vdsp_hw *hw, void *addr, u32 v)
{
	volatile u32 value;

	value = __raw_readl(addr);
	value = v & value;
	__raw_writel(value, addr);
}

static void regmap_update_bits_vdsp(struct register_gpr *p, uint32_t val)
{
	if ((!p) || (!(p->gpr))) {
		pr_err("HWW [error] %s:%s\n", __FILE__, __FUNCTION__);
		return;
	}
	regmap_update_bits(p->gpr, p->reg, p->mask, val);
}

static int regmap_read_vdsp(struct register_gpr *p, uint32_t * val)
{
	int ret = 0;

	if ((!p) || (!(p->gpr)) || (!val))
		return -1;
	ret = regmap_read(p->gpr, p->reg, val);
	if (!ret)
		*val &= (uint32_t) p->mask;
	return ret;
}

int regmap_read_separate(struct regmap *regmap,
			 uint32_t reg, uint32_t mask, uint32_t * val)
{
	int ret = 0;

	if ((!(regmap)) || (!val))
		return -1;
	ret = regmap_read(regmap, reg, val);
	if (!ret)
		*val &= (uint32_t) mask;
	return ret;
}

#if 0
static void parse_qos(void *hw_arg, void *of_node)
{
	struct device_node *qos_node = NULL;
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;

	if ((NULL == hw_arg) || (NULL == of_node)) {
		pr_err("hw_arg:%lx , of_node:%lx\n",
		       (unsigned long)hw_arg, (unsigned long)of_node);
		return;
	}
	qos_node = of_parse_phandle(of_node, "vdsp-qos", 0);
	if (qos_node) {
		if (of_property_read_u8(qos_node,
					"arqos-vdsp-msti",
					&hw->qos.ar_qos_vdsp_msti))
			hw->qos.ar_qos_vdsp_msti = 6;

		if (of_property_read_u8(qos_node,
					"awqos-vdsp-mstd",
					&hw->qos.aw_qos_vdsp_mstd))
			hw->qos.aw_qos_vdsp_mstd = 6;

		if (of_property_read_u8(qos_node,
					"arqos-vdsp-mstd",
					&hw->qos.ar_qos_vdsp_mstd))
			hw->qos.ar_qos_vdsp_mstd = 6;

		if (of_property_read_u8(qos_node,
					"arqos-vdsp-idma",
					&hw->qos.ar_qos_vdsp_idma))
			hw->qos.ar_qos_vdsp_idma = 1;

		if (of_property_read_u8(qos_node,
					"awqos-vdsp-idma",
					&hw->qos.aw_qos_vdsp_idma))
			hw->qos.aw_qos_vdsp_idma = 1;

		if (of_property_read_u8
		    (qos_node, "arqos-vdma", &hw->qos.ar_qos_vdma))
			hw->qos.ar_qos_vdma = 1;

		if (of_property_read_u8
		    (qos_node, "awqos-vdma", &hw->qos.aw_qos_vdma))
			hw->qos.aw_qos_vdma = 1;

		if (of_property_read_u8(qos_node,
					"arqos-threshold",
					&hw->qos.ar_qos_threshold))
			hw->qos.ar_qos_threshold = 0x0f;

		if (of_property_read_u8(qos_node,
					"awqos-threshold",
					&hw->qos.aw_qos_threshold))
			hw->qos.aw_qos_threshold = 0x0f;

	} else {
		hw->qos.ar_qos_vdsp_msti = 6;
		hw->qos.ar_qos_vdsp_mstd = 6;
		hw->qos.aw_qos_vdsp_mstd = 6;
		hw->qos.ar_qos_vdsp_idma = 1;
		hw->qos.aw_qos_vdsp_idma = 1;
		hw->qos.ar_qos_vdma = 1;
		hw->qos.aw_qos_vdma = 1;
		hw->qos.ar_qos_threshold = 0x0f;
		hw->qos.aw_qos_threshold = 0x0f;
	}
	return;
}

static void set_qos(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;

	/*set qos threshold */
	APAHB_HREG_MWR(hw->ahb + REG_QOS_THRESHOLD,
		       (0xf << 28 | 0xf << 24),
		       ((hw->qos.ar_qos_threshold << 28)
			| (hw->qos.aw_qos_threshold << 24)));
	/*set qos 3 */
	APAHB_HREG_MWR(hw->ahb + REG_QOS_3,
		       0xf0ffffff, ((hw->qos.ar_qos_vdsp_msti << 28)
				    | (hw->qos.ar_qos_vdsp_mstd << 20)
				    | (hw->qos.aw_qos_vdsp_mstd << 16)
				    | (hw->qos.ar_qos_vdsp_idma << 12)
				    | (hw->qos.aw_qos_vdsp_idma << 8)
				    | (hw->qos.ar_qos_vdma << 4) | (hw->qos.
								    aw_qos_vdma)));
	/*set qos sel 3 */
	APAHB_HREG_MWR(hw->ahb + REG_QOS_SEL3, 0x7f, 0x7f);	/*0 from master 1 from autoreg */
}
#endif //#if 0 1 qos

static uint32_t translate_dvfsindex_to_freq(uint32_t index)
{
	switch (index) {
	case 0:
		return VDSP_CLK260;
	case 1:
		return VDSP_CLK3072;
	case 2:
		return VDSP_CLK5120;
	case 3:
		return VDSP_CLK6144;
	case 4:
		return VDSP_CLK8192;
	case 5:
		return VDSP_CLK10140;
	default:
		return VDSP_CLK260;
	}
}

static void *get_hw_sync_data(void *hw_arg, size_t * sz, uint32_t log_addr)
{
	static const u32 irq_mode[] = {
		[XRP_IRQ_NONE] = XRP_DSP_SYNC_IRQ_MODE_NONE,
		[XRP_IRQ_LEVEL] = XRP_DSP_SYNC_IRQ_MODE_LEVEL,
		[XRP_IRQ_EDGE] = XRP_DSP_SYNC_IRQ_MODE_EDGE,
		[XRP_IRQ_EDGE_SW] = XRP_DSP_SYNC_IRQ_MODE_EDGE,
	};
	struct vdsp_hw *hw = hw_arg;
	struct vdsp_side_sync_data *hw_sync_data =
	    kmalloc(sizeof(*hw_sync_data), GFP_KERNEL);

	if (!hw_sync_data) {
		pr_err("hw_sync_data is NULL !!!\n");
		return NULL;
	}

	*hw_sync_data = (struct vdsp_side_sync_data) {
	.device_mmio_base = hw->mbox_phys,.host_irq_mode =
		    hw->host_irq_mode,.host_irq_offset =
		    hw->host_irq[0],.host_irq_bit =
		    hw->host_irq[1],.device_irq_mode =
		    irq_mode[hw->device_irq_mode],.device_irq_offset =
		    hw->device_irq[0],.device_irq_bit =
		    hw->device_irq[1],.device_irq =
		    hw->device_irq[2],.vdsp_smsg_addr =
		    (unsigned int)*sz,.vdsp_log_addr = log_addr,};
	pr_debug("device_mmio_base:%lx , (host_irq)mode:%d, offset:%d, bit:%d,"
		 "(device_irq)mode:%d, offset:%d, bit:%d, irq:%d"
		 "vdsp_smsg addr:0x%lx, vdsp_log_addr:0x%lx\n",
		 hw_sync_data->device_mmio_base, hw_sync_data->host_irq_mode,
		 hw_sync_data->host_irq_offset, hw_sync_data->host_irq_bit,
		 hw_sync_data->device_irq_mode, hw_sync_data->device_irq_offset,
		 hw_sync_data->device_irq_bit, hw_sync_data->device_irq,
		 hw_sync_data->vdsp_smsg_addr, hw_sync_data->vdsp_log_addr);

	*sz = sizeof(*hw_sync_data);

	return hw_sync_data;
}

static void reset(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;

	regmap_update_bits(hw->mm_ahb, REG_RESET, VDSP_RESET, ~((uint32_t) 0));
	regmap_update_bits(hw->mm_ahb, REG_RESET, VDSP_RESET, 0);
}

static void halt(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;
	uint32_t val = 0;

	if (regmap_read_separate(hw->mm_ahb, VDSP_CORE_CFG, 0xffffffff, &val))
		pr_err("error read mmahb:%d", VDSP_CORE_CFG);
	val |= (VDSP_RUNSTALL);
	if (regmap_write(hw->mm_ahb, VDSP_CORE_CFG, val))
		pr_err("error write mmahb:%d", VDSP_CORE_CFG);
}

static void stop_vdsp(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;
	uint32_t val = 0;

	/*mask all interrupt and wait vdsp to pwaitmode */
	regmap_update_bits_vdsp(&dts_info->regs[INTR_DIS], ~((uint32_t) 0));
	pr_debug("func:%s before wait pwaitmode\n", __func__);

	do {
		udelay(100);
		regmap_read_separate(hw->mm_ahb, VDSP_CORE_CFG, VDSP_PWAITMODE,
				     &val);
	}
	while (val == 0);
}

static void release(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;
	uint32_t val = 0;

	if (regmap_read_separate(hw->mm_ahb, VDSP_CORE_CFG, 0xffffffff, &val))
		pr_err("error read mmahb:%d", VDSP_CORE_CFG);
	val &= ~(VDSP_RUNSTALL);	/*clear bit2 */
	if (regmap_write(hw->mm_ahb, VDSP_CORE_CFG, val))
		pr_err("error write mmahb:%d", VDSP_CORE_CFG);
}

static void get_max_freq(uint32_t * max_freq)
{
	*max_freq = translate_dvfsindex_to_freq((uint32_t) 5);
	pr_debug("func:%s , max_freq:%d\n", __func__, *max_freq);
}

static int vdsp_power_on(void *hw_arg)
{
	int ret = 0;
	unsigned int power_state1 = 0;
	unsigned int power_state2 = 0;
	unsigned int power_state3 = 0;
	unsigned int cycle = 0;

	/* vdsp domain power on */
	/* 1:auto shutdown en, shutdown with ap; 0: control by bit25 */
	regmap_update_bits_vdsp(&dts_info->regs[AUTO_SHUTDOWN], 0);
	/* set 0 to shutdown */
	regmap_update_bits_vdsp(&dts_info->regs[FORCE_SHUTDOWN], 0);
	/* deep sleep disable */
	regmap_update_bits_vdsp(&dts_info->regs[DPSL_EN], 0);

	/*Wait for the pmu state to stabilize */
	do {
		udelay(25);	/*About a few hundred microsecond */
		cycle++;
		if (regmap_read_vdsp(&dts_info->regs[PW_STATUS], &power_state1)
		    || regmap_read_vdsp(&dts_info->regs[PW_STATUS],
					&power_state2)
		    || regmap_read_vdsp(&dts_info->regs[PW_STATUS],
					&power_state3)) {
			pr_err("[error] get vdsp power states\n");
			ret = -ENXIO;
			goto error;
		}
	}
	while (((power_state1 != PD_VDSP_WAKEUP) ||
		(power_state1 != power_state2) ||
		(power_state2 != power_state3)) && (cycle < 200));

	pr_debug("vdsp pw status, 0x%x, 0x%x, 0x%x\ncycle:%d\n",
		 power_state1, power_state2, power_state3, cycle);
	if (cycle >= 200) {
		ret = -EPERM;
		pr_err("[error]cycle timeout %d\n", cycle);
		goto error;
	}
	if (power_state1 != PD_VDSP_WAKEUP) {
		pr_err("[error]fail to get vdsp power state 0x%x\n",
		       power_state1);
		ret = -ENOSYS;
		goto error;
	}
	return ret;

error:
	regmap_update_bits_vdsp(&dts_info->regs[FORCE_SHUTDOWN],
				~((uint32_t) 0));
	return ret;
}

static int vdsp_power_off(void *hw_arg)
{
	unsigned int pwait = 0;
	int cycle;
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;
	unsigned int power_state1, power_state2, power_state3;

	pr_debug("%s,%d\n", __FUNCTION__, __LINE__);
	regmap_update_bits_vdsp(&dts_info->regs[INTR_DIS], ~((uint32_t) 0));
	/* Wait 100 ms for pwait mode */
	for (cycle = 0; cycle < 4000; cycle++) {
		if (regmap_read_separate
		    (hw->mm_ahb, VDSP_CORE_CFG, 0x1000, &pwait)) {
			pr_err("[error] can not get pwait states");
			return -ENXIO;
		}
		if ((pwait & VDSP_PWAITMODE) != VDSP_PWAITMODE) {
			udelay(25);
		} else {
			break;
		}
	}
	pr_debug("vdsp pwaitmode reg: 0x%x, wait cycle:%d\n", pwait, cycle);

	if (cycle == 4000) {
		pr_err("[error]vdsp cannot pwaitmode!!");
		regmap_update_bits_vdsp(&dts_info->regs[FORCE_SHUTDOWN],
					~((uint32_t) 0));
		regmap_update_bits_vdsp(&dts_info->regs[AUTO_SHUTDOWN], 0);
		return -ENOMSG;
	} else {
		regmap_update_bits_vdsp(&dts_info->regs[DPSL_EN],
					~((uint32_t) 0));
		regmap_update_bits_vdsp(&dts_info->regs[VDSP_PD_SEL], 0);
		regmap_update_bits_vdsp(&dts_info->regs[AUTO_SHUTDOWN],
					~((uint32_t) 0));
	}

	/*Wait for the pmu state to stabilize */
	cycle = 0;
	do {
		udelay(25);	/*About a few hundred microsecond */
		cycle++;
		if (regmap_read_vdsp(&dts_info->regs[PW_STATUS], &power_state1)
		    || regmap_read_vdsp(&dts_info->regs[PW_STATUS],
					&power_state2)
		    || regmap_read_vdsp(&dts_info->regs[PW_STATUS],
					&power_state3)) {
			pr_err("[error] get vdsp power states");
			return -ENXIO;
		}
	}
	while (((power_state1 != PD_VDSP_SHUTDOWN) ||
		(power_state1 != power_state2) ||
		(power_state2 != power_state3)) && (cycle < 200));

	pr_debug("vdsp pw status, 0x%x, 0x%x, 0x%x, cycle:%d\n",
		 power_state1, power_state2, power_state3, cycle);
	if (cycle >= 200) {
		pr_err("[error]cycle timeout %d\n", cycle);
		return -EPERM;
	}
	if (power_state1 != PD_VDSP_SHUTDOWN) {
		pr_err("[error]fail to get vdsp power state 0x%x\n",
		       power_state1);
		return -ENOSYS;
	}

	return 0;
}

static int vdsp_blk_enable(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;
	int ret = 0;

	/* vdsp blk en */
	ret =
	    regmap_update_bits(hw->mm_ahb, VDSP_BLK_EN, 0x1CFF,
			       ~((uint32_t) 0));
	if (ret)
		pr_err("set vdsp blk failed:%d\n", ret);
	/*vdsp int mask */
	ret = regmap_update_bits(hw->mm_ahb, VDSP_INT_MASK, 0xFFFF, 0);
	if (ret)
		pr_err("clear intr mask failed:%d\n", ret);
	return ret;
}

static int vdsp_blk_disable(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;
	int ret = 0;

	/* vdsp blk disable */
	ret = regmap_update_bits(hw->mm_ahb, VDSP_BLK_EN, 0x1CFF, 0);
	if (ret)
		pr_err("clear vdsp blk failed:%d\n", ret);
	/*vdsp int mask all */
	ret =
	    regmap_update_bits(hw->mm_ahb, VDSP_INT_MASK, 0x3F,
			       ~((uint32_t) 0));
	if (ret)
		pr_err("set vdsp blk failed:%d\n", ret);
	return ret;
}

static int enable(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;
	int ret = 0;

	pr_debug("func:%s hw:%lx, mmahb:%lx\n", __func__, hw, hw->mm_ahb);

	ret = sprd_cam_pw_on();	//set reference num increase
	if (ret) {
		pr_err("[error]cam pw on ret:%d", ret);
		return ret;
	}
	ret = sprd_cam_domain_eb();
	if (ret) {
		pr_err("[error]cam doamin eb ret:%d", ret);
		goto err_cam_eb;
	}

	/* cancel force off vdsppll, need to wait 224us */
	regmap_update_bits_vdsp(&dts_info->regs[VDSPPLL_FORCE_OFF], 0);
	udelay(224);

	ret = vdsp_power_on(hw);
	if (ret) {
		pr_err("[error]vdsp pw on ret:%d", ret);
		goto err_dsp_pw_on;
	}
	ret = vdsp_blk_enable(hw);
	if (ret) {
		pr_err("[error]vdsp pw on ret:%d", ret);
		goto err_dsp_blk_eb;
	}
	return ret;

err_dsp_blk_eb:
	vdsp_power_off(hw);
err_dsp_pw_on:
	regmap_update_bits_vdsp(&dts_info->regs[VDSPPLL_FORCE_OFF], 0x2);
	sprd_cam_domain_disable();
err_cam_eb:
	sprd_cam_pw_off();
	return ret;
}

static int disable(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;
	int ret = 0;

	ret = vdsp_blk_disable(hw);
	if (ret) {
		pr_err("[error]cam disable ret:%d\n", ret);
	}
	ret = vdsp_power_off(hw);
	if (ret) {
		pr_err("[error]vdsp pw off ret:%d\n", ret);
	}
	/*force off vdsppll, need after vdsp power off */
	regmap_update_bits_vdsp(&dts_info->regs[VDSPPLL_FORCE_OFF], 0x2);

	ret = sprd_cam_domain_disable();
	if (ret) {
		pr_err("[error]cam dm disable ret:%d\n", ret);
	}
	ret = sprd_cam_pw_off();	/*set reference num decrease */
	if (ret) {
		pr_err("[error]cam pw off ret:%d\n", ret);
	}
	return ret;
}

static void enable_dvfs(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;

	if (!(hw->mm_ahb)) {
		pr_err("Invalid argument\n");
		return;
	}
	if (regmap_update_bits(hw->mm_ahb, MM_SYS_EN, DVFS_EN, DVFS_EN))
		pr_err("error enable dvfs\n");
}

static void disable_dvfs(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;

	if (!(hw->mm_ahb)) {
		pr_err("Invalid argument\n");
		return;
	}
	if (regmap_update_bits(hw->mm_ahb, MM_SYS_EN, DVFS_EN, 0))
		pr_err("error disable dvfs\n");
}

static void setdvfs(void *hw_arg, uint32_t index)
{
#ifdef WORKAROUND_USING_SW_DVFS
	struct dvfs_ops_sw *dvfs_sw = get_dvfs_ops_sw();

	dvfs_sw->setdvfs_index(NULL, index);
	pr_debug("using sw dvfs [workaround], index:%d", index);
#else /* #ifdef WORKAROUND_USING_SW_DVFS */
	uint32_t freq = translate_dvfsindex_to_freq(index);

	struct ip_dvfs_ops *vdsp_dvfs_ops_ptr = NULL;

	vdsp_dvfs_ops_ptr = get_vdsp_dvfs_ops();
	pr_debug("freq:%d, index:%d\n", freq, index);
	vdsp_dvfs_ops_ptr->set_work_freq(NULL, freq);
#endif /* #ifdef WORKAROUND_USING_SW_DVFS */
}

static int send_irq(void *hw_arg)
{
	struct vdsp_hw *hw = hw_arg;
	uint64_t msg = VDSP_SEND_NORMAL_MSG;

	return hw->vdsp_mbox_desc->ops->irq_send(hw->device_irq[1], msg);
}

static irqreturn_t xrp_hw_irq_handler(void *ptr, void *dev_id)
{
	struct vdsp_hw *hw = dev_id;
	uint64_t msg = *((uint64_t *) ptr);

	/*if log interrupt */
	if (msg == VDSP_RECV_LOG_MSG) {
		return vdsp_log_irq_handler(0, hw->xrp);
	} else if (msg == VDSP_RECV_NORMAL_MSG) {	/*normal interrupt */
		return xrp_irq_handler(0, hw->xrp);
	}
	pr_err("func:%s err msg content msg:%llx\n", __func__, msg);
	return IRQ_NONE;
}

#if 0
static void memcpy_hw_function(void __iomem * dst, const void *src, size_t sz)
{
	memcpy(dst, src, sz);
	return;
}

static void memset_hw_function(void __iomem * dst, int c, size_t sz)
{
	memset(dst, c, sz);
	return;
}
#endif
static int init_mbox(void *hw_arg)
{
	struct vdsp_mbox_ctx_desc *mboxdesc = NULL;

	mboxdesc = get_vdsp_mbox_ctx_desc();
	if (mboxdesc) {
		pr_debug("func:%s mbox init called\n", __func__);
		return mboxdesc->ops->ctx_init(mboxdesc);
	}
	pr_err("func:%s get_vdsp_mbox_ctx_desc failed\n", __func__);
	return -1;
}

static int deinit_mbox(void *hw_arg)
{
	struct vdsp_mbox_ctx_desc *mboxdesc = NULL;

	mboxdesc = get_vdsp_mbox_ctx_desc();
	if (mboxdesc) {
		pr_debug("func:%s mbox deinit called\n", __func__);
		return mboxdesc->ops->ctx_deinit(mboxdesc);
	}
	pr_err("func:%s get_vdsp_mbox_ctx_desc failed\n", __func__);
	return -1;
}

static enum sprd_vdsp_kernel_power_level translate_powerlevel_fromuser(int32_t
								       in)
{
	switch (in) {
	case -1:
		return SPRD_VDSP_KERNEL_POWERHINT_RESTORE_DVFS;
	case 0:
		return SPRD_VDSP_KERNEL_POWERHINT_LEVEL_0;
	case 1:
		return SPRD_VDSP_KERNEL_POWERHINT_LEVEL_1;
	case 2:
		return SPRD_VDSP_KERNEL_POWERHINT_LEVEL_2;
	case 3:
		return SPRD_VDSP_KERNEL_POWERHINT_LEVEL_3;
	case 4:
		return SPRD_VDSP_KERNEL_POWERHINT_LEVEL_4;
	case 5:
		return SPRD_VDSP_KERNEL_POWERHINT_LEVEL_5;
	default:
		return SPRD_VDSP_KERNEL_POWERHINT_LEVEL_MAX;
	}
}

static enum sprd_vdsp_kernel_power_level get_maxsupported_level(void *data)
{
	struct xvp *xvp = (struct xvp *)data;

	pr_debug("xvp is:%lx\n", xvp);
	return SPRD_VDSP_KERNEL_POWERHINT_LEVEL_5;
}

int vdsp_request_irq(void *xvp_arg, void *hw_arg)
{
	struct device *dev = (struct device *)xvp_arg;
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;
	int ret;

	//struct platform_device *pdev = container_of(dev, struct platform_device, dev);

	pr_debug("dev %p ,request irq %d handle %p hw %p\n",
		 dev, hw->client_irq, hw->vdsp_mbox_desc->ops->irq_handler, hw);
	ret = devm_request_irq(dev,
			       hw->client_irq,
			       hw->vdsp_mbox_desc->ops->irq_handler,
			       IRQF_SHARED /*IRQF_ONESHOT */ ,
			       DRIVER_NAME, hw);
	if (ret < 0) {
		pr_err("devm_request_irq fail, ret %d", ret);
		return ret;
	}

	return ret;
}

void vdsp_free_irq(void *xvp_arg, void *hw_arg)
{
	struct device *dev = (struct device *)xvp_arg;
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;

	pr_debug("free irq %d dev %p hw %p\n", hw->client_irq, dev, hw);
	devm_free_irq(dev, hw->client_irq, hw);
}

static const struct xrp_hw_ops hw_ops = {
	.halt = halt,
	.release = release,
	.reset = reset,
	.get_hw_sync_data = get_hw_sync_data,
	.send_irq = send_irq,
	.memcpy_tohw = NULL,	/*memcpy_hw_function */
	.memset_hw = NULL,	/*memset_hw_function */
	.enable = enable,
	.disable = disable,
	.enable_dvfs = enable_dvfs,
	.disable_dvfs = disable_dvfs,
	.setdvfs = setdvfs,
	.vdsp_request_irq = vdsp_request_irq,
	.vdsp_free_irq = vdsp_free_irq,
	.get_max_freq = get_max_freq,
	.stop_vdsp = stop_vdsp,
	.init_communication_hw = init_mbox,
	.deinit_communication_hw = deinit_mbox,
	.translate_powerlevel = translate_powerlevel_fromuser,
	.get_maxsupported_level = get_maxsupported_level,
};

static long sprd_vdsp_parse_mem(struct vdsp_hw *hw)
{
	struct device_node *np;
	u32 out_values[4];
	int ret;

	np = of_find_node_by_name(NULL, "vdsp-mem");
	if (!np) {
		pr_err("find memory node fail\n");
		return -ENOENT;
	}
	ret = of_property_read_u32_array(np, "reg", out_values, 4);

	if (!ret) {
		hw->vdsp_reserved_mem_addr = out_values[0];
		hw->vdsp_reserved_mem_addr = hw->vdsp_reserved_mem_addr << 32;
		hw->vdsp_reserved_mem_addr |= out_values[1];

		hw->vdsp_reserved_mem_size = out_values[2];
		hw->vdsp_reserved_mem_size = hw->vdsp_reserved_mem_size << 32;
		hw->vdsp_reserved_mem_size |= out_values[3];
	} else {
		hw->vdsp_reserved_mem_addr = 0;
		hw->vdsp_reserved_mem_size = 0;
	}
	pr_debug("vdsp reserved mem addr %llx size %llx\n",
		 hw->vdsp_reserved_mem_addr, hw->vdsp_reserved_mem_size);

	return 0;
}

static long sprd_vdsp_parse_hw_dt(struct platform_device *pdev,
				  struct vdsp_hw *hw,
				  int mem_idx, enum vdsp_init_flags *init_flags)
{
	int irq;
	long ret;
	const char *pname;
	struct regmap *tregmap;
	int i, val;
	struct device_node *np = pdev->dev.of_node;
	uint32_t args[2];
	struct device_node *sub_np;
	struct resource parse_resource = { 0 };

	dts_info = devm_kzalloc(&pdev->dev, sizeof(*dts_info), GFP_KERNEL);
	hw->mm_ahb = syscon_regmap_lookup_by_phandle(np, "sprd,syscon-mmahb");
	hw->mailbox =
	    syscon_regmap_lookup_by_phandle(np, "sprd,syscon-mailbox");
	hw->mm_dvfs = syscon_regmap_lookup_by_phandle(np, "sprd,syscon-mmdvfs");
	hw->mm_clkcfg =
	    syscon_regmap_lookup_by_phandle(np, "sprd,syscon-mmclkcfg");
	hw->vdsp_uart =
	    syscon_regmap_lookup_by_phandle(np, "sprd,syscon-vdspuart");
	if ((!(hw->mm_ahb)) || (!(hw->mailbox)) || (!(hw->mm_dvfs))
	    || (!(hw->mm_clkcfg) || (!(hw->vdsp_uart)))) {
		pr_err("get regmap failed\n");
	}

	sub_np = of_parse_phandle(np, "sprd,syscon-mailbox", 0);
	if (of_address_to_resource(sub_np, 0, &parse_resource)) {
		pr_err("get resource failed\n");
	}
	hw->mbox_phys = parse_resource.start;
	pr_debug("hw->mbox_phys:%p\n", hw->mbox_phys);

	/* read global register */
	for (i = 0; i < ARRAY_SIZE(syscon_name); i++) {
		pname = syscon_name[i];
		tregmap = syscon_regmap_lookup_by_name(np, pname);
		pr_debug("parse tregmap:0x%x", tregmap);
		if (IS_ERR_OR_NULL(tregmap)) {
			pr_err("fail to read %s regmap\n", pname);
			continue;
		}
		val = syscon_get_args_by_name(np, pname, 2, args);
		if (val != 2) {
			pr_err("fail to read %s args, ret %d\n", pname, val);
			ret = -1;
			continue;
		}
		dts_info->regs[i].gpr = tregmap;
		dts_info->regs[i].reg = args[0];
		dts_info->regs[i].mask = args[1];
		pr_debug("dts[%s] 0x%x 0x%x\n", pname,
			 dts_info->regs[i].reg, dts_info->regs[i].mask);
	}

#if 0
	/* qos */
	parse_qos(hw, pdev->dev.of_node);
#endif
	/* irq */
	ret = of_property_read_u32_array(pdev->dev.of_node,
					 "device-irq",
					 hw->device_irq,
					 ARRAY_SIZE(hw->device_irq));
	if (ret == 0) {
		u32 device_irq_host_offset;

		ret = of_property_read_u32(pdev->dev.of_node,
					   "device-irq-host-offset",
					   &device_irq_host_offset);
		if (ret == 0) {
			hw->device_irq_host_offset = device_irq_host_offset;
		} else {
			hw->device_irq_host_offset = hw->device_irq[0];
			ret = 0;
		}
	}
	if (ret == 0) {
		u32 device_irq_mode;

		ret = of_property_read_u32(pdev->dev.of_node,
					   "device-irq-mode", &device_irq_mode);
		if (likely(device_irq_mode < XRP_IRQ_MAX))
			hw->device_irq_mode = device_irq_mode;
		else
			ret = -ENOENT;
	}
	if (ret == 0) {
		pr_debug("device IRQ MMIO host offset = 0x%08x,"
			 "offset = 0x%08x, bit = %d,"
			 "device IRQ = %d, IRQ mode = %d",
			 hw->device_irq_host_offset,
			 hw->device_irq[0], hw->device_irq[1],
			 hw->device_irq[2], hw->device_irq_mode);
	} else {
		pr_debug("using polling mode on the device side\n");
	}

	ret = of_property_read_u32_array(pdev->dev.of_node,
					 "host-irq",
					 hw->host_irq,
					 ARRAY_SIZE(hw->host_irq));
	if (ret == 0) {
		u32 host_irq_mode;

		ret = of_property_read_u32(pdev->dev.of_node,
					   "host-irq-mode", &host_irq_mode);
		if (likely(host_irq_mode < XRP_IRQ_MAX))
			hw->host_irq_mode = host_irq_mode;
		else
			return -ENOENT;
	}

	if (ret == 0 && hw->host_irq_mode != XRP_IRQ_NONE)
		hw->client_irq = platform_get_irq(pdev, 0);
	else
		hw->client_irq = -1;

	hw->vdsp_mbox_desc = get_vdsp_mbox_ctx_desc();
	if (hw->vdsp_mbox_desc) {
		ret = hw->vdsp_mbox_desc->ops->mbox_init();
		printk("mbox_init ret:%d\n", ret);
		if (ret != 0) {
			ret = -EFAULT;
			goto err;
		}
	} else {
		ret = -EFAULT;
		pr_err("vdsp_mbox_desc is null error\n");
		goto err;
	}
	pr_debug("irq is:%d , ret:%ld , host_irq_mode:%d\n",
		 hw->client_irq, ret, hw->host_irq_mode);
	if (hw->client_irq >= 0) {
		pr_debug("host IRQ = %d,", hw->client_irq);

		if (hw->vdsp_mbox_desc) {
			hw->vdsp_mbox_desc->mbox_regmap = hw->mailbox;
			hw->vdsp_mbox_desc->irq_mode = hw->host_irq_mode;
			hw->vdsp_mbox_desc->mm_ahb = hw->mm_ahb;
			ret = vdsp_request_irq(&pdev->dev, hw);
			if (ret < 0) {
				pr_err("request_irq %d failed\n", irq);
				goto err;
			}

			hw->vdsp_mbox_desc->ops->irq_register(MBOXID_VDSP,
							      xrp_hw_irq_handler,
							      hw);
			hw->vdsp_mbox_desc->ops->irq_register(MBOXID_CH,
							      xrp_hw_irq_handler,
							      hw);
			*init_flags |= XRP_INIT_USE_HOST_IRQ;
		}
	} else {
		pr_debug("using polling mode on the host side\n");
	}
	ret = dma_set_mask(&pdev->dev, DMA_BIT_MASK(36));
	if (ret) {
		pr_err("Error: dma_set_mask failed\n");
		goto err;
	}
	ret = sprd_vdsp_parse_mem(hw);
err:
	return ret;
}

static long init_sprd(struct platform_device *pdev, struct vdsp_hw *hw)
{
	long ret;
	enum vdsp_init_flags init_flags = 0;

	ret = sprd_vdsp_parse_hw_dt(pdev, hw, 0, &init_flags);
	if (unlikely(ret < 0))
		return ret;
	return sprd_vdsp_init(pdev, init_flags, &hw_ops, hw);
}

#ifdef CONFIG_OF
static const struct of_device_id vdsp_device_match[] = {
	{
	 .compatible = "sprd,qogirn6pro-vdsp",
	 .data = init_sprd,
	 },
	{},
};

MODULE_DEVICE_TABLE(of, vdsp_device_match);
#endif

static int vdsp_driver_probe(struct platform_device *pdev)
{
	struct vdsp_hw *hw = devm_kzalloc(&pdev->dev, sizeof(*hw), GFP_KERNEL);
	const struct of_device_id *match;
	long (*init) (struct platform_device * pdev, struct vdsp_hw * hw);
	long ret;

	if (!hw)
		return -ENOMEM;

	match = of_match_device(of_match_ptr(vdsp_device_match), &pdev->dev);
	if (!match)
		return -ENODEV;

	init = match->data;
	ret = init(pdev, hw);
	if (IS_ERR_VALUE(ret)) {
		return ret;
	} else {
		hw->xrp = ERR_PTR(ret);
		return 0;
	}

}

static int vdsp_driver_remove(struct platform_device *pdev)
{
	return sprd_vdsp_deinit(pdev);
}

int vdsp_runtime_resume(struct device *dev)
{
	struct vdsp_mbox_ctx_desc *mboxdesc = NULL;
	struct xvp *xvp = dev_get_drvdata(dev);

	if (unlikely(xvp->off)) {
		//TBD CHECK
	}

	mboxdesc = get_vdsp_mbox_ctx_desc();
	if (mboxdesc) {
		pr_debug("mbox init called\n");
		return mboxdesc->ops->ctx_init(mboxdesc);
	}
	pr_err("func:%s get_vdsp_mbox_ctx_desc failed\n", __func__);
	return -1;

#if 0
	/*set qos */
	xvp_set_qos(xvp);
#endif
	return 0;
}

EXPORT_SYMBOL(vdsp_runtime_resume);

int vdsp_runtime_suspend(struct device *dev)
{
	struct vdsp_mbox_ctx_desc *mboxdesc = NULL;
	struct xvp *xvp = dev_get_drvdata(dev);

	halt(xvp->hw_arg);
#if 1
	mboxdesc = get_vdsp_mbox_ctx_desc();
	if (mboxdesc) {
		pr_debug("mbox deinit called\n");
		return mboxdesc->ops->ctx_deinit(mboxdesc);
	}
	pr_err("func:%s get_vdsp_mbox_ctx_desc failed\n", __func__);
	return -1;
#else
	ret = vdsp_commu_hw_suspend();
#endif
	return 0;
}

EXPORT_SYMBOL(vdsp_runtime_suspend);

int vdsp_hw_irq_register(void *data)
{
	struct vdsp_mbox_ctx_desc *mboxdesc = NULL;

	mboxdesc = get_vdsp_mbox_ctx_desc();
	if (mboxdesc) {
		return mboxdesc->ops->irq_register(0, xrp_hw_irq_handler, data);
	}
	return -1;
}

int vdsp_hw_irq_unregister()
{
	struct vdsp_mbox_ctx_desc *mboxdesc = NULL;

	mboxdesc = get_vdsp_mbox_ctx_desc();
	if (mboxdesc) {
		return mboxdesc->ops->irq_unregister(0);
	}
	return -1;
}

static struct platform_driver vdsp_driver = {
	.probe = vdsp_driver_probe,
	.remove = vdsp_driver_remove,
	.driver = {
		   .name = DRIVER_NAME,
		   .of_match_table = of_match_ptr(vdsp_device_match),
		   },
};

module_platform_driver(vdsp_driver);

MODULE_DESCRIPTION("Sprd VDSP Driver");
MODULE_AUTHOR("Vdsp@unisoc");
MODULE_LICENSE("GPL");
