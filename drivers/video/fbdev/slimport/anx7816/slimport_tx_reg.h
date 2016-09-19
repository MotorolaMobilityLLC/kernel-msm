/*
 * Copyright(c) 2014, Analogix Semiconductor. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _SP_TX_REG_DEF_H
#define _SP_TX_REG_DEF_H

#define TX_P0 0x70
#define TX_P1 0x7A
#define TX_P2 0x72

#define RX_P0 0x7e
#define RX_P1 0x80

/***************************************************************/
/*Register definition of device address 0x7e*/
#define HDMI_RX_PORT_SEL_REG 0x10
#define DDC_EN 0x10
#define TMDS_EN 0x01

#define RX_SRST 0x11
#define VIDEO_RST 0x10
#define HDCP_MAN_RST 0X04
#define TMDS_RST 0X02
#define SW_MAN_RST 0X01

#define HDMI_RX_SYS_STATUS_REG			0X14
#define PWR5V			0X08
#define TMDS_VSYNC_DET			0X04
#define TMDS_CLOCK_DET		0X02
#define TMDS_DE_DET  0X01

#define HDMI_STATUS	0X15
#define DEEP_COLOR_MODE 0X40
#define HDMI_AUD_LAYOUT 0X08
#define MUTE_STAT				0X04
/* #define HDMI_MODE				0X01 */

#define RX_MUTE_CTRL 0X16
#define MUTE_POL	0X04
#define AUD_MUTE	0X02
#define VID_MUTE	0X01

#define HDMI_RX_SYS_CTRL1_REG 0X17

#define RX_SYS_PWDN1 0X18
#define PWDN_CTRL	0X01

#define RX_AEC_CTRL 0X20
#define AVC_OE 0x80
#define AAC_OE 0X40
#define AVC_EN 0X02
#define AAC_EN 0X01

#define RX_AEC_EN0 0X24
#define AEC_EN07 0X80
#define AEC_EN06 0X40
#define AEC_EN05 0X20
#define AEC_EN04 0X10
#define AEC_EN03 0X08
#define AEC_EN02 0X04
#define AEC_EN01 0X02
#define AEC_EN00 0X01

#define RX_AEC_EN1 0X25
#define AEC_EN15 0X80
#define AEC_EN14 0X40
#define AEC_EN13 0X20
#define AEC_EN12 0X10
#define AEC_EN11 0X08
#define AEC_EN10 0X04
#define AEC_EN09 0X02
#define AEC_EN08 0X01

#define RX_AEC_EN2 0X26
#define AEC_EN23 0X80
#define AEC_EN22 0X40
#define AEC_EN21 0X20
#define AEC_EN20 0X10
#define AEC_EN19 0X08
#define AEC_EN18 0X04
#define AEC_EN17 0X02
#define AEC_EN16 0X01


#define HDMI_RX_INT_STATUS1_REG 0X31
#define HDMI_DVI 0X80
#define CKDT_CHANGE 0X40
#define SCDT_CHANGE 0X20
#define PCLK_CHANGE 0X10
#define PLL_UNLOCK  0X08
#define CABLE_UNPLUG 0X04
#define SET_MUTE 0X02
#define SW_INTR 0X01

#define HDMI_RX_INT_STATUS2_REG 0X32
#define AUTH_START 0X80
#define AUTH_DONE 0X40
#define HDCP_ERR 0X20
#define ECC_ERR 0X10
#define AUDIO_SAMPLE_CHANGE 0X01

#define HDMI_RX_INT_STATUS3_REG 0X33
#define AUD_MODE_CHANGE 0X01

#define HDMI_RX_INT_STATUS4_REG 0X34
#define VSYNC_DET 0X80
#define SYNC_POL_CHANGE 0X40
#define V_RES_CHANGE 0X20
#define H_RES_CHANGE 0X10
#define I_P_CHANGE 0X08
#define DP_CHANGE 0X04
#define COLOR_DEPTH_CHANGE 0X02
#define COLOR_MODE_CHANGE 0X01

#define HDMI_RX_INT_STATUS5_REG 0X35
#define VFIFO_OVERFLOW 0X80
#define VFIFO_UNDERFLOW 0X40
#define CTS_N_ERR 0X08
#define NO_AVI 0X02
#define AUDIO_RCV 0X01

