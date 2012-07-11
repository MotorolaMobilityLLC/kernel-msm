/*
 * Copyright(c) 2012, Analogix Semiconductor. All rights reserved.
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

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include "slimport_tx_drv.h"
#include "slimport_tx_reg.h"
#include "slimport.h"

static unchar ByteBuf[MAX_BUF_CNT];

/* EDID access break */
unchar bEDID_break;
unchar bEDID_extblock[128];
unchar bEDID_firstblock[128];


static ulong pclk;
static ulong M_val, N_val;
enum SP_LINK_BW sp_tx_bw;
unchar sp_tx_link_config_done;
unchar sp_tx_hw_lt_done;
unchar sp_tx_hw_lt_enable;

/* for HDCP */
static unchar sp_tx_hdcp_auth_pass;
static unchar sp_tx_hdcp_auth_fail_counter;
static unchar sp_tx_hdcp_capable_chk;
static unchar sp_tx_hw_hdcp_en;
static unchar sp_tx_hdcp_auth_done;

unchar sp_tx_pd_mode;
unchar sp_tx_rx_anx7730;
unchar sp_tx_rx_mydp;

static struct AudiInfoframe SP_TX_AudioInfoFrmae;
static struct Packet_AVI SP_TX_Packet_AVI;
static struct Packet_SPD SP_TX_Packet_SPD;
static struct Packet_MPEG SP_TX_Packet_MPEG;
enum SP_TX_System_State sp_tx_system_state;

/* ***************************************************************** */

/* GLOBAL VARIABLES DEFINITION FOR HDMI START */

/* ***************************************************************** */

static unchar g_HDMI_DVI_Status;
static unchar g_Cur_Pix_Clk;
static unchar g_Video_Stable_Cntr;
static unchar g_Audio_Stable_Cntr;
static unchar g_Sync_Expire_Cntr;
static unchar g_HDCP_Err_Cnt;

static ulong g_Cur_H_Res;
static ulong g_Cur_V_Res;
static unchar g_Video_Muted;
static unchar g_Audio_Muted;
static unchar g_CTS_Got;
static unchar g_Audio_Got;
static unchar g_VSI_Got;
static unchar g_No_VSI_Counter;

static enum HDMI_RX_System_State hdmi_system_state;

/* ***************************************************************** */

/* GLOBAL VARIABLES DEFINITION FOR HDMI END */

/* ***************************************************************** */

void sp_tx_variable_init(void)
{

	sp_tx_hdcp_auth_fail_counter = 0;
	sp_tx_hdcp_auth_pass = 0;
	sp_tx_hw_hdcp_en = 0;
	sp_tx_hdcp_capable_chk = 0;
	sp_tx_hdcp_auth_done = 0;
	sp_tx_pd_mode = 1;
	sp_tx_rx_anx7730 = 0;
	sp_tx_rx_mydp = 0;
	sp_tx_hw_lt_done = 0;
	sp_tx_hw_lt_enable = 0;
	sp_tx_link_config_done = 0;

	bEDID_break = 0;
	sp_tx_bw = BW_54G;

}

static void sp_tx_api_m_gen_clk_select(unchar bSpreading)
{
	unchar c;

	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_M_CALCU_CTRL, &c);
	if (bSpreading) {
		c |= M_GEN_CLK_SEL;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_M_CALCU_CTRL, c);
	} else {
		c &= ~M_GEN_CLK_SEL;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_M_CALCU_CTRL, c);
	}
}

void sp_tx_initialization(void)
{
	unchar c;

	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL_REG, &c);
	c |= SP_TX_RST_SW_RST;
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL_REG, c);
	c &= ~SP_TX_RST_SW_RST;
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL_REG, c);

	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_EXTRA_ADDR_REG, 0x50);
	/* disable HDCP polling mode. */
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CTRL, 0x02);
	/* enable M value read out */
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_LINK_DEBUG_REG, 0x30);

	/* disable polling HPD, force hotplug for HDCP */
	/* enable polling, for pll lock */
	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_DEBUG_REG1, &c);
	c |= 0x8a;
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_DEBUG_REG1, c);

	/* change the c-wire termination to 50 ohm */
	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_PLL_FILTER_CTRL11, &c);
	c |= 0x30;
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_PLL_FILTER_CTRL11, c);

	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CTRL0_REG, &c);
	c |= SP_TX_HDCP_CTRL0_KSVLIST_VLD;
	c |= SP_TX_HDCP_CTRL0_BKSV_SRM_PASS;
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CTRL0_REG, c);

	/* open short portect and 5V detect */
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_PLL_FILTER_CTRL6, 0x00);
	/* set power on time 1.5ms */
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_ANALOG_DEBUG_REG2, 0x06);

	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK1, 0x00);
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK2, 0x00);
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK3, 0x00);
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK4, 0x00);
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_INT_MASK, 0xb4);

	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_LINK_BW_SET_REG, 0x14);

	/* PHY parameter for cts */
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG0, 0x16);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG4, 0x1b);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG7, 0x22);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG9, 0x23);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG14, 0x09);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG17, 0x16);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG19, 0x1F);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG1, 0x26);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG5, 0x28);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG8, 0x2F);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG15, 0x10);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG18, 0x1F);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG2, 0x36);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG6, 0x3c);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG16, 0x10);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG3, 0x3F);

	/* M value select, select clock with downspreading */
	sp_tx_api_m_gen_clk_select(1);

	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_DP_POLLING_CTRL_REG, 0x01);
}

void sp_tx_power_down(enum SP_TX_POWER_BLOCK sp_tx_pd_block)
{
	unchar c;

	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_POWERD_CTRL_REG, &c);
	if (sp_tx_pd_block == SP_TX_PWR_REG)
		c |= SP_POWERD_REGISTER_REG;
	else if (sp_tx_pd_block == SP_TX_PWR_HDCP)
		c |= SP_POWERD_HDCP_REG;
	else if (sp_tx_pd_block == SP_TX_PWR_AUDIO)
		c |= SP_POWERD_AUDIO_REG;
	else if (sp_tx_pd_block == SP_TX_PWR_VIDEO)
		c |= SP_POWERD_VIDEO_REG;
	else if (sp_tx_pd_block == SP_TX_PWR_LINK)
		c |= SP_POWERD_LINK_REG;
	else if (sp_tx_pd_block == SP_TX_PWR_TOTAL)
		c |= SP_POWERD_TOTAL_REG;

	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_POWERD_CTRL_REG, c);

	pr_info("sp_tx_power_down");

}

void sp_tx_power_on(enum SP_TX_POWER_BLOCK sp_tx_pd_block)
{
	unchar c;

	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_POWERD_CTRL_REG, &c);
	if (sp_tx_pd_block == SP_TX_PWR_REG)
		c &= ~SP_POWERD_REGISTER_REG;
	else if (sp_tx_pd_block == SP_TX_PWR_HDCP)
		c &= ~SP_POWERD_HDCP_REG;
	else if (sp_tx_pd_block == SP_TX_PWR_AUDIO)
		c &= ~SP_POWERD_AUDIO_REG;
	else if (sp_tx_pd_block == SP_TX_PWR_VIDEO)
		c &= ~SP_POWERD_VIDEO_REG;
	else if (sp_tx_pd_block == SP_TX_PWR_LINK)
		c &= ~SP_POWERD_LINK_REG;
	else if (sp_tx_pd_block == SP_TX_PWR_TOTAL)
		c &= ~SP_POWERD_TOTAL_REG;

	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_POWERD_CTRL_REG, c);
	pr_info("sp_tx_power_on");

}

void sp_tx_rst_aux(void)
{
	unchar c, c1;

	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_DEBUG_REG1, &c1);
	c = c1;
	/* clear HPD polling and Transmitter polling */
	c1 &= 0xdd;
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_DEBUG_REG1, c1);

	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL2_REG, &c1);
	c1 |= SP_TX_AUX_RST;
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL2_REG, c1);
	mdelay(1);
	c1 &= ~SP_TX_AUX_RST;
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL2_REG, c1);

	/* enable  polling  after reset AUX-ANX.Fei-2011.9.19 */
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_DEBUG_REG1, c);
}

static unchar sp_tx_wait_aux_finished(void)
{
	unchar c;
	unchar cCnt;
	cCnt = 0;

	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_STATUS, &c);

	while (c & 0x10) {
		cCnt++;
		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_STATUS, &c);
		if (cCnt > 100) {
			pr_err("AUX Operaton does not finished, and time out.");
			break;
		}
	}

	if (c & 0x0F) {
		pr_err("aux operation failed %.2x\n", (uint)c);
		return 0;
	} else
		return 1;

}

static unchar sp_tx_aux_dpcdread_bytes(unchar addrh, unchar addrm,
					unchar addrl, unchar cCount,
					unchar *pBuf)
{
	unchar c, i;
	unchar bOK;

	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_COUNT_REG, 0x80);
	c = ((cCount - 1) << 4) | 0x09;
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, c);

	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_7_0_REG, addrl);
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_15_8_REG, addrm);

	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_19_16_REG, &c);
	c = (c & 0xf0) | addrh;
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_19_16_REG, c);

	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	c |= 0x01;
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, c);

	mdelay(2);

	bOK = sp_tx_wait_aux_finished();

	if (!bOK) {
		pr_err("aux read failed");
		sp_tx_rst_aux();
		return AUX_ERR;
	}

	for (i = 0; i < cCount; i++) {
		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_0_REG + i, &c);

		*(pBuf + i) = c;

		if (i >= MAX_BUF_CNT)
			break;
	}

	return AUX_OK;
}

static unchar sp_tx_aux_dpcdwrite_bytes(unchar addrh, unchar addrm,
					unchar addrl, unchar cCount,
					unchar *pBuf)
{
	unchar c, i;
	unchar bOK;

	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_COUNT_REG, 0x80);

	c =  ((cCount - 1) << 4) | 0x08;
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, c);

	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_7_0_REG, addrl);
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_15_8_REG, addrm);

	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_19_16_REG, &c);
	c = (c & 0xf0) | addrh;
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_19_16_REG, c);

	for (i = 0; i < cCount; i++) {
		c = *pBuf;
		pBuf++;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_0_REG + i, c);

		if (i >= MAX_BUF_CNT)
			break;
	}

	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	c |= 0x01;
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, c);

	bOK = sp_tx_wait_aux_finished();

	if (bOK)
		return AUX_OK;
	else
		return AUX_ERR;
}

static void sp_tx_aux_dpcdwrite_byte(unchar addrh, unchar addrm,
					unchar addrl, unchar data1)
{
	unchar c;

	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_COUNT_REG, 0x80);
	c = (0 << 4) | 0x08;
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, c);

	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_7_0_REG, addrl);
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_15_8_REG, addrm);

	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_19_16_REG, &c);
	c = (c & 0xf0) | addrh;
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_19_16_REG, c);

	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_0_REG, data1);

	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	c |= 0x01;
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, c);

	sp_tx_wait_aux_finished();

	return;
}

void sp_tx_set_colorspace(void)
{
	unchar c;
	unchar Color_Space;

	sp_tx_read_reg(HDMI_PORT1_ADDR, HDMI_RX_AVI_DATA00_REG, &Color_Space);
	Color_Space &= 0x60;
	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
	c = (c & 0xfc) | Color_Space >> 5;
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, c);
}

static void sp_tx_vsi_setup(void)
{
	unchar c;
	int i;

	for (i = 0; i < 13; i++) {
		sp_tx_read_reg(HDMI_PORT1_ADDR, (HDMI_RX_MPEG_DATA00_REG + i), &c);
		SP_TX_Packet_MPEG.MPEG_data[i] = c;
	}
}

static void sp_tx_mpeg_setup(void)
{
	unchar c;
	int i;

	for (i = 0; i < 13; i++) {
		sp_tx_read_reg(HDMI_PORT1_ADDR, (HDMI_RX_MPEG_DATA00_REG + i), &c);
		SP_TX_Packet_MPEG.MPEG_data[i] = c;
	}
}

static void sp_tx_get_int_status(enum INTStatus IntIndex, unchar *cStatus)
{
	unchar c;

	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_STATUS1 + IntIndex, &c);
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_STATUS1 + IntIndex, c);

	*cStatus = c;
}

static unchar sp_tx_get_pll_lock_status(void)
{
	unchar c;

	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_DEBUG_REG1, &c);
	if (c & SP_TX_DEBUG_PLL_LOCK)
		return 1;
	else
		return 0;
}

