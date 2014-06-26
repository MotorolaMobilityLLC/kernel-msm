/*
 * platform_mt9d113.h: mt9d113 platform data header file
 *
 * (C) Copyright 2012 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_MT9D113_H_
#define _PLATFORM_MT9D113_H_

extern void *mt9d113_platform_data(void *info) __attribute__((weak));
extern void mt9d113_reset(struct v4l2_subdev *sd) __attribute__((weak));
#endif
