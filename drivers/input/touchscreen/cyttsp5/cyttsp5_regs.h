/*
 * cyttsp5_regs.h
 * Cypress TrueTouch(TM) Standard Product V5 Registers.
 * For use with Cypress Txx5xx parts.
 * Supported parts include:
 * TMA5XX
 *
 * Copyright (C) 2012-2013 Cypress Semiconductor
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
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#ifndef _CYTTSP5_REGS_H
#define _CYTTSP5_REGS_H

#include <linux/device.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <asm/unaligned.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/hid.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/stringify.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/i2c/cyttsp5_core.h>

extern int androidboot_mode_charger;
extern unsigned int system_rev;

#define ALWAYS_ON_TOUCH
#define CYTTSP5_DEVICE_ACCESS
#define CYTTSP5_LOADER
#define CYTTSP5_BINARY_FW_UPGRADE
#define SAMSUNG_FACTORY_TEST
#define SAMSUNG_TSP_INFO
#define SAMSUNG_TOUCH_MODE
#define SAMSUNG_PALM_MOTION

#define TTHE_TUNER_SUPPORT

#define CSP_SILICON_ID		0x0266
#define QFN_SILICON_ID		0x0254

#define CY_CSP_FW_FILE_NAME "tsp_cypress/cyttsp5_csp.fw"
#define CY_QFN_FW_FILE_NAME "tsp_cypress/cyttsp5_qfn.fw"
#define CY_QFN_03_FW_FILE_NAME "tsp_cypress/cyttsp5_qfn_03.fw"
#define CY_FW_FILE_PATH "/data/tmp/cyttsp5_fw.bin"

#ifdef TTHE_TUNER_SUPPORT
#define CYTTSP5_TTHE_TUNER_FILE_NAME "tthe_tuner"
#endif

#define CY_MAX_PRBUF_SIZE           PIPE_BUF
#define CY_PR_TRUNCATED             " truncated..."

#define CY_DEFAULT_CORE_ID          "cyttsp5_core0"
#define CY_MAX_NUM_CORE_DEVS        5

/* HID */
#define HID_CYVENDOR		0xff010000
#define CY_HID_VENDOR_ID	0x04B4
#define CY_HID_BL_PRODUCT_ID	0xC100
#define CY_HID_APP_PRODUCT_ID	0xC101
#define CY_HID_VERSION		0x0100
#define CY_HID_APP_REPORT_ID	0xF7
#define CY_HID_BL_REPORT_ID	0xFF

#define HID_TOUCH_REPORT_ID		0x1
#define HID_BTN_REPORT_ID		0x3
#define HID_WAKEUP_REPORT_ID		0x4
#define HID_TRACKING_HEATMAP_REPOR_ID	0xE
#define HID_SENSOR_DATA_REPORT_ID	0xF
#define HID_APP_RESPONSE_REPORT_ID	0x1F
#define HID_APP_OUTPUT_REPORT_ID	0x2F
#define HID_BL_RESPONSE_REPORT_ID	0x30
#define HID_BL_OUTPUT_REPORT_ID		0x40
#define HID_RESPONSE_REPORT_ID		0xF0

#define HID_OUTPUT_RESPONSE_REPORT_OFFSET	2
#define HID_OUTPUT_RESPONSE_CMD_OFFSET		4
#define HID_OUTPUT_RESPONSE_CMD_MASK		0x7F
#define HID_OUTPUT_CMD_OFFSET			6
#define HID_OUTPUT_CMD_MASK			0x7F

#define HID_SYSINFO_CYDATA_OFFSET	5
#define HID_SYSINFO_SENSING_OFFSET	33
#define HID_SYSINFO_BTN_OFFSET		48
#define HID_SYSINFO_BTN_MASK		0xF
#define HID_SYSINFO_MAX_BTN		4

#define HID_POWER_ON			0x0
#define HID_POWER_SLEEP			0x1
#define HID_LENGTH_BYTES		2
#define HID_LENGTH_AND_REPORT_ID_BYTES	3

/*  Timeout in ms */
#define CY_REQUEST_EXCLUSIVE_TIMEOUT 500
#define CY_WATCHDOG_TIMEOUT         1000
#define CY_WATCHDOG_REQUEST_EXCLUSIVE_TIMEOUT 6000
#define CY_CORE_RESET_AND_WAIT_TIMEOUT		500
#define CY_CORE_WAKEUP_TIMEOUT			500
#define CY_HID_RESET_TIMEOUT			5000
#define CY_HID_SET_POWER_TIMEOUT		500
#define CY_HID_GET_HID_DESCRIPTOR_TIMEOUT	500
#define CY_HID_GET_REPORT_DESCRIPTOR_TIMEOUT	500
#ifdef VERBOSE_DEBUG
#define CY_HID_OUTPUT_TIMEOUT			2000
#else
#define CY_HID_OUTPUT_TIMEOUT			200
#endif
#define CY_HID_OUTPUT_START_BOOTLOADER_TIMEOUT	2000
#define CY_HID_OUTPUT_USER_TIMEOUT		8000
#define CY_HID_OUTPUT_GET_SYSINFO_TIMEOUT	3000
#define CY_HID_OUTPUT_CALIBRATE_IDAC_TIMEOT	5000
#define CY_HID_OUTPUT_BL_INITIATE_BL_TIMEOUT	8000

