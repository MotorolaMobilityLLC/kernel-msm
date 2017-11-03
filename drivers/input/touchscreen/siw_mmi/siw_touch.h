/*
 * SiW touch core driver
 *
 * Copyright (C) 2016 Silicon Works - http://www.siliconworks.co.kr
 * Author: Hyunho Kim <kimhh@siliconworks.co.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#ifndef __SIW_TOUCH_H
#define __SIW_TOUCH_H

#include "siw_touch_cfg.h"

#include <linux/kernel.h>
#include <linux/async.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/string.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/notifier.h>
#include <linux/atomic.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/fs.h>
#include <linux/slab.h>

#if defined(__SIW_CONFIG_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif

#if defined(__SIW_CONFIG_FB)
#include <linux/fb.h>
#endif

#if defined(__SIW_SUPPORT_WAKE_LOCK)
#include <linux/wakelock.h>
#endif

#include <linux/input/siw_touch_notify.h>

#include "siw_touch_hal_reg.h"

#include "siw_touch_dbg.h"


#define SIW_DRV_VERSION		"v2.20b"


enum {
	SIW_TOUCH_NORMAL_MODE	= 0,
	SIW_TOUCH_CHARGER_MODE	= 1,
};

enum {
	SIW_TOUCH_MAX_BUF_SIZE		= (32<<10),
	SIW_TOUCH_MAX_BUF_IDX		= 4,
	/* */
	SIW_TOUCH_MAX_XFER_COUNT	= 10,
};

enum _SIW_BUS_IF {
	BUS_IF_I2C = 0,
	BUS_IF_SPI,
	BUS_IF_MAX,
};

enum {
	DRIVER_FREE = 0,
	DRIVER_INIT = 1,
};

enum {
	POWER_OFF = 0,
	POWER_SLEEP,
	POWER_WAKE,
	POWER_ON,
	POWER_SLEEP_STATUS,
	POWER_HW_RESET,
};

enum {
	UEVENT_IDLE = 0,
	UEVENT_BUSY,
};

enum {
	PROX_NEAR = 0,
	PROX_FAR,
};

enum {
	HOLE_FAR = 0,
	HOLE_NEAR,
};

enum {
	DEV_PM_RESUME = 0,
	DEV_PM_SUSPEND,
	DEV_PM_SUSPEND_IRQ,
};

enum {
	FB_RESUME = 0,
	FB_SUSPEND,
};

enum {
	SP_DISCONNECT = 0,
	SP_CONNECT,
};

/* Deep Sleep or not */
enum {
	IC_NORMAL = 0,
	IC_DEEP_SLEEP,
};

/* TCI */
enum {
	ENABLE_CTRL = 0,
	TAP_COUNT_CTRL,
	MIN_INTERTAP_CTRL,
	MAX_INTERTAP_CTRL,
	TOUCH_SLOP_CTRL,
	TAP_DISTANCE_CTRL,
	INTERRUPT_DELAY_CTRL,
	ACTIVE_AREA_CTRL,
	ACTIVE_AREA_RESET_CTRL,
	ACTIVE_AREA_QOPEN_CTRL,
	ACTIVE_AREA_QCLOSE_CTRL,
};

enum {
	LPWG_NONE = 0,
	LPWG_DOUBLE_TAP,
	LPWG_PASSWORD,
};

enum {
	LPWG_SLEEP = 0,
	LPWG_WAKE,
	LPWG_KNOCK,
	LPWG_PASSWD,
	LPWG_PARTIAL,
	LPWG_NORMAL,
};

enum {
	LPWG_CMD_MODE = 0,
	LPWG_CMD_AREA,
};

enum {
	TCI_1 = 0,
	TCI_2,
};

enum {
	LPWG_ENABLE = 1,
	LPWG_LCD,
	LPWG_ACTIVE_AREA,
	LPWG_TAP_COUNT,
	LPWG_LENGTH_BETWEEN_TAP,
	LPWG_EARLY_SUSPEND,
	LPWG_SENSOR_STATUS,
	LPWG_DOUBLE_TAP_CHECK,
	LPWG_UPDATE_ALL,
	LPWG_READ,
	LPWG_REPLY,
	LPWG_EXT_TCI_INFO_STORE = 0x80,
	LPWG_EXT_TCI_QOPEN_AREA_STORE,
	LPWG_EXT_TCI_QCLOSE_AREA_STORE,
	LPWG_EXT_SWIPE_INFO_STORE,
	LPWG_EXT_STORE_END,
	LPWG_EXT_TCI_INFO_SHOW = 0x90,
	LPWG_EXT_TCI_AREA_SHOW,
	LPWG_EXT_SWIPE_INFO_SHOW,
	LPWG_EXT_SHOW_END,
};

enum {
	TCON_GLOVE = 1,
	TCON_GRAB,
};

enum {
	LOCKSCREEN_UNLOCK = 0,
	LOCKSCREEN_LOCK,
};

enum {
	IME_OFF = 0,
	IME_ON,
	IME_SWYPE,
};

enum {
	QUICKCOVER_OPEN = 0,
	QUICKCOVER_CLOSE,
};

enum {
	INCOMING_CALL_IDLE,
	INCOMING_CALL_RINGING,
	INCOMING_CALL_OFFHOOK,
};

enum {
	MFTS_NONE = 0,
	MFTS_FOLDER,
	MFTS_FLAT,
	MFTS_CURVED,
};

enum { /* Command lists */
	CMD_VERSION,
	CMD_ATCMD_VERSION,
};

enum {
	INTERRUPT_DISABLE = 0,
	INTERRUPT_ENABLE,
};

enum {
	CORE_NONE = 0,
	CORE_EARLY_PROBE,
	CORE_PROBE,
	CORE_CHARGER_LOGO,
	CORE_MFTS,
	CORE_UPGRADE,
	CORE_NORMAL,
};

enum {
	CONNECT_INVALID = 0,
	CONNECT_SDP,
	CONNECT_DCP,
	CONNECT_CDP,
	CONNECT_PROPRIETARY,
	CONNECT_FLOATED,
	CONNECT_HUB, /* SHOULD NOT change the value */
};

enum {
	CONNECT_CHARGER_UNKNOWN = 0,
	CONNECT_STANDARD_HOST,		/* USB : 450mA */
	CONNECT_CHARGING_HOST,
	CONNECT_NONSTANDARD_CHARGER,	/* AC : 450mA~1A */
	CONNECT_STANDARD_CHARGER,	/* AC : ~1A */
	CONNECT_APPLE_2_1A_CHARGER,	/* 2.1A apple charger */
	CONNECT_APPLE_1_0A_CHARGER,	/* 1A apple charger */
	CONNECT_APPLE_0_5A_CHARGER,	/* 0.5A apple charger */
	CONNECT_WIRELESS_CHARGER,
	CONNECT_DISCONNECTED,
};

enum {
	EARJACK_NONE = 0,
	EARJACK_NORMAL,
	EARJACK_DEBUG,
};

enum {
	DEBUG_TOOL_DISABLE = 0,
	DEBUG_TOOL_ENABLE,
	DEBUG_TOOL_MAX,
};

