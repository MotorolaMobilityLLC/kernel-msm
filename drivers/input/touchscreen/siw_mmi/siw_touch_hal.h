/*
 * SiW touch hal driver
 *
 * Copyright (C) 2016 Silicon Works - http://www.siliconworks.co.kr
 * Author: Hyunho Kim <kimhh@siliconworks.co.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#ifndef __SIW_TOUCH_HAL_H
#define __SIW_TOUCH_HAL_H

#include "siw_touch_cfg.h"

#if defined(__SIW_SUPPORT_PM_QOS)
#include <linux/pm_qos.h>
#endif

#include "siw_touch_hal_reg.h"

enum {
	NON_FAULT_INT	= -1,
	NON_FAULT_U32	= ~0,
};

/* report packet - type 1 */
struct siw_hal_touch_data_type_1 {
	u32 track_id: 5;
	u32 tool_type: 3;
	u32 angle: 8;
	u32 event: 2;
	u32 x: 14;

	u32 y: 14;
	u32 pressure: 8;
	u32 reserve1: 10;

	u32 reserve2: 4;
	u32 width_major: 14;
	u32 width_minor: 14;
} __packed;

/* report packet */
struct siw_hal_touch_data {
	u8 tool_type: 4;
	u8 event: 4;
	u8 track_id;
	u16 x;
	u16 y;
	u8 pressure;
	u8 angle;
	u16 width_major;
	u16 width_minor;
} __packed;

struct siw_hal_touch_info {
	u32 ic_status;
	u32 device_status;
	u32 wakeup_type: 8;
	u32 touch_cnt: 5;
	u32 button_cnt: 3;
	u32 palm_bit: 16;
	struct siw_hal_touch_data data[MAX_FINGER];
} __packed;

#define PALM_ID					15

enum {
	TC_DRIVE_CTL_START		= (0x1 << 0),
	TC_DRIVE_CTL_STOP		= (0x1 << 1),
	TC_DRIVE_CTL_DISP_U0	= (0x0 << 7),
	TC_DRIVE_CTL_DISP_U2	= (0x2 << 7),
	TC_DRIVE_CTL_DISP_U3	= (0x3 << 7),
	/* */
	TC_DRIVE_CTL_PARTIAL	= (0x1 << 9),
	TC_DRIVE_CTL_QCOVER		= (0x1 << 10),
	/* */
	TC_DRIVE_CTL_MODE_VB	= (0x0 << 2),
	TC_DRIVE_CTL_MODE_6LHB	= (0x1 << 2),
	TC_DRIVE_CTL_MODE_MIK	= (0x1 << 3),
};

#define CMD_DIS					0xAA
#define CMD_ENA					0xAB
#define CMD_CLK_ON				0x83
#define CMD_CLK_OFF				0x82
#define CMD_OSC_ON				0x81
#define CMD_OSC_OFF				0x80
#define CMD_RESET_LOW			0x84
#define CMD_RESET_HIGH			0x85


#define CONNECT_NONE			(0x00)
#define CONNECT_USB				(0x01)
#define CONNECT_DC				(0x02)
#define CONNECT_OTG				(0x03)
#define CONNECT_WIRELESS		(0x04)

enum {
	SW_RESET = 0,
	HW_RESET_ASYNC,
	HW_RESET_SYNC,
	HW_RESET_COND = 0x5A,
};

enum {
	TOUCHSTS_IDLE = 0,
	TOUCHSTS_DOWN,
	TOUCHSTS_MOVE,
	TOUCHSTS_UP,
};

enum {
	LCD_MODE_U0	= 0,
	LCD_MODE_U2_UNBLANK,
	LCD_MODE_U2,
	LCD_MODE_U3,
	LCD_MODE_U3_PARTIAL,
	LCD_MODE_U3_QUICKCOVER,
	LCD_MODE_STOP,
	LCD_MODE_MAX,
};

static const char * const __siw_lcd_driving_mode_strs[] = {
	[LCD_MODE_U0] = "U0",
	[LCD_MODE_U2_UNBLANK] = "U2_UNBLANK",
	[LCD_MODE_U2] = "U2",
	[LCD_MODE_U3] = "U3",
	[LCD_MODE_U3_PARTIAL] = "U3_PARTIAL",
	[LCD_MODE_U3_QUICKCOVER] = "U3_QUICKCOVER",
	[LCD_MODE_STOP] = "STOP",
};

