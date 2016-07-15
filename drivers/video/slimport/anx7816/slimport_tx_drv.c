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

#include "slimport_custom_declare.h"
#include <linux/platform_data/slimport_device.h>
#ifdef QUICK_CHARGE_SUPPORT
#include "quick_charge.h"
#endif
#include <linux/slab.h>

#include <linux/mod_display.h>

#ifndef XTAL_CLK_DEF
#define XTAL_CLK_DEF XTAL_19D2M
#endif

#define XTAL_CLK_M10 pXTAL_data[XTAL_CLK_DEF].xtal_clk_m10
#define XTAL_CLK pXTAL_data[XTAL_CLK_DEF].xtal_clk

static unchar sp_tx_test_bw;
static bool sp_tx_test_lt;
static bool sp_tx_test_edid;

/*static unsigned char ext_int_index;*/
static unsigned char g_changed_bandwidth;
static unsigned char sp_rx_bandwidth;
static unsigned char g_hdmi_dvi_status;

static unsigned char g_need_clean_status;

/* HDCP control enable/disable from AP  */
/* external_block_en = 1: enable, 0: disable*/
extern int external_block_en;

#ifdef ENABLE_READ_EDID
unsigned char g_edid_break;
unsigned char g_edid_checksum;
unchar edid_blocks[256];
static unsigned char g_read_edid_flag;

#endif

#define HDCP_REPEATER_MODE (__i2c_read_byte(RX_P1, 0x2A) & REPEATER_M)


static struct Packet_AVI sp_tx_packet_avi;
static struct Packet_SPD sp_tx_packet_spd;
static struct Packet_MPEG sp_tx_packet_mpeg;
static struct AudiInfoframe sp_tx_audioinfoframe;

enum SP_TX_System_State sp_tx_system_state;
enum RX_CBL_TYPE sp_tx_rx_type;

enum CHARGING_STATUS downstream_charging_status;
enum AUDIO_OUTPUT_STATUS sp_tx_ao_state;
enum VIDEO_OUTPUT_STATUS sp_tx_vo_state;
enum SINK_CONNECTION_STATUS sp_tx_sc_state;
enum SP_TX_LT_STATUS sp_tx_LT_state;
enum SP_TX_System_State sp_tx_system_state_bak;
enum HDCP_STATUS HDCP_state;

uint chipid_list[CO3_NUMS] = {
	0x7818,
	0x7816,
	0x7812,
	0x7810,
	0x7806,
	0x7802
};

struct COMMON_INT common_int_status;
struct HDMI_RX_INT hdmi_rx_int_status;

#define COMMON_INT1 common_int_status.common_int[0]
#define COMMON_INT2 common_int_status.common_int[1]
#define COMMON_INT3 common_int_status.common_int[2]
#define COMMON_INT4 common_int_status.common_int[3]
#define COMMON_INT5 common_int_status.common_int[4]
#define COMMON_INT_CHANGED common_int_status.change_flag
#define HDMI_RX_INT1 hdmi_rx_int_status.hdmi_rx_int[0]
#define HDMI_RX_INT2 hdmi_rx_int_status.hdmi_rx_int[1]
#define HDMI_RX_INT3 hdmi_rx_int_status.hdmi_rx_int[2]
#define HDMI_RX_INT4 hdmi_rx_int_status.hdmi_rx_int[3]
#define HDMI_RX_INT5 hdmi_rx_int_status.hdmi_rx_int[4]
#define HDMI_RX_INT6 hdmi_rx_int_status.hdmi_rx_int[5]
#define HDMI_RX_INT7 hdmi_rx_int_status.hdmi_rx_int[6]
#define HDMI_RX_INT_CHANGED hdmi_rx_int_status.change_flag
static unchar down_sample_en;

static unsigned char assisted_HDCP_repeater;
#define HDCP_ERROR 2
#define HDCP_DOING 1
#define HDCP_DONE 0

static unsigned char __i2c_read_byte(unsigned char dev, unsigned char offset);
static void hardware_power_ctl(unchar enable);
static void hdmi_rx_new_vsi_int(void);

#define sp_tx_aux_polling_enable() sp_write_reg_or(TX_P0, TX_DEBUG1, POLLING_EN)
#define sp_tx_aux_polling_disable() sp_write_reg_and(TX_P0, TX_DEBUG1, ~POLLING_EN)

#define reg_bit_ctl(addr, offset, data, enable) \
	do { \
		unchar c; \
		sp_read_reg(addr, offset, &c); \
		if (enable) { \
			if ((c & data) != data) { \
				c |= data; \
				sp_write_reg(addr, offset, c); \
			} \
		} else { \
			if ((c & data) == data) { \
				c &= ~data; \
				sp_write_reg(addr, offset, c); \
			} \
		} \
	} while (0)

#define sp_tx_video_mute(enable) \
	reg_bit_ctl(TX_P2, VID_CTRL1, VIDEO_MUTE, enable)
#define hdmi_rx_mute_audio(enable) \
	reg_bit_ctl(RX_P0, RX_MUTE_CTRL, AUD_MUTE, enable)
#define hdmi_rx_mute_video(enable) \
	reg_bit_ctl(RX_P0, RX_MUTE_CTRL, VID_MUTE, enable)
#define sp_tx_addronly_set(enable) \
	reg_bit_ctl(TX_P0, AUX_CTRL2, ADDR_ONLY_BIT, enable)

#define sp_tx_set_link_bw(bw) \
	sp_write_reg(TX_P0, SP_TX_LINK_BW_SET_REG, bw)
#define sp_tx_get_link_bw() \
	__i2c_read_byte(TX_P0, SP_TX_LINK_BW_SET_REG)

#define sp_tx_get_pll_lock_status() \
	((__i2c_read_byte(TX_P0, TX_DEBUG1) & DEBUG_PLL_LOCK) != 0 ? 1 : 0)

#define gen_M_clk_with_downspeading() \
	sp_write_reg_or(TX_P0, SP_TX_M_CALCU_CTRL, M_GEN_CLK_SEL)
#define gen_M_clk_without_downspeading \
	sp_write_reg_and(TX_P0, SP_TX_M_CALCU_CTRL, (~M_GEN_CLK_SEL))

#define hdmi_rx_set_hpd(enable) \
	do { \
		if ((bool)enable) { \
			sp_write_reg_or(TX_P2, SP_TX_VID_CTRL3_REG, HPD_OUT); \
		} else {\
			sp_write_reg_and(TX_P2, SP_TX_VID_CTRL3_REG, \
								~HPD_OUT); \
		} \
	} while (0)
#define hdmi_rx_set_termination(enable) \
	do { \
		if ((bool)enable) \
			sp_write_reg_and(RX_P0, HDMI_RX_TMDS_CTRL_REG7, ~TERM_PD); \
		else \
			sp_write_reg_or(RX_P0, HDMI_RX_TMDS_CTRL_REG7, TERM_PD); \
	} while (0)
#define sp_tx_get_rx_bw(pdata) \
	sp_tx_aux_dpcdread_bytes(0x00, 0x00, DPCD_MAX_LINK_RATE, 1, pdata)

#define sp_tx_clean_hdcp_status() do { \
	sp_write_reg(TX_P0, TX_HDCP_CTRL0, 0x03); \
	sp_write_reg_or(TX_P0, TX_HDCP_CTRL0, RE_AUTH); \
	usleep_range(2000, 2000); \
	pr_debug("%s %s : sp_tx_clean_hdcp_status\n", LOG_TAG, __func__); \
	} while (0)
#define reg_hardware_reset() do { \
	sp_write_reg_or(TX_P2, SP_TX_RST_CTRL_REG, HW_RST); \
	sp_tx_clean_state_machine(); \
	sp_tx_set_sys_state(STATE_SP_INITIALIZED); \
	msleep(500); \
	} while (0)
#ifdef NEW_HDCP_CONTROL_LOGIC
unsigned char g_hdcp_cap_bak;
#endif
/*=====================================================================*/
#define write_dpcd_addr(addrh, addrm, addrl) \
	do { \
		unchar temp; \
		if (__i2c_read_byte(TX_P0, AUX_ADDR_7_0) != (unchar)addrl) \
			sp_write_reg(TX_P0, AUX_ADDR_7_0, (unchar)addrl); \
			if (__i2c_read_byte(TX_P0, AUX_ADDR_15_8) != (unchar)addrm) \
			sp_write_reg(TX_P0, AUX_ADDR_15_8, (unchar)addrm); \
		sp_read_reg(TX_P0, AUX_ADDR_19_16, &temp); \
		if ((unchar)(temp & 0x0F)  != ((unchar)addrh & 0x0F)) \
			sp_write_reg(TX_P0, AUX_ADDR_19_16, (temp  & 0xF0) | ((unchar)addrh)); \
	} while (0)


#define sp_tx_set_sys_state(ss) \
	do { \
		pr_debug("%s %s : set: clean_status: %x,\n ", LOG_TAG, __func__, (uint)g_need_clean_status); \
		if ((sp_tx_system_state >= STATE_LINK_TRAINING) && (ss < STATE_LINK_TRAINING)) \
			sp_write_reg_or(TX_P0, SP_TX_ANALOG_PD_REG, CH0_PD); \
		sp_tx_system_state_bak = sp_tx_system_state; \
		sp_tx_system_state = (unchar)ss; \
		g_need_clean_status = 1; \
		print_sys_state(sp_tx_system_state); \
	} while (0)

#define goto_next_system_state() \
	do { \
		pr_debug("%s %s : next: clean_status: %x,\n ", LOG_TAG, __func__, (uint)g_need_clean_status); \
		sp_tx_system_state_bak = sp_tx_system_state; \
		sp_tx_system_state++;\
		print_sys_state(sp_tx_system_state); \
	} while (0)

#define redo_cur_system_state() \
	do { \
		pr_debug("%s %s : redo: clean_status: %x,\n ", LOG_TAG, __func__, (uint)g_need_clean_status); \
		g_need_clean_status = 1; \
		sp_tx_system_state_bak = sp_tx_system_state; \
		print_sys_state(sp_tx_system_state); \
	} while (0)

#define system_state_change_with_case(status) \
	do { \
		if (sp_tx_system_state >= status) { \
			pr_debug("%s %s : change_case: clean_status: %xm,\n ", LOG_TAG, __func__, (uint)g_need_clean_status); \
			if ((sp_tx_system_state >= STATE_LINK_TRAINING) && (status < STATE_LINK_TRAINING)) \
			sp_write_reg_or(TX_P0, SP_TX_ANALOG_PD_REG, CH0_PD); \
			g_need_clean_status = 1; \
			sp_tx_system_state_bak = sp_tx_system_state; \
			sp_tx_system_state = (unchar)status; \
			print_sys_state(sp_tx_system_state); \
		} \
	} while (0)

#define sp_write_reg_or(address, offset, mask) \
		sp_write_reg(address, offset, ((unsigned char)__i2c_read_byte(address, offset) | (mask)))
#define sp_write_reg_and(address, offset, mask) \
	sp_write_reg(address, offset, ((unsigned char)__i2c_read_byte(address, offset) & (mask)))

#define sp_write_reg_and_or(address, offset, and_mask, or_mask) \
	sp_write_reg(address, offset, (((unsigned char)__i2c_read_byte(address, offset)) & and_mask) | (or_mask))
#define sp_write_reg_or_and(address, offset, or_mask, and_mask) \
	sp_write_reg(address, offset, (((unsigned char)__i2c_read_byte(address, offset)) | or_mask) & (and_mask))

static unsigned char __i2c_read_byte(unsigned char dev, unsigned char offset)
{
	unsigned char temp;

	sp_read_reg(dev, offset, &temp);
	return temp;
}

void hardware_power_ctl(unchar enable)
{
	/*static unsigned char supply_power = 0;
	if(supply_power != enable)
		supply_power = enable;
	else
		return; */

	if (enable == 0)
		sp_tx_hardware_powerdown();
	else
		sp_tx_hardware_poweron();
}

void wait_aux_op_finish(unchar *err_flag)
{
	unchar cnt;
	unchar c;

	*err_flag = 0;
	cnt = 150;
	while (__i2c_read_byte(TX_P0, AUX_CTRL2) & AUX_OP_EN) {
		usleep_range(2000, 2000);
		if ((cnt--) == 0) {
			pr_err("%s %s :aux operate failed!\n",
						LOG_TAG, __func__);
			*err_flag = 1;
			break;
		}
	}

	sp_read_reg(TX_P0, SP_TX_AUX_STATUS, &c);
	if (c & 0x0F) {
		pr_info("%s %s : wait aux operation status %.2x\n",
					LOG_TAG, __func__, (uint)c);
		*err_flag = 1;
	}
}

/*=====================================================================*/
void print_sys_state(unchar ss)
{
	switch (ss) {
	case STATE_INIT:
		pr_info("%s %s : -STATE_INIT-\n", LOG_TAG, __func__);
		break;
	case STATE_WAITTING_CABLE_PLUG:
		pr_info("%s %s : -STATE_WAITTING_CABLE_PLUG-\n",
							LOG_TAG, __func__);
		break;
	case STATE_SP_INITIALIZED:
		pr_info("%s %s : -STATE_SP_INITIALIZED-\n", LOG_TAG, __func__);
		break;
	case STATE_SINK_CONNECTION:
		pr_info("%s %s : -STATE_SINK_CONNECTION-\n", LOG_TAG, __func__);
		break;
	#ifdef ENABLE_READ_EDID
	case STATE_PARSE_EDID:
		pr_info("%s %s : -STATE_PARSE_EDID-\n", LOG_TAG, __func__);
		break;
	#endif
	case STATE_LINK_TRAINING:
		pr_info("%s %s : -STATE_LINK_TRAINING-\n", LOG_TAG, __func__);
		break;
	case STATE_VIDEO_OUTPUT:
		pr_info("%s %s : -STATE_VIDEO_OUTPUT-\n", LOG_TAG, __func__);
		break;
	case STATE_HDCP_AUTH:
		pr_info("%s %s : -STATE_HDCP_AUTH-\n", LOG_TAG, __func__);
		break;
	case STATE_AUDIO_OUTPUT:
		pr_info("%s %s : -STATE_AUDIO_OUTPUT-\n", LOG_TAG, __func__);
		break;
	case STATE_PLAY_BACK:
		pr_info("%s %s : -STATE_PLAY_BACK-\n", LOG_TAG, __func__);
		break;
	default:
		pr_err("%s %s : system state is error1\n", LOG_TAG, __func__);
		break;
	}
}

/*DPCD*/
void sp_tx_rst_aux(void)
{
	sp_tx_aux_polling_disable();
	sp_write_reg_or(TX_P2, RST_CTRL2, AUX_RST);
	sp_write_reg_and(TX_P2, RST_CTRL2, ~AUX_RST);
	sp_tx_aux_polling_enable();
}

unchar sp_tx_aux_dpcdread_bytes(unchar addrh, unchar addrm,
	unchar addrl, unchar cCount, unchar *pBuf)
{
	unchar c, c1, i;
	unchar bOK;

	sp_write_reg(TX_P0, BUF_DATA_COUNT, 0x80);/*clear buffer*/
	/*command and length*/
	c = ((cCount - 1) << 4) | 0x09;
	sp_write_reg(TX_P0, AUX_CTRL, c);
	write_dpcd_addr(addrh, addrm, addrl);
	sp_write_reg_or(TX_P0, AUX_CTRL2, AUX_OP_EN);
	usleep_range(2000, 2000);

	wait_aux_op_finish(&bOK);
	if (bOK == AUX_ERR) {
		pr_err("%s %s : aux read failed\n", LOG_TAG, __func__);
		/*add by span 20130217.*/
		sp_read_reg(TX_P2, SP_TX_INT_STATUS1, &c);
		sp_read_reg(TX_P0, TX_DEBUG1, &c1);
		/*if polling is enabled, wait polling error interrupt*/
		if (c1 & POLLING_EN) {
			if (c & POLLING_ERR)
				sp_tx_rst_aux();
		} else
			sp_tx_rst_aux();
		return AUX_ERR;
	}

	for (i = 0; i < cCount; i++) {
		sp_read_reg(TX_P0, BUF_DATA_0 + i, &c);
		*(pBuf + i) = c;
		if (i >= MAX_BUF_CNT)
			break;
	}
	return AUX_OK;
}


unchar sp_tx_aux_dpcdwrite_bytes(unchar addrh, unchar addrm,
				unchar addrl, unchar cCount, unchar *pBuf)
{
	unchar c, i, ret;

	c =  ((cCount - 1) << 4) | 0x08;
	sp_write_reg(TX_P0, AUX_CTRL, c);
	write_dpcd_addr(addrh, addrm, addrl);
	for (i = 0; i < cCount; i++) {
		c = *pBuf;
		pBuf++;
		sp_write_reg(TX_P0, BUF_DATA_0 + i, c);

		if (i >= 15)
			break;
	}
	sp_write_reg_or(TX_P0, AUX_CTRL2, AUX_OP_EN);
	wait_aux_op_finish(&ret);
	return ret;
}

unchar sp_tx_aux_dpcdwrite_byte(unchar addrh, unchar addrm,
						unchar addrl, unchar data1)
{
	unchar ret;

	sp_write_reg(TX_P0, AUX_CTRL, 0x08);/*one byte write.*/
	write_dpcd_addr(addrh, addrm, addrl);
	sp_write_reg(TX_P0, BUF_DATA_0, data1);
	sp_write_reg_or(TX_P0, AUX_CTRL2, AUX_OP_EN);
	wait_aux_op_finish(&ret);
	return ret;
}

/*========initialized system*/
void slimport_block_power_ctrl(enum SP_TX_POWER_BLOCK sp_tx_pd_block,
				unchar power)
{
	if (power == SP_POWER_ON)
		sp_write_reg_and(TX_P2, SP_POWERD_CTRL_REG, (~sp_tx_pd_block));
	else
		sp_write_reg_or(TX_P2, SP_POWERD_CTRL_REG, (sp_tx_pd_block));
	 pr_debug("%s %s : sp_tx_power_on: %.2x\n", LOG_TAG, __func__,
							(uint)sp_tx_pd_block);
}

void vbus_power_ctrl(unsigned char ON)
{
	unchar i;

	if (ON == 0) {
		/*sp_tx_aux_polling_disable();*/
		sp_write_reg_and(TX_P2, TX_PLL_FILTER, ~V33_SWITCH_ON);
		sp_write_reg_or(TX_P2, TX_PLL_FILTER5,
					P5V_PROTECT_PD | SHORT_PROTECT_PD);
		pr_debug("%s %s : 3.3V output disabled\n", LOG_TAG, __func__);
	} else {
		for (i = 0; i < 5; i++) {
			sp_write_reg_and(TX_P2, TX_PLL_FILTER5,
					(~P5V_PROTECT_PD & ~SHORT_PROTECT_PD));
			sp_write_reg_or(TX_P2, TX_PLL_FILTER, V33_SWITCH_ON);
			if (!((unchar)__i2c_read_byte(TX_P2, TX_PLL_FILTER5) &
								0xc0)) {
				pr_debug("%s %s : 3.3V output enabled\n",
							LOG_TAG, __func__);
				break;
			} else {
				pr_debug("%s %s : VBUS power can not be supplied\n",
							LOG_TAG, __func__);
			}
		}
	}
}

void system_power_ctrl(unchar enable)
{
	vbus_power_ctrl(enable);
	slimport_block_power_ctrl(SP_TX_PWR_REG, enable);
	slimport_block_power_ctrl(SP_TX_PWR_TOTAL, enable);
	hardware_power_ctl(enable);
	sp_tx_clean_state_machine();
	sp_tx_set_sys_state(STATE_WAITTING_CABLE_PLUG);
}

void sp_tx_clean_state_machine(void)
{
	sp_tx_system_state = STATE_INIT;
	sp_tx_system_state_bak = STATE_INIT;
	sp_tx_sc_state = SC_INIT;
	sp_tx_LT_state = LT_INIT;
	HDCP_state = HDCP_CAPABLE_CHECK;
	sp_tx_vo_state = VO_WAIT_VIDEO_STABLE;
	sp_tx_ao_state = AO_INIT;
}

enum CHARGING_STATUS downstream_charging_status_get(void)
{
	return downstream_charging_status;
}

void downstream_charging_status_set(void)
{
	unchar c1;

	if (AUX_OK == sp_tx_aux_dpcdread_bytes
				(0x00, 0x05, 0x22, 1, &c1)) {
		if ((c1&0x01) == 0x00) {
			downstream_charging_status = FAST_CHARGING;
			sp_write_reg_or(TX_P2, TX_ANALOG_CTRL, SHORT_DPDM);
			pr_debug("%s %s : fast charging!\n", LOG_TAG, __func__);
		} else {
			downstream_charging_status = NO_FAST_CHARGING;
			sp_write_reg_and(TX_P2, TX_ANALOG_CTRL, ~SHORT_DPDM);
			pr_debug("%s %s : no charging!\n", LOG_TAG, __func__);
		}
	}
}

unchar sp_tx_cur_states(void)
{
	return sp_tx_system_state;
}

unchar sp_tx_cur_cable_type(void)
{
	return sp_tx_rx_type;
}

unchar sp_rx_cur_bw(void)
{
	return sp_rx_bandwidth;
}

void sp_tx_variable_init(void)
{
	uint i;

	sp_tx_system_state = STATE_INIT;
	sp_tx_system_state_bak = STATE_INIT;
	sp_tx_rx_type = DWN_STRM_IS_NULL;
	#ifdef ENABLE_READ_EDID
	g_edid_break = 0;
	g_read_edid_flag = 0;
	g_edid_checksum = 0;
	for (i = 0; i < 256; i++)
		edid_blocks[i] = 0;
	#endif
	sp_tx_LT_state = LT_INIT;
	HDCP_state = HDCP_CAPABLE_CHECK;
	g_need_clean_status = 0;
	sp_tx_sc_state = SC_INIT;
	sp_tx_vo_state = VO_WAIT_VIDEO_STABLE;
	sp_tx_ao_state = AO_INIT;
	g_changed_bandwidth = LINK_5P4G;
	sp_rx_bandwidth = LINK_5P4G;
	g_hdmi_dvi_status = HDMI_MODE;

	sp_tx_test_lt = 0;
	sp_tx_test_bw = 0;
	sp_tx_test_edid = 0;

	downstream_charging_status = NO_CHARGING_CAPABLE;
	#ifdef NEW_HDCP_CONTROL_LOGIC
	g_hdcp_cap_bak = 0;
	#endif
	assisted_HDCP_repeater = HDCP_DONE;
}

