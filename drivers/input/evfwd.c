/*
* Copyright (C) 2007-2011 Motorola, Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
* 02111-1307, USA
*/

/* evfwd x 3 */

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/pci.h>
#include <linux/input.h>
#include <linux/platform_device.h>

#define DEVICE_NAME "evfwd"

/* --------------------------------------------------------------------
 * Mouse device
 * ------------------------------------------------------------------*/
#define DEVICE_NAME_REL DEVICE_NAME "rel"

static struct platform_device *evfwdrel_dev;	/* platform device */
static struct input_dev *evfwdrel_input_dev;	/* mouse device */

static int status_evfwdrel;

int start_evfwdrel(void)
{
	int ret;

	if (status_evfwdrel == 1)
		return 2;

	/* Allocate an input device data structure */
	evfwdrel_input_dev = input_allocate_device();
	if (!evfwdrel_input_dev) {
		printk(KERN_ERR "Bad input_allocate_device()"
				" evfwdrel\n");
		return 0;
	}

	/* same name for all evfwd devices is intentional */
	evfwdrel_input_dev->name = DEVICE_NAME;

	/* Announce that the device will generate relative coordinates */
	set_bit(EV_REL,			evfwdrel_input_dev->evbit);
	set_bit(REL_X,			evfwdrel_input_dev->relbit);
	set_bit(REL_Y,			evfwdrel_input_dev->relbit);
	set_bit(REL_WHEEL,	evfwdrel_input_dev->relbit);
	set_bit(REL_HWHEEL, evfwdrel_input_dev->relbit);

	/* set key codes for mouse buttons */
	set_bit(EV_KEY,			evfwdrel_input_dev->evbit);
	set_bit(BTN_0,			evfwdrel_input_dev->keybit);
	set_bit(BTN_MOUSE,	evfwdrel_input_dev->keybit);
	set_bit(BTN_LEFT,	 evfwdrel_input_dev->keybit);
	set_bit(BTN_RIGHT,	evfwdrel_input_dev->keybit);
	set_bit(BTN_MIDDLE, evfwdrel_input_dev->keybit);

	/* Register with the input subsystem */
	ret = input_register_device(evfwdrel_input_dev);
	if (ret) {
		input_free_device(evfwdrel_input_dev);
		printk(KERN_ERR "Failed to register device"
				"(evfwdrel err=%d)", ret);
		return 0;
	}

	status_evfwdrel = 1;
	return 1;
}

int stop_evfwdrel(void)
{
	if (status_evfwdrel == 0)
		return 2;

	/* Unregister the device from the input subsystem */
	input_unregister_device(evfwdrel_input_dev);

	status_evfwdrel = 0;
	return 1;
}

/* Sysfs method to input simulated relative events */
static ssize_t write_evfwdrel(struct device *dev,
				struct device_attribute *attr,
				const char *buffer,
				size_t count)
{
	struct input_event inev;

	if ((count % sizeof(inev)) == 0) {
		int i;
		for (i = 0; i < count; i += sizeof(inev)) {
			memcpy((char *)&inev, buffer+i, sizeof(inev));
			switch (inev.type) {
			case EV_KEY:
			case EV_REL:
				if (status_evfwdrel != 0)
					input_event(evfwdrel_input_dev,
						inev.type,
						inev.code,
						inev.value);
				break;
			case EV_SYN:
				if (inev.code == SYN_CONFIG) {
					if (inev.value != 0)
						start_evfwdrel();
					else
						stop_evfwdrel();
				} else if (status_evfwdrel != 0)
					input_event(evfwdrel_input_dev,
						inev.type,
						inev.code,
						inev.value);
				break;
			default:
				printk(KERN_ERR "Bad event type (%d)\n",
						inev.type);
			}
		}
	} else
		printk(KERN_ERR "Bad event block size (%d)\n", count);


	return count;
}

/* Attach the sysfs write method for mouse */
DEVICE_ATTR(rel, 0666, NULL, write_evfwdrel);

/* Attribute Descriptor */
static struct attribute *evfwdrel_attrs[] = {
	&dev_attr_rel.attr,
	NULL
};

/* Attribute group */
static struct attribute_group evfwdrel_attr_group = {
	.attrs = evfwdrel_attrs,
};

