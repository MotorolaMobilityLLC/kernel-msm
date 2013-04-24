/*
 * Copyright (C) 2013, Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include "msm_sensor.h"
#include "msm.h"
#include "ov660_v4l2.h"
#include <linux/regulator/consumer.h>

#define SENSOR_NAME "ov10820"
#define PLATFORM_DRIVER_NAME "msm_camera_ov10820"

#define OV10820_DEFAULT_MCLK_RATE 24000000
#define OV10820_SECONDARY_I2C_ADDRESS 0x20

#define OV10820_OTP_ADDR 0x6000
#define OV10820_OTP_BANK_SIZE  0x10
#define OV10820_OTP_BANK_COUNT 96
#define OV10820_OTP_SIZE       (OV10820_OTP_BANK_COUNT * OV10820_OTP_BANK_SIZE)

static uint8_t ov10820_otp[OV10820_OTP_SIZE];
static struct otp_info_t ov10820_otp_info;
static uint8_t is_ov10820_otp_read;
static uint8_t is_vdd_pk_powered_up;

static struct regulator *cam_vdig;
static struct regulator *cam_afvdd;
static struct regulator *cam_dvdd;
static struct regulator *cam_pk_dvdd;

DEFINE_MUTEX(ov10820_mut);
static struct msm_sensor_ctrl_t ov10820_s_ctrl;
static bool ov660_exists;
static uint16_t revision;

/* Used for debugging purposes */
static int ov10820_ov660_rev_check;
module_param(ov10820_ov660_rev_check, int, 0644);

static struct msm_cam_clk_info cam_mot_8960_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_24HZ},
};

static struct msm_camera_i2c_reg_conf ov10820_start_settings[] = {
	{0x0100, 0x01},
};

static struct msm_camera_i2c_reg_conf ov10820_stop_settings[] = {
	{0x0100, 0x00},
};

static struct msm_camera_i2c_reg_conf ov10820_groupon_settings[] = {
	{0x3208, 0x00},
};

static struct msm_camera_i2c_reg_conf ov10820_groupoff_settings[] = {
	{0x3208, 0x10},
	{0x3208, 0xA0},
};

static struct msm_camera_i2c_reg_conf ov10820_full_24fps_settings_rev_a1[] = {
	{0x3092, 0x03},
	{0x3083, 0x00},
	{0x308e, 0x00},
	{0x3720, 0x60},
	{0x372a, 0x08},
	{0x370a, 0x21},
	{0x380a, 0x09},
	{0x380b, 0x80},
	{0x380e, 0x0a},
	{0x380f, 0x38},
	{0x3813, 0x10},
	{0x3815, 0x11},
	{0x3820, 0x00},
	{0x380c, 0x19},
	{0x380d, 0x64},
	{0x4001, 0x08},
	{0x4003, 0x30},
	{0x402a, 0x0c},
	{0x402b, 0x08},
	{0x402e, 0x1c},
	{0x4837, 0x10},
};

static struct msm_camera_i2c_reg_conf ov10820_full_24fps_settings_rev_b1[] = {
	{0x3082, 0x64},
	{0x3092, 0x06},
	{0x3033, 0x42},
	{0x3034, 0x68},
	{0x3035, 0x4f},
	{0x3036, 0x75},
	{0x3037, 0x4f},
	{0x3038, 0x75},
	{0x3641, 0xa3},
	{0x3700, 0x33},
	{0x3701, 0x14},
	{0x3702, 0x67},
	{0x3703, 0x44},
	{0x3704, 0x22},
	{0x3706, 0x11},
	{0x3707, 0x43},
	{0x3708, 0x53},
	{0x3709, 0x2d},
	{0x370a, 0x21},
	{0x370b, 0x24},
	{0x370c, 0x90},
	{0x370d, 0x02},
	{0x370e, 0x08},
	{0x370f, 0x27},
	{0x3710, 0x1b},
	{0x3714, 0x32},
	{0x3719, 0x0c},
	{0x3730, 0x02},
	{0x3731, 0x30},
	{0x3732, 0x02},
	{0x3733, 0x54},
	{0x3738, 0x02},
	{0x3739, 0x54},
	{0x373a, 0x02},
	{0x373b, 0x30},
	{0x3740, 0x02},
	{0x3741, 0x27},
	{0x3742, 0x02},
	{0x3743, 0x4d},
	{0x3803, 0x00},
	{0x3806, 0x09},
	{0x3807, 0x9f},
	{0x380a, 0x09},
	{0x380b, 0x80},
	{0x380c, 0x1a},
	{0x380d, 0xe0},
	{0x380e, 0x09},
	{0x380f, 0xe0},
	{0x3810, 0x00},
	{0x3811, 0x10},
	{0x3813, 0x10},
	{0x3815, 0x11},
	{0x3820, 0x00},
	{0x4001, 0x08},
	{0x4003, 0x30},
	{0x402a, 0x0c},
	{0x402b, 0x08},
	{0x402e, 0x1c},
	{0x4837, 0x14},
	{0x5b04, 0xa2},
};

static struct msm_camera_i2c_reg_conf ov10820_full_15fps_settings_rev_a1[] = {
	{0x3092, 0x07},
	{0x3083, 0x01},
	{0x308e, 0x02},
	{0x3720, 0x30},
	{0x372a, 0x00},
	{0x370a, 0x21},
	{0x380a, 0x09},
	{0x380b, 0x80},
	{0x380e, 0x0a},
	{0x380f, 0x38},
	{0x3813, 0x10},
	{0x3815, 0x11},
	{0x3820, 0x00},
	{0x380c, 0x14},
	{0x380d, 0x50},
	{0x4001, 0x08},
	{0x4003, 0x30},
	{0x402a, 0x0c},
	{0x402b, 0x08},
	{0x402e, 0x1c},
	{0x4837, 0x20},
};


