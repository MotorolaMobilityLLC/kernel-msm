/*
 * Copyright (C) 2015 Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#include <linux/alarmtimer.h>
#include <linux/atomic.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/sysfs.h>
#include <linux/types.h>

/*
 * From device tree node
 * chg_name:		Sysfs name of wireless charger power_supply device
 * switch_name:		Sysfs name of dock switch device
 * gpio:		Dock status gpio
 * active_low:		Is the dock status gpio active low?
 * supplied_to:		Array of battery names that dock supplies power to
 * num_supplicants:	Number of entries in the supplied_to array
 * debounce_window:	How long debouncing is on after activated
 * debounce_duration:   Debounce duration of an undock event
 * suspend_delay:	Duration of timed wakelock after dock/undock events
 */
struct smelt_dock_dts {
	const char *chg_name;
	const char *switch_name;

	int gpio;
	int active_low;

	char **supplied_to;
	size_t num_supplicants;

	u32 debounce_window;	/* in S */
	u32 debounce_duration;	/* in mS */

	u32 suspend_delay;	/* in mS */
};

/* To debounce undocking */
struct debounce {
	atomic_t on;
	struct alarm alarm;
	ktime_t alarm_time;
};

struct smelt_dock {
	const struct smelt_dock_dts *dts;
	struct device *dev;
	unsigned int irq;
	struct power_supply charger;
	struct switch_dev *sdev;
	struct mutex sdev_mutex; /* to set switch state */
	struct debounce *debounce;
};

static ssize_t smelt_dock_debounce_enable(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct smelt_dock *chip = platform_get_drvdata(pdev);
	struct debounce *debounce = chip->debounce;

	if (debounce) {
		alarm_cancel(&debounce->alarm);
		atomic_set(&debounce->on, 1);
		alarm_start_relative(&debounce->alarm, debounce->alarm_time);
	}

	return count;
}

static DEVICE_ATTR(smart_debounce, S_IWUSR, NULL, smelt_dock_debounce_enable);

static struct attribute *smelt_dock_attrs[] = {
	&dev_attr_smart_debounce.attr,
	NULL,
};

static const struct attribute_group smelt_dock_attr_group = {
	.attrs = smelt_dock_attrs,
};

static inline int dock_state(const struct smelt_dock_dts *dts)
{
	int gpio_state = gpio_get_value_cansleep(dts->gpio);
	return dts->active_low ? !gpio_state : gpio_state;
}


static enum alarmtimer_restart
smelt_dock_debounce_alarm(struct alarm *alarm, ktime_t now)
{
	struct debounce *debounce = container_of(alarm, struct debounce, alarm);

	atomic_set(&debounce->on, 0);
	return ALARMTIMER_NORESTART;
}

static irqreturn_t smelt_dock_irq_thread(int irq, void *devid)
{
	struct smelt_dock *chip = devid;
	struct debounce *debounce = chip->debounce;
	int state = dock_state(chip->dts);

	if (debounce && atomic_read(&debounce->on) && !state) {
		msleep(chip->dts->debounce_duration);
		atomic_set(&debounce->on, 0);
		state = dock_state(chip->dts);
	}

	mutex_lock(&chip->sdev_mutex);
	switch_set_state(chip->sdev, state);
	power_supply_changed(&chip->charger);
	mutex_unlock(&chip->sdev_mutex);

	/* Delay suspend for user space to process dock/undock events */
	if (chip->dts->suspend_delay)
		pm_wakeup_event(chip->dev, chip->dts->suspend_delay);

	return IRQ_HANDLED;
}

