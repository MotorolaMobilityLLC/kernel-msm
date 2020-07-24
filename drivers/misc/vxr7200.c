/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/kernel.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/rwlock.h>
#include <linux/leds.h>
#include "vxr7200.h"

enum power_state {
	POWER_STATE_POWERKEY = 0,
	POWER_STATE_ENABLE = 1,
};

struct vxr7200 {
	struct device *dev;
	struct device_node *host_node;

	u8 i2c_addr;
	int irq;
	u32 vxr_3v3_en;
	u32 led_5v_en;
	u32 led_drive_en1;
	u32 led_drive_en2;
	u32 display_1v8_en;
	u32 mipi_switch_1v8_en;
	u32 display_res1;

	struct i2c_client *i2c_client;

	struct regulator *vddio;
	struct regulator *lab;
	struct regulator *ibb;

	bool power_on;
	bool enable;
	bool usb_plugged;
	enum power_state state;
};

static bool dsi_way;
static struct vxr7200 *pdata;

static int vxr7200_read(struct vxr7200 *pdata, u8 *reg, u8 *buf, u32 size)
{
	struct i2c_client *client = pdata->i2c_client;
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 4,
			.buf = reg,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = size,
			.buf = buf,
		}
	};

	if (i2c_transfer(client->adapter, msg, 2) != 2) {
		pr_err("%s i2c read failed\n", __func__);
		return -EIO;
	}

	return 0;
}

static void turn_gpio(struct vxr7200 *pdata, int gpio, bool on)
{

	pr_debug("%s vxr7200_turn_gpio gpio:%d, on:%d\n", __func__, gpio, on);

	if (on) {
		gpio_direction_output(gpio, 0);
		gpio_set_value(gpio, 1);
		pr_debug("%s vxr7200 gpio:%d set to high\n", __func__, gpio);
	} else {
		gpio_direction_output(gpio, 1);
		gpio_set_value(gpio, 0);
		pr_debug("%s vxr7200 gpio:%d set to low\n", __func__, gpio);
	}
}

static void vxr7200_set_gpios(struct vxr7200 *pdata, bool turn_on)
{
	pr_debug("%s, turn_on:%d\n", __func__, turn_on);
	if (pdata) {
		if (pdata->state == POWER_STATE_POWERKEY) {
			turn_gpio(pdata, pdata->vxr_3v3_en, turn_on);
			turn_gpio(pdata, pdata->display_res1, turn_on);
			turn_gpio(pdata, pdata->display_1v8_en, turn_on);
			turn_gpio(pdata, pdata->mipi_switch_1v8_en, turn_on);
			if (!pdata->enable && turn_on)
				return;
		}
		turn_gpio(pdata, pdata->led_5v_en, turn_on);
		turn_gpio(pdata, pdata->led_drive_en1, turn_on);
		turn_gpio(pdata, pdata->led_drive_en2, turn_on);
	}
	return;
}

static ssize_t vxr7200_enable_show(struct device *dev,
				 struct device_attribute *attr,
				char *buf)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct vxr7200 *pdata = i2c_get_clientdata(client);


	if (pdata->enable)
		ret = scnprintf(buf, PAGE_SIZE, "display on\n");
	else
		ret = scnprintf(buf, PAGE_SIZE, "display off\n");

	return ret;
}

static ssize_t vxr7200_enable_store(struct device *dev,
				 struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct vxr7200 *pdata = i2c_get_clientdata(client);
	long val;
	int ret;

	ret = kstrtol(buf, 0, &val);
	pr_debug("%s, count:%d  val:%lx, buf:%s\n",
				__func__, (int)count, val, buf);

	if (val == 0x1) {
		pr_debug("%s Turning on Display\n", __func__);
		pdata->enable = true;
		pdata->state = POWER_STATE_ENABLE;
		vxr7200_set_gpios(pdata, true);
	} else if (val == 0x0) {
		pr_debug("%s Turning off Display\n",
						 __func__);
		pdata->state = POWER_STATE_ENABLE;
		pdata->enable = false;
		vxr7200_set_gpios(pdata, false);
	} else
		pr_err("%s Invalid value. Valid vals(1-on,0-off)\n",
				__func__);

	return count;
}

