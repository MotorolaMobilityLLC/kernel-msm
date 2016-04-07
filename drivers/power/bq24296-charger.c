/* Copyright (c) 2016 Motorola Mobility LLC. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/bitops.h>
#include <linux/mutex.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/reboot.h>
#include <linux/qpnp/qpnp-adc.h>

#ifndef GENMASK
/* Mask/Bit helpers */
#define GENMASK(u, l) (((1 << ((u) - (l) + 1)) - 1) << (l))
#endif

#ifndef BIT
#define BIT(x)	(1 << (x))
#endif

/* Register definitions */
#define BQ00_INPUT_SRC_CONT_REG              0X00
#define BQ01_PWR_ON_CONF_REG                 0X01
#define BQ02_CHARGE_CUR_CONT_REG             0X02
#define BQ03_PRE_CHARGE_TERM_CUR_REG         0X03
#define BQ04_CHARGE_VOLT_CONT_REG            0X04
#define BQ05_CHARGE_TERM_TIMER_CONT_REG      0X05
#define BQ06_IR_COMP_THERM_CONT_REG          0X06
#define BQ07_MISC_OPERATION_CONT_REG         0X07
#define BQ08_SYSTEM_STATUS_REG               0X08
#define BQ09_FAULT_REG                       0X09
#define BQ0A_VENDOR_PART_REV_STATUS_REG      0X0A

/* BQ00 Input Source Control Register MASK */
#define EN_HIZ			BIT(7)
#define VINDPM_MASK		(BIT(6)|BIT(5)|BIT(4)|BIT(3))
#define IINLIM_MASK		(BIT(2)|BIT(1)|BIT(0))
#define EN_HIZ_SHIFT		7
#define IINLIM_MASK_SHIFT		0
#define VINDPM_SHIFT			3

/* BQ01 Power-On Configuration  Register MASK */
#define RESET_REG_MASK		BIT(7)
#define WDT_RESET_REG_MASK		BIT(6)
#define CHG_CONFIG_MASK		(BIT(5)|BIT(4))
#define OTG_ENABLE_MASK         BIT(5)
#define CHG_ENABLE_SHIFT  4

/* #define SYSTEM_MIN_VOLTAGE_MASK    0x0E */
#define SYS_MIN_VOL_MASK	(BIT(3)|BIT(2)|BIT(1))
#define BOOST_LIM		BIT(0)

/* BQ02 Charge Current Control Register MASK */
#define ICHG_MASK		(BIT(7)|BIT(6)|BIT(5)|BIT(4)|BIT(3)|BIT(2))
#define FORCE_20PCT_MASK	BIT(0)
#define ICHG_SHIFT	2


/* BQ03 Pre-Charge, Termination Current Control Register MASK */
#define IPRECHG_MASK	(BIT(7)|BIT(6)|BIT(5)|BIT(4))
#define ITERM_MASK		(BIT(3)|BIT(2)|BIT(1)|BIT(0))

/* BQ04 Charge Voltage Control Register MASK */
#define CHG_VOLTAGE_LIMIT_MASK	(BIT(7)|BIT(6)|BIT(5)|BIT(4)|BIT(3)|BIT(2))
#define BATLOWV_MASK	BIT(1)
#define VRECHG_MASK		BIT(0)
#define CHG_VOLTAGE_LIMIT_SHIFT		2

/* BQ05 Charge Termination, Timer-Control Register MASK */
#define EN_CHG_TERM_MASK	BIT(7)
#define I2C_TIMER_MASK          (BIT(5)|BIT(4))
#define EN_CHG_TIMER_MASK	BIT(3)
#define CHG_TIMER_MASK		(BIT(2)|BIT(1))
#define I2C_TIMER_SHIFT 4

/* BQ06 IR Compensation, Thermal Regulation Control Register MASK */
#define IR_COMP_R_MASK		(BIT(7)|BIT(6)|BIT(5))
#define IR_COMP_VCLAMP_MASK	(BIT(4)|BIT(3)|BIT(2))

/* BQ07 Misc-Operation Control Register MASK */
#define BATFET_DISABLE_MASK		BIT(5)

/* BQ08 SYSTEM_STATUS_REG Mask */
#define VBUS_STAT_MASK		(BIT(7)|BIT(6))
#define PRE_CHARGE_MASK		BIT(4)
#define FAST_CHARGE_MASK	BIT(5)
#define CHRG_STAT_MASK		(FAST_CHARGE_MASK|PRE_CHARGE_MASK)
#define DPM_STAT_MASK		BIT(3)
#define PG_STAT_MASK		BIT(2)
#define THERM_STAT_MASK		BIT(1)
#define VSYS_STAT_MASK		BIT(0)

/* BQ09 FAULT_REG Mask */
#define CHRG_FAULT_MASK		(BIT(5)|BIT(4))

#define NULL_CHECK(p, err) \
do { \
	if (!(p)) { \
		pr_err("FATAL (%s)\n", __func__); \
		return err; \
	} \
} while (0)

struct bq24296_reg {
	char *regname;
	u8   regaddress;
};

static struct bq24296_reg bq24296_regs[] = {
	{"REG0",   BQ00_INPUT_SRC_CONT_REG},
	{"REG1",   BQ01_PWR_ON_CONF_REG},
	{"REG2",       BQ02_CHARGE_CUR_CONT_REG},
	{"REG3",    BQ03_PRE_CHARGE_TERM_CUR_REG},
	{"REG4",       BQ04_CHARGE_VOLT_CONT_REG},
	{"REG5", BQ05_CHARGE_TERM_TIMER_CONT_REG},
	{"REG6",     BQ06_IR_COMP_THERM_CONT_REG},
	{"REG7", BQ07_MISC_OPERATION_CONT_REG},
	{"REG8",   BQ08_SYSTEM_STATUS_REG},
	{"REG9",   BQ09_FAULT_REG},
	{"REGA",        BQ0A_VENDOR_PART_REV_STATUS_REG},
};

struct bq_wakeup_source {
	struct wakeup_source	source;
	unsigned long	disabled;
};

struct bq24296_chg {
	struct i2c_client *client;
	struct device *dev;
	struct mutex read_write_lock;

	struct power_supply *usb_psy;
	struct power_supply batt_psy;
	struct power_supply *bms_psy;
	const char *bms_psy_name;

	bool test_mode;
	int test_mode_soc;
	int test_mode_temp;

	bool factory_mode;
	bool factory_present;
	bool chg_enabled;
	bool usb_present;
	bool batt_present;
	bool chg_done_batt_full;
	bool charging;
	bool batt_hot;
	bool batt_cold;
	bool batt_warm;
	bool batt_cool;
	uint8_t	prev_hz_opa;

	struct delayed_work heartbeat_work;
	struct notifier_block notifier;
	struct qpnp_vadc_chip	*vadc_dev;
	struct dentry	    *debug_root;
	u32    peek_poke_address;
	int ext_temp_volt_mv;
	int ext_high_temp;
	int temp_check;
	int bms_check;
	unsigned int voreg_mv;
	unsigned int low_voltage_uv;
	int input_current_limit;
	int vin_limit_mv;
	int sys_vmin_mv;
	int chg_current_ma;
	struct bq_wakeup_source bq_wake_source;
	struct bq_wakeup_source bq_wake_source_charger;
	struct qpnp_adc_tm_chip		*adc_tm_dev;
	struct qpnp_adc_tm_btm_param	vbat_monitor_params;
	bool poll_fast;
	bool shutdown_voltage_tripped;
	atomic_t otg_enabled;
	int ic_info_pn;
	bool factory_configured;
	int max_rate_cap;
	struct delayed_work iusb_work;
	bool demo_mode;
	bool usb_suspended;
	bool chg_init_finish;
	struct gpio *gpio_list;
	int num_gpio_list; /* Number of entries in gpio_list array */
};

static struct bq24296_chg *the_chip;
static void bq24296_set_chrg_path_temp(struct bq24296_chg *chip);
static int bq24296_check_temp_range(struct bq24296_chg *chip);

static void bq_stay_awake(struct bq_wakeup_source *source)
{
	if (__test_and_clear_bit(0, &source->disabled)) {
		__pm_stay_awake(&source->source);
		pr_debug("enabled source %s\n", source->source.name);
	}
}

static void bq_relax(struct bq_wakeup_source *source)
{
	if (!__test_and_set_bit(0, &source->disabled)) {
		__pm_relax(&source->source);
		pr_debug("disabled source %s\n", source->source.name);
	}
}

static bool bq_wake_active(struct bq_wakeup_source *source)
{
	return !source->disabled;
}

static int __bq24296_read(struct bq24296_chg *chip, int reg,
				uint8_t *val)
{
	int ret;

	ret = i2c_smbus_read_byte_data(chip->client, reg);
	if (ret < 0) {
		dev_err(chip->dev,
			"i2c read fail: can't read from %02x: %d\n", reg, ret);
		return ret;
	} else {
		*val = ret;
	}

	return 0;
}

