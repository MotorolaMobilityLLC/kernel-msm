/*
 * MTD SPI driver for ST mod dp bridge.
 *
 * Author: Mike Lavender, weiweij@motorola.com
 *
 * Copyright (c) 2005, motorola Inc.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>

#include <linux/mods/mod_display.h>
#include <linux/mods/mod_display_ops.h>

#define MOD_DP_BRIDGE_LABLE "MOD_DP_BRIDGE"
#define LOG_TAG MOD_DP_BRIDGE_LABLE

struct dp_bridge_data {
	struct platform_device *pdev;
	struct delayed_work work;
	struct workqueue_struct *workqueue;
	struct mutex lock;
	atomic_t dp_bridge_connected;
	struct completion connect_wait;
	struct completion video_stable_wait;
	struct dentry *debugfs;
	int gpio_sel;
};

static struct dp_bridge_data *dp_data;

#ifdef CONFIG_MOD_DISPLAY
extern int dp_enable_irq(bool en);
#else
static inline int dp_enable_irq(bool en)
	{ return 0; };
#endif

int dp_bridge_check_connect_state(bool connect)
{
	int value = connect ? 1 : -1;
	int count = connect ? 1 : 0;

	if (!dp_data) {
		pr_err("%s invalide dp_data\n", __func__);
		return -1;
	}

	return atomic_add_unless(&dp_data->dp_bridge_connected, value, count);
}
EXPORT_SYMBOL(dp_bridge_check_connect_state);

int dp_bridge_connect_wait_complete(void)
{
	if (!dp_data) {
		pr_err("%s invalide dp_data\n", __func__);
		return -1;
	}

	complete(&dp_data->connect_wait);

	return 0;
}
EXPORT_SYMBOL(dp_bridge_connect_wait_complete);

static int dp_bridge_parse_dt(struct device *dev,
			    struct dp_bridge_data *dp)
{
	struct device_node *np = dev->of_node;
	int ret = 0;

	//sel gpio
	dp->gpio_sel = of_get_named_gpio(np, "mmi,dp-sel-gpio", 0);
	if (dp->gpio_sel < 0) {
		pr_err("there is no dp-select-gpio.\n");
		goto end;
	} else if (gpio_is_valid(dp->gpio_sel)) {
		ret = gpio_request(dp->gpio_sel, MOD_DP_BRIDGE_LABLE "sel");
		if (ret == 0) {
			gpio_direction_output(dp->gpio_sel, 0);
			gpio_export(dp->gpio_sel, true);
		}
	}

end:
	return ret;
}

static int cable_disconnect(struct dp_bridge_data *dp)
{
	int ret = 0;

	/*dummy function*/

	return ret;
}

unsigned char* dp_bridge_mod_dispalay_get_edid(int size)
{
	struct mod_display_panel_config *display_config = NULL;
	unsigned char *edid;
	int ret;

	if ((edid = kmalloc(size, GFP_KERNEL)) == NULL)
		return NULL;

	ret = mod_display_get_display_config(&display_config);
	if (ret) {
		pr_err("%s: Failed to get display config: %d\n", __func__, ret);
		goto fail;
	}

	if (size != display_config->config_size) {
		pr_err("%s edid size %d not availabe(%d)\n",
			display_config->config_size, size);
		goto fail;
	}

	memcpy(edid, display_config->config_buf, display_config->config_size);

	return edid;

fail:
	kfree(edid);
	return 0;
}

static int dp_bridge_mod_display_handle_available(void *data)
{
	struct dp_bridge_data *dp;

	pr_debug("%s+\n", __func__);

	dp = (struct dp_bridge_data *)data;

	pr_debug("%s-\n", __func__);

	return 0;
}

static int dp_bridge_mod_display_handle_unavailable(void *data)
{
	struct dp_bridge_data *dp;
	int ret = 0;

	pr_debug("%s+\n", __func__);

	dp = (struct dp_bridge_data *)data;

	if (atomic_read(&dp->dp_bridge_connected)) {
		pr_err("%s: dp_bridge should not be connected!\n", __func__);
		ret = -EBUSY;
	}

	pr_debug("%s-\n", __func__);

	return ret;
}

