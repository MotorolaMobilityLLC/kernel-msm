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

#ifndef _SP_TX_DRV_H
#define _SP_TX_DRV_H

/*---SlimPort macro define for some feature---*/
/* #define DEMO_4K_2K */

/* #define SIMULATE_WATCHDOG */
/*---End---*/
#include "slimport.h"
#include "slimport_tx_reg.h"

/* #define CEC_PHYCISAL_ADDRESS_INSERT */
/* #define CEC_DBG_MSG_ENABLED */

#ifdef DEMO_4K_2K
#define FW_VERSION 0xA4
#else
#define FW_VERSION 0x16
#define FW_REVERSION 0x00
#endif
#define _BIT0	0x01
#define _BIT1	0x02
#define _BIT2	0x04
#define _BIT3	0x08
#define _BIT4	0x10
#define _BIT5	0x20
#define _BIT6	0x40
#define _BIT7	0x80

#define _bit0_(val)  ((bit)(val & _BIT0))
#define _bit1_(val)  ((bit)(val & _BIT1))
#define _bit2_(val)  ((bit)(val & _BIT2))
#define _bit3_(val)  ((bit)(val & _BIT3))
#define _bit4_(val)  ((bit)(val & _BIT4))
#define _bit5_(val)  ((bit)(val & _BIT5))
#define _bit6_(val)  ((bit)(val & _BIT6))
#define _bit7_(val)  ((bit)(val & _BIT7))

#ifdef SP_REGISTER_SET_TEST
/* For Slimport test */
extern unchar val_SP_TX_LT_CTRL_REG0;
extern unchar val_SP_TX_LT_CTRL_REG10;
extern unchar val_SP_TX_LT_CTRL_REG11;
extern unchar val_SP_TX_LT_CTRL_REG2;
extern unchar val_SP_TX_LT_CTRL_REG12;
extern unchar val_SP_TX_LT_CTRL_REG1;
extern unchar val_SP_TX_LT_CTRL_REG6;
extern unchar val_SP_TX_LT_CTRL_REG16;
extern unchar val_SP_TX_LT_CTRL_REG5;
extern unchar val_SP_TX_LT_CTRL_REG8;
extern unchar val_SP_TX_LT_CTRL_REG15;
extern unchar val_SP_TX_LT_CTRL_REG18;
#endif

#define DVI_MODE 0x00
#define HDMI_MODE 0x01
#define ENABLE_READ_EDID


enum CO3_CHIPID {
	ANX7818,
	ANX7816,
	ANX7812,
	ANX7810,
	ANX7806,
	ANX7802,
	CO3_NUMS,
};

enum RX_CBL_TYPE {
	DWN_STRM_IS_NULL,
	DWN_STRM_IS_HDMI,
	DWN_STRM_IS_DIGITAL,
	DWN_STRM_IS_ANALOG,
	DWN_STRM_IS_VGA_9832 = 0x04
};

enum CHARGING_STATUS {
	NO_CHARGING_CAPABLE = 0x00,
	NO_FAST_CHARGING = 0x01,
	FAST_CHARGING = 0x02,
	ERR_STATUS
};

enum SP_TX_System_State {
	STATE_INIT,
	STATE_WAITTING_CABLE_PLUG,
	STATE_SP_INITIALIZED,
	STATE_SINK_CONNECTION,
	#ifdef ENABLE_READ_EDID
	STATE_PARSE_EDID,
	#endif
	STATE_LINK_TRAINING,
	STATE_VIDEO_OUTPUT,
	STATE_HDCP_AUTH,
	STATE_AUDIO_OUTPUT,
	STATE_PLAY_BACK
};


enum SP_TX_POWER_BLOCK {
	SP_TX_PWR_REG = REGISTER_PD,
	SP_TX_PWR_HDCP = HDCP_PD,
	SP_TX_PWR_AUDIO = AUDIO_PD,
	SP_TX_PWR_VIDEO = VIDEO_PD,
	SP_TX_PWR_LINK = LINK_PD,
	SP_TX_PWR_TOTAL = TOTAL_PD,
	SP_TX_PWR_NUMS
};
enum HDMI_color_depth {
	Hdmi_legacy = 0x00,
	Hdmi_24bit = 0x04,
	Hdmi_30bit = 0x05,
	Hdmi_36bit = 0x06,
	Hdmi_48bit = 0x07,
};

enum SP_TX_SEND_MSG {
	MSG_OCM_EN,
	MSG_INPUT_HDMI,
	MSG_INPUT_DVI,
	MSG_CLEAR_IRQ,
};

enum SINK_CONNECTION_STATUS {
	SC_INIT,
	SC_CHECK_CABLE_TYPE,
	SC_WAITTING_CABLE_TYPE = SC_CHECK_CABLE_TYPE + 5,
	SC_SINK_CONNECTED,
	SC_NOT_CABLE,
	SC_STATE_NUM
};
enum CABLE_TYPE_STATUS {
	CHECK_AUXCH,
	GETTED_CABLE_TYPE,
	CABLE_TYPE_STATE_NUM
};

enum SP_TX_LT_STATUS {
	LT_INIT,
	LT_WAIT_PLL_LOCK,
	LT_CHECK_LINK_BW,
	LT_START,
	LT_WAITTING_FINISH,
	LT_ERROR,
	LT_FINISH,
	LT_END,
	LT_STATES_NUM
};

