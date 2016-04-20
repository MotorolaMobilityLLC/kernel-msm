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
#include <linux/workqueue.h>

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

#define FUSB301_TYPE_SNK                BIT(4)
#define FUSB301_TYPE_SRC                BIT(3)
#define FUSB301_TYPE_PWR_ACC            BIT(2)
#define FUSB301_TYPE_DBG_ACC            BIT(1)
#define FUSB301_TYPE_AUD_ACC            BIT(0)
#define FUSB301_TYPE_PWR_DBG_ACC       (FUSB301_TYPE_PWR_ACC|\
                                        FUSB301_TYPE_DBG_ACC)
#define FUSB301_TYPE_PWR_AUD_ACC       (FUSB301_TYPE_PWR_ACC|\
                                        FUSB301_TYPE_AUD_ACC)
#define FUSB301_TYPE_INVALID            0x00

#define FUSB301_INT_ACC_CH              BIT(3)
#define FUSB301_INT_BCLVL               BIT(2)
#define FUSB301_INT_DETACH              BIT(1)
#define FUSB301_INT_ATTACH              BIT(0)

#define FUSB301_REV10                   0x10
#define FUSB301_REV11                   0x11
#define FUSB301_REV12                   0x12

/* Mask */
#define FUSB301_TGL_MASK                0x30
#define FUSB301_HOST_CUR_MASK           0x06
#define FUSB301_INT_MASK                0x01
#define FUSB301_BCLVL_MASK              0x06
#define FUSB301_TYPE_MASK               0x1F
#define FUSB301_MODE_MASK               0x3F
#define FUSB301_INT_STS_MASK            0x0F

#define FUSB301_MAX_TRY_COUNT           10

/* FUSB STATES */
#define FUSB_STATE_DISABLED             0x00
#define FUSB_STATE_ERROR_RECOVERY       0x01
#define FUSB_STATE_UNATTACHED_SNK       0x02
#define FUSB_STATE_UNATTACHED_SRC       0x03
#define FUSB_STATE_ATTACHWAIT_SNK       0x04
#define FUSB_STATE_ATTACHWAIT_SRC       0x05
#define FUSB_STATE_ATTACHED_SNK         0x06
#define FUSB_STATE_ATTACHED_SRC         0x07
#define FUSB_STATE_AUDIO_ACCESSORY      0x08
#define FUSB_STATE_DEBUG_ACCESSORY      0x09
#define FUSB_STATE_TRY_SNK              0x0A
#define FUSB_STATE_TRYWAIT_SRC          0x0B
#define FUSB_STATE_TRY_SRC              0x0C
#define FUSB_STATE_TRYWAIT_SNK          0x0D

/* wake lock timeout in ms */
#define FUSB301_WAKE_LOCK_TIMEOUT       1000

#define ROLE_SWITCH_TIMEOUT		1500

struct fusb301_data {
	u32 int_gpio;
	u8 init_mode;
	u8 dfp_power;
	u8 dttime;
	bool try_snk_emulation;
	u32 ttry_timeout;
	u32 ccdebounce_timeout;
};

struct fusb301_chip {
	struct i2c_client *client;
	struct fusb301_data *pdata;
	struct workqueue_struct  *cc_wq;
	int irq_gpio;
	int ufp_power;
	u8 mode;
	u8 dev_id;
	u8 type;
	u8 state;
	u8 bc_lvl;
	u8 dfp_power;
	u8 dttime;
	bool triedsnk;
	int try_attcnt;
	struct work_struct dwork;
	struct delayed_work twork;
	struct wake_lock wlock;
	struct mutex mlock;
	struct power_supply *usb_psy;
	struct dual_role_phy_instance *dual_role;
	bool role_switch;
	struct dual_role_phy_desc *desc;
};

#define fusb_update_state(chip, st) \
	if(chip && st < FUSB_STATE_TRY_SRC) { \
		chip->state = st; \
		dev_info(&chip->client->dev, "%s: %s\n", __func__, #st); \
		wake_up_interruptible(&mode_switch); \
	}

#define STR(s)    #s
#define STRV(s)   STR(s)

static void fusb301_detach(struct fusb301_chip *chip);

DECLARE_WAIT_QUEUE_HEAD(mode_switch);

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

static int fusb301_update_status(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc;
	u16 control_now;

	/* read mode & control register */
	rc = i2c_smbus_read_word_data(chip->client, FUSB301_REG_MODES);
	if (IS_ERR_VALUE(rc)) {
		dev_err(cdev, "%s: fail to read mode\n", __func__);
		return rc;
	}

	chip->mode = rc & FUSB301_MODE_MASK;
	control_now = (rc >> 8) & 0xFF;

	chip->dfp_power = BITS_GET(control_now, FUSB301_HOST_CUR_MASK);
	chip->dttime = BITS_GET(control_now, FUSB301_TGL_MASK);

	return 0;
}

/*
 * spec lets transitioning to below states from any state
 *  FUSB_STATE_DISABLED
 *  FUSB_STATE_ERROR_RECOVERY
 *  FUSB_STATE_UNATTACHED_SNK
 *  FUSB_STATE_UNATTACHED_SRC
 */
static int fusb301_set_chip_state(struct fusb301_chip *chip, u8 state)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	if(state > FUSB_STATE_UNATTACHED_SRC)
		return -EINVAL;

	rc = i2c_smbus_write_byte_data(chip->client, FUSB301_REG_MANUAL,
				state == FUSB_STATE_DISABLED ? FUSB301_DISABLED:
				state == FUSB_STATE_ERROR_RECOVERY ? FUSB301_ERR_REC:
				state == FUSB_STATE_UNATTACHED_SNK ? FUSB301_UNATT_SNK:
				FUSB301_UNATT_SRC);

	if (IS_ERR_VALUE(rc)) {
		dev_err(cdev, "failed to write manual(%d)\n", rc);
	}

	return rc;
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
			dev_err(cdev, "%s: failed to write mode\n", __func__);
			return rc;
		}
		chip->mode = mode;
	}

	dev_dbg(cdev, "%s: mode (%d)(%d)\n", __func__, chip->mode , mode);

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

