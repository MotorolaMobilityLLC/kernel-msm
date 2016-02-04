/*
 * Copyright (C) 2016 Motorola Mobility, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>

#include <linux/mods/modbus_ext.h>

static DEFINE_MUTEX(modbus_ext_mutex);

enum {
	MODBUS_GPIO_SEL0,
	MODBUS_GPIO_SEL1,
	MODBUS_MAX_GPIOS,
};

enum {
	MODBUS_STATE_USB_SS,
	MODBUS_STATE_MPHY,
	MODBUS_STATE_I2S,
	MODBUS_STATE_HIGH_Z,
	MODBUS_STATE_INVALID,
};

struct modbus_data {
	int sel_gpios[MODBUS_MAX_GPIOS];
	const char *sel_gpio_labels[MODBUS_MAX_GPIOS];
	struct device *dev;
	int state;
};

struct modbus_data *modbus_dd;

#define MODBUS_STATE_MASK(bit) (1 << bit)

#define MODBUS_SEL0(state) (state & BIT(0))
#define MODBUS_SEL1(state) ((state & BIT(1)) >> 1UL)

static int modbus_set_switch_state(int state, struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct modbus_data *data = platform_get_drvdata(pdev);

	if (state >= MODBUS_STATE_INVALID) {
		dev_err(dev, "Invalid switch state: %d\n", state);
		return -EINVAL;
	}

	gpio_set_value(data->sel_gpios[MODBUS_GPIO_SEL0], MODBUS_SEL0(state));
	gpio_set_value(data->sel_gpios[MODBUS_GPIO_SEL1], MODBUS_SEL1(state));

	return 0;
}

static void modbus_select_state(struct modbus_data *data)
{
	/* Check the protocol state in order of priority; once the highest
	 * prioritized protocol is found, set the state and we're done.
	 */
	if (data->state & MODBUS_STATE_MASK(MODBUS_PROTO_MPHY)) {
		modbus_set_switch_state(MODBUS_STATE_MPHY, data->dev);
		return;
	}

	if (data->state & MODBUS_STATE_MASK(MODBUS_PROTO_USB_SS)) {
		modbus_set_switch_state(MODBUS_STATE_USB_SS, data->dev);
		return;
	}

	if (data->state & MODBUS_STATE_MASK(MODBUS_PROTO_I2S)) {
		modbus_set_switch_state(MODBUS_STATE_I2S, data->dev);
		return;
	}

	/* All others, we place in high-Z */
	modbus_set_switch_state(MODBUS_STATE_HIGH_Z, data->dev);
}

void modbus_ext_set_state(const struct modbus_ext_status *status)
{
	if (!modbus_dd)
		return;

	if (status->proto >= MODBUS_PROTO_INVALID)
		return;

	mutex_lock(&modbus_ext_mutex);
	if (status->active)
		modbus_dd->state |= MODBUS_STATE_MASK(status->proto);
	else
		modbus_dd->state &= ~MODBUS_STATE_MASK(status->proto);

	modbus_select_state(modbus_dd);

	mutex_unlock(&modbus_ext_mutex);
}
EXPORT_SYMBOL(modbus_ext_set_state);

static int modbus_of_get_gpios(struct modbus_data *data, struct device *dev)
{
	int ret;
	struct device_node *np = dev->of_node;
	int gpio_cnt;
	int i;

	if (!np) {
		dev_err(dev, "No OF node found!");
		return -EINVAL;
	}

	gpio_cnt = of_gpio_count(np);
	if (gpio_cnt != ARRAY_SIZE(data->sel_gpios)) {
		dev_err(dev, "gpio count is %d; expected %zu\n",
			gpio_cnt, ARRAY_SIZE(data->sel_gpios));
		return -EINVAL;
	}

	for (i = 0; i < gpio_cnt; i++) {
		enum of_gpio_flags flags = 0;
		int gpio;
		const char *name = "mmi,mod-switch-gpio-labels";
		const char *label = NULL;

		gpio = of_get_gpio_flags(np, i, &flags);
		if (!gpio_is_valid(gpio)) {
			dev_err(dev, "of_get_gpio failed %d index: %d\n",
				gpio, i);
			ret = -EINVAL;
			goto free_gpios;
		}

		ret = of_property_read_string_index(np, name, i, &label);
		if (ret) {
			dev_err(dev, "reading label failed: %d index: %d\n",
				ret, i);
			goto free_gpios;
		}

		ret = devm_gpio_request_one(dev, gpio, flags, label);
		if (ret) {
			dev_err(dev, "Failed to get gpio: %d index: %d\n",
				gpio, i);
			goto free_gpios;
		}

		ret = gpio_export(gpio, true);
		if (ret) {
			dev_err(dev, "Failed to export gpio: %d index: %d\n",
				gpio, i);
			goto free_gpios;
		}

		ret = gpio_export_link(dev, label, gpio);
		if (ret) {
			dev_err(dev, "Failed to link gpio: %d index: %d\n",
				gpio, i);
			gpio_unexport(gpio);
			goto free_gpios;
		}

		dev_dbg(dev, "gpio=%d flags=0x%x label=%s\n",
			gpio, flags, label);

		data->sel_gpios[i] = gpio;
		data->sel_gpio_labels[i] = label;
	}

	return 0;

free_gpios:
	for (--i; i >= 0; --i) {
		sysfs_remove_link(&dev->kobj, data->sel_gpio_labels[i]);
		gpio_unexport(data->sel_gpios[i]);
	}

	return ret;
}

static void modbus_gpio_free(struct modbus_data *data, struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(data->sel_gpios); i++) {
		sysfs_remove_link(&dev->kobj, data->sel_gpio_labels[i]);
		gpio_unexport(data->sel_gpios[i]);
	}
}

static int modbus_probe(struct platform_device *pdev)
{
	struct modbus_data *data;
	int ret;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	ret = modbus_of_get_gpios(data, &pdev->dev);
	if (ret)
		return ret;

	data->dev = &pdev->dev;
	platform_set_drvdata(pdev, data);
	modbus_dd = data;

	/* Configure to HighZ initially */
	modbus_set_switch_state(MODBUS_STATE_HIGH_Z, data->dev);

	return 0;
}

static int modbus_remove(struct platform_device *pdev)
{
	struct modbus_data *data = platform_get_drvdata(pdev);

	modbus_gpio_free(data, &pdev->dev);

	return 0;
}

static const struct of_device_id modbus_match_tbl[] = {
	{ .compatible = "mmi,modbus_switch" },
	{ },
};
MODULE_DEVICE_TABLE(of, modbus_match_tbl);

static struct platform_driver modbus_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "modbus_switch",
		.of_match_table = modbus_match_tbl,
	},
	.probe = modbus_probe,
	.remove = modbus_remove,
};

static int __init modbus_init(void)
{
	int ret;

	ret = platform_driver_register(&modbus_driver);
	if (ret) {
		pr_err("modbus failed to register driver\n");
		return ret;
	}

	return 0;
}

static void __exit modbus_exit(void)
{
	platform_driver_unregister(&modbus_driver);
}

module_init(modbus_init);
module_exit(modbus_exit);

MODULE_DESCRIPTION("MODS Bus Switch");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL");