enum {
	DEBUG_OPTION_DISABLE	= 0,
	DEBUG_OPTION_1		= 1,
	DEBUG_OPTION_2		= 2,
	DEBUG_OPTION_1_2	= 3,
	DEBUG_OPTION_3		= 4,
	DEBUG_OPTION_1_3	= 5,
	DEBUG_OPTION_2_3	= 6,
	DEBUG_OPTION_1_2_3	= 7,
	DEBUG_OPTION_4		= 8,
	DEBUG_OPTION_1_4	= 9,
	DEBUG_OPTION_2_4	= 10,
	DEBUG_OPTION_1_2_4	= 11,
	DEBUG_OPTION_3_4	= 12,
	DEBUG_OPTION_1_3_4	= 13,
	DEBUG_OPTION_2_3_4	= 14,
	DEBUG_OPTION_1_2_3_4	= 15,
};

enum {
	ASC_OFF = 0,
	ASC_ON,
};

enum {
	IN_HAND_ATTN = 0,
	NOT_IN_HAND,
	IN_HAND_NO_ATTN,
};

enum {
	ASC_READ_MAX_DELTA = 0,
	ASC_GET_FW_SENSITIVITY,
	ASC_WRITE_SENSITIVITY,
};

enum {
	DELTA_CHK_OFF = 0,
	DELTA_CHK_ON,
};

enum {
	NORMAL_SENSITIVITY = 0,
	ACUTE_SENSITIVITY,
	OBTUSE_SENSITIVITY,
};

enum {
	NORMAL_BOOT = 0,
	MINIOS_AAT,
	MINIOS_MFTS_FOLDER,
	MINIOS_MFTS_FLAT,
	MINIOS_MFTS_CURVED,
};

struct state_info {
	atomic_t core;
	atomic_t pm;
	atomic_t fb;
	atomic_t sleep;
	atomic_t uevent;
	atomic_t irq_enable;
	atomic_t connect; /* connection using USB port */
	atomic_t wireless; /* connection using wirelees_charger */
	atomic_t earjack; /* connection using earjack */
	atomic_t lockscreen;
	atomic_t ime;
	atomic_t quick_cover;
	atomic_t incoming_call;
	atomic_t mfts;
	atomic_t sp_link;
	atomic_t debug_tool;
	atomic_t debug_option_mask;
	atomic_t onhand;
	atomic_t hw_reset;
	atomic_t mon_ignore;
	atomic_t glove;
	atomic_t grab;
};

struct touch_pins {
	int reset_pin;
	int reset_pin_pol;
	int irq_pin;
	int maker_id_pin;
	int vdd_pin;
	int vio_pin;
	/* */
	void *vdd;
	void *vio;
};

struct touch_device_caps {
	u32 max_x;
	u32 max_y;
	u32 max_pressure;
	u32 max_width;
	u32 max_orientation;
	u32 max_id;
	u32 hw_reset_delay;
	u32 sw_reset_delay;
};

struct touch_operation_role {
	bool use_lpwg;
	bool use_firmware;
	bool use_fw_upgrade;
	u32 use_lpwg_test;
	u32 mfts_lpwg;
};

struct touch_quick_cover {
	u32 x1;
	u32 y1;
	u32 x2;
	u32 y2;
};

struct touch_pinctrl {
	struct pinctrl *ctrl;
	struct pinctrl_state *active;
	struct pinctrl_state *suspend;
};

struct touch_data {
	u16 id;
	u16 x;
	u16 y;
	u16 width_major;
	u16 width_minor;
	s16 orientation;
	u16 pressure;
	/* finger, palm, pen, glove, hover */
	u16 type;
	u16 event;
};

struct point {
	int x;
	int y;
};

struct lpwg_info {
	u8 mode;
	u8 screen;
	u8 sensor;
	u8 qcover;
	u8 code_num;
	struct point area[2];
	struct point code[MAX_LPWG_CODE];
};

struct active_area {
	u16 x1;
	u16 y1;
	u16 x2;
	u16 y2;
};

struct reset_area {
	u32 x1;
	u32 y1;
	u32 x2;
	u32 y2;
};

struct tci_info {
	u16 tap_count;
	u16 min_intertap;
	u16 max_intertap;
	u16 touch_slop;
	u16 tap_distance;
	u16 intr_delay;
};

struct tci_ctrl {
	u32 mode;
	struct active_area area;
	struct reset_area qcover_open;
	struct reset_area qcover_close;
	u8 double_tap_check;
	struct tci_info info[2];
};

struct asc_info {
	u32	use_asc;
	u8	curr_sensitivity;
	bool use_delta_chk;
	bool delta_updated;
	u32	delta;
	u32	low_delta_thres;
	u32	high_delta_thres;
};

typedef int (*siw_mon_handler_t)(struct device *dev, u32 opt);

struct siw_touch_operations {
	/* Register Map */
	struct siw_hal_reg *reg;
	/* Func. */
	int (*early_probe)(struct device *dev);
	int (*probe)(struct device *dev);
	int (*remove)(struct device *dev);
	int (*suspend)(struct device *dev);
	int (*resume)(struct device *dev);
	int (*init)(struct device *dev);
	int (*reset)(struct device *dev, int ctrl);
	int (*ic_info)(struct device *dev);
	int (*tc_con)(struct device *dev,	u32 code, void *param);
	int (*tc_driving)(struct device *dev, int mode);
	int (*chk_status)(struct device *dev);
	int (*irq_handler)(struct device *dev);
	int (*irq_abs)(struct device *dev);
	int (*irq_lpwg)(struct device *dev);
	int (*power)(struct device *dev, int power_mode);
	int (*upgrade)(struct device *dev);
	int (*lpwg)(struct device *dev,	u32 code, void *param);
	int (*asc)(struct device *dev, u32 code, u32 value);
	int (*notify)(struct device *dev, ulong event, void *data);
	int (*set)(struct device *dev, u32 cmd, void *buf);
	int (*get)(struct device *dev, u32 cmd, void *buf);
	/* */
	int (*sysfs)(struct device *dev, int on_off);
	/* */
	siw_mon_handler_t mon_handler;
	int mon_interval;
	/* */
	int (*abt_sysfs)(struct device *dev, int on_off);
	int (*prd_sysfs)(struct device *dev, int on_off);
	int (*watch_sysfs)(struct device *dev, int on_off);
};

enum {
	MON_FLAG_RST_ONLY	= (1<<8),
};

struct siw_touch_bus_info {
	u32 bus_type;
	u32 buf_size;
	/* */
	u32 spi_mode;
	u32 bits_per_word;
	u32 max_freq;
	/* */
	int	bus_tx_hdr_size;
	int	bus_rx_hdr_size;
	int bus_tx_dummy_size;
	int bus_rx_dummy_size;
};

struct siw_touch_buf {
	u8 *buf;
	dma_addr_t dma;
	int size;
};

struct siw_touch_second_screen {
	int bound_i;
	int bound_j;
};

struct siw_touch_fquirks {
	int (*parse_dts)(void *data);
	/* */
	int (*gpio_init)(struct device *dev, int pin, const char *name);
	int (*gpio_free)(struct device *dev, int pin);
	int (*gpio_dir_input)(struct device *dev, int pin);
	int (*gpio_dir_output)(struct device *dev, int pin, int value);
	int (*gpio_set_pull)(struct device *dev, int pin, int value);
	/* */
	int (*power_init)(struct device *dev);
	int (*power_free)(struct device *dev);
	int (*power_vdd)(struct device *dev, int value);
	int (*power_vio)(struct device *dev, int value);
	/* */
	int (*fwup_check)(struct device *dev, u8 *fw_buf);
	int (*fwup_upgrade)(struct device *dev, u8 *fw_buf,
		int fw_size, int retry);
	/* */
	int (*mon_handler)(struct device *dev, u32 opt);
	int mon_interval;
};