static int fusb301_init_reg(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	/* change current */
	rc = fusb301_init_force_dfp_power(chip);
	if (IS_ERR_VALUE(rc))
		dev_err(cdev, "%s: failed to force dfp power\n",
				__func__);

	/* change toggle time */
	rc = fusb301_set_toggle_time(chip, chip->pdata->dttime);
	if (IS_ERR_VALUE(rc))
		dev_err(cdev, "%s: failed to set toggle time\n",
				__func__);

	/* change mode */
	rc = fusb301_set_mode(chip, chip->pdata->init_mode);
	if (IS_ERR_VALUE(rc))
		dev_err(cdev, "%s: failed to set mode\n",
				__func__);

	/* set error recovery state */
	rc = fusb301_set_chip_state(chip,
				FUSB_STATE_ERROR_RECOVERY);
	if (IS_ERR_VALUE(rc))
		dev_err(cdev, "%s: failed to set error recovery state\n",
				__func__);

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

	rc = fusb301_update_status(chip);
	if (IS_ERR_VALUE(rc))
		dev_err(cdev, "fail to read status\n");

	rc = fusb301_init_reg(chip);
	if (IS_ERR_VALUE(rc))
		dev_err(cdev, "fail to init reg\n");

	fusb301_detach(chip);

	/* clear global interrupt mask */
	rc = fusb301_write_masked_byte(chip->client,
				FUSB301_REG_CONTROL,
				FUSB301_INT_MASK,
				FUSB301_INT_ENABLE);
	if (IS_ERR_VALUE(rc)) {
		dev_err(cdev, "%s: fail to init\n", __func__);
		return rc;
	}

	dev_info(cdev, "mode[0x%02x], host_cur[0x%02x], dttime[0x%02x]\n",
			chip->mode, chip->dfp_power, chip->dttime);

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
	int i, rc, ret = 0;

	mutex_lock(&chip->mlock);
	for (i = 0 ; i < 5; i++) {
		rc = i2c_smbus_read_word_data(chip->client, start_reg[(i*2)]);
		if (IS_ERR_VALUE(rc)) {
			pr_err("cannot read 0x%02x\n", start_reg[(i*2)]);
			rc = 0;
		}

		ret += snprintf(buf + ret, 1024, "from 0x%02x read 0x%02x\n"
						"from 0x%02x read 0x%02x\n",
							start_reg[(i*2)],
							(rc & 0xFF),
							start_reg[(i*2)+1],
							((rc >> 8) & 0xFF));
	}
	mutex_unlock(&chip->mlock);

	return ret;
}
DEVICE_ATTR(fregdump, S_IRUGO, fregdump_show, NULL);

static ssize_t ftype_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb301_chip *chip = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&chip->mlock);
	switch (chip->type) {
	case FUSB301_TYPE_SNK:
		ret = snprintf(buf, PAGE_SIZE, "SINK(%d)\n", chip->type);
		break;
	case FUSB301_TYPE_SRC:
		ret = snprintf(buf, PAGE_SIZE, "SOURCE(%d)\n", chip->type);
		break;
	case FUSB301_TYPE_PWR_ACC:
		ret = snprintf(buf, PAGE_SIZE, "PWRACC(%d)\n", chip->type);
		break;
	case FUSB301_TYPE_DBG_ACC:
		ret = snprintf(buf, PAGE_SIZE, "DEBUGACC(%d)\n", chip->type);
		break;
	case FUSB301_TYPE_PWR_DBG_ACC:
		ret = snprintf(buf, PAGE_SIZE, "POWEREDDEBUGACC(%d)\n", chip->type);
		break;
	case FUSB301_TYPE_AUD_ACC:
		ret = snprintf(buf, PAGE_SIZE, "AUDIOACC(%d)\n", chip->type);
		break;
	case FUSB301_TYPE_PWR_AUD_ACC:
		ret = snprintf(buf, PAGE_SIZE, "POWEREDAUDIOACC(%d)\n", chip->type);
		break;
	default:
		ret = snprintf(buf, PAGE_SIZE, "NOTYPE(%d)\n", chip->type);
		break;
	}
	mutex_unlock(&chip->mlock);

	return ret;
}

DEVICE_ATTR(ftype, S_IRUGO , ftype_show, NULL);

static ssize_t fchip_state_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE,
			STRV(FUSB_STATE_DISABLED) " - FUSB_STATE_DISABLED\n"
			STRV(FUSB_STATE_ERROR_RECOVERY) " - FUSB_STATE_ERROR_RECOVERY\n"
			STRV(FUSB_STATE_UNATTACHED_SNK) " - FUSB_STATE_UNATTACHED_SNK\n"
			STRV(FUSB_STATE_UNATTACHED_SRC) " - FUSB_STATE_UNATTACHED_SRC\n");

}

static ssize_t fchip_state_store(struct device *dev,
				struct device_attribute *attr,
				const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb301_chip *chip = i2c_get_clientdata(client);
	int state = 0;
	int rc = 0;

	if (sscanf(buff, "%d", &state) == 1) {
		mutex_lock(&chip->mlock);
		if(((state == FUSB_STATE_UNATTACHED_SNK) &&
			(chip->mode & (FUSB301_SRC | FUSB301_SRC_ACC))) ||
			((state == FUSB_STATE_UNATTACHED_SRC) &&
			(chip->mode & (FUSB301_SNK | FUSB301_SNK_ACC)))) {
			mutex_unlock(&chip->mlock);
			return -EINVAL;
		}

		rc = fusb301_set_chip_state(chip, (u8)state);
		if (IS_ERR_VALUE(rc)) {
			mutex_unlock(&chip->mlock);
			return rc;
		}

		fusb301_detach(chip);
		mutex_unlock(&chip->mlock);
		return size;
	}

	return -EINVAL;
}

