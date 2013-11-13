/*
 * Copyright (C) 2010 Melfas, Inc.
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
#ifndef _LINUX_MELFAS_TS_H
#define _LINUX_MELFAS_TS_H

#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#define DRIVER_NAME "touch_dev"

#define MAX_NUM_OF_BUTTON       4
#define MAX_SECTION_NUM	        3

#define TOUCH_VDD_VTG_MIN_UV    2950000
#define TOUCH_VDD_VTG_MAX_UV    2950000
#define TOUCH_LPM_LOAD_UA       10
#define TOUCH_ACTIVE_LOAD_UA    10000

#define TOUCH_I2C_VTG_MIN_UV    1800000
#define TOUCH_I2C_VTG_MAX_UV    1800000
#define TOUCH_I2C_LPM_LOAD_UA   10
#define TOUCH_I2C_LOAD_UA       10000

struct melfas_tsi_platform_data {
	uint32_t version;
	int x_max;
	int y_max;
	int max_pressure;
	int max_width;

	int gpio_scl;
	int gpio_sda;
	bool i2c_pull_up;

	int i2c_int_gpio;
	int i2c_sda_gpio;
	int i2c_scl_gpio;
	int touch_id_gpio;
	int gpio_ldo;
	u32 irq_flag;

	int auto_fw_update;
	bool force_upgrade;
	unsigned char num_of_finger;
	unsigned char num_of_button;
	unsigned short button[MAX_NUM_OF_BUTTON];
	const char *product;
	const char *fw_name;
};

struct mms_bin_hdr {
	char	tag[8];
	u16	core_version;
	u16	section_num;
	u16	contains_full_binary;
	u16	reserved0;

	u32	binary_offset;
	u32	binary_length;

	u32	extention_offset;
	u32	reserved1;

} __attribute__ ((packed));

struct mms_fw_img {
	u16	type;
	u16	version;

	u16	start_page;
	u16	end_page;

	u32	offset;
	u32	length;

} __attribute__ ((packed));

struct mms_ts_section {
	u8 version;
	u8 compatible_version;
	u8 start_addr;
	u8 end_addr;
	int offset;
	u32 crc;
};

struct mms_fw_info {
	struct mms_ts_section ts_section[MAX_SECTION_NUM];
	bool need_update[MAX_SECTION_NUM];
	struct mms_fw_img* fw_img[MAX_SECTION_NUM];
	struct mms_bin_hdr *fw_hdr;
};

#define TOUCH_INFO_MSG(fmt, args...) 	printk(KERN_ERR "[Touch] " fmt, ##args)

#endif /* _LINUX_MELFAS_TS_H */