static DEVICE_ATTR_RW(vxr7200_enable);

static struct attribute *vxr7200_fs_attrs[] = {
	&dev_attr_vxr7200_enable.attr,
	NULL
};

static struct attribute_group vxr7200_fs_attr_group = {
	.attrs = vxr7200_fs_attrs,
};

void vxr7200_usb_event(bool cable_plug)
{
	pr_debug("%s, cable_plug:%d\n", __func__, cable_plug);
	if (pdata) {
		pdata->state = POWER_STATE_POWERKEY;
		if (cable_plug) {
			pdata->usb_plugged = true;
			vxr7200_set_gpios(pdata, true);
		} else {
			pdata->usb_plugged = false;
			vxr7200_set_gpios(pdata, false);
		}
	}
}
EXPORT_SYMBOL(vxr7200_usb_event);

static int vxr7200_parse_dt(struct device *dev,
				struct vxr7200 *pdata)
{
	struct device_node *np = dev->of_node;
	int rc = 0;

	pdata->vxr_3v3_en =
		of_get_named_gpio(np, "qcom,vxr_3v3_en", 0);
	if (!gpio_is_valid(pdata->vxr_3v3_en)) {
		pr_err("vxr7200_3v3_en gpio not specified\n");
		rc = -EINVAL;
	}
	if (gpio_request(pdata->vxr_3v3_en, "vxr_3v3_en")) {
		pr_err("vxr7200_3v3_en request gpio failed\n");
		rc = -EINVAL;
		goto gpio1Fail;
	}

	pdata->led_5v_en =
		of_get_named_gpio(np, "qcom,led-5v-en-gpio", 0);
	if (!gpio_is_valid(pdata->led_5v_en)) {
		pr_err("vxr7200_led_5v_en gpio not specified\n");
		rc = -EINVAL;
	}
	if (gpio_request(pdata->led_5v_en, "led_5v_en")) {
		pr_err("vxr7200_led_5v_en request gpio failed\n");
		rc = -EINVAL;
		goto gpio2Fail;
	}

	pdata->led_drive_en1 =
		of_get_named_gpio(np, "qcom,led-driver-en1-gpio", 0);
	if (!gpio_is_valid(pdata->led_drive_en1)) {
		pr_err("vxr7200_led_drive_en1 gpio not specified\n");
		rc = -EINVAL;
	}
	if (gpio_request(pdata->led_drive_en1, "led_drive_en1")) {
		pr_err("vxr7200_led_drive_en1 request gpio failed\n");
		rc = -EINVAL;
		goto gpio3Fail;
	}

	pdata->led_drive_en2 =
		of_get_named_gpio(np, "qcom,led-driver-en2-gpio", 0);
	if (!gpio_is_valid(pdata->led_drive_en2)) {
		pr_err("vxr7200_led_drive_en2 gpio not specified\n");
		rc = -EINVAL;
	}
	if (gpio_request(pdata->led_drive_en2, "led_drive_en2")) {
		pr_err("vxr7200_led_drive_en2 request gpio failed\n");
		rc = -EINVAL;
		goto gpio4Fail;
	}

	pdata->display_1v8_en =
		of_get_named_gpio(np, "qcom,1p8-en-gpio", 0);
	if (!gpio_is_valid(pdata->display_1v8_en)) {
		pr_err("vxr7200_display_1v8_en gpio not specified\n");
		rc = -EINVAL;
	}
	if (gpio_request(pdata->display_1v8_en, "display_1v8_en")) {
		pr_err("vxr7200_display_1v8_en request gpio failed\n");
		rc = -EINVAL;
		goto gpio5Fail;
	}

	pdata->mipi_switch_1v8_en =
		of_get_named_gpio(np, "qcom,switch-power-gpio", 0);
	if (!gpio_is_valid(pdata->mipi_switch_1v8_en)) {
		pr_err("vxr7200_mipi_switch_1v8_en gpio not specified\n");
		rc = -EINVAL;
	}
	if (gpio_request(pdata->mipi_switch_1v8_en, "mipi_switch_1v8_en")) {
		pr_err("vxr7200_mipi_switch_1v8_en request gpio failed\n");
		rc = -EINVAL;
		goto gpio6Fail;
	}

