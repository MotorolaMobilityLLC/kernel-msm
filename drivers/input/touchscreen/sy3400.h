/*
 * Copyright (C) 2012 Motorola Mobility, Inc.
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

/* Local header for Synaptics S3400 touchscreens that uses tdat files */
#ifndef _LINUX_SY3400_H
#define _LINUX_SY3400_H

#include <linux/types.h>
#include <linux/input/touch_platform.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/mutex.h>
#include <mach/mmi_panel_notifier.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define SY3400_DRIVER_VERSION       "YF-01-00"
#define SY3400_DRIVER_DATE          "2012-12-21"

#ifdef CONFIG_TOUCHSCREEN_DEBUG
#define sy3400_dbg(dd, level, format, args...) \
{\
	if ((dd->dbg->dbg_lvl) >= level) \
		printk(KERN_INFO format, ## args); \
}
#else
#define sy3400_dbg(dd, level, format, args...) {}
#endif

#define SY3400_DBG0                 0
#define SY3400_DBG1                 1
#define SY3400_DBG2                 2
#define SY3400_DBG3                 3

#define SY3400_SWAP_XY_FLAG         0
#define SY3400_INVERT_X_FLAG        1
#define SY3400_INVERT_Y_FLAG        2

#define SY3400_IRQ_ENABLED_FLAG     0
#define SY3400_WAITING_FOR_TDAT     1
#define SY3400_REPORT_TOUCHES       2
#define SY3400_RESTART_REQUIRED     3

#define SY3400_I2C_ATTEMPTS         10
#define SY3400_I2C_WAIT_TIME        50
#define SY3400_MAX_TOUCHES          10
#define SY3400_MAX_BUTTONS          3
#define SY3400_ABS_RESERVED         0xFFFF
#define SY3400_IC_RESET_HOLD_TIME   1000
#define SY3400_BL_HOLDOFF_TIME      120

#define SY3400_QUERY(base, n)       (base + n)

#define SY3400_BLK_SZ               0x0EE0
#define SY3300_BLK_CT               0x00C0
#define SY3400_BLK_CT               0x00E0

#define SY3400_XY_COMPR_ADDR        0x18
#define SY3400_OBJ_REP_ADDR         0x1A

enum sy3400_driver_state {
	SY3400_DRV_ACTIVE,
	SY3400_DRV_IDLE,
	SY3400_DRV_REFLASH,
	SY3400_DRV_INIT,
};
static const char * const sy3400_driver_state_string[] = {
	"ACTIVE",
	"IDLE",
	"REFLASH",
	"INIT",
};

enum sy3400_ic_state {
	SY3400_IC_ACTIVE,
	SY3400_IC_SLEEP,
	SY3400_IC_UNKNOWN,
	SY3400_IC_BOOTLOADER,
	SY3400_IC_PRESENT,
};
static const char * const sy3400_ic_state_string[] = {
	"ACTIVE",
	"SLEEP",
	"UNKNOWN",
	"BOOTLOADER",
	"PRESENT",
};


struct sy3400_tdat {
	uint8_t         *data;
	size_t          size;
	uint8_t         *tsett;
	uint32_t        tsett_size;
	uint8_t         *fw;
	uint32_t        fw_size;
	uint8_t         addr[2];
} __packed;

struct sy3400_pdt_node {
	uint8_t         desc[6];
	uint8_t         page;
	struct sy3400_pdt_node   *next;
} __packed;

struct sy3400_page_block {
	struct sy3400_pdt_node    *pdt;
	uint8_t         bldid[3];
	uint8_t         prodid[11];
	uint8_t         cfgid[4];
	uint8_t         query_base_addr;
	uint8_t         *irq_table;
	uint8_t         irq_size;
} __packed;

struct sy3400_icdat {
	uint8_t         stat_addr;
	uint8_t         xy_addr;
	uint8_t         pwr_addr;
	uint8_t         pwr_dat;
	uint8_t         buttons_addr;
} __packed;

struct sy3400_touch_data {
	bool            active;
	uint16_t        x;
	uint16_t        y;
	uint8_t         p;
	uint8_t         w;
	uint8_t         id;
} __packed;

struct sy3400_report_data {
	uint16_t        axis[5];
	uint8_t         active_touches;
	struct sy3400_touch_data    tchdat[SY3400_MAX_TOUCHES];
} __packed;

struct sy3400_debug {
	uint8_t         dbg_lvl;
} __packed;


struct sy3400_driver_data {
	struct touch_platform_data  *pdata;
	struct sy3400_tdat          *tdat;
	struct i2c_client           *client;
	struct mutex                *mutex;
	struct input_dev            *in_dev;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend        es;
#endif

	enum sy3400_driver_state    drv_stat;
	enum sy3400_ic_state        ic_stat;

	struct sy3400_page_block    *pblk;
	struct sy3400_icdat         *icdat;
	struct sy3400_report_data   *rdat;
	struct sy3400_debug         *dbg;

	uint8_t         btn_curr;
	uint8_t         btn_prev;
	uint8_t         maxButtonCount;

	struct notifier_block       panel_nb;

	atomic_t        purge_flag;

	uint16_t        status;
	uint16_t        settings;
} __packed;

#endif /* _LINUX_SY3400_H */
