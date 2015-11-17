/*
 * Fuel gauge driver for Maxim 17042 / 8966 / 8997
 *  Note that Maxim 8966 and 8997 are mfd and this is its subdevice.
 *
 * Copyright (C) 2011 Samsung Electronics
 * MyungJoo Ham <myungjoo.ham@samsung.com>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver is based on max17040_battery.c
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/pm.h>
#include <linux/mod_devicetable.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/power/max17042_battery.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/of_gpio.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/dropbox.h>

#define FUEL_GAUGE_REPORT "Fuel-Gauge_Report"
#define MAX_FULL_CAP "MAX1704X - Full Cap = %d\n"
#define FUEL_GAUGE_REPORT_SIZE 64

/* Status register bits */
#define STATUS_POR_BIT         (1 << 1)
#define STATUS_BST_BIT         (1 << 3)
#define STATUS_VMN_BIT         (1 << 8)
#define STATUS_TMN_BIT         (1 << 9)
#define STATUS_SMN_BIT         (1 << 10)
#define STATUS_BI_BIT          (1 << 11)
#define STATUS_VMX_BIT         (1 << 12)
#define STATUS_TMX_BIT         (1 << 13)
#define STATUS_SMX_BIT         (1 << 14)
#define STATUS_BR_BIT          (1 << 15)

/* Interrupt mask bits */
#define CONFIG_ALRT_BIT_ENBL	(1 << 2)
#define STATUS_INTR_VMIN_BIT	(1 << 8)
#define STATUS_INTR_TEMPMIN_BIT	(1 << 9)
#define STATUS_INTR_SOCMIN_BIT	(1 << 10)
#define STATUS_INTR_VMAX_BIT	(1 << 12)
#define STATUS_INTR_TEMP_BIT	(1 << 13)
#define STATUS_INTR_SOCMAX_BIT	(1 << 14)

#define VFSOC0_LOCK		0x0000
#define VFSOC0_UNLOCK		0x0080
#define MODEL_UNLOCK1	0X0059
#define MODEL_UNLOCK2	0X00C4
#define MODEL_LOCK1		0X0000
#define MODEL_LOCK2		0X0000

#define dQ_ACC_DIV	0x4
#define dP_ACC_100	0x1900
#define dP_ACC_200	0x3200

#define MAX17042_IC_VERSION	0x0092
#define MAX17047_IC_VERSION	0x00AC	/* same for max17050 */

#define MAX17050_CFG_REV_REG	MAX17042_ManName
#define MAX17050_CFG_REV_MASK	0x0007

#define MAX17050_FORCE_POR	0x000F
#define MAX17050_POR_WAIT_MS	20

#define INIT_DATA_PROPERTY		"maxim,regs-init-data"
#define CONFIG_NODE			"maxim,configuration"
#define CONFIG_PROPERTY			"config"
#define FULL_SOC_THRESH_PROPERTY	"full_soc_thresh"
#define DESIGN_CAP_PROPERTY		"design_cap"
#define ICHGT_TERM_PROPERTY		"ichgt_term"
#define LEARN_CFG_PROPERTY		"learn_cfg"
#define FILTER_CFG_PROPERTY		"filter_cfg"
#define RELAX_CFG_PROPERTY		"relax_cfg"
#define MISC_CFG_PROPERTY		"misc_cfg"
#define FULLCAP_PROPERTY		"fullcap"
#define FULLCAPNOM_PROPERTY		"fullcapnom"
#define LAVG_EMPTY_PROPERTY		"lavg_empty"
#define QRTBL00_PROPERTY		"qrtbl00"
#define QRTBL10_PROPERTY		"qrtbl10"
#define QRTBL20_PROPERTY		"qrtbl20"
#define QRTBL30_PROPERTY		"qrtbl30"
#define RCOMP0_PROPERTY			"rcomp0"
#define TCOMPC0_PROPERTY		"tcompc0"
#define CELL_CHAR_TBL_PROPERTY		"maxim,cell-char-tbl"
#define TGAIN_PROPERTY			"tgain"
#define TOFF_PROPERTY			"toff"
#define CGAIN_PROPERTY			"cgain"
#define COFF_PROPERTY			"coff"
#define TEMP_CONV_NODE			"maxim,temp-conv"
#define RESULT_PROPERTY			"result"
#define START_PROPERTY			"start"
#define REVISION			"rev"

#define MAX17042_CHRG_CONV_FCTR         500

#define RETRY_COUNT 5
int retry_sleep_us[RETRY_COUNT] = {
	100, 200, 300, 400, 500
};

struct max17042_wakeup_source {
	struct wakeup_source    source;
	unsigned long           disabled;
};

struct max17042_chip {
	struct i2c_client *client;
	struct regmap *regmap;
	struct power_supply battery;
	enum max170xx_chip_type chip_type;
	struct max17042_platform_data *pdata;
	struct work_struct work;
	struct work_struct check_temp_work;
	struct mutex check_temp_lock;
	int    init_complete;
	bool batt_undervoltage;
#ifdef CONFIG_BATTERY_MAX17042_DEBUGFS
	struct dentry *debugfs_root;
	u8 debugfs_addr;
#endif
	struct power_supply *batt_psy;
	int temp_state;
	int hotspot_temp;

	struct delayed_work iterm_work;
	struct delayed_work thread_work;
	struct max17042_wakeup_source max17042_wake_source;
	int charge_full_des;
	int taper_reached;
	bool factory_mode;
	char fg_report_str[FUEL_GAUGE_REPORT_SIZE];
	bool fullcap_report_sent;
	int last_fullcap;
};

static irqreturn_t max17042_thread_handler(int id, void *dev);

static void max17042_stay_awake(struct max17042_wakeup_source *source)
{
	if (__test_and_clear_bit(0, &source->disabled)) {
		__pm_stay_awake(&source->source);
		pr_debug("enabled source %s\n", source->source.name);
	}
}

static void max17042_relax(struct max17042_wakeup_source *source)
{
	if (!__test_and_set_bit(0, &source->disabled)) {
		__pm_relax(&source->source);
		pr_debug("disabled source %s\n", source->source.name);
	}
}

static enum power_supply_property max17042_battery_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_TEMP_HOTSPOT,
	POWER_SUPPLY_PROP_TAPER_REACHED,
	POWER_SUPPLY_PROP_HEALTH,
};

/* input and output temperature is in deci-centigrade */
static int max17042_conv_temp(struct max17042_temp_conv *conv, int t)
{
	int i; /* conversion table index */
	s16 *r = conv->result;
	int dt;

	/*
	 * conv->result[0] corresponds to conv->start temp, conv->result[1] to
	 * conv->start + 1 temp, etc. Find index to the conv->result table for
	 * t to be between index and index + 1 temperatures.
	 */
	i = t / 10 - conv->start; /* t is in 1/10th C, conv->start is in C */
	if (t < 0)
		i -= 1;

	/* Interpolate linearly if index and index + 1 are within the table */
	if (i < 0) {
		return r[0];
	} else if (i >= conv->num_result - 1) {
		return r[conv->num_result - 1];
	} else {
		dt = t - (conv->start + i) * 10; /* in 1/10th C */
		return r[i] + (r[i + 1] - r[i]) * dt / 10;
	}
}

/* input and output temperature is in centigrade */
static int max17042_find_temp_threshold(struct max17042_temp_conv *conv,
					int t, bool low_side)
{
	int i; /* conversion table index */
	s16 *r = conv->result;
	int dt;

	dt = t * 10;

	if (low_side) {
		for (i = (conv->num_result - 1); i >= 0; i--) {
			if (r[i] <= dt)
				break;
		}
	} else {
		for (i = 0; i < conv->num_result; i++) {
			if (r[i] >= dt)
				break;
		}
	}

	return conv->start + i;
}

int max17042_read_charge_counter(struct max17042_chip *chip, int ql)
{
	int uah = 0, uah_old = 0, uahl = 0;
	int i;
	int ret;
	struct regmap *map = chip->regmap;

	ret = regmap_read(map, MAX17042_QH, &uah);
	if (ret < 0)
		return ret;
	if (ql) {
		/* check if QH data-change between QH read and QL read */
		for (i = 0; i < 3; i++) {
			ret = regmap_read(map, MAX17042_QL, &uahl);
			if (ret < 0)
				return ret;
			uah_old = uah;
			ret = regmap_read(map, MAX17042_QH, &uah);
			if (ret < 0)
				return ret;
			if (uah == uah_old)
				break;
		}
		if (uah == uah_old)
			uah = uah * MAX17042_CHRG_CONV_FCTR +
				((uahl * MAX17042_CHRG_CONV_FCTR) >> 16);
		else
			uah = -ETIMEDOUT;
	} else
		uah = uah * MAX17042_CHRG_CONV_FCTR;
	return uah;
}

int max17042_read_temp(struct max17042_chip *chip, int *temp_dc)
{
	int ret;
	int intval;
	struct regmap *map = chip->regmap;

	ret = regmap_read(map, MAX17042_TEMP, &intval);
	if (ret < 0)
		return ret;

	/* The value is signed. */
	if (intval & 0x8000) {
		intval = (0x7fff & ~intval) + 1;
		intval *= -1;
	}

	/* The value is converted into deci-centigrade scale */
	/* Units of LSB = 1 / 256 degree Celsius */
	intval = intval * 10 / 256;

	/* Convert IC temp to "real" temp */
	if (chip->pdata->tcnv)
		intval = max17042_conv_temp(chip->pdata->tcnv,
					    intval);
	*temp_dc = intval;

	return 0;
}

