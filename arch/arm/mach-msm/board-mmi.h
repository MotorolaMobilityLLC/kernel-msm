/* Copyright (c) 2012, Motorola Mobility. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_BOARD_MMI_8960_H
#define __ARCH_ARM_MACH_MSM_BOARD_MMI_8960_H
#include <mach/board.h>
#include "board-8960.h"
#include "clock-local.h"

/* MMI OEM Data Structure */
struct mmi_oem_data {
	int (*is_factory)(void);
	int (*is_meter_locked)(void);
	int (*is_no_eprom)(void);
	int (*is_bareboard)(void);
	bool mmi_camera;
};

/* from board-mmi-gsbi.c */
extern void mmi_init_gsbi_devices_from_dt(void);

/* from board-mmi-i2c.c */
extern void mmi_register_i2c_devices_from_dt(void);

/* from board-mmi-pmic.c */
extern void mmi_init_pm8921_gpio_mpp(void);
extern void mmi_pm8921_init(struct mmi_oem_data *, void *);
extern void mmi_load_rgb_leds_from_dt(void);

/* from board-mmi-keypad.c */
extern void mmi_pm8921_keypad_init(void *);

/* from board-mmi-clocks.c */
extern struct clk_lookup *mmi_init_clocks_from_dt(int *size);

/* from board-mmi-display.c */
extern void mmi_display_init(struct msm_fb_platform_data *msm_fb_pdata,
			struct mipi_dsi_platform_data *mipi_dsi_pdata);
int mmi_mipi_panel_power_en(int on);

/* from board-mmi-dsp.c */
extern void mmi_audio_dsp_init(void);

/* from board-mmi-regulator.c */
extern void mmi_regulator_init(struct msm8960_oem_init_ptrs *);

/* from board-mmi-vibrator.c */
#ifdef CONFIG_VIB_TIMED
void __init mmi_vibrator_init(void);
#else
static inline void mmi_vibrator_init(void) { }
#endif

/* set of data provided to the modem over SMEM */
#define MMI_UNIT_INFO_VER 2
#define BARCODE_MAX_LEN 64
#define MACHINE_MAX_LEN 32
#define CARRIER_MAX_LEN 64
#define BASEBAND_MAX_LEN 96
#define DEVICE_MAX_LEN 32
struct mmi_unit_info {
	uint32_t version;
	uint32_t system_rev;
	uint32_t system_serial_low;
	uint32_t system_serial_high;
	char machine[MACHINE_MAX_LEN+1];
	char barcode[BARCODE_MAX_LEN+1];
	char carrier[CARRIER_MAX_LEN+1];
	char baseband[BASEBAND_MAX_LEN+1];
	char device[DEVICE_MAX_LEN+1];
	uint32_t radio;
};

extern struct msm_camera_sensor_info msm_camera_sensor_s5k5b3g_data;
extern struct msm_camera_sensor_info msm_camera_sensor_ov8835_data;
extern struct msm_camera_sensor_info msm_camera_sensor_ov8820_data;
extern struct msm_camera_sensor_info msm_camera_sensor_ar0834_data;
extern struct msm_camera_sensor_info msm_camera_sensor_ov10820_data;

extern struct msm_camera_device_platform_data msm_camera_csi_device_data[];

extern struct lm3556_platform_data cam_flash_3556;
extern struct msm_camera_sensor_flash_data camera_flash_lm3556;

#endif

