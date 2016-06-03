/*
 * Copyright (c) 2015, LGE Inc. All rights reserved.
 *
 * TUSB320 USB TYPE-C Configuration Controller driver
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) "TUSB: %s: " fmt, __func__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/usb/class-dual-role.h>

#undef  __CONST_FFS
#define __CONST_FFS(_x) \
        ((_x) & 0x0F ? ((_x) & 0x03 ? ((_x) & 0x01 ? 0 : 1) :\
                                      ((_x) & 0x04 ? 2 : 3)) :\
                       ((_x) & 0x30 ? ((_x) & 0x10 ? 4 : 5) :\
                                      ((_x) & 0x40 ? 6 : 7)))

#undef  FFS
#define FFS(_x) \
        ((_x) ? __CONST_FFS(_x) : 0)

#undef  BITS
#define BITS(_end, _start) \
        ((BIT(_end) - BIT(_start)) + BIT(_end))

#undef  __BITS_GET
#define __BITS_GET(_byte, _mask, _shift) \
        (((_byte) & (_mask)) >> (_shift))

#undef  BITS_GET
#define BITS_GET(_byte, _bit) \
        __BITS_GET(_byte, _bit, FFS(_bit))

#undef  __BITS_SET
#define __BITS_SET(_byte, _mask, _shift, _val) \
        (((_byte) & ~(_mask)) | (((_val) << (_shift)) & (_mask)))

#undef  BITS_SET
#define BITS_SET(_byte, _bit, _val) \
        __BITS_SET(_byte, _bit, FFS(_bit), _val)

#undef  BITS_MATCH
#define BITS_MATCH(_byte, _bit) \
        (((_byte) & (_bit)) == (_bit))

#define TUBS320_CSR_REG_ID(nr)		(0x00 + (nr))
#define TUBS320_CSR_REG_ID_MAX		8

#define TUBS320_CSR_REG_08		0x08

#define TUBS320_DFP_POWER_MODE		BITS(7,6)
#define TUBS320_DFP_POWER_DEFAULT	0
#define TUBS320_DFP_POWER_MEDIUM	1
#define TUBS320_DFP_POWER_HIGH		2

#define TUBS320_UFP_POWER_MODE		BITS(5,4)
#define TUBS320_UFP_POWER_DEFAULT	0	/* 500/900mA */
#define TUBS320_UFP_POWER_MEDIUM	1	/* 1.5A */
#define TUBS320_UFP_POWER_ACC		2	/* Charge through accessory (500mA) */
#define TUBS320_UFP_POWER_HIGH		3	/* 3A */

#define TUBS320_ACC_CONN		BITS(3,1)
#define TUBS320_ACC_NOT_ATTACH		0
#define TUBS320_ACC_AUDIO_DFP		4
#define TUBS320_ACC_DEBUG_DFP		6

#define TUBS320_CSR_REG_09		0x09

#define TUBS320_ATTACH_STATE		BITS(7,6)
#define TUBS320_NOT_ATTACH		0
#define TUBS320_DFP_ATTACH_UFP		1
#define TUBS320_UFP_ATTACH_DFP		2
#define TUBS320_ATTACH_ACC		3

#define TUBS320_CABLE_DIR		BIT (5)

#define TUBS320_INT_STATE		BIT (4)
#define TUBS320_INT_INTR		0
#define TUBS320_INT_CLEAR		1

#define TUBS320_CSR_REG_0A		0x0A

#define TUBS320_MODE_SELECT		BITS(5,4)
#define TUBS320_MODE_DEFAULT		0
#define TUBS320_MODE_UFP		1
#define TUBS320_MODE_DFP		2
#define TUBS320_MODE_DRP		3
#define TUBS320_SOURCE_PREF		BITS(2,1)
#define TUBS320_SOURCE_TRY_SNK		1
#define TUBS320_SOURCE_TRY_SRC		3

#define TUBS320_SOFT_RESET		BIT(3)
#define TUBS320_RESET_VALUE		1

#define TUBS320_ENB_ENABLE		0
#define TUBS320_ENB_DISABLE		1
#define TUBS320_ENB_INTERVAL		100

#define TUBS320_I2C_RESET		1
#define TUBS320_GPIO_I2C_RESET		2

#define ROLE_SWITCH_TIMEOUT		1500

struct tusb320_data {
	unsigned int_gpio;
	unsigned enb_gpio;
	u8 select_mode;
	u8 dfp_power;
};

