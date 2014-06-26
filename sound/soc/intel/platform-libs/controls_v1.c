/*
 *  controls_v1.c - Intel MID Platform driver ALSA controls for CTP
 *
 *  Copyright (C) 2012 Intel Corp
 *  Author: Jeeja KP <jeeja.kp@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#include <linux/slab.h>
#include <sound/intel_sst_ioctl.h>
#include <sound/soc.h>
#include "../sst_platform.h"
#include "../sst_platform_pvt.h"


static int sst_set_mixer_param(unsigned int device_input_mixer)
{
	if (!sst_dsp) {
		pr_err("sst: DSP not registered\n");
		return -ENODEV;
	}

	/*allocate memory for params*/
	return sst_dsp->ops->set_generic_params(SST_SET_ALGO_PARAMS,
						(void *)&device_input_mixer);
}
static int lpe_mixer_ihf_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);

	ucontrol->value.integer.value[0] = sst->lpe_mixer_input_ihf;
	return 0;
}

static int lpe_mixer_ihf_set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int device_input_mixer;
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);

	switch (ucontrol->value.integer.value[0]) {
	case 0:
		pr_debug("input is None\n");
		device_input_mixer = SST_STREAM_DEVICE_IHF
					| SST_INPUT_STREAM_NONE;
		break;
	case 1:
		pr_debug("input is PCM stream\n");
		device_input_mixer = SST_STREAM_DEVICE_IHF
					| SST_INPUT_STREAM_PCM;
		break;
	case 2:
		pr_debug("input is Compress  stream\n");
		device_input_mixer = SST_STREAM_DEVICE_IHF
					| SST_INPUT_STREAM_COMPRESS;
		break;
	case 3:
		pr_debug("input is Mixed stream\n");
		device_input_mixer = SST_STREAM_DEVICE_IHF
					| SST_INPUT_STREAM_MIXED;
		break;
	default:
		pr_err("Invalid Input:%ld\n", ucontrol->value.integer.value[0]);
		return -EINVAL;
	}
	sst->lpe_mixer_input_ihf  = ucontrol->value.integer.value[0];
	return sst_set_mixer_param(device_input_mixer);
}

static int lpe_mixer_headset_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);

	ucontrol->value.integer.value[0] = sst->lpe_mixer_input_hs;
	return 0;
}

static int lpe_mixer_headset_set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int mixer_input_stream;
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);

	switch (ucontrol->value.integer.value[0]) {
	case 0:
		pr_debug("input is None\n");
		mixer_input_stream = SST_STREAM_DEVICE_HS
					| SST_INPUT_STREAM_NONE;
		break;
	case 1:
		pr_debug("input is PCM stream\n");
		mixer_input_stream = SST_STREAM_DEVICE_HS
					 | SST_INPUT_STREAM_PCM;
		break;
	case 2:
		pr_debug("input is Compress  stream\n");
		mixer_input_stream = SST_STREAM_DEVICE_HS
					 | SST_INPUT_STREAM_COMPRESS;
		break;
	case 3:
		pr_debug("input is Mixed stream\n");
		mixer_input_stream = SST_STREAM_DEVICE_HS
					 | SST_INPUT_STREAM_MIXED;
		break;
	default:
		pr_err("Invalid Input:%ld\n", ucontrol->value.integer.value[0]);
		return -EINVAL;
	}
	sst->lpe_mixer_input_hs  = ucontrol->value.integer.value[0];
	return sst_set_mixer_param(mixer_input_stream);
}

static int sst_probe_byte_control_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{

	if (!sst_dsp) {
		pr_err("sst: DSP not registered\n");
		return -ENODEV;
	}

	return sst_dsp->ops->set_generic_params(SST_GET_PROBE_BYTE_STREAM,
				ucontrol->value.bytes.data);
}

static int sst_probe_byte_control_set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{

	if (!sst_dsp) {
		pr_err("sst: DSP not registered\n");
		return -ENODEV;
	}

	return sst_dsp->ops->set_generic_params(SST_SET_PROBE_BYTE_STREAM,
				ucontrol->value.bytes.data);
}

static const char *lpe_mixer_text[] = {
	"None", "PCM", "Compressed", "Mixed",
};

static const struct soc_enum lpe_mixer_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(lpe_mixer_text), lpe_mixer_text);

static const struct snd_kcontrol_new sst_controls_clv[] = {
	SOC_ENUM_EXT("LPE IHF mixer", lpe_mixer_enum,
		lpe_mixer_ihf_get, lpe_mixer_ihf_set),
	SOC_ENUM_EXT("LPE headset mixer", lpe_mixer_enum,
		lpe_mixer_headset_get, lpe_mixer_headset_set),
	SND_SOC_BYTES_EXT("SST Probe Byte Control", SST_MAX_BIN_BYTES,
		sst_probe_byte_control_get,
		sst_probe_byte_control_set),
};

int sst_platform_clv_init(struct snd_soc_platform *platform)
{
	struct sst_data *ctx = snd_soc_platform_get_drvdata(platform);
	ctx->lpe_mixer_input_hs = 0;
	ctx->lpe_mixer_input_ihf = 0;
	snd_soc_add_platform_controls(platform, sst_controls_clv,
						ARRAY_SIZE(sst_controls_clv));
	return 0;
}