static struct msm_camera_i2c_reg_conf ov10820_full_15fps_settings_rev_b1[] = {
	{0x3082, 0x7e},
	{0x3092, 0x06},
	{0x3083, 0x01},
	{0x308e, 0x02},
	{0x3033, 0x42},
	{0x3034, 0x68},
	{0x3035, 0x4f},
	{0x3036, 0x75},
	{0x3037, 0x4f},
	{0x3038, 0x75},
	{0x3641, 0xa3},
	{0x3700, 0x33},
	{0x3701, 0x14},
	{0x3702, 0x67},
	{0x3703, 0x44},
	{0x3704, 0x22},
	{0x3706, 0x11},
	{0x3707, 0x43},
	{0x3708, 0x53},
	{0x3709, 0x2d},
	{0x370a, 0x21},
	{0x370b, 0x24},
	{0x370c, 0x90},
	{0x370d, 0x02},
	{0x370e, 0x08},
	{0x370f, 0x27},
	{0x3710, 0x1b},
	{0x3714, 0x32},
	{0x3719, 0x0c},
	{0x3730, 0x02},
	{0x3731, 0x30},
	{0x3732, 0x02},
	{0x3733, 0x54},
	{0x3738, 0x02},
	{0x3739, 0x54},
	{0x373a, 0x02},
	{0x373b, 0x30},
	{0x3740, 0x02},
	{0x3741, 0x27},
	{0x3742, 0x02},
	{0x3743, 0x4d},
	{0x3803, 0x00},
	{0x3806, 0x09},
	{0x3807, 0x9f},
	{0x380a, 0x09},
	{0x380b, 0x80},
	{0x380c, 0x15},
	{0x380d, 0x80},
	{0x380e, 0x09},
	{0x380f, 0xe0},
	{0x3810, 0x00},
	{0x3811, 0x10},
	{0x3813, 0x10},
	{0x3815, 0x11},
	{0x3820, 0x00},
	{0x4001, 0x08},
	{0x4003, 0x30},
	{0x402a, 0x0c},
	{0x402b, 0x08},
	{0x402e, 0x1c},
	{0x4837, 0x20},
	{0x5b04, 0xa2},
};

static struct msm_camera_i2c_reg_conf ov10820_qtr_30fps_settings_rev_a1[] = {
	{0x3092, 0x03},
	{0x3083, 0x01},
	{0x308e, 0x02},
	{0x3720, 0x60},
	{0x372a, 0x08},
	{0x370a, 0x23},
	{0x380a, 0x04},
	{0x380b, 0xc0},
	{0x380e, 0x05},
	{0x380f, 0x1c},
	{0x3813, 0x08},
	{0x3815, 0x22},
	{0x3820, 0x02},
	{0x380c, 0x14},
	{0x380d, 0x50},
	{0x4001, 0x00},
	{0x4003, 0x1c},
	{0x402a, 0x0a},
	{0x402b, 0x06},
	{0x402e, 0x14},
	{0x4837, 0x10},
};

static struct msm_camera_i2c_reg_conf ov10820_qtr_60fps_settings_rev_a1[] = {
	{0x3092, 0x03},
	{0x3083, 0x00},
	{0x308e, 0x00},
	{0x3720, 0x60},
	{0x372a, 0x08},
	{0x370a, 0x23},
	{0x380a, 0x04},
	{0x380b, 0xc0},
	{0x380e, 0x05},
	{0x380f, 0x1c},
	{0x3813, 0x08},
	{0x3815, 0x22},
	{0x3820, 0x02},
	{0x380c, 0x14},
	{0x380d, 0x50},
	{0x4001, 0x00},
	{0x4003, 0x1c},
	{0x402a, 0x0a},
	{0x402b, 0x06},
	{0x402e, 0x14},
	{0x4837, 0x10},
};

static struct msm_camera_i2c_reg_conf ov10820_qtr_30fps_settings_rev_b1[] = {
	{0x3082, 0x6c},
	{0x3092, 0x03},
	{0x3033, 0x22},
	{0x3034, 0x88},
	{0x3035, 0x2f},
	{0x3036, 0x95},
	{0x3037, 0x2f},
	{0x3038, 0x95},
	{0x3641, 0x83},
	{0x3700, 0x44},
	{0x3701, 0x24},
	{0x3702, 0xb4},
	{0x3703, 0x78},
	{0x3704, 0x3c},
	{0x3706, 0x1e},
	{0x3707, 0x75},
	{0x3708, 0x84},
	{0x3709, 0x50},
	{0x370a, 0x23},
	{0x370b, 0x40},
	{0x370c, 0xe5},
	{0x370d, 0x03},
	{0x370e, 0x0f},
	{0x370f, 0x45},
	{0x3710, 0x30},
	{0x3714, 0x42},
	{0x3719, 0x00},
	{0x3730, 0x03},
	{0x3731, 0xd0},
	{0x3732, 0x04},
	{0x3733, 0x0e},
	{0x3738, 0x04},
	{0x3739, 0x0e},
	{0x373a, 0x03},
	{0x373b, 0xd0},
	{0x3740, 0x03},
	{0x3741, 0xc0},
	{0x3742, 0x04},
	{0x3743, 0x01},
	{0x3803, 0x08},
	{0x3806, 0x09},
	{0x3807, 0x97},
	{0x380a, 0x04},
	{0x380b, 0xc0},
	{0x380c, 0x2d},
	{0x380d, 0x00},
	{0x380e, 0x09},
	{0x380f, 0xe0},
	{0x3810, 0x00},
	{0x3811, 0x10},
	{0x3813, 0x04},
	{0x3815, 0x22},
	{0x3820, 0x02},
	{0x4001, 0x00},
	{0x4003, 0x1c},
	{0x402a, 0x0a},
	{0x402b, 0x06},
	{0x402e, 0x14},
	{0x4837, 0x14},
	{0x5b04, 0xf3},
};

static struct msm_camera_i2c_reg_conf ov10820_qtr_60fps_settings_rev_b1[] = {
	{0x3082, 0x6c},
	{0x3092, 0x03},
	{0x3033, 0x22},
	{0x3034, 0x88},
	{0x3035, 0x2f},
	{0x3036, 0x95},
	{0x3037, 0x2f},
	{0x3038, 0x95},
	{0x3641, 0x83},
	{0x3700, 0x44},
	{0x3701, 0x24},
	{0x3702, 0xb4},
	{0x3703, 0x78},
	{0x3704, 0x3c},
	{0x3706, 0x1e},
	{0x3707, 0x75},
	{0x3708, 0x84},
	{0x3709, 0x50},
	{0x370a, 0x23},
	{0x370b, 0x40},
	{0x370c, 0xe5},
	{0x370d, 0x03},
	{0x370e, 0x0f},
	{0x370f, 0x45},
	{0x3710, 0x30},
	{0x3714, 0x42},
	{0x3719, 0x00},
	{0x3730, 0x03},
	{0x3731, 0xd0},
	{0x3732, 0x04},
	{0x3733, 0x0e},
	{0x3738, 0x04},
	{0x3739, 0x0e},
	{0x373a, 0x03},
	{0x373b, 0xd0},
	{0x3740, 0x03},
	{0x3741, 0xc0},
	{0x3742, 0x04},
	{0x3743, 0x01},
	{0x3803, 0x08},
	{0x3806, 0x09},
	{0x3807, 0x97},
	{0x380a, 0x04},
	{0x380b, 0xc0},
	{0x380c, 0x15},
	{0x380d, 0x80},
	{0x380e, 0x04},
	{0x380f, 0xf0},
	{0x3810, 0x00},
	{0x3811, 0x10},
	{0x3813, 0x04},
	{0x3815, 0x22},
	{0x3820, 0x02},
	{0x4001, 0x00},
	{0x4003, 0x1c},
	{0x402a, 0x0a},
	{0x402b, 0x06},
	{0x402e, 0x14},
	{0x4837, 0x13},
	{0x5b04, 0xf3},
};

