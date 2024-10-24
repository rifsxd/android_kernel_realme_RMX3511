#include "vdsp_uart.h"

static int uart_enable(struct vdsp_uart_desc *ctx)
{

	return 0;
}

static int uart_disable(struct vdsp_uart_desc *ctx)
{

	return 0;
}

struct vdsp_uart_ops vdsp_uart_ops = {
	.enable = uart_enable,
	.disable = uart_disable,

};

static struct vdsp_uart_desc sub_uart_desc = {
	.ops = &vdsp_uart_ops,
};

struct vdsp_uart_desc *get_vdsp_uart_desc(void)
{
	return &sub_uart_desc;
}