static void hdmi_rx_tmds_phy_initialization(void)
{
	sp_write_reg(RX_P0, HDMI_RX_TMDS_CTRL_REG2, 0xa9);
	/*sp_write_reg(RX_P0, HDMI_RX_TMDS_CTRL_REG4, 0x28);
	sp_write_reg(RX_P0, HDMI_RX_TMDS_CTRL_REG5, 0xe3);*/
	sp_write_reg(RX_P0, HDMI_RX_TMDS_CTRL_REG7, 0x80);
	/*sp_write_reg(RX_P0,HDMI_RX_TMDS_CTRL_REG19, 0x00);
	sp_write_reg(RX_P0, HDMI_RX_TMDS_CTRL_REG21, 0x04);
	sp_write_reg(RX_P0, HDMI_RX_TMDS_CTRL_REG22, 0x38);*/

	/*HDMI RX PHY*/
	sp_write_reg(RX_P0, HDMI_RX_TMDS_CTRL_REG1, 0x90);
	sp_write_reg(RX_P0, HDMI_RX_TMDS_CTRL_REG6, 0x92);
	sp_write_reg(RX_P0, HDMI_RX_TMDS_CTRL_REG20, 0xf2);
}

void hdmi_rx_initialization(void)
{
	#ifdef CHANGE_TX_P0_ADDR
	/*Change 0x70 I2C address to 0x78*/
	sp_write_reg(TX_P2, SP_TX_DP_ADDR_REG1, 0xBC);
	#endif
	sp_write_reg(RX_P0, RX_MUTE_CTRL, AUD_MUTE | VID_MUTE);
	sp_write_reg_or(RX_P0, RX_CHIP_CTRL,
		MAN_HDMI5V_DET | PLLLOCK_CKDT_EN | DIGITAL_CKDT_EN);
	/*sp_write_reg_or(RX_P0, RX_AEC_CTRL, AVC_OE);*/

	sp_write_reg_or(RX_P0, RX_SRST, HDCP_MAN_RST | SW_MAN_RST |
					TMDS_RST | VIDEO_RST);
	sp_write_reg_and(RX_P0, RX_SRST, (~HDCP_MAN_RST) & (~SW_MAN_RST) &
					(~TMDS_RST) & (~VIDEO_RST));
	/*Sync detect change , GP set mute*/
	sp_write_reg_or(RX_P0, RX_AEC_EN0, AEC_EN06 | AEC_EN05);
	sp_write_reg_or(RX_P0, RX_AEC_EN2, AEC_EN21);
	sp_write_reg_or(RX_P0, RX_AEC_CTRL, AVC_EN | AAC_OE | AAC_EN);

	sp_write_reg_and(RX_P0, RX_SYS_PWDN1, ~PWDN_CTRL);

	/*
	default value is 0x00
	sp_write_reg(RX_P0, HDMI_RX_INT_MASK1_REG, 0x00);
	sp_write_reg(RX_P0, HDMI_RX_INT_MASK2_REG, 0x00);
	sp_write_reg(RX_P0, HDMI_RX_INT_MASK3_REG, 0x00);
	sp_write_reg(RX_P0, HDMI_RX_INT_MASK4_REG, 0x00);
	sp_write_reg(RX_P0, HDMI_RX_INT_MASK5_REG, 0x00);
	sp_write_reg(RX_P0, HDMI_RX_INT_MASK6_REG, 0x00);
	sp_write_reg(RX_P0, HDMI_RX_INT_MASK7_REG, 0x00);
	*/
	sp_write_reg_or(RX_P0, RX_VID_DATA_RNG, R2Y_INPUT_LIMIT);
	#ifdef CEC_ENABLE
	cec_variable_init();    /*for CEC*/
	sp_write_reg(RX_P0, RX_CEC_SPEED, CEC_SPEED_27M);
	/*50ms polling period*/
	sp_write_reg(TX_P0, SP_TX_DP_POLLING_PERIOD, 0x32);
	#endif
	/*add10, dvdd10 and avdd18 setting */
	sp_write_reg(RX_P0, 0x65, 0xc4);
	sp_write_reg(RX_P0, 0x66, 0x18);

	hdmi_rx_tmds_phy_initialization();
	hdmi_rx_set_hpd(0);
	hdmi_rx_set_termination(0);
	pr_info("%s %s : HDMI Rx is initialized...\n", LOG_TAG, __func__);
}

struct clock_Data const pXTAL_data[XTAL_CLK_NUM] = {
	{19, 192},
	{24, 240},
	{25, 250},
	{26, 260},
	{27, 270},
	{38, 384},
	{52, 520},
	{27, 270},
};

void xtal_clk_sel(void)
{
	pr_info("%s %s : define XTAL_CLK:  %x\n ",
				LOG_TAG, __func__, (uint)XTAL_CLK_DEF);
	sp_write_reg_and_or(TX_P2, TX_ANALOG_DEBUG2,
				(~0x3c), 0x3c & (XTAL_CLK_DEF << 2));
	sp_write_reg(TX_P0, 0xEC, (unchar)(((uint)XTAL_CLK_M10)));
	sp_write_reg(TX_P0, 0xED,
		(unchar)(((uint)XTAL_CLK_M10 & 0xFF00) >> 2) | XTAL_CLK);
	sp_write_reg(TX_P0, I2C_GEN_10US_TIMER0,
				(unchar)(((uint)XTAL_CLK_M10)));
	sp_write_reg(TX_P0, I2C_GEN_10US_TIMER1,
				(unchar)(((uint)XTAL_CLK_M10 & 0xFF00) >> 8));
	sp_write_reg(TX_P0, 0xBF, (unchar)(((uint)XTAL_CLK - 1)));

	/*CEC function need to change the value.*/
	sp_write_reg_and_or(RX_P0, 0x49, 0x07,
				(unchar)(((((uint)XTAL_CLK) >> 1) - 2) << 3));
	/*sp_write_reg(RX_P0, 0x49, 0x5b); cec test */

}

void sp_tx_initialization(void)
{
	sp_write_reg(TX_P0, AUX_CTRL2, 0x30); /*set terminal reistor to 50ohm*/

	if (!HDCP_REPEATER_MODE) {
		sp_write_reg_and(TX_P0, TX_HDCP_CTRL,
					(~AUTO_EN) & (~AUTO_START));
		sp_write_reg(TX_P0, OTP_KEY_PROTECT1, OTP_PSW1);
		sp_write_reg(TX_P0, OTP_KEY_PROTECT2, OTP_PSW2);
		sp_write_reg(TX_P0, OTP_KEY_PROTECT3, OTP_PSW3);
		sp_write_reg_or(TX_P0, HDCP_KEY_CMD, DISABLE_SYNC_HDCP);
	} else
		sp_write_reg_or(TX_P0, 0x1D, 0xC0);

	sp_write_reg(TX_P2, SP_TX_VID_CTRL8_REG, VID_VRES_TH);

	sp_write_reg(TX_P0, HDCP_AUTO_TIMER, HDCP_AUTO_TIMER_VAL);
	sp_write_reg_or(TX_P0, TX_HDCP_CTRL , LINK_POLLING);

	/*sp_write_reg_or(TX_P0, 0x65 , 0x30);*/

	sp_write_reg_or(TX_P0, TX_LINK_DEBUG , M_VID_DEBUG);
	sp_write_reg_or(TX_P0, TX_DEBUG1, FORCE_HPD);
	/*sp_tx_aux_polling_disable(); */
	/*sp_write_reg_or(TX_P2, TX_PLL_FILTER, AUX_TERM_50OHM);
	sp_write_reg_and(TX_P2, TX_PLL_FILTER5,
		(~P5V_PROTECT_PD) & (~SHORT_PROTECT_PD)); */
	sp_write_reg_or(TX_P2, TX_ANALOG_DEBUG2, POWERON_TIME_1P5MS);
	/* sp_write_reg_or(TX_P0, TX_HDCP_CTRL0,
		BKSV_SRM_PASS |KSVLIST_VLD); */
	/*sp_write_reg(TX_P2, TX_ANALOG_CTRL, 0xC5);*/

	xtal_clk_sel();
	sp_write_reg(TX_P0, AUX_DEFER_CTRL, 0x8C);

	sp_write_reg_or(TX_P0, TX_DP_POLLING, AUTO_POLLING_DISABLE);
	/*Short the link intergrity check timer to speed up bstatus
	polling for HDCP CTS item 1A-07 */
	sp_write_reg(TX_P0, SP_TX_LINK_CHK_TIMER, 0x1d);
	sp_write_reg_or(TX_P0, TX_MISC, EQ_TRAINING_LOOP);

	/*power down main link by default*/
	sp_write_reg_or(TX_P0, SP_TX_ANALOG_PD_REG, CH0_PD);

	/*
	sp_write_reg(TX_P2, SP_COMMON_INT_MASK1, 0X00);
	sp_write_reg(TX_P2, SP_COMMON_INT_MASK2, 0X00);
	sp_write_reg(TX_P2, SP_COMMON_INT_MASK3, 0X00);
	sp_write_reg(TX_P2, SP_COMMON_INT_MASK4, 0X00);
	*/
	/*sp_write_reg(TX_P2, SP_INT_MASK, 0X90);*/
	sp_write_reg(TX_P2, SP_TX_INT_CTRL_REG, 0X01);
	/*disable HDCP mismatch function for VGA dongle*/
	/*sp_write_reg(TX_P0, SP_TX_LT_SET_REG, 0);*/
	sp_tx_link_phy_initialization();
	gen_M_clk_with_downspeading();

	down_sample_en = 0;
}

void slimport_chip_initial(void)
{
	sp_tx_variable_init();
	/*vbus_power_ctrl(0);*/
	hardware_power_ctl(0);
	/*sp_tx_set_sys_state(STATE_CABLE_PLUG);*/
}

bool slimport_chip_detect(void)
{
	uint c;
	unchar i;
	bool big_endian;
	unchar *ptemp;

	/*check whether CPU is big endian*/
	c = 0x1122;
	ptemp = (unchar *)&c;
	if (*ptemp == 0x11 && *(ptemp + 1) == 0x22)
		big_endian = 1;
	else
		big_endian = 0;
	hardware_power_ctl(1);
	c = 0;
	/*check chip id*/
	if (big_endian) {
		sp_read_reg(TX_P2, SP_TX_DEV_IDL_REG, (unchar *)(&c) + 1);
		sp_read_reg(TX_P2, SP_TX_DEV_IDH_REG, (unchar *)(&c));
	} else {
		sp_read_reg(TX_P2, SP_TX_DEV_IDL_REG, (unchar *)(&c));
		sp_read_reg(TX_P2, SP_TX_DEV_IDH_REG, (unchar *)(&c) + 1);
	}

	pr_info("%s %s : CHIPID: ANX%x\n", LOG_TAG, __func__, c & 0x0000FFFF);
	for (i = 0; i < CO3_NUMS; i++) {
		if (c == chipid_list[i])
			return 1;
	}
	return 0;
}

/*========cable plug*/
unchar is_cable_detected(void)
{
	return slimport_dongle_is_connected();
	/*
	if(cable_detected()) {
		msleep(50);
		return cable_detected();
	}
	return 0;
	*/
}

void slimport_waitting_cable_plug_process(void)
{
	if (is_cable_detected()) {
#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
		slimport_set_hdmi_hpd(1);
#endif
		hardware_power_ctl(1);
#ifdef QUICK_CHARGE_SUPPORT
		/*enable otg,QC2.0*/
		enable_otg();
#endif
		goto_next_system_state();
	} else {
#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
		slimport_set_hdmi_hpd(0);
#endif
		hardware_power_ctl(0);
	}
}


static unchar sp_tx_get_cable_type(enum CABLE_TYPE_STATUS det_cable_type_state,
						bool bdelay)
{
	unchar ds_port_preset;
	unchar aux_status;
	unchar data_buf[16];
	unchar cur_cable_type;

	ds_port_preset = 0;
	cur_cable_type = DWN_STRM_IS_NULL;
	downstream_charging_status = NO_CHARGING_CAPABLE; /*add for charging*/

	aux_status = sp_tx_aux_dpcdread_bytes(0x00, 0x00, 0x05,
							1, &ds_port_preset);
	pr_debug("%s %s : DPCD 0x005: %x\n", LOG_TAG, __func__,
							(int)ds_port_preset);

	switch (det_cable_type_state) {
	case CHECK_AUXCH:
		if (AUX_OK == aux_status) {
			sp_tx_aux_dpcdread_bytes(0x00, 0x00, 0, 0x0c, data_buf);
			det_cable_type_state = GETTED_CABLE_TYPE;
		} else {
			msleep(50);
			pr_err("%s %s : AUX access error\n", LOG_TAG, __func__);
			break;
		}
	case GETTED_CABLE_TYPE:
		switch ((ds_port_preset & (_BIT1 | _BIT2)) >> 1) {
		case 0x00:
			cur_cable_type = DWN_STRM_IS_DIGITAL;
			pr_debug("%s %s : Downstream is DP dongle.\n",
							LOG_TAG, __func__);
			break;
		case 0x01:
		case 0x03:
			cur_cable_type = DWN_STRM_IS_ANALOG;

			downstream_charging_status = NO_FAST_CHARGING;
			if (bdelay) {
				/*eeprom_reload();*/
				msleep(150);
			}
			pr_debug("%s %s : Downstream is general DP2VGA converter.\n",
						LOG_TAG, __func__);
			break;
		case 0x02:
			sp_tx_aux_dpcdread_bytes(0x00, 0x05, 0x23,
					1, data_buf); /*add for charging*/
			if ((data_buf[0]&0x7f) >= 0x15) /*version >= 2.1*/
				downstream_charging_status = NO_FAST_CHARGING;

			/*sp_tx_send_message(MSG_OCM_EN);*/
			cur_cable_type = DWN_STRM_IS_HDMI;
			pr_debug("%s %s : Downstream is HDMI dongle.\n",
							LOG_TAG, __func__);

			break;
		default:
			cur_cable_type = DWN_STRM_IS_NULL;
			pr_err("%s %s : Downstream can not recognized.\n",
							LOG_TAG, __func__);
			break;
		}
	default:
		break;
	}

	return cur_cable_type;
}

unchar sp_tx_get_vga_connection(void)
{
	unchar c;

	if (AUX_OK != sp_tx_aux_dpcdread_bytes(0x00, 0x02, DPCD_SINK_COUNT,
								1, &c)){
		pr_err("%s %s : aux error.\n", LOG_TAG, __func__);
		return 0;
	}

	if (c & 0x01)
		return 1;
	else
		return 0;
}
unchar sp_tx_get_dp_connection(void)
{
	unchar c;

	if (AUX_OK != sp_tx_aux_dpcdread_bytes(0x00, 0x02, DPCD_SINK_COUNT,
								1, &c))
		return 0;

	if (c & 0x1f) {
		sp_tx_aux_dpcdread_bytes(0x00, 0x00, 0x04, 1, &c);
		if (c & 0x20) {
			sp_tx_aux_dpcdread_bytes(0x00, 0x06, 0x00, 1, &c);
			/*Bit 5 = SET_DN_DEVICE_DP_PWR_5V
			  Bit 6 = SET_DN_DEVICE_DP_PWR_12V
			  Bit 7 = SET_DN_DEVICE_DP_PWR_18V*/
			c = c & 0x1F;
			sp_tx_aux_dpcdwrite_byte(0x00, 0x06, 0x00, c | 0x20);
		}
		return 1;
	} else
		return 0;
}
/********************************************/
/* Check if it is ANALOGIX dongle.
*/
/********************************************/
unsigned char ANX_OUI[3] = {0x00, 0x22, 0xB9};
unsigned char is_anx_dongle(void)
{
	unsigned char buf[3];

	sp_tx_aux_dpcdread_bytes(0x00, 0x04, 0x00, 3, buf);
	if (buf[0] == ANX_OUI[0] && buf[1] == ANX_OUI[1]
		&& buf[2] == ANX_OUI[2]) {
		return 1;
		/*debug_puts("DPCD 400 show ANX-dongle");*/
	}
	sp_tx_aux_dpcdread_bytes(0x00, 0x05, 0x00, 3, buf);/*just for ANX7730*/
	if (buf[0] == ANX_OUI[0] && buf[1] == ANX_OUI[1]
					&& buf[2] == ANX_OUI[2]) {
		return 1;
		/*debug_puts("DPCD 500 show ANX-dongle");*/
	}
	return 0;
}

unchar sp_tx_get_hdmi_connection(void)
{
	unchar c;

	/*msleep(200);*/
	if (is_anx_dongle()) {
		if (AUX_OK != sp_tx_aux_dpcdread_bytes(0x00, 0x05, 0x18, 1, &c))
			return 0;

		if ((c & 0x41) == 0x41)
			return 1;
		 else
			return 0;
	} else
		return sp_tx_get_dp_connection();
}

unchar sp_tx_get_downstream_connection(void)
{
	switch (sp_tx_rx_type) {
	case DWN_STRM_IS_HDMI:
		return sp_tx_get_hdmi_connection();
	case DWN_STRM_IS_DIGITAL:
		return sp_tx_get_dp_connection();
	case DWN_STRM_IS_ANALOG:
	case DWN_STRM_IS_VGA_9832:
		return sp_tx_get_vga_connection();
	case DWN_STRM_IS_NULL:
	default:
		return 0;
	}
	return 0;
}
void slimport_sink_connection(void)
{
	/*unchar val = 0;*/
	if (check_cable_det_pin() == 0) {
		sp_tx_set_sys_state(STATE_WAITTING_CABLE_PLUG);
		return;
	}

	switch (sp_tx_sc_state) {
	case SC_INIT:
		sp_tx_sc_state++;
	case SC_CHECK_CABLE_TYPE:
	case SC_WAITTING_CABLE_TYPE:
	default:
		sp_tx_rx_type = sp_tx_get_cable_type(CHECK_AUXCH, 1);
		if (sp_tx_rx_type == DWN_STRM_IS_NULL) {
			sp_tx_sc_state++;
			if (sp_tx_sc_state >= SC_WAITTING_CABLE_TYPE) {
				sp_tx_sc_state = SC_NOT_CABLE;
				pr_warn("%s %s : Can not get cable type!\n",
							LOG_TAG, __func__);
			}
			break;
		}
		/*dongle has fast charging detection capability*/
		if (downstream_charging_status != NO_CHARGING_CAPABLE)
			downstream_charging_status_set();

		sp_tx_sc_state = SC_SINK_CONNECTED;
	case SC_SINK_CONNECTED:
		if (sp_tx_get_downstream_connection()) {
		#ifdef CEC_ENABLE
			if (sp_tx_rx_type == DWN_STRM_IS_HDMI)
				cec_function_enable();
		#endif
			goto_next_system_state();
		}
		break;
	case SC_NOT_CABLE:
		vbus_power_ctrl(0);
		reg_hardware_reset();
		break;
	}
}
/******************start EDID process********************/
void sp_tx_enable_video_input(unchar enable)
{
	unchar c;

	sp_read_reg(TX_P2, VID_CTRL1, &c);
	if (enable) {
		if ((c & VIDEO_EN) != VIDEO_EN) {
			c = (c & 0xf7) | VIDEO_EN;
			sp_write_reg(TX_P2, VID_CTRL1, c);
			pr_debug("%s %s : Slimport Video is enabled!\n",
							LOG_TAG, __func__);
		}
	} else {
		if ((c & VIDEO_EN) == VIDEO_EN) {
			c &= ~VIDEO_EN;
			sp_write_reg(TX_P2, VID_CTRL1, c);
			pr_debug("%s %s : Slimport Video is disabled!\n",
							LOG_TAG, __func__);
		}
	}
}
void sp_tx_send_message(enum SP_TX_SEND_MSG message)
{
	unchar c;

	switch (message) {
	default:
		break;
	case MSG_INPUT_HDMI:
		if (sp_tx_rx_type == DWN_STRM_IS_HDMI)
			sp_tx_aux_dpcdwrite_byte(0x00, 0x05, 0x26, 0x01);
		break;

	case MSG_INPUT_DVI:
		if (sp_tx_rx_type == DWN_STRM_IS_HDMI)
			sp_tx_aux_dpcdwrite_byte(0x00, 0x05, 0x26, 0x00);
		break;

	case MSG_CLEAR_IRQ:
		/*pr_info("%s %s : clear irq start!\n", LOG_TAG, __func__);*/
		sp_tx_aux_dpcdread_bytes(0x00, 0x04, 0x10, 1, &c);
		/*pr_info("%s %s : clear irq middle!\n", LOG_TAG, __func__);*/
		c |= 0x01;
		sp_tx_aux_dpcdwrite_byte(0x00, 0x04, 0x10, c);
		/*pr_info("%s %s : clear irq end!\n", LOG_TAG, __func__);*/
		break;
	}

}
static unchar read_dvi_hdmi_mode(void)
{
	unchar temp;

	if (sp_tx_rx_type == DWN_STRM_IS_HDMI)
		sp_tx_aux_dpcdread_bytes(0x00, 0x05, 0x26, 1, &temp);
	else
		temp = g_hdmi_dvi_status;
	return temp;
}
#ifdef ENABLE_READ_EDID
static unchar get_edid_detail(unchar *data_buf)
{
	uint pixclock_edid;

	pixclock_edid = ((((uint)data_buf[1] << 8)) |
					((uint)data_buf[0] & 0xFF));
	/*pr_info("%s %s : =============pixclock  via EDID : %d\n",
				LOG_TAG, __func__, (uint)pixclock_edid); */
	if (pixclock_edid <= 5300)
		return LINK_1P62G;
	else if ((5300 < pixclock_edid) && (pixclock_edid <= 8900))
		return LINK_2P7G;
	else if ((8900 < pixclock_edid) && (pixclock_edid <= 18000))
		return LINK_5P4G;
	else
		return LINK_6P75G;


}
static unchar parse_edid_to_get_bandwidth(void)
{
	unchar desc_offset = 0;
	unchar i, bandwidth, temp;

	bandwidth = LINK_1P62G;
	temp = LINK_1P62G;
	i = 0;
	while (4 > i && 0 != edid_blocks[0x36 + desc_offset]) {
		temp = get_edid_detail(edid_blocks+0x36+desc_offset);
		pr_debug("%s %s : bandwidth via EDID : %x\n",
					LOG_TAG, __func__, (uint)temp);
		if (bandwidth < temp)
			bandwidth = temp;
		if (bandwidth > LINK_5P4G)
			break;
		desc_offset += 0x12;
		++i;
	}
	return bandwidth;

}
static void sp_tx_aux_wr(unchar offset)
{
	sp_write_reg(TX_P0, BUF_DATA_0, offset);
	sp_write_reg(TX_P0, AUX_CTRL, 0x04);
	sp_write_reg_or(TX_P0, AUX_CTRL2, AUX_OP_EN);
	wait_aux_op_finish(&g_edid_break);
}