void sp_tx_set_3d_packets(void)
{
	unchar c;
	unchar hdmi_video_format, vsi_header, v3d_structure;

	if (g_VSI_Got) {
		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_3D_VSC_CTRL, &c);

		if (!(c & 0x01)) {
			sp_tx_read_reg(HDMI_PORT1_ADDR, HDMI_RX_MPEG_TYPE_REG,
				       &vsi_header);
			sp_tx_read_reg(HDMI_PORT1_ADDR, HDMI_RX_MPEG_DATA03_REG,
				       &hdmi_video_format);

			if ((vsi_header == 0x81)
			    && ((hdmi_video_format & 0xe0) == 0x40)) {
				pr_info("3D VSI packet is detected. Config VSI and VSC packet");

				sp_tx_vsi_setup();
				sp_tx_config_packets(VSI_PACKETS);

				sp_tx_read_reg(HDMI_PORT1_ADDR,
					       HDMI_RX_MPEG_DATA05_REG,
					       &v3d_structure);

				switch (v3d_structure & 0xf0) {
				case 0x00:/* frame packing */
					v3d_structure = 0x02;
					break;
				case 0x20:/* Line alternative */
					v3d_structure = 0x03;
					break;
				case 0x30:	/* Side-by-side(full) */
					v3d_structure = 0x04;
					break;
				default:
					v3d_structure = 0x00;
					pr_warn("3D structure is not supported");
					break;
				}

				sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_VSC_DBYTE1, v3d_structure);
				sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_3D_VSC_CTRL, &c);
				c = c|0x01;
				sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_3D_VSC_CTRL, c);

				sp_tx_read_reg(SP_TX_PORT0_ADDR,  SP_TX_PKT_EN_REG, &c);
				c = c & 0xfe;
				sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c);

				sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
				c = c|0x10;
				sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c);

				sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
				c = c|0x01;
				sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c);
			}
		}
		g_No_VSI_Counter = 0;
		g_VSI_Got = 0;
	} else {
		g_No_VSI_Counter++;

		if (g_No_VSI_Counter > 5) {
			sp_tx_read_reg(SP_TX_PORT0_ADDR, 0xea, &c);
			if (c & 0x01) {
				pr_info("No new VSI is received, disable  VSC packet");
				/* disable VSC */
				sp_tx_write_reg(SP_TX_PORT0_ADDR, 0xea, 0x00);
				sp_tx_mpeg_setup();
				sp_tx_config_packets(MPEG_PACKETS);
			}
			g_No_VSI_Counter = 0;
		}
	}
}

static void sp_tx_lvttl_bit_mapping(void)
{
	enum HDMI_color_depth hdmi_input_color_depth = Hdmi_legacy;
	unchar c, c1;

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_VIDEO_STATUS_REG1, &c1);
	c1 &= 0xf0;
	if (c1 == 0x00)
		hdmi_input_color_depth = Hdmi_legacy;
	else if (c1 == 0x40)
		hdmi_input_color_depth = Hdmi_24bit;
	else if (c1 == 0x50)
		hdmi_input_color_depth = Hdmi_30bit;
	else if (c1 == 0x60)
		hdmi_input_color_depth = Hdmi_36bit;
	else
		pr_warn("HDMI input color depth is not supported .\n");

	switch (hdmi_input_color_depth) {
	case Hdmi_legacy:
	case Hdmi_24bit:
		sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		c = c & ~0x04;
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, c);

		sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
		c = (c & 0x8f) | 0x10;
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, c);

		for (c = 0; c < 24; c++)
			sp_tx_write_reg(SP_TX_PORT2_ADDR, (0x40 + c), (0x00 + c));

		break;
	case Hdmi_30bit:
		sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		c = c & ~0x04;
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, c);

		sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
		c = (c & 0x8f) | 0x20;
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, c);

		for (c = 0; c < 10; c++)
			sp_tx_write_reg(SP_TX_PORT2_ADDR, (0x40 + c), (0x02 + c));

		for (c = 0; c < 10; c++)
			sp_tx_write_reg(SP_TX_PORT2_ADDR, (c + 0x4a), (c + 0x0E));

		for (c = 0; c < 10; c++)
			sp_tx_write_reg(SP_TX_PORT2_ADDR, (0x54 + c), (0x1A + c));

		break;
	case Hdmi_36bit:
		sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		c = c & ~0x04;
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, c);

		sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
		c = ((c & 0x8f) | 0x30);
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, c);

		for (c = 0; c < 36; c++)
			sp_tx_write_reg(SP_TX_PORT2_ADDR, (0x40 + c), (0x00 + c));

		break;
	default:
		break;
	}

	/* config blank with  YUV color space video */
	sp_tx_read_reg(HDMI_PORT1_ADDR, HDMI_RX_AVI_DATA00_REG, &c);
	if (c & 0x60) {
		sp_tx_write_reg(SP_TX_PORT0_ADDR, 0x2c, 0x80);
		sp_tx_write_reg(SP_TX_PORT0_ADDR, 0x2d, 0x00);
		sp_tx_write_reg(SP_TX_PORT0_ADDR, 0x2e, 0x80);
	}
}

static void sp_tx_enable_video_input(unchar Enable)
{
	unchar c;

	if (Enable) {
		sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		c = (c & 0xf7) | SP_TX_VID_CTRL1_VID_EN;
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, c);
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK1, 0xf5);
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_STATUS1, 0x0a);
		pr_info("Slimport Video is Enabled!");

	} else {
		sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		c &= ~SP_TX_VID_CTRL1_VID_EN;
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, c);
		pr_info("Slimport Video is disabled!");

	}
}

static void sp_tx_enhacemode_set(void)
{
	unchar c;
	sp_tx_aux_dpcdread_bytes(0x00, 0x00, DPCD_MAX_LANE_COUNT, 1, &c);

	if (c & 0x80) {
		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL4_REG, &c);
		c |= SP_TX_SYS_CTRL4_ENHANCED;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL4_REG, c);

		sp_tx_aux_dpcdread_bytes(0x00, 0x01, DPCD_LANE_COUNT_SET, 1, &c);
		sp_tx_aux_dpcdwrite_byte(0x00, 0x01, DPCD_LANE_COUNT_SET, c | 0x80);

		pr_info("Enhance mode enabled");
	} else {
		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL4_REG, &c);
		c &= ~SP_TX_SYS_CTRL4_ENHANCED;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL4_REG, c);

		sp_tx_aux_dpcdread_bytes(0x00, 0x01, DPCD_LANE_COUNT_SET, 1, &c);
		sp_tx_aux_dpcdwrite_byte(0x00, 0x01, DPCD_LANE_COUNT_SET, c & (~0x80));

		pr_info("Enhance mode disabled");
	}
}

static void sp_tx_hdcp_reauth(void)
{
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CTRL0_REG, 0x23);
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CTRL0_REG, 0x03);
}

static void sp_tx_clean_hdcp_status(void)
{
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CTRL0_REG, 0x00);
	sp_tx_hdcp_reauth();
}

static void sp_tx_hdcp_encryption_disable(void)
{
	unchar c;
	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CTRL0_REG, &c);
	c &= ~SP_TX_HDCP_CTRL0_ENC_EN;
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CTRL0_REG, c);
}

static void sp_tx_hdcp_encryption_enable(void)
{
	unchar c;
	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CTRL0_REG, &c);
	c |= SP_TX_HDCP_CTRL0_ENC_EN;
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CTRL0_REG, c);
}

static void sp_tx_hw_hdcp_enable(void)
{
	unchar c;
	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CTRL0_REG, &c);
	c &= 0xf3;
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CTRL0_REG, c);
	c |= 0x08;
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CTRL0_REG, c);
	sp_tx_write_reg(SP_TX_PORT0_ADDR, 0x40, 0xb0);
	sp_tx_write_reg(SP_TX_PORT0_ADDR, 0x42, 0xc8);

	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK2, 0xfc);
	pr_info("Hardware HDCP is enabled.");
}

void sp_tx_clean_hdcp(void)
{
	sp_tx_hdcp_auth_fail_counter = 0;
	sp_tx_hdcp_auth_pass = 0;
	sp_tx_hw_hdcp_en = 0;
	sp_tx_hdcp_capable_chk = 0;
	sp_tx_hdcp_auth_done = 0;
	sp_tx_clean_hdcp_status();
}

static void sp_tx_pclk_calc(unchar *hbr_rbr)
{
	long int str_clk = 0;
	unchar c;
	unchar link_bw_current = *hbr_rbr;

	switch (link_bw_current) {

	case BW_54G:
		str_clk = 540;
		break;
	case BW_27G:
		str_clk = 270;
		break;
	case BW_162G:
		str_clk = 162;
		break;
	default:
		break;

	}

	sp_tx_read_reg(SP_TX_PORT0_ADDR, M_VID_2, &c);
	M_val = c * 0x10000;
	sp_tx_read_reg(SP_TX_PORT0_ADDR, M_VID_1, &c);
	M_val = M_val + c * 0x100;
	sp_tx_read_reg(SP_TX_PORT0_ADDR, M_VID_0, &c);
	M_val = M_val + c;

	sp_tx_read_reg(SP_TX_PORT0_ADDR, N_VID_2, &c);
	N_val = c * 0x10000;
	sp_tx_read_reg(SP_TX_PORT0_ADDR, N_VID_1, &c);
	N_val = N_val + c * 0x100;
	sp_tx_read_reg(SP_TX_PORT0_ADDR, N_VID_0, &c);
	N_val = N_val + c;

	str_clk = str_clk * M_val;
	pclk = str_clk;
	pclk = pclk / N_val;
}

void sp_tx_show_infomation(void)
{
	unchar c, c1;
	uint h_res, h_act, v_res, v_act;
	uint h_fp, h_sw, h_bp, v_fp, v_sw, v_bp;
	ulong fresh_rate;

	pr_info("\n*******SP Video Information*******\n");

	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_LINK_BW_SET_REG, &c);
	if (c == 0x06) {
		pr_info("   BW = 1.62G");
		sp_tx_pclk_calc(&c);
	} else if (c == 0x0a) {
		pr_info("   BW = 2.7G");
		sp_tx_pclk_calc(&c);
	} else if (c == 0x14) {
		pr_info("   BW = 5.4G");
		sp_tx_pclk_calc(&c);
	}
#ifdef SSC_EN
	pr_info("   SSC On");
#else
	pr_info("   SSC Off");
#endif

	pr_info("   M = %lu, N = %lu, PCLK = %ld MHz\n", M_val, N_val, pclk);

	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_LINE_STA_L, &c);
	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_LINE_STA_H, &c1);

	v_res = c1;
	v_res = v_res << 8;
	v_res = v_res + c;

	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINE_STA_L, &c);
	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINE_STA_H, &c1);

	v_act = c1;
	v_act = v_act << 8;
	v_act = v_act + c;

	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXEL_STA_L, &c);
	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXEL_STA_H, &c1);

	h_res = c1;
	h_res = h_res << 8;
	h_res = h_res + c;

	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXEL_STA_L, &c);
	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXEL_STA_H, &c1);

	h_act = c1;
	h_act = h_act << 8;
	h_act = h_act + c;

	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_H_F_PORCH_STA_L, &c);
	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_H_F_PORCH_STA_H, &c1);

	h_fp = c1;
	h_fp = h_fp << 8;
	h_fp = h_fp + c;

	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_H_SYNC_STA_L, &c);
	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_H_SYNC_STA_H, &c1);

	h_sw = c1;
	h_sw = h_sw << 8;
	h_sw = h_sw + c;

	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_H_B_PORCH_STA_L, &c);
	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_H_B_PORCH_STA_H, &c1);

	h_bp = c1;
	h_bp = h_bp << 8;
	h_bp = h_bp + c;

	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_V_F_PORCH_STA, &c);
	v_fp = c;

	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_V_SYNC_STA, &c);
	v_sw = c;

	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_V_B_PORCH_STA, &c);
	v_bp = c;

	pr_info("   Total resolution is %d * %d \n", h_res, v_res);

	pr_info("   HF=%d, HSW=%d, HBP=%d\n", h_fp, h_sw, h_bp);
	pr_info("   VF=%d, VSW=%d, VBP=%d\n", v_fp, v_sw, v_bp);
	pr_info("   Active resolution is %d * %d ", h_act, v_act);

	fresh_rate = pclk * 1000;
	fresh_rate = fresh_rate / h_res;
	fresh_rate = fresh_rate * 1000;
	fresh_rate = fresh_rate / v_res;
	pr_info(" @ %ldHz\n", fresh_rate);

	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_VID_CTRL, &c);

	if ((c & 0x06) == 0x00)
		pr_info("   ColorSpace: RGB,");
	else if ((c & 0x06) == 0x02)
		pr_info("   ColorSpace: YCbCr422,");
	else if ((c & 0x06) == 0x04)
		pr_info("   ColorSpace: YCbCr444,");

	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_VID_CTRL, &c);

	if ((c & 0xe0) == 0x00)
		pr_info("  6 BPC");
	else if ((c & 0xe0) == 0x20)
		pr_info("  8 BPC");
	else if ((c & 0xe0) == 0x40)
		pr_info("  10 BPC");
	else if ((c & 0xe0) == 0x60)
		pr_info("  12 BPC");

	pr_info("\n***************************\n");

}

static void sp_tx_aux_wr(unchar offset)
{
	unchar c, cnt;
	cnt = 0;

	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_0_REG, offset);
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, 0x04);
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x01);
	mdelay(10);
	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	while (c & 0x01) {
		mdelay(10);
		cnt++;

		if (cnt == 10) {
			pr_err("write break");
			cnt = 0;
			bEDID_break = 1;
			break;
		}

		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	}

}

static void sp_tx_aux_rd(unchar len_cmd)
{
	unchar c, cnt;
	cnt = 0;

	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, len_cmd);
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x01);
	mdelay(10);
	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);

	while (c & 0x01) {
		mdelay(10);
		cnt++;

		if (cnt == 10) {
			pr_err("read break");
			sp_tx_rst_aux();
			bEDID_break = 1;
			break;
		}

		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	}

}

