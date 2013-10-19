/*
 *  max17048_battery.c
 *  fuel-gauge systems for lithium-ion (Li+) batteries
 *
 *  Copyright (C) 2012 Nvidia Cooperation
 *  Chandler Zhang <chazhang@nvidia.com>
 *  Syed Rafiuddin <srafiuddin@nvidia.com>
 *
 *  Copyright (C) 2013 LGE Inc.
 *  ChoongRyeol Lee <choongryeol.lee@lge.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/power/max17048_battery.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/debugfs.h>
#include <linux/suspend.h>
#include <linux/reboot.h>
#include <linux/syscalls.h>

#define MODE_REG      0x06
#define VCELL_REG     0x02
#define SOC_REG       0x04
#define VERSION_REG   0x08
#define HIBRT_REG     0x0A
#define CONFIG_REG    0x0C
#define VALRT_REG     0x14
#define CRATE_REG     0x16
#define VRESET_REG    0x18
#define STATUS_REG    0x1A

#define CFG_ALRT_MASK    0x0020
#define CFG_ATHD_MASK    0x001F
#define CFG_ALSC_MASK    0x0040
#define CFG_RCOMP_MASK    0xFF00
#define CFG_RCOMP_SHIFT    8
#define CFG_ALSC_SHIFT   6
#define STAT_RI_MASK     0x0100
#define STAT_CLEAR_MASK  0xFF00

#define MAX17048_VERSION_11    0x11
#define MAX17048_VERSION_12    0x12

struct max17048_chip {
	struct i2c_client *client;
	struct power_supply batt_psy;
	struct power_supply *ac_psy;
	struct max17048_platform_data *pdata;
	struct dentry *dent;
	struct notifier_block pm_notifier;
	struct delayed_work monitor_work;
	int vcell;
	int soc;
	int capacity_level;
	struct wake_lock alert_lock;
	int alert_gpio;
	int alert_irq;
	int rcomp;
	int rcomp_co_hot;
	int rcomp_co_cold;
	int alert_threshold;
	int max_mvolt;
	int min_mvolt;
	int full_soc;
	int empty_soc;
	int batt_tech;
	int fcc_mah;
	int voltage;
	int lasttime_capacity_level;
	int chg_state;
	int batt_temp;
	int batt_health;
	int batt_current;
	int uvlo_thr_mv;
	int poll_interval_ms;
};

static struct max17048_chip *ref;
struct completion monitor_work_done;
static int max17048_get_prop_current(struct max17048_chip *chip);
static int max17048_get_prop_temp(struct max17048_chip *chip);
static int max17048_clear_interrupt(struct max17048_chip *chip);
static int max17048_get_prop_status(struct max17048_chip *chip);

struct debug_reg {
	char  *name;
	u8  reg;
};

#define MAX17048_DEBUG_REG(x) {#x, x##_REG}

static struct debug_reg max17048_debug_regs[] = {
	MAX17048_DEBUG_REG(MODE),
	MAX17048_DEBUG_REG(VCELL),
	MAX17048_DEBUG_REG(SOC),
	MAX17048_DEBUG_REG(VERSION),
	MAX17048_DEBUG_REG(HIBRT),
	MAX17048_DEBUG_REG(CONFIG),
	MAX17048_DEBUG_REG(VALRT),
	MAX17048_DEBUG_REG(CRATE),
	MAX17048_DEBUG_REG(VRESET),
	MAX17048_DEBUG_REG(STATUS),
};

static int bound_check(int max, int min, int val)
{
	val = max(min, val);
	val = min(max, val);
	return val;
}

static int max17048_write_word(struct i2c_client *client, int reg, u16 value)
{
	int ret;

	ret = i2c_smbus_write_word_data(client, reg, swab16(value));
	if (ret < 0)
		dev_err(&client->dev, "%s(): Failed in writing register"
					"0x%02x err %d\n", __func__, reg, ret);

	return ret;
}

static int max17048_read_word(struct i2c_client *client, int reg)
{
	int ret;

	ret = i2c_smbus_read_word_data(client, reg);
	if (ret < 0)
		dev_err(&client->dev, "%s(): Failed in reading register"
					"0x%02x err %d\n", __func__, reg, ret);
	else
		ret = (int)swab16((uint16_t)(ret & 0x0000ffff));

	return ret;
}

static int max17048_masked_write_word(struct i2c_client *client, int reg,
			       u16 mask, u16 val)
{
	s32 rc;
	u16 temp;

	temp = max17048_read_word(client, reg);
	if (temp < 0) {
		pr_err("max17048_read_word failed: reg=%03X, rc=%d\n",
				reg, temp);
		return temp;
	}

	if ((temp & mask) == (val & mask))
		return 0;

	temp &= ~mask;
	temp |= val & mask;
	rc = max17048_write_word(client, reg, temp);
	if (rc) {
		pr_err("max17048_write_word failed: reg=%03X, rc=%d\n",
				reg, rc);
		return rc;
	}

	return 0;
}

static int set_reg(void *data, u64 val)
{
	u32 addr = (u32) data;
	int ret;
	struct i2c_client *client = ref->client;

	ret = max17048_write_word(client, addr, (u16) val);

	return ret;
}

static int get_reg(void *data, u64 *val)
{
	u32 addr = (u32) data;
	int ret;
	struct i2c_client *client = ref->client;

	ret = max17048_read_word(client, addr);
	if (ret < 0)
		return ret;

	*val = ret;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(reg_fops, get_reg, set_reg, "0x%02llx\n");

/* Using Quickstart instead of reset for Power Test
*  DO NOT USE THIS COMMAND ANOTHER SCENE.
*/
static int max17048_set_reset(struct max17048_chip *chip)
{
	max17048_write_word(chip->client, MODE_REG, 0x4000);
	pr_info("%s: Reset (Quickstart)\n", __func__);
	return 0;
}

