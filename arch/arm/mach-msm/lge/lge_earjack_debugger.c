/* Copyright (c) 2014, LGE Inc. All rights reserved.
 * Devin Kim <dojip.kim@lge.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/qpnp/qpnp-adc.h>
#include <mach/gpiomux.h>
#include <mach/board_lge.h>

enum {
	TRIGGER_WITH_HIGH = 0,
	TRIGGER_WITH_LOW,
};

/* ADC threshold values */
static int adc_low_threshold = 74;  /* in mV */
static int adc_high_threshold = 91; /* in mV */
static int adc_meas_interval = ADC_MEAS1_INTERVAL_1S;
static bool always_enable = false;

struct earjack_debugger {
	struct device *dev;
	struct qpnp_adc_tm_btm_param adc_param;
	struct qpnp_adc_tm_chip *adc_tm_dev;
	struct qpnp_vadc_chip *vadc_dev;
	struct delayed_work init_adc_work;
	bool trigger_mode;
	bool id_adc_detect;
	int connected;   /* uart connected? */
};

/* GPIO mux */
static struct gpiomux_setting gpio_console_uart = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting gpio_console_uart_disabled = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config msm_console_uart_configs[] = {
	{
		.gpio     = 8,         /* BLSP1 QUP3 UART_TX */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_console_uart,
			[GPIOMUX_SUSPENDED] = &gpio_console_uart,
		},
	},
	{
		.gpio     = 9,         /* BLSP1 QUP3 UART_RX */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_console_uart,
			[GPIOMUX_SUSPENDED] = &gpio_console_uart,
		},
	},
};

static struct msm_gpiomux_config msm_console_uart_disabled_configs[] = {
	{
		.gpio     = 8,         /* GPIO */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_console_uart_disabled,
			[GPIOMUX_SUSPENDED] = &gpio_console_uart_disabled,
		},
	},
	{
		.gpio     = 9,         /* GPIO */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_console_uart_disabled,
			[GPIOMUX_SUSPENDED] = &gpio_console_uart_disabled,
		},
	},
};

static int earjack_debugger_usbid_voltage_now(
		struct earjack_debugger *earjack_dev);

static bool earjack_debugger_is_connected(struct earjack_debugger *earjack_dev)
{
	int mV;

	mV = earjack_debugger_usbid_voltage_now(earjack_dev);
	if (mV < 0)
		mV = 0;
	else
		mV = mV / 1000; /* convert to mV */

	if (mV >= adc_low_threshold  && mV <= adc_high_threshold)
		return 1;

	return 0;
}

static void earjack_debugger_enable_uart(struct earjack_debugger *earjack_dev,
		int connected)
{
	if (connected) {
		msm_gpiomux_install(msm_console_uart_configs,
			 ARRAY_SIZE(msm_console_uart_configs));
	} else {
		if (always_enable)
			return; /* do not disable the uart */

		msm_gpiomux_install(msm_console_uart_disabled_configs,
			ARRAY_SIZE(msm_console_uart_disabled_configs));
	}
}

static void earjack_debugger_adc_notification(enum qpnp_tm_state state,
		void *ctx)
{
	struct earjack_debugger *earjack_dev = ctx;
	int connected;

	if (state >= ADC_TM_STATE_NUM) {
		pr_err("%s: invalid notification %d\n", __func__, state);
		return;
	}

	dev_dbg(earjack_dev->dev, "%s: state = %s\n", __func__,
			state == ADC_TM_HIGH_STATE ? "high" : "low");

	if (state == ADC_TM_HIGH_STATE)
		earjack_dev->adc_param.state_request =
			ADC_TM_LOW_THR_ENABLE;
	else
		earjack_dev->adc_param.state_request =
			ADC_TM_HIGH_THR_ENABLE;

	connected = earjack_debugger_is_connected(earjack_dev);

	pr_info("Earjack-debugger: %s\n",
			connected ? "CONNECTED" : "DISCONNECTED");

	if (connected != earjack_dev->connected) {
		earjack_dev->connected = connected;
		earjack_debugger_enable_uart(earjack_dev, connected);
	}

	/* re-arm ADC interrupt */
	qpnp_adc_tm_usbid_configure(earjack_dev->adc_tm_dev,
			&earjack_dev->adc_param);
}

static int earjack_debugger_usbid_voltage_now(
		struct earjack_debugger *earjack_dev)
{
	int ret;
	struct qpnp_vadc_result results;

	if (IS_ERR_OR_NULL(earjack_dev->vadc_dev)) {
		earjack_dev->vadc_dev = qpnp_get_vadc(earjack_dev->dev,
				"usbid");
		if (IS_ERR(earjack_dev->vadc_dev))
			return PTR_ERR(earjack_dev->vadc_dev);
	}

	ret = qpnp_vadc_read(earjack_dev->vadc_dev, USB_ID_MV, &results);
	if (ret) {
		dev_err(earjack_dev->dev, "Read USBID VADC error\n");
		return 0;
	} else {
		dev_info(earjack_dev->dev, "VADC: %d", (int)results.physical);
		return results.physical;
	}
}

