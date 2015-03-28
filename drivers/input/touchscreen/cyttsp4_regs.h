/* add cypress new driver ttda-02.03.01.476713 */

/*
 * cyttsp4_regs.h
 * Cypress TrueTouch(TM) Standard Product V4 registers.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2012 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * Author: Aleksej Makarov <aleksej.makarov@sonyericsson.com>
 * Modified by: Cypress Semiconductor to add test modes and commands
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

#ifndef _CYTTSP4_REGS_H
#define _CYTTSP4_REGS_H

/* add a specific firmware for factory version */
#ifndef CONFIG_FACTORY_MODE_FEATURE
#define CY_FW_FILE_NAME			"cyttsp4_fw.bin"
#else
#define CY_FW_FILE_NAME			"cyttsp4_fw_ftm.bin"
#endif

/* fw name for recovery mode */
#define CY_RC_FW_FILE_NAME			"cyttsp4_fw_rc.bin"

#define CY_DEFAULT_ADAP_MAX_XFER	512
#define CY_ADAP_MIN_XFER		140

#define CY_MAX_PRBUF_SIZE		PIPE_BUF
#define CY_PR_TRUNCATED			" truncated..."

#define CY_DEFAULT_CORE_ID		"main_ttsp_core"
#define CY_MAX_NUM_CORE_DEVS		5

#define CY_TMA1036_MAX_TCH		0x0E
#define CY_TMA4XX_MAX_TCH		0x1E

#define IS_BOOTLOADER(hst_mode, reset_detect) \
		((hst_mode) & 0x01 || (reset_detect) != 0)
#define IS_BOOTLOADER_IDLE(hst_mode, reset_detect) \
		((hst_mode) & 0x01 && (reset_detect) & 0x01)

#define GET_HSTMODE(reg)		((reg & 0x70) >> 4)
#define GET_TOGGLE(reg)			((reg & 0x80) >> 7)

#define IS_LITTLEENDIAN(reg)		((reg & 0x01) == 1)
#define GET_PANELID(reg)		(reg & 0x07)

#define HI_BYTE(x)			(u8)(((x) >> 8) & 0xFF)
#define LO_BYTE(x)			(u8)((x) & 0xFF)

#define CY_REG_BASE			0x00
#define CY_NUM_REVCTRL			8
#define CY_NUM_DDATA			32
#define CY_NUM_MDATA			64

#define CY_REG_CAT_CMD			2
#define CY_CMD_COMPLETE_MASK		(1 << 6)
#define CY_CMD_MASK			0x3F

#define CY_TTCONFIG_VERSION_OFFSET	8
#define CY_TTCONFIG_VERSION_SIZE	2
#define CY_TTCONFIG_VERSION_ROW		0

#define CY_CONFIG_LENGTH_INFO_OFFSET	0
#define CY_CONFIG_LENGTH_INFO_SIZE	4
#define CY_CONFIG_LENGTH_OFFSET		0
#define CY_CONFIG_LENGTH_SIZE		2
#define CY_CONFIG_MAXLENGTH_OFFSET	2
#define CY_CONFIG_MAXLENGTH_SIZE	2

enum cyttsp4_ic_ebid {
	CY_TCH_PARM_EBID,
	CY_MDATA_EBID,
	CY_DDATA_EBID,
	CY_EBID_NUM,
};

/* touch record system information offset masks and shifts */
#define CY_BYTE_OFS_MASK		0x1F
#define CY_BOFS_MASK			0xE0
#define CY_BOFS_SHIFT			5

/* helpers */
#define GET_NUM_TOUCH_RECORDS(x)	((x) & 0x1F)
#define IS_LARGE_AREA(x)		((x) & 0x20)
#define IS_BAD_PKT(x)			((x) & 0x20)

#define HUAWEI_GESTURE_DETECT
#ifndef HUAWEI_GESTURE_DETECT
#define IS_TTSP_VER_GE(p, maj, min) \
		((p)->si_ptrs.cydata == NULL ? \
		0 : \
		((p)->si_ptrs.cydata->ttsp_ver_major < (maj) ? \
			0 : \
			((p)->si_ptrs.cydata->ttsp_ver_minor < (min) ? \
				0 : \
				1)))
#else
#define IS_TTSP_VER_GE(p, max, min) (1)
#endif

/* Timeout in ms. */
#define CY_COMMAND_COMPLETE_TIMEOUT	5000
#define CY_CALIBRATE_COMPLETE_TIMEOUT	5000
#define CY_WATCHDOG_TIMEOUT		3000

