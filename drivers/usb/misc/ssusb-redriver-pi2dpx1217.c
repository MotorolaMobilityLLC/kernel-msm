// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/usb/ch9.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/regmap.h>
#include <linux/ctype.h>
#include <linux/usb/ucsi_glink.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <linux/usb/redriver.h>
#include <linux/regulator/consumer.h>

/* priority: INT_MAX >= x >= 0 */
#define NOTIFIER_PRIORITY		1

/* Registers Address */
#define CHIP_VERSION_REG		0x00
#define GEN_DEV_CFG_REG			0x03
#define GEN_DEV_CTRL_REG		0x04
#define EQ_FG_BASE_REG			0x05
#define AUX_DP_FLIP_REG			0x09
#define REDRIVER_REG_MAX		0x1f

enum plug_orientation {
	ORIENTATION_CC1,
	ORIENTATION_CC2,
};

enum channel_type {
	CHAN_TYPE_RX2,
	CHAN_TYPE_TX2,
	CHAN_TYPE_TX1,
	CHAN_TYPE_RX1,
	CHAN_TYPE_MAX,
};

enum operation_mode {
	OP_MODE_NONE,		/* 4 lanes disabled */
	OP_MODE_USB,		/* 2 lanes for USB and 2 lanes disabled */
	OP_MODE_DP,		/* 4 lanes DP */
	OP_MODE_USB_AND_DP,	/* 2 lanes for USB and 2 lanes DP */
	OP_MODE_DEFAULT,	/* 4 lanes USB */
};

struct ssusb_redriver {
	struct device		*dev;
	struct regmap		*regmap;
	struct i2c_client	*client;

	struct regulator *vcc;
	int orientation_gpio;
	bool orientation_gpio_enable;
	enum plug_orientation typec_orientation;
	enum operation_mode op_mode;

	struct notifier_block ucsi_nb;

	u8 eq_fg[CHAN_TYPE_MAX];

	struct dentry *debug_root;
};

static const char * const opmode_string[] = {
	[OP_MODE_NONE] = "NONE",
	[OP_MODE_USB] = "USB",
	[OP_MODE_DP] = "DP",
	[OP_MODE_USB_AND_DP] = "USB and DP",
	[OP_MODE_DEFAULT] = "DEFAULT",
};
#define OPMODESTR(x) opmode_string[x]

static int redriver_i2c_reg_set(struct ssusb_redriver *redriver,
		u8 reg, u8 val)
{
	int ret;

	ret = regmap_write(redriver->regmap, (unsigned int)reg,
			(unsigned int)val);
	if (ret < 0) {
		dev_err(redriver->dev, "writing reg 0x%02x failure\n", reg);
		return ret;
	}

	dev_dbg(redriver->dev, "writing reg 0x%02x=0x%02x\n", reg, val);

	return 0;
}

static void ssusb_redriver_gen_dev_set(struct ssusb_redriver *redriver)
{
	u8 cfg_val = 0;
	u8 ctrl_val = 0;

	switch (redriver->op_mode) {
	case OP_MODE_DEFAULT:
		cfg_val = 0xc0;
		ctrl_val = 0x04;
		break;
	case OP_MODE_USB:
		/* Use source side I/O mapping */
		if (redriver->typec_orientation
				== ORIENTATION_CC1) {
			cfg_val = 0x40;
			ctrl_val = 0x34;
		} else if (redriver->typec_orientation
				== ORIENTATION_CC2) {
			cfg_val = 0x50;
			ctrl_val = 0xc4;
		} else {
			cfg_val = 0x04;
			ctrl_val = 0xf4;
		}
		break;
	case OP_MODE_DP:
		cfg_val = 0x30;
		ctrl_val = 0x06;
		break;
	case OP_MODE_USB_AND_DP:
		if (redriver->typec_orientation
				== ORIENTATION_CC1) {
			cfg_val = 0x60;
			ctrl_val = 0x06;
		} else if (redriver->typec_orientation
				== ORIENTATION_CC2) {
			cfg_val = 0x70;
			ctrl_val = 0x06;
		} else {
			cfg_val = 0x04;
			ctrl_val = 0xf4;
		}
		break;
	default:
		cfg_val = 0x04;
		ctrl_val = 0xf4;
		break;
	}

	redriver_i2c_reg_set(redriver, GEN_DEV_CFG_REG, cfg_val);
	redriver_i2c_reg_set(redriver, GEN_DEV_CTRL_REG, ctrl_val);
}