static void earjack_debugger_init_adc_work(struct work_struct *w)
{
	struct earjack_debugger *earjack_dev = container_of(w,
			struct earjack_debugger, init_adc_work.work);
	int ret;

	earjack_dev->adc_tm_dev = qpnp_get_adc_tm( earjack_dev->dev, "usbid");
	if (IS_ERR(earjack_dev->adc_tm_dev)) {
		if (PTR_ERR(earjack_dev->adc_tm_dev) == -EPROBE_DEFER)
			queue_delayed_work(system_nrt_wq, to_delayed_work(w),
					msecs_to_jiffies(100));
		else
			earjack_dev->adc_tm_dev = NULL;
		return;
	}

	if (earjack_dev->trigger_mode == TRIGGER_WITH_HIGH) {
		earjack_dev->adc_param.low_thr = adc_high_threshold;
		earjack_dev->adc_param.high_thr = adc_high_threshold;
	} else {
		earjack_dev->adc_param.low_thr = adc_low_threshold;
		earjack_dev->adc_param.high_thr = adc_low_threshold;
	}
	earjack_dev->adc_param.timer_interval = adc_meas_interval;
	earjack_dev->adc_param.state_request = ADC_TM_HIGH_LOW_THR_ENABLE;
	earjack_dev->adc_param.btm_ctx = earjack_dev;
	earjack_dev->adc_param.threshold_notification =
		earjack_debugger_adc_notification;

	ret = qpnp_adc_tm_usbid_configure(earjack_dev->adc_tm_dev,
			&earjack_dev->adc_param);
	if (ret) {
		dev_err(earjack_dev->dev, "%s: request ADC error %d\n",
				__func__, ret);
		return;
	}

	dev_info(earjack_dev->dev, "%s: initialized the ADC for USB ID\n",
			__func__);
	earjack_dev->id_adc_detect = true;

	earjack_debugger_usbid_voltage_now(earjack_dev);
}

static ssize_t earjack_debugger_always_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct earjack_debugger *earjack_dev = dev_get_drvdata(dev);
	long r;
	int ret;

	ret = kstrtol(buf, 10, &r);
	if (ret < 0) {
		dev_err(dev, "Store vlaue error\n");
		return ret;
	}

	always_enable = r ? true : false;

	earjack_debugger_enable_uart(earjack_dev, always_enable);

	return size;
}

static ssize_t earjack_debugger_always_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", always_enable);
}

static DEVICE_ATTR(always_enable, S_IRUGO | S_IWUSR,
			 earjack_debugger_always_enable_show,
			 earjack_debugger_always_enable_store);

static void earjack_debugger_dt(struct device *dev,
		struct earjack_debugger *earjack_dev)
{
	int ret;
	struct device_node *np = dev->of_node;
	const char *data;
	u32 tmp;

	ret = of_property_read_u32(np, "lge,adc-low-threshold", &tmp);
	if (!ret)
		adc_low_threshold = tmp;

	ret = of_property_read_u32(np, "lge,adc-high-threshold", &tmp);
	if (!ret)
		adc_high_threshold = tmp;

	data = of_get_property(np, "lge,trigger-mode", NULL);
	if (data && !strcmp(data, "low"))
		earjack_dev->trigger_mode = TRIGGER_WITH_LOW;
	else
		earjack_dev->trigger_mode = TRIGGER_WITH_HIGH;
}

static int earjack_debugger_probe(struct platform_device *pdev)
{
	struct earjack_debugger *earjack_dev = NULL;
	int ret;

	earjack_dev = kzalloc(sizeof(struct earjack_debugger), GFP_KERNEL);
	if (!earjack_dev) {
		dev_err(&pdev->dev, "%s: no memory\n", __func__);
		return -ENOMEM;
	}

	if (pdev->dev.of_node)
		earjack_debugger_dt(&pdev->dev, earjack_dev);

	platform_set_drvdata(pdev, earjack_dev);
	earjack_dev->dev = &pdev->dev;
	earjack_dev->connected = -1;

	ret =  device_create_file(&pdev->dev, &dev_attr_always_enable);
	if (ret < 0) {
		dev_err(&pdev->dev, "create sysfs error\n");
		goto err_sysfs_always_enable;
	}

	INIT_DELAYED_WORK(&earjack_dev->init_adc_work,
			earjack_debugger_init_adc_work);

	 queue_work(system_nrt_wq, &earjack_dev->init_adc_work.work);

	 return 0;

err_sysfs_always_enable:
	if (earjack_dev)
		kfree(earjack_dev);

	return ret;
}

static int earjack_debugger_remove(struct platform_device *pdev)
{
	struct earjack_debugger *earjack_dev = platform_get_drvdata(pdev);

	if (earjack_dev->id_adc_detect)
		qpnp_adc_tm_usbid_end(earjack_dev->adc_tm_dev);

	device_remove_file(&pdev->dev, &dev_attr_always_enable);

	platform_set_drvdata(pdev, NULL);
	if (earjack_dev)
		kfree(earjack_dev);

	return 0;
}

static struct of_device_id earjack_debugger_match_table[] = {
	{
		.compatible = "serial,earjack-debugger",
	},
};

static struct platform_driver earjack_debugger_driver = {
	.probe = earjack_debugger_probe,
	.remove = earjack_debugger_remove,
	.driver = {
		.name = "earjack-debugger",
		.of_match_table = earjack_debugger_match_table,
	},
};

static int __init earjack_debugger_init(void)
{
	if (lge_uart_console_enabled())
		return platform_driver_register(&earjack_debugger_driver);
	else
		return 0;
}

static void __exit earjack_debugger_exit(void)
{
	if (lge_uart_console_enabled())
		platform_driver_unregister(&earjack_debugger_driver);
}

static int __init earjack_debugger_setup(char *str)
{
	if (str && !strncmp(str, "always", 6))
		always_enable = true;

	return 1;
}

__setup("lge.earjack-debugger=", earjack_debugger_setup);

module_init(earjack_debugger_init);
module_exit(earjack_debugger_exit);

MODULE_AUTHOR("LG Electronics Inc.");
MODULE_DESCRIPTION("Earjack Debugger");
MODULE_LICENSE("GPL");