struct tusb320_chip {
	struct i2c_client *client;
	struct tusb320_data *pdata;
	int irq_gpio;
	u8 mode;
	u8 state;
	u8 cable_direction;
	u8 ufp_power;
	u8 accessory_mode;
	struct work_struct dwork;
	struct wake_lock wlock;
	struct power_supply *usb_psy;
        struct mutex mlock;
	struct dual_role_phy_instance *dual_role;
	struct dual_role_phy_desc *desc;
};

#define tusb_update_state(chip) \
	if (chip) { \
		wake_up_interruptible(&tusb_mode_switch); \
	}


DECLARE_WAIT_QUEUE_HEAD(tusb_mode_switch);

static int tusb320_write_masked_byte(struct i2c_client *client,
					u8 addr, u8 mask, u8 val)
{
	int rc;

	if (!mask) {
		/* no actual access */
		rc = -EINVAL;
		goto out;
	}

	rc = i2c_smbus_read_byte_data(client, addr);
	if (!IS_ERR_VALUE(rc)) {
		rc = i2c_smbus_write_byte_data(client,
		addr, BITS_SET((u8)rc, mask, val));
	}

out:
	return rc;
}

static int tusb320_read_device_id(struct tusb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	u8 buffer[TUBS320_CSR_REG_ID_MAX + 1];
	int rc = 0, i = 0;

	for (i = 0; i < TUBS320_CSR_REG_ID_MAX; i += 2) {
		rc = i2c_smbus_read_word_data(chip->client,
				TUBS320_CSR_REG_ID(i));
		if (IS_ERR_VALUE(rc)) {
			dev_err(cdev, "%s: failed to read REG_ID\n",
							__func__);
			return rc;
		}
		buffer[i] = rc & 0xFF;
		buffer[i+1] = (rc >> 8) & 0xFF;
	}

	buffer[i] = '\0';

	dev_info(cdev, "device id: %s\n", buffer);

	return 0;
}

static int tusb320_set_source_free(struct tusb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	rc = tusb320_write_masked_byte(chip->client,
		TUBS320_CSR_REG_0A,
		TUBS320_SOURCE_PREF,
		TUBS320_SOURCE_TRY_SNK);
	if (IS_ERR_VALUE(rc)) {
		dev_err(cdev, "failed to write SOURCE_PREF\n");
		return rc;
	}

	return rc;
}

static int tusb320_select_mode(struct tusb320_chip *chip, u8 sel_mode)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	if (sel_mode > TUBS320_MODE_DRP) {
		dev_err(cdev, "%s: sel_mode(%d) is unavailable\n",
				__func__, sel_mode);
		return -EINVAL;
	}


	if (chip->mode != sel_mode) {
		rc = tusb320_write_masked_byte(chip->client,
						TUBS320_CSR_REG_0A,
						TUBS320_MODE_SELECT,
						sel_mode);
		if (IS_ERR_VALUE(rc)) {
			dev_err(cdev, "failed to write MODE_SELECT\n");
			return rc;
		}

		chip->mode = sel_mode;
	}

	pr_warn(": mode (%d)\n", chip->mode);

	return rc;
}

static int tusb320_i2c_reset_device(struct tusb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	rc = tusb320_write_masked_byte(chip->client,
					TUBS320_CSR_REG_0A,
					TUBS320_SOFT_RESET,
					TUBS320_RESET_VALUE);
	if(IS_ERR_VALUE(rc)) {
		dev_err(cdev, "%s: failed to write REG_0A\n",
						__func__);
		return rc;
	}

	chip->mode = TUBS320_MODE_DEFAULT;
	chip->state = TUBS320_NOT_ATTACH;
	chip->cable_direction = 0;
	chip->ufp_power = TUBS320_UFP_POWER_DEFAULT;
	chip->accessory_mode = TUBS320_ACC_NOT_ATTACH;

	dev_dbg(cdev, "%s is done\n", __func__);

	return 0;
}

static int tusb320_gpio_reset_device(struct tusb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;

	gpio_set_value(chip->pdata->enb_gpio, TUBS320_ENB_DISABLE);
	msleep(TUBS320_ENB_INTERVAL);
	gpio_set_value(chip->pdata->enb_gpio, TUBS320_ENB_ENABLE);
	msleep(TUBS320_ENB_INTERVAL);

	chip->mode = TUBS320_MODE_DEFAULT;
	chip->state = TUBS320_NOT_ATTACH;
	chip->cable_direction = 0;
	chip->ufp_power = TUBS320_UFP_POWER_DEFAULT;
	chip->accessory_mode = TUBS320_ACC_NOT_ATTACH;

	dev_dbg(cdev, "%s is done\n", __func__);

	return 0;
}

