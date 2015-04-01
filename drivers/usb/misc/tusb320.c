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
#include <linux/wakelock.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/usb/tusb320_notify.h>

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

#define TUBS320_ACC_CONN		BITS(3,1)
#define TUBS320_ACC_NOT_ATTACH		0

#define TUBS320_CSR_REG_09		0x09

#define TUBS320_ATTACH_STATE		BITS(7,6)

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

#define TUBS320_SOFT_RESET		BIT(3)
#define TUBS320_RESET_VALUE		1

#define TUBS320_ENB_ENABLE		0
#define TUBS320_ENB_DISABLE		1
#define TUBS320_ENB_INTERVAL		100

#define TUBS320_I2C_RESET		1
#define TUBS320_GPIO_I2C_RESET		2

struct tusb320_data {
	unsigned int_gpio;
	unsigned enb_gpio;
	u8 select_mode;
	u8 dfp_power;
};

struct tusb320_chip {
	struct i2c_client *client;
	struct tusb320_data *pdata;
	struct tusb320_notify_param notify_param;
	int irq_gpio;
	u8 mode;
	u8 state;
	u8 cable_direction;
	u8 ufp_power;
	u8 accessory_mode;
	struct work_struct dwork;
	struct wake_lock wlock;
};

/*
 * Notify TUSB320 CC Controller events to USB or Other devices
 */
static BLOCKING_NOTIFIER_HEAD(tusb320_notifier_list);

int tusb320_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&tusb320_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(tusb320_register_notifier);

int tusb320_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&tusb320_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(tusb320_unregister_notifier);

static void tusb320_update_attach_state(struct tusb320_chip *chip, u8 ufp_pow)
{
	switch (chip->state) {
	case TUBS320_NOT_ATTACH:
	case TUBS320_DFP_ATTACH_UFP:
	case TUBS320_UFP_ATTACH_DFP:
		chip->notify_param.state = chip->state;
		break;
	case TUBS320_ATTACH_ACC:
		chip->notify_param.state = chip->accessory_mode;
	default:
		return;
	}

	chip->notify_param.ufp_power = ufp_pow;

	blocking_notifier_call_chain(&tusb320_notifier_list,
			TUBS320_ATTACH_STATE_EVENT, &chip->notify_param);
}

static void tusb320_update_ufp_power(struct tusb320_chip *chip, u8 ufp_pow)
{
	if(chip->ufp_power != ufp_pow) {
		chip->ufp_power = chip->notify_param.ufp_power
			= ufp_pow;

		blocking_notifier_call_chain(&tusb320_notifier_list,
				TUBS320_UFP_POWER_UPDATE_EVENT,
				&chip->notify_param);
	}
}

#if 0
static int tusb320_read_masked_byte(struct i2c_client *client,
		u8 addr, u8 mask, u8 *val)
{
	int rc;

	if (!mask) {
		/* no actual access */
		*val = 0;
		rc = -EINVAL;
		goto out;
	}

	rc = i2c_smbus_read_byte_data(client, addr);
	if (!IS_ERR_VALUE(rc)) {
		*val = BITS_GET((u8)rc, mask);
	}

out:
	return rc;
}
#endif

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
			dev_err(cdev, "%s: failed to read REG_ID\n", __func__);
			return rc;
		}
		buffer[i] = rc & 0xFF;
		buffer[i+1] = (rc >> 8) & 0xFF;
	}

	buffer[i] = '\0';

	dev_info(cdev, "device id: %s\n", buffer);

	return 0;
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

	/* mode selection is possibel only in standby/unattached mode */
	if (chip->state != TUBS320_NOT_ATTACH) {
		dev_err(cdev, "%s: unavailable in attached state (%d)\n",
				__func__, chip->state);
		return -EPERM;
	}

	if (chip->mode != sel_mode) {
		rc = tusb320_write_masked_byte(chip->client,
						TUBS320_CSR_REG_0A,
						TUBS320_MODE_SELECT,
						sel_mode);
		if (IS_ERR_VALUE(rc)) {
			dev_err(cdev, "failed to write TUBS320_MODE_SELECT\n");
			return rc;
		}

		chip->mode = sel_mode;
	}

	dev_dbg(cdev, "%s: mode (%d)\n", __func__, chip->mode);

	return rc;
}

