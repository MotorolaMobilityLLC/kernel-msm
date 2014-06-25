/* Copyright (c) 2014 Motorola Mobility LLC. All rights reserved.
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



/* Mask/Bit helpers */
#define GENMASK(u, l) (((1 << ((u) - (l) + 1)) - 1) << (l))

/* CONTROL0 Register */
#define REG_CONTROL0         0x00
#define CONTROL0_TMR_RST     BIT(7)
#define CONTROL0_EN_STAT     BIT(6)
#define CONTROL0_STAT        GENMASK(5, 4)
#define CONTROL0_STAT_SHIFT  4
#define STAT_READY           0x00
#define STAT_PWM_ENABLED     0x01
#define STAT_CHARGE_DONE     0x02
#define STAT_FAULT           0x03
#define CONTROL0_BOOST       BIT(3)
#define CONTROL0_FAULT       GENMASK(2, 0)
#define CONTROL0_FAULT_SHIFT 0
#define FAULT_NONE           0x00
#define FAULT_VBUS_OVP       0x01
#define FAULT_SLEEP_MODE     0x02
#define FAULT_POOR_INPUT     0x03
#define FAULT_BATT_OVP       0x04
#define FAULT_THERM_SHUTDOWN 0x05
#define FAULT_TIMER_FAULT    0x06
#define FAULT_NO_BATTERY     0x07

/* CONTROL1 Register */
#define REG_CONTROL1           0x01
#define CONTROL1_IBUSLIM       GENMASK(7, 6)
#define CONTROL1_IBUSLIM_SHIFT 6
#define IBUSLIM_100MA          0x00
#define IBUSLIM_500MA          0x01
#define IBUSLIM_800MA          0x02
#define IBUSLIM_NO_LIMIT       0x03
#define CONTROL1_VLOWV         GENMASK(5, 4)
#define CONTROL1_VLOWV_SHIFT   4
#define VLOWV_3_4V             0
#define VLOWV_3_5V             1
#define VLOWV_3_6V             2
#define VLOWV_3_7V             3
#define CONTROL1_TE            BIT(3)
#define CONTROL1_CE_N          BIT(2)
#define CONTROL1_HZ_MODE       BIT(1)
#define CONTROL1_OPA_MODE      BIT(0)

/* OREG Register */
#define REG_OREG              0x02
#define OREG_OREG             GENMASK(7, 2)
#define OREG_OREG_SHIFT       2
#define OREG_DBAT_B           BIT(1)
#define OREG_EOC              BIT(0)

/* IC INFO Register */
#define REG_IC_INFO           0x03
#define IC_INFO_VENDOR_CODE   GENMASK(7, 6)
#define VENDOR_FAIRCHILD_VAL  0x80
#define IC_INFO_PN            GENMASK(5, 3)
#define IC_INFO_PN_SHIFT      3
#define PN_FAN54040_VAL       0x00
#define PN_FAN54041_VAL       0x08
#define PN_FAN54042_VAL       0x10
#define PN_FAN54045_VAL       0x28
#define PN_FAN54046_VAL       0x30
#define PN_FAN54047_VAL       0x30 /* Spec correct? Same as 54046... */
#define IC_INFO_REV           GENMASK(2, 0)
#define IC_INFO_REV_SHIFT     0

/* IBAT Register */
#define REG_IBAT              0x04
#define IBAT_RESET            BIT(7)
#define IBAT_IOCHARGE         GENMASK(6, 3)
#define IBAT_IOCHARGE_SHIFT   3
#define IBAT_ITERM            GENMASK(2, 0)
#define IBAT_ITERM_SHIFT      0

/* VBUS CONTROL Register */
#define REG_VBUS_CONTROL      0x05
#define VBUS_PROD             BIT(6)
#define VBUS_IO_LEVEL         BIT(5)
#define VBUS_VBUS_CON         BIT(4)
#define VBUS_SP               BIT(3)
#define VBUS_VBUSLIM          GENMASK(2, 0)
#define VBUS_VBUSLIM_SHIFT    0

