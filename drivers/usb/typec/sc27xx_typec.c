// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for Spreadtrum SC27XX USB Type-C
 *
 * Copyright (C) 2020 Spreadtrum Communications Inc.
 */

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/usb/typec.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/extcon.h>
#include <linux/kernel.h>
#include <linux/nvmem-consumer.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/extcon-provider.h>
#include <linux/usb/tcpm.h>
#include <linux/iio/consumer.h>
#include <linux/workqueue.h>
/* registers definitions for controller REGS_TYPEC */
#define SC27XX_EN			0x00
#define SC27XX_MODE			0x04
#define SC27XX_INT_EN			0x0c
#define SC27XX_INT_CLR			0x10
#define SC27XX_INT_RAW			0x14
#define SC27XX_INT_MASK			0x18
#define SC27XX_STATUS			0x1c
#define SC27XX_TCCDE_CNT		0x20
#define SC27XX_RTRIM			0x3c
#define SC2730_DBG1			0x60

/* SC27XX_TYPEC_EN */
#define SC27XX_TYPEC_USB20_ONLY		BIT(4)

/* SC27XX_TYPEC MODE */
#define SC27XX_MODE_SNK			0
#define SC27XX_MODE_SRC			1
#define SC27XX_MODE_DRP			2
#define SC27XX_MODE_MASK		3

/* SC27XX_INT_EN */
#define SC27XX_ATTACH_INT_EN		BIT(0)
#define SC27XX_DETACH_INT_EN		BIT(1)

/* SC27XX_INT_CLR */
#define SC27XX_ATTACH_INT_CLR		BIT(0)
#define SC27XX_DETACH_INT_CLR		BIT(1)

/* SC27XX_INT_MASK */
#define SC27XX_ATTACH_INT		BIT(0)
#define SC27XX_DETACH_INT		BIT(1)

#define SC27XX_STATE_MASK		GENMASK(4, 0)
#define SC27XX_EVENT_MASK		GENMASK(9, 0)

#define SC2730_EFUSE_CC1_SHIFT		5
#define SC2730_EFUSE_CC2_SHIFT		0
#define SC2721_EFUSE_CC1_SHIFT		11
#define SC2721_EFUSE_CC2_SHIFT		6
#define UMP9620_EFUSE_CC1_SHIFT		1
#define UMP9620_EFUSE_CC2_SHIFT		11

#define SC27XX_CC1_MASK(n)		GENMASK((n) + 4, (n))
#define SC27XX_CC2_MASK(n)		GENMASK((n) + 4, (n))
#define SC27XX_CC_SHIFT(n)		(n)

/* sc2721 registers definitions for controller REGS_TYPEC */
#define SC2721_EN			0x00
#define SC2721_CLR			0x04
#define SC2721_MODE			0x08

/* SC2721_INT_EN */
#define SC2721_ATTACH_INT_EN		BIT(5)
#define SC2721_DETACH_INT_EN		BIT(6)

#define SC2721_STATE_MASK		GENMASK(3, 0)
#define SC2721_EVENT_MASK		GENMASK(6, 0)

/* modify sc2730 tcc debunce */
#define SC27XX_TCC_DEBOUNCE_CNT		0xc7f

/* sc2730 registers definitions for controller REGS_TYPEC */
#define SC2730_TYPEC_PD_CFG		0x08
/* SC2730_TYPEC_PD_CFG */
#define SC27XX_VCONN_LDO_EN		BIT(13)
#define SC27XX_VCONN_LDO_RDY		BIT(12)

/* 9620 typec ate current too larger */
#define UMP9620_RESERVERED_CORE		0x234c
#define TRIM_CURRENT_FROMEFUSE		BIT(4)

/* pmic name string */
#define SC2721		0x01
#define SC2730		0x02
#define UMP9620		0x03

/* SC2730_DBG1 */
#define SC2730_CC1_DFP_CHECK		BIT(7)
#define SC2730_CC2_DFP_CHECK		BIT(3)
#define SC2730_CC1_UFP_CON		BIT(0)
#define SC2730_CC_INSERT		GENMASK(7, 0)

