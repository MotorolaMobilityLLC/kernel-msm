/*
 * Copyright (C) 2014 Motorola Mobility, LLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include "tfa9890-core.h"

#define TFA9890_RATES	SNDRV_PCM_RATE_8000_48000
#define TFA9890_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE)

static struct snd_soc_codec_driver soc_codec_dev_stereo_tfa9890;

static int tfa9890_stereo_mute(struct snd_soc_dai *dai, int mute)
{
	if (mute)
		tfa9890_stereo_sync_set_mute(mute);

	return 0;
}

static int tfa9890_set_stereo_dai_sysclk(struct snd_soc_dai *codec_dai,
				  int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static const struct snd_soc_dai_ops tfa9890_stereo_ops = {
	.digital_mute = tfa9890_stereo_mute,
	.set_sysclk = tfa9890_set_stereo_dai_sysclk,
};

static struct snd_soc_dai_driver tfa9890_stereo_dai = {
	.name = "tfa9890_stereo",
	.playback = {
		     .stream_name = "Playback",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = TFA9890_RATES,
		     .formats = TFA9890_FORMATS,},
	.ops = &tfa9890_stereo_ops,
	.symmetric_rates = 1,
};

static int tfa9890_stereo_dev_probe(struct platform_device *pdev)
{
	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", "tfa9890-stereo-codec");

	dev_dbg(&pdev->dev, "dev name %s\n", dev_name(&pdev->dev));

	return snd_soc_register_codec(&pdev->dev,
		&soc_codec_dev_stereo_tfa9890,
		&tfa9890_stereo_dai, 1);
}

static int tfa9890_stereo_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}
static const struct of_device_id tfa9890_stereo_codec_dt_match[] = {
	{ .compatible = "nxp,tfa9890-stereo-codec", },
	{}
};

static struct platform_driver tfa9890_stereo_driver = {
	.driver = {
		.name = "tfa9890-stereo-codec",
		.owner = THIS_MODULE,
		.of_match_table = tfa9890_stereo_codec_dt_match,
	},
	.probe = tfa9890_stereo_dev_probe,
	.remove = tfa9890_stereo_dev_remove,
};

static int __init tfa9890_stereo_init(void)
{
	return platform_driver_register(&tfa9890_stereo_driver);
}
module_init(tfa9890_stereo_init);

static void __exit tfa9890_stereo_exit(void)
{
	platform_driver_unregister(&tfa9890_stereo_driver);
}
module_exit(tfa9890_stereo_exit);

MODULE_DESCRIPTION("TFA9890 Stereo Codec Stub driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");