static enum power_supply_property smelt_dock_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int smelt_dock_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	struct smelt_dock *chip = container_of(psy, struct smelt_dock, charger);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		mutex_lock(&chip->sdev_mutex);
		val->intval = chip->sdev->state;
		mutex_unlock(&chip->sdev_mutex);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static struct smelt_dock_dts *of_smelt_dock(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct smelt_dock_dts *dts;
	const char *string;
	int i, count, size;
	u32 val;

	dts = devm_kzalloc(dev, sizeof(*dts), GFP_KERNEL);
	if (!dts) {
		dev_err(dev, "Failed to alloc platform data\n");
		return NULL;
	}

	of_property_read_string(np, "charger-name", &dts->chg_name);

	of_property_read_string(np, "switch-name", &dts->switch_name);

	dts->gpio = of_get_named_gpio(np, "gpio", 0);
	if (!gpio_is_valid(dts->gpio)) {
		dev_err(dev, "No valid gpio property\n");
		return NULL;
	}

	dts->active_low = of_property_read_bool(np, "active_low");

	count = of_property_count_strings(np, "supplied_to");
	if (count > 0) {
		size = count * sizeof(*dts->supplied_to);
		dts->supplied_to = devm_kzalloc(dev, size, GFP_KERNEL);
		if (!dts->supplied_to) {
			dev_err(dev, "Failed to alloc supplied_to\n");
			return NULL;
		}

		/* Make copies of the DT strings for const-correctness */
		for (i = 0; i < count; i++) {
			if (of_property_read_string_index(np, "supplied_to", i,
							  &string)) {
				dev_err(dev, "Failed to read supplied_to"
					" supplied_to[%d]\n", i);
				goto free;
			}
			dts->supplied_to[i] = kstrdup(string,  GFP_KERNEL);
			if (!dts->supplied_to[i]) {
				dev_err(dev, "Failed to alloc space for"
					" supplied_to[%d]\n", i);
				goto free;
			}
		}
		dts->num_supplicants = count;
	}

	if (!of_property_read_u32(np, "debounce-window,s", &val))
		dts->debounce_window = val;

	if (!of_property_read_u32(np, "debounce-duration,ms", &val))
		dts->debounce_duration = val;

	if (!of_property_read_u32(np, "suspend-delay,ms", &val))
		dts->suspend_delay = val;

	return dts;
free:
	for (i = 0; i < count; i++)
		kfree(dts->supplied_to[i]);
	return NULL;
}

static int smelt_dock_probe(struct platform_device *pdev)
{
	struct smelt_dock *chip;
	int i, ret;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&pdev->dev, "Failed to alloc driver structure\n");
		return -ENOMEM;
	}

	chip->dev = &pdev->dev;

	chip->dts = of_smelt_dock(&pdev->dev);
	if (!chip->dts) {
		dev_err(&pdev->dev, "Failed to parse devtree node\n");
		return -EINVAL;
	}

	platform_set_drvdata(pdev, chip);

	ret = gpio_request_one(chip->dts->gpio, GPIOF_IN | GPIOF_EXPORT, NULL);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request gpio pin: %d\n", ret);
		goto err_gpio;
	}

	/*
	 * Create switch and power supply before requesting irq since they are
	 * used in the irq thread. Create switch before power supply since it
	 * uses switch state to report its own state
	 */
	chip->sdev = devm_kzalloc(&pdev->dev, sizeof(*chip->sdev), GFP_KERNEL);
	if (!chip->sdev) {
		dev_err(&pdev->dev, "Failed to alloc switch\n");
		ret = -ENOMEM;
		goto err_switch;
	}

	chip->sdev->name = chip->dts->switch_name ?
				chip->dts->switch_name : "smelt-dock";
	ret = switch_dev_register(chip->sdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register switch: %d\n", ret);
		goto err_switch;
	}

	/* Init mutex before power supply is created and irq is requested */
	mutex_init(&chip->sdev_mutex);

	chip->charger.name = chip->dts->chg_name ?
				chip->dts->chg_name : "smelt-dock";
	chip->charger.type = POWER_SUPPLY_TYPE_WIRELESS;
	chip->charger.properties = smelt_dock_properties;
	chip->charger.num_properties = ARRAY_SIZE(smelt_dock_properties);
	chip->charger.get_property = smelt_dock_get_property;
	chip->charger.supplied_to = chip->dts->supplied_to;
	chip->charger.num_supplicants = chip->dts->num_supplicants;

	ret = power_supply_register(&pdev->dev, &chip->charger);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register power supply: %d\n",
			ret);
		goto err_pwr_supply;
	}

	/* Setup debounce before irq is requested and sysfs entry is created */
	if (chip->dts->debounce_window && chip->dts->debounce_duration) {
		chip->debounce = devm_kzalloc(&pdev->dev,
					      sizeof(*chip->debounce),
					      GFP_KERNEL);

		if (!chip->debounce) {
			dev_err(&pdev->dev, "Failed to alloc debounce data\n");
			ret = -ENOMEM;
			goto err_debounce;
		}

		atomic_set(&chip->debounce->on, 0);
		alarm_init(&chip->debounce->alarm, ALARM_BOOTTIME,
			   smelt_dock_debounce_alarm);
		chip->debounce->alarm_time =
			ktime_set(chip->dts->debounce_window,
				  0 * NSEC_PER_MSEC);
	}

	/* Enable wakeup before requesting irq */
	ret = device_init_wakeup(&pdev->dev, 1);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to enable dev wakeup %d\n", ret);
		goto err_dev_wakeup;
	}

	chip->irq = gpio_to_irq(chip->dts->gpio);
	ret = request_threaded_irq(chip->irq, NULL, smelt_dock_irq_thread,
				   IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
				   IRQF_ONESHOT, dev_name(&pdev->dev), chip);

	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to request irq: %d\n", ret);
		goto err_irq;
	}

	ret = enable_irq_wake(chip->irq);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to enable irq wake: %d\n", ret);
		goto err_irq_wake;
	}

	/* Create sysfs files */
	ret = sysfs_create_group(&pdev->dev.kobj, &smelt_dock_attr_group);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to create sysfs files: %d\n", ret);
		goto err_sysfs;
	}

	/* Set initial switch state */
	if (mutex_trylock(&chip->sdev_mutex)) {
		switch_set_state(chip->sdev, dock_state(chip->dts));
		mutex_unlock(&chip->sdev_mutex);
	}

	return 0;