unchar sp_tx_chip_located(void)
{
	unchar c1, c2;

	sp_tx_hardware_poweron();
	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_DEV_IDL_REG, &c1);
	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_DEV_IDH_REG, &c2);
	if ((c1 == 0x06) && (c2 == 0x78)) {
		pr_info("ANX7808 is found. \n");
		return 1;
	} else {
		pr_info("ANX7808 is not found. \n");
		return 0;
	}
}

void sp_tx_vbus_poweron(void)
{
	unchar c;
	int i;

	for (i = 0; i < 5; i++) {
		/* power down macro */
		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_ANALOG_POWER_DOWN_REG, &c);
		c |= SP_TX_ANALOG_POWER_DOWN_MACRO_PD;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_ANALOG_POWER_DOWN_REG, c);
		/* Power up  5V detect and short portect circuit */
		sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_PLL_FILTER_CTRL6, &c);
		c &= ~0x30;
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_PLL_FILTER_CTRL6,  c);

		/*  Enable power 3.3v out */
		sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_PLL_FILTER_CTRL11, &c);
		c &= 0xFd;
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_PLL_FILTER_CTRL11, c);
		c |= 0x02;
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_PLL_FILTER_CTRL11, c);

		mdelay(100);

		/* power on macro */
		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_ANALOG_POWER_DOWN_REG, &c);
		c &= ~SP_TX_ANALOG_POWER_DOWN_MACRO_PD;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_ANALOG_POWER_DOWN_REG, c);

		pr_info("Try to enable VBUS power");
		sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_PLL_FILTER_CTRL6, &c);
		if (!(c & 0xc0)) {
			pr_info("3.3V output enabled");
			return;
		}
		msleep(100);
	}
}

void sp_tx_vbus_powerdown(void)
{
	unchar c;

	/*  Disableable power 3.3v out */
	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_PLL_FILTER_CTRL11, &c);
	c &= ~SP_TX_V33_SWITCH_ON;
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_PLL_FILTER_CTRL11, c);

	/* Power down  5V detect and short portect circuit */
	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_PLL_FILTER_CTRL6, &c);
	c |= 0x30;
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_PLL_FILTER_CTRL6, c);
	pr_info("3.3V output disabled");
}

static void sp_tx_spread_enable(unchar bEnable)
{
	unchar c;

	sp_tx_read_reg(SP_TX_PORT0_ADDR, SSC_CTRL_REG1, &c);

	if (bEnable) {
		c |= SPREAD_AMP;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SSC_CTRL_REG1, c);

		sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL2_REG, &c);
		c |= SP_TX_RST_SSC;
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL2_REG, c);
		c &= ~SP_TX_RST_SSC;
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL2_REG, c);

		sp_tx_aux_dpcdread_bytes(0x00, 0x01, DPCD_DOWNSPREAD_CTRL, 1, &c);
		c |= 0x10;
		sp_tx_aux_dpcdwrite_byte(0x00, 0x01, DPCD_DOWNSPREAD_CTRL, c);

	} else {

		c &= ~SPREAD_AMP;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SSC_CTRL_REG1, c);

		sp_tx_aux_dpcdread_bytes(0x00, 0x01, DPCD_DOWNSPREAD_CTRL, 1, &c);
		c &= 0xef;
		sp_tx_aux_dpcdwrite_byte(0x00, 0x01, DPCD_DOWNSPREAD_CTRL, c);
	}

}

static void sp_tx_config_ssc(enum SP_LINK_BW linkbw)
{
	unchar c;


	sp_tx_write_reg(SP_TX_PORT0_ADDR, SSC_CTRL_REG1, 0x00);
	sp_tx_aux_dpcdread_bytes(0x00, 0x00, DPCD_MAX_DOWNSPREAD, 1, &c);

#ifndef SSC_1
	/*	pr_info("*** Config SSC 0.4% ***");*/

	if (linkbw == BW_54G) {
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL1, 0xc0);
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL2, 0x00);
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL3, 0x75);
	} else if (linkbw == BW_27G) {
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL1, 0x5f);
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL2, 0x00);
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL3, 0x75);
	} else {
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL1, 0x9e);
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL2, 0x00);
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL3, 0x6d);
	}
#else
	/*	pr_info("*** Config SSC 1% ***");*/
	if (linkbw == BW_54G) {
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL1, 0xdd);
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL2, 0x01);
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL3, 0x76);
	} else if (linkbw == BW_27G) {
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL1, 0xef);
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL2, 0x00);
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL3, 0x76);
	} else {
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL1, 0x8e);
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL2, 0x01);
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL3, 0x6d);
	}
#endif
	sp_tx_spread_enable(1);
}

static void sp_tx_audioinfoframesetup(void)
{
	int i;
	unchar c;
	sp_tx_read_reg(HDMI_PORT1_ADDR, 0xc0, &c);
	SP_TX_AudioInfoFrmae.type = c;
	sp_tx_read_reg(HDMI_PORT1_ADDR, 0xc1, &c);
	SP_TX_AudioInfoFrmae.version = c;
	sp_tx_read_reg(HDMI_PORT1_ADDR, 0xc2, &c);
	SP_TX_AudioInfoFrmae.length = c;
	for (i = 0; i < 11; i++) {
		sp_tx_read_reg(HDMI_PORT1_ADDR, (0xc4 + i), &c);
		SP_TX_AudioInfoFrmae.pb_byte[i] = c;
	}
}

static void sp_tx_infoframeupdate(struct AudiInfoframe *pAudioInfoFrame)
{
	unchar c;

	c = pAudioInfoFrame->type;
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_TYPE, c);

	c = pAudioInfoFrame->version;
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_VER, c);

	c = pAudioInfoFrame->length;
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_LEN, c);

	c = pAudioInfoFrame->pb_byte[0];
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB0, c);

	c = pAudioInfoFrame->pb_byte[1];
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB1, c);

	c = pAudioInfoFrame->pb_byte[2];
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB2, c);

	c = pAudioInfoFrame->pb_byte[3];
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB3, c);

	c = pAudioInfoFrame->pb_byte[4];
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB4, c);

	c = pAudioInfoFrame->pb_byte[5];
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB5, c);

	c = pAudioInfoFrame->pb_byte[6];
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB6, c);

	c = pAudioInfoFrame->pb_byte[7];
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB7, c);

	c = pAudioInfoFrame->pb_byte[8];
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB8, c);

	c = pAudioInfoFrame->pb_byte[9];
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB9, c);

	c = pAudioInfoFrame->pb_byte[10];
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB10, c);

	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
	c |= SP_TX_PKT_AUD_UP;
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c);

	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
	c |= SP_TX_PKT_AUD_EN;
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c);
}

static void sp_tx_enable_audio_output(unchar bEnable)
{
	unchar c;

	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUD_CTRL, &c);

	if (bEnable) {
		c |= SP_TX_AUD_CTRL_AUD_EN;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUD_CTRL, c);

		sp_tx_audioinfoframesetup();
		sp_tx_infoframeupdate(&SP_TX_AudioInfoFrmae);
	} else {
		c &= ~SP_TX_AUD_CTRL_AUD_EN;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUD_CTRL, c);

		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
		c &= ~SP_TX_PKT_AUD_EN;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c);
	}
}

static void sp_tx_get_link_bw(unchar *bwtype)
{
	unchar c;

	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_LINK_BW_SET_REG, &c);

	*bwtype = c;
}

static void sp_tx_config_audio(void)
{
	unchar c, g_BW;
	int i;
	ulong M_AUD, LS_Clk = 0;
	ulong AUD_Freq = 0;
	pr_notice("##Config audio ##");
	sp_tx_power_on(SP_TX_PWR_AUDIO);
	sp_tx_read_reg(HDMI_PORT0_ADDR, 0xCA, &c);

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

	sp_tx_get_link_bw(&g_BW);

	switch (g_BW) {
	case BW_162G:
		LS_Clk = 162000;
		break;
	case BW_27G:
		LS_Clk = 270000;
		break;
	case BW_54G:
		LS_Clk = 540000;
		break;
	default:
		break;
	}

	pr_info("AUD_Freq = %ld , LS_CLK = %ld\n", AUD_Freq, LS_Clk);

	M_AUD = ((512 * AUD_Freq) / LS_Clk) * 32768;
	M_AUD = M_AUD + 0x05;
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_AUD_INTERFACE_CTRL4, (M_AUD & 0xff));
	M_AUD = M_AUD >> 8;
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_AUD_INTERFACE_CTRL5, (M_AUD & 0xff));
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_AUD_INTERFACE_CTRL6, 0x00);

	/* let hdmi audio sample into DP */
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_AUD_INTERFACE_CTRL0, 0x00);

	/* enable audio auto adjust */
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_AUD_INTERFACE_CTRL2, 0x04);

	/* configure layout and channel number */
	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_HDMI_STATUS_REG, &c);

	if (c & 0x08) {
		sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_CH_NUM_REG5, &c);
		c |= 0xe1;
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_CH_NUM_REG5, c);
	} else {
		sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_CH_NUM_REG5, &c);
		c &= ~0xe1;
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_CH_NUM_REG5, c);
	}

	/* transfer audio chaneel status from HDMI Rx to Slinmport Tx */
	for (i = 0; i < 5; i++) {
		sp_tx_read_reg(HDMI_PORT0_ADDR, (0xc7 + i), &c);
		sp_tx_write_reg(SP_TX_PORT2_ADDR, (0xD0 + i), c);
	}

	/* enable audio */
	sp_tx_enable_audio_output(1);
	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK1, &c);
	c |= 0x04;
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK1, c);
}


static void sp_tx_get_rx_bw(unchar bMax, unchar *cBw)
{
	if (bMax)
		sp_tx_aux_dpcdread_bytes(0x00, 0x00, DPCD_MAX_LINK_RATE, 1, cBw);
	else
		sp_tx_aux_dpcdread_bytes(0x00, 0x01, DPCD_LINK_BW_SET, 1, cBw);
}

static void sp_tx_edid_read_initial(void)
{
	unchar c;

	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_7_0_REG, 0x50);
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_15_8_REG, 0);
	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_19_16_REG, &c);
	c &= 0xf0;
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_19_16_REG, c);
}

static unchar sp_tx_aux_edidread_byte(unchar offset)
{

	unchar c, i, edid[16], data_cnt, cnt;
	unchar bReturn = 0;

	cnt = 0;

	sp_tx_aux_wr(offset);
	sp_tx_aux_rd(0xf5);

	data_cnt = 0;
	while (data_cnt < 16) {
		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_COUNT_REG, &c);
		c = c & 0x1f;
		if (c != 0) {
			for (i = 0; i < c; i++) {
				sp_tx_read_reg(SP_TX_PORT0_ADDR,
					       SP_TX_BUF_DATA_0_REG + i,
					       &edid[i + data_cnt]);
			}
		} else {
			sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, 0x01);
			sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x03);
			sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
			while (c & 0x01) {
				mdelay(2);
				cnt++;

				if (cnt == 10) {
					pr_err("read break");
					sp_tx_rst_aux();
					bEDID_break = 1;
					bReturn = 0x01;
				}

				sp_tx_read_reg(SP_TX_PORT0_ADDR,
					       SP_TX_AUX_CTRL_REG2, &c);
			}

			bReturn = 0x02;
			return bReturn;
		}

		data_cnt = data_cnt + c;
		if (data_cnt < 16) {
			sp_tx_rst_aux();
			mdelay(10);
			c = 0x05 | ((0x0f - data_cnt) << 4);
			sp_tx_aux_rd(c);
		}
	}

	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, 0x01);
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x03);
	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);

	while (c & 0x01)
		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);

	if (offset < 0x80) {

		for (i = 0; i < 16; i++)
			bEDID_firstblock[offset + i] = edid[i];
	} else if (offset >= 0x80) {

		for (i = 0; i < 16; i++)
			bEDID_extblock[offset - 0x80 + i] = edid[i];
	}

#ifdef EDID_DEBUG_PRINT

		for (i = 0; i < 16; i++) {
			if ((i & 0x0f) == 0)
				pr_info("\n edid: [%.2x]  %.2x  ",
				  (unsigned int)offset, (uint)edid[i]);
			else
				pr_info("%.2x  ", (uint)edid[i]);

			if ((i & 0x0f) == 0x0f)
				pr_info("\n");
		}

#endif

	return bReturn;
}

static void sp_tx_parse_segments_edid(unchar segment, unchar offset)
{
	unchar c, cnt;
	int i;

	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, 0x04);
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_7_0_REG, 0x30);
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x03);
	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	sp_tx_wait_aux_finished();
	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, &c);
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_0_REG, segment);

	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, 0x04);
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x01);
	cnt = 0;
	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	while (c & 0x01) {
		mdelay(10);
		cnt++;
		if (cnt == 10) {
			pr_err("write break");
			sp_tx_rst_aux();
			cnt = 0;
			bEDID_break = 1;
			return;
		}

		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);

	}

	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_7_0_REG, 0x50);
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x03);
	sp_tx_aux_wr(offset);
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x03);
	sp_tx_aux_rd(0xf5);
	cnt = 0;
	for (i = 0; i < 16; i++) {
		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_COUNT_REG, &c);
		while ((c & 0x1f) == 0) {
			mdelay(2);
			cnt++;
			sp_tx_read_reg(SP_TX_PORT0_ADDR,
				       SP_TX_BUF_DATA_COUNT_REG, &c);

			if (cnt == 10) {
				pr_err("read break");
				sp_tx_rst_aux();
				bEDID_break = 1;
				return;
			}
		}

		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_0_REG + i, &c);
	}

	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, 0x01);
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x03);
	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);

	while (c & 0x01)
		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);

}