static int __bq24296_write(struct bq24296_chg *chip, int reg,
						uint8_t val)
{
	int ret;

	if (chip->factory_mode)
		return 0;

	ret = i2c_smbus_write_byte_data(chip->client, reg, val);
	if (ret < 0) {
		dev_err(chip->dev,
			"i2c write fail: can't write %02x to %02x: %d\n",
			val, reg, ret);
		return ret;
	}

	dev_dbg(chip->dev, "Writing 0x%02x=0x%02x\n", reg, val);
	return 0;
}

static int bq24296_read(struct bq24296_chg *chip, int reg,
				uint8_t *val)
{
	int rc;

	mutex_lock(&chip->read_write_lock);
	rc = __bq24296_read(chip, reg, val);
	mutex_unlock(&chip->read_write_lock);

	return rc;
}

static int bq24296_charging_reboot(struct notifier_block *, unsigned long,
				void *);

static int bq24296_masked_write(struct bq24296_chg *chip, int reg,
						uint8_t mask, uint8_t val)
{
	int rc;
	uint8_t temp;

	mutex_lock(&chip->read_write_lock);
	rc = __bq24296_read(chip, reg, &temp);
	if (rc < 0) {
		dev_err(chip->dev, "read failed: reg=%03X, rc=%d\n", reg, rc);
		goto out;
	}
	temp &= ~mask;
	temp |= val & mask;
	rc = __bq24296_write(chip, reg, temp);
	if (rc < 0) {
		dev_err(chip->dev,
			"write failed: reg=%03X, rc=%d\n", reg, rc);
	}
out:
	mutex_unlock(&chip->read_write_lock);
	return rc;
}

static int __bq24296_write_fac(struct bq24296_chg *chip, int reg,
				uint8_t val)
{
	int ret;

	ret = i2c_smbus_write_byte_data(chip->client, reg, val);
	if (ret < 0) {
		dev_err(chip->dev,
			"i2c write fail: can't write %02x to %02x: %d\n",
			val, reg, ret);
		return ret;
	}

	pr_info("Writing 0x%02x=0x%02x\n", reg, val);
	return 0;
}

static int bq24296_masked_write_fac(struct bq24296_chg *chip, int reg,
				     uint8_t mask, uint8_t val)
{
	int rc;
	uint8_t temp;

	mutex_lock(&chip->read_write_lock);
	rc = __bq24296_read(chip, reg, &temp);
	if (rc < 0) {
		dev_err(chip->dev, "read failed: reg=%03X, rc=%d\n", reg, rc);
		goto out;
	}
	temp &= ~mask;
	temp |= val & mask;
	rc = __bq24296_write_fac(chip, reg, temp);
	if (rc < 0) {
		dev_err(chip->dev,
			"write failed: reg=%03X, rc=%d\n", reg, rc);
	}
out:
	mutex_unlock(&chip->read_write_lock);
	return rc;
}

#define VIN_LIMIT_MIN_MV  3880
#define VIN_LIMIT_MAX_MV  5080
#define VIN_LIMIT_STEP_MV  80
static int bq24296_set_input_vin_limit(struct bq24296_chg *chip, int mv)
{
	u8 reg_val = 0;
	int set_vin = 0;
	NULL_CHECK(chip, -EINVAL);

	if (mv < VIN_LIMIT_MIN_MV)
		mv = VIN_LIMIT_MIN_MV;
	if (mv > VIN_LIMIT_MAX_MV)
		mv = VIN_LIMIT_MAX_MV;

	reg_val = (mv - VIN_LIMIT_MIN_MV)/VIN_LIMIT_STEP_MV;
	set_vin = reg_val * VIN_LIMIT_STEP_MV + VIN_LIMIT_MIN_MV;
	reg_val = reg_val << 3;

	pr_debug("req_vin = %d set_vin = %d reg_val = 0x%02x\n",
				mv, set_vin, reg_val);

	return bq24296_masked_write(chip, BQ00_INPUT_SRC_CONT_REG,
			VINDPM_MASK, reg_val);
}

struct input_ma_limit_entry {
	int  icl_ma;
	u8  value;
};

static struct input_ma_limit_entry icl_ma_table[] = {
	{100, 0x00},
	{150, 0x01},
	{500, 0x02},
	{900, 0x03},
	{1000, 0x04},
	{1500, 0x05},
	{2000, 0x06},
	{3000, 0x07},
};

static int bq24296_set_input_i_limit(struct bq24296_chg *chip, int ma)
{
	int i;
	u8 temp;
	NULL_CHECK(chip, -EINVAL);

	for (i = ARRAY_SIZE(icl_ma_table) - 1; i > 0; i--) {
		if (ma >= icl_ma_table[i].icl_ma)
			break;
	}
	temp = icl_ma_table[i].value;

	pr_debug("input current limit=%d setting 0x%02x\n", ma, temp);
	return bq24296_masked_write(chip, BQ00_INPUT_SRC_CONT_REG,
			IINLIM_MASK, temp);
}

#define SYSTEM_VMIN_LOW_MV  3000
#define SYSTEM_VMIN_HIGH_MV  3700
#define SYSTEM_VMIN_STEP_MV  100
static int bq24296_set_system_vmin(struct bq24296_chg *chip, int mv)
{
	u8 reg_val = 0;
	int set_vmin = 0;
	NULL_CHECK(chip, -EINVAL);

	if (mv < SYSTEM_VMIN_LOW_MV)
		mv = SYSTEM_VMIN_LOW_MV;
	if (mv > SYSTEM_VMIN_HIGH_MV)
		mv = SYSTEM_VMIN_HIGH_MV;

	reg_val = (mv - SYSTEM_VMIN_LOW_MV)/SYSTEM_VMIN_STEP_MV;
	set_vmin = reg_val * SYSTEM_VMIN_STEP_MV + SYSTEM_VMIN_LOW_MV;
	reg_val = reg_val << 1;

	pr_debug("req_vmin = %d set_vmin = %d reg_val = 0x%02x\n",
				mv, set_vmin, reg_val);

	return bq24296_masked_write(chip, BQ01_PWR_ON_CONF_REG,
			SYS_MIN_VOL_MASK, reg_val);
}



#define IBAT_MAX_MA 3008
#define IBAT_MIN_MA  512
#define IBAT_DIS_MA  (100)
#define IBAT_STEP_MA  64
#define IBAT_DEFAULT  2048
static int bq24296_set_ibat_max(struct bq24296_chg *chip, int ma)
{
	u8 reg_val = 0;
	int set_ibat = 0;

	NULL_CHECK(chip, -EINVAL);

	if (ma < IBAT_MIN_MA)
		ma = IBAT_MIN_MA;

	if (ma > IBAT_MAX_MA)
		ma = IBAT_MAX_MA;

	reg_val = (ma - IBAT_MIN_MA)/IBAT_STEP_MA;
	set_ibat = reg_val * IBAT_STEP_MA + IBAT_MIN_MA;
	reg_val = reg_val << 2;
	pr_debug("req_ibat = %d set_ibat = %d reg_val = 0x%02x\n",
				ma, set_ibat, reg_val);

	return bq24296_masked_write(chip, BQ02_CHARGE_CUR_CONT_REG,
			ICHG_MASK, reg_val);
}

static int bq24296_force_ichg_decrease(struct bq24296_chg *chip, bool enable)
{
	int ret;
	u8 val = (u8)(!!enable);
	NULL_CHECK(chip, -EINVAL);

	pr_debug("enable=%d\n", enable);

	ret = bq24296_masked_write(chip, BQ02_CHARGE_CUR_CONT_REG,
			FORCE_20PCT_MASK, val);
	if (ret) {
		pr_err("failed to set FORCE_20PCT ret=%d\n", ret);
		return ret;
	}

	return 0;
}

#define VBAT_MAX_MV  4400
#define VBAT_MIN_MV  3504
#define VBAT_STEP_MV  16
static int bq24296_set_vbat_max(struct bq24296_chg *chip, int mv)
{
	u8 reg_val = 0;
	int set_vbat = 0;
	NULL_CHECK(chip, -EINVAL);

	if (mv < VBAT_MIN_MV)
		mv = VBAT_MIN_MV;
	if (mv > VBAT_MAX_MV)
		mv = VBAT_MAX_MV;

	reg_val = (mv - VBAT_MIN_MV)/VBAT_STEP_MV;
	set_vbat = reg_val * VBAT_STEP_MV + VBAT_MIN_MV;
	reg_val = reg_val << 2;

	pr_debug("req_vbat = %d set_vbat = %d reg_val = 0x%02x\n",
				mv, set_vbat, reg_val);

	return bq24296_masked_write(chip, BQ04_CHARGE_VOLT_CONT_REG,
			CHG_VOLTAGE_LIMIT_MASK, reg_val);
}

static int bq24296_set_chg_timer(struct bq24296_chg *chip, int time)
{
	int ret;
	NULL_CHECK(chip, -EINVAL);

	pr_debug("time = 0x%02x\n", time);

	time = time << I2C_TIMER_SHIFT;
	ret = bq24296_masked_write(chip, BQ05_CHARGE_TERM_TIMER_CONT_REG,
						I2C_TIMER_MASK, time);
	if (ret) {
		pr_err("failed to set chg safety timer ret=%d\n", ret);
		return ret;
	}

	return 0;
}