int __init evfwdrel_init(void)
{
	int ret;

	/* Register a platform device */
	evfwdrel_dev = platform_device_register_simple(
					DEVICE_NAME_REL, -1, NULL, 0);
	if (IS_ERR(evfwdrel_dev)) {
		printk(KERN_ERR "evfwdrel_init: error\n");
		return PTR_ERR(evfwdrel_dev);
	}

	/* Create a sysfs node to control the device */
	ret = sysfs_create_group(&evfwdrel_dev->dev.kobj,
				 &evfwdrel_attr_group);
	if (ret) {
		printk(KERN_ERR "Failed to create sysfs group"
				" for evfwdrel");
		return ret;
	}

	pr_debug("'%s' Initialized.\n", DEVICE_NAME_REL);
	return 0;
}

void evfwdrel_cleanup(void)
{
	/* unregister and release relative coords device */
	stop_evfwdrel();

	/* Cleanup sysfs nodes */
	sysfs_remove_group(&evfwdrel_dev->dev.kobj,
				&evfwdrel_attr_group);

	/* Unregister platform device */
	platform_device_unregister(evfwdrel_dev);

	pr_debug("'%s' unloaded (rel).\n", DEVICE_NAME);

	return;
}

/* --------------------------------------------------------------------
 * Kbd device
 * -------------------------------------------------------------------*/
#define DEVICE_NAME_KBD DEVICE_NAME "kbd"

static struct platform_device *evfwdkbd_dev;	/* platform device */
static struct input_dev *evfwdkbd_input_dev;	/* keyboard device */

static int status_evfwdkbd;

int start_evfwdkbd(void)
{
	int ret, i;

	if (status_evfwdkbd == 1)
		return 2;

	/* Allocate an input device data structure */
	evfwdkbd_input_dev = input_allocate_device();
	if (!evfwdkbd_input_dev) {
		printk(KERN_ERR "Bad input_allocate_device()"
				" for evfwdkbd\n");
		return -ENOMEM;
	}

	/* same name for all evfwd devices is intentional */
	evfwdkbd_input_dev->name = DEVICE_NAME;

	/* Announce the device will generate key and scan codes */
	set_bit(EV_KEY, evfwdkbd_input_dev->evbit);

	/* set key codes for keyboard (all key codes 1..255) */
	for (i = KEY_ESC; i < BTN_MISC; i++)
		set_bit(i, evfwdkbd_input_dev->keybit);

	/* this device doesn't handle reasigning keys so it doesn't send
	 * KEY_UNKNOWN code */
	clear_bit(KEY_UNKNOWN, evfwdkbd_input_dev->keybit);

	/* Register with the input subsystem */
	ret = input_register_device(evfwdkbd_input_dev);
	if (ret) {
		input_free_device(evfwdkbd_input_dev);
		printk(KERN_ERR "Failed to register device"
				" (evfwdkbd err=%d)", ret);
		return ret;
	}

	status_evfwdkbd = 1;
	return 1;
}

int stop_evfwdkbd(void)
{
	if (status_evfwdkbd == 0)
		return 2;

	/* Unregister the device from the input subsystem */
	input_unregister_device(evfwdkbd_input_dev);

	status_evfwdkbd = 0;
	return 1;
}

/* Sysfs method to input simulated kbd events to the event forwarder */
static ssize_t write_evfwdkbd(struct device *dev,
		struct device_attribute *attr,
		const char *buffer, size_t count)
{
	struct input_event inev;

	if ((count % sizeof(inev)) == 0) {
		int i;
		for (i = 0; i < count; i += sizeof(inev)) {
			memcpy((char *)&inev, buffer+i, sizeof(inev));
			switch (inev.type) {
			case EV_KEY:
				if ((inev.code != KEY_UNKNOWN) &&
				(status_evfwdkbd != 0)) {
					input_event(evfwdkbd_input_dev,
						inev.type,
						inev.code,
						inev.value);
				}
				break;
			case EV_SYN:
				if (inev.code == SYN_CONFIG) {
					if (inev.value != 0)
						start_evfwdkbd();
					else
						stop_evfwdkbd();
				} else if (status_evfwdkbd != 0) {
					input_event(evfwdkbd_input_dev,
						inev.type,
						inev.code,
						inev.value);
				}
				break;
			default:
				printk(KERN_ERR "Bad event type (%d)\n",
						inev.type);
			}
		}
	} else
		printk(KERN_ERR "Bad event block size (%d)\n", count);

	return count;
}