static unchar sp_tx_get_edid_block(void)
{
	unchar c;
	sp_tx_aux_wr(0x00);
	sp_tx_aux_rd(0x01);
	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_0_REG, &c);


	sp_tx_aux_wr(0x7e);
	sp_tx_aux_rd(0x01);
	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_0_REG, &c);
	pr_info("EDID Block = %d\n", (int)(c + 1));

	if (c > 3)
		bEDID_break = 1;

	return c;
}

static void sp_tx_addronly_set(unchar bSet)
{
	unchar c;

	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	if (bSet) {
		c |= SP_TX_ADDR_ONLY_BIT;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, c);
	} else {
		c &= ~SP_TX_ADDR_ONLY_BIT;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, c);
	}
}

static void sp_tx_load_packet(enum PACKETS_TYPE type)
{
	int i;
	unchar c;

	switch (type) {
	case AVI_PACKETS:
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_AVI_TYPE, 0x82);
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_AVI_VER, 0x02);
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_AVI_LEN, 0x0d);

		for (i = 0; i < 13; i++) {
			sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_AVI_DB0 + i,
					SP_TX_Packet_AVI.AVI_data[i]);
		}

		break;

	case SPD_PACKETS:
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_SPD_TYPE, 0x83);
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_SPD_VER, 0x01);
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_SPD_LEN, 0x19);

		for (i = 0; i < 25; i++) {
			sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_SPD_DATA1 + i,
					SP_TX_Packet_SPD.SPD_data[i]);
		}

		break;

	case VSI_PACKETS:
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_MPEG_TYPE, 0x81);
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_MPEG_VER, 0x01);
		sp_tx_read_reg(HDMI_PORT1_ADDR, HDMI_RX_MPEG_LEN_REG, &c);
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_MPEG_LEN, c);

		for (i = 0; i < 13; i++) {
			sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_MPEG_DATA0 + i,
					SP_TX_Packet_MPEG.MPEG_data[i]);
		}

		break;
	case MPEG_PACKETS:
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_MPEG_TYPE, 0x85);
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_MPEG_VER, 0x01);
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_MPEG_LEN, 0x0d);

		for (i = 0; i < 13; i++) {
			sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_MPEG_DATA0 + i,
					SP_TX_Packet_MPEG.MPEG_data[i]);
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

		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
		c &= ~SP_TX_PKT_AVI_EN;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c);
		sp_tx_load_packet(AVI_PACKETS);

		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
		c |= SP_TX_PKT_AVI_UD;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c);

		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
		c |= SP_TX_PKT_AVI_EN;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c);

		break;

	case SPD_PACKETS:
		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
		c &= ~SP_TX_PKT_SPD_EN;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c);
		sp_tx_load_packet(SPD_PACKETS);

		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
		c |= SP_TX_PKT_SPD_UD;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c);

		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
		c |=  SP_TX_PKT_SPD_EN;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c);

		break;

	case VSI_PACKETS:
		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
		c &= ~SP_TX_PKT_MPEG_EN;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c);

		sp_tx_load_packet(VSI_PACKETS);

		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
		c |= SP_TX_PKT_MPEG_UD;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c);

		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
		c |= SP_TX_PKT_MPEG_EN;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c);

		break;
	case MPEG_PACKETS:
		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
		c &= ~SP_TX_PKT_MPEG_EN;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c);


		sp_tx_load_packet(MPEG_PACKETS);

		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
		c |= SP_TX_PKT_MPEG_UD;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c);

		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
		c |= SP_TX_PKT_MPEG_EN;
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c);

		break;

	default:
		break;
	}
}

void sp_tx_avi_setup(void)
{
	unchar c;
	int i;
	for (i = 0; i < 13; i++) {
		sp_tx_read_reg(HDMI_PORT1_ADDR, (HDMI_RX_AVI_DATA00_REG + i), &c);
		SP_TX_Packet_AVI.AVI_data[i] = c;
	}
}

static unchar sp_tx_bw_lc_sel(void)
{
	unchar over_bw = 0;
	uint pixel_clk = 0;
	enum HDMI_color_depth hdmi_input_color_depth = Hdmi_legacy;
	unchar c;

	pr_info("input pclk = %d\n", (unsigned int)pclk);

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_VIDEO_STATUS_REG1, &c);
	c &= 0xf0;

	if (c == 0x00)
		hdmi_input_color_depth = Hdmi_legacy;
	else if (c == 0x40)
		hdmi_input_color_depth = Hdmi_24bit;
	else if (c == 0x50)
		hdmi_input_color_depth = Hdmi_30bit;
	else if (c == 0x60)
		hdmi_input_color_depth = Hdmi_36bit;
	else
		pr_warn("HDMI input color depth is not supported .\n");

	switch (hdmi_input_color_depth) {
	case Hdmi_legacy:
	case Hdmi_24bit:
		pixel_clk = pclk;
		break;
	case Hdmi_30bit:
		pixel_clk = pclk * 5 / 4;
		break;
	case Hdmi_36bit:
		pixel_clk = pclk * 3 / 2;
		break;
	default:
		break;
	}

	if (pixel_clk <= 54) {
		sp_tx_bw = BW_162G;
		over_bw = 0;
	} else if ((54 < pixel_clk) && (pixel_clk <= 90)) {
		if (sp_tx_bw >= BW_27G) {
			sp_tx_bw = BW_27G;
			over_bw = 0;
		} else
			over_bw = 1;
	} else if ((90 < pixel_clk) && (pixel_clk <= 180)) {
		if (sp_tx_bw >= BW_54G) {
			sp_tx_bw = BW_54G;
			over_bw = 0;
		} else
			over_bw = 1;
	} else
		over_bw = 1;

	if (over_bw)
		pr_err("over bw!\n");
	else
		pr_notice("The optimized BW =%.2x\n", sp_tx_bw);

	return over_bw;

}

unchar sp_tx_hw_link_training(void)
{

	unchar c;

	if (!sp_tx_hw_lt_enable) {

		pr_notice("Hardware link training");
#ifdef SSC_EN
		sp_tx_config_ssc(sp_tx_bw);
#else
		sp_tx_spread_enable(0);
#endif

		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_LINK_BW_SET_REG, sp_tx_bw);


		sp_tx_write_reg(SP_TX_PORT0_ADDR, 0xc8, 0x01);
		mdelay(2);
		sp_tx_write_reg(SP_TX_PORT0_ADDR, 0xc8, 0x00);

		sp_tx_enhacemode_set();

		sp_tx_aux_dpcdread_bytes(0x00, 0x06, 0x00, 0x01, &c);
		sp_tx_aux_dpcdwrite_byte(0x00, 0x06, 0x00, (c | 0x01));


		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_TRAINING_LANE0_SET_REG, 0x09);
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_LINK_TRAINING_CTRL_REG,
						SP_TX_LINK_TRAINING_CTRL_EN);

		sp_tx_hw_lt_enable = 1;
		return 1;

	}


	if (sp_tx_hw_lt_done) {
		sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x02, 1, ByteBuf);


		if (ByteBuf[0] != 0x07) {
			sp_tx_hw_lt_enable = 0;
			sp_tx_hw_lt_done = 0;
			return 1;
		} else {
			sp_tx_hw_lt_done = 1;

			/* if there is link error, adjust pre-emphsis to check error again.
			If there is no error,keep the setting, otherwise use 400mv0db */
			if (sp_tx_link_err_check()) {
				sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_TRAINING_LANE0_SET_REG, 0x09);

				if (sp_tx_link_err_check())
					sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_TRAINING_LANE0_SET_REG, 0x01);
			}

			sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_LINK_BW_SET_REG, &c);

			if (c != sp_tx_bw) {
				sp_tx_hw_lt_done = 0;
				sp_tx_hw_lt_enable = 0;
				return 1;
			}

			sp_tx_set_system_state(SP_TX_CONFIG_SLIMPORT_OUTPUT);
			return 0;
		}
	}
	return 0;
}

uint sp_tx_link_err_check(void)
{
	uint errl = 0, errh = 0;

	sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x10, 2, ByteBuf);
	mdelay(5);
	sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x10, 2, ByteBuf);
	errh = ByteBuf[1];

	if (errh & 0x80) {
		errl = ByteBuf[0];
		errh = (errh & 0x7f) << 8;
		errl = errh + errl;
	}

	pr_err(" Err of Lane = %d\n", errl);
	return errl;
}

unchar sp_tx_lt_pre_config(void)
{
	unchar legel_bw, c;
	unchar link_bw = 0;

	legel_bw = 1;

	if (!sp_tx_get_pll_lock_status()) {
		pr_err("PLL not lock!");
		return 1;
	}

	if (!sp_tx_link_config_done) {
		sp_tx_get_rx_bw(1, &c);
		sp_tx_bw = c;

		if ((sp_tx_bw != BW_27G) && (sp_tx_bw != BW_162G)
		    && (sp_tx_bw != BW_54G))
			return 1;

		sp_tx_power_on(SP_TX_PWR_VIDEO);
		sp_tx_video_mute(1);

		sp_tx_enable_video_input(1);
		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL2_REG, &c);
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL2_REG, c);
		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL2_REG, &c);

		if (c & SP_TX_SYS_CTRL2_CHA_STA) {
			pr_err("Stream clock not stable!");
			return 1;
		}

		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, &c);
		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, c);
		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, &c);

		if (!(c & SP_TX_SYS_CTRL3_STRM_VALID)) {
			pr_err("video stream not valid!");
			return 1;
		}

		sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_LINK_BW_SET_REG, 0x14);
		sp_tx_get_link_bw(&link_bw);
		sp_tx_pclk_calc(&link_bw);

		/* Optimize the LT to get minimum power consumption */
		if (sp_tx_bw_lc_sel()) {
			pr_err("****Over bandwidth****");
			return 1;
		}

		sp_tx_link_config_done = 1;
	}

	return 0;
}

void sp_tx_video_mute(unchar enable)
{
	unchar c;

	if (enable) {
		sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		c |= SP_TX_VID_CTRL1_VID_MUTE;
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, c);
	} else {
		sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		c &= ~SP_TX_VID_CTRL1_VID_MUTE;
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, c);
	}

}

void sp_tx_send_message(enum SP_TX_SEND_MSG message)
{
	unchar c;

	switch (message) {
	case MSG_OCM_EN:
		sp_tx_aux_dpcdwrite_byte(0x00, 0x05, 0x25, 0x5a);
		break;

	case MSG_INPUT_HDMI:
		sp_tx_aux_dpcdwrite_byte(0x00, 0x05, 0x26, 0x01);
		break;

	case MSG_INPUT_DVI:
		sp_tx_aux_dpcdwrite_byte(0x00, 0x05, 0x26, 0x00);
		break;

	case MSG_LINK_TRAINING:
		sp_tx_aux_dpcdwrite_byte(0x00, 0x05, 0x27, 0x01);
		break;

	case MSG_CLEAR_IRQ:
		sp_tx_aux_dpcdread_bytes(0x00, 0x04, 0x10, 1, &c);
		c |= 0x01;
		sp_tx_aux_dpcdwrite_byte(0x00, 0x04, 0x10, c);
		break;
	}

}


unchar sp_tx_get_cable_type(void)
{
	unchar SINK_OUI[8] = { 0 };
	unchar ds_port_preset = 0;
	unchar ds_port_recoginze = 0;
	int i;

	for (i = 0; i < 5; i++) {
		sp_tx_aux_dpcdread_bytes(0x00, 0x00, 0x05, 1, &ds_port_preset);
		switch (ds_port_preset & 0x07) {
		case 0x00:
			sp_tx_rx_mydp = 1;
			sp_tx_rx_anx7730 = 0;
			ds_port_recoginze = 1;
			pr_notice("Downstream is DP dongle.");
			break;
		case 0x03:
			sp_tx_aux_dpcdread_bytes(0x00, 0x04, 0x00, 8, SINK_OUI);

			if ((SINK_OUI[0] == 0x00) && (SINK_OUI[1] == 0x22)
			    && (SINK_OUI[2] == 0xb9)
			    && (SINK_OUI[3] == 0x61) && (SINK_OUI[4] == 0x39)
			    && (SINK_OUI[5] == 0x38)
			    && (SINK_OUI[6] == 0x33)) {
				pr_notice("Downstream is VGA dongle.");
				sp_tx_rx_anx7730 = 0;
				sp_tx_rx_mydp = 0;
				ds_port_recoginze = 1;
			}
			break;
		case 0x05:
			sp_tx_aux_dpcdread_bytes(0x00, 0x04, 0x00, 8, SINK_OUI);

			if ((SINK_OUI[0] == 0xb9) && (SINK_OUI[1] == 0x22)
			    && (SINK_OUI[2] == 0x00)
			    && (SINK_OUI[3] == 0x00) && (SINK_OUI[4] == 0x00)
			    && (SINK_OUI[5] == 0x00)
			    && (SINK_OUI[6] == 0x00)) {
				pr_notice("Downstream is HDMI dongle.");
				sp_tx_send_message(MSG_OCM_EN);
				sp_tx_rx_anx7730 = 1;
				sp_tx_rx_mydp = 0;
				ds_port_recoginze = 1;
			}
			break;
		default:
			msleep(1000);
			pr_err("Downstream can not recognized.");
			sp_tx_rx_anx7730 = 0;
			sp_tx_rx_mydp = 0;
			ds_port_recoginze = 0;
			break;
		}

		if (ds_port_recoginze)
			return 1;

	}

	return 0;
}