/* maximum number of concurrent tracks */
#define MAX_TOUCH_NUMBER            10
#define TOUCH_REPORT_SIZE           10
#define TOUCH_INPUT_HEADER_SIZE     7
#define TOUCH_COUNT_BYTE_OFFSET     5
#define BTN_REPORT_SIZE             9
#define BTN_INPUT_HEADER_SIZE       5
#define SENSOR_REPORT_SIZE          150
#define SENSOR_HEADER_SIZE          4

/* helpers */
#define GET_NUM_TOUCHES(x)          ((x) & 0x1F)
#define IS_LARGE_AREA(x)            ((x) & 0x20)
#define IS_BAD_PKT(x)               ((x) & 0x20)
#define IS_TMO(t)                   ((t) == 0)
#define HI_BYTE(x)                  (u8)(((x) >> 8) & 0xFF)
#define LOW_BYTE(x)                 (u8)((x) & 0xFF)
#define SET_CMD_LOW(byte, bits)	\
	((byte) = (((byte) & 0xF0) | ((bits) & 0x0F)))
#define SET_CMD_HIGH(byte, bits)\
	((byte) = (((byte) & 0x0F) | ((bits) & 0xF0)))

#define GET_MASK(length) \
	((1 << length) - 1)
#define GET_FIELD(name, length, shift) \
	((name >> shift) & GET_MASK(length))

#define HID_ITEM_SIZE_MASK	0x03
#define HID_ITEM_TYPE_MASK	0x0C
#define HID_ITEM_TAG_MASK	0xF0

#define HID_ITEM_SIZE_SHIFT	0
#define HID_ITEM_TYPE_SHIFT	2
#define HID_ITEM_TAG_SHIFT	4

#define HID_GET_ITEM_SIZE(x)  \
	((x & HID_ITEM_SIZE_MASK) >> HID_ITEM_SIZE_SHIFT)
#define HID_GET_ITEM_TYPE(x) \
	((x & HID_ITEM_TYPE_MASK) >> HID_ITEM_TYPE_SHIFT)
#define HID_GET_ITEM_TAG(x) \
	((x & HID_ITEM_TAG_MASK) >> HID_ITEM_TAG_SHIFT)

#define IS_DEEP_SLEEP_CONFIGURED(x) \
		((x) == 0 || (x) == 0xFF)

#define IS_PIP_VER_GE(p, maj, min) \
		((p)->cydata.pip_ver_major < (maj) ? \
			0 : \
			((p)->cydata.pip_ver_minor < (min) ? \
				0 : \
				1))

/* drv_debug commands */
#define CY_DBG_SUSPEND                  4
#define CY_DBG_RESUME                   5
#define CY_DBG_SOFT_RESET               97
#define CY_DBG_RESET                    98
#define CY_DBG_HID_RESET                50
#define CY_DBG_HID_GET_REPORT           51
#define CY_DBG_HID_SET_REPORT           52
#define CY_DBG_HID_SET_POWER_ON         53
#define CY_DBG_HID_SET_POWER_SLEEP      54
#define CY_DBG_HID_NULL                 100
#define CY_DBG_HID_ENTER_BL             101
#define CY_DBG_HID_SYSINFO              102
#define CY_DBG_HID_SUSPEND_SCAN         103
#define CY_DBG_HID_RESUME_SCAN          104
#define CY_DBG_HID_STOP_WD              105
#define CY_DBG_HID_START_WD             106

/* Recognized usages */
/* undef them first for possible redefinition in Linux */
#undef HID_DI_PRESSURE
#undef HID_DI_TIP
#undef HID_DI_CONTACTID
#undef HID_DI_CONTACTCOUNT
#undef HID_DI_SCANTIME
#define HID_DI_PRESSURE		0x000d0030
#define HID_DI_TIP		0x000d0042
#define HID_DI_CONTACTID	0x000d0051
#define HID_DI_CONTACTCOUNT	0x000d0054
#define HID_DI_SCANTIME		0x000d0056

/* Cypress vendor specific usages */
#define HID_CY_UNDEFINED	0xff010000
#define HID_CY_BOOTLOADER	0xff010001
#define HID_CY_TOUCHAPPLICATION	0xff010002
#define HID_CY_BUTTONS		0xff010020
#define HID_CY_GENERICITEM	0xff010030
#define HID_CY_LARGEOBJECT	0xff010040
#define HID_CY_NOISEEFFECTS	0xff010041
#define HID_CY_REPORTCOUNTER	0xff010042
#define HID_CY_TOUCHTYPE	0xff010060
#define HID_CY_EVENTID		0xff010061
#define HID_CY_MAJORAXISLENGTH	0xff010062
#define HID_CY_MINORAXISLENGTH	0xff010063
#define HID_CY_ORIENTATION	0xff010064
#define HID_CY_BUTTONSIGNAL	0xff010065
#define HID_CY_MAJOR_CONTACT_AXIS_LENGTH	0xff010066
#define HID_CY_MINOR_CONTACT_AXIS_LENGTH	0xff010067
#define HID_CY_TCH_COL_USAGE_PG 0x000D0022
#define HID_CY_BTN_COL_USAGE_PG 0xFF010020

/* FW RAM parameters */
#define CY_RAM_ID_TOUCHMODE_ENABLED	0xD0 /* Enable proximity */

#ifdef CONFIG_SEC_DVFS
#define TSP_BOOSTER
#else
#undef TSP_BOOSTER
#endif
/* abs signal capabilities offsets in the frameworks array */
enum cyttsp5_sig_caps {
	CY_SIGNAL_OST,
	CY_MIN_OST,
	CY_MAX_OST,
	CY_FUZZ_OST,
	CY_FLAT_OST,
	CY_NUM_ABS_SET	/* number of signal capability fields */
};

