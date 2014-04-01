/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __REGULATOR_MAX77836_REGULATOR_H__
#define __REGULATOR_MAX77836_REGULATOR_H__

#include <linux/regulator/machine.h>

#define MAX77836_DRIVER_NAME "max77836"
#define MAX77836_REGULATOR_DRIVER_NAME "maxim,max77836-regulator"

enum max77836_ldo_id {
	MAX77836_LDO1,
	MAX77836_LDO2,
	MAX77836_LDO_MAX,
};

enum max77836_opmode {
	REG_OUTPUT_DISABLE = 0,		/* Never use this value */
	REG_LOW_POWER_MODE,
	REG_LOW_POWER_MODE2,
	REG_NORMAL_MODE,
};

/*
 * Used with enable parameters to specify that hardware default register values
 * should be left unaltered.
 */
#define MAX77836_REGULATOR_USE_HW_DEFAULT		0xFF

/**
 * struct max77836_reg_platform_data - max77836-regulator initialization data
 * @init_data:		regulator constraints
 * @overvoltage_clamp_enable:	Overvoltage Clamp Enable for LDOx
 *			0 = overvoltage clamp disabled
 *			1 = overvoltage clamp enabled
 * @auto_low_power_mode_enable:	Auto Low Power Mode Enable
 *			0 = Disable
 *			1 = Enable
 * @compensation_ldo:	The optimum compensation for each LDO is dependent on
 *		the series R-L-C impedance from the LDO outpu and its ground.
 *			0 = Fast transconductance setting
 *			1 = Medium-Fast trnsconductance setting
 *			2 = Medium-slow transconductance setting
 *			3 = Slow transconductance setting
 *		MAX77836_REGULATOR_USE_HW_DEFAULT = do not modify
 * @active_discharge_enable:	Active Discharge Enable for LDOx
 *			0 = Disable
 *			1 = Enable
 * @soft_start_slew_rate:	Soft-Start Slew Rate Configuration for LDOx
 *			0 = Fast Startup and Dynamic Voltage Change - 100mV/us.
 *			1 = Slow Startup and Dynamic Voltage Change - 5mV/us.
 * @enable_mode: The enable mode when LDO turns on
 *			0 = Disable - Don't select this
 *			1 = Low-Power Mode
 *			2 = Low-Power Mode - same as 1
 *			3 = Normal Mode
 * @reg_id:		regulator id
 */

struct max77836_reg_platform_data {
	struct regulator_init_data		*init_data;
	int		overvoltage_clamp_enable;
	int		auto_low_power_mode_enable;
	int		compensation_ldo;
	int		active_discharge_enable;
	int		soft_start_slew_rate;
	int		enable_mode;
	int		reg_id;
};

#endif