DEVICE_ATTR(fchip_state, S_IRUGO | S_IWUSR, fchip_state_show, fchip_state_store);

static ssize_t fmode_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb301_chip *chip = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&chip->mlock);
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
	mutex_unlock(&chip->mlock);

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
		mutex_lock(&chip->mlock);

		/*
		 * since device trigger to usb happens independent
		 * from charger based on vbus, setting SRC modes
		 * doesn't prevent usb enumeration as device
		 * KNOWN LIMITATION
		 */
		rc = fusb301_set_mode(chip, (u8)mode);
		if (IS_ERR_VALUE(rc)) {
			mutex_unlock(&chip->mlock);
			return rc;
		}

		rc = fusb301_set_chip_state(chip,
					FUSB_STATE_ERROR_RECOVERY);
		if (IS_ERR_VALUE(rc)) {
			mutex_unlock(&chip->mlock);
			return rc;
		}

		fusb301_detach(chip);
		mutex_unlock(&chip->mlock);
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
	int ret;

	mutex_lock(&chip->mlock);
	ret = snprintf(buf, PAGE_SIZE, "%u\n", chip->dttime);
	mutex_unlock(&chip->mlock);
	return ret;
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
		mutex_lock(&chip->mlock);
		rc = fusb301_set_toggle_time(chip, (u8)dttime);
		mutex_unlock(&chip->mlock);
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
	int ret;

	mutex_lock(&chip->mlock);
	ret = snprintf(buf, PAGE_SIZE, "%u\n", chip->dfp_power);
	mutex_unlock(&chip->mlock);
	return ret;
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
		mutex_lock(&chip->mlock);
		rc = fusb301_set_dfp_power(chip, (u8)buf);
		mutex_unlock(&chip->mlock);
		if (IS_ERR_VALUE(rc))
			return rc;

		return size;
	}

	return -EINVAL;
}
DEVICE_ATTR(fhostcur, S_IRUGO | S_IWUSR, fhostcur_show, fhostcur_store);

static ssize_t fclientcur_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb301_chip *chip = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&chip->mlock);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", chip->ufp_power);
	mutex_unlock(&chip->mlock);
	return ret;
}
DEVICE_ATTR(fclientcur, S_IRUGO, fclientcur_show, NULL);

static ssize_t freset_store(struct device *dev,
				struct device_attribute *attr,
				const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb301_chip *chip = i2c_get_clientdata(client);
	u32 reset = 0;
	int rc = 0;

	if (sscanf(buff, "%u", &reset) == 1) {
		mutex_lock(&chip->mlock);
		rc = fusb301_reset_device(chip);
		mutex_unlock(&chip->mlock);
		if (IS_ERR_VALUE(rc))
			return rc;

		return size;
	}

	return -EINVAL;
}
DEVICE_ATTR(freset, S_IWUSR, NULL, freset_store);

static ssize_t fsw_trysnk_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb301_chip *chip = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&chip->mlock);
	ret = snprintf(buf, PAGE_SIZE, "%u\n", chip->pdata->try_snk_emulation);
	mutex_unlock(&chip->mlock);
	return ret;
}

static ssize_t fsw_trysnk_store(struct device *dev,
			struct device_attribute *attr,
			const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb301_chip *chip = i2c_get_clientdata(client);
	int buf = 0;

	if ((sscanf(buff, "%d", &buf) == 1) && (buf == 0 || buf == 1)) {
		mutex_lock(&chip->mlock);
		chip->pdata->try_snk_emulation = buf;
		if (chip->state == FUSB_STATE_ERROR_RECOVERY)
			chip->triedsnk = !chip->pdata->try_snk_emulation;
		mutex_unlock(&chip->mlock);

		return size;
	}

	return -EINVAL;
}
DEVICE_ATTR(fsw_trysnk, S_IRUGO | S_IWUSR,\
			fsw_trysnk_show, fsw_trysnk_store);

static ssize_t ftry_timeout_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb301_chip *chip = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&chip->mlock);
	ret = snprintf(buf, PAGE_SIZE, "%u\n", chip->pdata->ttry_timeout);
	mutex_unlock(&chip->mlock);
	return ret;
}

static ssize_t ftry_timeout_store(struct device *dev,
			struct device_attribute *attr,
			const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb301_chip *chip = i2c_get_clientdata(client);
	int buf = 0;

	if ((sscanf(buff, "%d", &buf) == 1) && (buf >= 0)) {
		mutex_lock(&chip->mlock);
		chip->pdata->ttry_timeout = buf;
		mutex_unlock(&chip->mlock);

		return size;
	}

	return -EINVAL;
}
DEVICE_ATTR(ftry_timeout, S_IRUGO | S_IWUSR,\
		ftry_timeout_show, ftry_timeout_store);

static ssize_t fccdebounce_timeout_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb301_chip *chip = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&chip->mlock);
	ret = snprintf(buf, PAGE_SIZE, "%u\n",
			chip->pdata->ccdebounce_timeout);
	mutex_unlock(&chip->mlock);
	return ret;
}

static ssize_t fccdebounce_timeout_store(struct device *dev,
				struct device_attribute *attr,
				const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb301_chip *chip = i2c_get_clientdata(client);
	int buf = 0;

	if ((sscanf(buff, "%d", &buf) == 1) && (buf >= 0)) {
		mutex_lock(&chip->mlock);
		chip->pdata->ccdebounce_timeout = buf;
		mutex_unlock(&chip->mlock);

		return size;
	}

	return -EINVAL;
}
DEVICE_ATTR(fccdebounce_timeout, S_IRUGO | S_IWUSR,\
                fccdebounce_timeout_show, fccdebounce_timeout_store);