#define HDMI_RX_INT_STATUS6_REG 0X36
#define CTS_RCV 0X80
#define NEW_UNR_PKT 0X40
#define NEW_MPEG 0X20
#define NEW_AUD 0X10
#define NEW_SPD 0X08
#define NEW_ACP 0X04
#define NEW_AVI 0X02
#define NEW_CP  0X01

#define HDMI_RX_INT_STATUS7_REG 0X37
#define NO_VSI 0X80
#define HSYNC_DET 0X20
#define NEW_VS 0X10
#define NO_ACP 0X08
#define REF_CLK_CHG 0X04
#define CEC_RX_READY 0X02
#define CEC_TX_DONE 0X01

#define HDMI_RX_PKT_RX_INDU_INT_CTRL 0X3F
#define NEW_VS_CTRL 0X80
#define NEW_UNR 0X40
#define NEW_MPEG 0X20
#define NEW_AUD 0X10
#define NEW_SPD 0X08
#define NEW_ACP 0X04
#define NEW_AVI 0X02


#define HDMI_RX_INT_MASK1_REG 0X41
#define HDMI_RX_INT_MASK2_REG 0X42
#define HDMI_RX_INT_MASK3_REG 0X43
#define HDMI_RX_INT_MASK4_REG 0X44
#define HDMI_RX_INT_MASK5_REG 0X45
#define HDMI_RX_INT_MASK6_REG 0X46
#define HDMI_RX_INT_MASK7_REG 0X47

#define HDMI_RX_TMDS_CTRL_REG1 0X50
#define HDMI_RX_TMDS_CTRL_REG2 0X51
#define HDMI_RX_TMDS_CTRL_REG4 0X53
#define HDMI_RX_TMDS_CTRL_REG5 0X54
#define HDMI_RX_TMDS_CTRL_REG6 0X55
#define HDMI_RX_TMDS_CTRL_REG7 0X56
#define TERM_PD         0x01

#define HDMI_RX_TMDS_CTRL_REG18 0X61
#define PLL_RESET 0x10

#define HDMI_RX_TMDS_CTRL_REG19 0X62
#define HDMI_RX_TMDS_CTRL_REG20 0X63
#define HDMI_RX_TMDS_CTRL_REG21 0X64
#define HDMI_RX_TMDS_CTRL_REG22 0X65


#define HDMI_RX_VIDEO_STATUS_REG1 0x70
#define COLOR_DEPTH     0xF0
#define DEFAULT_PHASE 0X08
#define VIDEO_TYPE 0X04


#define HDMI_RX_HTOTAL_LOW 0X71
#define HDMI_RX_HTOTAL_HIGH 0X72
#define HDMI_RX_VTOTAL_LOW 0X73
#define HDMI_RX_VTOTAL_HIGH 0X74

#define HDMI_RX_HACT_LOW 0X75
#define HDMI_RX_HACT_HIGH 0X76
#define HDMI_RX_VACT_LOW 0X77
#define HDMI_RX_VACT_HIGH 0X78

#define HDMI_RX_V_SYNC_WIDTH 0X79
#define HDMI_RX_V_BACK_PORCH 0X7A
#define HDMI_RX_H_FRONT_PORCH_LOW 0X7B
#define HDMI_RX_H_FRONT_PORCH_HIGH 0X7C

#define HDMI_RX_H_SYNC_WIDTH_LOW 0X7D
#define HDMI_RX_H_SYNC_WIDTH_HIGH 0X7E


#define RX_VID_DATA_RNG 0X83
#define YC_LIMT 0X10
#define OUTPUT_LIMIT_EN 0X08
#define OUTPUT_LIMIT_RANGE 0X04
#define R2Y_INPUT_LIMIT 0X02
#define XVYCC_LIMIT 0X01

#define HDMI_RX_VID_OUTPUT_CTRL3_REG 0X86

#define HDMI_RX_VID_PCLK_CNTR_REG 0X8B

#define HDMI_RX_AUD_IN_CH_STATUS1_REG 0xC7
#define HDMI_RX_AUD_IN_CH_STATUS4_REG 0XCA

