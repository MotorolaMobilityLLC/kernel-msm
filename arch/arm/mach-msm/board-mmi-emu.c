/* Copyright (c) 2012, Motorola Mobility, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <asm/mach-types.h>
#include <asm/mach/map.h>

#include <linux/clk.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/mfd/pm8xxx/pm8921.h>

#include <mach/board.h>
#include <mach/dma.h>
#include <mach/devtree_util.h>
#include <mach/mmi_emu_det.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>

#include "board-8960.h"


/* cache_erp.c */
extern void enable_msm_l2_erp_irq(void);
extern void disable_msm_l2_erp_irq(void);

/* clock-8960.c */
extern int msm_gsbi12_uart_clk_ptr(struct clk **ptr);
extern int msm_gsbi4_uart_clk_ptr(struct clk **ptr);
extern int msm_clocks_8960_v1_info(struct clk_lookup **ptr, int *num_lookups);


#define MSM_UART_NAME       "msm_serial_hs"
#define MSM_I2C_NAME            "qup_i2c"

#define MSM_GSBI12_PHYS     0x12480000
#define MSM_GSBI4_PHYS      0x16300000

#define GSBI_PROTOCOL_UNDEFINED 0x70
#define GSBI_PROTOCOL_I2C_UART  0x60


#define MSM_UART12DM_PHYS   (MSM_GSBI12_PHYS + 0x10000)
#define MSM_UART4DM_PHYS    (MSM_GSBI4_PHYS + 0x40000)

/*
  MUX switch GPIOs in case EMU detection is disabled
*/
#define GPIO_MUX_CTRL0	107
#define GPIO_MUX_CTRL1	96

#define IC_EMU_POWER		0xA0
#define IC_HONEY_BADGER		0xA1

#define UART_GSBI12		12

#define GPIO_IS_MSM		1
#define GPIO_IS_PMIC		2
#define GPIO_IS_PMIC_MPP	3

#define GPIO_STD_CFG		0
#define GPIO_ALT_CFG		1

#define EMU_DET_RESOURCE_MAX	EMU_DET_GPIO_MAX


static u8 uart_over_gsbi12;
static bool core_power_init, enable_5v_init;
static struct clk		*iface_clock;
static struct platform_device	emu_det_device;
static struct resource		mmi_emu_det_resources[EMU_DET_RESOURCE_MAX];


struct mmi_emu_det_gpio_data {
	int gpio;
	int owner;
	int num_cfg;
	void *gpio_configs;
};

struct emu_det_dt_data {
	int ic_type;
	int uart_gsbi;
	int accy_support;
	char *vdd_vreg;
	char *usb_otg_vreg;
	unsigned int vdd_voltage;
};

#define DECLARE(_id, _opt, _cfgs)	(#_id)
static const char *mmi_emu_det_io_map[EMU_DET_GPIO_MAX] = EMU_DET_IOs;
#undef DECLARE

static struct mmi_emu_det_gpio_data	mmi_emu_det_gpio[EMU_DET_GPIO_MAX] = {
	{
		.gpio = GPIO_MUX_CTRL0,
	},
	{
		.gpio = GPIO_MUX_CTRL1,
	},
};



static __init int mmi_emu_det_index_by_name(const char *name)
{
	int ret = -1, idx;
	for (idx = 0; idx < EMU_DET_GPIO_MAX; idx++)
		if (!strncmp(mmi_emu_det_io_map[idx],
			name, strlen(mmi_emu_det_io_map[idx]))) {
			ret = idx;
			break;
		}

	return ret;
}


static void configure_l2_error(int enable)
{
	if (enable)
		enable_msm_l2_erp_irq();
	else
		disable_msm_l2_erp_irq();
}