/* abs axis signal offsets in the framworks array  */
enum cyttsp5_sig_ost {
	CY_ABS_X_OST,
	CY_ABS_Y_OST,
	CY_ABS_P_OST,
	CY_ABS_W_OST,
	CY_ABS_ID_OST,
	CY_ABS_MAJ_OST,
	CY_ABS_MIN_OST,
	CY_ABS_OR_OST,
	CY_ABS_TOOL_OST,
	CY_NUM_ABS_OST	/* number of abs signals */
};

enum hid_command {
	HID_CMD_RESERVED,
	HID_CMD_RESET,
	HID_CMD_GET_REPORT,
	HID_CMD_SET_REPORT,
	HID_CMD_GET_IDLE,
	HID_CMD_SET_IDLE,
	HID_CMD_GET_PROTOCOL,
	HID_CMD_SET_PROTOCOL,
	HID_CMD_SET_POWER,
	HID_CMD_VENDOR = 0xE,
};

enum hid_output_cmd_type {
	HID_OUTPUT_CMD_APP,
	HID_OUTPUT_CMD_BL,
};

enum hid_output {
	HID_OUTPUT_NULL,
	HID_OUTPUT_START_BOOTLOADER,
	HID_OUTPUT_GET_SYSINFO,
	HID_OUTPUT_SUSPEND_SCANNING,
	HID_OUTPUT_RESUME_SCANNING,
	HID_OUTPUT_GET_PARAM,
	HID_OUTPUT_SET_PARAM,
	HID_OUTPUT_GET_NOISE_METRICS,
	HID_OUTPUT_RESERVED,
	HID_OUTPUT_ENTER_EASYWAKE_STATE,
	HID_OUTPUT_VERIFY_CONFIG_BLOCK_CRC = 0x20,
	HID_OUTPUT_GET_CONFIG_ROW_SIZE,
	HID_OUTPUT_READ_CONF_BLOCK,
	HID_OUTPUT_WRITE_CONF_BLOCK,
	HID_OUTPUT_GET_DATA_STRUCTURE,
	HID_OUTPUT_LOAD_SELF_TEST_PARAM,
	HID_OUTPUT_RUN_SELF_TEST,
	HID_OUTPUT_GET_SELF_TEST_RESULT,
	HID_OUTPUT_CALIBRATE_IDACS,
	HID_OUTPUT_INITIALIZE_BASELINES,
	HID_OUTPUT_EXEC_PANEL_SCAN,
	HID_OUTPUT_RETRIEVE_PANEL_SCAN,
	HID_OUTPUT_START_SENSOR_DATA_MODE,
	HID_OUTPUT_STOP_SENSOR_DATA_MODE,
	HID_OUTPUT_START_TRACKING_HEATMAP_MODE,
	HID_OUTPUT_INT_PIN_OVERRIDE = 0x40,
	HID_OUTPUT_STORE_PANEL_SCAN = 0x60,
	HID_OUTPUT_PROCESS_PANEL_SCAN,
	HID_OUTPUT_DISCARD_INPUT_REPORT,
	HID_OUTPUT_LAST,
	HID_OUTPUT_USER_CMD,
};

enum hid_output_bl {
	HID_OUTPUT_BL_VERIFY_APP_INTEGRITY = 0x31,
	HID_OUTPUT_BL_APPEND_DATA_BUFF = 0x37,
	HID_OUTPUT_BL_GET_INFO,
	HID_OUTPUT_BL_PROGRAM_AND_VERIFY,
	HID_OUTPUT_BL_LAUNCH_APP = 0x3B,
	HID_OUTPUT_BL_INITIATE_BL = 0x48,
	HID_OUTPUT_BL_LAST,
};

#define HID_OUTPUT_BL_SOP	0x1
#define HID_OUTPUT_BL_EOP	0x17

enum hid_output_bl_status {
	ERROR_SUCCESS,
	ERROR_KEY,
	ERROR_VERIFICATION,
	ERROR_LENGTH,
	ERROR_DATA,
	ERROR_COMMAND,
	ERROR_CRC = 8,
	ERROR_FLASH_ARRAY,
	ERROR_FLASH_ROW,
	ERROR_FLASH_PROTECTION,
	ERROR_UKNOWN = 15,
	ERROR_INVALID,
};

enum cyttsp5_mode {
	CY_MODE_UNKNOWN,
	CY_MODE_BOOTLOADER,
	CY_MODE_OPERATIONAL,
};

enum {
	CY_IC_GRPNUM_RESERVED,
	CY_IC_GRPNUM_CMD_REGS,
	CY_IC_GRPNUM_TCH_REP,
	CY_IC_GRPNUM_DATA_REC,
	CY_IC_GRPNUM_TEST_REC,
	CY_IC_GRPNUM_PCFG_REC,
	CY_IC_GRPNUM_TCH_PARM_VAL,
	CY_IC_GRPNUM_TCH_PARM_SIZE,
	CY_IC_GRPNUM_RESERVED1,
	CY_IC_GRPNUM_RESERVED2,
	CY_IC_GRPNUM_OPCFG_REC,
	CY_IC_GRPNUM_DDATA_REC,
	CY_IC_GRPNUM_MDATA_REC,
	CY_IC_GRPNUM_TEST_REGS,
	CY_IC_GRPNUM_BTN_KEYS,
	CY_IC_GRPNUM_TTHE_REGS,
	CY_IC_GRPNUM_SENSING_CONF,
	CY_IC_GRPNUM_NUM,
};