static void sp_tx_aux_rd(unchar len_cmd)
{
	sp_write_reg(TX_P0, AUX_CTRL, len_cmd);
	sp_write_reg_or(TX_P0, AUX_CTRL2, AUX_OP_EN);
	wait_aux_op_finish(&g_edid_break);
}

unchar sp_tx_get_edid_block(void)
{
	unchar c;

	sp_tx_aux_wr(0x7e);
	sp_tx_aux_rd(0x01);
	sp_read_reg(TX_P0, BUF_DATA_0, &c);
	pr_debug("%s %s : EDID Block = %d\n", LOG_TAG, __func__, (int)(c + 1));

	if (c > 3)
		c = 1;
		/*g_edid_break = 1;*/
	return c;
}
void edid_read(unchar offset, unchar *pblock_buf)
{
	unchar data_cnt, cnt;
	unchar c;

	sp_tx_aux_wr(offset);
	sp_tx_aux_rd(0xf5);/*set I2C read com 0x05 mot = 1 and read 16 bytes*/
	data_cnt = 0;
	cnt = 0;

	while ((data_cnt) < 16) {
		sp_read_reg(TX_P0, BUF_DATA_COUNT, &c);

		/*if(c != 0x10) */
			/*pr_info("%s %s : edid read: len : %x\n",
						LOG_TAG, __func__, (uint)c);*/

		if ((c & 0x1f) != 0) {
			data_cnt = data_cnt + c;
			do {
				sp_read_reg(TX_P0, BUF_DATA_0 + c - 1,
							&(pblock_buf[c - 1]));
				/*(pr_info("%s %s : index: %x, data: %x ",
						LOG_TAG, __func__, (uint)c,
						(uint)pblock_buf[c]);*/
				if (c == 1)
					break;
			} while (c--);

			/*pr_info("%s %s : edid read : length: 0\n",
						LOG_TAG, __func__);*/
			if (cnt++ <= 2) {
				sp_tx_rst_aux();
				c = 0x05 | ((0x0f - data_cnt) << 4);
				sp_tx_aux_rd(c);
			} else {
				 g_edid_break = 1;
				 break;
			}
		}
	}
	/*issue a stop every 16 bytes read*/
	sp_write_reg(TX_P0, AUX_CTRL, 0x01);
	sp_write_reg_or(TX_P0, AUX_CTRL2, ADDR_ONLY_BIT | AUX_OP_EN);
	wait_aux_op_finish(&g_edid_break);
	sp_tx_addronly_set(0);

}
void sp_tx_edid_read_initial(void)
{
	sp_write_reg(TX_P0, AUX_ADDR_7_0, 0x50);
	sp_write_reg(TX_P0, AUX_ADDR_15_8, 0);
	sp_write_reg_and(TX_P0, AUX_ADDR_19_16, 0xf0);
}
static void segments_edid_read(unchar segment, unchar offset)
{
	unchar c, cnt;
	int i;

	sp_write_reg(TX_P0, AUX_CTRL, 0x04);

	sp_write_reg(TX_P0, AUX_ADDR_7_0, 0x30);

	sp_write_reg_or(TX_P0, AUX_CTRL2, ADDR_ONLY_BIT | AUX_OP_EN);
	/*sp_tx_addronly_set(0);*/

	sp_read_reg(TX_P0, AUX_CTRL2, &c);

	wait_aux_op_finish(&g_edid_break);
	sp_read_reg(TX_P0, AUX_CTRL, &c);

	sp_write_reg(TX_P0, BUF_DATA_0, segment);

	/*set I2C write com 0x04 mot = 1*/
	sp_write_reg(TX_P0, AUX_CTRL, 0x04);

	sp_write_reg_and_or(TX_P0, AUX_CTRL2, ~ADDR_ONLY_BIT, AUX_OP_EN);
	cnt = 0;
	sp_read_reg(TX_P0, AUX_CTRL2, &c);
	while (c&AUX_OP_EN) {
		usleep_range(1000, 1000);
		cnt++;
		if (cnt == 10) {
			pr_debug("%s %s : write break", LOG_TAG, __func__);
			sp_tx_rst_aux();
			cnt = 0;
			g_edid_break = 1;
			return; /* bReturn;*/
		}
		sp_read_reg(TX_P0, AUX_CTRL2, &c);
	}

	sp_write_reg(TX_P0, AUX_ADDR_7_0, 0x50);

	sp_tx_aux_wr(offset);

	sp_tx_aux_rd(0xf5);
	cnt = 0;
	for (i = 0; i < 16; i++) {
		sp_read_reg(TX_P0, BUF_DATA_COUNT, &c);
		while ((c & 0x1f) == 0) {
			usleep_range(2000, 2000);
			cnt++;

			sp_read_reg(TX_P0, BUF_DATA_COUNT, &c);
			if (cnt == 10) {
				pr_debug("%s %s : read break",
						LOG_TAG, __func__);
				sp_tx_rst_aux();
				g_edid_break = 1;
				return;
			}
		}
		sp_read_reg(TX_P0, BUF_DATA_0+i, &c);
	}

	sp_write_reg(TX_P0, AUX_CTRL, 0x01);
	sp_write_reg_or(TX_P0, AUX_CTRL2, ADDR_ONLY_BIT | AUX_OP_EN);
	sp_write_reg_and(TX_P0, AUX_CTRL2, ~ADDR_ONLY_BIT);
	sp_read_reg(TX_P0, AUX_CTRL2, &c);
	while (c & AUX_OP_EN)
		sp_read_reg(TX_P0, AUX_CTRL2, &c);
/*	sp_tx_addronly_set(0);  */
}

static bool edid_checksum_result(unchar *pBuf)
{
	unchar cnt, checksum;

	checksum = 0;
	for (cnt = 0; cnt < 0x80; cnt++)
		checksum = checksum + pBuf[cnt];

	g_edid_checksum = checksum - pBuf[0x7f];
	g_edid_checksum = ~g_edid_checksum + 1;

/*	pr_info("%s %s : ====g_edid_checksum  ==%x\n", LOG_TAG, __func__,
				(uint)g_edid_checksum); */

	return checksum == 0 ? 1 : 0;
}

static void edid_header_result(unchar *pBuf)
{
	 if ((pBuf[0] == 0) && (pBuf[7] == 0) && (pBuf[1] == 0xff) &&
		(pBuf[2] == 0xff) && (pBuf[3] == 0xff) && (pBuf[4] == 0xff) &&
		(pBuf[5] == 0xff) && (pBuf[6] == 0xff))
		pr_debug("%s %s : Good EDID header!\n", LOG_TAG, __func__);
	 else
		pr_err("%s %s : Bad EDID header!\n", LOG_TAG, __func__);

}
void check_edid_data(unchar *pblock_buf)
{
	unchar i;

	edid_header_result(pblock_buf);
	for (i = 0; i <= ((pblock_buf[0x7e] > 1) ? 1 : pblock_buf[0x7e]); i++) {
		if (!edid_checksum_result(pblock_buf + i * 128))
			pr_err("%s %s : Block %x edid checksum error\n",
						LOG_TAG, __func__, (uint)i);
		else
			pr_debug("%s %s : Block %x edid checksum OK\n",
						LOG_TAG, __func__, (uint)i);
		}
}

bool sp_tx_edid_read(unchar *pedid_blocks_buf)
{
	unchar offset = 0;
	unchar count, blocks_num;
	unchar pblock_buf[16];
	unchar i, j, c;

	g_edid_break = 0;
	sp_tx_edid_read_initial();
	sp_write_reg(TX_P0, AUX_CTRL, 0x04);
	sp_write_reg_or(TX_P0, AUX_CTRL2, 0x03);
	wait_aux_op_finish(&g_edid_break);
	sp_tx_addronly_set(0);

	blocks_num = sp_tx_get_edid_block();

	count = 0;
	do {
		switch (count) {
		case 0:
		case 1:
			for (i = 0; i < 8; i++) {
				offset = (i+count*8) * 16;
				edid_read(offset, pblock_buf);
				if (g_edid_break == 1)
					break;
				for (j = 0; j < 16; j++) {
					pedid_blocks_buf[offset + j] =
							pblock_buf[j];
				}
			}
			break;
		case 2:
			offset = 0x00;
			for (j = 0; j < 8; j++) {
				if (g_edid_break == 1)
					break;
				segments_edid_read(count / 2, offset);
				offset = offset + 0x10;
			}
			break;
		case 3:
			offset = 0x80;
			for (j = 0; j < 8; j++) {
				if (g_edid_break == 1)
					break;
				segments_edid_read(count / 2, offset);
				offset = offset + 0x10;
			}
			break;
		default:
			break;
		}
		count++;
		if (g_edid_break == 1)
			break;
	} while (blocks_num >= count);

	sp_tx_rst_aux();
	/*check edid data */
	if (g_read_edid_flag == 0) {
		check_edid_data(pedid_blocks_buf);
		g_read_edid_flag = 1;
	}
	 /* test edid << */
	sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x18, 1, &c);
	if (c & 0x04) {
		pr_debug("%s %s : check sum = %.2x\n", LOG_TAG, __func__,
						(uint)g_edid_checksum);
		c = g_edid_checksum;
		sp_tx_aux_dpcdwrite_bytes(0x00, 0x02, 0x61, 1, &c);

		c = 0x04;
		sp_tx_aux_dpcdwrite_bytes(0x00, 0x02, 0x60, 1, &c);
		pr_debug("%s %s : Test EDID done\n", LOG_TAG, __func__);

	}
	/* test edid  >> */
	return 0;
}

static bool check_with_pre_edid(unchar *org_buf)
{
	unchar i;
	unchar temp_buf[16];
	bool return_flag;

	return_flag = 0;
	/*check checksum and blocks number */
	g_edid_break = 0;
	sp_tx_edid_read_initial();
	sp_write_reg(TX_P0, AUX_CTRL, 0x04);
	sp_write_reg_or(TX_P0, AUX_CTRL2, 0x03);
	wait_aux_op_finish(&g_edid_break);
	sp_tx_addronly_set(0);

	edid_read(0x70, temp_buf);

	if (g_edid_break == 0) {
		for (i = 0; i < 16; i++) {
			if (org_buf[0x70 + i] != temp_buf[i]) {
				pr_debug("%s %s : different checksum and blocks num\n",
						LOG_TAG, __func__);
				return_flag = 1;  /*need re-read edid*/
				break;
			}
		}
	} else
		return_flag = 1;

	if (return_flag)
		goto return_point;

	/*check edid information */
	edid_read(0x08, temp_buf);
	if (g_edid_break == 0) {
		for (i = 0; i < 16; i++) {
			if (org_buf[i + 8] != temp_buf[i]) {
				pr_debug("%s %s : different edid information\n",
							LOG_TAG, __func__);
				return_flag = 1;
				break;
			}
		}
	} else
		return_flag = 1;

return_point:
	sp_tx_rst_aux();
	return return_flag;
}
#ifdef NEW_HDCP_CONTROL_LOGIC

void hdmi_rx_hdcp_cap(unsigned char hdcp_cap)
{
	if (hdcp_cap == 1) {
		pr_debug("*****HDCP CAP changed to active*******!\n");
		sp_write_reg_and(RX_P1, HDMI_RX_HDCP_STATUS_REG, ~BKSV_DISABLE);
	} else {
		pr_debug("*****HDCP CAP changed to inactive*******!\n");
		sp_write_reg_or(RX_P1, HDMI_RX_HDCP_STATUS_REG, ~BKSV_DISABLE);
	}
	sp_write_reg_or(RX_P0, RX_SRST, HDCP_MAN_RST);
	msleep(10);
	sp_write_reg_and(RX_P0, RX_SRST, ~HDCP_MAN_RST);
	sp_tx_clean_hdcp_status();
}
#endif
void slimport_edid_process(void)
{
	unchar rx_bandwidth, tx_bandwidth;
	unchar i;
	struct mod_display_panel_config *display_config = NULL;
	int ret;

	pr_debug("%s %s : edid_process\n", LOG_TAG, __func__);

	sp_tx_get_rx_bw(&rx_bandwidth);
	pr_debug("%s %s : get rx bandwidth info = [%x]\n", LOG_TAG, __func__,
		(uint)rx_bandwidth);
	sp_rx_bandwidth = rx_bandwidth;

	ret = mod_display_get_display_config(&display_config);
	if (ret) {
		pr_err("%s: Failed to get display config: %d\n", __func__, ret);
		memset(edid_blocks, 0, 256);
		return;
	} else if (display_config->config_type == MOD_CONFIG_EDID_1_3) {
		if (display_config->config_size > 256) {
			pr_err("%s: EDID too big: %d\n", __func__,
				display_config->config_size);
		} else if (display_config->config_size == 0) {
			pr_debug("%s: Reading EDID over HDMI link...\n",
				__func__);
		} else {
			memcpy(edid_blocks, display_config->config_buf,
				display_config->config_size);
			goto skip_me;
		}
	} else if (display_config->config_type == MOD_CONFIG_EDID_DOWNSTREAM) {
		if (display_config->config_size !=
			sizeof(struct mod_display_downstream_config)) {
			pr_err("%s: bad downstream config size: %d != %zu\n",
			       __func__, display_config->config_size,
			       sizeof(struct mod_display_downstream_config));
		} else {
			struct mod_display_downstream_config downstream_config;

			memcpy(&downstream_config, display_config->config_buf,
				sizeof(downstream_config));
			if (downstream_config.max_link_bandwidth_khz) {
				u32 link_bw_max_khz_orig =
				       downstream_config.max_link_bandwidth_khz;
				unchar link_bw_max =
					sp_get_link_bandwidth_limit_from_khz(
						link_bw_max_khz_orig);
				u32 link_bw_max_khz = sp_get_link_bandwidth_khz(
					link_bw_max);

				pr_info("%s: limit rx bandwidth to %uKHz -> 0x%02x %uKHz\n",
					__func__, link_bw_max_khz_orig,
					link_bw_max, link_bw_max_khz);
				rx_bandwidth = min(rx_bandwidth, link_bw_max);
				sp_rx_bandwidth = rx_bandwidth;
			}
		}
	} else {
		pr_err("%s: Unknown display config type (%d)... Abort\n",
			__func__, display_config->config_type);
		memset(edid_blocks, 0, 256);
		return;
	}

	/* mdelay(200); */
	if (g_read_edid_flag == 1) {
		if (check_with_pre_edid(edid_blocks))
			g_read_edid_flag = 0;
		else
			pr_debug("%s %s : Don`t need to read edid!\n",
							LOG_TAG, __func__);
	}

	if (g_read_edid_flag == 0) {
		sp_tx_edid_read(edid_blocks);
		if (g_edid_break)
			pr_err("%s %s : ERR:EDID corruption!\n",
						LOG_TAG, __func__);
	}

skip_me:
	if (display_config)
		kfree(display_config);

	/*Release the HPD after the OTP loaddown*/
	i = 10;
	do {
		if ((__i2c_read_byte(TX_P0, HDCP_KEY_STATUS) & 0x01))
			break;
		else {
			pr_warn("%s %s : waiting HDCP KEY loaddown\n",
							LOG_TAG, __func__);
			usleep_range(1000, 1000);
		}
	} while (--i);
	sp_write_reg(RX_P0, HDMI_RX_INT_MASK1_REG, 0xe2);
	#ifdef NEW_HDCP_CONTROL_LOGIC
	if (sp_tx_rx_type == DWN_STRM_IS_HDMI)
		g_hdcp_cap_bak = 1;
	else
		g_hdcp_cap_bak = slimport_hdcp_cap_check();
	hdmi_rx_hdcp_cap(g_hdcp_cap_bak);
	#endif
	if (!HDCP_REPEATER_MODE) {
		hdmi_rx_set_hpd(1);
		pr_debug("%s %s : hdmi_rx_set_hpd 1 !\n", LOG_TAG, __func__);
		hdmi_rx_set_termination(1);
	}

	tx_bandwidth = parse_edid_to_get_bandwidth();

	g_changed_bandwidth = min(rx_bandwidth, tx_bandwidth);
	pr_debug("%s %s : set link bw in edid %x\n", LOG_TAG, __func__,
						(uint)g_changed_bandwidth);
	/*sp_tx_set_link_bw(g_changed_bandwidth); */
	/*
	sp_tx_send_message(
		(g_hdmi_dvi_status == HDMI_MODE) ?
					MSG_INPUT_HDMI : MSG_INPUT_DVI);
  */
	goto_next_system_state();
}
#endif
/******************End EDID process********************/
/******************start Link training process********************/

static void sp_tx_lvttl_bit_mapping(void)
{
	unchar c, colorspace;
	unchar vid_bit;

	vid_bit = 0;
	sp_read_reg(RX_P1, HDMI_RX_AVI_DATA00_REG, &colorspace);
	colorspace &= 0x60;

	switch (((__i2c_read_byte(RX_P0, HDMI_RX_VIDEO_STATUS_REG1) &
			COLOR_DEPTH) >> 4)) {
	default:
	case Hdmi_legacy:
		c = IN_BPC_8BIT;
		vid_bit = 0;
		break;
	case Hdmi_24bit:
		c = IN_BPC_8BIT;
		if (colorspace == 0x20)
			vid_bit = 5;
		else
			vid_bit = 1;
		break;
	case Hdmi_30bit:
		c = IN_BPC_10BIT;
		if (colorspace == 0x20)
			vid_bit = 6;
		else
			vid_bit = 2;
		break;
	case Hdmi_36bit:
		c = IN_BPC_12BIT;
		if (colorspace == 0x20)
			vid_bit = 6;
		else
			vid_bit = 3;
		break;
	}
	/* For down sample video (12bit, 10bit ---> 8bit),
	this register don`t change*/
	if (down_sample_en == 0)
		sp_write_reg_and_or(TX_P2, SP_TX_VID_CTRL2_REG, 0x8c,
							colorspace >> 5 | c);
	/*mdelay(500); */
	/*Patch: for 10bit video must be set this value to 12bit by someone*/
	if (down_sample_en == 1 && c == IN_BPC_10BIT)
		vid_bit = 3;

	sp_write_reg_and_or(TX_P2, BIT_CTRL_SPECIFIC, 0x00,
					ENABLE_BIT_CTRL | vid_bit << 1);

	if (sp_tx_test_edid) {
		/*set color depth to 18-bit for link cts*/
		sp_write_reg_and(TX_P2, SP_TX_VID_CTRL2_REG, 0x8f);
		sp_tx_test_edid = 0;
		pr_debug("%s %s : ***color space is set to 18bit***\n",
							LOG_TAG, __func__);
	}

	if (colorspace) {
		sp_write_reg(TX_P0, SP_TX_VID_BLANK_SET1, 0x80);
		sp_write_reg(TX_P0, SP_TX_VID_BLANK_SET2, 0x00);
		sp_write_reg(TX_P0, SP_TX_VID_BLANK_SET3, 0x80);
	} else {
		sp_write_reg(TX_P0, SP_TX_VID_BLANK_SET1, 0x0);
		sp_write_reg(TX_P0, SP_TX_VID_BLANK_SET2, 0x0);
		sp_write_reg(TX_P0, SP_TX_VID_BLANK_SET3, 0x0);
	}
}

ulong sp_tx_pclk_calc(void)
{
	ulong str_plck;
	uint vid_counter;
	unchar c;

	sp_read_reg(RX_P0, 0x8d, &c);
	vid_counter = c;
	vid_counter = vid_counter << 8;
	sp_read_reg(RX_P0, 0x8c, &c);
	vid_counter |=  c;
	str_plck = ((ulong)vid_counter * XTAL_CLK_M10)  >> 12;
	pr_debug("%s %s : PCLK = %d.%d\n", LOG_TAG, __func__,
		(((uint)(str_plck))/10),
		((uint)str_plck - (((uint)str_plck/10)*10)));
	return str_plck;
}

static unchar sp_tx_bw_lc_sel(ulong pclk)
{
	ulong pixel_clk;
	unchar c1;

	switch (((__i2c_read_byte(RX_P0, HDMI_RX_VIDEO_STATUS_REG1) &
				COLOR_DEPTH) >> 4)) {
		case Hdmi_legacy:
		case Hdmi_24bit:
		default:
			pixel_clk = pclk;
			break;
		case Hdmi_30bit:
			pixel_clk = (pclk * 5) >> 2;
			break;
		case Hdmi_36bit:
			pixel_clk = (pclk * 3) >> 1;
			break;
	}
	pr_debug("%s %s : pixel_clk = %d.%d\n", LOG_TAG, __func__,
			(((uint)(pixel_clk))/10),
			((uint)pixel_clk - (((uint)pixel_clk/10)*10)));

	down_sample_en = 0;
	if (pixel_clk <= 530)
		c1 = LINK_1P62G;
	else if ((530 < pixel_clk) && (pixel_clk <= 890))
			c1 = LINK_2P7G;
	else if ((890 < pixel_clk) && (pixel_clk <= 1800))
			c1 = LINK_5P4G;
	else {
		 c1 = LINK_6P75G;
		 if (pixel_clk > 2240)
			down_sample_en = 1;
	}

	if (sp_tx_get_link_bw() != c1) {
		g_changed_bandwidth = c1;
		pr_debug("%s %s : It is different bandwidth between sink support and cur video!%.2x\n",
				LOG_TAG, __func__, (uint)c1);
		return 1;
	}
	return 0;
}

