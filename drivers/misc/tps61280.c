/*
 * Driver for TI,TP61280 DC/DC Boost Converter
 *
 * Copyright (c) 2016 lenovo Corporation. All rights reserved.
 *
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
#include <linux/regmap.h>
#include <linux/platform_device.h>

#define TPS61280_REVISION	0x00
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
#define TPS61280_CONFIG_SSFM_MASK	0x04
#define TPS61280_CONFIG_GPIOCFG_MASK	0x08
#define TPS61280_CONFIG_MODE_VSEL	0x03
#define TPS61280_CONFIG_MODE_FORCE_PWM	0x02
#define TPS61280_CONFIG_MODE_AUTO_PWM	0x01
#define TPS61280_CONFIG_MODE_DEVICE	0x00

#define TPS61280_ILIM_MASK	0x0F
#define TPS61280_SOFT_START_MASK	0x10
#define TPS61280_ILIM_OFF_MASK	0x20
#define TPS61280_ILIM_ALL_MASK (TPS61280_ILIM_MASK |\
				TPS61280_SOFT_START_MASK |\
				TPS61280_ILIM_OFF_MASK)
#define TPS61280_VOUT_MASK	0x1F

#define TPS61280_CONFIG_ENABLE_MASK		0x60
#define TPS61280_CONFIG_BYPASS_HW_CONTRL	0x00
#define TPS61280_CONFIG_BYPASS_AUTO_TRANS	0x20
#define TPS61280_CONFIG_BYPASS_PASS_TROUGH	0x40
#define TPS61280_CONFIG_BYPASS_SHUTDOWN		0x60

/* in config register bit 7 is a reset. bit 4 is reserved */
#define TPS61280_CONFIG_MASK	(TPS61280_CONFIG_MODE_MASK |\
					TPS61280_CONFIG_SSFM_MASK |\
					TPS61280_CONFIG_GPIOCFG_MASK |\
					TPS61280_CONFIG_ENABLE_MASK)

#define TPS61280_PGOOD_MASK	BIT(0)
#define TPS61280_FAULT_MASK	BIT(1)
#define TPS61280_ILIMBST_MASK	BIT(2)
#define TPS61280_ILIMPT_MASK	BIT(3)
#define TPS61280_OPMODE_MASK	BIT(4)
#define TPS61280_DCDCMODE_MASK	BIT(5)
#define TPS61280_HOTDIE_MASK	BIT(6)
#define TPS61280_THERMALSD_MASK	BIT(7)

#define TPS61280_MAX_VOUT_REG	2
#define TPS61280_VOUT_MASK	0x1F

#define TPS61280_VOUT_VMIN	2850000
#define TPS61280_VOUT_VMAX	4400000
#define TPS61280_VOUT_VSTEP	 50000

/* four configuration registers of tps61280 */
struct tps61280_platform_data {
	int config;
	int vout_floor;
	int vout_roof;
	int ilim;
};

struct tps61280_chip {
	struct device			*dev;
	struct i2c_client		*client;
	struct regmap			*rmap;
	struct tps61280_platform_data	pdata;

};

static const struct regmap_config tps61280_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 5,
};

static int tps61280_config(struct tps61280_chip *tps61280)
{
	int ret;
	struct device *dev = tps61280->dev;
	struct tps61280_platform_data pdata = tps61280->pdata;

	ret = regmap_update_bits(tps61280->rmap, TPS61280_CONFIG,
		TPS61280_CONFIG_ENABLE_MASK, TPS61280_CONFIG_BYPASS_SHUTDOWN);
	if (ret < 0) {
		dev_err(dev, "CONFIG update failed %d\n", ret);
		return ret;
	}

	ret = regmap_update_bits(tps61280->rmap, TPS61280_VOUTFLOORSET,
				TPS61280_VOUT_MASK, pdata.vout_floor);
	if (ret < 0) {
		dev_err(dev, "VOUTFLOORSET update failed %d\n", ret);
		return ret;
	}

	ret = regmap_update_bits(tps61280->rmap, TPS61280_VOUTROOFSET,
				TPS61280_VOUT_MASK, pdata.vout_roof);
	if (ret < 0) {
		dev_err(dev, "VOUTROOFET update failed %d\n", ret);
		return ret;
	}

	ret = regmap_update_bits(tps61280->rmap, TPS61280_ILIMSET,
				TPS61280_ILIM_ALL_MASK, pdata.ilim);
	if (ret < 0) {
		dev_err(dev, "ILIM update failed %d\n", ret);
		return ret;
	}

	ret = regmap_update_bits(tps61280->rmap, TPS61280_CONFIG,
				TPS61280_CONFIG_MASK, pdata.config);
	if (ret < 0) {
		dev_err(dev, "CONFIG update failed %d\n", ret);
		return ret;
	}

	return 0;
}