static int bq24296_disable_en_term(struct bq24296_chg *chip)
{
	int ret;
	NULL_CHECK(chip, -EINVAL);

	ret = bq24296_masked_write(chip, BQ05_CHARGE_TERM_TIMER_CONT_REG,
						EN_CHG_TERM_MASK, 0);
	if (ret) {
		pr_err("failed to set chg safety timer ret=%d\n", ret);
		return ret;
	}

	return 0;
}

static int bq24296_set_hiz(struct bq24296_chg *chip, bool enable)
{
	int ret;
	NULL_CHECK(chip, -EINVAL);

	pr_debug("enable=%d\n", enable);

	ret = bq24296_masked_write(chip, BQ00_INPUT_SRC_CONT_REG, EN_HIZ,
			enable ? (1 << EN_HIZ_SHIFT) : (0 << EN_HIZ_SHIFT));
	if (ret) {
		pr_err("failed to set chg safety timer ret=%d\n", ret);
		return ret;
	}

	return 0;
}

static int bq24296_wdt_rest(struct bq24296_chg *chip)
{
	int ret;
	NULL_CHECK(chip, -EINVAL);

	pr_debug("in\n");

	ret = bq24296_masked_write(chip, BQ01_PWR_ON_CONF_REG,
				WDT_RESET_REG_MASK, WDT_RESET_REG_MASK);
	if (ret) {
		pr_err("failed to set chg safety timer ret=%d\n", ret);
		return ret;
	}

	return 0;
}

static int configure_for_factory_cable_insertion(struct bq24296_chg *chip)
{
	int rc = 0;

	/*
	 * Disable Charging, Set IBUS to Unlimited, VOREG to 4.2V and
	 * disable T32. This is needed to configure factory mode.
	 */

	/*WDT reset*/
	rc = bq24296_masked_write_fac(chip, BQ01_PWR_ON_CONF_REG,
				   WDT_RESET_REG_MASK, WDT_RESET_REG_MASK);
	if (rc) {
		dev_err(chip->dev, "Factory Mode Failure: wdt\n");
		return rc;
	}
	 /* Disable T32 */
	rc = bq24296_masked_write_fac(chip, BQ05_CHARGE_TERM_TIMER_CONT_REG,
				   I2C_TIMER_MASK, 0 << I2C_TIMER_SHIFT);
	if (rc) {
		dev_err(chip->dev, "Factory Mode Failure: Disable 32s timer\n");
		return rc;
	}

	rc = bq24296_masked_write_fac(chip, BQ01_PWR_ON_CONF_REG,
				CHG_CONFIG_MASK, 0 << CHG_ENABLE_SHIFT);
	if (rc) {
		dev_err(chip->dev, "Factory Mode Failure: Disable charging\n");
		return rc;
	}

	rc = bq24296_masked_write_fac(chip, BQ00_INPUT_SRC_CONT_REG,
				       EN_HIZ, 0 << EN_HIZ_SHIFT);
	if (rc) {
		dev_err(chip->dev, "Factory Mode Failure: Disable HZ mode\n");
		return rc;
	}

	rc = bq24296_masked_write_fac(chip, BQ00_INPUT_SRC_CONT_REG,
			       IINLIM_MASK, 0x07 << IINLIM_MASK_SHIFT);
	if (rc) {
		dev_err(chip->dev, "Factory Mode Failure: Set I/P current\n");
		return rc;
	}

	rc = bq24296_masked_write_fac(chip, BQ04_CHARGE_VOLT_CONT_REG,
		CHG_VOLTAGE_LIMIT_MASK, 0x2b << CHG_VOLTAGE_LIMIT_SHIFT);
	if (rc) {
		dev_err(chip->dev, "Factory Mode Failure: Set Voreg to 4.2\n");
		return rc;
	}

	rc = bq24296_masked_write_fac(chip, BQ00_INPUT_SRC_CONT_REG,
			VINDPM_MASK, 0x07 << VINDPM_SHIFT);
	if (rc) {
		dev_err(chip->dev, "Factory Mode set vindpm for 4.5v\n");
		return rc;
	}

	chip->factory_configured = true;

	return rc;
}

static void iusb_work(struct work_struct *work)
{
	struct bq24296_chg *chip =
		container_of(work, struct bq24296_chg,
					iusb_work.work);

	if (chip)
		chip->max_rate_cap = 0;
}

static int start_charging(struct bq24296_chg *chip)
{
	union power_supply_propval prop = {0,};
	int rc;
	int current_limit = 0;

	if (!chip->chg_enabled) {
		dev_dbg(chip->dev, "%s: charging enable = %d\n",
				 __func__, chip->chg_enabled);
		return 0;
	}

	pr_info("starting to charge...\n");

	if (chip->factory_mode) {
		rc = chip->usb_psy->get_property(chip->usb_psy,
				 POWER_SUPPLY_PROP_TYPE, &prop);
		pr_info("External Power Changed: usb=%d\n",
			prop.intval);
		if ((prop.intval == POWER_SUPPLY_TYPE_USB) ||
		    (prop.intval == POWER_SUPPLY_TYPE_USB_CDP))
			configure_for_factory_cable_insertion(chip);
	}

	if (!bq_wake_active(&chip->bq_wake_source_charger))
				bq_stay_awake(&chip->bq_wake_source_charger);


	rc = bq24296_wdt_rest(chip);
	if (rc) {
		pr_err("start-charge: Couldn't set TMR_RST\n");
		return rc;
	}

	rc = chip->usb_psy->get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_CURRENT_MAX, &prop);
	if (rc < 0) {
		dev_err(chip->dev,
			"could not read USB current_max property, rc=%d\n", rc);
		return rc;
	}

	current_limit = prop.intval / 1000;
	if (current_limit > 500)
		current_limit = chip->input_current_limit;

	rc = bq24296_set_input_i_limit(chip, current_limit);
	if (rc)
		return rc;

	/* Set charger current */
	rc = bq24296_set_ibat_max(chip, chip->chg_current_ma);
	if (rc)
		return rc;

	/* Set OREG  */
	rc = bq24296_set_vbat_max(chip, chip->voreg_mv);
	if (rc)
		return rc;

	/*Disable charger term*/
	rc = bq24296_disable_en_term(chip);
	if (rc)
		return rc;

	/* Disable T32 */
	rc  = bq24296_set_chg_timer(chip, 0);
	if (rc) {
		pr_err("failed to disable T32 timer\n");
		return rc;
	}

	/* Check battery charging temp thresholds */
	chip->temp_check = bq24296_check_temp_range(chip);

	if (chip->temp_check || chip->ext_high_temp)
		bq24296_set_chrg_path_temp(chip);
	else {
		rc =  bq24296_masked_write(chip, BQ01_PWR_ON_CONF_REG,
					CHG_CONFIG_MASK, 1 << CHG_ENABLE_SHIFT);
		if (rc) {
			dev_err(chip->dev,
				"start-charge: Failed to set charg enalbe\n");
			return rc;
		}

		chip->charging = true;
	}

	cancel_delayed_work(&chip->heartbeat_work);
	schedule_delayed_work(&chip->heartbeat_work, msecs_to_jiffies(0));

	return 0;
}

static int stop_charging(struct bq24296_chg *chip)
{
	int rc;

	if (bq_wake_active(&chip->bq_wake_source_charger))
				bq_relax(&chip->bq_wake_source_charger);

	rc = bq24296_masked_write(chip, BQ01_PWR_ON_CONF_REG,
			CHG_CONFIG_MASK, 0 << CHG_ENABLE_SHIFT);
	if (rc) {
		dev_err(chip->dev, "stop-charge: Failed to set disable\n");
		return rc;
	}

	chip->charging = false;
	chip->chg_done_batt_full = false;

	rc = bq24296_set_input_i_limit(chip, 500);
	if (rc)
		dev_err(chip->dev, "Failed to set Minimum input current value\n");

	cancel_delayed_work(&chip->iusb_work);
	schedule_delayed_work(&chip->iusb_work,
			      msecs_to_jiffies(1000));

	cancel_delayed_work(&chip->heartbeat_work);
	schedule_delayed_work(&chip->heartbeat_work, msecs_to_jiffies(0));

	return 0;
}

static int factory_kill_disable;
module_param(factory_kill_disable, int, 0644);

static void bq24296_external_power_changed(struct power_supply *psy)
{
	struct bq24296_chg *chip = container_of(psy,
				struct bq24296_chg, batt_psy);
	union power_supply_propval prop = {0,};
	int rc;

	if (!chip->chg_init_finish) {
		pr_err("chg init not finish\n");
		return;
	}
	if (chip->bms_psy_name && !chip->bms_psy)
		chip->bms_psy = power_supply_get_by_name(
					(char *)chip->bms_psy_name);

	rc = chip->usb_psy->get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_PRESENT, &prop);
	pr_info("External Power Changed: usb=%d\n", prop.intval);

	chip->usb_present = prop.intval;
	if (chip->usb_present)
		start_charging(chip);
	else
		stop_charging(chip);

	if (chip->factory_mode && chip->usb_present
		&& !chip->factory_present)
		chip->factory_present = true;



	if (chip->factory_mode && chip->usb_psy && chip->factory_present
						&& !factory_kill_disable) {
		rc = chip->usb_psy->get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_ONLINE, &prop);
		if (!rc && (prop.intval == 0) && !chip->usb_present &&
					!reboot_in_progress()) {
			pr_err("External Power Changed: UsbOnline=%d\n",
							prop.intval);
			kernel_power_off();
		}
	}
}

