/*
 * SiW touch hal driver - reg. map
 *
 * Copyright (C) 2016 Silicon Works - http://www.siliconworks.co.kr
 * Author: Hyunho Kim <kimhh@siliconworks.co.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#ifndef __SIW_TOUCH_HAL_REG_H
#define __SIW_TOUCH_HAL_REG_H

#include "siw_touch_cfg.h"

#define SPR_CHIP_TEST				(0x041)

#define SPR_CHIP_ID				(0x000)
#define SPR_RST_CTL				(0x006)
#define SPR_BOOT_CTL				(0x00F)
#define SPR_SRAM_CTL				(0x010)
#define SPR_BOOT_STS				(0x011)
#define SPR_SUBDISP_STS				(0x021)

#define SPR_CODE_OFFSET				(0x078)

/* siw_hal_touch_info base addr*/
#define TC_IC_STATUS				(0x200)
#define TC_STS					(0x201)
#define TC_VERSION				(0x242)
#define TC_PRODUCT_ID1				(0x244)
#define TC_PRODUCT_ID2				(0x245)
/* YYMMDDXX format (BCD)*/
#define TC_VERSION_EXT				(0x269)

#define INFO_FPC_TYPE				(0x278)
#define INFO_WFR_TYPE				(0x27B)
#define INFO_CHIP_VERSION			(0x27C)
#define INFO_CG_TYPE				(0x27D)
#define INFO_LOT_NUM				(0x27E)
#define INFO_SERIAL_NUM				(0x27F)
#define INFO_DATE				(0x280)
#define INFO_TIME				(0x281)

/* for abt monitor app. header */
#define CMD_ABT_LOC_X_START_READ		(0x2A6)
#define CMD_ABT_LOC_X_END_READ			(0x2A7)
#define CMD_ABT_LOC_Y_START_READ		(0x2A8)
#define CMD_ABT_LOC_Y_END_READ			(0x2A9)

#define CODE_ACCESS_ADDR			(0x300)
#define DATA_I2CBASE_ADDR			(0x301)
#define PRD_TCM_BASE_ADDR			(0x303)

#define TC_DEVICE_CTL				(0xC00)
#define TC_INTERRUPT_CTL			(0xC01)
#define TC_INTERRUPT_STS			(0xC02)
#define TC_DRIVE_CTL				(0xC03)

#define TCI_FAIL_DEBUG_R			(0x28D)
#define TCI_FAIL_BIT_R				(0x28E)
#define TCI_DEBUG_R				(0x2AE)
#define TCI_ENABLE_W				(0xC20)
#define TCI_FAIL_DEBUG_W			(0xC2C)
#define TCI_FAIL_BIT_W				(0xC2D)

#define TAP_COUNT_W				(0xC21)
#define MIN_INTERTAP_W				(0xC22)
#define MAX_INTERTAP_W				(0xC23)
#define TOUCH_SLOP_W				(0xC24)
#define TAP_DISTANCE_W				(0xC25)
#define INT_DELAY_W				(0xC26)
#define ACT_AREA_X1_W				(0xC27)
#define ACT_AREA_Y1_W				(0xC28)
#define ACT_AREA_X2_W				(0xC29)
#define ACT_AREA_Y2_W				(0xC2A)

#define TCI_DEBUG_FAIL_CTRL			(0xC2C)
#define TCI_DEBUG_FAIL_BUFFER			(0xC2D)
#define TCI_DEBUG_FAIL_STATUS			(0xC70)

#define SWIPE_ENABLE_W				(0xC30)
#define SWIPE_DIST_W				(0xC31)
#define SWIPE_RATIO_THR_W			(0xC32)
#define SWIPE_RATIO_DIST_W			(0xC33)
#define SWIPE_RATIO_PERIOD_W			(0xC34)
#define SWIPE_TIME_MIN_W			(0xC35)
#define SWIPE_TIME_MAX_W			(0xC36)
#define SWIPE_ACT_AREA_X1_W			(0xC37)
#define SWIPE_ACT_AREA_Y1_W			(0xC38)
#define SWIPE_ACT_AREA_X2_W			(0xC39)
#define SWIPE_ACT_AREA_Y2_W			(0xC3A)
#define SWIPE_FAIL_DEBUG_W			(0xC3D)
#define SWIPE_FAIL_DEBUG_R			(0x29D)
#define SWIPE_DEBUG_R				(0x2B6)

/* debug data report mode setting */
#define CMD_RAW_DATA_REPORT_MODE_READ		(0x2A4)
#define CMD_RAW_DATA_COMPRESS_WRITE		(0xC47)
#define CMD_RAW_DATA_REPORT_MODE_WRITE		(0xC49)