/* drv_debug commands */
#define CY_DBG_SUSPEND			4
#define CY_DBG_RESUME			5
#define CY_DBG_SOFT_RESET		97
#define CY_DBG_RESET			98

/* scan types */
#define CY_SCAN_TYPE_GLOVE		0x8
#define CY_SCAN_TYPE_STYLUS		0x10
#define CY_SCAN_TYPE_PROXIMITY		0x40
#define CY_SCAN_TYPE_APA_MC		0x80

enum cyttsp4_hst_mode_bits {
	CY_HST_TOGGLE = (1 << 7),
	CY_HST_MODE_CHANGE = (1 << 3),
	CY_HST_DEVICE_MODE = (7 << 4),
	CY_HST_OPERATE = (0 << 4),
	CY_HST_SYSINFO = (1 << 4),
	CY_HST_CAT = (2 << 4),
	CY_HST_LOWPOW = (1 << 2),
	CY_HST_SLEEP = (1 << 1),
	CY_HST_RESET = (1 << 0),
};

enum cyttsp_cmd_bits {
	CY_CMD_COMPLETE = (1 << 6),
};

/*some define for cypress */
enum cyttsp4_cmd_operate {
	CY_CMD_OP_NULL,
	CY_CMD_OP_RESERVED_1,
	CY_CMD_OP_GET_PARA,
	CY_CMD_OP_SET_PARA,
	CY_CMD_OP_RESERVED_2,
	CY_CMD_OP_GET_CRC,
	CY_CMD_OP_WAIT_FOR_EVENT,	//huawei 0701
	CY_CMD_OP_RESERVED_N,
};

#define CY_OP_PARA_SCAN_TYPE			0x4B
/* change command parameter */
#define CY_OP_PARA_FINGER_THRESHOLD		0x93

#define CY_OP_PARA_SCAN_TYPE_SZ			1
#define CY_OP_PARA_FINGER_THRESHOLD_SZ		2

#define CY_OP_PARA_FINGER_THRESHOLD_MIN_VAL	1
#define CY_OP_PARA_FINGER_THRESHOLD_MAX_VAL	2000

#define CY_OP_PARA_SCAN_TYPE_NORMAL		0
#define CY_OP_PARA_SCAN_TYPE_GLOVE_MASK	(1<<3)
#define CY_OP_PARA_SCAN_TYPE_STYLUS_MASK	(1<<4)
#define CY_OP_PARA_SCAN_TYPE_HOVER_MASK		(1<<5)
#define CY_OP_PARA_SCAN_TYPE_PROXIMITY_MASK	(1<<6)
#define CY_OP_PARA_SCAN_TYPE_APAMC_MASK		(1<<7)

enum cyttsp4_signal_disparity {
	CY_SIGNAL_DISPARITY_NONE,
	CY_SIGNAL_DISPARITY_SENSITIVITY,
	CY_SIGNAL_DISPARITY_PROXIMITY,
	CY_SIGNAL_DISPARITY_MAX,
};

enum cyttsp4_cmd_cat {
	CY_CMD_CAT_NULL,
	CY_CMD_CAT_RESERVED_1,
	CY_CMD_CAT_GET_CFG_ROW_SZ,
	CY_CMD_CAT_READ_CFG_BLK,
	CY_CMD_CAT_WRITE_CFG_BLK,
	CY_CMD_CAT_RESERVED_2,
	CY_CMD_CAT_LOAD_SELF_TEST_DATA,
	CY_CMD_CAT_RUN_SELF_TEST,
	CY_CMD_CAT_GET_SELF_TEST_RESULT,
	CY_CMD_CAT_CALIBRATE_IDACS,
	CY_CMD_CAT_INIT_BASELINES,
	CY_CMD_CAT_EXEC_PANEL_SCAN,
	CY_CMD_CAT_RETRIEVE_PANEL_SCAN,
	CY_CMD_CAT_START_SENSOR_DATA_MODE,
	CY_CMD_CAT_STOP_SENSOR_DATA_MODE,
	CY_CMD_CAT_INT_PIN_MODE,
	CY_CMD_CAT_RETRIEVE_DATA_STRUCTURE,
	CY_CMD_CAT_VERIFY_CFG_BLK_CRC,
	CY_CMD_CAT_RESERVED_N,
};

