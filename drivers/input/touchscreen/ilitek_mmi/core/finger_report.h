/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 * Based on TDD v7.0 implemented by Mstar & ILITEK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifndef __FINGER_REPORT_H
#define __FINGER_REPORT_H

struct core_fr_data {
	struct input_dev *input_device;

	/* the default of finger report is enabled */
	bool isEnableFR;
	/* used to send finger report packet to user psace */
	bool isEnableNetlink;
	/* allow input dev to report the value of physical touch */
	bool isEnablePressure;
	/* get screen resloution from fw if it's true */
	bool isSetResolution;
	/* used to change I2C Uart Mode when fw mode is in this mode */
	uint8_t i2cuart_mode;

	/* current firmware mode in driver */
	uint16_t actual_fw_mode;

	/* mutual firmware info */
	uint16_t log_packet_length;
	uint8_t log_packet_header;
	uint8_t type;
	uint8_t Mx;
	uint8_t My;
	uint8_t Sd;
	uint8_t Ss;
};

extern struct core_fr_data *core_fr;

extern uint8_t core_fr_calc_checksum(uint8_t *pMsg, uint32_t nLength);
extern void core_fr_touch_press(int32_t x, int32_t y, uint32_t pressure
, int32_t id);
extern void core_fr_touch_release(int32_t x, int32_t y, int32_t id);
extern void core_fr_mode_control(uint8_t *from_user);
extern void core_fr_handler(void);
extern void core_fr_input_set_param(struct input_dev *input_device);
extern int core_fr_init(void);
extern void core_fr_remove(void);

#endif /* __FINGER_REPORT_H */