static void ssusb_redriver_eq_fg_configure(struct ssusb_redriver *redriver)
{
	int i;

	for (i = 0; i < CHAN_TYPE_MAX; i++) {
		redriver_i2c_reg_set(redriver, EQ_FG_BASE_REG + i, redriver->eq_fg[i]);
	}
}

static int ssusb_redriver_enable_chip(struct ssusb_redriver *redriver, bool en)
{
	struct pinctrl *pinctrl;
	struct pinctrl_state *state;
	const char *pinctrl_name;
	int ret;

	pinctrl = pinctrl_get(redriver->dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		dev_err(redriver->dev, "Failed to get pinctrl\n");
		return -EINVAL;
	}

	if (en)
		pinctrl_name = "enable_gpio";
	else
		pinctrl_name = "disable_gpio";

	state = pinctrl_lookup_state(pinctrl, pinctrl_name);
	if (IS_ERR_OR_NULL(state)) {
		dev_err(redriver->dev, "fail to get %s state\n", pinctrl_name);
		ret = -ENODEV;
		goto put_pinctrl;
	}

	ret = pinctrl_select_state(pinctrl, state);
	if (ret) {
		dev_err(redriver->dev, "fail to set %s state\n", pinctrl_name);
		ret = -EINVAL;
		goto put_pinctrl;
	}

	ret = 0;

put_pinctrl:
	pinctrl_put(pinctrl);

	return ret;
}

static int ssusb_redriver_read_orientation(struct ssusb_redriver *redriver)
{
	int ret;

	if (!redriver->orientation_gpio_enable)
		return -EINVAL;

	/* wait for some time ??? */
	ret = gpio_get_value(redriver->orientation_gpio);
	if (ret < 0) {
		dev_err(redriver->dev, "fail to read CC out, ret=%d\n", ret);
		return ret;
	}

	if (ret == 0)
		redriver->typec_orientation = ORIENTATION_CC1;
	else
		redriver->typec_orientation = ORIENTATION_CC2;

	return 0;
}

static int ssusb_redriver_ucsi_notifier(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct ssusb_redriver *redriver =
			container_of(nb, struct ssusb_redriver, ucsi_nb);
	struct ucsi_glink_constat_info *info = data;
	enum operation_mode op_mode;

	if (info->connect && !info->partner_change)
		return NOTIFY_DONE;

	if (!info->connect) {
		if (info->partner_usb || info->partner_alternate_mode)
			dev_err(redriver->dev, "set partner when no connection\n");
		op_mode = OP_MODE_NONE;
	} else if (info->partner_usb && info->partner_alternate_mode) {
		/*
		 * when connect a DP only cable,
		 * ucsi set usb flag first, then set usb and alternate mode
		 * after dp start link training.
		 * it should only set alternate_mode flag ???
		 */
		if (redriver->op_mode == OP_MODE_DP)
			return NOTIFY_OK;
		op_mode = OP_MODE_USB_AND_DP;
	} else if (info->partner_usb) {
		if (redriver->op_mode == OP_MODE_DP)
			return NOTIFY_OK;
		op_mode = OP_MODE_USB;
	} else if (info->partner_alternate_mode) {
		op_mode = OP_MODE_DP;
	} else
		op_mode = OP_MODE_NONE;

	if (redriver->op_mode == op_mode)
		return NOTIFY_OK;

	dev_info(redriver->dev, "op mode %s -> %s\n",
		OPMODESTR(redriver->op_mode), OPMODESTR(op_mode));
	redriver->op_mode = op_mode;

	if (redriver->op_mode == OP_MODE_NONE) {
		ssusb_redriver_enable_chip(redriver, false);
		return NOTIFY_OK;
	}

	ssusb_redriver_enable_chip(redriver, true);
	if (redriver->op_mode == OP_MODE_USB ||
			redriver->op_mode == OP_MODE_USB_AND_DP) {
		ssusb_redriver_read_orientation(redriver);

		dev_info(redriver->dev, "orientation %s\n",
			redriver->typec_orientation == ORIENTATION_CC1 ?
			"CC1" : "CC2");
	}

	ssusb_redriver_eq_fg_configure(redriver);
	ssusb_redriver_gen_dev_set(redriver);

	return NOTIFY_OK;
}