static struct msm_camera_i2c_reg_conf ov10820_reset_settings[] = {
	{0x0103, 0x01},
};

static struct msm_camera_i2c_reg_conf ov10820_BLC_work_around_settings[] = {
	{0x4000, 0x00},
	{0x4001, 0x00},
	{0x4004, 0x00},
	{0x4005, 0x00},
	{0x4006, 0x00},
	{0x4007, 0x00},
	{0x4008, 0x00},
	{0x3509, 0x0a},
};

static struct msm_camera_i2c_reg_conf ov10820_recommend_settings_rev_a1[] = {
	{0x3080, 0x04},
	{0x3082, 0x7e},
	{0x3083, 0x00},
	{0x3084, 0x03},
	{0x308b, 0x05},
	{0x308d, 0xaa},
	{0x308e, 0x00},
	{0x308f, 0x09},
	{0x3092, 0x03},
	{0x3002, 0x80},
	{0x3009, 0x06},
	{0x300f, 0x11},
	{0x3011, 0xe0},
	{0x3012, 0x41},
	{0x301e, 0x00},
	{0x3106, 0x04},
	{0x3610, 0x01},
	{0x3305, 0x41},
	{0x3306, 0x30},
	{0x3307, 0x00},
	{0x3308, 0x00},
	{0x3309, 0xc8},
	{0x330a, 0x01},
	{0x330b, 0x90},
	{0x330c, 0x02},
	{0x330d, 0x58},
	{0x330e, 0x03},
	{0x330f, 0x20},
	{0x3300, 0x00},
	{0x3503, 0x07},
	{0x3506, 0x00},
	{0x3507, 0x02},
	{0x3508, 0x00},
	{0x350a, 0x00},
	{0x3603, 0x04},
	{0x3604, 0x00},
	{0x3641, 0x8f},
	{0x3645, 0x41},
	{0x3648, 0x0d},
	{0x3649, 0x86},
	{0x3656, 0xf8},
	{0x3660, 0x83},
	{0x3663, 0x02},
	{0x3664, 0x00},
	{0x3708, 0x84},
	{0x3709, 0x50},
	{0x370b, 0xa0},
	{0x370c, 0xf0},
	{0x370d, 0x03},
	{0x370e, 0x0f},
	{0x370f, 0x48},
	{0x3710, 0x05},
	{0x3714, 0x42},
	{0x3718, 0x00},
	{0x371b, 0x30},
	{0x371e, 0x01},
	{0x371f, 0x20},
	{0x3720, 0x30},
	{0x3723, 0x38},
	{0x372a, 0x00},
	{0x372c, 0x11},
	{0x3730, 0x03},
	{0x3731, 0xa0},
	{0x3732, 0x03},
	{0x3733, 0xde},
	{0x3738, 0x03},
	{0x3739, 0xde},
	{0x373a, 0x03},
	{0x373b, 0xa0},
	{0x373c, 0x00},
	{0x373d, 0x00},
	{0x373e, 0x00},
	{0x373f, 0x00},
	{0x3740, 0x03},
	{0x3741, 0xc8},
	{0x3742, 0x03},
	{0x3743, 0xe1},
	{0x3748, 0x1e},
	{0x3749, 0x93},
	{0x374a, 0x17},
	{0x37c5, 0x00},
	{0x37c6, 0x00},
	{0x37c7, 0x08},
	{0x37c9, 0x00},
	{0x37ca, 0x06},
	{0x37cb, 0x00},
	{0x37cc, 0x00},
	{0x37cd, 0x44},
	{0x37ce, 0x1f},
	{0x37cf, 0x40},
	{0x37d0, 0x00},
	{0x37d1, 0x01},
	{0x37d2, 0x00},
	{0x37de, 0x00},
	{0x37df, 0x04},
	{0x3810, 0x00},
	{0x3812, 0x00},
	{0x3814, 0x11},
	{0x3817, 0x00},
	{0x3821, 0x06},
	{0x3826, 0x00},
	{0x3829, 0x00},
	{0x382b, 0x16},
	{0x382e, 0x04},
	{0x3830, 0x00},
	{0x3835, 0x00},
	{0x3900, 0x00},
	{0x3901, 0x00},
	{0x3902, 0xc6},
	{0x3b00, 0x00},
	{0x3b02, 0x00},
	{0x3b03, 0x00},
	{0x3b05, 0x00},
	{0x4000, 0x71},
	{0x4001, 0x08},
	{0x4003, 0x30},
	{0x4004, 0x00},
	{0x4005, 0x40},
	{0x4011, 0x00},
	{0x4012, 0x00},
	{0x4013, 0x00},
	{0x401f, 0x00},
	{0x4020, 0x01},
	{0x4021, 0x90},
	{0x4022, 0x03},
	{0x4023, 0x0f},
	{0x4024, 0x0e},
	{0x4025, 0x10},
	{0x4026, 0x0f},
	{0x4027, 0x9f},
	{0x4028, 0x00},
	{0x4029, 0x04},
	{0x402a, 0x0c},
	{0x402b, 0x08},
	{0x402c, 0x04},
	{0x402d, 0x04},
	{0x402e, 0x1c},
	{0x402f, 0x08},
	{0x4308, 0x00},
	{0x430b, 0xff},
	{0x430c, 0x00},
	{0x430d, 0xf0},
	{0x4501, 0x00},
	{0x4602, 0x02},
	{0x481b, 0x35},
	{0x4823, 0x35},
	{0x4d00, 0x04},
	{0x4d01, 0x71},
	{0x4d02, 0xfd},
	{0x4d03, 0xf5},
	{0x4d04, 0x0c},
	{0x4d05, 0xcc},
	{0x4d0a, 0x00},
	{0x4d0b, 0x01},
	{0x5001, 0x07},
	{0x5056, 0x08},
	{0x5057, 0x00},
	{0x5058, 0x08},
	{0x5059, 0x00},
	{0x505a, 0x08},
	{0x505b, 0x00},
	{0x505c, 0x08},
	{0x505d, 0x00},
	{0x5a04, 0x00},
	{0x5a05, 0x80},
	{0x5a06, 0x00},
	{0x5a07, 0xf0},
	{0x5a08, 0x00},
	{0x5e01, 0x41},
	{0x3106, 0x00},
	{0x370a, 0x21},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x10},
	{0x3805, 0xff},
	{0x3806, 0x09},
	{0x3807, 0x9f},
	{0x3808, 0x10},
	{0x3809, 0xe0},
	{0x380a, 0x09},
	{0x380b, 0x80},
	{0x380c, 0x14},
	{0x380d, 0x50},
	{0x380e, 0x0a},
	{0x380f, 0x38},
	{0x3811, 0x10},
	{0x3813, 0x10},
	{0x3815, 0x11},
	{0x3820, 0x00},
	{0x3834, 0x00},
	/* Exposure Gain */
	{0x3500, 0x00},
	{0x3501, 0xfd},
	{0x3502, 0xa0},
	{0x350b, 0x10},
	{0x3083, 0x01},
	{0x308e, 0x02},
	{0x3092, 0x07},
	{0x4837, 0x10},
};