static int max17048_get_capacity_from_soc(struct max17048_chip *chip)
{
	u8 buf[2];
	int batt_soc = 0;

	buf[0] = (chip->soc & 0x0000FF00) >> 8;
	buf[1] = (chip->soc & 0x000000FF);

	pr_debug("%s: SOC raw = 0x%x%x\n", __func__, buf[0], buf[1]);

	batt_soc = (((int)buf[0]*256)+buf[1])*19531; /* 0.001953125 */
	batt_soc = (batt_soc - (chip->empty_soc * 1000000))
			/ ((chip->full_soc - chip->empty_soc) * 10000);

	batt_soc = bound_check(100, 0, batt_soc);

	return batt_soc;
}

static int max17048_get_vcell(struct max17048_chip *chip)
{
	int vcell;

	vcell = max17048_read_word(chip->client, VCELL_REG);
	if (vcell < 0) {
		pr_err("%s: err %d\n", __func__, vcell);
		return vcell;
	} else {
		chip->vcell = vcell >> 4;
		chip->voltage = (chip->vcell * 5) >> 2;
	}

	return 0;
}

static int max17048_check_recharge(struct max17048_chip *chip)
{
	union power_supply_propval ret = {true,};
	int chg_status;

	if (chip->batt_health != POWER_SUPPLY_HEALTH_GOOD)
		return false;

	chg_status = max17048_get_prop_status(chip);
	if (chip->capacity_level <= 99 &&
			chg_status == POWER_SUPPLY_STATUS_NOT_CHARGING) {
		chip->ac_psy->set_property(chip->ac_psy,
				POWER_SUPPLY_PROP_CHARGING_ENABLED,
				&ret);
		return true;
	}

	return false;
}

static int max17048_get_soc(struct max17048_chip *chip)
{
	int soc;

	soc = max17048_read_word(chip->client, SOC_REG);
	if (soc < 0) {
		pr_err("%s: err %d\n", __func__, soc);
		return soc;
	} else {
		chip->soc = soc;
		chip->capacity_level =
			max17048_get_capacity_from_soc(chip);
	}

	return 0;
}

static uint16_t max17048_get_version(struct max17048_chip *chip)
{
	return (uint16_t) max17048_read_word(chip->client, VERSION_REG);
}