	pdata->display_res1 =
		of_get_named_gpio(np, "qcom,platform-reset-gpio", 0);
	if (!gpio_is_valid(pdata->display_res1)) {
		pr_err("vxr7200_display_res1 gpio not specified\n");
		rc = -EINVAL;
	}
	if (gpio_request(pdata->display_res1, "display_res1")) {
		pr_err("vxr7200_display_res1 request gpio failed\n");
		rc = -EINVAL;
		goto gpio7Fail;
	}
	return rc;

gpio7Fail:
	gpio_free(pdata->display_res1);
gpio6Fail:
	gpio_free(pdata->mipi_switch_1v8_en);
gpio5Fail:
	gpio_free(pdata->display_1v8_en);
gpio4Fail:
	gpio_free(pdata->led_drive_en2);
gpio3Fail:
	gpio_free(pdata->led_drive_en1);
gpio2Fail:
	gpio_free(pdata->led_5v_en);
gpio1Fail:
	gpio_free(pdata->vxr_3v3_en);

	return rc;
}

static void vxr7200_display_pwr_enable_vregs(struct vxr7200 *pdata)
{
	int rc = 0;
	pdata->vddio = devm_regulator_get(pdata->dev, "pm660_l11");
	rc = PTR_RET(pdata->vddio);
	if (rc) {
		pr_err("Failed to get pm660_l11 regulator %s\n", __func__);
		goto vddio_fail;
	}
	rc = regulator_set_load(pdata->vddio, 62000);
	if (rc < 0) {
		pr_err("Load setting failed for vddio %s\n", __func__);
		goto vddio_fail;
	}
	rc = regulator_enable(pdata->vddio);
	msleep(20);
	if (rc) {
		pr_err("enable failed for vddio, rc=%d %s\n", rc, __func__);
		goto vddio_fail;
	}

	pdata->lab = devm_regulator_get(pdata->dev, "lcdb_ldo");
	rc = PTR_RET(pdata->lab);
	if (rc) {
		pr_err("Failed to get lcdb_ldo_vreg regulator %s\n", __func__);
		goto lab_fail;
	}
	rc = regulator_set_load(pdata->lab, 100000);
	if (rc < 0) {
		pr_err("Load Setting failed for lab %s\n", __func__);
		goto lab_fail;
	}
	rc = regulator_enable(pdata->lab);
	if (rc) {
		pr_err("enable failed for lab, rc=%d %s\n", rc, __func__);
		goto lab_fail;
	}
	msleep(20);

	pdata->ibb = devm_regulator_get(pdata->dev, "lcdb_ncp");
	rc = PTR_RET(pdata->ibb);
	if (rc) {
		pr_err("Failed to get lcdb_ncp_vreg regulator %s\n", __func__);
		goto ibb_fail;
	}
	rc = regulator_set_load(pdata->ibb, 100000);
	if (rc < 0) {
		pr_err("Load Setting failed for ibb %s\n", __func__);
		goto ibb_fail;
	}
	rc = regulator_enable(pdata->ibb);
	if (rc) {
		pr_err("enable failed for ibb, rc=%d %s\n", rc, __func__);
		goto ibb_fail;
	}
	msleep(20);

	return;

ibb_fail:
	devm_regulator_put(pdata->ibb);
	(void)regulator_set_load(pdata->ibb, 100);
	(void)regulator_set_voltage(pdata->ibb, 0, 6000000);

lab_fail:
	(void)regulator_set_voltage(pdata->lab, 0, 6000000);
	(void)regulator_set_load(pdata->lab, 100);
	devm_regulator_put(pdata->lab);

vddio_fail:
	(void)regulator_set_load(pdata->vddio, 100);
	(void)regulator_set_voltage(pdata->vddio, 0, 1800000);
	devm_regulator_put(pdata->vddio);
}

