/*
 * Copyright (c) 2015, LGE Inc. All rights reserved.
 *
 * fusb301 USB TYPE-C Configuration Controller driver
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
#include <linux/power_supply.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/wakelock.h>

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

/* Register Map */
#define FUSB301_REG_DEVICEID            0x01
#define FUSB301_REG_MODES               0x02
#define FUSB301_REG_CONTROL             0x03
#define FUSB301_REG_MANUAL              0x04
#define FUSB301_REG_RESET               0x05
#define FUSB301_REG_MASK                0x10
#define FUSB301_REG_STATUS              0x11
#define FUSB301_REG_TYPE                0x12
#define FUSB301_REG_INT                 0x13

/* Register Vaules */
#define FUSB301_DRP_ACC                 BIT(5)
#define FUSB301_DRP                     BIT(4)
#define FUSB301_SNK_ACC                 BIT(3)
#define FUSB301_SNK                     BIT(2)
#define FUSB301_SRC_ACC                 BIT(1)
#define FUSB301_SRC                     BIT(0)

#define FUSB301_TGL_35MS                0
#define FUSB301_TGL_30MS                1
#define FUSB301_TGL_25MS                2
#define FUSB301_TGL_20MS                3

#define FUSB301_HOST_0MA                0
#define FUSB301_HOST_DEFAULT            1
#define FUSB301_HOST_1500MA             2
#define FUSB301_HOST_3000MA             3

#define FUSB301_INT_ENABLE              0x00
#define FUSB301_INT_DISABLE             0x01

#define FUSB301_UNATT_SNK               BIT(3)
#define FUSB301_UNATT_SRC               BIT(2)
#define FUSB301_DISABLED                BIT(1)
#define FUSB301_ERR_REC                 BIT(0)

#define FUSB301_DISABLED_CLEAR          0x00

#define FUSB301_SW_RESET                BIT(0)

#define FUSB301_M_ACC_CH                BIT(3)
#define FUSB301_M_BCLVL                 BIT(2)
#define FUSB301_M_DETACH                BIT(1)
#define FUSB301_M_ATTACH                BIT(0)

#define FUSB301_FAULT_CC                0x30
#define FUSB301_CC2                     0x20
#define FUSB301_CC1                     0x10
#define FUSB301_NO_CONN                 0x00

#define FUSB301_VBUS_OK                 0x08

#define FUSB301_SNK_0MA                 0x00
#define FUSB301_SNK_DEFAULT             0x02
#define FUSB301_SNK_1500MA              0x04
#define FUSB301_SNK_3000MA              0x06

#define FUSB301_ATTACH                  0x01

#define FUSB301_DET_SNK                 BIT(4)
#define FUSB301_DET_SRC                 BIT(3)
#define FUSB301_DET_PWR_ACC             BIT(2)
#define FUSB301_DET_DBG_ACC             BIT(1)
#define FUSB301_DET_AUD_ACC             BIT(0)
#define FUSB301_NO_TYPE                 0x00

#define FUSB301_INT_ACC_CH              BIT(3)
#define FUSB301_INT_BCLVL               BIT(2)
#define FUSB301_INT_DETACH              BIT(1)
#define FUSB301_INT_ATTACH              BIT(0)

#define FUSB301_REV10                   0x10

/* Mask */
#define FUSB301_TGL_MASK                0x30
#define FUSB301_HOST_CUR_MASK           0x06
#define FUSB301_INT_MASK                0x01
#define FUSB301_BCLVL_MASK              0x06

struct fusb301_data {
	u8 int_gpio;
	u8 init_mode;
	u8 dfp_power;
	u8 dttime;
};

struct fusb301_chip {
	struct i2c_client *client;
	struct fusb301_data *pdata;
	int irq_gpio;
	u8 mode;
	u8 dev_id;
	u8 state;
	u8 ufp_power;
	u8 dfp_power;
	u8 dttime;
	struct work_struct dwork;
	struct wake_lock wlock;
	struct power_supply *usb_psy;
};

int fusb301_init_reg(struct fusb301_chip *chip);

static int pending_initialize = 1;
static int try_attcnt = 0;