static int max17042_set_property(struct power_supply *psy,
				 enum power_supply_property prop,
				 const union power_supply_propval *val)
{
	struct max17042_chip *chip = container_of(psy,
				struct max17042_chip, battery);

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP_HOTSPOT:
		chip->hotspot_temp = val->intval;
		schedule_work(&chip->check_temp_work);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int max17042_is_writeable(struct power_supply *psy,
				 enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP_HOTSPOT:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

static int max17042_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct max17042_chip *chip = container_of(psy,
				struct max17042_chip, battery);
	struct regmap *map = chip->regmap;
	int ret, cfd_max;
	u32 data;

	if (!chip->init_complete)
		return -EAGAIN;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		ret = regmap_read(map, MAX17042_STATUS, &data);
		if (ret < 0)
			return ret;

		if (data & MAX17042_STATUS_BattAbsent)
			val->intval = 0;
		else
			val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		ret = regmap_read(map, MAX17042_FullCAP, &data);
		if (ret < 0)
			return ret;
		if (!chip->charge_full_des)
			return data;
		val->intval = data * MAX17042_CHRG_CONV_FCTR;
		val->intval *= 100;
		val->intval /= chip->charge_full_des;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		ret = regmap_read(map, MAX17042_MinMaxVolt, &data);
		if (ret < 0)
			return ret;

		val->intval = data >> 8;
		val->intval *= 20000; /* Units of LSB = 20mV */
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		if (chip->chip_type == MAX17042)
			ret = regmap_read(map, MAX17042_V_empty, &data);
		else
			ret = regmap_read(map, MAX17047_V_empty, &data);
		if (ret < 0)
			return ret;

		val->intval = data >> 7;
		val->intval *= 10000; /* Units of LSB = 10mV */
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = regmap_read(map, MAX17042_VCELL, &data);
		if (ret < 0)
			return ret;

		val->intval = data * 625 / 8;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		ret = regmap_read(map, MAX17042_AvgVCELL, &data);
		if (ret < 0)
			return ret;

		val->intval = data * 625 / 8;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		ret = regmap_read(map, MAX17042_OCVInternal, &data);
		if (ret < 0)
			return ret;

		val->intval = data * 625 / 8;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		ret = regmap_read(map, MAX17042_RepSOC, &data);
		if (ret < 0)
			return ret;

		if ((data & 0xFF) != 0) {
			data >>= 8;
			data++; /* Round up */
		} else
			data >>= 8;

		if (data > 100)
			data = 100;

		if (data == 0 &&
		    chip->pdata->batt_undervoltage_zero_soc) {
			if (chip->batt_undervoltage)
				val->intval = 0;
			else
				val->intval = 1;
		} else
			val->intval = data;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		/* High Threshold for FCC reporting is 104% of design cap */
		cfd_max =
			(chip->charge_full_des +
				((chip->charge_full_des * 4) / 100));
		ret = regmap_read(map, MAX17042_FullCAP, &data);
		if (ret < 0)
			return ret;

		val->intval = data * 1000 / 2;

		if (chip->factory_mode)
			break;

		/* If Full Cap deviates from the range report it once */
		if (!chip->fullcap_report_sent &&
		    (val->intval > cfd_max ||
		     (val->intval * 2) < chip->charge_full_des)) {
			snprintf(chip->fg_report_str, FUEL_GAUGE_REPORT_SIZE,
				MAX_FULL_CAP, val->intval);
			dropbox_queue_event_text(FUEL_GAUGE_REPORT,
				chip->fg_report_str,
				strlen(chip->fg_report_str));
			chip->fullcap_report_sent = true;
		} else if (chip->fullcap_report_sent) {
			snprintf(chip->fg_report_str, FUEL_GAUGE_REPORT_SIZE,
				 MAX_FULL_CAP, val->intval);
			dropbox_queue_event_text(FUEL_GAUGE_REPORT,
						 chip->fg_report_str,
						 strlen(chip->fg_report_str));
			chip->fullcap_report_sent = false;
		}

		if ((val->intval * 2) < chip->charge_full_des) {
			dev_warn(&chip->client->dev,
				 "Error fullcap too small! Forcing POR!\n");
			dev_warn(&chip->client->dev,
				 "FullCap %d mAhr\n",
				  val->intval * 2);
			regmap_write(map, MAX17042_VFSOC0Enable,
				     MAX17050_FORCE_POR);
			msleep(MAX17050_POR_WAIT_MS);
			schedule_delayed_work(&chip->thread_work,
				msecs_to_jiffies(0));
		}

		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		ret = max17042_read_charge_counter(chip, 1);
		if (ret < 0)
			return ret;

		val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = max17042_read_temp(chip, &val->intval);
		if (ret < 0)
			return ret;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (chip->pdata->enable_current_sense) {
			ret = regmap_read(map, MAX17042_Current, &data);
			if (ret < 0)
				return ret;

			val->intval = data;
			if (val->intval & 0x8000) {
				/* Negative */
				val->intval = ~val->intval & 0x7fff;
				val->intval++;
				val->intval *= -1;
			}
			val->intval *= 1562500 / chip->pdata->r_sns;
		} else {
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		if (chip->pdata->enable_current_sense) {
			ret = regmap_read(map, MAX17042_AvgCurrent, &data);
			if (ret < 0)
				return ret;

			val->intval = data;
			if (val->intval & 0x8000) {
				/* Negative */
				val->intval = ~val->intval & 0x7fff;
				val->intval++;
				val->intval *= -1;
			}
			val->intval *= 1562500 / chip->pdata->r_sns;
		} else {
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_TEMP_HOTSPOT:
		val->intval = chip->hotspot_temp;
		break;
	case POWER_SUPPLY_PROP_TAPER_REACHED:
		val->intval = chip->taper_reached;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = chip->temp_state;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

#define USLEEP_RANGE 50
static int max17042_write_verify_reg(struct regmap *map, u8 reg, u32 value)
{
	int ret;
	u32 read_value;
	int i = 0;

	for (i = 0; i < RETRY_COUNT; i++) {
		ret = regmap_write(map, reg, value);
		if (ret < 0) {
			/* sleep for few micro sec before retrying */
			usleep_range(retry_sleep_us[i],
				     retry_sleep_us[i] + USLEEP_RANGE);
		} else {
			regmap_read(map, reg, &read_value);
			if (read_value != value) {
				ret = -EIO;
			}
			break;
		}
	}

	if (ret < 0) {
		pr_err("%s: err %d\n", __func__, ret);
		return ret;
	}

	return read_value;
}

static inline void max17042_override_por(struct regmap *map,
					 u8 reg, u16 value)
{
	if (value)
		regmap_write(map, reg, value);
}

static inline void max10742_unlock_model(struct max17042_chip *chip)
{
	struct regmap *map = chip->regmap;
	regmap_write(map, MAX17042_MLOCKReg1, MODEL_UNLOCK1);
	regmap_write(map, MAX17042_MLOCKReg2, MODEL_UNLOCK2);
}

static inline void max10742_lock_model(struct max17042_chip *chip)
{
	struct regmap *map = chip->regmap;

	regmap_write(map, MAX17042_MLOCKReg1, MODEL_LOCK1);
	regmap_write(map, MAX17042_MLOCKReg2, MODEL_LOCK2);
}

static inline void max17042_write_model_data(struct max17042_chip *chip,
					u8 addr, int size)
{
	struct regmap *map = chip->regmap;
	int i;
	for (i = 0; i < size; i++)
		regmap_write(map, addr + i,
			chip->pdata->config_data->cell_char_tbl[i]);
}

static inline void max17042_read_model_data(struct max17042_chip *chip,
					u8 addr, u16 *data, int size)
{
	struct regmap *map = chip->regmap;
	int i;
	u32 data_raw;

	for (i = 0; i < size; i++) {
		regmap_read(map, addr + i, &data_raw);
		data[i] = (u16)data_raw;
		data_raw = 0x0000;
	}
}

static inline int max17042_model_data_compare(struct max17042_chip *chip,
					u16 *data1, u16 *data2, int size)
{
	int i;

	if (memcmp(data1, data2, size)) {
		dev_err(&chip->client->dev, "%s compare failed\n", __func__);
		for (i = 0; i < size; i++)
			dev_info(&chip->client->dev, "0x%x, 0x%x",
				data1[i], data2[i]);
		dev_info(&chip->client->dev, "\n");
		return -EINVAL;
	}
	return 0;
}

static int max17042_init_model(struct max17042_chip *chip)
{
	int ret;
	int table_size = ARRAY_SIZE(chip->pdata->config_data->cell_char_tbl);
	u16 *temp_data;

	temp_data = kcalloc(table_size, sizeof(*temp_data), GFP_KERNEL);
	if (!temp_data)
		return -ENOMEM;

	max10742_unlock_model(chip);
	max17042_write_model_data(chip, MAX17042_MODELChrTbl,
				table_size);
	max17042_read_model_data(chip, MAX17042_MODELChrTbl, temp_data,
				table_size);

	ret = max17042_model_data_compare(
		chip,
		chip->pdata->config_data->cell_char_tbl,
		temp_data,
		table_size);

	max10742_lock_model(chip);
	kfree(temp_data);

	return ret;
}

static int max17042_verify_model_lock(struct max17042_chip *chip)
{
	int i;
	int table_size = ARRAY_SIZE(chip->pdata->config_data->cell_char_tbl);
	u16 *temp_data;
	int ret = 0;

	temp_data = kcalloc(table_size, sizeof(*temp_data), GFP_KERNEL);
	if (!temp_data)
		return -ENOMEM;

	max17042_read_model_data(chip, MAX17042_MODELChrTbl, temp_data,
				table_size);
	for (i = 0; i < table_size; i++)
		if (temp_data[i])
			ret = -EINVAL;

	kfree(temp_data);
	return ret;
}

static void max17042_write_config_regs(struct max17042_chip *chip)
{
	struct max17042_config_data *config = chip->pdata->config_data;
	struct regmap *map = chip->regmap;

	regmap_write(map, MAX17042_CONFIG, config->config);
	regmap_write(map, MAX17042_LearnCFG, config->learn_cfg);
	regmap_write(map, MAX17042_FilterCFG,
			config->filter_cfg);
	regmap_write(map, MAX17042_RelaxCFG, config->relax_cfg);
	if (chip->chip_type == MAX17047)
		regmap_write(map, MAX17047_FullSOCThr,
						config->full_soc_thresh);
}

static void  max17042_write_custom_regs(struct max17042_chip *chip)
{
	struct max17042_config_data *config = chip->pdata->config_data;
	struct regmap *map = chip->regmap;

	max17042_write_verify_reg(map, MAX17042_RCOMP0, config->rcomp0);
	max17042_write_verify_reg(map, MAX17042_TempCo,	config->tcompc0);
	max17042_write_verify_reg(map, MAX17042_ICHGTerm, config->ichgt_term);
	max17042_write_verify_reg(map, MAX17042_LAvg_empty,
				  config->lavg_empty);
	if (chip->chip_type == MAX17042) {
		regmap_write(map, MAX17042_EmptyTempCo,	config->empty_tempco);
		max17042_write_verify_reg(map, MAX17042_K_empty0,
					config->kempty0);
	} else {
		max17042_write_verify_reg(map, MAX17047_QRTbl00,
						config->qrtbl00);
		max17042_write_verify_reg(map, MAX17047_QRTbl10,
						config->qrtbl10);
		max17042_write_verify_reg(map, MAX17047_QRTbl20,
						config->qrtbl20);
		max17042_write_verify_reg(map, MAX17047_QRTbl30,
						config->qrtbl30);
	}
}

static void max17042_update_capacity_regs(struct max17042_chip *chip)
{
	struct max17042_config_data *config = chip->pdata->config_data;
	struct regmap *map = chip->regmap;

	max17042_write_verify_reg(map, MAX17042_FullCAP,
				config->fullcap);
	regmap_write(map, MAX17042_DesignCap, config->design_cap);
	max17042_write_verify_reg(map, MAX17042_FullCAPNom,
				config->fullcapnom);
}

static void max17042_reset_vfsoc0_qh0_regs(struct max17042_chip *chip)
{
	unsigned int data;
	struct regmap *map = chip->regmap;

	regmap_read(map, MAX17042_VFSOC, &data);
	regmap_write(map, MAX17042_VFSOC0Enable, VFSOC0_UNLOCK);
	max17042_write_verify_reg(map, MAX17042_VFSOC0, data);
	regmap_write(map, MAX17042_VFSOC0Enable, VFSOC0_LOCK);

	regmap_read(map, MAX17042_QH, &data);
	regmap_write(map, MAX17042_QH0, data);
}

static void max17042_load_new_capacity_params(struct max17042_chip *chip)
{
	u32 rem_cap, vfSoc, rep_cap, dq_acc;

	struct max17042_config_data *config = chip->pdata->config_data;
	struct regmap *map = chip->regmap;

	regmap_read(map, MAX17042_VFSOC, &vfSoc);

	/* vfSoc needs to shifted by 8 bits to get the
	 * perc in 1% accuracy, to get the right rem_cap multiply
	 * fullcapnom by vfSoc and devide by 100
	 */

	rem_cap = ((vfSoc >> 8) * config->fullcapnom) / 100;
	max17042_write_verify_reg(map, MAX17042_RemCap, rem_cap);

	rep_cap = rem_cap;
	max17042_write_verify_reg(map, MAX17042_RepCap, rep_cap);

	/* Write dQ_acc to 200% of Capacity and dP_acc to 200% */
	dq_acc = config->fullcap / dQ_ACC_DIV;
	max17042_write_verify_reg(map, MAX17042_dQacc, dq_acc);
	max17042_write_verify_reg(map, MAX17042_dPacc, dP_ACC_200);

	max17042_write_verify_reg(map, MAX17042_FullCAP,
			config->fullcap);
	regmap_write(map, MAX17042_DesignCap,
			config->design_cap);
	max17042_write_verify_reg(map, MAX17042_FullCAPNom,
			config->fullcapnom);
	/* Update SOC register with new SOC */
	regmap_write(map, MAX17042_RepSOC, vfSoc);
}

/*
 * Block write all the override values coming from platform data.
 * This function MUST be called before the POR initialization proceedure
 * specified by maxim.
 */
#define POR_VALRT_THRESHOLD 0xAA00
static inline void max17042_override_por_values(struct max17042_chip *chip)
{
	struct regmap *map = chip->regmap;
	struct max17042_config_data *config = chip->pdata->config_data;

	max17042_override_por(map, MAX17042_TGAIN, config->tgain);
	max17042_override_por(map, MAx17042_TOFF, config->toff);
	max17042_override_por(map, MAX17042_CGAIN, config->cgain);
	max17042_override_por(map, MAX17042_COFF, config->coff);

	if (!chip->factory_mode)
		max17042_override_por(map, MAX17042_VALRT_Th,
				      POR_VALRT_THRESHOLD);
	max17042_override_por(map, MAX17042_TALRT_Th, config->talrt_thresh);
	max17042_override_por(map, MAX17042_SALRT_Th,
						config->soc_alrt_thresh);
	max17042_override_por(map, MAX17042_CONFIG, config->config);
	max17042_override_por(map, MAX17042_SHDNTIMER, config->shdntimer);

	max17042_override_por(map, MAX17042_DesignCap, config->design_cap);
	max17042_override_por(map, MAX17042_ICHGTerm, config->ichgt_term);

	max17042_override_por(map, MAX17042_AtRate, config->at_rate);
	max17042_override_por(map, MAX17042_LearnCFG, config->learn_cfg);
	max17042_override_por(map, MAX17042_FilterCFG, config->filter_cfg);
	max17042_override_por(map, MAX17042_RelaxCFG, config->relax_cfg);
	max17042_override_por(map, MAX17042_MiscCFG, config->misc_cfg);
	max17042_override_por(map, MAX17042_MaskSOC, config->masksoc);

	max17042_override_por(map, MAX17042_FullCAP, config->fullcap);
	max17042_override_por(map, MAX17042_FullCAPNom, config->fullcapnom);

	if (chip->last_fullcap != -EINVAL) {
		max17042_override_por(map, MAX17042_FullCAP,
				      chip->last_fullcap);
		max17042_override_por(map, MAX17042_FullCAPNom,
				      chip->last_fullcap);
	} else {
		max17042_override_por(map, MAX17042_FullCAP,
				      config->fullcap);
		max17042_override_por(map, MAX17042_FullCAPNom,
				      config->fullcapnom);
	}
	if (chip->chip_type == MAX17042)
		max17042_override_por(map, MAX17042_SOC_empty,
						config->socempty);
	max17042_override_por(map, MAX17042_LAvg_empty, config->lavg_empty);
	max17042_override_por(map, MAX17042_dQacc, config->dqacc);
	max17042_override_por(map, MAX17042_dPacc, config->dpacc);

	if (chip->chip_type == MAX17042)
		max17042_override_por(map, MAX17042_V_empty, config->vempty);
	else
		max17042_override_por(map, MAX17047_V_empty, config->vempty);
	max17042_override_por(map, MAX17042_TempNom, config->temp_nom);
	max17042_override_por(map, MAX17042_TempLim, config->temp_lim);
	max17042_override_por(map, MAX17042_FCTC, config->fctc);
	max17042_override_por(map, MAX17042_RCOMP0, config->rcomp0);
	max17042_override_por(map, MAX17042_TempCo, config->tcompc0);
	if (chip->chip_type == MAX17042) {
		max17042_override_por(map, MAX17042_EmptyTempCo,
						config->empty_tempco);
		max17042_override_por(map, MAX17042_K_empty0,
						config->kempty0);
	}
}

static int max17042_init_chip(struct max17042_chip *chip)
{
	struct regmap *map = chip->regmap;
	int ret;
	int val;
	struct max17042_config_data *config = chip->pdata->config_data;

	max17042_override_por_values(chip);
	/* After Power up, the MAX17042 requires 500mS in order
	 * to perform signal debouncing and initial SOC reporting
	 */
	msleep(500);

	/* Initialize configaration */
	max17042_write_config_regs(chip);

	/* write cell characterization data */
	ret = max17042_init_model(chip);
	if (ret) {
		dev_err(&chip->client->dev, "%s init failed\n",
			__func__);
		return -EIO;
	}

	ret = max17042_verify_model_lock(chip);
	if (ret) {
		dev_err(&chip->client->dev, "%s lock verify failed\n",
			__func__);
		return -EIO;
	}
	/* write custom parameters */
	max17042_write_custom_regs(chip);

	/* update capacity params */
	max17042_update_capacity_regs(chip);

	/* delay must be atleast 350mS to allow VFSOC
	 * to be calculated from the new configuration
	 */
	msleep(350);

	/* reset vfsoc0 and qh0 regs */
	max17042_reset_vfsoc0_qh0_regs(chip);

	/* load new capacity params */
	max17042_load_new_capacity_params(chip);

	/* Set Config Revision bits */
	regmap_read(map, MAX17050_CFG_REV_REG, &val);
	val &= (~MAX17050_CFG_REV_MASK);
	val |= config->revision;
	regmap_write(map, MAX17050_CFG_REV_REG, val);

	/* Init complete, Clear the POR bit */
	regmap_read(map, MAX17042_STATUS, &val);
	regmap_write(map, MAX17042_STATUS, val & (~STATUS_POR_BIT));
	return 0;
}

static void max17042_set_temp_threshold(struct max17042_chip *chip,
					int max_c,
					int min_c)
{
	u16 temp_tr;
	s8 max;
	s8 min;
	struct regmap *map = chip->regmap;

	max = (s8)max17042_find_temp_threshold(chip->pdata->tcnv, max_c,
					       false);
	min = (s8)max17042_find_temp_threshold(chip->pdata->tcnv, min_c,
					       true);

	temp_tr = 0;
	temp_tr |= min;
	temp_tr &= 0x00FF;

	temp_tr |=  (max << 8) & 0xFF00;

	pr_debug("Program Temp Thresholds = 0x%X\n", temp_tr);

	regmap_write(map, MAX17042_TALRT_Th, temp_tr);
}

#define HYSTERISIS_DEGC 2
static int max17042_check_temp(struct max17042_chip *chip)
{
	int batt_temp;
	int hotspot;
	int max_t = 0;
	int min_t = 0;
	struct max17042_platform_data *pdata;
	int temp_state = POWER_SUPPLY_HEALTH_GOOD;
	union power_supply_propval ps = {0, };
	const char *batt_psy_name;
	int ret = 0;

	if (!chip)
		return 0;

	mutex_lock(&chip->check_temp_lock);
	pdata = chip->pdata;

	max17042_read_temp(chip, &batt_temp);

	/* Convert to Degrees C */
	batt_temp /= 10;
	hotspot = chip->hotspot_temp / 1000;

	/* Override batt_temp if battery hot spot condition
	   is active */
	if ((batt_temp > pdata->cool_temp_c) &&
	    (hotspot > batt_temp) &&
	    (hotspot >= pdata->hotspot_thrs_c)) {
		batt_temp = hotspot;
	}

	if (chip->temp_state == POWER_SUPPLY_HEALTH_WARM) {
		if (batt_temp >= pdata->hot_temp_c)
			/* Warm to Hot */
			temp_state = POWER_SUPPLY_HEALTH_OVERHEAT;
		else if (batt_temp <=
			 pdata->warm_temp_c - HYSTERISIS_DEGC)
			/* Warm to Normal */
			temp_state = POWER_SUPPLY_HEALTH_GOOD;
		else
			/* Stay Warm */
			temp_state = POWER_SUPPLY_HEALTH_WARM;
	} else if ((chip->temp_state == POWER_SUPPLY_HEALTH_GOOD) ||
		   (chip->temp_state == POWER_SUPPLY_HEALTH_UNKNOWN)) {
		if (batt_temp >= pdata->warm_temp_c)
			/* Normal to Warm */
			temp_state = POWER_SUPPLY_HEALTH_WARM;
		else if (batt_temp <= pdata->cool_temp_c)
			/* Normal to Cool */
			temp_state = POWER_SUPPLY_HEALTH_COOL;
		else
			/* Stay Normal */
			temp_state = POWER_SUPPLY_HEALTH_GOOD;
	} else if (chip->temp_state == POWER_SUPPLY_HEALTH_COOL) {
		if (batt_temp >=
		    pdata->cool_temp_c + HYSTERISIS_DEGC)
			/* Cool to Normal */
			temp_state = POWER_SUPPLY_HEALTH_GOOD;
		else if (batt_temp <= pdata->cold_temp_c)
			/* Cool to Cold */
			temp_state = POWER_SUPPLY_HEALTH_COLD;
		else
			/* Stay Cool */
			temp_state = POWER_SUPPLY_HEALTH_COOL;
	} else if (chip->temp_state == POWER_SUPPLY_HEALTH_COLD) {
		if (batt_temp >=
		    pdata->cold_temp_c + HYSTERISIS_DEGC)
			/* Cold to Cool */
			temp_state = POWER_SUPPLY_HEALTH_COOL;
		else
			/* Stay Cold */
			temp_state = POWER_SUPPLY_HEALTH_COLD;
	} else if (chip->temp_state == POWER_SUPPLY_HEALTH_OVERHEAT) {
		if (batt_temp <=
		    pdata->hot_temp_c - HYSTERISIS_DEGC)
			/* Hot to Warm */
			temp_state = POWER_SUPPLY_HEALTH_WARM;
		else
			/* Stay Hot */
			temp_state = POWER_SUPPLY_HEALTH_OVERHEAT;
	}

	if (chip->temp_state != temp_state) {
		chip->temp_state = temp_state;
		if (chip->temp_state == POWER_SUPPLY_HEALTH_WARM) {
			max_t = pdata->hot_temp_c;
			min_t = pdata->warm_temp_c - HYSTERISIS_DEGC;
		} else if (chip->temp_state == POWER_SUPPLY_HEALTH_GOOD) {
			max_t = pdata->warm_temp_c;
			min_t = pdata->cool_temp_c;
		} else if (chip->temp_state == POWER_SUPPLY_HEALTH_COOL) {
			max_t = pdata->cool_temp_c + HYSTERISIS_DEGC;
			min_t = pdata->cold_temp_c;
		} else if (chip->temp_state == POWER_SUPPLY_HEALTH_COLD) {
			max_t = pdata->cold_temp_c + HYSTERISIS_DEGC;
			min_t = pdata->cold_temp_c * 2;
		} else if (chip->temp_state == POWER_SUPPLY_HEALTH_OVERHEAT) {
			max_t = pdata->hot_temp_c * 2;
			min_t = pdata->hot_temp_c - HYSTERISIS_DEGC;
		}

		max17042_set_temp_threshold(chip, max_t, min_t);

		pr_warn("Battery Temp State = %d\n", chip->temp_state);

		if (!chip->batt_psy && chip->pdata->batt_psy_name) {
			batt_psy_name = chip->pdata->batt_psy_name;
			chip->batt_psy =
				power_supply_get_by_name((char *)batt_psy_name);
		}

		if (chip->batt_psy) {
			ps.intval = chip->temp_state;
			chip->batt_psy->set_property(chip->batt_psy,
					     POWER_SUPPLY_PROP_HEALTH, &ps);
		}

		power_supply_changed(&chip->battery);
		ret = 1;
	}
	mutex_unlock(&chip->check_temp_lock);

	return ret;
}

#define ZERO_PERC_SOC_THRESHOLD 0x0100
static void max17042_set_soc_threshold(struct max17042_chip *chip, u16 off)
{
	struct regmap *map = chip->regmap;
	u32 soc, soc_tr;

	/* program interrupt thesholds such that we should
	 * get interrupt for every 'off' perc change in the soc
	 */
	regmap_read(map, MAX17042_RepSOC, &soc);
	soc >>= 8;
	if (soc <= 0)
		regmap_write(map, MAX17042_SALRT_Th, ZERO_PERC_SOC_THRESHOLD);
	else {
		soc_tr = (soc + off) << 8;
		soc_tr |= (soc - off);
		regmap_write(map, MAX17042_SALRT_Th, soc_tr);
	}
}

static irqreturn_t max17042_thread_handler(int id, void *dev)
{
	struct max17042_chip *chip = dev;

	schedule_delayed_work(&chip->thread_work,
			      msecs_to_jiffies(10));

	return IRQ_HANDLED;
}

static void max17042_thread_worker(struct work_struct *work)
{
	u32 val;
	union power_supply_propval ret = {0, };
	const char *batt_psy_name;
	struct max17042_chip *chip =
		container_of(work, struct max17042_chip,
				thread_work.work);
	struct regmap *map = chip->regmap;

	if (!chip->batt_psy && chip->pdata->batt_psy_name) {
		batt_psy_name = chip->pdata->batt_psy_name;
		chip->batt_psy =
			power_supply_get_by_name((char *)batt_psy_name);
	}

	regmap_read(chip->regmap, MAX17042_STATUS, &val);
	if ((val & STATUS_POR_BIT) && chip->init_complete) {
		dev_warn(&chip->client->dev, "POR Detected, Loading Config\n");
		chip->init_complete = 0;
		schedule_work(&chip->work);

		return;
	}
	if ((val & STATUS_INTR_SOCMIN_BIT) ||
		(val & STATUS_INTR_SOCMAX_BIT)) {
		dev_info(&chip->client->dev, "SOC threshold INTR\n");
		max17042_set_soc_threshold(chip, 1);
	}

	if (val & STATUS_INTR_VMIN_BIT) {
		dev_info(&chip->client->dev, "Battery undervoltage INTR\n");
		chip->batt_undervoltage = true;
	} else if (val & STATUS_INTR_VMAX_BIT) {
		dev_info(&chip->client->dev, "Battery overvoltage INTR\n");
		if (!chip->factory_mode)
			regmap_write(map, MAX17042_VALRT_Th,
				   chip->pdata->config_data->valrt_thresh);
	}

	if ((val & STATUS_INTR_TEMP_BIT) ||
	    (val & STATUS_INTR_TEMPMIN_BIT)) {
		dev_info(&chip->client->dev, "Temperature INTR\n");
		max17042_check_temp(chip);
	}


	power_supply_changed(&chip->battery);

	if (chip->batt_psy)
		chip->batt_psy->set_property(chip->batt_psy,
				POWER_SUPPLY_PROP_CAPACITY_LEVEL, &ret);
	return;
}

static void max17042_init_worker(struct work_struct *work)
{
	struct max17042_chip *chip = container_of(work,
				struct max17042_chip, work);
	int ret;

	/* Initialize registers according to values from the platform data */
	if (chip->pdata->enable_por_init && chip->pdata->config_data) {
		ret = max17042_init_chip(chip);
		if (ret)
			return;
	}

	max17042_set_soc_threshold(chip, 1);
	chip->temp_state = POWER_SUPPLY_HEALTH_UNKNOWN;
	max17042_check_temp(chip);

	chip->init_complete = 1;
}

static void max17042_check_temp_worker(struct work_struct *work)
{
	struct max17042_chip *chip = container_of(work,
						  struct max17042_chip,
						  check_temp_work);

	max17042_check_temp(chip);
}

struct max17042_config_data eg30_lg_config = {
	/* External current sense resistor value in milli-ohms */
	.cur_sense_val = 10,

	/* A/D measurement */
	.tgain = 0xE71C,	/* 0x2C */
	.toff = 0x251A,		/* 0x2D */
	.cgain = 0x4000,	/* 0x2E */
	.coff = 0x0000,		/* 0x2F */

	/* Alert / Status */
	.valrt_thresh = 0xFF97,	/* 0x01 */
	.talrt_thresh = 0x7F80,	/* 0x02 */
	.soc_alrt_thresh = 0xFF00,	/* 0x03 */
	.config = 0x0214,		/* 0x01D */
	.shdntimer = 0xE000,	/* 0x03F */

	/* App data */
	.full_soc_thresh = 0x6200,	/* 0x13 */
	.design_cap = 4172,	/* 0x18 0.5 mAh per bit */
	.ichgt_term = 0x01C0,	/* 0x1E */

	/* MG3 config */
	.filter_cfg = 0x87A4,	/* 0x29 */

	/* MG3 save and restore */
	.fullcap = 4172,	/* 0x10 0.5 mAh per bit */
	.fullcapnom = 4172,	/* 0x23 0.5 mAh per bit */
	.qrtbl00 = 0x1E01,	/* 0x12 */
	.qrtbl10 = 0x1281,	/* 0x22 */
	.qrtbl20 = 0x0781,	/* 0x32 */
	.qrtbl30 = 0x0681,	/* 0x42 */

	/* Cell technology from power_supply.h */
	.cell_technology = POWER_SUPPLY_TECHNOLOGY_LION,

	/* Cell Data */
	.vempty = 0x7D5A,		/* 0x12 */
	.temp_nom = 0x1400,	/* 0x24 */
	.rcomp0 = 0x004A,		/* 0x38 */
	.tcompc0 = 0x243A,	/* 0x39 */
	.cell_char_tbl = {
		0x9B00,
		0xA470,
		0xB2E0,
		0xB7B0,
		0xB9B0,
		0xBB70,
		0xBC20,
		0xBD00,
		0xBE10,
		0xC060,
		0xC220,
		0xC750,
		0xCB80,
		0xCF10,
		0xD2A0,
		0xD840,
		0x0070,
		0x0020,
		0x0900,
		0x0D00,
		0x0B00,
		0x1B40,
		0x1B00,
		0x2030,
		0x0FB0,
		0x0BD0,
		0x08F0,
		0x08D0,
		0x06D0,
		0x06D0,
		0x07D0,
		0x07D0,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
	},
};

#ifdef CONFIG_OF
static  struct gpio *
max17042_get_gpio_list(struct device *dev, int *num_gpio_list)
{
	struct device_node *np = dev->of_node;
	struct gpio *gpio_list;
	int i, num_gpios, gpio_list_size;
	enum of_gpio_flags flags;

	if (!np)
		return NULL;

	num_gpios = of_gpio_count(np);
	if (num_gpios <= 0)
		return NULL;

	gpio_list_size = sizeof(struct gpio) * num_gpios;
	gpio_list = devm_kzalloc(dev, gpio_list_size, GFP_KERNEL);

	if (!gpio_list)
		return NULL;

	*num_gpio_list = num_gpios;
	for (i = 0; i < num_gpios; i++) {
		gpio_list[i].gpio = of_get_gpio_flags(np, i, &flags);
		gpio_list[i].flags = flags;
		of_property_read_string_index(np, "gpio-names", i,
					      &gpio_list[i].label);
	}

	return gpio_list;
}

static struct max17042_reg_data *
max17042_get_init_data(struct device *dev, int *num_init_data)
{
	struct device_node *np = dev->of_node;
	const __be32 *property;
	static struct max17042_reg_data *init_data;
	int i, lenp, num_cells, init_data_size;

	if (!np)
		return NULL;

	property = of_get_property(np, INIT_DATA_PROPERTY, &lenp);

	if (!property || lenp <= 0)
		return NULL;

	/*
	 * Check data validity and whether number of cells is even
	 */
	if (lenp % sizeof(*property)) {
		dev_err(dev, "%s has invalid data\n", INIT_DATA_PROPERTY);
		return NULL;
	}

	num_cells = lenp / sizeof(*property);
	if (num_cells % 2) {
		dev_err(dev, "%s must have even number of cells\n",
			INIT_DATA_PROPERTY);
		return NULL;
	}

	*num_init_data = num_cells / 2;
	init_data_size = sizeof(struct max17042_reg_data) * (num_cells / 2);
	init_data = (struct max17042_reg_data *)
		    devm_kzalloc(dev, init_data_size, GFP_KERNEL);

	if (init_data) {
		for (i = 0; i < num_cells / 2; i++) {
			init_data[i].addr = be32_to_cpu(property[2 * i]);
			init_data[i].data = be32_to_cpu(property[2 * i + 1]);
		}
	}

	return init_data;
}

static int max17042_get_cell_char_tbl(struct device *dev,
				      struct device_node *np,
				      struct max17042_config_data *config_data)
{
	const __be16 *property;
	int i, lenp;

	property = of_get_property(np, CELL_CHAR_TBL_PROPERTY, &lenp);
	if (!property)
		return -ENODEV;

	if (lenp != sizeof(*property) * MAX17042_CHARACTERIZATION_DATA_SIZE) {
		dev_err(dev, "%s must have %d cells\n", CELL_CHAR_TBL_PROPERTY,
			MAX17042_CHARACTERIZATION_DATA_SIZE);
		return -EINVAL;
	}

	for (i = 0; i < MAX17042_CHARACTERIZATION_DATA_SIZE; i++)
		config_data->cell_char_tbl[i] = be16_to_cpu(property[i]);

	return 0;
}

static int max17042_cfg_rqrd_prop(struct device *dev,
				  struct device_node *np,
				  struct max17042_config_data *config_data)
{
	if (of_property_read_u16(np, CONFIG_PROPERTY,
				 &config_data->config))
		return -EINVAL;
	if (of_property_read_u16(np, FILTER_CFG_PROPERTY,
				 &config_data->filter_cfg))
		return -EINVAL;
	if (of_property_read_u16(np, RELAX_CFG_PROPERTY,
				 &config_data->relax_cfg))
		return -EINVAL;
	if (of_property_read_u16(np, LEARN_CFG_PROPERTY,
				 &config_data->learn_cfg))
		return -EINVAL;
	if (of_property_read_u16(np, FULL_SOC_THRESH_PROPERTY,
				 &config_data->full_soc_thresh))
		return -EINVAL;
	if (of_property_read_u16(np, RCOMP0_PROPERTY,
				 &config_data->rcomp0))
		return -EINVAL;
	if (of_property_read_u16(np, TCOMPC0_PROPERTY,
				 &config_data->tcompc0))
		return -EINVAL;
	if (of_property_read_u16(np, ICHGT_TERM_PROPERTY,
				 &config_data->ichgt_term))
		return -EINVAL;
	if (of_property_read_u16(np, QRTBL00_PROPERTY,
				 &config_data->qrtbl00))
		return -EINVAL;
	if (of_property_read_u16(np, QRTBL10_PROPERTY,
				 &config_data->qrtbl10))
		return -EINVAL;
	if (of_property_read_u16(np, QRTBL20_PROPERTY,
				 &config_data->qrtbl20))
		return -EINVAL;
	if (of_property_read_u16(np, QRTBL30_PROPERTY,
				 &config_data->qrtbl30))
		return -EINVAL;
	if (of_property_read_u16(np, FULLCAP_PROPERTY,
				 &config_data->fullcap))
		return -EINVAL;
	if (of_property_read_u16(np, DESIGN_CAP_PROPERTY,
				 &config_data->design_cap))
		return -EINVAL;
	if (of_property_read_u16(np, FULLCAPNOM_PROPERTY,
				 &config_data->fullcapnom))
		return -EINVAL;
	if (of_property_read_u16(np, LAVG_EMPTY_PROPERTY,
				 &config_data->lavg_empty))
		return -EINVAL;

	return max17042_get_cell_char_tbl(dev, np, config_data);
}

static void max17042_cfg_optnl_prop(struct device_node *np,
				    struct max17042_config_data *config_data)
{
	of_property_read_u16(np, TGAIN_PROPERTY, &config_data->tgain);
	of_property_read_u16(np, TOFF_PROPERTY, &config_data->toff);
	of_property_read_u16(np, CGAIN_PROPERTY, &config_data->cgain);
	of_property_read_u16(np, COFF_PROPERTY, &config_data->coff);
	of_property_read_u16(np, MISC_CFG_PROPERTY, &config_data->misc_cfg);
	of_property_read_u16(np, REVISION, &config_data->revision);
	config_data->revision &= MAX17050_CFG_REV_MASK;
}

static const char *max17042_get_mmi_battid(void)
{
	struct device_node *np = of_find_node_by_path("/chosen");
	const char *battsn_buf;
	int retval;

	battsn_buf = NULL;

	if (np)
		retval = of_property_read_string(np, "mmi,battid",
						 &battsn_buf);
	else
		return NULL;

	if ((retval == -EINVAL) || !battsn_buf) {
		pr_err("Battsn unused\n");
		of_node_put(np);
		return NULL;

	} else
		pr_err("Battsn = %s\n", battsn_buf);

	of_node_put(np);

	return battsn_buf;
}

static struct max17042_config_data *
max17042_get_config_data(struct device *dev)
{
	struct max17042_config_data *config_data;
	struct device_node *np = dev->of_node;
	struct device_node *node, *df_node, *sn_node;
	const char *sn_buf, *df_sn, *dev_sn;
	int rc;
	bool df_read_sn;

	if (!np)
		return NULL;

	df_read_sn = of_property_read_bool(np, "maxim,df-read-battsn");

	np = of_get_child_by_name(np, CONFIG_NODE);
	if (!np)
		return NULL;

	dev_sn = NULL;
	df_sn = NULL;
	sn_buf = NULL;
	df_node = NULL;
	sn_node = NULL;

	dev_sn = max17042_get_mmi_battid();

	rc = of_property_read_string(np, "df-serialnum",
				     &df_sn);
	if (rc)
		dev_warn(dev, "No Default Serial Number defined");
	else if (df_sn)
		dev_warn(dev, "Default Serial Number %s", df_sn);

	for_each_child_of_node(np, node) {
		rc = of_property_read_string(node, "serialnum",
					     &sn_buf);
		if (!rc && sn_buf) {
			if (dev_sn)
				if (strnstr(dev_sn, sn_buf, 32))
					sn_node = node;
			if (df_sn)
				if (strnstr(df_sn, sn_buf, 32))
					df_node = node;
		}
	}

	if (sn_node) {
		np = sn_node;
		df_node = NULL;
		dev_warn(dev, "Battery Match Found using %s",
			 sn_node->name);
	} else if (df_node) {
		np = df_node;
		sn_node = NULL;
		dev_warn(dev, "Battery Match Found using default %s",
			 df_node->name);
	} else {
		dev_warn(dev, "No Battery Match Found!");
		return NULL;
	}

	config_data = devm_kzalloc(dev, sizeof(*config_data), GFP_KERNEL);
	if (!config_data)
		return NULL;

	if (max17042_cfg_rqrd_prop(dev, np, config_data)) {
		devm_kfree(dev, config_data);
		return NULL;
	}

	max17042_cfg_optnl_prop(np, config_data);

	if ((np == df_node) && !df_read_sn)
		config_data->revision = 1;

	dev_warn(dev, "Config Revision = %d", config_data->revision);

	config_data->cur_sense_val = 10;
	config_data->valrt_thresh = 0xFF97;
	config_data->talrt_thresh = 0x7F80;
	config_data->soc_alrt_thresh = 0xFF00;
	config_data->config = 0x0214;
	config_data->shdntimer = 0xE000;
	config_data->cell_technology = POWER_SUPPLY_TECHNOLOGY_LION;
	config_data->vempty = 0x964C;
	config_data->temp_nom = 0x1400;

	return config_data;
}

static struct max17042_temp_conv *
max17042_get_conv_table(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct max17042_temp_conv *temp_conv;
	const __be16 *property;
	int i, lenp, num;
	u16 temp;
	s16 start;

	if (!np)
		return NULL;

	np = of_get_child_by_name(np, TEMP_CONV_NODE);
	if (!np)
		return NULL;

	property = of_get_property(np, RESULT_PROPERTY, &lenp);
	if (!property || lenp <= 0) {
		dev_err(dev, "%s must have %s property\n", TEMP_CONV_NODE,
			RESULT_PROPERTY);
		return NULL;
	}

	if (of_property_read_u16(np, START_PROPERTY, &temp)) {
		dev_err(dev, "%s must have %s property\n", TEMP_CONV_NODE,
			START_PROPERTY);
		return NULL;
	}

	start = (s16) temp;

	temp_conv = devm_kzalloc(dev, sizeof(*temp_conv), GFP_KERNEL);
	if (!temp_conv)
		return NULL;

	num = lenp / sizeof(*property);
	temp_conv->result = devm_kzalloc(dev, sizeof(s16) * num, GFP_KERNEL);
	if (!temp_conv->result) {
		devm_kfree(dev, temp_conv);
		return NULL;
	}

	temp_conv->start = start;
	temp_conv->num_result = num;

	for (i = 0; i < num; i++) {
		temp = be16_to_cpu(property[i]);
		temp_conv->result[i] = (s16) temp;
	}

	return temp_conv;
}


static struct max17042_platform_data *
max17042_get_pdata(struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 prop;
	struct max17042_platform_data *pdata;
	int rc;

	if (!np)
		return dev->platform_data;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

	pdata->init_data = max17042_get_init_data(dev, &pdata->num_init_data);
	pdata->gpio_list = max17042_get_gpio_list(dev, &pdata->num_gpio_list);

	/*
	 * Require current sense resistor value to be specified for
	 * current-sense functionality to be enabled at all.
	 */
	if (of_property_read_u32(np, "maxim,rsns-microohm", &prop) == 0) {
		pdata->r_sns = prop;
		pdata->enable_current_sense = true;
	}

	pdata->enable_por_init =
		of_property_read_bool(np, "maxim,enable_por_init");

	pdata->batt_undervoltage_zero_soc =
		of_property_read_bool(np, "maxim,batt_undervoltage_zero_soc");

	rc = of_property_read_string(np, "maxim,batt-psy-name",
				     &pdata->batt_psy_name);
	if (rc)
		pdata->batt_psy_name = NULL;

	if (pdata->batt_psy_name)
		pr_warn("BATT SUPPLY NAME = %s\n", pdata->batt_psy_name);

	of_property_read_u32(np, "maxim,warm-temp-c", &pdata->warm_temp_c);
	of_property_read_u32(np, "maxim,hot-temp-c", &pdata->hot_temp_c);
	of_property_read_u32(np, "maxim,cool-temp-c", &pdata->cool_temp_c);
	of_property_read_u32(np, "maxim,cold-temp-c", &pdata->cold_temp_c);
	rc = of_property_read_u32(np, "maxim,hotspot-thrs-c",
				  &pdata->hotspot_thrs_c);
	if (rc) {
		pdata->hotspot_thrs_c = 50;
		pr_debug("DT hotspot threshold not available using %d\n",
			 pdata->hotspot_thrs_c);
	}

	pdata->tcnv = max17042_get_conv_table(dev);

	pdata->config_data = max17042_get_config_data(dev);

	if (!pdata->config_data) {
		dev_warn(dev, "config data is missing\n");

		pdata->config_data = &eg30_lg_config;
		pdata->enable_por_init = true;
	}

	/* Read Valrt threshold override */
	if (of_property_read_u32(np, "maxim,valrt-threshold", &prop) == 0)
		pdata->config_data->valrt_thresh = (u16)prop;

	return pdata;
}
#else
static struct max17042_platform_data *
max17042_get_pdata(struct device *dev)
{
	return dev->platform_data;
}
#endif

#ifdef CONFIG_BATTERY_MAX17042_DEBUGFS
static int max17042_debugfs_read_addr(void *data, u64 *val)
{
	struct max17042_chip *chip = (struct max17042_chip *)data;
	*val = chip->debugfs_addr;
	return 0;
}

static int max17042_debugfs_write_addr(void *data, u64 val)
{
	struct max17042_chip *chip = (struct max17042_chip *)data;
	chip->debugfs_addr = val;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(addr_fops, max17042_debugfs_read_addr,
			max17042_debugfs_write_addr, "0x%02llx\n");

static int max17042_debugfs_read_data(void *data, u64 *val)
{
	struct max17042_chip *chip = (struct max17042_chip *)data;
	struct regmap *map = chip->regmap;
	int ret = regmap_read(map, chip->debugfs_addr, val);

	if (ret < 0)
		return ret;

	return 0;
}

static int max17042_debugfs_write_data(void *data, u64 val)
{
	struct max17042_chip *chip = (struct max17042_chip *)data;
	struct regmap *map = chip->regmap;
	return regmap_write(map, chip->debugfs_addr, val);
}
DEFINE_SIMPLE_ATTRIBUTE(data_fops, max17042_debugfs_read_data,
			max17042_debugfs_write_data, "0x%02llx\n");

static int max17042_debugfs_create(struct max17042_chip *chip)
{
	chip->debugfs_root = debugfs_create_dir(dev_name(&chip->client->dev),
						NULL);
	if (!chip->debugfs_root)
		return -ENOMEM;

	if (!debugfs_create_file("addr", S_IRUGO | S_IWUSR, chip->debugfs_root,
				 chip, &addr_fops))
		goto err_debugfs;

	if (!debugfs_create_file("data", S_IRUGO | S_IWUSR, chip->debugfs_root,
				 chip, &data_fops))
		goto err_debugfs;

	return 0;

err_debugfs:
	debugfs_remove_recursive(chip->debugfs_root);
	chip->debugfs_root = NULL;
	return -ENOMEM;
}
#endif

#define MAX_TAPER_RETRY 5
static void iterm_work(struct work_struct *work)
{
	int ret = 0;
	int repsoc;
	int repcap, fullcap;
	int repcap_p, i;
	int socvf, fullsocthr;
	int curr_avg, curr_inst, iterm_max, iterm_min;
	int resch_time = 60000;
	int cfd_max;
	int val;
	int taper_hit = 0;
	struct max17042_chip *chip =
		container_of(work, struct max17042_chip,
				iterm_work.work);
	struct regmap *map = chip->regmap;

	max17042_stay_awake(&chip->max17042_wake_source);
	if (!chip->init_complete)
		goto iterm_fail;

	/* check to see if Already Full */
	ret = regmap_read(map, MAX17042_RepSOC, &val);
	if (ret < 0)
		goto iterm_fail;

	repsoc = (val >> 8) & 0xFF;
	if ((val & 0xFF) != 0)
		repsoc++; /* Round up */

	dev_dbg(&chip->client->dev, "ITERM RepSOC %d!\n", repsoc);

	cfd_max =
		(chip->charge_full_des + ((chip->charge_full_des * 3) / 100));

	if (repsoc >= 100) {
		ret = regmap_read(map, MAX17042_FullCAP, &val);
		if (ret < 0)
			goto iterm_fail;

		fullcap = val & 0xFFFF;

		/* Check that FullCap */
		/* is not less then half of Design Capacity */
		if ((fullcap * 1000) < chip->charge_full_des) {
			dev_warn(&chip->client->dev,
				 "Error fullcap too small! Forcing POR!\n");
			dev_warn(&chip->client->dev,
				 "FullCap %d mAhr\n",
				 fullcap / 2);
			regmap_write(map, MAX17042_VFSOC0Enable,
				     MAX17050_FORCE_POR);
			msleep(MAX17050_POR_WAIT_MS);
			schedule_delayed_work(&chip->thread_work,
				msecs_to_jiffies(0));
			goto iterm_fail;
		}

		/* Catch Increasing FullCap*/
		if (((fullcap * 1000) / 2) > cfd_max) {
			dev_warn(&chip->client->dev,
				 "Error fullcap too big! %d mAhr\n",
				 fullcap / 2);
			fullcap = (cfd_max * 2) / 1000;
			ret = regmap_write(map,
					   MAX17042_RepCap, fullcap);
			if (ret < 0)
				dev_err(&chip->client->dev,
					"Can't update Rep Cap!\n");
			ret = regmap_write(map,
					   MAX17042_FullCAP, fullcap);
			if (ret < 0)
				dev_err(&chip->client->dev,
					"Can't update Full Cap!\n");
		}
	}

	ret = regmap_read(map, MAX17042_Current, &val);
	if (ret < 0)
		goto iterm_fail;

	/* check for discharging */
	if (val & 0x8000) {
		dev_dbg(&chip->client->dev, "ITERM Curr Inst Discharge!\n");
		goto iterm_fail;
	}

	if (chip->taper_reached) {
		taper_hit = 1;
		goto iterm_fail;
	}

	curr_inst = ret & 0xFFFF;
	dev_dbg(&chip->client->dev, "ITERM Curr Inst %d uA!\n",
		 (curr_inst * 1562500) / chip->pdata->r_sns);

	ret = regmap_read(map, MAX17042_AvgCurrent, &val);
	if (ret < 0)
		goto iterm_fail;

	/* check for discharging */
	if (val & 0x8000) {
		dev_dbg(&chip->client->dev, "ITERM Curr Avg Discharge!\n");
		goto iterm_fail;
	}

	/* No Discharge so Monitor Faster */
	resch_time = 10000;

	curr_avg = ret & 0xFFFF;
	dev_dbg(&chip->client->dev, "ITERM Curr Avg %d uA!\n",
		 (curr_inst * 1562500) / chip->pdata->r_sns);

	ret = regmap_read(map, MAX17042_ICHGTerm, &val);
	if (ret < 0)
		goto iterm_fail;

	iterm_min = val & 0xFFFF;
	iterm_min = (iterm_min >> 2) & 0xFFFF;

	iterm_max = (ret & 0xFFFF) + iterm_min;

	iterm_min = (iterm_min >> 1) & 0xFFFF;

	if ((curr_inst <= iterm_min) || (curr_inst >= iterm_max) ||
	    (curr_avg <= iterm_min) || (curr_avg >= iterm_max))
		goto iterm_fail;

	ret = regmap_read(map, MAX17042_RepCap, &val);
	if (ret < 0)
		goto iterm_fail;

	repcap = val & 0xFFFF;
	dev_dbg(&chip->client->dev, "ITERM RepCap %d uAhr!\n",
		 (repcap * 1000) / 2);

	ret = regmap_read(map, MAX17042_FullCAP, &val);
	if (ret < 0)
		goto iterm_fail;

	fullcap = val & 0xFFFF;
	dev_dbg(&chip->client->dev, "ITERM FullCap %d uAhr!\n",
		 (fullcap * 1000) / 2);

	if (repcap >= fullcap) {
		taper_hit = 1;
		goto iterm_fail;
	}

	ret = regmap_read(map, MAX17042_VFSOC, &val);
	if (ret < 0)
		goto iterm_fail;

	socvf = (val >> 8) & 0xFF;

	ret = regmap_read(map, MAX17047_FullSOCThr, &val);
	if (ret < 0)
		goto iterm_fail;

	fullsocthr = (val >> 8) & 0xFF;

	dev_dbg(&chip->client->dev, "ITERM VFSOC %d!\n", socvf);
	dev_dbg(&chip->client->dev, "ITERM FullSOCThr %d!\n", fullsocthr);
	if (socvf <= fullsocthr)
		goto iterm_fail;

	/* Check that RepCap or FullCap */
	/* is not less then half of Design Capacity */
	if (((repcap * 1000) < chip->charge_full_des) ||
	    ((fullcap * 1000) < chip->charge_full_des)) {
		dev_warn(&chip->client->dev,
			 "Error fullcap too small! Forcing POR!\n");
		dev_warn(&chip->client->dev,
			 "FullCap %d mAhr\n",
			 fullcap / 2);
		regmap_write(map, MAX17042_VFSOC0Enable,
				   MAX17050_FORCE_POR);
		msleep(MAX17050_POR_WAIT_MS);
		schedule_delayed_work(&chip->thread_work,
			msecs_to_jiffies(0));

		goto iterm_fail;
	}

	/* Catch Increasing FullCap*/
	if (((fullcap * 1000) / 2) > cfd_max) {
		dev_warn(&chip->client->dev, "Error fullcap too big!\n");
		dev_warn(&chip->client->dev,
			 "RepCap %d mAhr FullCap %d mAhr\n",
			 repcap / 2, fullcap / 2);
		repcap = (cfd_max * 2) / 1000;
		fullcap = repcap;
		ret = regmap_write(map, MAX17042_RepSOC, repcap);
		if (ret < 0)
			dev_err(&chip->client->dev,
				"Can't update Rep Cap!\n");
	}

	chip->last_fullcap = repcap;

	dev_warn(&chip->client->dev, "Taper Reached!\n");
	dev_warn(&chip->client->dev, "RepCap %d mAhr FullCap %d mAhr\n",
		 repcap / 2, fullcap / 2);

	repcap_p = repcap;
	taper_hit = 1;

	ret = regmap_write(map, MAX17042_FullCAP, repcap);
	if (ret < 0)
		dev_err(&chip->client->dev, "Can't update Full Cap!\n");

	for (i = 0; i < MAX_TAPER_RETRY; i++) {
		ret = regmap_read(map, MAX17042_RepCap, &val);
		if (ret < 0)
			goto iterm_fail;

		repcap = val & 0xFFFF;

		ret = regmap_read(map, MAX17042_FullCAP, &val);
		if (ret < 0)
			goto iterm_fail;

		fullcap = val & 0xFFFF;
		dev_warn(&chip->client->dev,
			 "Checking RepCap %d mAhr FullCap %d mAhr\n",
			 repcap / 2, fullcap / 2);
		if ((repcap > (repcap_p + 2)) || (repcap < (repcap_p - 2)) ||
		    (fullcap != repcap_p)) {
			dev_warn(&chip->client->dev,
				 "ITERM values don't match rewrite!\n");
			dev_warn(&chip->client->dev,
				 "RepCap %d mAhr FullCap %d mAhr\n",
				 repcap / 2, fullcap / 2);
			ret = regmap_write(map,
					   MAX17042_FullCAP, repcap_p);
			if (ret < 0)
				dev_err(&chip->client->dev,
					"Can't update Full Cap!\n");
			ret = regmap_write(map,
					   MAX17042_RepCap, repcap_p);
			if (ret < 0)
				dev_err(&chip->client->dev,
					"Can't update Rep Cap!\n");
		} else
			break;
	}


iterm_fail:
	dev_dbg(&chip->client->dev,
		"SW ITERM Done!\n");
	chip->taper_reached = taper_hit;
	schedule_delayed_work(&chip->iterm_work,
			      msecs_to_jiffies(resch_time));
	max17042_relax(&chip->max17042_wake_source);
	return;
}

static bool max17042_mmi_factory(void)
{
	struct device_node *np = of_find_node_by_path("/chosen");
	u32 fact_cable = 0;

	if (np)
		of_property_read_u32(np, "mmi,factory-cable", &fact_cable);

	of_node_put(np);
	return !!fact_cable ? true : false;
}

static struct regmap_config max17042_regmap_config = {
	.reg_bits = 8,
	.val_bits = 16,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
};

static int max17042_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct max17042_chip *chip;
	struct max17042_config_data *config;
	int ret;
	int i;
	u32 val, val2;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA))
		return -EIO;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->regmap = devm_regmap_init_i2c(client, &max17042_regmap_config);
	if (IS_ERR(chip->regmap)) {
		dev_err(&client->dev, "Failed to initialize regmap\n");
		return -EINVAL;
	}

	chip->pdata = max17042_get_pdata(&client->dev);
	if (!chip->pdata) {
		dev_err(&client->dev, "no platform data provided\n");
		return -EINVAL;
	}

	chip->last_fullcap = -EINVAL;
	chip->factory_mode = false;
	chip->charge_full_des =
		(chip->pdata->config_data->design_cap / 2) * 1000;

	i2c_set_clientdata(client, chip);

	regmap_read(chip->regmap, MAX17042_DevName, &val);
	if (val == MAX17042_IC_VERSION) {
		dev_dbg(&client->dev, "chip type max17042 detected\n");
		chip->chip_type = MAX17042;
	} else if (val == MAX17047_IC_VERSION) {
		dev_dbg(&client->dev, "chip type max17047/50 detected\n");
		chip->chip_type = MAX17047;
	} else {
		dev_err(&client->dev, "device version mismatch: %x\n", val);
		return -EIO;
	}

	chip->battery.name		= "max170xx_battery";
	chip->battery.type		= POWER_SUPPLY_TYPE_BMS;
	chip->battery.get_property	= max17042_get_property;
	chip->battery.set_property	= max17042_set_property;
	chip->battery.properties	= max17042_battery_props;
	chip->battery.num_properties	= ARRAY_SIZE(max17042_battery_props);
	chip->battery.property_is_writeable = max17042_is_writeable;

	/* When current is not measured,
	 * CURRENT_NOW and CURRENT_AVG properties should be invisible. */
	if (!chip->pdata->enable_current_sense)
		chip->battery.num_properties -= 2;

	if (chip->pdata->r_sns == 0)
		chip->pdata->r_sns = MAX17042_DEFAULT_SNS_RESISTOR;

	if (chip->pdata->init_data)
		for (i = 0; i < chip->pdata->num_init_data; i++)
			regmap_write(chip->regmap,
					chip->pdata->init_data[i].addr,
					chip->pdata->init_data[i].data);

	if (!chip->pdata->enable_current_sense) {
		regmap_write(chip->regmap, MAX17042_CGAIN, 0x0000);
		regmap_write(chip->regmap, MAX17042_MiscCFG, 0x0003);
		regmap_write(chip->regmap, MAX17042_LearnCFG, 0x0007);
	}

	ret = power_supply_register(&client->dev, &chip->battery);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		return ret;
	}

	chip->factory_mode = max17042_mmi_factory();
	if (chip->factory_mode)
		dev_info(&client->dev, "max17042: Factory Mode\n");

	chip->temp_state = POWER_SUPPLY_HEALTH_UNKNOWN;
	chip->taper_reached = 0;
	mutex_init(&chip->check_temp_lock);
	max17042_check_temp(chip);
	INIT_WORK(&chip->check_temp_work, max17042_check_temp_worker);

	ret = gpio_request_array(chip->pdata->gpio_list,
				 chip->pdata->num_gpio_list);
	if (ret) {
		dev_err(&client->dev, "cannot request GPIOs\n");
		return ret;
	}

	for (i = 0; i < chip->pdata->num_gpio_list; i++) {
		if (chip->pdata->gpio_list[i].flags & GPIOF_EXPORT) {
			ret = gpio_export_link(&client->dev,
					       chip->pdata->gpio_list[i].label,
					       chip->pdata->gpio_list[i].gpio);
			if (ret) {
				dev_err(&client->dev,
					"Failed to link GPIO %s: %d\n",
					chip->pdata->gpio_list[i].label,
					chip->pdata->gpio_list[i].gpio);
				return ret;
			}
		}
	}

	/* Override Voltage Alert Threshold */
	max17042_override_por(chip->regmap, MAX17042_VALRT_Th,
				chip->pdata->config_data->valrt_thresh);

	if (client->irq) {
		ret = request_threaded_irq(client->irq, NULL,
					max17042_thread_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					chip->battery.name, chip);
		if (!ret) {
			regmap_read(chip->regmap, MAX17042_CONFIG, &val);
			val |= CONFIG_ALRT_BIT_ENBL;
			regmap_write(chip->regmap, MAX17042_CONFIG, val);
			max17042_set_soc_threshold(chip, 1);
		} else {
			client->irq = 0;
			dev_err(&client->dev, "%s(): cannot get IRQ\n",
				__func__);
		}
	}

	regmap_read(chip->regmap, MAX17042_STATUS, &val);
	regmap_read(chip->regmap, MAX17050_CFG_REV_REG, &val2);
	val2 &= MAX17050_CFG_REV_MASK;

	INIT_WORK(&chip->work, max17042_init_worker);

	if (val & STATUS_POR_BIT) {
		dev_warn(&client->dev, "POR Detected, Loading Config\n");
		schedule_work(&chip->work);
	} else if (val2 != chip->pdata->config_data->revision) {
		dev_warn(&client->dev, "Revision Change, Loading Config\n");
		schedule_work(&chip->work);
	} else {
		config = chip->pdata->config_data;
		if (chip->chip_type == MAX17047)
			regmap_write(chip->regmap, MAX17047_FullSOCThr,
				     config->full_soc_thresh);
		chip->init_complete = 1;
	}

#ifdef CONFIG_BATTERY_MAX17042_DEBUGFS
	ret = max17042_debugfs_create(chip);
	if (ret) {
		dev_err(&client->dev, "cannot create debugfs\n");
		return ret;
	}
#endif
	wakeup_source_init(&chip->max17042_wake_source.source,
			   "max17042_wake");
	INIT_DELAYED_WORK(&chip->iterm_work,
			  iterm_work);
	INIT_DELAYED_WORK(&chip->thread_work, max17042_thread_worker);

	schedule_delayed_work(&chip->iterm_work,
			      msecs_to_jiffies(10000));

	return 0;
}

static int max17042_remove(struct i2c_client *client)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);

	cancel_work_sync(&chip->check_temp_work);
	mutex_destroy(&chip->check_temp_lock);

