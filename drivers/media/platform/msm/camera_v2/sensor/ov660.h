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
#ifndef OV660_H
#define OV660_H

#include "msm_sensor.h"
#include <linux/kernel.h>

#define OV660_NAME "ov660"
#define OV660_NAME_LEN (sizeof(OV660_NAME)-1)

#define I2C_ADDR_BYPASS 0x6106
#define AF_STATISTICS_ADDR 0x7980
#define AF_USEFUL_STATISTICS_ADDR_OFFSET (0x79C0 - AF_STATISTICS_ADDR)
#define AF_STATISTICS_DATA 120
#define AF_USEFUL_STATISTICS_DATA 24

/* MISC DEVICE COMMANDS */
#define MAJOR_NUM 100
#define CAMERA_WRITE_I2C _IOR(MAJOR_NUM, 0, long *)


extern int32_t ov660_set_i2c_bypass(struct msm_sensor_ctrl_t *s_ctrl,
		int bypassOn);
extern int32_t ov660_check_probe(struct msm_sensor_ctrl_t *s_ctrl);
#endif /* __OV660_V4L2_H__ */