enum {
	TOUCH_IRQ_NONE			= 0,
	TOUCH_IRQ_FINGER		= (1 << 0),
	TOUCH_IRQ_KNOCK			= (1 << 1),
	TOUCH_IRQ_PASSWD		= (1 << 2),
	TOUCH_IRQ_SWIPE_RIGHT		= (1 << 3),
	TOUCH_IRQ_SWIPE_LEFT		= (1 << 4),
	TOUCH_IRQ_GESTURE		= (1 << 5),
	TOUCH_IRQ_ERROR			= (1 << 15),
};

enum {
	ABS_MODE = 0,
	KNOCK_1,
	KNOCK_2,
	SWIPE_RIGHT,
	SWIPE_LEFT,
	/* */
	GESTURE_C,
	GESTURE_W,
	GESTURE_V,
	GESTURE_O,
	GESTURE_S,
	GESTURE_E,
	GESTURE_M,
	GESTURE_Z,
	/* */
	GESTURE_DIR_RIGHT = 20,
	GESTURE_DIR_DOWN,
	GESTURE_DIR_LEFT,
	GESTURE_DIR_UP,
	GESTURE_FAIL = 100,
	/* */
	CUSTOM_DEBUG = 200,
	KNOCK_OVERTAP,
};

enum _SIW_TOUCH_UEVENT {
	TOUCH_UEVENT_KNOCK = 0,
	TOUCH_UEVENT_PASSWD,
	TOUCH_UEVENT_SWIPE_RIGHT,
	TOUCH_UEVENT_SWIPE_LEFT,
	TOUCH_UEVENT_GESTURE_C,
	TOUCH_UEVENT_GESTURE_W,
	TOUCH_UEVENT_GESTURE_V,
	TOUCH_UEVENT_GESTURE_O,
	TOUCH_UEVENT_GESTURE_S,
	TOUCH_UEVENT_GESTURE_E,
	TOUCH_UEVENT_GESTURE_M,
	TOUCH_UEVENT_GESTURE_Z,
	TOUCH_UEVENT_GESTURE_DIR_RIGHT,
	TOUCH_UEVENT_GESTURE_DIR_DOWN,
	TOUCH_UEVENT_GESTURE_DIR_LEFT,
	TOUCH_UEVENT_GESTURE_DIR_UP,
	TOUCH_UEVENT_MAX,
};

struct siw_touch_uevent_ctrl {
	char *str[TOUCH_UEVENT_MAX][2];
	u32 flags;
};

struct siw_touch_fw_bin {
	u8 *fw_data;
	int fw_size;
};

struct siw_touch_font_bin {
	u8 *font_data;
	int font_size;
};

struct siw_touch_pdata {
	/* Config. */
	char *chip_id;
	char *chip_name;
	char *drv_name;
	char *idrv_name;
	char *ext_watch_name;
	struct module *owner;
	u32 max_finger;
	u32 chip_type;
	u32 mode_allowed;
	u32 fw_size;

	u32 flags;
	unsigned long irqflags;

	unsigned long quirks;
#define _CHIP_QUIRK_NOT_SUPPORT_XFER		(1L<<0)

#define _CHIP_QUIRK_NOT_SUPPORT_ASC		(1L<<16)
#define _CHIP_QUIRK_NOT_SUPPORT_LPWG		(1L<<17)
#define _CHIP_QUIRK_NOT_SUPPORT_WATCH		(1L<<18)

#define _CHIP_QUIRK_NOT_SUPPORT_IME		(1L<<28)

	unsigned long abt_quirks;
#define _ABT_QUIRK_RAW_RETURN_MODE_VAL		(1L<<0)

	unsigned long prd_quirks;
#define _PRD_QUIRK_RAW_RETURN_MODE_VAL		(1L<<0)

	struct siw_touch_bus_info bus_info;

#if defined(__SIW_CONFIG_OF)
	const struct of_device_id *of_match_table;
#else
	struct touch_pins pins;
	struct touch_device_caps caps;
#endif

	/* Input Device ID */
	struct input_id i_id;

	/* Hal operations */
	struct siw_touch_operations *ops;

	void *tci_info;
	void *tci_qcover_open;
	void *tci_qcover_close;
	void *swipe_ctrl;
	void *watch_win;

	void *reg_quirks;

	struct siw_touch_fquirks fquirks;

	void *uevent_ctrl;

	void *fw_bin;

	void *font_bin;

	int senseless_margin;
};

struct siw_touch_chip_data {
	const struct siw_touch_pdata *pdata;
	void *bus_drv;
};

enum {
	CHIP_QUIRK_NOT_SUPPORT_XFER	= _CHIP_QUIRK_NOT_SUPPORT_XFER,
	/* */
	CHIP_QUIRK_NOT_SUPPORT_ASC	= _CHIP_QUIRK_NOT_SUPPORT_ASC,
	CHIP_QUIRK_NOT_SUPPORT_LPWG	= _CHIP_QUIRK_NOT_SUPPORT_LPWG,
	CHIP_QUIRK_NOT_SUPPORT_WATCH	= _CHIP_QUIRK_NOT_SUPPORT_WATCH,
	/* */
	CHIP_QUIRK_NOT_SUPPORT_IME	= _CHIP_QUIRK_NOT_SUPPORT_IME,
};

enum {
	ABT_QUIRK_RAW_RETURN_MODE_VAL	= _ABT_QUIRK_RAW_RETURN_MODE_VAL,
};

enum {
	PRD_QUIRK_RAW_RETURN_MODE_VAL	= _PRD_QUIRK_RAW_RETURN_MODE_VAL,
};

static inline unsigned long pdata_get_quirks(struct siw_touch_pdata *pdata)
{
	return pdata->quirks;
}

static inline unsigned long pdata_get_abt_quirks(struct siw_touch_pdata *pdata)
{
	return pdata->abt_quirks;
}

static inline unsigned long pdata_get_prd_quirks(struct siw_touch_pdata *pdata)
{
	return pdata->prd_quirks;
}

static inline unsigned long pdata_test_quirks(struct siw_touch_pdata *pdata,
			unsigned long quirk_bit)
{
	return (pdata_get_quirks(pdata) & quirk_bit);
}

static inline unsigned long pdata_test_abt_quirks(struct siw_touch_pdata *pdata,
			unsigned long quirk_bit)
{
	return (pdata_get_abt_quirks(pdata) & quirk_bit);
}

static inline unsigned long pdata_test_prd_quirks(struct siw_touch_pdata *pdata,
			unsigned long quirk_bit)
{
	return (pdata_get_prd_quirks(pdata) & quirk_bit);
}

static inline char *pdata_chip_id(struct siw_touch_pdata *pdata)
{
	return pdata->chip_id;
}

static inline char *pdata_chip_name(struct siw_touch_pdata *pdata)
{
	return pdata->chip_name;
}

static inline char *pdata_drv_name(struct siw_touch_pdata *pdata)
{
	return pdata->drv_name;
}

static inline char *pdata_idrv_name(struct siw_touch_pdata *pdata)
{
	return pdata->idrv_name;
}

