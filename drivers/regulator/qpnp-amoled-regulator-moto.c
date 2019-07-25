/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/power_supply.h>
#include <linux/slab.h>

#define QPNP_AMOLED_REGULATOR_DRIVER_NAME_MOTO	"qcom,qpnp-amoled-regulator-moto"

/* Register definitions */

#define PERIPH_TYPE	0x04
#define AB_PERIPH_TYPE	0x24

/* AB */
#define AB_SW_HIGH_PSRR_CTL(chip)	(chip->ab_base + 0x70)

#define USB_DETECT_IN	1
#define USB_DETECT_OUT	0

#define HiGH_PSRR_EN	0x81
#define HiGH_PSRR_DIS	0x80

struct qpnp_amoled {
	struct device		*dev;
	struct regmap		*regmap;
	struct mutex		reg_lock;

	/* DT params */
	u32			ab_base;

	struct notifier_block charger_notif;
	int usb_connected;
	struct workqueue_struct *charger_notify_wq;
	struct work_struct charger_notify_work;
};

static int qpnp_amoled_write(struct qpnp_amoled *chip,
			u16 addr, u8 *value, u8 count)
{
	int rc;

	rc = regmap_bulk_write(chip->regmap, addr, value, count);
	if (rc < 0)
		pr_err("Failed to write to addr=0x%02x rc=%d\n", addr, rc);

	return rc;
}

static void config_ab_sw_high_psrr_ctl(struct qpnp_amoled *chip, int state)
{
	u8 val = 0;

	pr_info("config_ab_sw_high_psrr_ctl state = %d\n", state);

	if (state == USB_DETECT_IN)
		val = HiGH_PSRR_EN;
	else
		val = HiGH_PSRR_DIS;

	qpnp_amoled_write(chip, AB_SW_HIGH_PSRR_CTL(chip), &val, 1);
}

static void charger_notify_work(struct work_struct *work)
{
	struct qpnp_amoled *chip = container_of(work, struct qpnp_amoled,
							charger_notify_work);

	if (chip == NULL) {
		pr_err("%s:  chip  is null!\n", __func__);
		return;
	}

	config_ab_sw_high_psrr_ctl(chip, chip->usb_connected);
}

static int charger_notifier_callback(struct notifier_block *nb,
		unsigned long val, void *v)
{
	int ret = 0;
	struct power_supply *usb_psy = NULL;
	struct qpnp_amoled *chip = container_of(nb, struct qpnp_amoled,
							charger_notif);
	union power_supply_propval prop;

	usb_psy = power_supply_get_by_name("usb");

	if (!usb_psy || !chip) {
		return -EINVAL;
		pr_err("Couldn't get usbpsy or chip\n");
	}

	if (val == POWER_SUPPLY_PROP_STATUS) {

		ret = power_supply_get_property(usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &prop);
		if (ret < 0)
			pr_err("Couldn't get POWER_SUPPLY_PROP_ONLINE rc=%d\n",
				ret);
		else {
			if (prop.intval != chip->usb_connected) {
				chip->usb_connected = prop.intval;

				queue_work(chip->charger_notify_wq,
					&chip->charger_notify_work);
			}
		}
	}

	return ret;
}

static int charger_notify_init(struct qpnp_amoled *chip)
{
	int rc;

	struct power_supply *psy = NULL;
	union power_supply_propval prop = {0};

	chip->usb_connected = 0;
	chip->charger_notify_wq = create_singlethread_workqueue("charger_wq");
	if (!chip->charger_notify_wq) {
		pr_err("allocate charger_notify_wq failed\n");
		goto err_charger_notify_wq_failed;
	}

	INIT_WORK(&chip->charger_notify_work,
		charger_notify_work);

	chip->charger_notif.notifier_call = charger_notifier_callback;
	rc = power_supply_reg_notifier(&chip->charger_notif);
	if (rc) {
		pr_err("Unable to register charger_notifier:%d\n", rc);
		goto err_register_charger_notify_failed;
	}

	/* if power supply supplier registered before
	 * ps_notify_callback will not receive PSY_EVENT_PROP_ADDED
	 * event, and will cause miss to set register into charger state.
	 * So check PS state in probe.
	 */
	psy = power_supply_get_by_name("usb");

	if (psy) {
		rc = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_PRESENT, &prop);
		if (rc < 0) {
			pr_err("Couldn't get POWER_SUPPLY_PROP_ONLINE rc one\n");
			goto err_register_charger_notify_failed;
		} else {
			if (prop.intval != chip->usb_connected) {
				chip->usb_connected = prop.intval;

				config_ab_sw_high_psrr_ctl(chip,
					chip->usb_connected);
			}
		}
	}


	return 0;