/* Attach the sysfs write method for keyboard events */
DEVICE_ATTR(kbd, 0644, NULL, write_evfwdkbd);

/* Attribute Descriptor */
static struct attribute *evfwdkbd_attrs[] = {
	&dev_attr_kbd.attr,
	NULL
};

/* Attribute group */
static struct attribute_group evfwdkbd_attr_group = {
	.attrs = evfwdkbd_attrs,
};

int __init evfwdkbd_init(void)
{
	int ret;

	/* Register a platform device */
	evfwdkbd_dev =
		platform_device_register_simple(DEVICE_NAME_KBD, -1,
						NULL, 0);
	if (IS_ERR(evfwdkbd_dev)) {
		printk(KERN_ERR "evfwdkbd_init: error\n");
		return PTR_ERR(evfwdkbd_dev);
	}

	/* Create a sysfs node to read simulated coordinates */
	ret = sysfs_create_group(&evfwdkbd_dev->dev.kobj,
				  &evfwdkbd_attr_group);
	if (ret) {
		printk(KERN_ERR "Failed to create sysfs group"
				" for evfwdkbd");
		return ret;
	}

	pr_debug("'%s' Initialized.\n", DEVICE_NAME_KBD);
	return 0;
}

void evfwdkbd_cleanup(void)
{
	/* unregister kbd device and release struct */
	stop_evfwdkbd();

	/* Cleanup sysfs nodes */
	sysfs_remove_group(&evfwdkbd_dev->dev.kobj,
				&evfwdkbd_attr_group);

	/* Unregister devices from platform */
	platform_device_unregister(evfwdkbd_dev);

	pr_debug("'%s' unloaded.\n", DEVICE_NAME_KBD);

	return;
}

/* --------------------------------------------------------------------
 * Touchpad device
 * -------------------------------------------------------------------*/
#define DEVICE_NAME_ABS DEVICE_NAME "abs"

static struct platform_device *evfwdabs_dev;	/* platform touchpad */
static struct input_dev *evfwdabs_input_dev;	/* device touchpad */
static int status_evfwdabs;

int start_evfwdabs(void)
{
	int ret;

	if (status_evfwdabs == 1)
		return 2;

	/* Allocate an input device data structure */
	evfwdabs_input_dev = input_allocate_device();
	if (!evfwdabs_input_dev) {
		printk(KERN_ERR "Bad input_allocate_device()"
				" for evfwdabs\n");
		return -ENOMEM;
	}

	/* Allocate an input device data structure */
	/* same name for all evfwd devices is intentional */
	evfwdabs_input_dev->name = DEVICE_NAME;

	/* needed ?
	evfwdabs_input_dev->phys = "aev_abs/input0";
	evfwdabs_input_dev->id.bustype = BUS_VIRTUAL;
	evfwdabs_input_dev->id.version = 0x0100;
	*/

	/* Expose multi-touch capabilities.
	 * Rather than specify the size of the screen, evfwd uses
	 * a max x and y of 2000.	This requires clients to scale
	 * the coordinates appropriately, but allows the code
	 * to function with any display resolution.
	 */
	set_bit(EV_ABS, evfwdabs_input_dev->evbit);
	input_set_abs_params(evfwdabs_input_dev, ABS_MT_POSITION_X,
				0, 2000, 0, 0);
	input_set_abs_params(evfwdabs_input_dev, ABS_MT_POSITION_Y,
				0, 2000, 0, 0);
	input_set_abs_params(evfwdabs_input_dev, ABS_MT_TOUCH_MAJOR,
				0, 8, 0, 0);
	input_set_abs_params(evfwdabs_input_dev, ABS_MT_PRESSURE,
				0, 255, 0, 0);
	input_set_abs_params(evfwdabs_input_dev, ABS_MT_TRACKING_ID,
				0, 4, 0, 0);

	set_bit(ABS_MT_POSITION_X,  evfwdabs_input_dev->absbit);
	set_bit(ABS_MT_POSITION_Y,  evfwdabs_input_dev->absbit);
	set_bit(ABS_MT_TOUCH_MAJOR, evfwdabs_input_dev->absbit);
	set_bit(ABS_MT_WIDTH_MAJOR, evfwdabs_input_dev->absbit);
	set_bit(ABS_MT_PRESSURE, evfwdabs_input_dev->absbit);
	set_bit(ABS_MT_TRACKING_ID, evfwdabs_input_dev->absbit);

	set_bit(EV_KEY, evfwdabs_input_dev->evbit);
	set_bit(BTN_TOUCH, evfwdabs_input_dev->keybit);

	/* Register with the input subsystem */
	ret = input_register_device(evfwdabs_input_dev);
	if (ret) {
		input_free_device(evfwdabs_input_dev);
		printk(KERN_ERR "Failed to register device"
				" (evfwdabs err=%d)", ret);
		return ret;
	}

	status_evfwdabs = 1;
	return 1;
}

