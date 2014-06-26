/*
 * platform_camera.h: CAMERA platform library header file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_CAMERA_H_
#define _PLATFORM_CAMERA_H_

#include <linux/atomisp_platform.h>
#include <asm/intel-mid.h>
#include <asm/intel_scu_ipcutil.h>

extern const struct intel_v4l2_subdev_id v4l2_ids[] __attribute__((weak));

#define IS_BYT (INTEL_MID_BOARD(1, PHONE, BYT) || \
	INTEL_MID_BOARD(1, TABLET, BYT))

#define IS_CHT (INTEL_MID_BOARD(1, PHONE, CHT) || \
	INTEL_MID_BOARD(1, TABLET, CHT))

/* MFLD iCDK camera sensor GPIOs */

/* Obsolete pin, maybe used by old MFLD iCDK */
#define GP_CAMERA_0_POWER_DOWN          "cam0_vcm_2p8"
/* Camera VDD 1.8V Switch */
#define GP_CAMERA_1P8			"camera_on_n"
/* Camera0 Standby pin control */
#define GP_CAMERA_0_STANDBY		"camera_0_power"
#define GP_CAMERA_1_POWER_DOWN          "camera_1_power"
#define GP_CAMERA_0_RESET               "camera_0_reset"
#define GP_CAMERA_1_RESET               "camera_1_reset"
#define GP_CAMERA_MUX_CONTROL		"camera_mux"
#define GP_CAMERA_2_3_RESET		"cam_2_3_reset"
#define GP_CAMERA_ISP1_POWERDOWN	"isp1_powerdown"

extern int camera_sensor_gpio(int gpio, char *name, int dir, int value);
extern int camera_sensor_csi(struct v4l2_subdev *sd, u32 port,
			u32 lanes, u32 format, u32 bayer_order, int flag);
extern void intel_register_i2c_camera_device(
				struct sfi_device_table_entry *pentry,
				struct devs_id *dev)
				__attribute__((weak));
char *camera_get_msr_filename(char *buf, int buf_size, char *sensor, int cam);

struct vprog_status {
	unsigned int user;
};

/*
 * FIXME! This PMIC power access workaround for CHT
 * since currently no VRF for CHT
 */
#ifdef CONFIG_CRYSTAL_COVE
#define VPROG_2P8V 0x5D
#define VPROG_1P8V 0x57
#define VPROG_ENABLE 0x63
#define VPROG_DISABLE 0x62

enum camera_pmic_pin {
	CAMERA_1P8V,
	CAMERA_2P8V,
	CAMERA_POWER_NUM,
};

int camera_set_pmic_power(enum camera_pmic_pin pin, bool flag);
#endif

#ifdef CONFIG_INTEL_SCU_IPC_UTIL
enum camera_vprog {
	CAMERA_VPROG1,
	CAMERA_VPROG2,
	CAMERA_VPROG3,
	CAMERA_VPROG_NUM,
};

enum camera_vprog_voltage {
	DEFAULT_VOLTAGE,
	CAMERA_1_05_VOLT,
	CAMERA_1_5_VOLT,
	CAMERA_1_8_VOLT,
	CAMERA_1_83_VOLT,
	CAMERA_1_85_VOLT,
	CAMERA_2_5_VOLT,
	CAMERA_2_8_VOLT,
	CAMERA_2_85_VOLT,
	CAMERA_2_9_VOLT,
};

int camera_set_vprog_power(enum camera_vprog vprog, bool flag,
			   enum camera_vprog_voltage voltage);
#endif

#endif