#define RX_CEC_CTRL 0XD0
#define CEC_RX_EN 0X08
#define CEC_TX_ST 0X04
#define CEC_PIN_SEL 0X02
#define CEC_RST 0X01

/* new added 2013-02-16 */
#define HDMI_RX_CEC_RX_STATUS_REG 0XD1
#define HDMI_RX_CEC_RX_BUSY 0X80
#define HDMI_RX_CEC_RX_FULL 0X20
#define HDMI_RX_CEC_RX_EMP 0X10

#define HDMI_RX_CEC_TX_STATUS_REG 0XD2
#define HDMI_RX_CEC_TX_BUSY 0X80
#define HDMI_RX_CEC_TX_FAIL 0X40
#define HDMI_RX_CEC_TX_FULL 0X20
#define HDMI_RX_CEC_TX_EMP 0X10


#define HDMI_RX_CEC_FIFO_REG 0XD3

#define RX_CEC_SPEED 0XD4
#define CEC_SPEED_27M 0x40

#define HDMI_RX_HDMI_CRITERIA_REG 0XE1

#define HDMI_RX_HDCP_EN_CRITERIA_REG 0XE2
#define ENC_EN_MODE 0X20

#define RX_CHIP_CTRL 0XE3
#define MAN_HDMI5V_DET 0X08
#define PLLLOCK_CKDT_EN 0X04
#define ANALOG_CKDT_EN 0X02
#define DIGITAL_CKDT_EN 0X01

#define RX_PACKET_REV_STA 0XF3
#define AVI_RCVD 0X40
#define VSI_RCVD 0X20
/***************************************************************/
/*Register definition of device address 0x80*/
#define HDCP_BCAPS_SHADOW 0x2A
#define REPEATER_M 0x40


#define HDMI_RX_HDCP_STATUS_REG 0X3F
#define ADV_CIPHER 0X80
#define LOAD_KEY_DONE 0X40
#define DECRYPT_EN 0X20
#define AUTH_EN 0X10
#define BKSV_DISABLE 0X02
#define CLEAR_RI 0X01

#define HDMI_RX_SPD_TYPE_REG 0X40
#define HDMI_RX_SPD_VER_REG 0X41
#define HDMI_RX_SPD_LEN_REG 0X42
#define HDMI_RX_SPD_CHKSUM_REG 0X43
#define HDMI_RX_SPD_DATA00_REG 0X44

#define HDMI_RX_ACP_HB0_REG 0X60
#define HDMI_RX_ACP_HB1_REG 0X61
#define HDMI_RX_ACP_HB2_REG 0X62
#define HDMI_RX_ACP_DATA00_REG 0X63

#define HDMI_RX_AVI_TYPE_REG 0XA0
#define HDMI_RX_AVI_VER_REG 0XA1
#define HDMI_RX_AVI_LEN_REG 0XA2
#define HDMI_RX_AVI_CHKSUM_REG 0XA3
#define HDMI_RX_AVI_DATA00_REG 0XA4

#define HDMI_RX_AUDIO_TYPE_REG 0XC0
#define HDMI_RX_AUDIO_VER_REG 0XC1
#define HDMI_RX_AUDIO_LEN_REG 0XC2
#define HDMI_RX_AUDIO_CHKSUM_REG 0XC3
#define HDMI_RX_AUDIO_DATA00_REG 0XC4

#define HDMI_RX_MPEG_TYPE_REG 0XE0
#define HDMI_RX_MPEG_VER_REG 0XE1
#define HDMI_RX_MPEG_LEN_REG 0XE2
#define HDMI_RX_MPEG_CHKSUM_REG 0XE3
#define HDMI_RX_MPEG_DATA00_REG 0XE4
#define HDMI_RX_MPEG_DATA03_REG 0XE7
#define HDMI_RX_MPEG_DATA05_REG 0XE9

#define HDMI_RX_SPD_INFO_CTRL 0X5F
#define HDMI_RX_ACP_INFO_CTRL 0X7F

#define HDMI_RX_GENERAL_CTRL 0X9F
#define CLEAR_AVMUTE 0x10
#define SET_AVMUTE 0x01

