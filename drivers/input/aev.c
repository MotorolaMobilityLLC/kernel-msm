/*
* Copyright (C) 2007-2009 Motorola, Inc.
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

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/pci.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>

struct input_dev *aev_abs_dev;
static struct platform_device *aev_dev;

/*
 * c: 0 = abs, 1 = key
 * x/y: ABS_X/ABS_Y coordinates
 * b: BTN_TOUCH, 0 = release, 1 = touch, same for pressure
 */
static ssize_t write_abs(struct device *dev, struct device_attribute *attr,
	const char *buffer, size_t count)
{
	int c, x, y, b;
	sscanf(buffer, "%d %d %d %d", &c, &x, &y, &b);
	printk(KERN_DEBUG "%d %d %d %d\n", c, x, y, b);

	if (c == 0) {
		input_report_abs(aev_abs_dev, ABS_MT_TOUCH_MAJOR, 1);
		input_report_abs(aev_abs_dev, ABS_MT_WIDTH_MAJOR, 1);
		input_report_abs(aev_abs_dev, ABS_MT_POSITION_X, x);
		input_report_abs(aev_abs_dev, ABS_MT_POSITION_Y, y);
		input_mt_sync(aev_abs_dev);
	} else if (c == 1) {
		input_report_abs(aev_abs_dev, ABS_MT_TOUCH_MAJOR, b);
		input_report_abs(aev_abs_dev, ABS_MT_WIDTH_MAJOR, b);
		input_report_abs(aev_abs_dev, ABS_MT_POSITION_X, x);
		input_report_abs(aev_abs_dev, ABS_MT_POSITION_Y, y);
		input_report_key(aev_abs_dev, BTN_TOUCH, b);
		input_mt_sync(aev_abs_dev);
	}
	/* This command does not function and will be removed... */
	else if (c == 2) {
		/* input_set_abs_params(aev_abs_dev, ABS_X, 0, x, 0, 0); */
		/* input_set_abs_params(aev_abs_dev, ABS_Y, 0, y, 0, 0); */
	} else if (c == 3) {
		/* raw command (used to send events as they come) */
		int ev    = x;
		int code  = y;
		int val   = b;
		input_event(aev_abs_dev, ev, code, val);
		/* no sync command for raw command. return immediately. */
		return count;
	} else if (c > 3) {
		/*
		 * KEY_ESC happens to be 1. This is what I get for trying
		 * to squeeze this into the original keyless design without
		 * adapting it properly.
		 */
		input_report_key(aev_abs_dev, c - 3, b);
	}

	input_sync(aev_abs_dev);

	return count;
}

DEVICE_ATTR(abs, 0666, NULL, write_abs);

static struct attribute *abs_attrs[] = {
	&dev_attr_abs.attr,
	NULL
};

static struct attribute_group abs_attr_group = {
	.attrs = abs_attrs,
};

int __init aev_init(void)
{
	int ret;

	aev_dev = platform_device_register_simple("aev", -1, NULL, 0);

	if (IS_ERR(aev_dev)) {
		printk(KERN_ERR "aev_init: error\n");
		return PTR_ERR(aev_dev);
	}

	ret = sysfs_create_group(&aev_dev->dev.kobj, &abs_attr_group);
	if (ret) {
		printk(KERN_ERR "Failed to create sysfs group");
		return ret;
	}

	aev_abs_dev = input_allocate_device();
	if (!aev_abs_dev) {
		printk(KERN_ERR "Bad input_allocate_device()\n");
		return -ENOMEM;
	}

	aev_abs_dev->name = "aev_abs";
	aev_abs_dev->phys = "aev_abs/input0";
	aev_abs_dev->id.bustype = BUS_VIRTUAL;
	aev_abs_dev->id.version = 0x0100;

	set_bit(EV_KEY, aev_abs_dev->evbit);
	set_bit(EV_ABS, aev_abs_dev->evbit);

	/*
	 * Rather than specify the size of the screen, AEV uses
	 * a max x and y of 2000.  This requires AIW to scale
	 * the coordinates appropriately, but allows the code
	 * to function with any display resolution.
	 */
	/*
	input_set_abs_params(aev_abs_dev, ABS_X, 0, 2000, 0, 0);
	input_set_abs_params(aev_abs_dev, ABS_Y, 0, 2000, 0, 0);
	set_bit(ABS_X, aev_abs_dev->absbit);
	set_bit(ABS_Y, aev_abs_dev->absbit);
	*/
	/* expose multi-touch capabilities */
	input_set_abs_params(aev_abs_dev, ABS_MT_POSITION_X,  0, 2000, 0, 0);
	input_set_abs_params(aev_abs_dev, ABS_MT_POSITION_Y,  0, 2000, 0, 0);
	input_set_abs_params(aev_abs_dev, ABS_MT_TOUCH_MAJOR, 0,    8, 0, 0);
	input_set_abs_params(aev_abs_dev, ABS_MT_WIDTH_MAJOR, 0,    8, 0, 0);
	set_bit(ABS_MT_POSITION_X,  aev_abs_dev->absbit);
	set_bit(ABS_MT_POSITION_Y,  aev_abs_dev->absbit);
	set_bit(ABS_MT_TOUCH_MAJOR, aev_abs_dev->absbit);
	set_bit(ABS_MT_WIDTH_MAJOR, aev_abs_dev->absbit);

	set_bit(BTN_TOUCH, aev_abs_dev->keybit);
	set_bit(KEY_ESC, aev_abs_dev->keybit);
	set_bit(KEY_HOME, aev_abs_dev->keybit);
	set_bit(KEY_RIGHTMETA, aev_abs_dev->keybit);
	set_bit(KEY_UP, aev_abs_dev->keybit);
	set_bit(KEY_DOWN, aev_abs_dev->keybit);
	set_bit(KEY_SEARCH, aev_abs_dev->keybit);
	set_bit(KEY_RIGHT, aev_abs_dev->keybit);
	set_bit(KEY_LEFT, aev_abs_dev->keybit);

	ret = input_register_device(aev_abs_dev);
	if (ret) {
		input_free_device(aev_abs_dev);
		printk(KERN_ERR "Failed to register input device");
		return ret;
	}

	printk(KERN_DEBUG "AEV initialized\n");

	return 0;
}

void __exit aev_cleanup(void)
{
	input_unregister_device(aev_abs_dev);

	sysfs_remove_group(&aev_dev->dev.kobj, &abs_attr_group);

	platform_device_unregister(aev_dev);
}

module_init(aev_init);
module_exit(aev_cleanup);

