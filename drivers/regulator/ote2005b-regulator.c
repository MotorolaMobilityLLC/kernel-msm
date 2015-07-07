/* Regulator driver for OTE2005B-HH053 + Moto Silego clock generator
 *
 * Copyright (C) 2015 Motorola Mobility, Inc.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/spmi.h>
#include <linux/delay.h>

#define OTE2005B_REGULATOR_DRIVER_NAME	"mot,regulator-ote2005b"
#define REG_ENABLE_OFFSET		0x46
#define REG_ENABLE_MASK			0x80
#define REG_PULLDOWN_OFFSET		0x48
#define REG_PULLDOWN_MASK		0x80


#define dbg_out		pr_debug

enum ote2005b_mode {
	MODE_OFF,
	MODE_ON,
	MODE_IDLE,
	MODE_MAX
};
#define GETMODE(reg_mode)	(((reg_mode) == REGULATOR_MODE_IDLE) \
				 ? MODE_IDLE : MODE_ON)

struct ote2005b_data {
	struct regulator_desc desc;
	struct spmi_device *spmi_dev;
	struct regulator_dev *rdev;
	int enabled;
	unsigned int cur_mode;
	int microvolts;
	int gpio_cnt;
	int *ctrl_gpios;
	u8 *gpio_state[MODE_MAX];
	int pull_down_delay;
	u16 spmi_base_addr;
};

static int spmi_masked_write(struct spmi_device *spmi,
				u16 addr, u8 value, u8 mask)
{
	int r;
	u8 write = value & mask;
	if (mask && (mask != 0xFF)) {
		u8 read;
		int r = spmi_ext_register_readl(spmi->ctrl, spmi->sid,
						addr, &read, 1);
		if (r)
			return r;
		dbg_out("read %02x write %02x mask %02x\n", read, value, mask);
		if ((read&mask) == write)
			return 0;
		write |= (read & ~mask);
	}
	r = spmi_ext_register_writel(spmi->ctrl, spmi->sid, addr, &write, 1);
	dbg_out("write to %x%04x with %02x, r = %d\n",
		spmi->sid, addr, write, r);
	return r;
}

static int ote2005b_set_pulldown(struct ote2005b_data *drvdata, bool enable)
{
	int r = spmi_masked_write(drvdata->spmi_dev,
				  drvdata->spmi_base_addr+REG_PULLDOWN_OFFSET,
				  enable ? REG_PULLDOWN_MASK : 0,
				  REG_PULLDOWN_MASK);
	if (r) {
		pr_err("Failed(%d) set pull down %d\n", r, enable);
		return r;
	}
	return spmi_masked_write(drvdata->spmi_dev,
			      drvdata->spmi_base_addr + REG_ENABLE_OFFSET,
			      0, REG_ENABLE_MASK);
}

static void ote2005b_set_mode(struct ote2005b_data *drvdata,
				enum ote2005b_mode mode)
{
	int i;
	u8 *state = drvdata->gpio_state[mode];

	dbg_out("mode = %d\n", mode);
	for (i = 0; i < drvdata->gpio_cnt; i++)
		gpio_set_value(drvdata->ctrl_gpios[i], !!state[i]);
}

static int ote2005b_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct ote2005b_data *drvdata = rdev_get_drvdata(rdev);

	dbg_out("microvolts = %d\n", drvdata->microvolts);
	if (drvdata->microvolts)
		return drvdata->microvolts;

	return -EINVAL;
}

static int ote2005b_regulator_enable(struct regulator_dev *rdev)
{
	struct ote2005b_data *drvdata = rdev_get_drvdata(rdev);

	dbg_out("current enabled = %d\n", drvdata->enabled);
	if (!drvdata->enabled) {
		ote2005b_set_mode(drvdata, GETMODE(drvdata->cur_mode));
		drvdata->enabled = true;
	}

	return 0;
}

static int ote2005b_regulator_disable(struct regulator_dev *rdev)
{
	struct ote2005b_data *drvdata = rdev_get_drvdata(rdev);

	dbg_out("current enabled = %d, PD delay = %d ms\n",
		drvdata->enabled, drvdata->pull_down_delay);
	if (drvdata->enabled) {
		/* Workaround for charger pump shut down:
		 * it needs let charger pump works a while without pull down
		 * that could discharge the big capacity connect with display
		 */
		if (drvdata->pull_down_delay) {
			if (drvdata->cur_mode == REGULATOR_MODE_IDLE)
				ote2005b_set_mode(drvdata,
						  REGULATOR_MODE_NORMAL);
			ote2005b_set_pulldown(drvdata, false);
			usleep(drvdata->pull_down_delay * 1000);
			ote2005b_set_pulldown(drvdata, true);
		}
		ote2005b_set_mode(drvdata, MODE_OFF);
		drvdata->enabled = false;
	}

	return 0;
}