static inline char *pdata_ext_watch_name(struct siw_touch_pdata *pdata)
{
	return pdata->ext_watch_name;
}

static inline u32 pdata_max_finger(struct siw_touch_pdata *pdata)
{
	return (pdata->max_finger) ? pdata->max_finger : MAX_FINGER;
}

static inline u32 pdata_chip_type(struct siw_touch_pdata *pdata)
{
	return pdata->chip_type;
}

static inline u32 pdata_mode_allowed(struct siw_touch_pdata *pdata, u32 mode)
{
	return (pdata->mode_allowed & BIT(mode));
}

static inline u32 pdata_fw_size(struct siw_touch_pdata *pdata)
{
	return pdata->fw_size;
}

static inline u32 pdata_flags(struct siw_touch_pdata *pdata)
{
	return pdata->flags;
}

static inline unsigned long pdata_irqflags(struct siw_touch_pdata *pdata)
{
	return pdata->irqflags;
}

static inline u32 pdata_bus_type(struct siw_touch_pdata *pdata)
{
	return pdata->bus_info.bus_type;
}

static inline u32 pdata_buf_size(struct siw_touch_pdata *pdata)
{
	return pdata->bus_info.buf_size;
}

static inline u32 pdata_spi_mode(struct siw_touch_pdata *pdata)
{
	return pdata->bus_info.spi_mode;
}

static inline u32 pdata_bits_per_word(struct siw_touch_pdata *pdata)
{
	return pdata->bus_info.bits_per_word;
}

static inline u32 pdata_max_freq(struct siw_touch_pdata *pdata)
{
	return pdata->bus_info.max_freq;
}

static inline int pdata_tx_hdr_size(struct siw_touch_pdata *pdata)
{
	return pdata->bus_info.bus_tx_hdr_size;
}

static inline int pdata_rx_hdr_size(struct siw_touch_pdata *pdata)
{
	return pdata->bus_info.bus_rx_hdr_size;
}

static inline int pdata_tx_dummy_size(struct siw_touch_pdata *pdata)
{
	return pdata->bus_info.bus_tx_dummy_size;
}

static inline int pdata_rx_dummy_size(struct siw_touch_pdata *pdata)
{
	return pdata->bus_info.bus_rx_dummy_size;
}

static inline void *pdata_tci_info(struct siw_touch_pdata *pdata)
{
	return pdata->tci_info;
}

static inline void *pdata_tci_qcover_open(struct siw_touch_pdata *pdata)
{
	return pdata->tci_qcover_open;
}

static inline void *pdata_tci_qcover_close(struct siw_touch_pdata *pdata)
{
	return pdata->tci_qcover_close;
}

static inline void *pdata_swipe_ctrl(struct siw_touch_pdata *pdata)
{
	return pdata->swipe_ctrl;
}

static inline void *pdata_watch_win(struct siw_touch_pdata *pdata)
{
	return pdata->watch_win;
}

#if defined(__SIW_SUPPORT_XFER)
static inline int pdata_xfer_allowed(struct siw_touch_pdata *pdata)
{
	if (pdata_bus_type(pdata) != BUS_IF_SPI)
		return 0;

	if (pdata_test_quirks(pdata, CHIP_QUIRK_NOT_SUPPORT_XFER))
		return 0;

	return 1;
}
#else	/* __SIW_SUPPORT_XFER */
static inline int pdata_xfer_allowed(struct siw_touch_pdata *pdata)
{
	return 0;
}
#endif	/* __SIW_SUPPORT_XFER */

static inline struct siw_touch_fquirks *pdata_fquirks(
						struct siw_touch_pdata *pdata)
{
	return &pdata->fquirks;
}

static inline struct siw_touch_uevent_ctrl *pdata_uevent_ctrl(
						struct siw_touch_pdata *pdata)
{
	return pdata->uevent_ctrl;
}

static inline struct siw_touch_fw_bin *pdata_fw_bin(
						struct siw_touch_pdata *pdata)
{
	return pdata->fw_bin;
}

static inline struct siw_touch_font_bin *pdata_font_bin(
						struct siw_touch_pdata *pdata)
{
	return pdata->font_bin;
}

static inline int pdata_senseless_margin(struct siw_touch_pdata *pdata)
{
	return pdata->senseless_margin;
}

enum {
	TS_THREAD_OFF = 0,
	TS_THREAD_ON,
	TS_THREAD_PAUSE,
};

struct siw_ts_thread {
	struct task_struct *thread;
	atomic_t state;
	int interval;
	siw_mon_handler_t handler;
	struct mutex lock;
};

enum {
	DEFAULT_NAME_SZ = PATH_MAX,
};

struct siw_ts {
	size_t addr;

	char *chip_id;
	char *chip_name;
	char *drv_name;
	char *idrv_name;
	char *ext_watch_name;
	int max_finger;
	int chip_type;

	int irq;
	unsigned long irqflags;
	unsigned long irqflags_curr;
	irq_handler_t handler_fn;
	irq_handler_t thread_fn;
	struct delayed_work work_irq;

	void *bus_dev;
	struct device *dev;
	void *dev_data;

	struct device udev;
	struct bus_type ubus;

	struct input_dev *input;
	struct siw_touch_pdata *pdata;

	struct kobject kobj;
#if defined(__SIW_SUPPORT_WAKE_LOCK)
	struct wake_lock lpwg_wake_lock;
#endif
	struct state_info state;

	struct touch_pins pins;
	struct touch_device_caps caps;
	struct touch_operation_role role;
	struct touch_quick_cover qcover;
	struct touch_pinctrl pinctrl;

	u32 intr_status;
	u32 intr_gesture;
	u16 new_mask;
	u16 old_mask;
	int tcount;
	struct touch_data tdata[MAX_FINGER];
	int is_palm;
	struct lpwg_info lpwg;
	struct tci_ctrl tci;
	struct asc_info asc;

	int def_fwcnt;
	const char *def_fwpath[4];
	char test_fwpath[DEFAULT_NAME_SZ];
	const char *panel_spec;
	const char *panel_spec_mfts;
	u32 force_fwup;
#define _FORCE_FWUP_CLEAR		0
#define _FORCE_FWUP_ON			(1<<0)
#define _FORCE_FWUP_SYS_SHOW	(1<<2)
#define _FORCE_FWUP_SYS_STORE	(1<<3)

	/* __SIW_SUPPORT_WATCH */
	const char *watch_font_image;
	/* */

	/* __SIW_SUPPORT_PRD */
	const char *prd_in_file_path;
	const char *prd_in_file_m_path;
	const char *prd_out_file_path;
	const char *prd_out_file_mo_aat_path;
	const char *prd_out_file_mo_mfo_path;
	const char *prd_out_file_mo_mfl_path;
	const char *prd_out_file_mo_mcv_path;
	/* */

	int buf_size;
	struct touch_xfer_msg *xfer;
	struct siw_touch_buf tx_buf[SIW_TOUCH_MAX_BUF_IDX];
	struct siw_touch_buf rx_buf[SIW_TOUCH_MAX_BUF_IDX];
	int tx_buf_idx;
	int rx_buf_idx;

	struct mutex lock;
	struct mutex reset_lock;
	struct mutex probe_lock;
	struct workqueue_struct *wq;
	struct delayed_work init_work;
	struct delayed_work upgrade_work;
	struct delayed_work notify_work;
	struct delayed_work fb_work;
	struct delayed_work toggle_delta_work;
	struct delayed_work finger_input_work;
	struct delayed_work sys_reset_work;