/* SAFETY Register */
#define REG_SAFETY            0x06
#define SAFETY_ISAFE          GENMASK(7, 4)
#define SAFETY_ISAFE_SHIFT    4
#define SAFETY_VSAFE          GENMASK(3, 0)
#define SAFETY_VSAFE_SHIFT    0

/* POST CHARGING Register */
#define REG_POST_CHARGING     0x07
#define PC_BDET               GENMASK(7, 6)
#define PC_BDET_SHIFT         6
#define PC_VBUS_LOAD          GENMASK(5, 4)
#define PC_VBUS_LOAD_SHIFT    4
#define PC_PC_EN              BIT(3)
#define PC_PC_IT              GENMASK(2, 0)
#define PC_PC_IT_SHIFT        0

/* MONITOR0 Register */
#define REG_MONITOR0          0x10
#define MONITOR0_ITERM_CMP    BIT(7)
#define MONITOR0_VBAT_CMP     BIT(6)
#define MONITOR0_LINCHG       BIT(5)
#define MONITOR0_T_120        BIT(4)
#define MONITOR0_ICHG         BIT(3)
#define MONITOR0_IBUS         BIT(2)
#define MONITOR0_VBUS_VALID   BIT(1)
#define MONITOR0_CV           BIT(0)

/* MONITOR1 Register */
#define REG_MONITOR1          0x11
#define MONITOR1_GATE         BIT(7)
#define MONITOR1_VBAT         BIT(6)
#define MONITOR1_POK_B        BIT(5)
#define MONITOR1_DIS_LEVEL    BIT(4)
#define MONITOR1_NOBAT        BIT(3)
#define MONITOR1_PC_ON        BIT(2)

/* NTC Register */
#define REG_NTC               0x12

/* WD CONTROL Register */
#define REG_WD_CONTROL        0x13
#define WD_CONTROL_EN_VREG    BIT(2)
#define WD_CONTROL_WD_DIS     BIT(1)

/* REG RESTART Register */
#define REG_RESTART           0xFA

static char *version_str[] = {
	[0] = "fan54040",
	[1] = "fan54041",
	[2] = "fan54042",
	[3] = "unknown",
	[4] = "unknown",
	[5] = "fan54045",
	[6] = "fan54046",
	[7] = "fan54047",

};

struct fan5404x_chg {
	struct i2c_client *client;
	struct device *dev;
	struct mutex read_write_lock;

	struct power_supply *usb_psy;
	struct power_supply batt_psy;
	struct power_supply *bms_psy;
	const char *bms_psy_name;

	int fake_battery_soc;

	bool factory_mode;
	bool chg_enabled;
	bool usb_present;
	bool batt_present;
	bool chg_done_batt_full;
};

