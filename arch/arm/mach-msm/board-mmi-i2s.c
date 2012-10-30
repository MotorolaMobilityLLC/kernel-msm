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
#include <sound/msm-dai-q6.h>

/* Device tree node */
#define DT_PATH_I2S			"/System@0/I2S@0"
#define DT_PROP_I2S_TYPE		"type"
#define DT_TYPE_MI2S		0x001E0016


static struct gpio mmi_mi2s_gpios[] = {
	{
		.label = "gpio_ws",
	},
	{
		.label = "gpio_sck",
	},
	{
		.label = "gpio_din",
	},
	{
		.label = "gpio_dout",
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

void __init mmi_i2s_dai_init(void)
{
	struct device_node *node;
	const void *prop;
	int len = 0;
	int type;
	int i;

	node = of_find_node_by_path(DT_PATH_I2S);
	if (!node)
		return;

	prop = of_get_property(node, DT_PROP_I2S_TYPE, &len);
	if (prop && (len == sizeof(int)))
		type = *((int *)prop);
	else
		goto exit;

	if (type != DT_TYPE_MI2S) {
		pr_err("%s: DT node wrong type: 0x%08X.\n", __func__, type);
		goto exit;
	}

	for (i = 0; i < ARRAY_SIZE(mmi_mi2s_gpios); ++i) {
		prop = of_get_property(node, mmi_mi2s_gpios[i].label, &len);
		if (prop && (len == sizeof(u32)))
			mmi_mi2s_gpios[i].gpio = *((u32 *)prop);
		else {
			pr_err("%s: Missing %s\n", __func__, mmi_mi2s_gpios[i].label);
			goto exit;
		}
	}

	platform_device_register(&mmi_msmcpudai_mi2s_rx);

exit:
	of_node_put(node);
}