static inline const char *siw_lcd_driving_mode_str(int mode)
{
	return (mode < LCD_MODE_MAX) ?
	       __siw_lcd_driving_mode_strs[mode] : "(invalid)";
}

/*
 * for pdata->mode_allowed
 */
enum {
	LCD_MODE_BIT_U0			= BIT(LCD_MODE_U0),
	LCD_MODE_BIT_U2_UNBLANK		= BIT(LCD_MODE_U2_UNBLANK),
	LCD_MODE_BIT_U2			= BIT(LCD_MODE_U2),
	LCD_MODE_BIT_U3			= BIT(LCD_MODE_U3),
	LCD_MODE_BIT_U3_PARTIAL		= BIT(LCD_MODE_U3_PARTIAL),
	LCD_MODE_BIT_U3_QUICKCOVER	= BIT(LCD_MODE_U3_QUICKCOVER),
	LCD_MODE_BIT_STOP		= BIT(LCD_MODE_STOP),
	LCD_MODE_BIT_MAX		= BIT(LCD_MODE_MAX),
};


enum {
	SWIPE_R = 0,
	SWIPE_L,
};

enum {
	SWIPE_RIGHT_BIT	= 1,
	SWIPE_LEFT_BIT = (1 << 16),
};

/* swipe */
enum {
	SWIPE_ENABLE_CTRL = 0,
	SWIPE_DISABLE_CTRL,
	SWIPE_DIST_CTRL,
	SWIPE_RATIO_THR_CTRL,
	SWIPE_RATIO_PERIOD_CTRL,
	SWIPE_RATIO_DIST_CTRL,
	SWIPE_TIME_MIN_CTRL,
	SWIPE_TIME_MAX_CTRL,
	SWIPE_AREA_CTRL,
};

enum {
	IC_INIT_NEED = 0,
	IC_INIT_DONE,
};

enum {
	IC_BOOT_DONE = 0,
	IC_BOOT_FAIL,
};

enum {
	ATTN_ESD_EN				= (1U << 0),
	ATTN_WDOG_EN			= (1U << 1),
	ATTN_ABNORMAL_SPI_EN	= (1U << 2),
	/* */
	ABNORMAL_IC_DETECTION	= (ATTN_ESD_EN | ATTN_WDOG_EN),
};

#define MAX_RW_SIZE_POW			(10)
#define MAX_RW_SIZE			__SIZE_POW(MAX_RW_SIZE_POW)
#define FLASH_CONF_SIZE_POWER		(10)
#define FLASH_CONF_SIZE			(1 * __SIZE_POW(FLASH_CONF_SIZE_POWER))

#define FLASH_KEY_CODE_CMD		0xDFC1
#define FLASH_KEY_CONF_CMD		0xE87B
#define FLASH_BOOTCHK_VALUE		0x0A0A0000
#define FLASH_CODE_DNCHK_VALUE		0x42
#define FLASH_CONF_DNCHK_VALUE		0x84


enum {
	WAFER_TYPE_MASK = (0x07),
};

struct siw_hal_tc_version_bin {
	u8 major: 4;
	u8 build: 4;
	u8 minor;
	u16 rsvd: 12;
	u16 ext: 4;
};

struct siw_hal_tc_version {
	u8 minor;
	u8 major: 4;
	u8 build: 4;
	u8 chip;
	u8 protocol: 4;
	u8 ext: 4;
};

struct siw_hal_tc_version_ext_date {
	u8 rr;
	u8 dd;
	u8 mm;
	u8 yy;
};

struct siw_hal_fw_info {
	u32 chip_id_raw;
	u8 chip_id[8];
	/* */
	union {
		struct siw_hal_tc_version version;
		u32 version_raw;
	} v;
	u32 version_ext;
	/* */
	u8 product_id[8 + 4];
	u8 image_version[4];
	u8 image_product_id[8 + 4];
	u8 revision;
	u32 fpc;
	u32 wfr;
	u32 cg;
	u32 lot;
	u32 sn;
	u32 date;
	u32 time;
	u32 conf_index;
	/* __SIW_FW_TYPE_1 */
	u32 conf_idx_addr;
	u32 conf_dn_addr;
	u32 boot_code_addr;
	int conf_skip;
};