static __init void mot_set_gsbi_clk(const char *con_id,
		struct clk *clk_entity, const char *dev_id)
{
	int num_lookups;
	struct clk_lookup *lk;
	if (!msm_clocks_8960_v1_info(&lk, &num_lookups)) {
		int i;
		for (i = 0; i < num_lookups; i++) {
			if (!strncmp((lk+i)->con_id, con_id,
				strlen((lk+i)->con_id)) &&
				((lk+i)->clk == clk_entity)) {
				(lk+i)->dev_id = dev_id;
				break;
			}
		}
	}
}


static __init void mot_setup_whisper_clk(void)
{
	struct clk *clk;

	if (uart_over_gsbi12) {
		if (!msm_gsbi12_uart_clk_ptr(&clk))
			mot_set_gsbi_clk("core_clk", clk,
					MSM_UART_NAME ".1");
	} else {
		if (!msm_gsbi4_uart_clk_ptr(&clk))
			mot_set_gsbi_clk("core_clk", clk,
					MSM_UART_NAME ".1");
	}
}


static void emu_mux_ctrl_config_pin(int io_num, int value)
{
	int rc, gpio;
	const char *name;
	struct resource *res;

	if (!(io_num >= 0 && io_num < EMU_DET_GPIO_MAX)) {
		pr_err("EMU invalid gpio index %d\n", io_num);
		return;
	}

	res = platform_get_resource_byname(&emu_det_device,
			IORESOURCE_IO, mmi_emu_det_io_map[io_num]);
	if (!res) {
		gpio = mmi_emu_det_gpio[io_num].gpio;
		name = mmi_emu_det_io_map[io_num];
	} else {
		gpio = res->start;
		name = res->name;
	}
	rc = gpio_request(gpio, name);
	if (rc) {
		pr_err("Could not request %s for GPIO %d" \
			"(should only reach here if factory shutdown)\n",
			name, gpio);
	}
	rc = gpio_direction_output(gpio, value);
	if (rc) {
		pr_err("Could not set %s for GPIO %d\n", name, gpio);
		gpio_free(gpio);
		return;
	}
}

static __init int dt_read_msm_gpio_config(struct device_node *parent,
			const char *property, int property_len,
			struct gpiomux_setting *msm_gpio)
{
	int ret = -1, len = 0;
	const void *config_prop;

	config_prop = of_get_property(parent, property, &len);

	if (config_prop && (len == property_len)) {
		u8 *config = (u8 *)config_prop;
			msm_gpio->func = config[0];
			msm_gpio->drv  = config[1];
			msm_gpio->pull = config[2];
			msm_gpio->dir  = config[3];
			ret = 0;
		}
		return ret;
}

static __init int dt_read_pmic_gpio_config(struct device_node *parent,
		const char *property, int property_len,
		struct pm_gpio *pm_gpio)
{

	int ret = -1, len = 0;
	const void *config_prop;

	config_prop = of_get_property(parent, property, &len);
	if (config_prop) {
		u8 *config = (u8 *)config_prop;
			pm_gpio->direction      = config[0];
			pm_gpio->output_buffer  = config[1];
			pm_gpio->output_value   = config[2];
			pm_gpio->pull           = config[3];
			pm_gpio->vin_sel        = config[4];
			pm_gpio->out_strength   = config[5];
			pm_gpio->function       = config[6];
			pm_gpio->inv_int_pol    = config[7];
			pm_gpio->disable_pin    = config[8];

			ret = 0;
		}
		return ret;
}

static __init int dt_read_mpp_config(struct device_node *parent,
			const char *property, int property_len,
			struct pm8xxx_mpp_config_data *pmic_mpp)
{
	int ret = -1, len = 0;
	const void *config_prop;

	config_prop = of_get_property(parent, property, &len);

	if (config_prop) {
		u8 *config = (u8 *)config_prop;
		pmic_mpp->type    = config[0];
		pmic_mpp->level   = config[1];
		pmic_mpp->control = config[2];

		ret = 0;
	}
	return ret;
}



