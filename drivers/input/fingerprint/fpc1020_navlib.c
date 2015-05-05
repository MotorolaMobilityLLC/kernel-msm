/* FPC1020 Touch sensor driver
 *
 * Copyright (c) 2014 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#include "fpc1020_navlib.h"
#include "fpc1020_common.h"
#ifdef FPC1020_NAVLIB_EXTERNAL_DEBUG
#define EINVAL 5
#else
#include <linux/errno.h>
#endif /* #ifdef FPC1020_NAVLIB_EXTERNAL_DEBUG */
#define I32_MAX      (0x7fffffff)	// Max positive value representable as 32-bit signed integer
#define ABS(X)       (((X) < 0) ? -(X) : (X))	// Fast absolute value for 32-bit integers

static nav_setup_t nav_setup;

// Threshold of how different the best match may be, if the best match is more different than this value then we consider the movement to be nonexistant
#define NORM_MIN_DIFF_THRESHOLD 20

static int calculate_diff(const u8 * p_curr, const u8 * p_prev, int cmp_width,
			  int cmp_height, int diff_limit)
{
	int x, y;
	int diff = 0;

	for (y = 0; y < cmp_height; y++) {
		for (x = 0; x < cmp_width; x++) {
			int i = x + y * nav_setup.width;
			diff += ABS(p_curr[i] - p_prev[i]);
		}

		// Not good enough, abandon early
		if (diff > diff_limit) {
			return I32_MAX;
		}
	}

	return diff;
}

int fpc1020_navlib_initialize(fpc1020_chip_t chip_type)
{
	if ((chip_type == FPC1020_CHIP_1021A)
	    || (chip_type == FPC1020_CHIP_1021B)) {
		nav_setup.row_skip = 4;
		nav_setup.col_mask = 4;
		nav_setup.crop_x = 0;
		nav_setup.crop_y = 0;
		nav_setup.crop_w = 160;
		nav_setup.crop_h = 160;
		nav_setup.max_dx = 64 / nav_setup.col_mask;
		nav_setup.max_dy = 64 / nav_setup.row_skip;
		nav_setup.delay_size = 3;
		// Backmounted
		nav_setup.dx_to_dx = -1;
		nav_setup.dy_to_dx = 0;
		nav_setup.dx_to_dy = 0;
		nav_setup.dy_to_dy = 1;
		nav_setup.time = 20000u;
	} else if (chip_type == FPC1020_CHIP_1140B) {
		nav_setup.row_skip = 4;
		nav_setup.col_mask = 4;
		nav_setup.crop_x = 0;
		nav_setup.crop_y = 0;
		nav_setup.crop_w = 56;
		nav_setup.crop_h = 192;
		nav_setup.max_dx = 32 / nav_setup.col_mask;
		nav_setup.max_dy = 64 / nav_setup.row_skip;
		nav_setup.delay_size = 7;
		// Sidemounted
		nav_setup.dx_to_dx = -1;
		nav_setup.dy_to_dx = 0;
		nav_setup.dx_to_dy = 0;
		nav_setup.dy_to_dy = -1;
		nav_setup.time = 60000u;
	} else {
		pr_err("%s fail: line:%d fpc1020->chip.type=%d \n", __func__,
		       __LINE__, chip_type);
		return -EINVAL;
	}

	nav_setup.width = nav_setup.crop_w / nav_setup.col_mask;
	nav_setup.height = nav_setup.crop_h / nav_setup.row_skip;
	nav_setup.size = nav_setup.width * nav_setup.height;
	pr_info("%s : line:%d nav_setup.size =%d\n", __func__, __LINE__,
		nav_setup.size);

	return 0;
}

nav_setup_t *fpc1020_navlib_get_nav_setup(void)
{
	return &nav_setup;
}

int fpc1020_navlib_handle_sensor_positioning_dx(int dx, int dy)
{
	return dx * nav_setup.dx_to_dx + dy * nav_setup.dy_to_dx;
}

int fpc1020_navlib_handle_sensor_positioning_dy(int dx, int dy)
{
	return dx * nav_setup.dx_to_dy + dy * nav_setup.dy_to_dy;
}

int get_nav_img_capture_size(void)
{
	return nav_setup.size;
}

// TODO There is a problem with this algorithm where it returns movement when the finger enters/leaves the sensor
void get_movement(const u8 * p_curr, const u8 * p_prev, int *p_dx, int *p_dy)
{
	int x, y;
	int comp_w = nav_setup.width - nav_setup.max_dx;
	int comp_h = nav_setup.height - nav_setup.max_dy;
	int min_diff = I32_MAX;
	// Default vector
	*p_dx = 0;
	*p_dy = 0;

	// Calculate translation vector
	for (y = -nav_setup.max_dy; y <= nav_setup.max_dy; ++y) {
		for (x = -nav_setup.max_dx; x <= nav_setup.max_dx; ++x) {

			int diff;

			diff =
			    calculate_diff(p_curr +
					   ((x + nav_setup.max_dx) / 2) +
					   ((y +
					     nav_setup.max_dy) / 2) *
					   nav_setup.width,
					   p_prev +
					   ((nav_setup.max_dx - x) / 2) +
					   ((nav_setup.max_dy -
					     y) / 2) * nav_setup.width, comp_w,
					   comp_h, min_diff);
			if (diff < min_diff) {
				min_diff = diff;
				*p_dx = x;
				*p_dy = y;
			}
		}
	}

	if ((min_diff / (comp_w * comp_h)) > NORM_MIN_DIFF_THRESHOLD) {
		*p_dx = 0;
		*p_dy = 0;
	}
	// Account for masked columns and skipped rows
	*p_dx *= nav_setup.col_mask;
	*p_dy *= nav_setup.row_skip;
}