static int max17048_set_rcomp(struct max17048_chip *chip)
{
	int ret;
	int scale_coeff;
	int rcomp;
	int temp;

	temp = chip->batt_temp / 10;

	if (temp > 20)
		scale_coeff = chip->rcomp_co_hot;
	else if (temp < 20)
		scale_coeff = chip->rcomp_co_cold;
	else
		scale_coeff = 0;

	rcomp = chip->rcomp * 1000 - (temp-20) * scale_coeff;
	rcomp = bound_check(255, 0, rcomp / 1000);

	pr_debug("%s: new RCOMP = 0x%02X\n", __func__, rcomp);

	rcomp = rcomp << CFG_RCOMP_SHIFT;

	ret = max17048_masked_write_word(chip->client,
			CONFIG_REG, CFG_RCOMP_MASK, rcomp);
	if (ret < 0)
		pr_err("%s: failed to set rcomp\n", __func__);

	return ret;
}

#define UVLO_COUNT 3
#define UVLO_FAST_POLL_MS 1000
#define UVLO_NORMAL_POLL_MS 10000
#define UVLO_INIT_POLL_MS 25000
static void max17048_check_low_vbatt(struct max17048_chip *chip)
{
	static int uvlo_cnt = 0;

	if (system_state == SYSTEM_BOOTING) {
		chip->poll_interval_ms = UVLO_INIT_POLL_MS;
		return;
	}

	if (chip->voltage < chip->uvlo_thr_mv) {
		chip->poll_interval_ms = UVLO_FAST_POLL_MS;
		uvlo_cnt ++;
	} else {
		chip->poll_interval_ms = UVLO_NORMAL_POLL_MS;
		uvlo_cnt = 0;
	}

	if (uvlo_cnt >= UVLO_COUNT) {
		pr_info("%s: power down by very low battery\n", __func__);
		sys_sync();
		kernel_power_off();
	}
}

static void max17048_work(struct work_struct *work)
{
	struct max17048_chip *chip =
		container_of(work, struct max17048_chip, monitor_work.work);
	int ret = 0;
	int rechg_now;

	wake_lock(&chip->alert_lock);

	pr_debug("%s.\n", __func__);

	max17048_get_prop_temp(chip);
	max17048_get_prop_current(chip);

	ret = max17048_set_rcomp(chip);
	if (ret)
		pr_err("%s : failed to set rcomp\n", __func__);

	max17048_get_vcell(chip);
	max17048_get_soc(chip);
	complete_all(&monitor_work_done);

	if (chip->capacity_level != chip->lasttime_capacity_level ||
			chip->capacity_level == 0) {
		rechg_now = max17048_check_recharge(chip);
		chip->lasttime_capacity_level = chip->capacity_level;
		if (!rechg_now)
			power_supply_changed(&chip->batt_psy);
	}

	ret = max17048_clear_interrupt(chip);
	if (ret < 0)
		pr_err("%s : error clear alert irq register.\n", __func__);

	if (chip->capacity_level == 0) {
		max17048_check_low_vbatt(chip);
		schedule_delayed_work(&chip->monitor_work,
				msecs_to_jiffies(chip->poll_interval_ms));
	} else {
		pr_info("%s: rsoc=0x%04X rvcell=0x%04X soc=%d"\
			" v_mv=%d i_ua=%d t=%d\n", __func__,
				chip->soc, chip->vcell,
				chip->capacity_level, chip->voltage,
				chip->batt_current, chip->batt_temp);
		wake_unlock(&chip->alert_lock);
	}
}

static irqreturn_t max17048_interrupt_handler(int irq, void *data)
{
	struct max17048_chip *chip = data;

	pr_debug("%s : interupt occured\n", __func__);
	schedule_delayed_work(&chip->monitor_work, 0);

	return IRQ_HANDLED;
}

static int max17048_clear_interrupt(struct max17048_chip *chip)
{
	int ret;

	pr_debug("%s.\n", __func__);

	ret = max17048_masked_write_word(chip->client,
			CONFIG_REG, CFG_ALRT_MASK, 0);
	if (ret < 0) {
		pr_err("%s: failed to clear alert status bit\n", __func__);
		return ret;
	}

	ret = max17048_masked_write_word(chip->client,
			STATUS_REG, STAT_CLEAR_MASK, 0);
	if (ret < 0) {
		pr_err("%s: failed to clear status reg\n", __func__);
		return ret;
	}

	return 0;
}

static int max17048_set_athd_alert(struct max17048_chip *chip, int level)
{
	int ret;

	pr_debug("%s.\n", __func__);

	level = bound_check(32, 1, level);
	level = 32 - level;

	ret = max17048_masked_write_word(chip->client,
			CONFIG_REG, CFG_ATHD_MASK, level);
	if (ret < 0)
		pr_err("%s: failed to set athd alert\n", __func__);

	return ret;
}