static enum power_supply_property bq24296_batt_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	/* Block from Fuel Gauge */
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_HOTSPOT,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_TAPER_REACHED,
	/* Notification from Fuel Gauge */
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_HEALTH,
};

static int bq24296_reset_vbat_monitoring(struct bq24296_chg *chip)
{
	int rc = 0;
	chip->vbat_monitor_params.state_request = ADC_TM_HIGH_LOW_THR_DISABLE;

	rc = qpnp_adc_tm_channel_measure(chip->adc_tm_dev,
					 &chip->vbat_monitor_params);

	if (rc)
		dev_err(chip->dev, "tm disable failed: %d\n", rc);

	return rc;
}

static int bq24296_get_prop_batt_present(struct bq24296_chg *chip)
{
	bool prev_batt_status;
	union power_supply_propval ret = {0, };

	prev_batt_status = chip->batt_present;

	if (!chip->bms_psy && chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);

	if (chip->bms_psy) {
		chip->bms_psy->get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_TEMP, &ret);

		if (ret.intval <= -400)
			chip->batt_present = 0;
		else
			chip->batt_present = 1;
	} else {
		return 0;
	}

	if ((prev_batt_status != chip->batt_present)
		&& (!prev_batt_status))
		bq24296_reset_vbat_monitoring(chip);

	return chip->batt_present;
}

static int bq24296_get_prop_charge_type(struct bq24296_chg *chip)
{
	int rc = 0;
	u8 sys_status;
	int chg_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;

	rc = bq24296_read(chip, BQ08_SYSTEM_STATUS_REG, &sys_status);
	if (rc) {
		pr_err("fail to read BQ08_SYSTEM_STATUS_REG. rc=%d\n", rc);
		chg_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}

	sys_status &= CHRG_STAT_MASK;
	if (sys_status == 0x10)
		chg_type = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
	else if (sys_status == 0x20)
		chg_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
	else if (sys_status == 0x30)
		chg_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
	else
		chg_type = POWER_SUPPLY_CHARGE_TYPE_NONE;

	return chg_type;
}

static int bq24296_get_prop_batt_status(struct bq24296_chg *chip)
{
	int chg_type = bq24296_get_prop_charge_type(chip);

	if (chip->usb_present && chip->chg_done_batt_full)
		return POWER_SUPPLY_STATUS_FULL;

	if (chg_type == POWER_SUPPLY_CHARGE_TYPE_TRICKLE ||
		chg_type == POWER_SUPPLY_CHARGE_TYPE_FAST)
		return POWER_SUPPLY_STATUS_CHARGING;

	if (chip->usb_present)
		return POWER_SUPPLY_STATUS_NOT_CHARGING;

	return POWER_SUPPLY_STATUS_DISCHARGING;
}

#define DEFAULT_BATT_CAPACITY 50
static int bq24296_get_prop_batt_capacity(struct bq24296_chg *chip)
{
	union power_supply_propval ret = {0, };
	int cap = DEFAULT_BATT_CAPACITY;
	int rc;

	if (chip->test_mode)
		return chip->test_mode_soc;

	if (!chip->bms_psy && chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);

	if (chip->shutdown_voltage_tripped && !chip->factory_mode) {
		if ((chip->usb_psy) && chip->usb_present) {
			power_supply_set_present(chip->usb_psy, false);
			power_supply_set_online(chip->usb_psy, false);
			chip->usb_present = false;
		}
		return 0;
	}

	if (chip->bms_psy) {
		rc = chip->bms_psy->get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_CAPACITY, &ret);
		if (rc)
			dev_err(chip->dev, "Couldn't get batt capacity\n");
		else {
			if (!ret.intval	&& !chip->factory_mode) {
				chip->shutdown_voltage_tripped = true;
				if ((chip->usb_psy) && chip->usb_present) {
					power_supply_set_present(chip->usb_psy,
									false);
					power_supply_set_online(chip->usb_psy,
									false);
					chip->usb_present = false;
				}
			}
			cap = ret.intval;
		}
	}

	return cap;
}

#define DEFAULT_BATT_VOLT_MV	4000
static int bq24296_get_prop_batt_voltage_now(struct bq24296_chg *chip,
						int *volt_mv)
{
	int rc = 0;
	union power_supply_propval ret = {0, };

	if (!chip->bms_psy && chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);

	if (chip->bms_psy) {
		rc = chip->bms_psy->get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &ret);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't get batt voltage\n");
			*volt_mv = DEFAULT_BATT_VOLT_MV;
			return rc;
		}

		*volt_mv = ret.intval / 1000;
		return 0;
	}

	return -EINVAL;
}

static bool bq24296_get_prop_taper_reached(struct bq24296_chg *chip)
{
	int rc = 0;
	union power_supply_propval ret = {0, };

	if (!chip->bms_psy && chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);

	if (chip->bms_psy) {
		rc = chip->bms_psy->get_property(chip->bms_psy,
					 POWER_SUPPLY_PROP_TAPER_REACHED, &ret);
		if (rc < 0) {
			dev_err(chip->dev,
				"couldn't read Taper Reached property, rc=%d\n",
				rc);
			return false;
		}

		if (ret.intval)
			return true;
	}

	return false;
}

static void bq24296_notify_vbat(enum qpnp_tm_state state, void *ctx)
{
	struct bq24296_chg *chip = ctx;
	struct qpnp_vadc_result result;
	int batt_volt;
	int rc;
	int adc_volt = 0;

	pr_err("shutdown voltage tripped\n");

	if (chip->vadc_dev) {
		rc = qpnp_vadc_read(chip->vadc_dev, VBAT_SNS, &result);
		adc_volt = (int)(result.physical)/1000;
		pr_info("vbat = %d, raw = 0x%x\n", adc_volt,
							result.adc_code);
	}

	bq24296_get_prop_batt_voltage_now(chip, &batt_volt);
	pr_info("vbat is at %d, state is at %d\n", batt_volt, state);

	if (state == ADC_TM_LOW_STATE)
		if (adc_volt <= (chip->low_voltage_uv/1000)) {
			pr_info("shutdown now\n");
			chip->shutdown_voltage_tripped = 1;
		} else {
			qpnp_adc_tm_channel_measure(chip->adc_tm_dev,
				&chip->vbat_monitor_params);
		}
	else
		qpnp_adc_tm_channel_measure(chip->adc_tm_dev,
				&chip->vbat_monitor_params);

	power_supply_changed(&chip->batt_psy);
}

static int bq24296_setup_vbat_monitoring(struct bq24296_chg *chip)
{
	int rc;

	chip->vbat_monitor_params.low_thr = chip->low_voltage_uv;
	chip->vbat_monitor_params.high_thr = (chip->voreg_mv * 1000) * 2;
	chip->vbat_monitor_params.state_request = ADC_TM_HIGH_LOW_THR_ENABLE;
	chip->vbat_monitor_params.channel = VBAT_SNS;
	chip->vbat_monitor_params.btm_ctx = (void *)chip;

	if (chip->poll_fast) { /* the adc polling rate is higher*/
		chip->vbat_monitor_params.timer_interval =
			ADC_MEAS1_INTERVAL_31P3MS;
	} else /* adc polling rate is default*/ {
		chip->vbat_monitor_params.timer_interval =
			ADC_MEAS1_INTERVAL_1S;
	}

	chip->vbat_monitor_params.threshold_notification =
					&bq24296_notify_vbat;
	pr_debug("set low thr to %d and high to %d\n",
		chip->vbat_monitor_params.low_thr,
			chip->vbat_monitor_params.high_thr);

	if (!bq24296_get_prop_batt_present(chip)) {
		pr_info("no battery inserted,vbat monitoring disabled\n");
		chip->vbat_monitor_params.state_request =
						ADC_TM_HIGH_LOW_THR_DISABLE;
	} else {
		rc = qpnp_adc_tm_channel_measure(chip->adc_tm_dev,
			&chip->vbat_monitor_params);
		if (rc) {
			pr_err("tm setup failed: %d\n", rc);
			return rc;
		}
	}

	pr_debug("vbat monitoring setup complete\n");
	return 0;
}

static int bq24296_temp_charging(struct bq24296_chg *chip, int enable)
{
	int rc = 0;

	pr_debug("%s: charging enable = %d\n", __func__, enable);

	if (enable && (!chip->chg_enabled)) {
		dev_dbg(chip->dev,
			"%s: chg_enabled is %d, not to enable charging\n",
					__func__, chip->chg_enabled);
		return 0;
	}

	rc = bq24296_masked_write(chip, BQ01_PWR_ON_CONF_REG, CHG_CONFIG_MASK,
				enable ? (1 << CHG_ENABLE_SHIFT) : 0);
	if (rc) {
		dev_err(chip->dev, "start-charge: Failed to set CE_N\n");
		return rc;
	}

	chip->charging = enable;

	return 0;
}

