/*
 * platform_wm8994.c: wm8994 platform data initilization file
 *
 * (C) Copyright 2013 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/gpio.h>
#include <linux/lnw_gpio.h>
#include <asm/intel-mid.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/mfd/wm8994/pdata.h>
#include "platform_wm8994.h"

/***********WM89941 REGUATOR platform data*************/
static struct regulator_consumer_supply vwm89941_consumer[] = {
	REGULATOR_SUPPLY("DBVDD", "1-001a"),
	REGULATOR_SUPPLY("DBVDD1", "1-001a"),
	REGULATOR_SUPPLY("DBVDD2", "1-001a"),
	REGULATOR_SUPPLY("DBVDD3", "1-001a"),
	REGULATOR_SUPPLY("AVDD2", "1-001a"),
	REGULATOR_SUPPLY("CPVDD", "1-001a"),
};

static struct regulator_init_data vwm89941_data = {
		.constraints = {
			.always_on = 1,
		},
		.num_consumer_supplies	=	ARRAY_SIZE(vwm89941_consumer),
		.consumer_supplies	=	vwm89941_consumer,
};

static struct fixed_voltage_config vwm89941_config = {
	.supply_name	= "VCC_1.8V_PDA",
	.microvolts	= 1800000,
	.gpio		= -EINVAL,
	.init_data	= &vwm89941_data,
};

static struct platform_device vwm89941_device = {
	.name = "reg-fixed-voltage",
	.id = PLATFORM_DEVID_AUTO,
	.dev = {
		.platform_data = &vwm89941_config,
	},
};

/***********WM89942 REGUATOR platform data*************/
static struct regulator_consumer_supply vwm89942_consumer[] = {
	REGULATOR_SUPPLY("SPKVDD1", "1-001a"),
	REGULATOR_SUPPLY("SPKVDD2", "1-001a"),
};

static struct regulator_init_data vwm89942_data = {
		.constraints = {
			.always_on = 1,
		},
		.num_consumer_supplies	=	ARRAY_SIZE(vwm89942_consumer),
		.consumer_supplies	=	vwm89942_consumer,
};

static struct fixed_voltage_config vwm89942_config = {
	.supply_name	= "V_BAT",
	.microvolts	= 3700000,
	.gpio		= -EINVAL,
	.init_data  = &vwm89942_data,
};

static struct platform_device vwm89942_device = {
	.name = "reg-fixed-voltage",
	.id = PLATFORM_DEVID_AUTO,
	.dev = {
		.platform_data = &vwm89942_config,
	},
};

static struct platform_device wm8994_ldo1_device;
static struct platform_device wm8994_ldo2_device;
static struct platform_device *wm1811a_reg_devices[] __initdata = {
	&vwm89941_device,
	&vwm89942_device,
	&wm8994_ldo1_device,
	&wm8994_ldo2_device
};

static struct platform_device *wm8958_reg_devices[] __initdata = {
	&vwm89941_device,
	&vwm89942_device
};

static struct regulator_consumer_supply wm8994_avdd1_supply =
	REGULATOR_SUPPLY("AVDD1", "1-001a");

static struct regulator_consumer_supply wm8994_dcvdd_supply =
	REGULATOR_SUPPLY("DCVDD", "1-001a");

static struct regulator_init_data wm8994_ldo1_data = {
	.constraints	= {
		.always_on	= 1,
		.name		= "AVDD1_3.0V",
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &wm8994_avdd1_supply,
};

static struct fixed_voltage_config wm8994_ldo1_config = {
	.supply_name	= "V_BAT_X",
	.microvolts	= 3700000,
	.gpio		= -EINVAL,
	.init_data  = &wm8994_ldo1_data,
};

static struct platform_device wm8994_ldo1_device = {
	.name = "reg-fixed-voltage",
	.id = PLATFORM_DEVID_AUTO,
	.dev = {
		.platform_data = &wm8994_ldo1_config,
	},
};


static struct regulator_init_data wm8994_ldo2_data = {
	.constraints	= {
		.always_on	= 1,
		.name		= "DCVDD_1.0V",
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &wm8994_dcvdd_supply,
};

static struct fixed_voltage_config wm8994_ldo2_config = {
	.supply_name	= "V_BAT_Y",
	.microvolts	= 3700000,
	.gpio		= -EINVAL,
	.init_data  = &wm8994_ldo2_data,
};

static struct platform_device wm8994_ldo2_device = {
	.name = "reg-fixed-voltage",
	.id = PLATFORM_DEVID_AUTO,
	.dev = {
		.platform_data = &wm8994_ldo2_config,
	},
};

static struct  wm8958_custom_config custom_config = {
	.format = 6,
	.rate = 48000,
	.channels = 2,
};

static struct wm8994_pdata wm8994_pdata = {
	/* configure gpio1 function: 0x0001(Logic level input/output) */
	.gpio_defaults[0] = 0x0003,
	.irq_flags = IRQF_TRIGGER_RISING | IRQF_ONESHOT,
	/* FIXME: Below are 1811A specfic, we need to use SPID for these */

