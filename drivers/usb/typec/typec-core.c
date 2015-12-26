/*
 * typec-core.c - Type-C connector Framework
 *
 * Copyright (C) 2015 HUAWEI, Inc.
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 * Author: HUAWEI, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/usb/typec.h>
#include <linux/power_supply.h>

static struct class *typec_class;
static struct device *typec_dev;
struct power_supply *usb_psy;

/* to get the Type-C Current mode */
static ssize_t current_detect_show(struct device *pdev,
				   struct device_attribute *attr, char *buf)
{
	struct typec_device_ops *typec_ops = dev_get_drvdata(pdev);
	enum typec_current_mode current_mode = typec_ops->current_detect();
	return snprintf(buf, PAGE_SIZE, "%d\n", current_mode);
}

/* to get the attached state and determine what was attached */
static ssize_t attached_state_show(struct device *pdev,
				   struct device_attribute *attr, char *buf)
{
	struct typec_device_ops *typec_ops = dev_get_drvdata(pdev);
	enum typec_attached_state attached_state =
	    typec_ops->attached_state_detect();
	return snprintf(buf, PAGE_SIZE, "%d\n", attached_state);
}

/* to get the current advertisement in DFP or DRP modes */
static ssize_t current_advertise_show(struct device *pdev,
				      struct device_attribute *attr, char *buf)
{
	struct typec_device_ops *typec_ops = dev_get_drvdata(pdev);
	enum typec_current_mode current_mode =
	    typec_ops->current_advertise_get();
	return snprintf(buf, PAGE_SIZE, "%d\n", current_mode);
	return 0;
}

/* to set the current advertisement in DFP or DRP modes */
static ssize_t current_advertise_store(struct device *pdev,
				       struct device_attribute *attr,
				       const char *buff, size_t size)
{
	struct typec_device_ops *typec_ops = dev_get_drvdata(pdev);
	int current_mode;

	if (sscanf(buff, "%d", &current_mode) != 1)
		return -EINVAL;

	if (current_mode >= TYPEC_CURRENT_MODE_UNSPPORTED)
		return -EINVAL;

	if (typec_ops->current_advertise_set((enum typec_current_mode)
					     current_mode))
		return -1;

	return size;
}

/* to get the port mode (UFP, DFP or DRP) */
static ssize_t port_mode_ctrl_show(struct device *pdev,
				   struct device_attribute *attr, char *buf)
{
	struct typec_device_ops *typec_ops = dev_get_drvdata(pdev);
	enum typec_port_mode port_mode = typec_ops->port_mode_get();
	return snprintf(buf, PAGE_SIZE, "%d\n", port_mode);
	return 0;
}

/* to set the port mode (UFP, DFP or DRP), the chip will operate according the mode */
static ssize_t port_mode_ctrl_store(struct device *pdev,
				    struct device_attribute *attr,
				    const char *buff, size_t size)
{
	struct typec_device_ops *typec_ops = dev_get_drvdata(pdev);
	int port_mode;

	if (sscanf(buff, "%d", &port_mode) != 1)
		return -EINVAL;

	if (port_mode > TYPEC_DRP_MODE)
		return -EINVAL;

	if (typec_ops->port_mode_set((enum typec_port_mode)port_mode))
		return -1;

	return size;
}

/* to get all the register value */
static ssize_t dump_regs_show(struct device *pdev,
			      struct device_attribute *attr, char *buf)
{
	struct typec_device_ops *typec_ops = dev_get_drvdata(pdev);
	return typec_ops->dump_regs(buf);
}

static DEVICE_ATTR(current_detect, S_IRUGO, current_detect_show, NULL);
static DEVICE_ATTR(attached_state, S_IRUGO, attached_state_show, NULL);
static DEVICE_ATTR(current_advertise, S_IRUGO | S_IWUSR, current_advertise_show,
		   current_advertise_store);