static int fusb301_write_masked_byte(struct i2c_client *client,
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

static int fusb301_read_device_id(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc;

	rc = i2c_smbus_read_byte_data(chip->client,
				FUSB301_REG_DEVICEID);
	if (IS_ERR_VALUE(rc))
		return rc;

	chip->dev_id = rc;
	dev_info(cdev, "device id: 0x%02x\n", rc);

	return 0;
}

static int fusb301_set_mode(struct fusb301_chip *chip, u8 mode)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	if (mode > FUSB301_DRP_ACC) {
		dev_err(cdev, "mode(%d) is unavailable\n", mode);
		return -EINVAL;
	}

	if (mode != chip->mode) {
		rc = i2c_smbus_write_byte_data(chip->client,
				FUSB301_REG_MODES, mode);
		if (IS_ERR_VALUE(rc)) {
			dev_err(cdev, "failed to write FUSB301_REG_MODES\n");
			return rc;
		}
		chip->mode = mode;
	}

	dev_dbg(cdev, "%s: mode (%d)(%d)(%d)\n", __func__,
				chip->mode , rc, mode);

	return rc;
}

static int fusb301_set_dfp_power(struct fusb301_chip *chip, u8 hcurrent)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	if (hcurrent > FUSB301_HOST_3000MA) {
		dev_err(cdev, "hcurrent(%d) is unavailable\n",
					hcurrent);
		return -EINVAL;
	}

	if (hcurrent == chip->dfp_power) {
		dev_dbg(cdev, "vaule is not updated(%d)\n",
					hcurrent);
		return rc;
	}

	rc = fusb301_write_masked_byte(chip->client,
					FUSB301_REG_CONTROL,
					FUSB301_HOST_CUR_MASK,
					hcurrent);
	if (IS_ERR_VALUE(rc)) {
		dev_err(cdev, "failed to write current(%d)\n", rc);
		return rc;
	}

	chip->dfp_power = hcurrent;

	dev_dbg(cdev, "%s: host current(%d)\n", __func__, hcurrent);

	return rc;
}

/*
 * When 3A capable DRP device is connected without VBUS,
 * DRP always detect it as SINK device erroneously.
 * Since USB Type-C specification 1.0 and 1.1 doesn't
 * consider this corner case, apply workaround for this case.
 * Set host mode current to 1.5A initially, and then change
 * it to default USB current right after detection SINK port.
 */
static int fusb301_init_force_dfp_power(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	rc = fusb301_write_masked_byte(chip->client,
					FUSB301_REG_CONTROL,
					FUSB301_HOST_CUR_MASK,
					FUSB301_HOST_1500MA);
	if (IS_ERR_VALUE(rc)) {
		dev_err(cdev, "failed to write current\n");
		return rc;
	}

	chip->dfp_power = FUSB301_HOST_1500MA;

	dev_dbg(cdev, "%s: host current (%d)\n", __func__, rc);

	return rc;
}

static int fusb301_set_toggle_time(struct fusb301_chip *chip, u8 toggle_time)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	if (toggle_time > FUSB301_TGL_20MS) {
		dev_err(cdev, "toggle_time(%d) is unavailable\n", toggle_time);
		return -EINVAL;
	}

	if (toggle_time == chip->dttime) {
		dev_dbg(cdev, "vaule is not updated (%d)\n", toggle_time);
		return rc;
	}

	rc = fusb301_write_masked_byte(chip->client,
					FUSB301_REG_CONTROL,
					FUSB301_TGL_MASK,
					toggle_time);
	if (IS_ERR_VALUE(rc)) {
		dev_err(cdev, "failed to write toggle time\n");
		return rc;
	}

	chip->dttime = toggle_time;

	dev_dbg(cdev, "%s: host current (%d)\n", __func__, rc);

	return rc;
}

static int fusb301_reset_device(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	rc = i2c_smbus_write_byte_data(chip->client,
					FUSB301_REG_RESET,
					FUSB301_SW_RESET);
	if (IS_ERR_VALUE(rc)) {
		dev_err(cdev, "reset fails\n");
		return rc;
	}

	msleep(10);

	rc = fusb301_init_reg(chip);
	if (IS_ERR_VALUE(rc))
		dev_err(cdev, "fail to init reg\n");

	/* clear global interrupt mask */
	rc = fusb301_write_masked_byte(chip->client,
				FUSB301_REG_CONTROL,
				FUSB301_INT_MASK,
				FUSB301_INT_ENABLE);
	if (IS_ERR_VALUE(rc)) {
		dev_err(cdev, "%s: fail to init\n", __func__);
		return rc;
	}

	dev_dbg(cdev, "reset sucessed\n");

	return rc;
}

