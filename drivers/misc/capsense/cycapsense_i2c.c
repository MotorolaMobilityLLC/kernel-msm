/*
 * Source file for:
 * Cypress CapSense(TM) drivers.
 * Supported parts include:
 * CY8C20237
 *
 * Copyright (C) 2009-2011 Cypress Semiconductor, Inc.
 * Copyright (c) 2012-2013 Motorola Mobility LLC
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
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/firmware.h>
#include <linux/limits.h>
#include <linux/uaccess.h>
#include "cycapsense_issp.h"
#include "cycapsense.h"


struct cycapsense_i2c {
	struct i2c_client *client;
	struct cycapsense_platform_data *pdata;
	struct issp_data issp_d;
	struct work_struct fw_update_w;
	struct completion fw_completion;
};

static void cycapsense_fw_update(struct work_struct *w)
{
	struct cycapsense_i2c *cs = container_of(w, struct cycapsense_i2c,
							fw_update_w);
	struct device *dev = &cs->client->dev;
	const struct firmware *fw = NULL;
	char *fw_name = NULL;
	unsigned int chs;
	struct hex_info *inf = &cs->issp_d.inf;

	device_lock(dev);

	gpio_direction_input(cs->issp_d.d_gpio);
	gpio_direction_output(cs->issp_d.c_gpio, 0);

	if (cs->issp_d.inf.fw_name != NULL
		&& !strcmp(cs->issp_d.inf.fw_name, "erase")) {
		cycapsense_issp_erase(&cs->issp_d);
		goto fw_upd_all_end;
	}

	cs->issp_d.id = cycapsense_issp_verify_id(&cs->issp_d);
	if (cs->issp_d.id == NULL)
		goto fw_upd_all_end;

	fw_name = kzalloc(NAME_MAX, GFP_KERNEL);
	if (fw_name == NULL) {
		dev_err(dev, "No memory for FW name\n");
		goto fw_upd_all_end;
	}
	if (inf->fw_name == NULL) {
		strlcpy(fw_name, cs->issp_d.id->name, NAME_MAX);
		strlcat(fw_name, ".hex", NAME_MAX);
	} else {
		strlcpy(fw_name, inf->fw_name, NAME_MAX);
	}
	if (request_firmware(&fw, fw_name, dev) < 0 || !fw || !fw->data
		|| !fw->size) {
		dev_err(dev, "Unable to get firmware %s\n", fw_name);
		goto fw_upd_all_end;
	}
	if (cycapsense_issp_parse_hex(inf, fw->data, fw->size, cs->issp_d.id)
		|| cycapsense_issp_get_cs(&cs->issp_d, &chs))
		goto fw_upd_all_end;

	if (inf->fw_name != NULL || chs != inf->cs) {
		/* force update regardless check sum, if user requested */
		dev_info(dev, "Flashing firmware %s\n", fw_name);
		if (!cycapsense_issp_dnld(&cs->issp_d))
			dev_info(dev, "%s flashed successful\n", fw_name);
	} else
		dev_info(dev, "Checksum is matching. No firmware upgrade.\n");


fw_upd_all_end:
	if (inf->data != NULL) {
		kfree(inf->data);
		inf->data = NULL;
	}
	if (inf->s_data != NULL) {
		kfree(inf->s_data);
		inf->s_data = NULL;
	}
	if (fw_name != NULL)
		kfree(fw_name);
	complete(&cs->fw_completion);
	device_unlock(&cs->client->dev);
}

static ssize_t cycapsense_fw_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct cycapsense_i2c *cs = dev_get_drvdata(dev);
	char *cp;

	if (!count)
		return -EINVAL;

	cp = memchr(buf, '\n', count);
	if (cp)
		*(char *)cp = 0;

	cs->issp_d.inf.fw_name = buf;

	INIT_COMPLETION(cs->fw_completion);
	schedule_work(&cs->fw_update_w);
	wait_for_completion_interruptible(&cs->fw_completion);
	cs->issp_d.inf.fw_name = NULL;

	return count;
}

