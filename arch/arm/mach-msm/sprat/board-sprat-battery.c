/*
 *  board-sprat-battery.c
 *
 * Copyright (C) 2014 Samsung Electronics
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/platform_data/android_battery.h>
#include <linux/qpnp/qpnp-adc.h>

struct android_bat_platform_data android_battery_pdata;
static struct qpnp_vadc_chip *adc_client;
static struct android_bat_callbacks *bat_callbacks;
extern int current_cable_type;
static struct power_supply *charger_power_supply = NULL;
static struct power_supply *fuelgauge_power_supply = NULL;
#define ADC_CHECK_COUNT 6

struct sprat_bat_adc_table_data {
	int adc;
	int data;
};

static struct sprat_bat_adc_table_data temp_table[] = {
	{25534, 900},
	{25779, 850},
	{26041, 800},
	{26330, 750},
	{26680, 700},
	{27025, 650},
	{27440, 600},
	{28030, 550},
	{28614, 500},
	{29337, 450},
	{30083, 400},
	{30988, 350},
	{31972, 300},
	{32995, 250},
	{34020, 200},
	{35091, 150},
	{36247, 100},
	{37292, 50},
	{37477, 40},
	{37666, 30},
	{37899, 20},
	{38052, 10},
	{38277, 0},
	{38420, -10},
	{38621, -20},
	{38790, -30},
	{38996, -40},
	{39157, -50},
	{40045, -100},
	{40779, -150},
	{41287, -200},
	{41693, -250},
	{42005, -300},
};

extern int androidboot_mode_charger;
static int sprat_bat_is_poweroff_charging(void)
{
	return androidboot_mode_charger;
}

static int sprat_bat_read_adc(enum power_supply_property psp) {
	struct qpnp_vadc_result results;
	int rc = -1;
	int data = -1;

	switch (psp)
	{
	case POWER_SUPPLY_PROP_TEMP:
		rc = qpnp_vadc_read(adc_client, P_MUX2_1_1, &results);
		if (rc) {
			pr_err("%s: Unable to read batt temperature rc=%d\n",
					__func__, rc);
			return data;
		}
		data = (int)results.adc_code;
		break;
	default:
		break;
	}
	pr_debug("%s: data (%d)\n", __func__, data);
	return data;
}

static int sprat_bat_get_adc_data(enum power_supply_property psp, int count)
{
	int adc_data;
	int adc_max;
	int adc_min;
	int adc_total;
	int i;

	adc_data = 0;
	adc_max = 0;
	adc_min = 0;
	adc_total = 0;

	for (i = 0; i < count; i++) {
		adc_data = sprat_bat_read_adc(psp);

		if (adc_data < 0)
			goto err;

		if (i != 0) {
			if (adc_data > adc_max)
				adc_max = adc_data;
			else if (adc_data < adc_min)
				adc_min = adc_data;
		} else {
			adc_max = adc_data;
			adc_min = adc_data;
		}
		adc_total += adc_data;
	}

	return (adc_total - adc_max - adc_min) / (count - 2);
err:
	return adc_data;
}

static int sprat_bat_get_charger_power_supply(void)
{
	if (!charger_power_supply)
		charger_power_supply =
			power_supply_get_by_name(android_battery_pdata.charger_name);

	if (!charger_power_supply) {
		pr_err_once("%s: failed to get %s\n",
				__func__, android_battery_pdata.charger_name);
		return -ENODEV;
	}
	return 0;
}

static int sprat_bat_get_fuelgauge_power_supply(void)
{
	if (!fuelgauge_power_supply)
		fuelgauge_power_supply =
			power_supply_get_by_name(android_battery_pdata.fuelgauge_name);

	if (!fuelgauge_power_supply) {
		pr_err_once("%s: failed to get %s\n",
				__func__, android_battery_pdata.fuelgauge_name);
		return -ENODEV;
	}
	return 0;
}

static int sprat_bat_get_capacity(void)
{
	union power_supply_propval soc;
	int ret = -ENXIO;

	if (sprat_bat_get_fuelgauge_power_supply())
		return ret;
	ret = fuelgauge_power_supply->get_property(
			fuelgauge_power_supply, POWER_SUPPLY_PROP_CAPACITY, &soc);
	if (ret >= 0)
		ret = soc.intval;
	return ret;
}

static int sprat_bat_get_voltage_now(void)
{
	union power_supply_propval vcell;
	int ret = -ENXIO;

	if (sprat_bat_get_fuelgauge_power_supply())
		return ret;
	ret = fuelgauge_power_supply->get_property(
			fuelgauge_power_supply, POWER_SUPPLY_PROP_VOLTAGE_NOW, &vcell);
	if (ret >= 0)
		ret = vcell.intval;
	return ret;
}

static int sprat_bat_get_current_now(int * uA)
{
	union power_supply_propval value;
	int ret = -ENXIO;

	if (sprat_bat_get_charger_power_supply())
		return ret;
	ret = charger_power_supply->get_property(
			charger_power_supply, POWER_SUPPLY_PROP_CURRENT_NOW, &value);
	if(ret >= 0)
		*uA = value.intval/1000;
	return ret;
}

static int sprat_bat_get_temperature_data(struct android_bat_data * battery, int *value)
{
	int temp = 0;
	int temp_adc;
	int low = 0;
	int high = 0;
	int mid = 0;
	unsigned int temp_table_size;

	temp_adc = sprat_bat_get_adc_data(POWER_SUPPLY_PROP_TEMP, ADC_CHECK_COUNT);
	temp_table_size = sizeof(temp_table) / sizeof(struct sprat_bat_adc_table_data);

	if (temp_adc < 0)
		return -ENXIO;

	battery->temp_adc = temp_adc;

	if (temp_table[0].adc >= temp_adc) {
		temp = temp_table[0].data;
		goto temp_by_adc_goto;
	} else if (temp_table[temp_table_size-1].adc <= temp_adc) {
		temp = temp_table[temp_table_size-1].data;
		goto temp_by_adc_goto;
	}

	high = temp_table_size - 1;

	while (low <= high) {
		mid = (low + high) / 2;
		if (temp_table[mid].adc > temp_adc)
			high = mid - 1;
		else if (temp_table[mid].adc < temp_adc)
			low = mid + 1;
		else {
			temp = temp_table[mid].data;
			goto temp_by_adc_goto;
		}
	}

	temp = temp_table[high].data;
	temp += ((temp_table[low].data - temp_table[high].data) *
		(temp_adc - temp_table[high].adc)) /
		(temp_table[low].adc - temp_table[high].adc);

temp_by_adc_goto:
	*value = temp;

	pr_debug("%s: Temp(%d), Temp-ADC(%d)\n",
		__func__, temp, temp_adc);

	return 0;
}

static void sprat_bat_get_temperature(struct android_bat_data * battery, int *temp)
{
	union power_supply_propval value;
	int ret;
	ret = sprat_bat_get_temperature_data(battery, temp);

	/* If ret is -ENXIO, it couldn't get the temperature ADC*/
	if(ret >= 0) {
		/* Rcomp update */
		value.intval = *temp;
		psy_do_property(battery->pdata->fuelgauge_name, set,
				POWER_SUPPLY_PROP_TEMP, value);
	}
}