static ssize_t fregdump_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb301_chip *chip = i2c_get_clientdata(client);
	u8 start_reg[] = {0x01, 0x02,
			  0x03, 0x04,
			  0x05, 0x06,
			  0x10, 0x11,
			  0x12, 0x13}; /* reserved 0x06 */
	int i, ret = 0;
	u16 rc;

	for (i = 0 ; i < 5; i++) {
		rc = i2c_smbus_read_word_data(chip->client, start_reg[(i*2)]);
		if (IS_ERR_VALUE(rc))
			pr_err("cannot read 0x%02x\n", start_reg[(i*2)]);

		ret += snprintf(buf + ret, 1024, "from 0x%02x read 0x%02x\n"
						"from 0x%02x read 0x%02x\n",
							start_reg[(i*2)],
							(rc & 0xFF),
							start_reg[(i*2)+1],
							(rc >> 8));
	}

	return ret;
}
DEVICE_ATTR(fregdump, S_IRUGO, fregdump_show, NULL);

static ssize_t fstate_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb301_chip *chip = i2c_get_clientdata(client);
	int ret;

	switch (chip->state) {
	case FUSB301_DET_SNK:
		ret = snprintf(buf, PAGE_SIZE, "SINK(%d)\n", chip->state);
		break;
	case FUSB301_DET_SRC:
		ret = snprintf(buf, PAGE_SIZE, "SOURCE(%d)\n", chip->state);
		break;
	case FUSB301_DET_PWR_ACC:
		ret = snprintf(buf, PAGE_SIZE, "PWRACC(%d)\n", chip->state);
		break;
	case FUSB301_DET_DBG_ACC:
		ret = snprintf(buf, PAGE_SIZE, "DEBUGACC(%d)\n", chip->state);
		break;
	case FUSB301_DET_AUD_ACC:
		ret = snprintf(buf, PAGE_SIZE, "AUDIOACC(%d)\n", chip->state);
		break;
	default:
		ret = snprintf(buf, PAGE_SIZE, "NOTYPE(%d)\n", chip->state);
		break;
	}

	return ret;
}

DEVICE_ATTR(fstate, S_IRUGO , fstate_show, NULL);

static ssize_t fmode_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb301_chip *chip = i2c_get_clientdata(client);
	int ret;

	switch (chip->mode) {
	case FUSB301_DRP_ACC:
		ret = snprintf(buf, PAGE_SIZE, "DRP+ACC(%d)\n", chip->mode);
		break;
	case FUSB301_DRP:
		ret = snprintf(buf, PAGE_SIZE, "DRP(%d)\n", chip->mode);
		break;
        case FUSB301_SNK_ACC:
		ret = snprintf(buf, PAGE_SIZE, "SNK+ACC(%d)\n", chip->mode);
		break;
	case FUSB301_SNK:
		ret = snprintf(buf, PAGE_SIZE, "SNK(%d)\n", chip->mode);
		break;
	case FUSB301_SRC_ACC:
		ret = snprintf(buf, PAGE_SIZE, "SRC+ACC(%d)\n", chip->mode);
		break;
	case FUSB301_SRC:
		ret = snprintf(buf, PAGE_SIZE, "SRC(%d)\n", chip->mode);
		break;
	default:
		ret = snprintf(buf, PAGE_SIZE, "UNKNOWN(%d)\n", chip->mode);
		break;
	}

	return ret;
}

static ssize_t fmode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb301_chip *chip = i2c_get_clientdata(client);
	int mode = 0;
	int rc = 0;

	if (sscanf(buff, "%d", &mode) == 1) {
		rc = fusb301_set_mode(chip, (u8)mode);
		if (IS_ERR_VALUE(rc))
			return rc;

		return size;
	}

	return -EINVAL;
}
DEVICE_ATTR(fmode, S_IRUGO | S_IWUSR, fmode_show, fmode_store);

static ssize_t fdttime_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb301_chip *chip = i2c_get_clientdata(client);

	return snprintf(buf, PAGE_SIZE, "%u\n", chip->dttime);
}

static ssize_t fdttime_store(struct device *dev,
				struct device_attribute *attr,
				const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb301_chip *chip = i2c_get_clientdata(client);
	int dttime = 0;
	int rc = 0;

	if (sscanf(buff, "%d", &dttime) == 1) {
		rc = fusb301_set_toggle_time(chip, (u8)dttime);
		if (IS_ERR_VALUE(rc))
			return rc;

		return size;
	}

	return -EINVAL;
}
DEVICE_ATTR(fdttime, S_IRUGO | S_IWUSR, fdttime_show, fdttime_store);

static ssize_t fhostcur_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb301_chip *chip = i2c_get_clientdata(client);

	return snprintf(buf, PAGE_SIZE, "%u\n", chip->mode);
}

