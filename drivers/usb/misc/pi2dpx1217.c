// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/extcon.h>
#include <linux/power_supply.h>
#include <linux/usb/ch9.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/regmap.h>
#include <linux/ctype.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>

/* priority: INT_MAX >= x >= 0 */
#define NOTIFIER_PRIORITY		1

/* Registers Address */
#define CHIP_VERSION_REG		0x00
#define DEVICE_ID_REG			0x01
#define GEN_DEV_CFG_REG			0x03
#define GEN_DEV_CTRL_REG		0x04
#define EQ_FG_BASE_REG			0x05
#define AUX_DP_FLIP_REG			0x09
#define REDRIVER_REG_MAX		0x1f

/* chip id */
#define DEVICE_ID			0x12

enum plug_orientation {
	ORIENTATION_NONE,
	ORIENTATION_CC1,
	ORIENTATION_CC2,
};

enum channel_mode {
	CHAN_MODE_USB,
	CHAN_MODE_DP,
	CHAN_MODE_MAX
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

struct pi2dpx1217_redriver {
	struct device		*dev;
	struct regmap		*regmap;
	struct i2c_client	*client;

	struct regulator *vcc;
	bool host_active;
	bool vbus_active;
	enum plug_orientation typec_orientation;
	enum operation_mode op_mode;

	struct extcon_dev	*extcon_usb;
	struct notifier_block	vbus_nb;
	struct notifier_block	id_nb;
	struct notifier_block	dp_nb;

	struct dentry *debug_root;

	unsigned char *eq_fg;
	enum	channel_mode chan_mode[CHAN_MODE_MAX];
};

static const char * const opmode_string[] = {
	[OP_MODE_NONE] = "NONE",
	[OP_MODE_USB] = "USB",
	[OP_MODE_DP] = "DP",
	[OP_MODE_USB_AND_DP] = "USB and DP",
	[OP_MODE_DEFAULT] = "DEFAULT",
};
#define OPMODESTR(x) opmode_string[x]

static int pi2dpx1217_i2c_reg_set(struct pi2dpx1217_redriver *redriver,
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

static int pi2dpx1217_gen_dev_set(struct pi2dpx1217_redriver *redriver)
{
	int ret;
	u8 cfg_val = 0;
	u8 ctrl_val = 0;

	dev_info(redriver->dev, "%s op_mode=%d\n", __func__, redriver->op_mode);

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
		if (redriver->typec_orientation
				== ORIENTATION_CC1) {
			cfg_val = 0x20;
			ctrl_val = 0x06;
		} else if (redriver->typec_orientation
				== ORIENTATION_CC2) {
			cfg_val = 0x30;
			ctrl_val = 0x06;
		} else {
			cfg_val = 0x20;
			ctrl_val = 0x06;
			dev_err(redriver->dev, "unknown DP orientation, typec_orientation=%d, set default\n", redriver->typec_orientation);
		}

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
			cfg_val = 0x60;
			ctrl_val = 0x06;
			dev_err(redriver->dev, "unknown USB AND DP orientation, typec_orientation=%d, set default\n", redriver->typec_orientation);
		}
		break;
	default:
		cfg_val = 0x04;
		ctrl_val = 0xf4;
		break;
	}

	ret = pi2dpx1217_i2c_reg_set(redriver, GEN_DEV_CFG_REG, cfg_val);
	if (ret < 0) {
		dev_err(redriver->dev, "Failed to set CFG_REG, ret=%d\n", ret);
		return ret;
	}

	ret = pi2dpx1217_i2c_reg_set(redriver, GEN_DEV_CTRL_REG, ctrl_val);
	if (ret < 0) {
		dev_err(redriver->dev, "Failed to set CTRL_REG, ret=%d\n", ret);
		return ret;
	}

	return 0;
}