void sp_tx_spread_enable(unchar benable)
{
	unchar c;

	sp_read_reg(TX_P0, SP_TX_DOWN_SPREADING_CTRL1, &c);

	if (benable) {
		c |= SP_TX_SSC_DWSPREAD;
		sp_write_reg(TX_P0, SP_TX_DOWN_SPREADING_CTRL1, c);

		sp_tx_aux_dpcdread_bytes(0x00, 0x01,
			DPCD_DOWNSPREAD_CTRL, 1, &c);
		c |= SPREAD_AMPLITUDE;
		sp_tx_aux_dpcdwrite_byte(0x00, 0x01, DPCD_DOWNSPREAD_CTRL, c);

	} else {
		c &= ~SP_TX_SSC_DISABLE;
		sp_write_reg(TX_P0, SP_TX_DOWN_SPREADING_CTRL1, c);

		sp_tx_aux_dpcdread_bytes(0x00, 0x01,
			DPCD_DOWNSPREAD_CTRL, 1, &c);
		c &= ~SPREAD_AMPLITUDE;
		sp_tx_aux_dpcdwrite_byte(0x00, 0x01, DPCD_DOWNSPREAD_CTRL, c);
	}

}

void sp_tx_config_ssc(enum SP_SSC_DEP sscdep)
{
	sp_write_reg(TX_P0, SP_TX_DOWN_SPREADING_CTRL1, 0x0); /*clear register*/
	sp_write_reg(TX_P0, SP_TX_DOWN_SPREADING_CTRL1, sscdep);
	sp_tx_spread_enable(1);
}

void sp_tx_enhancemode_set(void)
{
	unchar c;

	sp_tx_aux_dpcdread_bytes(0x00, 0x00, DPCD_MAX_LANE_COUNT, 1, &c);
	if (c & ENHANCED_FRAME_CAP) {

		sp_write_reg_or(TX_P0, SP_TX_SYS_CTRL4_REG, ENHANCED_MODE);

		sp_tx_aux_dpcdread_bytes(0x00, 0x01,
			DPCD_LANE_COUNT_SET, 1, &c);
		c |= ENHANCED_FRAME_EN;
		sp_tx_aux_dpcdwrite_byte(0x00, 0x01,
			DPCD_LANE_COUNT_SET, c);

		pr_debug("%s %s : Enhance mode enabled\n", LOG_TAG, __func__);
	} else {

		sp_write_reg_and(TX_P0, SP_TX_SYS_CTRL4_REG, ~ENHANCED_MODE);

		sp_tx_aux_dpcdread_bytes(0x00, 0x01,
					DPCD_LANE_COUNT_SET, 1, &c);
		c &= ~ENHANCED_FRAME_EN;
		sp_tx_aux_dpcdwrite_byte(0x00, 0x01, DPCD_LANE_COUNT_SET, c);

		pr_debug("%s %s : Enhance mode disabled\n", LOG_TAG, __func__);
	}
}

uint sp_tx_link_err_check(void)
{
	uint errl = 0, errh = 0;
	unchar bytebuf[2];

	sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x10, 2, bytebuf);
	usleep_range(5000, 5000);
	sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x10, 2, bytebuf);
	errh = bytebuf[1];

	if (errh & 0x80) {
		errl = bytebuf[0];
		errh = (errh & 0x7f) << 8;
		errl = errh + errl;
		pr_err("%s %s :  Err of Lane = %d\n", LOG_TAG, __func__, errl);
	}

	return errl;
}

static void serdes_fifo_reset(void)
{
	sp_write_reg_or(TX_P2, RST_CTRL2, SERDES_FIFO_RST);
	msleep(20);
	sp_write_reg_and(TX_P2, RST_CTRL2, (~SERDES_FIFO_RST));
}

void slimport_link_training(void)
{
	unchar temp_value, return_value, c;

	return_value = 1;
	pr_debug("%s %s : sp_tx_LT_state : %x\n", LOG_TAG, __func__,
						(int)sp_tx_LT_state);
	switch (sp_tx_LT_state) {
	case LT_INIT:
		slimport_block_power_ctrl(SP_TX_PWR_VIDEO, SP_POWER_ON);
		sp_tx_video_mute(1);
		sp_tx_enable_video_input(0);
		sp_tx_LT_state++;

		sp_tx_send_message((g_hdmi_dvi_status == HDMI_MODE) ?
					MSG_INPUT_HDMI : MSG_INPUT_DVI);

	case LT_WAIT_PLL_LOCK:
		if (!sp_tx_get_pll_lock_status()) {
			/*pll reset when plll not lock. */
			sp_read_reg(TX_P0, SP_TX_PLL_CTRL_REG, &temp_value);
			temp_value |= PLL_RST;
			sp_write_reg(TX_P0, SP_TX_PLL_CTRL_REG, temp_value);
			temp_value &= ~PLL_RST;
			sp_write_reg(TX_P0, SP_TX_PLL_CTRL_REG, temp_value);
			pr_warn("%s %s : PLL not lock!\n", LOG_TAG, __func__);
		} else
			sp_tx_LT_state = LT_CHECK_LINK_BW;
		SP_BREAK(LT_WAIT_PLL_LOCK, sp_tx_LT_state);
	case LT_CHECK_LINK_BW:
		sp_tx_get_rx_bw(&temp_value);
		if (temp_value < g_changed_bandwidth) {
			pr_warn("%s %s : ****Over bandwidth****\n",
						LOG_TAG, __func__);
			g_changed_bandwidth = temp_value;
		} else
			sp_tx_LT_state++;
	case LT_START:
		if (sp_tx_test_lt) {
			/*sp_tx_test_lt = 0; */
			g_changed_bandwidth = sp_tx_test_bw;
			sp_write_reg_and(TX_P2, SP_TX_VID_CTRL2_REG, 0x8f);
		} else {
			#ifdef DEMO_4K_2K
			if (down_sample_en)
				sp_write_reg(TX_P0, SP_TX_LT_SET_REG, 0x09);
			else
			#endif
			sp_write_reg(TX_P0, SP_TX_LT_SET_REG, 0x0);
		}

		/*power on main link before link training */
		sp_write_reg_and(TX_P0, SP_TX_ANALOG_PD_REG, ~CH0_PD);

		sp_tx_config_ssc(SSC_DEP_4000PPM);
		sp_tx_set_link_bw(g_changed_bandwidth);
		sp_tx_enhancemode_set();

		/* judge downstream DP version  and set downstream
			sink to D0 (normal operation mode) */
		sp_tx_aux_dpcdread_bytes(0x00, 0x00, 0x00, 0x01, &c);
		sp_tx_aux_dpcdread_bytes(0x00, 0x06, 0x00, 0x01, &temp_value);
		if (c >= 0x12) {  /*DP 1.2 and above */
			temp_value &= 0xf8;
		} else {/*DP 1.1 or 1.0 */
			temp_value &= 0xfc;
		}
		temp_value |= 0x01;
		sp_tx_aux_dpcdwrite_byte(0x00, 0x06, 0x00, temp_value);

		/*sp_write_reg_or(TX_P2, SP_INT_MASK, 0X20);
			hardware interrupt mask enable */

		sp_write_reg(TX_P0, LT_CTRL, SP_TX_LT_EN);
		sp_tx_LT_state = LT_WAITTING_FINISH;
		/*There is no break;*/
	case LT_WAITTING_FINISH:
		/*here : waitting interrupt to change training state.*/
		break;
	case LT_ERROR:
		serdes_fifo_reset();
		pr_err("LT ERROR Status: SERDES FIFO reset.");
		redo_cur_system_state();
		sp_tx_LT_state = LT_INIT;
		break;
	case LT_FINISH:
		sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x02, 1, &temp_value);
		if ((temp_value&0x07) == 0x07) { /*for one lane case.*/
			/* if there is link error, adjust pre-emphsis
			 to check error again. If there is no error,
			 keep the setting, otherwise use 400mv0db */
			if (!sp_tx_test_lt) {
				if (sp_tx_link_err_check()) {
					sp_read_reg(TX_P0, SP_TX_LT_SET_REG,
							&temp_value);
					if (!(temp_value & MAX_PRE_REACH)) {
						/*increase one pre-level */
					sp_write_reg(TX_P0,
						SP_TX_LT_SET_REG,
						(temp_value + 0x08));
					/*if error still exist, return to the
						link traing value*/
						if (sp_tx_link_err_check())
								sp_write_reg(
							TX_P0, SP_TX_LT_SET_REG,
							temp_value);
					}
				}

				sp_read_reg(TX_P0, SP_TX_LINK_BW_SET_REG,
						&temp_value);
				if (temp_value == g_changed_bandwidth) {
					pr_debug("%s %s : LT succeed, bw: %.2x ",
							LOG_TAG, __func__,
							(uint) temp_value);

					pr_debug("Lane0 Set: %.2x\n",
						(uint) __i2c_read_byte(TX_P0,
							SP_TX_LT_SET_REG));
					sp_tx_LT_state = LT_INIT;
					if (HDCP_REPEATER_MODE) {
						hdmi_rx_set_hpd(1);
						pr_debug("%s %s : hdmi_rx_set_hpd 1 !\n",
							LOG_TAG, __func__);
						hdmi_rx_set_termination(1);
					}
				/******A Patch for JIRA CLD-142******
				Description: Under low voltage(DVD10 = 0.97V),
				some chip(4/46) can not output video,
				link down interrupt always happen.
				*************************************/
					if (sp_tx_link_err_check() > 200) {
						pr_warn("%s %s :Need to reset Serdes FIFO.\n",
							LOG_TAG, __func__);
						sp_tx_LT_state = LT_ERROR;
					} else
						goto_next_system_state();
					/*************Patch END***************/
				} else {
					pr_debug("%s %s : bw cur:%.2x, per:%.2x\n",
							LOG_TAG, __func__,
							(uint)temp_value,
					(uint)g_changed_bandwidth);
					sp_tx_LT_state = LT_ERROR;
				}
			} else {
				sp_tx_test_lt = 0;
				sp_tx_LT_state = LT_INIT;
				goto_next_system_state();
			}
		} else {
			pr_debug("%s %s : LANE0 Status error: %.2x\n",
					LOG_TAG, __func__,
					(uint)(temp_value&0x07));
			sp_tx_LT_state = LT_ERROR;
		}
		break;
	default:
		break;
	}
}

/******************End Link training process********************/
/******************Start Output video process********************/
void sp_tx_set_colordepth(void)
{
	unsigned char color_depth;
	unsigned char tmp, tmp2;
	unsigned char data_buf[4];

	if (is_anx_dongle()) {
		sp_tx_aux_dpcdread_bytes(0x00, 0x05, 0x03, 0x04, data_buf);
		if ((data_buf[0] == 0x37) && (data_buf[1] == 0x37)
		    && (data_buf[2] == 0x35) && (data_buf[3] == 0x31)) {
			sp_read_reg(TX_P2, SP_TX_VID_CTRL2_REG, &color_depth);
			i2c_master_read_reg(0x09, 0x09, &tmp2);
			if ((color_depth & 0x70) != (tmp2 & 0x70)) {
				pr_info("%s: reprogramming bits per channel: %d -> %d\n",
					__func__, ((tmp2 & 0x70) >> 3) + 6,
					((color_depth & 0x70) >> 3) + 6);
				tmp = (tmp2 & 0x8f) | (color_depth & 0x70);
				i2c_master_write_reg(0x09, 0x09, tmp);
			}
		}
	}
}

void sp_tx_set_colorspace(void)
{
	unchar color_space;
	unchar c;

	if (down_sample_en) {
		sp_read_reg(RX_P1, HDMI_RX_AVI_DATA00_REG, &color_space);
		color_space &= 0x60;
		if (color_space == 0x20) {
			pr_debug("%s %s : YCbCr4:2:2 ---> PASS THROUGH.\n",
							LOG_TAG, __func__);
			sp_write_reg(TX_P2, SP_TX_VID_CTRL6_REG, 0x00);
			sp_write_reg(TX_P2, SP_TX_VID_CTRL5_REG, 0x00);
			sp_write_reg(TX_P2, SP_TX_VID_CTRL2_REG, 0x11);
			/*sp_write_reg_and_or(TX_P2, SP_TX_VID_CTRL2_REG,
							0xfc,0x01); */
		} else if (color_space == 0x40) {
			pr_debug("%s %s : YCbCr4:4:4 ---> YCbCr4:2:2\n",
							LOG_TAG, __func__);
			sp_write_reg(TX_P2, SP_TX_VID_CTRL6_REG, 0x41);
			sp_write_reg(TX_P2, SP_TX_VID_CTRL5_REG, 0x00);
			sp_write_reg(TX_P2, SP_TX_VID_CTRL2_REG, 0x12);
			/*sp_write_reg_and_or(TX_P2, SP_TX_VID_CTRL2_REG,
							0xfc,0x02); */
		} else if (color_space == 0x00) {
			pr_debug("%s %s : RGB4:4:4 ---> YCbCr4:2:2\n",
							LOG_TAG, __func__);
			sp_write_reg(TX_P2, SP_TX_VID_CTRL6_REG, 0x41);
			sp_write_reg(TX_P2, SP_TX_VID_CTRL5_REG, 0x83);
			sp_write_reg(TX_P2, SP_TX_VID_CTRL2_REG, 0x10);
			/*sp_write_reg_and(TX_P2, SP_TX_VID_CTRL2_REG, 0xfc);*/
		}
	} else {
		switch (sp_tx_rx_type) {
		case DWN_STRM_IS_VGA_9832:
		case DWN_STRM_IS_ANALOG:
		case DWN_STRM_IS_DIGITAL:
			sp_read_reg(TX_P2, SP_TX_VID_CTRL2_REG, &color_space);
			if ((color_space & 0x03) == 0x01) { /*YCBCR422*/
				sp_write_reg_or(TX_P2, SP_TX_VID_CTRL5_REG,
					RANGE_Y2R|CSPACE_Y2R);  /*YUV->RGB*/

				sp_read_reg(RX_P1,
					(HDMI_RX_AVI_DATA00_REG + 3), &c);
				/*vic for BT709.*/
				if ((c == 0x04) || (c == 0x05) || (c == 0x10) ||
					(c == 0x13) || (c == 0x14) ||
					(c == 0x1F) || (c == 0x20) ||
					(c == 0x21) || (c == 0x22) ||
					(c == 0x27) || (c == 0x28) ||
					(c == 0x29) || (c == 0x2E) ||
					(c == 0x2F) || (c == 0x3C) ||
					(c == 0x3D) || (c == 0x3E) ||
					(c == 0x3F) || (c == 0x40))
					sp_write_reg_or(TX_P2,
						SP_TX_VID_CTRL5_REG,
						CSC_STD_SEL);
				else
					sp_write_reg_and(TX_P2,
						SP_TX_VID_CTRL5_REG,
						~CSC_STD_SEL);

				sp_write_reg_or(TX_P2, SP_TX_VID_CTRL6_REG,
				VIDEO_PROCESS_EN|UP_SAMPLE);/*up sample*/
			} else if ((color_space & 0x03) == 0x02) {/*YCBCR444 */

				sp_write_reg_or(TX_P2, SP_TX_VID_CTRL5_REG,
					RANGE_Y2R|CSPACE_Y2R);  /*YUV->RGB*/

				sp_read_reg(RX_P1,
					(HDMI_RX_AVI_DATA00_REG + 3), &c);
				/*vic for BT709. */
				if ((c == 0x04) || (c == 0x05) || (c == 0x10) ||
					(c == 0x13) || (c == 0x14) ||
					(c == 0x1F) || (c == 0x20) ||
					(c == 0x21) || (c == 0x22) ||
					(c == 0x27) || (c == 0x28) ||
					(c == 0x29) || (c == 0x2E) ||
					(c == 0x2F) || (c == 0x3C) ||
					(c == 0x3D) || (c == 0x3E) ||
					(c == 0x3F) ||
					(c == 0x40))
					sp_write_reg_or(TX_P2,
						SP_TX_VID_CTRL5_REG,
						CSC_STD_SEL);
				else
					sp_write_reg_and(TX_P2,
						SP_TX_VID_CTRL5_REG,
						~CSC_STD_SEL);

				sp_write_reg_or_and(TX_P2, SP_TX_VID_CTRL6_REG,
						VIDEO_PROCESS_EN, ~UP_SAMPLE);
			} else if ((color_space & 0x03) == 0x00)  {/*RGB*/
				sp_write_reg_and(TX_P2, SP_TX_VID_CTRL5_REG,
					(~RANGE_Y2R) & (~CSPACE_Y2R) &
					(~CSC_STD_SEL));
				sp_write_reg_and(TX_P2, SP_TX_VID_CTRL6_REG,
					(~VIDEO_PROCESS_EN) & (~UP_SAMPLE));
			}
			break;

		case DWN_STRM_IS_HDMI:
			sp_write_reg(TX_P2, SP_TX_VID_CTRL6_REG, 0x00);
			sp_write_reg(TX_P2, SP_TX_VID_CTRL5_REG, 0x00);
			break;
		default:
			break;
		}
	}
}

void sp_tx_avi_setup(void)
{
	unchar c;
	int i;

	for (i = 0; i < 13; i++) {
		sp_read_reg(RX_P1, (HDMI_RX_AVI_DATA00_REG + i), &c);
		sp_tx_packet_avi.AVI_data[i] = c;
	}
	/*  not change AVI for down sample video
	if(down_sample_en)
		sp_tx_packet_avi.AVI_data[0] =(sp_tx_packet_avi.AVI_data[0] &
								0x9f) |0x20;
	else
	*/
	{
		switch (sp_tx_rx_type) {
		case DWN_STRM_IS_VGA_9832:
		case DWN_STRM_IS_ANALOG:
		case DWN_STRM_IS_DIGITAL:
			sp_tx_packet_avi.AVI_data[0] &= ~0x60;
			break;
		case DWN_STRM_IS_HDMI:
		/*case DWN_STRM_IS_DIGITAL:i */
			break;
		default:
			break;
		}
	}
}
static void sp_tx_load_packet(enum PACKETS_TYPE type)
{
	int i;
	unchar c;

	switch (type) {
	case AVI_PACKETS:
		sp_write_reg(TX_P2, SP_TX_AVI_TYPE, 0x82);
		sp_write_reg(TX_P2, SP_TX_AVI_VER, 0x02);
		sp_write_reg(TX_P2, SP_TX_AVI_LEN, 0x0d);

		for (i = 0; i < 13; i++) {
			sp_write_reg(TX_P2, SP_TX_AVI_DB0 + i,
					sp_tx_packet_avi.AVI_data[i]);
		}

		break;

	case SPD_PACKETS:
		sp_write_reg(TX_P2, SP_TX_SPD_TYPE, 0x83);
		sp_write_reg(TX_P2, SP_TX_SPD_VER, 0x01);
		sp_write_reg(TX_P2, SP_TX_SPD_LEN, 0x19);

		for (i = 0; i < 25; i++) {
			sp_write_reg(TX_P2, SP_TX_SPD_DB0 + i,
					sp_tx_packet_spd.SPD_data[i]);
		}

		break;

	case VSI_PACKETS:
		sp_write_reg(TX_P2, SP_TX_MPEG_TYPE, 0x81);
		sp_write_reg(TX_P2, SP_TX_MPEG_VER, 0x01);
		sp_read_reg(RX_P1, HDMI_RX_MPEG_LEN_REG, &c);
		sp_write_reg(TX_P2, SP_TX_MPEG_LEN, c);

		for (i = 0; i < 10; i++) {
			sp_write_reg(TX_P2, SP_TX_MPEG_DB0 + i,
					sp_tx_packet_mpeg.MPEG_data[i]);
		}

		break;
	case MPEG_PACKETS:
		sp_write_reg(TX_P2, SP_TX_MPEG_TYPE, 0x85);
		sp_write_reg(TX_P2, SP_TX_MPEG_VER, 0x01);
		sp_write_reg(TX_P2, SP_TX_MPEG_LEN, 0x0d);

		for (i = 0; i < 10; i++) {
			sp_write_reg(TX_P2, SP_TX_MPEG_DB0 + i,
					sp_tx_packet_mpeg.MPEG_data[i]);
		}

		break;
	case AUDIF_PACKETS:
		sp_write_reg(TX_P2, SP_TX_AUD_TYPE, 0x84);
		sp_write_reg(TX_P2, SP_TX_AUD_VER, 0x01);
		sp_write_reg(TX_P2, SP_TX_AUD_LEN, 0x0a);
		for (i = 0; i < 10; i++) {
			sp_write_reg(TX_P2, SP_TX_AUD_DB0 + i,
					sp_tx_audioinfoframe.pb_byte[i]);
		}

		break;

	default:
		break;
	}
}

void sp_tx_config_packets(enum PACKETS_TYPE bType)
{
	unchar c;

	switch (bType) {
	case AVI_PACKETS:

		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c &= ~AVI_IF_EN;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);
		sp_tx_load_packet(AVI_PACKETS);

		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= AVI_IF_UD;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);

		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= AVI_IF_EN;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);

		break;

	case SPD_PACKETS:
		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c &= ~SPD_IF_EN;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);
		sp_tx_load_packet(SPD_PACKETS);

		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= SPD_IF_UD;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);

		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c |=  SPD_IF_EN;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);

		break;

	case VSI_PACKETS:
		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c &= ~MPEG_IF_EN;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);

		sp_tx_load_packet(VSI_PACKETS);

		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= MPEG_IF_UD;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);

		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= MPEG_IF_EN;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);

		break;
	case MPEG_PACKETS:
		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c &= ~MPEG_IF_EN;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);


		sp_tx_load_packet(MPEG_PACKETS);

		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= MPEG_IF_UD;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);

		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= MPEG_IF_EN;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);

		break;
	case AUDIF_PACKETS:
		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c &= ~AUD_IF_EN;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);

		sp_tx_load_packet(AUDIF_PACKETS);

		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= AUD_IF_UP;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);

		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= AUD_IF_EN;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);
		break;
	default:
		break;
	}
}

