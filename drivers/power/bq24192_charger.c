/* Copyright (c) 2013 LGE Inc. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/power/bq24192_charger.h>
#include <linux/bitops.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

/* Register definitions */
#define INPUT_SRC_CONT_REG              0X00
#define PWR_ON_CONF_REG                 0X01
#define CHARGE_CUR_CONT_REG             0X02
#define PRE_CHARGE_TERM_CUR_REG         0X03
#define CHARGE_VOLT_CONT_REG            0X04
#define CHARGE_TERM_TIMER_CONT_REG      0X05
#define IR_COMP_THERM_CONT_REG          0X06
#define MISC_OPERATION_CONT_REG         0X07
#define SYSTEM_STATUS_REG               0X08
#define FAULT_REG                       0X09
#define VENDOR_PART_REV_STATUS_REG      0X0A

#define RESET_REGISTER_MASK        0x80
#define CHG_CONFIG_MASK            0x30
#define PG_STAT_MASK               0x04
#define VBUS_STAT_MASK             0xC0
#define PRE_CHARGE_MASK            0x10
#define FAST_CHARGE_MASK           0x20
#define CHARGING_MASK (PRE_CHARGE_MASK | FAST_CHARGE_MASK)
#define INPUT_CURRENT_LIMIT_MASK   0x07
#define INPUT_VOLTAGE_LIMIT_MASK   0x78
#define SYSTEM_MIN_VOLTAGE_MASK    0x0E
#define PRECHG_CURRENT_MASK        0xF0
#define TERM_CURRENT_MASK          0x0F
#define CHG_VOLTAGE_LIMIT_MASK     0xFC
#define CHG_CURRENT_LIMIT_MASK     0xFC
#define EN_CHG_TERM_MASK           0x80
#define EN_CHG_TIMER_MASK          0x08
#define I2C_TIMER_MASK             0x30
#define CHG_TIMER_LIMIT_MASK       0x06
#define IR_COMP_R_MASK             0xE0
#define IR_COMP_VCLAMP_MASK        0x1C
#define THERM_REG_MASK             0x03

struct bq24192_chip {
	int  chg_current_ma;
	int  term_current_ma;
	int  int_gpio;
	int  irq;
	struct i2c_client  *client;
	struct delayed_work  irq_work;
	struct dentry  *dent;
	struct wake_lock  chg_wake_lock;
	struct power_supply  *usb_psy;
	struct power_supply  ac_psy;
	struct power_supply  *wlc_psy;
	struct power_supply  *batt_psy;
	int  usb_online;
	int  ac_online;
	int  ext_pwr;
	int  wlc_support;
};

static struct bq24192_chip *the_chip;

struct debug_reg {
	char  *name;
	u8  reg;
};

#define BQ24192_DEBUG_REG(x) {#x, x##_REG}

static struct debug_reg bq24192_debug_regs[] = {
	BQ24192_DEBUG_REG(INPUT_SRC_CONT),
	BQ24192_DEBUG_REG(PWR_ON_CONF),
	BQ24192_DEBUG_REG(CHARGE_CUR_CONT),
	BQ24192_DEBUG_REG(PRE_CHARGE_TERM_CUR),
	BQ24192_DEBUG_REG(CHARGE_VOLT_CONT),
	BQ24192_DEBUG_REG(CHARGE_TERM_TIMER_CONT),
	BQ24192_DEBUG_REG(IR_COMP_THERM_CONT),
	BQ24192_DEBUG_REG(MISC_OPERATION_CONT),
	BQ24192_DEBUG_REG(SYSTEM_STATUS),
	BQ24192_DEBUG_REG(FAULT),
	BQ24192_DEBUG_REG(VENDOR_PART_REV_STATUS),
};

static int bq24192_read_reg(struct i2c_client *client, int reg, u8 *val)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		dev_err(&client->dev,
			"i2c read fail: can't read from %02x: %d\n",
			reg, ret);
		return ret;
	} else {
		*val = ret;
	}

	return 0;
}

