/*
 * Copyright (C) 2016 Motorola Mobility LLC
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
 */
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>
#include <linux/mods/modbus_ext.h>

enum hd3ss460_device_list {
	USBC = 0,
	MODS,
	MAX_DEV,
};

#define HD3_POL_INDEX 0
#define HD3_AMSEL_INDEX 1
#define HD3_EN_INDEX 2
#define HD3_NUM_GPIOS 3

struct hd3ss460_info {
	int num_gpios;
	struct gpio *list;
	enum hd3ss460_device_list mux_dev;
	struct power_supply *usbc;
	struct power_supply *usbc_switch;
	struct notifier_block switch_notifier;
	struct notifier_block modbus_notifier;
	bool switch_is_fusb340;
	struct delayed_work hd3ss460_switch_work;
	struct delayed_work hd3ss460_work;
	struct regulator *vdd;
	int vdd_levels[2];
	int current_levels[2];
	struct device *dev;
	bool vdd_enabled;
	int usbc_switch_state;
	int ext_state;
};

static ssize_t switch_to_dev_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long r;
	unsigned long mode;
	struct hd3ss460_info *info = dev_get_drvdata(dev);

	r = kstrtoul(buf, 0, &mode);
	if (r) {
		pr_err("Invalid value = %lu\n", mode);
		return -EINVAL;
	}

	/* Handle the mode change if it is valid */
	if (mode < MAX_DEV && !info->switch_is_fusb340) {
		info->mux_dev = mode;
		schedule_delayed_work(&info->hd3ss460_switch_work,
					msecs_to_jiffies(0));
	}

	return count;
}

static ssize_t switch_to_dev_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct hd3ss460_info *info = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", info->mux_dev);
}

static DEVICE_ATTR(switch_to_dev, 0664,
		   switch_to_dev_show,
		   switch_to_dev_store);


static int hd3ss460_vdd_enable(struct hd3ss460_info *info, bool enable)
{
	int rc = 0;

	dev_dbg(info->dev, "reg (%s)\n", enable ? "HPM" : "LPM");

	if ((enable && info->vdd_enabled) || (!enable && !info->vdd_enabled))
		return 0;

	if (!enable)
		goto disable_regulators;


	rc = regulator_set_optimum_mode(info->vdd, info->current_levels[1]);
	if (rc < 0) {
		dev_err(info->dev, "Unable to set HPM of vdd\n");
		return rc;
	}

	rc = regulator_set_voltage(info->vdd, info->vdd_levels[0],
						info->vdd_levels[1]);
	if (rc) {
		dev_err(info->dev, "unable to set voltage for vdd\n");
		goto put_vdd_lpm;
	}

	rc = regulator_enable(info->vdd);
	if (rc) {
		dev_err(info->dev, "Unable to enable vdd\n");
		goto unset_vdd;
	}

	info->vdd_enabled = true;
	return 0;

disable_regulators:
	rc = regulator_disable(info->vdd);
	if (rc)
		dev_err(info->dev, "Unable to disable vdd\n");

unset_vdd:
	rc = regulator_set_voltage(info->vdd, 0, info->vdd_levels[1]);
	if (rc)
		dev_err(info->dev, "unable to set voltage for vdd\n");

put_vdd_lpm:
	rc = regulator_set_optimum_mode(info->vdd, info->current_levels[0]);
	if (rc < 0)
		dev_err(info->dev, "Unable to set LPM of vdd\n");

	info->vdd_enabled = false;

	return rc < 0 ? rc : 0;
}

static void hd3ss460_switch_set_state(struct hd3ss460_info *info, int pol,
			int amsel, int en)
{
	gpio_set_value(info->list[HD3_POL_INDEX].gpio, pol);
	gpio_set_value(info->list[HD3_AMSEL_INDEX].gpio, amsel);
	if (!info->switch_is_fusb340)
		gpio_set_value(info->list[HD3_EN_INDEX].gpio, en);
}

