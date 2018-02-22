/*
 * Copyright (C) 2017 Motorola Mobility, Inc.
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
#include <linux/regulator/consumer.h>
#include <linux/mods/modbus_ext.h>

static DEFINE_MUTEX(modbus_ext_mutex);

/*mods swich cross bar's select gpios normally less than 4*/
#define MODBUS_MAX_GPIOS		4
/*mods swich cross bar's select states normally less than 4*/
#define MODBUS_MAX_PROTOCOLS		4

/*convert between state enum and bitmask*/
#define MODBUS_PROTO_MASK(bit) (1 << bit)

/*need to be adjusted by enum modbus_ext_protocol(modbus_ext.h)*/
const static char *protocol_list[] = {
							"usbss",
							"mphy",
							"i2s"
							};

struct modbus_seq {
	u32 val[MODBUS_MAX_GPIOS * MODBUS_MAX_PROTOCOLS];
	u32 len;
	char *lable;
};

struct modbus_data {
	int sel_gpios[MODBUS_MAX_GPIOS];
	const char *sel_gpio_labels[MODBUS_MAX_GPIOS];
	struct device *dev;
	int state;
	int gpio_nums;
	struct modbus_seq modbus_switch_seqs;
	const char *modbus_protocol_labels[MODBUS_MAX_PROTOCOLS];
	struct regulator *vdd_switch;
};

struct modbus_data *modbus_dd;

static int modbus_set_switch_state(struct modbus_data *data)
{
	int state = MODBUS_PROTO_INVALID; /*why don't we have hi-Z anymore?*/
	int i, index;
	int count;

	if (data->state & MODBUS_PROTO_MASK(MODBUS_PROTO_MPHY)) {
		state = MODBUS_PROTO_MPHY;
	} else if (data->state & MODBUS_PROTO_MASK(MODBUS_PROTO_USB_SS)) {
		state = MODBUS_PROTO_USB_SS;
	} else if (data->state & MODBUS_PROTO_MASK(MODBUS_PROTO_I2S)) {
		state = MODBUS_PROTO_I2S;
	}

	if (state >= MODBUS_PROTO_INVALID) {
		pr_err("%s Invalid switch state: %d\n", __func__, state);
		return -EINVAL;
	}

	count = sizeof(protocol_list);
	for (i = 0; i < count; i++) {
		if (!strcmp(protocol_list[state],
				data->modbus_protocol_labels[i])) {
			pr_debug("%s find label %s at %d\n", __func__,
				data->modbus_protocol_labels[state], i);
			index = i;
			break;
		}
	}

	if (i >= count) {
		pr_err("%s do not find %s\n", __func__, protocol_list[state]);
		return -EINVAL;
	}

	index = index * data->gpio_nums;
	for (i = 0; i < data->gpio_nums; i++) {
		pr_debug("%s set gpio%d as %d\n", __func__,
			data->sel_gpios[i], data->modbus_switch_seqs.val[i + index]);
		gpio_set_value(data->sel_gpios[i],
				data->modbus_switch_seqs.val[i + index]);
	}

	return 0;
}

void modbus_ext_set_state(const struct modbus_ext_status *status)
{
	if (!modbus_dd)
		return;

	if (status->proto >= MODBUS_PROTO_INVALID)
		return;

	mutex_lock(&modbus_ext_mutex);

	if (status->active)
		modbus_dd->state |= MODBUS_PROTO_MASK(status->proto);
	else
		modbus_dd->state &= ~MODBUS_PROTO_MASK(status->proto);

	modbus_set_switch_state(modbus_dd);

	mutex_unlock(&modbus_ext_mutex);
}
EXPORT_SYMBOL(modbus_ext_set_state);

static int modbus_parse_protocol(struct modbus_data *data,
					struct device *dev, const char *name)
{
	int ret = 0;
	int cnt = 0;
	int i;

	cnt = data->modbus_switch_seqs.len / data->gpio_nums;
	of_property_read_string_array (dev->of_node, name,
					data->modbus_protocol_labels, cnt);
	for (i = 0; i < cnt; i++) {
		pr_debug("%s protocl %i is %s\n",
			__func__, i, data->modbus_protocol_labels[i]);
	}

