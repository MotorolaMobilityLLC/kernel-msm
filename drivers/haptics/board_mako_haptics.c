/*
** =============================================================================
** Copyright (c) 2014  Texas Instruments Inc.
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
** File:
**     board-mako-haptics.c
**
** Description:
**     platform data for Haptics devices
**
** =============================================================================
*/

#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#if defined(CONFIG_HAPTICS_DRV2605)
#include <linux/haptics/drv2605.h>
#elif defined(CONFIG_HAPTICS_DRV2604)
#include <linux/haptics/drv2604.h>
#elif defined(CONFIG_HAPTICS_DRV2604L)
#include <linux/haptics/drv2604l.h>
#elif defined(CONFIG_HAPTICS_DRV2667)
#include <linux/haptics/drv2667.h>
#endif

#define APQ8064_GSBI3_QUP_I2C_BUS_ID            7

#if defined(CONFIG_HAPTICS_DRV2605)
static struct drv2605_platform_data  drv2605_plat_data = {
        .GpioEnable = 60,                       //enable the chip
        .GpioTrigger = 59,                      //external trigger pin, (0: internal trigger)
#if defined(CONFIG_HAPTICS_LRA_SEMCO1030)
	//rated = 1.5Vrms, ov=2.1Vrms, f=204hz
	.loop = CLOSE_LOOP,
	.RTPFormat = Signed,
	.BIDIRInput = BiDirectional,
	.actuator = {
		.device_type = LRA,
		.rated_vol = 0x3d,
		.g_effect_bank = LIBRARY_F,
		.over_drive_vol = 0x87,
		.LRAFreq = 204,
	},
	.a2h = {
		.a2h_min_input = AUDIO_HAPTICS_MIN_INPUT_VOLTAGE,
		.a2h_max_input = AUDIO_HAPTICS_MAX_INPUT_VOLTAGE,
		.a2h_min_output = AUDIO_HAPTICS_MIN_OUTPUT_VOLTAGE,
		.a2h_max_output = AUDIO_HAPTICS_MAX_OUTPUT_VOLTAGE,		
	},
#elif defined(CONFIG_HAPTICS_ERM_EVOWAVE_Z4TJGB1512658)
	//rated vol = 3.0 v, ov = 3.6 v, risetime = 150 ms
	.loop = CLOSE_LOOP,
	.RTPFormat = Signed,
	.BIDIRInput = BiDirectional,
	.actuator = {
		.device_type = ERM,
		.g_effect_bank = LIBRARY_E,
		.rated_vol = 0x8d,
		.over_drive_vol = 0xa9,
	},
	.a2h = {
		.a2h_min_input = AUDIO_HAPTICS_MIN_INPUT_VOLTAGE,
		.a2h_max_input = AUDIO_HAPTICS_MAX_INPUT_VOLTAGE,
		.a2h_min_output = AUDIO_HAPTICS_MIN_OUTPUT_VOLTAGE,
		.a2h_max_output = AUDIO_HAPTICS_MAX_OUTPUT_VOLTAGE,		
	},
#elif defined(CONFIG_HAPTICS_ERM_HW)
    .loop = CLOSE_LOOP,
    .RTPFormat = Signed,
    .BIDIRInput = BiDirectional,
    .actuator = {
        .device_type = ERM,
        .g_effect_bank = LIBRARY_D,
        .rated_vol = 0x7e,
        .over_drive_vol = 0xa0,
    },
    .a2h = {
        .a2h_min_input = AUDIO_HAPTICS_MIN_INPUT_VOLTAGE,
        .a2h_max_input = AUDIO_HAPTICS_MAX_INPUT_VOLTAGE,
        .a2h_min_output = AUDIO_HAPTICS_MIN_OUTPUT_VOLTAGE,
        .a2h_max_output = AUDIO_HAPTICS_MAX_OUTPUT_VOLTAGE,                             
    },
#else
#error "please define actuator type"
#endif	
};
#elif defined(CONFIG_HAPTICS_DRV2604)
static struct drv2604_platform_data  drv2604_plat_data = {
	.GpioEnable = 0,			//enable the chip
	.GpioTrigger = 0,			//external trigger pin, (0: internal trigger)
#if defined(CONFIG_HAPTICS_LRA_SEMCO1030)
	//rated = 1.5Vrms, ov=2.1Vrms, f=204hz
	.loop = CLOSE_LOOP,
	.RTPFormat = Signed,
	.BIDIRInput = BiDirectional,
	.actuator = {
		.device_type = LRA,
		.rated_vol = 0x3d,
		.over_drive_vol = 0x87,
		.LRAFreq = 204,
	},
#elif defined(CONFIG_HAPTICS_ERM_EVOWAVE_Z4TJGB1512658)
	//rated vol = 3.0 v, ov = 3.6 v, risetime = 150 ms
	.loop = CLOSE_LOOP,
	.RTPFormat = Signed,
	.BIDIRInput = BiDirectional,	
	.actuator = {
		.device_type = ERM,
		.rated_vol = 0x8d,
		.over_drive_vol = 0xa9,
	},
#else
#error "please define actuator type"
#endif	
};
#elif defined(CONFIG_HAPTICS_DRV2604L)
static struct DRV2604L_platform_data  drv2604l_plat_data = {
	.GpioEnable = 0,			//enable the chip
	.GpioTrigger = 0,			//external trigger pin, (0: internal trigger)
#if defined(CONFIG_HAPTICS_LRA_SEMCO1030)
	//rated = 1.5Vrms, ov=2.1Vrms, f=204hz
	.loop = CLOSE_LOOP,
	.RTPFormat = Signed,
	.BIDIRInput = BiDirectional,
	.actuator = {
		.device_type = LRA,
		.rated_vol = 0x3d,
		.over_drive_vol = 0x87,
		.LRAFreq = 204,
	},
#elif defined(CONFIG_HAPTICS_ERM_EVOWAVE_Z4TJGB1512658)
	//rated vol = 3.0 v, ov = 3.6 v, risetime = 150 ms
	.loop = CLOSE_LOOP,
	.RTPFormat = Signed,
	.BIDIRInput = BiDirectional,	
	.actuator = {
		.device_type = ERM,		
		.rated_vol = 0x8d,
		.over_drive_vol = 0xa9,
	},
#else
#error "please define actuator type"
#endif	
};
#elif defined(CONFIG_HAPTICS_DRV2667)
static struct drv2667_platform_data  drv2667_plat_data = {
	.inputGain = DRV2667_GAIN_50VPP,	
	.boostTimeout = DRV2667_IDLE_TIMEOUT_20MS,
};
#endif
 
