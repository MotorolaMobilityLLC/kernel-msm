/*
 * Copyright (c) 2016 Motorola Mobility, LLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __CAMERA_EXT_DEFS_H__
#define __CAMERA_EXT_DEFS_H__

#define CAMERA_EXT_STREAM_CAP_PREVIEW   (1 << 0)
#define CAMERA_EXT_STREAM_CAP_VIDEO     (1 << 1)
#define CAMERA_EXT_STREAM_CAP_SNAPSHOT  (1 << 2)

/* Async message sent from MOD to AP */
#define CAMERA_EXT_REPORT_ERROR		0x00
#define CAMERA_EXT_REPORT_METADATA	0x01

/* error code for CAMERA_EXT_REPORT_ERROR */
#define CAMERA_EXT_ERROR_FATAL		0x00
#define CAMERA_EXT_ERROR_POWER_ON	0x01
#define CAMERA_EXT_ERROR_STREAM_ON	0x02
#define CAMERA_EXT_ERROR_CSI_RESET	0x03

#endif