static int ote2005b_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct ote2005b_data *drvdata = rdev_get_drvdata(rdev);
	return drvdata->enabled;
}

static int ote2005b_regulator_set_mode(struct regulator_dev *rdev,
		unsigned int mode)
{
	struct ote2005b_data *drvdata = rdev_get_drvdata(rdev);

	dbg_out("reg_mode = %d, current = %d\n", mode, drvdata->cur_mode);
	if (mode != drvdata->cur_mode) {
		if (drvdata->enabled)
			ote2005b_set_mode(drvdata, GETMODE(mode));
		drvdata->cur_mode = mode;
	}

	return 0;
}

static unsigned int ote2005b_regulator_get_mode(struct regulator_dev *rdev)
{
	struct ote2005b_data *drvdata = rdev_get_drvdata(rdev);
	return drvdata->cur_mode;
}

static struct regulator_ops ote2005b_regulator_ops = {
	.get_voltage    = ote2005b_regulator_get_voltage,
	.enable		= ote2005b_regulator_enable,
	.disable	= ote2005b_regulator_disable,
	.is_enabled	= ote2005b_regulator_is_enabled,
	.set_mode       = ote2005b_regulator_set_mode,
	.get_mode       = ote2005b_regulator_get_mode,
};

static int ote2005b_regulator_init_dt(struct device *dev,
		struct ote2005b_data *drvdata, struct regulator_config *cfg)
{
	const char * const prop_gpios = "mot,ctrl-gpios";
	struct device_node *of = dev->of_node;
	u8 *buf;
	int i;
	struct regulator_init_data *init_data =
			of_get_regulator_init_data(dev, of);
	if (!init_data || !init_data->constraints.name)
		return -EINVAL;
	if (!of_get_property(of, "parent-supply", NULL)) {
		dev_err(dev, "no parent power supply defined\n");
		return -EINVAL;
	}
	if (init_data->constraints.min_uV != init_data->constraints.max_uV) {
		dev_err(dev, "regulator specified with variable voltages\n");
		return -EINVAL;
	}
	drvdata->microvolts = init_data->constraints.min_uV;

