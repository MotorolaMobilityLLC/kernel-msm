/*
 * Copyright (C) 2010-2012 Motorola Mobility, Inc.
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

/* Local header for Atmel maXTouch touchscreens that uses tdat files */
#ifndef _LINUX_ATMXT_H
#define _LINUX_ATMXT_H

#include <linux/types.h>
#include <linux/input/touch_platform.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/mutex.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define ATMXT_DRIVER_VERSION        "YN-04-01"
#define ATMXT_DRIVER_DATE           "2012-06-28"

#ifdef CONFIG_TOUCHSCREEN_DEBUG
#define atmxt_dbg(dd, level, format, args...) \
{\
	if ((dd->dbg->dbg_lvl) >= level) \
		printk(KERN_INFO format, ## args); \
}
#else
#define atmxt_dbg(dd, level, format, args...) {}
#endif

#define ATMXT_DBG0                  0
#define ATMXT_DBG1                  1
#define ATMXT_DBG2                  2
#define ATMXT_DBG3                  3

#define ATMXT_IRQ_ENABLED_FLAG      0
#define ATMXT_WAITING_FOR_TDAT      1
#define ATMXT_CHECKSUM_FAILED       2
#define ATMXT_IGNORE_CHECKSUM       3
#define ATMXT_REPORT_TOUCHES        4
#define ATMXT_FIXING_CALIBRATION    5
#define ATMXT_RECEIVED_CALIBRATION  6
#define ATMXT_RESTART_REQUIRED      7
#define ATMXT_SET_MESSAGE_POINTER   8

#define ATMXT_I2C_ATTEMPTS          10
#define ATMXT_I2C_WAIT_TIME         50
#define ATMXT_MAX_TOUCHES           10
#define ATMXT_ABS_RESERVED          0xFFFF
#define ATMXT_IC_RESET_HOLD_TIME    1000


enum atmxt_driver_state {
	ATMXT_DRV_ACTIVE,
	ATMXT_DRV_IDLE,
	ATMXT_DRV_REFLASH,
	ATMXT_DRV_INIT,
};
static const char * const atmxt_driver_state_string[] = {
	"ACTIVE",
	"IDLE",
	"REFLASH",
	"INIT",
};

enum atmxt_ic_state {
	ATMXT_IC_ACTIVE,
	ATMXT_IC_SLEEP,
	ATMXT_IC_UNKNOWN,
	ATMXT_IC_BOOTLOADER,
	ATMXT_IC_PRESENT,
};
static const char * const atmxt_ic_state_string[] = {
	"ACTIVE",
	"SLEEP",
	"UNKNOWN",
	"BOOTLOADER",
	"PRESENT",
};


struct atmxt_util_data {
	uint8_t         *data;
	size_t          size;
	uint8_t         *tsett;
	uint32_t        tsett_size;
	uint8_t         *fw;
	uint32_t        fw_size;
	uint8_t         addr[2];
} __packed;

struct atmxt_nvm {
	uint8_t           *data;
	uint16_t          size;
	uint8_t           addr[2];
	uint8_t           chksum[3];
} __packed;

struct atmxt_info_block {
	uint8_t         header[7];
	uint8_t         *data;
	uint8_t         size;
	uint8_t         *msg_id;
	uint8_t         id_size;
} __packed;

struct atmxt_addr {
	uint8_t         msg[2];
	uint8_t         pwr[2];
	uint8_t         rst[2];
	uint8_t         nvm[2];
	uint8_t         cal[2];
	uint8_t         acq[2];
} __packed;

struct atmxt_data {
	uint8_t         pwr[2];
	uint8_t         max_msg_size;
	uint8_t         touch_id_offset;
	bool            res[2];
	uint8_t         acq[6];
	unsigned long   timer;
	uint8_t         last_stat;
} __packed;

struct atmxt_touch_data {
	bool            active;
	uint16_t        x;
	uint16_t        y;
	uint8_t         p;
	uint8_t         w;
	uint8_t         id;
} __packed;

struct atmxt_report_data {
	uint16_t        axis[5];
	uint8_t         active_touches;
	struct atmxt_touch_data     tchdat[ATMXT_MAX_TOUCHES];
} __packed;

struct atmxt_debug {
	uint8_t         dbg_lvl;
	uint8_t         grp_num;
	uint8_t         grp_off;
} __packed;


struct atmxt_driver_data {
	struct touch_platform_data  *pdata;
	struct atmxt_util_data      *util;
	struct i2c_client           *client;
	struct mutex                *mutex;
	struct input_dev            *in_dev;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend        es;
#endif

	enum atmxt_driver_state     drv_stat;
	enum atmxt_ic_state         ic_stat;

	struct atmxt_info_block     *info_blk;
	struct atmxt_nvm            *nvm;
	struct atmxt_addr           *addr;
	struct atmxt_data           *data;
	struct atmxt_report_data    *rdat;
	struct atmxt_debug          *dbg;

	uint16_t        status;
	uint16_t        settings;
} __packed;

#endif /* _LINUX_ATMXT_H */