static int bq24192_write_reg(struct i2c_client *client, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(client, reg, val);
	if (ret < 0) {
		dev_err(&client->dev,
			"i2c write fail: can't write %02x to %02x: %d\n",
			val, reg, ret);
		return ret;
	}
	return 0;
}

static int bq24192_masked_write(struct i2c_client *client, int reg,
			       u8 mask, u8 val)
{
	s32 rc;
	u8 temp;

	rc = bq24192_read_reg(client, reg, &temp);
	if (rc) {
		pr_err("bq24192_read_reg failed: reg=%03X, rc=%d\n",
				reg, rc);
		return rc;
	}

	temp &= ~mask;
	temp |= val & mask;

	rc = bq24192_write_reg(client, reg, temp);
	if (rc) {
		pr_err("bq24192_write failed: reg=%03X, rc=%d\n",
				reg, rc);
		return rc;
	}

	return 0;
}

static bool bq24192_is_charger_present(struct bq24192_chip *chip)
{
	u8 temp;
	bool chg_online;
	int ret;

	ret = bq24192_read_reg(chip->client, SYSTEM_STATUS_REG, &temp);
	if (ret) {
		pr_err("failed to read SYSTEM_STATUS_REG rc=%d\n", ret);
		return false;
	}

	chg_online = temp & PG_STAT_MASK;
	pr_debug("charger present = %d\n", chg_online);

	return !!chg_online;
}

#define CHG_ENABLE_SHIFT  4
static int bq24192_enable_charging(struct bq24192_chip *chip, bool enable)
{
	int ret;
	u8 val = (u8)(!!enable << CHG_ENABLE_SHIFT);

	pr_info("enable=%d\n", enable);

	ret = bq24192_masked_write(chip->client, PWR_ON_CONF_REG,
						CHG_CONFIG_MASK, val);
	if (ret) {
		pr_err("failed to set CHG_CONFIG rc=%d\n", ret);
		return ret;
	}

	return 0;
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
	{1200, 0x04},
	{1500, 0x05},
	{2000, 0x06},
	{3000, 0x07},
};

#define INPUT_CURRENT_LIMIT_MIN_MA  100
#define INPUT_CURRENT_LIMIT_MAX_MA  3000
static int bq24192_set_input_i_limit(struct bq24192_chip *chip, int ma)
{
	int i;
	u8 temp;

	if (ma < INPUT_CURRENT_LIMIT_MIN_MA
			|| ma > INPUT_CURRENT_LIMIT_MAX_MA) {
		pr_err("bad mA=%d asked to set\n", ma);
		return -EINVAL;
	}


	for (i = ARRAY_SIZE(icl_ma_table) - 1; i >= 0; i--) {
		if (icl_ma_table[i].icl_ma == ma)
			break;
	}

	if (i < 0) {
		pr_err("can't find %d in icl_ma_table. Use min.\n", ma);
		i = 0;
	}

	temp = icl_ma_table[i].value;

	pr_info("input current limit=%d setting 0x%02x\n", ma, temp);
	return bq24192_masked_write(chip->client, INPUT_SRC_CONT_REG,
			INPUT_CURRENT_LIMIT_MASK, temp);
}

#define IBAT_MAX_MA  4532
#define IBAT_MIN_MA  500
#define IBAT_STEP_MA  64
static int bq24192_set_ibat_max(struct bq24192_chip *chip, int ma)
{
	u8 reg_val = 0;
	u8 set_ibat = 0;

	if (ma < IBAT_MIN_MA || ma > IBAT_MAX_MA) {
		pr_err("bad mA=%d asked to set\n", ma);
		return -EINVAL;
	}

	reg_val = (ma - IBAT_MIN_MA)/IBAT_STEP_MA;
	set_ibat = reg_val * IBAT_STEP_MA + IBAT_MIN_MA;
	reg_val = reg_val << 2;

	pr_info("req_ibat = %d set_ibat = %d reg_val = 0x%02x\n",
				ma, set_ibat, reg_val);

	return bq24192_masked_write(chip->client, CHARGE_CUR_CONT_REG,
			CHG_CURRENT_LIMIT_MASK, reg_val);
}

