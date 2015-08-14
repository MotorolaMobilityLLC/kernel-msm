/*
 * arch/arm/mach-msm/asustek/asustek-keypad.c
 * Keys configuration for Qualcomm platform.
 *
 * Copyright (C) 2012-2013 Asustek, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/gpio_keys.h>

#include <mach/board_asustek.h>
#include "board-8064.h"

#define GPIO_KEY_POWER		26
#define GPIO_KEY_VOLUMEUP	15
#define GPIO_KEY_VOLUMEDOWN	36
#define GPIO_PM8921_KEY_VOLUME_UP	PM8921_GPIO_PM_TO_SYS(35)
#define GPIO_PM8921_KEY_VOLUME_DOWN	PM8921_GPIO_PM_TO_SYS(38)
#define GPIO_PM8921_KEY2_VOLUME_UP	PM8921_GPIO_PM_TO_SYS(4)
#define GPIO_PM8921_KEY2_VOLUME_DOWN	GPIO_PM8921_KEY_VOLUME_DOWN

#define GPIO_KEY(_id, _iswake)		\
	{					\
		.code = _id,			\
		.gpio = GPIO_##_id,		\
		.active_low = 1,		\
		.desc = #_id,			\
		.type = EV_KEY,			\
		.wakeup = _iswake,		\
		.debounce_interval = 5,	\
	}

static struct gpio_keys_button asustek_keys[] = {
	[0] = GPIO_KEY(KEY_POWER, 1),
	[1] = GPIO_KEY(KEY_VOLUMEUP, 0),
	[2] = GPIO_KEY(KEY_VOLUMEDOWN, 0),
};

static struct gpio_keys_platform_data asustek_keys_platform_data = {
	.buttons	= asustek_keys,
	.nbuttons	= ARRAY_SIZE(asustek_keys),
};

static struct platform_device asustek_keys_device = {
	.name   = "gpio-keys",
	.id     = 0,
	.dev    = {
		.platform_data  = &asustek_keys_platform_data,
	},
};

static void gpio_keys_remap(void)
{
	hw_rev ret = HW_REV_INVALID;

	ret = asustek_get_hw_rev();
	switch (ret) {
	case HW_REV_B:
		pr_info(
		"Reconfigure VOL_UP with GPIO%d and VOL_DOWN with GPIO%d\n",
				GPIO_KEY_VOLUMEUP, GPIO_KEY_VOLUMEDOWN);
		break;

	case HW_REV_C:
		pr_info(
		"Reconfigure VOL_UP(GPIO%d) and VOL_DOWN(GPIO%d) with PMIC\n",
					GPIO_PM8921_KEY_VOLUME_UP,
					GPIO_PM8921_KEY_VOLUME_DOWN);
		asustek_keys[1].gpio = GPIO_PM8921_KEY_VOLUME_UP;
		asustek_keys[2].gpio = GPIO_PM8921_KEY_VOLUME_DOWN;
		break;

	case HW_REV_D:
	case HW_REV_E:
		pr_info(
		"Reconfigure VOL_UP(GPIO%d) and VOL_DOWN(GPIO%d) with PMIC\n",
					GPIO_PM8921_KEY2_VOLUME_UP,
					GPIO_PM8921_KEY2_VOLUME_DOWN);
		asustek_keys[1].gpio = GPIO_PM8921_KEY2_VOLUME_UP;
		asustek_keys[2].gpio = GPIO_PM8921_KEY2_VOLUME_DOWN;
		break;

	default:
		break;
	}
}

void __init asustek_add_keypad(void)
{
	pr_info("Registering gpio keys\n");

	gpio_keys_remap();

	platform_device_register(&asustek_keys_device);
}