static ssize_t fhostcur_store(struct device *dev,
				struct device_attribute *attr,
				const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb301_chip *chip = i2c_get_clientdata(client);
	int buf = 0;
	int rc = 0;

	if (sscanf(buff, "%d", &buf) == 1) {
		rc = fusb301_set_dfp_power(chip, (u8)buf);
		if (IS_ERR_VALUE(rc))
			return rc;

		return size;
	}

	return -EINVAL;
}
DEVICE_ATTR(fhostcur, S_IRUGO | S_IWUSR, fhostcur_show, fhostcur_store);

static ssize_t freset_store(struct device *dev,
				struct device_attribute *attr,
				const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb301_chip *chip = i2c_get_clientdata(client);
	u32 reset = 0;
	int rc = 0;

	if (sscanf(buff, "%u", &reset) == 1) {
		rc = fusb301_reset_device(chip);
		if (IS_ERR_VALUE(rc))
			return rc;

		return size;
	}

	return -EINVAL;
}
DEVICE_ATTR(freset, S_IWUSR, NULL, freset_store);

static int fusb301_create_devices(struct device *cdev)
{
	int ret = 0;

	ret = device_create_file(cdev, &dev_attr_fstate);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_fstate\n");
		ret = -ENODEV;
		goto err1;
	}

	ret = device_create_file(cdev, &dev_attr_fmode);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_fmode\n");
		ret = -ENODEV;
		goto err2;
	}

	ret = device_create_file(cdev, &dev_attr_freset);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_freset\n");
		ret = -ENODEV;
		goto err3;
	}

	ret = device_create_file(cdev, &dev_attr_fdttime);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_fdttime\n");
		ret = -ENODEV;
		goto err4;
	}

	ret = device_create_file(cdev, &dev_attr_fhostcur);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_fhostcur\n");
		ret = -ENODEV;
		goto err5;
	}

	ret = device_create_file(cdev, &dev_attr_fregdump);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_fregdump\n");
		ret = -ENODEV;
		goto err6;
	}

	return ret;

err6:
	device_remove_file(cdev, &dev_attr_fhostcur);
err5:
	device_remove_file(cdev, &dev_attr_fdttime);
err4:
	device_remove_file(cdev, &dev_attr_freset);
err3:
	device_remove_file(cdev, &dev_attr_fmode);
err2:
	device_remove_file(cdev, &dev_attr_fstate);
err1:
	return ret;
}

static void fusb301_destory_device(struct device *cdev)
{
	device_remove_file(cdev, &dev_attr_fstate);
	device_remove_file(cdev, &dev_attr_fmode);
	device_remove_file(cdev, &dev_attr_freset);
	device_remove_file(cdev, &dev_attr_fdttime);
	device_remove_file(cdev, &dev_attr_fhostcur);
	device_remove_file(cdev, &dev_attr_fregdump);
}

static int fusb301_power_set_icurrent_max(struct power_supply *psy,
						int icurrent)
{
	const union power_supply_propval ret = {icurrent,};

	if (psy->set_property)
		return psy->set_property(psy,
				POWER_SUPPLY_PROP_INPUT_CURRENT_MAX, &ret);
	return -ENXIO;
}

static void fusb301_bclvl_changed(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int limit;
	u16 rc;
	u8 status, type, bclvl;

	rc = i2c_smbus_read_word_data(chip->client,
				FUSB301_REG_STATUS);
	if (IS_ERR_VALUE(rc)) {
		dev_err(cdev, "%s: failed to read\n", __func__);
		return;
	}

	status = rc & 0xFF;
	type = rc >> 8;

	dev_dbg(cdev, "sts[0x%02x], type[0x%02x]\n", status, type);

	if (type != FUSB301_DET_SRC)
		return;

	bclvl = status & 0x06;

	if (bclvl == chip->ufp_power)
		return;

	chip->ufp_power = bclvl;
	limit = (bclvl == FUSB301_SNK_3000MA ? 3000 :
			(bclvl == FUSB301_SNK_1500MA ? 1500 : 0));

	fusb301_power_set_icurrent_max(chip->usb_psy, limit);
}

static void fusb301_acc_changed(struct fusb301_chip *chip)
{
	/* TODO */
	/* implement acc changed work */
	return;
}