static void bq24296_charge_enable(struct bq24296_chg *chip, bool enable)
{
	if (!enable) {
		if (chip->charging)
			stop_charging(chip);
	} else {
		if (chip->usb_present && !(chip->charging))
			start_charging(chip);
	}
}

static void bq24296_usb_suspend(struct bq24296_chg *chip, bool enable)
{
	if ((chip->usb_suspended && enable) ||
	    (!chip->usb_suspended && !enable))
		return;

	bq24296_set_hiz(chip, enable);
	chip->usb_suspended = enable;
}

static void bq24296_set_chrg_path_temp(struct bq24296_chg *chip)
{
	if (chip->demo_mode) {
		bq24296_set_vbat_max(chip, 4000);
		bq24296_temp_charging(chip, 1);
		return;
	} else if ((chip->batt_cool || chip->batt_warm)
		   && !chip->ext_high_temp)
		bq24296_set_vbat_max(chip, chip->ext_temp_volt_mv);
	else
		bq24296_set_vbat_max(chip, chip->voreg_mv);

	if (chip->ext_high_temp ||
		chip->batt_cold ||
		chip->batt_hot ||
		chip->chg_done_batt_full)
		bq24296_temp_charging(chip, 0);
	else
		bq24296_temp_charging(chip, 1);
}

static int bq24296_check_temp_range(struct bq24296_chg *chip)
{
	int batt_volt;
	int batt_soc;
	int ext_high_temp = 0;

	if (bq24296_get_prop_batt_voltage_now(chip, &batt_volt))
		return 0;

	batt_soc = bq24296_get_prop_batt_capacity(chip);

	if ((chip->batt_cool || chip->batt_warm) &&
		(batt_volt > chip->ext_temp_volt_mv))
			ext_high_temp = 1;

	if (chip->ext_high_temp != ext_high_temp) {
		chip->ext_high_temp = ext_high_temp;

		dev_warn(chip->dev, "Ext High = %s\n",
			chip->ext_high_temp ? "High" : "Low");

		return 1;
	}

	return 0;
}

static int bq24296_get_prop_batt_health(struct bq24296_chg *chip)
{
	int batt_health = POWER_SUPPLY_HEALTH_UNKNOWN;

	if (chip->batt_hot)
		batt_health = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (chip->batt_cold)
		batt_health = POWER_SUPPLY_HEALTH_COLD;
	else if (chip->batt_warm)
		batt_health = POWER_SUPPLY_HEALTH_WARM;
	else if (chip->batt_cool)
		batt_health = POWER_SUPPLY_HEALTH_COOL;
	else
		batt_health = POWER_SUPPLY_HEALTH_GOOD;

	return batt_health;
}

static int bq24296_set_prop_batt_health(struct bq24296_chg *chip, int health)
{
	switch (health) {
	case POWER_SUPPLY_HEALTH_OVERHEAT:
		chip->batt_hot = true;
		chip->batt_cold = false;
		chip->batt_warm = false;
		chip->batt_cool = false;
		break;
	case POWER_SUPPLY_HEALTH_COLD:
		chip->batt_cold = true;
		chip->batt_hot = false;
		chip->batt_warm = false;
		chip->batt_cool = false;
		break;
	case POWER_SUPPLY_HEALTH_WARM:
		chip->batt_warm = true;
		chip->batt_hot = false;
		chip->batt_cold = false;
		chip->batt_cool = false;
		break;
	case POWER_SUPPLY_HEALTH_COOL:
		chip->batt_cool = true;
		chip->batt_hot = false;
		chip->batt_cold = false;
		chip->batt_warm = false;
		break;
	case POWER_SUPPLY_HEALTH_GOOD:
	default:
		chip->batt_hot = false;
		chip->batt_cold = false;
		chip->batt_warm = false;
		chip->batt_cool = false;
	}

	return 0;
}

static int bq24296_batt_set_property(struct power_supply *psy,
					enum power_supply_property prop,
					const union power_supply_propval *val)
{
	struct bq24296_chg *chip = container_of(psy,
				struct bq24296_chg, batt_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		chip->chg_enabled = !!val->intval;
		bq24296_charge_enable(chip, chip->chg_enabled);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (chip->test_mode)
			chip->test_mode_soc = val->intval;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		bq_stay_awake(&chip->bq_wake_source);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		bq_stay_awake(&chip->bq_wake_source);
		bq24296_set_prop_batt_health(chip, val->intval);
		bq24296_check_temp_range(chip);
		bq24296_set_chrg_path_temp(chip);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (chip->test_mode)
			chip->test_mode_temp = val->intval;
		break;
	default:
		return -EINVAL;
	}

	cancel_delayed_work(&chip->heartbeat_work);
	schedule_delayed_work(&chip->heartbeat_work,
			      msecs_to_jiffies(0));

	return 0;
}

static int bq24296_batt_is_writeable(struct power_supply *psy,
					enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
	case POWER_SUPPLY_PROP_HEALTH:
	case POWER_SUPPLY_PROP_TEMP:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}

	return rc;
}

static int bq24296_bms_get_property(struct bq24296_chg *chip,
				    enum power_supply_property prop)
{
	union power_supply_propval ret = {0, };

	if (!chip->bms_psy && chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);

	if (chip->bms_psy) {
		chip->bms_psy->get_property(chip->bms_psy,
				prop, &ret);
		return ret.intval;
	}

	return -EINVAL;
}

