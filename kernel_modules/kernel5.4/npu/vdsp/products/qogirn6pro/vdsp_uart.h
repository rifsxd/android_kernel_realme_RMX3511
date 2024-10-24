
struct vdsp_uart_ops;
struct vdsp_uart_desc {
	struct vdsp_uart_ops *ops;
	struct regmap *mm_ahb_base;
	struct regmap *vdsp_cfg_base;
};

struct vdsp_uart_ops {
	int (*enable) (struct vdsp_uart_desc * ctx);
	int (*disable) (struct vdsp_uart_desc * ctx);

};

struct vdsp_uart_desc *get_vdsp_uart_desc(void);