enum cyttsp5_event_id {
	CY_EV_NO_EVENT,
	CY_EV_TOUCHDOWN,
	CY_EV_MOVE,		/* significant displacement (> act dist) */
	CY_EV_LIFTOFF,		/* record reports last position */
};

enum cyttsp5_object_id {
	CY_OBJ_STANDARD_FINGER,
	CY_OBJ_PROXIMITY,
	CY_OBJ_STYLUS,
	CY_OBJ_HOVER,
	CY_OBJ_GLOVE,
};

#define CY_NUM_MFGID                8

/* System Information interface definitions */
struct cyttsp5_cydata_dev {
	u8 pip_ver_major;
	u8 pip_ver_minor;
	__le16 fw_pid;
	u8 fw_ver_major;
	u8 fw_ver_minor;
	__le32 revctrl;
	__le16 fw_ver_conf;
	u8 bl_ver_major;
	u8 bl_ver_minor;
	__le16 jtag_si_id_l;
	__le16 jtag_si_id_h;
	u8 mfg_id[CY_NUM_MFGID];
	__le16 post_code;
} __packed;

struct cyttsp5_sensing_conf_data_dev {
	u8 electrodes_x;
	u8 electrodes_y;
	__le16 len_x;
	__le16 len_y;
	__le16 res_x;
	__le16 res_y;
	__le16 max_z;
	u8 origin_x;
	u8 origin_y;
	u8 panel_id;
	u8 btn;
	u8 scan_mode;
	u8 max_num_of_tch_per_refresh_cycle;
} __packed;

struct cyttsp5_cydata {
	u8 pip_ver_major;
	u8 pip_ver_minor;
	u8 bl_ver_major;
	u8 bl_ver_minor;
	u8 fw_ver_major;
	u8 fw_ver_minor;
	u16 fw_pid;
	u16 fw_ver_conf;
	u16 post_code;
	u32 revctrl;
	u16 jtag_id_l;
	u16 jtag_id_h;
	u8 mfg_id[CY_NUM_MFGID];
};

struct cyttsp5_sensing_conf_data {
	u16 res_x;
	u16 res_y;
	u16 max_z;
	u16 len_x;
	u16 len_y;
	u8 electrodes_x;
	u8 electrodes_y;
	u8 origin_x;
	u8 origin_y;
	u8 panel_id;
	u8 btn;
	u8 scan_mode;
	u8 max_num_of_tch_per_refresh_cycle;
};

enum cyttsp5_tch_abs {	/* for ordering within the extracted touch data array */
	CY_TCH_X,	/* X */
	CY_TCH_Y,	/* Y */
	CY_TCH_P,	/* P (Z) */
	CY_TCH_T,	/* TOUCH ID */
	CY_TCH_E,	/* EVENT ID */
	CY_TCH_O,	/* OBJECT ID */
	CY_TCH_TIP,	/* OBJECT ID */
	CY_TCH_MAJ,	/* TOUCH_MAJOR */
	CY_TCH_MIN,	/* TOUCH_MINOR */
	CY_TCH_OR,	/* ORIENTATION */
	CY_TCH_NUM_ABS,
};

enum cyttsp5_tch_hdr {
	CY_TCH_TIME,	/* SCAN TIME */
	CY_TCH_NUM,	/* NUMBER OF RECORDS */
	CY_TCH_LO,	/* LARGE OBJECT */
	CY_TCH_NOISE,	/* NOISE EFFECT */
	CY_TCH_COUNTER,	/* REPORT_COUNTER */
	CY_TCH_NUM_HDR,
};

static const char * const cyttsp5_tch_abs_string[] = {
	[CY_TCH_X]	= "X",
	[CY_TCH_Y]	= "Y",
	[CY_TCH_P]	= "P",
	[CY_TCH_T]	= "T",
	[CY_TCH_E]	= "E",
	[CY_TCH_O]	= "O",
	[CY_TCH_TIP]	= "TIP",
	[CY_TCH_MAJ]	= "MAJ",
	[CY_TCH_MIN]	= "MIN",
	[CY_TCH_OR]	= "OR",
	[CY_TCH_NUM_ABS] = "INVALID",
};

static const char * const cyttsp5_tch_hdr_string[] = {
	[CY_TCH_TIME]	= "SCAN TIME",
	[CY_TCH_NUM]	= "NUMBER OF RECORDS",
	[CY_TCH_LO]	= "LARGE OBJECT",
	[CY_TCH_NOISE]	= "NOISE EFFECT",
	[CY_TCH_COUNTER] = "REPORT_COUNTER",
	[CY_TCH_NUM_HDR] = "INVALID",
};

static const int cyttsp5_tch_abs_field_map[] = {
	[CY_TCH_X]	= 0x00010030 /* HID_GD_X */,
	[CY_TCH_Y]	= 0x00010031 /* HID_GD_Y */,
	[CY_TCH_P]	= HID_DI_PRESSURE,
	[CY_TCH_T]	= HID_DI_CONTACTID,
	[CY_TCH_E]	= HID_CY_EVENTID,
	[CY_TCH_O]	= HID_CY_TOUCHTYPE,
	[CY_TCH_TIP]	= HID_DI_TIP,
	[CY_TCH_MAJ]	= HID_CY_MAJORAXISLENGTH,
	[CY_TCH_MIN]	= HID_CY_MINORAXISLENGTH,
	[CY_TCH_OR]	= HID_CY_ORIENTATION,
	[CY_TCH_NUM_ABS] = 0,
};