#define VIN_LIMIT_MIN_MV  3880
#define VIN_LIMIT_MAX_MV  5080
#define VIN_LIMIT_STEP_MV  80
static int bq24192_set_input_vin_limit(struct bq24192_chip *chip, int mv)
{
	u8 reg_val = 0;
	u8 set_vin = 0;

	if (mv < VIN_LIMIT_MIN_MV || mv > VIN_LIMIT_MAX_MV) {
		pr_err("bad mV=%d asked to set\n", mv);
		return -EINVAL;
	}

	reg_val = (mv - VIN_LIMIT_MIN_MV)/VIN_LIMIT_STEP_MV;
	set_vin = reg_val * VIN_LIMIT_STEP_MV + VIN_LIMIT_MIN_MV;
	reg_val = reg_val << 3;

	pr_info("req_vin = %d set_vin = %d reg_val = 0x%02x\n",
				mv, set_vin, reg_val);

	return bq24192_masked_write(chip->client, INPUT_SRC_CONT_REG,
			INPUT_VOLTAGE_LIMIT_MASK, reg_val);
}

#define VBAT_MAX_MV  4400
#define VBAT_MIN_MV  3504
#define VBAT_STEP_MV  16
static int bq24192_set_vbat_max(struct bq24192_chip *chip, int mv)
{
	u8 reg_val = 0;
	u8 set_vbat = 0;

	if (mv < VBAT_MIN_MV || mv > VBAT_MAX_MV) {
		pr_err("bad mv=%d asked to set\n", mv);
		return -EINVAL;
	}

	reg_val = (mv - VBAT_MIN_MV)/VBAT_STEP_MV;
	set_vbat = reg_val * VBAT_STEP_MV + VBAT_MIN_MV;
	reg_val = reg_val << 2;

	pr_info("req_vbat = %d set_vbat = %d reg_val = 0x%02x\n",
				mv, set_vbat, reg_val);

	return bq24192_masked_write(chip->client, CHARGE_VOLT_CONT_REG,
			CHG_VOLTAGE_LIMIT_MASK, reg_val);
}

#define SYSTEM_VMIN_LOW_MV  3000
#define SYSTEM_VMIN_HIGH_MV  3700
#define SYSTEM_VMIN_STEP_MV  100
static int bq24192_set_system_vmin(struct bq24192_chip *chip, int mv)
{
	u8 reg_val = 0;
	u8 set_vmin = 0;

	if (mv < SYSTEM_VMIN_LOW_MV || mv > SYSTEM_VMIN_HIGH_MV) {
		pr_err("bad mv=%d asked to set\n", mv);
		return -EINVAL;
	}

	reg_val = (mv - SYSTEM_VMIN_LOW_MV)/SYSTEM_VMIN_STEP_MV;
	set_vmin = reg_val * SYSTEM_VMIN_STEP_MV + SYSTEM_VMIN_LOW_MV;
	reg_val = reg_val << 1;

	pr_info("req_vmin = %d set_vmin = %d reg_val = 0x%02x\n",
				mv, set_vmin, reg_val);

	return bq24192_masked_write(chip->client, PWR_ON_CONF_REG,
			SYSTEM_MIN_VOLTAGE_MASK, reg_val);
}

#define IPRECHG_MIN_MA  128
#define IPRECHG_MAX_MA  2048
#define IPRECHG_STEP_MA  128
static int bq24192_set_prechg_i_limit(struct bq24192_chip *chip, int ma)
{
	u8 reg_val = 0;
	u8 set_ma = 0;

	if (ma < IPRECHG_MIN_MA || ma > IPRECHG_MAX_MA) {
		pr_err("bad ma=%d asked to set\n", ma);
		return -EINVAL;
	}

	reg_val = (ma - IPRECHG_MIN_MA)/IPRECHG_STEP_MA;
	set_ma = reg_val * IPRECHG_STEP_MA + IPRECHG_MIN_MA;
	reg_val = reg_val << 4;

	pr_info("req_i = %d set_i = %d reg_val = 0x%02x\n",
				ma, set_ma, reg_val);

	return bq24192_masked_write(chip->client, PRE_CHARGE_TERM_CUR_REG,
			PRECHG_CURRENT_MASK, reg_val);
}