static int fusb301_create_devices(struct device *cdev)
{
	int ret = 0;

	ret = device_create_file(cdev, &dev_attr_fchip_state);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_fchip_state\n");
		ret = -ENODEV;
		goto err0;
	}

	ret = device_create_file(cdev, &dev_attr_ftype);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_ftype\n");
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

	ret = device_create_file(cdev, &dev_attr_fclientcur);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_fufpcur\n");
		ret = -ENODEV;
		goto err6;
	}

	ret = device_create_file(cdev, &dev_attr_fregdump);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_fregdump\n");
		ret = -ENODEV;
		goto err7;
	}

	ret = device_create_file(cdev, &dev_attr_fsw_trysnk);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_fsw_trysnk\n");
		ret = -ENODEV;
		goto err8;
	}

	ret = device_create_file(cdev, &dev_attr_ftry_timeout);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_ftry_timeout\n");;
		ret = -ENODEV;
		goto err9;
	}

	ret = device_create_file(cdev, &dev_attr_fccdebounce_timeout);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_fccdebounce_timeout\n");
		ret = -ENODEV;
		goto err10;
	}

	return ret;

err10:
	device_remove_file(cdev, &dev_attr_ftry_timeout);
err9:
	device_remove_file(cdev, &dev_attr_fsw_trysnk);
err8:
	device_remove_file(cdev, &dev_attr_fregdump);
err7:
	device_remove_file(cdev, &dev_attr_fclientcur);
err6:
	device_remove_file(cdev, &dev_attr_fhostcur);
err5:
	device_remove_file(cdev, &dev_attr_fdttime);
err4:
	device_remove_file(cdev, &dev_attr_freset);
err3:
	device_remove_file(cdev, &dev_attr_fmode);
err2:
	device_remove_file(cdev, &dev_attr_ftype);
err1:
	device_remove_file(cdev, &dev_attr_fchip_state);
err0:
	return ret;
}

static void fusb301_destory_device(struct device *cdev)
{
	device_remove_file(cdev, &dev_attr_ftype);
	device_remove_file(cdev, &dev_attr_fmode);
	device_remove_file(cdev, &dev_attr_freset);
	device_remove_file(cdev, &dev_attr_fdttime);
	device_remove_file(cdev, &dev_attr_fhostcur);
	device_remove_file(cdev, &dev_attr_fclientcur);
	device_remove_file(cdev, &dev_attr_fregdump);
}

static int fusb301_power_set_icurrent_max(struct fusb301_chip *chip,
						int icurrent)
{
	const union power_supply_propval ret = {icurrent,};

	chip->ufp_power = icurrent;

	if (chip->usb_psy->set_property)
		return chip->usb_psy->set_property(chip->usb_psy,
				POWER_SUPPLY_PROP_INPUT_CURRENT_MAX, &ret);
	return -ENXIO;
}

static void fusb301_bclvl_changed(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc, limit;
	u8 status, type;

	rc = i2c_smbus_read_word_data(chip->client,
				FUSB301_REG_STATUS);
	if (IS_ERR_VALUE(rc)) {
		dev_err(cdev, "%s: failed to read\n", __func__);
		if(IS_ERR_VALUE(fusb301_reset_device(chip)))
			dev_err(cdev, "%s: failed to reset\n", __func__);
		return;
	}

	status = rc & 0xFF;
	type = (status & FUSB301_ATTACH) ?
			(rc >> 8) & FUSB301_TYPE_MASK : FUSB301_TYPE_INVALID;

	dev_dbg(cdev, "sts[0x%02x], type[0x%02x]\n", status, type);

	if (type == FUSB301_TYPE_SRC ||
			type == FUSB301_TYPE_PWR_AUD_ACC ||
			type == FUSB301_TYPE_PWR_DBG_ACC ||
			type == FUSB301_TYPE_PWR_ACC) {

		chip->bc_lvl = status & 0x06;
		limit = (chip->bc_lvl == FUSB301_SNK_3000MA ? 3000 :
				(chip->bc_lvl == FUSB301_SNK_1500MA ? 1500 : 0));

		fusb301_power_set_icurrent_max(chip, limit);
	}
}

static void fusb301_acc_changed(struct fusb301_chip *chip)
{
	/* TODO */
	/* implement acc changed work */
	return;
}

static void fusb301_src_detected(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;

	if (chip->mode & (FUSB301_SRC | FUSB301_SRC_ACC)) {
		dev_err(cdev, "not support in source mode\n");
		if(IS_ERR_VALUE(fusb301_reset_device(chip)))
			dev_err(cdev, "%s: failed to reset\n", __func__);
		return;
	}

	if (chip->state == FUSB_STATE_TRY_SNK)
		cancel_delayed_work(&chip->twork);
	fusb_update_state(chip, FUSB_STATE_ATTACHED_SNK);
	dual_role_instance_changed(chip->dual_role);
	chip->type = FUSB301_TYPE_SRC;
}

static void fusb301_snk_detected(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;

	if (chip->mode & (FUSB301_SNK | FUSB301_SNK_ACC)) {
		dev_err(cdev, "not support in sink mode\n");
		if(IS_ERR_VALUE(fusb301_reset_device(chip)))
			dev_err(cdev, "%s: failed to reset\n", __func__);
		return;
	}

	/* SW Try.SNK Workaround below Rev 1.2 */
	if ((!chip->triedsnk) &&
		(chip->mode & (FUSB301_DRP | FUSB301_DRP_ACC))) {
		if (IS_ERR_VALUE(fusb301_set_mode(chip, FUSB301_SNK)) ||
			IS_ERR_VALUE(fusb301_set_chip_state(chip,
						FUSB_STATE_UNATTACHED_SNK))) {
			dev_err(cdev, "%s: failed to config trySnk\n", __func__);
			if(IS_ERR_VALUE(fusb301_reset_device(chip)))
				dev_err(cdev, "%s: failed to reset\n", __func__);
		} else {
			fusb_update_state(chip, FUSB_STATE_TRY_SNK);
			chip->triedsnk = true;
			queue_delayed_work(chip->cc_wq, &chip->twork,
					msecs_to_jiffies(chip->pdata->ttry_timeout));
		}
	} else {
		/*
		 * chip->triedsnk == true
		 * or
		 * mode == FUSB301_SRC/FUSB301_SRC_ACC
		 */
		power_supply_set_usb_otg(chip->usb_psy, true);
		fusb301_set_dfp_power(chip, chip->pdata->dfp_power);
		if (chip->state == FUSB_STATE_TRYWAIT_SRC)
			cancel_delayed_work(&chip->twork);
		fusb_update_state(chip, FUSB_STATE_ATTACHED_SRC);
		dual_role_instance_changed(chip->dual_role);
		chip->type = FUSB301_TYPE_SNK;
	}
}

