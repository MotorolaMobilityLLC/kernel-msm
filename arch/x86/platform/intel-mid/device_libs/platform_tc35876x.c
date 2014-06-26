/*
 * platform_tc35876x.c: tc35876x platform data initilization file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/gpio.h>
#include <linux/i2c/tc35876x.h>
#include <asm/intel-mid.h>
#include "platform_tc35876x.h"

/*tc35876x DSI_LVDS bridge chip and panel platform data*/
void *tc35876x_platform_data(void *data)
{
	static struct tc35876x_platform_data pdata;
	pdata.gpio_bridge_reset = get_gpio_by_name("LCMB_RXEN");
	pdata.gpio_panel_bl_en = get_gpio_by_name("6S6P_BL_EN");
	pdata.gpio_panel_vadd = get_gpio_by_name("EN_VREG_LCD_V3P3");
	return &pdata;
}