static void fusb301_detach(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;

	dev_dbg(cdev, "%s: state (%d)\n", __func__, chip->state);

	switch (chip->state) {
	case FUSB301_DET_SNK:
		power_supply_set_usb_otg(chip->usb_psy, false);
		fusb301_init_force_dfp_power(chip);
		break;
	case FUSB301_DET_SRC:
		break;
	case FUSB301_DET_PWR_ACC:
	case FUSB301_DET_DBG_ACC:
	case FUSB301_DET_AUD_ACC:
	case FUSB301_NO_TYPE:
		break;
	default:
		dev_err(cdev, "%s: Invaild state\n", __func__);
		break;
	}

	chip->state = FUSB301_NO_TYPE;
	chip->ufp_power = FUSB301_SNK_0MA;

	if (pending_initialize) {
		int ret;
		ret = fusb301_reset_device(chip);
		if (ret)
			dev_err(cdev, "failed to initialize\n");

		pending_initialize = 0;
	}
}

static void fusb301_set_sink_mode(struct fusb301_chip *chip, u8 status)
{
	struct device *cdev = &chip->client->dev;
	int limit;
	u8 bclvl;

	dev_dbg(cdev, "%s: state (%d)\n", __func__, chip->state);

	if (!!(chip->mode & (FUSB301_SRC_ACC | FUSB301_SRC))) {
		dev_err(cdev, "not support in source mode\n");
		return;
	}

	bclvl = status & 0x06;

	limit = (bclvl == FUSB301_SNK_3000MA ? 3000 :
			(bclvl == FUSB301_SNK_1500MA ? 1500 : 0));

	switch (chip->state) {
	case FUSB301_DET_SNK:
		power_supply_set_usb_otg(chip->usb_psy, false);
	case FUSB301_NO_TYPE:
		chip->state = FUSB301_DET_SRC;
		chip->ufp_power = bclvl;
		fusb301_power_set_icurrent_max(chip->usb_psy, limit);
		break;
	case FUSB301_DET_SRC:
		if (chip->ufp_power != bclvl) {
			chip->ufp_power = bclvl;
			fusb301_power_set_icurrent_max(chip->usb_psy, limit);
		}
		break;
	case FUSB301_DET_PWR_ACC:
	case FUSB301_DET_DBG_ACC:
	case FUSB301_DET_AUD_ACC:
	default:
		fusb301_reset_device(chip);
		dev_err(cdev, "%s: Invaild state\n", __func__);
		break;
	}
}

static void fusb301_set_source_mode(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;

	dev_dbg(cdev, "%s: state (%d)\n", __func__, chip->state);

	if (!!(chip->mode & (FUSB301_SNK_ACC | FUSB301_SNK))) {
		dev_err(cdev, "not support in sink mode\n");
		return;
	}

	switch (chip->state) {
	case FUSB301_DET_SRC:
		power_supply_set_present(chip->usb_psy, false);
	case FUSB301_NO_TYPE:
		chip->state = FUSB301_DET_SNK;
		power_supply_set_usb_otg(chip->usb_psy, true);
		fusb301_set_dfp_power(chip, chip->pdata->dfp_power);
		break;
	case FUSB301_DET_SNK:
		break;
	case FUSB301_DET_PWR_ACC:
	case FUSB301_DET_DBG_ACC:
	case FUSB301_DET_AUD_ACC:
	default:
		fusb301_reset_device(chip);
		dev_err(cdev, "%s: Invaild state\n", __func__);
		break;
	}
}

static void fusb301_set_power_acc_mode(struct fusb301_chip *chip)
{
	/* TODO */
	/* Need to set how it works */
	return;
}

static void fusb301_set_debug_acc_mode(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;

	if (!!(chip->mode & (FUSB301_SRC | FUSB301_SNK | FUSB301_DRP))) {
		dev_err(cdev, "not support accessory mode\n");
		return;
	}

	switch (chip->state) {
	case FUSB301_NO_TYPE:
		chip->state = FUSB301_DET_DBG_ACC;
	case FUSB301_DET_AUD_ACC:
		/* TODO */
		/* need to set how works. */
		break;
	case FUSB301_DET_SNK:
	case FUSB301_DET_SRC:
	case FUSB301_DET_PWR_ACC:
	case FUSB301_DET_DBG_ACC:
	default:
		fusb301_reset_device(chip);
		dev_err(cdev, "%s: Invaild state\n", __func__);
		break;
	}
}