static void fusb301_dbg_acc_detected(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;

	if (chip->mode & (FUSB301_SRC | FUSB301_SNK | FUSB301_DRP)) {
		dev_err(cdev, "not support accessory mode\n");
		if(IS_ERR_VALUE(fusb301_reset_device(chip)))
			dev_err(cdev, "%s: failed to reset\n", __func__);
		return;
	}

	/*
	 * TODO
	 * need to implement
	 */
	fusb_update_state(chip, FUSB_STATE_DEBUG_ACCESSORY);
}

static void fusb301_aud_acc_detected(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;

	if (chip->mode & (FUSB301_SRC | FUSB301_SNK | FUSB301_DRP)) {
		dev_err(cdev, "not support accessory mode\n");
		if(IS_ERR_VALUE(fusb301_reset_device(chip)))
			dev_err(cdev, "%s: failed to reset\n", __func__);
		return;
	}

	/*
	 * TODO
	 * need to implement
	 */
	fusb_update_state(chip, FUSB_STATE_AUDIO_ACCESSORY);
}

static void fusb301_timer_try_expired(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;

	if (IS_ERR_VALUE(fusb301_set_mode(chip, FUSB301_SRC)) ||
		IS_ERR_VALUE(fusb301_set_chip_state(chip,
					FUSB_STATE_UNATTACHED_SRC))) {
		dev_err(cdev, "%s: failed to config tryWaitSrc\n", __func__);
		if(IS_ERR_VALUE(fusb301_reset_device(chip)))
			dev_err(cdev, "%s: failed to reset\n", __func__);
	} else {
		fusb_update_state(chip, FUSB_STATE_TRYWAIT_SRC);
		queue_delayed_work(chip->cc_wq, &chip->twork,
			msecs_to_jiffies(chip->pdata->ccdebounce_timeout));
	}
}

static void fusb301_detach(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;

	dev_dbg(cdev, "%s: type[0x%02x] chipstate[0x%02x]\n",
			__func__, chip->type, chip->state);

	switch (chip->state) {
	case FUSB_STATE_ATTACHED_SRC:
		power_supply_set_usb_otg(chip->usb_psy, false);
		fusb301_init_force_dfp_power(chip);
		break;
	case FUSB_STATE_ATTACHED_SNK:
		fusb301_power_set_icurrent_max(chip, 0);
		break;
	case FUSB_STATE_DEBUG_ACCESSORY:
	case FUSB_STATE_AUDIO_ACCESSORY:
		break;
	case FUSB_STATE_TRY_SNK:
	case FUSB_STATE_TRYWAIT_SRC:
		cancel_delayed_work(&chip->twork);
		break;
	case FUSB_STATE_DISABLED:
	case FUSB_STATE_ERROR_RECOVERY:
		break;
	case FUSB_STATE_TRY_SRC:
	case FUSB_STATE_TRYWAIT_SNK:
	default:
		dev_err(cdev, "%s: Invaild chipstate[0x%02x]\n",
				__func__, chip->state);
		break;
	}

	if ((chip->triedsnk && chip->pdata->try_snk_emulation)
					|| chip->role_switch) {
		chip->role_switch = false;
		if (IS_ERR_VALUE(fusb301_set_mode(chip,
						chip->pdata->init_mode)) ||
			IS_ERR_VALUE(fusb301_set_chip_state(chip,
						FUSB_STATE_ERROR_RECOVERY))) {
			dev_err(cdev, "%s: failed to set init mode\n", __func__);
		}
	}

	chip->type = FUSB301_TYPE_INVALID;
	chip->bc_lvl = FUSB301_SNK_0MA;
	chip->ufp_power = 0;
	/* Try.Snk in HW ? */
	chip->triedsnk = !chip->pdata->try_snk_emulation;
	chip->try_attcnt = 0;
	fusb_update_state(chip, FUSB_STATE_ERROR_RECOVERY);
	dual_role_instance_changed(chip->dual_role);
}

static bool fusb301_is_vbus_off(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc;

	rc = i2c_smbus_read_byte_data(chip->client,
				FUSB301_REG_STATUS);
	if (IS_ERR_VALUE(rc)) {
		dev_err(cdev, "%s: failed to read status\n", __func__);
		return false;
	}

	return !((rc & FUSB301_ATTACH) && (rc & FUSB301_VBUS_OK));
}

static bool fusb301_is_vbus_on(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc;

	rc = i2c_smbus_read_byte_data(chip->client, FUSB301_REG_STATUS);
	if (IS_ERR_VALUE(rc)) {
		dev_err(cdev, "%s: failed to read status\n", __func__);
		return false;
	}

	return !!(rc & FUSB301_VBUS_OK);
}

/* workaround BC Level detection plugging slowly with C ot A on Rev1.0 */
static bool fusb301_bclvl_detect_wa(struct fusb301_chip *chip,
							u8 status, u8 type)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	if (((type == FUSB301_TYPE_SRC) ||
		((type == FUSB301_TYPE_INVALID) && (status & FUSB301_VBUS_OK))) &&
		!(status & FUSB301_BCLVL_MASK) &&
		(chip->try_attcnt < FUSB301_MAX_TRY_COUNT)) {
		rc = fusb301_set_chip_state(chip,
					FUSB_STATE_ERROR_RECOVERY);
		if (IS_ERR_VALUE(rc)) {
			dev_err(cdev, "%s: failed to set error recovery state\n",
					__func__);
			goto err;
		}

		chip->try_attcnt++;
		msleep(100);

		/*
		 * when cable is unplug during bc level workaournd,
		 * detach interrupt does not occur
		 */
		if (fusb301_is_vbus_off(chip)) {
			chip->try_attcnt = 0;
			dev_dbg(cdev, "%s: vbus is off\n", __func__);
		}
		return true;
	}