/* charger status */
#define SPR_CHARGER_STS				(0xC50)

#define IME_STATE				(0xC51)
#define MAX_DELTA				(0x2AD)
#define TOUCH_MAX_R				(0x2A1)
#define TOUCH_MAX_W				(0xC53)
#define CALL_STATE				(0xC54)

/* production test */
#define TC_TSP_TEST_CTL				(0xC04)
#define TC_TSP_TEST_STS				(0x265)
#define TC_TSP_TEST_PF_RESULT			(0x266)
#define TC_TSP_TEST_OFF_INFO			(0x2FB)

/* Firmware control */
#define TC_FLASH_DN_STS				(0x247)
#define TC_CONFDN_BASE_ADDR			(0x2F9)
#define TC_FLASH_DN_CTL				(0xC05)

/* */
#define RAW_DATA_CTL_READ			(0x2A4)
#define RAW_DATA_CTL_WRITE			(0xC49)
#define SERIAL_DATA_OFFSET			(0x07B)


/* __SIW_SUPPORT_WATCH */
#define EXT_WATCH_FONT_OFFSET			(0x07F)
#define EXT_WATCH_FONT_ADDR			(0x307)
#define EXT_WATCH_FONT_DN_ADDR_INFO		(0x2F7)
#define EXT_WATCH_FONT_CRC			(0xC18)
#define EXT_WATCH_DCS_CTRL			(0xC19)
#define EXT_WATCH_MEM_CTRL			(0xC1A)

#define EXT_WATCH_CTRL				(0x2D2)
#define EXT_WATCH_AREA_X			(0x2D3)
#define EXT_WATCH_AREA_Y			(0x2D4)
#define EXT_WATCH_BLINK_AREA			(0x2D5)
#define EXT_WATCH_LUT				(0x2D6)

/* Watch display off :0, on :1*/
#define EXT_WATCH_DISPLAY_ON			(0xC1B)
#define EXT_WATCH_DISPLAY_STATUS		(0x039)

/* Synchronous current time */
#define EXT_WATCH_RTC_SCT			(0x081)
/* Synchronous current time for milesec*/
#define EXT_WATCH_RTC_SCTCNT			(0x082)
/* Target time for occurring date change int */
/* Current time capture*/
#define EXT_WATCH_RTC_CAPTURE			(0x084)
/* Current time */
#define EXT_WATCH_RTC_CTST			(0x087)
/* end count */
#define EXT_WATCH_RTC_ECNT			(0x088)
/* 0 :zerodisp, 1:h24en, 2:dispmode */
#define EXT_WATCH_HOUR_DISP			(0xC14)
/* Stop :0x00, 1s :0x01, 1.5s :0x10 */
#define EXT_WATCH_BLINK_PRD			(0xC15)
/* Watch RTC Stop :0x10, Start 0x01 */
#define EXT_WATCH_RTC_RUN			(0xC10)
/*Write only*/
#define EXT_WATCH_POSITION			(0xC11)
/*Read only*/
#define EXT_WATCH_POSITION_R			(0x271)
/*Watch state, Read only*/
#define EXT_WATCH_STATE				(0x270)

/* DIC status */
#define SYS_DISPMODE_STATUS			(0x021)
/* __SIW_SUPPORT_WATCH */


/* __SIW_SUPPORT_PRD */
#define PRD_SERIAL_TCM_OFFSET			(0x07C)
#define PRD_TC_MEM_SEL				(0x457)
#define PRD_TC_TEST_MODE_CTL			(0xC6E)
#define PRD_M1_M2_RAW_OFFSET			(0x287)
#define PRD_TUNE_RESULT_OFFSET			(0x289)
#define PRD_OPEN3_SHORT_OFFSET			(0x288)
#define PRD_IC_AIT_START_REG			(0xC6C)
#define PRD_IC_AIT_DATA_READYSTATUS		(0xC64)

#define GLOVE_EN				(0xF22)
#define GRAB_EN					(0xF24)

/* __SIW_SUPPORT_PRD */