int redriver_release_usb_lanes(struct device_node *node)
{
	struct ssusb_redriver *redriver;
	struct i2c_client *client;

	if (!node)
		return -EINVAL;

	client = of_find_i2c_device_by_node(node);
	if (!client)
		return -EINVAL;

	redriver = i2c_get_clientdata(client);

	if (redriver->op_mode == OP_MODE_DP)
		return 0;

	dev_info(redriver->dev, "display notify 4 lane mode\n");
	dev_info(redriver->dev, "op mode %s -> %s\n",
		OPMODESTR(redriver->op_mode), OPMODESTR(OP_MODE_DP));
	redriver->op_mode = OP_MODE_DP;

	ssusb_redriver_enable_chip(redriver, true);
	ssusb_redriver_eq_fg_configure(redriver);
	ssusb_redriver_gen_dev_set(redriver);

	return 0;
}
EXPORT_SYMBOL(redriver_release_usb_lanes);

/* NOTE: DO NOT change mode in this funciton */
int redriver_gadget_pullup(struct device_node *node, int is_on)
{
	struct ssusb_redriver *redriver;
	struct i2c_client *client;

	if (!node)
		return -EINVAL;

	client = of_find_i2c_device_by_node(node);
	if (!client)
		return -EINVAL;

	redriver = i2c_get_clientdata(client);

	/*
	 * when redriver connect to a USB hub, and do adb root operation,
	 * due to redriver rx termination detection issue,
	 * hub will not detct device logical removal.
	 * workaround to temp disable/enable redriver when usb pullup operation.
	 */
	if (redriver->op_mode != OP_MODE_USB)
		return 0;

	if (is_on) {
		ssusb_redriver_enable_chip(redriver, true);
		ssusb_redriver_eq_fg_configure(redriver);
		ssusb_redriver_gen_dev_set(redriver);
	} else {
		ssusb_redriver_enable_chip(redriver, false);
	}

	return 0;
}
EXPORT_SYMBOL(redriver_gadget_pullup);

static void ssusb_redriver_orientation_gpio_init(
		struct ssusb_redriver *redriver)
{
	struct device *dev = redriver->dev;
	int rc;

	redriver->orientation_gpio =
			of_get_named_gpio(dev->of_node, "orientation_gpio", 0);
	if (redriver->orientation_gpio < 0) {
		dev_err(dev, "Failed to get gpio\n");
		return;
	}

	rc = devm_gpio_request(dev, redriver->orientation_gpio, "redriver");
	if (rc < 0) {
		dev_err(dev, "Failed to request gpio\n");
		return;
	}

	redriver->orientation_gpio_enable = true;
}

static void ssusb_redriver_debugfs_entries(
		struct ssusb_redriver *redriver)
{
	redriver->debug_root = debugfs_create_dir("ssusb_redriver", NULL);
	if (!redriver->debug_root) {
		dev_warn(redriver->dev, "Couldn't create debug dir\n");
		return;
	}

        debugfs_create_x8("rx2", 0644, redriver->debug_root,
					&redriver->eq_fg[CHAN_TYPE_RX2]);
        debugfs_create_x8("tx2", 0644, redriver->debug_root,
					&redriver->eq_fg[CHAN_TYPE_TX2]);
        debugfs_create_x8("tx1", 0644, redriver->debug_root,
					&redriver->eq_fg[CHAN_TYPE_TX1]);
        debugfs_create_x8("rx1", 0644, redriver->debug_root,
					&redriver->eq_fg[CHAN_TYPE_RX1]);
}

static const struct regmap_config redriver_regmap = {
	.max_register = REDRIVER_REG_MAX,
	.reg_bits = 8,
	.val_bits = 8,
};