static int __fan5404x_read(struct fan5404x_chg *chip, int reg,
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

static int __fan5404x_write(struct fan5404x_chg *chip, int reg,
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

static int fan5404x_read(struct fan5404x_chg *chip, int reg,
				uint8_t *val)
{
	int rc;

	mutex_lock(&chip->read_write_lock);
	rc = __fan5404x_read(chip, reg, val);
	mutex_unlock(&chip->read_write_lock);

	return rc;
}

static int fan5404x_masked_write(struct fan5404x_chg *chip, int reg,
						uint8_t mask, uint8_t val)
{
	int rc;
	uint8_t temp;

	mutex_lock(&chip->read_write_lock);
	rc = __fan5404x_read(chip, reg, &temp);
	if (rc < 0) {
		dev_err(chip->dev, "read failed: reg=%03X, rc=%d\n", reg, rc);
		goto out;
	}
	temp &= ~mask;
	temp |= val & mask;
	rc = __fan5404x_write(chip, reg, temp);
	if (rc < 0) {
		dev_err(chip->dev,
			"write failed: reg=%03X, rc=%d\n", reg, rc);
	}
out:
	mutex_unlock(&chip->read_write_lock);
	return rc;
}

static int fan5404x_stat_read(struct fan5404x_chg *chip)
{
	int rc;
	uint8_t reg;

	rc = fan5404x_read(chip, REG_CONTROL0, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read STAT rc = %d\n", rc);
		return rc;
	}

	return (reg & CONTROL0_STAT) >> CONTROL0_STAT_SHIFT;
}

static int fan5404x_fault_read(struct fan5404x_chg *chip)
{
	int rc;
	uint8_t reg;

	rc = fan5404x_read(chip, REG_CONTROL0, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read STAT rc = %d\n", rc);
		return rc;
	}

	return (reg & CONTROL0_FAULT) >> CONTROL0_FAULT_SHIFT;
}

static irqreturn_t fan5404x_chg_stat_handler(int irq, void *dev_id)
{
	struct fan5404x_chg *chip = dev_id;
	int stat;
	int fault;

	stat = fan5404x_stat_read(chip);
	fault = fan5404x_fault_read(chip);

	pr_debug("CONTROL0.STAT: %X CONTROL0.FAULT: %X\n", stat, fault);

	power_supply_changed(&chip->batt_psy);

	return IRQ_HANDLED;
}

#define OREG_MIN      3500
#define OREG_STEP_MV  20
#define OREG_STEPS    48
#define OREG_VALUE(s) (OREG_MIN + s*OREG_STEP_MV)
static int fan5404x_set_oreg(struct fan5404x_chg *chip, int value)
{
	int i;
	int rc;

	for (i = OREG_STEPS; i >= 0; i--)
		if (value >= OREG_VALUE(i))
			break;

	if (i < 0)
		return -EINVAL;

	rc = fan5404x_masked_write(chip, REG_OREG, OREG_OREG,
					i << OREG_OREG_SHIFT);
	if (rc) {
		dev_err(chip->dev, "Failed to set OREG_OREG: %d\n", rc);
		return rc;
	}

	return 0;
}

static int ibuslim_vals[] = {
	[IBUSLIM_100MA] = 100,
	[IBUSLIM_500MA] = 500,
	[IBUSLIM_800MA] = 800,
	[IBUSLIM_100MA] = INT_MAX
};

static int fan5404x_set_ibuslim(struct fan5404x_chg *chip,
				int limit)
{
	int i;
	int rc;

	for (i = ARRAY_SIZE(ibuslim_vals) - 1; i >= 0; i--)
		if (limit >= ibuslim_vals[i])
			break;

	if (i < 0)
		return -EINVAL;

	rc = fan5404x_masked_write(chip, REG_CONTROL1, CONTROL1_IBUSLIM,
					i << CONTROL1_IBUSLIM_SHIFT);
	if (rc) {
		dev_err(chip->dev, "Failed to set IBUSLIM: %d\n", rc);
		return rc;
	}

	return 0;
}

#define IBAT_IOCHARGE_MIN     550
#define IBAT_IOCHARGE_STEP_MA 100
#define IBAT_IOCHARGE_STEPS   11
#define IBAT_STEP_CURRENT(s)  (IBAT_IOCHARGE_MIN + s*IBAT_IOCHARGE_STEP_MA)
static int fan5404x_set_iocharge(struct fan5404x_chg *chip,
				int limit)
{
	int i;
	int rc;

	for (i = IBAT_IOCHARGE_STEPS; i >= 0; i--)
		if (limit >= IBAT_STEP_CURRENT(i))
			break;

	if (i < 0)
		return -EINVAL;

	/* Need to keep RESET low... */
	rc = fan5404x_masked_write(chip, REG_IBAT, IBAT_IOCHARGE | IBAT_RESET,
						i << IBAT_IOCHARGE_SHIFT);
	if (rc) {
		dev_err(chip->dev, "Failed to set IOCHARGE: %d\n", rc);
		return rc;
	}

	return 0;
}

static int start_charging(struct fan5404x_chg *chip)
{
	union power_supply_propval prop = {0,};
	int rc;
	int current_limit = 0;

	dev_dbg(chip->dev, "starting to charge...\n");

	rc = chip->usb_psy->get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_CURRENT_MAX, &prop);
	if (rc < 0) {
		dev_err(chip->dev,
			"could not read USB current_max property, rc=%d\n", rc);
		return rc;
	}

	current_limit = prop.intval / 1000;
	rc = fan5404x_set_ibuslim(chip, current_limit);
	if (rc)
		return rc;

	/* Set IOCHARGE */
	rc = fan5404x_set_iocharge(chip, 550);
	if (rc)
		return rc;

	/* Clear IO_LEVEL */
	rc = fan5404x_masked_write(chip, REG_VBUS_CONTROL,
					VBUS_IO_LEVEL, VBUS_IO_LEVEL);
	if (rc) {
		dev_err(chip->dev, "start-charge: Couldn't clear IOLEVEL\n");
		return rc;
	}

	/* Set OREG to 4.2V */
	rc = fan5404x_set_oreg(chip, 4200);
	if (rc)
		return rc;

	/* Set CE# Low, TE High */
	rc = fan5404x_masked_write(chip, REG_CONTROL1,
				CONTROL1_TE | CONTROL1_CE_N, CONTROL1_TE);
	if (rc) {
		dev_err(chip->dev, "start-charge: Failed to set TE/CE_N\n");
		return rc;
	}

	return 0;
}

static int stop_charging(struct fan5404x_chg *chip)
{
	int rc;

	/* Set CE# High, TE Low */
	rc = fan5404x_masked_write(chip, REG_CONTROL1,
				CONTROL1_CE_N | CONTROL1_CE_N, CONTROL1_TE);
	if (rc) {
		dev_err(chip->dev, "stop-charge: Failed to set TE/CE_N\n");
		return rc;
	}

	return 0;
}

static void fan5404x_external_power_changed(struct power_supply *psy)
{
	struct fan5404x_chg *chip = container_of(psy,
				struct fan5404x_chg, batt_psy);
	union power_supply_propval prop = {0,};
	int rc;

	if (chip->bms_psy_name && !chip->bms_psy)
		chip->bms_psy = power_supply_get_by_name(
					(char *)chip->bms_psy_name);

	rc = chip->usb_psy->get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_PRESENT, &prop);
	pr_debug("External Power Changed: usb=%d\n", prop.intval);

	chip->usb_present = prop.intval;
	if (chip->usb_present)
		start_charging(chip);
	else
		stop_charging(chip);

}