static const int cyttsp5_tch_hdr_field_map[] = {
	[CY_TCH_TIME]	= HID_DI_SCANTIME,
	[CY_TCH_NUM]	= HID_DI_CONTACTCOUNT,
	[CY_TCH_LO]	= HID_CY_LARGEOBJECT,
	[CY_TCH_NOISE]	= HID_CY_NOISEEFFECTS,
	[CY_TCH_COUNTER] = HID_CY_REPORTCOUNTER,
	[CY_TCH_NUM_HDR] = 0,
};

#define CY_NUM_EXT_TCH_FIELDS   3

struct cyttsp5_tch_abs_params {
	size_t ofs;	/* abs byte offset */
	size_t size;	/* size in bits */
	size_t min;	/* min value */
	size_t max;	/* max value */
	size_t bofs;	/* bit offset */
	u8 report;
};

struct cyttsp5_touch {
	int hdr[CY_TCH_NUM_HDR];
	int abs[CY_TCH_NUM_ABS];
};

/* button to keycode support */
#define CY_BITS_PER_BTN		1
#define CY_NUM_BTN_EVENT_ID	((1 << CY_BITS_PER_BTN) - 1)

enum cyttsp5_btn_state {
	CY_BTN_RELEASED = 0,
	CY_BTN_PRESSED = 1,
	CY_BTN_NUM_STATE
};

struct cyttsp5_btn {
	bool enabled;
	int state;	/* CY_BTN_PRESSED, CY_BTN_RELEASED */
	int key_code;
};

enum cyttsp5_ic_ebid {
	CY_TCH_PARM_EBID,
	CY_MDATA_EBID,
	CY_DDATA_EBID,
};

struct cyttsp4_ttconfig {
	u16 version;
	u16 crc;
};

struct cyttsp5_report_desc_data {
	u16 tch_report_id;
	u16 tch_record_size;
	u16 tch_header_size;
	u16 btn_report_id;
};

#ifdef SAMSUNG_TSP_INFO
struct cyttsp5_samsung_tsp_info_dev {
	u8 thresholdh;
	u8 thresholdl;
	u8 gidac_nodes;
	u8 rx_nodes;
	u8 ic_vendorh;
	u8 ic_vendorl;
	u8 module_vendorh;
	u8 module_vendorl;
	u8 hw_version;
	u8 fw_versionh;
	u8 fw_versionl;
	u8 config_version;
	u8 ic_series;
	u8 num_sensor_x;
	u8 num_sensor_y;
	u8 resolution_xh;
	u8 resolution_xl;
	u8 resolution_yh;
	u8 resolution_yl;
	u8 button_info;
	u8 set_comb_info;
} __packed;
#endif

struct cyttsp5_sysinfo {
	bool ready;
	struct cyttsp5_cydata cydata;
	struct cyttsp5_sensing_conf_data sensing_conf_data;
	struct cyttsp5_report_desc_data desc;
	int num_btns;
	struct cyttsp5_btn *btn;
	struct cyttsp4_ttconfig ttconfig;
	struct cyttsp5_tch_abs_params tch_hdr[CY_TCH_NUM_HDR];
	struct cyttsp5_tch_abs_params tch_abs[CY_TCH_NUM_ABS];
	u8 *xy_mode;
	u8 *xy_data;
};

enum cyttsp5_atten_type {
	CY_ATTEN_IRQ,
	CY_ATTEN_STARTUP,
	CY_ATTEN_EXCLUSIVE,
	CY_ATTEN_WAKE,
	CY_ATTEN_NUM_ATTEN,
};

enum cyttsp5_sleep_state {
	SS_SLEEP_OFF,
	SS_SLEEP_ON,
	SS_SLEEPING,
	SS_WAKING,
};

enum cyttsp5_startup_state {
	STARTUP_NONE,
	STARTUP_QUEUED,
	STARTUP_RUNNING,
	STARTUP_ILLEGAL,
};

struct cyttsp5_hid_desc {
	__le16 hid_desc_len;
	u8 packet_id;
	u8 reserved_byte;
	__le16 bcd_version;
	__le16 report_desc_len;
	__le16 report_desc_register;
	__le16 input_register;
	__le16 max_input_len;
	__le16 output_register;
	__le16 max_output_len;
	__le16 command_register;
	__le16 data_register;
	__le16 vendor_id;
	__le16 product_id;
	__le16 version_id;
	u8 reserved[4];
} __packed;

struct cyttsp5_hid_core {
	u16 hid_vendor_id;
	u16 hid_product_id;
	__le16 hid_desc_register;
	u16 hid_report_desc_len;
	u16 hid_max_input_len;
	u16 hid_max_output_len;
};

#define CY_HID_MAX_REPORTS		8
#define CY_HID_MAX_FIELDS		128
#define CY_HID_MAX_COLLECTIONS		3
#define CY_HID_MAX_NESTED_COLLECTIONS	CY_HID_MAX_COLLECTIONS

#define CY_MAX_INPUT		512

enum cyttsp5_module_id {
	CY_MODULE_MT,
	CY_MODULE_BTN,
	CY_MODULE_PROX,
	CY_MODULE_DEBUG,
	CY_MODULE_LOADER,
	CY_MODULE_DEVICE_ACCESS,
	CY_MODULE_LAST,
};

