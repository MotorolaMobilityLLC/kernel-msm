/*
 * Driver for TI,TP61280 DC/DC Boost Converter
 *
 * Copyright (c) 2014, NVIDIA Corporation. All rights reserved.
 *
 * Author: Venkat Reddy Talla <vreddytalla@nvidia.com>
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/regulator/driver.h>
#include <linux/mfd/core.h>
#include <linux/thermal.h>

#define TPS61280_CONFIG		0x01
#define TPS61280_VOUTFLOORSET	0x02
#define TPS61280_VOUTROOFSET	0x03
#define TPS61280_ILIMSET	0x04
#define TPS61280_STATUS		0x05
#define TPS61280_E2PROMCTRL	0xFF

#define TPS61280_ILIM_BASE	0x08
#define TPS61280_ILIM_OFFSET	1500
#define TPS61280_ILIM_STEP	500
#define TPS61280_ENABLE_ILIM	0x0
#define TPS61280_DISABLE_ILIM	0x20
#define TPS61280_VOUT_OFFSET	2850
#define TPS61280_VOUT_STEP	50
#define TPS61280_HOTDIE_TEMP	120
#define TPS61280_NORMAL_OPTEMP	40

#define TPS61280_CONFIG_MODE_MASK	0x03
#define TPS61280_ILIM_MASK	0x0F
#define TPS61280_VOUT_MASK	0x1F

#define TPS61280_PGOOD_MASK	BIT(0)
#define TPS61280_FAULT_MASK	BIT(1)
#define TPS61280_ILIMBST_MASK	BIT(2)
#define TPS61280_ILIMPT_MASK	BIT(3)
#define TPS61280_OPMODE_MASK	BIT(4)
#define TPS61280_DCDCMODE_MASK	BIT(5)
#define TPS61280_HOTDIE_MASK	BIT(6)
#define TPS61280_THERMALSD_MASK	BIT(7)

enum tps61280_device_modes {
	HARDWARE_CONTROL,
	PFM_AUTO_TRANSTION_INTO_PWM,
	FORCED_PWM_OPERATION,
	PFM_AUTO_TRANSITION_VSEL,
};

struct tps61280_platform_data {
	int		gpio_en;
	int		gpio_vsel;
	int		device_mode;
	int		voutfloor_voltage;
	int		voutroof_voltage;
	int		ilim_current_limit;
};

struct tps61280_chip {
	struct device	*dev;
	struct i2c_client	*client;
	struct regulator_desc	rdesc;
	struct regulator_dev	*rdev;
	struct tps61280_platform_data	*pdata;
	struct device_node		*tps_np;
	struct regmap		*regmap;
	struct thermal_zone_device	*tz_device;
	struct mutex		mutex;
	int		gpio_en;
	int		gpio_vsel;
};

static const struct regmap_config tps61280_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 5,
};

static int tps61280_val_to_reg(int val, int offset, int div, int nbits,
					bool roundup)
{
	int max_val = offset + (BIT(nbits) - 1) * div;

	if (val <= offset)
		return 0;

	if (val >= max_val)
		return BIT(nbits) - 1;

	if (roundup)
		return DIV_ROUND_UP(val - offset, div);
	else
		return (val - offset) / div;
}

static int tps61280_set_voltage(struct regulator_dev *rdev,
			int min_uv, int max_uv, unsigned *selector)
{
	struct tps61280_chip *tps = rdev_get_drvdata(rdev);
	int val;
	int ret = 0;
	int gpio_val;

	if (!max_uv)
		return -EINVAL;

	dev_info(tps->dev, "Setting output voltage %d\n", max_uv/1000);

	val = tps61280_val_to_reg(max_uv/1000, TPS61280_VOUT_OFFSET,
					TPS61280_VOUT_STEP, 5, 0);
	if (val < 0)
		return -EINVAL;

	if (gpio_is_valid(tps->gpio_vsel))
		gpio_val = gpio_get_value_cansleep(tps->gpio_vsel);
	else
		return -EINVAL;

	if (gpio_val) {
		ret = regmap_update_bits(tps->regmap, TPS61280_VOUTROOFSET,
					TPS61280_VOUT_MASK, val);
		if (ret < 0) {
			dev_err(tps->dev, "VOUTFLR REG update failed %d\n",
					ret);
			return ret;
		}
	} else {
		ret = regmap_update_bits(tps->regmap, TPS61280_VOUTFLOORSET,
					TPS61280_VOUT_MASK, val);
		if (ret < 0) {
			dev_err(tps->dev, "VOUTROOF REG update failed %d\n",
					ret);
			return ret;
		}
	}

	return ret;
}

static int tps61280_get_voltage(struct regulator_dev *rdev)
{
	struct tps61280_chip *tps = rdev_get_drvdata(rdev);
	u32 val;
	int ret;
	int gpio_val;

	if (gpio_is_valid(tps->gpio_vsel))
		gpio_val = gpio_get_value_cansleep(tps->gpio_vsel);
	else
		return -EINVAL;

	if (gpio_val) {
		ret = regmap_read(tps->regmap, TPS61280_VOUTROOFSET, &val);
		if (ret < 0) {
			dev_err(tps->dev, "VOUTROOF REG read failed %d\n",
					ret);
			return ret;
		}
	} else {
		ret = regmap_read(tps->regmap, TPS61280_VOUTFLOORSET, &val);
		if (ret < 0) {
			dev_err(tps->dev, "VOUTFLR REG read failed %d\n",
					ret);
			return ret;
		}
	}

	val &= TPS61280_VOUT_MASK;
	val = val * TPS61280_VOUT_STEP + TPS61280_VOUT_OFFSET;
	return val;
}

static int tps61280_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct tps61280_chip *tps = rdev_get_drvdata(rdev);
	int val;
	int ret = 0;

	switch (mode) {
	case HARDWARE_CONTROL:
		val = HARDWARE_CONTROL;
		break;

	case PFM_AUTO_TRANSTION_INTO_PWM:
		val = PFM_AUTO_TRANSTION_INTO_PWM;
		break;

	case FORCED_PWM_OPERATION:
		val = FORCED_PWM_OPERATION;
		break;

	case PFM_AUTO_TRANSITION_VSEL:
		val = PFM_AUTO_TRANSITION_VSEL;
		break;

	default:
		return -EINVAL;
	}

	ret = regmap_update_bits(tps->regmap, TPS61280_CONFIG,
				TPS61280_CONFIG_MODE_MASK, val);
	if (ret < 0) {
		dev_err(tps->dev, "CONFIG REG update failed %d\n", ret);
		return ret;
	}
	return ret;
}

static unsigned int tps61280_get_mode(struct regulator_dev *rdev)
{
	struct tps61280_chip *tps = rdev_get_drvdata(rdev);
	unsigned int data;
	int ret;

	ret = regmap_read(tps->regmap, TPS61280_CONFIG, &data);
	if (ret < 0) {
		dev_err(tps->dev, "CONFIG REG read failed %d\n", ret);
		return ret;
	}
	return data & TPS61280_CONFIG_MODE_MASK;
}

static int tps61280_set_current_limit(struct regulator_dev *rdev, int min_ua,
					int max_ua)
{
	struct tps61280_chip *tps = rdev_get_drvdata(rdev);
	int val;
	int ret = 0;

	if (!max_ua)
		return -EINVAL;

	dev_info(tps->dev, "Setting current limit %d\n", max_ua/1000);

	val = tps61280_val_to_reg(max_ua/1000, TPS61280_ILIM_OFFSET,
					TPS61280_ILIM_STEP, 3, 0);
	if (val < 0)
		return -EINVAL;

	ret = regmap_update_bits(tps->regmap, TPS61280_ILIMSET,
			TPS61280_ILIM_MASK, (val | TPS61280_ILIM_BASE));
	if (ret < 0) {
		dev_err(tps->dev, "ILIM REG update failed %d\n", ret);
		return ret;
	}

	return ret;
}

static int tps61280_get_current_limit(struct regulator_dev *rdev)
{
	struct tps61280_chip *tps = rdev_get_drvdata(rdev);
	u32 val;
	int ret;

	ret = regmap_read(tps->regmap, TPS61280_ILIMSET, &val);
	if (ret < 0) {
		dev_err(tps->dev, "ILIM REG read failed %d\n", ret);
		return ret;
	}

	val &= (TPS61280_ILIM_MASK & ~TPS61280_ILIM_BASE);
	val = val * TPS61280_ILIM_STEP + TPS61280_ILIM_OFFSET;
	return val;
}

static struct regulator_ops tps61280_ops = {
	.set_voltage		= tps61280_set_voltage,
	.get_voltage		= tps61280_get_voltage,
	.set_mode		= tps61280_set_mode,
	.get_mode		= tps61280_get_mode,
	.set_current_limit	= tps61280_set_current_limit,
	.get_current_limit	= tps61280_get_current_limit,
};

static int tps61280_thermal_read_temp(void *data, long *temp)
{
	struct tps61280_chip *tps = data;
	u32 val;
	int ret;

	ret = regmap_read(tps->regmap, TPS61280_STATUS, &val);
	if (ret < 0) {
		dev_err(tps->dev, "STATUS REG read failed %d\n", ret);
		return -EINVAL;
	}

	if (val & TPS61280_HOTDIE_MASK)
		*temp = TPS61280_HOTDIE_TEMP * 1000;
	else
		*temp = TPS61280_NORMAL_OPTEMP * 1000;
	return 0;
}

static irqreturn_t tps61280_irq(int irq, void *dev)
{
	struct tps61280_chip *tps = dev;
	struct i2c_client *client = tps->client;
	int val;
	int ret;

	ret = regmap_read(tps->regmap, TPS61280_STATUS, &val);
	if (ret < 0) {
		dev_err(&client->dev, "%s() STATUS REG read failed %d\n",
			__func__, ret);
		return -EINVAL;
	}

	dev_info(&client->dev, "%s() Irq %d status 0x%02x\n",
				__func__, irq, val);

	if (val & TPS61280_PGOOD_MASK)
		dev_info(&client->dev,
			"PG Status:output voltage is within normal range\n");

	if (val & TPS61280_FAULT_MASK)
		dev_err(&client->dev, "FAULT condition has occurred\n");

	if (val & TPS61280_ILIMBST_MASK)
		dev_err(&client->dev,
			"Input current triggered for 1.5ms in boost mode\n");

	if (val & TPS61280_ILIMPT_MASK)
		dev_err(&client->dev, "Bypass FET Current has triggered\n");

	if (val & TPS61280_OPMODE_MASK)
		dev_err(&client->dev, "Device operates in dc/dc mode\n");
	else
		dev_err(&client->dev,
			"Device operates in pass-through mode\n");

	if (val & TPS61280_DCDCMODE_MASK)
		dev_err(&client->dev, "Device operates in PFM mode\n");
	else
		dev_err(&client->dev, "Device operates in PWM mode\n");

	if (val & TPS61280_HOTDIE_MASK) {
		dev_info(&client->dev, "TPS61280 in thermal regulation\n");
		if (tps->tz_device)
			thermal_zone_device_update(tps->tz_device);
	}
	if (val & TPS61280_THERMALSD_MASK)
		dev_err(&client->dev, "Thermal shutdown tripped\n");

	return IRQ_HANDLED;
}

static int tps61280_initialize(struct tps61280_chip *tps61280)
{
	int device_mode;
	int voutfloor_voltage;
	int voutroof_voltage;
	int ilim_current_limit;
	int val;
	int ret;

	if (tps61280->pdata) {
		device_mode = tps61280->pdata->device_mode ?: 01;
		voutfloor_voltage =
			tps61280->pdata->voutfloor_voltage ?: 3150;
		voutroof_voltage =
			tps61280->pdata->voutroof_voltage ?: 3350;
		ilim_current_limit =
			tps61280->pdata->ilim_current_limit ?: 3000;
	} else {
		device_mode = 01;
		voutfloor_voltage = 3150;
		voutroof_voltage = 3350;
		ilim_current_limit = 3000;
	}

	/* Configure device mode of operation */
	ret = regmap_update_bits(tps61280->regmap, TPS61280_CONFIG,
				TPS61280_CONFIG_MODE_MASK, device_mode);
	if (ret < 0) {
		dev_err(tps61280->dev, "CONFIG REG update failed %d\n", ret);
		return ret;
	}

	/* Configure vout floor output voltage */
	val = tps61280_val_to_reg(voutfloor_voltage, TPS61280_VOUT_OFFSET,
				TPS61280_VOUT_STEP, 5, 0);

	ret = regmap_update_bits(tps61280->regmap, TPS61280_VOUTFLOORSET,
				TPS61280_VOUT_MASK, val);
	if (ret < 0) {
		dev_err(tps61280->dev, "VOUTFLR REG update failed %d\n", ret);
		return ret;
	}

	/* Configure vout roof output voltage */
	val = tps61280_val_to_reg(voutroof_voltage, TPS61280_VOUT_OFFSET,
				TPS61280_VOUT_STEP, 5, 0);

	ret = regmap_update_bits(tps61280->regmap, TPS61280_VOUTROOFSET,
				TPS61280_VOUT_MASK, val);
	if (ret < 0) {
		dev_err(tps61280->dev, "VOUTROOF REG update failed %d\n", ret);
		return ret;
	}

	/* Configure inductor valley current limit */
	val = tps61280_val_to_reg(ilim_current_limit, TPS61280_ILIM_OFFSET,
				TPS61280_ILIM_STEP, 3, 0);

	ret = regmap_update_bits(tps61280->regmap, TPS61280_ILIMSET,
			TPS61280_ILIM_MASK, (val | TPS61280_ILIM_BASE));
	if (ret < 0) {
		dev_err(tps61280->dev, "ILIM REG update failed %d\n", ret);
		return ret;
	}
	return 0;
}