static void fusb301_set_audio_acc_mode(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;

	if (!!(chip->mode & (FUSB301_SRC | FUSB301_SNK | FUSB301_DRP))) {
		dev_err(cdev, "not support accessory mode\n");
		return;
	}

	switch (chip->state) {
	case FUSB301_NO_TYPE:
		chip->state = FUSB301_DET_AUD_ACC;
	case FUSB301_DET_DBG_ACC:
		/* TODO */
		/* need to set how works */
		break;
	case FUSB301_DET_SNK:
	case FUSB301_DET_SRC:
	case FUSB301_DET_PWR_ACC:
	case FUSB301_DET_AUD_ACC:
	default:
		fusb301_reset_device(chip);
		dev_err(cdev, "%s: Invaild state\n", __func__);
		break;
	}
}

static bool fusb301_is_vbus_ok(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc;

	rc = i2c_smbus_read_byte_data(chip->client,
				FUSB301_REG_STATUS);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read status\n", __func__);
		return false;
	}

	return !!(rc & FUSB301_VBUS_OK);
}

#define MAX_TRY_COUNT 10
static void fusb301_attach(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	u16 rc;
	u8 status, type;

	/* Get Status and type */
	rc = i2c_smbus_read_word_data(chip->client,
			FUSB301_REG_STATUS);
	if (IS_ERR_VALUE(rc)) {
		dev_err(cdev, "%s: failed to read status\n", __func__);
		return;
	}

	status = rc & 0xFF;
	type = rc >> 8;

	dev_info(cdev, "sts[0x%02x], type[0x%02x]\n", status, type);
	if (chip->dev_id == FUSB301_REV10) {
		/* workaround BC Level detection on Rev1.0 */
		if ((type & FUSB301_DET_SRC || type == FUSB301_NO_TYPE) &&
				!(status & FUSB301_BCLVL_MASK) &&
				(try_attcnt < MAX_TRY_COUNT)) {
			i2c_smbus_write_byte_data(chip->client,
						FUSB301_REG_MANUAL,
						FUSB301_ERR_REC);
			msleep(100);
			try_attcnt++;
			if (!fusb301_is_vbus_ok(chip))
				try_attcnt = 0;
			return;
		}
		try_attcnt = 0;
	}

	switch (type) {
	case FUSB301_DET_SRC:
		fusb301_set_sink_mode(chip, status);
		break;
	case FUSB301_DET_SNK:
		fusb301_set_source_mode(chip);
		break;
	case FUSB301_DET_PWR_ACC:
		fusb301_set_power_acc_mode(chip);
		break;
	case FUSB301_DET_DBG_ACC:
		fusb301_set_debug_acc_mode(chip);
		break;
	case FUSB301_DET_AUD_ACC:
		fusb301_set_audio_acc_mode(chip);
		break;
	default:
		break;
	}
}

static void fusb301_work_handler(struct work_struct *work)
{
	struct fusb301_chip *chip =
			container_of(work, struct fusb301_chip, dwork);
	struct device *cdev = &chip->client->dev;
	u8 int_sts;

	/* Get Interrupt */
	int_sts = i2c_smbus_read_byte_data(chip->client, FUSB301_REG_INT);
	if (IS_ERR_VALUE(int_sts)) {
		dev_err(cdev, "%s: failed to read interrupt\n", __func__);
		goto work_unlock;
	}

	dev_dbg(cdev, "%s: int_sts[0x%02x]\n",__func__, int_sts);

	if (int_sts & FUSB301_INT_ATTACH) {
		fusb301_attach(chip);
	} else if (int_sts & FUSB301_INT_DETACH) {
		fusb301_detach(chip);
	} else if (int_sts & FUSB301_INT_BCLVL) {
		fusb301_bclvl_changed(chip);
	} else if (int_sts & FUSB301_INT_ACC_CH) {
		fusb301_acc_changed(chip);
	} else {
		fusb301_reset_device(chip);
                dev_err(cdev, "%s: Invalid state\n", __func__);
	}

work_unlock:
	wake_unlock(&chip->wlock);
}

static irqreturn_t fusb301_interrupt(int irq, void *data)
{
	struct fusb301_chip *chip = (struct fusb301_chip *)data;

	if (!chip) {
		pr_err("%s : called before init.\n", __func__);
		return IRQ_HANDLED;
	}

	dev_dbg(&chip->client->dev, "%s\n", __func__);

	wake_lock(&chip->wlock);
	if(!schedule_work(&chip->dwork))
		wake_unlock(&chip->wlock);

	return IRQ_HANDLED;
}