#define BIT_TYPEC_CC_V2AD_EN		BIT(15)
#define CC_VOLT_LIMIT			4000 //CC short to vbus thershold
enum sc27xx_typec_connection_state {
	SC27XX_DETACHED_SNK,
	SC27XX_ATTACHWAIT_SNK,
	SC27XX_ATTACHED_SNK,
	SC27XX_DETACHED_SRC,
	SC27XX_ATTACHWAIT_SRC,
	SC27XX_ATTACHED_SRC,
	SC27XX_POWERED_CABLE,
	SC27XX_AUDIO_CABLE,
	SC27XX_DEBUG_CABLE,
	SC27XX_TOGGLE_SLEEP,
	SC27XX_ERR_RECOV,
	SC27XX_DISABLED,
	SC27XX_TRY_SNK,
	SC27XX_TRY_WAIT_SRC,
	SC27XX_TRY_SRC,
	SC27XX_TRY_WAIT_SNK,
	SC27XX_UNSUPOORT_ACC,
	SC27XX_ORIENTED_DEBUG,
};

struct sprd_typec_variant_data {
	u32 efuse_cc1_shift;
	u32 efuse_cc2_shift;
	u32 int_en;
	u32 int_clr;
	u32 mode;
	u32 attach_en;
	u32 detach_en;
	u32 state_mask;
	u32 event_mask;
};

static const struct sprd_typec_variant_data sc2730_data = {
	.efuse_cc1_shift = SC2730_EFUSE_CC1_SHIFT,
	.efuse_cc2_shift = SC2730_EFUSE_CC2_SHIFT,
	.int_en = SC27XX_INT_EN,
	.int_clr = SC27XX_INT_CLR,
	.mode = SC27XX_MODE,
	.attach_en = SC27XX_ATTACH_INT_EN,
	.detach_en = SC27XX_DETACH_INT_EN,
	.state_mask = SC27XX_STATE_MASK,
	.event_mask = SC27XX_EVENT_MASK,
};

static const struct sprd_typec_variant_data sc2721_data = {
	.efuse_cc1_shift = SC2721_EFUSE_CC1_SHIFT,
	.efuse_cc2_shift = SC2721_EFUSE_CC2_SHIFT,
	.int_en = SC2721_EN,
	.int_clr = SC2721_CLR,
	.mode = SC2721_MODE,
	.attach_en = SC2721_ATTACH_INT_EN,
	.detach_en = SC2721_DETACH_INT_EN,
	.state_mask = SC2721_STATE_MASK,
	.event_mask = SC2721_EVENT_MASK,
};

static const struct sprd_typec_variant_data ump9620_data = {
	.efuse_cc1_shift = UMP9620_EFUSE_CC1_SHIFT,
	.efuse_cc2_shift = UMP9620_EFUSE_CC2_SHIFT,
	.int_en = SC27XX_INT_EN,
	.int_clr = SC27XX_INT_CLR,
	.mode = SC27XX_MODE,
	.attach_en = SC27XX_ATTACH_INT_EN,
	.detach_en = SC27XX_DETACH_INT_EN,
	.state_mask = SC27XX_STATE_MASK,
	.event_mask = SC27XX_EVENT_MASK,
};

struct sc27xx_typec {
	struct device *dev;
	struct regmap *regmap;
	u32 base;
	int irq;
	struct extcon_dev *edev;
	struct gpio_desc *gpiod;
	bool usb20_only;

	enum sc27xx_typec_connection_state state;
	enum sc27xx_typec_connection_state pre_state;
	struct typec_port *port;
	struct typec_partner *partner;
	struct typec_capability typec_cap;
	const struct sprd_typec_variant_data *var_data;
	struct iio_channel	*cc1;
	struct iio_channel	*cc2;
	struct delayed_work		irq_enable_work;
};