unchar sp_tx_get_hdmi_connection(void)
{
	unchar c;
	msleep(200);

	sp_tx_aux_dpcdread_bytes(0x00, 0x05, 0x18, 1, &c);
	if ((c & 0x41) == 0x41) {
		sp_tx_aux_dpcdwrite_byte(0x00, 0x05, 0xf3, 0x70);
		return 1;
	} else
		return 0;
}

unchar sp_tx_get_vga_connection(void)
{
	unchar c;
	sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x00, 1, &c);
	if (c & 0x01)
		return 1;
	else
		return 0;
}

static unchar sp_tx_get_ds_video_status(void)
{
	unchar c;
	sp_tx_aux_dpcdread_bytes(0x00, 0x05, 0x27, 1, &c);
	if (c & 0x01)
		return 1;
	else
		return 0;
}

unchar sp_tx_get_dp_connection(void)
{
	unchar c;

	sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x00, 1, &c);
	if (c & 0x01) {
		sp_tx_aux_dpcdread_bytes(0x00, 0x00, 0x04, 1, &c);
		if (c & 0x20)
			sp_tx_aux_dpcdwrite_byte(0x00, 0x06, 0x00, 0x20);
		return 1;
	} else
		return 0;
}

void sp_tx_edid_read(void)
{
	unchar i, j, edid_block = 0, segment = 0, offset = 0;

	sp_tx_edid_read_initial();

	bEDID_break = 0;

	sp_tx_addronly_set(1);
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, 0x04);
	sp_tx_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x01);
	sp_tx_wait_aux_finished();

	edid_block = sp_tx_get_edid_block();

	if (edid_block < 2) {
		edid_block = 8 * (edid_block + 1);
		for (i = 0; i < edid_block; i++) {
			if (!bEDID_break)
				sp_tx_aux_edidread_byte(i * 16);
			mdelay(10);
		}

		sp_tx_addronly_set(0);

	} else {

		for (i = 0; i < 16; i++) {
			if (!bEDID_break)
				sp_tx_aux_edidread_byte(i * 16);
		}

		sp_tx_addronly_set(0);

		if (!bEDID_break) {
			edid_block = (edid_block + 1);

			for (i = 0; i < ((edid_block - 1) / 2); i++) {
				pr_notice("EXT 256 EDID block");
				segment = i + 1;

				for (j = 0; j < 16; j++) {
					sp_tx_parse_segments_edid(segment,
								  offset);
					offset = offset + 0x10;
				}
			}

			if (edid_block % 2) {
				pr_notice("Last block");
				segment = segment + 1;

				for (j = 0; j < 8; j++) {
					sp_tx_parse_segments_edid(segment,
								  offset);
					offset = offset + 0x10;
				}
			}
		}
	}

	sp_tx_addronly_set(0);
	sp_tx_rst_aux();

}

static void sp_tx_pll_changed_int_handler(void)
{

	if (sp_tx_system_state > SP_TX_PARSE_EDID) {
		if (!sp_tx_get_pll_lock_status()) {
			pr_err("PLL:_______________PLL not lock!");
			sp_tx_clean_hdcp();
			sp_tx_set_system_state(SP_TX_LINK_TRAINING);
			sp_tx_link_config_done = 0;
			sp_tx_hw_lt_done = 0;
			sp_tx_hw_lt_enable = 0;
		}
	}
}

static void sp_tx_auth_done_int_handler(void)
{
	unchar c;

	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_STATUS, &c);

	if (c & SP_TX_HDCP_AUTH_PASS) {
		sp_tx_aux_dpcdread_bytes(0x06, 0x80, 0x2A, 2, ByteBuf);
		c = ByteBuf[1];

		if (c & 0x08) {
			/* max cascade read, fail */
			sp_tx_video_mute(1);
			sp_tx_clean_hdcp();
			pr_err("Re-authentication!");
		} else {
			pr_notice("Authentication pass in Auth_Done");
			sp_tx_hdcp_auth_pass = 1;
			sp_tx_hdcp_auth_fail_counter = 0;
		}

	} else {
		pr_err("Authentication failed in AUTH_done");
		sp_tx_hdcp_auth_pass = 0;
		sp_tx_hdcp_auth_fail_counter++;

		if (sp_tx_hdcp_auth_fail_counter >= SP_TX_HDCP_FAIL_THRESHOLD) {
			sp_tx_video_mute(1);
			sp_tx_clean_hdcp();
		} else {
			sp_tx_video_mute(1);
			sp_tx_clean_hdcp();
			pr_err("Re-authentication!");

			if (sp_tx_system_state > SP_TX_CONFIG_SLIMPORT_OUTPUT) {
				sp_tx_set_system_state
				    (SP_TX_HDCP_AUTHENTICATION);
				return;
			}

		}
	}

	sp_tx_hdcp_auth_done = 1;
}

static void sp_tx_lt_done_int_handler(void)
{
	unchar c, c1;

	if ((sp_tx_hw_lt_done) || (sp_tx_system_state != SP_TX_LINK_TRAINING))
		return;

	sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_LINK_TRAINING_CTRL_REG, &c);
	if (c & 0x80) {
		c = (c & 0x70) >> 4;
		pr_err("HW LT failed in interrupt, ERR code = %.2x\n", (uint) c);

		sp_tx_link_config_done = 0;
		sp_tx_hw_lt_enable = 0;
		sp_tx_hw_lt_done = 0;
		sp_tx_set_system_state(SP_TX_LINK_TRAINING);

	} else {

		sp_tx_hw_lt_done = 1;
		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_TRAINING_LANE0_SET_REG, &c);
		sp_tx_read_reg(SP_TX_PORT0_ADDR, SP_TX_LINK_BW_SET_REG, &c1);
		pr_notice("HW LT succeed,LANE0_SET = %.2x, link_bw = %.2x\n", (uint) c,
		  (uint) c1);
	}
}

static void sp_tx_link_change_int_handler(void)
{
	unchar lane0_1_status, sl_cr, al;

	if (sp_tx_system_state < SP_TX_CONFIG_SLIMPORT_OUTPUT)
		return;

	sp_tx_link_err_check();

	sp_tx_aux_dpcdread_bytes(0x00, 0x02, DPCD_LANE_ALIGN_STATUS_UPDATED, 1,
				 ByteBuf);
	al = ByteBuf[0];
	sp_tx_aux_dpcdread_bytes(0x00, 0x02, DPCD_LANE0_1_STATUS, 1, ByteBuf);
	lane0_1_status = ByteBuf[0];

	if (((lane0_1_status & 0x01) == 0) || ((lane0_1_status & 0x04) == 0))
		sl_cr = 0;
	else
		sl_cr = 1;
	if (((al & 0x01) == 0) || (sl_cr == 0)) {
		if ((al & 0x01) == 0)
			pr_err("Lane align not done\n");

		if (sl_cr == 0)
			pr_err("Lane clock recovery not done\n");

		if ((sp_tx_system_state > SP_TX_LINK_TRAINING)
		    && sp_tx_link_config_done) {
			sp_tx_link_config_done = 0;
			sp_tx_set_system_state(SP_TX_LINK_TRAINING);
			pr_err("IRQ:____________re-LT request!");
		}
	}

}

static void sp_tx_polling_err_int_handler(void)
{
	unchar c;
	int i;

	if ((sp_tx_system_state < SP_TX_WAIT_SLIMPORT_PLUGIN) || sp_tx_pd_mode)
		return;

	for (i = 0; i < 5; i++) {
		sp_tx_aux_dpcdread_bytes(0x00, 0x00, 0x00, 1, &c);

		if (c == 0x11)
			return;

		mdelay(10);
	}

	if (sp_tx_pd_mode == 0) {
		pr_err("Cwire polling is corrupted,power down ANX7808.\n");
		sp_tx_clean_hdcp();
		sp_tx_vbus_powerdown();
		sp_tx_power_down(SP_TX_PWR_TOTAL);
		sp_tx_power_down(SP_TX_PWR_REG);
		sp_tx_hardware_powerdown();
		sp_tx_set_system_state(SP_TX_WAIT_SLIMPORT_PLUGIN);
		sp_tx_pd_mode = 1;
		sp_tx_link_config_done = 0;
		sp_tx_hw_lt_enable = 0;
		sp_tx_hw_lt_done = 0;
		sp_tx_rx_anx7730 = 0;
		sp_tx_rx_mydp = 0;
	}
}

