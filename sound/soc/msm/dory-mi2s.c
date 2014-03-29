/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 * Copyright (C) 2013 Google, Inc.
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
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/q6afe-v2.h>
#include "qdsp6v2/msm-pcm-routing-v2.h"

#define DRV_NAME "dory-mi2s"

struct dory_data {
	struct regulator *mic_supply;
	atomic_t mi2s_rsc_ref;
};

static struct gpio mi2s_gpio[] = {
	{
		.flags = 0,
		.label = "dory,mic-sck-gpio",
	},
	{
		.flags = 0,
		.label = "dory,mic-ws-gpio",
	},
	{
		.flags = 0,
		.label = "dory,mic-din-gpio",
	},
};

static struct gpio mic_en_gpio = {
	.flags = GPIOF_OUT_INIT_HIGH,
	.label = "dory,mic-en-gpio",
};

static struct afe_clk_cfg lpass_mi2s = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_IBIT_CLK_3_P072_MHZ,
	Q6AFE_LPASS_OSR_CLK_12_P288_MHZ,
	Q6AFE_LPASS_CLK_SRC_INTERNAL,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	Q6AFE_LPASS_MODE_BOTH_VALID,
	0,
};

static struct afe_clk_cfg lpass_mi2s_disable = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_IBIT_CLK_DISABLE,
	Q6AFE_LPASS_OSR_CLK_DISABLE,
	Q6AFE_LPASS_CLK_SRC_INTERNAL,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	Q6AFE_LPASS_MODE_BOTH_VALID,
	0,
};

static int dory_request_gpios(struct platform_device *pdev)
{
	int ret;
	int i;

	for (i = 0; i < ARRAY_SIZE(mi2s_gpio); i++)
		mi2s_gpio[i].gpio = of_get_named_gpio(pdev->dev.of_node,
						mi2s_gpio[i].label, 0);

	ret = gpio_request_array(mi2s_gpio, ARRAY_SIZE(mi2s_gpio));
	if (ret) {
		pr_err("Unable to request gpios");
		return ret;
	}

	/* Request optional mic enable GPIO */
	mic_en_gpio.gpio = of_get_named_gpio(pdev->dev.of_node,
							mic_en_gpio.label, 0);
	if (gpio_is_valid(mic_en_gpio.gpio)) {
		ret = gpio_request_one(mic_en_gpio.gpio, mic_en_gpio.flags,
							mic_en_gpio.label);
		if (ret) {
			pr_err("Unable to request mic_en_gpio");
			gpio_free_array(mi2s_gpio, ARRAY_SIZE(mi2s_gpio));

			return ret;
		}
	}

	return 0;
}

static void dory_regulator_enable(struct dory_data *machine, bool enable)
{
	if (enable) {
		if (regulator_enable(machine->mic_supply))
			pr_err("enable mic-supply failed");
	} else {
		regulator_disable(machine->mic_supply);
	}
}

static int dory_mi2s_startup(struct snd_pcm_substream *substream)
{
	int ret;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct dory_data *machine = snd_soc_card_get_drvdata(rtd->card);

	if (atomic_inc_return(&machine->mi2s_rsc_ref) == 1) {

		if (gpio_is_valid(mic_en_gpio.gpio))
			gpio_set_value(mic_en_gpio.gpio, 1);

		dory_regulator_enable(machine, true);

		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
		if (ret < 0)
			dev_err(cpu_dai->dev, "set format for CPU dai failed\n");
	}

	return 0;
}

static int dory_mi2s_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	/*
	 * This causes the mic to always run at 48KHz which results
	 * in a smaller warm-up delay. Remove this function to allow
	 * 8, 16 and 48KHz.
	 */
	rate->min = 48000;
	rate->max = 48000;

	return 0;
}