	/* configure gpio3/4/5/7 function for AIF2 voice */
	.gpio_defaults[2] = 0x8100,
	.gpio_defaults[3] = 0x8100,
	.gpio_defaults[4] = 0x8100,
	.gpio_defaults[6] = 0x0100,
	/* configure gpio8/9/10/11 function for AIF3 BT */
	/* gpio7 is codec intr pin for GV M2 */
	.gpio_defaults[7] = 0x0003,
	.gpio_defaults[8] = 0x0105,
	.gpio_defaults[9] = 0x0100,
	.gpio_defaults[10] = 0x0100,
	.ldo[0]	= { 0, &wm8994_ldo1_data }, /* set actual value at wm8994_platform_data() */
	.ldo[1]	= { 0, &wm8994_ldo2_data },
	.ldo_ena_always_driven = 1,

	.mic_id_delay = 300, /*300ms delay*/
	.micdet_delay = 500,
	.micb_en_delay = 5000, /* Keeps MICBIAS2 high for 5sec during jack insertion/removal */

	.custom_cfg = &custom_config,
};

static struct wm8994_pdata wm8994_mofd_pr_pdata = {
	/* configure gpio1 function as irq */
	.gpio_defaults[0] = 0x0003,

	/* configure gpio 6 as output to control DMIC clock */
	/* Pull up, Invert, CMOS, default value=1 (driven low due to invert) */
	.gpio_defaults[5] = 0x4441,
	/* configure gpio8/9/10/11 function for AIF3 BT */
	/* GPIO8 => DAC3 (Rx) pin, configure it as alt fn & i/p */
	/* GPIO9 => ADC3 (Tx) pin, configure it as alt fn & o/p */
	/* GPIO10 => LRCLK (FS) pin, configure it as alt fn & o/p */
	/* GPIO11 => BCLK pin, configure it as alt fn & o/p */
	.gpio_defaults[7] = 0x1000,
	.gpio_defaults[8] = 0x0100,
	.gpio_defaults[9] = 0x0100,
	.gpio_defaults[10] = 0x0100,
	.irq_flags = IRQF_TRIGGER_HIGH | IRQF_ONESHOT,

	.mic_id_delay = 300, /*300ms delay*/
	.micdet_delay = 500,
	.micb_en_delay = 5000, /* Keeps MICBIAS2 high for 5sec during jack insertion/removal */

	.custom_cfg = &custom_config,
};

static int wm8994_get_irq_data(struct wm8994_pdata *pdata,
			struct i2c_board_info *i2c_info, char *name)
{
	int codec_gpio;

	/* alek tells me that since driver is registering a new chip
	 * irq we need to give it a base which is unused so put
	 * 256+192 here */
	pdata->irq_base = (256 + 192);
	codec_gpio = get_gpio_by_name(name);
	if (codec_gpio < 0) {
		pr_err("%s failed for : %d\n", __func__, codec_gpio);
		return -EINVAL;
	}
	i2c_info->irq = codec_gpio + INTEL_MID_IRQ_OFFSET;
	return codec_gpio;
}

static int wm8994_fill_mofd_pr_data(struct wm8994_pdata *pdata)
{
	if (!pdata) {
		pr_err("%s: pdata is NULL\n", __func__);
		return -EINVAL;
	}

	/* Only MOFD v0-PR0 & v1-PR1, utilizes the LDOs */
	if (INTEL_MID_BOARD(3, PHONE, MOFD, V0, PRO, PR0) ||
		INTEL_MID_BOARD(3, PHONE, MOFD, V1, PRO, PR1)) {

		pr_debug("%s: Assign LDOs to MOFD PR0's pdata...\n", __func__);
		pdata->ldo[0].enable = pdata->ldo[1].enable = 0;
		pdata->ldo[0].init_data = &wm8994_ldo1_data;
		pdata->ldo[1].init_data = &wm8994_ldo2_data;
		pdata->ldo_ena_always_driven = 1;
	}

	return 0;
}

void __init *wm8994_platform_data(void *info)
{
	struct i2c_board_info *i2c_info = (struct i2c_board_info *)info;
	int irq = 0, ret = 0;
	struct wm8994_pdata *pdata = &wm8994_pdata;

	if ((INTEL_MID_BOARD(1, PHONE, MRFL)) ||
		   (INTEL_MID_BOARD(1, TABLET, MRFL))) {
		platform_add_devices(wm8958_reg_devices,
				ARRAY_SIZE(wm8958_reg_devices));
		irq = wm8994_get_irq_data(pdata, i2c_info, "audiocodec_int");
		if (irq < 0)
			return NULL;
	} else if ((INTEL_MID_BOARD(1, PHONE, MOFD)) ||
		   (INTEL_MID_BOARD(1, TABLET, MOFD))) {
		platform_add_devices(wm8958_reg_devices,
				ARRAY_SIZE(wm8958_reg_devices));

		/* if it is not VV, then use PR pdata */
		if (!(SPID_PRODUCT(INTEL, MOFD, PHONE, MP))) {
			pdata = &wm8994_mofd_pr_pdata;
			ret = wm8994_fill_mofd_pr_data(pdata);
			if (ret < 0)
				return NULL;
		}

		irq = wm8994_get_irq_data(pdata, i2c_info, "audiocodec_int");
		if (irq < 0)
			return NULL;
	} else if ((SPID_PRODUCT(INTEL, CLVTP, PHONE, RHB)) ||
		   (SPID_PRODUCT(INTEL, CLVT, TABLET, TBD))) {

		platform_add_devices(wm1811a_reg_devices,
			ARRAY_SIZE(wm1811a_reg_devices));

		i2c_info->addr = 0x1a;
	} else {
		pr_err("Not supported....\n");
		return NULL;
	}

	return pdata;
}