static struct msm_camera_i2c_reg_conf ov10820_recommend_settings_rev_b1[] = {
	{0x3080, 0x04},
	{0x3082, 0x64},
	{0x3083, 0x00},
	{0x3084, 0x03},
	{0x308b, 0x05},
	{0x308d, 0xae},
	{0x308e, 0x00},
	{0x308f, 0x09},
	{0x3092, 0x06},
	{0x3002, 0x80},
	{0x3009, 0x06},
	{0x300d, 0x25},
	{0x300f, 0x11},
	{0x3011, 0xe0},
	{0x3012, 0x41},
	{0x301e, 0x00},
	{0x3031, 0x0c},
	{0x3032, 0x00},
	{0x3305, 0x41},
	{0x3306, 0x30},
	{0x3307, 0x00},
	{0x3308, 0x00},
	{0x3309, 0xc8},
	{0x330a, 0x01},
	{0x330b, 0x90},
	{0x330c, 0x02},
	{0x330d, 0x58},
	{0x330e, 0x03},
	{0x330f, 0x20},
	{0x3300, 0x00},
	{0x3503, 0x07},
	{0x3506, 0x00},
	{0x3507, 0x02},
	{0x3508, 0x00},
	{0x350a, 0x00},
	{0x3603, 0x04},
	{0x3604, 0x00},
	{0x3642, 0x33},
	{0x3643, 0x02},
	{0x3645, 0x41},
	{0x3646, 0x83},
	{0x3648, 0x0d},
	{0x3649, 0x86},
	{0x3656, 0x88},
	{0x3658, 0x86},
	{0x3659, 0x57},
	{0x365c, 0x5a},
	{0x3660, 0x83},
	{0x3663, 0x01},
	{0x3664, 0x00},
	{0x3705, 0x22},
	{0x3718, 0x00},
	{0x371b, 0x30},
	{0x371e, 0x01},
	{0x371f, 0x20},
	{0x3720, 0x60},
	{0x3721, 0xf0},
	{0x3723, 0x38},
	{0x372a, 0x08},
	{0x371c, 0x40},
	{0x372c, 0x11},
	{0x373c, 0x00},
	{0x373d, 0x00},
	{0x373e, 0x00},
	{0x373f, 0x00},
	{0x3748, 0x00},
	{0x3749, 0x00},
	{0x374a, 0x00},
	{0x374c, 0x48},
	{0x37c5, 0x00},
	{0x37c6, 0x00},
	{0x37c7, 0x08},
	{0x37c9, 0x00},
	{0x37ca, 0x06},
	{0x37cb, 0x00},
	{0x37cc, 0x00},
	{0x37cd, 0x44},
	{0x37ce, 0x1f},
	{0x37cf, 0x40},
	{0x37d0, 0x00},
	{0x37d1, 0x01},
	{0x37d2, 0x00},
	{0x37de, 0x00},
	{0x37df, 0x04},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3804, 0x10},
	{0x3805, 0xff},
	{0x3812, 0x00},
	{0x3814, 0x11},
	{0x3817, 0x00},
	{0x3821, 0x04},
	{0x3826, 0x00},
	{0x3829, 0x00},
	{0x382b, 0x16},
	{0x382e, 0x04},
	{0x3830, 0x00},
	{0x3835, 0x00},
	{0x3900, 0x00},
	{0x3901, 0x80},
	{0x3902, 0xc6},
	{0x3b00, 0x00},
	{0x3b02, 0x00},
	{0x3b03, 0x00},
	{0x3b05, 0x00},
	{0x4000, 0xf1},
	{0x4004, 0x00},
	{0x4005, 0x20},
	{0x4011, 0x00},
	{0x4012, 0x00},
	{0x4013, 0x00},
	{0x401f, 0x00},
	{0x4020, 0x01},
	{0x4021, 0x90},
	{0x4022, 0x03},
	{0x4023, 0x0f},
	{0x4024, 0x0e},
	{0x4025, 0x10},
	{0x4026, 0x0f},
	{0x4027, 0x9f},
	{0x4028, 0x00},
	{0x4029, 0x04},
	{0x402c, 0x04},
	{0x402d, 0x04},
	{0x402e, 0x1c},
	{0x402f, 0x08},
	{0x4308, 0x00},
	{0x430b, 0xff},
	{0x430c, 0x00},
	{0x430d, 0xf0},
	{0x4501, 0x00},
	{0x4602, 0x02},
	{0x4819, 0xa0},
	{0x481b, 0x35},
	{0x4823, 0x35},
	{0x4d00, 0x04},
	{0x4d01, 0x71},
	{0x4d02, 0xfd},
	{0x4d03, 0xf5},
	{0x4d04, 0x0c},
	{0x4d05, 0xcc},
	{0x4d0a, 0x00},
	{0x4d0b, 0x01},
	{0x5001, 0x05},
	{0x5002, 0x08},
	{0x5056, 0x04},
	{0x5057, 0x00},
	{0x5058, 0x04},
	{0x5059, 0x00},
	{0x505a, 0x04},
	{0x505b, 0x00},
	{0x505c, 0x04},
	{0x505d, 0x00},
	{0x5a04, 0x00},
	{0x5a05, 0x80},
	{0x5a06, 0x00},
	{0x5a07, 0xf0},
	{0x5a08, 0x00},
	{0x5b01, 0x10},
	{0x5b05, 0xef},
	{0x5b09, 0x02},
	{0x5e01, 0x41},
	{0x3106, 0x00},
	{0x370a, 0x21},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3806, 0x09},
	{0x3807, 0x9f},
	{0x3808, 0x10},
	{0x3809, 0xe0},
	{0x380a, 0x09},
	{0x380b, 0x80},
	{0x380c, 0x1a},
	{0x380d, 0xe0},
	{0x380e, 0x09},
	{0x380f, 0xe0},
	{0x3811, 0x10},
	{0x3813, 0x10},
	{0x3815, 0x11},
	{0x3820, 0x00},
	{0x3834, 0x00},
	{0x400d, 0x10},
	/* Exposure Gain */
	{0x3500, 0x00},
	{0x3501, 0xa1},
	{0x3502, 0x80},
	{0x350b, 0x40},
	{0x4837, 0x14},
	{0x5b04, 0xa2},
};