static int max17048_set_alsc_alert(struct max17048_chip *chip, bool enable)
{
	int ret;
	u16 val;

	pr_debug("%s. with %d\n", __func__, enable);

	val = (u16)(!!enable << CFG_ALSC_SHIFT);

	ret = max17048_masked_write_word(chip->client,
			CONFIG_REG, CFG_ALSC_MASK, val);
	if (ret < 0)
		pr_err("%s: failed to set alsc alert\n", __func__);

	return ret;
}

ssize_t max17048_store_status(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf,
			  size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct max17048_chip *chip = i2c_get_clientdata(client);

	if (!chip)
		return -ENODEV;

	if (strncmp(buf, "reset", 5) == 0) {
		max17048_set_reset(chip);
		schedule_delayed_work(&chip->monitor_work, 0);
	} else {
		return -EINVAL;
	}

	return count;
}
DEVICE_ATTR(fuelrst, 0664, NULL, max17048_store_status);

static int max17048_parse_dt(struct device *dev,
		struct max17048_chip *chip)
{
	struct device_node *dev_node = dev->of_node;
	int ret = 0;

	chip->alert_gpio = of_get_named_gpio(dev_node,
			"max17048,alert_gpio", 0);
	if (chip->alert_gpio < 0) {
		pr_err("failed to get stat-gpio.\n");
		ret = chip->alert_gpio;
		goto out;
	}

	ret = of_property_read_u32(dev_node, "max17048,rcomp",
				&chip->rcomp);
	if (ret) {
		pr_err("%s: failed to read rcomp\n", __func__);
		goto out;
	}

	ret = of_property_read_u32(dev_node, "max17048,rcomp-co-hot",
				&chip->rcomp_co_hot);
	if (ret) {
		pr_err("%s: failed to read rcomp_co_hot\n", __func__);
		goto out;
	}

	ret = of_property_read_u32(dev_node, "max17048,rcomp-co-cold",
				&chip->rcomp_co_cold);
	if (ret) {
		pr_err("%s: failed to read rcomp_co_cold\n", __func__);
		goto out;
	}

	ret = of_property_read_u32(dev_node, "max17048,alert_threshold",
				&chip->alert_threshold);
	if (ret) {
		pr_err("%s: failed to read alert_threshold\n", __func__);
		goto out;
	}

	ret = of_property_read_u32(dev_node, "max17048,max-mvolt",
				   &chip->max_mvolt);
	if (ret) {
		pr_err("%s: failed to read max voltage\n", __func__);
		goto out;
	}

	ret = of_property_read_u32(dev_node, "max17048,min-mvolt",
				   &chip->min_mvolt);
	if (ret) {
		pr_err("%s: failed to read min voltage\n", __func__);
		goto out;
	}

	ret = of_property_read_u32(dev_node, "max17048,full-soc",
				   &chip->full_soc);
	if (ret) {
		pr_err("%s: failed to read full soc\n", __func__);
		goto out;
	}

	ret = of_property_read_u32(dev_node, "max17048,empty-soc",
				   &chip->empty_soc);
	if (ret) {
		pr_err("%s: failed to read empty soc\n", __func__);
		goto out;
	}

	ret = of_property_read_u32(dev_node, "max17048,batt-tech",
				   &chip->batt_tech);
	if (ret) {
		pr_err("%s: failed to read batt technology\n", __func__);
		goto out;
	}

	ret = of_property_read_u32(dev_node, "max17048,fcc-mah",
				   &chip->fcc_mah);
	if (ret) {
		pr_err("%s: failed to read batt fcc\n", __func__);
		goto out;
	}

	ret = of_property_read_u32(dev_node, "max17048,uvlo-thr-mv",
				   &chip->uvlo_thr_mv);
	if (ret) {
		pr_err("%s: failed to read uvlo threshold\n", __func__);
		goto out;
	}

	pr_info("%s: rcomp = %d rcomp_co_hot = %d rcomp_co_cold = %d",
			__func__, chip->rcomp, chip->rcomp_co_hot,
			chip->rcomp_co_cold);
	pr_info("%s: alert_thres = %d full_soc = %d empty_soc = %d uvlo=%d\n",
			__func__, chip->alert_threshold,
			chip->full_soc, chip->empty_soc, chip->uvlo_thr_mv);

out:
	return ret;
}

