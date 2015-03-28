/* add cypress new driver ttda-02.03.01.476713 */

/*
 * cyttsp4_core.h
 * Cypress TrueTouch(TM) Standard Product V4 Core driver module.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2012 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * Author: Aleksej Makarov <aleksej.makarov@sonyericsson.com>
 * Modifed by: Cypress Semiconductor to add touch settings
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

#ifndef _LINUX_CYTTSP4_CORE_H
#define _LINUX_CYTTSP4_CORE_H

#include <linux/stringify.h>

#define CYTTSP4_CORE_NAME "cyttsp4_core"

#define CY_DRIVER_NAME TTDA
#define CY_DRIVER_MAJOR 02
#define CY_DRIVER_MINOR 03
#define CY_DRIVER_SUBRELEASE 01

#define CY_DRIVER_REVCTRL 485332

#define CY_DRIVER_VERSION		    \
__stringify(CY_DRIVER_NAME)		    \
"." __stringify(CY_DRIVER_MAJOR)	    \
"." __stringify(CY_DRIVER_MINOR)	    \
"." __stringify(CY_DRIVER_SUBRELEASE)	    \
"." __stringify(CY_DRIVER_REVCTRL)

#define CY_DRIVER_DATE "20130612"	/* YYYYMMDD */

/* x-axis resolution of panel in pixels */
#define CY_PCFG_RESOLUTION_X_MASK 0x7F

/* y-axis resolution of panel in pixels */
#define CY_PCFG_RESOLUTION_Y_MASK 0x7F

/* x-axis, 0:origin is on left side of panel, 1: right */
#define CY_PCFG_ORIGIN_X_MASK 0x80

/* y-axis, 0:origin is on top side of panel, 1: bottom */
#define CY_PCFG_ORIGIN_Y_MASK 0x80

#define CY_TOUCH_SETTINGS_MAX 32
#define CY_TOUCH_SETTINGS_PARAM_REGS 6

#define SET_AT_LATE_RESUME_NEED_NOT	0
#define SET_AT_LATE_RESUME_NEEDED	1
/* define the log level for cyttsp driver */
/* add vdebug */
#define TP_ERR  1
#define TP_INFO 2
#define TP_DBG  3
#define TP_VDBG  4
extern int cyttsp_debug_mask;

#ifndef tp_log_err
#define tp_log_err(x...)                \
do{                                     \
    if( cyttsp_debug_mask >= TP_ERR )   \
    {                                   \
        printk(KERN_ERR "[CYTTSP] " x); \
    }                                   \
                                        \
}while(0)
#endif

#ifndef tp_log_info
#define tp_log_info(x...)               \
do{                                     \
    if( cyttsp_debug_mask >= TP_INFO )  \
    {                                   \
        printk(KERN_ERR "[CYTTSP] " x); \
    }                                   \
                                        \
}while(0)
#endif

#ifndef tp_log_debug
#define tp_log_debug(x...)              \
do{                                     \
    if( cyttsp_debug_mask >= TP_DBG )   \
    {                                   \
        printk(KERN_ERR "[CYTTSP] " x); \
    }                                   \
                                        \
}while(0)
#endif

#ifndef tp_log_vdebug
#define tp_log_vdebug(x...)              \
do{                                     \
    if( cyttsp_debug_mask >= TP_VDBG )   \
    {                                   \
        printk(KERN_ERR "[CYTTSP] " x); \
    }                                   \
                                        \
}while(0)
#endif

enum cyttsp4_core_platform_flags {
	CY_CORE_FLAG_NONE = 0x00,
	CY_CORE_FLAG_WAKE_ON_GESTURE = 0x01,
};

enum cyttsp4_core_platform_easy_wakeup_gesture {
	CY_CORE_EWG_NONE = 0x00,
	CY_CORE_EWG_TAP_TAP = 0x01,
	CY_CORE_EWG_TWO_FINGER_SLIDE = 0x02,
	CY_CORE_EWG_RESERVED = 0x03,
	CY_CORE_EWG_WAKE_ON_INT_FROM_HOST = 0xFF,
};

enum cyttsp4_loader_platform_flags {
	CY_LOADER_FLAG_NONE = 0x00,
	CY_LOADER_FLAG_CALIBRATE_AFTER_FW_UPGRADE = 0x01,
	/* Use CONFIG_VER field in TT_CFG to decide TT_CFG update */
	CY_LOADER_FLAG_CHECK_TTCONFIG_VERSION = 0x02,
	CY_FLAG_AUTO_CALIBRATE = 0x03,	//huawei 0701
};

struct touch_settings {
	const uint8_t *data;
	uint32_t size;
	uint8_t tag;
} __packed;

struct cyttsp4_touch_firmware {
	const uint8_t *img;
	uint32_t size;
	const uint8_t *ver;
	uint8_t vsize;
} __packed;

struct cyttsp4_touch_config {
	struct touch_settings *param_regs;
	struct touch_settings *param_size;
	const uint8_t *fw_ver;
	uint8_t fw_vsize;
};

struct cyttsp4_sett_ttconfig_map {
	u8 id;
	char *product_family;
	struct cyttsp4_touch_config *ttconfig_param;
};

typedef enum {
	TP_COF = 0,
	TP_COB = 1,
	TP_UNKNOW
} tp_location;

struct cyttsp4_loader_platform_data {
	struct cyttsp4_touch_firmware *fw;
	struct cyttsp4_touch_config *ttconfig;
	struct cyttsp4_sett_ttconfig_map *ttconfig_map;
	//huawei add
	char const *firmware_name;
	tp_location tp_location;
	//huawei add
	u32 flags;
	char const *tp_product_family;
} __packed;

typedef int (*cyttsp4_platform_read) (struct device * dev, u16 addr,
				      void *buf, int size);

struct cyttsp4_core_platform_data {
	int irq_gpio;
	int rst_gpio;
	int level_irq_udelay;
	int max_xfer_len;
	int (*xres) (struct cyttsp4_core_platform_data * pdata,
		     struct device * dev);
	int (*init) (struct cyttsp4_core_platform_data * pdata,
		     int on, struct device * dev);
	int (*power) (struct cyttsp4_core_platform_data * pdata,
		      int on, struct device * dev, atomic_t * ignore_irq);
	int (*detect) (struct cyttsp4_core_platform_data * pdata,
		       struct device * dev, cyttsp4_platform_read read);
	int (*irq_stat) (struct cyttsp4_core_platform_data * pdata,
			 struct device * dev);
	struct touch_settings *sett[CY_TOUCH_SETTINGS_MAX];
	struct cyttsp4_loader_platform_data *loader_pdata;
	u32 flags;
	u8 easy_wakeup_gesture;
	u8 easy_wakeup_supported_gestures;
	/*add for glove function */
	u32 use_configure_sensitivity;
};

#ifdef VERBOSE_DEBUG
extern void cyttsp4_pr_buf(struct device *dev, u8 * pr_buf, u8 * dptr, int size,
			   const char *data_name);
#else
#define cyttsp4_pr_buf(a, b, c, d, e) do { } while (0)
#endif

#endif /* _LINUX_CYTTSP4_CORE_H */