static inline void siw_hal_fw_set_chip_id(struct siw_hal_fw_info *fw,
					u32 chip_id)
{
	fw->chip_id_raw = chip_id;
	memset(fw->chip_id, 0, sizeof(fw->chip_id));
	fw->chip_id[0] = (chip_id >> 24) & 0xFF;
	fw->chip_id[1] = (chip_id >> 16) & 0xFF;
	fw->chip_id[2] = (chip_id >> 8) & 0xFF;
	fw->chip_id[3] = chip_id & 0xFF;
}

enum {
	VERSION_EXT_DATE = 1,
};

static inline int siw_hal_fw_chk_version_ext(u32 version_ext,
					u32 ext_flag)
{
	switch (ext_flag) {
	case VERSION_EXT_DATE: {
		struct siw_hal_tc_version_ext_date ext_date;

		memcpy(&ext_date, &version_ext, sizeof(ext_date));

		if (ext_date.yy &&
		    ext_date.mm &&
		    ext_date.dd) {

			if (ext_date.yy < 0x16)
				break;

			if (ext_date.mm > 0x12)
				break;

			if (ext_date.dd > 0x31)
				break;

			return 0;
		}
	}
	break;
	}

	return -EINVAL;
}

static inline void siw_hal_fw_set_version(struct siw_hal_fw_info *fw,
		u32 version, u32 version_ext)
{
	fw->v.version_raw = version;
	fw->version_ext = (fw->v.version.ext) ? version_ext : 0;
}

static inline void siw_hal_fw_set_revision(struct siw_hal_fw_info *fw,
					u32 revision)
{
	fw->revision = revision & 0xFF;
}

static inline void siw_hal_fw_set_prod_id(struct siw_hal_fw_info *fw,
					u8 *prod, u32 size)
{
	int len = min_t(int, sizeof(fw->product_id), size);

	memset(fw->product_id, 0, sizeof(fw->product_id));
	memcpy(fw->product_id, prod, len);
}


struct siw_hal_asc_info {
	u16 normal_s;
	u16 acute_s;
	u16 obtuse_s;
};

struct siw_hal_swipe_info {
	u8	distance;
	u8	ratio_thres;
	u8	ratio_distance;
	u8	ratio_period;
	u16	min_time;
	u16	max_time;
	struct active_area area;
};

struct siw_hal_swipe_ctrl {
	u32 mode;
	struct siw_hal_swipe_info info[2]; /* down is 0, up 1 - LG4894 use up */
};

enum {
	CHIP_REPORT_NONE = 0,
	CHIP_REPORT_TYPE_0,
	CHIP_REPORT_TYPE_1,
};

enum {
	CHIP_STATUS_NONE = 0,
	CHIP_STATUS_TYPE_0,
	CHIP_STATUS_TYPE_1,
	CHIP_STATUS_TYPE_2,
};

enum {
	STS_ID_NONE = 0,
	STS_ID_VALID_DEV_CTL,
	STS_ID_VALID_CODE_CRC,
	STS_ID_VALID_CFG_CRC,
	STS_ID_VALID_FONT_CRC,
	STS_ID_ERROR_ABNORMAL,
	STS_ID_ERROR_SYSTEM,
	STS_ID_ERROR_MISMTACH,
	STS_ID_VALID_IRQ_PIN,
	STS_ID_VALID_IRQ_EN,
	STS_ID_ERROR_MEM,
	STS_ID_VALID_TC_DRV,
	STS_ID_ERROR_DISP,
};

enum {
	STS_POS_VALID_DEV_CTL			= 5,
	STS_POS_VALID_CODE_CRC			= 6,
	STS_POS_VALID_CFG_CRC			= 7,
	STS_POS_VALID_FONT_CRC			= 8,
	STS_POS_ERROR_ABNORMAL			= 9,
	STS_POS_ERROR_SYSTEM			= 10,
	STS_POS_ERROR_MISMTACH			= 13,
	STS_POS_VALID_IRQ_PIN			= 15,
	STS_POS_VALID_IRQ_EN			= 20,
	STS_POS_ERROR_MEM				= 21,
	STS_POS_VALID_TC_DRV			= 22,
	STS_POS_ERROR_DISP				= 31,
	/* */
	STS_POS_VALID_CODE_CRC_TYPE_0	= 22,
};

struct siw_hal_status_mask_bit {
	u32 valid_dev_ctl;
	u32 valid_code_crc;
	u32 valid_cfg_crc;
	u32 error_abnormal;
	u32 error_system;
	u32 error_mismtach;
	u32 valid_irq_pin;
	u32 valid_irq_en;
	u32 error_mem;
	u32 valid_tv_drv;
	u32 error_disp;
};