static struct v4l2_subdev_info ov10820_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array ov10820_init_conf_rev_a1[] = {
	{&ov10820_reset_settings[0],
		ARRAY_SIZE(ov10820_reset_settings), 50,
		MSM_CAMERA_I2C_BYTE_DATA},
	{&ov10820_recommend_settings_rev_a1[0],
		ARRAY_SIZE(ov10820_recommend_settings_rev_a1), 0,
		MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array ov10820_init_conf_rev_b1[] = {
	{&ov10820_reset_settings[0],
		ARRAY_SIZE(ov10820_reset_settings), 50,
		MSM_CAMERA_I2C_BYTE_DATA},
	{&ov10820_recommend_settings_rev_b1[0],
		ARRAY_SIZE(ov10820_recommend_settings_rev_b1), 0,
		MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array ov10820_confs_rev_a1[] = {
	{&ov10820_full_24fps_settings_rev_a1[0],
		ARRAY_SIZE(ov10820_full_24fps_settings_rev_a1), 0,
		MSM_CAMERA_I2C_BYTE_DATA},
	{&ov10820_qtr_30fps_settings_rev_a1[0],
		ARRAY_SIZE(ov10820_qtr_30fps_settings_rev_a1), 0,
		MSM_CAMERA_I2C_BYTE_DATA},
	{&ov10820_full_15fps_settings_rev_a1[0],
		ARRAY_SIZE(ov10820_full_15fps_settings_rev_a1), 0,
		MSM_CAMERA_I2C_BYTE_DATA},
	{&ov10820_qtr_60fps_settings_rev_a1[0],
		ARRAY_SIZE(ov10820_qtr_60fps_settings_rev_a1), 0,
		MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_camera_i2c_conf_array ov10820_confs_rev_b1[] = {
	{&ov10820_full_24fps_settings_rev_b1[0],
		ARRAY_SIZE(ov10820_full_24fps_settings_rev_b1), 0,
		MSM_CAMERA_I2C_BYTE_DATA},
	{&ov10820_qtr_30fps_settings_rev_b1[0],
		ARRAY_SIZE(ov10820_qtr_30fps_settings_rev_b1), 0,
		MSM_CAMERA_I2C_BYTE_DATA},
	{&ov10820_full_15fps_settings_rev_b1[0],
		ARRAY_SIZE(ov10820_full_15fps_settings_rev_b1), 0,
		MSM_CAMERA_I2C_BYTE_DATA},
	{&ov10820_qtr_60fps_settings_rev_b1[0],
		ARRAY_SIZE(ov10820_qtr_60fps_settings_rev_b1), 0,
		MSM_CAMERA_I2C_BYTE_DATA},
};

/* vt_pixel_clk==pll sys clk, op_pixel_clk==mipi clk */
static struct msm_sensor_output_info_t ov10820_r1a_dimensions[] = {
	{	/* 24 FPS Full */
		.x_output = 0x10E0,/*4320*/
		.y_output = 0x0980,/*2432*/
		.line_length_pclk = 0x13e8, /*5096*/
		.frame_length_lines = 0xa38,/*2616*/
		.vt_pixel_clk = 320000000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	{	/* 30 FPS QTR */
		.x_output = 0x870,/*2160*/
		.y_output = 0x4c0,/*1216*/
		.line_length_pclk = 0x1506,/*5382*/
		.frame_length_lines = 0x51c, /*1308*/
		.vt_pixel_clk = 211200000,
		.op_pixel_clk = 264000000,
		.binning_factor = 2,
	},
	{	/* 15 FPS FULL */
		.x_output = 0x10E0,/*4320*/
		.y_output = 0x0980,/*2432*/
		.line_length_pclk = 0x1450, /*5200*/
		.frame_length_lines = 0xa38,/*2616*/
		.vt_pixel_clk = 201600000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	{	/* 60 FPS QTR */
		.x_output = 0x870,/*2160*/
		.y_output = 0x4c0,/*1216*/
		.line_length_pclk = 0xa83,/*2691*/
		.frame_length_lines = 0x51c, /*1308*/
		.vt_pixel_clk = 211200000,
		.op_pixel_clk = 264000000,
		.binning_factor = 2,
	},
};

/* vt_pixel_clk==pll sys clk, op_pixel_clk==mipi clk */
static struct msm_sensor_output_info_t ov10820_r1b_dimensions[] = {
	{	/* 24 FPS Full */
		.x_output = 0x10E0,/*4320*/
		.y_output = 0x0980,/*2432*/
		.line_length_pclk = 0x149A, /*5274*/
		.frame_length_lines = 0x9e5, /*2528*/
		.vt_pixel_clk = 320000000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	{	/* 30 FPS QTR */
		.x_output = 0x870,/*2160*/
		.y_output = 0x4c0,/*1216*/
		.line_length_pclk = 0x20f6, /*8438*/
		.frame_length_lines = 0x4f0, /*1264*/
		.vt_pixel_clk = 320000000,
		.op_pixel_clk = 320000000,
		.binning_factor = 2,
	},
	{	/* 15 FPS Full */
		.x_output = 0x10E0,/*4320*/
		.y_output = 0x0980,/*2432*/
		.line_length_pclk = 0x14c4, /*5316*/
		.frame_length_lines = 0x9e0,/*2528*/
		.vt_pixel_clk = 201600000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	{	/* 60 FPS QTR */
		.x_output = 0x870,/*2160*/
		.y_output = 0x4c0,/*1216*/
		.line_length_pclk = 0x107b, /*4219*/
		.frame_length_lines = 0x4f0, /*1264*/
		.vt_pixel_clk = 320000000,
		.op_pixel_clk = 320000000,
		.binning_factor = 2,
	},
};


static struct msm_sensor_output_reg_addr_t ov10820_reg_addr = {
	.x_output = 0x3808,
	.y_output = 0x380a,
	.line_length_pclk = 0x380c,
	.frame_length_lines = 0x380e,
};

/* Use address 300A with word length id 8830 if OTP not programmed */
static struct msm_sensor_id_info_t ov10820_id_info = {
	.sensor_id_reg_addr = 0x300A,
	.sensor_id = 0xA8, /* This is for ov10820 */
};

static struct msm_sensor_exp_gain_info_t ov10820_exp_gain_info = {
	.coarse_int_time_addr = 0x3500,
	.global_gain_addr = 0x350A,
	.vert_offset = 6,
};

/* TBD: Need to revisit*/
static int32_t ov10820_write_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line, int32_t luma_avg, uint16_t fgain)
{
	uint32_t fl_lines, offset;
	uint8_t int_time[3];

	fl_lines =
		(s_ctrl->curr_frame_length_lines * s_ctrl->fps_divider) / Q10;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (line > (fl_lines - offset))
		fl_lines = line + offset;

	fl_lines += (fl_lines & 0x1);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				s_ctrl->sensor_output_reg_addr->
				frame_length_lines, fl_lines,
				MSM_CAMERA_I2C_WORD_DATA);

	int_time[0] = line >> 12;
	int_time[1] = line >> 4;
	int_time[2] = line << 4;

	if (allow_asic_control) {
		ov660_set_i2c_bypass(0);
		ov660_set_exposure_gain(gain, line);
		ov660_set_i2c_bypass(1);
	} else {
		s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
		msm_camera_i2c_write_seq(s_ctrl->sensor_i2c_client,
				s_ctrl->sensor_exp_gain_info->
				coarse_int_time_addr, &int_time[0], 3);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				s_ctrl->sensor_exp_gain_info->
				global_gain_addr, gain,
				MSM_CAMERA_I2C_WORD_DATA);
		s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	}

	return 0;
}

static int32_t ov10820_sensor_setting(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
	int32_t rc = 0;

	if (update_type == MSM_SENSOR_REG_INIT) {
		s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
		msm_sensor_write_init_settings(s_ctrl);
	} else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {
		if (ov660_exists) {
			rc = ov660_set_sensor_mode(res, revision);
			if (rc < 0)
				return rc;
		}
		rc = msm_sensor_write_conf_array(
				s_ctrl->sensor_i2c_client,
				s_ctrl->msm_sensor_reg->mode_settings, res);
		if (rc < 0)
			return rc;
		v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
			NOTIFY_PCLK_CHANGE, &s_ctrl->msm_sensor_reg->
			output_settings[res].op_pixel_clk);
	}
	return rc;
}

static int32_t ov10820_regulator_on(struct regulator **reg,
				struct device *dev, char *regname, int uV)
{
	int32_t rc;

	pr_debug("%s: %s %d\n", __func__, regname, uV);

	*reg = regulator_get(dev, regname);
	if (IS_ERR(*reg)) {
		pr_err("%s: failed to get %s (%d)\n",
				__func__, regname, rc = PTR_ERR(*reg));
		goto reg_on_done;
	}

	if (uV != 0) {
		rc = regulator_set_voltage(*reg, uV, uV);
		if (rc) {
			pr_err("%s: failed to set voltage for %s (%d)\n",
					__func__, regname, rc);
			goto reg_on_done;
		}
	}

	rc = regulator_enable(*reg);
	if (rc) {
		pr_err("%s: failed to enable %s (%d)\n",
				__func__, regname, rc);
		goto reg_on_done;
	}

reg_on_done:
	return rc;
}

static int32_t ov10820_regulator_off(struct regulator **reg, char *regname)
{
	int32_t rc;

	if (regname)
		pr_debug("%s: %s\n", __func__, regname);

	if (IS_ERR_OR_NULL(*reg)) {
		if (regname)
			pr_err("%s: %s is null, aborting\n", __func__,
								regname);
		rc = -EINVAL;
		goto reg_off_done;
	}

	rc = regulator_disable(*reg);
	if (rc) {
		if (regname)
			pr_err("%s: failed to disable %s (%d)\n", __func__,
								regname, rc);
		goto reg_off_done;
	}

	regulator_put(*reg);
	*reg = NULL;

reg_off_done:
	return rc;
}

static int32_t ov10820_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct device *dev = &s_ctrl->sensor_i2c_client->client->dev;
	struct msm_camera_sensor_info *info = s_ctrl->sensordata;

	if (!info->oem_data) {
		pr_err("%s: oem data NULL in sensor info, aborting",
				__func__);
		rc = -EINVAL;
		goto power_up_done;
	}

	pr_debug("%s: R: %d, P: %d D: %d DVDD %d ASIC rev %d PK VDD %d\n",
			__func__,
			info->sensor_reset,
			info->sensor_pwd,
			info->oem_data->sensor_dig_en,
			info->oem_data->sensor_using_separate_dvdd,
			info->oem_data->sensor_asic_revision,
			info->oem_data->sensor_using_new_pk_dvdd);

	/* Request gpios */
	rc = gpio_request(info->sensor_pwd, "ov10820");
	if (rc < 0) {
		pr_err("%s: gpio request sensor_pwd failed (%d)\n",
				__func__, rc);
		goto abort0;
	}

	rc = gpio_request(info->sensor_reset, "ov10820");
	if (rc < 0) {
		pr_err("%s: gpio request sensor_reset failed (%d)\n",
				__func__, rc);
		goto abort1;
	}

	rc = gpio_request(info->oem_data->sensor_dig_en, "ov10820");
	if (rc < 0) {
		pr_err("%s: gpio request sensor_dig_en failed (%d)\n",
				__func__, rc);
		goto abort2;
	}

	if (info->oem_data->sensor_using_new_pk_dvdd &
			(is_vdd_pk_powered_up == 0)) {
		rc = ov10820_regulator_on(&cam_pk_dvdd, dev,
				"cam_pk_dvdd", 1200000);
		if (rc < 0) {
			pr_err("%s: Unable to turn on cam_pk_dvdd (%d)\n",
					__func__, rc);
			goto abort2;
		} else {
			is_vdd_pk_powered_up = 1;
			usleep_range(1000, 2000);
		}
	}

	/* Set PWDN to low for power up */
	gpio_direction_output(info->sensor_pwd, 0);
	usleep(500);

	/* Enable AF supply */
	rc = ov10820_regulator_on(&cam_afvdd, dev, "cam_afvdd", 2800000);
	if (rc < 0) {
		pr_err("%s: cam_afvdd was unable to be turned on (%d)\n",
				__func__, rc);
		goto abort2;
	}

	/* Wait for core supplies to power up */
	usleep_range(1000, 2000);

	/* Need to enable DVDD */
	gpio_direction_output(info->oem_data->sensor_dig_en, 1);
	usleep(500);

	/* Set reset high */
	gpio_direction_output(info->sensor_reset, 0);
	usleep(500);

	/* Set PWDN to high for normal operation */
	gpio_direction_output(info->sensor_pwd, 1);
	usleep(500);

	if (info->oem_data->sensor_using_separate_dvdd) {
		rc = ov10820_regulator_on(&cam_dvdd, dev, "cam_dvdd", 0);
		if (rc < 0) {
			pr_err("%s: cam_dvdd was unable to be turned on (%d)\n",
					__func__, rc);
			goto abort2;
		}
	} else {
		rc = ov10820_regulator_on(&cam_vdig, dev, "cam_vdig", 1200000);
		if (rc < 0)
			goto abort2;

	}

	/* Wait for DVDD to rise */
	usleep_range(1000, 2000);

	/* Set reset high */
	gpio_direction_output(info->sensor_reset, 1);
	usleep(500);

	/*Enable MCLK*/
	cam_mot_8960_clk_info->clk_rate = s_ctrl->clk_rate;
	rc = msm_cam_clk_enable(dev, cam_mot_8960_clk_info,
			s_ctrl->cam_clk, ARRAY_SIZE(cam_mot_8960_clk_info), 1);
	if (rc < 0) {
		pr_err("ov10820: msm_cam_clk_enable failed (%d)\n", rc);
		goto abort2;
	}
	usleep(500);
	goto power_up_done;

	/* Cleanup, ignore errors during abort */
abort2:
	gpio_free(info->oem_data->sensor_dig_en);
abort1:
	gpio_free(info->sensor_reset);
abort0:
	gpio_free(info->sensor_pwd);

	if (info->oem_data->sensor_using_separate_dvdd)
		ov10820_regulator_off(&cam_dvdd, NULL);
	else
		ov10820_regulator_off(&cam_vdig, NULL);

	ov10820_regulator_off(&cam_afvdd, NULL);
power_up_done:
	return rc;
}

static int32_t ov10820_power_down(
		struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct device *dev = &s_ctrl->sensor_i2c_client->client->dev;
	struct msm_camera_sensor_info *info = s_ctrl->sensordata;

	pr_debug("%s\n", __func__);

	/*Disable MCLK*/
	rc = msm_cam_clk_enable(dev, cam_mot_8960_clk_info, s_ctrl->cam_clk,
			ARRAY_SIZE(cam_mot_8960_clk_info), 0);
	if (rc < 0)
		pr_err("ov10820: msm_cam_clk_enable failed (%d)\n", rc);
	usleep(500);

	/*Set Reset Low*/
	gpio_direction_output(info->sensor_reset, 0);
	usleep(500);

	if (info->oem_data->sensor_using_separate_dvdd)
		ov10820_regulator_off(&cam_dvdd, "cam_dvdd");
	else
		ov10820_regulator_off(&cam_vdig, "cam_vdig");

	usleep_range(500, 1000);

	/* Turn off AF regulator supply */
	ov10820_regulator_off(&cam_afvdd, NULL);

	/*Set pwd low, which also sets avdd low*/
	gpio_direction_output(info->sensor_pwd, 0);
	usleep(500);

	/*Set Dig En Low */
	gpio_direction_output(info->oem_data->sensor_dig_en, 0);
	usleep(500);

	/*Clean up*/
	gpio_free(info->sensor_pwd);
	gpio_free(info->sensor_reset);
	gpio_free(info->oem_data->sensor_dig_en);
	return rc;
}

static const struct i2c_device_id ov10820_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&ov10820_s_ctrl},
	{ }
};

static struct i2c_driver ov10820_i2c_driver = {
	.id_table = ov10820_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ov10820_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int32_t ov10820_read_otp(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;

	if (is_ov10820_otp_read == 1)
		return rc;


	/* Start Stream to read OTP Data */
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			0x0100, 0x01, MSM_CAMERA_I2C_BYTE_DATA);

	if (rc < 0) {
		pr_err("%s: Unable to read otp\n", __func__);
		return rc;
	}

	usleep_range(1000, 2000);

	rc = msm_camera_i2c_read_seq(s_ctrl->sensor_i2c_client,
			OV10820_OTP_ADDR, (uint8_t *)ov10820_otp,
			OV10820_OTP_SIZE);
	if (rc < 0) {
		pr_err("%s: Unable to read i2c seq!\n", __func__);
		goto exit;
	}

	is_ov10820_otp_read = 1;

exit:
	/* Stop Streaming */
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			0x0100, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0)
		pr_err("%s: Unable to stop streaming of imager\n", __func__);

	return rc;
}

static int32_t ov10820_get_module_info(struct msm_sensor_ctrl_t *s_ctrl)
{
	if (ov10820_otp_info.size > 0) {
		s_ctrl->sensor_otp.otp_info = ov10820_otp_info.otp_info;
		s_ctrl->sensor_otp.size = ov10820_otp_info.size;
		s_ctrl->sensor_otp.hw_rev = revision;
		s_ctrl->sensor_otp.asic_rev = ov10820_otp_info.asic_rev;
		return 0;
	} else {
		pr_err("%s: Unable to get module info as otp failed!\n",
				__func__);
		return -EINVAL;
	}
}

static int32_t ov10820_check_hw_rev(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;

	ov660_set_i2c_bypass(1);

	rc = msm_camera_i2c_read(
			s_ctrl->sensor_i2c_client,
			0x302A, &revision, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s: unable to read revision (%d)\n", __func__, rc);
	} else {
		pr_info("%s: revision is 0x%x\n", __func__, revision);
		if (revision == 0xB1) {
			pr_info("%s: new revision, using no BLC fix\n",
					__func__);
			allow_asic_control = false;
			return 0;
		}
	}

	return rc;
}

static int32_t ov10820_check_i2c_configuration(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;


	ov660_set_i2c_bypass(0);

	rc = msm_camera_i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_id_info->sensor_id_reg_addr, &chipid,
			MSM_CAMERA_I2C_BYTE_DATA);

	if (rc < 0) {
		pr_info("%s: I2C is configured through ASIC.\n", __func__);
		allow_asic_control = true;

		rc = ov660_add_blc_firmware(s_ctrl->sensor_i2c_addr);
		if (rc < 0)
			pr_err("%s: Unable to set blc firmware!\n", __func__);

	} else {
		pr_info("%s: I2C is configured around ASIC.\n", __func__);
		allow_asic_control = false;
		rc = ov660_use_work_around_blc();

		if (rc < 0)
			pr_err("%s: unable to set ov660 blc workaround!\n",
					__func__);

		rc = msm_camera_i2c_write_tbl(s_ctrl->sensor_i2c_client,
				ov10820_BLC_work_around_settings,
				ARRAY_SIZE(ov10820_BLC_work_around_settings),
				MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0)
			pr_err("%s: unable to write 10MP blc workaround!\n",
					__func__);
	}

	ov660_set_i2c_bypass(1);
	return 0;
}

static void ov10820_init_confs(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_sensor_reg_t *r = s_ctrl->msm_sensor_reg;
	if (revision == 0xB1) {
		r->init_settings = &ov10820_init_conf_rev_b1[0];
		r->init_size     = ARRAY_SIZE(ov10820_init_conf_rev_b1);
		r->mode_settings = &ov10820_confs_rev_b1[0];
		r->num_conf      = ARRAY_SIZE(ov10820_confs_rev_b1);
		r->output_settings = &ov10820_r1b_dimensions[0];
	} else {
		r->init_settings = &ov10820_init_conf_rev_a1[0];
		r->init_size     = ARRAY_SIZE(ov10820_init_conf_rev_a1);
		r->mode_settings = &ov10820_confs_rev_a1[0];
		r->num_conf      = ARRAY_SIZE(ov10820_confs_rev_a1);
		r->output_settings = &ov10820_r1a_dimensions[0];
	}
}

static int32_t ov10820_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;
	struct msm_camera_sensor_info *info = s_ctrl->sensordata;