static int tusb320_reset_device(struct tusb320_chip *chip, int mode)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	dev_dbg(cdev, "%s: mode(%d)\n", __func__, mode);

	switch (mode) {
	case TUBS320_I2C_RESET:
		rc = tusb320_i2c_reset_device(chip);
		if (!IS_ERR_VALUE(rc))
			break;
	case TUBS320_GPIO_I2C_RESET:
		tusb320_gpio_reset_device(chip);
		rc = tusb320_i2c_reset_device(chip);
		if (IS_ERR_VALUE(rc))
			dev_err(cdev, "%s: TUBS320_GPIO_I2C_RESET fails\n",
								__func__);
		break;
	default:
		rc = -EINVAL;
		dev_err(cdev, "%s: Invaild mode\n", __func__);
		break;
	}

	return rc;
}

#define TUSB320_DEV_ATTR(field, format_string)				\
	static ssize_t							\
field ## _show(struct device *dev, struct device_attribute *attr,	\
		char *buf)						\
{									\
	struct i2c_client *client = to_i2c_client(dev);			\
	struct tusb320_chip *chip = i2c_get_clientdata(client);		\
	\
	return snprintf(buf, PAGE_SIZE,					\
			format_string, chip->field);			\
}									\
static DEVICE_ATTR(field, S_IRUGO, field ## _show, NULL);

TUSB320_DEV_ATTR(state, "%u\n")
TUSB320_DEV_ATTR(accessory_mode, "%u\n")
TUSB320_DEV_ATTR(cable_direction, "%u\n")

ssize_t tusb320_show_mode(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tusb320_chip *chip = i2c_get_clientdata(client);

	return snprintf(buf, PAGE_SIZE, "%u\n", chip->mode);
}

static ssize_t tusb320_store_mode(struct device *dev,
				struct device_attribute *attr,
				const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tusb320_chip *chip = i2c_get_clientdata(client);
	unsigned mode = 0;
	int rc = 0;

	if (sscanf(buff, "%d", &mode) == 1) {
		rc = tusb320_select_mode(chip, (u8)mode);
		if (IS_ERR_VALUE(rc))
			return rc;

		return size;
	}

	return -EINVAL;
}
DEVICE_ATTR(mode, S_IRUGO | S_IWUSR, tusb320_show_mode, tusb320_store_mode);

static ssize_t tusb320_store_reset(struct device *dev,
				struct device_attribute *attr,
				const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tusb320_chip *chip = i2c_get_clientdata(client);
	unsigned state = 0;
	int rc = 0;

	if (sscanf(buff, "%u", &state) == 1) {
		rc = tusb320_reset_device(chip, state);
		if (IS_ERR_VALUE(rc))
			return rc;

		return size;
	}

	return -EINVAL;
}
DEVICE_ATTR(reset, S_IWUSR, NULL, tusb320_store_reset);

static int tusb320_create_devices(struct device *cdev)
{
	int ret = 0;

	ret = device_create_file(cdev, &dev_attr_state);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_state\n");
		ret = -ENODEV;
		goto err1;
	}

	ret = device_create_file(cdev, &dev_attr_accessory_mode);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_accessory_mode\n");
		ret = -ENODEV;
		goto err2;
	}

	ret = device_create_file(cdev, &dev_attr_cable_direction);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_cable_direction\n");
		ret = -ENODEV;
		goto err3;
	}

	ret = device_create_file(cdev, &dev_attr_mode);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_mode\n");
		ret = -ENODEV;
		goto err4;
	}

	ret = device_create_file(cdev, &dev_attr_reset);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_reset\n");
		ret = -ENODEV;
		goto err5;
	}

	return ret;

err5:
	device_remove_file(cdev, &dev_attr_mode);
err4:
	device_remove_file(cdev, &dev_attr_cable_direction);
err3:
	device_remove_file(cdev, &dev_attr_accessory_mode);
err2:
	device_remove_file(cdev, &dev_attr_state);
err1:
	return ret;
}

static void tusb320_destory_device(struct device *cdev)
{
	device_remove_file(cdev, &dev_attr_state);
	device_remove_file(cdev, &dev_attr_accessory_mode);
	device_remove_file(cdev, &dev_attr_cable_direction);
	device_remove_file(cdev, &dev_attr_mode);
	device_create_file(cdev, &dev_attr_reset);
}

static int tusb320_set_current_max(struct power_supply *psy,
					int icurrent)
{
	const union power_supply_propval ret = {icurrent,};

	if (psy->set_property)
		return psy->set_property(psy,
				POWER_SUPPLY_PROP_INPUT_CURRENT_MAX, &ret);

	return -ENXIO;
}

