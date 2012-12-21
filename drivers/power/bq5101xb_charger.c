/*
 * Copyright (C) 2012 Motorola Mobility LLC
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
 *
 */
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/param.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/bq5101xb_charger.h>
#include <linux/printk.h>

#define BQ5101XB_TEMP_HYS 2
#define BQ5101XB_CHRG_CMPLT_SOC 100
#define BQ5101XB_PSY_NAME "wireless"

struct bq5101xb_chip {
	struct device *dev;
	unsigned int state;
	unsigned int irq_num;
	struct delayed_work bq5101xb_work;
	struct power_supply wl_psy;
	struct power_supply *batt_psy;
	struct notifier_block psy_notifier;
};

enum bq5101xb_state {
	BQ5101XB_WAIT = 0,
	BQ5101XB_RUNNING,
	BQ5101XB_OUT_OF_TEMP,
	BQ5101XB_CHRG_CMPLT,
	BQ5101XB_WIRED_CONN,
};

static int bq5101xb_psy_notify(struct notifier_block *nb,
			       unsigned long status, void *unused)
{
	struct bq5101xb_chip *chip;
	struct power_supply *psy;

	psy = power_supply_get_by_name(BQ5101XB_PSY_NAME);
	if (!psy) {
		pr_err("bq5101xb_psy_notify No PSY!\n");
		return 0;
	}

	chip = dev_get_drvdata(psy->dev->parent);
	if (chip)
		schedule_delayed_work(&chip->bq5101xb_work,
				      msecs_to_jiffies(0));
	else
		pr_err("bq5101xb_psy_notify No Chip!\n");

	return 0;
}

static void bq5101xb_set_pins(struct bq5101xb_charger_platform_data *pdata,
			      u8 en1, u8 en2, u8 term, u8 fault)
{
	if (pdata) {
		if (pdata->en1_pin >= 0)
			gpio_set_value(pdata->en1_pin, en1);

		if (pdata->en2_pin >= 0)
			gpio_set_value(pdata->en2_pin, en2);

		if (pdata->ts_ctrl_term_b_pin >= 0)
			gpio_set_value(pdata->ts_ctrl_term_b_pin, term);

		if (pdata->ts_ctrl_fault_pin >= 0)
			gpio_set_value(pdata->ts_ctrl_fault_pin, fault);
	}
}

static int bq5101xb_get_batt_info(struct power_supply *psy,
				  enum power_supply_property psp, int *data)
{
	union power_supply_propval ret = {0,};

	if (psy) {
		if (psy->get_property) {
			if (!psy->get_property(psy, psp, &ret)) {
				*data = ret.intval;
				return 0;
			}
		}
	}
	pr_err("bq5101xb_get_batt_info: get_prop Fail!\n");
	return 1;
}