	if (!info->oem_data) {
		pr_err("%s: oem data NULL in sensor info, aborting",
				__func__);
		return -EINVAL;
	}

	if (ov660_check_probe() >= 0)
		ov660_exists = true;
	if (ov660_exists) {
		ov660_set_i2c_bypass(1);
		if (ov10820_ov660_rev_check == 0x1) {
			ov660_read_revision();
			/* Reset Flag automatically */
			ov10820_ov660_rev_check = 0x0;
			return -EINVAL;
		}
	}

	/* TODO Need to understand if better way to read chip id is
	 * by the OTP or by normal method per below */

	/*Read sensor id*/
	rc = msm_camera_i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_id_info->sensor_id_reg_addr, &chipid,
			MSM_CAMERA_I2C_BYTE_DATA);
	if (rc >= 0)
		goto check_chipid;

	/* Need to re-assign i2c address to secondary address of the
	 * sensor to see if the part exists under the second address.
	 */
	s_ctrl->sensor_i2c_addr = OV10820_SECONDARY_I2C_ADDRESS;
	s_ctrl->sensor_i2c_client->client->addr = OV10820_SECONDARY_I2C_ADDRESS;
	rc = msm_camera_i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_id_info->sensor_id_reg_addr, &chipid,
			MSM_CAMERA_I2C_BYTE_DATA);

	if (rc < 0) {
		pr_err("%s: read id failed\n", __func__);
		return rc;
	}