err:
	chip->try_attcnt = 0;
	return false;
}

static void fusb301_attach(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc;
	u8 status, type;

	/* get status and type */
	rc = i2c_smbus_read_word_data(chip->client,
			FUSB301_REG_STATUS);
	if (IS_ERR_VALUE(rc)) {
		dev_err(cdev, "%s: failed to read status\n", __func__);
		return;
	}

	status = rc & 0xFF;
	type = (status & FUSB301_ATTACH) ?
			(rc >> 8) & FUSB301_TYPE_MASK : FUSB301_TYPE_INVALID;
	dev_info(cdev, "sts[0x%02x], type[0x%02x]\n", status, type);

	if ((chip->state != FUSB_STATE_ERROR_RECOVERY) &&
		(chip->state != FUSB_STATE_TRY_SNK) &&
		(chip->state != FUSB_STATE_TRYWAIT_SRC)) {
		rc = fusb301_set_chip_state(chip,
				FUSB_STATE_ERROR_RECOVERY);
		if (IS_ERR_VALUE(rc))
			dev_err(cdev, "%s: failed to set error recovery\n",
					__func__);
		fusb301_detach(chip);
		dev_err(cdev, "%s: Invaild chipstate[0x%02x]\n",
				__func__, chip->state);
		return;
	}

	if((chip->dev_id == FUSB301_REV10) &&
		fusb301_bclvl_detect_wa(chip, status, type)) {
		return;
	}

	switch (type) {
	case FUSB301_TYPE_SRC:
		fusb301_src_detected(chip);
		break;
	case FUSB301_TYPE_SNK:
		fusb301_snk_detected(chip);
		break;
	case FUSB301_TYPE_PWR_ACC:
		/*
		 * just power without functional dbg/aud determination
		 * ideally should not happen
		 */
		chip->type = type;
		break;
	case FUSB301_TYPE_DBG_ACC:
	case FUSB301_TYPE_PWR_DBG_ACC:
		fusb301_dbg_acc_detected(chip);
		chip->type = type;
		break;
	case FUSB301_TYPE_AUD_ACC:
	case FUSB301_TYPE_PWR_AUD_ACC:
		fusb301_aud_acc_detected(chip);
		chip->type = type;
		break;
	case FUSB301_TYPE_INVALID:
		fusb301_detach(chip);
		dev_err(cdev, "%s: Invaild type[0x%02x]\n", __func__, type);
		break;
	default:
		rc = fusb301_set_chip_state(chip,
				FUSB_STATE_ERROR_RECOVERY);
		if (IS_ERR_VALUE(rc))
			dev_err(cdev, "%s: failed to set error recovery\n",
					__func__);

		fusb301_detach(chip);
		dev_err(cdev, "%s: Unknwon type[0x%02x]\n", __func__, type);
		break;
	}
}

static void fusb301_timer_work_handler(struct work_struct *work)
{
	struct fusb301_chip *chip =
			container_of(work, struct fusb301_chip, twork.work);
	struct device *cdev = &chip->client->dev;

	mutex_lock(&chip->mlock);

	if (chip->state == FUSB_STATE_TRY_SNK) {
		if (fusb301_is_vbus_on(chip)) {
			if (IS_ERR_VALUE(fusb301_set_mode(chip,	chip->pdata->init_mode))) {
				dev_err(cdev, "%s: failed to set init mode\n", __func__);
			}
			chip->triedsnk = !chip->pdata->try_snk_emulation;
			mutex_unlock(&chip->mlock);
			return;
		}
		fusb301_timer_try_expired(chip);
	} else if (chip->state == FUSB_STATE_TRYWAIT_SRC) {
		fusb301_detach(chip);
	}

	mutex_unlock(&chip->mlock);
}

static void fusb301_work_handler(struct work_struct *work)
{
	struct fusb301_chip *chip =
			container_of(work, struct fusb301_chip, dwork);
	struct device *cdev = &chip->client->dev;
	int rc;
	u8 int_sts;

	mutex_lock(&chip->mlock);
	/* get interrupt */
	rc = i2c_smbus_read_byte_data(chip->client, FUSB301_REG_INT);
	if (IS_ERR_VALUE(rc)) {
		dev_err(cdev, "%s: failed to read interrupt\n", __func__);
		goto work_unlock;
	}
	int_sts = rc & FUSB301_INT_STS_MASK;

	dev_info(cdev, "%s: int_sts[0x%02x]\n", __func__, int_sts);

	if (int_sts & FUSB301_INT_DETACH) {
		fusb301_detach(chip);
	} else {
		if (int_sts & FUSB301_INT_ATTACH) {
			fusb301_attach(chip);
		}
		if (int_sts & FUSB301_INT_BCLVL) {
			fusb301_bclvl_changed(chip);
		}
		if (int_sts & FUSB301_INT_ACC_CH) {
			fusb301_acc_changed(chip);
		}
	}

work_unlock:
	mutex_unlock(&chip->mlock);
}

static irqreturn_t fusb301_interrupt(int irq, void *data)
{
	struct fusb301_chip *chip = (struct fusb301_chip *)data;

	if (!chip) {
		pr_err("%s : called before init.\n", __func__);
		return IRQ_HANDLED;
	}

	/*
	 * wake_lock_timeout, prevents multiple suspend entries
	 * before charger gets chance to trigger usb core for device
	 */
	wake_lock_timeout(&chip->wlock,
				msecs_to_jiffies(FUSB301_WAKE_LOCK_TIMEOUT));
	if (!queue_work(chip->cc_wq, &chip->dwork))
		dev_err(&chip->client->dev, "%s: can't alloc work\n", __func__);

	return IRQ_HANDLED;
}

