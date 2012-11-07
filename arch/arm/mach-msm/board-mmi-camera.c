/* Copyright (c) 2012, Motorola Mobility, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <asm/mach-types.h>
#include <linux/gpio.h>
#include <mach/gpiomux.h>
#include <mach/camera.h>
#include <linux/leds-lm3556.h>

#include "board-8960.h"
#include "board-mmi.h"

#ifdef CONFIG_MSM_CAMERA

static struct msm_camera_csi_lane_params ov8835_csi_lane_params = {
	.csi_lane_assign = 0xE4,
	.csi_lane_mask = 0xF,
};

struct msm_camera_sensor_platform_info sensor_board_info_ov8835 = {
	.mount_angle = 90,
	.csi_lane_params = &ov8835_csi_lane_params,
};

static struct msm_camera_sensor_flash_data flash_ov8835 = {
	.flash_type = MSM_CAMERA_FLASH_NONE
};

struct msm_camera_sensor_info msm_camera_sensor_ov8835_data = {
	.sensor_name = "ov8835",
	.pdata = &msm_camera_csi_device_data[0],
	.flash_data = &flash_ov8835,
	.sensor_platform_info = &sensor_board_info_ov8835,
	.csi_if = 1,
	.camera_type = BACK_CAMERA_2D,
	.sensor_type = BAYER_SENSOR,
};

#endif