enum cyttsp4_cmd_status {
	CY_CMD_STATUS_SUCCESS,
	CY_CMD_STATUS_FAILURE,
};

/* Operational Mode Command Sizes */
/* NULL Command */
#define CY_CMD_OP_NULL_CMD_SZ			1
#define CY_CMD_OP_NULL_RET_SZ			0
/* Get Parameter */
#define CY_CMD_OP_GET_PARAM_CMD_SZ		2
#define CY_CMD_OP_GET_PARAM_RET_SZ		6
/* Set Parameter */
#define CY_CMD_OP_SET_PARAM_CMD_SZ		7
#define CY_CMD_OP_SET_PARAM_RET_SZ		2
/* Get Config Block CRC */
#define CY_CMD_OP_GET_CFG_BLK_CRC_CMD_SZ	2
#define CY_CMD_OP_GET_CFG_BLK_CRC_RET_SZ	3
/* Wait For Event */
#define CY_CMD_OP_WAIT_FOR_EVENT_CMD_SZ		2

/* CaT Mode Command Sizes */
/* NULL Command */
#define CY_CMD_CAT_NULL_CMD_SZ			1
#define CY_CMD_CAT_NULL_RET_SZ			0
/* Get Config Row Size */
#define CY_CMD_CAT_GET_CFG_ROW_SIZE_CMD_SZ	1
#define CY_CMD_CAT_GET_CFG_ROW_SIZE_RET_SZ	2
/* Read Config Block */
#define CY_CMD_CAT_READ_CFG_BLK_CMD_SZ		6
#define CY_CMD_CAT_READ_CFG_BLK_RET_SZ		7	/* + Data */
#define CY_CMD_CAT_READ_CFG_BLK_RET_HDR_SZ	5
/* Write Config Block */
#define CY_CMD_CAT_WRITE_CFG_BLK_CMD_SZ		8	/* + Data + Security Key */
#define CY_CMD_CAT_WRITE_CFG_BLK_RET_SZ		5
#define CY_CMD_CAT_WRITE_CFG_BLK_CMD_HDR_SZ	6
/* Load Self-Test Data */
#define CY_CMD_CAT_LOAD_SELFTEST_DATA_CMD_SZ	6
#define CY_CMD_CAT_LOAD_SELFTEST_DATA_RET_SZ	5	/* + Data */
/* Run Self-Test */
#define CY_CMD_CAT_RUN_SELFTEST_CMD_SZ		2
#define CY_CMD_CAT_RUN_SELFTEST_RET_SZ		3
/* Calibrate IDACs */
#define CY_CMD_CAT_CALIBRATE_IDAC_CMD_SZ	2
#define CY_CMD_CAT_CALIBRATE_IDAC_RET_SZ	1
/* Get Self-Test Results */
#define CY_CMD_CAT_GET_SELFTEST_RES_CMD_SZ	6
#define CY_CMD_CAT_GET_SELFTEST_RES_RET_SZ	5	/* + Data */
/* Initialize Baselines */
#define CY_CMD_CAT_INIT_BASELINE_CMD_SZ		2
#define CY_CMD_CAT_INIT_BASELINE_RET_SZ		1
/* Execute Panel Scan */
#define CY_CMD_CAT_EXECUTE_PANEL_SCAN_CMD_SZ	1
#define CY_CMD_CAT_EXECUTE_PANEL_SCAN_RET_SZ	1
/* Retrieve Panel Scan */
#define CY_CMD_CAT_RETRIEVE_PANEL_SCAN_CMD_SZ	6
#define CY_CMD_CAT_RETRIEVE_PANEL_SCAN_RET_SZ	5	/* + Data */
/* Start Sensor Data Mode */
#define CY_CMD_CAT_START_SENSOR_MODE_CMD_SZ	1	/* + Data */
#define CY_CMD_CAT_START_SENSOR_MODE_RET_SZ	0	/* + Data */
/* Stop Sensor Data Mode */
#define CY_CMD_CAT_STOP_SENSOR_MODE_CMD_SZ	1
#define CY_CMD_CAT_STOP_SENSOR_MODE_RET_SZ	0
/* Interrupt Pin Override */
#define CY_CMD_CAT_INT_PIN_OVERRIDE_CMD_SZ	2
#define CY_CMD_CAT_INT_PIN_OVERRIDE_RET_SZ	1
/* Retrieve Data Structure */
#define CY_CMD_CAT_RETRIEVE_DATA_STRUCT_CMD_SZ	6
#define CY_CMD_CAT_RETRIEVE_DATA_STRUCT_RET_SZ	5	/* + Data */
/* Verify Config Block CRC */
#define CY_CMD_CAT_VERIFY_CFG_BLK_CRC_CMD_SZ	2
#define CY_CMD_CAT_VERIFY_CFG_BLK_CRC_RET_SZ	5