#define HDMI_RX_MPEG_VS_CTRL 0XDF
#define HDMI_RX_MPEG_VS_INFO_CTRL 0XFF


/***************************************************************/
/*Register definition of device address 0x70*/
#define SP_TX_HDCP_STATUS	0x00
#define SP_TX_HDCP_AUTH_PASS	0x02
#define SP_TX_HDCP_AUTH_FAIL	0x40
/*bit defination*/
#define SP_TX_HDCP_PASSED_STATUS 0x0F
#define TX_HDCP_FUNCTION_ENABLE 0x0F

#define TX_HDCP_CTRL0 0x01
#define STORE_AN 0x80
#define RX_REPEATER  0x40
#define RE_AUTH  0x20
#define SW_AUTH_OK  0x10
#define HARD_AUTH_EN 0x08
#define ENC_EN 0x04
#define BKSV_SRM_PASS  0x02
#define KSVLIST_VLD 0x01

#define SP_TX_HDCP_CTRL1_REG 0x02
#define AINFO_EN 0x04
#define RCV_11_EN 0x02
#define HDCP_11_EN 0x01

#define SP_TX_HDCP_LINK_CHK_FRAME_NUM 0x03
#define SP_TX_HDCP_CTRL2_REG 0x04


#define SP_TX_VID_BLANK_SET1 0X2C
#define SP_TX_VID_BLANK_SET2 0X2D
#define SP_TX_VID_BLANK_SET3 0X2E

#define SP_TX_WAIT_R0_TIME	 0x40
#define SP_TX_LINK_CHK_TIMER 0x41
#define SP_TX_WAIT_KSVR_TIME 0X42

#define HDCP_KEY_STATUS 0x5E


#define M_VID_0 0xC0
#define M_VID_1 0xC1
#define M_VID_2 0xC2
#define N_VID_0 0xC3
#define N_VID_1 0xC4
#define N_VID_2 0xC5
#define HDCP_AUTO_TIMER 0x51
#define HDCP_AUTO_TIMER_VAL 0x00

#define HDCP_KEY_CMD 0x5F
#define DISABLE_SYNC_HDCP 0x04

#define OTP_KEY_PROTECT1 0x60
#define OTP_KEY_PROTECT2 0x61
#define OTP_KEY_PROTECT3 0x62
#define OTP_PSW1 0xa2
#define OTP_PSW2 0x7e
#define OTP_PSW3 0xc6


#define SP_TX_SYS_CTRL1_REG 0x80
#define CHIP_AUTH_RESET 0x80
#define PD_BYPASS_CHIP_AUTH 0x40
#define DET_STA  0x04
#define FORCE_DET 0x02
#define DET_CTRL 0x01

#define SP_TX_SYS_CTRL2_REG 0x81
#define CHA_STA  0x04
#define FORCE_CHA  0x02
#define CHA_CTRL 0x01

#define SP_TX_SYS_CTRL3_REG 0x82
#define HPD_STATUS 0x40
#define F_HPD  0x20
#define HPD_CTRL 0x10
#define STRM_VALID 0x04
#define F_VALID 0x02
#define VALID_CTRL 0x01

#define SP_TX_SYS_CTRL4_REG 0x83
#define ENHANCED_MODE	0x08

#define SP_TX_VID_CTRL 0x84

#define SP_TX_AUD_CTRL	0x87
#define AUD_EN 0x01

#define  I2C_GEN_10US_TIMER0	0x88
#define  I2C_GEN_10US_TIMER1	0x89

#define SP_TX_PKT_EN_REG 0x90
#define AUD_IF_UP 0x80
#define AVI_IF_UD 0x40
#define MPEG_IF_UD 0x20
#define SPD_IF_UD 0x10
#define AUD_IF_EN 0x08
#define AVI_IF_EN 0x04
#define MPEG_IF_EN 0x02
#define SPD_IF_EN 0x01

#define TX_HDCP_CTRL 0x92
#define AUTO_EN	0x80
#define AUTO_START 0x20
#define LINK_POLLING 0x02

#define SP_TX_LINK_BW_SET_REG 0xA0
#define LINK_6P75G 0x19
#define LINK_5P4G 0x14
#define LINK_2P7G 0x0A
#define LINK_1P62G 0x06