static int pi2dpx1217_eq_fg_configure(struct pi2dpx1217_redriver *redriver)
{
	int i;
	int ret = 0;
	int chan_mode;

	for (i = 0; i < CHAN_TYPE_MAX && !ret; i++) {
		chan_mode = redriver->chan_mode[i];
		ret = pi2dpx1217_i2c_reg_set(redriver,
					EQ_FG_BASE_REG + i,
					redriver->eq_fg[i+chan_mode*CHAN_TYPE_MAX]);
	}

	return ret;
}

static int pi2dpx1217_enable_chip(struct pi2dpx1217_redriver *redriver, bool en)
{
	struct pinctrl *pinctrl;
	struct pinctrl_state *state;
	const char *pinctrl_name;
	int ret;

	pinctrl = devm_pinctrl_get(redriver->dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		dev_err(redriver->dev, "Failed to get pinctrl\n");
		return -EINVAL;
	}

	if (en)
		pinctrl_name = "enable_gpio";
	else
		pinctrl_name = "disable_gpio";

	dev_info(redriver->dev, "%s, en=%d,  %s state\n", __func__, en, pinctrl_name);
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
	devm_pinctrl_put(pinctrl);

	return ret;
}


static int pi2dpx1217_config_work_mode(struct pi2dpx1217_redriver *redriver,
	enum operation_mode op_mode)
{
	int ret = 0;

	dev_info(redriver->dev, "%s op_mode=%d\n", __func__, op_mode);

	if (op_mode && redriver->op_mode == op_mode) {
		dev_info(redriver->dev, "%s: USB SS redriver config work mode %d already\n",
			__func__, op_mode);

		return NOTIFY_OK;
	}

	redriver->op_mode = op_mode;

	if (redriver->op_mode == OP_MODE_NONE) {
		pi2dpx1217_enable_chip(redriver, false);
		return NOTIFY_OK;
	}

	switch (op_mode) {
		case OP_MODE_USB:
		case OP_MODE_DEFAULT:
			redriver->chan_mode[CHAN_TYPE_RX2] = CHAN_MODE_USB;
			redriver->chan_mode[CHAN_TYPE_TX2] = CHAN_MODE_USB;
			redriver->chan_mode[CHAN_TYPE_TX1] = CHAN_MODE_USB;
			redriver->chan_mode[CHAN_TYPE_RX1] = CHAN_MODE_USB;
			break;
		case OP_MODE_USB_AND_DP:
			if (redriver->typec_orientation == ORIENTATION_CC1) {
				redriver->chan_mode[CHAN_TYPE_RX2] = CHAN_MODE_DP;
				redriver->chan_mode[CHAN_TYPE_TX2] = CHAN_MODE_DP;
				redriver->chan_mode[CHAN_TYPE_TX1] = CHAN_MODE_USB;
				redriver->chan_mode[CHAN_TYPE_RX1] = CHAN_MODE_USB;
			} else {
				redriver->chan_mode[CHAN_TYPE_RX2] = CHAN_MODE_USB;
				redriver->chan_mode[CHAN_TYPE_TX2] = CHAN_MODE_USB;
				redriver->chan_mode[CHAN_TYPE_TX1] = CHAN_MODE_DP;
				redriver->chan_mode[CHAN_TYPE_RX1] = CHAN_MODE_DP;
			}
			break;
		case OP_MODE_DP:
			redriver->chan_mode[CHAN_TYPE_RX2] = CHAN_MODE_DP;
			redriver->chan_mode[CHAN_TYPE_TX2] = CHAN_MODE_DP;
			redriver->chan_mode[CHAN_TYPE_TX1] = CHAN_MODE_DP;
			redriver->chan_mode[CHAN_TYPE_RX1] = CHAN_MODE_DP;
			break;
		default:
			break;
	}

	pi2dpx1217_enable_chip(redriver, true);

	ret = pi2dpx1217_eq_fg_configure(redriver);
	if (ret) {
		dev_info(redriver->dev, "i2c bus may not resume(%d)\n", ret);
		return NOTIFY_DONE;
	}
	pi2dpx1217_gen_dev_set(redriver);

	return NOTIFY_OK;

}