#define ITERM_MIN_MA  128
#define ITERM_MAX_MA  2048
#define ITERM_STEP_MA  128
static int bq24192_set_term_current(struct bq24192_chip *chip, int ma)
{
	u8 reg_val = 0;
	u8 set_ma = 0;

	if (ma < ITERM_MIN_MA || ma > ITERM_MAX_MA) {
		pr_err("bad mv=%d asked to set\n", ma);
		return -EINVAL;
	}

	reg_val = (ma - ITERM_MIN_MA)/ITERM_STEP_MA;
	set_ma = reg_val * ITERM_STEP_MA + ITERM_MIN_MA;

	pr_info("req_i = %d set_i = %d reg_val = 0x%02x\n",
				ma, set_ma, reg_val);

	return bq24192_masked_write(chip->client, PRE_CHARGE_TERM_CUR_REG,
			TERM_CURRENT_MASK, reg_val);
}

#define IRCOMP_R_MIN_MOHM  0
#define IRCOMP_R_MAX_MOHM  70
#define IRCOMP_R_STEP_MOHM  10
static int bq24192_set_ir_comp_resister(struct bq24192_chip *chip, int mohm)
{
	u8 reg_val = 0;
	u8 set_ma = 0;

	if (mohm < IRCOMP_R_MIN_MOHM
			|| mohm > IRCOMP_R_MAX_MOHM) {
		pr_err("bad r=%d asked to set\n", mohm);
		return -EINVAL;
	}

	reg_val = (mohm - IRCOMP_R_MIN_MOHM)/IRCOMP_R_STEP_MOHM;
	set_ma = reg_val * IRCOMP_R_STEP_MOHM + IRCOMP_R_MIN_MOHM;
	reg_val = reg_val << 5;

	pr_info("req_r = %d set_r = %d reg_val = 0x%02x\n",
				mohm, set_ma, reg_val);

	return bq24192_masked_write(chip->client, IR_COMP_THERM_CONT_REG,
			IR_COMP_R_MASK, reg_val);
}

#define IRCOMP_VCLAMP_MIN_MV  0
#define IRCOMP_VCLAMP_MAX_MV  112
#define IRCOMP_VCLAMP_STEP_MV  16
static int bq24192_set_vclamp_mv(struct bq24192_chip *chip, int mv)
{
	u8 reg_val = 0;
	u8 set_ma = 0;

	if (mv < IRCOMP_VCLAMP_MIN_MV
			|| mv > IRCOMP_VCLAMP_MAX_MV) {
		pr_err("bad mv=%d asked to set\n", mv);
		return -EINVAL;
	}

	reg_val = (mv - IRCOMP_VCLAMP_MIN_MV)/IRCOMP_VCLAMP_STEP_MV;
	set_ma = reg_val * IRCOMP_VCLAMP_STEP_MV + IRCOMP_VCLAMP_MIN_MV;
	reg_val = reg_val << 2;

	pr_info("req_mv = %d set_mv = %d reg_val = 0x%02x\n",
				mv, set_ma, reg_val);

	return bq24192_masked_write(chip->client, IR_COMP_THERM_CONT_REG,
			IR_COMP_VCLAMP_MASK, reg_val);
}

static void bq24192_irq_worker(struct work_struct *work)
{
	struct bq24192_chip *chip =
		container_of(work, struct bq24192_chip, irq_work.work);
	bool ext_pwr;

	ext_pwr = bq24192_is_charger_present(chip);

	if (chip->ext_pwr ^ ext_pwr) {
		chip->ext_pwr = ext_pwr;
		pr_info("notify vbus to usb otg ext_pwr = %d\n", ext_pwr);
		power_supply_set_present(chip->usb_psy, chip->ext_pwr);
	}
	power_supply_changed(&chip->ac_psy);
}

