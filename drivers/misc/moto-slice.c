/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/i2c/at24.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/workqueue.h>
#include <linux/switch.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_i2c.h>


#define ACCYS { \
	DEF(NONE), \
	DEF(JUICE), \
	DEF(SHIELD), \
	DEF(FLUX), \
	DEF(MAX) }

#define DEF(a)	ACCY_##a
enum slice_det_accy ACCYS;
#undef DEF

#define DEF(a)	#a
static char *accy_names[] = ACCYS;
#undef DEF

static int connected;
static BLOCKING_NOTIFIER_HEAD(slice_notifier_list);

/**
 * register_slice_accessory_notify - register accessory notifier callback
 * @nb: pointer to the notifier block for the callback events.
 *
 * Calls the notifier callback to when accessory is connected or removed,
 * indicated by a boolean passed to the callback's action parameter.
 */
void register_slice_accessory_notify(struct notifier_block *nb)
{
	if (!nb)
		return;

	/* inform new client of current state */
	if (nb->notifier_call)
		nb->notifier_call(nb, connected, NULL);

	blocking_notifier_chain_register(&slice_notifier_list, nb);
}
EXPORT_SYMBOL(register_slice_accessory_notify);

/**
 * unregister_slice_accessory_notify - unregister a notifier callback
 * @nb: pointer to the notifier block for the callback events.
 *
 * register_slice_accessory_notify() must have been previously called for this
 * function to work properly.
 */
void unregister_slice_accessory_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_unregister(&slice_notifier_list, nb);
}
EXPORT_SYMBOL(unregister_slice_accessory_notify);

struct at24_data {
	struct at24_platform_data chip;
	struct memory_accessor macc;
};
struct eeprom_data {
	char     company[9];
	uint8_t  type;
	char     name[14];
	uint32_t serial;
	uint8_t  revision;
	uint8_t  b_cell_rev;
	uint8_t  b_date_yy;
	uint8_t  b_date_mm;
	uint8_t  b_date_dd;
	uint8_t  b_data_on;
	uint8_t  b_data[221];
};

struct slice_accessory {
	struct device		*dev;
	struct work_struct	slice_work;
	struct switch_dev	swdev;
	int			slice_detect;
	int			state;
	struct platform_device	*i2c_bus_pdev;
	struct eeprom_data	eeprom;
	struct i2c_adapter	*i2c_adapter;
};

void read_eeprom(struct memory_accessor *mem_acc, void *context)
{
	struct slice_accessory *slice = context;
	int ret;

	if (!slice)
		return;

	ret = mem_acc->read(mem_acc, (char *) &slice->eeprom, 0,
				sizeof(struct eeprom_data));
	if (ret == sizeof(struct eeprom_data))
		slice->state = slice->eeprom.type;
	else
		dev_err(slice->dev, "Slice EEPROM Read Failed\n");
}

static struct at24_platform_data br24g02 = {
	.byte_len	= 256,
	.page_size	= 1,
	.setup		= read_eeprom,
};

static struct i2c_board_info slice_i2c_board_info = {
	/* Slice EEPROM */
	I2C_BOARD_INFO("24c02", 0x50),
	.platform_data	= &br24g02,
};

static int slice_unregister_client(struct device *dev, void *dummy)
{
	struct i2c_client *client = i2c_verify_client(dev);
	if (client && strcmp(client->name, "dummy"))
		i2c_unregister_device(client);
	return 0;
}




static int slice_try_eeprom(struct slice_accessory *slice)
{
	struct i2c_client	*client;

	if (!slice->i2c_adapter)
		return -ENODEV;

	client = i2c_new_device(slice->i2c_adapter, &slice_i2c_board_info);

	if (!client) {
		dev_err(slice->dev, "unable add eeprom i2c driver\n");
		return -ENODEV;
	}

	return 0;
}