#ifdef CONFIG_BATTERY_MAX17042_DEBUGFS
	debugfs_remove_recursive(chip->debugfs_root);
#endif

	if (client->irq)
		free_irq(client->irq, chip);
	gpio_free_array(chip->pdata->gpio_list, chip->pdata->num_gpio_list);
	power_supply_unregister(&chip->battery);
	wakeup_source_trash(&chip->max17042_wake_source.source);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int max17042_suspend(struct device *dev)
{
	struct max17042_chip *chip = dev_get_drvdata(dev);

	/*
	 * disable the irq and enable irq_wake
	 * capability to the interrupt line.
	 */
	if (chip->client->irq) {
		disable_irq(chip->client->irq);
		enable_irq_wake(chip->client->irq);
	}

	return 0;
}

static int max17042_resume(struct device *dev)
{
	struct max17042_chip *chip = dev_get_drvdata(dev);

	if (chip->client->irq) {
		disable_irq_wake(chip->client->irq);
		enable_irq(chip->client->irq);
		/* re-program the SOC thresholds to 1% change */
		max17042_set_soc_threshold(chip, 1);
	}

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(max17042_pm_ops, max17042_suspend,
			max17042_resume);

#ifdef CONFIG_OF
static const struct of_device_id max17042_dt_match[] = {
	{ .compatible = "maxim,max17042" },
	{ .compatible = "maxim,max17047" },
	{ .compatible = "maxim,max17050" },
	{ },
};
MODULE_DEVICE_TABLE(of, max17042_dt_match);
#endif

static const struct i2c_device_id max17042_id[] = {
	{ "max17042", 0 },
	{ "max17047", 1 },
	{ "max17050", 2 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17042_id);

static struct i2c_driver max17042_i2c_driver = {
	.driver	= {
		.name	= "max17042",
		.of_match_table = of_match_ptr(max17042_dt_match),
		.pm	= &max17042_pm_ops,
	},
	.probe		= max17042_probe,
	.remove		= max17042_remove,
	.id_table	= max17042_id,
};
module_i2c_driver(max17042_i2c_driver);

MODULE_AUTHOR("MyungJoo Ham <myungjoo.ham@samsung.com>");
MODULE_DESCRIPTION("MAX17042 Fuel Gauge");
MODULE_LICENSE("GPL");