static int tusb320_set_dfp_power(struct tusb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	rc = tusb320_write_masked_byte(chip->client,
					TUBS320_CSR_REG_08,
					TUBS320_DFP_POWER_MODE,
					chip->pdata->dfp_power);
	if(IS_ERR_VALUE(rc)) {
		dev_err(cdev, "failed to write TUBS320_DFP_POWER_MODE\n");
		return rc;
	}

	dev_dbg(cdev, "%s: dfp_power (%d)\n", __func__, chip->pdata->dfp_power);

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
		dev_err(cdev, "%s: failed to write REG_0A\n", __func__);
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
	static ssize_t								\
field ## _show(struct device *dev, struct device_attribute *attr,	\
		char *buf)						\
{									\
	struct i2c_client *client = to_i2c_client(dev);		\
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

static void tusb320_not_attach(struct tusb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;

	dev_dbg(cdev, "%s: state (%d)\n", __func__, chip->state);

	chip->state = TUBS320_NOT_ATTACH;
	chip->cable_direction = 0;
	chip->ufp_power = TUBS320_UFP_POWER_DEFAULT;
	chip->accessory_mode = TUBS320_ACC_NOT_ATTACH;

	tusb320_update_attach_state(chip, TUBS320_UFP_POWER_DEFAULT);
}

static void tusb320_dfp_attach_ufp(struct tusb320_chip *chip,
					u8 direction, u8 detail)
{
	struct device *cdev = &chip->client->dev;

	dev_dbg(cdev, "%s: state (%d)\n", __func__, chip->state);

	if (chip->mode == TUBS320_MODE_UFP) {
		dev_err(cdev, "%s: mode is UFP\n", __func__);
		return;
	}

	switch (chip->state) {
	case TUBS320_NOT_ATTACH :
		chip->cable_direction = direction;
		chip->state = TUBS320_DFP_ATTACH_UFP;
		tusb320_update_attach_state(chip, TUBS320_UFP_POWER_DEFAULT);
		tusb320_set_dfp_power(chip);
		break;
	case TUBS320_DFP_ATTACH_UFP :
		if (chip->cable_direction != direction)
			chip->cable_direction = direction;
		break;
	case TUBS320_UFP_ATTACH_DFP :
	case TUBS320_ATTACH_ACC :
	default:
		tusb320_reset_device(chip, TUBS320_I2C_RESET);
		dev_err(cdev, "%s: Invaild state\n", __func__);
		break;
	}
}

static void tusb320_ufp_attach_dfp(struct tusb320_chip *chip,
				u8 direction, u8 detail)
{
	struct device *cdev = &chip->client->dev;
	u8 ufp_pow = BITS_GET(detail, TUBS320_UFP_POWER_MODE);

	dev_dbg(cdev, "%s: state (%d)\n", __func__, chip->state);
	dev_dbg(cdev, "%s: ufp_power [before(%d) vs after(%d)]\n",
			__func__, chip->ufp_power, ufp_pow);

	if (chip->mode == TUBS320_MODE_DFP) {
		dev_err(cdev, "%s: mode is DFP\n", __func__);
		return;
	}

	switch (chip->state) {
	case TUBS320_NOT_ATTACH :
		chip->cable_direction = direction;
		chip->state = TUBS320_UFP_ATTACH_DFP;
		tusb320_update_attach_state(chip, ufp_pow);
		break;
	case TUBS320_UFP_ATTACH_DFP :
		if(chip->cable_direction != direction)
			chip->cable_direction = direction;
		tusb320_update_ufp_power(chip, ufp_pow);
		break;
	case TUBS320_DFP_ATTACH_UFP :
	case TUBS320_ATTACH_ACC :
	default:
		tusb320_reset_device(chip, TUBS320_I2C_RESET);
		dev_err(cdev, "%s: Invaild state\n", __func__);
		break;
	}
}

static void tusb320_attach_accessory_detail(struct tusb320_chip *chip,
						u8 acc_conn, u8 detail)
{
	struct device *cdev = &chip->client->dev;
	u8 ufp_pow = BITS_GET(detail, TUBS320_UFP_POWER_MODE);

	dev_dbg(cdev, "%s: ufp_power [before(%d) vs after(%d)]\n",
			__func__, chip->ufp_power, ufp_pow);

	if (chip->accessory_mode != acc_conn) {
		chip->accessory_mode = acc_conn;

		switch (acc_conn) {
		case TUBS320_ACC_AUDIO_DFP :
		case TUBS320_ACC_DEBUG_DFP :
			tusb320_update_attach_state(chip,
					TUBS320_UFP_POWER_DEFAULT);
			break;
		case TUBS320_ACC_AUDIO_UFP :
		case TUBS320_ACC_DEBUG_UFP :
			tusb320_update_attach_state(chip, ufp_pow);
			break;
		default:
			tusb320_reset_device(chip, TUBS320_I2C_RESET);
			dev_err(cdev, "%s: Invaild state\n", __func__);
			break;
		}
	}

	if ((chip->accessory_mode == TUBS320_ACC_AUDIO_UFP) ||
			(chip->accessory_mode == TUBS320_ACC_DEBUG_UFP)) {
		tusb320_update_ufp_power(chip, ufp_pow);
	}
}

static void tusb320_attach_accessory(struct tusb320_chip *chip,
					u8 direction, u8 detail)
{
	struct device *cdev = &chip->client->dev;
	u8 acc_conn = BITS_GET(detail, TUBS320_ACC_CONN);

	dev_dbg(cdev, "%s: state (%d)\n", __func__, chip->state);
	dev_dbg(cdev, "%s: accessory_mode [before(%d) vs after(%d)]\n",
			__func__, chip->accessory_mode, acc_conn);

	switch (chip->state) {
	case TUBS320_NOT_ATTACH:
		chip->cable_direction = direction;
		chip->state = TUBS320_ATTACH_ACC;
		tusb320_attach_accessory_detail(chip, acc_conn, detail);
		break;
	case TUBS320_ATTACH_ACC:
		if(chip->cable_direction != direction)
			chip->cable_direction = direction;
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
	u8 reg09, reg08, state, direction;

	wake_lock(&chip->wlock);

	/* Get status (reg8/reg9) */
	ret = i2c_smbus_read_word_data(chip->client, TUBS320_CSR_REG_08);
	if (IS_ERR_VALUE(ret)) {
		dev_err(cdev, "%s: failed to read REG_09\n", __func__);
		goto work_unlock;
	}
	reg08 = ret & 0xFF;
	reg09 = (ret >> 8) & 0xFF;

	dev_dbg(cdev, "reg08[0x%02x] , reg09[0x%02x]\n", reg08, reg09);

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
	direction = BITS_GET(reg08, TUBS320_CABLE_DIR);

	dev_info(cdev, "%s: [state %d] [direction: %d]\n",
			__func__, state, direction);

	switch (state) {
		case TUBS320_NOT_ATTACH:
			tusb320_not_attach(chip);
			break;

		case TUBS320_DFP_ATTACH_UFP:
			tusb320_dfp_attach_ufp(chip, direction, reg08);
			break;

		case TUBS320_UFP_ATTACH_DFP:
			tusb320_ufp_attach_dfp(chip, direction, reg08);
			break;

		case TUBS320_ATTACH_ACC:
			tusb320_attach_accessory(chip, direction, reg08);
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

	/* Start to enable TUSB320 Chip */
	if (gpio_is_valid(chip->pdata->enb_gpio)) {
		rc = gpio_request_one(chip->pdata->enb_gpio,
				GPIOF_OUT_INIT_LOW, "tusb320_enb_gpio");
		if (rc) {
			dev_err(cdev, "unable to request enb_gpio %d\n",
					chip->pdata->enb_gpio);
			goto err1;
		}
	} else {
		dev_err(cdev, "enb_gpio %d is not valid\n",
				chip->pdata->enb_gpio);
		rc = -EINVAL;
		goto err1;
	}

	if (gpio_is_valid(chip->pdata->int_gpio)) {
		rc = gpio_request_one(chip->pdata->int_gpio,
				GPIOF_DIR_IN, "tusb320_int_gpio");
		if (rc) {
			dev_err(cdev, "unable to request int_gpio %d\n",
					chip->pdata->int_gpio);
			goto err2;
		}
	} else {
		dev_err(cdev, "int_gpio %d is not valid\n",
				chip->pdata->int_gpio);
		rc = -EINVAL;
		goto err2;
	}

	return rc;

err2:
	gpio_free(chip->pdata->enb_gpio);
err1:
	return rc;
}

static void tusb320_free_gpio(struct tusb320_chip *chip)
{
	if (gpio_is_valid(chip->pdata->enb_gpio))
		gpio_free(chip->pdata->enb_gpio);

	if (gpio_is_valid(chip->pdata->int_gpio))
		gpio_free(chip->pdata->int_gpio);
}

static int tusb320_parse_dt(struct device *cdev, struct tusb320_data *data)
{
	struct device_node *dev_node = cdev->of_node;
	int rc = 0;

	data->enb_gpio = of_get_named_gpio(dev_node,
				"tusb320,enb-gpio", 0);
	if (data->enb_gpio < 0) {
		dev_err(cdev, "enb_gpio is not available\n");
		rc = data->enb_gpio;
		goto out;
	}

	data->int_gpio = of_get_named_gpio(dev_node,
				"tusb320,int-gpio", 0);
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

	dev_info(cdev, "select_mode: %d dfp_power %d\n",
			data->select_mode, data->dfp_power);

out:
	return rc;
}

static int tusb320_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tusb320_chip *chip;
	struct device *cdev = &client->dev;
	unsigned long flags;
	int ret = 0, is_active;

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

	INIT_WORK(&chip->dwork, tusb320_work_handler);
	wake_lock_init(&chip->wlock, WAKE_LOCK_SUSPEND, "tusb320_wake");

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

	/* Update initial Interrupt state */
	local_irq_save(flags);
	is_active = !gpio_get_value(chip->pdata->int_gpio);
	local_irq_restore(flags);
	enable_irq_wake(chip->irq_gpio);

	ret = tusb320_read_device_id(chip);
	if (IS_ERR_VALUE(ret)) {
		dev_err(cdev, "failed to read device id\n");
	}

	if (is_active) {
		dev_info(cdev, "presents interrupt initially\n");
		schedule_work(&chip->dwork);
	} else {
		ret = tusb320_select_mode(chip, chip->pdata->select_mode);
		if (IS_ERR_VALUE(ret))
			dev_err(cdev, "failed to select mode and work as default\n");
	}

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

#ifdef CONFIG_PM
static int tusb320_suspend(struct device *dev)
{
	return 0;
}

static int tusb320_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops tusb320_dev_pm_ops = {
	.suspend = tusb320_suspend,
	.resume  = tusb320_resume,
};
#endif

static const struct i2c_device_id tusb320_id_table[] = {
	{"tusb320", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, tusb320_id_table);

#ifdef CONFIG_OF
static struct of_device_id tusb320_match_table[] = {
	{ .compatible = "ti,tusb320",},
	{ },
};
#else
#define tusb320_match_table NULL
#endif


static struct i2c_driver tusb320_i2c_driver = {
	.driver = {
		.name = "tusb320",
		.owner = THIS_MODULE,
		.of_match_table = tusb320_match_table,
#ifdef CONFIG_PM
		.pm = &tusb320_dev_pm_ops,
#endif
	},
	.probe = tusb320_probe,
	.remove = tusb320_remove,
	.id_table = tusb320_id_table,
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