	drvdata->gpio_cnt = of_gpio_named_count(of, prop_gpios);
	if (drvdata->gpio_cnt < 2) {
		dev_err(dev, "invalid control gpios\n");
		return -EINVAL;
	}
	buf = devm_kzalloc(dev, (sizeof(int) + sizeof(u8)*MODE_MAX) *
			   drvdata->gpio_cnt, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	drvdata->ctrl_gpios = (int *)buf;
	for (i = 0; i < drvdata->gpio_cnt; i++) {
		int gpio = of_get_named_gpio(of, prop_gpios, i);
		if (!gpio_is_valid(gpio)) {
			dev_err(dev, "invalid control gpio %d/%d\n",
				i, drvdata->gpio_cnt);
			return -EINVAL;
		}
		drvdata->ctrl_gpios[i] = gpio;
	};
	buf += sizeof(int) * i;
	for (i = 0; i < MODE_MAX; i++) {
		const char * const props[MODE_MAX] = {
			[MODE_OFF]  = "mot,gpio-state-off",
			[MODE_ON]   = "mot,gpio-state-on",
			[MODE_IDLE] = "mot,gpio-state-idle",
		};
		int len;
		if (!of_find_property(of, props[i], &len)) {
			dev_err(dev, "missing %s\n", props[i]);
			return -EINVAL;
		}
		if (len != drvdata->gpio_cnt) {
			dev_err(dev, "mismatched length %s\n", props[i]);
			return -EINVAL;
		}
		of_property_read_u8_array(of, props[i], buf, len);
		drvdata->gpio_state[i] = buf;
		buf += len;
	}
	drvdata->enabled = !!init_data->constraints.boot_on;
	drvdata->cur_mode = REGULATOR_MODE_NORMAL;
	buf = drvdata->gpio_state[drvdata->enabled ? MODE_ON : MODE_OFF];
	for (i = 0; i < drvdata->gpio_cnt; i++) {
		int flag = buf[i] ? GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW;
		int gpio = drvdata->ctrl_gpios[i];
		char name[32];
		snprintf(name, sizeof(name), "ote2005b.%d", i);
		if (devm_gpio_request_one(dev, gpio, flag, name)) {
			dev_err(dev, "failed request gpio %d\n", gpio);
			return -EINVAL;
		}
	}
	if (!of_property_read_u32(of, "mot,pull-down-delay", &i))
		drvdata->pull_down_delay = i;

	init_data->supply_regulator = "parent";
	init_data->constraints.apply_uV = 0;
	init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_MODE;
	init_data->constraints.valid_modes_mask
		|= REGULATOR_MODE_NORMAL | REGULATOR_MODE_IDLE;

	drvdata->desc.name = init_data->constraints.name;
	drvdata->desc.type = REGULATOR_VOLTAGE;
	drvdata->desc.owner = THIS_MODULE;
	drvdata->desc.ops = &ote2005b_regulator_ops;
	drvdata->desc.n_voltages = 1;

	cfg->dev = dev;
	cfg->init_data = init_data;
	cfg->driver_data = drvdata;
	cfg->of_node = of;

	return 0;
}

static int ote2005b_regulator_probe(struct spmi_device *spmi)
{
	struct ote2005b_data *drvdata;
	struct resource *res;
	struct regulator_config cfg = { };
	int r;

	drvdata = devm_kzalloc(&spmi->dev, sizeof(struct ote2005b_data),
			       GFP_KERNEL);
	if (drvdata == NULL) {
		dev_err(&spmi->dev, "Failed to allocate device data\n");
		r = -ENOMEM;
		return r;
	}

	res = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&spmi->dev, "Node is missing base address\n");
		return -EINVAL;
	}
	drvdata->spmi_base_addr = res->start;

	r = ote2005b_regulator_init_dt(&spmi->dev, drvdata, &cfg);
	if (r) {
		dev_err(&spmi->dev, "Failed to initialize device data\n");
		return r;
	}

	drvdata->spmi_dev = spmi;
	drvdata->rdev = regulator_register(&drvdata->desc, &cfg);
	if (IS_ERR(drvdata->rdev)) {
		r = PTR_ERR(drvdata->rdev);
		dev_err(&spmi->dev, "Failed to register regulator: %d\n", r);
		return r;
	}

	dev_set_drvdata(&spmi->dev, drvdata);

	return r;
}

static int ote2005b_regulator_remove(struct spmi_device *spmi)
{
	struct ote2005b_data *drvdata = dev_get_drvdata(&spmi->dev);

	if (drvdata != NULL) {
		dev_set_drvdata(&spmi->dev, NULL);
		regulator_unregister(drvdata->rdev);
	}

	return 0;
}

static const struct of_device_id ote2005b_of_match[] = {
	{ .compatible = OTE2005B_REGULATOR_DRIVER_NAME, },
	{},
};

static const struct spmi_device_id ote2005b_regulator_id[] = {
	{ OTE2005B_REGULATOR_DRIVER_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(spmi, ote2005b_of_match);

static struct spmi_driver ote2005b_regulator_driver = {
	.driver = {
		.name	= OTE2005B_REGULATOR_DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(ote2005b_of_match),
	},
	.probe	= ote2005b_regulator_probe,
	.remove	= ote2005b_regulator_remove,
	.id_table	= ote2005b_regulator_id,
};

static int __init ote2005b_regulator_init(void)
{
	static bool has_registered;

	if (has_registered)
		return 0;
	has_registered = true;
	return spmi_driver_register(&ote2005b_regulator_driver);
}

static void __exit ote2005b_regulator_exit(void)
{
	spmi_driver_unregister(&ote2005b_regulator_driver);
}

subsys_initcall(ote2005b_regulator_init);
module_exit(ote2005b_regulator_exit);

MODULE_AUTHOR("Wengang Wu <wgw@motorola.com>");
MODULE_DESCRIPTION("Orise + Silego Regulator Driver");
MODULE_LICENSE("GPL");