#define CY_RAM_ID_ACTIVE_DISTANCE		0x4A
#define CY_RAM_ID_SCAN_TYPE			0x4B
#define CY_RAM_ID_LOW_POWER_INTERVAL		0x4C
#define CY_RAM_ID_REFRESH_INTERVAL		0x4D
#define CY_RAM_ID_ACTIVE_MODE_TIMEOUT		0x4E
#define CY_RAM_ID_ACTIVE_LFT_INTERVAL		0x4F
#define CY_RAM_ID_ACTIVE_DISTANCE2		0x50
#define CY_RAM_ID_CHARGER_STATUS		0x51
#define CY_RAM_ID_IMO_TRIM_VALUE		0x52
#define CY_RAM_ID_FINGER_THRESHOLH		0x93
#define CY_RAM_ID_DETECT_AREA_MARGIN_X		0x58
#define CY_RAM_ID_DETECT_AREA_MARGIN_Y		0x59
#define CY_RAM_ID_GRIP_XEDGE_A			0x70
#define CY_RAM_ID_GRIP_XEDGE_B			0x71
#define CY_RAM_ID_GRIP_XEXC_A			0x72
#define CY_RAM_ID_GRIP_XEXC_B			0x73
#define CY_RAM_ID_GRIP_YEDGE_A			0x74
#define CY_RAM_ID_GRIP_YEDGE_B			0x75
#define CY_RAM_ID_GRIP_YEXC_A			0x76
#define CY_RAM_ID_GRIP_YEXC_B			0x77
#define CY_RAM_ID_GRIP_FIRST_EXC		0x78
#define CY_RAM_ID_GRIP_EXC_EDGE_ORIGIN		0x79
#define CY_RAM_ID_PROX_ACTIVE_DIST_Z_VALUE	0x9B

enum cyttsp4_scan_type {
	CY_ST_APA_MC,
	CY_ST_GLOVE,
	CY_ST_STYLUS,
	CY_ST_PROXIMITY,
};

enum cyttsp4_mode {
	CY_MODE_UNKNOWN = 0,
	CY_MODE_BOOTLOADER = (1 << 1),
	CY_MODE_OPERATIONAL = (1 << 2),
	CY_MODE_SYSINFO = (1 << 3),
	CY_MODE_CAT = (1 << 4),
	CY_MODE_STARTUP = (1 << 5),
	CY_MODE_LOADER = (1 << 6),
	CY_MODE_CHANGE_MODE = (1 << 7),
	CY_MODE_CHANGED = (1 << 8),
	CY_MODE_CMD_COMPLETE = (1 << 9),
};

enum cyttsp4_int_state {
	CY_INT_NONE,
	CY_INT_IGNORE = (1 << 0),
	CY_INT_MODE_CHANGE = (1 << 1),
	CY_INT_EXEC_CMD = (1 << 2),
	CY_INT_AWAKE = (1 << 3),
};

enum cyttsp4_ic_grpnum {
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
	CY_IC_GRPNUM_NUM
};

enum cyttsp4_event_id {
	CY_EV_NO_EVENT,
	CY_EV_TOUCHDOWN,
	CY_EV_MOVE,		/* significant displacement (> act dist) */
	CY_EV_LIFTOFF,		/* record reports last position */
};

enum cyttsp4_object_id {
	CY_OBJ_STANDARD_FINGER = 0,
	CY_OBJ_PROXIMITY = 1,
	CY_OBJ_STYLUS = 2,
	CY_OBJ_GLOVE = 4,
};

#define CY_POST_CODEL_WDG_RST           0x01
#define CY_POST_CODEL_CFG_DATA_CRC_FAIL 0x02
#define CY_POST_CODEL_PANEL_TEST_FAIL   0x04

/* test mode NULL command driver codes */
enum cyttsp4_null_test_cmd_code {
	CY_NULL_CMD_NULL,
	CY_NULL_CMD_MODE,
	CY_NULL_CMD_STATUS_SIZE,
	CY_NULL_CMD_HANDSHAKE,
	CY_NULL_CMD_LOW_POWER,
};