static void bq5101xb_worker(struct work_struct *work)
{
	int batt_temp;
	int batt_volt;
	int batt_soc;
	int powered = 0;
	int wired = 0;
	int i;
	struct delayed_work *dwork;
	struct bq5101xb_chip *chip;
	struct bq5101xb_charger_platform_data *pdata;
	struct blocking_notifier_head *ntfy_hd;
	struct notifier_block *batt_ntfy;

	dwork = to_delayed_work(work);
	chip = container_of(dwork, struct bq5101xb_chip, bq5101xb_work);
	pdata = chip->dev->platform_data;

	if (pdata->chrg_b_pin > 0)
		powered = !gpio_get_value(pdata->chrg_b_pin);
	else if (pdata->check_powered)
		powered = pdata->check_powered();

	if (pdata->check_wired)
		wired = pdata->check_wired();

	if (!chip->batt_psy) {
		for (i = 0; i < pdata->num_supplies; i++) {
			chip->batt_psy =
				power_supply_get_by_name(pdata->supply_list[i]);

			if (!chip->batt_psy) {
				dev_err(chip->dev,
					"Batt PSY Not Found\n");
				continue;
			}

			/* Confirm a batt psy */
			if (chip->batt_psy->type != POWER_SUPPLY_TYPE_BATTERY)
				chip->batt_psy = NULL;

			if (chip->batt_psy) {
				ntfy_hd = &chip->batt_psy->notify_head;
				batt_ntfy = &chip->psy_notifier;
				blocking_notifier_chain_register(ntfy_hd,
								 batt_ntfy);
				break;
			}
		}
	}

	if (chip->batt_psy) {
		if (bq5101xb_get_batt_info(chip->batt_psy,
					   POWER_SUPPLY_PROP_TEMP,
					   &batt_temp)) {
			dev_err(chip->dev, "Error Reading Temperature\n");
			return;
		}
		/* Convert Units to Celsius */
		batt_temp /= 10;

		if (bq5101xb_get_batt_info(chip->batt_psy,
					   POWER_SUPPLY_PROP_VOLTAGE_NOW,
					   &batt_volt)) {
			dev_err(chip->dev, "Error Reading Voltage\n");
			return;
		}

		if (bq5101xb_get_batt_info(chip->batt_psy,
					   POWER_SUPPLY_PROP_CAPACITY,
					   &batt_soc)) {
			dev_err(chip->dev, "Error Reading Capacity\n");
			return;
		}
	} else {
		dev_err(chip->dev, "batt_psy not found\n");
		schedule_delayed_work(&chip->bq5101xb_work,
				      msecs_to_jiffies(100));
		return;
	}

	dev_dbg(chip->dev, "State Before = %d\n", chip->state);

	switch (chip->state) {
	case BQ5101XB_WAIT:
		if (wired && (pdata->priority == BQ5101XB_WIRED)) {
			bq5101xb_set_pins(pdata, 1, 1, 0, 0);
			chip->state = BQ5101XB_WIRED_CONN;
		} else if (powered)
			chip->state = BQ5101XB_RUNNING;
		break;
	case BQ5101XB_WIRED_CONN:
		if (!wired) {
			bq5101xb_set_pins(pdata, 0, 0, 1, 0);
			chip->state = BQ5101XB_WAIT;
		}
		break;
	case BQ5101XB_RUNNING:
		if (wired && (pdata->priority == BQ5101XB_WIRED)) {
			bq5101xb_set_pins(pdata, 1, 1, 0, 0);
			chip->state = BQ5101XB_WIRED_CONN;
		} else if (!powered) {
			bq5101xb_set_pins(pdata, 0, 0, 1, 0);
			chip->state = BQ5101XB_WAIT;
		} else if ((batt_temp >= pdata->hot_temp) ||
			   (batt_temp <= pdata->cold_temp)) {
			bq5101xb_set_pins(pdata, 0, 0, 1, 1);
			chip->state = BQ5101XB_OUT_OF_TEMP;
		} else if (batt_soc >= BQ5101XB_CHRG_CMPLT_SOC) {
			bq5101xb_set_pins(pdata, 1, 1, 0, 0);
			chip->state = BQ5101XB_CHRG_CMPLT;
		}
		break;
	case BQ5101XB_OUT_OF_TEMP:
		if (wired && (pdata->priority == BQ5101XB_WIRED)) {
			bq5101xb_set_pins(pdata, 1, 1, 0, 0);
			chip->state = BQ5101XB_WIRED_CONN;
		} else if ((batt_temp < (pdata->hot_temp -
					 BQ5101XB_TEMP_HYS)) ||
			   (batt_temp > (pdata->cold_temp +
					 BQ5101XB_TEMP_HYS))) {
			if (batt_soc >= BQ5101XB_CHRG_CMPLT_SOC) {
				bq5101xb_set_pins(pdata, 1, 1, 0, 0);
				chip->state = BQ5101XB_CHRG_CMPLT;
			} else {
				bq5101xb_set_pins(pdata, 0, 0, 1, 0);
				chip->state = BQ5101XB_RUNNING;
			}
		}
		break;
	case BQ5101XB_CHRG_CMPLT:
		if (wired && (pdata->priority == BQ5101XB_WIRED)) {
			bq5101xb_set_pins(pdata, 1, 1, 0, 0);
			chip->state = BQ5101XB_WIRED_CONN;
		} else if ((batt_temp >= pdata->hot_temp) ||
			   (batt_temp <= pdata->cold_temp)) {
			bq5101xb_set_pins(pdata, 0, 0, 1, 1);
			chip->state = BQ5101XB_OUT_OF_TEMP;
		} else if ((batt_soc <= pdata->resume_soc) ||
			   (batt_volt <= pdata->resume_vbatt)){
			bq5101xb_set_pins(pdata, 0, 0, 1, 0);
			chip->state = BQ5101XB_RUNNING;
		}
		break;
	}

	dev_dbg(chip->dev, "State After = %d\n", chip->state);

	return;
}