static void slice_register_i2c_devices(struct slice_accessory *slice)
{
	switch (slice->eeprom.type) {
	case ACCY_JUICE:
		dev_info(slice->dev, "Juice: register i2c bus %d devices\n",
					slice->i2c_bus_pdev->id);
		of_i2c_register_devices(slice->i2c_adapter);
		break;
	case ACCY_SHIELD:
	case ACCY_FLUX:
	default:
		break;
	}
}

static void slice_detected_work(struct work_struct *w)
{
	struct slice_accessory *slice = container_of(w, struct slice_accessory,
						 slice_work);
	int value;

	value = gpio_get_value(slice->slice_detect);

	if ((slice->state < 0) || (value != !slice->state)) {
		if (!value) {
			if (slice_try_eeprom(slice)) {
				dev_err(slice->dev, "Can't find slice eeprom\n");
				/* Allow system suspend */
				pm_relax(slice->dev);
				return;
			}
			slice_register_i2c_devices(slice);
			switch_set_state(&slice->swdev, slice->state);
			dev_info(slice->dev, "Detected %s S/N: %d\n",
				slice->eeprom.name, slice->eeprom.serial);
		} else {
			switch_set_state(&slice->swdev, 0);
			slice->state = 0;

			dev_info(slice->dev, "Slice accessory disconnected\n");

			if (slice->i2c_adapter)
				device_for_each_child(&slice->i2c_adapter->dev,
						NULL, slice_unregister_client);
		}

		if (connected != slice->state) {
			connected = slice->state;
			blocking_notifier_call_chain(&slice_notifier_list,
							connected, NULL);
		}
	}
	/* Allow system suspend */
	pm_relax(slice->dev);
}

static irqreturn_t slice_detected(int irq, void *data)
{
	struct slice_accessory *slice = data;

	/* Ensure suspend can't happen until after work function commpletes */
	pm_stay_awake(slice->dev);
	schedule_work(&slice->slice_work);
	return IRQ_HANDLED;
}

static const char *slice_print_accy_name(enum slice_det_accy accy)
{
	return accy < ACCY_MAX ? accy_names[accy] : "NA";
}

static ssize_t slice_print_accessory_name(struct switch_dev *switch_dev,
						char *buf)
{
	return snprintf(buf, 16, "%s\n",
		slice_print_accy_name(switch_get_state(switch_dev)));
}

static ssize_t slice_serial_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct switch_dev *swdev = (struct switch_dev *)
				 dev_get_drvdata(dev);
	struct slice_accessory *slice =
			container_of(swdev, struct slice_accessory, swdev);

	return snprintf(buf, 16, "%d\n",
			slice->state ? slice->eeprom.serial : 0);
}

static ssize_t slice_eeprom_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct switch_dev *swdev = (struct switch_dev *)
				 dev_get_drvdata(dev);
	struct slice_accessory *slice =
			container_of(swdev, struct slice_accessory, swdev);
	ssize_t ret;

	if (slice->state > 0)
		ret = snprintf(buf, 128, "Company: %s\n"
				"Type: %d\n"
				"Name: %s\n"
				"Serial: %d\n"
				"Rev: %d\n"
				"Cell Rev: %d\n"
				"Cell Date: %d/%d/%d\n"
				"Battery Data: %s\n",
				slice->eeprom.company, slice->eeprom.type,
				slice->eeprom.name, slice->eeprom.serial,
				slice->eeprom.revision,
				slice->eeprom.b_cell_rev,
				slice->eeprom.b_date_mm,
				slice->eeprom.b_date_dd,
				slice->eeprom.b_date_yy,
				slice->eeprom.b_data_on ? "Yes" : "No");
	else
		ret = snprintf(buf, 5, "N/A\n");

	return ret;
}

static DEVICE_ATTR(serial, S_IRUGO , slice_serial_show, NULL);
static DEVICE_ATTR(eeprom, S_IRUGO , slice_eeprom_show, NULL);