static int dory_mi2s_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	int ret;

	/*
	 * This I2S mic only supports 24 bit output, which is why the
	 * clocks are never modified for 16 bit use.
	 */
	switch (params_rate(params)) {
	case 8000:
		lpass_mi2s.clk_val1 = Q6AFE_LPASS_IBIT_CLK_512_KHZ;
		lpass_mi2s.clk_val2 = Q6AFE_LPASS_OSR_CLK_2_P048_MHZ;
		break;
	case 16000:
		lpass_mi2s.clk_val1 = Q6AFE_LPASS_IBIT_CLK_1_P024_MHZ;
		lpass_mi2s.clk_val2 = Q6AFE_LPASS_OSR_CLK_4_P096_MHZ;
		break;
	default:
		lpass_mi2s.clk_val1 = Q6AFE_LPASS_IBIT_CLK_3_P072_MHZ;
		lpass_mi2s.clk_val2 = Q6AFE_LPASS_OSR_CLK_12_P288_MHZ;
	}

	ret = afe_set_lpass_clock(AFE_PORT_ID_PRIMARY_MI2S_RX,
							&lpass_mi2s);
	if (ret < 0) {
		pr_err("Unable to enable LPASS clock");
		return ret;
	}

	return 0;
}

static void dory_mi2s_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct dory_data *machine = snd_soc_card_get_drvdata(rtd->card);

	if (atomic_dec_return(&machine->mi2s_rsc_ref) == 0) {
		if (afe_set_lpass_clock(MI2S_RX, &lpass_mi2s_disable) < 0)
			pr_err("Unable to disable LPASS clock");

		if (gpio_is_valid(mic_en_gpio.gpio))
			gpio_set_value(mic_en_gpio.gpio, 0);

		dory_regulator_enable(machine, false);
	}
}

static struct snd_soc_ops dory_mi2s_be_ops = {
	.startup = dory_mi2s_startup,
	.hw_params = dory_mi2s_hw_params,
	.shutdown = dory_mi2s_shutdown
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link dory_dai[] = {
	/* FrontEnd DAI Links */
	{
		.name = "Dory Media1",
		.stream_name = "MultiMedia1",
		.cpu_dai_name	= "MultiMedia1",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* This dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA1
	},
	/* Backend DAI Links */
	{
		.name = LPASS_BE_PRI_MI2S_TX,
		.stream_name = "Primary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.0",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_PRI_MI2S_TX,
		.be_hw_params_fixup = dory_mi2s_hw_params_fixup,
		.ops = &dory_mi2s_be_ops,
		.ignore_suspend = 1,
	},
};

struct snd_soc_card snd_soc_card_dory = {
	.name		= "dory-mi2s-snd-card",
	.dai_link	= dory_dai,
	.num_links	= ARRAY_SIZE(dory_dai),
};

static int dory_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_card_dory;
	int ret;
	struct dory_data *machine;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No platform supplied from device tree");
		return -EINVAL;
	}

	machine = devm_kzalloc(&pdev->dev, sizeof(struct dory_data),
								GFP_KERNEL);
	if (!machine) {
		dev_err(&pdev->dev, "Can't allocate machine data\n");
		return -ENOMEM;
	}

	machine->mic_supply = regulator_get(&pdev->dev, "dory,mic");

	if (!machine->mic_supply) {
		dev_err(&pdev->dev, "unable to get mic-supply regulator");
		return -EINVAL;
	}

	ret = dory_request_gpios(pdev);
	if (ret)
		goto err_request_gpios;

	atomic_set(&machine->mi2s_rsc_ref, 0);

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, machine);

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err_register_card;
	}

	return 0;

err_register_card:
	gpio_free_array(mi2s_gpio, ARRAY_SIZE(mi2s_gpio));
	if (gpio_is_valid(mic_en_gpio.gpio))
		gpio_free(mic_en_gpio.gpio);
err_request_gpios:
	regulator_put(machine->mic_supply);

	return ret;
}

static int dory_asoc_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct dory_data *machine = snd_soc_card_get_drvdata(card);

	gpio_free_array(mi2s_gpio, ARRAY_SIZE(mi2s_gpio));
	regulator_put(machine->mic_supply);
	snd_soc_unregister_card(card);

	return 0;
}

static const struct of_device_id dory_of_match[]  = {
	{ .compatible = "dory,dory-mi2s-audio", },
	{},
};

static struct platform_driver dory_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = dory_of_match,
	},
	.probe = dory_asoc_machine_probe,
	.remove = dory_asoc_machine_remove,
};
module_platform_driver(dory_asoc_machine_driver);

MODULE_DESCRIPTION("ALSA SoC dory-mi2s");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, dory_of_match);
