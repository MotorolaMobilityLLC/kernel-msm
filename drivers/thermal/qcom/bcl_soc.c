// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/thermal.h>
#include <linux/iio/consumer.h>
#include "../thermal_core.h"

#define BCL_DRIVER_NAME       "bcl_soc_peripheral"

struct bcl_device {
	struct device				*dev;
	struct notifier_block			psy_nb;
	struct work_struct			soc_eval_work;
	long					trip_temp;
	int					trip_val;
	struct mutex				state_trans_lock;
	bool					irq_enabled;
	struct thermal_zone_device		*tz_dev;
	struct thermal_zone_of_device_ops	ops;
	int					bcl_not_mitigate_icl;
	struct iio_channel	**ext_iio_chans;
};

static struct bcl_device *bcl_perph;

enum bcl_mmi_ext_iio_channels {
	SMB5_HW_CURRENT_MAX = 0,
};

static const char * const bcl_mmi_ext_iio_chan_name[] = {
	[SMB5_HW_CURRENT_MAX] = "usb_hw_current_max",
};

static bool is_chan_valid(enum bcl_mmi_ext_iio_channels chan)
{
	int rc;

	if (IS_ERR(bcl_perph->ext_iio_chans[chan]))
		return false;

	if (!bcl_perph->ext_iio_chans[chan]) {
		bcl_perph->ext_iio_chans[chan] = iio_channel_get(bcl_perph->dev,
					bcl_mmi_ext_iio_chan_name[chan]);
		if (IS_ERR(bcl_perph->ext_iio_chans[chan])) {
			rc = PTR_ERR(bcl_perph->ext_iio_chans[chan]);
			bcl_perph->ext_iio_chans[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				bcl_mmi_ext_iio_chan_name[chan], rc);
			return false;
		}
	}

	return true;
}

int bcl_mmi_read_iio_chan(enum bcl_mmi_ext_iio_channels chan, int *val)
{
	int rc;

	if (is_chan_valid(chan)) {
		rc = iio_read_channel_processed(
				bcl_perph->ext_iio_chans[chan], val);
		return (rc < 0) ? rc : 0;
	}

	return -EINVAL;
}

static int bcl_set_soc(void *data, int low, int high)
{
	if (high == bcl_perph->trip_temp)
		return 0;

	mutex_lock(&bcl_perph->state_trans_lock);
	pr_debug("socd threshold:%d\n", high);
	bcl_perph->trip_temp = high;
	if (high == INT_MAX) {
		bcl_perph->irq_enabled = false;
		goto unlock_and_exit;
	}
	bcl_perph->irq_enabled = true;
	schedule_work(&bcl_perph->soc_eval_work);

unlock_and_exit:
	mutex_unlock(&bcl_perph->state_trans_lock);
	return 0;
}

static int bcl_read_soc(void *data, int *val)
{
	static struct power_supply *batt_psy;
	union power_supply_propval ret = {0,};
	int err = 0;

	*val = 0;

	if (bcl_perph->bcl_not_mitigate_icl) {
		err = bcl_mmi_read_iio_chan(SMB5_HW_CURRENT_MAX, &ret.intval);
		if (err)
			pr_err("HW current max read error:%d\n", err);
		else if ((ret.intval /1000) >= bcl_perph->bcl_not_mitigate_icl) {
			pr_debug("charger current more than bcl_not_mitigate_icl %dmA\n",
				bcl_perph->bcl_not_mitigate_icl);
			return err;
		}
	}

	if (!batt_psy)
		batt_psy = power_supply_get_by_name("battery");
	if (batt_psy) {
		err = power_supply_get_property(batt_psy,
				POWER_SUPPLY_PROP_CAPACITY, &ret);
		if (err) {
			pr_err("battery percentage read error:%d\n",
				err);
			return err;
		}
		*val = 100 - ret.intval;
	}
	pr_debug("soc:%d\n", *val);

	return err;
}