int fusb301_init_reg(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	u16 rc;
	u8 mode_now, control_now, cur_now, tgl_now;

	/* read mode & control register */
	rc = i2c_smbus_read_word_data(chip->client, FUSB301_REG_MODES);
	if (IS_ERR_VALUE(rc)) {
		dev_err(cdev, "%s: fail read mode\n", __func__);
		return rc;
	}

	mode_now = rc & 0xFF;
	control_now = rc >> 8;

	cur_now = BITS_GET(control_now, FUSB301_HOST_CUR_MASK);
	tgl_now = BITS_GET(control_now, FUSB301_TGL_MASK);
	/* set disable bit */
	rc = i2c_smbus_write_byte_data(chip->client,
			FUSB301_REG_MANUAL,
			FUSB301_DISABLED);
	if (IS_ERR_VALUE(rc))
		return rc;

	chip->mode = mode_now;
	chip->dfp_power = cur_now;
	chip->dttime = tgl_now;

	/* change mode */
	fusb301_set_mode(chip, chip->pdata->init_mode);
	/* change current */
	fusb301_init_force_dfp_power(chip);
	/* change toggle time */
	fusb301_set_toggle_time(chip, chip->pdata->dttime);

	/* clear disable bit */
	rc = i2c_smbus_write_byte_data(chip->client,
			FUSB301_REG_MANUAL,
			FUSB301_DISABLED_CLEAR);
	if (IS_ERR_VALUE(rc))
		return rc;

	msleep(200);

	return rc;

}

int fusb301_init_gpio(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	/* Start to enable fusb301 Chip */
	if (gpio_is_valid(chip->pdata->int_gpio)) {
		rc = gpio_request_one(chip->pdata->int_gpio,
				GPIOF_DIR_IN, "fusb301_int_gpio");
		if (rc)
			dev_err(cdev, "unable to request int_gpio %d\n",
					chip->pdata->int_gpio);
	} else {
		dev_err(cdev, "int_gpio %d is not valid\n",
				chip->pdata->int_gpio);
		rc = -EINVAL;
	}

	return rc;
}

static void fusb301_free_gpio(struct fusb301_chip *chip)
{
	if (gpio_is_valid(chip->pdata->int_gpio))
		gpio_free(chip->pdata->int_gpio);
}

static int fusb301_parse_dt(struct device *cdev, struct fusb301_data *data)
{
	struct device_node *dev_node = cdev->of_node;
	int rc = 0;

	data->int_gpio = of_get_named_gpio(dev_node,
				"fusb301,int-gpio", 0);
	if (data->int_gpio < 0) {
		dev_err(cdev, "int_gpio is not available\n");
		rc = data->int_gpio;
		goto out;
	}

	rc = of_property_read_u8(dev_node,
				"fusb301,init-mode", &data->init_mode);
	if (rc || (data->init_mode > FUSB301_DRP_ACC)) {
		dev_err(cdev, "init mode is not available and set default\n");
		data->init_mode = FUSB301_DRP_ACC;
		rc = 0;
	}

	rc = of_property_read_u8(dev_node,
				"fusb301,host-current", &data->dfp_power);
	if (rc || (data->dfp_power > FUSB301_HOST_3000MA)) {
		dev_err(cdev, "host current is not available and set default\n");
		data->dfp_power = FUSB301_HOST_DEFAULT;
		rc = 0;
	}

	rc = of_property_read_u8(dev_node,
				"fusb301,drp-toggle-time", &data->dttime);
	if (rc || (data->dttime > FUSB301_TGL_20MS)) {
		dev_err(cdev, "drp time is not available and set default\n");
		data->dttime = FUSB301_TGL_35MS;
		rc = 0;
	}

	dev_dbg(cdev, "init_mode:%d dfp_power:%d toggle_time:%d\n",
			data->init_mode, data->dfp_power, data->dttime);

out:
	return rc;
}