static enum power_supply_property fan5404x_batt_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TECHNOLOGY,
};

static int fan5404x_get_prop_batt_status(struct fan5404x_chg *chip)
{
	int rc;
	int stat_reg;
	uint8_t ctrl1;

	if (chip->fake_battery_soc >= 0)
		return chip->fake_battery_soc;

	stat_reg = fan5404x_stat_read(chip);
	if (stat_reg < 0) {
		dev_err(chip->dev, "Fail read STAT bits, rc = %d\n", stat_reg);
		return POWER_SUPPLY_STATUS_UNKNOWN;
	}

	if (stat_reg == STAT_CHARGE_DONE)
		return POWER_SUPPLY_STATUS_FULL;

	rc = fan5404x_read(chip, REG_CONTROL1, &ctrl1);
	if (rc < 0) {
		dev_err(chip->dev, "Unable to read REG_CONTROL1 rc = %d\n", rc);
		return POWER_SUPPLY_STATUS_UNKNOWN;
	}

	if (stat_reg == STAT_PWM_ENABLED && !(ctrl1 & CONTROL1_CE_N))
		return POWER_SUPPLY_STATUS_CHARGING;

	return POWER_SUPPLY_STATUS_DISCHARGING;
}

static int fan5404x_get_prop_batt_present(struct fan5404x_chg *chip)
{
	int rc;
	uint8_t reg;

	rc = fan5404x_read(chip, REG_MONITOR1, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read monitor1 rc = %d\n", rc);
		return 0;
	}

	if (reg & MONITOR1_NOBAT)
		return 0;

	return 1;
}

static int fan5404x_get_prop_charge_type(struct fan5404x_chg *chip)
{
	int rc;
	int stat_reg;
	uint8_t ctrl1;
	uint8_t mon0;

	stat_reg = fan5404x_stat_read(chip);
	if (stat_reg < 0) {
		dev_err(chip->dev, "Fail read STAT bits, rc = %d\n", stat_reg);
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}

	rc = fan5404x_read(chip, REG_MONITOR0, &mon0);
	if (rc < 0) {
		dev_err(chip->dev, "Unable to read REG_MONITOR0 rc = %d\n", rc);
		return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
	}

	if (mon0 & MONITOR0_LINCHG)
		return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;

	rc = fan5404x_read(chip, REG_CONTROL1, &ctrl1);
	if (rc < 0) {
		dev_err(chip->dev, "Unable to read REG_CONTROL1 rc = %d\n", rc);
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}

	if (stat_reg == STAT_PWM_ENABLED && !(ctrl1 & CONTROL1_CE_N))
		return POWER_SUPPLY_CHARGE_TYPE_FAST;

	return POWER_SUPPLY_CHARGE_TYPE_NONE;
}