void slimport_config_video_output(void)
{
	unchar temp_value;

	switch (sp_tx_vo_state) {
	default:
	case VO_WAIT_VIDEO_STABLE:
		/*HDMI RX video judgei*/
		sp_read_reg(RX_P0, HDMI_RX_SYS_STATUS_REG, &temp_value);
		if ((temp_value & (TMDS_DE_DET | TMDS_CLOCK_DET)) ==
						0x03) { /*input video stable */
			sp_tx_bw_lc_sel(sp_tx_pclk_calc());
			sp_tx_enable_video_input(0);
			sp_tx_avi_setup();
			sp_tx_config_packets(AVI_PACKETS);
			sp_tx_set_colorspace();
			sp_tx_lvttl_bit_mapping();
			if (__i2c_read_byte(RX_P0, RX_PACKET_REV_STA) &
					VSI_RCVD)  /*VSI packet received*/
					hdmi_rx_new_vsi_int();
			sp_tx_enable_video_input(1);
			sp_tx_vo_state = VO_WAIT_TX_VIDEO_STABLE;
		} else {
			pr_warn("%s %s :HDMI input video not stable!\n",
						LOG_TAG, __func__);
		}
		SP_BREAK(VO_WAIT_VIDEO_STABLE, sp_tx_vo_state);
	case VO_WAIT_TX_VIDEO_STABLE:
		/*DP TX video judge */
		sp_read_reg(TX_P0, SP_TX_SYS_CTRL2_REG, &temp_value);
		sp_write_reg(TX_P0, SP_TX_SYS_CTRL2_REG, temp_value);
		sp_read_reg(TX_P0, SP_TX_SYS_CTRL2_REG, &temp_value);
		if (temp_value & CHA_STA) {
			pr_warn("%s %s : Stream clock not stable!\n",
							LOG_TAG, __func__);
		} else {
			sp_read_reg(TX_P0, SP_TX_SYS_CTRL3_REG, &temp_value);
			sp_write_reg(TX_P0, SP_TX_SYS_CTRL3_REG, temp_value);
			sp_read_reg(TX_P0, SP_TX_SYS_CTRL3_REG, &temp_value);
			if (!(temp_value & STRM_VALID)) {
				pr_err("%s %s : video stream not valid!\n",
							LOG_TAG, __func__);
			} else {
			#ifdef DEMO_4K_2K
				if (down_sample_en)
					sp_tx_vo_state = VO_FINISH;
				else
			#endif
					sp_tx_vo_state = VO_CHECK_VIDEO_INFO;
				}
		}
		SP_BREAK(VO_WAIT_TX_VIDEO_STABLE, sp_tx_vo_state);
	/*
	case VO_WAIT_PLL_LOCK:
		if (!sp_tx_get_pll_lock_status()) {
			//pll reset when plll not lock. by span 20130217
			sp_read_reg(TX_P0, SP_TX_PLL_CTRL_REG, &temp_value);
				temp_value |= PLL_RST;
			sp_write_reg(TX_P0, SP_TX_PLL_CTRL_REG, temp_value);
				temp_value &=~PLL_RST;
			sp_write_reg(TX_P0, SP_TX_PLL_CTRL_REG, temp_value);
				pr_info("%s %s : PLL not lock!\n",
							LOG_TAG, __func__);
		} else
			sp_tx_vo_state = VO_CHECK_BW;
			SP_BREAK(VO_WAIT_PLL_LOCK, sp_tx_vo_state);
		*/
	case VO_CHECK_VIDEO_INFO:
		temp_value = __i2c_read_byte(RX_P0, HDMI_STATUS) & HDMI_MODE;
		g_hdmi_dvi_status = read_dvi_hdmi_mode();
		if (!sp_tx_bw_lc_sel(sp_tx_pclk_calc())
				&& (g_hdmi_dvi_status & _BIT0) == temp_value)
			sp_tx_vo_state++;
		else {
			if ((g_hdmi_dvi_status & _BIT0) != temp_value) {
				pr_debug("%s %s : Different mode of DVI or HDMI mode\n",
							LOG_TAG, __func__);
				g_hdmi_dvi_status = temp_value;
				if ((g_hdmi_dvi_status & _BIT0) !=
								HDMI_MODE)
					hdmi_rx_mute_audio(0);
			}
			sp_tx_set_sys_state(STATE_LINK_TRAINING);
		}
		SP_BREAK(VO_CHECK_VIDEO_INFO, sp_tx_vo_state);
	case VO_FINISH:
		slimport_block_power_ctrl(SP_TX_PWR_AUDIO, SP_POWER_DOWN);
		hdmi_rx_mute_video(0);
		/*sp_tx_lvttl_bit_mapping();*/
		sp_tx_video_mute(0);
		/*sp_tx_set_colorspace();
		sp_tx_avi_setup();
		sp_tx_config_packets(AVI_PACKETS); */
		/*sp_tx_enable_video_input(1);i*/
		sp_tx_show_infomation();
		goto_next_system_state();
		break;
	}
}
/******************End Output video process********************/
/******************Start HDCP process********************/
static void sp_tx_hdcp_encryption_disable(void)
{
	sp_write_reg_and(TX_P0, TX_HDCP_CTRL0, ~ENC_EN);
}

static void sp_tx_hdcp_encryption_enable(void)
{
	sp_write_reg_or(TX_P0, TX_HDCP_CTRL0, ENC_EN);
}

static void sp_tx_hw_hdcp_enable(void)
{
	unchar c;

	sp_write_reg_and(TX_P0, TX_HDCP_CTRL0, (~ENC_EN) & (~HARD_AUTH_EN));
	sp_write_reg_or(TX_P0, TX_HDCP_CTRL0, HARD_AUTH_EN |
				BKSV_SRM_PASS | KSVLIST_VLD | ENC_EN);

	sp_read_reg(TX_P0, TX_HDCP_CTRL0, &c);
	pr_debug("%s %s : TX_HDCP_CTRL0 = %.2x\n", LOG_TAG, __func__, (uint)c);
	sp_write_reg(TX_P0, SP_TX_WAIT_R0_TIME, 0xb0);
	sp_write_reg(TX_P0, SP_TX_WAIT_KSVR_TIME, 0xc8);

	pr_debug("%s %s : Hardware HDCP is enabled.\n", LOG_TAG, __func__);

}

static int hdcp_encryption_check(void)
{
	unchar c;
	sp_read_reg(TX_P0, TX_HDCP_CTRL0, &c);
	if ((c & ENC_EN) && (c & HARD_AUTH_EN))
		return 1;
	else
		return 0;
}

static bool sp_tx_get_ds_video_status(void)
{
	unchar c;

	sp_tx_aux_dpcdread_bytes(0x00, 0x05, 0x27, 1, &c);
	pr_debug("%s %s : 0x00527 = %.2x.\n", LOG_TAG, __func__, (uint)c);

	if (c & 0x01)
		return 1;
	else
		return 0;
}

#ifdef NEW_HDCP_CONTROL_LOGIC
unsigned char check_rx_hdcp_status(void)
{
	unsigned char counter;

	counter = 30;
	do {
		if (__i2c_read_byte(RX_P1, HDMI_RX_HDCP_STATUS_REG) & AUTH_EN)
			break;
		else
			msleep(10);
	} while (--counter);

	return (counter == 0) ? 0 : 1;
}
#endif
void slimport_hdcp_process(void)
{
	/*unchar c; */
	static unchar ds_vid_stb_cntr, HDCP_fail_count;

	switch (HDCP_state) {
	case HDCP_CAPABLE_CHECK:
		ds_vid_stb_cntr = 0;
		HDCP_fail_count = 0;
		HDCP_state = HDCP_WAITTING_VID_STB;

		if (external_block_en == 0) {
			#ifdef NEW_HDCP_CONTROL_LOGIC
			unsigned char temp_var;

			temp_var = 0;

			if (check_rx_hdcp_status()) {
				/*AP enable HDCP*/
				if (sp_tx_rx_type == DWN_STRM_IS_HDMI) {
					temp_var = slimport_hdcp_cap_check();
					if (g_hdcp_cap_bak != temp_var) {
						hdmi_rx_hdcp_cap(
							g_hdcp_cap_bak);
						g_hdcp_cap_bak = temp_var;
						sp_tx_set_sys_state(
							STATE_LINK_TRAINING);
					}
				} else {
					/*AP does not do HDCP*/
					if (g_hdcp_cap_bak == 0)
						HDCP_state = HDCP_NOT_SUPPORT;
				}
			} else {/*AP disable HDCP*/
				HDCP_state = HDCP_NOT_SUPPORT;
				pr_warn("%s %s : Source disable HDCP.\n",
					LOG_TAG, __func__); /*for debug*/
			}

			#else
			if ((sp_tx_rx_type == DWN_STRM_IS_ANALOG)
				|| (sp_tx_rx_type == DWN_STRM_IS_VGA_9832)
				|| (slimport_hdcp_cap_check() == 0))
				HDCP_state = HDCP_NOT_SUPPORT;
			#endif
		}

		SP_BREAK(HDCP_CAPABLE_CHECK, HDCP_state);
	case HDCP_WAITTING_VID_STB:
		/*In case ANX7730 can not get ready video*/
		if (sp_tx_rx_type == DWN_STRM_IS_HDMI) {
			/*pr_info("%s %s : video stb : count%.2x\n",
				LOG_TAG, __func__,(WORD)ds_vid_stb_cntr); */
			if (!sp_tx_get_ds_video_status()) {
				if (ds_vid_stb_cntr >= 50) {
					vbus_power_ctrl(0);
					reg_hardware_reset();
					ds_vid_stb_cntr = 0;
				} else {
					ds_vid_stb_cntr++;
					usleep_range(10000, 10000);
				}
				#ifdef SLIMPORT_DRV_DEBUG
				pr_warn("%s %s : downstream video not stable\n",
					LOG_TAG, __func__); /*for debug*/
				#endif
			} else {
				ds_vid_stb_cntr = 0;
				HDCP_state = HDCP_HW_ENABLE;
			}
		} else {
			HDCP_state = HDCP_HW_ENABLE;
		}
		SP_BREAK(HDCP_WAITTING_VID_STB, HDCP_state);
	case HDCP_HW_ENABLE:
		sp_tx_video_mute(1);
		/*sp_tx_clean_hdcp_status();*/
		slimport_block_power_ctrl(SP_TX_PWR_HDCP, SP_POWER_ON);
		sp_write_reg(TX_P2, SP_COMMON_INT_MASK2, 0x01);
		/*sp_tx_video_mute(0);
		sp_tx_aux_polling_disable();
		sp_tx_clean_hdcp_status(); */
		msleep(50);
		/*disable auto polling during hdcp. */
		sp_tx_hw_hdcp_enable();
		HDCP_state = HDCP_WAITTING_FINISH;
	case HDCP_WAITTING_FINISH:
		break;
	case HDCP_FINISH:
		sp_tx_hdcp_encryption_enable();
		hdmi_rx_mute_video(0);
		sp_tx_video_mute(0);
		/*enable auto polling after hdcp. */
		/*sp_tx_aux_polling_enable(); */
		goto_next_system_state();
		HDCP_state = HDCP_CAPABLE_CHECK;
		pr_debug("%s %s : @@@@@@@hdcp_auth_pass@@@@@@\n",
							LOG_TAG, __func__);
		break;
	case HDCP_FAILE:
		if (HDCP_fail_count > 5) {
			vbus_power_ctrl(0);
			reg_hardware_reset();
			HDCP_state = HDCP_CAPABLE_CHECK;
			HDCP_fail_count = 0;
			pr_err("%s %s : *********hdcp_auth_failed*********\n",
							LOG_TAG, __func__);
		} else {
			HDCP_fail_count++;
			HDCP_state = HDCP_WAITTING_VID_STB;
		}
		break;
	default:
	case HDCP_NOT_SUPPORT:
		pr_debug("%s %s : Sink is not capable HDCP\n",
							LOG_TAG, __func__);
		sp_tx_video_mute(1); /* Block output without HDCP */
		vbus_power_ctrl(0);
		reg_hardware_reset();
		HDCP_state = HDCP_CAPABLE_CHECK;
		HDCP_fail_count = 0;
		break;
	}
}
#if 0
void slimport_hdcp_repeater_reauth(void) /* hdcp issue test 20140527 */
{
	unchar c, hdcp_ctrl, hdcp_staus;

	if ((!hdcp_repeater_passed)
		&& (sp_tx_system_state > STATE_HDCP_AUTH)
		&& (sp_tx_rx_type != DWN_STRM_IS_ANALOG)
		&& (sp_tx_rx_type != DWN_STRM_IS_VGA_9832)) {
		msleep(50);

		sp_read_reg(RX_P1, HDMI_RX_HDCP_STATUS_REG, &c);
		if ((c & AUTH_EN) != 0) {
			debug_puts("Upstream HDMI HDCP request!\n");
			sp_read_reg(TX_P0, TX_HDCP_CTRL0, &hdcp_ctrl);
			if ((hdcp_ctrl & HARD_AUTH_EN) == 0) {
				debug_puts("Repeater Mode: Enable HW HDCP\n");
				slimport_block_power_ctrl(SP_TX_PWR_HDCP,
								SP_POWER_ON);
				sp_write_reg(TX_P2, SP_COMMON_INT_MASK2, 0x01);
				delay_ms(50);
				sp_tx_hw_hdcp_enable();
			} else {
			sp_read_reg(TX_P0, SP_TX_HDCP_STATUS, &hdcp_staus);
				if (((hdcp_staus & SP_TX_HDCP_AUTH_PASS) == 0)
					& ((hdcp_staus & SP_TX_HDCP_AUTH_FAIL)
									== 1)) {
					int ret = 0;
			debug_puts("Repeater Mode: %%%%HDCP Failure%%%%\n");
					sp_tx_clean_hdcp_status();

					slimport_block_power_ctrl(
						SP_TX_PWR_HDCP, SP_POWER_ON);
					sp_write_reg(TX_P2, SP_COMMON_INT_MASK2,
									0X01);
					msleep(50);
					sp_tx_hw_hdcp_enable();
		debug_puts("Repeater Mode: Clean HDCP and re-auth HDCP\n");
				} else if ((hdcp_staus & SP_TX_HDCP_AUTH_PASS)
									== 1) {
			debug_puts("Repeater Mode: $$$$$HDCP Pass$$$$$\n");
				}
			}
		}
	}
	if ((hdcp_repeater_passed)
		&& (sp_tx_system_state > STATE_HDCP_AUTH)
		&& (sp_tx_rx_type == DWN_STRM_IS_HDMI)) {
		sp_read_reg(TX_P0, TX_HDCP_CTRL0, &hdcp_ctrl);
		sp_read_reg(TX_P0, SP_TX_HDCP_STATUS, &hdcp_staus);
		if ((hdcp_ctrl == TX_HDCP_FUNCTION_ENABLE)
			&& ((hdcp_staus & SP_TX_HDCP_PASSED_STATUS)
			!= SP_TX_HDCP_PASSED_STATUS)) {
			debug_puts("main link error:HDCP encryption failure\n");
			sp_tx_set_sys_state(STATE_LINK_TRAINING);
		}
	}
}
#endif
/******************End HDCP process********************/
/******************Start Audio process********************/
static void sp_tx_audioinfoframe_setup(void)
{
	int i;
	unchar c;

	sp_read_reg(RX_P1, HDMI_RX_AUDIO_TYPE_REG, &c);
	sp_tx_audioinfoframe.type = c;
	sp_read_reg(RX_P1, HDMI_RX_AUDIO_VER_REG, &c);
	sp_tx_audioinfoframe.version = c;
	sp_read_reg(RX_P1, HDMI_RX_AUDIO_LEN_REG, &c);
	sp_tx_audioinfoframe.length = c;
	for (i = 0; i < 11; i++) {
		sp_read_reg(RX_P1, (HDMI_RX_AUDIO_DATA00_REG + i), &c);
		sp_tx_audioinfoframe.pb_byte[i] = c;
	}
}

static void info_ANX7730_AUIF_changed(void)
{
	unchar temp, count;
	unchar pBuf[3] = {0x01, 0xd1, 0x02};

	usleep_range(20000, 20000);
	if (sp_tx_rx_type == DWN_STRM_IS_HDMI) {/*assuming it is anx7730*/
		sp_tx_aux_dpcdread_bytes(0x00, 0x05, 0x23, 1, &temp);
		if (temp < 0x94) {
			count = 3;
			do {
				usleep_range(20000, 20000);
				if (sp_tx_aux_dpcdwrite_bytes(0x00, 0x05,
						0xf0, 3, pBuf) == AUX_OK)
					break;
				if (!count)
					pr_err("%s %s : dpcd write error\n",
							LOG_TAG, __func__);
			} while (--count);
		}
	}
}

static void sp_tx_enable_audio_output(unchar benable)
{
	unchar c;

	sp_read_reg(TX_P0, SP_TX_AUD_CTRL, &c);
	if (benable) {
		if (c & AUD_EN) {/*if it has been enabled, disable first.*/
			c &= ~AUD_EN;
			sp_write_reg(TX_P0, SP_TX_AUD_CTRL, c);
		}
		sp_tx_audioinfoframe_setup();
		sp_tx_config_packets(AUDIF_PACKETS);

		/*for audio multi-ch*/
		info_ANX7730_AUIF_changed();
		/*end audio multi-ch */
		c |= AUD_EN;
		sp_write_reg(TX_P0, SP_TX_AUD_CTRL, c);
	} else {
		c &= ~AUD_EN;
		sp_write_reg(TX_P0, SP_TX_AUD_CTRL, c);
		sp_write_reg_and(TX_P0, SP_TX_PKT_EN_REG, ~AUD_IF_EN);
	}
}

static void sp_tx_config_audio(void)
{
	unchar c;
	int i;
	ulong M_AUD, LS_Clk = 0;
	ulong AUD_Freq = 0;

	pr_debug("%s %s : **Config audio **\n", LOG_TAG, __func__);
	slimport_block_power_ctrl(SP_TX_PWR_AUDIO, SP_POWER_ON);
	sp_read_reg(RX_P0, 0xCA, &c);

	switch (c & 0x0f) {
	case 0x00:
		AUD_Freq = 44.1;
		break;
	case 0x02:
		AUD_Freq = 48;
		break;
	case 0x03:
		AUD_Freq = 32;
		break;
	case 0x08:
		AUD_Freq = 88.2;
		break;
	case 0x0a:
		AUD_Freq = 96;
		break;
	case 0x0c:
		AUD_Freq = 176.4;
		break;
	case 0x0e:
		AUD_Freq = 192;
		break;
	default:
		break;
	}

	switch (sp_tx_get_link_bw()) {
	case LINK_1P62G:
		LS_Clk = 162000;
		break;
	case LINK_2P7G:
		LS_Clk = 270000;
		break;
	case LINK_5P4G:
		LS_Clk = 540000;
		break;
	case LINK_6P75G:
		LS_Clk = 675000;
		break;
	default:
		break;
	}

	pr_debug("%s %s : AUD_Freq = %ld , LS_CLK = %ld\n",
					LOG_TAG, __func__, AUD_Freq, LS_Clk);

	M_AUD = ((512 * AUD_Freq) / LS_Clk) * 32768;
	M_AUD = M_AUD + 0x05;
	sp_write_reg(TX_P1, SP_TX_AUD_INTERFACE_CTRL4, (M_AUD & 0xff));
	M_AUD = M_AUD >> 8;
	sp_write_reg(TX_P1, SP_TX_AUD_INTERFACE_CTRL5, (M_AUD & 0xff));
	sp_write_reg(TX_P1, SP_TX_AUD_INTERFACE_CTRL6, 0x00);

	sp_write_reg_and(TX_P1, SP_TX_AUD_INTERFACE_CTRL0,
						~AUD_INTERFACE_DISABLE);

	sp_write_reg_or(TX_P1, SP_TX_AUD_INTERFACE_CTRL2, M_AUD_ADJUST_ST);

	sp_read_reg(RX_P0, HDMI_STATUS, &c);
	if (c & HDMI_AUD_LAYOUT)
		sp_write_reg_or(TX_P2, SP_TX_AUD_CH_NUM_REG5,
						CH_NUM_8 | AUD_LAYOUT);
	 else
		sp_write_reg_and(TX_P2, SP_TX_AUD_CH_NUM_REG5,
					(~CH_NUM_8) & (~AUD_LAYOUT));

	/* transfer audio chaneel status from HDMI Rx to Slinmport Tx */
	for (i = 0; i < 5; i++) {
		sp_read_reg(RX_P0, (HDMI_RX_AUD_IN_CH_STATUS1_REG + i), &c);
		sp_write_reg(TX_P2, (SP_TX_AUD_CH_STATUS_REG1 + i), c);
	}

	/* enable audio */
	sp_tx_enable_audio_output(1);
	/*
	sp_read_reg(TX_P2, SP_COMMON_INT_MASK1, &c);
	c |= 0x04;
	sp_write_reg(TX_P2, SP_COMMON_INT_MASK1, c);
	*/

}

void slimport_config_audio_output(void)
{
	static unchar count;

	switch (sp_tx_ao_state) {
	default:
	case AO_INIT:
	case AO_CTS_RCV_INT:
	case AO_AUDIO_RCV_INT:
		if (!(__i2c_read_byte(RX_P0, HDMI_STATUS) & HDMI_MODE)) {
			sp_tx_ao_state = AO_INIT;
			goto_next_system_state();
		}
		break;
	case AO_RCV_INT_FINISH:
		if (count++ > 2)
			sp_tx_ao_state = AO_OUTPUT;
		else
			sp_tx_ao_state = AO_INIT;
		SP_BREAK(AO_INIT, sp_tx_ao_state);
	case AO_OUTPUT:
		count = 0;
		sp_tx_ao_state = AO_INIT;
		sp_tx_video_mute(0);
		hdmi_rx_mute_audio(0);
		sp_tx_config_audio();
		goto_next_system_state();
		break;
	}
}