static irqreturn_t bq24192_irq(int irq, void *dev_id)
{
	struct bq24192_chip *chip = dev_id;

	schedule_delayed_work(&chip->irq_work, msecs_to_jiffies(100));

	return IRQ_HANDLED;
}

static int set_reg(void *data, u64 val)
{
	u32 addr = (u32) data;
	int ret;
	struct i2c_client *client = the_chip->client;

	ret = bq24192_write_reg(client, addr, (u8) val);

	return ret;
}

static int get_reg(void *data, u64 *val)
{
	u32 addr = (u32) data;
	u8 temp;
	int ret;
	struct i2c_client *client = the_chip->client;

	ret = bq24192_read_reg(client, addr, &temp);
	if (ret < 0)
		return ret;

	*val = temp;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(reg_fops, get_reg, set_reg, "0x%02llx\n");

static void bq24192_external_power_changed(struct power_supply *psy)
{
	struct bq24192_chip *chip = container_of(psy,
					struct bq24192_chip, ac_psy);
	union power_supply_propval ret = {0,};
	int wlc_online = 0;
	int wlc_chg_current_ma = 0;

	chip->usb_psy->get_property(chip->usb_psy,
			  POWER_SUPPLY_PROP_ONLINE, &ret);
	chip->usb_online = ret.intval;

	if (chip->wlc_support) {
		chip->wlc_psy->get_property(chip->wlc_psy,
				  POWER_SUPPLY_PROP_ONLINE, &ret);
		wlc_online = ret.intval;

		chip->wlc_psy->get_property(chip->wlc_psy,
				  POWER_SUPPLY_PROP_CURRENT_MAX, &ret);
		wlc_chg_current_ma = ret.intval / 1000;
		pr_info(" wlc_online = %d current = %d\n", wlc_online,
						wlc_chg_current_ma);
	}

	if (chip->usb_online &&
			bq24192_is_charger_present(chip)) {
		chip->usb_psy->get_property(chip->usb_psy,
				  POWER_SUPPLY_PROP_CURRENT_MAX, &ret);
		chip->chg_current_ma = ret.intval / 1000;
		bq24192_set_input_i_limit(chip,
					chip->chg_current_ma);
		pr_info("usb is online! i_limit = %d\n",
				chip->chg_current_ma);
	} else if (chip->ac_online &&
			bq24192_is_charger_present(chip)) {
		bq24192_set_input_i_limit(chip,
			INPUT_CURRENT_LIMIT_MAX_MA);
		bq24192_set_ibat_max(chip, chip->chg_current_ma);
		pr_info("ac is online! i_limit = %d\n",
				chip->chg_current_ma);
	} else if (wlc_online && bq24192_is_charger_present(chip)) {
		bq24192_set_input_i_limit(chip, wlc_chg_current_ma);
		pr_info("wlc is online! i_limit = %d\n",
				wlc_chg_current_ma);
	}

	chip->usb_psy->get_property(chip->usb_psy,
			  POWER_SUPPLY_PROP_SCOPE, &ret);
	if (ret.intval)
		pr_info("usb host mode = %d\n", ret.intval);

	power_supply_changed(&chip->ac_psy);
}

#define FAIL_DEFAULT_SOC 50
static int bq24192_get_soc_from_batt_psy(struct bq24192_chip *chip)
{
	union power_supply_propval ret = {0,};
	int soc = 0;

	if (!chip->batt_psy)
		chip->batt_psy = power_supply_get_by_name("battery");

	if (chip->batt_psy) {
		chip->batt_psy->get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_CAPACITY, &ret);
		soc = ret.intval;
	} else {
		pr_warn("battery power supply is not registered yet\n");
		soc = FAIL_DEFAULT_SOC;
	}

	return soc;
}