static int fusb301_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct fusb301_chip *chip;
	struct device *cdev = &client->dev;
	struct power_supply *usb_psy;
	unsigned long flags;
	int ret = 0, is_active;

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

	chip = devm_kzalloc(cdev, sizeof(struct fusb301_chip), GFP_KERNEL);
	if (!chip) {
		dev_err(cdev, "can't alloc fusb301_chip\n");
		return -ENOMEM;
	}

	chip->client = client;
	i2c_set_clientdata(client, chip);

	ret = fusb301_read_device_id(chip);
	if (ret) {
		dev_err(cdev, "fusb301 not support\n");
		goto err1;
        }

	if (&client->dev.of_node) {
		struct fusb301_data *data = devm_kzalloc(cdev,
				sizeof(struct fusb301_data), GFP_KERNEL);

		if (!data) {
			dev_err(cdev, "can't alloc fusb301_data\n");
			ret = -ENOMEM;
			goto err1;
		}

		chip->pdata = data;

		ret = fusb301_parse_dt(cdev, data);
		if (ret) {
			dev_err(cdev, "can't parse dt\n");
			goto err2;
		}
	} else {
		chip->pdata = client->dev.platform_data;
	}

	ret = fusb301_init_gpio(chip);
	if (ret) {
		dev_err(cdev, "fail to init gpio\n");
		goto err2;
	}

	chip->mode = FUSB301_DRP_ACC;
	chip->dfp_power = FUSB301_HOST_DEFAULT;
	chip->dttime = FUSB301_TGL_35MS;

	chip->state = FUSB301_NO_TYPE;
	chip->ufp_power = FUSB301_SNK_0MA;
	chip->usb_psy = usb_psy;

	INIT_WORK(&chip->dwork, fusb301_work_handler);
	wake_lock_init(&chip->wlock, WAKE_LOCK_SUSPEND, "fusb301_wake");

	ret = fusb301_create_devices(cdev);
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

	ret = devm_request_irq(cdev, chip->irq_gpio,
				fusb301_interrupt,
				IRQF_TRIGGER_FALLING,
				"fusb301_int_irq", chip);
	if (ret) {
		dev_err(cdev, "failed to reqeust IRQ\n");
		goto err4;
	}

	/* Update initial Interrupt state */
	local_irq_save(flags);
	is_active = !gpio_get_value(chip->pdata->int_gpio);
	local_irq_restore(flags);
	enable_irq_wake(chip->irq_gpio);

	if (!is_active) {
		ret = fusb301_reset_device(chip);
		if (ret) {
			dev_err(cdev, "failed to initialize\n");
			goto err4;
		}
		pending_initialize = 0;
	} else {
		/* Update initial state */
		schedule_work(&chip->dwork);
	}

	return 0;

err4:
	fusb301_destory_device(cdev);
err3:
	wake_lock_destroy(&chip->wlock);
	fusb301_free_gpio(chip);
err2:
	if (&client->dev.of_node)
		devm_kfree(cdev, chip->pdata);
err1:
	i2c_set_clientdata(client, NULL);
	devm_kfree(cdev, chip);

	return ret;
}

static int fusb301_remove(struct i2c_client *client)
{
	struct fusb301_chip *chip = i2c_get_clientdata(client);
	struct device *cdev = &client->dev;

	if (!chip) {
		pr_err("%s : chip is null\n", __func__);
		return -ENODEV;
	}

	if (chip->irq_gpio > 0)
		devm_free_irq(cdev, chip->irq_gpio, chip);

	fusb301_destory_device(cdev);
	wake_lock_destroy(&chip->wlock);
	fusb301_free_gpio(chip);

	if (&client->dev.of_node)
		devm_kfree(cdev, chip->pdata);

	i2c_set_clientdata(client, NULL);
	devm_kfree(cdev, chip);

	return 0;
}

#ifdef CONFIG_PM
static int fusb301_suspend(struct device *dev)
{
	return 0;
}

static int fusb301_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops fusb301_dev_pm_ops = {
	.suspend = fusb301_suspend,
	.resume  = fusb301_resume,
};
#endif

static const struct i2c_device_id fusb301_id_table[] = {
	{"fusb301", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, fusb301_id_table);

#ifdef CONFIG_OF
static struct of_device_id fusb301_match_table[] = {
	{ .compatible = "fc,fusb301",},
	{ },
};
#else
#define fusb301_match_table NULL
#endif

static struct i2c_driver fusb301_i2c_driver = {
	.driver = {
		.name = "fusb301",
		.owner = THIS_MODULE,
		.of_match_table = fusb301_match_table,
#ifdef CONFIG_PM
		.pm = &fusb301_dev_pm_ops,
#endif
	},
	.probe = fusb301_probe,
	.remove = fusb301_remove,
	.id_table = fusb301_id_table,
};

static __init int fusb301_i2c_init(void)
{
	return i2c_add_driver(&fusb301_i2c_driver);
}

static __exit void fusb301_i2c_exit(void)
{
	i2c_del_driver(&fusb301_i2c_driver);
}

module_init(fusb301_i2c_init);
module_exit(fusb301_i2c_exit);

MODULE_AUTHOR("jude84.kim@lge.com");
MODULE_DESCRIPTION("I2C bus driver for fusb301 USB Type-C");
MODULE_LICENSE("GPL v2");