	struct notifier_block blocking_notif;
	struct notifier_block atomic_notif;
	unsigned long notify_event;
	int notify_data;
#if defined(__SIW_CONFIG_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
#if defined(__SIW_CONFIG_FB)
	struct notifier_block fb_notif;
#endif

	struct siw_ts_thread mon_thread;

	int vdd_id;
	int vdd_vol;
	int vio_id;
	int vio_vol;

	void *abt;
	void *prd;

	u32 flags;
#define _IRQ_USE_WAKE			(1UL<<0)
#define _IRQ_USE_SCHEDULE_WORK		(1UL<<1)

#define _TOUCH_USE_MON_THREAD		(1UL<<8)
#define _TOUCH_USE_PINCTRL		(1UL<<9)
#define _TOUCH_USE_PWRCTRL		(1UL<<10)

#define _TOUCH_USE_VBLANK		(1UL<<12)

#define _TOUCH_USE_INPUT_PARENT		(1UL<<15)

#define _TOUCH_USE_VIRT_DIR_WATCH	(1UL<<16)
#define _TOUCH_USE_DRV_NAME_SYSFS	(1UL<<17)
#define _TOUCH_USE_FW_BINARY		(1UL<<18)
#define _TOUCH_USE_FONT_BINARY		(1UL<<19)

#define _TOUCH_USE_PROBE_INIT_LATE	(1UL<<24)

#define _TOUCH_SKIP_ESD_EVENT		(1UL<<28)
#define _TOUCH_SKIP_RESET_PIN		(1UL<<29)

#define _TOUCH_IGNORE_DT_FLAGS		(1UL<<31)

	/* Input Device ID */
	struct input_id i_id;

	/* Hal operations */
	struct siw_touch_operations *ops_ext;
	struct siw_touch_operations ops_in;
	struct siw_touch_operations *ops;
	struct siw_hal_reg __reg;

	/* */
	int (*bus_init)(struct device *dev);
	int (*bus_read)(struct device *dev, void *msg);
	int (*bus_write)(struct device *dev, void *msg);
	int (*bus_xfer)(struct device *dev, void *xfer);
	int	bus_tx_hdr_size;
	int	bus_rx_hdr_size;
	int bus_tx_dummy_size;
	int bus_rx_dummy_size;

	/* */
	atomic_t recur_chk;

	/* */
	int is_charger;

	/* */
	int (*init_late)(void *data);
	int init_late_done;
};

enum {
	FORCE_FWUP_CLEAR		= _FORCE_FWUP_CLEAR,
	FORCE_FWUP_ON			= _FORCE_FWUP_ON,
	FORCE_FWUP_SYS_SHOW		= _FORCE_FWUP_SYS_SHOW,
	FORCE_FWUP_SYS_STORE		= _FORCE_FWUP_SYS_STORE,
};

enum {
	IRQ_USE_WAKE			= _IRQ_USE_WAKE,
	IRQ_USE_SCHEDULE_WORK		= _IRQ_USE_SCHEDULE_WORK,
	/* */
	TOUCH_USE_MON_THREAD		= _TOUCH_USE_MON_THREAD,
	TOUCH_USE_PINCTRL		= _TOUCH_USE_PINCTRL,
	TOUCH_USE_PWRCTRL		= _TOUCH_USE_PWRCTRL,
	/* */
	TOUCH_USE_VBLANK		= _TOUCH_USE_VBLANK,
	TOUCH_USE_INPUT_PARENT		= _TOUCH_USE_INPUT_PARENT,
	/* */
	TOUCH_USE_VIRT_DIR_WATCH	= _TOUCH_USE_VIRT_DIR_WATCH,
	TOUCH_USE_DRV_NAME_SYSFS	= _TOUCH_USE_DRV_NAME_SYSFS,
	TOUCH_USE_FW_BINARY			= _TOUCH_USE_FW_BINARY,
	TOUCH_USE_FONT_BINARY		= _TOUCH_USE_FONT_BINARY,
	/* */
	TOUCH_USE_PROBE_INIT_LATE	= _TOUCH_USE_PROBE_INIT_LATE,
	/* */
	TOUCH_SKIP_ESD_EVENT		= _TOUCH_SKIP_ESD_EVENT,
	TOUCH_SKIP_RESET_PIN		= _TOUCH_SKIP_RESET_PIN,
	/* */
	TOUCH_IGNORE_DT_FLAGS		= _TOUCH_IGNORE_DT_FLAGS,
};


/* goes to siw_touch_init_work_func */
#define __siw_touch_qd_init_work(_ts, _delay)	\
		queue_delayed_work(_ts->wq, &_ts->init_work, _delay)
/* goes to siw_touch_upgrade_work_func */
#define __siw_touch_qd_upgrade_work(_ts, _delay)	\
		queue_delayed_work(_ts->wq, &_ts->upgrade_work, _delay)
/* goes to siw_touch_atomic_notifer_work_func */
#define __siw_touch_qd_notify_work(_ts, _delay)	\
		queue_delayed_work(_ts->wq, &_ts->notify_work, _delay)
/* goes to siw_touch_fb_work_func  */
#define __siw_touch_qd_fb_work(_ts, _delay)	\
		queue_delayed_work(_ts->wq, &_ts->fb_work, _delay)

#if defined(__SIW_SUPPORT_ASC)
/* goes to siw_touch_toggle_delta_check_work_func */
#define __siw_touch_qd_toggle_delta_work(_ts, _delay)	\
		queue_delayed_work(_ts->wq, &_ts->toggle_delta_work, _delay)
/* goes to siw_touch_finger_input_check_work_func */
#define __siw_touch_qd_finger_input_work(_ts, _delay)	\
		queue_delayed_work(_ts->wq, &_ts->finger_input_work, _delay)
#endif	/* __SIW_SUPPORT_ASC */

/* goes to siw_touch_sys_reset_work_func  */
#define __siw_touch_qd_sys_reset_work(_ts, _delay)	\
		queue_delayed_work(_ts->wq, &_ts->sys_reset_work, _delay)


#define siw_touch_qd_init_work_now(_ts)	\
		__siw_touch_qd_init_work(_ts, 0)
#define siw_touch_qd_init_work_jiffies(_ts, _jiffies)	\
		__siw_touch_qd_init_work(_ts, msecs_to_jiffies(_jiffies))
#define siw_touch_qd_init_work_sw(_ts)	\
		__siw_touch_qd_init_work(_ts,	\
			msecs_to_jiffies(_ts->caps.sw_reset_delay))
#define siw_touch_qd_init_work_hw(_ts)	\
		__siw_touch_qd_init_work(_ts,	\
			msecs_to_jiffies(_ts->caps.hw_reset_delay))

#define siw_touch_qd_upgrade_work_now(_ts)	\
		__siw_touch_qd_upgrade_work(_ts, 0)
#define siw_touch_qd_upgrade_work_jiffies(_ts, _jiffies)	\
		__siw_touch_qd_upgrade_work(_ts, msecs_to_jiffies(_jiffies))

#define siw_touch_qd_notify_work_now(_ts)	\
		__siw_touch_qd_notify_work(_ts, 0)
#define siw_touch_qd_notify_work_jiffies(_ts, _jiffies)	\
		__siw_touch_qd_notify_work(_ts, msecs_to_jiffies(_jiffies))