bool sc27xx_typec_cc1_cc2_voltage_detect(struct sc27xx_typec *sc)
{
	bool flag = false;
	int cc1_voltage, cc2_voltage;
	int cnt = 20;

	if (IS_ERR_OR_NULL(sc->cc1) || IS_ERR_OR_NULL(sc->cc2))
		return flag;

	regmap_update_bits(sc->regmap,
		sc->base + SC27XX_MODE,
		BIT_TYPEC_CC_V2AD_EN,
		BIT_TYPEC_CC_V2AD_EN);

	msleep(300);

	do {
		iio_read_channel_processed(sc->cc1, &cc1_voltage);
		iio_read_channel_processed(sc->cc2, &cc2_voltage);
		cc1_voltage *= 2;
		cc2_voltage *= 2;
		if (cc1_voltage >= CC_VOLT_LIMIT ||
			cc2_voltage >= CC_VOLT_LIMIT) {
			flag = true;
			pr_info("[CCS] true cc1_voltage = %d cc2_voltage = %d\n",cc1_voltage,cc2_voltage);
			break;
		}
			cnt--;
	} while (cnt > 0);

	regmap_update_bits(sc->regmap,
		sc->base + SC27XX_MODE,
		BIT_TYPEC_CC_V2AD_EN, 0);
	/*pr_info("[CCS] test cc1_voltage = %d cc2_voltage = %d\n",cc1_voltage,cc2_voltage);*/
	return flag;
}
EXPORT_SYMBOL_GPL(sc27xx_typec_cc1_cc2_voltage_detect);

int force_set_typec_mode(struct sc27xx_typec *sc, const char *str)
{
	int val, ret;

	ret = regmap_read(sc->regmap, sc->base + SC27XX_MODE, &val);
	dev_dbg(sc->dev, "before write, SC27XX_MODE = 0x%x\n", val);
	if (ret)
		return ret;

	val &= ~SC27XX_MODE_MASK;
	if (strcmp(str, "OTG_ON"))
		val |= SC27XX_MODE_DRP;
	else if (strcmp(str, "OTG_OFF"))
		val |= SC27XX_MODE_SNK;
	else
		return 0;

	ret = regmap_write(sc->regmap, sc->base + SC27XX_MODE, val);
	if (ret)
		return ret;

	regmap_read(sc->regmap, sc->base + SC27XX_MODE, &val);
	dev_dbg(sc->dev, "after write  SC27XX_MODE = 0x%x\n", val);

	return 0;
}
EXPORT_SYMBOL_GPL(force_set_typec_mode);
volatile struct sc27xx_typec *psc = NULL;
int g_cc_polarity = 0;
EXPORT_SYMBOL_GPL(psc);
EXPORT_SYMBOL_GPL(g_cc_polarity);
static int sc27xx_typec_connect(struct sc27xx_typec *sc, u32 status)
{
	enum typec_data_role data_role = TYPEC_DEVICE;
	enum typec_role power_role = TYPEC_SOURCE;
	enum typec_role vconn_role = TYPEC_SOURCE;
	struct typec_partner_desc desc;
	enum typec_cc_polarity cc_polarity;
	u32 val, tmp1, tmp2;
	int ret;

	if (sc->partner)
		return 0;

	switch (sc->state) {
	case SC27XX_ATTACHED_SNK:
	case SC27XX_DEBUG_CABLE:
		power_role = TYPEC_SINK;
		data_role = TYPEC_DEVICE;
		vconn_role = TYPEC_SINK;
		break;
	case SC27XX_ATTACHED_SRC:
	case SC27XX_AUDIO_CABLE:
		power_role = TYPEC_SOURCE;
		data_role = TYPEC_HOST;
		vconn_role = TYPEC_SOURCE;
		break;
	default:
		break;
	}

	desc.usb_pd = 0;
	desc.accessory = TYPEC_ACCESSORY_NONE;
	desc.identity = NULL;

	sc->partner = typec_register_partner(sc->port, &desc);
	if (!sc->partner)
		return -ENODEV;

	typec_set_pwr_opmode(sc->port, TYPEC_PWR_MODE_USB);
	typec_set_pwr_role(sc->port, power_role);
	typec_set_data_role(sc->port, data_role);
	typec_set_vconn_role(sc->port, vconn_role);

	switch (sc->state) {
	case SC27XX_ATTACHED_SNK:
	case SC27XX_DEBUG_CABLE:
		sc->pre_state = SC27XX_ATTACHED_SNK;
		extcon_set_state_sync(sc->edev, EXTCON_USB, true);
		break;
	case SC27XX_ATTACHED_SRC:
		sc->pre_state = SC27XX_ATTACHED_SRC;
		if (!IS_ERR(sc->gpiod) && gpiod_get_value(sc->gpiod))
			return 0;
		extcon_set_state_sync(sc->edev, EXTCON_USB_HOST, true);
		break;
	case SC27XX_AUDIO_CABLE:
		sc->pre_state = SC27XX_AUDIO_CABLE;
		extcon_set_state_sync(sc->edev, EXTCON_JACK_HEADPHONE, true);
		break;
	default:
		break;
	}

	ret = regmap_read(sc->regmap, sc->base + SC2730_DBG1, &val);
	if (ret < 0) {
		dev_err(sc->dev, "failed to read DBG1 register.\n");
		return ret;
	}

	val &= SC2730_CC_INSERT;
	tmp1 = val & SC2730_CC1_DFP_CHECK;
	tmp2 = val & SC2730_CC2_DFP_CHECK;

	if (tmp1 || tmp2) {
		/* DFP MODE */
		if (tmp1)
			cc_polarity = TYPEC_POLARITY_CC1;
		else
			cc_polarity = TYPEC_POLARITY_CC2;
	} else {
		/* UFP MODE */
		if (val & SC2730_CC1_UFP_CON)
			cc_polarity = TYPEC_POLARITY_CC1;
		else
			cc_polarity = TYPEC_POLARITY_CC2;
	}

	if (!val || val == SC2730_CC_INSERT)
		g_cc_polarity = 0;

	switch (cc_polarity) {
	case TYPEC_POLARITY_CC1:
		g_cc_polarity = 1;
		break;
	case TYPEC_POLARITY_CC2:
		g_cc_polarity = 2;
		break;
	default:
		g_cc_polarity = 0;
		break;
	}
	return 0;
}

