/*
 * RTC device/driver based on SensorHub
 * Copyright (C) 2015 Motorola Mobility LLC
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/m4sensorhub.h>
#include <linux/m4sensorhub/m4sensorhub_registers.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/time_update_notifier.h>
#include <linux/timekeeper_internal.h>

#define DRIVER_NAME "m4sensorhub-rtc"

struct rtc_sensorhub_private_data {
	struct platform_device *platdev;
	struct m4sensorhub_data *p_m4sensorhub_data;
};

static struct rtc_sensorhub_private_data *priv_data;

static int time_update_notify(struct notifier_block *nb, unsigned long unused,
	void *priv)
{
	struct platform_device *p_platdev = priv_data->platdev;
	struct m4sensorhub_data *p_m4_drvdata =
			priv_data->p_m4sensorhub_data;
	struct timeval tv;
#ifdef DEBUG
	struct rtc_time tm;
#endif

	if (!(p_m4_drvdata)) {
		dev_err(&(p_platdev->dev), "M4 not ready\n");
		return 0;
	}

	do_gettimeofday(&tv);
	/* M4 accepts time as u32*/
	if (m4sensorhub_reg_getsize(p_m4_drvdata, M4SH_REG_GENERAL_UTC) !=
		m4sensorhub_reg_write(p_m4_drvdata, M4SH_REG_GENERAL_UTC,
				      (char *)&(tv.tv_sec), m4sh_no_mask)) {
		dev_err(&(p_platdev->dev),
			"set time, but failed to set M4 clock!\n");
		return -EIO;
	}
#ifdef DEBUG
	rtc_time_to_tm(tv.tv_sec, &tm);
	dev_dbg(&(p_platdev->dev),
		"Set RTC time to %d-%02d-%02d %02d:%02d:%02d UTC (%ld)\n",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_sec);
#endif

	return 0;
}

static struct notifier_block time_update_notifier = {
	.notifier_call = time_update_notify,
};

static int rtc_sensorhub_init(struct init_calldata *p_arg)
{
	struct rtc_sensorhub_private_data *p_priv_data =
			(struct rtc_sensorhub_private_data *)(p_arg->p_data);
	struct platform_device *p_platdev = p_priv_data->platdev;
	struct timeval tv;
#ifdef DEBUG
	struct rtc_time tm;
#endif

	p_priv_data->p_m4sensorhub_data = p_arg->p_m4sensorhub_data;

	/* Send initial time when M4 is first ready */
	do_gettimeofday(&tv);
	/* M4 accepts time as u32*/
	if (m4sensorhub_reg_getsize(p_priv_data->p_m4sensorhub_data,
				    M4SH_REG_GENERAL_UTC) !=
	    m4sensorhub_reg_write(p_priv_data->p_m4sensorhub_data,
				  M4SH_REG_GENERAL_UTC, (char *)&(tv.tv_sec),
				  m4sh_no_mask)) {
		dev_err(&(p_platdev->dev),
			"set time, but failed to set M4 clock!\n");
		return -EIO;
	}
#ifdef DEBUG
	rtc_time_to_tm(tv.tv_sec, &tm);
	dev_dbg(&(p_platdev->dev),
		"Set RTC time to %d-%02d-%02d %02d:%02d:%02d UTC (%ld)\n",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_sec);
#endif

	return 0;
}

static int rtc_sensorhub_probe(struct platform_device *p_platdev)
{
	int err;
	struct rtc_sensorhub_private_data *p_priv_data;

	p_priv_data = kzalloc(sizeof(*p_priv_data),
					GFP_KERNEL);
	if (!p_priv_data)
		return -ENOMEM;

	p_priv_data->p_m4sensorhub_data = NULL;

	/* Set the private data before registering this driver with RTC core
	since hctosys will call rtc interface right away, we need to make sure
	our private data is set by this time */
	platform_set_drvdata(p_platdev, p_priv_data);

	/* Need a local copy for notifier to use it */
	priv_data = p_priv_data;
	priv_data->platdev = p_platdev;

	time_update_register_notifier(&time_update_notifier);

	err = m4sensorhub_register_initcall(rtc_sensorhub_init, p_priv_data);
	if (err) {
		dev_err(&(p_platdev->dev), "can't register init with m4\n");
		goto err_free_priv_data;
	}

	return 0;

err_free_priv_data:
	kfree(p_priv_data);
	priv_data = NULL;
	return err;
}

static int rtc_sensorhub_remove(struct platform_device *p_platdev)
{
	struct rtc_sensorhub_private_data *p_priv_data =
						platform_get_drvdata(p_platdev);
	m4sensorhub_unregister_initcall(rtc_sensorhub_init);
	time_update_unregister_notifier(&time_update_notifier);
	kfree(p_priv_data);
	priv_data = NULL;
	return 0;
}

static const struct of_device_id of_rtc_sensorhub_match[] = {
	{ .compatible = "mot,m4rtc", },
	{},
};

static struct platform_driver rtc_sensorhub_driver = {
	.probe	= rtc_sensorhub_probe,
	.remove = rtc_sensorhub_remove,
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_rtc_sensorhub_match,
	},
};

module_platform_driver(rtc_sensorhub_driver);

MODULE_AUTHOR("Motorola Mobility LLC");
MODULE_DESCRIPTION("SensorHub RTC driver/device");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rtc_sensorhub");