struct siw_hal_reg {
	u32 spr_chip_test;
	u32 spr_chip_id;
	u32 spr_rst_ctl;
	u32 spr_boot_ctl;
	u32 spr_sram_ctl;
	u32 spr_boot_status;
	u32 spr_subdisp_status;
	u32 spr_code_offset;
	u32 tc_ic_status;
	u32 tc_status;
	u32 tc_version;
	u32 tc_product_id1;
	u32 tc_product_id2;
	u32 tc_version_ext;
	u32 info_fpc_type;
	u32 info_wfr_type;
	u32 info_chip_version;
	u32 info_cg_type;
	u32 info_lot_num;
	u32 info_serial_num;
	u32 info_date;
	u32 info_time;
	u32 cmd_abt_loc_x_start_read;
	u32 cmd_abt_loc_x_end_read;
	u32 cmd_abt_loc_y_start_read;
	u32 cmd_abt_loc_y_end_read;
	u32 code_access_addr;
	u32 data_i2cbase_addr;
	u32 prd_tcm_base_addr;
	u32 tc_device_ctl;
	u32 tc_interrupt_ctl;
	u32 tc_interrupt_status;
	u32 tc_drive_ctl;
	u32 tci_enable_w;
	u32 tap_count_w;
	u32 min_intertap_w;
	u32 max_intertap_w;
	u32 touch_slop_w;
	u32 tap_distance_w;
	u32 int_delay_w;
	u32 act_area_x1_w;
	u32 act_area_y1_w;
	u32 act_area_x2_w;
	u32 act_area_y2_w;
	u32 tci_debug_fail_ctrl;
	u32 tci_debug_fail_buffer;
	u32 tci_debug_fail_status;
	u32 swipe_enable_w;
	u32 swipe_dist_w;
	u32 swipe_ratio_thr_w;
	u32 swipe_ratio_period_w;
	u32 swipe_ratio_dist_w;
	u32 swipe_time_min_w;
	u32 swipe_time_max_w;
	u32 swipe_act_area_x1_w;
	u32 swipe_act_area_y1_w;
	u32 swipe_act_area_x2_w;
	u32 swipe_act_area_y2_w;
	u32 swipe_fail_debug_r;
	u32 swipe_debug_r;
	u32 cmd_raw_data_report_mode_read;
	u32 cmd_raw_data_compress_write;
	u32 cmd_raw_data_report_mode_write;
	u32 spr_charger_status;
	u32 ime_state;
	u32 max_delta;
	u32 touch_max_w;
	u32 touch_max_r;
	u32 call_state;
	u32 tc_tsp_test_ctl;
	u32 tc_tsp_test_status;
	u32 tc_tsp_test_pf_result;
	u32 tc_tsp_test_off_info;
	u32 tc_flash_dn_status;
	u32 tc_confdn_base_addr;
	u32 tc_flash_dn_ctl;
	u32 raw_data_ctl_read;
	u32 raw_data_ctl_write;
	u32 serial_data_offset;
	/* __SIW_SUPPORT_WATCH */
	u32 ext_watch_font_offset;
	u32 ext_watch_font_addr;
	u32 ext_watch_font_dn_addr_info;
	u32 ext_watch_font_crc;
	u32 ext_watch_dcs_ctrl;
	u32 ext_watch_mem_ctrl;
	u32 ext_watch_ctrl;
	u32 ext_watch_area_x;
	u32 ext_watch_area_y;
	u32 ext_watch_blink_area;
	u32 ext_watch_lut;
	u32 ext_watch_display_on;
	u32 ext_watch_display_status;
	u32 ext_watch_rtc_sct;
	u32 ext_watch_rtc_sctcnt;
	u32 ext_watch_rtc_capture;
	u32 ext_watch_rtc_ctst;
	u32 ext_watch_rtc_ecnt;
	u32 ext_watch_hour_disp;
	u32 ext_watch_blink_prd;
	u32 ext_watch_rtc_run;
	u32 ext_watch_position;
	u32 ext_watch_position_r;
	u32 ext_watch_state;
	u32 sys_dispmode_status;
	/* __SIW_SUPPORT_PRD */
	u32 prd_serial_tcm_offset;
	u32 prd_tc_mem_sel;
	u32 prd_tc_test_mode_ctl;
	u32 prd_m1_m2_raw_offset;
	u32 prd_tune_result_offset;
	u32 prd_open3_short_offset;
	u32 prd_ic_ait_start_reg;
	u32 prd_ic_ait_data_readystatus;
	/* */
	u32 glove_en;
	u32 grab_en;
};

#define cmd_abt_ocd_on_write			cmd_raw_data_report_mode_write
#define cmd_abt_ocd_on_read			cmd_raw_data_report_mode_read

/* Reg. exchange */
struct siw_hal_reg_quirk {
	u32 old_addr;
	u32 new_addr;
};

#endif	/* __SIW_TOUCH_HAL_REG_H */