static enum power_supply_property bq5101xb_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
};

static int bq5101xb_get_property(struct power_supply *psy,
				 enum power_supply_property psp,
				 union power_supply_propval *val)
{
	struct bq5101xb_chip *chip;

	if (!psy || !psy->dev || !val) {
		pr_err("bq5101xb_get_property NO dev!\n");
		return -ENODEV;
	}

	chip = dev_get_drvdata(psy->dev->parent);
	if (!chip) {
		pr_err("bq5101xb_get_property NO dev chip!\n");
		return -ENODEV;
	}

	val->intval = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		if ((chip->state != BQ5101XB_WAIT) &&
		    (chip->state != BQ5101XB_WIRED_CONN))
			val->intval = 1;
	case POWER_SUPPLY_PROP_ONLINE:
		if (chip->state == BQ5101XB_RUNNING)
			val->intval = 1;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int __devinit bq5101xb_charger_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct bq5101xb_chip *chip;
	const struct bq5101xb_charger_platform_data *pdata
				= pdev->dev.platform_data;

	if ((pdata->en1_pin < 0) &&
	    (pdata->en2_pin < 0) &&
	    (pdata->ts_ctrl_term_b_pin < 0) &&
	    (pdata->ts_ctrl_fault_pin < 0) &&
	    (pdata->chrg_b_pin < 0)) {
		ret = -ENODEV;
		goto fail;
	}

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		ret = -ENOMEM;
		goto fail;
	}

	chip->state = BQ5101XB_WAIT;
	INIT_DELAYED_WORK(&chip->bq5101xb_work, bq5101xb_worker);

	chip->wl_psy.name = BQ5101XB_PSY_NAME;
	chip->wl_psy.type = POWER_SUPPLY_TYPE_WIRELESS;
	if (pdata->num_supplies) {
		chip->wl_psy.supplied_to = pdata->supply_list;
		chip->wl_psy.num_supplicants = pdata->num_supplies;
	}
	chip->wl_psy.properties = bq5101xb_props;
	chip->wl_psy.num_properties = ARRAY_SIZE(bq5101xb_props);
	chip->wl_psy.get_property = bq5101xb_get_property;
	ret = power_supply_register(&pdev->dev, &chip->wl_psy);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"power_supply_register wireless failed ret = %d\n",
			ret);
		goto fail_free_dev;
	}

	chip->psy_notifier.notifier_call = bq5101xb_psy_notify;

	if (pdata->en1_pin >= 0) {
		ret = gpio_request_one(pdata->en1_pin,
				       GPIOF_OUT_INIT_LOW,
				       "bq5101xb_en1");
		if (ret) {
			dev_err(&pdev->dev,
				"Failed to request en1_pin: %d\n",
				ret);
			goto fail_free_dev;
		}
	}

	if (pdata->en2_pin >= 0) {
		ret = gpio_request_one(pdata->en2_pin,
				       GPIOF_OUT_INIT_LOW,
				       "bq5101xb_en2");
		if (ret) {
			dev_err(&pdev->dev,
				"Failed to request en2_pin: %d\n",
				ret);
			goto fail_free_en1;
		}
	}

	if (pdata->ts_ctrl_term_b_pin >= 0) {
		ret = gpio_request_one(pdata->ts_ctrl_term_b_pin,
				       GPIOF_OUT_INIT_HIGH,
				       "bq5101xb_ts_ctrl_term_b");
		if (ret) {
			dev_err(&pdev->dev,
				"Failed to request ts_ctrl_term_b_pin: %d\n",
				ret);
			goto fail_free_en2;
		}
	}

	if (pdata->ts_ctrl_fault_pin >= 0) {
		ret = gpio_request_one(pdata->ts_ctrl_fault_pin,
				       GPIOF_OUT_INIT_LOW,
				       "bq5101xb_ts_ctrl_fault");
		if (ret) {
			dev_err(&pdev->dev,
				"Failed to request ts_ctrl_fault_pin: %d\n",
				ret);
			goto fail_free_term;
		}
	}

	if (pdata->chrg_b_pin >= 0) {
		ret = gpio_request_one(pdata->chrg_b_pin,
				       GPIOF_IN,
				       "bq5101xb_chrg_b");
		if (ret) {
			dev_err(&pdev->dev,
				"Failed to request chrg_b_pin: %d\n",
				ret);
			goto fail_free_fault;
		}
	}

	platform_set_drvdata(pdev, chip);
	chip->dev = &pdev->dev;

	schedule_delayed_work(&chip->bq5101xb_work,
			      msecs_to_jiffies(100));

	return 0;