static void tps61280_parse_dt_data(struct i2c_client *client,
				struct tps61280_platform_data *pdata)
{
	struct device_node *np = client->dev.of_node;
	u32 pval;
	int ret;

	ret = of_property_read_u32(np, "ti,config-device-mode", &pval);
	if (!ret)
		pdata->device_mode = pval;

	ret = of_property_read_u32(np, "ti,voutfloor-voltage", &pval);
	if (!ret)
		pdata->voutfloor_voltage = pval;

	ret = of_property_read_u32(np, "ti,voutroof-voltage", &pval);
	if (!ret)
		pdata->voutroof_voltage = pval;

	ret = of_property_read_u32(np, "ti,ilim-current-limit", &pval);
	if (!ret)
		pdata->ilim_current_limit = pval;

	pdata->gpio_en = of_get_named_gpio(np, "ti,gpio-en", 0);

	pdata->gpio_vsel = of_get_named_gpio(np, "ti,gpio-vsel", 0);
}

static int tps61280_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tps61280_chip *tps61280;
	struct regulator_config rconfig = { };
	int ret;

	tps61280 = devm_kzalloc(&client->dev, sizeof(*tps61280), GFP_KERNEL);
	if (!tps61280)
		return -ENOMEM;

	tps61280->client = client;
	if (client->dev.of_node) {
		tps61280->pdata = devm_kzalloc(&client->dev,
					sizeof(*tps61280->pdata), GFP_KERNEL);
		if (!tps61280->pdata)
			return -ENOMEM;
		tps61280_parse_dt_data(client, tps61280->pdata);
	} else {
		tps61280->pdata = client->dev.platform_data;
	}

	if (!tps61280->pdata)
		return -ENODATA;

	tps61280->regmap = devm_regmap_init_i2c(client,
					&tps61280_regmap_config);
	if (IS_ERR(tps61280->regmap)) {
		ret = PTR_ERR(tps61280->regmap);
		dev_err(&client->dev, "regmap init failed with err%d\n", ret);
		return ret;
	}

	tps61280->gpio_en = tps61280->pdata->gpio_en;
	tps61280->gpio_vsel = tps61280->pdata->gpio_vsel;
	tps61280->tps_np = client->dev.of_node;
	tps61280->dev = &client->dev;
	i2c_set_clientdata(client, tps61280);
	mutex_init(&tps61280->mutex);

	tps61280->rdesc.name  = "tps61280-dcdc";
	tps61280->rdesc.ops   = &tps61280_ops;
	tps61280->rdesc.type  = REGULATOR_VOLTAGE;
	tps61280->rdesc.owner = THIS_MODULE;
	tps61280->rdesc.min_uV = 2850000;
	tps61280->rdesc.uV_step = 50000;

	rconfig.dev = tps61280->dev;
	rconfig.of_node =  tps61280->tps_np;
	rconfig.driver_data = tps61280;
	rconfig.regmap = tps61280->regmap;
	if (gpio_is_valid(tps61280->gpio_en)) {
		rconfig.ena_gpio = tps61280->gpio_en;
		rconfig.ena_gpio_flags = GPIOF_OUT_INIT_HIGH;
	}

	tps61280->rdev = devm_regulator_register(tps61280->dev,
				&tps61280->rdesc, &rconfig);
	if (IS_ERR(tps61280->rdev)) {
		ret = PTR_ERR(tps61280->rdev);
		dev_err(&client->dev, "regulator register failed %d\n", ret);
	}

	if (gpio_is_valid(tps61280->gpio_vsel)) {
		ret = devm_gpio_request(tps61280->dev,
				tps61280->gpio_vsel, "tps61280_vsel");
		if (ret < 0) {
			dev_err(tps61280->dev, "gpio request failed  %d\n",
				ret);
			return ret;
		}
	}

	if (client->irq) {
		ret = devm_request_threaded_irq(&client->dev, client->irq,
			NULL, tps61280_irq,
			IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
			dev_name(&client->dev), tps61280);
		if (ret < 0) {
			dev_err(&client->dev,
				"%s: request IRQ %d fail, err = %d\n",
				__func__, client->irq, ret);
			client->irq = 0;
			return ret;
		}
	}
	device_set_wakeup_capable(&client->dev, 1);
	device_wakeup_enable(&client->dev);

	tps61280->tz_device = thermal_zone_of_sensor_register(tps61280->dev, 0,
				tps61280, tps61280_thermal_read_temp, NULL);
	if (IS_ERR(tps61280->tz_device)) {
		ret = PTR_ERR(tps61280->tz_device);
		dev_err(&client->dev, "TZ device register failed: %d\n", ret);
		return ret;
	}

	ret = tps61280_initialize(tps61280);
	if (ret < 0)
		dev_err(&client->dev, "chip init failed - %d\n", ret);

	dev_info(&client->dev, "DC-Boost probe successful\n");

	return 0;
}