static int bq24192_get_prop_charge_type(struct bq24192_chip *chip)
{
	int chg_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	int ret;
	u8 temp;

	ret = bq24192_read_reg(chip->client, SYSTEM_STATUS_REG, &temp);
	if (ret) {
		pr_err("i2c read fail\n");
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}

	if (temp && FAST_CHARGE_MASK)
		chg_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
	else if (temp && PRE_CHARGE_MASK)
		chg_type = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
	else
		chg_type = POWER_SUPPLY_CHARGE_TYPE_NONE;

	return chg_type;
}

static int bq24192_get_prop_chg_status(struct bq24192_chip *chip)
{
	int chg_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	int chg_status = POWER_SUPPLY_STATUS_UNKNOWN;
	int soc = 0;

	soc = bq24192_get_soc_from_batt_psy(chip);
	chg_type = bq24192_get_prop_charge_type(chip);

	switch (chg_type) {
	case POWER_SUPPLY_CHARGE_TYPE_NONE:
		if (bq24192_is_charger_present(chip)) {
			if (soc >= 100)
				chg_status = POWER_SUPPLY_STATUS_FULL;
			else
				chg_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		} else {
			chg_status = POWER_SUPPLY_STATUS_DISCHARGING;
		}
		break;
	case POWER_SUPPLY_CHARGE_TYPE_TRICKLE:
	case POWER_SUPPLY_CHARGE_TYPE_FAST:
		chg_status = POWER_SUPPLY_STATUS_CHARGING;
		break;
	default:
		break;
	}

	pr_info("chg status = %d soc = %d\n", chg_status, soc);
	return chg_status;
}

static char *bq24192_power_supplied_to[] = {
	"battery",
};

static enum power_supply_property bq24192_power_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
};

static int bq24192_power_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	struct bq24192_chip *chip = container_of(psy,
					struct bq24192_chip,
					ac_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bq24192_get_prop_chg_status(chip);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = POWER_SUPPLY_HEALTH_UNKNOWN;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = chip->chg_current_ma * 1000;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->ac_online;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = bq24192_get_prop_charge_type(chip);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int bq24192_power_set_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	struct bq24192_chip *chip = container_of(psy,
						struct bq24192_chip,
						ac_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
		chip->ac_online = val->intval;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		chip->chg_current_ma = val->intval / 1000;
		break;
	default:
		return -EINVAL;
	}
	power_supply_changed(&chip->ac_psy);
	return 0;
}

static int bq24192_create_debugfs_entries(struct bq24192_chip *chip)
{
	int i;

	chip->dent = debugfs_create_dir(BQ24192_NAME, NULL);
	if (IS_ERR(chip->dent)) {
		pr_err("bq24192 driver couldn't create debugfs dir\n");
		return -EFAULT;
	}

	for (i = 0 ; i < ARRAY_SIZE(bq24192_debug_regs) ; i++) {
		char *name = bq24192_debug_regs[i].name;
		u32 reg = bq24192_debug_regs[i].reg;
		struct dentry *file;

		file = debugfs_create_file(name, 0644, chip->dent,
					(void *) reg, &reg_fops);
		if (IS_ERR(file)) {
			pr_err("debugfs_create_file %s failed.\n", name);
			return -EFAULT;
		}
	}

	return 0;
}

static int bq24192_init_ac_psy(struct bq24192_chip *chip)
{
	int ret = 0;

	chip->ac_psy.name = "ac";
	chip->ac_psy.type = POWER_SUPPLY_TYPE_MAINS;
	chip->ac_psy.supplied_to = bq24192_power_supplied_to;
	chip->ac_psy.num_supplicants = ARRAY_SIZE(bq24192_power_supplied_to);
	chip->ac_psy.properties = bq24192_power_props;
	chip->ac_psy.num_properties = ARRAY_SIZE(bq24192_power_props);
	chip->ac_psy.get_property = bq24192_power_get_property;
	chip->ac_psy.set_property = bq24192_power_set_property;
	chip->ac_psy.external_power_changed =
				bq24192_external_power_changed;

	ret = power_supply_register(&chip->client->dev,
				&chip->ac_psy);
	if (ret) {
		pr_err("failed to register power_supply. ret=%d.\n", ret);
		return ret;
	}

	return 0;
}