static int pi2dpx1217_vbus_notifier(struct notifier_block *nb,
		unsigned long event, void *ptr)
{
	struct pi2dpx1217_redriver *redriver = container_of(nb,
			struct pi2dpx1217_redriver, vbus_nb);
	struct extcon_dev *edev = NULL;
	union extcon_property_value val;
	int state;

	dev_info(redriver->dev, "%s: vbus:%ld event received\n", __func__, event);

	if (redriver->vbus_active == event)
		return NOTIFY_DONE;

	redriver->vbus_active = event;
	edev = redriver->extcon_usb;

	if(edev) {
		state = extcon_get_state(edev, EXTCON_USB);
		if (state) {
			extcon_get_property(edev, EXTCON_USB,
					EXTCON_PROP_USB_TYPEC_POLARITY, &val);
			redriver->typec_orientation =
				val.intval ? ORIENTATION_CC2 : ORIENTATION_CC1;
			pi2dpx1217_config_work_mode(redriver, OP_MODE_USB);
		} else {
			pi2dpx1217_config_work_mode(redriver, OP_MODE_NONE);
		}
	} else {
		dev_err(redriver->dev, "vbus, extcon_usb err(%d)\n", edev);
	}

	return NOTIFY_DONE;
}

static int pi2dpx1217_id_notifier(struct notifier_block *nb,
		unsigned long event, void *ptr)
{
	struct pi2dpx1217_redriver *redriver = container_of(nb,
			struct pi2dpx1217_redriver, id_nb);
	struct extcon_dev *edev = NULL;
	union extcon_property_value val;
	int state;

	dev_info(redriver->dev, "%s: id:%ld event received\n", __func__, event);

	if (redriver->host_active == event)
		return NOTIFY_DONE;

	redriver->host_active = event;
	edev = redriver->extcon_usb;

	if(edev) {
		state = extcon_get_state(edev, EXTCON_USB_HOST);
		if (state) {
			extcon_get_property(edev, EXTCON_USB_HOST,
				EXTCON_PROP_USB_TYPEC_POLARITY, &val);
			redriver->typec_orientation =
				val.intval ? ORIENTATION_CC2 : ORIENTATION_CC1;
			pi2dpx1217_config_work_mode(redriver, OP_MODE_USB);

		} else {
			pi2dpx1217_config_work_mode(redriver, OP_MODE_NONE);
		}
	} else {
		dev_err(redriver->dev, "vbus, extcon_usb err(%x)\n", edev);
	}

	return NOTIFY_DONE;
}

static int pi2dpx1217_dp_notifier(struct notifier_block *nb,
		unsigned long dp_lane, void *ptr)
{
	struct pi2dpx1217_redriver *redriver = container_of(nb,
			struct pi2dpx1217_redriver, dp_nb);
	enum operation_mode op_mode = OP_MODE_NONE;
	struct extcon_dev *edev = NULL;
	union extcon_property_value val;
	int state;

	dev_info(redriver->dev, "%s: dp:%lu event received\n", __func__, dp_lane);

	switch (dp_lane) {
	case 0: /* cable disconnected */
		op_mode = OP_MODE_NONE;
		break;
	case 2:
		op_mode = OP_MODE_USB_AND_DP;
		break;
	case 4:
		op_mode = OP_MODE_DP;
		break;
	default:
		break;
	}

	if (redriver->op_mode == op_mode)
		return NOTIFY_DONE;

	switch (op_mode) {
		case OP_MODE_USB:
			pi2dpx1217_config_work_mode(redriver, OP_MODE_USB);
			break;
		case OP_MODE_USB_AND_DP:
			edev = redriver->extcon_usb;
			if(edev) {
				state = extcon_get_state(edev, EXTCON_USB_HOST);
				if (state) {
					extcon_get_property(edev, EXTCON_USB_HOST,
							EXTCON_PROP_USB_TYPEC_POLARITY, &val);
					redriver->typec_orientation =
						val.intval ? ORIENTATION_CC2 : ORIENTATION_CC1;
				}
			} else {
				dev_err(redriver->dev, "dp, extcon_usb err(%d)\n", edev);
			}
			pi2dpx1217_config_work_mode(redriver, OP_MODE_USB_AND_DP);
			break;
		case OP_MODE_DP:
			pi2dpx1217_config_work_mode(redriver, OP_MODE_DP);
			break;
		default:
			break;
	}

	redriver->op_mode = op_mode;

	return NOTIFY_DONE;
}


