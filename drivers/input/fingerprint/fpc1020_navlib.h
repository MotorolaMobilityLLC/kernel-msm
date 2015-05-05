/* FPC1020 Touch sensor driver
 *
 * Copyright (c) 2014 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#ifndef LINUX_SPI_FPC1020_NAVLIB_H
#define LINUX_SPI_FPC1020_NAVLIB_H

#ifdef FPC1020_NAVLIB_EXTERNAL_DEBUG
typedef unsigned char u8;
#else
#include <linux/types.h>
#endif /* #ifdef FPC1020_NAVLIB_EXTERNAL_DEBUG */

#include "fpc1020_common.h"
/** Qualification time for click
	Android ViewConfiguration: 180 ms */
#define FPC1020_QUALIFICATION_TIME_NS_CLICK      (180000000)

/** Qualification time for long click
	Android ViewConfiguration: 500 ms */
#define FPC1020_QUALIFICATION_TIME_NS_LONGPRESS  (500000000)

/** Navigation speed and sensitivity to movement */
#define SENSOR_PIXEL_TO_REPORT_REL_DIVIDER 8
#define SENSOR_PIXEL_TOUCH_SLOP 8

/** Handle sensor placement and rotation */

typedef struct {
	// Source image crop area (unaffected by downscaling)
	int crop_x;
	int crop_y;
	int crop_w;
	int crop_h;
	// Downscaling
	int row_skip;
	int col_mask;
	// Max displacement to search for (after downscaling)
	int max_dx;
	int max_dy;
	// Image metrics (after downscaling)
	int width;
	int height;
	int size;
	// Position and rotation of sensor
	int dx_to_dx;
	int dy_to_dx;
	int dx_to_dy;
	int dy_to_dy;
	//Size of delays used to remove jittering problems (upper limit is defined, see: DELAY_QUEUE_MAX_SIZE)
	int delay_size;		// TODO Make this time dependant instead
	unsigned long time;	// the max diff time used when check move
} nav_setup_t;

/**
 * Initialize internal structures based on chip type.
 * Call this before calling any other navlib functions.
 *
 * @param chip_type Chip type used
 * @return 0 is successful, linux system error code otherwise
 */
int fpc1020_navlib_initialize(fpc1020_chip_t chip_type);

/**
 * Get access to navigation settings.
 * fpc1020_navlib_initialize must be called before calling this function
 * otherwise the state of the settings is undefined.
 *
 * @return Navigation settings
 */
nav_setup_t *fpc1020_navlib_get_nav_setup(void);
/** 
 * Calculate translation vector between two different navigation images
 *
 * @param p_curr	Pointer to image buffer of latest navigation image
 * @param p_prev	Pointer to image buffer of previous navigation image
 * @param p_dx     	Out parameter for translation vector x
 * @param p_dy     	Out parameter for translation vector y
 */
/**
 * Get access to navigation settings.
 * fpc1020_navlib_initialize must be called before calling this function
 * otherwise the state of the settings is undefined.
 *
 * @return Navigation settings
 */
int fpc1020_navlib_handle_sensor_positioning_dx(int dx, int dy);

/**
 * Convert dx and dy from translation calculations to the actual
 * dy value taking into account sensor rotation and positioning.
 *
 * @param dx Resulting dx from translation calculations
 * @param dy Resulting dy from translation calculations
 * @return The dy value after taking into account rotation and positioning.
 */
int fpc1020_navlib_handle_sensor_positioning_dy(int dx, int dy);
int get_nav_img_capture_size(void);
void get_movement(const u8 * p_curr, const u8 * p_prev, int *p_dx, int *p_dy);

#endif /* LINUX_SPI_FPC1020_NAVLIB_H */