	return ret;
}

static int modbus_parse_seq(struct modbus_data *data, struct device *dev,
					const char *name, struct modbus_seq *seq)
{
	int ret;
	int cnt = 0;
	struct property *pp = of_find_property(dev->of_node, name, &cnt);

	cnt /= sizeof(u32);
	if (!pp || cnt == 0 || cnt % data->gpio_nums) {
		pr_err("%s: error reading property %s, cnt = %d\n",
			__func__, name, cnt);
		ret = -EINVAL;
	} else {
		ret = of_property_read_u32_array(dev->of_node, name,
			seq->val, cnt);
		if (ret) {
			pr_err("%s: unable to read %s, ret = %d\n",
				__func__, name, ret);
		} else {
			seq->len = cnt;
		}
	}

	return ret;
}

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
	if (gpio_cnt <= 0) {
		dev_err(dev, "gpio count is %d return\n", gpio_cnt);
		return -EINVAL;
	}

	data->gpio_nums = gpio_cnt;

	for (i = 0; i < gpio_cnt; i++) {
		enum of_gpio_flags flags = 0;
		int gpio;
		const char *name = "mmi,mods-switch-gpio-labels";
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
	pr_info("%s find %d gpios\n", __func__, data->gpio_nums);

	return 0;

free_gpios:
	for (--i; i >= 0; --i) {
		sysfs_remove_link(&dev->kobj, data->sel_gpio_labels[i]);
		gpio_unexport(data->sel_gpios[i]);
	}

	return ret;
}

static int modbus_parse_dt(struct modbus_data *data, struct device *dev)
{
	int ret;

	ret = modbus_of_get_gpios(data, dev);
	if (ret)
		goto end;

	ret = modbus_parse_seq(data, dev, "mmi,mods-switch-seq",
					&data->modbus_switch_seqs);
	if (ret)
		goto end;

	ret = modbus_parse_protocol(data, dev, "mmi,mods-switch-seq-labels");
	if (ret)
		goto end;

end:
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

	ret = modbus_parse_dt(data, &pdev->dev);
	if (ret)
		return ret;

	data->dev = &pdev->dev;
	data->state = 0; /*really want MODBUS_PROTO_HIGH_Z??*/
	platform_set_drvdata(pdev, data);
	modbus_dd = data;

	data->vdd_switch = devm_regulator_get(&pdev->dev, "vdd-switch");
	if (!IS_ERR(data->vdd_switch)) {
		struct device_node *node = pdev->dev.of_node;
		const __be32 *prop;
		int len;
		int supply_vol_low, supply_vol_high;

		prop = of_get_property(node, "mmi,vdd-voltage-level", &len);
		if (!prop || (len != (2 * sizeof(__be32)))) {
			dev_err(&pdev->dev, "supply voltage levels not specified\n");
			return -EINVAL;
		} else {
			supply_vol_low = be32_to_cpup(&prop[0]);
			supply_vol_high = be32_to_cpup(&prop[1]);
		}
		dev_info(&pdev->dev, "set switch supply as %d %d\n",
					supply_vol_low, supply_vol_high);
		regulator_set_voltage(data->vdd_switch, supply_vol_low, supply_vol_high);
		ret = regulator_enable(data->vdd_switch);
		if (ret) {
			dev_err(&pdev->dev, "Unable to enable vdd_switch\n");
			return ret;
		}
	} else {
		dev_info(&pdev->dev, "there is no switch supply\n");
	}

	/* Configure to HighZ initially */
	/* modbus_set_switch_state(modbus_dd); */

	return 0;
}

static int modbus_remove(struct platform_device *pdev)
{
	struct modbus_data *data = platform_get_drvdata(pdev);

	modbus_gpio_free(data, &pdev->dev);

	return 0;
}

static const struct of_device_id modbus_match_tbl[] = {
	{ .compatible = "mmi,mods-switch" },
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