static int max17048_get_prop_status(struct max17048_chip *chip)
{
	union power_supply_propval ret = {0,};

	chip->ac_psy->get_property(chip->ac_psy, POWER_SUPPLY_PROP_STATUS,
				&ret);
	return ret.intval;
}

static int max17048_get_prop_health(struct max17048_chip *chip)
{
	union power_supply_propval ret = {0,};

	chip->ac_psy->get_property(chip->ac_psy, POWER_SUPPLY_PROP_HEALTH,
				&ret);
	return ret.intval;
}

static int max17048_get_prop_vbatt_uv(struct max17048_chip *chip)
{
	max17048_get_vcell(chip);
	return chip->voltage * 1000;
}

static int max17048_get_prop_present(struct max17048_chip *chip)
{
	/*FIXME - need to implement */
	return true;
}

#define DEFAULT_TEMP    250
#ifdef CONFIG_SENSORS_QPNP_ADC_VOLTAGE
static int qpnp_get_battery_temp(int *temp)
{
	int ret = 0;
	struct qpnp_vadc_result results;

	if (qpnp_vadc_is_ready()) {
		*temp = DEFAULT_TEMP;
		return 0;
	}

	ret = qpnp_vadc_read(LR_MUX1_BATT_THERM, &results);
	if (ret) {
		pr_err("%s: Unable to read batt temp\n", __func__);
		*temp = DEFAULT_TEMP;
		return ret;
	}

	*temp = (int)results.physical;

	return 0;
}
#endif

static int max17048_get_prop_temp(struct max17048_chip *chip)
{
#ifdef CONFIG_SENSORS_QPNP_ADC_VOLTAGE
	int ret;

	ret = qpnp_get_battery_temp(&chip->batt_temp);
	if (ret)
		pr_err("%s: failed to get batt temp.\n", __func__);
#else
	pr_warn("%s: batt temp is not supported!\n", __func__);
	chip->batt_temp = DEFAULT_TEMP;
#endif
	pr_debug("%s: batt_temp = %d\n", __func__, chip->batt_temp);

	return chip->batt_temp;
}

#ifdef CONFIG_SENSORS_QPNP_ADC_CURRENT
static int qpnp_get_battery_current(int *current_ua)
{
	struct qpnp_iadc_result i_result;
	int ret;

	if (qpnp_iadc_is_ready()) {
		pr_err("%s: qpnp iadc is not ready!\n", __func__);
		*current_ua = 0;
		return 0;
	}

	ret = qpnp_iadc_read(EXTERNAL_RSENSE, &i_result);
	if (ret) {
		pr_err("%s: failed to read iadc\n", __func__);
		*current_ua = 0;
		return ret;
	}

	*current_ua = -i_result.result_ua;

	return 0;
}
#endif

static int max17048_get_prop_current(struct max17048_chip *chip)
{
#ifdef CONFIG_SENSORS_QPNP_ADC_CURRENT
	int ret;

	ret = qpnp_get_battery_current(&chip->batt_current);
	if (ret)
		pr_err("%s: failed to get batt current.\n", __func__);
#else
	pr_warn("%s: batt current is not supported!\n", __func__);
	chip->batt_current = 0;
#endif
	pr_debug("%s: ibatt_ua = %d\n", __func__, chip->batt_current);

	return chip->batt_current;
}

static int max17048_get_prop_capacity(struct max17048_chip *chip)
{
	int ret;

	if (chip->capacity_level == -EINVAL)
		return -EINVAL;

	ret = wait_for_completion_timeout(&monitor_work_done,
					msecs_to_jiffies(500));
	if (!ret)
		pr_err("%s: timeout monitor work done\n", __func__);

	return chip->capacity_level;
}

static enum power_supply_property max17048_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
};

