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
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/keyreset.h>
#include <linux/module.h>

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

static struct gpio_keys_button mmi_gpio_keys_table_asanti[] = {
	{
		.code			= KEY_VOLUMEDOWN,
		.gpio			= PM8921_GPIO_PM_TO_SYS(33),
		.active_low		= 1,
		.desc			= "VOLUME_DOWN",
		.type			= EV_KEY,
		.wakeup			= 1,
		.debounce_interval	= 20,
	},
	{
		.code			= SW_LID,
		.gpio			= 11,
		.active_low		= 1,
		.desc			= "SLIDE",
		.type			= EV_SW,
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

static const unsigned int mmi_qwerty_keymap[] = {

	KEY(0, 0, KEY_9),
	KEY(0, 1, KEY_R),
	KEY(0, 2, KEY_CAMERA),
	KEY(0, 3, 0),
	KEY(0, 4, 0),
	KEY(0, 5, KEY_VOLUMEUP),
	KEY(0, 6, KEY_MINUS),
	KEY(0, 7, KEY_D),

	KEY(1, 0, KEY_7),
	KEY(1, 1, KEY_M),
	KEY(1, 2, KEY_L),
	KEY(1, 3, KEY_K),
	KEY(1, 4, KEY_N),
	KEY(1, 5, KEY_C),
	KEY(1, 6, KEY_Z),
	KEY(1, 7, 0),

	KEY(2, 0, KEY_1),
	KEY(2, 1, KEY_Y),
	KEY(2, 2, KEY_I),
	KEY(2, 3, KEY_SLASH),
	KEY(2, 4, KEY_LEFTALT),
	KEY(2, 5, KEY_DOT),
	KEY(2, 6, KEY_G),
	KEY(2, 7, KEY_E),

	KEY(3, 0, 0),
	KEY(3, 1, 0),
	KEY(3, 2, KEY_3),
	KEY(3, 3, KEY_DOWN),
	KEY(3, 4, KEY_UP),
	KEY(3, 5, KEY_LEFT),
	KEY(3, 6, KEY_RIGHT),
	KEY(3, 7, KEY_REPLY),

	KEY(4, 0, KEY_5),
	KEY(4, 1, KEY_J),
	KEY(4, 2, KEY_B),
	KEY(4, 3, KEY_GRAVE),
	KEY(4, 4, KEY_T),
	KEY(4, 5, KEY_TAB),
	KEY(4, 6, 0),
	KEY(4, 7, KEY_X),

	KEY(5, 0, KEY_8),
	KEY(5, 1, KEY_SPACE),
	KEY(5, 2, KEY_LEFTSHIFT),
	KEY(5, 3, KEY_COMMA),
	KEY(5, 4, KEY_RIGHTALT),
	KEY(5, 5, KEY_6),
	KEY(5, 6, KEY_BACKSPACE),
	KEY(5, 7, 0),

	KEY(6, 0, KEY_2),
	KEY(6, 1, KEY_0),
	KEY(6, 2, KEY_F),
	KEY(6, 3, KEY_CAPSLOCK),
	KEY(6, 4, KEY_ENTER),
	KEY(6, 5, KEY_O),
	KEY(6, 6, KEY_H),
	KEY(6, 7, KEY_Q),

	KEY(7, 0, KEY_4),
	KEY(7, 1, KEY_V),
	KEY(7, 2, KEY_S),
	KEY(7, 3, KEY_P),
	KEY(7, 4, KEY_EQUAL),
	KEY(7, 5, KEY_A),
	KEY(7, 6, KEY_U),
	KEY(7, 7, KEY_W),

};

static struct matrix_keymap_data mmi_qwerty_keymap_data = {
	.keymap_size    = ARRAY_SIZE(mmi_qwerty_keymap),
	.keymap         = mmi_qwerty_keymap,
};

struct pm8xxx_keypad_platform_data mmi_qwerty_keypad_data = {
	.input_name             = "keypad_8960",
	.input_phys_device      = "keypad_8960/input0",
	.num_rows               = 8,
	.num_cols               = 8,
	.rows_gpio_start	= PM8921_GPIO_PM_TO_SYS(9),
	.cols_gpio_start	= PM8921_GPIO_PM_TO_SYS(1),
	.debounce_ms            = 15,
	.scan_delay_ms          = 32,
	.row_hold_ns            = 91500,
	.wakeup                 = 1,
	.keymap_data            = &mmi_qwerty_keymap_data,
};

static int lid_state, filtered;

static int keypad_lock_connect(struct input_handler *handler,
		struct input_dev *dev, const struct input_device_id *id)
{
	int ret;
	struct input_handle *handle;

	if (strncmp(dev->name , "gpio-keys", sizeof("gpio-keys")) &&
		strncmp(dev->name , "keypad_8960", sizeof("keypad_8960")))
		return -ENODEV;

	handle = kzalloc(sizeof(*handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;
	handle->dev = dev;
	handle->handler = handler;
	handle->name = "keypad_lock";

	ret = input_register_handle(handle);
	if (ret)
		goto err_input_register_handle;

	ret = input_open_device(handle);
	if (ret)
		goto err_input_open_device;

	pr_info("GNV ==> added keypad filter to input dev %s\n", dev->name);

	return 0;

err_input_open_device:
	input_unregister_handle(handle);
err_input_register_handle:
	kfree(handle);
	return ret;
}

static void keypad_lock_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static bool keypad_lock_filter(struct input_handle *handle, unsigned int type,
						unsigned int code, int value)
{
	if ((type == EV_SW) && (code == SW_LID)) {
		lid_state = value;
		return false;
	}

	if ((type == EV_KEY) && lid_state) {
		if ((code != KEY_VOLUMEUP) && (code != KEY_VOLUMEDOWN) &&
		    (code != KEY_CAMERA) && (code != KEY_CAMERA_SNAPSHOT) &&
		    (code != KEY_POWER) && (code != BTN_MISC)) {
			filtered = 1;
			return true;
		}
	}
	/* Filter all scan events events if the slider is closed. */
	if ((type == EV_MSC) && (code == MSC_SCAN) && lid_state)
		return true;
	/* Filter sync event following filtered keyevent */
	if ((type == EV_SYN) && lid_state && filtered) {
		filtered = 0;
		return true;
	}
	return false;
}


static const struct input_device_id keypad_lock_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) },
	},
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_SW) },
		.absbit = { BIT_MASK(SW_LID) },
	},
	{ },
};