static bool switch_flag = 0;

static void tusb320_not_attach(struct tusb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;

	dev_dbg(cdev, "%s: state (%d)\n", __func__, chip->state);

	switch (chip->state) {
	case TUBS320_UFP_ATTACH_DFP:
		break;
	case TUBS320_DFP_ATTACH_UFP:
		break;
	case TUBS320_ATTACH_ACC:
		break;
	case TUBS320_NOT_ATTACH:
		break;
	default:
		dev_err(cdev, "%s: Invaild state\n", __func__);
		break;
	}

	if (switch_flag) {
		tusb320_select_mode(chip, TUBS320_MODE_DRP);
		tusb320_set_source_free(chip);
		tusb320_i2c_reset_device(chip);
		switch_flag = 0;
	}

	chip->state = TUBS320_NOT_ATTACH;
	chip->cable_direction = 0;
	chip->ufp_power = TUBS320_UFP_POWER_DEFAULT;
	chip->accessory_mode = TUBS320_ACC_NOT_ATTACH;

	tusb_update_state(chip);
	dual_role_instance_changed(chip->dual_role);
}

static void tusb320_dfp_attach_ufp(struct tusb320_chip *chip, u8 detail)
{
	struct device *cdev = &chip->client->dev;

	dev_dbg(cdev, "%s: state (%d)\n", __func__, chip->state);

	if (chip->mode == TUBS320_MODE_UFP) {
		dev_err(cdev, "%s: mode is UFP\n", __func__);
		return;
	}

	switch (chip->state) {
	case TUBS320_NOT_ATTACH:
		chip->state = TUBS320_DFP_ATTACH_UFP;
		break;
	case TUBS320_DFP_ATTACH_UFP:
		break;
	case TUBS320_UFP_ATTACH_DFP:
	case TUBS320_ATTACH_ACC:
	default:
		tusb320_reset_device(chip, TUBS320_I2C_RESET);
		dev_err(cdev, "%s: Invaild state\n", __func__);
		break;
	}

	tusb_update_state(chip);
	dual_role_instance_changed(chip->dual_role);
}

static void tusb320_ufp_attach_dfp(struct tusb320_chip *chip, u8 detail)
{
	struct device *cdev = &chip->client->dev;
	u8 ufp_pow = BITS_GET(detail, TUBS320_UFP_POWER_MODE);
	int limit = 0;

	dev_dbg(cdev, "%s: state (%d)\n", __func__, chip->state);

	if (chip->mode == TUBS320_MODE_DFP) {
		dev_err(cdev, "%s: mode is DFP\n", __func__);
		return;
	}

	dev_dbg(cdev, "%s: ufp_power [before(%d) vs after(%d)]\n",
			__func__, chip->ufp_power, ufp_pow);

	limit = (ufp_pow == TUBS320_UFP_POWER_HIGH ? 3000 :
			(ufp_pow == TUBS320_UFP_POWER_MEDIUM ? 1500 : 0));

	switch (chip->state) {
	case TUBS320_NOT_ATTACH:
		chip->state = TUBS320_UFP_ATTACH_DFP;
		chip->ufp_power = ufp_pow;
		tusb320_set_current_max(chip->usb_psy, limit);
		break;
	case TUBS320_UFP_ATTACH_DFP:
		if (chip->ufp_power != ufp_pow) {
			tusb320_set_current_max(chip->usb_psy, limit);
		}
		break;
	case TUBS320_DFP_ATTACH_UFP:
	case TUBS320_ATTACH_ACC:
	default:
		tusb320_reset_device(chip, TUBS320_I2C_RESET);
		dev_err(cdev, "%s: Invaild state\n", __func__);
		break;
	}

	tusb_update_state(chip);
	dual_role_instance_changed(chip->dual_role);
}

static void tusb320_attach_accessory_detail(struct tusb320_chip *chip,
						u8 acc_conn, u8 detail)
{
	struct device *cdev = &chip->client->dev;

	if (chip->accessory_mode == TUBS320_ACC_NOT_ATTACH) {
		chip->accessory_mode = acc_conn;

		switch (acc_conn) {
		case TUBS320_ACC_AUDIO_DFP:
			break;
		case TUBS320_ACC_DEBUG_DFP:
			break;
		default:
			tusb320_reset_device(chip, TUBS320_I2C_RESET);
			dev_err(cdev, "%s: Invaild state\n", __func__);
			break;
		}
	}
}

