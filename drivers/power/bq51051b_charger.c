/*
 * BQ51051B Wireless Charging(WLC) control driver
 *
 * Copyright (C) 2012 LG Electronics, Inc
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/mfd/pm8xxx/pm8921-charger.h>
#include <linux/mfd/pm8xxx/pm8921-bms.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/leds-pm8xxx.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/errno.h>
#include <linux/power_supply.h>

#include <linux/power_supply.h>
#include <linux/power/bq51051b_charger.h>

struct bq51051b_wlc_chip {
	struct device *dev;
	struct power_supply wireless_psy;
	struct work_struct wireless_interrupt_work;
	struct wake_lock wireless_chip_wake_lock;
	unsigned int active_n_gpio;
	int (*wlc_is_plugged)(void);
};

static const struct platform_device_id bq51051b_id[] = {
	{BQ51051B_WLC_DEV_NAME, 0},
	{},
};

static struct bq51051b_wlc_chip *the_chip;
static bool wireless_charging;
static bool wireless_charge_done;

static void bms_notify(struct bq51051b_wlc_chip *chip, int value)
{
	if (value)
		pm8921_bms_charging_began();
	else
		pm8921_bms_charging_end(0);
}

static enum power_supply_property pm_power_props_wireless[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
};

static char *pm_power_supplied_to[] = {
	"battery",
#ifdef CONFIG_TOUCHSCREEN_CHARGER_NOTIFY
	"touch"
#endif
};

static int pm_power_get_property_wireless(struct power_supply *psy,
					  enum power_supply_property psp,
					  union power_supply_propval *val)
{
	/* Check if called before init */
	if (!the_chip)
		return -EINVAL;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;	//always battery_on
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = wireless_charging;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void wireless_set(struct bq51051b_wlc_chip *chip)
{
	WLC_DBG_INFO("wireless_set\n");

	wake_lock(&chip->wireless_chip_wake_lock);

	wireless_charging = true;
	wireless_charge_done = false;

	bms_notify(chip, 1);

	power_supply_changed(&chip->wireless_psy);
	set_wireless_power_supply_control(wireless_charging);
}

static void wireless_reset(struct bq51051b_wlc_chip *chip)
{
	WLC_DBG_INFO("wireless_reset\n");

	wireless_charging = false;
	wireless_charge_done = false;

	bms_notify(chip, 0);

	power_supply_changed(&chip->wireless_psy);
	set_wireless_power_supply_control(wireless_charging);

	wake_unlock(&chip->wireless_chip_wake_lock);
}

static void wireless_interrupt_worker(struct work_struct *work)
{
	struct bq51051b_wlc_chip *chip =
	    container_of(work, struct bq51051b_wlc_chip,
			 wireless_interrupt_work);

	if (chip->wlc_is_plugged())
		wireless_set(chip);
	else
		wireless_reset(chip);
}

static irqreturn_t wireless_interrupt_handler(int irq, void *data)
{
	int chg_state;
	struct bq51051b_wlc_chip *chip = data;

	chg_state = chip->wlc_is_plugged();
	WLC_DBG_INFO("\nwireless is plugged state = %d\n\n", chg_state);
	schedule_work(&chip->wireless_interrupt_work);
	return IRQ_HANDLED;
}

static int __devinit bq51051b_wlc_hw_init(struct bq51051b_wlc_chip *chip)
{
	int ret;
	WLC_DBG_INFO("hw_init");

	/* active_n pin must be monitoring the bq51051b status */
	ret = request_irq(gpio_to_irq(chip->active_n_gpio),
			wireless_interrupt_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"wireless_charger", chip);
	if (ret < 0) {
		pr_err("wlc: wireless_charger request irq failed\n");
		return ret;
	}
	enable_irq_wake(gpio_to_irq(chip->active_n_gpio));

	return 0;
}

static int bq51051b_wlc_resume(struct device *dev)
{
	return 0;
}

static int bq51051b_wlc_suspend(struct device *dev)
{
	return 0;
}

