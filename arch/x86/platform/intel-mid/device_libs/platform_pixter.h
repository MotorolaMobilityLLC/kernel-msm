/*
 * platform_pixter.h: Pixter2+ platform library header file
 *
 * (C) Copyright 2014 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_PIXTER_H_
#define _PLATFORM_PIXTER_H_

#include <linux/atomisp_platform.h>

/* Supported TYPE:
	RAW_CAMERA
	SOC_CAMERA
   Supported FORMAT:
	ATOMISP_INPUT_FORMAT_RAW_8
	ATOMISP_INPUT_FORMAT_RAW_10
	ATOMISP_INPUT_FORMAT_YUV420_8
	ATOMISP_INPUT_FORMAT_YUV422_8
   Supported BAYER:
	atomisp_bayer_order_grbg
	atomisp_bayer_order_rggb
	atomisp_bayer_order_bggr
	atomisp_bayer_order_gbrg
   For YUV formats, BAYER should be set to 0.
*/

#define PIXTER_0_TYPE		RAW_CAMERA
#define PIXTER_0_FORMAT		ATOMISP_INPUT_FORMAT_RAW_10
#define PIXTER_0_BAYER		atomisp_bayer_order_rggb
#define PIXTER_0_LANES		4
#define PIXTER_0_STREAMS	1

#define PIXTER_1_TYPE		SOC_CAMERA
#define PIXTER_1_FORMAT		ATOMISP_INPUT_FORMAT_YUV422_8
#define PIXTER_1_BAYER		0
#define PIXTER_1_LANES		1
#define PIXTER_1_STREAMS	1

#define PIXTER_2_TYPE		RAW_CAMERA
#define PIXTER_2_FORMAT		ATOMISP_INPUT_FORMAT_RAW_10
#define PIXTER_2_BAYER		atomisp_bayer_order_rggb
#define PIXTER_2_LANES		2
#define PIXTER_2_STREAMS	1


extern void *pixter_0_platform_data(void *info) __attribute__((weak));
extern void *pixter_1_platform_data(void *info) __attribute__((weak));
extern void *pixter_2_platform_data(void *info) __attribute__((weak));

#endif