static void bcl_evaluate_soc(struct work_struct *work)
{
	int battery_depletion;

	if (!bcl_perph->tz_dev)
		return;

	if (bcl_read_soc(NULL, &battery_depletion))
		return;

	mutex_lock(&bcl_perph->state_trans_lock);
	if (!bcl_perph->irq_enabled)
		goto eval_exit;
	if (battery_depletion < bcl_perph->trip_temp)
		goto eval_exit;

	bcl_perph->trip_val = battery_depletion;
	mutex_unlock(&bcl_perph->state_trans_lock);
	of_thermal_handle_trip_temp(bcl_perph->dev,
			bcl_perph->tz_dev, bcl_perph->trip_val);

	return;
eval_exit:
	mutex_unlock(&bcl_perph->state_trans_lock);
}

static int battery_supply_callback(struct notifier_block *nb,
			unsigned long event, void *data)
{
	struct power_supply *psy = data;

	if (strcmp(psy->desc->name, "battery"))
		return NOTIFY_OK;
	schedule_work(&bcl_perph->soc_eval_work);

	return NOTIFY_OK;
}

static int bcl_soc_remove(struct platform_device *pdev)
{
	power_supply_unreg_notifier(&bcl_perph->psy_nb);
	flush_work(&bcl_perph->soc_eval_work);
	if (bcl_perph->tz_dev)
		thermal_zone_of_sensor_unregister(&pdev->dev,
				bcl_perph->tz_dev);

	return 0;
}

static int bcl_mmi_init_iio_psy(struct platform_device *pdev)
{
	bcl_perph->ext_iio_chans = devm_kcalloc(bcl_perph->dev,
				ARRAY_SIZE(bcl_mmi_ext_iio_chan_name),
				sizeof(*bcl_perph->ext_iio_chans),
				GFP_KERNEL);
	if (!bcl_perph->ext_iio_chans)
		return -ENOMEM;

	return 0;
}

static int bcl_soc_probe(struct platform_device *pdev)
{
	int ret = 0;

	bcl_perph = devm_kzalloc(&pdev->dev, sizeof(*bcl_perph), GFP_KERNEL);
	if (!bcl_perph)
		return -ENOMEM;

	mutex_init(&bcl_perph->state_trans_lock);
	bcl_perph->dev = &pdev->dev;
	bcl_perph->ops.get_temp = bcl_read_soc;
	bcl_perph->ops.set_trips = bcl_set_soc;
	INIT_WORK(&bcl_perph->soc_eval_work, bcl_evaluate_soc);
	bcl_perph->psy_nb.notifier_call = battery_supply_callback;
	ret = power_supply_reg_notifier(&bcl_perph->psy_nb);
	if (ret < 0) {
		pr_err("soc notifier registration error. defer. err:%d\n",
			ret);
		ret = -EPROBE_DEFER;
		goto bcl_soc_probe_exit;
	}
	bcl_perph->tz_dev = thermal_zone_of_sensor_register(&pdev->dev,
				0, bcl_perph, &bcl_perph->ops);
	if (IS_ERR(bcl_perph->tz_dev)) {
		pr_err("soc TZ register failed. err:%ld\n",
				PTR_ERR(bcl_perph->tz_dev));
		ret = PTR_ERR(bcl_perph->tz_dev);
		bcl_perph->tz_dev = NULL;
		goto bcl_soc_probe_exit;
	}

	bcl_mmi_init_iio_psy(pdev);

	ret = of_property_read_u32(pdev->dev.of_node,
			"qcom,bcl-not-mitigate-icl",
			&bcl_perph->bcl_not_mitigate_icl);
	if (ret < 0) {
		bcl_perph->bcl_not_mitigate_icl = 0;
	}

	thermal_zone_device_update(bcl_perph->tz_dev, THERMAL_DEVICE_UP);
	schedule_work(&bcl_perph->soc_eval_work);

	dev_set_drvdata(&pdev->dev, bcl_perph);

	return 0;

bcl_soc_probe_exit:
	bcl_soc_remove(pdev);
	return ret;
}

static const struct of_device_id bcl_match[] = {
	{
		.compatible = "qcom,msm-bcl-soc",
	},
	{},
};

static struct platform_driver bcl_driver = {
	.probe  = bcl_soc_probe,
	.remove = bcl_soc_remove,
	.driver = {
		.name           = BCL_DRIVER_NAME,
		.of_match_table = bcl_match,
	},
};

module_platform_driver(bcl_driver);
MODULE_LICENSE("GPL v2");
