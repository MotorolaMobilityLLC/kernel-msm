/*
 * Copyright (C) 2012 Motorola Mobility LLC.
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

/* Local header for Synaptics S3200 touchscreens that uses tdat files */
#ifndef _LINUX_SY3200_H
#define _LINUX_SY3200_H

#include <linux/types.h>
#include <linux/input/touch_platform.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/mutex.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define SY3200_DRIVER_VERSION       "AR-01-00-v2"
#define SY3200_DRIVER_DATE          "2013-01-09"

#ifdef CONFIG_TOUCHSCREEN_DEBUG
#define sy3200_dbg(dd, level, format, args...) \
{\
	if ((dd->dbg->dbg_lvl) >= level) \
		printk(KERN_INFO format, ## args); \
}
#else
#define sy3200_dbg(dd, level, format, args...) {}
#endif

#define SY3200_DBG0                 0
#define SY3200_DBG1                 1
#define SY3200_DBG2                 2
#define SY3200_DBG3                 3

#define SY3200_IRQ_ENABLED_FLAG     0
#define SY3200_WAITING_FOR_TDAT     1
#define SY3200_REPORT_TOUCHES       2
#define SY3200_RESTART_REQUIRED     3

#define SY3200_I2C_ATTEMPTS         10
#define SY3200_I2C_WAIT_TIME        50
#define SY3200_MAX_TOUCHES          10
#define SY3200_ABS_RESERVED         0xFFFF
#define SY3200_IC_RESET_HOLD_TIME   1000
#define SY3200_BL_HOLDOFF_TIME      800

#define SY3200_QUERY(base, n)       (base + n)

#define SY3200_BLK_SZ               0x0010
#define SY3200_BLK_CT               0x0B00

enum sy3200_driver_state {
	SY3200_DRV_ACTIVE,
	SY3200_DRV_IDLE,
	SY3200_DRV_REFLASH,
	SY3200_DRV_INIT,
};
static const char * const sy3200_driver_state_string[] = {
	"ACTIVE",
	"IDLE",
	"REFLASH",
	"INIT",
};

enum sy3200_ic_state {
	SY3200_IC_ACTIVE,
	SY3200_IC_SLEEP,
	SY3200_IC_UNKNOWN,
	SY3200_IC_BOOTLOADER,
	SY3200_IC_PRESENT,
};
static const char * const sy3200_ic_state_string[] = {
	"ACTIVE",
	"SLEEP",
	"UNKNOWN",
	"BOOTLOADER",
	"PRESENT",
};


struct sy3200_tdat {
	uint8_t         *data;
	size_t          size;
	uint8_t         *tsett;
	uint32_t        tsett_size;
	uint8_t         *fw;
	uint32_t        fw_size;
	uint8_t         addr[2];
} __packed;

struct sy3200_pdt_node {
	uint8_t         desc[6];
	struct sy3200_pdt_node   *next;
} __packed;

struct sy3200_page_block {
	struct sy3200_pdt_node    *pdt;
	uint8_t         bldid[3];
	uint8_t         prodid[11];
	uint8_t         cfgid[4];
	uint8_t	        query_base_addr;
	uint8_t         *irq_table;
	uint8_t         irq_size;
} __packed;

struct sy3200_icdat {
	uint8_t         stat_addr;
	uint8_t         xy_addr;
	uint8_t         pwr_addr;
	uint8_t         pwr_dat;
} __packed;

struct sy3200_touch_data {
	bool            active;
	uint16_t        x;
	uint16_t        y;
	uint8_t         p;
	uint8_t         w;
	uint8_t         id;
} __packed;

struct sy3200_report_data {
	uint16_t        axis[5];
	uint8_t         active_touches;
	struct sy3200_touch_data    tchdat[SY3200_MAX_TOUCHES];
} __packed;

struct sy3200_debug {
	uint8_t         dbg_lvl;
} __packed;


struct sy3200_driver_data {
	struct touch_platform_data  *pdata;
	struct sy3200_tdat          *tdat;
	struct i2c_client           *client;
	struct mutex                *mutex;
	struct input_dev            *in_dev;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend        es;
#endif

	enum sy3200_driver_state    drv_stat;
	enum sy3200_ic_state        ic_stat;

	struct sy3200_page_block    *pblk;
	struct sy3200_icdat         *icdat;
	struct sy3200_report_data   *rdat;
	struct sy3200_debug         *dbg;

	uint16_t        status;
	uint16_t        settings;
} __packed;

#endif /* _LINUX_SY3200_H */