#ifdef CONFIG_PM_SLEEP
static int tps61280_suspend(struct device *dev)
{
	struct tps61280_chip *tps = dev_get_drvdata(dev);

	if (device_may_wakeup(&tps->client->dev) && tps->client->irq)
		enable_irq_wake(tps->client->irq);

	return 0;
}

static int tps61280_resume(struct device *dev)
{
	struct tps61280_chip *tps = dev_get_drvdata(dev);

	if (device_may_wakeup(&tps->client->dev) && tps->client->irq)
		disable_irq_wake(tps->client->irq);

	return 0;
}
#endif /* CONFIG_PM */

static SIMPLE_DEV_PM_OPS(tps61280_pm_ops, tps61280_suspend, tps61280_resume);

static const struct of_device_id tps61280_dt_match[] = {
	{ .compatible = "ti,tps61280" },
	{ },
};
MODULE_DEVICE_TABLE(of, tps61280_dt_match);

struct i2c_driver tps61280_driver = {
	.driver = {
		.name = "tps61280",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tps61280_dt_match),
		.pm = &tps61280_pm_ops,
	},
	.probe = tps61280_probe,
};

static int __init tps61280_init(void)
{
	return i2c_add_driver(&tps61280_driver);
}
subsys_initcall(tps61280_init);

static void __exit tps61280_exit(void)
{
	i2c_del_driver(&tps61280_driver);
}
module_exit(tps61280_exit);

module_i2c_driver(tps61280_driver);

MODULE_DESCRIPTION("tps61280 driver");
MODULE_AUTHOR("Venkat Reddy Talla <vreddytalla@nvidia.com>");
MODULE_ALIAS("i2c:tps61280");
MODULE_LICENSE("GPL v2");