static int __devinit cycapsense_validate_gpio(struct device *dev,
			int gpio, char *name, int dir_out, int out_val)
{
	int error;
	error = devm_gpio_request(dev, gpio, name);
	if (error) {
		dev_err(dev, "unable to request gpio [%d]\n", gpio);
		goto f_end;
	}
	if (dir_out == 2)
		goto f_end;

	if (dir_out == 1)
		error = gpio_direction_output(gpio, out_val);
	else
		error = gpio_direction_input(gpio);
	if (error) {
		dev_err(dev, "unable to set direction for gpio [%d]\n", gpio);
		goto f_end;
	}
f_end:
	return error;
}
static DEVICE_ATTR(cycapsense_fw, 0664, NULL, cycapsense_fw_store);

static int __devinit cycapsense_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct cycapsense_i2c *cs;
	struct cycapsense_platform_data *pdata;
	struct device_node *np = client->dev.of_node;
	int error;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -EIO;

	if (np) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct cycapsense_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}
		pdata->irq_gpio = of_get_gpio(np, 0);
		error = cycapsense_validate_gpio(&client->dev,
			pdata->irq_gpio, "capsense_irq_gpio", 0, 0);
		if (error)
			return error;
		pdata->irq = client->irq = gpio_to_irq(pdata->irq_gpio);

		pdata->reset_gpio = of_get_gpio(np, 1);
		error = cycapsense_validate_gpio(&client->dev,
			pdata->reset_gpio, "capsense_reset_gpio", 1, 0);
		if (error)
			return error;

		pdata->sclk_gpio = of_get_gpio(np, 2);
		/*request only, direction == 2. Will be set by firmware loader*/
		error = cycapsense_validate_gpio(&client->dev,
			pdata->sclk_gpio, "capsense_sclk_gpio", 2, 0);
		if (error)
			return error;

		pdata->sdata_gpio = of_get_gpio(np, 3);
		/*request only, direction == 2. Will be set by firmware loader*/
		error = cycapsense_validate_gpio(&client->dev,
			pdata->sdata_gpio, "capsense_sdata_gpio", 2, 0);
		if (error)
			return error;

	} else
		pdata = client->dev.platform_data;

	if (!pdata)
		return -EINVAL;

	error = device_create_file(&client->dev, &dev_attr_cycapsense_fw);
	if (error < 0) {
		dev_err(&client->dev,
			"Create attr file for device failed (%d)\n", error);
		return error;
	}

	/* allocate and clear memory */
	cs = devm_kzalloc(sizeof(*cs), GFP_KERNEL);
	if (!cs) {
		dev_err(&client->dev, "%s: Error, devm_kzalloc.\n", __func__);
		return -ENOMEM;
	}
	init_completion(&cs->fw_completion);
	INIT_WORK(&cs->fw_update_w, cycapsense_fw_update);

	cs->client = client;
	cs->pdata = pdata;
	cs->issp_d.c_gpio = pdata->sclk_gpio;
	cs->issp_d.d_gpio = pdata->sdata_gpio;
	cs->issp_d.rst_gpio = pdata->reset_gpio;
	i2c_set_clientdata(client, cs);

	schedule_work(&cs->fw_update_w);
	dev_dbg(&client->dev, "Cypress CapSense probe complete\n");

	return 0;
}

static int __devexit cycapsense_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static const struct of_device_id cycapsense_dt_match[] = {
	{.compatible = "cypress,cy8c20237_i2c"},
	{}
};
MODULE_DEVICE_TABLE(of, cycapsense_dt_match);

static const struct i2c_device_id cycapsense_i2c_id[] = {
	{ CYCAPSENSE_I2C_NAME, 0 },  { }
};

static struct i2c_driver cycapsense_i2c_driver = {
	.driver = {
		.name = CYCAPSENSE_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(cycapsense_dt_match),
	},
	.probe = cycapsense_i2c_probe,
	.remove = __devexit_p(cycapsense_i2c_remove),
	.id_table = cycapsense_i2c_id,
};

static int __init cycapsense_i2c_init(void)
{
	return i2c_add_driver(&cycapsense_i2c_driver);
}

static void __exit cycapsense_i2c_exit(void)
{
	return i2c_del_driver(&cycapsense_i2c_driver);
}

module_init(cycapsense_i2c_init);
module_exit(cycapsense_i2c_exit);

MODULE_ALIAS("i2c:cycapsense");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Cypress CapSense(R) I2C driver");
MODULE_AUTHOR("Motorola LLC");