static void sc27xx_typec_disconnect(struct sc27xx_typec *sc, u32 status)
{
	typec_unregister_partner(sc->partner);
	sc->partner = NULL;
	typec_set_pwr_opmode(sc->port, TYPEC_PWR_MODE_USB);
	typec_set_pwr_role(sc->port, TYPEC_SINK);
	typec_set_data_role(sc->port, TYPEC_DEVICE);
	typec_set_vconn_role(sc->port, TYPEC_SINK);

	switch (sc->pre_state) {
	case SC27XX_ATTACHED_SNK:
	case SC27XX_DEBUG_CABLE:
		extcon_set_state_sync(sc->edev, EXTCON_USB, false);
		break;
	case SC27XX_ATTACHED_SRC:
		extcon_set_state_sync(sc->edev, EXTCON_USB_HOST, false);
		break;
	case SC27XX_AUDIO_CABLE:
		extcon_set_state_sync(sc->edev, EXTCON_JACK_HEADPHONE, false);
		break;
	default:
		break;
	}
	g_cc_polarity = 0;
}

#if 0
static int sc27xx_typec_dr_set(const struct typec_capability *cap,
				enum typec_data_role role)
{
	/* TODO: Data role set */
	return 0;
}

static int sc27xx_typec_pr_set(const struct typec_capability *cap,
				enum typec_role role)
{
	/* TODO: Power role set */
	return 0;
}

static int sc27xx_typec_vconn_set(const struct typec_capability *cap,
				enum typec_role role)
{
	/* TODO: Vconn set */
	return 0;
}
#endif

