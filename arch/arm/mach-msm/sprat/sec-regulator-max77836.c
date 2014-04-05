/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/regulator/max77836-regulator.h>

static struct regulator_consumer_supply ldo1_consumer[] = {
	REGULATOR_SUPPLY("max77836_ldo1", NULL)
};

static struct regulator_consumer_supply ldo2_consumer[] = {
	REGULATOR_SUPPLY("max77836_ldo2", NULL)
};

static struct regulator_init_data max77836_ldo1_data = {
	.constraints	= {
		.name		= "max77836_l1",
		.min_uV		= 800000,
		.max_uV		= 3950000,
		.apply_uV	= 1,
		.always_on	= 0,
		.state_mem	= {
			.enabled	= 0,
		},
		.valid_ops_mask =
			REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo1_consumer),
	.consumer_supplies	= ldo1_consumer,
};

static struct regulator_init_data max77836_ldo2_data = {
	.constraints	= {
		.name		= "max77836_l2",
		.min_uV		= 800000,
		.max_uV		= 3950000,
		.apply_uV	= 1,
		.always_on	= 0,
		.state_mem	= {
			.enabled	= 0,
		},
		.valid_ops_mask =
			REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo2_consumer),
	.consumer_supplies	= ldo2_consumer,
};

struct max77836_reg_platform_data max77836_reglator_pdata[] = {
	{
		.reg_id = MAX77836_LDO1,
		.init_data = &max77836_ldo1_data,
		.overvoltage_clamp_enable = 0,
		.auto_low_power_mode_enable = 0,
		.compensation_ldo = 0,
		.active_discharge_enable = 0,
		.soft_start_slew_rate = 0,
		.enable_mode = REG_NORMAL_MODE,

	},
	{
		.reg_id = MAX77836_LDO2,
		.init_data = &max77836_ldo2_data,
		.overvoltage_clamp_enable = 0,
		.auto_low_power_mode_enable = 0,
		.compensation_ldo = 0,
		.active_discharge_enable = 0,
		.soft_start_slew_rate = 0,
		.enable_mode = REG_NORMAL_MODE,

	},
};