static struct emu_det_dt_data	emu_det_dt_data = {
	.ic_type	= IC_EMU_POWER,
	.uart_gsbi	= UART_GSBI12,
	.vdd_vreg	= "EMU_POWER",
	.usb_otg_vreg	= "8921_usb_otg",
	.vdd_voltage	= 2650000,
};


static int emu_det_enable_5v(int on)
{
	int rc = 0;
	static struct regulator *emu_ext_5v;

	if (!enable_5v_init) {
		emu_ext_5v = regulator_get(&emu_det_device.dev,
				emu_det_dt_data.usb_otg_vreg);
		if (IS_ERR(emu_ext_5v)) {
			pr_err("unable to get %s reg\n",
				emu_det_dt_data.usb_otg_vreg);
			return PTR_ERR(emu_ext_5v);
		}
		enable_5v_init = true;
	}

	if (on) {
		rc = regulator_enable(emu_ext_5v);
		if (rc)
			pr_err("failed to enable %s reg\n",
				emu_det_dt_data.usb_otg_vreg);
	} else {
		rc = regulator_disable(emu_ext_5v);
		if (rc)
			pr_err("failed to disable %s reg\n",
				emu_det_dt_data.usb_otg_vreg);
	}
	return rc;
}

static void emu_det_hb_powerup(void)
{
	static bool init_done;
	if (!init_done) {
		if (emu_det_dt_data.ic_type == IC_HONEY_BADGER) {
			struct pm8xxx_adc_chan_result res;
			memset(&res, 0, sizeof(res));
			if (!pm8xxx_adc_read(CHANNEL_USBIN, &res) &&
				res.physical < 4000000) {
				pr_info("Powering up HoneyBadger\n");
				emu_det_enable_5v(1);
				msleep(50);
				emu_det_enable_5v(0);
			}
		}
		init_done = true;
	}
}

static int emu_det_core_power(int on)
{
	int rc = 0;
	static struct regulator *emu_vdd;

	if (!core_power_init) {
		emu_vdd = regulator_get(&emu_det_device.dev,
				emu_det_dt_data.vdd_vreg);
		if (IS_ERR(emu_vdd)) {
			pr_err("unable to get EMU_POWER reg\n");
			return PTR_ERR(emu_vdd);
		}
		rc = regulator_set_voltage(emu_vdd,
				emu_det_dt_data.vdd_voltage,
				emu_det_dt_data.vdd_voltage);
		if (rc) {
			pr_err("unable to set voltage %s reg\n",
				emu_det_dt_data.vdd_vreg);
			goto put_vdd;
		}
		core_power_init = true;
	}

	if (on) {
		rc = regulator_enable(emu_vdd);
		if (rc)
			pr_err("failed to enable %s reg\n",
				emu_det_dt_data.vdd_vreg);
		else
			emu_det_hb_powerup();
	} else {
		rc = regulator_disable(emu_vdd);
		if (rc)
			pr_err("failed to disable %s reg\n",
				emu_det_dt_data.vdd_vreg);
	}
	return rc;

put_vdd:
	if (emu_vdd)
		regulator_put(emu_vdd);
	emu_vdd = NULL;
	return rc;
}

static int emu_det_configure_gpio(
		struct mmi_emu_det_gpio_data *gpio_data, int alt)
{
	int ret = -1;

	if (gpio_data->gpio_configs && gpio_data->num_cfg) {
		struct pm8xxx_mpp_config_data *mpp;
		struct msm_gpiomux_config gmux;
		struct gpiomux_setting *ms;
		struct pm_gpio *pm;

		switch (gpio_data->owner) {
		case GPIO_IS_MSM:
			memset(&gmux, 0, sizeof(gmux));
			ms = gpio_data->gpio_configs;
			if (alt)
				ms++;
			gmux.gpio = gpio_data->gpio;
			gmux.settings[GPIOMUX_ACTIVE] = ms;
			gmux.settings[GPIOMUX_SUSPENDED] = ms;
			msm_gpiomux_install(&gmux, 1);
			ret = 0;
				break;
		case GPIO_IS_PMIC:
			pm = (struct pm_gpio *)gpio_data->gpio_configs;
			if (alt)
				pm++;
			ret = pm8xxx_gpio_config(gpio_data->gpio, pm);
				break;
		case GPIO_IS_PMIC_MPP:
			mpp = gpio_data->gpio_configs;
			if (alt)
				mpp++;
			ret = pm8xxx_mpp_config(gpio_data->gpio, mpp);
				break;
		}
	} else
		pr_err("EMU missing gpio %d data\n", gpio_data->gpio);

