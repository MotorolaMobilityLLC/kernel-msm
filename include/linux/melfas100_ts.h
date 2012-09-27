/*
 * include/linux/melfas100_ts.h
 *
 * Copyright (C) 2011 Motorola Mobility, Inc.
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

#ifndef _LINUX_MELFAS100_TS_H
#define _LINUX_MELFAS100_TS_H

#include <linux/earlysuspend.h>

#define MELFAS_TS_NAME "melfas-ts"

#define TS_FLIP_X           (1 << 0)
#define TS_FLIP_Y           (1 << 1)

#define MELFAS_IRQ_ENABLED_FLAG      0
#define MELFAS_WAITING_FOR_FW_FLAG   1
#define MELFAS_CHECKSUM_FAILED       2
#define MELFAS_IGNORE_CHECKSUM       3

struct melfas_ts_version_info {
	uint8_t panel_ver;
	uint8_t hw_ver;
	uint8_t hw_comp_grp;
	uint8_t core_fw_ver;
	uint8_t priv_fw_ver;
	uint8_t pub_fw_ver;
};

struct melfas_ts_data {
	struct i2c_client *client;
	struct device *dev;
	struct input_dev *input_dev;
	struct touch_platform_data *pdata;
	struct early_suspend early_suspend;
	struct melfas_ts_version_info version_info;
	atomic_t irq_enabled;
	uint16_t	status;
};
#endif /* _LINUX_MELFAS_TS100_H */