static int bq24296_batt_get_property(struct power_supply *psy,
					enum power_supply_property prop,
					union power_supply_propval *val)
{
	struct bq24296_chg *chip = container_of(psy,
				struct bq24296_chg, batt_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bq24296_get_prop_batt_status(chip);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = bq24296_get_prop_batt_present(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = chip->chg_enabled;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = bq24296_get_prop_charge_type(chip);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = bq24296_get_prop_batt_capacity(chip);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = bq24296_get_prop_batt_health(chip);

		if (val->intval ==  POWER_SUPPLY_HEALTH_WARM) {
			if (chip->ext_high_temp)
				val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
			else
				val->intval = POWER_SUPPLY_HEALTH_GOOD;
		}

		if (val->intval ==  POWER_SUPPLY_HEALTH_COOL) {
			if (chip->ext_high_temp)
				val->intval = POWER_SUPPLY_HEALTH_COLD;
			else
				val->intval = POWER_SUPPLY_HEALTH_GOOD;
		}
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (chip->test_mode) {
			val->intval = chip->test_mode_temp;
			return 0;
		}
		/* Fall through if test mode temp is not set */
	/* Block from Fuel Gauge */
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
	case POWER_SUPPLY_PROP_TEMP_HOTSPOT:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_CURRENT_AVG:
	case POWER_SUPPLY_PROP_TAPER_REACHED:
		val->intval = bq24296_bms_get_property(chip, prop);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = 0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void bq24296_dump_register(void)
{
	int rc, i;
	u8 reg;

	if (!the_chip) {
		pr_err("chip not valid\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(bq24296_regs); i++) {
		rc = bq24296_read(the_chip, bq24296_regs[i].regaddress, &reg);
		if (!rc)
			pr_info("%s - 0x%02x = 0x%02x\n",
					bq24296_regs[i].regname,
					bq24296_regs[i].regaddress,
					reg);
	}
}

#define DEMO_MODE_MAX_SOC 35
#define DEMO_MODE_HYS_SOC 5
static void heartbeat_work(struct work_struct *work)
{
	struct bq24296_chg *chip =
		container_of(work, struct bq24296_chg,
					heartbeat_work.work);

	bool poll_status = chip->poll_fast;
	int batt_soc = bq24296_get_prop_batt_capacity(chip);
	int batt_health = bq24296_get_prop_batt_health(chip);
	bool taper_reached = bq24296_get_prop_taper_reached(chip);

	dev_dbg(chip->dev, "HB Pound!\n");

	bq24296_dump_register();

	if (batt_soc < 20)
		chip->poll_fast = true;
	else
		chip->poll_fast = false;

	if (poll_status != chip->poll_fast)
		bq24296_setup_vbat_monitoring(chip);

	if ((batt_health == POWER_SUPPLY_HEALTH_WARM) ||
	    (batt_health == POWER_SUPPLY_HEALTH_COOL) ||
	    (batt_health == POWER_SUPPLY_HEALTH_OVERHEAT) ||
	    (batt_health == POWER_SUPPLY_HEALTH_COLD))
		bq24296_check_temp_range(chip);

	if (chip->demo_mode) {
		dev_warn(chip->dev, "Battery in Demo Mode charging Limited\n");

		if (!chip->usb_suspended &&
		    (batt_soc >= DEMO_MODE_MAX_SOC))
			bq24296_usb_suspend(chip, true);
		else if (chip->usb_suspended &&
			   (batt_soc <=
			    (DEMO_MODE_MAX_SOC - DEMO_MODE_HYS_SOC)))
			bq24296_usb_suspend(chip, false);

	} else if (taper_reached && !chip->chg_done_batt_full) {
		dev_dbg(chip->dev, "Charge Complete!\n");
		chip->chg_done_batt_full = true;
	} else if (chip->chg_done_batt_full && batt_soc < 100) {
		dev_dbg(chip->dev, "SOC dropped,  Charge Resume!\n");
		chip->chg_done_batt_full = false;
	}

	bq24296_set_chrg_path_temp(chip);

	power_supply_changed(&chip->batt_psy);
	schedule_delayed_work(&chip->heartbeat_work,
			      msecs_to_jiffies(60000));
	bq_relax(&chip->bq_wake_source);
}

static  struct gpio *
bq24296_get_gpio_list(struct device *dev, int *num_gpio_list)
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

static int bq24296_of_init(struct bq24296_chg *chip)
{
	int rc;
	struct device_node *node = chip->dev->of_node;

	chip->gpio_list = bq24296_get_gpio_list(chip->dev, &chip->num_gpio_list);

	rc = of_property_read_string(node, "ti,bms-psy-name",
				&chip->bms_psy_name);
	if (rc)
		chip->bms_psy_name = NULL;

	rc = of_property_read_u32(node, "ti,ext-temp-volt-mv",
						&chip->ext_temp_volt_mv);
	if (rc < 0)
		chip->ext_temp_volt_mv = 0;

	rc = of_property_read_u32(node, "ti,oreg-voltage-mv",
						&chip->voreg_mv);
	if (rc < 0)
		chip->voreg_mv = 4350;

	rc = of_property_read_u32(node, "ti,low-voltage-uv",
						&chip->low_voltage_uv);
	if (rc < 0)
		chip->low_voltage_uv = 3200000;

	rc = of_property_read_u32(node, "ti,vin-limit-mv",
						&chip->vin_limit_mv);
	if (rc < 0)
		chip->vin_limit_mv = 4500;

	rc = of_property_read_u32(node, "ti,input-current-limit",
						&chip->input_current_limit);
	if (rc < 0)
		chip->input_current_limit = 2000;

	rc = of_property_read_u32(node, "ti,sys-vmin-mv",
						&chip->sys_vmin_mv);
	if (rc < 0)
		chip->sys_vmin_mv = 3500;

	rc = of_property_read_u32(node, "ti,chg-current-ma",
						&chip->chg_current_ma);
	if (rc < 0)
		chip->chg_current_ma = 2000;

	return 0;
}

static int bq24296_hw_init(struct bq24296_chg *chip)
{
	int ret = 0;

	NULL_CHECK(chip, -EINVAL);

	ret = bq24296_wdt_rest(chip);
	if (ret) {
		pr_err("failed to wdt reset\n");
		return ret;
	}

	ret = bq24296_set_input_vin_limit(chip, chip->vin_limit_mv);
	if (ret) {
		pr_err("failed to set input voltage limit\n");
		return ret;
	}

	ret = bq24296_set_input_i_limit(chip, chip->input_current_limit);
	if (ret) {
		pr_err("failed to set input current limit\n");
		return ret;
	}

	ret = bq24296_set_system_vmin(chip, chip->sys_vmin_mv);
	if (ret) {
		pr_err("failed to set system min voltage\n");
		return ret;
	}

	ret = bq24296_set_ibat_max(chip, chip->chg_current_ma);
	if (ret) {
		pr_err("failed to set charging current\n");
		return ret;
	}

	ret = bq24296_force_ichg_decrease(chip, 0);
	if (ret) {
		pr_err("failed to set charging current as reg[ICHG] programmed\n");
		return ret;
	}

	ret = bq24296_disable_en_term(chip);
	if (ret) {
		pr_err("failed to set disable term\n");
		return ret;
	}

	ret = bq24296_set_vbat_max(chip, chip->voreg_mv);
	if (ret) {
		pr_err("failed to set vbat max\n");
		return ret;
	}

	ret = bq24296_set_chg_timer(chip, 0);
	if (ret) {
		pr_err("failed to disable chg safety timer\n");
		return ret;
	}

	return 0;
}

#define VBUS_OFF_THRESHOLD 2000000
static int bq24296_charging_reboot(struct notifier_block *nb,
				unsigned long event, void *unused)
{
	struct qpnp_vadc_result result;

	/*
	 * Hack to power down when both VBUS and BPLUS are present.
	 * This targets factory environment, where we need to power down
	 * units with non-removable batteries between stations so that we
	 * do not drain batteries to death.
	 * Poll for VBUS to got away (controlled by external supply)
	 * before proceeding with shutdown.
	 */
	switch (event) {
	case SYS_POWER_OFF:
		if (!the_chip) {
			pr_err("called before bq24296 charging init\n");
			break;
		}

		if (!the_chip->factory_mode)
			break;
		do {
			if (qpnp_vadc_read(the_chip->vadc_dev,
				USBIN, &result)) {
				pr_err("VBUS ADC read err\n");
				break;
			} else
				pr_info("VBUS:= %lld mV\n", result.physical);
			mdelay(100);
		} while (result.physical > VBUS_OFF_THRESHOLD);
		break;
	default:
		break;
	}

	if (the_chip->factory_mode)
		pr_info("Reboot Notification: FACTORY MODE VBUS missing!!\n");

	return NOTIFY_DONE;
}

#define VBUS_POWERUP_THRESHOLD 4000000
static int determine_initial_status(struct bq24296_chg *chip)
{
	struct qpnp_vadc_result result;
	int rc;
	union power_supply_propval prop = {0,};

	chip->batt_present = bq24296_get_prop_batt_present(chip);
	chip->chg_done_batt_full = false;

	if (chip->factory_mode) {
		if (qpnp_vadc_read(chip->vadc_dev, USBIN, &result))
			pr_err("VBUS ADC read err\n");
		else {
			pr_info("VBUS:= %lld mV\n", result.physical);
			if (result.physical > VBUS_POWERUP_THRESHOLD)
				chip->factory_present = true;
		}
	}

	rc = chip->usb_psy->get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_PRESENT, &prop);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't get USB present rc = %d\n", rc);
		return rc;
	}
	chip->usb_present = !!prop.intval;

	if (chip->usb_present)
		start_charging(chip);
	else
		stop_charging(chip);

	return 0;
}

#define CHG_SHOW_MAX_SIZE 50
static ssize_t force_demo_mode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long r;
	unsigned long mode;

	r = kstrtoul(buf, 0, &mode);
	if (r) {
		pr_err("Invalid usb suspend mode value = %lu\n", mode);
		return -EINVAL;
	}

	if (!the_chip) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	the_chip->demo_mode = (mode) ? true : false;

	return r ? r : count;
}

static ssize_t force_demo_mode_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	int state;

	if (!the_chip) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	state = (the_chip->demo_mode) ? 1 : 0;

	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_demo_mode, 0644,
		force_demo_mode_show,
		force_demo_mode_store);

#define USB_SUSPEND_BIT BIT(4)
static ssize_t force_chg_usb_suspend_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned long r;
	unsigned long mode;

	r = kstrtoul(buf, 0, &mode);
	if (r) {
		pr_err("Invalid usb suspend mode value = %lu\n", mode);
		return -EINVAL;
	}

	if (!the_chip) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	r = bq24296_masked_write_fac(the_chip, BQ00_INPUT_SRC_CONT_REG, EN_HIZ,
		mode ? (1 << EN_HIZ_SHIFT) : (0 << EN_HIZ_SHIFT));

	return r ? r : count;
}

#define USB_SUSPEND_STATUS_BIT BIT(3)
static ssize_t force_chg_usb_suspend_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int state;
	int ret;
	u8 value;

	if (!the_chip) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	ret = bq24296_read(the_chip, BQ00_INPUT_SRC_CONT_REG, &value);
	if (ret) {
		pr_err("USB_SUSPEND_STATUS_BIT failed ret = %d\n", ret);
		state = -EFAULT;
		goto end;
	}

	state = (EN_HIZ & value) ? 1 : 0;

end:
	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_chg_usb_suspend, 0664,
		force_chg_usb_suspend_show,
		force_chg_usb_suspend_store);

static ssize_t force_chg_fail_clear_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned long r;
	unsigned long mode;

	r = kstrtoul(buf, 0, &mode);
	if (r) {
		pr_err("Invalid chg fail mode value = %lu\n", mode);
		return -EINVAL;
	}

	/* do nothing for bq24296 */
	r = 0;

	return r ? r : count;
}

static ssize_t force_chg_fail_clear_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	/* do nothing for bq24296 */
	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "0\n");
}

static DEVICE_ATTR(force_chg_fail_clear, 0664,
		force_chg_fail_clear_show,
		force_chg_fail_clear_store);

static ssize_t force_chg_auto_enable_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned long r;
	unsigned long mode;

	r = kstrtoul(buf, 0, &mode);
	if (r) {
		pr_err("Invalid chrg enable value = %lu\n", mode);
		return -EINVAL;
	}

	if (!the_chip) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	r = bq24296_masked_write_fac(the_chip, BQ01_PWR_ON_CONF_REG,
			CHG_CONFIG_MASK, mode ? (1 << CHG_ENABLE_SHIFT) : 0);
	if (r < 0) {
		dev_err(the_chip->dev,
			"Couldn't set CHG_ENABLE_BIT enable = %d r = %d\n",
			(int)mode, (int)r);
		return r;
	}

	return r ? r : count;
}