static void sp_tx_irq_isp(void)
{
	unchar c, c1, lane0_1_status, sl_cr, al;
	unchar IRQ_Vector, Int_vector1, Int_vector2;

	sp_tx_aux_dpcdread_bytes(0x00, 0x02, DPCD_DEVICE_SERVICE_IRQ_VECTOR, 1,
				 ByteBuf);
	IRQ_Vector = ByteBuf[0];
	sp_tx_aux_dpcdwrite_bytes(0x00, 0x02, DPCD_DEVICE_SERVICE_IRQ_VECTOR, 1,
				  ByteBuf);

	/* HDCP IRQ */
	if (IRQ_Vector & 0x04) {
		if (sp_tx_hdcp_auth_pass) {
			sp_tx_aux_dpcdread_bytes(0x06, 0x80, 0x29, 1, &c1);
			if (c1 & 0x04) {
				if (sp_tx_system_state >
				    SP_TX_HDCP_AUTHENTICATION) {
					sp_tx_set_system_state
					    (SP_TX_HDCP_AUTHENTICATION);
					sp_tx_clean_hdcp();
					pr_err("IRQ:____________HDCP Sync lost!");
				}
			}
		}
	}

	/* specific int */
	if ((IRQ_Vector & 0x40) && (sp_tx_rx_anx7730)) {


		sp_tx_aux_dpcdread_bytes(0x00, 0x05, DPCD_SPECIFIC_INTERRUPT_1,
					 1, &Int_vector1);
		sp_tx_aux_dpcdwrite_byte(0x00, 0x05, DPCD_SPECIFIC_INTERRUPT_1,
					 Int_vector1);

		sp_tx_aux_dpcdread_bytes(0x00, 0x05, DPCD_SPECIFIC_INTERRUPT_2,
					 1, &Int_vector2);
		sp_tx_aux_dpcdwrite_byte(0x00, 0x05, DPCD_SPECIFIC_INTERRUPT_2,
					 Int_vector2);

		if ((Int_vector1 & 0x01) == 0x01) {
			sp_tx_aux_dpcdread_bytes(0x00, 0x05, 0x18, 1, &c);
			if (c & 0x01)
				pr_notice("Downstream HDMI is pluged!\n");
		}

		if ((Int_vector1 & 0x02) == 0x02) {
			sp_tx_aux_dpcdread_bytes(0x00, 0x05, 0x18, 1, &c);
			if ((c & 0x01) != 0x01) {
				pr_notice("Downstream HDMI is unpluged!\n");

				if ((sp_tx_system_state >
				     SP_TX_WAIT_SLIMPORT_PLUGIN)
				    && (!sp_tx_pd_mode)) {
					sp_tx_clean_hdcp();
					sp_tx_power_down(SP_TX_PWR_REG);
					sp_tx_power_down(SP_TX_PWR_TOTAL);
					sp_tx_hardware_powerdown();
					sp_tx_set_system_state(SP_TX_WAIT_SLIMPORT_PLUGIN);
					sp_tx_pd_mode = 1;
					sp_tx_link_config_done = 0;
					sp_tx_hw_lt_done = 0;
					sp_tx_hw_lt_enable = 0;
				}
			}
		}

		if (((Int_vector1 & 0x04) == 0x04)
		    && (sp_tx_system_state > SP_TX_CONFIG_SLIMPORT_OUTPUT)) {

			pr_err("Rx specific  IRQ: Link is down!\n");

			sp_tx_aux_dpcdread_bytes(0x00, 0x02,
						 DPCD_LANE_ALIGN_STATUS_UPDATED,
						 1, ByteBuf);
			al = ByteBuf[0];

			sp_tx_aux_dpcdread_bytes(0x00, 0x02,
						 DPCD_LANE0_1_STATUS, 1,
						 ByteBuf);
			lane0_1_status = ByteBuf[0];

			if (((lane0_1_status & 0x01) == 0)
			    || ((lane0_1_status & 0x04) == 0))
				sl_cr = 0;
			else
				sl_cr = 1;

			if (((al & 0x01) == 0) || (sl_cr == 0)) {
				if ((al & 0x01) == 0)
					pr_err("Lane align not done\n");

				if (sl_cr == 0)
					pr_err("Lane clock recovery not done\n");

				if ((sp_tx_system_state > SP_TX_LINK_TRAINING)
				    && sp_tx_link_config_done) {
					sp_tx_link_config_done = 0;
					sp_tx_hw_lt_enable = 0;
					sp_tx_hw_lt_done = 0;
					sp_tx_set_system_state(SP_TX_LINK_TRAINING);
					pr_err("IRQ:____________re-LT request!");
				}
			}

			sp_tx_aux_dpcdread_bytes(0x00, 0x05, 0x18, 1, &c);
			if (!(c & 0x40)) {
				if ((sp_tx_system_state > SP_TX_WAIT_SLIMPORT_PLUGIN)
				    && (!sp_tx_pd_mode)) {
					sp_tx_clean_hdcp();
					sp_tx_power_down(SP_TX_PWR_REG);
					sp_tx_power_down(SP_TX_PWR_TOTAL);
					sp_tx_hardware_powerdown();
					sp_tx_set_system_state(SP_TX_WAIT_SLIMPORT_PLUGIN);
					sp_tx_pd_mode = 1;
					sp_tx_link_config_done = 0;
					sp_tx_hw_lt_done = 0;
					sp_tx_hw_lt_enable = 0;
				}
			}

		}

		if ((Int_vector1 & 0x08) == 0x08) {
			pr_info("Downstream HDCP is done!\n");

			if ((Int_vector1 & 0x10) != 0x10)
				pr_info("Downstream HDCP is passed!\n");
			else {
				if (sp_tx_system_state > SP_TX_CONFIG_SLIMPORT_OUTPUT) {
					sp_tx_video_mute(1);
					sp_tx_clean_hdcp();
					sp_tx_set_system_state(SP_TX_HDCP_AUTHENTICATION);
					pr_err("Re-authentication due to downstream HDCP failure!");
				}
			}
		}

		if ((Int_vector1 & 0x20) == 0x20) {
			pr_err(" Downstream HDCP link integrity check fail!");

			if (sp_tx_system_state > SP_TX_HDCP_AUTHENTICATION) {
				sp_tx_set_system_state(SP_TX_HDCP_AUTHENTICATION);
				sp_tx_clean_hdcp();
				pr_err("IRQ:____________HDCP Sync lost!");
			}
		}

		if ((Int_vector1 & 0x40) == 0x40)
			pr_info("Receive CEC command from upstream done!");


		if ((Int_vector1 & 0x80) == 0x80)
			pr_info("CEC command transfer to downstream done!");


		if ((Int_vector2 & 0x04) == 0x04) {
			sp_tx_aux_dpcdread_bytes(0x00, 0x05, 0x18, 1, &c);

			if ((c & 0x40) == 0x40)
				pr_notice("Downstream HDMI termination is detected!\n");
		}

		/* specific int */
	} else if ((IRQ_Vector & 0x40) && (!sp_tx_rx_anx7730)) {

		sp_tx_send_message(MSG_CLEAR_IRQ);
		sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x00, 1, &c);
		if (!(c & 0x01)) {
			if ((sp_tx_system_state > SP_TX_WAIT_SLIMPORT_PLUGIN)
			    && (!sp_tx_pd_mode)) {
				sp_tx_power_down(SP_TX_PWR_TOTAL);
				sp_tx_power_down(SP_TX_PWR_REG);
				sp_tx_vbus_powerdown();
				sp_tx_hardware_powerdown();
				sp_tx_clean_hdcp();
				sp_tx_pd_mode = 1;
				sp_tx_link_config_done = 0;
				sp_tx_hw_lt_enable = 0;
				sp_tx_hw_lt_done = 0;
				sp_tx_set_system_state(SP_TX_WAIT_SLIMPORT_PLUGIN);
			}
		}

		sp_tx_aux_dpcdread_bytes(0x00, 0x02, DPCD_LANE_ALIGN_STATUS_UPDATED, 1, ByteBuf);
		al = ByteBuf[0];
		sp_tx_aux_dpcdread_bytes(0x00, 0x02, DPCD_LANE0_1_STATUS, 1, ByteBuf);
		lane0_1_status = ByteBuf[0];

		if (((lane0_1_status & 0x01) == 0)
		    || ((lane0_1_status & 0x04) == 0))
			sl_cr = 0;
		else
			sl_cr = 1;
		if (((al & 0x01) == 0) || (sl_cr == 0)) {
			if ((al & 0x01) == 0)
				pr_err("Lane align not done\n");

			if (sl_cr == 0)
				pr_err("Lane clock recovery not done\n");

			if ((sp_tx_system_state > SP_TX_LINK_TRAINING)
			    && sp_tx_link_config_done) {
				sp_tx_link_config_done = 0;
				sp_tx_hw_lt_enable = 0;
				sp_tx_hw_lt_done = 0;
				sp_tx_set_system_state(SP_TX_LINK_TRAINING);
				pr_err("IRQ:____________re-LT request!");
			}
		}
	}
}

static void sp_tx_sink_irq_int_handler(void)
{
	sp_tx_irq_isp();
}

void sp_tx_hdcp_process(void)
{
	unchar c;

	if (!sp_tx_hdcp_capable_chk) {
		sp_tx_hdcp_capable_chk = 1;

		sp_tx_aux_dpcdread_bytes(0x06, 0x80, 0x28, 1, &c);
		if (!(c & 0x01)) {
			pr_err("Sink is not capable HDCP");
			sp_tx_video_mute(1);
			sp_tx_set_system_state(SP_TX_PLAY_BACK);
			return;
		}
	}

	if (!sp_tx_get_ds_video_status())
		return;

	if (!sp_tx_hw_hdcp_en) {
		sp_tx_power_on(SP_TX_PWR_HDCP);
		mdelay(50);
		sp_tx_hw_hdcp_enable();
		sp_tx_hw_hdcp_en = 1;
	}

	if (sp_tx_hdcp_auth_done) {
		sp_tx_hdcp_auth_done = 0;

		if (sp_tx_hdcp_auth_pass) {

			sp_tx_hdcp_encryption_enable();
			sp_tx_video_mute(0);
			pr_notice("@@@@@@@hdcp_auth_pass@@@@@@\n");

		} else {
			sp_tx_hdcp_encryption_disable();
			sp_tx_video_mute(1);
			pr_notice("*********hdcp_auth_failed*********\n");
			return;
		}

		sp_tx_set_system_state(SP_TX_PLAY_BACK);
		sp_tx_show_infomation();
	}
}

void sp_tx_set_system_state(enum SP_TX_System_State ss)
{

	pr_notice("SP_TX To System State: ");

	switch (ss) {
	case SP_TX_INITIAL:
		sp_tx_system_state = SP_TX_INITIAL;
		pr_notice("SP_TX_INITIAL");
		break;
	case SP_TX_WAIT_SLIMPORT_PLUGIN:
		sp_tx_system_state = SP_TX_WAIT_SLIMPORT_PLUGIN;
		pr_notice("SP_TX_WAIT_SLIMPORT_PLUGIN");
		break;
	case SP_TX_PARSE_EDID:
		sp_tx_system_state = SP_TX_PARSE_EDID;
		pr_notice("SP_TX_READ_PARSE_EDID");
		break;
	case SP_TX_CONFIG_HDMI_INPUT:
		sp_tx_system_state = SP_TX_CONFIG_HDMI_INPUT;
		pr_notice("SP_TX_CONFIG_HDMI_INPUT");
		break;
	case SP_TX_CONFIG_SLIMPORT_OUTPUT:
		sp_tx_system_state = SP_TX_CONFIG_SLIMPORT_OUTPUT;
		pr_notice("SP_TX_CONFIG_SLIMPORT_OUTPUT");
		break;
	case SP_TX_LINK_TRAINING:
		sp_tx_system_state = SP_TX_LINK_TRAINING;
		sp_tx_link_config_done = 0;
		sp_tx_hw_lt_enable = 0;
		sp_tx_hw_lt_done = 0;
		pr_notice("SP_TX_LINK_TRAINING");
		break;
	case SP_TX_HDCP_AUTHENTICATION:
		sp_tx_system_state = SP_TX_HDCP_AUTHENTICATION;
		pr_notice("SP_TX_HDCP_AUTHENTICATION");
		break;
	case SP_TX_PLAY_BACK:
		sp_tx_system_state = SP_TX_PLAY_BACK;
		pr_notice("SP_TX_PLAY_BACK");
		break;
	default:
		break;
	}
}

void sp_tx_int_irq_handler(void)
{
	unchar c1, c2, c3, c4, c5;

	sp_tx_get_int_status(COMMON_INT_1, &c1);
	sp_tx_get_int_status(COMMON_INT_2, &c2);
	sp_tx_get_int_status(COMMON_INT_3, &c3);
	sp_tx_get_int_status(COMMON_INT_4, &c4);
	sp_tx_get_int_status(SP_INT_STATUS, &c5);


	if (c1 & SP_COMMON_INT1_PLL_LOCK_CHG)
		sp_tx_pll_changed_int_handler();

	if (c2 & SP_COMMON_INT2_AUTHDONE)
		sp_tx_auth_done_int_handler();

	if (c5 & SP_TX_INT_DPCD_IRQ_REQUEST)
		sp_tx_sink_irq_int_handler();

	if (c5 & SP_TX_INT_STATUS1_POLLING_ERR)
		sp_tx_polling_err_int_handler();

	if (c5 & SP_TX_INT_STATUS1_TRAINING_Finish)
		sp_tx_lt_done_int_handler();

	if (c5 & SP_TX_INT_STATUS1_LINK_CHANGE)
		sp_tx_link_change_int_handler();

}

#ifdef EYE_TEST
void sp_tx_eye_diagram_test(void)
{
	unchar c;
	int i;

	sp_tx_write_reg(SP_TX_PORT2_ADDR, 0x05, 0x00);

	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL_REG, &c);
	c |= SP_TX_RST_SW_RST;
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL_REG, c);
	c &= ~SP_TX_RST_SW_RST;
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL_REG, c);

	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG0, 0x16);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG4, 0x1b);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG7, 0x22);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG9, 0x23);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG14, 0x09);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG17, 0x16);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG19, 0x1F);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG1, 0x26);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG5, 0x28);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG8, 0x2F);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG15, 0x10);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG18, 0x1F);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG2, 0x36);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG6, 0x3c);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG16, 0x10);
	sp_tx_write_reg(SP_TX_PORT1_ADDR, SP_TX_LT_CTRL_REG3, 0x3F);
	/* set link bandwidth 5.4G */
	sp_tx_write_reg(SP_TX_PORT0_ADDR, 0xa0, 0x14);
	/* set swing 400mv3.5db */
	sp_tx_write_reg(SP_TX_PORT0_ADDR, 0xa3, 0x09);
	/* send link error measurement patterns */
	sp_tx_write_reg(SP_TX_PORT0_ADDR, 0xa2, 0x08);

	pr_info
	    ("        0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F");

	for (i = 0; i < 256; i++) {
		sp_tx_read_reg(0x72, i, &c);

		if ((i & 0x0f) == 0)
			pr_info("\n [%.2x]	%.2x  ", (unsigned int)i,
			       (unsigned int)c);
		else
			pr_info("%.2x  ", (unsigned int)c);

		if ((i & 0x0f) == 0x0f)
			pr_info("\n-------------------------------------");
	}
	pr_info("\n");
	pr_info
	    ("        0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F");

	for (i = 0; i < 256; i++) {
		sp_tx_read_reg(0x70, i, &c);

		if ((i & 0x0f) == 0)
			pr_info("\n [%.2x]	%.2x  ", (unsigned int)i,
			       (unsigned int)c);
		else
			pr_info("%.2x  ", (unsigned int)c);

		if ((i & 0x0f) == 0x0f)
			pr_info("\n-------------------------------------");
	}

	pr_info("*******Eye Diagram Test Pattern is sent********\n");

}
#endif

/* ***************************************************************** */
/* Functions defination for HDMI Input */
/* ***************************************************************** */

void hdmi_rx_set_hpd(unchar Enable)
{
	unchar c;

	if (Enable) {
		/* set HPD high */
		sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL3_REG, &c);
		c |= SP_TX_VID_CTRL3_HPD_OUT;
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL3_REG, c);
		msleep(100);
		pr_notice("HPD high is issued\n");
	} else {
		/* set HPD low */
		sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL3_REG, &c);
		c &= ~SP_TX_VID_CTRL3_HPD_OUT;
		sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL3_REG, c);
		msleep(100);
		pr_notice("HPD low is issued\n");
	}
}

void hdmi_rx_set_termination(unchar Enable)
{
	unchar c;

	if (Enable) {
		/* set termination high */
		sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_TMDS_CTRL_REG6, &c);
		c &= ~0x01;
		sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_TMDS_CTRL_REG6, c);
		mdelay(10);
		pr_notice("Termination high is issued\n");
	} else {
		/* set termination low */
		sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_TMDS_CTRL_REG6, &c);
		c |= 0x01;
		sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_TMDS_CTRL_REG6, c);
		mdelay(10);
		pr_notice("Termination low is issued\n");
	}
}