static void tusb320_attach_accessory(struct tusb320_chip *chip, u8 detail)
{
	struct device *cdev = &chip->client->dev;
	u8 acc_conn = BITS_GET(detail, TUBS320_ACC_CONN);

	dev_dbg(cdev, "%s: state (%d)\n", __func__, chip->state);
	dev_dbg(cdev, "%s: accessory_mode [before(%d) vs after(%d)]\n",
			__func__, chip->accessory_mode, acc_conn);

	switch (chip->state) {
	case TUBS320_NOT_ATTACH:
		chip->state = TUBS320_ATTACH_ACC;
		tusb320_attach_accessory_detail(chip, acc_conn, detail);
		break;
	case TUBS320_ATTACH_ACC:
		tusb320_attach_accessory_detail(chip, acc_conn, detail);
		break;
	case TUBS320_DFP_ATTACH_UFP:
	case TUBS320_UFP_ATTACH_DFP:
	default:
		tusb320_reset_device(chip, TUBS320_I2C_RESET);
		dev_err(cdev, "%s: Invaild state\n", __func__);
		break;
	}
}

static void tusb320_work_handler(struct work_struct *work)
{
	struct tusb320_chip *chip =
		container_of(work, struct tusb320_chip, dwork);
	struct device *cdev = &chip->client->dev;
	int ret;
	u8 reg09, reg08, state;

	wake_lock(&chip->wlock);

	/* Get status (reg8/reg9) */
	ret = i2c_smbus_read_word_data(chip->client, TUBS320_CSR_REG_08);
	if (IS_ERR_VALUE(ret)) {
		dev_err(cdev, "%s: failed to read REG_09\n", __func__);
		goto work_unlock;
	}
	reg08 = ret & 0xFF;
	reg09 = (ret >> 8) & 0xFF;

	dev_info(cdev, "reg08[0x%02x] , reg09[0x%02x]\n", reg08, reg09);

	/* Clear Interrupt */
	ret = tusb320_write_masked_byte(chip->client,
					TUBS320_CSR_REG_09,
					TUBS320_INT_STATE,
					TUBS320_INT_CLEAR);
	if (IS_ERR_VALUE(ret)) {
		dev_err(cdev, "%s: failed to write REG_09\n", __func__);
		goto work_unlock;
	}

	state = BITS_GET(reg09, TUBS320_ATTACH_STATE);
	chip->cable_direction = BITS_GET(reg09, TUBS320_CABLE_DIR);

	dev_dbg(cdev, "%s: [state %d] [direction: %d]\n",
				__func__, state, chip->cable_direction);

	switch (state) {
	case TUBS320_NOT_ATTACH:
		tusb320_not_attach(chip);
		break;
	case TUBS320_DFP_ATTACH_UFP:
		tusb320_dfp_attach_ufp(chip, reg08);
		break;
	case TUBS320_UFP_ATTACH_DFP:
		tusb320_ufp_attach_dfp(chip, reg08);
		break;
	case TUBS320_ATTACH_ACC:
		tusb320_attach_accessory(chip, reg08);
		break;
	default:
		tusb320_reset_device(chip, TUBS320_I2C_RESET);
		dev_err(cdev, "%s: Invalid state\n", __func__);
		break;
	}
work_unlock:
	wake_unlock(&chip->wlock);
}

static irqreturn_t tusb320_interrupt(int irq, void *data)
{
	struct tusb320_chip *chip = (struct tusb320_chip *)data;

	if (!chip) {
		pr_err("%s : called before init.\n", __func__);
		return IRQ_HANDLED;
	}

	dev_dbg(&chip->client->dev, "%s\n", __func__);

	schedule_work(&chip->dwork);

	return IRQ_HANDLED;
}

int tusb320_init_gpio(struct tusb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	if (gpio_is_valid(chip->pdata->int_gpio)) {
		rc = gpio_request(chip->pdata->int_gpio, "tusb-irq-pin");
		if (rc) {
			dev_err(cdev, "unable to request gpio [%d]\n",
							chip->pdata->int_gpio);
			goto err_request;
		}

		rc = gpio_direction_input(chip->pdata->int_gpio);
		if (rc) {
			dev_err(cdev, "unable to set gpio [%d]\n", chip->pdata->int_gpio);
			goto err_set;
		}
	}
	return rc;
err_set:
	gpio_free(chip->pdata->int_gpio);
err_request:
	return rc;
}

static void tusb320_free_gpio(struct tusb320_chip *chip)
{
	if (gpio_is_valid(chip->pdata->int_gpio))
		gpio_free(chip->pdata->int_gpio);
}

