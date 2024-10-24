
#ifndef VDSP_MAILBOX_DRV_H
#define VDSP_MAILBOX_DRV_H

#include <linux/sprd_mailbox.h>

#define SEND_FIFO_LEN 64
#define MBOX_INVALID_CORE	0xff

enum xrp_irq_mode {
	XRP_IRQ_NONE,
	XRP_IRQ_LEVEL,
	XRP_IRQ_EDGE,
	XRP_IRQ_EDGE_SW,
	XRP_IRQ_MAX,
};

struct mbox_dts_cfg_tag {
	struct regmap *gpr;
	u32 enable_reg;
	u32 mask_bit;
	struct resource inboxres;
	struct resource outboxres;
	struct resource commonres;
	u32 inbox_irq;
	u32 outbox_irq;
	u32 outbox_sensor_irq;
	u32 sensor_core;
	u32 core_cnt;
	u32 version;
};

struct mbox_chn_tag {
	MBOX_FUNCALL mbox_smsg_handler;
	unsigned long max_irq_proc_time;
	unsigned long max_recv_flag_cnt;
	void *mbox_priv_data;
};

struct mbox_operations_tag {
	int (*cfg_init) (struct mbox_dts_cfg_tag *, u8 *);
	int (*phy_register_irq_handle) (u8, MBOX_FUNCALL, void *);
	int (*phy_unregister_irq_handle) (u8);
	 irqreturn_t(*src_irqhandle) (int, void *);
	 irqreturn_t(*recv_irqhandle) (int, void *);
	 irqreturn_t(*sensor_recv_irqhandle) (int, void *);
	int (*phy_send) (u8, u64);
	void (*process_bak_msg) (void);
	 u32(*phy_core_fifo_full) (int);
	void (*phy_just_sent) (u8, u64);
	 bool(*outbox_has_irq) (void);
	int (*enable) (void *ctx);
	int (*disable) (void *ctx);
};

struct vdsp_mbox_ops;
struct vdsp_mbox_ctx_desc {
	struct regmap *mm_ahb;
	struct regmap *mbox_regmap;
	struct mbox_dts_cfg_tag mbox_cfg;
	/* how IRQ is used to notify the device of incoming data */
	enum xrp_irq_mode irq_mode;
	struct vdsp_mbox_ops *ops;
	spinlock_t mbox_spinlock;
	struct mutex mbox_lock;
	uint32_t mbox_active;
};

struct vdsp_mbox_ops {
	int (*ctx_init) (struct vdsp_mbox_ctx_desc * ctx);
	int (*ctx_deinit) (struct vdsp_mbox_ctx_desc * ctx);
	 irqreturn_t(*irq_handler) (int irq, void *arg);
	int (*irq_register) (u8 idx, MBOX_FUNCALL handler, void *arg);
	int (*irq_unregister) (u8 idx);
	int (*irq_send) (u8 idx, u64 msg);
	int (*irq_clear) (int idx);
	int (*mbox_init) (void);
};

struct vdsp_mbox_ctx_desc *get_vdsp_mbox_ctx_desc(void);
int vdsp_commu_hw_resume(void);
int vdsp_commu_hw_suspend(void);
#endif /* SPRD_MAILBOX_DRV_H */