fail_free_fault:
	if (pdata->ts_ctrl_fault_pin >= 0)
		gpio_free(pdata->ts_ctrl_fault_pin);
fail_free_term:
	if (pdata->ts_ctrl_term_b_pin >= 0)
		gpio_free(pdata->ts_ctrl_term_b_pin);
fail_free_en2:
	if (pdata->en2_pin >= 0)
		gpio_free(pdata->en2_pin);
fail_free_en1:
	if (pdata->en1_pin >= 0)
		gpio_free(pdata->en1_pin);
fail_free_dev:
	kfree(chip);
fail:
	return ret;
}

static int __devexit bq5101xb_charger_remove(struct platform_device *pdev)
{
	struct bq5101xb_info *chip = platform_get_drvdata(pdev);
	const struct bq5101xb_charger_platform_data *pdata
		= pdev->dev.platform_data;

	if (pdata->chrg_b_pin >= 0)
		gpio_free(pdata->chrg_b_pin);
	if (pdata->ts_ctrl_fault_pin >= 0)
		gpio_free(pdata->ts_ctrl_fault_pin);
	if (pdata->ts_ctrl_term_b_pin >= 0)
		gpio_free(pdata->ts_ctrl_term_b_pin);
	if (pdata->en2_pin >= 0)
		gpio_free(pdata->en2_pin);
	if (pdata->en1_pin >= 0)
		gpio_free(pdata->en1_pin);
	kfree(chip);
	return 0;
}

MODULE_ALIAS("platform:bq5101xb_charger");

static struct platform_driver bq5101xb_charger_driver = {
	.driver = {
		.name = BQ5101XB_DRV_NAME,
	},
	.probe    = bq5101xb_charger_probe,
	.remove   = __devexit_p(bq5101xb_charger_remove),
};

static int __init bq5101xb_charger_init(void)
{
	return platform_driver_register(&bq5101xb_charger_driver);
}

static void __exit bq5101xb_charger_exit(void)
{
	platform_driver_unregister(&bq5101xb_charger_driver);
}

module_init(bq5101xb_charger_init);
module_exit(bq5101xb_charger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Motorola Mobility LLC");
MODULE_DESCRIPTION("bq5101xB driver");