static int pi2dpx1217_extcon_register(struct pi2dpx1217_redriver *redriver,
		struct extcon_dev *edev)
{
	int ret = 0;

	redriver->extcon_usb = edev;

	redriver->vbus_nb.notifier_call = pi2dpx1217_vbus_notifier;
	redriver->vbus_nb.priority = NOTIFIER_PRIORITY;
	extcon_register_notifier(edev, EXTCON_USB, &redriver->vbus_nb);
	if (ret < 0) {
		dev_err(redriver->dev,
			"failed to register notifier for USB\n");
		return ret;
	}

	redriver->id_nb.notifier_call = pi2dpx1217_id_notifier;
	redriver->id_nb.priority = NOTIFIER_PRIORITY;
	ret = extcon_register_notifier(edev, EXTCON_USB_HOST, &redriver->id_nb);
	if (ret < 0) {
		dev_err(redriver->dev,
			"failed to register notifier for USB-HOST\n");
		return ret;
	}

	redriver->dp_nb.notifier_call = pi2dpx1217_dp_notifier;
	redriver->dp_nb.priority = NOTIFIER_PRIORITY;
	ret = extcon_register_blocking_notifier(edev, EXTCON_DISP_DP, &redriver->dp_nb);
	if (ret < 0) {
		dev_err(redriver->dev,
			"failed to register blocking notifier for DP\n");
		return ret;
	}

	/* Update initial VBUS/ID state from extcon */
	if (extcon_get_state(edev, EXTCON_USB)) {
		dev_info(redriver->dev, "vbus present before driver load\n");
		pi2dpx1217_vbus_notifier(&redriver->vbus_nb, true, edev);
	} else if (extcon_get_state(edev, EXTCON_USB_HOST)) {
		dev_info(redriver->dev, "id present before driver load\n");
		pi2dpx1217_id_notifier(&redriver->id_nb, true, edev);
	}

	return 0;
}

static void pi2dpx1217_debugfs_entries(
		struct pi2dpx1217_redriver *redriver)
{
	redriver->debug_root = debugfs_create_dir("pi2dpx1217_redriver", NULL);
	if (!redriver->debug_root) {
		dev_warn(redriver->dev, "Couldn't create debug dir\n");
		return;
	}

        debugfs_create_x8("usb1_rx2", 0644, redriver->debug_root,
					&redriver->eq_fg[CHAN_TYPE_RX2]);
        debugfs_create_x8("usb2_tx2", 0644, redriver->debug_root,
					&redriver->eq_fg[CHAN_TYPE_TX2]);
        debugfs_create_x8("usb3_tx1", 0644, redriver->debug_root,
					&redriver->eq_fg[CHAN_TYPE_TX1]);
        debugfs_create_x8("usb4_rx1", 0644, redriver->debug_root,
					&redriver->eq_fg[CHAN_TYPE_RX1]);
        debugfs_create_x8("dp1_rx2", 0644, redriver->debug_root,
					&redriver->eq_fg[CHAN_TYPE_MAX+CHAN_TYPE_RX2]);
        debugfs_create_x8("dp2_tx2", 0644, redriver->debug_root,
					&redriver->eq_fg[CHAN_TYPE_MAX+CHAN_TYPE_TX2]);
        debugfs_create_x8("dp3_tx1", 0644, redriver->debug_root,
					&redriver->eq_fg[CHAN_TYPE_MAX+CHAN_TYPE_TX1]);
        debugfs_create_x8("dp4_rx1", 0644, redriver->debug_root,
					&redriver->eq_fg[CHAN_TYPE_MAX+CHAN_TYPE_RX1]);
}