#ifdef QUICK_CHARGE_SUPPORT
void QC20_process(void)
{
	unchar val = 0;

	/*Write dpcd for QC2.0*/
	val = get_current_voltage();
	pr_debug("QC20_process, val=%d\n", val);
	switch (val) {
	case 0:
	case 5:
		val = 0x09;
		sp_tx_aux_dpcdwrite_bytes(0, 5, SINK_POWER_DELIVERY, 1, &val);
		break;
	default:
		break;
	}
	val = 0x01;
	sp_tx_aux_dpcdwrite_bytes(0, 5, SINK_GOOD_BATTERY, 1, &val);

}
#endif
/******************End Audio process********************/
void slimport_initialization(void)
{
	#ifdef ENABLE_READ_EDID
	g_read_edid_flag = 0;
	#endif
	slimport_block_power_ctrl(SP_TX_PWR_REG, SP_POWER_ON);
	slimport_block_power_ctrl(SP_TX_PWR_TOTAL, SP_POWER_ON);
	/*Driver Version*/
	sp_write_reg(TX_P1, FW_VER_REG, FW_VERSION);
	/*disable OCM*/
	sp_write_reg_or(TX_P1, OCM_REG3, OCM_RST);
	vbus_power_ctrl(1);
	hdmi_rx_initialization();
	sp_tx_initialization();
	msleep(200);
	#ifdef QUICK_CHARGE_SUPPORT
	QC20_process();
	#endif
	sp_tx_aux_polling_enable();
	goto_next_system_state();
}

void slimport_cable_monitor(void)
{
	unchar cur_cable_type;

	if (sp_tx_system_state_bak != sp_tx_system_state
			&& HDCP_state != HDCP_WAITTING_FINISH
			&& sp_tx_LT_state != LT_WAITTING_FINISH) {
		if (sp_tx_cur_states() > STATE_SINK_CONNECTION) {
			cur_cable_type =
				sp_tx_get_cable_type(GETTED_CABLE_TYPE, 0);
			if (cur_cable_type != sp_tx_rx_type) {/*cable changed*/
				/*reg_hardware_reset(); */
				sp_tx_clean_state_machine();
				sp_tx_set_sys_state(STATE_SP_INITIALIZED);
				msleep(500);
			}
		}
	}
}

void hdcp_external_ctrl_flag_monitor(void)
{
	static unchar cur_flag;

	if ((sp_tx_rx_type == DWN_STRM_IS_ANALOG)
			|| (sp_tx_rx_type == DWN_STRM_IS_VGA_9832)) {
		if (external_block_en != cur_flag) {
			cur_flag = external_block_en;
			system_state_change_with_case(STATE_HDCP_AUTH);
		}
	}
}

void slimport_state_process(void)
{
	switch (sp_tx_system_state) {
	case STATE_INIT:
		goto_next_system_state();
	case STATE_WAITTING_CABLE_PLUG:
		slimport_waitting_cable_plug_process();
		SP_BREAK(STATE_WAITTING_CABLE_PLUG, sp_tx_system_state);
	case STATE_SP_INITIALIZED:
		slimport_initialization();
		SP_BREAK(STATE_SP_INITIALIZED, sp_tx_system_state);
	case STATE_SINK_CONNECTION:
		slimport_sink_connection();
		SP_BREAK(STATE_SINK_CONNECTION, sp_tx_system_state);
#ifdef ENABLE_READ_EDID
	case STATE_PARSE_EDID:
		slimport_edid_process();
		SP_BREAK(STATE_PARSE_EDID, sp_tx_system_state);
#endif
	case STATE_LINK_TRAINING:
		slimport_link_training();
		SP_BREAK(STATE_LINK_TRAINING, sp_tx_system_state);
	case STATE_VIDEO_OUTPUT:
		slimport_config_video_output();
		SP_BREAK(STATE_VIDEO_OUTPUT, sp_tx_system_state);
	case STATE_HDCP_AUTH:
		slimport_hdcp_process();
		sp_tx_set_colordepth();
		SP_BREAK(STATE_HDCP_AUTH, sp_tx_system_state);
	case STATE_AUDIO_OUTPUT:
		slimport_config_audio_output();
		SP_BREAK(STATE_AUDIO_OUTPUT, sp_tx_system_state);
	case STATE_PLAY_BACK:
		/*slimport_playback_process();*/
		if (!hdcp_encryption_check()) {
			pr_err("%s: HDCP Disabled! Re-init!\n", __func__);
			sp_tx_video_mute(1);
			sp_tx_clean_hdcp_status();
			system_state_change_with_case(STATE_HDCP_AUTH);
		}
		SP_BREAK(STATE_PLAY_BACK, sp_tx_system_state);
	default:
		break;
	}
}
/******************Start INT process********************/
void sp_tx_int_rec(void)
{
	sp_read_reg(TX_P2, SP_COMMON_INT_STATUS1, &COMMON_INT1);
	sp_write_reg(TX_P2, SP_COMMON_INT_STATUS1, COMMON_INT1);

	sp_read_reg(TX_P2, SP_COMMON_INT_STATUS1 + 1, &COMMON_INT2);
	sp_write_reg(TX_P2, SP_COMMON_INT_STATUS1 + 1, COMMON_INT2);

	sp_read_reg(TX_P2, SP_COMMON_INT_STATUS1 + 2, &COMMON_INT3);
	sp_write_reg(TX_P2, SP_COMMON_INT_STATUS1 + 2, COMMON_INT3);

	sp_read_reg(TX_P2, SP_COMMON_INT_STATUS1 + 3, &COMMON_INT4);
	sp_write_reg(TX_P2, SP_COMMON_INT_STATUS1 + 3, COMMON_INT4);

	sp_read_reg(TX_P2, SP_COMMON_INT_STATUS1 + 6, &COMMON_INT5);
	sp_write_reg(TX_P2, SP_COMMON_INT_STATUS1 + 6, COMMON_INT5);
}

void hdmi_rx_int_rec(void)
{
	/* if ((hdmi_system_state < HDMI_CLOCK_DET) ||
			(sp_tx_system_state < STATE_CONFIG_HDMI))
		return;	*/
	 sp_read_reg(RX_P0, HDMI_RX_INT_STATUS1_REG , &HDMI_RX_INT1);
	 sp_write_reg(RX_P0, HDMI_RX_INT_STATUS1_REG , HDMI_RX_INT1);
	 sp_read_reg(RX_P0, HDMI_RX_INT_STATUS2_REG , &HDMI_RX_INT2);
	 sp_write_reg(RX_P0, HDMI_RX_INT_STATUS2_REG , HDMI_RX_INT2);
	 sp_read_reg(RX_P0, HDMI_RX_INT_STATUS3_REG , &HDMI_RX_INT3);
	 sp_write_reg(RX_P0, HDMI_RX_INT_STATUS3_REG , HDMI_RX_INT3);
	 sp_read_reg(RX_P0, HDMI_RX_INT_STATUS4_REG , &HDMI_RX_INT4);
	 sp_write_reg(RX_P0, HDMI_RX_INT_STATUS4_REG , HDMI_RX_INT4);
	 sp_read_reg(RX_P0, HDMI_RX_INT_STATUS5_REG , &HDMI_RX_INT5);
	 sp_write_reg(RX_P0, HDMI_RX_INT_STATUS5_REG , HDMI_RX_INT5);
	 sp_read_reg(RX_P0, HDMI_RX_INT_STATUS6_REG , &HDMI_RX_INT6);
	 sp_write_reg(RX_P0, HDMI_RX_INT_STATUS6_REG , HDMI_RX_INT6);
	 sp_read_reg(RX_P0, HDMI_RX_INT_STATUS7_REG , &HDMI_RX_INT7);
	 sp_write_reg(RX_P0, HDMI_RX_INT_STATUS7_REG , HDMI_RX_INT7);
}
void slimport_int_rec(void)
{
	sp_tx_int_rec();
	hdmi_rx_int_rec();
	/*
	if(!sp_tx_pd_mode ){
	       sp_tx_int_irq_handler();
		hdmi_rx_int_irq_handler();
	}
	*/
}
/******************End INT process********************/
/******************Start task process********************/

static void sp_tx_pll_changed_int_handler(void)
{
	if (sp_tx_system_state >= STATE_LINK_TRAINING) {
		if (!sp_tx_get_pll_lock_status()) {
			pr_warn("%s %s : PLL:PLL not lock!\n",
							LOG_TAG, __func__);
			sp_tx_set_sys_state(STATE_LINK_TRAINING);
		}
	}
}

static void sp_tx_hdcp_link_chk_fail_handler(void)
{
	if (!HDCP_REPEATER_MODE)
		system_state_change_with_case(STATE_HDCP_AUTH);
	pr_err("%s %s : hdcp_link_chk_fail:HDCP Sync lost!\n",
						LOG_TAG, __func__);
}

static unchar link_down_check(void)
{
	unchar return_value;

	return_value = 0;
	pr_warn("%s %s : link_down_check\n", LOG_TAG, __func__);
	if ((sp_tx_system_state > STATE_LINK_TRAINING)) {
		if (!(__i2c_read_byte(TX_P0, DPCD_204) & 0x01)
			|| ((__i2c_read_byte(TX_P0, DPCD_202) &
				(0x01 | 0x04)))  != 0x05) { /*link down flag*/
			if (sp_tx_get_downstream_connection()) {
				sp_tx_set_sys_state(STATE_LINK_TRAINING);
				pr_debug("%s %s : INT:re-LT request!\n",
							LOG_TAG, __func__);
			} else {
				vbus_power_ctrl(0);
				reg_hardware_reset();
				return_value = 1;
			}
		} else {
			pr_debug("%s %s : Lane align %x\n", LOG_TAG, __func__,
				(uint)__i2c_read_byte(TX_P0, DPCD_204));
			pr_debug("%s %s : Lane clock recovery %x\n",
				LOG_TAG, __func__,
				(uint)__i2c_read_byte(TX_P0, DPCD_202));
		}
	}

	return return_value;

}

static void sp_tx_phy_auto_test(void)
{
	unchar b_sw;
	unchar c1;
	unchar bytebuf[16];
	unchar link_bw;

	/*DPCD 0x219 TEST_LINK_RATE*/
	sp_tx_aux_dpcdread_bytes(0x0, 0x02, 0x19, 1, bytebuf);
	pr_debug("%s %s : DPCD:0x00219 = %.2x\n", LOG_TAG, __func__,
						(uint)bytebuf[0]);
	switch (bytebuf[0]) {
	case 0x06:
	case 0x0A:
	case 0x14:
	case 0x19:
		sp_tx_set_link_bw(bytebuf[0]);
		sp_tx_test_bw = bytebuf[0];
		break;
	default:
		sp_tx_set_link_bw(0x19);
		sp_tx_test_bw = 0x19;
		break;
	}


	/*DPCD 0x248 PHY_TEST_PATTERN*/
	sp_tx_aux_dpcdread_bytes(0x0, 0x02, 0x48, 1, bytebuf);
	pr_debug("%s %s : DPCD:0x00248 = %.2x\n", LOG_TAG, __func__,
							(uint)bytebuf[0]);
	switch (bytebuf[0]) {
	case 0:
		/*pr_info("%s %s : No test pattern selected\n",
						LOG_TAG, __func__); */
		break;
	case 1:
		sp_write_reg(TX_P0, SP_TX_TRAINING_PTN_SET_REG, 0x04);
		/*pr_info("%s %s : D10.2 Pattern\n", LOG_TAG, __func__); */
		break;
	case 2:
		sp_write_reg(TX_P0, SP_TX_TRAINING_PTN_SET_REG, 0x08);
		/*pr_info("%s %s : Symbol Error Measurement Count\n",
						LOG_TAG, __func__); */
		break;
	case 3:
		sp_write_reg(TX_P0, SP_TX_TRAINING_PTN_SET_REG, 0x0c);
		/*pr_info("%s %s : PRBS7 Pattern\n", LOG_TAG, __func__); */
		break;
	case 4:
		sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x50, 0xa, bytebuf);
		sp_write_reg(TX_P1, 0x80, bytebuf[0]);
		sp_write_reg(TX_P1, 0x81, bytebuf[1]);
		sp_write_reg(TX_P1, 0x82, bytebuf[2]);
		sp_write_reg(TX_P1, 0x83, bytebuf[3]);
		sp_write_reg(TX_P1, 0x84, bytebuf[4]);
		sp_write_reg(TX_P1, 0x85, bytebuf[5]);
		sp_write_reg(TX_P1, 0x86, bytebuf[6]);
		sp_write_reg(TX_P1, 0x87, bytebuf[7]);
		sp_write_reg(TX_P1, 0x88, bytebuf[8]);
		sp_write_reg(TX_P1, 0x89, bytebuf[9]);
		sp_write_reg(TX_P0, SP_TX_TRAINING_PTN_SET_REG, 0x30);
		/*pr_info("%s %s : 80bit custom pattern transmitted\n",
							LOG_TAG, __func__); */
		break;
	case 5:
		sp_write_reg(TX_P0, 0xA9, 0x00);
		sp_write_reg(TX_P0, 0xAA, 0x01);
		sp_write_reg(TX_P0, SP_TX_TRAINING_PTN_SET_REG, 0x14);
		/*pr_info("%s %s : HBR2 Compliance Eye Pattern\n",
							LOG_TAG, __func__); */
		break;
	}

	sp_tx_aux_dpcdread_bytes(0x00, 0x00, 0x03, 1, bytebuf);
	pr_debug("%s %s : DPCD:0x00003 = %.2x\n", LOG_TAG, __func__,
					(uint)bytebuf[0]);
	switch (bytebuf[0] & 0x01) {
	case 0:
		sp_tx_spread_enable(0);
		/*pr_info("%s %s : SSC OFF\n", LOG_TAG, __func__);*/
		break;
	case 1:
		sp_read_reg(TX_P0, SP_TX_LINK_BW_SET_REG, &c1);
		switch (c1) {
		case 0x06:
			link_bw = 0x06;
			break;
		case 0x0a:
			link_bw = 0x0a;
			break;
		case 0x14:
			link_bw = 0x14;
			break;
		case 0x19:
			link_bw = 0x19;
			break;
		default:
			link_bw = 0x00;
			break;
		}
		sp_tx_config_ssc(SSC_DEP_4000PPM);
		/*pr_info("%s %s : SSC ON\n", LOG_TAG, __func__); */
		break;
	}

	/*get swing and emphasis adjust request*/
	sp_read_reg(TX_P0, 0xA3, &b_sw);

	sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x06, 1, bytebuf);
	pr_debug("%s %s : DPCD:0x00206 = %.2x\n", LOG_TAG, __func__,
				(uint)bytebuf[0]);
	c1 = bytebuf[0] & 0x0f;
	switch (c1) {
	case 0x00:
		sp_write_reg(TX_P0, 0xA3, (b_sw & ~0x1b) | 0x00);
		/*pr_info("%s %s : lane0,Swing0, emp 0db.\n",
						LOG_TAG, __func__);*/
		break;
	case 0x01:
		sp_write_reg(TX_P0, 0xA3, (b_sw & ~0x1b) | 0x01);
		/*pr_info("%s %s : lane0,Swing1, emp 0db.\n",
						LOG_TAG, __func__); */
		break;
	case 0x02:
		sp_write_reg(TX_P0, 0xA3, (b_sw & ~0x1b) | 0x02);
		/*pr_info("%s %s : lane0,Swing2, emp 0db.\n",
						LOG_TAG, __func__); */
		break;
	case 0x03:
		sp_write_reg(TX_P0, 0xA3, (b_sw & ~0x1b) | 0x03);
		/*pr_info("%s %s : lane0,Swing3, emp 0db.\n",
						LOG_TAG, __func__);  */
		break;
	case 0x04:
		sp_write_reg(TX_P0, 0xA3, (b_sw & ~0x1b) | 0x08);
		/* pr_info("%s %s : lane0,Swing0, emp 3.5db.\n",
						LOG_TAG, __func__); */
		break;
	case 0x05:
		sp_write_reg(TX_P0, 0xA3, (b_sw & ~0x1b) | 0x09);
		/*pr_info("%s %s : lane0,Swing1, emp 3.5db.\n",
						LOG_TAG, __func__); */
		break;
	case 0x06:
		sp_write_reg(TX_P0, 0xA3, (b_sw & ~0x1b) | 0x0a);
		/*pr_info("%s %s : lane0,Swing2, emp 3.5db.\n",
						LOG_TAG, __func__); */
		break;
	case 0x08:
		sp_write_reg(TX_P0, 0xA3, (b_sw & ~0x1b) | 0x10);
		/*pr_info("%s %s : lane0,Swing0, emp 6db.\n",
						LOG_TAG, __func__);  */
		break;
	case 0x09:
		sp_write_reg(TX_P0, 0xA3, (b_sw & ~0x1b) | 0x11);
		/*pr_info("%s %s : lane0,Swing1, emp 6db.\n",
						LOG_TAG, __func__);  */
		break;
	case 0x0c:
		sp_write_reg(TX_P0, 0xA3, (b_sw & ~0x1b) | 0x18);
		/*pr_info("%s %s : lane0,Swing0, emp 9.5db.\n",
						LOG_TAG, __func__); */
		break;
	default:
		break;
	}
}

static void sp_tx_sink_irq_int_handler(void)
{
	unchar c, c1;
	unchar IRQ_Vector;/* Int_vector1, Int_vector2; */
	unchar Int_vector[2];/*by span 20130217*/
	unchar test_vector;
	unchar temp, need_return;

	need_return = 0;
	pr_debug("%s %s : sp_tx_sink_irq_int_handler\n" , LOG_TAG, __func__);
	IRQ_Vector = __i2c_read_byte(TX_P0, DPCD_201);
	if (IRQ_Vector != 0)
		sp_tx_aux_dpcdwrite_bytes(0x00, 0x02,
				DPCD_SERVICE_IRQ_VECTOR, 1, &IRQ_Vector);
	else
		return;

	/* HDCP IRQ */
	if ((IRQ_Vector & CP_IRQ)
		&& ((HDCP_state > HDCP_WAITTING_FINISH) ||
			(sp_tx_system_state > STATE_HDCP_AUTH))) {
		sp_tx_aux_dpcdread_bytes(0x06, 0x80, 0x29, 1, &c1);
		if (c1 & 0x04) {
			if (!HDCP_REPEATER_MODE) {
				system_state_change_with_case(STATE_HDCP_AUTH);
				sp_tx_clean_hdcp_status();
			} else
				assisted_HDCP_repeater = HDCP_ERROR;
			pr_debug("%s %s :CP_IRQ, HDCP sync lost.\n",
							LOG_TAG, __func__);
		}
	}
	/******************************************************
	JIRA:CLD-153 Charger  IRQ is not reported when dongle
	is not connected with TV
	******************************************************/
	if ((IRQ_Vector & SINK_SPECIFIC_IRQ)
		&& (sp_tx_system_state > STATE_SP_INITIALIZED)
		&& (downstream_charging_status != NO_CHARGING_CAPABLE)) {
		downstream_charging_status_set();
	}
	/*if current state < sink connection state,
	  the below IRQ don`t need to handler*/
	if (sp_tx_system_state <= STATE_SINK_CONNECTION)
		return;
	/*******End***********************************************/
	/* specific int */
	if (IRQ_Vector & SINK_SPECIFIC_IRQ) {
		if (sp_tx_rx_type == DWN_STRM_IS_HDMI) {
			sp_tx_aux_dpcdread_bytes(0x00, 0x05,
				DPCD_SPECIFIC_INTERRUPT1, 2, Int_vector);
			sp_tx_aux_dpcdwrite_bytes(0x00, 0x05,
						DPCD_SPECIFIC_INTERRUPT1,
						2, Int_vector);

			temp = 0x01;
			do {
				switch (Int_vector[0] & temp) {
				default:
					break;
				case 0x01:
					/*downstream HPD changed from 0 to 1*/
					if ((Int_vector[0] & 0x01) == 0x01) {
						sp_tx_aux_dpcdread_bytes(0x00,
							0x05, 0x18, 1, &c);
						if (c & 0x01)
							pr_debug("%s %s : Downstream HDMI is pluged!\n",
							LOG_TAG, __func__);
						}
					break;
				case 0x02:
					sp_tx_aux_dpcdread_bytes(0x00,
						0x05, 0x18, 1, &c);
					if ((c & (DOWN_STRM_HPD |
						DOWN_R_TERM_DET)) != 0x00) {
						pr_debug("%s %s : fake unplug event!\n",
							LOG_TAG, __func__);
						sp_tx_clean_state_machine();
						sp_tx_set_sys_state(
							STATE_LINK_TRAINING);
					} else {
						pr_warn("%s %s : Downstream HDMI is unpluged!\n",
							LOG_TAG, __func__);
						vbus_power_ctrl(0);
						reg_hardware_reset();
						sp_tx_set_sys_state(
							STATE_SP_INITIALIZED);
						need_return = 1;
					}
					break;
				case 0x04:
					pr_warn("%s %s : Here is check: Link is down!\n",
							LOG_TAG, __func__);
					if ((sp_tx_system_state >
							STATE_VIDEO_OUTPUT)) {
						pr_warn("%s %s : Rx specific  IRQ: Link is down!\n",
						LOG_TAG, __func__);
						need_return = link_down_check();
					}
					break;
				case 0x08:
					if ((Int_vector[0] & 0x10)
								!= 0x10){
						pr_debug("%s %s : Downstream HDCP is passed!\n",
							LOG_TAG, __func__);
					} else {
						if ((!HDCP_REPEATER_MODE)
						&& (sp_tx_system_state >=
							STATE_HDCP_AUTH)) {

							sp_tx_video_mute(1);
							sp_tx_set_sys_state(
							STATE_HDCP_AUTH);
							pr_debug("%s %s : Re-authentication due to downstream HDCP failure!\n",
							LOG_TAG, __func__);
						sp_tx_clean_hdcp_status();
						} else if (
							HDCP_REPEATER_MODE) {
							assisted_HDCP_repeater =
								HDCP_ERROR;

								pr_debug("%s %s : Re-authentication due to downstream HDCP failure!\n",
							LOG_TAG, __func__);
						}
					}
					break;
				case 0x10:
					break;
				case 0x20:
					pr_warn("%s %s : Downstream HDCP link integrity check fail!\n",
							LOG_TAG, __func__);
					if (!HDCP_REPEATER_MODE) {
						system_state_change_with_case(
							STATE_HDCP_AUTH);
						/*msleep(2000); 20130217 by
						span for samsung monitor */
						sp_tx_clean_hdcp_status();
					} else {
						if ((sp_tx_system_state
							> STATE_HDCP_AUTH))
							assisted_HDCP_repeater =
								HDCP_ERROR;
					}
					pr_debug("%s %s : IRQ:____________HDCP Sync lost!\n",
							LOG_TAG, __func__);
					break;
				#ifdef CEC_ENABLE
				case 0x40:
					if (TASK_OK == dp_cec_recv_process())
						cec.dp_cec_receive_done = 1;
					pr_debug("%s %s : Receive CEC command from downstream done!\n",
							LOG_TAG, __func__);
					break;
				case 0x80:
					cec.dp_cec_transmit_done = 1;
					pr_debug("%s %s : CEC command transfer to downstream done!\n",
						LOG_TAG, __func__);
					break;
				 #endif
				}

				if (need_return == 1)
					return;

				temp = (temp << 1);
			} while (temp != 0);

			#ifdef CEC_ENABLE
			if (Int_vector[1] & 0x03)   /*tx error or no ack */
				cec.dp_frametx_error = 1;
			#endif

			if ((Int_vector[1] & 0x04) == 0x04) {
				sp_tx_aux_dpcdread_bytes(0x00, 0x05, 0x18,
								1, &c);
				if ((c & 0x40) == 0x40)
					pr_debug("%s %s : Downstream HDMI termination is detected!\n",
							LOG_TAG, __func__);
			}
			/*QC2.0*/
			if (Int_vector[1] & 0x08) {
				/*charger status changed*/
				pr_debug("%s,%s, chager status changed!!\n",
							LOG_TAG, __func__);
				/*Get the status of charger*/
				sp_tx_aux_dpcdread_bytes(0, 5,
					DPCD_CHARGER_TYPE_OFFSET, 1, &temp);
				/*charger plugged in*/
				pr_debug("charger_status_changed, status=%d\n",
									temp);
#ifdef QUICK_CHARGE_SUPPORT
				set_charger_status(temp);
#endif
			}
		} else if (sp_tx_rx_type != DWN_STRM_IS_HDMI) {
			/*sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x00, 1, &c); */
			c = __i2c_read_byte(TX_P0, DPCD_200);
			if (!(c & 0x3f)) {
				if (sp_tx_system_state >
						STATE_SINK_CONNECTION) {
					vbus_power_ctrl(0);
					reg_hardware_reset();
				} else {
					if (sp_tx_rx_type ==
							DWN_STRM_IS_VGA_9832)
						sp_tx_send_message(
							MSG_CLEAR_IRQ);
				}

				if (sp_tx_system_state >= STATE_LINK_TRAINING)
					link_down_check();
			}
		}
	}

	/* AUTOMATED TEST IRQ */
	if (IRQ_Vector & TEST_IRQ) {
		sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x18, 1, &test_vector);

		if (test_vector & 0x01) {/*test link training */
			sp_tx_test_lt = 1;

			sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x19, 1, &c);
			switch (c) {
			case 0x06:
			case 0x0A:
			case 0x14:
			case 0x19:
				sp_tx_set_link_bw(c);
				sp_tx_test_bw = c;
				break;
			default:
				sp_tx_set_link_bw(0x19);
				sp_tx_test_bw = 0x19;
				break;
			}

			pr_debug("%s %s :  test_bw = %.2x\n",
				LOG_TAG, __func__, (uint)sp_tx_test_bw);

			sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x60, 1, &c);
			c = c | TEST_ACK;
			sp_tx_aux_dpcdwrite_bytes(0x00, 0x02, 0x60, 1, &c);

			pr_debug("%s %s : Set TEST_ACK!\n", LOG_TAG, __func__);
			if (sp_tx_system_state >= STATE_LINK_TRAINING) {
				sp_tx_LT_state = LT_INIT;
				sp_tx_set_sys_state(STATE_LINK_TRAINING);
			}
			pr_debug("%s %s : IRQ:test-LT request!\n",
							LOG_TAG, __func__);
		}

		if (test_vector & 0x02) {
			/*pr_info("-------------------test pattern\n"); */
			sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x60, 1, &c);
			c = c | TEST_ACK;
			sp_tx_aux_dpcdwrite_bytes(0x00, 0x02, 0x60, 1, &c);
		}
		if (test_vector & 0x04) {/*test edid */
			if (sp_tx_system_state > STATE_PARSE_EDID)
				sp_tx_set_sys_state(STATE_PARSE_EDID);
			sp_tx_test_edid = 1;
			pr_debug("%s %s : Test EDID Requested!\n",
							LOG_TAG, __func__);
		}

		if (test_vector & 0x08) {/*phy test pattern */
			sp_tx_test_lt = 1;

			sp_tx_phy_auto_test();

			sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x60, 1, &c);
			c = c | 0x01;
			sp_tx_aux_dpcdwrite_bytes(0x00, 0x02, 0x60, 1, &c);
