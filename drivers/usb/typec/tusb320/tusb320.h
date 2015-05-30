/*
 * tusb320.h - Driver for TUSB320 USB Type-C connector chip
 *
 * Copyright (C) 2015 HUAWEI, Inc.
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 * Author: HUAWEI, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _TYPEC_TUSB320_H_
#define _TYPEC_TUSB320_H_

#include <linux/bitops.h>

struct tusb320_device_info {
	int gpio_enb;
	int gpio_intb;
	int irq_intb;
	int irq_active;
	struct i2c_client *client;
	struct work_struct g_intb_work;
	struct device *dev;
};

#define REGISTER_NUM    12

/* Register address */
#define TUSB320_REG_DEVICE_ID				0x00
#define TUSB320_REG_CURRENT_MODE			0x08
#define TUSB320_REG_ATTACH_STATUS			0x09
#define TUSB320_REG_MODE_SET				0x0a

/* Register REG_CURRENT_MODE 08 */
#define TUSB320_REG_CUR_MODE_ADVERTISE_MASK		(BIT(7) | BIT(6))
#define TUSB320_REG_CUR_MODE_ADVERTISE_DEFAULT	0x00
#define TUSB320_REG_CUR_MODE_ADVERTISE_MID		BIT(6)
#define TUSB320_REG_CUR_MODE_ADVERTISE_HIGH		BIT(7)
#define TUSB320_REG_CUR_MODE_DETECT_MASK		(BIT(5) | BIT(4))
#define TUSB320_REG_CUR_MODE_DETECT_DEFAULT		0x00
#define TUSB320_REG_CUR_MODE_DETECT_MID			BIT(4)
#define TUSB320_REG_CUR_MODE_DETECT_HIGH		(BIT(5) | BIT(4))

/* Register REG_ATTACH_STATUS 09 */
#define TUSB320_REG_STATUS_MODE					(BIT(7) | BIT(6))
#define TUSB320_REG_STATUS_NOT_ATTACHED			0x00
#define TUSB320_REG_STATUS_AS_DFP				BIT(6)
#define TUSB320_REG_STATUS_AS_UFP				BIT(7)
#define TUSB320_REG_STATUS_TO_ACCESSORY			(BIT(7) | BIT(6))
#define TUSB320_REG_STATUS_CC					BIT(5)
#define TUSB320_REG_STATUS_INT					BIT(4)

/* Register REG_MODE_SET 0a */
#define TUSB320_REG_SET_MODE					(BIT(5) | BIT(4))
#define TUSB320_REG_SET_BY_PORT					0x00
#define TUSB320_REG_SET_UFP						BIT(4)
#define TUSB320_REG_SET_DFP						BIT(5)
#define TUSB320_REG_SET_DRP						(BIT(5) | BIT(4))

#endif /*_TYPEC_TUSB320_H_*/