check_chipid:
	if (chipid != s_ctrl->sensor_id_info->sensor_id) {
		pr_err("%s: chip id %x does not match expected %x\n", __func__,
				chipid, s_ctrl->sensor_id_info->sensor_id);
		return -ENODEV;
	}

	ov10820_check_hw_rev(s_ctrl);

	if (ov660_exists) {
		ov660_initialize_10MP(revision);
		usleep(10000);
	}

	/* Need to determine when to apply BLC firmware fix
	 * or when to use the old method of blc work around fix */
	if (revision <= 0xB0)
		ov10820_check_i2c_configuration(s_ctrl);

	pr_debug("%s: success and using i2c address of: %x\n", __func__,
			s_ctrl->sensor_i2c_client->client->addr);

	ov10820_init_confs(s_ctrl);

	if ((!ov660_exists) &&
			(info->oem_data->sensor_allow_asic_bypass != 1)) {
		pr_err("%s: detected 10mp, but asic not working!\n",
				__func__);
		return -ENODEV;
	}

	ov660_set_i2c_bypass(1);
	rc = ov10820_read_otp(&ov10820_s_ctrl);
	if (rc < 0) {
		pr_err("%s: unable to read otp data!\n", __func__);
		ov10820_otp_info.size = 0;
	} else {
		ov10820_otp_info.otp_info = (uint8_t *)ov10820_otp;
		ov10820_otp_info.size = OV10820_OTP_SIZE;
	}
	ov10820_otp_info.asic_rev = info->oem_data->sensor_asic_revision;

	return 0;
}

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&ov10820_i2c_driver);
}