enum HDCP_STATUS {
	HDCP_CAPABLE_CHECK,
	HDCP_WAITTING_VID_STB,
	HDCP_HW_ENABLE,
	HDCP_WAITTING_FINISH,
	HDCP_FINISH,
	HDCP_FAILE,
	HDCP_NOT_SUPPORT,
	HDCP_PROCESS_STATE_NUM
};

enum VIDEO_OUTPUT_STATUS {
	VO_WAIT_VIDEO_STABLE,
	VO_WAIT_TX_VIDEO_STABLE,
	/*VO_WAIT_PLL_LOCK,*/
	VO_CHECK_VIDEO_INFO,
	VO_FINISH,
	VO_STATE_NUM
};
enum AUDIO_OUTPUT_STATUS {
	AO_INIT,
	AO_CTS_RCV_INT,
	AO_AUDIO_RCV_INT,
	AO_RCV_INT_FINISH,
	AO_OUTPUT,
	AO_STATE_NUM
};
struct Packet_AVI {
	unchar AVI_data[13];
};


struct Packet_SPD {
	unchar SPD_data[25];
};


struct Packet_MPEG {
	unchar MPEG_data[13];
};


struct AudiInfoframe {
	unchar type;
	unchar version;
	unchar length;
	unchar pb_byte[11];
};


enum PACKETS_TYPE {
	AVI_PACKETS,
	SPD_PACKETS,
	MPEG_PACKETS,
	VSI_PACKETS,
	AUDIF_PACKETS
};
struct COMMON_INT {
	unchar common_int[5];
	unchar change_flag;
};
struct HDMI_RX_INT {
	unchar hdmi_rx_int[7];
	unchar change_flag;
};

enum xtal_enum {
	XTAL_19D2M,
	XTAL_24M,
	XTAL_25M,
	XTAL_26M,
	XTAL_27M,
	XTAL_38D4M,
	XTAL_52M,
	XTAL_NOT_SUPPORT,
	XTAL_CLK_NUM
};

/* SSC settings */
enum SP_SSC_DEP {
	SSC_DEP_DISABLE = 0x0,
	SSC_DEP_500PPM,
	SSC_DEP_1000PPM,
	SSC_DEP_1500PPM,
	SSC_DEP_2000PPM,
	SSC_DEP_2500PPM,
	SSC_DEP_3000PPM,
	SSC_DEP_3500PPM,
	SSC_DEP_4000PPM,
	SSC_DEP_4500PPM,
	SSC_DEP_5000PPM,
	SSC_DEP_5500PPM,
	SSC_DEP_6000PPM
};

struct clock_Data {
	unsigned char xtal_clk;
	unsigned int xtal_clk_m10;
};


#define SP_POWER_ON 1
#define SP_POWER_DOWN 0

#define VID_DVI_MODE 0x00
#define VID_HDMI_MODE 0x01

#define check_cable_det_pin() 1

extern unchar edid_blocks[256];

#define MAKE_WORD(ch, cl) ((uint)(((uint)ch<<8) | (uint)cl))
#define MAX_BUF_CNT 16


#define SP_BREAK(current_status, next_status) { \
	if (next_status != (current_status) + 1) \
		break; }

#ifdef ENABLE_READ_EDID
void sp_tx_edid_read_initial(void);
unchar sp_tx_get_edid_block(void);
void sp_tx_rst_aux(void);
void edid_read(unchar offset, unchar *pblock_buf);
bool sp_tx_edid_read(unchar *pBuf);
void check_edid_data(unchar *pblock_buf);
#endif

bool slimport_chip_detect(void);
void slimport_chip_initial(void);
void slimport_main_process(void);
unchar is_cable_detected(void);
unchar sp_tx_aux_dpcdread_bytes(unchar addrh, unchar addrm,
	unchar addrl, unchar cCount, unchar *pBuf);
unchar sp_tx_aux_dpcdwrite_bytes(unchar addrh, unchar addrm, unchar addrl,
					unchar cCount, unchar *pBuf);
unchar sp_tx_aux_dpcdwrite_byte(unchar addrh, unchar addrm, unchar addrl,
					unchar data1);
void sp_tx_show_infomation(void);
void hdmi_rx_show_video_info(void);
void slimport_block_power_ctrl(enum SP_TX_POWER_BLOCK sp_tx_pd_block,
					unchar power);
void vbus_power_ctrl(unsigned char ON);
void slimport_initialization(void);
void sp_tx_clean_state_machine(void);
unchar sp_tx_cur_states(void);
void print_sys_state(unchar ss);
unchar slimport_hdcp_cap_check(void);
unchar sp_tx_cur_cable_type(void);

void sp_tx_initialization(void);
unchar sp_rx_cur_bw(void);

/* ***************************************************************** */
/* Functions protoype for slimport_rx anx7730 */
/* ***************************************************************** */
bool source_aux_read_7730dpcd(long addr, unchar cCount, unchar *pBuf);
bool source_aux_write_7730dpcd(long addr, unchar cCount, unchar *pBuf);
bool i2c_master_read_reg(unchar Sink_device_sel, unchar offset, unchar *Buf);
bool i2c_master_write_reg(unchar Sink_device_sel, unchar offset, unchar value);



#endif