	return ret;
}


static __init void emu_det_gpio_init(void)
{
	int i;

	for (i = 0; i <  EMU_DET_GPIO_MAX; i++) {
		struct mmi_emu_det_gpio_data *gpio_data =
						&mmi_emu_det_gpio[i];

		if (gpio_data->gpio)
			emu_det_configure_gpio(gpio_data, GPIO_MODE_STANDARD);
	}
}


static void emu_det_dp_dm_mode(int mode)
{
	int alt = !!mode;
	struct pm_gpio rx_gpio;
	struct pm_gpio *cfg_rx_gpio = (struct pm_gpio *)
			mmi_emu_det_gpio[DPLUS_GPIO].gpio_configs;
	if (alt)
		cfg_rx_gpio++;

	rx_gpio = *cfg_rx_gpio;

	if (mode == GPIO_MODE_ALTERNATE_2)
		rx_gpio.pull = PM_GPIO_PULL_DN;

	emu_det_configure_gpio(&mmi_emu_det_gpio[RX_PAIR_GPIO], alt);
	pm8xxx_gpio_config(mmi_emu_det_gpio[DPLUS_GPIO].gpio, &rx_gpio);

	emu_det_configure_gpio(&mmi_emu_det_gpio[TX_PAIR_GPIO], alt);
	emu_det_configure_gpio(&mmi_emu_det_gpio[DMINUS_GPIO],  alt);
}

static int emu_det_id_protect(int on)
{
	int ret = 0;
	int alt = !on;
	struct mmi_emu_det_gpio_data *gpio_data =
				&mmi_emu_det_gpio[EMU_ID_EN_GPIO];
	if (gpio_data->gpio)
		ret = emu_det_configure_gpio(gpio_data, alt);
	return ret;
}

static int emu_det_alt_mode(int on)
{
	int ret = 0;
	struct mmi_emu_det_gpio_data *gpio_data =
				&mmi_emu_det_gpio[SEMU_ALT_MODE_EN_GPIO];
	if (gpio_data->gpio)
		ret = emu_det_configure_gpio(gpio_data, on);
	return ret;
}

static void emu_det_gpio_mode(int mode)
{
	struct mmi_emu_det_gpio_data *tx_gpio_data =
				&mmi_emu_det_gpio[WHISPER_UART_TX_GPIO];
	struct mmi_emu_det_gpio_data *rx_gpio_data =
				&mmi_emu_det_gpio[WHISPER_UART_RX_GPIO];

	if (tx_gpio_data->gpio && rx_gpio_data->gpio) {
		emu_det_configure_gpio(tx_gpio_data, mode);
		emu_det_configure_gpio(rx_gpio_data, mode);
	}
}

static int emu_det_adc_id(void)
{
	struct pm8xxx_adc_chan_result res;
	memset(&res, 0, sizeof(res));
	if (pm8xxx_adc_read(CHANNEL_MPP_2, &res))
		pr_err("CHANNEL_MPP_2 ADC read error\n");

	return (int)res.physical/1000;
}


