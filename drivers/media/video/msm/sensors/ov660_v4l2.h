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
#ifndef OV660_V4L2_H
#define OV660_V4L2_H

#include "msm_sensor.h"
#include <linux/kernel.h>

#define OV660_NAME "ov660"
#define OV660_NAME_LEN (sizeof(OV660_NAME)-1)

#define I2C_ADDR_BYPASS 0x6106
#define I2C_ADDR_RGBC_OUTPUT 0x7002
#define AF_STATISTICS_ADDR 0x7980
#define AF_USEFUL_STATISTICS_ADDR_OFFSET (0x79C0 - AF_STATISTICS_ADDR)
#define AF_STATISTICS_DATA 120
#define AF_USEFUL_STATISTICS_DATA 24

/* MISC DEVICE COMMANDS */
#define MAJOR_NUM 100
#define CAMERA_SET_RGBC_OUTPUT _IOR(MAJOR_NUM, 0, long *)
#define CAMERA_SET_FOCUS_WINDOW _IOR(MAJOR_NUM, 1, long *)
#define CAMERA_GET_LSC_VALUES _IOWR(MAJOR_NUM, 2, long *)

struct ov660_reg_i2c_tbl {
	uint16_t reg_addr;
	uint8_t reg_data;
};

extern bool allow_asic_control;

extern int32_t ov660_set_sensor_mode(int mode, uint16_t revision);
extern int32_t ov660_set_exposure_gain(uint16_t gain, uint32_t line);
extern int32_t ov660_set_exposure_gain2(uint16_t gain, uint32_t line);
extern int32_t ov660_set_i2c_bypass(int bypassOn);
extern int32_t ov660_add_blc_firmware(uint16_t addr);
extern int32_t ov660_use_work_around_blc(void);
extern int32_t ov660_check_probe(void);
extern int32_t ov660_initialize_8MP(void);
extern int32_t ov660_initialize_10MP(uint16_t revision);
extern int32_t ov660_read_revision(void);
extern int32_t ov660_is_rgbc_output(void);

#endif /* __OV660_V4L2_H__ */