/*
			sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x60, 1, &c);
			while((c & 0x03) == 0){
				c = c | 0x01;
				sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x60,
									1, &c);
				sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x60,
									1, &c);
			}*/
		}
	}

}

static void sp_tx_vsi_setup(void)
{
	unchar c;
	int i;

	for (i = 0; i < 10; i++) {
		sp_read_reg(RX_P1, (HDMI_RX_MPEG_DATA00_REG + i), &c);
		sp_tx_packet_mpeg.MPEG_data[i] = c;
	}
}


static void sp_tx_mpeg_setup(void)
{
	unchar c;
	int i;

	for (i = 0; i < 10; i++) {
		sp_read_reg(RX_P1, (HDMI_RX_MPEG_DATA00_REG + i), &c);
		sp_tx_packet_mpeg.MPEG_data[i] = c;
	}
}

static void sp_tx_auth_done_int_handler(void)
{
	unchar bytebuf[2];

	if (HDCP_REPEATER_MODE) {
		if ((__i2c_read_byte(TX_P0, SP_TX_HDCP_STATUS) &
				SP_TX_HDCP_AUTH_PASS) &&
				(assisted_HDCP_repeater == HDCP_DOING))
			assisted_HDCP_repeater = HDCP_DONE;
		else
			assisted_HDCP_repeater = HDCP_ERROR;
		return;
	}
	if (HDCP_state > HDCP_HW_ENABLE
		&& sp_tx_system_state == STATE_HDCP_AUTH) {
		sp_read_reg(TX_P0, SP_TX_HDCP_STATUS, bytebuf);
		if (bytebuf[0] & SP_TX_HDCP_AUTH_PASS) {
			sp_tx_aux_dpcdread_bytes(0x06, 0x80, 0x2A, 2, bytebuf);
			/*max cascade exceeded or max devs exceed, disable
			encryption */
			if ((bytebuf[1] & 0x08) || (bytebuf[0] & 0x80)) {
				pr_warn("%s %s : max cascade/devs exceeded!\n",
							LOG_TAG, __func__);
				sp_tx_hdcp_encryption_disable();
			} else
				pr_debug("%s %s : Authentication pass in Auth_Done\n",
							LOG_TAG, __func__);

			HDCP_state = HDCP_FINISH;

		} else {
			pr_err("%s %s : Authentication failed in AUTH_done\n",
							LOG_TAG, __func__);
			sp_tx_video_mute(1);
			sp_tx_clean_hdcp_status();
			HDCP_state = HDCP_FAILE;
		}
	}
	pr_debug("%s %s : sp_tx_auth_done_int_handler\n", LOG_TAG, __func__);

}

#ifdef CO2_CABLE_DET_MODE
static void sp_tx_polling_err_int_handler(void)
{
	/*unchar temp;*/
#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
	slimport_set_hdmi_hpd(0);
#endif

	pr_debug("%s %s : sp_tx_polling_err_int_handler\n", LOG_TAG, __func__);
	/*if(AUX_ERR== sp_tx_aux_dpcdread_bytes(0x00, 0x00, 0x00, 1, &temp)) */
		system_power_ctrl(0);

}
#endif

static void sp_tx_lt_done_int_handler(void)
{
	unchar c;

	if (sp_tx_LT_state == LT_WAITTING_FINISH
		&& sp_tx_system_state == STATE_LINK_TRAINING) {
		sp_read_reg(TX_P0, LT_CTRL, &c);
		if (c & 0x70) {
			c = (c & 0x70) >> 4;
			pr_err("%s %s : LT failed in interrupt, ERR code = %.2x\n",
						LOG_TAG, __func__, (uint) c);
			sp_tx_LT_state = LT_ERROR;
		} else {
			pr_debug("%s %s : lt_done: LT Finish\n",
							LOG_TAG, __func__);
			sp_tx_LT_state = LT_FINISH;
		}
	}
}

static void sp_tx_link_change_int_handler(void)
{
	pr_debug("%s %s : sp_tx_link_change_int_handler\n", LOG_TAG, __func__);
	if (sp_tx_system_state < STATE_LINK_TRAINING)
		return;
	sp_tx_link_err_check();
	link_down_check();
}

static void hdmi_rx_clk_det_int(void)
{
	pr_debug("%s %s : *HDMI_RX Interrupt: Pixel Clock Change.\n",
							LOG_TAG, __func__);
	if (sp_tx_system_state > STATE_VIDEO_OUTPUT) {
		sp_tx_video_mute(1);
		sp_tx_enable_audio_output(0);
		sp_tx_set_sys_state(STATE_VIDEO_OUTPUT);
	}
}

static void hdmi_rx_sync_det_int(void)
{
	pr_debug("%s %s : *HDMI_RX Interrupt: Sync Detect.\n",
						LOG_TAG, __func__);
}

static void hdmi_rx_hdmi_dvi_int(void)
{
	unchar c;

	pr_debug("%s %s : hdmi_rx_hdmi_dvi_int.\n", LOG_TAG, __func__);
	sp_read_reg(RX_P0, HDMI_STATUS, &c);
	g_hdmi_dvi_status = read_dvi_hdmi_mode();
	if ((c & _BIT0) != (g_hdmi_dvi_status & _BIT0)) {
		pr_debug("%s %s : hdmi_dvi_int: Is HDMI MODE: %x.\n",
				LOG_TAG, __func__, (uint)(c & HDMI_MODE));
		g_hdmi_dvi_status = (c & _BIT0);
		hdmi_rx_mute_audio(1);
		system_state_change_with_case(STATE_LINK_TRAINING);
	}
}

/*
static void hdmi_rx_avmute_int(void)
{
	unchar avmute_status, c;

	sp_read_reg(RX_P0, HDMI_STATUS,
		       &avmute_status);
	if (avmute_status & MUTE_STAT) {
		pr_info("%s %s : HDMI_RX AV mute packet received.\n",
						LOG_TAG, __func__);
		hdmi_rx_mute_video(1);
		hdmi_rx_mute_audio(1);
		c = avmute_status & (~MUTE_STAT);
		sp_write_reg(RX_P0, HDMI_STATUS, c);
	}
}
*/

static void hdmi_rx_new_avi_int(void)
{
	pr_debug("%s %s : *HDMI_RX Interrupt: New AVI Packet.\n",
							LOG_TAG, __func__);
	sp_tx_lvttl_bit_mapping();
	sp_tx_set_colorspace();
	sp_tx_avi_setup();
	sp_tx_config_packets(AVI_PACKETS);
	mdelay(50);
	sp_tx_set_colordepth();
}

static void hdmi_rx_new_vsi_int(void)
{
	/*unchar c;*/
	unchar hdmi_video_format, v3d_structure;

	pr_debug("%s %s : *HDMI_RX Interrupt: NEW VSI packet.\n",
							LOG_TAG, __func__);

	sp_write_reg_and(TX_P0, SP_TX_3D_VSC_CTRL, ~INFO_FRAME_VSC_EN);
	/*VSI package header*/
	if ((__i2c_read_byte(RX_P1, HDMI_RX_MPEG_TYPE_REG) != 0x81)
		|| (__i2c_read_byte(RX_P1, HDMI_RX_MPEG_VER_REG) != 0x01))
		return;

	pr_debug("%s %s : Setup VSI package!\n", LOG_TAG, __func__);

	sp_tx_vsi_setup(); /*use mpeg packet as mail box to send vsi packet */
	sp_tx_config_packets(VSI_PACKETS);

	sp_read_reg(RX_P1, HDMI_RX_MPEG_DATA03_REG, &hdmi_video_format);
	if ((hdmi_video_format & 0xe0) == 0x40) {
		pr_debug("%s %s : 3D VSI packet is detected. Config VSC packet\n",
							LOG_TAG, __func__);
		sp_read_reg(RX_P1, HDMI_RX_MPEG_DATA05_REG, &v3d_structure);
		switch (v3d_structure & 0xf0) {
		case 0x00:/*frame packing */
			v3d_structure = 0x02;
			break;
		case 0x20:/*Line alternative */
			v3d_structure = 0x03;
			break;
		case 0x30:/*Side-by-side(full)*/
			v3d_structure = 0x04;
			break;
		default:
			v3d_structure = 0x00;
			pr_err("%s %s : 3D structure is not supported\n",
							LOG_TAG, __func__);
			break;
		}
	sp_write_reg(TX_P0, SP_TX_VSC_DB1, v3d_structure);
	}
	sp_write_reg_or(TX_P0, SP_TX_3D_VSC_CTRL, INFO_FRAME_VSC_EN);
	sp_write_reg_and(TX_P0, SP_TX_PKT_EN_REG, ~SPD_IF_EN);
	sp_write_reg_or(TX_P0, SP_TX_PKT_EN_REG, SPD_IF_UD);
	sp_write_reg_or(TX_P0, SP_TX_PKT_EN_REG, SPD_IF_EN);
}


static void hdmi_rx_no_vsi_int(void)
{
	unchar c;

	sp_read_reg(TX_P0, SP_TX_3D_VSC_CTRL, &c);
	if (c & INFO_FRAME_VSC_EN) {
		pr_debug("%s %s : No new VSI is received, disable VSC packet\n",
							LOG_TAG, __func__);
		c &= ~INFO_FRAME_VSC_EN;
		sp_write_reg(TX_P0, SP_TX_3D_VSC_CTRL, c);
		sp_tx_mpeg_setup();
		sp_tx_config_packets(MPEG_PACKETS);
	}

}

static void hdmi_rx_restart_audio_chk(void)
{
	pr_debug("%s %s : WAIT_AUDIO: hdmi_rx_restart_audio_chk.\n",
							LOG_TAG, __func__);
	system_state_change_with_case(STATE_AUDIO_OUTPUT);
}

static void hdmi_rx_cts_rcv_int(void)
{
	if (sp_tx_ao_state == AO_INIT)
		sp_tx_ao_state = AO_CTS_RCV_INT;
	else if (sp_tx_ao_state == AO_AUDIO_RCV_INT)
		sp_tx_ao_state = AO_RCV_INT_FINISH;

	/*pr_info("%s %s : *hdmi_rx_cts_rcv_int.\n", LOG_TAG, __func__);*/
}

static void hdmi_rx_audio_rcv_int(void)
{
	if (sp_tx_ao_state == AO_INIT)
		sp_tx_ao_state = AO_AUDIO_RCV_INT;
	else if (sp_tx_ao_state == AO_CTS_RCV_INT)
		sp_tx_ao_state = AO_RCV_INT_FINISH;
	/*pr_info("%s %s : *hdmi_rx_audio_rcv_int\n", LOG_TAG, __func__); */
}

static void hdmi_rx_audio_samplechg_int(void)
{
	uint i;
	unchar c;

	/* transfer audio chaneel status from HDMI Rx to Slinmport Tx */
	for (i = 0; i < 5; i++) {
		sp_read_reg(RX_P0, (HDMI_RX_AUD_IN_CH_STATUS1_REG + i), &c);
		sp_write_reg(TX_P2, (SP_TX_AUD_CH_STATUS_REG1 + i), c);
	}
	/* pr_info("%s %s : *hdmi_rx_audio_samplechg_int\n",
						LOG_TAG, __func__); */
}

static void hdmi_rx_hdcp_error_int(void)
{
	static unchar count;

	pr_err("%s %s : *HDMI_RX Interrupt: hdcp error.\n", LOG_TAG, __func__);
	if (count >= 40) {
		count = 0;
		pr_err("%s %s : Lots of hdcp error occurred ...\n",
							LOG_TAG, __func__);
		hdmi_rx_mute_audio(1);
		hdmi_rx_mute_video(1);
		hdmi_rx_set_hpd(0);
		usleep_range(10000, 10000);
		hdmi_rx_set_hpd(1);
	} else
		count++;
}

static void hdmi_rx_new_gcp_int(void)
{
	unchar c;

	/*pr_info("%s %s : *HDMI_RX Interrupt: New GCP Packet.\n",
						LOG_TAG, __func__); */
	sp_read_reg(RX_P1, HDMI_RX_GENERAL_CTRL, &c);
	if (c & SET_AVMUTE) {
			hdmi_rx_mute_video(1);
			hdmi_rx_mute_audio(1);

	} else if (c & CLEAR_AVMUTE) {
		/*if ((g_video_muted) &&
			(hdmi_system_state >HDMI_VIDEO_CONFIG)) */
			hdmi_rx_mute_video(0);
		/*if ((g_audio_muted) &&
			(hdmi_system_state >HDMI_AUDIO_CONFIG)) */
			hdmi_rx_mute_audio(0);
	}
}

void system_isr_handler(void)
{
	/*if (sp_tx_system_state < STATE_PLAY_BACK)
		pr_info("%s %s : =========system_isr==========\n",
						LOG_TAG, __func__); */

	if (COMMON_INT1 & PLL_LOCK_CHG)
		sp_tx_pll_changed_int_handler();

	if (COMMON_INT2 & HDCP_AUTH_DONE)
		sp_tx_auth_done_int_handler();

	if (COMMON_INT3 & HDCP_LINK_CHECK_FAIL)
		sp_tx_hdcp_link_chk_fail_handler();

	if (COMMON_INT5 & DPCD_IRQ_REQUEST)
		sp_tx_sink_irq_int_handler();

	#ifdef CO2_CABLE_DET_MODE
	if (sp_tx_system_state > STATE_SP_INITIALIZED) {
		if (COMMON_INT5 & POLLING_ERR)
			sp_tx_polling_err_int_handler();
	}
	#endif

	if (COMMON_INT5 & TRAINING_Finish)
		sp_tx_lt_done_int_handler();

	if (COMMON_INT5 & LINK_CHANGE)
		sp_tx_link_change_int_handler();

	if (sp_tx_system_state > STATE_SINK_CONNECTION) {
		if (HDMI_RX_INT6 & NEW_AVI)
			hdmi_rx_new_avi_int();

		#ifdef CEC_ENABLE
		if (HDMI_RX_INT7 & CEC_RX_READY) {
			if (TASK_OK == hdmi_cec_recv_process())
				cec.hdmi_cec_receive_done = 1;
			pr_debug("%s %s : HDMI -> DP receive CEC data done!\n",
							LOG_TAG, __func__);
		}

		if (HDMI_RX_INT7 & CEC_TX_DONE) {
			cec.hdmi_cec_transmit_done = 1;
			pr_debug("%s %s : DP -> HDMI CEC data transmit done!\n",
							LOG_TAG, __func__);
		}
	#endif
	}

	if (sp_tx_system_state > STATE_VIDEO_OUTPUT) {

		if (HDMI_RX_INT7 & NEW_VS) {
			HDMI_RX_INT7 &= ~NO_VSI; /*clear no_vsi interrupt flag*/
			hdmi_rx_new_vsi_int();
		}
		if (HDMI_RX_INT7 & NO_VSI)
			hdmi_rx_no_vsi_int();
	}

	if (sp_tx_system_state >= STATE_VIDEO_OUTPUT) {
		if (HDMI_RX_INT1 & CKDT_CHANGE)
			hdmi_rx_clk_det_int();

		if (HDMI_RX_INT1 & SCDT_CHANGE)
			hdmi_rx_sync_det_int();

		if (HDMI_RX_INT1 & HDMI_DVI)
			hdmi_rx_hdmi_dvi_int();
		/*
		if (HDMI_RX_INT1 & SET_MUTE)
			hdmi_rx_avmute_int();
		*/

		if ((HDMI_RX_INT6 & NEW_AUD) ||
					(HDMI_RX_INT3 & AUD_MODE_CHANGE))
			hdmi_rx_restart_audio_chk();

		if (HDMI_RX_INT6 & CTS_RCV)
			hdmi_rx_cts_rcv_int();

		if (HDMI_RX_INT5 & AUDIO_RCV)
			hdmi_rx_audio_rcv_int();


		if (HDMI_RX_INT2 & HDCP_ERR)
			hdmi_rx_hdcp_error_int();

		if (HDMI_RX_INT6 & NEW_CP)
			hdmi_rx_new_gcp_int();

		if (HDMI_RX_INT2 & AUDIO_SAMPLE_CHANGE)
			hdmi_rx_audio_samplechg_int();
	}
}