static const struct regmap_config pi2dpx1217_regmap = {
	.max_register = REDRIVER_REG_MAX,
	.reg_bits = 8,
	.val_bits = 8,
};

static ssize_t typec_cc_orientation_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pi2dpx1217_redriver *redriver = dev_get_drvdata(dev);

	if (redriver->typec_orientation == ORIENTATION_CC1)
		return scnprintf(buf, PAGE_SIZE, "CC1\n");
	if (redriver->typec_orientation == ORIENTATION_CC2)
		return scnprintf(buf, PAGE_SIZE, "CC2\n");

	return scnprintf(buf, PAGE_SIZE, "NONE\n");
}

static DEVICE_ATTR_RO(typec_cc_orientation);

static int pi2dpx1217_i2c_probe(struct i2c_client *client,
			       const struct i2c_device_id *dev_id)
{
	struct pi2dpx1217_redriver *redriver;
	struct device *dev = &client->dev;
	struct device_node *of = dev->of_node;
	struct extcon_dev *edev;
	u32 device_id;
	enum operation_mode op_mode = OP_MODE_NONE;
	int ret;

	if (!of_property_read_bool(of, "extcon")) {
		dev_err(dev, "need extcon property\n");
		return -EINVAL;
	}

	edev = extcon_get_edev_by_phandle(dev, 0);
	if (IS_ERR(edev)) {
		dev_err(dev, "extcon device not found, defer probe\n");
		return -EPROBE_DEFER;
	}

	redriver = devm_kzalloc(&client->dev, sizeof(struct pi2dpx1217_redriver),
			GFP_KERNEL);
	if (!redriver)
		return -ENOMEM;

	redriver->regmap = devm_regmap_init_i2c(client, &pi2dpx1217_regmap);
	if (IS_ERR(redriver->regmap)) {
		ret = PTR_ERR(redriver->regmap);
		dev_err(&client->dev,
			"Failed to allocate register map: %d\n", ret);
		goto err_detect;
	}

	redriver->dev = &client->dev;
	pi2dpx1217_enable_chip(redriver, true);

	ret = regmap_raw_read(redriver->regmap, DEVICE_ID_REG, &device_id, 1);
	if (ret != 0) {
		dev_err(dev, "%s,device id read failed:%d\n", __func__, ret);
		goto err_detect;
	} else if ((device_id&0xff) != DEVICE_ID) {
		dev_err(dev, "%s,device id unknown: 0x%x\n", __func__, device_id);
		goto err_detect;
	}
	dev_info(dev, "%s,device_id=0x%x\n", __func__, (device_id&0xff));

	redriver->vcc = devm_regulator_get_optional(&client->dev, "vcc");
	if (IS_ERR(redriver->vcc) || redriver->vcc == NULL) {
		dev_info(&client->dev, "Could not get vcc power regulator\n");
	} else {
		ret = regulator_enable(redriver->vcc);
		if (ret)
			dev_err(&client->dev, "Could not enable vcc power regulator\n");
	}

	i2c_set_clientdata(client, redriver);

	if (of_find_property(redriver->dev->of_node, "eq-fg", NULL)) {
		redriver->eq_fg = devm_kzalloc(&client->dev, (CHAN_MODE_MAX*CHAN_TYPE_MAX),
			GFP_KERNEL);
		if (!redriver->eq_fg) {
			dev_err(dev, "%s,eq_fg no mem\n", __func__);
			goto err_detect;
		}
		ret = of_property_read_u8_array(redriver->dev->of_node, "eq-fg",
				redriver->eq_fg, (CHAN_MODE_MAX*CHAN_TYPE_MAX));
		if (ret < 0) {
			dev_err(&client->dev,
				"Failed to read default configuration: %d\n", ret);
			return ret;
		}
	}

	if (of_property_read_bool(redriver->dev->of_node, "init-none")) {
		op_mode = OP_MODE_NONE;
		redriver->chan_mode[CHAN_TYPE_RX2] = CHAN_MODE_USB;
		redriver->chan_mode[CHAN_TYPE_TX2] = CHAN_MODE_USB;
		redriver->chan_mode[CHAN_TYPE_TX1] = CHAN_MODE_USB;
		redriver->chan_mode[CHAN_TYPE_RX1] = CHAN_MODE_USB;
	} else {
		op_mode = OP_MODE_DEFAULT;
	}

	pi2dpx1217_extcon_register(redriver, edev);

	pi2dpx1217_config_work_mode(redriver, op_mode);

	pi2dpx1217_debugfs_entries(redriver);

	device_create_file(redriver->dev, &dev_attr_typec_cc_orientation);

	return 0;

err_detect:
	dev_err(dev, "%s,probe failed.\n", __func__);
	devm_kfree(&client->dev, redriver);
	return ret;
}

