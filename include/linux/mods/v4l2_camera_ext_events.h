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

#ifndef __V4L2_CAMERA_EXT_EVENTS_H
#define __V4L2_CAMERA_EXT_EVENTS_H

#include <linux/videodev2.h>
#include "v4l2_camera_ext_defs.h"

#define V4L2_CAMERA_EXT_EVENT_TYPE (V4L2_EVENT_PRIVATE_START + 0x00001000)

/* Event object is passed to user space in v4l2_event data field which is
 * char[64]. So the object's size should not exceed 64 bytes.
 */

struct v4l2_camera_ext_event {
	int type; /* CAMERA_EXT_REPORT_XXX */
	union {
		uint32_t error_code;
	};
};

#endif /* __V4L2_CAMERA_EXT_EVENTS_H */