enum cyttsp4_test_mode {
	CY_TEST_MODE_NORMAL_OP,	/* Send touch data to OS; normal op */
	CY_TEST_MODE_CAT,	/* Configuration and Test */
	CY_TEST_MODE_SYSINFO,	/* System information mode */
	CY_TEST_MODE_CLOSED_UNIT,	/* Send scan data to sysfs */
};

struct cyttsp4_test_mode_params {
	int cur_mode;
	int cur_cmd;
	u32 cur_status_size;
};

/* GEN4/SOLO Operational interface definitions */
/* TTSP System Information interface definitions */
struct cyttsp4_cydata {
	u8 ttpidh;
	u8 ttpidl;
	u8 fw_ver_major;
	u8 fw_ver_minor;
	u8 revctrl[CY_NUM_REVCTRL];
	u8 blver_major;
	u8 blver_minor;
	u8 jtag_si_id3;
	u8 jtag_si_id2;
	u8 jtag_si_id1;
	u8 jtag_si_id0;
	u8 mfgid_sz;
	u8 cyito_idh;
	u8 cyito_idl;
	u8 cyito_verh;
	u8 cyito_verl;
	u8 ttsp_ver_major;
	u8 ttsp_ver_minor;
	u8 device_info;
	u8 mfg_id[];
} __packed;

struct cyttsp4_test {
	u8 post_codeh;
	u8 post_codel;
} __packed;

struct cyttsp4_pcfg {
	u8 electrodes_x;
	u8 electrodes_y;
	u8 len_xh;
	u8 len_xl;
	u8 len_yh;
	u8 len_yl;
	u8 res_xh;
	u8 res_xl;
	u8 res_yh;
	u8 res_yl;
	u8 max_zh;
	u8 max_zl;
	u8 panel_info0;
} __packed;

enum cyttsp4_tch_abs {		/* for ordering within the extracted touch data array */
	CY_TCH_X,		/* X */
	CY_TCH_Y,		/* Y */
	CY_TCH_P,		/* P (Z) */
	CY_TCH_T,		/* TOUCH ID */
	CY_TCH_E,		/* EVENT ID */
	CY_TCH_O,		/* OBJECT ID */
	CY_TCH_W,		/* SIZE */
	CY_TCH_MAJ,		/* TOUCH_MAJOR */
	CY_TCH_MIN,		/* TOUCH_MINOR */
	CY_TCH_OR,		/* ORIENTATION */
	CY_TCH_NUM_ABS
};

static const char *const cyttsp4_tch_abs_string[] = {
	[CY_TCH_X] = "X",
	[CY_TCH_Y] = "Y",
	[CY_TCH_P] = "P",
	[CY_TCH_T] = "T",
	[CY_TCH_E] = "E",
	[CY_TCH_O] = "O",
	[CY_TCH_W] = "W",
	[CY_TCH_MAJ] = "MAJ",
	[CY_TCH_MIN] = "MIN",
	[CY_TCH_OR] = "OR",
	[CY_TCH_NUM_ABS] = "INVALID"
};

#define CY_NUM_TCH_FIELDS		7
#define CY_NUM_EXT_TCH_FIELDS		3

struct cyttsp4_tch_rec_params {
	u8 loc;
	u8 size;
} __packed;

struct cyttsp4_opcfg {
	u8 cmd_ofs;
	u8 rep_ofs;
	u8 rep_szh;
	u8 rep_szl;
	u8 num_btns;
	u8 tt_stat_ofs;
	u8 obj_cfg0;
	u8 max_tchs;
	u8 tch_rec_size;
	struct cyttsp4_tch_rec_params tch_rec_old[CY_NUM_TCH_FIELDS];
	u8 btn_rec_size;	/* btn record size (in bytes) */
	u8 btn_diff_ofs;	/* btn data loc ,diff counts, (Op-Mode byte ofs) */
	u8 btn_diff_size;	/* btn size of diff counts (in bits) */
	struct cyttsp4_tch_rec_params tch_rec_new[CY_NUM_EXT_TCH_FIELDS];
	u8 noise_data_ofs;
	u8 noise_data_sz;
} __packed;

struct cyttsp4_sysinfo_data {
	u8 hst_mode;
	u8 reserved;
	u8 map_szh;
	u8 map_szl;
	u8 cydata_ofsh;
	u8 cydata_ofsl;
	u8 test_ofsh;
	u8 test_ofsl;
	u8 pcfg_ofsh;
	u8 pcfg_ofsl;
	u8 opcfg_ofsh;
	u8 opcfg_ofsl;
	u8 ddata_ofsh;
	u8 ddata_ofsl;
	u8 mdata_ofsh;
	u8 mdata_ofsl;
} __packed;