static ssize_t force_chg_auto_enable_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int state;
	int ret;
	u8 value;

	if (!the_chip) {
		pr_err("chip not valid\n");
		state = -ENODEV;
		goto end;
	}

	ret = bq24296_read(the_chip, BQ01_PWR_ON_CONF_REG, &value);
	if (ret) {
		pr_err("CHG_EN_BIT failed ret = %d\n", ret);
		state = -EFAULT;
		goto end;
	}

	state = (CHG_CONFIG_MASK & value) ? 1 : 0;

end:
	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_chg_auto_enable, 0664,
		force_chg_auto_enable_show,
		force_chg_auto_enable_store);

#define MAX_IBATT_LEVELS 31
static ssize_t force_chg_ibatt_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned long r;
	unsigned long chg_current;
	u8 reg_val = 0;

	r = kstrtoul(buf, 0, &chg_current);
	if (r) {
		pr_err("Invalid ibatt value = %lu\n", chg_current);
		return -EINVAL;
	}

	if (!the_chip) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}


	if (chg_current < IBAT_MIN_MA)
		chg_current = IBAT_MIN_MA;
	if (chg_current > IBAT_MAX_MA)
		chg_current = IBAT_MAX_MA;

	reg_val = (chg_current - IBAT_MIN_MA)/IBAT_STEP_MA;
	reg_val = reg_val << 2;



	r = bq24296_masked_write_fac(the_chip, BQ02_CHARGE_CUR_CONT_REG,
				     ICHG_MASK, reg_val);
	if (r) {
		dev_err(the_chip->dev,
			"Couldn't set Fast Charge Current = %d r = %d\n",
			(int)chg_current, (int)r);
		return r;
	}

	return r ? r : count;
}

static ssize_t force_chg_ibatt_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int state;
	int ret;
	u8 value;

	if (!the_chip) {
		pr_err("chip not valid\n");
		state = -ENODEV;
		goto end;
	}

	ret = bq24296_read(the_chip, BQ02_CHARGE_CUR_CONT_REG, &value);

	if (ret) {
		pr_err("Fast Charge Current failed ret = %d\n", ret);
		state = -EFAULT;
		goto end;
	}

	value = ((value & ICHG_MASK) >> ICHG_SHIFT);
	state = value * IBAT_STEP_MA + IBAT_MIN_MA;

end:
	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_chg_ibatt, 0664,
		force_chg_ibatt_show,
		force_chg_ibatt_store);

static ssize_t force_chg_iusb_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned long r;
	unsigned long usb_curr;
	u8 temp;
	int i;

	r = kstrtoul(buf, 0, &usb_curr);
	if (r) {
		pr_err("Invalid iusb value = %lu\n", usb_curr);
		return -EINVAL;
	}

	if (!the_chip) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	for (i = ARRAY_SIZE(icl_ma_table) - 1; i > 0; i--) {
		if (usb_curr >= icl_ma_table[i].icl_ma)
			break;
	}
	temp = icl_ma_table[i].value;

	r = bq24296_masked_write_fac(the_chip, BQ00_INPUT_SRC_CONT_REG,
				      IINLIM_MASK, temp);
	if (r) {
		dev_err(the_chip->dev,
			"Couldn't set USBIN Current = %d r = %d\n",
			(int)usb_curr, (int)r);
		return r;
	}

	return r ? r : count;
}

static ssize_t force_chg_iusb_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int state = 0;
	int ret;
	u8 value;

	if (!the_chip) {
		pr_err("chip not valid\n");
		ret = -ENODEV;
		goto end;
	}

	ret = bq24296_read(the_chip, BQ00_INPUT_SRC_CONT_REG, &value);
	if (ret) {
		pr_err("USBIN Current failed ret = %d\n", ret);
		state = -EFAULT;
		goto end;
	}

	value = ((value & IINLIM_MASK) >> IINLIM_MASK_SHIFT);
	state = icl_ma_table[value].icl_ma;

end:
	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_chg_iusb, 0664,
		force_chg_iusb_show,
		force_chg_iusb_store);

static ssize_t force_chg_itrick_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned long r;
	unsigned long mode;

	r = kstrtoul(buf, 0, &mode);
	if (r) {
		pr_err("Invalid itrick value = %lu\n", mode);
		return -EINVAL;
	}

	/* do nothing for bq24296 */
	r = 0;

	return r ? r : count;
}

static ssize_t force_chg_itrick_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	/* do nothing for bq24296 */
	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "340\n");
}

static DEVICE_ATTR(force_chg_itrick, 0664,
		   force_chg_itrick_show,
		   force_chg_itrick_store);

static int get_reg(void *data, u64 *val)
{
	int rc;
	u8 temp;

	if (!the_chip) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	rc = bq24296_read(the_chip, the_chip->peek_poke_address, &temp);
	if (rc < 0) {
		dev_err(the_chip->dev,
			"Couldn't read reg %x rc = %d\n",
			the_chip->peek_poke_address, rc);
		return -EAGAIN;
	}
	*val = temp;
	return 0;
}

static int set_reg(void *data, u64 val)
{
	int rc;
	u8 temp;

	if (!the_chip) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	temp = (u8) val;
	rc = __bq24296_write_fac(the_chip, the_chip->peek_poke_address, temp);
	if (rc < 0) {
		dev_err(the_chip->dev,
			"Couldn't write 0x%02x to 0x%02x rc= %d\n",
			the_chip->peek_poke_address, temp, rc);
		return -EAGAIN;
	}
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(poke_poke_debug_ops, get_reg, set_reg, "0x%02llx\n");

static int show_registers(struct seq_file *m, void *data)
{
	int rc, i;
	u8 reg;

	if (!the_chip) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	for (i = 0; i < ARRAY_SIZE(bq24296_regs); i++) {
		rc = bq24296_read(the_chip, bq24296_regs[i].regaddress, &reg);
		if (!rc)
			seq_printf(m, "%s - 0x%02x = 0x%02x\n",
					bq24296_regs[i].regname,
					bq24296_regs[i].regaddress,
					reg);
	}

	return 0;
}

static int registers_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_registers, the_chip);
}

static const struct file_operations registers_debugfs_ops = {
	.owner          = THIS_MODULE,
	.open           = registers_debugfs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static int show_config(struct seq_file *m, void *data)
{
	if (!the_chip) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	seq_printf(m, "bms_psy_name\t=\t%s\n"
			"ext_temp_volt_mv\t=\t%d\n"
			"voreg_mv\t=\t%d\n"
			"low_voltage_uv\t=\t%d\n"
			"vin_limit_mv\t=\t%d\n"
			"input_current_limit\t=\t%d\n"
			"sys_vmin_mv\t=\t%d\n"
			"chg_current_ma\t=\t%d\n",
			the_chip->bms_psy_name,
			the_chip->ext_temp_volt_mv,
			the_chip->voreg_mv,
			the_chip->low_voltage_uv,
			the_chip->vin_limit_mv,
			the_chip->input_current_limit,
			the_chip->sys_vmin_mv,
			the_chip->chg_current_ma);

	return 0;
}

static int config_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_config, the_chip);
}