#define DEFAULT_BATT_CAPACITY 50
static int fan5404x_get_prop_batt_capacity(struct fan5404x_chg *chip)
{
	union power_supply_propval ret = {0, };
	int cap = DEFAULT_BATT_CAPACITY;
	int rc;

	if (chip->fake_battery_soc >= 0)
		return chip->fake_battery_soc;

	if (!chip->bms_psy && chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);

	if (chip->bms_psy) {
		rc = chip->bms_psy->get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_CAPACITY, &ret);
		if (rc)
			dev_err(chip->dev, "Couldn't get batt capacity\n");
		else
			cap = ret.intval;
	}

	return cap;
}

static int fan5404x_batt_set_property(struct power_supply *psy,
					enum power_supply_property prop,
					const union power_supply_propval *val)
{
	struct fan5404x_chg *chip = container_of(psy,
				struct fan5404x_chg, batt_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		/* TODO */
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		chip->fake_battery_soc = val->intval;
		power_supply_changed(&chip->batt_psy);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int fan5404x_batt_is_writeable(struct power_supply *psy,
					enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}

	return rc;
}

static int fan5404x_batt_get_property(struct power_supply *psy,
					enum power_supply_property prop,
					union power_supply_propval *val)
{
	struct fan5404x_chg *chip = container_of(psy,
				struct fan5404x_chg, batt_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = fan5404x_get_prop_batt_status(chip);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = fan5404x_get_prop_batt_present(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = chip->chg_enabled;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = fan5404x_get_prop_charge_type(chip);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = fan5404x_get_prop_batt_capacity(chip);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int fan5404x_of_init(struct fan5404x_chg *chip)
{
	int rc;
	struct device_node *node = chip->dev->of_node;

	rc = of_property_read_string(node, "fairchild,bms-psy-name",
						&chip->bms_psy_name);
	if (rc)
		chip->bms_psy_name = NULL;

	return 0;
}

static int fan5404x_hw_init(struct fan5404x_chg *chip)
{
	int rc;

	/* Disable T32 */
	rc = fan5404x_masked_write(chip, REG_WD_CONTROL, WD_CONTROL_WD_DIS,
							WD_CONTROL_WD_DIS);
	if (rc) {
		dev_err(chip->dev, "couldn't disable T32 rc = %d\n", rc);
		return rc;
	}

	return 0;
}

static int determine_initial_status(struct fan5404x_chg *chip)
{
	int rc;
	union power_supply_propval prop = {0,};
	uint8_t reg;

	chip->batt_present = true;
	rc = fan5404x_read(chip, REG_MONITOR1, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read monitor1 rc = %d\n", rc);
		return rc;
	}

	if (reg & MONITOR1_NOBAT)
		chip->batt_present = false;

	if (fan5404x_stat_read(chip) == STAT_CHARGE_DONE)
		chip->chg_done_batt_full = true;

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

static int fan5404x_read_chip_id(struct fan5404x_chg *chip, uint8_t *val)
{
	int rc;

	rc = fan5404x_read(chip, REG_IC_INFO, val);
	if (rc)
		return rc;

	if ((*val & IC_INFO_VENDOR_CODE) != VENDOR_FAIRCHILD_VAL) {
		dev_err(chip->dev, "Unknown vendor IC_INFO: %.2X\n", *val);
		return -EINVAL;
	}

	dev_dbg(chip->dev, "Found PN: %s Revision: 1.%d\n",
		version_str[(*val & IC_INFO_PN) >> IC_INFO_PN_SHIFT],
		*val & IC_INFO_REV);

	return 0;
}

static bool fan5404x_charger_mmi_factory(void)
{
	struct device_node *np = of_find_node_by_path("/chosen");
	bool factory = false;

	if (np)
		factory = of_property_read_bool(np, "mmi,factory-cable");

	of_node_put(np);

	return factory;
}

static struct of_device_id fan5404x_match_table[] = {
	{ .compatible = "fairchild,fan54046-charger", },
	{ },
};

static int fan5404x_charger_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int rc;
	struct fan5404x_chg *chip;
	struct power_supply *usb_psy;
	uint8_t reg;

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
	chip->fake_battery_soc = -EINVAL;

	mutex_init(&chip->read_write_lock);

	rc = fan5404x_read_chip_id(chip, &reg);
	if (rc) {
		dev_err(&client->dev, "Could not read from FAN5404x: %d\n", rc);
		return -ENODEV;
	}

	i2c_set_clientdata(client, chip);

	chip->batt_psy.name = "battery";
	chip->batt_psy.type = POWER_SUPPLY_TYPE_BATTERY;
	chip->batt_psy.get_property = fan5404x_batt_get_property;
	chip->batt_psy.set_property = fan5404x_batt_set_property;
	chip->batt_psy.properties = fan5404x_batt_properties;
	chip->batt_psy.num_properties = ARRAY_SIZE(fan5404x_batt_properties);
	chip->batt_psy.external_power_changed = fan5404x_external_power_changed;
	chip->batt_psy.property_is_writeable = fan5404x_batt_is_writeable;

	rc = power_supply_register(chip->dev, &chip->batt_psy);
	if (rc < 0) {
		dev_err(&client->dev,
			"Unable to register batt_psy rc = %d\n", rc);
		return rc;
	}

	chip->factory_mode = fan5404x_charger_mmi_factory();
	if (chip->factory_mode)
		dev_info(&client->dev, "Factory Mode: writes disabled\n");

	rc = fan5404x_of_init(chip);
	if (rc) {
		dev_err(&client->dev, "Couldn't initialize OF DT values\n");
		goto unregister_batt_psy;
	}

	fan5404x_hw_init(chip);

	determine_initial_status(chip);

	/* STAT irq configuration */
	if (client->irq) {
		rc = devm_request_threaded_irq(&client->dev, client->irq, NULL,
				fan5404x_chg_stat_handler,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING |
				IRQF_ONESHOT,
				"fan5404x_chg_stat_irq", chip);
		if (rc < 0) {
			dev_err(&client->dev,
				"request_irq for irq=%d  failed rc = %d\n",
				client->irq, rc);
			goto unregister_batt_psy;
		}
		enable_irq_wake(client->irq);
	}

	dev_dbg(&client->dev, "FAN5404X batt=%d usb=%d done=%d\n",
			chip->batt_present, chip->usb_present,
			chip->chg_done_batt_full);

	return 0;

unregister_batt_psy:
	power_supply_unregister(&chip->batt_psy);

	return rc;
}

static int fan5404x_charger_remove(struct i2c_client *client)
{
	struct fan5404x_chg *chip = i2c_get_clientdata(client);

	power_supply_unregister(&chip->batt_psy);

	return 0;
}

static int fan5404x_suspend(struct device *dev)
{
	return 0;
}

static int fan5404x_suspend_noirq(struct device *dev)
{
	return 0;
}

static int fan5404x_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops fan5404x_pm_ops = {
	.resume		= fan5404x_resume,
	.suspend_noirq	= fan5404x_suspend_noirq,
	.suspend	= fan5404x_suspend,
};

static const struct i2c_device_id fan5404x_charger_id[] = {
	{"fan5404x-charger", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, fan5404x_charger_id);

static struct i2c_driver fan5404x_charger_driver = {
	.driver		= {
		.name		= "fan5404x-charger",
		.owner		= THIS_MODULE,
		.of_match_table	= fan5404x_match_table,
		.pm		= &fan5404x_pm_ops,
	},
	.probe		= fan5404x_charger_probe,
	.remove		= fan5404x_charger_remove,
	.id_table	= fan5404x_charger_id,
};

module_i2c_driver(fan5404x_charger_driver);

MODULE_DESCRIPTION("FAN5404x Charger");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:fan5404x-charger");

