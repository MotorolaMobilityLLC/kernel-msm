 /*
 * Copyright(c) 2012, LG Electronics Inc. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/platform_data/battery_temp_ctrl.h>

struct batt_temp_chip {
	struct delayed_work monitor_work;
	struct batt_temp_pdata *pdata;
};

#define INIT_VAL 1000
static int fake_temp = INIT_VAL;
static ssize_t batt_temp_fake_temp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int level;

	if (!count)
		return -EINVAL;

	level = simple_strtoul(buf, NULL, 10);
	if (level > 1000)
		fake_temp = 1000-level;
	else
		fake_temp = level;

	pr_info("%s: fake_temp = %d\n", __func__, fake_temp);
	return count;
}
DEVICE_ATTR(fake_temp, 0664, NULL, batt_temp_fake_temp_store);

static int determin_batt_temp_state(struct batt_temp_pdata *pdata,
			int batt_temp, int batt_mvolt)
{
	static int temp_state = TEMP_LEVEL_NORMAL;

	if (pdata->is_ext_power()) {
		pr_debug("%s : is_ext_power = true\n", __func__);

		if (batt_temp >= pdata->temp_level[TEMP_LEVEL_POWER_OFF]) {
			temp_state = TEMP_LEVEL_POWER_OFF;
		} else if (batt_temp >= pdata->temp_level[TEMP_LEVEL_HOT_STOPCHARGING]) {
			temp_state = TEMP_LEVEL_HOT_STOPCHARGING;
		} else if (batt_temp >= pdata->temp_level[TEMP_LEVEL_DECREASING]) {
			if (batt_mvolt > pdata->thr_mvolt
					|| temp_state == TEMP_LEVEL_HOT_STOPCHARGING)
				temp_state = TEMP_LEVEL_HOT_STOPCHARGING;
			else
				temp_state = TEMP_LEVEL_DECREASING;
		} else if (batt_temp > pdata->temp_level[TEMP_LEVEL_HOT_RECHARGING]) {
			if (temp_state == TEMP_LEVEL_HOT_STOPCHARGING
					|| temp_state == TEMP_LEVEL_DECREASING)
				temp_state = TEMP_LEVEL_HOT_STOPCHARGING;
			else if (temp_state == TEMP_LEVEL_DECREASING)
				temp_state = TEMP_LEVEL_DECREASING;
			else
				temp_state = TEMP_LEVEL_NORMAL;
		} else if (batt_temp >= pdata->temp_level[TEMP_LEVEL_COLD_RECHARGING]) {
			if (temp_state == TEMP_LEVEL_HOT_STOPCHARGING
					|| temp_state == TEMP_LEVEL_DECREASING)
				temp_state = TEMP_LEVEL_HOT_RECHARGING;
			else if (temp_state == TEMP_LEVEL_COLD_STOPCHARGING)
				temp_state = TEMP_LEVEL_COLD_RECHARGING;
			else
				temp_state = TEMP_LEVEL_NORMAL;
		} else if (batt_temp <= pdata->temp_level[TEMP_LEVEL_COLD_STOPCHARGING]) {
			temp_state = TEMP_LEVEL_COLD_STOPCHARGING;
		} else if (batt_temp < pdata->temp_level[TEMP_LEVEL_COLD_RECHARGING]) {
			if (temp_state == TEMP_LEVEL_COLD_STOPCHARGING)
				temp_state = TEMP_LEVEL_COLD_STOPCHARGING;
			else
				temp_state = TEMP_LEVEL_NORMAL;
		} else if (batt_temp <= pdata->temp_level[TEMP_LEVEL_HOT_RECHARGING]) {
			if (temp_state == TEMP_LEVEL_HOT_STOPCHARGING)
				temp_state = TEMP_LEVEL_HOT_RECHARGING;
			else if (temp_state == TEMP_LEVEL_COLD_STOPCHARGING)
				temp_state = TEMP_LEVEL_COLD_RECHARGING;
			else
				temp_state = TEMP_LEVEL_NORMAL;
		} else {
			temp_state = TEMP_LEVEL_NORMAL;
		}
	} else {
		pr_debug("%s : is_ext_power = false\n", __func__);

		if (batt_temp >= pdata->temp_level[TEMP_LEVEL_POWER_OFF])
			temp_state = TEMP_LEVEL_POWER_OFF;
		else if (batt_temp >= pdata->temp_level[TEMP_LEVEL_WARNINGOVERHEAT])
			temp_state = TEMP_LEVEL_WARNINGOVERHEAT;
		else
			temp_state = TEMP_LEVEL_NORMAL;
	}

	return temp_state;
}

static void batt_temp_monitor_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct batt_temp_chip *chip = container_of(dwork,
					struct batt_temp_chip, monitor_work);
	struct batt_temp_pdata *pdata = chip->pdata;
	static struct power_supply *psy = NULL;
	int batt_temp = 0;
	int batt_mvolt = 0;
	int temp_state = TEMP_LEVEL_NORMAL;
	static int temp_old_state = TEMP_LEVEL_NORMAL;
	union power_supply_propval ret = {0,};
	int rc;

	if (!psy) {
		psy = power_supply_get_by_name("battery");
		if (!psy) {
			pr_err("%s: failed to get power supply\n", __func__);
			return;
		}
	}

	rc = psy->get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &ret);
	if (rc) {
		pr_err("%s: failed to get voltage\n", __func__);
		return;
	}
	batt_mvolt = ret.intval/1000;

	rc = psy->get_property(psy, POWER_SUPPLY_PROP_TEMP, &ret);
	if (rc) {
		pr_err("%s: failed to get temperature\n", __func__);
		return;
	}

	if (fake_temp != INIT_VAL)
		batt_temp = fake_temp;
	else
		batt_temp = ret.intval;

	temp_state = determin_batt_temp_state(pdata, batt_temp, batt_mvolt);
	pr_debug("%s: batt_temp = %d batt_mvolt = %d state = %d\n", __func__,
			batt_temp, batt_mvolt, temp_state);

	switch (temp_state) {
	case TEMP_LEVEL_POWER_OFF:
	case TEMP_LEVEL_HOT_STOPCHARGING:
	case TEMP_LEVEL_COLD_STOPCHARGING:
		pr_info("%s: stop charging!! state = %d temp = %d mvolt = %d \n",
				__func__, temp_state, batt_temp, batt_mvolt);
		pdata->set_chg_i_limit(pdata->i_restore);
		pdata->disable_charging();
		pdata->set_health_state(POWER_SUPPLY_HEALTH_OVERHEAT, 0);
		break;
	case TEMP_LEVEL_WARNINGOVERHEAT:
		pdata->set_health_state(POWER_SUPPLY_HEALTH_OVERHEAT, 0);
		break;
	case TEMP_LEVEL_DECREASING:
		pr_info("%s: decrease current!! state = %d temp = %d mvolt = %d \n",
				__func__, temp_state, batt_temp, batt_mvolt);
		pdata->set_chg_i_limit(pdata->i_decrease);
		pdata->set_health_state(POWER_SUPPLY_HEALTH_GOOD,
				pdata->i_decrease);
		break;
	case TEMP_LEVEL_COLD_RECHARGING:
	case TEMP_LEVEL_HOT_RECHARGING:
		pr_info("%s: restart charging!! state = %d temp = %d mvolt = %d \n",
				__func__, temp_state, batt_temp, batt_mvolt);
		pdata->set_chg_i_limit(pdata->i_restore);
		pdata->enable_charging();
		pdata->set_health_state(POWER_SUPPLY_HEALTH_GOOD,
				pdata->i_restore);
		break;
	case TEMP_LEVEL_NORMAL:
		if (temp_old_state != TEMP_LEVEL_NORMAL) {
			pdata->set_chg_i_limit(pdata->i_restore);
			pdata->enable_charging();
		}
		pdata->set_health_state(POWER_SUPPLY_HEALTH_GOOD, 0);
	default:
		break;
	}

	temp_old_state=temp_state;
	power_supply_changed(psy);
	schedule_delayed_work(&chip->monitor_work,
			round_jiffies_relative(msecs_to_jiffies(pdata->update_time)));

}

static int batt_temp_ctrl_suspend(struct device *dev)
{
	struct batt_temp_chip *chip = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&chip->monitor_work);
	return 0;
}

static int batt_temp_ctrl_resume(struct device *dev)
{
	struct batt_temp_chip *chip = dev_get_drvdata(dev);

	schedule_delayed_work(&chip->monitor_work,
			  round_jiffies_relative(msecs_to_jiffies
				 (30000)));

	return 0;
}

static int batt_temp_ctrl_probe(struct platform_device *pdev)
{
	struct batt_temp_chip *chip;
	struct batt_temp_pdata *pdata = pdev->dev.platform_data;

	pr_info("%s\n", __func__);

	if (!pdata) {
		pr_err("%s: no pdata\n", __func__);
		return -ENODEV;
	}

	chip = kzalloc(sizeof(struct batt_temp_chip), GFP_KERNEL);
	if (!chip) {
		pr_err("%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	chip->pdata = pdata;
	platform_set_drvdata(pdev, chip);

	device_create_file(&pdev->dev, &dev_attr_fake_temp);

	INIT_DELAYED_WORK(&chip->monitor_work,
					batt_temp_monitor_work);
	schedule_delayed_work(&chip->monitor_work,
				  round_jiffies_relative(msecs_to_jiffies
						(pdata->update_time)));
	return 0;
}

static const struct dev_pm_ops batt_temp_ops = {
	.suspend = batt_temp_ctrl_suspend,
	.resume = batt_temp_ctrl_resume,
};

static struct platform_driver this_driver = {
	.probe  = batt_temp_ctrl_probe,
	.driver = {
		.name  = "batt_temp_ctrl",
		.owner = THIS_MODULE,
		.pm = &batt_temp_ops,
	},
};

int __init batt_temp_ctrl_init(void)
{
	pr_info("batt_temp_ctrl_init \n");
	return platform_driver_register(&this_driver);
}

late_initcall(batt_temp_ctrl_init);

MODULE_DESCRIPTION("Battery Temperature Control Driver");
MODULE_AUTHOR("ChoongRyeol Lee <choongryeol.lee@lge.com>");
MODULE_LICENSE("GPL");