static int slice_accessory_probe(struct platform_device *pdev)
{
	struct device_node	*node = pdev->dev.of_node;
	struct slice_accessory	*slice;
	struct device_node	*i2c_bus_node;
	struct platform_device	*i2c_pdev;
	int ret;

	i2c_bus_node = of_parse_phandle(node, "mmi,i2c_bus", 0);
	if (!i2c_bus_node) {
		dev_err(&pdev->dev, "unable to get i2c_bus node\n");
		return -ENODEV;
	}

	i2c_pdev = of_find_device_by_node(i2c_bus_node);
	if (!i2c_pdev || !i2c_pdev->dev.driver) {
		dev_err(&pdev->dev, "i2c_bus %s pdev is not ready, try later\n",
					i2c_bus_node->name);
		of_node_put(i2c_bus_node);
		return -EPROBE_DEFER;
	}

	of_node_put(i2c_bus_node);

	slice = devm_kzalloc(&pdev->dev, sizeof(*slice), GFP_KERNEL);
	if (!slice)
		return -ENOMEM;

	slice->dev = &pdev->dev;
	slice->i2c_bus_pdev = i2c_pdev;

	platform_set_drvdata(pdev, slice);

	INIT_WORK(&slice->slice_work, slice_detected_work);

	slice->slice_detect = of_get_named_gpio(node,
					"mmi,slice-detect-gpio", 0);
	if (slice->slice_detect < 0) {
		dev_err(slice->dev, "unable to get slice-detect-gpio\n");
		return slice->slice_detect;
	}

	ret = devm_gpio_request(slice->dev,
				slice->slice_detect, "slice_detect");
	if (ret)
		return ret;
	gpio_export(slice->slice_detect, 1);

	ret = devm_request_irq(&pdev->dev, gpio_to_irq(slice->slice_detect),
				slice_detected, IRQF_TRIGGER_RISING |
				IRQF_TRIGGER_FALLING | IRQF_SHARED,
				"slice_detect_irq", slice);
	if (ret)
		return ret;



	slice->swdev.name = "slice";
	slice->swdev.print_name = slice_print_accessory_name;
	ret = switch_dev_register(&slice->swdev);
	if (ret) {
		dev_err(slice->dev, "couldn't register switch (%s) rc=%d\n",
					slice->swdev.name, ret);
		return -ENODEV;
	}

	ret = device_create_file(slice->swdev.dev, &dev_attr_serial);
	if (ret < 0)
		dev_err(slice->dev, "couldn't create 'serial' (%s) rc=%d\n",
					slice->swdev.name, ret);

	ret = device_create_file(slice->swdev.dev, &dev_attr_eeprom);
	if (ret < 0)
		dev_err(slice->dev, "couldn't create 'eeprom' (%s) rc=%d\n",
					slice->swdev.name, ret);

	/* Set EEPROM driver context data */
	br24g02.context = slice;
	/* Schedule detection to check if a slice is already attached */
	slice->state = -1;

	slice->i2c_adapter = i2c_get_adapter(slice->i2c_bus_pdev->id);
	if (!slice->i2c_adapter) {
		dev_err(slice->dev, "unable to get i2c adapter %d\n",
					slice->i2c_bus_pdev->id);
		return -ENODEV;
	}
	schedule_work(&slice->slice_work);
	device_init_wakeup(slice->dev, true);
	enable_irq_wake(gpio_to_irq(slice->slice_detect));

	return 0;
}

static int slice_accessory_remove(struct platform_device *pdev)
{
	struct slice_accessory *slice = platform_get_drvdata(pdev);
	switch_dev_unregister(&slice->swdev);
	disable_irq_wake(gpio_to_irq(slice->slice_detect));
	cancel_work_sync(&slice->slice_work);
	platform_device_put(slice->i2c_bus_pdev);

	return 0;
}

static struct of_device_id of_match_table[] = {
	{ .compatible = "mmi,slice-accessory", },
	{ },
};

static struct platform_driver slice_accessory_driver = {
	.driver         = {
		.name   = "mmi-slice-driver",
		.of_match_table = of_match_table,
	},
	.probe          = slice_accessory_probe,
	.remove		= slice_accessory_remove,
};

module_platform_driver(slice_accessory_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Motorola Slice Accessory driver");