#define SP_TX_TRAINING_PTN_SET_REG 0xA2
#define SCRAMBLE_DISABLE 0x20

#define SP_TX_LT_SET_REG 0xA3
#define MAX_PRE_REACH 0x20
#define MAX_DRIVE_REACH 0x04
#define DRVIE_CURRENT_LEVEL1 0x01
#define PRE_EMP_LEVEL1 0x08

/* #define SSC_CTRL_REG1 0xA7 */
/* #define SPREAD_AMP 0x10 */
/*#define MODULATION_FREQ 0x01 */

#define LT_CTRL 0xA8
#define SP_TX_LT_EN	0x01

#define TX_DEBUG1 0xB0
#define FORCE_HPD 0X80
#define HPD_POLLING_DET 0x40
#define HPD_POLLING_EN	0x20
#define DEBUG_PLL_LOCK	0x10
#define FORCE_PLL_LOCK 0X08
#define POLLING_EN 0x02

#define SP_TX_DP_POLLING_PERIOD 0xB3

#define TX_DP_POLLING 0xB4
#define AUTO_POLLING_DISABLE 0x01

#define TX_LINK_DEBUG 0xB8
#define M_VID_DEBUG 0x20
#define NEW_PRBS7 0x10
#define INSERT_ER  0x02
#define PRBS31_EN  0x01

#define DPCD_200 0xB9
#define DPCD_201 0xBA
#define DPCD_202 0xBB
#define DPCD_203 0xBC
#define DPCD_204 0xBD
#define DPCD_205 0xBE

#define SP_TX_PLL_CTRL_REG 0xC7
#define PLL_RST 0x40

#define SP_TX_ANALOG_PD_REG 0xC8
#define MACRO_PD 0x20
#define AUX_PD 0x10
#define CH0_PD 0x01

#define TX_MISC 0xCD
#define EQ_TRAINING_LOOP 0x40


#define SP_TX_DOWN_SPREADING_CTRL1 0xD0
#define SP_TX_SSC_DISABLE 0xC0
#define SP_TX_SSC_DWSPREAD 0x40


#define SP_TX_M_CALCU_CTRL	0xD9
#define M_GEN_CLK_SEL 0x01

#define TX_EXTRA_ADDR 0xCE
#define I2C_STRETCH_DISABLE 0X80
#define I2C_EXTRA_ADDR 0X50

#define SP_TX_AUX_STATUS 0xE0
#define AUX_BUSY  0x10

#define AUX_DEFER_CTRL  0xE2
#define BUF_DATA_COUNT 0xE4

#define AUX_CTRL 0xE5
#define AUX_ADDR_7_0 0xE6
#define AUX_ADDR_15_8  0xE7
#define AUX_ADDR_19_16 0xE8

#define AUX_CTRL2 0xE9
#define ADDR_ONLY_BIT 0x02
#define AUX_OP_EN 0x01

#define SP_TX_3D_VSC_CTRL 0xEA
#define INFO_FRAME_VSC_EN 0x01

#define SP_TX_VSC_DB1 0xEB

#define BUF_DATA_0 0xF0


/***************************************************************/
/*Register definition of device address 0x72*/
#define SP_TX_VND_IDL_REG 0x00
#define SP_TX_VND_IDH_REG 0x01
#define SP_TX_DEV_IDL_REG 0x02
#define SP_TX_DEV_IDH_REG 0x03
#define SP_TX_DEV_REV_REG 0x04

#define SP_POWERD_CTRL_REG 0x05
#define REGISTER_PD	0x80
#define HDCP_PD	0x20
#define AUDIO_PD 0x10
#define VIDEO_PD 0x08
#define LINK_PD 0x04
#define TOTAL_PD 0x02

#define SP_TX_RST_CTRL_REG 0x06
#define MISC_RST 0x80
#define VIDCAP_RST 0x40
#define VIDFIF_RST 0x20
#define AUDFIF_RST 0x10
#define AUDCAP_RST 0x08
#define HDCP_RST 0x04
#define SW_RST 0x02
#define HW_RST 0x01