static int tusb320_parse_dt(struct device *cdev, struct tusb320_data *data)
{
	struct device_node *dev_node = cdev->of_node;
	int rc = 0;

	data->int_gpio = of_get_named_gpio(dev_node, "qcom,fusb-irq", 0);
	if (data->int_gpio < 0) {
		dev_err(cdev, "int_gpio is not available\n");
		rc = data->int_gpio;
		goto out;
	}

	rc = of_property_read_u8(dev_node,
				"tusb320,select-mode", &data->select_mode);
	if(rc || (data->select_mode > TUBS320_MODE_DRP)) {
		dev_err(cdev, "select_mode is not available and set default\n");
		data->select_mode = TUBS320_MODE_DEFAULT;
		rc = 0;
	}

	rc = of_property_read_u8(dev_node,
				"tusb320,dfp-power", &data->dfp_power);
	if(rc || (data->dfp_power > TUBS320_DFP_POWER_HIGH)) {
		dev_err(cdev, "dfp-power is not available and set default\n");
		data->dfp_power = TUBS320_DFP_POWER_DEFAULT;
		rc = 0;
	}

	pr_warn("select_mode: %d dfp_power %d\n",
			data->select_mode, data->dfp_power);

out:
	return rc;
}

static enum dual_role_property tusb_drp_properties[] = {
	DUAL_ROLE_PROP_MODE,
	DUAL_ROLE_PROP_PR,
	DUAL_ROLE_PROP_DR,
};

 /* Callback for "cat /sys/class/dual_role_usb/otg_default/<property>" */
static int dual_role_get_local_prop(struct dual_role_phy_instance *dual_role,
			enum dual_role_property prop,
			unsigned int *val)
{
	struct tusb320_chip *chip;
	struct i2c_client *client = dual_role_get_drvdata(dual_role);
	int ret = 0;

	if (!client)
		return -EINVAL;

	chip = i2c_get_clientdata(client);

	mutex_lock(&chip->mlock);
	if (chip->state == TUBS320_DFP_ATTACH_UFP) {
                /* attached src(DUT as DFP) */
		if (prop == DUAL_ROLE_PROP_MODE)
			*val = DUAL_ROLE_PROP_MODE_DFP;
		else if (prop == DUAL_ROLE_PROP_PR)
			*val = DUAL_ROLE_PROP_PR_SRC;
		else if (prop == DUAL_ROLE_PROP_DR)
			*val = DUAL_ROLE_PROP_DR_HOST;
		else
			ret = -EINVAL;
	} else if (chip->state == TUBS320_UFP_ATTACH_DFP) {
        	/* attached snk(DUT as UFP) */
		if (prop == DUAL_ROLE_PROP_MODE)
			*val = DUAL_ROLE_PROP_MODE_UFP;
		else if (prop == DUAL_ROLE_PROP_PR)
			*val = DUAL_ROLE_PROP_PR_SNK;
		else if (prop == DUAL_ROLE_PROP_DR)
			*val = DUAL_ROLE_PROP_DR_DEVICE;
		else
			ret = -EINVAL;
	} else {
		if (prop == DUAL_ROLE_PROP_MODE)
			*val = DUAL_ROLE_PROP_MODE_NONE;
		else if (prop == DUAL_ROLE_PROP_PR)
			*val = DUAL_ROLE_PROP_PR_NONE;
		else if (prop == DUAL_ROLE_PROP_DR)
			*val = DUAL_ROLE_PROP_DR_NONE;
		else
			ret = -EINVAL;
	}
	mutex_unlock(&chip->mlock);

	return ret;
}

/* Decides whether userspace can change a specific property */
static int dual_role_is_writeable(struct dual_role_phy_instance *drp,
				enum dual_role_property prop) {

	if (prop == DUAL_ROLE_PROP_MODE)
		return 1;
	else
		return 0;
}

/* Callback for "echo <value> >
 *                      /sys/class/dual_role_usb/<name>/<property>"
 * Block until the entire final state is reached.
 * Blocking is one of the better ways to signal when the operation
 * is done.
 * This function tries to switched to Attached.SRC or Attached.SNK
 * by forcing the mode into SRC or SNK.
 * On failure, we fall back to Try.SNK state machine.
 */