static int fusb301_init_gpio(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	/* Start to enable fusb301 Chip */
	if (gpio_is_valid(chip->pdata->int_gpio)) {
		rc = gpio_request(chip->pdata->int_gpio, "fusb-irq-pin");
		if (rc) {
			dev_err(cdev, "unable to request int_gpio %d\n",
					chip->pdata->int_gpio);
			goto err;
		}
		rc = gpio_direction_input(chip->pdata->int_gpio);
		if (rc) {
			dev_err(cdev, "unable to set gpio direction %d\n",
					chip->pdata->int_gpio);
			goto err;
		}
	} else {
		dev_err(cdev, "int_gpio %d is not valid\n",
				chip->pdata->int_gpio);
		rc = -EINVAL;
	}

err:
	return rc;
}

static void fusb301_free_gpio(struct fusb301_chip *chip)
{
	if (gpio_is_valid(chip->pdata->int_gpio))
		gpio_free(chip->pdata->int_gpio);
}

static int fusb301_parse_dt(struct fusb301_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	struct device_node *dev_node = cdev->of_node;
	struct fusb301_data *data = chip->pdata;
	u32 timeoutValues[10];
	int len, rc = 0;

	data->int_gpio = of_get_named_gpio(dev_node,
				"qcom,fusb-irq", 0);
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

	data->try_snk_emulation = of_property_read_bool(dev_node,
			"fusb301,use-try-snk-emulation");

	if (data->try_snk_emulation) {
		of_get_property(dev_node, "fusb301,ttry-timer-value", &len);
		if (len > 0) {
			of_property_read_u32_array(dev_node,
					"fusb301,ttry-timer-value",
					timeoutValues,
					len/sizeof(u32));
			data->ttry_timeout = timeoutValues[chip->dev_id & 0x0F];
		} else {
			dev_err(cdev, "default ttry timeout\n");
			data->ttry_timeout = 600;
		}

		of_get_property(dev_node, "fusb301,ccdebounce-timer-value", &len);
		if (len > 0) {
			of_property_read_u32_array(dev_node,
					"fusb301,ccdebounce-timer-value",
					timeoutValues,
					len/sizeof(u32));
			data->ccdebounce_timeout = timeoutValues[chip->dev_id & 0x0F];
		} else {
			dev_err(cdev, "default ccdebounce timeout\n");
			data->ccdebounce_timeout = 200;
		}
		dev_info(cdev, "ttry-timeout:%d ccdebounce-timeout:%d\n",
				data->ttry_timeout, data->ccdebounce_timeout);
	} else {
		dev_dbg(cdev, "try snk in firmware\n");
	}
out:
	return rc;
}


static enum dual_role_property fusb_drp_properties[] = {
	DUAL_ROLE_PROP_MODE,
	DUAL_ROLE_PROP_PR,
	DUAL_ROLE_PROP_DR,
};

 /* Callback for "cat /sys/class/dual_role_usb/otg_default/<property>" */
