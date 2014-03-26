 /*
 * Copyright(c) 2013-2014, LGE Inc. All rights reserved.
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
#include <linux/of_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/suspend.h>

struct batt_tm_data {
	struct device *dev;
	struct qpnp_adc_tm_chip *adc_tm_dev;
	struct power_supply *batt_psy;
	struct qpnp_adc_tm_btm_param adc_param;
	struct tm_ctrl_data *warm_cfg;
	struct tm_ctrl_data *cool_cfg;
	bool tm_disabled_in_suspend;
	unsigned int warm_cfg_size;
	unsigned int cool_cfg_size;
	int batt_vreg_mv;
	int low_batt_vreg_mv;
	int current_ma;
	int low_current_ma;
};

struct tm_ctrl_data {
	int thr;
	int next_cool_thr;
	int next_warm_thr;
	unsigned int action;
	unsigned int health;
};

enum {
	CHG_ENABLE,
	CHG_DECREASE,
	CHG_STOP,
	CHG_SHUTDOWN
};

static int power_supply_set_max_voltage(struct power_supply *psy, int limit)
{
	const union power_supply_propval ret = {limit,};

	if (psy->set_property)
		return psy->set_property(psy, POWER_SUPPLY_PROP_VOLTAGE_MAX,
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

static void batt_tm_notification(enum qpnp_tm_state state, void *ctx)
{
	struct batt_tm_data *batt_tm = ctx;
	int temp;
	int i;
	int tm_action;
	int batt_health;
	union power_supply_propval ret = {0,};
	int rc;

	if (state >= ADC_TM_STATE_NUM) {
		pr_err("invalid notification %d\n", state);
		return;
	}

	batt_tm->batt_psy->get_property(batt_tm->batt_psy,
				POWER_SUPPLY_PROP_TEMP, &ret);
	temp = ret.intval;

	pr_debug("temp = %d state = %s\n", temp,
				state == ADC_TM_WARM_STATE ? "warm" : "cool");

	if (state == ADC_TM_WARM_STATE) {
		i = batt_tm->warm_cfg_size - 1;
		while ((batt_tm->adc_param.high_temp !=
					batt_tm->warm_cfg[i].thr) && (i > 0))
			i--;

		batt_tm->adc_param.low_temp =
					batt_tm->warm_cfg[i].next_cool_thr;
		batt_tm->adc_param.high_temp =
					batt_tm->warm_cfg[i].next_warm_thr;
		tm_action = batt_tm->warm_cfg[i].action;
		batt_health = batt_tm->warm_cfg[i].health;
	} else {
		i = batt_tm->cool_cfg_size - 1;
		while ((batt_tm->adc_param.low_temp !=
					batt_tm->cool_cfg[i].thr) && (i > 0))
			i--;

		batt_tm->adc_param.low_temp =
					batt_tm->cool_cfg[i].next_cool_thr;
		batt_tm->adc_param.high_temp =
					batt_tm->cool_cfg[i].next_warm_thr;
		tm_action = batt_tm->cool_cfg[i].action;
		batt_health = batt_tm->cool_cfg[i].health;
	}

	power_supply_set_health_state(batt_tm->batt_psy, batt_health);

	switch (tm_action) {
	case CHG_ENABLE:
		pr_debug("Enable charging. vbatt_max = %d\n",
					batt_tm->batt_vreg_mv);
		power_supply_set_chg_enable(batt_tm->batt_psy, true);
		power_supply_set_current_limit(batt_tm->batt_psy,
					batt_tm->current_ma * 1000);
		power_supply_set_max_voltage(batt_tm->batt_psy,
					batt_tm->batt_vreg_mv * 1000);
		break;
	case CHG_DECREASE:
		pr_debug("Decrease current to %d, vbatt_max to %d\n",
						batt_tm->low_current_ma,
						batt_tm->low_batt_vreg_mv);
		power_supply_set_chg_enable(batt_tm->batt_psy, true);
		power_supply_set_current_limit(batt_tm->batt_psy,
					batt_tm->low_current_ma * 1000);
		power_supply_set_max_voltage(batt_tm->batt_psy,
					batt_tm->low_batt_vreg_mv * 1000);
		break;
	case CHG_STOP:
		pr_debug("Stop charging!\n");
		power_supply_set_chg_enable(batt_tm->batt_psy, false);
		break;
	case CHG_SHUTDOWN:
		pr_debug("Shutdown!\n");
		power_supply_changed(batt_tm->batt_psy);
		break;
	default:
		break;
	}

	pr_info("action : %d next low temp = %d next high temp = %d\n",
					tm_action,
					batt_tm->adc_param.low_temp,
					batt_tm->adc_param.high_temp);

	rc = qpnp_adc_tm_channel_measure(batt_tm->adc_tm_dev,
						&batt_tm->adc_param);
	if (rc)
		pr_err("request adc_tm error\n");

}

static int batt_tm_notification_init(struct batt_tm_data *batt_tm)
{
	int rc;

	batt_tm->adc_param.low_temp =
				batt_tm->warm_cfg[0].next_cool_thr;
	batt_tm->adc_param.high_temp =
				batt_tm->warm_cfg[0].next_warm_thr;
	batt_tm->adc_param.state_request =
				ADC_TM_HIGH_LOW_THR_ENABLE;
	batt_tm->adc_param.timer_interval = ADC_MEAS1_INTERVAL_8S;
	batt_tm->adc_param.btm_ctx = batt_tm;
	batt_tm->adc_param.threshold_notification =
					batt_tm_notification;
	batt_tm->adc_param.channel = LR_MUX1_BATT_THERM;

	rc = qpnp_adc_tm_channel_measure(batt_tm->adc_tm_dev,
						&batt_tm->adc_param);
	if (rc)
		pr_err("request adc_tm error %d\n", rc);

	return rc;
}

static int batt_tm_parse_dt(struct device_node *np,
				struct batt_tm_data *batt_tm)
{
	int ret;
	struct device_node *charger_node = NULL;

	batt_tm->adc_tm_dev = qpnp_get_adc_tm(batt_tm->dev, "batt-tm");
	if (IS_ERR(batt_tm->adc_tm_dev)) {
		ret = PTR_ERR(batt_tm->adc_tm_dev);
		pr_err("adc-tm not ready\n");
		goto out;
	}

	charger_node = of_parse_phandle(np, "qcom,charger", 0);
	if (!charger_node) {
		pr_err("failed to get charger phandle\n");
		ret = -EINVAL;
		goto out;
	}

	ret = of_property_read_u32(charger_node, "qcom,ibatmax-ma",
					&batt_tm->current_ma);
	if (ret) {
		pr_err("failed to get tm,current-ma\n");
		goto out;
	}

	ret = of_property_read_u32(charger_node, "qcom,vddmax-mv",
					&batt_tm->batt_vreg_mv);
	if (ret) {
		pr_err("failed to get qcom,vddmax-mv\n");
		goto out;
	}

	of_node_put(charger_node);

	ret = of_property_read_u32(np, "tm,low-vreg-mv",
				&batt_tm->low_batt_vreg_mv);
	if (ret) {
		pr_err("failed to get tm,low-vbatt-mv\n");
		goto out;
	}

	ret = of_property_read_u32(np, "tm,low-current-ma",
				&batt_tm->low_current_ma);
	if (ret) {
		pr_err("failed to get tm,low-current-ma\n");
		goto out;
	}

	if (!of_get_property(np, "tm,warm-cfg",
				&batt_tm->warm_cfg_size)) {
		pr_err("failed to get warm_cfg\n");
		ret = -EINVAL;
		goto out;
	}

	batt_tm->warm_cfg = devm_kzalloc(batt_tm->dev,
				batt_tm->warm_cfg_size, GFP_KERNEL);
	if (!batt_tm->warm_cfg) {
		pr_err("Unable to allocate memory\n");
		ret = -ENOMEM;
		goto out;
	}

	ret = of_property_read_u32_array(np, "tm,warm-cfg",
					(u32 *)batt_tm->warm_cfg,
					batt_tm->warm_cfg_size / sizeof(u32));
	if (ret) {
		pr_err("failed to get tm,warm-cfg\n");
		goto out;
	}

	batt_tm->warm_cfg_size /= sizeof(struct tm_ctrl_data);

	if (!of_get_property(np, "tm,cool-cfg",
			&batt_tm->cool_cfg_size)) {
		pr_err("failed to get cool_cfg\n");
		ret = -EINVAL;
		goto out;
	}

	batt_tm->cool_cfg = devm_kzalloc(batt_tm->dev,
				batt_tm->cool_cfg_size, GFP_KERNEL);
	if (!batt_tm->cool_cfg) {
		pr_err("Unable to allocate memory\n");
		ret = -ENOMEM;
		goto out;
	}

	ret = of_property_read_u32_array(np, "tm,cool-cfg",
					(u32 *)batt_tm->cool_cfg,
					batt_tm->cool_cfg_size / sizeof(u32));
	if (ret) {
		pr_err("failed to get tm,warm-cfg\n");
		goto out;
	}

	batt_tm->cool_cfg_size /= sizeof(struct tm_ctrl_data);

out:
	return ret;
}

static int batt_tm_probe(struct platform_device *pdev)
{
	struct batt_tm_data *batt_tm;
	struct device_node *dev_node = pdev->dev.of_node;
	int ret = 0;

	batt_tm = devm_kzalloc(&pdev->dev,
			sizeof(struct batt_tm_data), GFP_KERNEL);
	if (!batt_tm) {
		pr_err("falied to alloc memory\n");
		return -ENOMEM;
	}

	batt_tm->dev = &pdev->dev;
	platform_set_drvdata(pdev, batt_tm);

	if (dev_node) {
		ret = batt_tm_parse_dt(dev_node, batt_tm);
		if (ret) {
			pr_err("failed to parse dt\n");
			goto out;
		}
	} else {
		pr_err("not supported for non OF\n");
		ret = -ENODEV;
		goto out;
	}

	batt_tm->batt_psy = power_supply_get_by_name("battery");
	if (!batt_tm->batt_psy) {
		pr_err("battery supply not found\n");
		ret =  -EPROBE_DEFER;
		goto out;
	}

	ret = batt_tm_notification_init(batt_tm);
	if (ret) {
		pr_err("failed to init adc tm\n");
		goto out;
	}

out:
	return ret;
}

static int batt_tm_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static int batt_tm_pm_prepare(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct batt_tm_data *batt_tm = platform_get_drvdata(pdev);

	if (!power_supply_is_system_supplied()) {
		qpnp_adc_tm_disable_chan_meas(batt_tm->adc_tm_dev,
					&batt_tm->adc_param);
		batt_tm->tm_disabled_in_suspend = true;
	}
	return 0;
}

static void batt_tm_pm_complete(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct batt_tm_data *batt_tm = platform_get_drvdata(pdev);

	if (batt_tm->tm_disabled_in_suspend) {
		qpnp_adc_tm_channel_measure(batt_tm->adc_tm_dev,
					&batt_tm->adc_param);
		batt_tm->tm_disabled_in_suspend = false;
	}
}

static const struct dev_pm_ops batt_tm_pm_ops = {
	.prepare = batt_tm_pm_prepare,
	.complete = batt_tm_pm_complete,
};

static struct of_device_id batt_tm_match[] = {
	{.compatible = "battery_tm", },
	{}
};

static struct platform_driver batt_tm_driver = {
	.probe = batt_tm_probe,
	.remove = batt_tm_remove,
	.driver = {
		.name = "batt_tm",
		.owner = THIS_MODULE,
		.of_match_table = batt_tm_match,
		.pm = &batt_tm_pm_ops,
	},
};

static int __init batt_tm_init(void)
{
	return platform_driver_register(&batt_tm_driver);
}
late_initcall(batt_tm_init);

static void __exit batt_tm_exit(void)
{
	platform_driver_unregister(&batt_tm_driver);
}
module_exit(batt_tm_exit);

MODULE_DESCRIPTION("Battery Temp Monitor Driver");
MODULE_AUTHOR("ChoongRyeol Lee <choongryeol.lee@lge.com>");
MODULE_LICENSE("GPL");