static int dp_bridge_mod_display_handle_connect(void *data)
{
	struct dp_bridge_data *dp;
	int retries = 2;
	int ret = 0;

	pr_info("%s+\n", __func__);

	dp = (struct dp_bridge_data *)data;

	if (gpio_is_valid(dp->gpio_sel))
		gpio_set_value(dp->gpio_sel, 1);

	reinit_completion(&dp->connect_wait);

	dp_enable_irq(1);

	mod_display_set_display_state(MOD_DISPLAY_ON);

	while (!wait_for_completion_timeout(&dp->connect_wait,
				msecs_to_jiffies(1000)) && retries) {
		pr_warn("%s: dp_bridge not connected... Retries left: %d\n",
					__func__, retries);
		retries--;

		/* Power cycle the chip here to work around cable detection
		 * issues we've seen
		 */
		//sp_tx_hardware_poweron();
		//sp_tx_hardware_powerdown();
	}

/*	if (!atomic_read(&dp->dp_bridge_connected)) {
		pr_warn("%s: dp_bridge failed to connect...\n", __func__);
		ret = -ENODEV;
		goto exit;
	}*/

	pr_info("%s-\n", __func__);

	return ret;
}

static int dp_bridge_mod_display_handle_disconnect(void *data)
{
	struct dp_bridge_data *dp;
	int retries = 2;

	pr_info("%s+\n", __func__);

	dp = (struct dp_bridge_data *)data;

	reinit_completion(&dp->connect_wait);

	mod_display_set_display_state(MOD_DISPLAY_OFF);

	while (atomic_read(&dp->dp_bridge_connected) &&
			!wait_for_completion_timeout(&dp->connect_wait,
			msecs_to_jiffies(1000)) && retries) {
		pr_warn("%s: dp_bridge not disconnected... Retries left: %d\n",
			__func__, retries);
		retries--;
	}

	/* This should never happen, but just in case... */
	if (atomic_add_unless(&dp->dp_bridge_connected, -1, 0)) {
		pr_err("%s %s : dp_bridge failed to disconnect... Force cable removal\n",
			LOG_TAG, __func__);
		cable_disconnect(dp);
	}

	dp_enable_irq(0);

	if (gpio_is_valid(dp->gpio_sel))
		gpio_set_value(dp->gpio_sel, 0);

	pr_info("%s-\n", __func__);

	return 0;
}

static struct mod_display_ops dp_bridge_mod_display_ops = {
	.handle_available = dp_bridge_mod_display_handle_available,
	.handle_unavailable = dp_bridge_mod_display_handle_unavailable,
	.handle_connect = dp_bridge_mod_display_handle_connect,
	.handle_disconnect = dp_bridge_mod_display_handle_disconnect,
	.data = NULL,
};

static struct mod_display_impl_data dp_bridge_mod_display_impl = {
	.mod_display_type = MOD_DISPLAY_TYPE_DP,
	.ops = &dp_bridge_mod_display_ops,
};

static int dp_bridge_probe(struct platform_device *pdev)
{
	struct dp_bridge_data *dp;
	int ret = 0;

	pr_debug("%s %s start\n", LOG_TAG, __func__);

	dp = devm_kzalloc(&pdev->dev,
			     sizeof(struct dp_bridge_data),
			     GFP_KERNEL);
	if (!dp) {
		pr_err("%s: Failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	dp_data = dp;

	dp->pdev = pdev;
	platform_set_drvdata(pdev, dp);

	dp_bridge_parse_dt(&pdev->dev, dp);

	init_completion(&dp->connect_wait);

	dp_bridge_mod_display_ops.data = (void *)dp;
	mod_display_register_impl(&dp_bridge_mod_display_impl);

	pr_info("%s %s end\n", LOG_TAG, __func__);
	goto exit;

exit:
	return ret;
}

static int dp_bridge_remove(struct platform_device *pdev)
{
	struct dp_bridge_data *dp = platform_get_drvdata(pdev);

	kfree(dp);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id dp_bridge_match_table[] = {
	{.compatible = "mmi,mods-dp-bridge",},
	{},
};
#endif

static struct platform_driver dp_bridge_driver = {
	.driver = {
		   .name = "mods_dp_bridge",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = dp_bridge_match_table,
#endif
		   },
	.probe = dp_bridge_probe,
	.remove = dp_bridge_remove,
};

static int __init dp_bridge_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&dp_bridge_driver);
	if (ret < 0)
		pr_err("%s: failed to register dp_bridge drivern", __func__);

	return 0;
}

static void __exit dp_bridge_exit(void)
{
	platform_driver_unregister(&dp_bridge_driver);
}

module_init(dp_bridge_init);
module_exit(dp_bridge_exit);

MODULE_LICENSE("GPL");
