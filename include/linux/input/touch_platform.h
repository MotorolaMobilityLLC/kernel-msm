/*
 * Copyright (C) 2010 Motorola Mobility, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

/* Defines generic platform structures for touch drivers */
#ifndef _LINUX_TOUCH_PLATFORM_H
#define _LINUX_TOUCH_PLATFORM_H

#include <linux/types.h>

#define ATMXT_I2C_NAME		"atmxt-i2c"

struct touch_settings {
	uint8_t		*data;
	uint8_t		size;
	uint8_t		tag;
} __attribute__ ((packed));

struct touch_firmware {
	uint8_t		*img;
	uint32_t	size;
	uint8_t		*ver;
	uint8_t		vsize;
	uint8_t		*private_fw_v;
	uint8_t		private_fw_v_size;
	uint8_t		*public_fw_v;
	uint8_t		public_fw_v_size;
} __attribute__ ((packed));

struct touch_framework {
	const uint16_t	*abs;
	uint8_t		size;
	uint8_t		enable_vkeys;
} __attribute__ ((packed));

/* Not all the fields will be used by all the drivers */
struct touch_platform_data {
	struct touch_settings	*sett[256];
	struct touch_firmware	*fw;
	struct touch_framework	*frmwrk;

	uint8_t			addr[2];
	uint32_t		flags;

	int			gpio_enable;
	int			gpio_reset;
	int			gpio_interrupt;
	int			gpio_sda;
	int			gpio_scl;
	char			*filename;

	/* as of 2/8/2012, used only by Melfas driver */
	int			max_x;
	int			max_y;

	bool			invert_x;
	bool			invert_y;
	char			fw_name[20];

	int 			(*mux_fw_flash)(bool to_gpios);
	void			(*int_latency)(void);
	void			(*int_time)(void);
	u32			(*get_avg_lat)(void);
	u32			(*get_high_lat)(void);
	u32			(*get_slow_cnt)(void);
	u32			(*get_int_cnt)(void);
	void			(*set_dbg_lvl)(u8 debug_level);
	u8			(*get_dbg_lvl)(void);
	u32			(*get_time_ptr)(u64 **timestamp);
	u32			(*get_lat_ptr)(u32 **latency);
	void			(*set_time_ptr)(unsigned long ptr);
	void			(*set_lat_ptr)(unsigned long ptr);

} __attribute__ ((packed));

#endif /* _LINUX_TOUCH_PLATFORM_H */