void hdmi_rx_tmds_en(void)
{
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_PORT_SEL_REG, 0x11);
	pr_notice("TMDS and DDC are enabled\n");
}

static void hdmi_rx_restart_audio_chk(void)
{
	if (hdmi_system_state == HDMI_AUDIO_CONFIG) {
		pr_info("WAIT_AUDIO: HDMI_RX_Restart_Audio_Chk.");
		g_CTS_Got = 0;
		g_Audio_Got = 0;
	}
}

static void hdmi_rx_set_sys_state(unchar ss)
{
	if (hdmi_system_state != ss) {
		pr_notice("");
		hdmi_system_state = ss;

		switch (ss) {
		case HDMI_CLOCK_DET:
			pr_notice("HDMI_RX:  HDMI_CLOCK_DET");
			break;
		case HDMI_SYNC_DET:
			pr_notice("HDMI_RX:  HDMI_SYNC_DET");
			break;
		case HDMI_VIDEO_CONFIG:
			pr_notice("HDMI_RX:  HDMI_VIDEO_CONFIG");
			break;
		case HDMI_AUDIO_CONFIG:
			pr_notice("HDMI_RX:  HDMI_AUDIO_CONFIG");
			hdmi_rx_restart_audio_chk();
			break;
		case HDMI_PLAYBACK:
			pr_notice("HDMI_RX:  HDMI_PLAYBACK");
			break;
		default:
			break;
		}
	}
}

static void hdmi_rx_mute_video(void)
{
	unchar c;

	pr_info("Mute Video.");
	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_HDMI_MUTE_CTRL_REG, &c);
	c |=  HDMI_RX_VID_MUTE;
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_HDMI_MUTE_CTRL_REG, c);
	g_Video_Muted = 1;
}

static void hdmi_rx_unmute_video(void)
{
	unchar c;

	pr_info("Unmute Video.");
	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_HDMI_MUTE_CTRL_REG, &c);
	c &= ~HDMI_RX_VID_MUTE;
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_HDMI_MUTE_CTRL_REG, c);
	g_Video_Muted = 0;
}

static void hdmi_rx_mute_audio(void)
{
	unchar c;

	pr_info("Mute Audio.");
	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_HDMI_MUTE_CTRL_REG, &c);
	c |= HDMI_RX_AUD_MUTE;
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_HDMI_MUTE_CTRL_REG, c);
	g_Audio_Muted = 1;
}

static void hdmi_rx_unmute_audio(void)
{
	unchar c;

	pr_info("Unmute Audio.");
	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_HDMI_MUTE_CTRL_REG, &c);
	c &= ~HDMI_RX_AUD_MUTE;
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_HDMI_MUTE_CTRL_REG, c);
	g_Audio_Muted = 0;
}

static unchar hdmi_rx_is_video_change(void)
{
	unchar ch, cl;
	ulong n;
	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_HTOTAL_LOW, &cl);
	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_HTOTAL_HIGH, &ch);
	n = ch;
	n = (n << 8) + cl;

	if ((g_Cur_H_Res < (n - 10)) || (g_Cur_H_Res > (n + 10))) {
		pr_err("H_Res changed.");
		pr_err("Current H_Res = %ld\n", n);
		return 1;
	}

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_VTOTAL_LOW, &cl);
	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_VTOTAL_HIGH, &ch);
	n = ch;
	n = (n << 8) + cl;

	if ((g_Cur_V_Res < (n - 10)) || (g_Cur_V_Res > (n + 10))) {
		pr_err("V_Res changed.\n");
		pr_err("Current V_Res = %ld\n", n);
		return 1;
	}

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_HDMI_STATUS_REG, &cl);

	cl &= HDMI_RX_HDMI_MODE;

	if (g_HDMI_DVI_Status != cl) {
		pr_err("DVI to HDMI or HDMI to DVI Change.");
		return 1;
	}

	return 0;
}

static void hdmi_rx_get_video_info(void)
{
	unchar ch, cl;
	uint n;

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_HTOTAL_LOW, &cl);
	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_HTOTAL_HIGH, &ch);
	n = ch;
	n = (n << 8) + cl;
	g_Cur_H_Res = n;
	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_VTOTAL_LOW, &cl);
	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_VTOTAL_HIGH, &ch);
	n = ch;
	n = (n << 8) + cl;
	g_Cur_V_Res = n;

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_VID_PCLK_CNTR_REG, &cl);
	g_Cur_Pix_Clk = cl;
	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_HDMI_STATUS_REG, &cl);
	g_HDMI_DVI_Status = ((cl & HDMI_RX_HDMI_MODE) == HDMI_RX_HDMI_MODE);
}

static void hdmi_rx_show_video_info(void)
{
	unchar c, c1;
	unchar cl, ch;
	ulong n;
	ulong h_res, v_res;

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_HACT_LOW, &cl);
	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_HACT_HIGH, &ch);
	n = ch;
	n = (n << 8) + cl;
	h_res = n;

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_VACT_LOW, &cl);
	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_VACT_HIGH, &ch);
	n = ch;
	n = (n << 8) + cl;
	v_res = n;

	pr_info("");
	pr_info("*****************HDMI_RX Info*******************");
	pr_info("HDMI_RX Is Normally Play Back.\n");
	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_HDMI_STATUS_REG, &c);

	if (c & HDMI_RX_HDMI_MODE)
		pr_info("HDMI_RX Mode = HDMI Mode.\n");
	else
		pr_info("HDMI_RX Mode = DVI Mode.\n");

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_VIDEO_STATUS_REG1, &c);
	if (c & HDMI_RX_VIDEO_TYPE)
		v_res += v_res;

	pr_info("HDMI_RX Video Resolution = %ld * %ld ", h_res, v_res);
	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_VIDEO_STATUS_REG1, &c);

	if (c & HDMI_RX_VIDEO_TYPE)
		pr_info("    Interlace Video.");
	else
		pr_info("    Progressive Video.");

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_SYS_CTRL1_REG, &c);

	if ((c & 0x30) == 0x00)
		pr_info("Input Pixel Clock = Not Repeated.\n");
	else if ((c & 0x30) == 0x10)
		pr_info("Input Pixel Clock = 2x Video Clock. Repeated.\n");
	else if ((c & 0x30) == 0x30)
		pr_info("Input Pixel Clock = 4x Vvideo Clock. Repeated.\n");

	if ((c & 0xc0) == 0x00)
		pr_info("Output Video Clock = Not Divided.\n");
	else if ((c & 0xc0) == 0x40)
		pr_info("Output Video Clock = Divided By 2.\n");
	else if ((c & 0xc0) == 0xc0)
		pr_info("Output Video Clock = Divided By 4.\n");

	if (c & 0x02)
		pr_info("Output Video Using Rising Edge To Latch Data.\n");
	else
		pr_info("Output Video Using Falling Edge To Latch Data.\n");

	pr_info("Input Video Color Depth = ");
	sp_tx_read_reg(HDMI_PORT0_ADDR, 0x70, &c1);
	c1 &= 0xf0;

	if (c1 == 0x00)
		pr_info("Legacy Mode.\n");
	else if (c1 == 0x40)
		pr_info("24 Bit Mode.\n");
	else if (c1 == 0x50)
		pr_info("30 Bit Mode.\n");
	else if (c1 == 0x60)
		pr_info("36 Bit Mode.\n");
	else if (c1 == 0x70)
		pr_info("48 Bit Mode.\n");

	pr_info("Input Video Color Space = ");
	sp_tx_read_reg(HDMI_PORT1_ADDR, HDMI_RX_AVI_DATA00_REG, &c);
	c &= 0x60;

	if (c == 0x20)
		pr_info("YCbCr4:2:2 .\n");
	else if (c == 0x40)
		pr_info("YCbCr4:4:4 .\n");
	else if (c == 0x00)
		pr_info("RGB.\n");
	else
		pr_info("Unknow 0x44 = 0x%.2x\n", (int)c);

	sp_tx_read_reg(HDMI_PORT1_ADDR, HDMI_RX_HDCP_STATUS_REG, &c);

	if (c & HDMI_RX_AUTHEN)
		pr_info("Authentication is attempted.");
	else
		pr_info("Authentication is not attempted.");

	for (cl = 0; cl < 20; cl++) {
		sp_tx_read_reg(HDMI_PORT1_ADDR, HDMI_RX_HDCP_STATUS_REG, &c);

		if (c & HDMI_RX_DECRYPT)
			break;
		else
			mdelay(10);
	}

	if (cl < 20)
		pr_info("Decryption is active.");
	else
		pr_info("Decryption is not active.");


	pr_info("********************************************************");
	pr_info("");
}

static void hdmi_rx_show_audio_info(void)
{
	unchar c;

	pr_info("Audio Fs = ");
	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_AUD_IN_SPDIF_CH_STATUS4_REG,
		       &c);
	c &= 0x0f;

	switch (c) {
	case 0x00:
		pr_info("44.1 KHz.");
		break;
	case 0x02:
		pr_info("48 KHz.");
		break;
	case 0x03:
		pr_info("32 KHz.");
		break;
	case 0x08:
		pr_info("88.2 KHz.");
		break;
	case 0x0a:
		pr_info("96 KHz.");
		break;
	case 0x0e:
		pr_info("192 KHz.");
		break;
	default:
		break;
	}

	pr_info("");
}

static void hdmi_rx_init_var(void)
{
	hdmi_rx_set_sys_state(HDMI_CLOCK_DET);
	g_Cur_H_Res = 0;
	g_Cur_V_Res = 0;
	g_Cur_Pix_Clk = 0;
	g_Video_Muted = 1;
	g_Audio_Muted = 1;
	g_Audio_Stable_Cntr = 0;
	g_Video_Stable_Cntr = 0;
	g_Sync_Expire_Cntr = 0;
	g_HDCP_Err_Cnt = 0;
	g_HDMI_DVI_Status = DVI_MODE;
	g_CTS_Got = 0;
	g_Audio_Got = 0;
	g_VSI_Got = 0;
	g_No_VSI_Counter = 0;
}

void hdmi_rx_initialize(void)
{
	unchar c;

	hdmi_rx_init_var();

	/*  Mute audio and video output */
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_HDMI_MUTE_CTRL_REG, 0x03);

	/*  Enable pll_lock, digital clock detect, disable analog clock detect */
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_CHIP_CTRL_REG, 0xe5);

	/* enable HW AVMUTE control */
	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_AEC_CTRL_REG, &c);
	c |= HDMI_RX_AVC_OE;
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_AEC_CTRL_REG, c);

	/* change HDCP enable criteria value to meet HDCP CTS */
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_HDCP_EN_CRITERIA_REG, 0x08);

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_SRST_REG, &c);
	c |= HDMI_RX_HDCP_MAN_RST;
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_SRST_REG, c);
	mdelay(10);
	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_SRST_REG, &c);
	c &= ~HDMI_RX_HDCP_MAN_RST;
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_SRST_REG, c);

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_SRST_REG, &c);
	c |= HDMI_RX_SW_MAN_RST;
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_SRST_REG, c);
	mdelay(10);
	c &= ~HDMI_RX_SW_MAN_RST;
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_SRST_REG, c);

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_SRST_REG, &c);
	c |= HDMI_RX_TMDS_RST;
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_SRST_REG, c);

	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_INT_MASK1_REG, 0xff);
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_INT_MASK2_REG, 0xf3);
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_INT_MASK3_REG, 0x3f);
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_INT_MASK4_REG, 0x17);
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_INT_MASK5_REG, 0xff);
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_INT_MASK6_REG, 0xff);
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_INT_MASK7_REG, 0x07);

	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_AEC_EN0_REG, 0xe7);
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_AEC_EN1_REG, 0x17);
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_AEC_EN2_REG, 0xf0);

	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_VID_OUTPUT_CTRL3_REG, 0X00);

	/* # Enable AVC and AAC */
	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_AEC_CTRL_REG, &c);
	c |=  HDMI_RX_AVC_EN;
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_AEC_CTRL_REG, c);

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_AEC_CTRL_REG, &c);
	c |= HDMI_RX_AAC_EN;
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_AEC_CTRL_REG, c);

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_SYS_PWDN1_REG, &c);
	c &= ~HDMI_RX_PWDN_CTRL;
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_SYS_PWDN1_REG, c);

	/*  Set EQ Value */
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_TMDS_CTRL_REG2, 0x00);
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_TMDS_CTRL_REG4, 0X28);
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_TMDS_CTRL_REG5, 0xe3);
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_TMDS_CTRL_REG7, 0X50);
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_TMDS_CTRL_REG21, 0x04);
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_TMDS_CTRL_REG22, 0x38);

	/* force 5V detection */
	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_CHIP_CTRL_REG, &c);
	c |= HDMI_RX_MAN_HDMI5V_DET;
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_CHIP_CTRL_REG, c);

	/* software reset  for  HDMI key load issue */
	sp_tx_read_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL_REG, &c);
	c |= SP_TX_RST_SW_RST;
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL_REG, c);
	c &= ~SP_TX_RST_SW_RST;
	sp_tx_write_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL_REG, c);

	/* Range limitation for RGB input */
	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_VID_DATA_RNG_CTRL_REG, &c);
	c |= HDMI_RX_R2Y_INPUT_LIMIT;
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_VID_DATA_RNG_CTRL_REG, c);

	/* set GPIO control by HPD */
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_PIO_CTRL, 0x02);

	/* generate interrupt on any received HDMI Vendor Specific packet; */
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_PKT_RX_INDU_INT_CTRL, 0x80);

	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_CEC_CTRL_REG, 0x01);
	/* CEC SPPED to 27MHz */
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_CEC_SPEED_CTRL_REG, 0x40);
	/* Set  CEC Logic address to 15,unregistered, and enable CEC receiver */
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_CEC_CTRL_REG, 0x08);
	hdmi_rx_set_hpd(0);
	hdmi_rx_set_termination(0);
	pr_notice("HDMI Rx is initialized...");
}