static void sprat_bat_set_charging_enable(int enable) {
	union power_supply_propval value;

	value.intval = current_cable_type;
	psy_do_property(android_battery_pdata.charger_name, set,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, value);

	value.intval = enable;
	psy_do_property(android_battery_pdata.charger_name, set,
			POWER_SUPPLY_PROP_ONLINE, value);
}

void sprat_bat_initial_check(void) {
	union power_supply_propval value;

	pr_info("%s : current_cable_type : (%d)\n", __func__, current_cable_type);
	if (CHARGE_SOURCE_NONE != current_cable_type) {
		value.intval = current_cable_type;
		psy_do_property("battery", set,
				POWER_SUPPLY_PROP_ONLINE, value);
	}
}

void sprat_bat_init_adc(struct android_bat_data * battery) {
	adc_client = qpnp_get_vadc(battery->dev, "android-battery");

	if (IS_ERR(adc_client)) {
		int rc;
		rc = PTR_ERR(adc_client);
		if (rc != -EPROBE_DEFER)
			pr_err("%s: Fail to get vadc %d\n", __func__, rc);
	}
}

static void sprat_bat_change_charge_state(struct android_bat_data * battery)
{
	union power_supply_propval value;
	if (battery->charging_status == POWER_SUPPLY_STATUS_NOT_CHARGING) {
		pr_info("%s: No Need to Check Full-Charged\n", __func__);
		return;
	}
	psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_PROP_STATUS, value);

	if (value.intval == POWER_SUPPLY_STATUS_FULL &&
		bat_callbacks && bat_callbacks->battery_set_full)
			    bat_callbacks->battery_set_full(
					                bat_callbacks);
}

static int sprat_bat_poll_charge_source(struct android_bat_data * battery)
{
	sprat_bat_change_charge_state(battery);
	return current_cable_type;
}

static void sprat_bat_register_callbacks(struct android_bat_callbacks *ptr)
{
	bat_callbacks = ptr;
}

static void sprat_bat_unregister_callbacks(void)
{
	bat_callbacks = NULL;
}

struct android_bat_platform_data android_battery_pdata = {
	.register_callbacks = sprat_bat_register_callbacks,
	.unregister_callbacks = sprat_bat_unregister_callbacks,
	.poll_charge_source = sprat_bat_poll_charge_source,
	.initial_check = sprat_bat_initial_check,
	.set_charging_enable = sprat_bat_set_charging_enable,
	.get_temperature = sprat_bat_get_temperature,
	.get_capacity = sprat_bat_get_capacity,
	.get_voltage_now = sprat_bat_get_voltage_now,
	.get_current_now = sprat_bat_get_current_now,
	.init_adc = sprat_bat_init_adc,
	.is_poweroff_charging = sprat_bat_is_poweroff_charging,
};

MODULE_DESCRIPTION("Sprat board battery file");
MODULE_LICENSE("GPL");
