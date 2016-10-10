/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/clk/msm-clk.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <dt-bindings/clock/msm-clocks-8909.h>
#include <sound/q6afe-v2.h>
#include <sound/q6core.h>
#include <linux/io.h>
#include "../msm/msm-audio-pinctrl.h"

#define LPASS_CSR_GP_IO_MUX_MIC_CTL 0x07702000
#define LPASS_CSR_GP_IO_MUX_SPKR_CTL 0x07702004

struct pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *sleep;
	struct pinctrl_state *active;
};

struct audio_ext_ap_clk {
	bool enabled;
	u32 lpass_clock;
	int gpio;
	struct clk c;
	struct pinctrl_info pnctrl_info;
};

struct audio_ext_pmi_clk {
	int gpio;
	struct clk c;
};

static struct afe_clk_set digital_cdc_core_clk = {
	Q6AFE_LPASS_CLK_CONFIG_API_VERSION,
	Q6AFE_LPASS_CLK_ID_INTERNAL_DIGITAL_CODEC_CORE,
	Q6AFE_LPASS_OSR_CLK_9_P600_MHZ,
	Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
};

static int audio_ext_clk_prepare(struct clk *clk);
static void audio_ext_clk_unprepare(struct clk *clk);


static struct clk_ops audio_ext_ap_clk_ops = {
	.prepare = audio_ext_clk_prepare,
	.unprepare = audio_ext_clk_unprepare,
};

static struct audio_ext_pmi_clk audio_pmi_clk = {
	.c = {
		.dbg_name = "audio_ext_pmi_clk",
		.ops = &clk_ops_dummy,
		CLK_INIT(audio_pmi_clk.c),
	},
};

static struct audio_ext_ap_clk audio_ap_clk = {
	.c = {
		.dbg_name = "audio_ext_ap_clk",
		.ops = &audio_ext_ap_clk_ops,
		CLK_INIT(audio_ap_clk.c),
	},
};

static struct clk_lookup audio_ref_clock[] = {
	CLK_LIST(audio_ap_clk),
	CLK_LIST(audio_pmi_clk),
};


struct afe_digital_clk_cfg tasha_digital_cdc_clk;

static inline struct audio_ext_ap_clk *to_audio_ap_clk(struct clk *clk)
{
	return container_of(clk, struct audio_ext_ap_clk, c);
}

static int audio_ext_clk_prepare(struct clk *clk)
{

	int ret = 0, val = 0;
	void __iomem *vaddr = NULL;
	struct audio_ext_ap_clk *audio_clk = to_audio_ap_clk(clk);

	if (audio_ap_clk.lpass_clock) {

		/* Reset the TLMM pins to a default state */
		ret = pinctrl_select_state(audio_ap_clk.pnctrl_info.pinctrl,
					   audio_ap_clk.pnctrl_info.sleep);
		if (ret != 0) {
			pr_err("%s: Disable TLMM pins failed with %d\n",
				   __func__, ret);
			ret = -EIO;
		}

		 ret = pinctrl_select_state(audio_ap_clk.pnctrl_info.pinctrl,
					  audio_ap_clk.pnctrl_info.active);
		if (ret) {
			pr_err("%s: active state select failed with %d\n",
					__func__, ret);
			ret = -EIO;
			goto err;
		}

		vaddr = ioremap(LPASS_CSR_GP_IO_MUX_SPKR_CTL , 4);
		val = ioread32(vaddr);
		val = val | 0x00000002;
		iowrite32(val, vaddr);

		vaddr = ioremap(LPASS_CSR_GP_IO_MUX_MIC_CTL , 4);
		val = ioread32(vaddr);
		val = val | 0x00000002;
		iowrite32(val, vaddr);

		switch (q6core_get_avs_version()) {
		case (Q6_SUBSYS_AVS2_6):
			tasha_digital_cdc_clk.i2s_cfg_minor_version =
				AFE_API_VERSION_I2S_CONFIG;
			tasha_digital_cdc_clk.clk_val = 9600000;
			tasha_digital_cdc_clk.clk_root = 5;
			tasha_digital_cdc_clk.reserved = 0;
			ret = afe_set_digital_codec_core_clock(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&tasha_digital_cdc_clk);
			if (ret < 0) {
				pr_err("%s afe_set_digital_codec_core_clock failed\n",
					__func__);
				return ret;
			}
			break;
		case (Q6_SUBSYS_AVS2_7):
		case (Q6_SUBSYS_AVS2_8):
			digital_cdc_core_clk.enable = 1;
			ret = afe_set_lpass_clock_v2(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&digital_cdc_core_clk);
			break;
		case (Q6_SUBSYS_INVALID):
		default:
			ret = -EINVAL;
			pr_err("%s: INVALID AVS IMAGE\n", __func__);
			break;
		}
	} else {
		pr_err("%s: gpio: %d\n", __func__, audio_clk->gpio);
		return gpio_direction_output(audio_clk->gpio, 1);
	}
err:
	return 0;
}

