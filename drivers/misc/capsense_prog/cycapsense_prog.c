/*
 * Source file for:
 * Cypress CapSense(TM) programming driver.
 *
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
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/firmware.h>
#include <linux/limits.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include "cycapsense_issp.h"

#define CYCAPSENSE_PROG_NAME	"cycapsense_prog"

struct cycapsense_ctrl_data {
	struct device *dev;
	struct issp_data issp_d;
};

struct cycapsense_ctrl_data *ctrl_data;

int cycapsense_fw_update(void)
{
	const struct firmware *fw = NULL;
	char *fw_name = NULL;
	unsigned int chs, fw_rev;
	struct hex_info *inf;
	int error = 0;

	if (ctrl_data == NULL || ctrl_data->dev == NULL) {
		pr_err("%s: Ctrl data not initialized\n", __func__);
		return -ENODEV;
	}

	inf = &ctrl_data->issp_d.inf;

	device_lock(ctrl_data->dev);
	gpio_direction_input(ctrl_data->issp_d.d_gpio);
	gpio_direction_output(ctrl_data->issp_d.c_gpio, 0);

	if (inf->fw_name != NULL && !strcmp(inf->fw_name, "erase")) {
		error = cycapsense_issp_erase(&ctrl_data->issp_d);
		goto fw_upd_end;
	}
	error = cycapsense_issp_verify_id(&ctrl_data->issp_d);
	if (error)
		goto fw_upd_end;

	fw_name = kzalloc(NAME_MAX, GFP_KERNEL);
	if (fw_name == NULL) {
		dev_err(ctrl_data->dev, "No memory for FW name\n");
		error = -ENOMEM;
		goto fw_upd_end;
	}
	if (inf->fw_name == NULL) {
		strlcpy(fw_name, ctrl_data->issp_d.name, NAME_MAX);
		strlcat(fw_name, ".hex", NAME_MAX);
	} else {
		strlcpy(fw_name, inf->fw_name, NAME_MAX);
	}
	if (request_firmware(&fw, fw_name, ctrl_data->dev) < 0 || !fw
		|| !fw->data || !fw->size) {
		dev_err(ctrl_data->dev, "Unable to get firmware %s\n", fw_name);
		error = -ENOENT;
		goto fw_upd_end;
	}
	error = cycapsense_issp_parse_hex(&ctrl_data->issp_d, fw->data,
					fw->size);
	if (error)
		goto fw_upd_end;

	error = cycapsense_issp_get_cs(&ctrl_data->issp_d, &chs);
	if (error)
		goto fw_upd_end;
	if (chs && (chs != inf->cs) && inf->fw_rev) {
		cycapsense_issp_get_fw_rev(&ctrl_data->issp_d, &fw_rev);
		dev_dbg(ctrl_data->dev,
			"Device checksum 0x%x ,Firmware rev. 0x%x\n",
					chs, fw_rev);
		/* Skip FW update if FW revision is matching */
		if (fw_rev == inf->fw_rev)
			chs = inf->cs;
	}

	if (inf->fw_name != NULL || chs != inf->cs) {
		/* force update regardless check sum, if user requested */
		dev_info(ctrl_data->dev,
			"Flashing firmware %s,cs 0x%x, rev 0x%x\n",
					fw_name, inf->cs, inf->fw_rev);
		error = cycapsense_issp_dnld(&ctrl_data->issp_d);
		if (!error)
			dev_info(ctrl_data->dev, "%s flashed successful\n",
						fw_name);
	} else
		dev_info(ctrl_data->dev,
			"Firmware revision is matching,No upgrade.\n");

fw_upd_end:
	if (inf->data != NULL) {
		kfree(inf->data);
		inf->data = NULL;
	}
	if (inf->s_data != NULL) {
		kfree(inf->s_data);
		inf->s_data = NULL;
	}
	if (fw != NULL)
		release_firmware(fw);
	if (fw_name != NULL)
		kfree(fw_name);
	/* Reset IC after download */
	gpio_set_value(ctrl_data->issp_d.rst_gpio, 1);
	usleep_range(1000, 2000);
	gpio_set_value(ctrl_data->issp_d.rst_gpio, 0);

	device_unlock(ctrl_data->dev);
	return error;
}
EXPORT_SYMBOL(cycapsense_fw_update);

int cycapsense_reset(void)
{
	if (ctrl_data == NULL || ctrl_data->dev == NULL) {
		pr_err("%s: Ctrl data not initialized\n", __func__);
		return -ENODEV;
	}
	dev_info(ctrl_data->dev, "Reset requested\n");
	device_lock(ctrl_data->dev);
	gpio_set_value(ctrl_data->issp_d.rst_gpio, 1);
	usleep_range(1000, 2000);
	gpio_set_value(ctrl_data->issp_d.rst_gpio, 0);
	device_unlock(ctrl_data->dev);
	return 0;
}
EXPORT_SYMBOL(cycapsense_reset);

