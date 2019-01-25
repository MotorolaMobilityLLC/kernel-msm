/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef DSI_IRIS2P_DEVICE_H
#define DSI_IRIS2P_DEVICE_H

#define MSMFB_IOCTL_MAGIC 'p'
#define MSMFB_IRIS_OPERATE_CONF _IOW(MSMFB_IOCTL_MAGIC, 201, struct msmfb_iris_operate_value)
#define MSMFB_IRIS_OPERATE_MODE _IOW(MSMFB_IOCTL_MAGIC, 202, struct msmfb_iris_operate_value)
#define MSMFB_IRIS_OPERATE_TOOL _IOW(MSMFB_IOCTL_MAGIC, 203, struct msmfb_iris_operate_value)

struct msmfb_iris_operate_value {
	unsigned int type;
	unsigned int count;
	void *values;
};

enum iris_config_type {
	IRIS_PEAKING = 0,			 // MB3[3:0] | [24]
	IRIS_SHARPNESS = 1, 		 // MB3[7:4] | [25]
	IRIS_MEMC_DEMO = 2, 		 // MB3[9:8] | [26]
	IRIS_PEAKING_DEMO = 3,		 // MB3[11:10] | [27]
	IRIS_GAMMA = 4, 			 // MB3[13:12] | [28]
	IRIS_MEMC_LEVEL = 5,		 // MB3[15:14] | [29]
	IRIS_CONTRAST = 6,			 // MB3[23:16] | [30]
	IRIS_BRIGHTNESS = 7,		 // MB5[14:8]
	IRIS_EXTERNAL_PWM = 8,		 // MB5[15]
	IRIS_DBC_QUALITY = 9,		 // MB5[19:16]
	IRIS_DLV_SENSITIVITY = 10,	 // MB5[31:20]
	IRIS_DBC_CONFIG = 11,		 // MB5
	IRIS_PQ_CONFIG = 12,		 // MB3
	IRIS_MEMC_ENABLE = 13,		 // 0: OFF, 1: ON
	IRIS_MEMC_OPTION = 14,		 // 0: PANEL_RESOLUTION, 1: NATIVE_RESOLUTION, 2: LOW_POWER
	IRIS_LPMEMC_CONFIG = 15,
	IRIS_DCE_LEVEL = 16,		 // MB5[7:4]
	IRIS_USER_DEMO_WND = 17,
	IRIS_MEMC_ACTIVE = 18,
	IRIS_WHITE_LIST_ADD = 19,	 // add white list
	IRIS_WHITE_LIST_RST = 20,	 // reset white list
	IRIS_LAYER_SIZE = 21,		 // layer size
	IRIS_BLACK_BORDER = 22, 	 // 1: has black border, 0: no black border
	IRIS_CINEMA_MODE = 23,		 // MB3[31] | [25]
	IRIS_BLACK_LIST_ADD = 24,	 // add black list
	IRIS_BLACK_LIST_RST = 25,	 // reset black list
	IRIS_COLOR_ADJUST = 26,
	IRIS_TRUE_CUT_CAP = 27, 	 // read only: true-cut capability
	IRIS_TRUE_CUT_DET = 28, 	 // read only true-cut video detected
	IRIS_PT_ENABLE = 29,
	IRIS_BYPASS_ENABLE = 30,
	IRIS_LCE_SETTING = 31,
	IRIS_CM_SETTING = 32,
	IRIS_CHIP_VERSION = 33, 	 // 0x0 : IRIS2, 0x1 : IRIS2-plus
	IRIS_LUX_VALUE = 34,
	IRIS_CCT_VALUE = 35,												//35
	IRIS_READING_MODE = 36,
	IRIS_HDR_SETTING = 50,
	IRIS_SDR_TO_HDR = 51,
	IRIS_GAMMA_TABLE_EN = 58,
	IRIS_FTC_ENABLE = 59,
	IRIS_OLED_BRIGHTNESS = 61,
	IRIS_CMD_PANEL_TE_TWEAK = 80,

	// For config only used in system, start from 100 if possible.
	IRIS_SEND_FRAME = 100,
	IRIS_OSD_PATTERN_SHOW = 101,
	IRIS_DBG_TARGET_PI_REGADDR_SET = 102,
	IRIS_DBG_TARGET_REGADDR_VALUE_GET = 103,
	IRIS_MEMC_ENABLE_FOR_ASUS_CAMERA = 104,
	IRIS_DBG_TARGET_REGADDR_VALUE_SET = 105,
	IRIS_APP_FILTER = 111,
	IRIS_CONFIG_TYPE_MAX
};



/* mode switch */
#define IRIS_MODE_RFB                                0x0
#define IRIS_MODE_FRC_PREPARE                 0x1
#define IRIS_MODE_FRC_PREPARE_DONE       0x2
#define IRIS_MODE_FRC                                0x3
#define IRIS_MODE_FRC_CANCEL                   0x4
#define IRIS_MODE_FRC_PREPARE_RFB          0x5
#define IRIS_MODE_FRC_PREPARE_TIMEOUT	   0x6
#define IRIS_MODE_RFB2FRC                         0x7
#define IRIS_MODE_RFB_PREPARE                  0x8
#define IRIS_MODE_RFB_PREPARE_DONE        0x9
#define IRIS_MODE_RFB_PREPARE_TIMEOUT   0xa
#define IRIS_MODE_FRC2RFB                         0xb
#define IRIS_MODE_PT_PREPARE                    0xc
#define IRIS_MODE_PT_PREPARE_DONE          0xd
#define IRIS_MODE_PT_PREPARE_TIMEOUT     0xe
#define IRIS_MODE_RFB2PT                           0xf
#define IRIS_MODE_PT2RFB                           0x10
#define IRIS_MODE_PT                                  0x11
#define IRIS_MODE_PT2BYPASS                     0x14
#define IRIS_MODE_BYPASS                          0x15
#define IRIS_MODE_BYPASS2PT                     0x16
#define IRIS_MODE_PTLOW_PREPARE             0x17
#define IRIS_MODE_HDR_EN                          0x20 /*make same as the one in libirishwc/pxlw/irisData.h*/



enum iris_oprt_type {
	IRIS_OPRT_CONFIGURE = 1,
	IRIS_OPRT_CONFIGURE_NEW,
	IRIS_OPRT_ROTATION_SET,
	IRIS_OPRT_DYNAMIC_FPS_SET,
	IRIS_OPRT_VIDEO_FRAME_RATE_SET,
	IRIS_OPRT_CONFIGURE_NEW_GET,
	IRIS_OPRT_TOOL_DSI,
	IRIS_OPRT_TOOL_I2C,
	IRIS_OPRT_MODE_SET,
	IRIS_OPRT_MODE_GET,
	IRIS_OPRT_DPORT_WRITEBACK_SKIP,
	IRIS_OPRT_MIPITX_MODESWITCH,
	IRIS_OPRT_GET_FRCTIMING,
	IRIS_OPRT_SET_DRC_SIZE,
	IRIS_OPRT_GET_AVAILABLE_MODE,
	IRIS_OPRT_GET_LCD_CALIBRATE_DATA,
};
#endif
