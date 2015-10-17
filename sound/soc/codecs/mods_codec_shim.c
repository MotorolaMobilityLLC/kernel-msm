/*
 * Copyright (C) 2015 Motorola Mobility, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <sound/core.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/mods_codec_dev.h>

#define MODS_RATES				SNDRV_PCM_RATE_48000
#define MODS_FMTS				SNDRV_PCM_FMTBIT_S16_LE

static struct mods_codec_device *mods_codec_dev;
static struct snd_soc_codec *priv_codec;
static DEFINE_MUTEX(mods_shim_lock);

static int mods_codec_shim_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	const struct snd_soc_dai_ops *dai_ops;
	int ret = 0;

	mutex_lock(&mods_shim_lock);
	if (!mods_codec_dev) {
		pr_warn("%s(): mods codec not connected yet", __func__);
		mutex_unlock(&mods_shim_lock);
		return -ENODEV;
	}
	dai_ops = mods_codec_dev->ops->dai_ops;
	if (dai_ops && dai_ops->hw_params)
		ret = dai_ops->hw_params(substream, params, dai);
	else
		pr_warn("%s(): mods codec hw_params() op not found\n",
						__func__);

	mutex_unlock(&mods_shim_lock);

	return ret;
}

static int mods_codec_shim_dai_trigger(struct snd_pcm_substream *substream,
			int cmd,
			struct snd_soc_dai *dai)
{
	const struct snd_soc_dai_ops *dai_ops;
	int ret = 0;

	if (!mods_codec_dev) {
		pr_warn("%s(): mods codec not connected yet", __func__);
		return 0;
	}
	dai_ops = mods_codec_dev->ops->dai_ops;
	if (dai_ops && dai_ops->trigger)
		ret = dai_ops->trigger(substream, cmd, dai);
	else
		pr_warn("%s(): mods codec trigger() op not found\n", __func__);

	return ret;
}

static int mods_codec_shim_dai_set_fmt(struct snd_soc_dai *dai,
			unsigned int fmt)
{
	const struct snd_soc_dai_ops *dai_ops;
	int ret = 0;

	mutex_lock(&mods_shim_lock);
	if (!mods_codec_dev) {
		pr_warn("%s(): mods codec not connected yet", __func__);
		mutex_unlock(&mods_shim_lock);
		return 0;
	}
	dai_ops = mods_codec_dev->ops->dai_ops;
	if (dai_ops && dai_ops->set_fmt)
		ret = dai_ops->set_fmt(dai, fmt);
	else
		pr_warn("%s(): mods codec set_fmt() op not found\n", __func__);

	mutex_unlock(&mods_shim_lock);

	return ret;
}

static int mods_codec_shim_set_dai_sysclk(struct snd_soc_dai *dai,
				  int clk_id, unsigned int freq, int dir)
{
	const struct snd_soc_dai_ops *dai_ops;
	int ret = 0;

	mutex_lock(&mods_shim_lock);
	if (!mods_codec_dev) {
		pr_warn("%s(): mods codec not connected yet", __func__);
		mutex_unlock(&mods_shim_lock);
		return -ENODEV;
	}
	dai_ops = mods_codec_dev->ops->dai_ops;
	if (dai_ops && dai_ops->set_sysclk)
		ret = dai_ops->set_sysclk(dai, clk_id, freq, dir);
	else
		pr_warn("%s(): mods codec set_sysclk() op not found\n",
					__func__);

	mutex_unlock(&mods_shim_lock);

	return ret;
}

static int mods_codec_shim_probe(struct snd_soc_codec *codec)
{
	int ret;

	mutex_lock(&mods_shim_lock);
	priv_codec = codec;
	if (mods_codec_dev) {
		snd_soc_codec_set_drvdata(priv_codec,
					mods_codec_dev->priv_data);
		/* probe mods gb codec */
		ret = mods_codec_dev->ops->probe(codec);
		if (ret) {
			pr_err("%s: failed probing %s\n", __func__,
				dev_name(&mods_codec_dev->dev));
			mods_codec_dev = NULL;
			mutex_unlock(&mods_shim_lock);
			return ret;
		}
	}
	mutex_unlock(&mods_shim_lock);

	pr_info("mods codec shim probed\n");

	return 0;
}

static int mods_codec_shim_remove(struct snd_soc_codec *codec)
{

	mutex_lock(&mods_shim_lock);

	if (mods_codec_dev)
		/* remove mods gb codec */
		mods_codec_dev->ops->remove(codec);

	mutex_unlock(&mods_shim_lock);

	return 0;
}

static const struct snd_soc_dai_ops mods_codec_shim_dai_ops = {
	.hw_params = mods_codec_shim_hw_params,
	.trigger	= mods_codec_shim_dai_trigger,
	.set_fmt	= mods_codec_shim_dai_set_fmt,
	.set_sysclk = mods_codec_shim_set_dai_sysclk,
};

static struct snd_soc_codec_driver soc_codec_shim_drv = {
	.probe = mods_codec_shim_probe,
	.remove = mods_codec_shim_remove,
};

