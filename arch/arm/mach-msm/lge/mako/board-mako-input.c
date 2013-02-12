/* Copyright (c) 2009-2010, The Linux Foundation. All rights reserved.
 * Copyright (c) 2012, LGE Inc.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/gpio_event.h>

#include <mach/vreg.h>
#include <mach/rpc_server_handset.h>
#include <mach/board.h>

/* keypad */
#include <linux/mfd/pm8xxx/pm8921.h>

/* i2c */
#include <linux/regulator/consumer.h>
#include <linux/i2c.h>

#include <linux/earlysuspend.h>
#include <linux/input/lge_touch_core.h>
#include <mach/board_lge.h>
#include "board-mako.h"

/* TOUCH GPIOS */
#define SYNAPTICS_TS_I2C_SDA                 	8
#define SYNAPTICS_TS_I2C_SCL                 	9
#define SYNAPTICS_TS_I2C_INT_GPIO            	6
#define TOUCH_RESET                             33

#define TOUCH_FW_VERSION                        1

/* touch screen device */
#define APQ8064_GSBI3_QUP_I2C_BUS_ID            3

int synaptics_t1320_power_on(int on)
{
	int rc = -EINVAL;
	static struct regulator *vreg_l15 = NULL;
	static struct regulator *vreg_l22 = NULL;

	/* 3.3V_TOUCH_VDD, VREG_L15: 2.75 ~ 3.3 */
	if (!vreg_l15) {
		vreg_l15 = regulator_get(NULL, "touch_vdd");
		if (IS_ERR(vreg_l15)) {
			pr_err("%s: regulator get of 8921_l15 failed (%ld)\n",
					__func__,
			       PTR_ERR(vreg_l15));
			rc = PTR_ERR(vreg_l15);
			vreg_l15 = NULL;
			return rc;
		}
	}
	/* 1.8V_TOUCH_IO, VREG_L22: 1.7 ~ 2.85 */
	if (!vreg_l22) {
		vreg_l22 = regulator_get(NULL, "touch_io");
		if (IS_ERR(vreg_l22)) {
			pr_err("%s: regulator get of 8921_l22 failed (%ld)\n",
					__func__,
			       PTR_ERR(vreg_l22));
			rc = PTR_ERR(vreg_l22);
			vreg_l22 = NULL;
			return rc;
		}
	}

	rc = regulator_set_voltage(vreg_l15, 3300000, 3300000);
	rc |= regulator_set_voltage(vreg_l22, 1800000, 1800000);
	if (rc < 0) {
		printk(KERN_INFO "[Touch D] %s: cannot control regulator\n",
		       __func__);
		return rc;
	}

	if (on) {
		printk("[Touch D]touch enable\n");
		regulator_enable(vreg_l15);
		regulator_enable(vreg_l22);
	} else {
		printk("[Touch D]touch disable\n");
		regulator_disable(vreg_l15);
		regulator_disable(vreg_l22);
	}

	return rc;
}

static struct touch_power_module touch_pwr = {
	.use_regulator = 0,
	.vdd = "8921_l15",
	.vdd_voltage = 3300000,
	.vio = "8921_l22",
	.vio_voltage = 1800000,
	.power = synaptics_t1320_power_on,
};

static struct touch_device_caps touch_caps = {
	.button_support = 0,
	.is_width_major_supported = 1,
	.is_width_minor_supported = 0,
	.is_pressure_supported = 1,
	.is_id_supported = 1,
	.max_width_major = 15,
	.max_width_minor = 15,
	.max_pressure = 0xFF,
	.max_id = 10,
	.lcd_x = 768,
	.lcd_y = 1280,
	.x_max = 1536-1,
	.y_max = 2560-1,
};

static struct touch_operation_role touch_role = {
	.operation_mode = INTERRUPT_MODE,
	.key_type = KEY_NONE,
	.report_mode = REDUCED_REPORT_MODE,
	.delta_pos_threshold = 1,
	.orientation = 0,
	.booting_delay = 400,
	.reset_delay = 20,
	.suspend_pwr = POWER_OFF,
	.resume_pwr = POWER_ON,
	.jitter_filter_enable = 0,
	.jitter_curr_ratio = 30,
	.accuracy_filter_enable = 0,
	.irqflags = IRQF_TRIGGER_FALLING,
	.show_touches = 0,
	.pointer_location = 0,
};

static struct touch_platform_data mako_ts_data = {
	.int_pin = SYNAPTICS_TS_I2C_INT_GPIO,
	.reset_pin = TOUCH_RESET,
	.maker = "Synaptics",
	.caps = &touch_caps,
	.role = &touch_role,
	.pwr = &touch_pwr,
};

static struct i2c_board_info synaptics_ts_info[] = {
	[0] = {
		I2C_BOARD_INFO(LGE_TOUCH_NAME, 0x20),
		.platform_data = &mako_ts_data,
		.irq = MSM_GPIO_TO_INT(SYNAPTICS_TS_I2C_INT_GPIO),
	},
};

void __init apq8064_init_input(void)
{
	printk(KERN_INFO "[Touch D] %s: NOT DCM KDDI, reg synaptics driver \n",
	       __func__);
	i2c_register_board_info(APQ8064_GSBI3_QUP_I2C_BUS_ID,
				&synaptics_ts_info[0], 1);
}