static DEVICE_ATTR(port_mode_ctrl, S_IRUGO | S_IWUSR, port_mode_ctrl_show,
		   port_mode_ctrl_store);
static DEVICE_ATTR(dump_regs, S_IRUGO, dump_regs_show, NULL);

static struct device_attribute *typec_attributes[] = {
	&dev_attr_current_detect,
	&dev_attr_attached_state,
	&dev_attr_current_advertise,
	&dev_attr_port_mode_ctrl,
	&dev_attr_dump_regs,
	NULL
};

int add_typec_device(struct device *parent, struct typec_device_ops *typec_ops)
{
	struct device *dev;
	struct device_attribute **attrs = typec_attributes;
	struct device_attribute *attr;
	int err;

	if (!typec_ops || !typec_ops->current_detect
	    || !typec_ops->attached_state_detect
	    || !typec_ops->current_advertise_get
	    || !typec_ops->current_advertise_set || !typec_ops->port_mode_get
	    || !typec_ops->port_mode_set || !typec_ops->dump_regs
	    || !typec_ops->dynamic_current_detect) {
		pr_err("%s: ops is NULL\n", __func__);
		return -1;
	}

	dev = device_create(typec_class, NULL, MKDEV(0, 0), typec_ops,
			    "typec_device");
	if (IS_ERR(dev)) {
		pr_err("%s: device_create fail\n", __func__);
		return -1;
	}

	while ((attr = *attrs++)) {
		err = device_create_file(dev, attr);
		if (err) {
			pr_err("%s: device_create_file fail\n", __func__);
			device_destroy(typec_class, dev->devt);
			return -1;
		}
	}

	typec_dev = dev;

	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		pr_err("%s USB supply not found\n", __func__);
	}

	return 0;
}

/* for charger to detect the typec current mode */
enum typec_current_mode typec_current_mode_detect(void)
{
	struct typec_device_ops *typec_ops;
	enum typec_current_mode current_mode = TYPEC_CURRENT_MODE_DEFAULT;

	if (!typec_dev) {
		pr_err("%s: no typec device registered\n", __func__);
		return current_mode;
	}

	typec_ops = dev_get_drvdata(typec_dev);
	current_mode = typec_ops->dynamic_current_detect();
	return current_mode;
}

int typec_sink_detected_handler(enum typec_event typec_event)
{
	if (!usb_psy) {
		pr_err("%s USB supply not found\n", __func__);
		return -1;
	}

	power_supply_set_usb_otg(usb_psy,
				 (typec_event == TYPEC_SINK_DETECTED) ? 1 : 0);
	return 0;
}

#define HIGH_CURRENT_UA         1800000
#define MEDIUM_CURRENT_UA       1500000
#define DEFAULT_CURRENT_UA      500000
void typec_current_changed(enum typec_current_mode current_mode)
{
	int limit = DEFAULT_CURRENT_UA;
	if (!usb_psy) {
		pr_err("%s USB supply not found\n", __func__);
	}

	switch (current_mode) {
	case TYPEC_CURRENT_MODE_MID:
		limit = MEDIUM_CURRENT_UA;
		break;
	case TYPEC_CURRENT_MODE_HIGH:
		limit = HIGH_CURRENT_UA;
		break;
	default:
		return;
	}

	power_supply_set_current_limit(usb_psy, limit);
}

static int __init typec_init(void)
{
	typec_class = class_create(THIS_MODULE, "typec");
	if (IS_ERR(typec_class)) {
		pr_err("failed to create typec class --> %ld\n",
		       PTR_ERR(typec_class));
		return PTR_ERR(typec_class);
	}

	return 0;
}

subsys_initcall(typec_init);

static void __exit typec_exit(void)
{
	class_destroy(typec_class);
}

module_exit(typec_exit);

MODULE_AUTHOR("HUAWEI");
MODULE_DESCRIPTION("Type-C connector Framework");
MODULE_LICENSE("GPL v2");