static int modbus_notifier_call(struct notifier_block *nb,
			unsigned long val,
			void *v)
{
	struct hd3ss460_info *info = container_of(nb,
				struct hd3ss460_info, modbus_notifier);

	struct modbus_ext_status *sl_status = v;
	if (sl_status->proto > MODBUS_PROTO_MPHY)
		return NOTIFY_DONE;

	if (sl_status->active)
		info->ext_state |= sl_status->proto;
	else
		info->ext_state &= ~(sl_status->proto);
	schedule_delayed_work(&info->hd3ss460_work,
				msecs_to_jiffies(0));

	return NOTIFY_OK;
}

static int usbcswitch_notifier_call(struct notifier_block *nb,
			unsigned long val,
			void *v)
{
	struct hd3ss460_info *info = container_of(nb,
				struct hd3ss460_info, switch_notifier);
	struct power_supply *psy = v;
	int rc;
	union power_supply_propval prop = {0,};

	/* Ignore switch events when switch is not in USBC mode */
	if (info->mux_dev != USBC)
		return NOTIFY_OK;

	if (val != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if (!psy || (psy != info->usbc_switch))
		return NOTIFY_OK;

	dev_dbg(info->dev, "usbc switch notifier call was called\n");

	prop.intval = 0;
	rc = psy->get_property(psy,
			       POWER_SUPPLY_PROP_SWITCH_STATE,
			       &prop);
	if (rc < 0) {
		dev_err(info->dev,
			"could not read Switch State property, rc=%d\n", rc);
		return NOTIFY_DONE;
	}

	info->usbc_switch_state = prop.intval;
	schedule_delayed_work(&info->hd3ss460_work,
				msecs_to_jiffies(0));

	return NOTIFY_OK;
}


static void hd3ss460_w(struct work_struct *work)
{
	struct hd3ss460_info *info =
			container_of(work, struct hd3ss460_info,
			hd3ss460_work.work);
	switch (info->usbc_switch_state) {
	case 0:
		if (info->ext_state) {
			hd3ss460_vdd_enable(info, true);
			hd3ss460_switch_set_state(info, 0, 0, 1);
		} else {
			hd3ss460_switch_set_state(info, 0, 0, 0);
			hd3ss460_vdd_enable(info, false);
		}
		break;
	case 1:
		hd3ss460_vdd_enable(info, true);
		hd3ss460_switch_set_state(info, 0, 0, 1);
		break;
	case 2:
		hd3ss460_vdd_enable(info, true);
		hd3ss460_switch_set_state(info, 0, 1, 1);
		break;
	default:
		break;
	}
}

static void hd3ss460_switch_w(struct work_struct *work)
{
	struct hd3ss460_info *info =
			container_of(work, struct hd3ss460_info,
			hd3ss460_switch_work.work);
	struct power_supply *usb_psy = power_supply_get_by_name("usb");
	union power_supply_propval ret = {0,};

	dev_dbg(info->dev, "Setting mux_dev to %d\n", info->mux_dev);
	if (info->mux_dev == USBC) {
		/* Disable Host mode in the USB Controller */
		if (usb_psy)
			power_supply_set_usb_otg(usb_psy, 0);
		/* Disable the Crossbar switch */
		hd3ss460_switch_set_state(info, 0, 0, 0);
		hd3ss460_vdd_enable(info, false);
		/* Enable the USB C psy */
		info->usbc->set_property(info->usbc,
				POWER_SUPPLY_PROP_DISABLE_USB,
				&ret);
		/* Query for initial reported state*/
		usbcswitch_notifier_call(&info->switch_notifier,
			PSY_EVENT_PROP_CHANGED,
			info->usbc_switch);
	} else if (info->mux_dev == MODS) {
		ret.intval = 1;
		/* Disable the USB C psy */
		info->usbc->set_property(info->usbc,
				POWER_SUPPLY_PROP_DISABLE_USB,
				&ret);
		hd3ss460_vdd_enable(info, true);
		hd3ss460_switch_set_state(info, 1, 0, 1);
		/* Enable Host mode in the USB Controller */
		if (usb_psy)
			power_supply_set_usb_otg(usb_psy, 1);
	} else
		dev_err(info->dev, "Invalid mode set\n");
}

static struct hd3ss460_info *hd3ss460_parse_of(struct platform_device *pdev)
{
	struct hd3ss460_info *info;
	int gpio_count;
	struct device_node *np = pdev->dev.of_node;
	int i, ret;
	enum of_gpio_flags flags;

	if (!np) {
		dev_err(&pdev->dev, "No OF DT node found.\n");
		return ERR_PTR(-ENODEV);
	}

	gpio_count = of_gpio_count(np);

	if (!gpio_count) {
		dev_err(&pdev->dev, "No GPIOS defined.\n");
		return ERR_PTR(-EINVAL);
	}

	/* Make sure number of GPIOs defined matches the supplied number of
	 * GPIO name strings.
	 */
	if (gpio_count != of_property_count_strings(np, "hd3-gpio-labels")) {
		dev_err(&pdev->dev, "GPIO info and name mismatch\n");
		return ERR_PTR(-EINVAL);
	}

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return ERR_PTR(-ENOMEM);

	info->list = devm_kzalloc(&pdev->dev,
				sizeof(struct gpio) * gpio_count,
				GFP_KERNEL);
	if (!info->list)
		return ERR_PTR(-ENOMEM);

	info->num_gpios = gpio_count;
	for (i = 0; i < gpio_count; i++) {
		int gpio = of_get_gpio_flags(np, i, &flags);

		/* Handle the error conditions in case need to defer */
		if (gpio < 0)
			return ERR_PTR(gpio);

		info->list[i].gpio = gpio;
		info->list[i].flags = flags;
		of_property_read_string_index(np, "hd3-gpio-labels", i,
						&info->list[i].label);
		dev_dbg(&pdev->dev, "GPIO: %d  FLAGS: %ld  LABEL: %s\n",
			info->list[i].gpio, info->list[i].flags,
			info->list[i].label);
	}

	info->switch_is_fusb340 = of_property_read_bool(np,
						"hd3-is-fusb340");

	info->vdd = devm_regulator_get(&pdev->dev, "vdd-hd3ss460");
	if (IS_ERR(info->vdd)) {
		dev_err(&pdev->dev, "unable to get vdd supply\n");
		return ERR_PTR(-EINVAL);
	}

	ret = of_property_read_u32_array(np, "vdd-voltage-level",
					(u32 *) info->vdd_levels,
					ARRAY_SIZE(info->vdd_levels));
	if (ret) {
		dev_err(&pdev->dev,
			"error reading vdd-voltage-level property\n");
		return ERR_PTR(-EINVAL);
	}

	ret = of_property_read_u32_array(np, "vdd-current-level",
					(u32 *) info->current_levels,
					ARRAY_SIZE(info->current_levels));
	if (ret) {
		dev_err(&pdev->dev,
			"error reading vdd-voltage-level property\n");
		return ERR_PTR(-EINVAL);
	}
	return info;
}

static int hd3ss460_probe(struct platform_device *pdev)
{
	struct hd3ss460_info *info;
	int ret, i;
	struct power_supply *usbc_psy, *usbc_switch_psy;
	struct modbus_ext_status sl_status;

	usbc_psy = power_supply_get_by_name("usbc");
	if (!usbc_psy) {
		dev_err(&pdev->dev, "USBC supply not found, deferring probe\n");
		return -EPROBE_DEFER;
	}

	usbc_switch_psy = power_supply_get_by_name("usbc_switch");
	if (!usbc_switch_psy) {
		dev_err(&pdev->dev,
			"USBC Switch supply not found, deferring probe\n");
		return -EPROBE_DEFER;
	}


	info = hd3ss460_parse_of(pdev);
	if (IS_ERR(info)) {
		dev_err(&pdev->dev, "failed to parse node\n");
		return PTR_ERR(info);
	}

	ret = gpio_request_array(info->list, info->num_gpios);
	if (ret) {
		dev_err(&pdev->dev, "failed to request GPIOs\n");
		return ret;
	}

	for (i = 0; i < info->num_gpios; i++) {
		ret = gpio_export(info->list[i].gpio, 1);
		if (ret) {
			dev_err(&pdev->dev, "Failed to export GPIO %s: %d\n",
				info->list[i].label, info->list[i].gpio);
			goto fail;
		}

		ret = gpio_export_link(&pdev->dev, info->list[i].label,
					info->list[i].gpio);
		if (ret) {
			dev_err(&pdev->dev, "Failed to link GPIO %s: %d\n",
				info->list[i].label, info->list[i].gpio);
			goto fail;
		}
	}

	info->mux_dev = USBC;
	info->usbc = usbc_psy;
	info->usbc_switch = usbc_switch_psy;
	info->modbus_notifier.notifier_call = modbus_notifier_call;
	ret = modbus_ext_register_notifier(&info->modbus_notifier, NULL);
	if (ret) {
		dev_err(&pdev->dev, "couldn't register modbus notifier\n");
		goto fail;
	}

	info->switch_notifier.notifier_call = usbcswitch_notifier_call;
	ret = power_supply_reg_notifier(&info->switch_notifier);
	if (ret) {
		dev_err(&pdev->dev, "couldn't register usbc_switch notifier\n");
		goto fail_modbus_reg;
	}

	ret = device_create_file(&pdev->dev,
				 &dev_attr_switch_to_dev);
	if (ret)
		dev_err(&pdev->dev,
			"couldn't create switch_to_dev\n");

	INIT_DELAYED_WORK(&info->hd3ss460_switch_work, hd3ss460_switch_w);
	INIT_DELAYED_WORK(&info->hd3ss460_work, hd3ss460_w);
	info->usbc_switch_state = 0;

	dev_set_drvdata(&pdev->dev, info);
	info->dev = &pdev->dev;

	modbus_ext_get_state(&sl_status);

	if (sl_status.active)
		info->ext_state = sl_status.proto &
					(MODBUS_PROTO_I2S | MODBUS_PROTO_MPHY);

	/* Query for initial reported state*/
	usbcswitch_notifier_call(&info->switch_notifier, PSY_EVENT_PROP_CHANGED,
		info->usbc_switch);

	return 0;

fail_modbus_reg:
	modbus_ext_unregister_notifier(&info->modbus_notifier);
fail:
	gpio_free_array(info->list, info->num_gpios);
	return ret;
}

static int hd3ss460_remove(struct platform_device *pdev)
{
	struct hd3ss460_info *info = platform_get_drvdata(pdev);

	device_remove_file(&pdev->dev,
			   &dev_attr_switch_to_dev);

	if (info) {
		modbus_ext_unregister_notifier(&info->modbus_notifier);
		power_supply_unreg_notifier(&info->switch_notifier);
		gpio_free_array(info->list, info->num_gpios);
		cancel_delayed_work_sync(&info->hd3ss460_switch_work);
		power_supply_put(info->usbc);
		power_supply_put(info->usbc_switch);
		hd3ss460_vdd_enable(info, false);
		devm_regulator_put(info->vdd);
		kfree(info);
	}

	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id hd3ss460_of_tbl[] = {
	{	.compatible = "ti,hd3ss460",
	},
	{},
};
#endif

static struct platform_driver hd3ss460_driver = {
	.probe = hd3ss460_probe,
	.remove = hd3ss460_remove,
	.driver = {
		.name = "hd3ss460",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(hd3ss460_of_tbl),
	},
};

static int __init hd3ss460_init(void)
{
	return platform_driver_register(&hd3ss460_driver);
}

static void __exit hd3ss460_exit(void)
{
	platform_driver_unregister(&hd3ss460_driver);
}

module_init(hd3ss460_init);
module_exit(hd3ss460_exit);

MODULE_ALIAS("platform:hd3ss460");
MODULE_AUTHOR("Motorola Mobility LLC");
MODULE_DESCRIPTION("Motorola Mobility Type C Mux");
MODULE_LICENSE("GPL");