err_register_charger_notify_failed:
	pr_err("usb psy err");
	if (chip->charger_notif.notifier_call)
		power_supply_unreg_notifier(&chip->charger_notif);

	destroy_workqueue(chip->charger_notify_wq);
	chip->charger_notify_wq = NULL;

err_charger_notify_wq_failed:

	return rc;
}

static int qpnp_amoled_parse_dt(struct qpnp_amoled *chip)
{
	struct device_node *temp, *node = chip->dev->of_node;
	const __be32 *prop_addr;
	int rc = 0;
	u32 base, val;

	for_each_available_child_of_node(node, temp) {
		prop_addr = of_get_address(temp, 0, NULL, NULL);
		if (!prop_addr) {
			pr_err("Couldn't get reg address\n");
			return -EINVAL;
		}

		base = be32_to_cpu(*prop_addr);
		rc = regmap_read(chip->regmap, base + PERIPH_TYPE, &val);
		if (rc < 0) {
			pr_err("Couldn't read PERIPH_TYPE for base %x\n", base);
			return rc;
		}

		switch (val) {
		case AB_PERIPH_TYPE:
			chip->ab_base = base;
			break;
		default:
			pr_err("Unknown peripheral type 0x%x\n", val);
			return -EINVAL;
		}

	}

	return 0;
}

static int qpnp_amoled_regulator_probe(struct platform_device *pdev)
{
	int rc;
	struct device_node *node;
	struct qpnp_amoled *chip;

	node = pdev->dev.of_node;
	if (!node) {
		pr_err("No nodes defined\n");
		return -ENODEV;
	}
	printk("mahj: qpnp_amoled_regulator_probe\n");
	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	mutex_init(&chip->reg_lock);
	chip->dev = &pdev->dev;

	chip->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!chip->regmap) {
		dev_err(&pdev->dev, "Failed to get the regmap handle\n");
		rc = -EINVAL;
		goto error;
	}

	dev_set_drvdata(&pdev->dev, chip);

	rc = qpnp_amoled_parse_dt(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to parse DT params rc=%d\n", rc);
		goto error;
	}

	rc = charger_notify_init(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to init charger notify rc=%d\n", rc);
		goto error;
	}
	return 0;
error:

	return rc;
}

static int qpnp_amoled_regulator_remove(struct platform_device *pdev)
{
	struct qpnp_amoled *chip = dev_get_drvdata(&pdev->dev);

	if (chip->charger_notif.notifier_call)
		power_supply_unreg_notifier(&chip->charger_notif);

	destroy_workqueue(chip->charger_notify_wq);
	chip->charger_notify_wq = NULL;

	return 0;
}

static const struct of_device_id amoled_match_table[] = {
	{ .compatible = QPNP_AMOLED_REGULATOR_DRIVER_NAME_MOTO, },
	{ },
};

static struct platform_driver qpnp_amoled_regulator_driver = {
	.driver		= {
		.name		= QPNP_AMOLED_REGULATOR_DRIVER_NAME_MOTO,
		.of_match_table	= amoled_match_table,
	},
	.probe		= qpnp_amoled_regulator_probe,
	.remove		= qpnp_amoled_regulator_remove,
};

static int __init qpnp_amoled_regulator_init(void)
{
	return platform_driver_register(&qpnp_amoled_regulator_driver);
}

static void __exit qpnp_amoled_regulator_exit(void)
{
	platform_driver_unregister(&qpnp_amoled_regulator_driver);
}

MODULE_DESCRIPTION("QPNP AMOLED regulator driver moto");
MODULE_LICENSE("GPL v2");

module_init(qpnp_amoled_regulator_init);
module_exit(qpnp_amoled_regulator_exit);
