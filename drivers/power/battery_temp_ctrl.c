 /*
 * Copyright(c) 2013, LGE Inc. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/of_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/suspend.h>

struct batt_tm_data {
	struct device *dev;
	struct power_supply tm_psy;
	struct power_supply *batt_psy;
	struct power_supply *ac_psy;
	struct qpnp_adc_tm_btm_param adc_param;
	struct delayed_work tm_work;
	struct wake_lock tm_wake_lock;
	struct tm_ctrl_data *warm_cfg;
	struct tm_ctrl_data *cold_cfg;
	struct notifier_block pm_notifier;
	int warm_cfg_size;
	int cold_cfg_size;
	int batt_vreg_uv;
	int low_batt_vreg_mv;
	int low_current_ma;
	int chg_online;
	int tm_noti_stat;
};

struct tm_ctrl_data {
	int thr;
	int action;
	int next_cool_thr;
	int next_warm_thr;
	int health;
};

enum {
	CHG_ENABLE,
	CHG_DECREASE,
	CHG_STOP,
	CHG_NONE,
};

static int power_supply_set_max_voltage(struct power_supply *psy, int limit)
{
	const union power_supply_propval ret = {limit,};

	if (psy->set_property)
		return psy->set_property(psy, POWER_SUPPLY_PROP_VOLTAGE_MAX,
								&ret);

	return -ENXIO;
}

static int power_supply_set_batt_health(struct power_supply *psy, int health)
{
	const union power_supply_propval ret = {health,};

	if (psy->set_property)
		return psy->set_property(psy, POWER_SUPPLY_PROP_HEALTH,
								&ret);

	return -ENXIO;
}

static int power_supply_set_chg_enable(struct power_supply *psy, int enable)
{
	const union power_supply_propval ret = {enable,};

	if (psy->set_property)
		return psy->set_property(psy,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &ret);

	return -ENXIO;
}

static void batt_tm_worker(struct work_struct *work)
{
	struct batt_tm_data *batt_tm =
		container_of(work, struct batt_tm_data, tm_work.work);
	int temp;
	int i;
	int tm_action;
	int batt_health;
	union power_supply_propval ret = {0,};
	int rc;

	if (batt_tm->tm_noti_stat >= ADC_TM_STATE_NUM) {
		pr_err("invalid notification %d\n", batt_tm->tm_noti_stat);
		goto out;
	}

	batt_tm->batt_psy->get_property(batt_tm->batt_psy,
				POWER_SUPPLY_PROP_TEMP, &ret);
	temp = ret.intval;

	pr_debug("temp = %d state = %s\n", temp,
		batt_tm->tm_noti_stat == ADC_TM_WARM_STATE ? "warm" : "cool");

	if (batt_tm->tm_noti_stat == ADC_TM_WARM_STATE) {
		i = batt_tm->warm_cfg_size - 1;
		while ((batt_tm->adc_param.high_temp
				< batt_tm->warm_cfg[i].thr) && (i > 0))
			i--;

		batt_tm->adc_param.low_temp =
					batt_tm->warm_cfg[i].next_cool_thr;
		batt_tm->adc_param.high_temp =
					batt_tm->warm_cfg[i].next_warm_thr;
		tm_action = batt_tm->warm_cfg[i].action;
		batt_health = batt_tm->warm_cfg[i].health;
	} else {
		i = 0;
		while ((batt_tm->adc_param.low_temp > batt_tm->cold_cfg[i].thr)
				&& (i < batt_tm->cold_cfg_size-1))
			i++;

		batt_tm->adc_param.low_temp =
					batt_tm->cold_cfg[i].next_cool_thr;
		batt_tm->adc_param.high_temp =
					batt_tm->cold_cfg[i].next_warm_thr;
		tm_action = batt_tm->cold_cfg[i].action;
		batt_health = batt_tm->cold_cfg[i].health;
	}

	power_supply_set_batt_health(batt_tm->ac_psy, batt_health);

	if (!batt_tm->chg_online)
		tm_action = CHG_NONE;

	switch (tm_action) {
	case CHG_ENABLE:
		pr_info("Enable charging. vbatt_max = %d\n",
					batt_tm->batt_vreg_uv/1000);
		power_supply_set_chg_enable(batt_tm->ac_psy, true);
		power_supply_set_max_voltage(batt_tm->ac_psy,
						batt_tm->batt_vreg_uv);
		break;
	case CHG_DECREASE:
		pr_info("Decrease current to %d, vbatt_max to %d\n",
						batt_tm->low_current_ma,
						batt_tm->low_batt_vreg_mv);
		power_supply_set_current_limit(batt_tm->ac_psy,
					batt_tm->low_current_ma * 1000);
		power_supply_set_max_voltage(batt_tm->ac_psy,
					batt_tm->low_batt_vreg_mv * 1000);
		break;
	case CHG_STOP:
		pr_info("Stop charging !!\n");
		power_supply_set_chg_enable(batt_tm->ac_psy, false);
		break;
	case CHG_NONE:
		pr_info("No charger.\n");
		break;
	default:
		break;
	}

	pr_info("set new threshold : low_temp = %d high_temp = %d\n",
					batt_tm->adc_param.low_temp,
					batt_tm->adc_param.high_temp);

	rc = qpnp_adc_tm_channel_measure(&batt_tm->adc_param);
	if (rc)
		pr_err("request ADC error\n");

out:
	wake_unlock(&batt_tm->tm_wake_lock);
}

static void batt_tm_ctrl_notification(enum qpnp_tm_state state, void *ctx)
{
	struct batt_tm_data *batt_tm = ctx;

	wake_lock(&batt_tm->tm_wake_lock);
	batt_tm->tm_noti_stat = state;
	schedule_delayed_work(&batt_tm->tm_work,
				msecs_to_jiffies(2000));
}

static int batt_tm_notification_start(struct batt_tm_data *batt_tm)
{
	int rc = 0;
	union power_supply_propval ret = {0,};

	rc = qpnp_adc_tm_is_ready();
	if (rc) {
		pr_err("qpnp_adc is not ready");
		return rc;
	}

	if (batt_tm->ac_psy && batt_tm->chg_online) {
		batt_tm->ac_psy->get_property(batt_tm->ac_psy,
					POWER_SUPPLY_PROP_VOLTAGE_MAX, &ret);
		batt_tm->batt_vreg_uv = ret.intval;
		power_supply_set_chg_enable(batt_tm->ac_psy, true);
		power_supply_set_max_voltage(batt_tm->ac_psy,
					batt_tm->batt_vreg_uv);
	}

	batt_tm->adc_param.low_temp =
				batt_tm->warm_cfg[0].next_cool_thr;
	batt_tm->adc_param.high_temp =
				batt_tm->warm_cfg[0].next_warm_thr;
	batt_tm->adc_param.state_request =
				ADC_TM_HIGH_LOW_THR_ENABLE;
	batt_tm->adc_param.timer_interval = ADC_MEAS1_INTERVAL_8S;
	batt_tm->adc_param.btm_ctx = batt_tm;
	batt_tm->adc_param.threshold_notification =
					batt_tm_ctrl_notification;
	batt_tm->adc_param.channel = LR_MUX1_BATT_THERM;

	rc = qpnp_adc_tm_channel_measure(&batt_tm->adc_param);
	if (rc)
		pr_err("request ADC error %d\n", rc);

	return rc;
}

static int batt_tm_notification_end(struct batt_tm_data *batt_tm)
{
	int ret;

	batt_tm->adc_param.state_request =
				ADC_TM_HIGH_LOW_THR_DISABLE;
	batt_tm->adc_param.threshold_notification =
				batt_tm_ctrl_notification;
	batt_tm->adc_param.channel = LR_MUX1_BATT_THERM;

	ret = qpnp_adc_tm_channel_measure(&batt_tm->adc_param);
	if (ret)
		pr_err("request ADC error %d\n", ret);

	cancel_delayed_work_sync(&batt_tm->tm_work);
	if (wake_lock_active(&batt_tm->tm_wake_lock))
		wake_unlock(&batt_tm->tm_wake_lock);

	return ret;
}

static void batt_tm_external_power_changed(struct power_supply *psy)
{
	struct batt_tm_data *batt_tm = container_of(psy,
					struct batt_tm_data, tm_psy);
	int chg_online;

	chg_online = power_supply_is_system_supplied();
	if (batt_tm->chg_online ^ chg_online) {
		batt_tm_notification_end(batt_tm);
		power_supply_set_batt_health(batt_tm->ac_psy,
				POWER_SUPPLY_HEALTH_GOOD);
		batt_tm->chg_online = chg_online;
		batt_tm_notification_start(batt_tm);
	}
}

static enum power_supply_property power_props_batt_therm[] = {
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static int batt_tm_get_property(struct power_supply *psy,
					  enum power_supply_property psp,
					  union power_supply_propval *val)
{
	struct batt_tm_data *batt_tm = container_of(psy, struct batt_tm_data,
						tm_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = batt_tm->low_current_ma;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int batt_tm_psy_init(struct batt_tm_data *batt_tm)
{
	int ret = 0;

	batt_tm->batt_psy = power_supply_get_by_name("battery");
	if (!batt_tm->batt_psy) {
		pr_err("battery supply not found\n");
		return -EPROBE_DEFER;
	}

	batt_tm->ac_psy = power_supply_get_by_name("ac");
	if (!batt_tm->ac_psy) {
		pr_err("ac supply not found\n");
		return -EPROBE_DEFER;
	}

	batt_tm->tm_psy.name = "batt_therm";
	batt_tm->tm_psy.type = POWER_SUPPLY_TYPE_BMS;
	batt_tm->tm_psy.properties = power_props_batt_therm;
	batt_tm->tm_psy.num_properties = ARRAY_SIZE(power_props_batt_therm);
	batt_tm->tm_psy.get_property = batt_tm_get_property;
	batt_tm->tm_psy.external_power_changed =
				batt_tm_external_power_changed;

	ret = power_supply_register(batt_tm->dev, &batt_tm->tm_psy);
	if (ret) {
		pr_err("failed to register power_supply. ret=%d.\n", ret);
		return ret;
	}

	return 0;
}

#define KELVIN_TO_CELSIUS(x) ((x)-2730)
static int batt_tm_parse_dt(struct device_node *np,
				struct batt_tm_data *batt_tm)
{
	int *buf;
	int ret;
	int i;

	ret = of_property_read_u32(np, "tm,low-vbatt-mv",
				&batt_tm->low_batt_vreg_mv);
	if (ret) {
		pr_err("failed to get tm,low-vbatt-mv\n");
		goto error;
	}

	ret = of_property_read_u32(np, "tm,low-current-ma",
				&batt_tm->low_current_ma);
	if (ret) {
		pr_err("failed to get tm,low-current-ma\n");
		goto error;
	}

	ret = of_property_read_u32(np, "tm,warm-cfg-size",
				&batt_tm->warm_cfg_size);
	if (ret) {
		pr_err("failed to get tm,warm-cfg-size\n");
		goto error;
	}

	batt_tm->warm_cfg = kzalloc(sizeof(struct tm_ctrl_data) *
				batt_tm->warm_cfg_size, GFP_KERNEL);
	if (!batt_tm->warm_cfg) {
		pr_err("Unable to allocate memory\n");
		ret = -ENOMEM;
		goto error;
	}

	buf = kzalloc(sizeof(int32_t) * batt_tm->warm_cfg_size * 5,
						GFP_KERNEL);
	if (!buf) {
		pr_err("Unable to allocate memory\n");
		ret = -ENOMEM;
		goto alloc_fail0;
	}

	ret = of_property_read_u32_array(np, "tm,warm-cfg",
				buf, batt_tm->warm_cfg_size * 5);
	if (ret) {
		pr_err("failed to get tm,warm-cfg\n");
		kfree(buf);
		goto alloc_fail0;
	}

	for(i = 0; i < batt_tm->warm_cfg_size; i++) {
		batt_tm->warm_cfg[i].thr = KELVIN_TO_CELSIUS(buf[i*5]);
		batt_tm->warm_cfg[i].action = buf[i*5+1];
		batt_tm->warm_cfg[i].next_cool_thr =
					KELVIN_TO_CELSIUS(buf[i*5+2]);
		batt_tm->warm_cfg[i].next_warm_thr =
					KELVIN_TO_CELSIUS(buf[i*5+3]);
		batt_tm->warm_cfg[i].health = buf[i*5+4];
	}

	kfree(buf);

	ret = of_property_read_u32(np, "tm,cold-cfg-size",
				&batt_tm->cold_cfg_size);
	if (ret) {
		pr_err("failed to get tm,cold-cfg-size\n");
		goto alloc_fail0;
	}

	batt_tm->cold_cfg = kzalloc(sizeof(struct tm_ctrl_data) *
				batt_tm->cold_cfg_size, GFP_KERNEL);
	if (!batt_tm->cold_cfg) {
		pr_err("Unable to allocate memory\n");
		ret = -ENOMEM;
		goto  alloc_fail0;
	}

	buf = kzalloc(sizeof(int32_t) * batt_tm->cold_cfg_size * 5,
						GFP_KERNEL);
	if (!buf) {
		pr_err("Unable to allocate memory\n");
		ret = -ENOMEM;
		goto  alloc_fail1;
	}

	ret = of_property_read_u32_array(np, "tm,cold-cfg",
				buf, batt_tm->cold_cfg_size * 5);
	if (ret) {
		pr_err("failed to get tm,cold-cfg\n");
		kfree(buf);
		goto alloc_fail1;
	}

	for(i = 0; i < batt_tm->cold_cfg_size; i++) {
		batt_tm->cold_cfg[i].thr = KELVIN_TO_CELSIUS(buf[i*5]);
		batt_tm->cold_cfg[i].action = buf[i*5+1];
		batt_tm->cold_cfg[i].next_cool_thr =
					KELVIN_TO_CELSIUS(buf[i*5+2]);
		batt_tm->cold_cfg[i].next_warm_thr =
					KELVIN_TO_CELSIUS(buf[i*5+3]);
		batt_tm->cold_cfg[i].health = buf[i*5+4];
	}

	kfree(buf);

	return 0;

alloc_fail1:
	kfree(batt_tm->cold_cfg);
alloc_fail0:
	kfree(batt_tm->warm_cfg);
error:
	return ret;
}

static int batt_tm_pm_notifier(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	struct batt_tm_data *batt_tm = container_of(notifier,
				struct batt_tm_data, pm_notifier);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		if (!batt_tm->chg_online)
			batt_tm_notification_end(batt_tm);
		break;
	case PM_POST_SUSPEND:
		if (!batt_tm->chg_online)
			batt_tm_notification_start(batt_tm);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static int batt_tm_ctrl_probe(struct platform_device *pdev)
{
	struct batt_tm_data *batt_tm;
	struct device_node *dev_node = pdev->dev.of_node;
	int ret = 0;

	batt_tm = kzalloc(sizeof(struct batt_tm_data), GFP_KERNEL);
	if (!batt_tm) {
		pr_err("falied to alloc memory\n");
		return -ENOMEM;
	}

	if (dev_node) {
		ret = batt_tm_parse_dt(dev_node, batt_tm);
		if (ret) {
			pr_err("failed to parse dt\n");
			goto err_parse_dt;
		}
	}

	batt_tm->dev = &pdev->dev;

	wake_lock_init(&batt_tm->tm_wake_lock,
			WAKE_LOCK_SUSPEND, "batt_tm_wake_lock");
	INIT_DELAYED_WORK(&batt_tm->tm_work, batt_tm_worker);

	ret = batt_tm_psy_init(batt_tm);
	if (ret) {
		pr_err("failed to init psy\n");
		goto err_psy_init;
	}

	batt_tm->chg_online = power_supply_is_system_supplied();
	ret = batt_tm_notification_start(batt_tm);
	if (ret) {
		pr_err("failed to init adc tm\n");
		goto err_tm_init;
	}

	batt_tm->pm_notifier.notifier_call = batt_tm_pm_notifier;
	ret = register_pm_notifier(&batt_tm->pm_notifier);
	if (ret) {
		pr_err("failed to register pm notifier\n");
		goto err_tm_init;
	}

	platform_set_drvdata(pdev, batt_tm);
	pr_info("probe success\n");

	return 0;

err_tm_init:
	power_supply_unregister(&batt_tm->tm_psy);
err_psy_init:
	if(batt_tm->warm_cfg)
		kfree(batt_tm->warm_cfg);
	if(batt_tm->cold_cfg)
		kfree(batt_tm->cold_cfg);
	wake_lock_destroy(&batt_tm->tm_wake_lock);
err_parse_dt:
	kfree(batt_tm);
	return ret;
}

static int batt_tm_ctrl_remove(struct platform_device *pdev)
{
	struct batt_tm_data *batt_tm = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	unregister_pm_notifier(&batt_tm->pm_notifier);
	power_supply_unregister(&batt_tm->tm_psy);
	wake_lock_destroy(&batt_tm->tm_wake_lock);
	if(batt_tm->warm_cfg)
		kfree(batt_tm->warm_cfg);
	if(batt_tm->cold_cfg)
		kfree(batt_tm->cold_cfg);
	kfree(batt_tm);
	return 0;
}

static struct of_device_id batt_tm_match[] = {
	{.compatible = "battery_tm_ctrl", },
	{}
};

static struct platform_driver batt_tm_driver = {
	.probe = batt_tm_ctrl_probe,
	.remove = batt_tm_ctrl_remove,
	.driver = {
		.name = "batt_tm_ctrl",
		.owner = THIS_MODULE,
		.of_match_table = batt_tm_match,
	},
};

static int __init batt_tm_ctrl_init(void)
{
	return platform_driver_register(&batt_tm_driver);
}
late_initcall(batt_tm_ctrl_init);

static void __exit batt_tm_ctrl_exit(void)
{
	platform_driver_unregister(&batt_tm_driver);
}
module_exit(batt_tm_ctrl_exit);

MODULE_DESCRIPTION("Battery Temperature Control Driver");
MODULE_AUTHOR("ChoongRyeol Lee <choongryeol.lee@lge.com>");
MODULE_LICENSE("GPL");