err_sysfs:
	disable_irq_wake(chip->irq);
err_irq_wake:
	free_irq(chip->irq, chip);
err_irq:
	device_init_wakeup(&pdev->dev, 0);
err_dev_wakeup:
	/* Alarm cannot be on before sysfs file is created */
err_debounce:
	power_supply_unregister(&chip->charger);
err_pwr_supply:
	mutex_destroy(&chip->sdev_mutex);
	switch_dev_unregister(chip->sdev);
err_switch:
	gpio_free(chip->dts->gpio);
err_gpio:
	/* Free memory allocated for the copies of the DT strings */
	for (i = 0; i < chip->dts->num_supplicants; i++)
		kfree(chip->dts->supplied_to[i]);
	platform_set_drvdata(pdev, NULL);
	return ret;
}

static int smelt_dock_remove(struct platform_device *pdev)
{
	struct smelt_dock *chip = platform_get_drvdata(pdev);
	int i;

	sysfs_remove_group(&pdev->dev.kobj, &smelt_dock_attr_group);
	disable_irq_wake(chip->irq);
	free_irq(chip->irq, chip);
	device_init_wakeup(&pdev->dev, 0);
	if (chip->debounce)
		alarm_cancel(&chip->debounce->alarm);
	power_supply_unregister(&chip->charger);
	mutex_destroy(&chip->sdev_mutex);
	switch_dev_unregister(chip->sdev);
	gpio_free(chip->dts->gpio);
	/* Free memory allocated for the copies of the DT strings */
	for (i = 0; i < chip->dts->num_supplicants; i++)
		kfree(chip->dts->supplied_to[i]);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id smelt_dock_of_match[] = {
	{ .compatible = "mmi,smelt-dock", },
	{ }
};
MODULE_DEVICE_TABLE(of, smelt_dock_of_match);

static struct platform_driver smelt_dock_driver = {
	.probe = smelt_dock_probe,
	.remove = smelt_dock_remove,
	.driver = {
		.name = "smelt-dock",
		.owner = THIS_MODULE,
		.of_match_table = smelt_dock_of_match,
	},
};
module_platform_driver(smelt_dock_driver);

MODULE_ALIAS("platform:smelt-dock");
MODULE_AUTHOR("Motorola Mobility LLC");
MODULE_DESCRIPTION("Smelt Wireless Dock Detection Driver");
MODULE_LICENSE("GPL");