#define RST_CTRL2 0x07
/* #define SSC_RST 0x80 */
#define AUX_RST	0x04
#define SERDES_FIFO_RST 0x02
#define I2C_REG_RST	0x01

#define VID_CTRL1 0x08
#define VIDEO_EN 0x80
#define VIDEO_MUTE 0x40
#define IN_BIT_SEl 0x04
#define DDR_CTRL 0x02
#define EDGE_CTRL 0x01

#define SP_TX_VID_CTRL2_REG 0x09
#define IN_BPC_12BIT 0x30
#define IN_BPC_10BIT 0x20
#define IN_BPC_8BIT 0x10

#define SP_TX_VID_CTRL3_REG 0x0A
#define HPD_OUT 0x40

#define SP_TX_VID_CTRL5_REG 0x0C
#define CSC_STD_SEL 0x80
#define RANGE_Y2R 0x20
#define CSPACE_Y2R 0x10

#define SP_TX_VID_CTRL6_REG 0x0D
#define VIDEO_PROCESS_EN 0x40
#define UP_SAMPLE 0x02
#define DOWN_SAMPLE 0x01

#define SP_TX_VID_CTRL8_REG 0x0F
#define VID_VRES_TH 0x01

#define SP_TX_TOTAL_LINE_STA_L 0x24
#define SP_TX_TOTAL_LINE_STA_H 0x25
#define SP_TX_ACT_LINE_STA_L 0x26
#define SP_TX_ACT_LINE_STA_H 0x27
#define SP_TX_V_F_PORCH_STA 0x28
#define SP_TX_V_SYNC_STA 0x29
#define SP_TX_V_B_PORCH_STA 0x2A
#define SP_TX_TOTAL_PIXEL_STA_L 0x2B
#define SP_TX_TOTAL_PIXEL_STA_H 0x2C
#define SP_TX_ACT_PIXEL_STA_L 0x2D
#define SP_TX_ACT_PIXEL_STA_H 0x2E
#define SP_TX_H_F_PORCH_STA_L 0x2F
#define SP_TX_H_F_PORCH_STA_H 0x30
#define SP_TX_H_SYNC_STA_L 0x31
#define SP_TX_H_SYNC_STA_H 0x32
#define SP_TX_H_B_PORCH_STA_L 0x33
#define SP_TX_H_B_PORCH_STA_H 0x34

#define SP_TX_DP_ADDR_REG1 0x3E

#define SP_TX_VID_BIT_CTRL0_REG 0x40
#define SP_TX_VID_BIT_CTRL10_REG 0x4a
#define SP_TX_VID_BIT_CTRL20_REG 0x54

#define SP_TX_AVI_TYPE 0x70
#define SP_TX_AVI_VER 0x71
#define SP_TX_AVI_LEN 0x72
#define SP_TX_AVI_DB0 0x73

#define BIT_CTRL_SPECIFIC 0x80
#define ENABLE_BIT_CTRL 0x01

#define SP_TX_AUD_TYPE 0x83
#define SP_TX_AUD_VER 0x84
#define SP_TX_AUD_LEN 0x85
#define SP_TX_AUD_DB0 0x86

#define SP_TX_SPD_TYPE 0x91
#define SP_TX_SPD_VER 0x92
#define SP_TX_SPD_LEN 0x93
#define SP_TX_SPD_DB0 0x94

#define SP_TX_MPEG_TYPE 0xB0
#define SP_TX_MPEG_VER  0xB1
#define SP_TX_MPEG_LEN  0xB2
#define SP_TX_MPEG_DB0 0xB3

#define SP_TX_AUD_CH_STATUS_REG1 0xD0

#define SP_TX_AUD_CH_NUM_REG5 0xD5
#define CH_NUM_8 0xE0
#define AUD_LAYOUT 0x01

#define GPIO_1_CONTROL 0xD6
#define GPIO_1_PULL_UP 0x04
#define GPIO_1_OEN 0x02
#define GPIO_1_DATA 0x01

#define TX_ANALOG_DEBUG2 0xDD
#define POWERON_TIME_1P5MS 0X03

#define TX_PLL_FILTER 0xDF
#define PD_RING_OSC	0x40
/* #define AUX_TERM_50OHM 0x30 */
#define V33_SWITCH_ON 0x08

