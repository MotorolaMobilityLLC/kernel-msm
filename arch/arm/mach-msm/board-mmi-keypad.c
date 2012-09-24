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
#include <linux/slab.h>
#include <mach/devtree_util.h>
#include "board-8960.h"

static const unsigned int mmi_keymap[] = {
	KEY(0, 0, 0),
	KEY(0, 1, 0),
	KEY(0, 2, KEY_VOLUMEDOWN),
	KEY(0, 3, 0),
	KEY(0, 4, KEY_VOLUMEUP),
};

static struct matrix_keymap_data mmi_keymap_data = {
	.keymap_size    = ARRAY_SIZE(mmi_keymap),
	.keymap         = mmi_keymap,
};

static struct pm8xxx_keypad_platform_data mmi_keypad_data = {
	.input_name             = "keypad_8960",
	.input_phys_device      = "keypad_8960/input0",
	.num_rows               = 1,
	.num_cols               = 5,
	.rows_gpio_start        = PM8921_GPIO_PM_TO_SYS(9),
	.cols_gpio_start        = PM8921_GPIO_PM_TO_SYS(1),
	.debounce_ms            = 15,
	.scan_delay_ms          = 32,
	.row_hold_ns            = 91500,
	.wakeup                 = 1,
	.keymap_data            = &mmi_keymap_data,
};

void __init mmi_pm8921_keypad_init(void *ptr)
{
	struct pm8921_platform_data *pdata;

	pdata = (struct pm8921_platform_data *)ptr;
	pdata->keypad_pdata = &mmi_keypad_data;
}