static irqreturn_t sc27xx_typec_interrupt(int irq, void *data)
{
	struct sc27xx_typec *sc = data;
	u32 event;
	int ret;

	ret = regmap_read(sc->regmap, sc->base + SC27XX_INT_MASK, &event);
	if (ret)
		return ret;

	event &= sc->var_data->event_mask;

	ret = regmap_read(sc->regmap, sc->base + SC27XX_STATUS, &sc->state);
	if (ret)
		goto clear_ints;

	sc->state &= sc->var_data->state_mask;

	if (event & SC27XX_ATTACH_INT) {
		ret = sc27xx_typec_connect(sc, sc->state);
		if (ret)
			dev_warn(sc->dev, "failed to register partner\n");
	} else if (event & SC27XX_DETACH_INT) {
		sc27xx_typec_disconnect(sc, sc->state);
	}

clear_ints:
	regmap_write(sc->regmap, sc->base + sc->var_data->int_clr, event);

	dev_info(sc->dev, "now works as DRP and is in %d state, event %d\n",
		sc->state, event);
	return IRQ_HANDLED;
}

static int sc27xx_typec_enable(struct sc27xx_typec *sc)
{
	int ret;
	u32 val;

	/* Set typec mode */
	ret = regmap_read(sc->regmap, sc->base + sc->var_data->mode, &val);
	if (ret)
		return ret;

	val &= ~SC27XX_MODE_MASK;
	switch (sc->typec_cap.type) {
	case TYPEC_PORT_DFP:
		val |= SC27XX_MODE_SRC;
		break;
	case TYPEC_PORT_UFP:
		val |= SC27XX_MODE_SNK;
		break;
	case TYPEC_PORT_DRP:
		val |= SC27XX_MODE_DRP;
		break;
	default:
		return -EINVAL;
	}

	ret = regmap_write(sc->regmap, sc->base + sc->var_data->mode, val);
	if (ret)
		return ret;

	/* typec USB20 only flag, only work in snk mode */
	if (sc->typec_cap.data == TYPEC_PORT_UFP && sc->usb20_only) {
		ret = regmap_update_bits(sc->regmap, sc->base + SC27XX_EN,
					 SC27XX_TYPEC_USB20_ONLY,
					 SC27XX_TYPEC_USB20_ONLY);
		if (ret)
			return ret;
	}

	/* modify sc2730 tcc debounce to 100ms while PD signal occur at 150ms
	 * and effect tccde reginize.Reason is hardware signal and clk not
	 * accurate.
	 */
	if (sc->var_data->efuse_cc2_shift == SC2730_EFUSE_CC2_SHIFT) {
		ret = regmap_write(sc->regmap, sc->base + SC27XX_TCCDE_CNT,
				SC27XX_TCC_DEBOUNCE_CNT);
		if (ret)
			return ret;
	}

	/* Enable typec interrupt and enable typec */
	ret = regmap_read(sc->regmap, sc->base + sc->var_data->int_en, &val);
	if (ret)
		return ret;

	val |= sc->var_data->attach_en | sc->var_data->detach_en;
	return regmap_write(sc->regmap, sc->base + sc->var_data->int_en, val);
}

int sc27xx_typec_set_sink(struct sc27xx_typec *sc){
	int ret;
	u32 val;

	ret = regmap_read(sc->regmap, sc->base + sc->var_data->mode, &val);
	if (ret)
		return ret;

	val &= ~SC27XX_MODE_MASK;
	val |= SC27XX_MODE_SNK;

	ret = regmap_write(sc->regmap, sc->base + sc->var_data->mode, val);
	if (ret){
		printk("sc27xx typec set sink fail\n");
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sc27xx_typec_set_sink);

int sc27xx_typec_set_enable(struct sc27xx_typec *sc){
	int ret;

	ret = sc27xx_typec_enable(sc);
	if (ret){
		printk("sc27xx typec enable fail\n");
		return ret;
		}

	return 0;
}

EXPORT_SYMBOL_GPL(sc27xx_typec_set_enable);

static const u32 sc27xx_typec_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_JACK_HEADPHONE,
	EXTCON_NONE,
};

static int sc27xx_typec_get_cc1_efuse(struct sc27xx_typec *sc)
{
	struct nvmem_cell *cell;
	u32 calib_data = 0;
	void *buf;
	size_t len;

	cell = nvmem_cell_get(sc->dev, "typec_cc1_cal");
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	memcpy(&calib_data, buf, min(len, sizeof(u32)));
	calib_data = (calib_data & SC27XX_CC1_MASK(sc->var_data->efuse_cc1_shift))
			>> SC27XX_CC_SHIFT(sc->var_data->efuse_cc1_shift);
	kfree(buf);

	return calib_data;
}