static void hdmi_rx_clk_detected_int(void)
{
	unchar c;

	pr_notice("*HDMI_RX Interrupt: Pixel Clock Change.\n");
	if (sp_tx_system_state > SP_TX_CONFIG_HDMI_INPUT) {
		hdmi_rx_mute_audio();
		hdmi_rx_mute_video();
		sp_tx_video_mute(1);
		sp_tx_enable_video_input(0);
		sp_tx_enable_audio_output(0);
		sp_tx_set_system_state(SP_TX_CONFIG_HDMI_INPUT);

		if (hdmi_system_state > HDMI_CLOCK_DET)
			hdmi_rx_set_sys_state(HDMI_CLOCK_DET);
	}

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_SYS_STATUS_REG, &c);

	if (c & HDMI_RX_CLK_DET) {
		pr_err("Pixel clock existed. \n");

		if (hdmi_system_state == HDMI_CLOCK_DET)
			hdmi_rx_set_sys_state(HDMI_SYNC_DET);
	} else {
		if (hdmi_system_state > HDMI_CLOCK_DET)
			hdmi_rx_set_sys_state(HDMI_CLOCK_DET);
		pr_err("Pixel clock lost. \n");
		g_Sync_Expire_Cntr = 0;
	}
}

static void hdmi_rx_sync_det_int(void)
{
	unchar c;

	pr_notice("*hdmi_rx interrupt: Sync Detect.");

	if (sp_tx_system_state > SP_TX_CONFIG_HDMI_INPUT) {
		hdmi_rx_mute_audio();
		hdmi_rx_mute_video();
		sp_tx_video_mute(1);
		sp_tx_enable_video_input(0);
		sp_tx_enable_audio_output(0);
		sp_tx_set_system_state(SP_TX_CONFIG_HDMI_INPUT);

		if (hdmi_system_state > HDMI_SYNC_DET)
			hdmi_rx_set_sys_state(HDMI_SYNC_DET);
	}

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_SYS_STATUS_REG, &c);
	if (c & HDMI_RX_HDMI_DET) {
		pr_notice("Sync found.");

		if (hdmi_system_state == HDMI_SYNC_DET)
			hdmi_rx_set_sys_state(HDMI_VIDEO_CONFIG);

		g_Video_Stable_Cntr = 0;
		hdmi_rx_get_video_info();
	} else {
		pr_err("Sync lost.");

		if ((c & 0x02) && (hdmi_system_state > HDMI_SYNC_DET))
			hdmi_rx_set_sys_state(HDMI_SYNC_DET);
		else
			hdmi_rx_set_sys_state(HDMI_CLOCK_DET);
	}
}

static void hdmi_rx_hdmi_dvi_int(void)
{
	unchar c;

	pr_notice("*HDMI_RX Interrupt: HDMI-DVI Mode Change.");
	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_HDMI_STATUS_REG, &c);
	hdmi_rx_get_video_info();

	if ((c & HDMI_RX_HDMI_MODE) == HDMI_RX_HDMI_MODE) {
		pr_notice("HDMI_RX_HDMI_DVI_Int: HDMI MODE.");

		if (hdmi_system_state == HDMI_PLAYBACK)
			hdmi_rx_set_sys_state(HDMI_AUDIO_CONFIG);
	} else {
		hdmi_rx_unmute_audio();
	}
}

static void hdmi_rx_av_mute_int(void)
{
	unchar AVMUTE_STATUS, c;

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_HDMI_STATUS_REG,
		       &AVMUTE_STATUS);
	if (AVMUTE_STATUS & HDMI_RX_MUTE_STAT) {
		pr_notice("HDMI_RX AV mute packet received.");

		if (!g_Video_Muted)
			hdmi_rx_mute_video();

		if (!g_Audio_Muted)
			hdmi_rx_mute_audio();

		c = AVMUTE_STATUS & (~HDMI_RX_MUTE_STAT);
		sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_HDMI_STATUS_REG, c);
	}
}

static void hdmi_rx_cts_rcv_int(void)
{
	unchar c;
	g_CTS_Got = 1;
	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_SYS_STATUS_REG, &c);

	if ((hdmi_system_state == HDMI_AUDIO_CONFIG) && (c & HDMI_RX_HDMI_DET)) {
		if (g_CTS_Got && g_Audio_Got) {
			if (g_Audio_Stable_Cntr >= AUDIO_STABLE_TH) {
				hdmi_rx_unmute_audio();
				hdmi_rx_unmute_video();
				g_Audio_Stable_Cntr = 0;
				hdmi_rx_show_audio_info();
				hdmi_rx_set_sys_state(HDMI_PLAYBACK);
				sp_tx_config_audio();
			} else
				g_Audio_Stable_Cntr++;
		} else
			g_Audio_Stable_Cntr = 0;
	}
}

static void hdmi_rx_audio_rcv_int(void)
{
	unchar c;
	g_Audio_Got = 1;

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_SYS_STATUS_REG, &c);

	if ((hdmi_system_state == HDMI_AUDIO_CONFIG) && (c & HDMI_RX_HDMI_DET)) {
		if (g_CTS_Got && g_Audio_Got) {
			if (g_Audio_Stable_Cntr >= AUDIO_STABLE_TH) {
				hdmi_rx_unmute_audio();
				hdmi_rx_unmute_video();
				g_Audio_Stable_Cntr = 0;
				hdmi_rx_show_audio_info();
				hdmi_rx_set_sys_state(HDMI_PLAYBACK);
				sp_tx_config_audio();
			} else
				g_Audio_Stable_Cntr++;
		} else
			g_Audio_Stable_Cntr = 0;
	}
}

static void hdmi_rx_hdcp_error_int(void)
{
	g_Audio_Got = 0;
	g_CTS_Got = 0;

	if (g_HDCP_Err_Cnt >= 40) {
		g_HDCP_Err_Cnt = 0;
		pr_err("Lots of hdcp error occured ...");
		hdmi_rx_mute_audio();
		hdmi_rx_mute_video();

		/* issue hotplug */
		hdmi_rx_set_hpd(0);
		mdelay(100);
		hdmi_rx_set_hpd(1);

	} else if ((hdmi_system_state == HDMI_CLOCK_DET)
		   || (hdmi_system_state == HDMI_SYNC_DET)) {
		g_HDCP_Err_Cnt = 0;
	} else {
		g_HDCP_Err_Cnt++;
	}
}

static void hdmi_rx_new_avi_int(void)
{
	pr_notice("*HDMI_RX Interrupt: New AVI Packet.");
	sp_tx_avi_setup();
	sp_tx_config_packets(AVI_PACKETS);
}

static void hdmi_rx_new_vsi_int(void)
{
	/* sp_tx_set_3d_packets(); */
	g_VSI_Got = 1;
	/* pr_notice("New VSI packet is received."); */
}

static void hdmi_rx_no_vsi_int(void)
{
	/* unchar c; */
	/* sp_tx_write_reg(SP_TX_PORT0_ADDR, 0xea, 0x00);//enable VSC */
}

void sp_tx_config_hdmi_input(void)
{
	unchar c;
	unchar AVMUTE_STATUS, SYS_STATUS;

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_SYS_STATUS_REG, &SYS_STATUS);
	if ((SYS_STATUS & HDMI_RX_CLK_DET)
	    && (hdmi_system_state == HDMI_CLOCK_DET))
		hdmi_rx_set_sys_state(HDMI_SYNC_DET);

	if (hdmi_system_state == HDMI_SYNC_DET) {
		sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_SYS_STATUS_REG, &c);

		if (!(c & HDMI_RX_HDMI_DET)) {
			if (g_Sync_Expire_Cntr >= SCDT_EXPIRE_TH) {
				pr_err("No sync for long time.");
				/* misc reset */
				sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_TMDS_CTRL_REG18, &c);
				c |= 0x10;
				sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_TMDS_CTRL_REG18, c);
				mdelay(10);
				sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_TMDS_CTRL_REG18, &c);
				c &= ~0x10;
				sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_TMDS_CTRL_REG18, c);
				hdmi_rx_set_sys_state(HDMI_CLOCK_DET);
				g_Sync_Expire_Cntr = 0;
			} else
				g_Sync_Expire_Cntr++;

			return;
		} else {
			g_Sync_Expire_Cntr = 0;
			hdmi_rx_set_sys_state(HDMI_VIDEO_CONFIG);
		}
	}

	if (hdmi_system_state < HDMI_VIDEO_CONFIG)
		return;

	if (hdmi_rx_is_video_change()) {
		pr_err("Video Changed , mute video and mute audio");
		g_Video_Stable_Cntr = 0;

		if (!g_Video_Muted)
			hdmi_rx_mute_video();

		if (!g_Audio_Muted)
			hdmi_rx_mute_audio();

	} else if (g_Video_Stable_Cntr < VIDEO_STABLE_TH) {
		g_Video_Stable_Cntr++;
		pr_notice("WAIT_VIDEO: Wait for video stable cntr.");
	} else if (hdmi_system_state == HDMI_VIDEO_CONFIG) {
		sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_HDMI_STATUS_REG,
			       &AVMUTE_STATUS);

		if (!(AVMUTE_STATUS & HDMI_RX_MUTE_STAT)) {
			hdmi_rx_get_video_info();
			hdmi_rx_unmute_video();
			sp_tx_lvttl_bit_mapping();
			sp_tx_set_system_state(SP_TX_LINK_TRAINING);
			hdmi_rx_show_video_info();
			sp_tx_power_down(SP_TX_PWR_AUDIO);

			if (g_HDMI_DVI_Status) {
				pr_notice("HDMI mode: Video is stable.");
				sp_tx_send_message(MSG_INPUT_HDMI);
				hdmi_rx_set_sys_state(HDMI_AUDIO_CONFIG);
			} else {
				pr_notice("DVI mode: Video is stable.");
				sp_tx_send_message(MSG_INPUT_DVI);
				hdmi_rx_unmute_audio();
				hdmi_rx_set_sys_state(HDMI_PLAYBACK);
			}
		}
	}

	hdmi_rx_get_video_info();
}

void hdmi_rx_int_irq_handler(void)
{
	unchar c1, c2, c3, c4, c5, c6, c7;

	if ((hdmi_system_state < HDMI_CLOCK_DET)
	    || (sp_tx_system_state < SP_TX_CONFIG_HDMI_INPUT))
		return;

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_INT_STATUS1_REG, &c1);
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_INT_STATUS1_REG, c1);

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_INT_STATUS2_REG, &c2);
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_INT_STATUS2_REG, c2);

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_INT_STATUS3_REG, &c3);
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_INT_STATUS3_REG, c3);

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_INT_STATUS4_REG, &c4);
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_INT_STATUS4_REG, c4);

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_INT_STATUS5_REG, &c5);
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_INT_STATUS5_REG, c5);

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_INT_STATUS6_REG, &c6);
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_INT_STATUS6_REG, c6);

	sp_tx_read_reg(HDMI_PORT0_ADDR, HDMI_RX_INT_STATUS7_REG, &c7);
	sp_tx_write_reg(HDMI_PORT0_ADDR, HDMI_RX_INT_STATUS7_REG, c7);

	if (c1 & HDMI_RX_CKDT_CHANGE)
		hdmi_rx_clk_detected_int();

	if (c1 & HDMI_RX_SCDT_CHANGE)
		hdmi_rx_sync_det_int();

	if (c1 & HDMI_RX_HDMI_DVI)
		hdmi_rx_hdmi_dvi_int();

	if (c1 & HDMI_RX_SET_MUTE)
		hdmi_rx_av_mute_int();

	if (c6 & HDMI_RX_NEW_AVI)
		hdmi_rx_new_avi_int();

	if (c7 & HDMI_RX_NEW_VS)
		hdmi_rx_new_vsi_int();

	if (c7 & HDMI_RX_NO_VSI)
		hdmi_rx_no_vsi_int();

	if ((c6 & HDMI_RX_NEW_AUD) || (c3 & HDMI_RX_AUD_MODE_CHANGE))
		hdmi_rx_restart_audio_chk();

	if (c6 & HDMI_RX_CTS_RCV)
		hdmi_rx_cts_rcv_int();

	if (c5 & HDMI_RX_AUDIO_RCV)
		hdmi_rx_audio_rcv_int();

	if (c2 & HDMI_RX_HDCP_ERR)
		hdmi_rx_hdcp_error_int();

}

MODULE_DESCRIPTION("Slimport transmitter ANX7808 driver");
MODULE_AUTHOR("FeiWang <fwang@analogixsemi.com>");
MODULE_LICENSE("GPL");
