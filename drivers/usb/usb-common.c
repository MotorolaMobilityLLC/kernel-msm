/*
 * Provides code common for host and device side USB.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 *
 * If either host side (ie. CONFIG_USB=y) or device side USB stack
 * (ie. CONFIG_USB_GADGET=y) is compiled in the kernel, this module is
 * compiled-in as well.  Otherwise, if either of the two stacks is
 * compiled as module, this file is compiled as module as well.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/usb/ch9.h>
#include <linux/usb/of.h>

static const char *const speed_names[] = {
	[USB_SPEED_UNKNOWN] = "UNKNOWN",
	[USB_SPEED_LOW] = "low-speed",
	[USB_SPEED_FULL] = "full-speed",
	[USB_SPEED_HIGH] = "high-speed",
	[USB_SPEED_WIRELESS] = "wireless",
	[USB_SPEED_SUPER] = "super-speed",
};

const char *usb_speed_string(enum usb_device_speed speed)
{
	if (speed < 0 || speed >= ARRAY_SIZE(speed_names))
		speed = USB_SPEED_UNKNOWN;
	return speed_names[speed];
}
EXPORT_SYMBOL_GPL(usb_speed_string);


#ifdef CONFIG_OF
/**
 * of_usb_get_maximum_speed - Get maximum requested speed for a given USB
 * controller.
 * @np: Pointer to the given device_node
 *
 * The function gets the maximum speed string from property "maximum-speed",
 * and returns the corresponding enum usb_device_speed.
 */
enum usb_device_speed of_usb_get_maximum_speed(struct device_node *np)
{
	const char *maximum_speed;
	int err;
	int i;

	err = of_property_read_string(np, "maximum-speed", &maximum_speed);
	if (err < 0)
		return USB_SPEED_UNKNOWN;

	for (i = 0; i < ARRAY_SIZE(speed_names); i++)
		if (strcmp(maximum_speed, speed_names[i]) == 0)
			return i;

	return USB_SPEED_UNKNOWN;
}
EXPORT_SYMBOL_GPL(of_usb_get_maximum_speed);
#endif

MODULE_LICENSE("GPL");