static int dual_role_get_local_prop(struct dual_role_phy_instance *dual_role,
			enum dual_role_property prop,
			unsigned int *val)
{
	struct fusb301_chip *chip;
	struct i2c_client *client = dual_role_get_drvdata(dual_role);
	int ret = 0;

	if (!client)
		return -EINVAL;

	chip = i2c_get_clientdata(client);

	mutex_lock(&chip->mlock);
	if (chip->state == FUSB_STATE_ATTACHED_SRC) {
		if (prop == DUAL_ROLE_PROP_MODE)
			*val = DUAL_ROLE_PROP_MODE_DFP;
		else if (prop == DUAL_ROLE_PROP_PR)
			*val = DUAL_ROLE_PROP_PR_SRC;
		else if (prop == DUAL_ROLE_PROP_DR)
			*val = DUAL_ROLE_PROP_DR_HOST;
		else
			ret = -EINVAL;
	} else if (chip->state == FUSB_STATE_ATTACHED_SNK) {
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
	struct fusb301_chip *chip;
	struct i2c_client *client = dual_role_get_drvdata(dual_role);
	u8 mode, target_state, fallback_mode, fallback_state;
	int rc;
	bool try_snk_emulation;
	struct device *cdev;
	long timeout;

	if (!client)
		return -EIO;

	chip = i2c_get_clientdata(client);
	cdev = &client->dev;

	if (prop == DUAL_ROLE_PROP_MODE) {
		if (*val == DUAL_ROLE_PROP_MODE_DFP) {
			dev_dbg(cdev, "%s: Setting SRC mode\n", __func__);
			mode = FUSB301_SRC;
			fallback_mode = FUSB301_SNK;
			target_state = FUSB_STATE_ATTACHED_SRC;

			fallback_state = FUSB_STATE_ATTACHED_SNK;
		} else if (*val == DUAL_ROLE_PROP_MODE_UFP) {
			dev_dbg(cdev, "%s: Setting SNK mode\n", __func__);
			mode = FUSB301_SNK;
			fallback_mode = FUSB301_SRC;
			target_state = FUSB_STATE_ATTACHED_SNK;
			fallback_state = FUSB_STATE_ATTACHED_SRC;
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
	try_snk_emulation = chip->pdata->try_snk_emulation;
	/* role_switch is used a flag to force the chip back Try.SNK
	 * state machine. */
	chip->role_switch = false;
	chip->pdata->try_snk_emulation = false;
	chip->triedsnk = !chip->pdata->try_snk_emulation;
	rc = fusb301_set_mode(chip, (u8)mode);
	if (IS_ERR_VALUE(rc)) {
		if (IS_ERR_VALUE(fusb301_reset_device(chip)))
			dev_err(cdev, "%s: failed to reset\n", __func__);
		mutex_unlock(&chip->mlock);
		return rc;
	}

	rc = fusb301_set_chip_state(chip, FUSB_STATE_ERROR_RECOVERY);
	if (IS_ERR_VALUE(rc)) {
		if (IS_ERR_VALUE(fusb301_reset_device(chip)))
			dev_err(cdev, "%s: failed to reset\n", __func__);
		mutex_unlock(&chip->mlock);
		return rc;
	}

	fusb301_detach(chip);
	chip->pdata->try_snk_emulation = try_snk_emulation;
	chip->triedsnk = !chip->pdata->try_snk_emulation;
	chip->role_switch = true;
	mutex_unlock(&chip->mlock);

	timeout = wait_event_interruptible_timeout(mode_switch,
			chip->state == target_state,
			msecs_to_jiffies(ROLE_SWITCH_TIMEOUT));

	if (timeout > 0)
		return 0;

	mutex_lock(&chip->mlock);
	rc = fusb301_set_mode(chip, (u8)fallback_mode);
	if (IS_ERR_VALUE(rc)) {
		if (IS_ERR_VALUE(fusb301_reset_device(chip)))
			dev_err(cdev, "%s: failed to set mode\n", __func__);
		mutex_unlock(&chip->mlock);
		return rc;
	}
	rc = fusb301_set_chip_state(chip,
			fallback_mode == FUSB301_SRC ?
			FUSB_STATE_UNATTACHED_SRC :
			fallback_mode == FUSB301_SNK ?
			FUSB_STATE_UNATTACHED_SNK :
			FUSB_STATE_ERROR_RECOVERY);
	if (IS_ERR_VALUE(rc)) {
		if (IS_ERR_VALUE(fusb301_reset_device(chip)))
			dev_err(cdev, "%s: failed to set state\n", __func__);
		mutex_unlock(&chip->mlock);
		return rc;
	}
	mutex_unlock(&chip->mlock);

	timeout = wait_event_interruptible_timeout(mode_switch,
			chip->state == fallback_state,
			msecs_to_jiffies(ROLE_SWITCH_TIMEOUT));

	mutex_lock(&chip->mlock);
	if (IS_ERR_VALUE(fusb301_set_mode(chip,	chip->pdata->init_mode)))
		dev_err(cdev, "%s: failed to set init mode\n", __func__);
	mutex_unlock(&chip->mlock);

	return -EIO;
}

static int fusb301_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct fusb301_chip *chip;
	struct device *cdev = &client->dev;
	struct power_supply *usb_psy;
	struct dual_role_phy_desc *desc;
	struct dual_role_phy_instance *dual_role;
	int ret = 0;

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

		ret = fusb301_parse_dt(chip);
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

	chip->type = FUSB301_TYPE_INVALID;
	chip->state = FUSB_STATE_ERROR_RECOVERY;
	chip->bc_lvl = FUSB301_SNK_0MA;
	chip->ufp_power = 0;
	/* Try.Snk in HW? */
	chip->triedsnk = !chip->pdata->try_snk_emulation;
	chip->try_attcnt = 0;
	chip->usb_psy = usb_psy;

	chip->cc_wq = alloc_ordered_workqueue("fusb301-wq", WQ_HIGHPRI);
	if (!chip->cc_wq) {
		dev_err(cdev, "unable to create workqueue fuxb301-wq\n");
		goto err2;
	}
	INIT_WORK(&chip->dwork, fusb301_work_handler);
	INIT_DELAYED_WORK(&chip->twork, fusb301_timer_work_handler);
	wake_lock_init(&chip->wlock, WAKE_LOCK_SUSPEND, "fusb301_wake");
	mutex_init(&chip->mlock);

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
				"fusb301_irq", chip);
	if (ret) {
		dev_err(cdev, "failed to reqeust IRQ\n");
		goto err4;
	}

	if (IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)) {
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
		desc->properties = fusb_drp_properties;
		desc->num_properties = ARRAY_SIZE(fusb_drp_properties);
		desc->property_is_writeable = dual_role_is_writeable;
		dual_role = devm_dual_role_instance_register(cdev, desc);
		dual_role->drv_data = client;
		chip->dual_role = dual_role;
		chip->desc = desc;
	}

	ret = fusb301_reset_device(chip);
	if (ret) {
		dev_err(cdev, "failed to initialize\n");
		goto err5;
	}

	enable_irq_wake(chip->irq_gpio);

	queue_work(chip->cc_wq, &chip->dwork);

	return 0;

err5:
	if (IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF))
		devm_kfree(cdev, chip->desc);
err4:
	fusb301_destory_device(cdev);
err3:
	destroy_workqueue(chip->cc_wq);
	mutex_destroy(&chip->mlock);
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

	if (IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)) {
		devm_dual_role_instance_unregister(cdev, chip->dual_role);
		devm_kfree(cdev, chip->desc);
	}

	fusb301_destory_device(cdev);
	destroy_workqueue(chip->cc_wq);
	mutex_destroy(&chip->mlock);
	wake_lock_destroy(&chip->wlock);
	fusb301_free_gpio(chip);

	if (&client->dev.of_node)
		devm_kfree(cdev, chip->pdata);

	i2c_set_clientdata(client, NULL);
	devm_kfree(cdev, chip);

	return 0;
}

static void fusb301_shutdown(struct i2c_client *client)
{
	struct fusb301_chip *chip = i2c_get_clientdata(client);
	struct device *cdev = &client->dev;

	if (IS_ERR_VALUE(fusb301_set_mode(chip, FUSB301_SNK)) ||
			IS_ERR_VALUE(fusb301_set_chip_state(chip,
					FUSB_STATE_ERROR_RECOVERY)))
		dev_err(cdev, "%s: failed to set sink mode\n", __func__);
}

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

static const struct i2c_device_id fusb301_id_table[] = {
	{"fusb301", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, fusb301_id_table);

static struct of_device_id fusb301_match_table[] = {
	{ .compatible = "fusb301",},
	{ },
};

static struct i2c_driver fusb301_i2c_driver = {
	.driver = {
		.name = "fusb301",
		.owner = THIS_MODULE,
		.of_match_table = fusb301_match_table,
		.pm = &fusb301_dev_pm_ops,
	},
	.probe = fusb301_probe,
	.remove = fusb301_remove,
	.shutdown = fusb301_shutdown,
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