#define siw_touch_qd_fb_work_now(_ts)	\
		__siw_touch_qd_fb_work(_ts, 0)
#define siw_touch_qd_fb_work_jiffies(_ts, _jiffies)	\
		__siw_touch_qd_fb_work(_ts, msecs_to_jiffies(_jiffies))

#if defined(__SIW_SUPPORT_ASC)
#define siw_touch_qd_toggle_delta_work_now(_ts)	\
		__siw_touch_qd_toggle_delta_work(_ts, 0)
#define siw_touch_qd_toggle_delta_work_jiffies(_ts, _jiffies)	\
	__siw_touch_qd_toggle_delta_work(_ts, msecs_to_jiffies(_jiffies))

#define siw_touch_qd_finger_input_work_now(_ts)	\
		__siw_touch_qd_finger_input_work(_ts, 0)
#define siw_touch_qd_finger_input_work_jiffies(_ts, _jiffies)	\
	__siw_touch_qd_finger_input_work(_ts, msecs_to_jiffies(_jiffies))
#endif	/* __SIW_SUPPORT_ASC */

#define siw_touch_qd_sys_reset_work_now(_ts)	\
		__siw_touch_qd_sys_reset_work(_ts, 0)
#define siw_touch_qd_sys_reset_work_jiffies(_ts, _jiffies)	\
		__siw_touch_qd_sys_reset_work(_ts, msecs_to_jiffies(_jiffies))



struct siw_touch_attribute {
	struct attribute attr;
	ssize_t (*show)(struct device *dev, char *buf);
	ssize_t (*store)(struct device *dev, const char *buf, size_t count);
};

#define __TOUCH_ATTR(_name, _attr, _show, _store)		\
			struct siw_touch_attribute touch_attr_##_name	\
			= __ATTR(_name, _attr, _show, _store)

#if defined(__SIW_ATTR_PERMISSION_ALL)
#define __TOUCH_DEFAULT_PERM	(S_IRUGO | S_IWUGO)
#else
#define __TOUCH_DEFAULT_PERM	(S_IRUGO | S_IWUSR | S_IWGRP)
#endif

#define TOUCH_ATTR(_name, _show, _store)	\
			__TOUCH_ATTR(_name, __TOUCH_DEFAULT_PERM, _show, _store)


static inline void touch_set_dev_data(struct siw_ts *ts, void *data)
{
	ts->dev_data = data;
}

static inline void *touch_get_dev_data(struct siw_ts *ts)
{
	return ts->dev_data;
}

static inline struct siw_ts *to_touch_core(struct device *dev)
{
	return dev ? (struct siw_ts *)dev_get_drvdata(dev) : NULL;
}

static inline unsigned long touch_get_quirks(struct siw_ts *ts)
{
	return pdata_get_quirks(ts->pdata);
}

static inline unsigned long touch_test_quirks(struct siw_ts *ts,
			unsigned long quirk_bit)
{
	return pdata_test_quirks(ts->pdata, quirk_bit);
}

static inline unsigned long touch_test_abt_quirks(struct siw_ts *ts,
			unsigned long quirk_bit)
{
	return pdata_test_abt_quirks(ts->pdata, quirk_bit);
}

static inline unsigned long touch_test_prd_quirks(struct siw_ts *ts,
			unsigned long quirk_bit)
{
	return pdata_test_prd_quirks(ts->pdata, quirk_bit);
}

static inline char *touch_chip_id(struct siw_ts *ts)
{
	return ts->chip_id;
}

static inline char *touch_chip_name(struct siw_ts *ts)
{
	return ts->chip_name;
}

static inline char *touch_drv_name(struct siw_ts *ts)
{
	return ts->drv_name;
}

static inline char *touch_idrv_name(struct siw_ts *ts)
{
	return ts->idrv_name;
}

static inline char *touch_ext_watch_name(struct siw_ts *ts)
{
	return ts->ext_watch_name;
}

static inline u32 touch_max_finger(struct siw_ts *ts)
{
	return ts->max_finger;
}

static inline u32 touch_chip_type(struct siw_ts *ts)
{
	return ts->chip_type;
}

static inline u32 touch_mode_allowed(struct siw_ts *ts, u32 mode)
{
	return pdata_mode_allowed(ts->pdata, mode);
}

static inline u32 touch_mode_not_allowed(struct siw_ts *ts, u32 mode)
{
	int ret;

	ret = !pdata_mode_allowed(ts->pdata, mode);
	if (ret)
		t_dev_warn(ts->dev, "target mode(%d) not supported\n", mode);

	return ret;
}

static inline u32 touch_fw_size(struct siw_ts *ts)
{
	return pdata_fw_size(ts->pdata);
}

static inline u32 touch_flags(struct siw_ts *ts)
{
	return ts->flags;
}

static inline unsigned long touch_irqflags(struct siw_ts *ts)
{
	return ts->irqflags;
}

static inline u32 touch_bus_type(struct siw_ts *ts)
{
	return pdata_bus_type(ts->pdata);
}

static inline u32 touch_buf_size(struct siw_ts *ts)
{
	return pdata_buf_size(ts->pdata);
}

static inline u32 touch_spi_mode(struct siw_ts *ts)
{
	return pdata_spi_mode(ts->pdata);
}

static inline u32 touch_bits_per_word(struct siw_ts *ts)
{
	return pdata_bits_per_word(ts->pdata);
}

static inline u32 touch_max_freq(struct siw_ts *ts)
{
	return pdata_max_freq(ts->pdata);
}

static inline int touch_tx_hdr_size(struct siw_ts *ts)
{
	return ts->bus_tx_hdr_size;
}

static inline int touch_rx_hdr_size(struct siw_ts *ts)
{
	return ts->bus_rx_hdr_size;
}

static inline int touch_tx_dummy_size(struct siw_ts *ts)
{
	return ts->bus_tx_dummy_size;
}

static inline int touch_rx_dummy_size(struct siw_ts *ts)
{
	return ts->bus_rx_dummy_size;
}

static inline int touch_xfer_allowed(struct siw_ts *ts)
{
	return pdata_xfer_allowed(ts->pdata);
}

static inline struct siw_touch_fquirks *touch_fquirks(struct siw_ts *ts)
{
	return pdata_fquirks(ts->pdata);
}

static inline struct siw_touch_uevent_ctrl *touch_uevent_ctrl(
							struct siw_ts *ts)
{
	return pdata_uevent_ctrl(ts->pdata);
}

static inline struct siw_touch_fw_bin *touch_fw_bin(
							struct siw_ts *ts)
{
	return pdata_fw_bin(ts->pdata);
}

static inline struct siw_touch_font_bin *touch_font_bin(
							struct siw_ts *ts)
{
	return pdata_font_bin(ts->pdata);
}

static inline int touch_senseless_margin(struct siw_ts *ts)
{
	return pdata_senseless_margin(ts->pdata);
}


static inline u32 touch_get_act_buf_size(struct siw_ts *ts)
{
	return ts->buf_size;
}

static inline void touch_set_act_buf_size(struct siw_ts *ts, int size)
{
	ts->buf_size = size;
}

static inline void touch_set_caps(struct siw_ts *ts,
					struct touch_device_caps *caps_src)
{
	struct touch_device_caps *caps = &ts->caps;

