/*
 * platform_fsa642.c: fsa642 platform data initialization file
 *
 * (C) Copyright 2014 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/gpio.h>
#include "platform_camera.h"
#include "platform_fsa642.h"

static int mux_gpio_number = -1;
static int mux_gpio_searched = -1;

int fsa642_gpio_ctrl(int flag)
{
	if (mux_gpio_searched < 0) {
		/* The FSA642 mux is not used in all HW configurations, hence
		 * it is okay to receive an error if the GPIO is not found */
		mux_gpio_number = camera_sensor_gpio(-1, GP_CAMERA_MUX_CONTROL,
						     GPIOF_DIR_OUT, 0);
		mux_gpio_searched = 1;
	}

	/* If the FSA642 mux is available, switch it */
	if (mux_gpio_searched && mux_gpio_number >= 0)
		gpio_set_value(mux_gpio_number, flag);

	return 0;
}
