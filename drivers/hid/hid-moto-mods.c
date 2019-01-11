/*
 * HID driver for motorola mods
 *
 * Copyright (C) 2016 Motorola Mobility, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>

#include "hid-ids.h"

#define MODS_REDCARPET	0x01

#define mods_map_key_clear(c)	hid_map_usage_clear(hi, usage, bit, max, \
					EV_KEY, (c))
static int mods_input_mapping(struct hid_device *hdev, struct hid_input *hi,
		struct hid_field *field, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	if ((usage->hid & HID_USAGE_PAGE) != HID_UP_CONSUMER)
		return 0;

	switch (usage->hid & HID_USAGE) {
	case 0x066:
		mods_map_key_clear(KEY_CAMERA_FOCUS);
		break;
	case 0x067:
		mods_map_key_clear(KEY_CAMERA_UP);
		break;
	default:
		return 0;
	}
	return 1;
}

static int mods_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	unsigned long quirks = id->driver_data;
	int ret;

	hid_set_drvdata(hdev, (void *)quirks);

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed\n");
		goto err_free;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		goto err_free;
	}

	return 0;
err_free:
	return ret;
}

static const struct hid_device_id mods_devices[] = {
	{ HID_DEVICE(BUS_GREYBUS, HID_GROUP_ANY,
		     USB_VENDOR_ID_MOTOROLA_PCS, USB_DEVICE_ID_MODS_CAMERA),
	.driver_data = MODS_REDCARPET },
	{ }
};
MODULE_DEVICE_TABLE(hid, mods_devices);

static struct hid_driver mods_driver = {
	.name = "Motorola MODS",
	.id_table = mods_devices,
	.input_mapping = mods_input_mapping,
	.probe = mods_probe,
};
module_hid_driver(mods_driver);

MODULE_LICENSE("GPL");