static int bq24192_hw_init(struct bq24192_chip *chip)
{
	int ret = 0;

	ret = bq24192_write_reg(chip->client, PWR_ON_CONF_REG,
			RESET_REGISTER_MASK);
	if (ret) {
		pr_err("failed to reset register\n");
		return ret;
	}

	ret = bq24192_set_input_vin_limit(chip, 4360);
	if (ret) {
		pr_err("failed to set input voltage limit\n");
		return ret;
	}

	ret = bq24192_set_input_i_limit(chip,
				INPUT_CURRENT_LIMIT_MAX_MA);
	if (ret) {
		pr_err("failed to set input current limit\n");
		return ret;
	}

	ret = bq24192_set_system_vmin(chip, 3500);
	if (ret) {
		pr_err("failed to set system min voltage\n");
		return ret;
	}

	ret = bq24192_set_ibat_max(chip, chip->chg_current_ma);
	if (ret) {
		pr_err("failed to set charging current\n");
		return ret;
	}

	ret = bq24192_set_prechg_i_limit(chip, 256);
	if (ret) {
		pr_err("failed to set pre-charge current\n");
		return ret;
	}

	ret = bq24192_set_term_current(chip, chip->term_current_ma);
	if (ret) {
		pr_err("failed to set charge termination current\n");
		return ret;
	}

	ret = bq24192_set_vbat_max(chip, 4352);
	if (ret) {
		pr_err("failed to set vbat max\n");
		return ret;
	}

	ret = bq24192_write_reg(chip->client, CHARGE_TERM_TIMER_CONT_REG,
			EN_CHG_TERM_MASK);
	if (ret) {
		pr_err("failed to enable chg termination\n");
		return ret;
	}

	ret = bq24192_set_ir_comp_resister(chip, IRCOMP_R_MAX_MOHM);
	if (ret) {
		pr_err("failed to set ir compensation resister\n");
		return ret;
	}

	ret = bq24192_set_vclamp_mv(chip, IRCOMP_VCLAMP_MAX_MV);
	if (ret) {
		pr_err("failed to set ir vclamp voltage\n");
		return ret;
	}

	return 0;
}

static int bq24192_parse_dt(struct device_node *dev_node,
			   struct bq24192_chip *chip)
{
	int ret = 0;

	chip->int_gpio =
		of_get_named_gpio(dev_node, "ti,int-gpio", 0);
	if (chip->int_gpio < 0) {
		pr_err("failed to get int-gpio.\n");
		ret = chip->int_gpio;
		goto out;
	}

	ret = of_property_read_u32(dev_node, "ti,chg-current-ma",
				   &(chip->chg_current_ma));
	if (ret) {
		pr_err("Unable to read chg_current.\n");
		return ret;
	}
	ret = of_property_read_u32(dev_node, "ti,term-current-ma",
				   &(chip->term_current_ma));
	if (ret) {
		pr_err("Unable to read term_current_ma.\n");
		return ret;
	}

	chip->wlc_support =
			of_property_read_bool(dev_node, "ti,wlc-support");

	pr_info("chg_i_ma = %d term_i_ma = %d\n",
			chip->chg_current_ma,
			chip->term_current_ma);
out:
	return ret;
}