static int __devinit bq51051b_wlc_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct bq51051b_wlc_chip *chip;
	const struct bq51051b_wlc_platform_data *pdata =
		pdev->dev.platform_data;

	WLC_DBG_INFO("probe\n");

	if (!pdata) {
		pr_err("wlc: missing platform data\n");
		return -ENODEV;
	}

	chip = kzalloc(sizeof(struct bq51051b_wlc_chip), GFP_KERNEL);
	if (!chip) {
		pr_err("wlc: Cannot allocate bq51051b_wlc_chip\n");
		return -ENOMEM;
	}

	chip->dev = &pdev->dev;

	chip->active_n_gpio = pdata->active_n_gpio;
	chip->wlc_is_plugged = pdata->wlc_is_plugged;

	rc = bq51051b_wlc_hw_init(chip);
	if (rc) {
		pr_err("wlc: couldn't init hardware rc = %d\n", rc);
		goto free_chip;
	}

	chip->wireless_psy.name = "wireless";
	chip->wireless_psy.type = POWER_SUPPLY_TYPE_WIRELESS;
	chip->wireless_psy.supplied_to = pm_power_supplied_to;
	chip->wireless_psy.num_supplicants = ARRAY_SIZE(pm_power_supplied_to);
	chip->wireless_psy.properties = pm_power_props_wireless;
	chip->wireless_psy.num_properties = ARRAY_SIZE(pm_power_props_wireless);
	chip->wireless_psy.get_property = pm_power_get_property_wireless;

	rc = power_supply_register(chip->dev, &chip->wireless_psy);
	if (rc < 0) {
		pr_err("wlc: power_supply_register wireless failed rx = %d\n",
			      rc);
		goto free_chip;
	}

	platform_set_drvdata(pdev, chip);
	the_chip = chip;

	INIT_WORK(&chip->wireless_interrupt_work, wireless_interrupt_worker);
	wake_lock_init(&chip->wireless_chip_wake_lock, WAKE_LOCK_SUSPEND,
		       "bq51051b_wireless_chip");

	/* For Booting Wireless_charging and For Power Charging Logo In Wireless Charging */
	if (chip->wlc_is_plugged())
		wireless_set(chip);

	return 0;

free_chip:
	kfree(chip);
	return rc;
}

static int __devexit bq51051b_wlc_remove(struct platform_device *pdev)
{
	struct bq51051b_wlc_chip *chip = platform_get_drvdata(pdev);

	WLC_DBG_INFO("remove\n");
	wake_lock_destroy(&chip->wireless_chip_wake_lock);
	the_chip = NULL;
	platform_set_drvdata(pdev, NULL);
	power_supply_unregister(&chip->wireless_psy);
	free_irq(gpio_to_irq(chip->active_n_gpio), chip);
	gpio_free(chip->active_n_gpio);
	kfree(chip);
	return 0;
}

static const struct dev_pm_ops bq51051b_pm_ops = {
	.suspend = bq51051b_wlc_suspend,
	.resume = bq51051b_wlc_resume,
};

static struct platform_driver bq51051b_wlc_driver = {
	.probe = bq51051b_wlc_probe,
	.remove = __devexit_p(bq51051b_wlc_remove),
	.id_table = bq51051b_id,
	.driver = {
		.name = BQ51051B_WLC_DEV_NAME,
		.owner = THIS_MODULE,
		.pm = &bq51051b_pm_ops,
	},
};

static int __init bq51051b_wlc_init(void)
{
	return platform_driver_register(&bq51051b_wlc_driver);
}

static void __exit bq51051b_wlc_exit(void)
{
	platform_driver_unregister(&bq51051b_wlc_driver);
}

late_initcall(bq51051b_wlc_init);
module_exit(bq51051b_wlc_exit);

MODULE_AUTHOR("Kyungtae Oh <Kyungtae.oh@lge.com>");
MODULE_DESCRIPTION("BQ51051B Wireless Charger Control Driver");
MODULE_LICENSE("GPL v2");
