/*
 * Copyright (c) 2012, Motorola Mobility, Inc
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

#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <sound/apr_audio.h>
#include <sound/msm-dai-q6.h>

/* Device tree node */
#define DT_I2S_NODE_NAME		"I2S"
#define DT_PROP_I2S_TYPE		"type"
#define DT_TYPE_MI2S		0x001E0016
#define DT_TYPE_PRI_I2S		0x001E001C


static struct gpio mmi_mi2s_gpios[] = {
	{
		.label = "gpio_ws",
		.gpio = -1,
	},
	{
		.label = "gpio_sck",
		.gpio = -1,
	},
	{
		.label = "gpio_din",
		.gpio = -1,
	},
	{
		.label = "gpio_dout",
		.gpio = -1,
	},
};

static struct msm_mi2s_pdata mmi_msm_mi2s_rx_data = {
	.gpios = mmi_mi2s_gpios,
	.num_gpios = ARRAY_SIZE(mmi_mi2s_gpios),
	.rx_sd_lines = MSM_MI2S_SD2,
};

struct platform_device mmi_msmcpudai_mi2s_rx = {
	.name = "msm-dai-q6-mi2s",
	.id = -1,
	.dev = {
		.platform_data = &mmi_msm_mi2s_rx_data,
	},
};

static struct gpio mmi_pri_i2s_gpios[] = {
	{
		.label = "gpio_ws",
		.gpio = -1,
	},
	{
		.label = "gpio_sck",
		.gpio = -1,
	},
	{
		.label = "gpio_din",
		.gpio = -1,
	},
};

static struct msm_primary_i2s_pdata mmi_msm_pri_i2s_tx_data = {
	.gpios = mmi_pri_i2s_gpios,
	.num_gpios = ARRAY_SIZE(mmi_pri_i2s_gpios),
};

struct platform_device mmi_msmcpudai_pri_tx = {
	.name   = "msm-dai-q6",
	.id     = PRIMARY_I2S_TX,
	.dev = {
		.platform_data = &mmi_msm_pri_i2s_tx_data,
	},
};

void __init mmi_i2s_dai_init(void)
{
	struct device_node *node;
	int type;
	int i;

	/* Read I2S node from device tree */
	for_each_node_by_name(node, DT_I2S_NODE_NAME) {
		if (of_property_read_u32(node, DT_PROP_I2S_TYPE, &type))
			continue;

		switch (type) {
		case DT_TYPE_MI2S:
			for (i = 0; i < ARRAY_SIZE(mmi_mi2s_gpios); ++i) {
				if (of_property_read_u32(node,
						mmi_mi2s_gpios[i].label,
						&mmi_mi2s_gpios[i].gpio)) {
					pr_err("%s: Missing %s\n", __func__,
						mmi_mi2s_gpios[i].label);
					break;
				}
			}
			break;
		case DT_TYPE_PRI_I2S:
			for (i = 0; i < ARRAY_SIZE(mmi_pri_i2s_gpios); ++i) {
				if (of_property_read_u32(node,
						mmi_pri_i2s_gpios[i].label,
						&mmi_pri_i2s_gpios[i].gpio)) {
					pr_err("%s: Missing %s\n", __func__,
						mmi_pri_i2s_gpios[i].label);
					break;
				}
			}
			break;
		default:
			pr_err("%s: Invalid I2S node type", __func__);
			break;
		}
	}

	of_node_put(node);
	/* Always register the devices and let driver do the
		platform data validation */
	platform_device_register(&mmi_msmcpudai_mi2s_rx);
	platform_device_register(&mmi_msmcpudai_pri_tx);
}