#define TX_PLL_FILTER5	0xE0
#define SP_TX_ANALOG_CTRL0 0xE1
#define P5V_PROTECT 0X80
#define SHORT_PROTECT 0X40
#define P5V_PROTECT_PD 0X20
#define SHORT_PROTECT_PD 0X10

#define TX_ANALOG_CTRL 0xE5
#define SHORT_DPDM 0X4

#define SP_COMMON_INT_STATUS1 0xF1
#define PLL_LOCK_CHG 0x40
#define VIDEO_FORMAT_CHG 0x08
#define AUDIO_CLK_CHG 0x04
#define VIDEO_CLOCK_CHG 0x02

#define SP_COMMON_INT_STATUS2 0xF2
#define HDCP_AUTH_CHG 0x02
#define HDCP_AUTH_DONE 0x01

#define SP_COMMON_INT_STATUS3 0xF3
#define HDCP_LINK_CHECK_FAIL 0x01

#define SP_COMMON_INT_STATUS4 0xF4
#define PLUG 0x01
#define ESYNC_ERR 0x10
#define HPD_LOST 0x02
#define HPD_CHANGE 0x04

#define SP_TX_INT_STATUS1 0xF7
#define DPCD_IRQ_REQUEST 0x80
#define HPD 0x40
#define TRAINING_Finish 0x20
#define POLLING_ERR 0x10
#define LINK_CHANGE 0x04
#define SINK_CHG 0x08

#define SP_COMMON_INT_MASK1 0xF8
#define SP_COMMON_INT_MASK2 0xF9
#define SP_COMMON_INT_MASK3 0xFA
#define SP_COMMON_INT_MASK4 0xFB
#define SP_INT_MASK	 0xFE
#define SP_TX_INT_CTRL_REG 0xFF


/***************************************************************/
/*Register definition of device address 0x7a*/

#define SP_TX_LT_CTRL_REG0 0x30
#define SP_TX_LT_CTRL_REG1 0x31
#define SP_TX_LT_CTRL_REG2 0x34
#define SP_TX_LT_CTRL_REG3 0x35
#define SP_TX_LT_CTRL_REG4 0x36
#define SP_TX_LT_CTRL_REG5 0x37
#define SP_TX_LT_CTRL_REG6 0x38
#define SP_TX_LT_CTRL_REG7 0x39
#define SP_TX_LT_CTRL_REG8 0x3A
#define SP_TX_LT_CTRL_REG9 0x3B
#define SP_TX_LT_CTRL_REG10 0x40
#define SP_TX_LT_CTRL_REG11 0x41
#define SP_TX_LT_CTRL_REG12 0x44
#define SP_TX_LT_CTRL_REG13 0x45
#define SP_TX_LT_CTRL_REG14 0x46
#define SP_TX_LT_CTRL_REG15 0x47
#define SP_TX_LT_CTRL_REG16 0x48
#define SP_TX_LT_CTRL_REG17 0x49
#define SP_TX_LT_CTRL_REG18 0x4A
#define SP_TX_LT_CTRL_REG19 0x4B

#define SP_TX_AUD_INTERFACE_CTRL0  0x5f
#define AUD_INTERFACE_DISABLE 0x80

#define SP_TX_AUD_INTERFACE_CTRL2 0x60
#define M_AUD_ADJUST_ST 0x04

#define SP_TX_AUD_INTERFACE_CTRL3 0x62
#define SP_TX_AUD_INTERFACE_CTRL4 0x67
#define SP_TX_AUD_INTERFACE_CTRL5 0x68
#define SP_TX_AUD_INTERFACE_CTRL6 0x69

#define OCM_REG3 0x96
#define OCM_RST 0x80

#define FW_VER_REG 0xB7


/***************************************************************/
/*Definition of DPCD*/


/*DPCD 0518*/
#define DOWN_R_TERM_DET _BIT6
#define SRAM_EEPROM_LOAD_DONE _BIT5
#define SRAM_CRC_CHK_DONE _BIT4
#define SRAM_CRC_CHK_PASS _BIT3
#define DOWN_STRM_ENC _BIT2
#define DOWN_STRM_AUTH _BIT1
#define DOWN_STRM_HPD _BIT0