int stop_evfwdabs(void)
{
	if (status_evfwdabs == 0)
		return 2;

	/* Unregister the device from the input subsystem */
	input_unregister_device(evfwdabs_input_dev);

	status_evfwdabs = 0;
	return 1;
}

/* Sysfs method to input simulated abs events to the event forwarder */
static ssize_t write_evfwdabs(struct device *dev,
		struct device_attribute *attr,
		const char *buffer, size_t count)
{
	struct input_event inev;

	if ((count % sizeof(inev)) == 0) {
		int i;
		for (i = 0; i < count; i += sizeof(inev)) {
			memcpy((char *)&inev, buffer+i, sizeof(inev));
			if (inev.type == EV_SYN &&
			inev.code == SYN_CONFIG)
				if (inev.value != 0)
					start_evfwdabs();
				else
					stop_evfwdabs();
			else if (status_evfwdabs != 0)
				input_event(evfwdabs_input_dev,
						inev.type,
						inev.code,
						inev.value);
		}
	} else
		printk(KERN_ERR "Bad event block size (%d)\n", count);

	return count;
}

/* Attach the sysfs write method for keyboard events */
static DEVICE_ATTR(abs, 0644, NULL, write_evfwdabs);

/* Attribute Descriptor */
static struct attribute *evfwdabs_attrs[] = {
	&dev_attr_abs.attr,
	NULL
};

/* Attribute group */
static struct attribute_group evfwdabs_attr_group = {
	.attrs = evfwdabs_attrs,
};

int __init evfwdabs_init(void)
{
	int ret;

	/* Register a platform device */
	evfwdabs_dev =
		platform_device_register_simple(DEVICE_NAME_ABS, -1,
						NULL, 0);
	if (IS_ERR(evfwdabs_dev)) {
		printk(KERN_ERR "evfwdabs_init: error\n");
		return PTR_ERR(evfwdabs_dev);
	}

	/* Create a sysfs node to read simulated coordinates */
	ret = sysfs_create_group(&evfwdabs_dev->dev.kobj,
				  &evfwdabs_attr_group);
	if (ret) {
		printk(KERN_ERR "Failed to create sysfs group"
				" for evfwdabs");
		return ret;
	}

	pr_debug("'%s' Initialized.\n", DEVICE_NAME_ABS);
	return 0;
}

void evfwdabs_cleanup(void)
{
	/* unregister and release abs device */
	stop_evfwdabs();

	/* Cleanup sysfs nodes */
	sysfs_remove_group(&evfwdabs_dev->dev.kobj,
				&evfwdabs_attr_group);

	/* Unregister devices from platform */
	platform_device_unregister(evfwdabs_dev);

	pr_debug("'%s' unloaded.\n", DEVICE_NAME);

	return;
}

/* --------------------------------------------------------------------
 * Driver stuff
 * -------------------------------------------------------------------*/

/* Driver Initialization */
int __init evfwd_init(void)
{
	int ret;

	ret = evfwdrel_init();
	if (ret != 0)
		return ret;

	ret = evfwdkbd_init();
	if (ret != 0) {
		evfwdrel_cleanup();
		return ret;
	}

	ret = evfwdabs_init();
	if (ret != 0) {
		evfwdrel_cleanup();
		evfwdkbd_cleanup();
	}

	return ret;
}

/* Driver Exit */
void evfwd_cleanup(void)
{
	evfwdrel_cleanup();
	evfwdkbd_cleanup();
	evfwdabs_cleanup();
}

module_init(evfwd_init);
module_exit(evfwd_cleanup);