void sp_tx_show_infomation(void)
{
	unchar c, c1;
	uint h_res, h_act, v_res, v_act;
	uint h_fp, h_sw, h_bp, v_fp, v_sw, v_bp;
	ulong fresh_rate;
	ulong pclk;

	pr_debug("\n******************SP Video Information*****************\n");


	sp_read_reg(TX_P0, SP_TX_LINK_BW_SET_REG, &c);
	switch (c) {
	case 0x06:
		pr_debug("%s %s : BW = 1.62G\n", LOG_TAG, __func__);
		break;
	case 0x0a:
		pr_debug("%s %s : BW = 2.7G\n", LOG_TAG, __func__);
		break;
	case 0x14:
		pr_debug("%s %s : BW = 5.4G\n", LOG_TAG, __func__);
		break;
	case 0x19:
		pr_debug("%s %s : BW = 6.75G\n", LOG_TAG, __func__);
		break;
	default:
		break;
	}

	pclk = sp_tx_pclk_calc();
	pclk = pclk / 10;
	/*pr_info("%s %s : SSC On\n", LOG_TAG, __func__); */


	sp_read_reg(TX_P2, SP_TX_TOTAL_LINE_STA_L, &c);
	sp_read_reg(TX_P2, SP_TX_TOTAL_LINE_STA_H, &c1);

	v_res = c1;
	v_res = v_res << 8;
	v_res = v_res + c;


	sp_read_reg(TX_P2, SP_TX_ACT_LINE_STA_L, &c);
	sp_read_reg(TX_P2, SP_TX_ACT_LINE_STA_H, &c1);

	v_act = c1;
	v_act = v_act << 8;
	v_act = v_act + c;

	sp_read_reg(TX_P2, SP_TX_TOTAL_PIXEL_STA_L, &c);
	sp_read_reg(TX_P2, SP_TX_TOTAL_PIXEL_STA_H, &c1);

	h_res = c1;
	h_res = h_res << 8;
	h_res = h_res + c;

	sp_read_reg(TX_P2, SP_TX_ACT_PIXEL_STA_L, &c);
	sp_read_reg(TX_P2, SP_TX_ACT_PIXEL_STA_H, &c1);

	h_act = c1;
	h_act = h_act << 8;
	h_act = h_act + c;

	sp_read_reg(TX_P2, SP_TX_H_F_PORCH_STA_L, &c);
	sp_read_reg(TX_P2, SP_TX_H_F_PORCH_STA_H, &c1);

	h_fp = c1;
	h_fp = h_fp << 8;
	h_fp = h_fp + c;

	sp_read_reg(TX_P2, SP_TX_H_SYNC_STA_L, &c);
	sp_read_reg(TX_P2, SP_TX_H_SYNC_STA_H, &c1);

	h_sw = c1;
	h_sw = h_sw << 8;
	h_sw = h_sw + c;

	sp_read_reg(TX_P2, SP_TX_H_B_PORCH_STA_L, &c);
	sp_read_reg(TX_P2, SP_TX_H_B_PORCH_STA_H, &c1);

	h_bp = c1;
	h_bp = h_bp << 8;
	h_bp = h_bp + c;

	sp_read_reg(TX_P2, SP_TX_V_F_PORCH_STA, &c);
	v_fp = c;

	sp_read_reg(TX_P2, SP_TX_V_SYNC_STA, &c);
	v_sw = c;

	sp_read_reg(TX_P2, SP_TX_V_B_PORCH_STA, &c);
	v_bp = c;

	pr_debug("%s %s : Total resolution is %d * %d\n",
					LOG_TAG, __func__, h_res, v_res);

	pr_debug("%s %s : HF=%d, HSW=%d, HBP=%d\n",
					LOG_TAG, __func__, h_fp, h_sw, h_bp);
	pr_debug("%s %s : VF=%d, VSW=%d, VBP=%d\n",
					LOG_TAG, __func__, v_fp, v_sw, v_bp);
	pr_debug("%s %s : Active resolution is %d * %d\n",
					LOG_TAG, __func__, h_act, v_act);

	if (h_res == 0 || v_res == 0)
		fresh_rate = 0;
	else {
		fresh_rate = pclk * 1000;
		fresh_rate = fresh_rate / h_res;
		fresh_rate = fresh_rate * 1000;
		fresh_rate = fresh_rate / v_res;
	}
	pr_debug("   @ %ldHz\n", fresh_rate);

	sp_read_reg(TX_P0, SP_TX_VID_CTRL, &c);

	if ((c & 0x06) == 0x00)
		pr_debug("%s %s : ColorSpace: RGB,", LOG_TAG, __func__);
	else if ((c & 0x06) == 0x02)
		pr_debug("%s %s : ColorSpace: YCbCr422,", LOG_TAG, __func__);
	else if ((c & 0x06) == 0x04)
		pr_debug("%s %s : ColorSpace: YCbCr444,", LOG_TAG, __func__);

	sp_read_reg(TX_P0, SP_TX_VID_CTRL, &c);

	if ((c & 0xe0) == 0x00)
		pr_debug("6 BPC\n");
	else if ((c & 0xe0) == 0x20)
		pr_debug("8 BPC\n");
	else if ((c & 0xe0) == 0x40)
		pr_debug("10 BPC\n");
	else if ((c & 0xe0) == 0x60)
		pr_debug("12 BPC\n");


	if (sp_tx_rx_type == DWN_STRM_IS_HDMI) {/*assuming it is anx7730 */
		sp_tx_aux_dpcdread_bytes(0x00, 0x05, 0x23, 1, &c);
		pr_debug("%s %s : HDMI Dongle FW Ver %.2x\n",
					LOG_TAG, __func__, (uint)(c & 0x7f));
	}

	pr_debug("\n**************************************************\n");
}

void hdmi_rx_show_video_info(void)
{
	unchar c, c1;
	unchar cl, ch;
	uint n;
	uint h_res, v_res;

	sp_read_reg(RX_P0, HDMI_RX_HACT_LOW, &cl);
	sp_read_reg(RX_P0, HDMI_RX_HACT_HIGH, &ch);
	n = ch;
	n = (n << 8) + cl;
	h_res = n;

	sp_read_reg(RX_P0, HDMI_RX_VACT_LOW, &cl);
	sp_read_reg(RX_P0, HDMI_RX_VACT_HIGH, &ch);
	n = ch;
	n = (n << 8) + cl;
	v_res = n;

	pr_debug("%s %s : >HDMI_RX Info<\n", LOG_TAG, __func__);
	sp_read_reg(RX_P0, HDMI_STATUS, &c);
	if (c & HDMI_MODE)
		pr_debug("%s %s : HDMI_RX Mode = HDMI Mode.\n",
							LOG_TAG, __func__);
	else
		pr_debug("%s %s : HDMI_RX Mode = DVI Mode.\n",
							LOG_TAG, __func__);

	sp_read_reg(RX_P0, HDMI_RX_VIDEO_STATUS_REG1, &c);

	if (c & VIDEO_TYPE)
		v_res += v_res;

	pr_debug("%s %s : HDMI_RX Video Resolution = %d * %d ",
					LOG_TAG, __func__, h_res, v_res);
	sp_read_reg(RX_P0, HDMI_RX_VIDEO_STATUS_REG1, &c);
	if (c & VIDEO_TYPE)
		pr_debug("Interlace Video.\n");
	else
		pr_debug("Progressive Video.\n");

	sp_read_reg(RX_P0, HDMI_RX_SYS_CTRL1_REG, &c);
	if ((c & 0x30) == 0x00)
		pr_debug("%s %s : Input Pixel Clock = Not Repeated.\n",
							LOG_TAG, __func__);
	else if ((c & 0x30) == 0x10)
		pr_debug("%s %s : Input Pixel Clock = 2x Video Clock. Repeated.\n",
							LOG_TAG, __func__);
	else if ((c & 0x30) == 0x30)
		pr_debug("%s %s : Input Pixel Clock = 4x Vvideo Clock. Repeated.\n",
							LOG_TAG, __func__);

	if ((c & 0xc0) == 0x00)
		pr_debug("%s %s : Output Video Clock = Not Divided.\n",
							LOG_TAG, __func__);
	else if ((c & 0xc0) == 0x40)
		pr_debug("%s %s : Output Video Clock = Divided By 2.\n",
							LOG_TAG, __func__);
	else if ((c & 0xc0) == 0xc0)
		pr_debug("%s %s : Output Video Clock = Divided By 4.\n",
							LOG_TAG, __func__);

	if (c & 0x02)
		pr_debug("%s %s : Output Video Using Rising Edge To Latch Data.\n",
							LOG_TAG, __func__);
	else
		pr_debug("%s %s : Output Video Using Falling Edge To Latch Data.\n",
							LOG_TAG, __func__);

	pr_debug("Input Video Color Depth = ");
	sp_read_reg(RX_P0, 0x70, &c1);
	c1 &= 0xf0;
	if (c1 == 0x00)
		pr_debug("%s %s : Legacy Mode.\n", LOG_TAG, __func__);
	else if (c1 == 0x40)
		pr_debug("%s %s : 24 Bit Mode.\n", LOG_TAG, __func__);
	else if (c1 == 0x50)
		pr_debug("%s %s : 30 Bit Mode.\n", LOG_TAG, __func__);
	else if (c1 == 0x60)
		pr_debug("%s %s : 36 Bit Mode.\n", LOG_TAG, __func__);
	else if (c1 == 0x70)
		pr_debug("%s %s : 48 Bit Mode.\n", LOG_TAG, __func__);

	pr_debug("%s %s : Input Video Color Space = ", LOG_TAG, __func__);
	sp_read_reg(RX_P1, HDMI_RX_AVI_DATA00_REG, &c);
	c &= 0x60;
	if (c == 0x20)
		pr_debug("YCbCr4:2:2 .\n");
	else if (c == 0x40)
		pr_debug("YCbCr4:4:4 .\n");
	else if (c == 0x00)
		pr_debug("RGB.\n");
	else
		pr_err("Unknown 0x44 = 0x%.2x\n", (int)c);

	sp_read_reg(RX_P1, HDMI_RX_HDCP_STATUS_REG, &c);
	if (c & AUTH_EN)
		pr_debug("%s %s : Authentication is attempted.\n",
							LOG_TAG, __func__);
	else
		pr_debug("%s %s : Authentication is not attempted.\n",
							LOG_TAG, __func__);

	for (cl = 0; cl < 20; cl++) {
		sp_read_reg(RX_P1, HDMI_RX_HDCP_STATUS_REG, &c);
		if (c & DECRYPT_EN)
			break;
		else
			usleep_range(10000, 10000);
	}
	if (cl < 20)
		pr_debug("%s %s : Decryption is active.\n", LOG_TAG, __func__);
	else
		pr_debug("%s %s : Decryption is not active.\n",
							LOG_TAG, __func__);
}

void clean_system_status(void)
{
	if (g_need_clean_status) {
		pr_debug("%s %s : clean_system_status. A -> B;\n",
						LOG_TAG, __func__);
		pr_debug("A:");
		print_sys_state(sp_tx_system_state_bak);
		pr_debug("B:");
		print_sys_state(sp_tx_system_state);

		g_need_clean_status = 0;
		if (sp_tx_system_state_bak >= STATE_LINK_TRAINING) {
			if (sp_tx_system_state >= STATE_AUDIO_OUTPUT)
				hdmi_rx_mute_audio(1);
			else {
				hdmi_rx_mute_video(1);
				sp_tx_video_mute(1);
			}
			/*sp_tx_enable_video_input(0); */
			/*sp_tx_enable_audio_output(0); */
			/*slimport_block_power_ctrl(SP_TX_PWR_VIDEO,
							SP_POWER_DOWN); */
			/*slimport_block_power_ctrl(SP_TX_PWR_AUDIO,
							SP_POWER_DOWN); */
		}
		if (!HDCP_REPEATER_MODE) {
			if (sp_tx_system_state_bak >= STATE_HDCP_AUTH
				&& sp_tx_system_state <= STATE_HDCP_AUTH) {
				if (__i2c_read_byte(TX_P0, TX_HDCP_CTRL0) &
									0xFC)
					sp_tx_clean_hdcp_status();
			}
		} else {
			if (sp_tx_system_state_bak > STATE_LINK_TRAINING
				&& sp_tx_system_state <= STATE_LINK_TRAINING) {
				/*Inform AP to re-auth*/
					hdmi_rx_set_hpd(0);
					pr_debug("%s %s : hdmi_rx_set_hpd 0!\n",
							LOG_TAG, __func__);
					hdmi_rx_set_termination(0);
					msleep(50);
			}
		}
		if (HDCP_state != HDCP_CAPABLE_CHECK)
			HDCP_state = HDCP_CAPABLE_CHECK;

		if (sp_tx_sc_state != SC_INIT)
			sp_tx_sc_state = SC_INIT;
		if (sp_tx_LT_state != LT_INIT)
			sp_tx_LT_state = LT_INIT;
		if (sp_tx_vo_state != VO_WAIT_VIDEO_STABLE)
			sp_tx_vo_state = VO_WAIT_VIDEO_STABLE;
	}
}
/******************add for HDCP cap check********************/
void sp_tx_get_ddc_hdcp_BCAP(unchar *bcap)
{
	unchar c;

	sp_write_reg(TX_P0, AUX_ADDR_7_0, 0X3A);/*0X74 FOR HDCP DDC */
	sp_write_reg(TX_P0, AUX_ADDR_15_8, 0);
	sp_read_reg(TX_P0, AUX_ADDR_19_16, &c);
	c &= 0xf0;
	sp_write_reg(TX_P0, AUX_ADDR_19_16, c);

	sp_tx_aux_wr(0x00);
	sp_tx_aux_rd(0x01);
	sp_read_reg(TX_P0, BUF_DATA_0, &c);

	sp_tx_aux_wr(0x40);/*0X40: bcap */
	sp_tx_aux_rd(0x01);
	sp_read_reg(TX_P0, BUF_DATA_0, bcap);
	/*pr_info("%s %s :HDCP BCAP = %.2x\n",
			LOG_TAG, __func__, (uint)*bcap); */
}

void sp_tx_get_ddc_hdcp_BKSV(unchar *bksv)
{
	unchar c;

	sp_write_reg(TX_P0, AUX_ADDR_7_0, 0X3A);/*0X74 FOR HDCP DDC*/
	sp_write_reg(TX_P0, AUX_ADDR_15_8, 0);
	sp_read_reg(TX_P0, AUX_ADDR_19_16, &c);
	c &= 0xf0;
	sp_write_reg(TX_P0, AUX_ADDR_19_16, c);

	sp_tx_aux_wr(0x00);
	sp_tx_aux_rd(0x01);
	sp_read_reg(TX_P0, BUF_DATA_0, &c);


	sp_tx_aux_wr(0x00);/*0X00: bksv*/
	sp_tx_aux_rd(0x01);
	sp_read_reg(TX_P0, BUF_DATA_0, bksv);
	/*pr_info("%s %s :HDCP BKSV0 = %.2x\n",
				LOG_TAG, __func__, (uint)*bksv); */
}


unchar slimport_hdcp_cap_check(void)
{
	unchar g_hdcp_cap = 0;
	unchar c, DDC_HDCP_BCAP, DDC_HDCP_BKSV;

	if ((sp_tx_rx_type == DWN_STRM_IS_VGA_9832)
		|| (sp_tx_rx_type == DWN_STRM_IS_ANALOG)) {
		g_hdcp_cap = 0;	/*  not capable HDCP */
	} else if (sp_tx_rx_type == DWN_STRM_IS_DIGITAL) {
		if (AUX_OK == sp_tx_aux_dpcdread_bytes(0x06, 0x80, 0x28,
								1, &c)) {
			if (!(c & 0x01)) {
				pr_warn("%s %s : Sink is not capable HDCP\n",
						LOG_TAG, __func__);
				g_hdcp_cap = 0; /*not capable HDCP*/
			} else
				g_hdcp_cap = 1;  /*capable HDCP*/
		} else
			pr_err("%s %s : HDCP CAPABLE: read AUX err!\n",
							LOG_TAG, __func__);
	} else if (sp_tx_rx_type == DWN_STRM_IS_HDMI) {
			sp_tx_get_ddc_hdcp_BCAP(&DDC_HDCP_BCAP);
			sp_tx_get_ddc_hdcp_BKSV(&DDC_HDCP_BKSV);
			if ((DDC_HDCP_BCAP == 0x80) || (DDC_HDCP_BCAP == 0x82)
			|| (DDC_HDCP_BCAP == 0x83) || (DDC_HDCP_BCAP == 0xc0) ||
			(DDC_HDCP_BCAP == 0xe0) || (DDC_HDCP_BKSV != 0x00))
				g_hdcp_cap = 1;
			else
				g_hdcp_cap = 0;
	} else
		g_hdcp_cap = 1;

	return g_hdcp_cap;
}
/******************End HDCP cap check********************/
#ifdef SIMULATE_WATCHDOG
void simulate_watchdog_func(void)
{
	static unchar bak_states;

	if ((sp_tx_cur_states() < STATE_AUDIO_OUTPUT)
		&& (sp_tx_cur_states() == sp_tx_system_state_bak)) {
		bak_states++;
		/*100ms about one main loop, then 100*200 = 20s for watchdog*/
		if (bak_states > 200) {
			bak_states = 0;
			system_power_ctrl(0);
			pr_err("%s %s :SlimPort Watchdog start!\n",
							LOG_TAG, __func__);
		}
	} else {
		if (bak_states != 0)
			bak_states = 0;
	}
}
#endif
void slimport_hdcp_repeater_reauth(void) /* A patch for HDCP repeater mode*/
{
	unchar c, hdcp_ctrl, hdcp_staus;

	if (HDCP_REPEATER_MODE
		&& (sp_tx_system_state > STATE_HDCP_AUTH)
		&& (sp_tx_rx_type != DWN_STRM_IS_ANALOG)
		&& (sp_tx_rx_type != DWN_STRM_IS_VGA_9832)) {
		msleep(50);

		sp_read_reg(RX_P1, HDMI_RX_HDCP_STATUS_REG, &c);
		if ((c & AUTH_EN) != 0) {
			/*debug_puts("Upstream HDMI HDCP request!\n"); */
			sp_read_reg(TX_P0, TX_HDCP_CTRL0, &hdcp_ctrl);
			if ((hdcp_ctrl & HARD_AUTH_EN) == 0) {
				pr_debug("Repeater Mode: Enable HW HDCP\n");
				assisted_HDCP_repeater = HDCP_ERROR;
			} else {
				sp_read_reg(TX_P0, SP_TX_HDCP_STATUS,
								&hdcp_staus);
				if (((hdcp_staus & SP_TX_HDCP_AUTH_PASS) == 0)
					& ((hdcp_staus & SP_TX_HDCP_AUTH_FAIL)
								== 1)) {
					assisted_HDCP_repeater = HDCP_ERROR;
					pr_err("Repeater Mode: Clean HDCP and re-auth HDCP\n");
				} else if ((hdcp_staus & SP_TX_HDCP_AUTH_PASS)
									== 1) {
					pr_err("Repeater Mode: $$$$$HDCP Pass$$$$$\n");
				}
			}
		}

		sp_read_reg(TX_P0, TX_HDCP_CTRL0, &hdcp_ctrl);
		sp_read_reg(TX_P0, SP_TX_HDCP_STATUS, &hdcp_staus);
		if ((hdcp_ctrl == TX_HDCP_FUNCTION_ENABLE)
			&& ((hdcp_staus & SP_TX_HDCP_AUTH_FAIL))) {
			pr_err("main link error: HDCP encryption failure %x\n",
							(int)hdcp_staus);
			assisted_HDCP_repeater = HDCP_ERROR;
			/*sp_tx_set_sys_state(STATE_LINK_TRAINING); */
		}

		if (assisted_HDCP_repeater == HDCP_ERROR) {
			sp_tx_clean_hdcp_status();
			msleep(50);
			/*Clear HDCP AUTH interrupt*/
			sp_write_reg_or(TX_P2, SP_COMMON_INT_STATUS1 + 1,
							HDCP_AUTH_DONE);
			sp_tx_hw_hdcp_enable();
			assisted_HDCP_repeater = HDCP_DOING;
		}
	}
}
void slimport_tasks_handler(void)
{
	/*isr*/
	if (sp_tx_system_state > STATE_WAITTING_CABLE_PLUG)
		system_isr_handler();
	#ifdef CEC_ENABLE
	if (sp_tx_system_state > STATE_SINK_CONNECTION)
		CEC_handler();
	#endif
	if (HDCP_REPEATER_MODE)
		slimport_hdcp_repeater_reauth();

	hdcp_external_ctrl_flag_monitor();
	clean_system_status();
	#ifdef SIMULATE_WATCHDOG
	simulate_watchdog_func();
	#endif
	slimport_cable_monitor();
	/*clear up backup system state*/
	if (sp_tx_system_state_bak != sp_tx_system_state)
		sp_tx_system_state_bak = sp_tx_system_state;
}
/******************End task  process********************/

void slimport_main_process(void)
{
	slimport_state_process();
	slimport_int_rec();
	slimport_tasks_handler();
}



/* ***************************************************************** */
/* Functions for slimport_rx anx7730 */
/* ***************************************************************** */

#define SOURCE_AUX_OK 1
#define SOURCE_AUX_ERR 0
#define SOURCE_REG_OK 1
#define SOURCE_REG_ERR 0

#define SINK_DEV_SEL  0x005f0
#define SINK_ACC_OFS  0x005f1
#define SINK_ACC_REG  0x005f2

bool source_aux_read_7730dpcd(long addr, unchar cCount, unchar *pBuf)
{
	unchar c;
	unchar addr_l;
	unchar addr_m;
	unchar addr_h;

	addr_l = (unchar)addr;
	addr_m = (unchar)(addr>>8);
	addr_h = (unchar)(addr>>16);
	c = 0;
	while (1) {
		if (sp_tx_aux_dpcdread_bytes(addr_h, addr_m, addr_l,
						cCount, pBuf) == AUX_OK)
			return SOURCE_AUX_OK;
		c++;
		if (c > 3)
			return SOURCE_AUX_ERR;
	}
}

bool source_aux_write_7730dpcd(long addr, unchar cCount, unchar *pBuf)
{
	unchar c;
	unchar addr_l;
	unchar addr_m;
	unchar addr_h;

	addr_l = (unchar)addr;
	addr_m = (unchar)(addr>>8);
	addr_h = (unchar)(addr>>16);
	c = 0;
	while (1) {
		if (sp_tx_aux_dpcdwrite_bytes(addr_h, addr_m, addr_l,
						cCount, pBuf) == AUX_OK)
			return SOURCE_AUX_OK;
		c++;
		if (c > 3)
			return SOURCE_AUX_ERR;
	}
}

bool i2c_master_read_reg(unchar Sink_device_sel, unchar offset, unchar *Buf)
{
	unchar sbytebuf[2] = {0};
	long a0, a1;

	a0 = SINK_DEV_SEL;
	a1 = SINK_ACC_REG;
	sbytebuf[0] = Sink_device_sel;
	sbytebuf[1] = offset;

	if (source_aux_write_7730dpcd(a0, 2, sbytebuf) == SOURCE_AUX_OK) {
		if (source_aux_read_7730dpcd(a1, 1, Buf) == SOURCE_AUX_OK)
			return SOURCE_REG_OK;
	}
	return SOURCE_REG_ERR;
}

bool i2c_master_write_reg(unchar Sink_device_sel, unchar offset, unchar value)
{
	unchar sbytebuf[3] = {0};
	long a0;

	a0 = SINK_DEV_SEL;
	sbytebuf[0] = Sink_device_sel;
	sbytebuf[1] = offset;
	sbytebuf[2] = value;

	if (source_aux_write_7730dpcd(a0, 3, sbytebuf) == SOURCE_AUX_OK)
		return SOURCE_REG_OK;
	else
		return SOURCE_REG_ERR;
}


#ifdef ANX_LINUX_ENV
MODULE_DESCRIPTION("Slimport transmitter ANX7816 driver");
MODULE_AUTHOR("<swang@analogixsemi.com>");
MODULE_LICENSE("GPL");
#endif

