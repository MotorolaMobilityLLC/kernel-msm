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

struct lm3556_platform_data cam_flash_3556 = {
	.flags = (LM3556_TORCH | LM3556_FLASH |
			LM3556_ERROR_CHECK),
	.si_rev_filter_time_def = 0x00,
	.ivfm_reg_def = 0x80,
	.ntc_reg_def = 0x12,
	.ind_ramp_time_reg_def = 0x00,
	.ind_blink_reg_def = 0x00,
	.ind_period_cnt_reg_def = 0x00,
	.torch_ramp_time_reg_def = 0x00,
	.config_reg_def = 0x7f,
	.flash_features_reg_def = 0xd2,
	.current_cntrl_reg_def = 0x2b,
	.torch_brightness_def = 0x10,
	.enable_reg_def = 0x40,
	.flag_reg_def = 0x00,
	.torch_enable_val = 0x42,
	.flash_enable_val = 0x43,
	.hw_enable = 0,
};

static struct msm_camera_sensor_flash_src flash_src_lm3556 = {
	.flash_sr_type  = MSM_CAMERA_FLASH_SRC_LED,
	._fsrc  = {
		.led_src = {
			.led_name = LM3556_NAME,
			.led_name_len = LM3556_NAME_LEN,
		},
	},
};

struct msm_camera_sensor_flash_data camera_flash_lm3556 = {
	.flash_type = MSM_CAMERA_FLASH_LED,
	.flash_src = &flash_src_lm3556,
};

static struct msm_camera_sensor_flash_data camera_flash_none = {
	.flash_type = MSM_CAMERA_FLASH_NONE
};

static struct msm_gpiomux_config sasquatch_cam_2d_configs[] = {
};

static struct gpio sasquatch_common_cam_gpio[] = {
};

static struct gpio sasquatch_front_cam_gpio[] = {
	{76, GPIOF_DIR_OUT, "CAM_RESET"},
	{82, GPIOF_DIR_OUT, "CAM_PWRDN"},
	{89, GPIOF_DIR_OUT, "CAM_DIGEN"},
};

static struct msm_gpio_set_tbl sasquatch_front_cam_gpio_set_tbl[] = {
	{76, GPIOF_OUT_INIT_LOW, 1000},
	{89, GPIOF_OUT_INIT_HIGH, 1000},
	{82, GPIOF_OUT_INIT_HIGH, 1000},
	{76, GPIOF_OUT_INIT_HIGH, 4000},
};

static struct msm_camera_gpio_conf sasquatch_front_cam_gpio_conf = {
	.cam_gpiomux_conf_tbl = sasquatch_cam_2d_configs,
	.cam_gpiomux_conf_tbl_size = ARRAY_SIZE(sasquatch_cam_2d_configs),
	.cam_gpio_common_tbl = sasquatch_common_cam_gpio,
	.cam_gpio_common_tbl_size = ARRAY_SIZE(sasquatch_common_cam_gpio),
	.cam_gpio_req_tbl = sasquatch_front_cam_gpio,
	.cam_gpio_req_tbl_size = ARRAY_SIZE(sasquatch_front_cam_gpio),
	.cam_gpio_set_tbl = sasquatch_front_cam_gpio_set_tbl,
	.cam_gpio_set_tbl_size = ARRAY_SIZE(sasquatch_front_cam_gpio_set_tbl),
};

static struct msm_camera_csi_lane_params s5k5b3g_csi_lane_params = {
	.csi_lane_assign = 0xE4,
	.csi_lane_mask = 0x1,
};

static struct camera_vreg_t s5k5b3g_cam_vreg[] = {
	{"cam_vdig", REG_LDO, 1200000, 1200000, 105000},
	{"cam_vio", REG_VS, 0, 0, 0},
	{"cam_mipi_mux", REG_LDO, 2800000, 2800000, 0},
};

static struct msm_camera_sensor_platform_info sensor_board_info_s5k5b3g = {
	.mount_angle = 90,
	.cam_vreg = s5k5b3g_cam_vreg,
	.num_vreg = ARRAY_SIZE(s5k5b3g_cam_vreg),
	.gpio_conf = &sasquatch_front_cam_gpio_conf,
	.csi_lane_params = &s5k5b3g_csi_lane_params,
};

struct msm_camera_sensor_info msm_camera_sensor_s5k5b3g_data = {
	.sensor_name = "s5k5b3g",
	.pdata = &msm_camera_csi_device_data[1],
	.flash_data = &camera_flash_none,
	.sensor_platform_info = &sensor_board_info_s5k5b3g,
	.csi_if = 1,
	.camera_type = FRONT_CAMERA_2D,
	.sensor_type = BAYER_SENSOR,
};


static struct msm_camera_csi_lane_params ov8835_csi_lane_params = {
	.csi_lane_assign = 0xE4,
	.csi_lane_mask = 0xF,
};

struct msm_camera_sensor_platform_info sensor_board_info_ov8835 = {
	.mount_angle = 90,
	.csi_lane_params = &ov8835_csi_lane_params,
};

struct msm_camera_sensor_info msm_camera_sensor_ov8835_data = {
	.sensor_name = "ov8835",
	.pdata = &msm_camera_csi_device_data[0],
	.flash_data = &camera_flash_none,
	.sensor_platform_info = &sensor_board_info_ov8835,
	.csi_if = 1,
	.camera_type = BACK_CAMERA_2D,
	.sensor_type = BAYER_SENSOR,
};


static struct msm_camera_csi_lane_params ar0834_csi_lane_params = {
	.csi_lane_assign = 0xE4,
	.csi_lane_mask = 0xF,
};

struct msm_camera_sensor_platform_info sensor_board_info_ar0834 = {
	.mount_angle = 90,
	.csi_lane_params = &ar0834_csi_lane_params,
};

static struct msm_camera_sensor_flash_data flash_ar0834 = {
	.flash_type = MSM_CAMERA_FLASH_NONE
};

struct msm_camera_sensor_info msm_camera_sensor_ar0834_data = {
	.sensor_name = "ar0834",
	.pdata = &msm_camera_csi_device_data[0],
	.flash_data = &flash_ar0834,
	.sensor_platform_info = &sensor_board_info_ar0834,
	.csi_if = 1,
	.camera_type = BACK_CAMERA_2D,
	.sensor_type = BAYER_SENSOR,
};


#endif
