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

struct ov660_reg_i2c_tbl {
	uint16_t reg_addr;
	uint8_t reg_data;
};

extern int32_t ov660_check_probe(void);
extern int32_t ov660_intialize_8MP(void);
extern int32_t ov660_intialize_10MP(void);

#endif /* __OV660_V4L2_H__ */