#define DPCD_DPCD_REV  0x00
#define DPCD_MAX_LINK_RATE 0x01

#define DPCD_MAX_LANE_COUNT 0x02
#define ENHANCED_FRAME_CAP 0x80

#define DPCD_MAX_DOWNSPREAD 0x03
#define DPCD_NORP 0x04
#define DPCD_DSPORT_PRESENT 0x05

#define DPCD_LINK_BW_SET 0x00
#define DPCD_LANE_COUNT_SET 0x01
#define ENHANCED_FRAME_EN 0x80

#define DPCD_TRAINING_PATTERN_SET  0x02
#define DPCD_TRAINNIG_LANE0_SET 0x03

#define DPCD_DOWNSPREAD_CTRL  0x07
#define SPREAD_AMPLITUDE 0X10

#define DPCD_SINK_COUNT 0x00
#define DPCD_SERVICE_IRQ_VECTOR  0x01
#define TEST_IRQ  0x02
#define CP_IRQ  0x04
#define SINK_SPECIFIC_IRQ   0x40

#define DPCD_LANE0_1_STATUS  0x02

#define DPCD_LANE_ALIGN_UD  0x04
#define DPCD_SINK_STATUS  0x05

#define DPCD_TEST_Response 0x60
#define TEST_ACK  0x01
#define DPCD_TEST_EDID_Checksum_Write 0x04

#define DPCD_TEST_EDID_Checksum 0x61


#define DPCD_SPECIFIC_INTERRUPT1 0x10
#define DPCD_USER_COMM1 0x22

#define DPCD_SPECIFIC_INTERRUPT2 0x11

#define DPCD_TEST_REQUEST			0x18
#define DPCD_TEST_LINK_RATE			0x19

#define DPCD_TEST_LANE_COUNT			0x20

#define DPCD_PHY_TEST_PATTERN			0x48


#ifdef ANX7730_DEBUG
/* for analogix  downstream DP RX */

#define DP_RX_LINK_BW_SET			0x0a

#define  DP_RX_H_RES_LOW			0x90
#define  DP_RX_H_RES_HIGH			0x91

#define  DP_RX_V_RES_LOW			0x92
#define  DP_RX_V_RES_HIGH			0x93

#define  DP_RX_ACT_PIX_LOW			0x94
#define  DP_RX_ACT_PIX_HIGH			0x95

#define  DP_RX_ACT_LINE_LOW			0x96
#define  DP_RX_ACT_LINE_HIGH			0x97

#define  DP_RX_VSYNC_TO_ACT_LINE		0x98
#define  DP_RX_ACT_LINE_TO_VSYNC		0x99

#define  DP_RX_H_F_PORCH_LOW			0x9b
#define  DP_RX_H_F_PORCH_HIGH			0x9c

#define  DP_RX_HSYNC_WIDTH_LOW			0x9d
#define  DP_RX_HSYNC_WIDTH_HIG			0x9e

#define DP_RX_M_FORCE_VALUE_3			0x0d
#define DP_RX_M_FORCE_VALUE_			0x0e
#define DP_RX_M_FORCE_VALUE_1			0x0F
#define DP_RX_N_FORCE_VALUE_3			0x10
#define DP_RX_N_FORCE_VALUE_2			0x11
#define DP_RX_N_FORCE_VALUE_1			0x12


#define DP_TX_HDCP_LINK_CHK_FRAME_NUM                     0x03
#define DP_TX_HDCP_CONTROL_2_REG                        0x04
#define DP_TX_HDCP_AUTO_MODE_EN                         0x10 /* bit4 */
#define DP_TX_DOWN_HDCP_AUTO_START                  0x20 /* bit5 */

#define DP_RX_SYSTEM_CTRL_1          0x08
#define DP_RX_HW_HDCP_REP_MODE 0x80	/* bit7 */
#define DP_RX_FORCE_MN_VAL            0x40
#endif

#define DPCD_CHARGER_TYPE_OFFSET 0xf7
#define SINK_POWER_DELIVERY 0x32
#define SINK_GOOD_BATTERY   0x34

#endif