static int sc27xx_typec_get_cc2_efuse(struct sc27xx_typec *sc)
{
	struct nvmem_cell *cell;
	u32 calib_data = 0;
	void *buf;
	size_t len = 0;

	cell = nvmem_cell_get(sc->dev, "typec_cc2_cal");
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	memcpy(&calib_data, buf, min(len, sizeof(u32)));
	calib_data = (calib_data & SC27XX_CC2_MASK(sc->var_data->efuse_cc2_shift))
			>> SC27XX_CC_SHIFT(sc->var_data->efuse_cc2_shift);
	kfree(buf);

	return calib_data;
}

static int typec_set_rtrim(struct sc27xx_typec *sc)
{
	int calib_cc1;
	int calib_cc2;
	u32 rtrim;

	calib_cc1 = sc27xx_typec_get_cc1_efuse(sc);
	if (calib_cc1 < 0)
		return calib_cc1;
	calib_cc2 = sc27xx_typec_get_cc2_efuse(sc);
	if (calib_cc2 < 0)
		return calib_cc2;

	rtrim = calib_cc1 | calib_cc2<<5;

	return regmap_write(sc->regmap, sc->base + SC27XX_RTRIM, rtrim);
}

static void typec_irq_enable_work(struct work_struct *work)
{
	struct sc27xx_typec *sc = container_of(work,
				 struct sc27xx_typec, irq_enable_work.work);
	int ret;

	pr_err("typec_irq_enable_work\n");

	ret = devm_request_threaded_irq(sc->dev, sc->irq, NULL,
					sc27xx_typec_interrupt,
					IRQF_EARLY_RESUME | IRQF_ONESHOT,
					dev_name(sc->dev), sc);
	if (ret) {
		dev_err(sc->dev, "failed to request irq %d\n", ret);
		goto error;
	}

	ret = sc27xx_typec_enable(sc);
	if (ret)
		goto error;

	return;

	error:
	typec_unregister_port(sc->port);
}