static int bq24192_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	const struct bq24192_platform_data *pdata;
	struct device_node *dev_node = client->dev.of_node;
	struct bq24192_chip *chip;
	int ret = 0;

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("i2c func fail.\n");
		return -EIO;
	}

	chip = kzalloc(sizeof(struct bq24192_chip), GFP_KERNEL);
	if (!chip) {
		pr_err("failed to alloc memory\n");
		return -ENOMEM;
	}

	chip->client = client;

	chip->usb_psy = power_supply_get_by_name("usb");
	if (!chip->usb_psy) {
		pr_err("usb supply not found deferring probe\n");
		ret = -EPROBE_DEFER;
		goto error;
	}

	if (dev_node) {
		ret = bq24192_parse_dt(dev_node, chip);
		if (ret) {
			pr_err("failed to parse dt\n");
			goto error;
		}
	} else {
		pdata = client->dev.platform_data;
		if (pdata == NULL) {
			pr_err("no platform data.\n");
			return -EINVAL;
		}

		chip->int_gpio = pdata->int_gpio;
		chip->chg_current_ma = pdata->chg_current_ma;
		chip->term_current_ma = pdata->term_current_ma;
		chip->wlc_support = pdata->wlc_support;
	}

	if (chip->wlc_support) {
		chip->wlc_psy = power_supply_get_by_name("wireless");
		if (!chip->wlc_psy) {
			pr_err("wireless supply not found deferring probe\n");
			ret = -EPROBE_DEFER;
			goto error;
		}
	}

	ret =  gpio_request_one(chip->int_gpio, GPIOF_DIR_IN,
			"bq24192_int");
	if (ret) {
		pr_err("failed to request int_gpio\n");
		goto error;
	}

	chip->irq = gpio_to_irq(chip->int_gpio);

	i2c_set_clientdata(client, chip);

	ret = bq24192_hw_init(chip);
	if (ret) {
		pr_err("failed to init hw\n");
		goto err_hw_init;
	}

	the_chip = chip;

	ret = bq24192_init_ac_psy(chip);
	if (ret) {
		pr_err("bq24192_init_ac_psy failed\n");
		goto err_hw_init;
	}

	ret = bq24192_create_debugfs_entries(chip);
	if (ret) {
		pr_err("bq24192_create_debugfs_entries failed\n");
		goto err_debugfs;
	}

	INIT_DELAYED_WORK(&chip->irq_work, bq24192_irq_worker);
	if (chip->irq) {
		ret = request_irq(chip->irq, bq24192_irq,
				IRQF_TRIGGER_FALLING,
				"bq24192_irq", chip);
		if (ret) {
			pr_err("request_irq %d failed\n", chip->irq);
			goto err_req_irq;
		}
		enable_irq_wake(chip->irq);
	}

	bq24192_enable_charging(chip, true);
	schedule_delayed_work(&chip->irq_work, msecs_to_jiffies(2000));
	pr_info("probe success\n");

	return 0;

err_req_irq:
	if (chip->dent)
		debugfs_remove_recursive(chip->dent);
err_debugfs:
	power_supply_unregister(&chip->ac_psy);
err_hw_init:
	if (chip->int_gpio)
		gpio_free(chip->int_gpio);
error:
	kfree(chip);
	pr_info("fail to probe\n");
	return ret;

}

static int bq24192_remove(struct i2c_client *client)
{
	struct bq24192_chip *chip = i2c_get_clientdata(client);

	if (chip->irq)
		free_irq(chip->irq, chip);
	if (chip->dent)
		debugfs_remove_recursive(chip->dent);
	power_supply_unregister(&chip->ac_psy);
	if (chip->int_gpio)
		gpio_free(chip->int_gpio);
	kfree(chip);

	return 0;
}

static const struct i2c_device_id bq24192_id[] = {
	{BQ24192_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, bq24192_id);

static const struct of_device_id bq24192_match[] = {
	{ .compatible = "ti,bq24192-charger", },
	{ },
};

static struct i2c_driver bq24192_driver = {
	.driver	= {
			.name	= BQ24192_NAME,
			.owner	= THIS_MODULE,
			.of_match_table = of_match_ptr(bq24192_match),
	},
	.probe		= bq24192_probe,
	.remove		= bq24192_remove,
	.id_table	= bq24192_id,
};

static int __init bq24192_init(void)
{
	return i2c_add_driver(&bq24192_driver);
}
module_init(bq24192_init);

static void __exit bq24192_exit(void)
{
	return i2c_del_driver(&bq24192_driver);
}
module_exit(bq24192_exit);

MODULE_AUTHOR("ChoongRyeol Lee <choongryeol.lee@lge.com>");
MODULE_DESCRIPTION("BQ24192 Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:" BQ24192_NAME);