static ssize_t cycapsense_fw_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	char *cp;

	if (!count)
		return -EINVAL;

	cp = memchr(buf, '\n', count);
	if (cp)
		*(char *)cp = 0;

	ctrl_data->issp_d.inf.fw_name = buf;

	if (!strcmp(buf, "reset")) {
		cycapsense_reset();
		return count;
	}

	if (!strcmp(buf, "1"))
		ctrl_data->issp_d.inf.fw_name = NULL;
	cycapsense_fw_update();
	ctrl_data->issp_d.inf.fw_name = NULL;
	return count;
}

static int cycapsense_validate_gpio(struct device *dev,
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
static DEVICE_ATTR(cycapsense_fw, 0220, NULL, cycapsense_fw_store);

static int cycapsense_prog_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int error;

	ctrl_data = devm_kzalloc(&pdev->dev,
		sizeof(struct cycapsense_ctrl_data), GFP_KERNEL);
	if (ctrl_data == NULL) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	ctrl_data->issp_d.rst_gpio = of_get_gpio(np, 0);
	error = cycapsense_validate_gpio(&pdev->dev,
		ctrl_data->issp_d.rst_gpio, "capsense_reset_gpio", 1, 0);
	if (error)
		return error;

	gpio_export(ctrl_data->issp_d.rst_gpio, false);

	ctrl_data->issp_d.c_gpio = of_get_gpio(np, 1);
	/*request only, direction == 2. Will be set by firmware loader*/
	error = cycapsense_validate_gpio(&pdev->dev,
		ctrl_data->issp_d.c_gpio, "capsense_sclk_gpio", 2, 0);
	if (error)
		return error;

	ctrl_data->issp_d.d_gpio = of_get_gpio(np, 2);
	/*request only, direction == 2. Will be set by firmware loader*/
	error = cycapsense_validate_gpio(&pdev->dev,
		ctrl_data->issp_d.d_gpio, "capsense_sdata_gpio", 2, 0);
	if (error)
		return error;

	error = of_property_read_string(np, "label", &ctrl_data->issp_d.name);
	if (error) {
		dev_err(&pdev->dev, "Error reading node label\n");
		return error;
	}
	error = of_property_read_u32(np, "silicon_id",
		&ctrl_data->issp_d.silicon_id);
	if (error) {
		dev_err(&pdev->dev, "Error reading node silicon id\n");
		return error;
	}
	error = of_property_read_u32(np, "block_size",
		&ctrl_data->issp_d.block_size);
	if (error) {
		dev_err(&pdev->dev, "Error reading node block size\n");
		return error;
	}
	error = of_property_read_u32(np, "num_of_blocks",
		&ctrl_data->issp_d.num_of_blocks);
	if (error) {
		dev_err(&pdev->dev, "Error reading node num_of_blocks\n");
		return error;
	}
	error = of_property_read_u32(np, "secure_bytes",
		&ctrl_data->issp_d.secure_bytes);
	if (error) {
		dev_err(&pdev->dev, "Error reading node secure_bytes\n");
		return error;
	}
	error = of_property_read_u32(np, "fw_rev_offset",
		&ctrl_data->issp_d.fw_rev_offset);
	if (error) {
		dev_err(&pdev->dev, "Firmware revision offset isn't set\n");
	} else
		if ((ctrl_data->issp_d.fw_rev_offset < 0) ||
			(ctrl_data->issp_d.fw_rev_offset >
			ctrl_data->issp_d.num_of_blocks *
			ctrl_data->issp_d.block_size))
				ctrl_data->issp_d.fw_rev_offset = 0;

	error = device_create_file(&pdev->dev, &dev_attr_cycapsense_fw);
	if (error < 0) {
		dev_err(&pdev->dev,
			"Create attr file for device failed (%d)\n", error);
		return error;
	}
	ctrl_data->dev = &pdev->dev;
	platform_set_drvdata(pdev, ctrl_data);

	dev_dbg(&pdev->dev, "Cypress CapSense probe complete\n");

	return 0;
}

static int cycapsense_prog_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id cycapsense_dt_match[] = {
	{.compatible = "cypress,cycapsense_prog",},
	{}
};
MODULE_DEVICE_TABLE(of, cycapsense_dt_match);

static struct platform_driver cycapsense_prog_driver = {
	.driver = {
		.name = CYCAPSENSE_PROG_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(cycapsense_dt_match),
	},
	.probe = cycapsense_prog_probe,
	.remove = cycapsense_prog_remove,
};


module_platform_driver(cycapsense_prog_driver);

MODULE_ALIAS("prog:cycapsense");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Cypress CapSense(R) programming driver");
MODULE_AUTHOR("Motorola Mobility LLC");