static int vxr7200_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc;
	u8 reg[4] = {0x00, 0x20, 0x01, 0x60};
	u8 buf[4] = {0x00, 0x0, 0x0, 0x0};

	if (!client || !client->dev.of_node) {
		pr_err("%s invalid input\n", __func__);
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s device doesn't support I2C\n", __func__);
		return -ENODEV;
	}

	if (dsi_way)
		return -EINVAL;

	pdata = devm_kzalloc(&client->dev,
		sizeof(struct vxr7200), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->state = POWER_STATE_POWERKEY;
	pdata->enable = true;

	rc = vxr7200_parse_dt(&client->dev, pdata);
	if (rc) {
		pr_err("%s failed to parse device tree\n", __func__);
		goto err_dt_parse;
	}
	pdata->dev = &client->dev;
	pdata->i2c_client = client;

	rc = sysfs_create_group(&pdata->dev->kobj, &vxr7200_fs_attr_group);
	if (rc) {
		pr_err("%s unable to register vxr7200 sysfs nodes\n",
					 __func__);
		goto err_dt_parse;
	}


	vxr7200_display_pwr_enable_vregs(pdata);

	i2c_set_clientdata(client, pdata);
	dev_set_drvdata(&client->dev, pdata);

	//vxr7200_write(pdata, 0x0A, 0x02);//Enable 4-lane DP
	vxr7200_read(pdata, reg, buf, 4);//Enable 4-lane DP

	return rc;

err_dt_parse:
	devm_kfree(&client->dev, pdata);

	return rc;
}

static int vxr7200_remove(struct i2c_client *client)
{
	struct vxr7200 *pdata = i2c_get_clientdata(client);

	if (pdata) {
		sysfs_remove_group(&pdata->dev->kobj, &vxr7200_fs_attr_group);
		devm_kfree(&client->dev, pdata);
	}
	return 0;
}


static void vxr7200_shutdown(struct i2c_client *client)
{
	dev_info(&(client->dev), "shutdown");
}

static int vxr7200_pm_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct vxr7200 *pdata = i2c_get_clientdata(client);

	dev_info(dev, "suspend");
	pdata->state = POWER_STATE_POWERKEY;
	vxr7200_set_gpios(pdata, false);
	return 0;
}

static int vxr7200_pm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct vxr7200 *pdata = i2c_get_clientdata(client);

	dev_info(dev, "resume");
	pdata->state = POWER_STATE_POWERKEY;
	vxr7200_set_gpios(pdata, true);
	return 0;
}

static const struct dev_pm_ops vxr7200_dev_pm_ops = {
	.suspend	= vxr7200_pm_suspend,
	.resume	 = vxr7200_pm_resume,
	.poweroff	= vxr7200_pm_suspend,
};

static const struct i2c_device_id vxr7200_id_table[] = {
	{"vxr7200", 0},
	{}
};

static struct i2c_driver vxr7200_i2c_driver = {
	.probe = vxr7200_probe,
	.remove = vxr7200_remove,
	.shutdown = vxr7200_shutdown,
	.driver = {
		.name = "vxr7200",
		.owner = THIS_MODULE,
		.pm    = &vxr7200_dev_pm_ops,
	},
	.id_table = vxr7200_id_table,
};

static int __init vxr7200_init(void)
{
	char *cmdline;

	cmdline = strnstr(boot_command_line,
			 "msm_drm.dsi_display0=dsi_sim_vid_display",
						strlen(boot_command_line));
	if (cmdline) {
		pr_debug("%s DSI SIM mode, going to dp init cmdline:%s\n",
					__func__, cmdline);
		dsi_way = false;
	} else {
		pr_debug("%s DSI WAY, going to dsi init cmdline:%s\n",
					__func__, cmdline);
		dsi_way = true;
	}

	return 0;

}

device_initcall(vxr7200_init);
module_i2c_driver(vxr7200_i2c_driver);
MODULE_DEVICE_TABLE(i2c, vxr7200_id_table);
MODULE_DESCRIPTION("VXR7200 DP2DSI Bridge");
