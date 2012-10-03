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
#include <linux/gpio_keys.h>
#include "board-8960.h"

static struct gpio_keys_button mmi_gpio_keys_table[] = {
	{
		.code			= KEY_VOLUMEDOWN,
		.gpio			= PM8921_GPIO_PM_TO_SYS(3),
		.active_low		= 1,
		.desc			= "VOLUME_DOWN",
		.type			= EV_KEY,
		.wakeup			= 1,
		.debounce_interval	= 20,
	},
	{
		.code			= KEY_VOLUMEUP,
		.gpio			= PM8921_GPIO_PM_TO_SYS(5),
		.active_low		= 1,
		.desc			= "VOLUME_UP",
		.type			= EV_KEY,
		.wakeup			= 1,
		.debounce_interval	= 20,
	},
};

static struct gpio_keys_platform_data mmi_gpio_keys_data = {
	.buttons        = mmi_gpio_keys_table,
	.nbuttons       = ARRAY_SIZE(mmi_gpio_keys_table),
	.rep		= false,
};

static struct platform_device mmi_gpiokeys_device = {
	.name           = "gpio-keys",
	.id		= -1,
	.dev = {
		.platform_data = &mmi_gpio_keys_data,
	},
};


void __init mmi_pm8921_keypad_init(void *ptr)
{
	struct pm8921_platform_data *pdata;

	pdata = (struct pm8921_platform_data *)ptr;
	pdata->keypad_pdata = NULL;

	platform_device_register(&mmi_gpiokeys_device);
}
