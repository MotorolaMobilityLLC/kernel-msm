/*
* Copyright(c) 2013, LGE Inc. All rights reserved.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
*/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/power/bq51013b_charger.h>

#define BQ51013B_NAME "bq51013b"

struct bq51013b_chip {
	struct device *dev;
	struct power_supply wlc_psy;
	struct power_supply *ac_psy;
	unsigned int chg_ctrl_gpio;
	unsigned int wlc_int_gpio;
	int current_ma;
	int present;
	int online;
};

static const struct platform_device_id bq51013b_id[] = {
	{BQ51013B_NAME, 0},
	{},
};

static struct of_device_id bq51013b_match[] = {
	{ .compatible = "ti,bq51013b", },
	{}
};

static enum power_supply_property pm_power_props_wireless[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
};

static char *pm_power_supplied_to[] = {
	"ac",
#ifdef CONFIG_TOUCHSCREEN_CHARGER_NOTIFY
	"touch",
#endif
};

static int bq51013b_charger_is_present(struct bq51013b_chip *chip)
{
	return !gpio_get_value(chip->wlc_int_gpio);
}

static int pm_power_get_property_wireless(struct power_supply *psy,
					  enum power_supply_property psp,
					  union power_supply_propval *val)
{
	struct bq51013b_chip *chip = container_of(psy, struct bq51013b_chip,
						wlc_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = bq51013b_charger_is_present(chip);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->online;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = chip->current_ma * 1000;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static irqreturn_t bq51013b_irq_handler(int irq, void *data)
{
	struct bq51013b_chip *chip = data;
	int present;

	if (!chip->ac_psy)
		chip->ac_psy = power_supply_get_by_name("ac");

	if (!chip->ac_psy)
		return IRQ_HANDLED;

	present = bq51013b_charger_is_present(chip);

	pr_debug("%s: wlc present = %d\n", __func__, present);
	power_supply_set_present(chip->ac_psy, present);

	return IRQ_HANDLED;
}

static int pm_power_set_property_wireless(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	struct bq51013b_chip *chip = container_of(psy, struct bq51013b_chip,
						wlc_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		chip->online = val->intval;
		break;
	default:
		return -EINVAL;
	}

	power_supply_changed(&chip->wlc_psy);
	return 0;
}

static int bq51013b_gpio_init(struct bq51013b_chip *chip)
{
	int ret = 0;

	ret =  gpio_request_one(chip->chg_ctrl_gpio, GPIOF_OUT_INIT_LOW,
			"chg_ctrl_gpio");
	if (ret) {
		pr_err("%s: failed to request chg_ctrl_gpio\n", __func__);
		goto err_chg_ctrl_gpio;
	}

	ret =  gpio_request_one(chip->wlc_int_gpio, GPIOF_DIR_IN,
			"wlc_int_gpio");
	if (ret) {
		pr_err("%s: failed to request wlc_int_gpio\n", __func__);
		goto err_wlc_int;
	}

err_wlc_int:
	gpio_free(chip->chg_ctrl_gpio);
err_chg_ctrl_gpio:
	return ret;

}

static int bq51013b_init_wlc_psy(struct bq51013b_chip *chip)
{
	int ret = 0;

	chip->wlc_psy.name = "wireless";
	chip->wlc_psy.type = POWER_SUPPLY_TYPE_WIRELESS;
	chip->wlc_psy.supplied_to = pm_power_supplied_to;
	chip->wlc_psy.num_supplicants = ARRAY_SIZE(pm_power_supplied_to);
	chip->wlc_psy.properties = pm_power_props_wireless;
	chip->wlc_psy.num_properties = ARRAY_SIZE(pm_power_props_wireless);
	chip->wlc_psy.get_property = pm_power_get_property_wireless;
	chip->wlc_psy.set_property = pm_power_set_property_wireless;

	ret = power_supply_register(chip->dev, &chip->wlc_psy);
	if (ret) {
		pr_err("%s: failed to register power_supply. ret = %d\n",
				__func__, ret);
		return ret;
	}

	return 0;
}

static int bq51013b_parse_dt(struct device_node *dev_node,
				struct bq51013b_chip *chip)
{
	int ret = 0;

	chip->chg_ctrl_gpio = of_get_named_gpio(dev_node,
					"ti,chg-ctrl-gpio", 0);
	if (chip->chg_ctrl_gpio < 0) {
		pr_err("%s: failed to get chg_ctrl_gpio\n", __func__);
		ret = chip->chg_ctrl_gpio;
		goto out;
	}

	chip->wlc_int_gpio = of_get_named_gpio(dev_node,
					"ti,wlc-int-gpio", 0);
	if (chip->wlc_int_gpio < 0) {
		pr_err("%s: failed to get wlc_int_gpio\n", __func__);
		ret = chip->chg_ctrl_gpio;
		goto out;
	}

	ret = of_property_read_u32(dev_node, "ti,chg-current-ma",
				   &(chip->current_ma));
	if (ret) {
		pr_err("%s: failed to get current_ma\n", __func__);
		goto out;
	}

	pr_info("%s: current_ma = %d\n", __func__, chip->current_ma);
out:
	return ret;
}

static int bq51013b_probe(struct platform_device *pdev)
{
	struct bq51013b_chip *chip;
	struct device_node *dev_node = pdev->dev.of_node;
	struct bq51013b_platform_data *pdata;
	int ret = 0;

	pr_info("%s: start\n", __func__);

	chip = kzalloc(sizeof(struct bq51013b_chip), GFP_KERNEL);
	if (!chip) {
		pr_err("%s: failed to alloc memory\n", __func__);
		return -ENOMEM;
	}

	if (dev_node) {
		ret = bq51013b_parse_dt(dev_node, chip);
		if (ret) {
			pr_err("failed to parse dt\n");
			goto err_parse_dt;
		}
	} else {
		pdata = pdev->dev.platform_data;
		chip->chg_ctrl_gpio = pdata->chg_ctrl_gpio;
		chip->wlc_int_gpio = pdata->wlc_int_gpio;
		chip->current_ma = pdata->current_ma;
	}

	chip->dev = &pdev->dev;

	ret = bq51013b_init_wlc_psy(chip);
	if (ret) {
		pr_err("%s: failed to init power supply", __func__);
		goto err_parse_dt;
	}

	ret = bq51013b_gpio_init(chip);
	if (ret) {
		pr_err("%s: gpio int failed", __func__);
		goto err_gpio_init;
	}

	ret = request_threaded_irq(gpio_to_irq(chip->wlc_int_gpio), NULL,
				bq51013b_irq_handler,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"bq51013b", chip);
	if (ret) {
		pr_err("%s: failed to request irq", __func__);
		goto err_req_irq;
	}
	enable_irq_wake(gpio_to_irq(chip->wlc_int_gpio));
	platform_set_drvdata(pdev, chip);

	return 0;

err_req_irq:
	gpio_free(chip->chg_ctrl_gpio);
	gpio_free(chip->wlc_int_gpio);
err_gpio_init:
	power_supply_unregister(&chip->wlc_psy);
err_parse_dt:
	kfree(chip);
	return ret;
}

static int bq51013b_remove(struct platform_device *pdev)
{
	struct bq51013b_chip *chip = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	free_irq(gpio_to_irq(chip->wlc_int_gpio), chip);
	gpio_free(chip->wlc_int_gpio);
	gpio_free(chip->chg_ctrl_gpio);
	power_supply_unregister(&chip->wlc_psy);
	kfree(chip);
	return 0;
}

static struct platform_driver bq51013b_driver = {
	.probe = bq51013b_probe,
	.remove = bq51013b_remove,
	.id_table = bq51013b_id,
	.driver = {
		.name = BQ51013B_NAME,
		.owner = THIS_MODULE,
		.of_match_table = bq51013b_match,
	},
};

static int __init bq51013b_init(void)
{
	return platform_driver_register(&bq51013b_driver);
}
module_init(bq51013b_init);

static void __exit bq51013b_exit(void)
{
	platform_driver_unregister(&bq51013b_driver);
}
module_exit(bq51013b_exit);

MODULE_AUTHOR("ChoongRyeol Lee <choongryeol.lee@lge.com>");
MODULE_DESCRIPTION("BQ51013B Wireless Charger Driver");
MODULE_LICENSE("GPL v2");
