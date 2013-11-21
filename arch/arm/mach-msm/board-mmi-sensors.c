/*
 * linux/arch/arm/mach-msm/board-8960-sensors.c
 *
 * Copyright (C) 2009-2012 Motorola Mobility, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifdef CONFIG_INPUT_CT406
#include <linux/ct406.h>
#endif
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>

#include "board-mmi.h"

#ifdef CONFIG_INPUT_CT406
/*
 * CT406
 */

static struct gpiomux_setting ct406_reset_suspend_config = {
                        .func = GPIOMUX_FUNC_GPIO,
                        .drv = GPIOMUX_DRV_2MA,
                        .pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting ct406_reset_active_config = {
                        .func = GPIOMUX_FUNC_GPIO,
                        .drv = GPIOMUX_DRV_2MA,
                        .pull = GPIOMUX_PULL_NONE,
                        .dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config ct406_irq_gpio_config = {
        .settings = {
                [GPIOMUX_SUSPENDED] = &ct406_reset_suspend_config,
                [GPIOMUX_ACTIVE] = &ct406_reset_active_config,
        },
};

struct ct406_platform_data mp_ct406_pdata = {
	.regulator_name = "",
	.prox_samples_for_noise_floor = 0x05,
	.ct406_prox_covered_offset = 0x008c,
	.ct406_prox_uncovered_offset = 0x0046,
	.ct406_prox_recalibrate_offset = 0x0046,
	.ct406_prox_pulse_count = 0x02,
	.ct406_prox_offset = 0x00,
};

int __init ct406_init(struct i2c_board_info *info, struct device_node *child)

{
	int ret = 0;
	int value = 0;
	unsigned gpio = 0;

	info->platform_data = &mp_ct406_pdata;

	if (!of_property_read_u32(child,"irq,gpio", &gpio))
		info->irq = gpio_to_irq(gpio);

	ct406_irq_gpio_config.gpio = gpio;
	msm_gpiomux_install(&ct406_irq_gpio_config, 1);
	ret = gpio_request(gpio, "ct406 proximity int");
	if (ret) {
		pr_err("ct406 gpio_request failed: %d\n", ret);
		goto fail;
	}

	ret = gpio_export(gpio, 0);
	if (ret) {
		pr_err("ct406 gpio_export failed: %d\n", ret);
	}

	mp_ct406_pdata.irq = info->irq;

	if(!of_property_read_u32(child, "prox_samples_for_noise_floor", &value))
		mp_ct406_pdata.prox_samples_for_noise_floor = (u8 )value;
	if(!of_property_read_u32(child, "ct406_prox_covered_offset", &value))
		mp_ct406_pdata.ct406_prox_covered_offset = (u16 )value;
	if(!of_property_read_u32(child, "ct406_prox_uncovered_offset", &value))
		mp_ct406_pdata.ct406_prox_uncovered_offset = (u16 )value;
	if(!of_property_read_u32(child, "ct406_prox_recalibrate_offset", &value))
		mp_ct406_pdata.ct406_prox_recalibrate_offset = (u16 )value;
	if(!of_property_read_u32(child, "ct406_prox_pulse_count", &value))
		mp_ct406_pdata.ct406_prox_pulse_count = (u8 )value;
	if(!of_property_read_u32(child, "ct406_prox_offset", &value))
		mp_ct406_pdata.ct406_prox_offset = (u8 )value;

	return 0;

fail:
	return ret;
}
#else
static int __init ct406_init(void)
{
	return 0;
}
#endif //CONFIG_CT406


/*
 * DSPS Firmware
 */

void msm8960_get_dsps_fw_name(char *name)
{
	struct device_node *dsps_node;
	int len = 0;
	const void *prop;

	dsps_node = of_find_node_by_path("/Chosen@0");
	if (!dsps_node)
		return;
	prop = of_get_property(dsps_node, "qualcomm,dsps,fw", &len);

	if (prop) {
		strcpy(name, (char *)prop);
		printk(KERN_INFO "%s: DSPS FW version from devtree: %s\n",
			 __func__, (char *)prop);
	}
}

/*
 * TMP105/LM75 temperature sensor
 */

static struct gpiomux_setting tmp105_int_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting tmp105_int_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config tmp105_gpio_configs = {
	.settings = {
		[GPIOMUX_ACTIVE]    = &tmp105_int_act_cfg,
		[GPIOMUX_SUSPENDED] = &tmp105_int_sus_cfg,
	},
};

int  __init msm8960_tmp105_init(struct i2c_board_info *info,
				struct device_node *child)
{
	int ret = 0;
	unsigned gpio = 0;

	pr_debug("msm8960_tmp105_init\n");

	if(!of_property_read_u32(child, "irq,gpio", &gpio))
		tmp105_gpio_configs.gpio = gpio;

	msm_gpiomux_install(&tmp105_gpio_configs, 1);

	ret = gpio_request(gpio, "tmp105_intr");
	if (ret) {
		pr_err("gpio_request tmp105_intr GPIO (%d) failed (%d)\n",
			gpio, ret);
		return ret;
	}
	gpio_direction_input(gpio);

	ret = gpio_export(gpio, 0);
	if (ret)
		pr_err("tmp105 gpio_export failed: %d\n", ret);
	return 0;
}