	caps->max_x = caps_src->max_x;
	caps->max_y = caps_src->max_y;
	caps->max_pressure = caps_src->max_pressure;
	caps->max_width = caps_src->max_width;
	caps->max_orientation = caps_src->max_orientation;
	caps->max_id = caps_src->max_id;
	caps->hw_reset_delay = caps_src->hw_reset_delay;
	caps->sw_reset_delay = caps_src->sw_reset_delay;
}

static inline void touch_set_pins(struct siw_ts *ts,
					struct touch_pins *pins_src)
{
	struct touch_pins *pins = &ts->pins;

	pins->reset_pin = pins_src->reset_pin;
	pins->reset_pin_pol = pins_src->reset_pin_pol;
	pins->irq_pin = pins_src->irq_pin;
	pins->maker_id_pin = pins_src->maker_id_pin;
	/* Power */
	pins->vdd_pin = pins_src->vdd_pin;
	pins->vio_pin = pins_src->vio_pin;
}

static inline int touch_reset_pin(struct siw_ts *ts)
{
	return ts->pins.reset_pin;
}

static inline int touch_reset_pin_pol(struct siw_ts *ts)
{
	return ts->pins.reset_pin_pol;
}

static inline int touch_irq_pin(struct siw_ts *ts)
{
	return ts->pins.irq_pin;
}

static inline int touch_maker_id_pin(struct siw_ts *ts)
{
	return ts->pins.maker_id_pin;
}

static inline int touch_vdd_pin(struct siw_ts *ts)
{
	return ts->pins.vdd_pin;
}

static inline int touch_vio_pin(struct siw_ts *ts)
{
	return ts->pins.vio_pin;
}

static inline void *touch_get_vdd(struct siw_ts *ts)
{
	return ts->pins.vdd;
}

static inline void touch_set_vdd(struct siw_ts *ts, void *vdd)
{
	ts->pins.vdd = vdd;
}

static inline void *touch_get_vio(struct siw_ts *ts)
{
	return ts->pins.vio;
}

static inline void touch_set_vio(struct siw_ts *ts, void *vio)
{
	ts->pins.vio = vio;
}


static inline void *siw_ops_reg(struct siw_ts *ts)
{
	return ts->ops->reg;
}

static inline void siw_ops_set_irq_handler(struct siw_ts *ts, void *handler)
{
	ts->ops->irq_handler = handler;
}

static inline void siw_ops_restore_irq_handler(struct siw_ts *ts)
{
	ts->ops->irq_handler = ts->ops_ext->irq_handler;
}


#define siw_ops_is_null(_ts, _ops)	(_ts->ops->_ops == NULL)