static int redriver_i2c_probe(struct i2c_client *client,
			       const struct i2c_device_id *dev_id)
{
	struct ssusb_redriver *redriver;
	int ret;

	redriver = devm_kzalloc(&client->dev, sizeof(struct ssusb_redriver),
			GFP_KERNEL);
	if (!redriver)
		return -ENOMEM;

	redriver->regmap = devm_regmap_init_i2c(client, &redriver_regmap);
	if (IS_ERR(redriver->regmap)) {
		ret = PTR_ERR(redriver->regmap);
		dev_err(&client->dev,
			"Failed to allocate register map: %d\n", ret);
		return ret;
	}

	redriver->vcc = devm_regulator_get_optional(&client->dev, "vcc");
	if (IS_ERR(redriver->vcc) || redriver->vcc == NULL) {
		dev_err(&client->dev, "Could not get vcc power regulator\n");
	} else {
		ret = regulator_enable(redriver->vcc);
		if (ret)
			dev_err(&client->dev, "Could not enable vcc power regulator\n");
	}

	redriver->dev = &client->dev;
	i2c_set_clientdata(client, redriver);

	if (of_find_property(redriver->dev->of_node, "eq-fg", NULL)) {
		ret = of_property_read_u8_array(redriver->dev->of_node, "eq-fg",
				redriver->eq_fg, sizeof(redriver->eq_fg));
		if (ret < 0) {
			dev_err(&client->dev,
				"Failed to read default configuration: %d\n", ret);
			return ret;
		}
	}

	if (of_property_read_bool(redriver->dev->of_node, "init-none"))
		redriver->op_mode = OP_MODE_NONE;
	else
		redriver->op_mode = OP_MODE_DEFAULT;

	ssusb_redriver_orientation_gpio_init(redriver);
	ssusb_redriver_enable_chip(redriver, true);
	ssusb_redriver_read_orientation(redriver);
	ssusb_redriver_eq_fg_configure(redriver);
	ssusb_redriver_gen_dev_set(redriver);

	redriver->ucsi_nb.notifier_call = ssusb_redriver_ucsi_notifier;
	register_ucsi_glink_notifier(&redriver->ucsi_nb);

	ssusb_redriver_debugfs_entries(redriver);

	return 0;
}

static int redriver_i2c_remove(struct i2c_client *client)
{
	struct ssusb_redriver *redriver = i2c_get_clientdata(client);

	debugfs_remove_recursive(redriver->debug_root);
	redriver->debug_root = NULL;
	unregister_ucsi_glink_notifier(&redriver->ucsi_nb);

	if (redriver->vcc) {
		if (regulator_is_enabled(redriver->vcc))
			regulator_disable(redriver->vcc);
		devm_regulator_put(redriver->vcc);
	}

	return 0;
}

static int __maybe_unused redriver_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ssusb_redriver *redriver = i2c_get_clientdata(client);

	dev_dbg(redriver->dev, "%s: SS USB redriver suspend.\n",
			__func__);

	/*
	 * 1. when in 4 lanes display mode, it can't disable;
	 * 2. when in NONE mode, there is no need to re-disable;
	 * 3. when in DEFAULT mode, there is no adsp and can't disable;
	 */
	if (redriver->op_mode == OP_MODE_DP ||
	    redriver->op_mode == OP_MODE_NONE ||
	    redriver->op_mode == OP_MODE_DEFAULT)
		return 0;

	redriver->op_mode = OP_MODE_NONE;
	ssusb_redriver_enable_chip(redriver, false);

	return 0;
}

static int __maybe_unused redriver_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ssusb_redriver *redriver = i2c_get_clientdata(client);

	dev_dbg(redriver->dev, "%s: SS USB redriver resume.\n",
			__func__);

	return 0;
}

static SIMPLE_DEV_PM_OPS(redriver_i2c_pm, redriver_i2c_suspend,
			 redriver_i2c_resume);

static void redriver_i2c_shutdown(struct i2c_client *client)
{
	struct ssusb_redriver *redriver = i2c_get_clientdata(client);
	int ret;

	ret = ssusb_redriver_enable_chip(redriver, false);
	if (ret < 0)
		dev_err(&client->dev,
			"%s: fail to disable redriver.\n",
			__func__);
	else
		dev_dbg(&client->dev,
			"%s: successfully disable redriver.\n",
			__func__);
}

static const struct of_device_id redriver_match_table[] = {
	{ .compatible = "diodes,redriver",},
	{ },
};

static const struct i2c_device_id redriver_i2c_id[] = {
	{ "ssusb redriver", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, redriver_i2c_id);

static struct i2c_driver redriver_i2c_driver = {
	.driver = {
		.name	= "ssusb redriver",
		.of_match_table	= redriver_match_table,
		.pm	= &redriver_i2c_pm,
	},

	.probe		= redriver_i2c_probe,
	.remove		= redriver_i2c_remove,

	.shutdown	= redriver_i2c_shutdown,

	.id_table	= redriver_i2c_id,
};

module_i2c_driver(redriver_i2c_driver);

MODULE_DESCRIPTION("USB Super Speed Linear Re-Driver Driver");
MODULE_LICENSE("GPL v2");