struct cyttsp5_mt_data;
struct cyttsp5_mt_function {
	int (*mt_release)(struct device *dev);
	int (*mt_probe)(struct device *dev, struct cyttsp5_mt_data *md);
	void (*report_slot_liftoff)(struct cyttsp5_mt_data *md, int max_slots);
	void (*input_sync)(struct input_dev *input);
	void (*input_report)(struct input_dev *input, int sig, int t, int type);
	void (*final_sync)(struct input_dev *input, int max_slots,
			int mt_sync_count, unsigned long *ids);
	int (*input_register_device)(struct input_dev *input, int max_slots);
};

struct cyttsp5_mt_data {
	struct device *dev;
	struct cyttsp5_mt_platform_data *pdata;
	struct cyttsp5_sysinfo *si;
	struct input_dev *input;
	struct cyttsp5_mt_function mt_function;
	struct mutex mt_lock;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend es;
	bool is_suspended;
#endif
#if defined(TSP_BOOSTER)
	struct delayed_work work_dvfs_off;
	struct delayed_work work_dvfs_chg;
	bool	dvfs_lock_status;
	struct mutex dvfs_lock;
	int dvfs_old_status;
	int dvfs_boost_mode;
	int dvfs_freq;
#endif
	bool input_device_registered;
	char phys[NAME_MAX];
	int num_prv_tch;
#ifdef VERBOSE_DEBUG
	u8 pr_buf[CY_MAX_PRBUF_SIZE];
#endif
#ifdef SAMSUNG_PALM_MOTION
	bool palm;
#endif
	bool prevent_touch;
	u8 fw_ver_ic;
};

typedef int (*cyttsp5_upgrade_firmware_from_builtin) (struct device *, bool);

#ifdef SAMSUNG_FACTORY_TEST
#define FACTORY_CMD_STR_LEN 32
#define FACTORY_CMD_RESULT_STR_LEN 512
#define FACTORY_CMD_PARAM_NUM 8

struct cyttsp5_core_commands;

struct cyttsp5_sfd_panel_scan_data {
	u8 *buf;
	s16 min;
	s16 max;
	u8 element_size;	/* in bytes */
};

struct cyttsp5_sfd_idac {
	u8 *buf;	/* has global, local idac both. */
	u8 gidac_min;
	u8 gidac_max;
	u8 lidac_min;
	u8 lidac_max;
};

struct cyttsp5_samsung_factory_data {
	struct device *dev;
	struct device *factory_dev;
	struct cyttsp5_core_commands *corecmd;
	struct cyttsp5_sysinfo *si;
	struct list_head factory_cmd_list_head;

	struct mutex factory_cmd_lock;
	int factory_cmd_state;
	int factory_cmd_param[FACTORY_CMD_PARAM_NUM];
	char factory_cmd[FACTORY_CMD_STR_LEN];
	char factory_cmd_result[FACTORY_CMD_RESULT_STR_LEN];
	bool factory_cmd_is_running;
	bool sysfs_nodes_created;

	struct cyttsp5_sfd_panel_scan_data raw;
	struct cyttsp5_sfd_panel_scan_data diff;
	struct cyttsp5_sfd_idac mutual_idac;

	int num_all_nodes;
};
#endif

struct cyttsp5_core_nonhid_cmd {
	int (*start_bl) (struct device *dev, int protect);
	int (*suspend_scanning) (struct device *dev, int protect);
	int (*resume_scanning) (struct device *dev, int protect);
	int (*get_param) (struct device *dev, int protect, u8 param_id,
			u32 *value);
	int (*set_param) (struct device *dev, int protect, u8 param_id,
			u32 value);
	int (*verify_config_block_crc) (struct device *dev, int protect,
			u8 ebid, u8 *status, u16 *calculated_crc,
			u16 *stored_crc);
	int (*get_config_row_size) (struct device *dev, int protect,
			u16 *row_size);
	int (*calibrate_idacs) (struct device *dev, int protect, u8 mode);
	int (*initialize_baselines) (struct device *dev, int protect,
			u8 test_id);
	int (*retrieve_data_structure) (struct device *dev, int protect,
			u16 read_offset, u16 read_count, u8 data_id,
			u8 *response, u8 *config, u16 *actual_read_len,
			u8 *read_buf);
	int (*exec_panel_scan) (struct device *dev, int protect);
	int (*retrieve_panel_scan) (struct device *dev, int protect,
			u16 read_offset, u16 read_count, u8 data_id,
			u8 *response, u8 *config, u16 *actual_read_len,
			u8 *read_buf);
	int (*write_conf_block) (struct device *dev, int protect,
			u16 row_number, u16 write_length, u8 ebid,
			u8 *write_buf, u8 *security_key, u16 *actual_write_len);
	int (*user_cmd)(struct device *dev, int protect, u16 read_len,
			u8 *read_buf, u16 write_len, u8 *write_buf,
			u16 *actual_read_len);
	int (*get_bl_info) (struct device *dev, int protect, u8 *return_data);
	int (*initiate_bl) (struct device *dev, int protect, u16 key_size,
			u8 *key_buf, u16 row_size, u8 *metadata_row_buf);
	int (*launch_app) (struct device *dev, int protect);
	int (*prog_and_verify) (struct device *dev, int protect, u16 data_len,
			u8 *data_buf);
	int (*verify_app_integrity) (struct device *dev, int protect,
			u8 *result);
};