static struct v4l2_subdev_core_ops ov10820_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops ov10820_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops ov10820_subdev_ops = {
	.core = &ov10820_subdev_core_ops,
	.video = &ov10820_subdev_video_ops,
};

static struct msm_sensor_fn_t ov10820_func_tbl = {
	.sensor_start_stream            = msm_sensor_start_stream,
	.sensor_stop_stream             = msm_sensor_stop_stream,
	.sensor_group_hold_on           = msm_sensor_group_hold_on,
	.sensor_group_hold_off          = msm_sensor_group_hold_off,
	.sensor_set_fps                 = msm_sensor_set_fps,
	.sensor_write_exp_gain          = ov10820_write_exp_gain,
	.sensor_write_snapshot_exp_gain = ov10820_write_exp_gain,
	.sensor_setting                 = ov10820_sensor_setting,
	.sensor_set_sensor_mode         = msm_sensor_set_sensor_mode,
	.sensor_mode_init               = msm_sensor_mode_init,
	.sensor_get_output_info         = msm_sensor_get_output_info,
	.sensor_config                  = msm_sensor_config,
	.sensor_power_up                = ov10820_power_up,
	.sensor_power_down              = ov10820_power_down,
	.sensor_match_id                = ov10820_match_id,
	.sensor_get_csi_params          = msm_sensor_get_csi_params,
	.sensor_get_module_info         = ov10820_get_module_info,
};

static struct msm_sensor_reg_t ov10820_regs = {
	.default_data_type        = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf        = ov10820_start_settings,
	.start_stream_conf_size   = ARRAY_SIZE(ov10820_start_settings),
	.stop_stream_conf         = ov10820_stop_settings,
	.stop_stream_conf_size    = ARRAY_SIZE(ov10820_stop_settings),
	.group_hold_on_conf       = ov10820_groupon_settings,
	.group_hold_on_conf_size  = ARRAY_SIZE(ov10820_groupon_settings),
	.group_hold_off_conf      = ov10820_groupoff_settings,
	.group_hold_off_conf_size = ARRAY_SIZE(ov10820_groupoff_settings),
	.init_settings            = NULL, /*populated by ov10820_init_confs()*/
	.init_size                = 0,    /*populated by ov10820_init_confs()*/
	.mode_settings            = NULL, /*populated by ov10820_init_confs()*/
	.num_conf                 = 0,    /*populated by ov10820_init_confs()*/
	.output_settings          = NULL, /*populated by ov10820_init_confs()*/
};

static struct msm_sensor_ctrl_t ov10820_s_ctrl = {
	.msm_sensor_reg               = &ov10820_regs,
	.sensor_i2c_client            = &ov10820_sensor_i2c_client,
	.sensor_i2c_addr              = 0x6C,
	.sensor_output_reg_addr       = &ov10820_reg_addr,
	.sensor_id_info               = &ov10820_id_info,
	.sensor_exp_gain_info         = &ov10820_exp_gain_info,
	.cam_mode                     = MSM_SENSOR_MODE_INVALID,
	.msm_sensor_mutex             = &ov10820_mut,
	.sensor_i2c_driver            = &ov10820_i2c_driver,
	.sensor_v4l2_subdev_info      = ov10820_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ov10820_subdev_info),
	.sensor_v4l2_subdev_ops       = &ov10820_subdev_ops,
	.func_tbl                     = &ov10820_func_tbl,
	.clk_rate                     = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Omnivision 10820 RGBC sensor driver");
MODULE_LICENSE("GPL v2");