static void configure_gsbi_ctrl(int restore)
{
	void *gsbi_ctrl;
	uint32_t new;
	static uint32_t value;
	static bool stored;
	unsigned gsbi_phys = MSM_GSBI12_PHYS;

	if (!uart_over_gsbi12)
		gsbi_phys = MSM_GSBI4_PHYS;
	if (iface_clock)
		clk_prepare_enable(iface_clock);

	gsbi_ctrl = ioremap_nocache(gsbi_phys, 4);
	if (IS_ERR_OR_NULL(gsbi_ctrl)) {
		pr_err("cannot map GSBI ctrl reg\n");

		return;
	} else {
		if (restore) {
			if (stored) {
				writel_relaxed(value, gsbi_ctrl);
				stored = false;

				pr_info("GSBI reg 0x%x restored\n", value);
			}
		} else {
			new = value = readl_relaxed(gsbi_ctrl);
			stored = true;
			new &= ~GSBI_PROTOCOL_UNDEFINED;
			new |= GSBI_PROTOCOL_I2C_UART;
			writel_relaxed(new, gsbi_ctrl);

			mb();

			pr_info("GSBI reg 0x%x updated, " \
				"stored value 0x%x\n", new, value);
		}
	}

	if (!uart_over_gsbi12 && iface_clock)
		clk_disable_unprepare(iface_clock);

	iounmap(gsbi_ctrl);
}


static struct resource resources_uart_gsbi[] = {
	{
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name   = "uartdm_resource",
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "uartdm_channels",
		.flags  = IORESOURCE_DMA,
	},
	{
		.name   = "uartdm_crci",
		.flags  = IORESOURCE_DMA,
	},
};

static u64 msm_uart_dmov_dma_mask = DMA_BIT_MASK(32);
	struct platform_device msm8960_device_uart_gsbi = {
	.name   = MSM_UART_NAME,
	.id = 1,
	.num_resources  = ARRAY_SIZE(resources_uart_gsbi),
	.resource   = resources_uart_gsbi,
	.dev = {
		.dma_mask       = &msm_uart_dmov_dma_mask,
		.coherent_dma_mask  = DMA_BIT_MASK(32),
	},
};


static __init void mot_setup_whisper_uart(void)
{
	struct resource *res;

	res = platform_get_resource(&msm8960_device_uart_gsbi,
			IORESOURCE_IRQ, 0);
	res->start = res->end = uart_over_gsbi12 ?
			GSBI12_UARTDM_IRQ :
			GSBI4_UARTDM_IRQ;

	res = platform_get_resource(&msm8960_device_uart_gsbi,
			IORESOURCE_MEM, 0);
	res->start = uart_over_gsbi12 ?
			MSM_UART12DM_PHYS :
			MSM_UART4DM_PHYS;
	res->end = res->start + PAGE_SIZE - 1;

	res = platform_get_resource(&msm8960_device_uart_gsbi,
			IORESOURCE_DMA, 0);
	res->start = DMOV_WHISPER_TX_CHAN;
	res->end   = DMOV_WHISPER_RX_CHAN;

	res = platform_get_resource(&msm8960_device_uart_gsbi,
			IORESOURCE_DMA, 1);
	res->start = uart_over_gsbi12 ?
			DMOV_WHISPER_TX_CRCI_GSBI12 :
			DMOV_WHISPER_TX_CRCI_GSBI4;
	res->end = uart_over_gsbi12 ?
			DMOV_WHISPER_RX_CRCI_GSBI12 :
			DMOV_WHISPER_RX_CRCI_GSBI4;
}



static struct mmi_emu_det_platform_data mmi_emu_det_data = {
	.enable_5v = emu_det_enable_5v,
	.core_power = emu_det_core_power,
	.id_protect = emu_det_id_protect,
	.alt_mode = emu_det_alt_mode,
	.gpio_mode = emu_det_gpio_mode,
	.adc_id = emu_det_adc_id,
	.dp_dm_mode = emu_det_dp_dm_mode,
	.gsbi_ctrl = configure_gsbi_ctrl,
	.cfg_l2_err = configure_l2_error,
};