static int tps61280_parse_dt_data(struct i2c_client *client,
				struct tps61280_platform_data *pdata)
{
	struct device_node *np = client->dev.of_node;
	int rc;

	rc = of_property_read_u32(np, "ti,config", &pdata->config);
	if (rc) {
		dev_err(&client->dev, "%s: config read error\n", __func__);
		return -EIO;
	}

	rc = of_property_read_u32(np, "ti,vout-floor", &pdata->vout_floor);
	if (rc) {
		dev_err(&client->dev, "%s: vout_floor read error\n", __func__);
		return -EIO;
	}

	rc = of_property_read_u32(np, "ti,vout-roof", &pdata->vout_roof);
	if (rc) {
		dev_err(&client->dev, "%s: vout_roof read error\n", __func__);
		return -EIO;
	}

	rc = of_property_read_u32(np, "ti,ilim", &pdata->ilim);
	if (rc) {
		dev_err(&client->dev, "%s: ilim read error\n", __func__);
		return -EIO;
	}

	return 0;
}


static int tps61280_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tps61280_chip *tps61280;
	int ret;
	unsigned readout;

	if (!client->dev.of_node) {
		dev_err(&client->dev, "Only Supporting DT\n");
		return -ENODEV;
	}

	tps61280 = devm_kzalloc(&client->dev, sizeof(*tps61280), GFP_KERNEL);
	if (!tps61280)
		return -ENOMEM;

	ret = tps61280_parse_dt_data(client, &tps61280->pdata);
	if (ret < 0) {
		dev_err(&client->dev, "Platform data pasring failed: %d\n",
			ret);
		return ret;
	}

	tps61280->rmap = devm_regmap_init_i2c(client, &tps61280_regmap_config);
	if (IS_ERR(tps61280->rmap)) {
		ret = PTR_ERR(tps61280->rmap);
		dev_err(&client->dev, "regmap init failed with err%d\n", ret);
		return ret;
	}

	tps61280->client = client;
	tps61280->dev = &client->dev;
	i2c_set_clientdata(client, tps61280);

	ret = regmap_read(tps61280->rmap, TPS61280_STATUS, &readout);
	if (ret < 0)
		dev_err(&client->dev, "STATUS read failed %d\n", ret);
	else
		dev_info(&client->dev, "power on status %#x\n", readout);

	ret = regmap_read(tps61280->rmap, TPS61280_REVISION, &readout);
	if (ret < 0)
		dev_err(&client->dev, "Revision read failed %d\n", ret);
	else
		dev_info(&client->dev, "Revision %#x\n", readout);

	ret = tps61280_config(tps61280);
	if (ret < 0) {
		dev_err(&client->dev, "Mode init failed: %d\n", ret);
		return ret;
	}

	dev_info(&client->dev, "DC-Boost probe successful\n");

	ret = regmap_read(tps61280->rmap, TPS61280_STATUS, &readout);
	if (ret < 0)
		dev_err(&client->dev, "STATUS read failed %d\n", ret);
	else
		dev_info(&client->dev, "current status %#x\n", readout);

	return 0;
}


static const struct of_device_id tps61280_dt_match[] = {
	{ .compatible = "ti,tps61280" },
	{ },
};
MODULE_DEVICE_TABLE(of, tps61280_dt_match);

static const struct i2c_device_id tps61280_i2c_id[] = {
	{ "tps61280", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tps61280_i2c_id);

static struct i2c_driver tps61280_driver = {
	.driver = {
		.name = "tps61280",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tps61280_dt_match),
	},
	.probe = tps61280_probe,
	.id_table = tps61280_i2c_id,
};

static int __init tps61280_init(void)
{
	return i2c_add_driver(&tps61280_driver);
}
fs_initcall_sync(tps61280_init);

static void __exit tps61280_exit(void)
{
	i2c_del_driver(&tps61280_driver);
}
module_exit(tps61280_exit);

MODULE_DESCRIPTION("simple tps61280 driver");
MODULE_ALIAS("i2c:tps61280");
MODULE_LICENSE("GPL v2");