static const struct file_operations config_debugfs_ops = {
	.owner          = THIS_MODULE,
	.open           = config_debugfs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static bool bq24296_charger_mmi_factory(void)
{
	struct device_node *np = of_find_node_by_path("/chosen");
	u32 fact_cable = 0;

	if (np)
		of_property_read_u32(np, "mmi,factory-cable", &fact_cable);

	of_node_put(np);
	return !!fact_cable ? true : false;
}

static bool bq24296_charger_test_mode(void)
{
	struct device_node *np = of_find_node_by_path("/chosen");
	const char *mode;
	int rc;
	bool test = false;

	if (!np)
		return test;

	rc = of_property_read_string(np, "mmi,battery", &mode);
	if ((rc >= 0) && mode) {
		if (strcmp(mode, "test") == 0)
			test = true;
	}
	of_node_put(np);

	return test;
}

static struct of_device_id bq24296_match_table[] = {
	{ .compatible = "ti,bq24296-charger", },
	{ },
};

#define DEFAULT_TEST_MODE_SOC  52
#define DEFAULT_TEST_MODE_TEMP  225
static int bq24296_charger_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int rc;
	struct bq24296_chg *chip;
	struct power_supply *usb_psy;
	union power_supply_propval ret = {0, };

	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		dev_dbg(&client->dev, "USB supply not found; defer probe\n");
		return -EPROBE_DEFER;
	}

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	chip->client = client;
	chip->dev = &client->dev;
	chip->usb_psy = usb_psy;
	chip->test_mode_soc = DEFAULT_TEST_MODE_SOC;
	chip->test_mode_temp = DEFAULT_TEST_MODE_TEMP;
	chip->demo_mode = false;
	chip->usb_suspended = false;
	chip->factory_mode = false;
	chip->test_mode = false;
	chip->factory_present = false;
	chip->poll_fast = false;
	chip->shutdown_voltage_tripped = false;
	chip->chg_enabled = true;
	chip->factory_configured = false;
	chip->chg_init_finish = false;

	mutex_init(&chip->read_write_lock);

	i2c_set_clientdata(client, chip);

	wakeup_source_init(&chip->bq_wake_source.source, "bq24296_wake");
	wakeup_source_init(&chip->bq_wake_source_charger.source,
		"bq24296_wake_charger");
	chip->bq_wake_source_charger.disabled = 1;

	INIT_DELAYED_WORK(&chip->heartbeat_work,
					heartbeat_work);
	INIT_DELAYED_WORK(&chip->iusb_work,
					iusb_work);

	chip->batt_psy.name = "battery";
	chip->batt_psy.type = POWER_SUPPLY_TYPE_BATTERY;
	chip->batt_psy.get_property = bq24296_batt_get_property;
	chip->batt_psy.set_property = bq24296_batt_set_property;
	chip->batt_psy.properties = bq24296_batt_properties;
	chip->batt_psy.num_properties = ARRAY_SIZE(bq24296_batt_properties);
	chip->batt_psy.external_power_changed = bq24296_external_power_changed;
	chip->batt_psy.property_is_writeable = bq24296_batt_is_writeable;

	rc = power_supply_register(chip->dev, &chip->batt_psy);
	if (rc < 0) {
		dev_err(&client->dev,
			"Unable to register batt_psy rc = %d\n", rc);
		return rc;
	}

	chip->factory_mode = bq24296_charger_mmi_factory();
	if (chip->factory_mode)
		dev_info(&client->dev, "Factory Mode: writes disabled\n");

	chip->test_mode = bq24296_charger_test_mode();
	if (chip->test_mode)
		dev_info(&client->dev, "Test Mode Enabled\n");

	rc = bq24296_of_init(chip);
	if (rc) {
		dev_err(&client->dev, "Couldn't initialize OF DT values\n");
		goto unregister_batt_psy;
	}

	chip->vadc_dev = qpnp_get_vadc(chip->dev, "bq24296");
	if (IS_ERR(chip->vadc_dev)) {
		rc = PTR_ERR(chip->vadc_dev);
		if (rc == -EPROBE_DEFER)
			pr_err("vadc not ready, defer probe\n");
		goto unregister_batt_psy;
	}

	chip->adc_tm_dev = qpnp_get_adc_tm(chip->dev, "bq24296");
	if (IS_ERR(chip->adc_tm_dev)) {
		rc = PTR_ERR(chip->adc_tm_dev);
		if (rc == -EPROBE_DEFER)
			pr_err("adc_tm not ready, defer probe\n");
		goto unregister_batt_psy;
	}

	if (!chip->bms_psy && chip->bms_psy_name) {
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);

		if (!chip->bms_psy) {
			if (chip->factory_mode) {
				chip->bms_psy = NULL;
				pr_err("bms not ready, factory mode\n");
			} else {
				dev_dbg(&client->dev,
					"%s not found; defer probe\n",
					chip->bms_psy_name);
				rc = -EPROBE_DEFER;
				goto unregister_batt_psy;
			}
		}
	}

	if (chip->gpio_list) {
		rc = gpio_request_array(chip->gpio_list, chip->num_gpio_list);
		if (rc) {
			dev_err(&client->dev, "cannot request GPIOs\n");
			return rc;
		}
	} else
		dev_err(&client->dev, "gpio_list is NULL\n");

	bq24296_hw_init(chip);

	chip->chg_init_finish = true;

	determine_initial_status(chip);

	chip->notifier.notifier_call = &bq24296_charging_reboot;
	the_chip = chip;

	rc = device_create_file(chip->dev,
				&dev_attr_force_demo_mode);
	if (rc) {
		pr_err("couldn't create force_demo_mode\n");
		goto unregister_batt_psy;
	}

	chip->debug_root = debugfs_create_dir("bq24296", NULL);
	if (!chip->debug_root)
		dev_err(chip->dev, "Couldn't create debug dir\n");
	else {
		struct dentry *ent;

		ent = debugfs_create_x32("address", S_IFREG | S_IWUSR | S_IRUGO,
					chip->debug_root,
					&(chip->peek_poke_address));
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create address debug file rc = %d\n",
				rc);

		ent = debugfs_create_file("data", S_IFREG | S_IWUSR | S_IRUGO,
					chip->debug_root, chip,
					&poke_poke_debug_ops);
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create data debug file rc = %d\n",
				rc);

		ent = debugfs_create_file("registers", S_IFREG | S_IRUGO,
					chip->debug_root, chip,
					&registers_debugfs_ops);
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create regs debug file rc = %d\n",
				rc);

		ent = debugfs_create_file("config", S_IFREG | S_IRUGO,
					chip->debug_root, chip,
					&config_debugfs_ops);
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create config debug file rc = %d\n",
				rc);
	}

	if (chip->factory_mode) {
		rc = device_create_file(chip->dev,
					&dev_attr_force_chg_usb_suspend);
		if (rc) {
			pr_err("couldn't create force_chg_usb_suspend\n");
			goto unregister_batt_psy;
		}

		rc = device_create_file(chip->dev,
					&dev_attr_force_chg_fail_clear);
		if (rc) {
			pr_err("couldn't create force_chg_fail_clear\n");
			goto unregister_batt_psy;
		}

		rc = device_create_file(chip->dev,
					&dev_attr_force_chg_auto_enable);
		if (rc) {
			pr_err("couldn't create force_chg_auto_enable\n");
			goto unregister_batt_psy;
		}

		rc = device_create_file(chip->dev,
				&dev_attr_force_chg_ibatt);
		if (rc) {
			pr_err("couldn't create force_chg_ibatt\n");
			goto unregister_batt_psy;
		}

		rc = device_create_file(chip->dev,
					&dev_attr_force_chg_iusb);
		if (rc) {
			pr_err("couldn't create force_chg_iusb\n");
			goto unregister_batt_psy;
		}

		rc = device_create_file(chip->dev,
					&dev_attr_force_chg_itrick);
		if (rc) {
			pr_err("couldn't create force_chg_itrick\n");
			goto unregister_batt_psy;
		}

	}

	rc = register_reboot_notifier(&chip->notifier);
	if (rc)
		pr_err("%s can't register reboot notifier\n", __func__);

	rc = bq24296_setup_vbat_monitoring(chip);
	if (rc < 0)
		pr_err("failed to set up voltage notifications: %d\n", rc);

	if (chip->bms_psy) {
		rc = chip->bms_psy->get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_HEALTH, &ret);
		if (rc)
			dev_err(chip->dev, "Couldn't get batt health\n");
		else
			bq24296_set_prop_batt_health(chip, ret.intval);
	}

	schedule_delayed_work(&chip->heartbeat_work,
				msecs_to_jiffies(60000));

	dev_dbg(&client->dev, "BQ24296 batt=%d usb=%d done=%d\n",
			chip->batt_present, chip->usb_present,
			chip->chg_done_batt_full);

	return 0;

unregister_batt_psy:
	dev_err(&client->dev, "err\n");
	power_supply_unregister(&chip->batt_psy);
	wakeup_source_trash(&chip->bq_wake_source.source);
	wakeup_source_trash(&chip->bq_wake_source_charger.source);
	devm_kfree(chip->dev, chip);
	chip->chg_init_finish = false;
	the_chip = NULL;

	return rc;
}

static int bq24296_charger_remove(struct i2c_client *client)
{
	struct bq24296_chg *chip = i2c_get_clientdata(client);

	device_remove_file(chip->dev,
			   &dev_attr_force_demo_mode);
	unregister_reboot_notifier(&chip->notifier);
	debugfs_remove_recursive(chip->debug_root);
	power_supply_unregister(&chip->batt_psy);
	wakeup_source_trash(&chip->bq_wake_source.source);
	wakeup_source_trash(&chip->bq_wake_source_charger.source);
	devm_kfree(chip->dev, chip);
	the_chip = NULL;

	return 0;
}

static int bq24296_suspend(struct device *dev)
{
	return 0;
}

static int bq24296_suspend_noirq(struct device *dev)
{
	return 0;
}

static int bq24296_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops bq24296_pm_ops = {
	.resume		= bq24296_resume,
	.suspend_noirq	= bq24296_suspend_noirq,
	.suspend	= bq24296_suspend,
};

static const struct i2c_device_id bq24296_charger_id[] = {
	{"bq24296-charger", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, bq24296_charger_id);

static struct i2c_driver bq24296_charger_driver = {
	.driver		= {
		.name		= "bq24296-charger",
		.owner		= THIS_MODULE,
		.of_match_table	= bq24296_match_table,
		.pm		= &bq24296_pm_ops,
	},
	.probe		= bq24296_charger_probe,
	.remove		= bq24296_charger_remove,
	.id_table	= bq24296_charger_id,
};

module_i2c_driver(bq24296_charger_driver);

MODULE_DESCRIPTION("bq24296 Charger");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:bq24296-charger");