static int sc27xx_typec_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = pdev->dev.of_node;
	struct sc27xx_typec *sc;
	const struct sprd_typec_variant_data *pdata;
	int mode, ret, gpio_num;

	pdata = of_device_get_match_data(dev);
	if (!pdata) {
		dev_err(&pdev->dev, "No matching driver data found\n");
		return -EINVAL;
	}

	sc = devm_kzalloc(&pdev->dev, sizeof(*sc), GFP_KERNEL);
	if (!sc)
		return -ENOMEM;

	sc->edev = devm_extcon_dev_allocate(&pdev->dev, sc27xx_typec_cable);
	if (IS_ERR(sc->edev)) {
		dev_err(&pdev->dev, "failed to allocate extcon device\n");
		return PTR_ERR(sc->edev);
	}

	ret = devm_extcon_dev_register(&pdev->dev, sc->edev);
	if (ret < 0) {
		dev_err(&pdev->dev, "can't register extcon device: %d\n", ret);
		return ret;
	}

	sc->dev = &pdev->dev;
	sc->irq = platform_get_irq(pdev, 0);
	if (sc->irq < 0) {
		dev_err(sc->dev, "failed to get typec interrupt.\n");
		return sc->irq;
	}

	sc->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!sc->regmap) {
		dev_err(sc->dev, "failed to get regmap.\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(node, "reg", &sc->base);
	if (ret) {
		dev_err(dev, "failed to get reg offset!\n");
		return ret;
	}

	ret = of_property_read_u32(node, "sprd,mode", &mode);
	if (ret) {
		dev_err(dev, "failed to get typec port mode type\n");
		return ret;
	}

	if (mode < TYPEC_PORT_DFP || mode > TYPEC_PORT_DRP
	    || mode == TYPEC_PORT_UFP) {
		mode = TYPEC_PORT_UFP;
		sc->usb20_only = true;
		dev_info(dev, "usb 2.0 only is enabled\n");
	}

	node = of_find_compatible_node(NULL, NULL, "linux,extcon-usb-gpio");
	if (!node) {
		dev_warn(dev, "failed to find vbus gpio node.\n");
	} else {
		gpio_num = of_get_named_gpio(node, "vbus-gpio", 0);
		of_node_put(node);
		if (gpio_is_valid(gpio_num)) {
			sc->gpiod = gpio_to_desc(gpio_num);
			if (IS_ERR(sc->gpiod))
				dev_warn(dev, "failed to get vbus gpio.\n");
		} else {
			dev_warn(dev, "failed to get valid vbus gpio.\n");
		}
	}

	sc->var_data = pdata;
	sc->typec_cap.type = mode;
	sc->typec_cap.data = TYPEC_PORT_DRD;
	//sc->typec_cap.dr_set = sc27xx_typec_dr_set;
	//sc->typec_cap.pr_set = sc27xx_typec_pr_set;
	//sc->typec_cap.vconn_set = sc27xx_typec_vconn_set;
	sc->typec_cap.revision = USB_TYPEC_REV_1_2;
	sc->typec_cap.prefer_role = TYPEC_NO_PREFERRED_ROLE;
	sc->port = typec_register_port(&pdev->dev, &sc->typec_cap);
	if (!sc->port) {
		dev_err(sc->dev, "failed to register port!\n");
		return -ENODEV;
	}

	ret = typec_set_rtrim(sc);
	if (ret < 0) {
		dev_err(sc->dev, "failed to set typec rtrim %d\n", ret);
		goto error;
	}

	if (of_property_read_bool(pdev->dev.of_node,"cc-to-vbus")) {
		sc->cc1 = devm_iio_channel_get(dev, "cc1");
		if (IS_ERR_OR_NULL(sc->cc1)) {
			ret = PTR_ERR(sc->cc1);
			dev_err(dev, "CC1 channel not found %d\n",ret);
			return ret;
		}

		sc->cc2 = devm_iio_channel_get(dev, "cc2");
		if (IS_ERR_OR_NULL(sc->cc2)) {
			ret = PTR_ERR(sc->cc2);
			dev_err(dev, "CC2 channel not found %d\n",ret);
			return ret;
		}
	}

	psc = sc;
#if 0
	ret = devm_request_threaded_irq(sc->dev, sc->irq, NULL,
					sc27xx_typec_interrupt,
					IRQF_EARLY_RESUME | IRQF_ONESHOT,
					dev_name(sc->dev), sc);
	if (ret) {
		dev_err(sc->dev, "failed to request irq %d\n", ret);
		goto error;
	}

	ret = sc27xx_typec_enable(sc);
	if (ret)
		goto error;
#endif
	INIT_DELAYED_WORK(&sc->irq_enable_work, typec_irq_enable_work);

	platform_set_drvdata(pdev, sc);
	schedule_delayed_work(&sc->irq_enable_work,
				(HZ * 1));
	return 0;

error:
	typec_unregister_port(sc->port);
	return ret;
}

static int sc27xx_typec_remove(struct platform_device *pdev)
{
	struct sc27xx_typec *sc = platform_get_drvdata(pdev);

	sc27xx_typec_disconnect(sc, 0);
	typec_unregister_port(sc->port);

	return 0;
}

static const struct of_device_id typec_sprd_match[] = {
	{.compatible = "sprd,sc2730-typec", .data = &sc2730_data},
	{.compatible = "sprd,sc2721-typec", .data = &sc2721_data},
	{.compatible = "sprd,ump96xx-typec", .data = &ump9620_data},
	{},
};
MODULE_DEVICE_TABLE(of, typec_sprd_match);

static struct platform_driver sc27xx_typec_driver = {
	.probe = sc27xx_typec_probe,
	.remove = sc27xx_typec_remove,
	.driver = {
		.name = "sc27xx-typec",
		.of_match_table = typec_sprd_match,
	},
};
module_platform_driver(sc27xx_typec_driver);
MODULE_LICENSE("GPL v2");

