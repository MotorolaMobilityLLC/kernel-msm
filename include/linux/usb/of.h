/*
 * OF helpers for usb devices.
 *
 * This file is released under the GPLv2
 */

#ifndef __LINUX_USB_OF_H
#define __LINUX_USB_OF_H

#include <linux/usb/ch9.h>

#if IS_ENABLED(CONFIG_OF)
enum usb_device_speed of_usb_get_maximum_speed(struct device_node *np);
#else
static inline enum usb_device_speed
of_usb_get_maximum_speed(struct device_node *np)
{
	return USB_SPEED_UNKNOWN;
}
#endif

#endif /* __LINUX_USB_OF_H */