#define siw_ops_xxx(_ops, _ret, _ts, args...)	\
({	int _r = 0;	\
	do {	\
		if (_ts->ops->_ops == NULL) {	\
			if ((_ret) < 0) {	\
				t_dev_err(ts->dev,	\
					"%s isn't assigned\n", #_ops);	\
				_r = _ret;	\
			}	\
			break;	\
		}	\
		_r = _ts->ops->_ops(_ts->dev, ##args);	\
	} while (0);	\
	_r;	\
})

#define siw_ops_early_probe(_ts, args...)	\
	siw_ops_xxx(early_probe, 0, _ts, ##args)
#define siw_ops_probe(_ts, args...)		\
	siw_ops_xxx(probe, -ESRCH, _ts, ##args)
#define siw_ops_remove(_ts, args...)		\
	siw_ops_xxx(remove, -ESRCH, _ts, ##args)
#define siw_ops_suspend(_ts, args...)		\
	siw_ops_xxx(suspend, -ESRCH, _ts, ##args)
#define siw_ops_resume(_ts, args...)		\
	siw_ops_xxx(resume, -ESRCH, _ts, ##args)
#define siw_ops_init(_ts, args...)		\
	siw_ops_xxx(init, -ESRCH, _ts, ##args)
#define siw_ops_reset(_ts, args...)		\
	siw_ops_xxx(reset, -ESRCH, _ts, ##args)
#define siw_ops_ic_info(_ts, args...)		\
	siw_ops_xxx(ic_info, -ESRCH, _ts, ##args)
#define siw_ops_tc_con(_ts, args...)		\
	siw_ops_xxx(tc_con, 0, _ts, ##args)
#define siw_ops_tc_driving(_ts, args...)	\
	siw_ops_xxx(tc_driving, -ESRCH, _ts, ##args)
#define siw_ops_chk_status(_ts, args...)	\
	siw_ops_xxx(chk_status, -ESRCH, _ts, ##args)
#define siw_ops_irq_handler(_ts, args...)	\
	siw_ops_xxx(irq_handler, -ESRCH, _ts, ##args)
#define siw_ops_irq_abs(_ts, args...)		\
	siw_ops_xxx(irq_abs, -ESRCH, _ts, ##args)
#define siw_ops_irq_lpwg(_ts, args...)		\
	siw_ops_xxx(irq_lpwg, -ESRCH, _ts, ##args)
#define siw_ops_power(_ts, args...)		\
	siw_ops_xxx(power, -ESRCH, _ts, ##args)
#define siw_ops_upgrade(_ts, args...)		\
	siw_ops_xxx(upgrade, -ESRCH, _ts, ##args)
#define siw_ops_lpwg(_ts, args...)		\
	siw_ops_xxx(lpwg, 0, _ts, ##args)
#define siw_ops_asc(_ts, args...)		\
	siw_ops_xxx(asc, -ESRCH, _ts, ##args)
#define siw_ops_notify(_ts, args...)		\
	siw_ops_xxx(notify, 0, _ts, ##args)
#define siw_ops_set(_ts, args...)		\
	siw_ops_xxx(set, 0, _ts, ##args)
#define siw_ops_get(_ts, args...)		\
	siw_ops_xxx(get, 0, _ts, ##args)
#define siw_ops_sysfs(_ts, args...)		\
	siw_ops_xxx(sysfs, 0, _ts, ##args)
#define siw_ops_mon_handler(_ts, args...)	\
	siw_ops_xxx(mon_handler, -ESRCH, _ts, ##args)

#define siw_ops_abt_sysfs(_ts, args...)		\
	siw_ops_xxx(abt_sysfs, 0, _ts, ##args)
#define siw_ops_prd_sysfs(_ts, args...)		\
	siw_ops_xxx(prd_sysfs, 0, _ts, ##args)
#define siw_ops_watch_sysfs(_ts, args...)	\
	siw_ops_xxx(watch_sysfs, 0, _ts, ##args)


static inline void touch_msleep(unsigned int msecs)
{
	if (!msecs)
		return;

	if (msecs >= 20)
		msleep(msecs);
	else
		usleep_range(msecs * 1000, msecs * 1000);
}

static inline void *touch_kzalloc(struct device *dev, size_t size, gfp_t gfp)
{
	return devm_kzalloc(dev, size, gfp);
}

static inline void touch_kfree(struct device *dev, void *p)
{
	return devm_kfree(dev, p);
}

static inline void *touch_getname(void)
{
	void *name = __getname();

	if (name != NULL)
		memset(name, 0, PATH_MAX);

	return name;
}

static inline void touch_putname(void *name)
{
	if (name != NULL)
		__putname(name);
}


struct siw_op_dbg {
	char		*name;
	int		(*func)(void *data);
	void		(*debug)(const void *op, void *data);
	int		flags;
};

#define _SIW_OP_DBG(_func, _debug, _flags)	\
	{		\
		.name = (#_func),		\
		.func = (_func),		\
		.debug = (_debug),	\
		.flags = (_flags),	\
	}

static inline int __siw_touch_op_dbg(const struct siw_op_dbg *op,
						void *data)
{
	int ret = 0;

	ret = op->func(data);
	if (op->debug)
		op->debug(op, data);

	return ret;
}

#define __siw_snprintf(_buf, _buf_max, _size, _fmt, _args...) \
({	\
	int _n_size = 0;	\
	if (_size < _buf_max)	\
		_n_size = snprintf(_buf + _size, _buf_max - _size,\
				(const char *)_fmt, ##_args);	\
	_n_size;	\
})


#define siw_snprintf(_buf, _size, _fmt, _args...) \
		__siw_snprintf(_buf, PAGE_SIZE, _size, _fmt, ##_args)


extern u32 t_mfts_lpwg;
extern u32 t_lpwg_mode;
extern u32 t_lpwg_screen;
extern u32 t_lpwg_sensor;
extern u32 t_lpwg_qcover;

extern int siw_setup_params
(struct siw_ts *ts, struct siw_touch_pdata *pdata);

extern void *siw_setup_operations
(struct siw_ts *ts, struct siw_touch_operations *ops_ext);

extern int siw_touch_set(struct device *dev, u32 cmd, void *buf);
extern int siw_touch_get(struct device *dev, u32 cmd, void *buf);

extern void siw_touch_suspend_call(struct device *dev);
extern void siw_touch_resume_call(struct device *dev);

extern void siw_touch_change_sensitivity(struct siw_ts *ts,
						int target);

extern int siw_touch_init_notify(struct siw_ts *ts);
extern void siw_touch_free_notify(struct siw_ts *ts);

extern void siw_touch_atomic_notifer_work_func
(struct work_struct *work);

extern void siw_touch_mon_pause(struct device *dev);
extern void siw_touch_mon_resume(struct device *dev);

extern int siw_touch_probe(struct siw_ts *ts);
extern int siw_touch_remove(struct siw_ts *ts);

extern int siw_touch_init_late(struct siw_ts *ts, int value);

extern int siw_touch_notify
(struct siw_ts *ts, unsigned long event, void *data);

extern int siw_touch_parse_data(struct siw_ts *ts);

extern int siw_hal_sysfs(struct device *dev, int on_off);

#if defined(CONFIG_TOUCHSCREEN_SIW_MMI_MON) ||	\
	defined(CONFIG_TOUCHSCREEN_SIW_MMI_MON_MODULE)

struct touch_bus_msg;

struct siw_mon_operations {
	void (*submit_bus)(struct device *dev,
		char *dir, void *data, int ret);
	void (*submit_evt)(struct device *dev,
		char *type, int type_v, char *code,
		int code_v, int value, int ret);
	void (*submit_ops)(struct device *dev,
		char *ops, void *data, int size, int ret);
};

extern struct siw_mon_operations *siw_mon_ops;

static inline void siwmon_submit_bus
(struct device *dev, char *dir, void *data, int ret)
{
	if (siw_mon_ops && siw_mon_ops->submit_bus)
		(*siw_mon_ops->submit_bus)(dev, dir, data, ret);
}

static inline void siwmon_submit_evt
(struct device *dev, char *type, int type_v,
	char *code, int code_v, int value, int ret)
{
	if (siw_mon_ops && siw_mon_ops->submit_evt)
		(*siw_mon_ops->submit_evt)(dev, type, type_v,
			code, code_v, value, ret);
}

static inline void siwmon_submit_ops(struct device *dev, char *ops,
	void *data, int size, int ret)
{
	if (siw_mon_ops && siw_mon_ops->submit_ops)
		(*siw_mon_ops->submit_ops)(dev, ops, data, size, ret);
}

extern int siw_mon_register(struct siw_mon_operations *ops);
extern void siw_mon_deregister(void);

#else	/* CONFIG_TOUCHSCREEN_SIW_MMI_MON */

static inline void siwmon_submit_bus(struct device *dev,
	char *dir, void *data, int ret){ }
static inline void siwmon_submit_evt(struct device *dev,
	char *type, int type_v, char *code, int code_v, int value, int ret){ }
static inline void siwmon_submit_ops(struct device *dev,
	char *ops, void *data, int size, int ret){ }

#endif	/* CONFIG_TOUCHSCREEN_SIW_MMI_MON */

#define siwmon_submit_ops_wh_name(_dev, _fmt, _name, _val, _size, _ret)	\
do {	\
	char _mstr[64];	\
	snprintf(_mstr, sizeof(_mstr), _fmt, _name);	\
	siwmon_submit_ops(_dev, _mstr, _val, _size, _ret);	\
} while (0)

#define siwmon_submit_ops_step(_dev, _ops)	\
		siwmon_submit_ops(_dev, _ops, NULL, 0, 0)

#define siwmon_submit_ops_step_core(_dev, _ops, _ret)	\
		siwmon_submit_ops(_dev, "[S] " _ops, NULL, 0, _ret)

#define siwmon_submit_ops_step_chip(_dev, _ops, _ret)	\
		siwmon_submit_ops(_dev, "(c) " _ops, NULL, 0, _ret)

#define siwmon_submit_ops_step_chip_wh_name(_dev, _fmt, _name, _ret)	\
do {	\
	char _mstr[64];	\
	snprintf(_mstr, sizeof(_mstr), "(c) " _fmt, _name);	\
	siwmon_submit_ops(_dev, _mstr, NULL, 0, _ret);	\
} while (0)

#if !defined(MODULE)
#define __siw_setup_u32(_name, _fn, _var)	\
	static int __init _fn(char *in_str)	\
	{	\
		int rc = kstrtol(in_str, 0, (long *)&_var);	\
\
		if (rc)	\
			return rc; \
		return 1;	\
	}	\
	__setup(_name, _fn)

#define __siw_setup_str(_name, _fn, _var)	\
	static int __init _fn(char *in_str)	\
	{	\
		strlcpy(_var, in_str, sizeof(_var));	\
		return 1;	\
	}	\
	__setup(_name, _fn)
#else	/* MODULE */
#define __siw_setup_u32(_name, _fn, _var)
#define __siw_setup_str(_name, _fn, _var)
#endif	/* MODULE */

#define siw_chip_module_init(_name, _data, _desc, _author)	\
	static int __init chip_##_name##_driver_init(void)\
	{	\
		touch_msleep(200);	\
		t_pr_info("%s: %s driver init - %s\n",	\
			_data.pdata->drv_name, _name, SIW_DRV_VERSION);	\
		return siw_touch_bus_add_driver(&_data);	\
	}	\
	static void __exit chip_##_name##_driver_exit(void)	\
	{	\
		(void)siw_touch_bus_del_driver(&_data);\
		t_pr_info("%s: %s driver exit - %s\n",	\
			_data.pdata->drv_name, _name, SIW_DRV_VERSION);	\
	}	\
	module_init(chip_##_name##_driver_init);	\
	module_exit(chip_##_name##_driver_exit);	\
	MODULE_AUTHOR(_author);	\
	MODULE_DESCRIPTION(_desc);	\
	MODULE_VERSION(SIW_DRV_VERSION);	\
	MODULE_LICENSE("GPL")

#endif /* __SIW_TOUCH_H */