typedef int (*cyttsp5_atten_func) (struct device *);

struct cyttsp5_core_commands {
	int (*subscribe_attention)(struct device *dev,
			enum cyttsp5_atten_type type, char id,
			cyttsp5_atten_func func, int flags);
	int (*unsubscribe_attention)(struct device *dev,
			enum cyttsp5_atten_type type, char id,
			cyttsp5_atten_func func, int flags);
	int (*request_exclusive)(struct device *dev, int timeout_ms);
	int (*release_exclusive)(struct device *dev);
	int (*request_reset)(struct device *dev);
	int (*request_restart)(struct device *dev);
	struct cyttsp5_sysinfo *(*request_sysinfo)(struct device *dev);
	struct cyttsp5_loader_platform_data
		*(*request_loader_pdata)(struct device *dev);
	int (*request_stop_wd)(struct device *dev);
	int (*request_get_hid_desc)(struct device *dev, int protect);
	int (*request_get_mode)(struct device *dev, int protect, u8 *mode);
	int (*request_enable_scan_type)(struct device *dev, u8 scan_type);
	int (*request_disable_scan_type)(struct device *dev, u8 scan_type);
#ifdef TTHE_TUNER_SUPPORT
	int (*request_tthe_print)(struct device *dev, u8 *buf, int buf_len,
			const u8 *data_name);
#endif
	struct cyttsp5_core_nonhid_cmd *cmd;
};

struct cyttsp5_core_data {
	struct list_head node;
	char core_id[20];
	struct device *dev;
	struct list_head atten_list[CY_ATTEN_NUM_ATTEN];
	struct mutex system_lock;
	struct mutex adap_lock;
	struct mutex hid_report_lock;
	enum cyttsp5_mode mode;
	spinlock_t spinlock;
	struct cyttsp5_mt_data md;
	u16		silicon_id;
	const char	*fw_path;
#ifdef SAMSUNG_FACTORY_TEST
	struct cyttsp5_samsung_factory_data sfd;
#endif
	void *cyttsp5_dynamic_data[CY_MODULE_LAST];
	struct cyttsp5_platform_data *pdata;
	struct cyttsp5_core_platform_data *cpdata;
	const struct cyttsp5_bus_ops *bus_ops;
	wait_queue_head_t wait_q;
	enum cyttsp5_sleep_state sleep_state;
	enum cyttsp5_startup_state startup_state;
	int irq;
	bool irq_enabled;
	bool irq_wake;
	bool touch_wake;
	bool palm_ignore;
	bool stay_awake;
	struct wake_lock report_touch_wake_lock;
	bool irq_disabled;
	u8 easy_wakeup_gesture;
	bool wake_initiated_by_device;
	bool wait_until_wake;
	struct work_struct startup_work;
	struct cyttsp5_sysinfo sysinfo;
#ifdef SAMSUNG_PALM_MOTION
	struct delayed_work work_palm;
#endif
#ifdef SAMSUNG_TSP_INFO
	struct cyttsp5_samsung_tsp_info_dev samsung_tsp_info;
#endif
	cyttsp5_upgrade_firmware_from_builtin upgrade_firmware_from_builtin;
	void *exclusive_dev;
	int exclusive_waits;
	struct work_struct watchdog_work;
	struct timer_list watchdog_timer;
	struct cyttsp5_hid_core hid_core;
	int hid_cmd_state;
	int hid_reset_cmd_state; /* reset can happen any time */
	struct cyttsp5_hid_desc hid_desc;
	struct cyttsp5_hid_report *hid_reports[CY_HID_MAX_REPORTS];
	int num_hid_reports;
#ifdef TTHE_TUNER_SUPPORT
	struct dentry *tthe_debugfs;
	u8 *tthe_buf;
	u32 tthe_buf_len;
	struct mutex tthe_lock;
	u8 tthe_exit;
#endif
	u8 input_buf[CY_MAX_INPUT];
	u8 response_buf[CY_MAX_INPUT];
#ifdef VERBOSE_DEBUG
	u8 pr_buf[CY_MAX_PRBUF_SIZE];
#endif
};

struct cyttsp5_bus_ops {
	u16 bustype;
	int (*read_default)(struct device *dev, void *buf, int size);
	int (*read_default_nosize)(struct device *dev, u8 *buf, u32 max);
	int (*write_read_specific)(struct device *dev, u8 write_len,
			u8 *write_buf, u8 *read_buf);
};

#if defined(TTHE_TUNER_SUPPORT) || defined(SAMSUNG_FACTORY_TEST)
#define CY_CMD_RET_PANEL_IN_DATA_OFFSET	0
#define CY_CMD_RET_PANEL_ELMNT_SZ_MASK	0x07
#define CY_CMD_RET_PANEL_HDR		0x0A
#define CY_CMD_RET_PANEL_ELMNT_SZ_MAX	0x2

enum scan_data_type_list {
	CY_MUT_RAW,
	CY_MUT_BASE,
	CY_MUT_DIFF,
	CY_SELF_RAW,
	CY_SELF_BASE,
	CY_SELF_DIFF,
	CY_BAL_RAW,
	CY_BAL_BASE,
	CY_BAL_DIFF,
};

enum pwc_data_type_list {
	CY_PWC_MUT,
	CY_PWC_SELF,
	CY_PWC_BUTTON,
	CY_PWC_FREQ1,
	CY_PWC_FREQ2,
	CY_PWC_FREQ3,
	CY_PWC_FREQ4,
	CY_PWC_FREQ5,
};
#endif

