/*
 * platform_i2c_gpio.c: i2c_gpio platform data initilization file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/input.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/i2c-gpio.h>
#include <linux/platform_device.h>
#include <linux/lnw_gpio.h>
#include <asm/intel-mid.h>
#include "platform_i2c_gpio.h"

static int hdmi_i2c_workaround(void)
{
	struct platform_device *pdev;
	struct i2c_gpio_platform_data *pdata;

	/*
	 * Hard code a gpio controller platform device to take over
	 * the two gpio pins used to be controlled by i2c bus 3.
	 * This is to support HDMI EDID extension block read, which
	 * is not supported by the current i2c controller, so we use
	 * GPIO pin the simulate an i2c bus.
	 */

	/* On Merrifield, bus number 8 is used for battery charger.
	 *  use 10 across medfield/ctp/merrifield platforms.
	 */
	pdev = platform_device_alloc(DEVICE_NAME, 10);

	if (!pdev) {
		pr_err("i2c-gpio: failed to alloc platform device\n");
		return -ENOMEM;
	}

	pdata = kzalloc(sizeof(struct i2c_gpio_platform_data), GFP_KERNEL);
	if (!pdata) {
		pr_err("i2c-gpio: failed to alloc platform data\n");
		kfree(pdev);
		return -ENOMEM;
	}
	if (INTEL_MID_BOARD(1, PHONE, MRFL) ||
	    INTEL_MID_BOARD(1, TABLET, MRFL) ||
		INTEL_MID_BOARD(1, PHONE, MOFD) ||
		INTEL_MID_BOARD(1, TABLET, MOFD)) {
		/* Pins 17 and 18 are used in Merrifield/MOOR-PRH for HDMI i2c (bus3) */
		pdata->scl_pin = 17;
		pdata->sda_pin = 18;
	} else {
		pdata->scl_pin = 35 + 96;
		pdata->sda_pin = 36 + 96;
	}
	pdata->sda_is_open_drain = 0;
	pdata->scl_is_open_drain = 0;
	pdev->dev.platform_data = pdata;

	platform_device_add(pdev);

	lnw_gpio_set_alt(pdata->sda_pin, LNW_GPIO);
	lnw_gpio_set_alt(pdata->scl_pin, LNW_GPIO);

	return 0;
}
rootfs_initcall(hdmi_i2c_workaround);