static int max17048_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct max17048_chip *chip = container_of(psy,
				struct max17048_chip, batt_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = max17048_get_prop_status(chip);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = max17048_get_prop_health(chip);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = max17048_get_prop_present(chip);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = chip->batt_tech;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = chip->max_mvolt * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = chip->min_mvolt * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = max17048_get_prop_vbatt_uv(chip);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = max17048_get_prop_temp(chip);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = max17048_get_prop_capacity(chip);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = max17048_get_prop_current(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = chip->fcc_mah;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void max17048_external_power_changed(struct power_supply *psy)
{
	struct max17048_chip *chip = container_of(psy, struct max17048_chip,
						batt_psy);
	int chg_state;
	int batt_health;

	chg_state = max17048_get_prop_status(chip);
	batt_health = max17048_get_prop_health(chip);

	if ((chip->chg_state ^ chg_state)
			||(chip->batt_health ^ batt_health)) {
		chip->chg_state = chg_state;
		chip->batt_health = batt_health;
		pr_info("%s: power supply changed state = %d health = %d",
					__func__, chg_state, batt_health);
		power_supply_changed(psy);
	}
}

static int max17048_create_debugfs_entries(struct max17048_chip *chip)
{
	int i;

	chip->dent = debugfs_create_dir("max17048", NULL);
	if (IS_ERR(chip->dent)) {
		pr_err("max17048 driver couldn't create debugfs dir\n");
		return -EFAULT;
	}

	for (i = 0 ; i < ARRAY_SIZE(max17048_debug_regs) ; i++) {
		char *name = max17048_debug_regs[i].name;
		u32 reg = max17048_debug_regs[i].reg;
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

static int max17048_hw_init(struct max17048_chip *chip)
{
	int ret;

	ret = max17048_masked_write_word(chip->client,
			STATUS_REG, STAT_RI_MASK, 0);
	if (ret) {
		pr_err("%s: failed to clear ri bit\n", __func__);
		return ret;
	}

	ret = max17048_set_athd_alert(chip, chip->alert_threshold);
	if (ret) {
		pr_err("%s: failed to set athd alert threshold\n", __func__);
		return ret;
	}

	ret = max17048_set_alsc_alert(chip, true);
	if (ret) {
		pr_err("%s: failed to set alsc alert\n", __func__);
		return ret;
	}

	return 0;
}

static int max17048_pm_notifier(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	struct max17048_chip *chip = container_of(notifier,
				struct max17048_chip, pm_notifier);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		max17048_set_alsc_alert(chip, false);
		cancel_delayed_work_sync(&chip->monitor_work);
		break;
	case PM_POST_SUSPEND:
		INIT_COMPLETION(monitor_work_done);
		schedule_delayed_work(&chip->monitor_work,
					msecs_to_jiffies(200));
		max17048_set_alsc_alert(chip, true);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static int max17048_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct max17048_chip *chip;
	int ret;
	uint16_t version;

	pr_info("%s: start\n", __func__);

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_WORD_DATA)) {
		pr_err("%s: i2c_check_functionality fail\n", __func__);
		return -EIO;
	}

	chip->client = client;
	chip->ac_psy = power_supply_get_by_name("ac");
	if (!chip->ac_psy) {
		pr_err("ac supply not found deferring probe\n");
		ret = -EPROBE_DEFER;
		goto error;
	}

	if (&client->dev.of_node) {
		ret = max17048_parse_dt(&client->dev, chip);
		if (ret) {
			pr_err("%s: failed to parse dt\n", __func__);
			goto  error;
		}
	} else {
		chip->pdata = client->dev.platform_data;
	}

	i2c_set_clientdata(client, chip);

	version = max17048_get_version(chip);
	dev_info(&client->dev, "MAX17048 Fuel-Gauge Ver 0x%x\n", version);
	if (version != MAX17048_VERSION_11 &&
	    version != MAX17048_VERSION_12) {
		pr_err("%s: Not supported version: 0x%x\n", __func__,
				version);
		ret = -ENODEV;
		goto error;
	}

	init_completion(&monitor_work_done);
	chip->capacity_level = -EINVAL;
	ref = chip;
	chip->batt_psy.name = "battery";
	chip->batt_psy.type = POWER_SUPPLY_TYPE_BATTERY;
	chip->batt_psy.get_property = max17048_get_property;
	chip->batt_psy.properties = max17048_battery_props;
	chip->batt_psy.num_properties = ARRAY_SIZE(max17048_battery_props);
	chip->batt_psy.external_power_changed =
				max17048_external_power_changed;

	ret = power_supply_register(&client->dev, &chip->batt_psy);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		goto error;
	}

	INIT_DELAYED_WORK(&chip->monitor_work, max17048_work);
	wake_lock_init(&chip->alert_lock, WAKE_LOCK_SUSPEND,
			"max17048_alert");

	ret = gpio_request_one(chip->alert_gpio, GPIOF_DIR_IN,
				"max17048_alert");
	if (ret) {
		pr_err("%s: GPIO Request Failed : return %d\n",
				__func__, ret);
		goto err_gpio_request;
	}

	chip->alert_irq = gpio_to_irq(chip->alert_gpio);
	if (chip->alert_irq < 0) {
		pr_err("%s: failed to get alert irq\n", __func__);
		goto err_request_irq;
	}

	ret = request_threaded_irq(chip->alert_irq, NULL,
				max17048_interrupt_handler,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"MAX17048_Alert", chip);
	if (ret) {
		pr_err("%s: IRQ Request Failed : return %d\n",
				__func__, ret);
		goto err_request_irq;
	}

	ret = enable_irq_wake(chip->alert_irq);
	if (ret) {
		pr_err("%s: set irq to wakeup source failed.\n", __func__);
		goto err_request_wakeup_irq;
	}

	disable_irq(chip->alert_irq);

	ret = device_create_file(&client->dev, &dev_attr_fuelrst);
	if (ret) {
		pr_err("%s: fuelrst creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_create_file_fuelrst;
	}

	ret = max17048_create_debugfs_entries(chip);
	if (ret) {
		pr_err("max17048_create_debugfs_entries failed\n");
		goto err_create_debugfs;
	}

	ret = max17048_hw_init(chip);
	if (ret) {
		pr_err("%s: failed to init hw.\n", __func__);
		goto err_hw_init;
	}

	chip->pm_notifier.notifier_call = max17048_pm_notifier;
	ret = register_pm_notifier(&chip->pm_notifier);
	if (ret) {
		pr_err("%s: failed to register pm notifier\n", __func__);
		goto err_hw_init;
	}

	schedule_delayed_work(&chip->monitor_work, 0);
	enable_irq(chip->alert_irq);

	pr_info("%s: done\n", __func__);
	return 0;

err_hw_init:
	debugfs_remove_recursive(chip->dent);
err_create_debugfs:
	device_remove_file(&client->dev, &dev_attr_fuelrst);
err_create_file_fuelrst:
	disable_irq_wake(chip->alert_irq);
err_request_wakeup_irq:
	free_irq(chip->alert_irq, NULL);
err_request_irq:
	gpio_free(chip->alert_gpio);
err_gpio_request:
	wake_lock_destroy(&chip->alert_lock);
	power_supply_unregister(&chip->batt_psy);
error:
	ref = NULL;
	kfree(chip);
	return ret;
}

static int max17048_remove(struct i2c_client *client)
{
	struct max17048_chip *chip = i2c_get_clientdata(client);

	unregister_pm_notifier(&chip->pm_notifier);
	debugfs_remove_recursive(chip->dent);
	device_remove_file(&client->dev, &dev_attr_fuelrst);
	disable_irq_wake(chip->alert_irq);
	free_irq(chip->alert_irq, NULL);
	gpio_free(chip->alert_gpio);
	wake_lock_destroy(&chip->alert_lock);
	power_supply_unregister(&chip->batt_psy);
	ref = NULL;
	kfree(chip);

	return 0;
}

static struct of_device_id max17048_match_table[] = {
	{ .compatible = "maxim,max17048", },
	{ },
};

static const struct i2c_device_id max17048_id[] = {
	{ "max17048", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, max17048_id);

static struct i2c_driver max17048_i2c_driver = {
	.driver	= {
		.name = "max17048",
		.owner = THIS_MODULE,
		.of_match_table = max17048_match_table,
	},
	.probe = max17048_probe,
	.remove = max17048_remove,
	.id_table = max17048_id,
};

static int __init max17048_init(void)
{
	return i2c_add_driver(&max17048_i2c_driver);
}
module_init(max17048_init);

static void __exit max17048_exit(void)
{
	i2c_del_driver(&max17048_i2c_driver);
}
module_exit(max17048_exit);

MODULE_DESCRIPTION("MAX17048 Fuel Gauge");
MODULE_LICENSE("GPL");
