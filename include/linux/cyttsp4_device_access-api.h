/* < DTS2013062605264 sunlibin 20130702 begin */
/* add cypress new driver ttda-02.03.01.476713 */
/*
 * cyttsp4_device_access-api.h
 * Cypress TrueTouch(TM) Standard Product V4 Device Access API module.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2012 Cypress Semiconductor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#ifndef _LINUX_CYTTSP4_DEVICE_ACCESS_API_H
#define _LINUX_CYTTSP4_DEVICE_ACCESS_API_H

#include <linux/types.h>
#include <linux/device.h>

#define GRPNUM_OP_COMMAND	1
#define GRPNUM_TOUCH_CONFIG	6

#define OP_CMD_NULL		0
#define OP_CMD_GET_PARAMETER	2
#define OP_CMD_SET_PARAMETER	3
#define OP_CMD_GET_CONFIG_CRC	5

#define OP_PARAM_ACTIVE_DISTANCE		0x4A
#define OP_PARAM_SCAN_TYPE			0x4B
#define OP_PARAM_LOW_POWER_INTERVAL		0x4C
#define OP_PARAM_REFRESH_INTERVAL		0x4D
#define OP_PARAM_ACTIVE_MODE_TIMEOUT		0x4E
#define OP_PARAM_ACTIVE_LOOK_FOR_TOUCH_INTERVAL 0x4F

extern int cyttsp4_device_access_read_command(const char *core_name,
		int ic_grpnum, int ic_grpoffset, u8 *buf, int buf_size);

extern int cyttsp4_device_access_write_command(const char *core_name,
		int ic_grpnum, int ic_grpoffset, u8 *buf, int length);

#endif /* _LINUX_CYTTSP4_DEVICE_ACCESS_API_H */

/* DTS2013062605264 sunlibin 20130702 end > */