static struct snd_soc_dai_driver mods_codec_shim_dai = {
	.name			= "mods_codec_shim_dai",
	.playback = {
		.stream_name = "Mods Dai Playback",
		.rates		= MODS_RATES,
		.formats	= MODS_FMTS,
		.channels_min	= 1,
		.channels_max	= 2,
	},
	.capture = {
		.stream_name = "Mods Dai Capture",
		.rates		= MODS_RATES,
		.formats	= MODS_FMTS,
		.channels_min	= 1,
		.channels_max	= 2,
	},
	.ops = &mods_codec_shim_dai_ops,
};

static int mods_codec_dev_drv_probe(struct mods_codec_device *mdev)
{
	int ret;

	pr_debug("%s: probing %s\n", __func__, dev_name(&mdev->dev));

	mutex_lock(&mods_shim_lock);
	if (!mdev->ops || !mdev->ops->probe || !mdev->ops->remove ||
				!mdev->ops->dai_ops)
		return -EINVAL;

	/* if plaform shim codec is already registered and dai link is probed
	 * probe the mod codec here.
	 */
	if (priv_codec) {
		snd_soc_codec_set_drvdata(priv_codec, mdev->priv_data);
		ret = mdev->ops->probe(priv_codec);
		if (ret) {
			pr_err("%s: failed probing %s\n", __func__,
					dev_name(&mdev->dev));
			mutex_unlock(&mods_shim_lock);
			return ret;
		}
	}

	mods_codec_dev = mdev;
	mutex_unlock(&mods_shim_lock);
	return 0;
}

static int mods_codec_dev_drv_remove(struct mods_codec_device *mdev)
{
	mutex_lock(&mods_shim_lock);
	mods_codec_dev->ops->remove(priv_codec);
	mods_codec_dev = NULL;
	mutex_unlock(&mods_shim_lock);
	return 0;
}

static const struct mods_codec_device_id mods_codec_dev_drv_table[] = {
	{"mods_codec", 0},
	{},
};
MODULE_DEVICE_TABLE(of, mods_codec_dev_drv_table);

static struct mods_codec_dev_driver mods_codec_drv  = {
	.driver = {
		.name = "mods_codec_drv",
		.owner = THIS_MODULE,
		.bus = &mods_codec_bus_type,
	},
	.probe = mods_codec_dev_drv_probe,
	.remove = mods_codec_dev_drv_remove,
	.id_table = mods_codec_dev_drv_table,
};

static int mods_codec_shim_drv_probe(struct platform_device *pdev)
{
	int ret;

	dev_set_name(&pdev->dev, "%s", "mods_codec_shim");
	/* register shim codec */
	ret = snd_soc_register_codec(&pdev->dev, &soc_codec_shim_drv,
						&mods_codec_shim_dai, 1);
	if (ret) {
		pr_err("%s: failed to register mods shim codec", __func__);
		return ret;
	}

	/* register mods codec device driver */
	ret = driver_register(&mods_codec_drv.driver);
	if (ret) {
		pr_err("%s: failed to register mods shim codec driver",
				__func__);
		goto unreg_codec;
	}

	pr_debug("%s: probed %s\n", __func__, dev_name(&pdev->dev));

	return 0;
unreg_codec:
	snd_soc_unregister_codec(&pdev->dev);
	return ret;
}

static int mods_codec_shim_drv_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	driver_unregister(&mods_codec_drv.driver);
	return 0;
}

static const struct of_device_id mods_codec_shim_match[] = {
	{.compatible = "mmi,mods-codec-shim",},
	{},
};
MODULE_DEVICE_TABLE(of, mods_codec_shim_match);

static const struct platform_device_id mods_codec_shim_id_table[] = {
	{"mods_codec_shim", 0},
	{},
};
MODULE_DEVICE_TABLE(of, mods_codec_shim_id_table);

static struct platform_driver mod_codec_shim_driver = {
	.driver = {
		.name = "mods_codec_shim",
		.owner = THIS_MODULE,
		.of_match_table = mods_codec_shim_match,
	},
	.probe = mods_codec_shim_drv_probe,
	.remove = mods_codec_shim_drv_remove,
	.id_table = mods_codec_shim_id_table,
};

static int __init mods_codec_shim_init(void)
{
	int ret;

	ret = mods_codec_bus_init();
	if (ret) {
		pr_err("%s: failed to register aud mods bus\n", __func__);
		return ret;
	}

	ret = platform_driver_register(&mod_codec_shim_driver);
	if (ret) {
		pr_err("%s: failed to register device driver\n", __func__);
		return ret;
	}
	return 0;
}

static void __exit mods_codec_shim_exit(void)
{
	mods_codec_bus_exit();
	platform_driver_unregister(&mod_codec_shim_driver);
}

module_init(mods_codec_shim_init);
module_exit(mods_codec_shim_exit);

MODULE_ALIAS("platform:mods_codec_shim");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Motorola");
MODULE_DESCRIPTION("Mods Codec Shim");