static void audio_ext_clk_unprepare(struct clk *clk)
{

	int ret = 0;
	struct audio_ext_ap_clk *audio_clk = to_audio_ap_clk(clk);

	if (audio_ap_clk.lpass_clock) {
		ret = pinctrl_select_state(audio_ap_clk.pnctrl_info.pinctrl,
					   audio_ap_clk.pnctrl_info.sleep);
		if (ret != 0) {
			pr_err("%s: Disable TLMM pins failed with %d\n",
				   __func__, ret);
			ret = -EIO;
		}
		switch (q6core_get_avs_version()) {
		case (Q6_SUBSYS_AVS2_6):
			tasha_digital_cdc_clk.clk_val = 0;

			ret = afe_set_digital_codec_core_clock(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&tasha_digital_cdc_clk);

			if (ret < 0) {
				pr_err("%s afe_set_digital_codec_core_clock failed\n",
					__func__);
			}
		case (Q6_SUBSYS_AVS2_7):
		case (Q6_SUBSYS_AVS2_8):
			digital_cdc_core_clk.enable = 0;
			ret = afe_set_lpass_clock_v2(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&digital_cdc_core_clk);
			break;
		case (Q6_SUBSYS_INVALID):
		default:
			ret = -EINVAL;
			pr_err("%s: INVALID AVS IMAGE\n", __func__);
			break;
		}
	} else {
		pr_debug("%s: gpio: %d\n", __func__, audio_clk->gpio);
		gpio_direction_output(audio_clk->gpio, 0);
	}
}

static int audio_get_pinctrl(struct platform_device *pdev)
{
	struct pinctrl_info *pinctrl_info;
	struct pinctrl *pinctrl;

	int ret = 0;

	pinctrl_info = &audio_ap_clk.pnctrl_info;

	if (pinctrl_info == NULL) {
		pr_err("%s pinctrl_info is NULL\n", __func__);
		return -EINVAL;
	}

	pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		pr_err("%s: Unable to get pinctrl handle\n", __func__);
		return -EINVAL;
	}
	pinctrl_info->pinctrl = pinctrl;
	/* get all the states handles from Device Tree */
	pinctrl_info->sleep = pinctrl_lookup_state(pinctrl, "i2s_mclk_sleep");
	if (IS_ERR(pinctrl_info->sleep)) {
		pr_err("%s: could not get sleep pinstate\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	pinctrl_info->active = pinctrl_lookup_state(pinctrl,
					"i2s_mclk_active");
	if (IS_ERR(pinctrl_info->active)) {
		pr_err("%s: could not get active pinstate\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	return 0;
err:
	devm_pinctrl_put(pinctrl);
	pinctrl_info->pinctrl = NULL;
	return ret;
}

static int audio_ref_clk_probe(struct platform_device *pdev)
{
	int clk_gpio;
	int ret;
	struct clk *div_clk1;

	ret = of_property_read_u32(pdev->dev.of_node,
					"qcom,lpass-clock",
					&audio_ap_clk.lpass_clock);
	if (audio_ap_clk.lpass_clock) {
		ret = audio_get_pinctrl(pdev);
		if (ret)
			pr_err("debug %s: Parsing pinctrl failed\n",
				__func__);
	} else {
		clk_gpio = of_get_named_gpio(pdev->dev.of_node,
					"qcom,audio-ref-clk-gpio", 0);
		if (clk_gpio < 0) {
			dev_err(&pdev->dev,
			"Looking up %s property in node %s failed %d\n",
				"qcom,audio-ref-clk-gpio",
				pdev->dev.of_node->full_name,
				clk_gpio);
			ret = -EINVAL;
			goto err;
		}
		ret = gpio_request(clk_gpio, "EXT_CLK");
		if (ret) {
			dev_err(&pdev->dev,
				"Request ext clk gpio failed %d, err:%d\n",
				clk_gpio, ret);
			goto err;
		}
		if (of_property_read_bool(pdev->dev.of_node,
				"qcom,node_has_rpm_clock")) {
			div_clk1 = clk_get(&pdev->dev, "osr_clk");
			if (IS_ERR(div_clk1)) {
				dev_err(&pdev->dev,
					"Failed to get RPM div clk\n");
				ret = PTR_ERR(div_clk1);
				goto err_gpio;
			}
			audio_pmi_clk.c.parent = div_clk1;
			audio_pmi_clk.gpio = clk_gpio;
		} else
			audio_ap_clk.gpio = clk_gpio;
	}
	ret = of_msm_clock_register(pdev->dev.of_node, audio_ref_clock,
			      ARRAY_SIZE(audio_ref_clock));
	if (ret) {
		dev_err(&pdev->dev, "%s: audio ref clock register failed\n",
			__func__);
		goto err_gpio;
	 }

	return 0;

err_gpio:
	gpio_free(clk_gpio);
err:
	return ret;
}

static int audio_ref_clk_remove(struct platform_device *pdev)
{
	if (audio_pmi_clk.gpio > 0)
		gpio_free(audio_pmi_clk.gpio);
	else if (audio_ap_clk.gpio > 0)
		gpio_free(audio_ap_clk.gpio);

	return 0;
}

static const struct of_device_id audio_ref_clk_match[] = {
	{.compatible = "qcom,audio-ref-clk"},
	{}
};
MODULE_DEVICE_TABLE(of, audio_ref_clk_match);

static struct platform_driver audio_ref_clk_driver = {
	.driver = {
		.name = "audio-ref-clk",
		.owner = THIS_MODULE,
		.of_match_table = audio_ref_clk_match,
	},
	.probe = audio_ref_clk_probe,
	.remove = audio_ref_clk_remove,
};

static int __init audio_ref_clk_platform_init(void)
{
	return platform_driver_register(&audio_ref_clk_driver);
}
module_init(audio_ref_clk_platform_init);

static void __exit audio_ref_clk_platform_exit(void)
{
	platform_driver_unregister(&audio_ref_clk_driver);
}
module_exit(audio_ref_clk_platform_exit);

MODULE_DESCRIPTION("Audio Ref Clock module platform driver");
MODULE_LICENSE("GPL v2");
