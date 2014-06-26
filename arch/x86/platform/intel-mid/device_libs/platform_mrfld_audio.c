/*
 * platform_mrfld_audio.c: MRFLD audio platform data initilization file
 *
 * (C) Copyright 2012 Intel Corporation
 * Author: Dharageswari R <dharageswari.r@intel.com>
 *	Vinod Koul <vinod.koul@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/scatterlist.h>
#include <linux/init.h>
#include <linux/sfi.h>
#include <linux/platform_device.h>
#include <asm/intel-mid.h>
#include <linux/platform_data/intel_mid_remoteproc.h>
#include <asm/platform_sst_audio.h>
#include <asm/platform_mrfld_audio.h>
#include "platform_msic.h"

static struct mrfld_audio_platform_data mrfld_audio_pdata = {
	.spid = &spid,
};

void *merfld_audio_platform_data(void *info)
{
	struct platform_device *pdev;
	int ret;

	pr_debug("in %s\n", __func__);

	ret = add_sst_platform_device();
	if (ret < 0) {
		pr_err("%s failed to sst_platform device\n", __func__);
		return NULL;
	}

	pdev = platform_device_alloc("hdmi-audio", -1);
	if (!pdev) {
		pr_err("failed to allocate hdmi-audio platform device\n");
		return NULL;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		pr_err("failed to add hdmi-audio platform device\n");
		platform_device_put(pdev);
		return NULL;
	}

	/* request the gpios for audio */
	mrfld_audio_pdata.codec_gpio = get_gpio_by_name("audiocodec_int");
	mrfld_audio_pdata.codec_rst = get_gpio_by_name("audiocodec_rst");

	pdev = platform_device_alloc("mrfld_lm49453", -1);
	if (!pdev) {
		pr_err("failed to allocate mrfld_lm49453 platform device\n");
		return NULL;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		pr_err("failed to add mrfld_lm49453 platform device\n");
		platform_device_put(pdev);
		return NULL;
	}
	if (platform_device_add_data(pdev, &mrfld_audio_pdata,
				     sizeof(mrfld_audio_pdata))) {
		pr_err("failed to add mrfld_lm49453 platform data\n");
		platform_device_put(pdev);
		return NULL;
	}

	register_rpmsg_service("rpmsg_msic_mrfld_audio", RPROC_SCU,
				RP_MSIC_MRFLD_AUDIO);

	return NULL;
}

void *merfld_wm8958_audio_platform_data(void *info)
{
	struct platform_device *pdev;
	int ret;

	ret = add_sst_platform_device();
	if (ret < 0) {
		pr_err("%s failed to sst_platform device\n", __func__);
		return NULL;
	}

	pdev = platform_device_alloc("hdmi-audio", -1);
	if (!pdev) {
		pr_err("failed to allocate hdmi-audio platform device\n");
		return NULL;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		pr_err("failed to add hdmi-audio platform device\n");
		platform_device_put(pdev);
		return NULL;
	}

	pdev = platform_device_alloc("mrfld_wm8958", -1);
	if (!pdev) {
		pr_err("failed to allocate mrfld_wm8958 platform device\n");
		return NULL;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		pr_err("failed to add mrfld_wm8958 platform device\n");
		platform_device_put(pdev);
		return NULL;
	}
	/* Speaker boost gpio is required on moorefield mofd_v0 PR1 phone
	 * If its not populated get_gpio_by_name will return -1 */
	mrfld_audio_pdata.spk_gpio = get_gpio_by_name("spkr_boost_en");
	pr_info("Speaker boost gpio is %d\n", mrfld_audio_pdata.spk_gpio);
	if (platform_device_add_data(pdev, &mrfld_audio_pdata,
				     sizeof(mrfld_audio_pdata))) {
		pr_err("failed to add mrfld_wm8958 platform data\n");
		platform_device_put(pdev);
		return NULL;
	}

	register_rpmsg_service("rpmsg_mrfld_wm8958_audio", RPROC_SCU,
				RP_MSIC_MRFLD_AUDIO);

	return NULL;
}