static inline int cyttsp5_adap_read_default(struct cyttsp5_core_data *cd,
		void *buf, int size)
{
	return cd->bus_ops->read_default(cd->dev, buf, size);
}

static inline int cyttsp5_adap_read_default_nosize(struct cyttsp5_core_data *cd,
		void *buf, int max)
{
	return cd->bus_ops->read_default_nosize(cd->dev, buf, max);
}

static inline int cyttsp5_adap_write_read_specific(struct cyttsp5_core_data *cd,
		u8 write_len, u8 *write_buf, u8 *read_buf)
{
	return cd->bus_ops->write_read_specific(cd->dev, write_len, write_buf,
			read_buf);
}

static inline void *cyttsp5_get_dynamic_data(struct device *dev, int id)
{
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	return cd->cyttsp5_dynamic_data[id];
}

int request_exclusive(struct cyttsp5_core_data *cd, void *ownptr,
		int timeout_ms);
int release_exclusive(struct cyttsp5_core_data *cd, void *ownptr);
int _cyttsp5_request_hid_output_get_param(struct device *dev,
		int protect, u8 param_id, u32 *value);
int _cyttsp5_request_hid_output_set_param(struct device *dev,
		int protect, u8 param_id, u32 value);

static inline int cyttsp5_request_exclusive(struct device *dev, int timeout_ms)
{
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	return request_exclusive(cd, dev, timeout_ms);
}

static inline int cyttsp5_release_exclusive(struct device *dev)
{
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	return release_exclusive(cd, dev);
}

static inline int cyttsp5_request_nonhid_get_param(struct device *dev,
		int protect, u8 param_id, u32 *value)
{
	return _cyttsp5_request_hid_output_get_param(dev, protect, param_id,
			value);
}

static inline int cyttsp5_request_nonhid_set_param(struct device *dev,
		int protect, u8 param_id, u32 value)
{
	return _cyttsp5_request_hid_output_set_param(dev, protect, param_id,
			value);
}

#ifdef VERBOSE_DEBUG
void cyttsp5_pr_buf(struct device *dev, u8 *pr_buf, u8 *dptr, int size,
			   const char *data_name);
#else
#define cyttsp5_pr_buf(a, b, c, d, e) do { } while (0)
#endif

int cyttsp5_probe(const struct cyttsp5_bus_ops *ops, struct device *dev,
		u16 irq, size_t xfer_buf_size);
int cyttsp5_release(struct cyttsp5_core_data *cd);

struct cyttsp5_core_commands *cyttsp5_get_commands(void);
struct cyttsp5_core_data *cyttsp5_get_core_data(char *id);
#ifdef SAMSUNG_TSP_INFO
struct cyttsp5_samsung_tsp_info_dev *
cyttsp5_get_samsung_tsp_info(struct device *dev);
#endif

cyttsp5_upgrade_firmware_from_builtin
cyttsp5_request_upgrade_firmware_from_builtin(struct device *dev);

void cyttsp5_set_upgrade_firmware_from_builtin(struct device *dev,
	cyttsp5_upgrade_firmware_from_builtin upgrade_firmware_from_builtin);

int upgrade_firmware_from_sdcard(struct device *dev,
	const u8 *fw_data, int fw_size);

void cyttsp5_mt_lift_all(struct cyttsp5_mt_data *md);
int cyttsp5_mt_release(struct device *dev);
int cyttsp5_mt_probe(struct device *dev);


#ifdef SAMSUNG_FACTORY_TEST
int cyttsp5_samsung_factory_probe(struct device *dev);
int cyttsp5_samsung_factory_release(struct device *dev);

extern struct class *sec_class;
#else
static inline int cyttsp5_samsung_factory_probe(struct device *dev)
{ return 0; }
static inline int cyttsp5_samsung_factory_release(struct device *dev)
{ return 0; }
#endif

#ifdef CYTTSP5_LOADER
int cyttsp5_loader_probe(struct device *dev);
int cyttsp5_loader_release(struct device *dev);
#else
static inline int cyttsp5_loader_probe(struct device *dev) { return 0; }
static inline int cyttsp5_loader_release(struct device *dev) { return 0; }
#endif
extern struct cyttsp5_loader_platform_data _cyttsp5_loader_platform_data;

#ifdef CYTTSP5_DEVICE_ACCESS
int cyttsp5_device_access_probe(struct device *dev);
int cyttsp5_device_access_release(struct device *dev);
#else
static inline int cyttsp5_device_access_probe(struct device *dev)
{ return 0; }
static inline int cyttsp5_device_access_release(struct device *dev)
{ return 0; }
#endif

int _cyttsp5_subscribe_attention(struct device *dev,
	enum cyttsp5_atten_type type, char id, int (*func)(struct device *),
	int mode);
int _cyttsp5_unsubscribe_attention(struct device *dev,
	enum cyttsp5_atten_type type, char id, int (*func)(struct device *),
	int mode);
struct cyttsp5_sysinfo *_cyttsp5_request_sysinfo(struct device *dev);

extern const struct dev_pm_ops cyttsp5_pm_ops;

int cyttsp5_core_ambient_on(struct device *dev);
int cyttsp5_core_ambient_off(struct device *dev);
int cyttsp5_core_suspend(struct device *dev);
int cyttsp5_core_resume(struct device *dev);

#endif /* _CYTTSP5_REGS_H */