struct cyttsp4_sysinfo_ptr {
	struct cyttsp4_cydata *cydata;
	struct cyttsp4_test *test;
	struct cyttsp4_pcfg *pcfg;
	struct cyttsp4_opcfg *opcfg;
	struct cyttsp4_ddata *ddata;
	struct cyttsp4_mdata *mdata;
} __packed;

struct cyttsp4_touch {
	int abs[CY_TCH_NUM_ABS];
};

struct cyttsp4_tch_abs_params {
	u32 ofs;		/* abs byte offset */
	u32 size;		/* size in bits */
	u32 max;		/* max value */
	u32 bofs;		/* bit offset */
};

#define CY_NORMAL_ORIGIN		0	/* upper, left corner */
#define CY_INVERT_ORIGIN		1	/* lower, right corner */

struct cyttsp4_sysinfo_ofs {
	u32 chip_type;
	u32 cmd_ofs;
	u32 rep_ofs;
	u32 rep_sz;
	u32 num_btns;
	u32 num_btn_regs;	/* ceil(num_btns/4) */
	u32 tt_stat_ofs;
	u32 tch_rec_size;
	u32 obj_cfg0;
	u32 max_tchs;
	u32 mode_size;
	u32 data_size;
	u32 rep_hdr_size;
	u32 map_sz;
	u32 max_x;
	u32 x_origin;		/* left or right corner */
	u32 max_y;
	u32 y_origin;		/* upper or lower corner */
	u32 max_p;
	u32 cydata_ofs;
	u32 test_ofs;
	u32 pcfg_ofs;
	u32 opcfg_ofs;
	u32 ddata_ofs;
	u32 mdata_ofs;
	u32 cydata_size;
	u32 test_size;
	u32 pcfg_size;
	u32 opcfg_size;
	u32 ddata_size;
	u32 mdata_size;
	u32 btn_keys_size;
	struct cyttsp4_tch_abs_params tch_abs[CY_TCH_NUM_ABS];
	u32 btn_rec_size;	/* btn record size (in bytes) */
	u32 btn_diff_ofs;	/* btn data loc ,diff counts, (Op-Mode byte ofs) */
	u32 btn_diff_size;	/* btn size of diff counts (in bits) */
	u32 noise_data_ofs;
	u32 noise_data_sz;
};

/* button to keycode support */
#define CY_NUM_BTN_PER_REG		4
#define CY_BITS_PER_BTN			2

enum cyttsp4_btn_state {
	CY_BTN_RELEASED = 0,
	CY_BTN_PRESSED = 1,
	CY_BTN_NUM_STATE
};

struct cyttsp4_btn {
	bool enabled;
	int state;		/* CY_BTN_PRESSED, CY_BTN_RELEASED */
	int key_code;
};

struct cyttsp4_ttconfig {
	u16 version;
	u16 length;
	u16 max_length;
	u16 crc;
};

struct cyttsp4_sysinfo {
	bool ready;
	struct cyttsp4_sysinfo_data si_data;
	struct cyttsp4_sysinfo_ptr si_ptrs;
	struct cyttsp4_sysinfo_ofs si_ofs;
	struct cyttsp4_ttconfig ttconfig;
	struct cyttsp4_btn *btn;	/* button states */
	u8 *btn_rec_data;	/* button diff count data */
	u8 *xy_mode;		/* operational mode and status regs */
	u8 *xy_data;		/* operational touch regs */
	u32 wakeup_event_id;	/*for gesture detection */
};

/* tp module defination for huawei product */
enum cyttsp4_module_name {
	/*MODULE      ID2    ID1    ID0    ID_CODE */
	OFILM = 0,		/*GND    GND    GND    000     */
	ECW = 1,		/*GND    GND    NC     001     */
	TRULY = 2,		/*GND    NC     GND    010     */
	MUTTO = 3,		/*GND    NC     NC     011     */
	GIS = 4,		/*NC     GND    GND    100     */
	JUNDA = 5,		/*NC     GND    NC     101     */
	/*NC     NC     GND    110     */
	/*NC     NC     NC     111     */
	UNKNOWN_TP
};

#endif /* _CYTTSP4_REGS_H */