struct siw_hal_status_filter {
	int id;
	u32 width;
	u32 pos;
	u32 flag;
#define _STS_FILTER_FLAG_TYPE_ERROR		(1<<0)
#define _STS_FILTER_FLAG_ESD_SEND		(1<<16)
#define _STS_FILTER_FLAG_CHK_FAULT		(1<<17)
	const char *str;
};

#define _STS_FILTER(_id, _width, _pos, _flag, _str)	\
	{ .id = _id, .width = _width, .pos = _pos, .flag = _flag, .str = _str, }

enum {
	STS_FILTER_FLAG_TYPE_ERROR		= _STS_FILTER_FLAG_TYPE_ERROR,
	STS_FILTER_FLAG_ESD_SEND		= _STS_FILTER_FLAG_ESD_SEND,
	STS_FILTER_FLAG_CHK_FAULT		= _STS_FILTER_FLAG_CHK_FAULT,
};

#define REG_LOG_MAX		8
#define REG_DIR_NONE	0
#define REG_DIR_RD		1
#define REG_DIR_WR		2
#define REG_DIR_ERR		(1<<8)
#define REG_DIR_MASK	(REG_DIR_ERR-1)

struct siw_hal_reg_log {
	int dir;
	u32 addr;
	u32 data;
};

struct siw_touch_chip_opt {
	u32 f_info_more:1;
	u32 f_ver_ext:1;
	u32 f_attn_opt:1;
	u32 f_glove_en:1;
	u32 f_grab_en:1;
	u32 f_dbg_report:1;
	u32 f_u2_blank_chg:1;
	u32 f_rsvd00:1;
	/* */
	u32 f_flex_report:1;
	u32 f_rsvd01:7;
	/* */
	u32 f_no_disp_sts:1;
	u32 f_no_sram_ctl:1;
	u32 f_rsvd02:6;
	/* */
	u32 f_rsvd03:8;
	/* */
	u32 t_boot_mode:4;
	u32 t_sts_mask:4;
	u32 t_chk_mode:4;
	u32 t_sw_rst:4;
	u32 t_clock:4;
	u32 t_chk_mipi:4;
	u32 t_chk_frame:4;
	u32 t_chk_tci_debug:4;
	/* */
	u32 t_chk_sys_error:4;
	u32 t_chk_sys_fault:4;
	u32 t_chk_fault:4;
	u32 rsvd21:4;
	u32 rsvd22:8;
	u32 rsvd23:8;
};

enum {
	HAL_DBG_GRP_0 = 0,
	HAL_DBG_GRP_MAX,
};

enum {
	HAL_DBG_DLY_TC_DRIVING_0 = 0,
	HAL_DBG_DLY_TC_DRIVING_1,
	HAL_DBG_DLY_FW_0,
	HAL_DBG_DLY_FW_1,
	HAL_DBG_DLY_FW_2,
	HAL_DBG_DLY_HW_RST_0,
	HAL_DBG_DLY_HW_RST_1,
	HAL_DBG_DLY_HW_RST_2,
	HAL_DBG_DLY_SW_RST_0,
	HAL_DBG_DLY_SW_RST_1,
	HAL_DBG_DLY_NOTIFY,
	HAL_DBG_DLY_LPWG,
	HAL_DBG_DLY_MAX,
};

struct siw_hal_debug {
	/* group 0 : delay */
	u32 delay[HAL_DBG_DLY_MAX];
	/* group 1 : rsvd */
	/* group 2 : rsvd */
	/* group 3 : rsvd */
};

struct siw_touch_chip {
	void *ts;
	struct siw_touch_chip_opt opt;
	struct siw_hal_reg *reg;
	struct device *dev;
	struct kobject kobj;
	struct siw_hal_touch_info info;
	struct siw_hal_fw_info fw;
	struct siw_hal_asc_info asc;
	struct siw_hal_swipe_ctrl swipe;
	/* */
	int report_type;
	int status_type;
	u32 status_mask;
	u32 status_mask_normal;
	u32 status_mask_logging;
	u32 status_mask_reset;
	u32 status_mask_ic_abnormal;
	u32 status_mask_ic_error;
	u32 status_mask_ic_valid;
	u32 status_mask_ic_disp_err;
	struct siw_hal_status_mask_bit status_mask_bit;
	struct siw_hal_status_filter *status_filter;
	/* */
	int mode_allowed_partial;
	int mode_allowed_qcover;
	int drv_reset_low;
	int drv_delay;
	int drv_opt_delay;
	u8 prev_lcd_mode;
	u8 lcd_mode;
	u8 driving_mode;
	u8 u3fake;
	void *watch;
	struct mutex bus_lock;
	struct delayed_work font_download_work;
	struct delayed_work fb_notify_work;
	u32 charger;
	u32 earjack;
	u8 tci_debug_type;
	u8 swipe_debug_type;
	atomic_t block_watch_cfg;
	atomic_t init;
	atomic_t boot;
	atomic_t esd_noti_sent;
	int boot_fail_cnt;
#if defined(__SIW_SUPPORT_PM_QOS)
	struct pm_qos_request pm_qos_req;
#endif
	struct siw_hal_reg_log reg_log[REG_LOG_MAX];
	struct siw_hal_debug dbg;
	int fw_abs_path;
};