static int dual_role_set_prop(struct dual_role_phy_instance *dual_role,
				enum dual_role_property prop,
				const unsigned int *val) {
	struct tusb320_chip *chip;
	struct i2c_client *client = dual_role_get_drvdata(dual_role);
	u8 mode, target_state;
	int rc;
	struct device *cdev;
	long timeout;

	if (!client)
		return -EIO;

	chip = i2c_get_clientdata(client);
	cdev = &client->dev;

	if (prop == DUAL_ROLE_PROP_MODE) {
		if (*val == DUAL_ROLE_PROP_MODE_DFP) {
			dev_dbg(cdev, "%s: Setting SRC mode\n", __func__);
			mode = TUBS320_MODE_DFP;
			target_state = TUBS320_DFP_ATTACH_UFP;
		} else if (*val == DUAL_ROLE_PROP_MODE_UFP) {
			dev_dbg(cdev, "%s: Setting SNK mode\n", __func__);
			mode = TUBS320_MODE_UFP;
			target_state = TUBS320_UFP_ATTACH_DFP;
		} else {
			dev_err(cdev, "%s: Trying to set invalid mode\n",
								__func__);
			return -EINVAL;
		}
	} else {
		dev_err(cdev, "%s: Property cannot be set\n", __func__);
		return -EINVAL;
	}

	if (chip->state == target_state)
		return 0;

	mutex_lock(&chip->mlock);
        switch_flag = 0;
	rc = tusb320_select_mode(chip, mode);
	if (IS_ERR_VALUE(rc))
		dev_err(cdev, "failed to select mode\n");

	tusb320_i2c_reset_device(chip);
	mutex_unlock(&chip->mlock);

	timeout = wait_event_interruptible_timeout(tusb_mode_switch,
			chip->state == target_state,
			msecs_to_jiffies(ROLE_SWITCH_TIMEOUT));

	if (timeout > 0) {
		switch_flag = 1;
		return 0;
	}

	mutex_lock(&chip->mlock);
	rc = tusb320_select_mode(chip, TUBS320_MODE_DRP);
	if (IS_ERR_VALUE(rc))
		dev_err(cdev, "failed to select back mode\n");

	tusb320_set_source_free(chip);
	tusb320_i2c_reset_device(chip);
	mutex_unlock(&chip->mlock);

	switch_flag = 1;

	return -EIO;
}

static int tusb320_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tusb320_chip *chip;
	struct device *cdev = &client->dev;
	unsigned long flags;
	int ret = 0, is_active;
	struct power_supply *usb_psy;
	struct dual_role_phy_desc *desc;
	struct dual_role_phy_instance *dual_role;

	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		dev_err(cdev, "USB supply not found, deferring probe\n");
		return -EPROBE_DEFER;
	}

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA |
				I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_err(cdev, "smbus data not supported!\n");
		return -EIO;
	}

	chip = devm_kzalloc(cdev, sizeof(struct tusb320_chip), GFP_KERNEL);
	if (!chip) {
		dev_err(cdev, "can't alloc tusb320_chip\n");
		return -ENOMEM;
	}

	chip->client = client;
	i2c_set_clientdata(client, chip);

	if (&client->dev.of_node) {
		struct tusb320_data *data = devm_kzalloc(cdev,
				sizeof(struct tusb320_data), GFP_KERNEL);

		if (!data) {
			dev_err(cdev, "can't alloc tusb320_data\n");
			ret = -ENOMEM;
			goto err1;
		}

		chip->pdata = data;

		ret = tusb320_parse_dt(cdev, data);
		if (ret) {
			dev_err(cdev, "can't parse dt\n");
			goto err2;
		}
	} else {
		chip->pdata = client->dev.platform_data;
	}

	ret = tusb320_init_gpio(chip);
	if (ret) {
		dev_err(cdev, "failed to init gpio\n");
		goto err2;
	}

	chip->mode = TUBS320_MODE_DEFAULT;
	chip->state = TUBS320_NOT_ATTACH;
	chip->cable_direction = 0;
	chip->ufp_power = TUBS320_UFP_POWER_DEFAULT;
	chip->accessory_mode = TUBS320_ACC_NOT_ATTACH;
	chip->usb_psy = usb_psy;

	INIT_WORK(&chip->dwork, tusb320_work_handler);
	wake_lock_init(&chip->wlock, WAKE_LOCK_SUSPEND, "tusb320_wake");
        mutex_init(&chip->mlock);

	ret = tusb320_create_devices(cdev);
	if (IS_ERR_VALUE(ret)) {
		dev_err(cdev, "could not create devices\n");
		goto err3;
	}

	chip->irq_gpio = gpio_to_irq(chip->pdata->int_gpio);
	if (chip->irq_gpio < 0) {
		dev_err(cdev, "could not register int_gpio\n");
		ret = -ENXIO;
		goto err4;
	}

	ret = devm_request_irq(cdev, chip->irq_gpio, tusb320_interrupt,
			IRQF_TRIGGER_FALLING, "tusb320_int_irq", chip);
	if (ret) {
		dev_err(cdev, "failed to reqeust IRQ\n");
		goto err4;
	}

	/* Update initial interrupt state */
	local_irq_save(flags);
	is_active = !gpio_get_value(chip->pdata->int_gpio);
	local_irq_restore(flags);
	enable_irq_wake(chip->irq_gpio);

	ret = tusb320_read_device_id(chip);
	if (IS_ERR_VALUE(ret)) {
		dev_err(cdev, "failed to read device id\n");
	}

	ret = tusb320_set_source_free(chip);
	if (IS_ERR_VALUE(ret))
		dev_err(cdev, "failed to set src free as try snk\n");

	ret = tusb320_select_mode(chip, chip->pdata->select_mode);
	if (IS_ERR_VALUE(ret))
		dev_err(cdev, "failed to select mode and work as default\n");

	if (IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)) {
                pr_err("to support select mode\n");
		desc = devm_kzalloc(cdev, sizeof(struct dual_role_phy_desc),
					GFP_KERNEL);
		if (!desc) {
			dev_err(cdev, "unable to allocate dual role descriptor\n");
			goto err4;
		}

		desc->name = "otg_default";
		desc->supported_modes = DUAL_ROLE_SUPPORTED_MODES_DFP_AND_UFP;
		desc->get_property = dual_role_get_local_prop;
		desc->set_property = dual_role_set_prop;
		desc->properties = tusb_drp_properties;
		desc->num_properties = ARRAY_SIZE(tusb_drp_properties);
		desc->property_is_writeable = dual_role_is_writeable;
		dual_role = devm_dual_role_instance_register(cdev, desc);
		dual_role->drv_data = client;
		chip->dual_role = dual_role;
		chip->desc = desc;
	}

	pr_info("force to trigger a interrupt handler\n");
	schedule_work(&chip->dwork);

	return 0;