static struct platform_device emu_det_device = {
	.name		= "emu_det",
	.id		= -1,
	.num_resources	= EMU_DET_RESOURCE_MAX,
	.resource	= mmi_emu_det_resources,
	.dev.platform_data = &mmi_emu_det_data,
};


static __init void config_EMU_detection_from_dt(void)
{
	struct device_node *parent, *child;
	int count = 0;
	u32 value = 0;
	struct resource *res;

	parent = of_find_node_by_path("/System@0/EMUDetection@0");
	if (!parent)
		goto out;

	if (!of_property_read_u32(parent, "ic-type", &value))
		emu_det_dt_data.ic_type = value | 0xA0;

	if (!of_property_read_u32(parent, "uart-gsbi", &value))
		emu_det_dt_data.uart_gsbi = value;

	if (!of_property_read_u32(parent, "accy-support", &value))
		emu_det_dt_data.accy_support = value;

	if (!of_property_read_u32(parent, "vdd-vreg-mv", &value))
		emu_det_dt_data.vdd_voltage = value;

	if (!of_property_read_string(parent, "vdd-vreg",
		 (const char **)&emu_det_dt_data.vdd_vreg))
		pr_err("%s: vdd-vreg not found\n", __func__);

	if (!of_property_read_string(parent, "usb-otg-vreg",
		 (const char **)&emu_det_dt_data.usb_otg_vreg))
		pr_err("%s: usb-otg-vreg not found\n", __func__);

	/* count the child GPIO nodes */
	for_each_child_of_node(parent, child) {
		if (!of_property_read_u32(child, "type", &value)) {
			if (value == 0x001E0010) {
				pr_err("%s: Found  GPIO entry#%d\n",
						__func__, count);
				count++;
			}
		}
	}

	if (count >= EMU_DET_RESOURCE_MAX) {
		pr_err("EMU resource: invalid number of resorces\n");
		goto out;
	}

	res = mmi_emu_det_resources;

	for_each_child_of_node(parent, child) {
		int gpio_idx = -1;
		size_t ds;
		struct mmi_emu_det_gpio_data *emud;
		struct gpiomux_setting msm_gpio[2];
		struct pm_gpio pmic_gpio[2];
		struct pm8xxx_mpp_config_data pmic_mpp[2];
		const char *name;
		int ret;

		/* Sample entry
		 * <property name="gpio-info" value="{GPIO_MSM, 107}" */
		if (!of_property_read_u32(child, "gpio-info", &value)) {
			u8 *ptr;
			u8 info[2];

			ptr =  (u8 *)&value;
			/* Work-Around: Correcting the endianess problem */
			info[0] = ptr[1];
			info[1] = ptr[0];

			if (info[0] == GPIO_IS_MSM) {
				res->start = res->end = info[1];
			} else if (info[0] == GPIO_IS_PMIC) {
				res->start = res->end =
					PM8921_GPIO_PM_TO_SYS(info[1]);
			} else if (info[0] == GPIO_IS_PMIC_MPP) {
				res->start = res->end =
					PM8921_MPP_PM_TO_SYS(info[1]);
			} else {
				pr_err("EMU unknown gpio %d owner %d; " \
					"skipping...\n", info[1], info[0]);
				continue;
			}

			ret = of_property_read_string(child, "pin-name", &name);
			if (!ret) {
				res->name = (const char *)name;
			} else {
				pr_err("EMU resource: unable to read name.." \
					"but continuing ret=%d\n", ret);
			}

			gpio_idx = mmi_emu_det_index_by_name(res->name);
			if (gpio_idx != -1)
				emud = &mmi_emu_det_gpio[gpio_idx];
			else {
				pr_err("EMU resource: unknown name: %s\n",
						res->name);
				goto out;
			}
			emud->gpio = res->start;
			emud->owner = info[0];

			switch (info[0]) {
			case GPIO_IS_MSM:
				ds = sizeof(struct gpiomux_setting);
				memset(msm_gpio, 0, sizeof(msm_gpio));
				if (!dt_read_msm_gpio_config(child,
					"msm-gpio-cfg-alt",
					sizeof(u8)*4, &msm_gpio[1])) {
					emud->num_cfg++;
				}

				if (!dt_read_msm_gpio_config(child,
					"msm-gpio-cfg",
					sizeof(u8)*4, &msm_gpio[0])) {

					emud->num_cfg++;
					emud->gpio_configs = kmemdup(msm_gpio,
						ds*emud->num_cfg, GFP_KERNEL);
				}

				break;
			case GPIO_IS_PMIC:
				ds = sizeof(struct pm_gpio);
				memset(pmic_gpio, 0, sizeof(pmic_gpio));
				if (!dt_read_pmic_gpio_config(child,
					"pmic-gpio-cfg-alt",
					sizeof(u8)*9, &pmic_gpio[1])) {

					emud->num_cfg++;
				}
				if (!dt_read_pmic_gpio_config(child,
					"pmic-gpio-cfg",
					sizeof(u8)*9, &pmic_gpio[0])) {

					emud->num_cfg++;
					emud->gpio_configs = kmemdup(pmic_gpio,
						ds*emud->num_cfg, GFP_KERNEL);

				}
				break;
			case GPIO_IS_PMIC_MPP:
				ds = sizeof(struct pm8xxx_mpp_config_data);
				memset(pmic_mpp, 0, sizeof(pmic_mpp));
				if (!dt_read_mpp_config(child,
					"pmic-mpp-cfg-alt",
					sizeof(u8)*3, &pmic_mpp[1])) {

					emud->num_cfg++;
				}

				if (!dt_read_mpp_config(child,
					"pmic-mpp-cfg",
					sizeof(u8)*3, &pmic_mpp[0])) {

					emud->num_cfg++;
					emud->gpio_configs = kmemdup(pmic_mpp,
						ds*emud->num_cfg, GFP_KERNEL);
				}
				break;
			}
			res->flags = IORESOURCE_IO;
			res++;
		}
	}

	printk(KERN_DEBUG "%s: EMU detection IC:%X, UART@GSBI%d, resources=%d"\
			", accy.support: %s\n",
			__func__,
			emu_det_dt_data.ic_type,
			emu_det_dt_data.uart_gsbi, count,
			emu_det_dt_data.accy_support ? "BASIC" : "FULL");

	mmi_emu_det_data.accy_support = emu_det_dt_data.accy_support;
	uart_over_gsbi12 = emu_det_dt_data.uart_gsbi == UART_GSBI12;

	mot_setup_whisper_clk();

	of_node_put(parent);

	return;
out:
	/* TODO: release allocated memory */
	/* disable EMU detection in case of error in device tree */
	pr_err("EMU error reading devtree; EMU disabled\n");

	return;
}



__init void mmi_init_emu_detection(struct msm_otg_platform_data *ctrl_data)
{
	/* EMU related entries are moved to OF-DT */
	config_EMU_detection_from_dt();

	if (ctrl_data) {
		ctrl_data->otg_control = OTG_ACCY_CONTROL;
		ctrl_data->pmic_id_irq = 0;
		ctrl_data->accy_pdev = &emu_det_device;
		iface_clock = clk_get_sys(uart_over_gsbi12 ?
			MSM_I2C_NAME ".12" : MSM_I2C_NAME ".4", "iface_clk");
		if (IS_ERR(iface_clock))
			pr_err("error getting GSBI iface clk\n");

		emu_det_gpio_init();

		mot_setup_whisper_uart();

	} else {
		/* If platform data is not set, safely drive the MUX
		 * CTRL pins to the USB configuration.
		 */
		emu_mux_ctrl_config_pin(EMU_MUX_CTRL0_GPIO, 1);
		emu_mux_ctrl_config_pin(EMU_MUX_CTRL1_GPIO, 0);
	}
}
