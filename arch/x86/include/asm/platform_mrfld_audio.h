/*
 * platform_mrfld_audio.h: MRFLD audio platform data header file
 *
 * (C) Copyright 2012 Intel Corporation
 * Author: Vinod Koul <vinod.koul@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_MRFLD_AUDIO_H_
#define _PLATFORM_MRFLD_AUDIO_H_

#include <linux/sfi.h>

struct mrfld_audio_platform_data {
	const struct soft_platform_id *spid;
	int codec_gpio;
	int codec_rst;
	int spk_gpio;
};

extern void __init *merfld_audio_platform_data(void *info) __attribute__((weak));
extern void __init *merfld_wm8958_audio_platform_data(void *info) __attribute__((weak));
#endif
