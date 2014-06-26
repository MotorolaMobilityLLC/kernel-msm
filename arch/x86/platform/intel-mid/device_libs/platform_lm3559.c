/*
 * platform_lm3559.c: lm3559 platform data initilization file
 *
 * (C) Copyright 2012 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/init.h>
#include <linux/types.h>
#include <asm/intel-mid.h>

#include <media/lm3559.h>

#include "platform_lm3559.h"

void *lm3559_platform_data_func(void *info)
{
	static struct lm3559_platform_data platform_data;

	platform_data.gpio_reset  = get_gpio_by_name("GP_FLASH_RESET");
	platform_data.gpio_strobe = get_gpio_by_name("GP_FLASH_STROBE");
	platform_data.gpio_torch  = get_gpio_by_name("GP_FLASH_TORCH");

	if (platform_data.gpio_reset == -1) {
		pr_err("%s: Unable to find GP_FLASH_RESET\n", __func__);
		return NULL;
	}
	if (platform_data.gpio_strobe == -1) {
		pr_err("%s: Unable to find GP_FLASH_STROBE\n", __func__);
		return NULL;
	}
	if (platform_data.gpio_torch == -1) {
		pr_err("%s: Unable to find GP_FLASH_TORCH\n", __func__);
		return NULL;
	}

	pr_info("camera pdata: lm3559: reset: %d strobe %d torch %d\n",
		platform_data.gpio_reset, platform_data.gpio_strobe,
		platform_data.gpio_torch);

	/* Set to TX2 mode, then ENVM/TX2 pin is a power amplifier sync input:
	 * ENVM/TX pin asserted, flash forced into torch;
	 * ENVM/TX pin desserted, flash set back;
	 */
	platform_data.envm_tx2 = 1;
	platform_data.tx2_polarity = 0;
	platform_data.disable_tx2 = 0;
	platform_data.flash_current_limit = 15;

	/* set peak current limit to be 3.2A */
	platform_data.current_limit = 0x3;

	return &platform_data;
}