err4:
	tusb320_destory_device(cdev);
err3:
	wake_lock_destroy(&chip->wlock);
	tusb320_free_gpio(chip);
err2:
	if (&client->dev.of_node)
		devm_kfree(cdev, chip->pdata);
err1:
	i2c_set_clientdata(client, NULL);
	devm_kfree(cdev, chip);

	return ret;
}

static int tusb320_remove(struct i2c_client *client)
{
	struct tusb320_chip *chip = i2c_get_clientdata(client);
	struct device *cdev = &client->dev;

	if (!chip) {
		pr_err("%s : chip is null\n", __func__);
		return -ENODEV;
	}

	if (IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)) {
		devm_dual_role_instance_unregister(cdev, chip->dual_role);
		devm_kfree(cdev, chip->desc);
	}

	if (chip->irq_gpio > 0)
		devm_free_irq(cdev, chip->irq_gpio, chip);

	tusb320_destory_device(cdev);
	wake_lock_destroy(&chip->wlock);
	tusb320_free_gpio(chip);

	if (&client->dev.of_node)
		devm_kfree(cdev, chip->pdata);

	i2c_set_clientdata(client, NULL);
	devm_kfree(cdev, chip);

	return 0;
}

static int  tusb320_suspend(struct i2c_client *client, pm_message_t message)
{
	return 0;
}

static int  tusb320_resume(struct i2c_client *client)
{
	return 0;
}

static const struct of_device_id tusb320_id[] = {
		{.compatible = "tusb320"},
		{},
};
MODULE_DEVICE_TABLE(of, tusb320_id);

static const struct i2c_device_id tusb320_i2c_id[] = {
	{ "tusb320", 0 },
	{ }
};

static struct i2c_driver tusb320_i2c_driver = {
	.driver = {
		.name = "tusb320",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tusb320_id),
	},
	.probe = tusb320_probe,
	.remove = tusb320_remove,
	.suspend = tusb320_suspend,
	.resume = tusb320_resume,
	.id_table = tusb320_i2c_id,
};

static __init int tusb320_i2c_init(void)
{
	return i2c_add_driver(&tusb320_i2c_driver);
}

static __exit void tusb320_i2c_exit(void)
{
	i2c_del_driver(&tusb320_i2c_driver);
}

module_init(tusb320_i2c_init);
module_exit(tusb320_i2c_exit);

MODULE_AUTHOR("jaesung.woo@lge.com");
MODULE_DESCRIPTION("I2C bus driver for TUSB320 USB Type-C");
MODULE_LICENSE("GPL v2");