MODULE_DEVICE_TABLE(input, keypad_lock_ids);

static struct input_handler keypad_lock_handler = {
	.filter     = keypad_lock_filter,
	.connect    = keypad_lock_connect,
	.disconnect = keypad_lock_disconnect,
	.name       = "keypad_lock",
	.id_table   = keypad_lock_ids,
};

static __init bool config_keyboard_from_dt(void)
{
	struct device_node *chosen;
	int len = 0;
	const void *prop;

	chosen = of_find_node_by_path("/Chosen@0");
	if (!chosen)
		return false;

	prop = of_get_property(chosen, "qwerty_keyboard", &len);
	of_node_put(chosen);

	if (prop)
		return true;
	else
		return false;
}

void __init mmi_pm8921_keypad_init(void *ptr)
{
	struct pm8921_platform_data *pdata;

	pdata = (struct pm8921_platform_data *)ptr;

	if (config_keyboard_from_dt()) {
		pdata->keypad_pdata = &mmi_qwerty_keypad_data;
		mmi_gpio_keys_data.buttons = mmi_gpio_keys_table_asanti;
		if (input_register_handler(&keypad_lock_handler))
			pr_info("input_register_handler failed\n");
	} else {
		pdata->keypad_pdata = NULL;
	}

	platform_device_register(&mmi_gpiokeys_device);
}