static struct i2c_board_info haptics_dev[] __initdata = {
#if defined(CONFIG_HAPTICS_DRV2605)
	{
		I2C_BOARD_INFO(HAPTICS_DEVICE_NAME, 0x5a),
		.platform_data = &drv2605_plat_data,
	},
#elif defined(CONFIG_HAPTICS_DRV2604)
	{
		I2C_BOARD_INFO(HAPTICS_DEVICE_NAME, 0x5a),
		.platform_data = &drv2604_plat_data,
	},	
#elif defined(CONFIG_HAPTICS_DRV2604L)
	{
		I2C_BOARD_INFO(HAPTICS_DEVICE_NAME, 0x5a),
		.platform_data = &drv2604l_plat_data,
	},	
#elif defined(CONFIG_HAPTICS_DRV2667)	
	{
		I2C_BOARD_INFO(HAPTICS_DEVICE_NAME, 0x59),
		.platform_data = &drv2667_plat_data,
	},
#endif
 };

static void __init lge_add_i2c_haptics_device(void)
{
	i2c_register_board_info(APQ8064_GSBI3_QUP_I2C_BUS_ID,
			       haptics_dev,
			       ARRAY_SIZE(haptics_dev));
}

static int __init lge_add_haptics_device(void)
{
	lge_add_i2c_haptics_device();
    return 0;
}
postcore_initcall(lge_add_haptics_device);
