/*
 * Copyright(c) 2012, Analogix Semiconductor. All rights reserved.
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

#ifndef SLIMPORT_DEVICE
#define SLIMPORT_DEVICE
#include <linux/pinctrl/consumer.h>

struct anx7816_platform_data
{
	bool check_slimport_connection;
	int gpio_p_dwn;
	int gpio_reset;
	int gpio_int;
	int gpio_cbl_det;
	int gpio_v10_ctrl;
	int gpio_v33_ctrl;
	spinlock_t lock;
	int external_ldo_control;
	int (* avdd_power) (unsigned int onoff);
	int (* dvdd_power) (unsigned int onoff);
	struct regulator *avdd_33;
	struct regulator *dvdd_10;
	struct pinctrl *pinctrl;
	struct pinctrl_state *hdmi_pinctrl_active;
	struct pinctrl_state *hdmi_pinctrl_suspend;
#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
	struct platform_device *hdmi_pdev;
#endif

};

struct anx7808_platform_data
{
	int gpio_p_dwn;
	int gpio_reset;
	int gpio_int;
	int gpio_cbl_det;
	int gpio_v10_ctrl;
	int gpio_v33_ctrl;
	spinlock_t lock;
	int external_ldo_control;
	int (* avdd_power) (unsigned int onoff);
	int (* dvdd_power) (unsigned int onoff);
	struct regulator *avdd_10;
	struct regulator *dvdd_10;
#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
	struct platform_device *hdmi_pdev;
#endif

};

#endif /* SLIMPORT_DEVICE */