static int pi2dpx1217_i2c_remove(struct i2c_client *client)
{
	struct pi2dpx1217_redriver *redriver = i2c_get_clientdata(client);

	debugfs_remove_recursive(redriver->debug_root);
	device_remove_file(redriver->dev, &dev_attr_typec_cc_orientation);
	redriver->debug_root = NULL;

	if (redriver->vcc) {
		if (regulator_is_enabled(redriver->vcc))
			regulator_disable(redriver->vcc);
		devm_regulator_put(redriver->vcc);
	}

	return 0;
}

static int __maybe_unused pi2dpx1217_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pi2dpx1217_redriver *redriver = i2c_get_clientdata(client);

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

	pi2dpx1217_enable_chip(redriver, false);

	return 0;
}

static int __maybe_unused pi2dpx1217_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pi2dpx1217_redriver *redriver = i2c_get_clientdata(client);

	dev_dbg(redriver->dev, "%s: SS USB redriver resume.\n",
			__func__);

	/* no suspend happen in following mode */
	if (redriver->op_mode == OP_MODE_DP ||
	    redriver->op_mode == OP_MODE_NONE ||
	    redriver->op_mode == OP_MODE_DEFAULT)
		return 0;

	pi2dpx1217_enable_chip(redriver, true);
	pi2dpx1217_eq_fg_configure(redriver);
	pi2dpx1217_gen_dev_set(redriver);

	return 0;
}

static SIMPLE_DEV_PM_OPS(pi2dpx1217_i2c_pm, pi2dpx1217_i2c_suspend,
			 pi2dpx1217_i2c_resume);

static void pi2dpx1217_i2c_shutdown(struct i2c_client *client)
{
	struct pi2dpx1217_redriver *redriver = i2c_get_clientdata(client);
	int ret;

	ret = pi2dpx1217_enable_chip(redriver, false);
	if (ret < 0)
		dev_err(&client->dev,
			"%s: fail to disable redriver.\n",
			__func__);
	else
		dev_dbg(&client->dev,
			"%s: successfully disable redriver.\n",
			__func__);
}

static const struct of_device_id pi2dpx1217_match_table[] = {
	{ .compatible = "diodes,pi2dpx1217",},
	{ },
};

static const struct i2c_device_id pi2dpx1217_i2c_id[] = {
	{ "diodes redriver", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, pi2dpx1217_i2c_id);

static struct i2c_driver pi2dpx1217_i2c_driver = {
	.driver = {
		.name	= "diodes redriver",
		.of_match_table	= pi2dpx1217_match_table,
		.pm	= &pi2dpx1217_i2c_pm,
	},

	.probe		= pi2dpx1217_i2c_probe,
	.remove		= pi2dpx1217_i2c_remove,

	.shutdown	= pi2dpx1217_i2c_shutdown,

	.id_table	= pi2dpx1217_i2c_id,
};

module_i2c_driver(pi2dpx1217_i2c_driver);

MODULE_DESCRIPTION("Diodes PI2DPX1217 USB Super Speed Linear Re-Driver Driver");
MODULE_LICENSE("GPL v2");