static inline int hal_dbg_delay(struct siw_touch_chip *chip, int index)
{
	return (index < HAL_DBG_DLY_MAX) ? chip->dbg.delay[index] : 0;
}

enum {
	BOOT_FAIL_RECOVERY_MAX = 3,	/* to avoid infinite repetition */
};

enum {
	CHIP_INIT_RETRY_PROBE = 2,
	CHIP_INIT_RETRY_MAX = 5,
};

#define TCI_MAX_NUM				2
#define SWIPE_MAX_NUM				2
#define TCI_DEBUG_MAX_NUM			16
#define SWIPE_DEBUG_MAX_NUM			8
#define DISTANCE_INTER_TAP			__SIZE_POW(1)	/* 2 */
#define DISTANCE_TOUCHSLOP			__SIZE_POW(2)	/* 4 */
#define TIMEOUT_INTER_TAP_LONG			__SIZE_POW(3)	/* 8 */
#define MULTI_FINGER				__SIZE_POW(4)	/* 16 */
#define DELAY_TIME				__SIZE_POW(5)	/* 32 */
#define TIMEOUT_INTER_TAP_SHORT			__SIZE_POW(6)	/* 64 */
#define PALM_STATE				__SIZE_POW(7)	/* 128 */
#define TAP_TIMEOVER				__SIZE_POW(8)	/* 256 */

#define TCI_DEBUG_ALL		(0 | \
				DISTANCE_INTER_TAP |	\
				DISTANCE_TOUCHSLOP | \
				TIMEOUT_INTER_TAP_LONG | \
				MULTI_FINGER | \
				DELAY_TIME | \
				TIMEOUT_INTER_TAP_SHORT | \
				PALM_STATE | \
				TAP_TIMEOVER | \
				0)


static inline struct siw_touch_chip *to_touch_chip(struct device *dev)
{
	return (struct siw_touch_chip *)touch_get_dev_data(to_touch_core(dev));
}

static inline struct siw_touch_chip *to_touch_chip_from_kobj
(struct kobject *kobj)
{
	return (struct siw_touch_chip *)container_of(kobj,
			struct siw_touch_chip, kobj);
}

enum {
	ADDR_SKIP_MASK  = 0xFFFF,
};

extern int siw_hal_read_value(struct device *dev, u32 addr, u32 *value);
extern int siw_hal_write_value(struct device *dev, u32 addr, u32 value);
extern int siw_hal_reg_read(struct device *dev, u32 addr, void *data,
				int size);
extern int siw_hal_reg_write(struct device *dev, u32 addr, void *data,
				int size);
extern int siw_hal_read_value_chk(struct device *dev, u32 addr, u32 *value);
extern int siw_hal_write_value_chk(struct device *dev, u32 addr, u32 value);
extern int siw_hal_reg_read_chk(struct device *dev, u32 addr, void *data,
				int size);
extern int siw_hal_reg_write_chk(struct device *dev, u32 addr, void *data,
				int size);
extern void siw_hal_xfer_init(struct device *dev, void *xfer_data);
extern int siw_hal_xfer_msg(struct device *dev,
				struct touch_xfer_msg *xfer);
extern void siw_hal_xfer_add_rx(void *xfer_data, u32 reg, void *buf,
				u32 size);
extern void siw_hal_xfer_add_tx(void *xfer_data, u32 reg, void *buf,
				u32 size);

extern int siw_hal_ic_test_unit(struct device *dev, u32 data);

extern struct siw_touch_operations *siw_hal_get_default_ops(int opt);

#endif	/* __SIW_TOUCH_HAL_H */


