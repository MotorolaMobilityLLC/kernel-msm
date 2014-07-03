/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2011 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  Contact Information:

  Intel Corporation
  2200 Mission College Blvd.
  Santa Clara, CA  95054

  BSD LICENSE

  Copyright(c) 2011 Intel Corporation. All rights reserved.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include <linux/delay.h>
#include "otm_hdmi.h"
#include "ipil_hdmi.h"
#include "ipil_utils.h"
#include "ipil_internal.h"
#include "ips_hdmi.h"

/* TMDS clocks in KHz*/
#define IPS_MIN_PIXEL_CLOCK 25174	/* 640x480@59.94Hz */
#define IPS_MAX_PIXEL_CLOCK 148500	/* 1920x1080@60Hz */

/* Preferred mode if none is indicated in EDID */
#define IPS_PREFERRED_HDISPLAY 1920
#define IPS_PREFERRED_VDISPLAY 1080
#define IPS_PREFERRED_REFRESH_RATE 60

struct data_rate_divider_selector_list_t {
	uint32_t target_data_rate;
	int m1;
	int m2;
	int n;
	int p1;
	int p2;
};
static struct data_rate_divider_selector_list_t
	data_rate_divider_selector_list[] = {
			{25175, 2, 118, 1, 3, 12},
			{25200, 2, 105, 1, 2, 16},
			{26000, 2, 122, 1, 3, 12},
			{27000, 3, 75, 1, 2, 16},
			{27027, 3, 75, 1, 2, 16},
			{27500, 2, 129, 1, 3, 12},
			{28000, 2, 153, 1, 3, 14},
			{28320, 2, 118, 1, 2, 16},
			{28322, 2, 118, 1, 2, 16},
			{29000, 2, 151, 1, 2, 20},
			{30000, 2, 125, 1, 2, 16},
			{31000, 2, 109, 1, 3, 9},
			{31500, 2, 123, 1, 3, 10},
			{32000, 2, 150, 1, 3, 12},
			{32500, 3, 79, 1, 2, 14},
			{33000, 2, 116, 1, 3, 9},
			{33750, 2, 123, 1, 2, 14},
			{33784, 2, 132, 1, 3, 10},
			{34000, 2, 124, 1, 2, 14},
			{34870, 3, 71, 1, 3, 8},
			{35000, 2, 123, 1, 3, 9},
			{35500, 2, 111, 1, 3, 8},
			{36000, 2, 150, 1, 2, 16},
			{37000, 2, 130,1, 3, 9},
			{38000, 3, 99, 1, 3, 10},
			{39000, 2, 137, 1, 3, 9},
			{40000, 2, 125, 1, 3, 8},
			{40500, 3, 95, 1, 3, 9},
			{40541, 3, 95, 1, 3, 9},
			{41000, 2, 144, 1, 3, 9},
			{41540, 2, 146, 1, 3, 9},
			{42000, 2, 153, 1, 2, 14},
			{43000, 2, 112, 1, 2, 10},
			{43163, 2, 118, 1, 3, 7},
			{44000, 3, 103, 1, 3, 9},
			{44900, 2, 117, 1, 2, 10},
			{45000, 2, 123, 1, 3, 7},
			{46000, 2, 144, 1, 3, 8},
			{47000, 2, 147, 1, 3, 8},
			{48000, 2, 150, 1, 3, 8},
			{49000, 2, 134, 1, 3, 7},
			{49500, 2, 116, 1, 3, 6},
			{50000, 2, 156, 1, 3, 8},
			{51000, 3, 93, 1, 3, 7},
			{52000, 2, 122, 1, 3, 6},
			{52406, 3, 91, 1, 2, 10},
			{53000, 2, 138, 1, 2, 10},
			{54000, 3, 75, 1, 2, 8},
			{54054, 3, 75, 1, 2, 8},
			{55000, 2, 129, 1, 3, 6},
			{56000, 2, 153, 1, 3, 7},
			{56250, 2, 154, 1, 3, 7},
			{57000, 3, 99, 1, 2, 10},
			{58000, 2, 151, 1, 2, 10},
			{59000, 2, 123, 1, 2, 8},
			{60000, 2, 125, 1, 2, 8},
			{61000, 2, 143, 1, 3, 6},
			{62000, 2, 113, 1, 2, 7},
			{63000, 2, 123, 1, 3, 5},
			{64000, 2, 150, 1, 3, 6},
			{65000, 3, 79, 1, 2, 7},
			{66000, 2, 129, 1, 3, 5},
			{66667, 3, 81, 1, 2, 7},
			{67000, 3, 93, 1, 2, 8},
			{67500, 2, 123, 1, 2, 7},
			{67567, 2, 132, 1, 3, 5},
			{68000, 2, 124, 1, 2, 7},
			{68075, 2, 133, 1, 3, 5},
			{68179, 3, 71, 1, 3, 4},
			{69000, 2, 135, 1, 3, 5},
			{70000, 3, 85, 1, 2, 7},
			{71000, 2, 111, 1, 3, 4},
			{72000, 2, 150, 1, 2, 8},
			{73000, 3, 95, 1, 3, 5},
			{74000, 2, 135, 1, 2, 7},
			{74175, 3, 103, 1, 2, 8},
			{74250, 2, 145, 1, 3, 5},
			{74481, 3, 97, 1, 3, 5},
			{75000, 2, 117, 1, 3, 4},
			{76000, 3, 99, 1, 3, 5},
			{77000, 2, 150, 1, 3, 5},
			{78000, 2, 122, 1, 3, 4},
			{78750, 2, 123, 1, 3, 4},
			{79000, 2, 144, 1, 2, 7},
			{80000, 2, 125, 1, 3, 4},
			{81000, 2, 148, 1, 2, 7},
			{81081, 2, 148, 1, 2, 7},
			{81624, 3, 85, 1, 3, 4},
			{82000, 2, 128, 1, 3, 4},
			{83000, 2, 108, 1, 2, 5},
			{83950, 2, 153, 1, 2, 7},
			{84000, 2, 153, 1, 2, 7},
			{85000, 2, 155, 1, 2, 7},
			{86000, 2, 112, 1, 2, 5},
			{87000, 2, 136, 1, 3, 4},
			{88000, 2, 138, 1, 3, 4},
			{89000, 2, 139, 1, 3, 4},
			{90000, 2, 117, 1, 2, 5},
			{91000, 3, 79, 1, 2, 5},
			{92000, 2, 144, 1, 3, 4},
			{92719, 2, 145, 1, 3, 4},
			{92812, 2, 145, 1, 3, 4},
			{93000, 2, 109, 1, 3, 3},
			{93438, 2, 146, 1, 3, 4},
			{94000, 2, 147, 1, 3, 4},
			{94500, 2, 123, 1, 2, 5},
			{95000, 3, 99, 1, 3, 4},
			{95654, 3, 83, 1, 2, 5},
			{96000, 2, 150, 1, 3, 4},
			{97000, 3, 101, 1, 3, 4},
			{98000, 2, 153, 1, 3, 4},
			{99000, 2, 116, 1, 3, 3},
			{100000, 2, 156, 1, 3, 4},
			{101000, 3, 79, 1, 3, 3},
			{102000, 2, 133, 1, 2, 5},
			{103000, 2, 134, 1, 2, 5},
			{104000, 2, 122, 1, 3, 3},
			{105000, 2, 123, 1, 3, 3},
			{106000, 2, 138, 1, 2, 5},
			{107000, 3, 93, 1, 2, 5},
			{107214, 3, 93, 1, 2, 5},
			{108000, 3, 75, 1, 2, 4},
			{109000, 2, 142, 1, 2, 5},
			{110000, 2, 129, 1, 3, 3},
			{110013, 2, 129, 1, 3, 3},
			{111000, 2, 130, 1, 3, 3},
			{111263, 3, 87, 1, 3, 3},
			{111375, 3, 87, 1, 3, 3},
			{112000, 2, 146, 1, 2, 5},
			{113000, 2, 147, 1, 2, 5},
			{113100, 2, 118, 1, 2, 4},
			{113309, 2, 118, 1, 2, 4},
			{114000, 3, 99, 1, 2, 5},
			{115000, 2, 135, 1, 3, 3},
			{116000, 2, 151, 1, 2, 5},
			{117000, 2, 137, 1, 3, 3},
			{118000, 2, 123, 1, 2, 4},
			{119000, 3, 93, 1, 3, 3},
			{119651, 3, 83, 1, 2, 4},
			{120000, 2, 125, 1, 2, 4},
			{121000, 2, 126, 1, 2, 4},
			{122000, 2, 143, 1, 3, 3},
			{122614, 3, 85, 1, 2, 4},
			{123000, 2, 144, 1, 3, 3},
			{123379, 2, 145, 1, 3, 3},
			{124000, 3, 97, 1, 3, 3},
			{125000, 2, 130, 1, 2, 4},
			{126000, 2, 131, 1, 2, 4},
			{127000, 2, 149, 1, 3, 3},
			{128000, 2, 150, 1, 3, 3},
			{129000, 2, 151, 1, 3, 3},
			{129859, 2, 152, 1, 3, 3},
			{130000, 2, 152, 1, 3, 3},
			{131000, 3, 91, 1, 2, 4},
			{131850, 3, 103, 1, 3, 3},
			{132000, 3, 103, 1, 3, 3},
			{133000, 2, 156, 1, 3, 3},
			{133330, 2, 139, 1, 2, 4},
			{134000, 3, 93, 1, 2, 4},
			{135000, 2, 141, 1, 2, 4},
			{136000, 2, 106, 1, 3, 2},
			{137000, 2, 107, 1, 3, 2},
			{138000, 2, 108, 1, 3, 2},
			{139000, 2, 145, 1, 2, 4},
			{139050, 2, 145, 1, 2, 4},
			{139054, 2, 145, 1, 2, 4},
			{140000, 3, 73, 1, 3, 2},
			{141000, 2, 147, 1, 2, 4},
			{142000, 2, 111, 1, 3, 2},
			{143000, 2, 149, 1, 2, 4},
			{143472, 2, 112, 1, 3, 2},
			{144000, 2, 150, 1, 2, 4},
			{145000, 2, 151, 1, 2, 4},
			{146000, 2, 114, 1, 3, 2},
			{147000, 2, 153, 1, 2, 4},
			{147891, 3, 77, 1, 3, 2},
			{148000, 3, 77, 1, 3, 2},
			{148352, 3, 103, 1, 2, 4},
			{148500, 2, 116, 1, 3, 2}
};

#define NUM_SELECTOR_LIST (sizeof( \
		data_rate_divider_selector_list) \
	/ sizeof(struct data_rate_divider_selector_list_t))

/* DPLL registers on IOSF */
#define PLLA_DWORD3_1   0x800C
#define PLLA_DWORD3_2   0x802C
#define PLLA_DWORD5_1   0x8014
#define PLLA_DWORD5_2   0x8034
#define PLLA_DWORD7_1   0x801C
#define PLLA_DWORD7_2   0x803C
#define PLLB_DWORD8     0x8040
#define PLLB_DWORD10_1  0x8048
#define PLLB_DWORD10_2  0x8068
#define CMN_DWORD3      0x810C
#define CMN_DWORD8      0x8100
#define REF_DWORD18     0x80C0
#define REF_DWORD22     0x80D0
#define DPLL_CML_CLK1   0x8238
#define DPLL_CML_CLK2   0x825C
#define DPLL_LRC_CLK    0x824C
#define DPLL_Tx_GRC     0x8244
#define PCS_DWORD12_1   0x0230
#define PCS_DWORD12_2   0x0430
#define TX_SWINGS_1     0x8294
#define TX_SWINGS_2     0x8290
#define TX_SWINGS_3     0x8288
#define TX_SWINGS_4     0x828C
#define TX_SWINGS_5     0x0690
#define TX_SWINGS_6     0x822C
#define TX_SWINGS_7     0x8224
#define TX_GROUP_1      0x82AC
#define TX_GROUP_2      0x82B8

#define DPLL_IOSF_EP 0x13

/**
 * Description: Write to DPLL register via IOSF
 *
 * @ep_id:	IOSF endpoint ID (0x13 for DPLL)
 * @reg:        address of register
 * @val:        value to write to register
 *
 * Returns:	none
 */
void gunit_iosf_write32(u32 ep_id, u32 reg, u32 val)
{
	u32 ret;
	int retry = 0;
	u32 sb_pkt = (1 << 16) | (ep_id << 8) | 0xf0;

	/* Write value to side band register */
	hdmi_write32(0x2108, reg);
	hdmi_write32(0x2104, val);
	hdmi_write32(0x2100, sb_pkt);

	/* Check if transaction is complete */
	ret = hdmi_read32(0x210C);
	while ((retry++ < 0x1000) && (ret != 0x2)) {
		usleep_range(500, 1000);
		ret = hdmi_read32(0x210C);
	}

	if (ret != 2)
		pr_err("%s: failed to program DPLL\n", __func__);
}

/**
 * Description: Read DPLL register via IOSF
 *
 * @ep_id:	IOSF endpoint ID (0x13 for DPLL)
 * @reg:        address of register
 *
 * Returns:	value of register
 */
u32 gunit_iosf_read32(u32 ep_id, u32 reg)
{
	u32 ret;
	int retry = 0;
	u32 sb_pkt = (0 << 16) | (ep_id << 8) | 0xf0;

	/* Read side band register */
	hdmi_write32(0x2108, reg);
	hdmi_write32(0x2100, sb_pkt);

	/* Check if transaction is complete */
	ret = hdmi_read32(0x210C);
	while ((retry++ < 0x1000) && (ret != 2)) {
		usleep_range(500, 1000);
		ret = hdmi_read32(0x210C);
	}

	if (ret != 2)
		pr_err("%s: Failed to read\n", __func__);
	else
		ret = hdmi_read32(0x2104);

	return ret;
}

/**
 * Description: Find the m, n and p for DPLL.
 *              Use the nominal pixel clock as TMDS clock.
 *
 * @dclk:	refresh rate dot clock in kHz of current mode
 * @real_clk:   nominal dot clock used as TMDS dot clock. Note it
 *              has a small difference from real HW clock.
 * @m1, m2:     DPLL m values
 * @n:          DPLL n value
 * @p1, p2:     DPLL p values
 *
 * Returns:	true on success
 *		false on NULL input arguments
 */
static bool __ips_hdmi_get_divider_selector(
			uint32_t dclk,
			uint32_t *real_dclk,
			int *m1, int *m2,
			int *n, int *p1, int *p2)
{
	int i, matched_idx = 0;
	uint32_t min_diff = 0xffffffff, curr_diff;

	if (dclk > data_rate_divider_selector_list[NUM_SELECTOR_LIST - 1].target_data_rate ||
			dclk < data_rate_divider_selector_list[0].target_data_rate ) {
		pr_err("Could not find supported mode\n");
		return false;
	}

	for (i = 0; i < NUM_SELECTOR_LIST; i++) {
		curr_diff = abs(dclk - data_rate_divider_selector_list[i].target_data_rate);
		if (min_diff > curr_diff) {
			min_diff = curr_diff;
			matched_idx = i;
		}
	}
	*m1 = data_rate_divider_selector_list[matched_idx].m1;
	*m2 = data_rate_divider_selector_list[matched_idx].m2;
	*n = data_rate_divider_selector_list[matched_idx].n;
	*p1 = data_rate_divider_selector_list[matched_idx].p1;
	*p2 = data_rate_divider_selector_list[matched_idx].p2;
	*real_dclk = data_rate_divider_selector_list[matched_idx].target_data_rate;
	pr_debug("dclk: %d, real_dclk: %d", dclk,  *real_dclk);
	return true;
}

/**
 * Description: programs dpll clocks, enables dpll and waits
 *		till it locks with DSI PLL
 *
 * @m1, m2:     DPLL m values
 * @n:          DPLL n value
 * @p1, p2:     DPLL p values
 *
 * Returns:	none
*/
static void __ips_hdmi_set_program_dpll(int n, int p1, int p2, int m1, int m2)
{
	u32 ret, tmp;
	int retry = 0;
	u32 div = (0x11 << 24) | (p1 << 21) | (p2 << 16) | (n << 12) |
		  (0x1 << 11)  | (m1 << 8)  | (m2);

	pr_debug("enter %s\n", __func__);

	/* Common reset */
	hdmi_write32(IPS_DPLL_B, 0x70006800);

	/* Program DPLL registers via IOSF (TNG display HAS) */

	/* Process monitor to 19.2MHz */
	gunit_iosf_write32(DPLL_IOSF_EP, REF_DWORD22, 0x19080000);

	/* LRC clock to 19.2MHz */
	gunit_iosf_write32(DPLL_IOSF_EP, DPLL_LRC_CLK, 0x00000F10);

	/* Disable periodic GRC IREF update for DPLL */
	tmp = gunit_iosf_read32(DPLL_IOSF_EP, PLLB_DWORD8);
	gunit_iosf_write32(DPLL_IOSF_EP, PLLB_DWORD8, tmp & 0x00FFFFFF);

	/* Enable Tx for periodic GRC update*/
	gunit_iosf_write32(DPLL_IOSF_EP, DPLL_Tx_GRC, 0x00004000);

	/* GRC cal clock set to 19.2MHZ */
	gunit_iosf_write32(DPLL_IOSF_EP, REF_DWORD18, 0x30002400);

	/* Set lock time to 53us.
	 * Disable fast lock.
	 */
	gunit_iosf_write32(DPLL_IOSF_EP, CMN_DWORD8, 0x0);

	/* Stagger Programming */
	gunit_iosf_write32(DPLL_IOSF_EP, TX_GROUP_1, 0x00001500);
	gunit_iosf_write32(DPLL_IOSF_EP, TX_GROUP_2, 0x40400000);
	gunit_iosf_write32(DPLL_IOSF_EP, PCS_DWORD12_1, 0x00220F00);
	gunit_iosf_write32(DPLL_IOSF_EP, PCS_DWORD12_2, 0x00750F00);

	/* Set divisors*/
	gunit_iosf_write32(DPLL_IOSF_EP, PLLA_DWORD3_1, div);
	gunit_iosf_write32(DPLL_IOSF_EP, PLLA_DWORD3_2, div);

	/* Set up LCPLL in digital mode */
	gunit_iosf_write32(DPLL_IOSF_EP, PLLA_DWORD5_1, 0x0DF44300);
	gunit_iosf_write32(DPLL_IOSF_EP, PLLA_DWORD5_2, 0x0DF44300);

	/* LPF co-efficients for LCPLL in digital mode */
	gunit_iosf_write32(DPLL_IOSF_EP, PLLB_DWORD10_1, 0x005F0021);
	gunit_iosf_write32(DPLL_IOSF_EP, PLLB_DWORD10_2, 0x005F0021);

	/* Disable unused TLine clocks on right side */
	gunit_iosf_write32(DPLL_IOSF_EP, CMN_DWORD3, 0x14540000);

	/* Enable DPLL */
	tmp = hdmi_read32(IPS_DPLL_B);
	hdmi_write32(IPS_DPLL_B, tmp | IPIL_DPLL_VCO_ENABLE);

	/* Enable DCLP to core */
	tmp = gunit_iosf_read32(DPLL_IOSF_EP, PLLA_DWORD7_1);
	gunit_iosf_write32(DPLL_IOSF_EP, PLLA_DWORD7_1, tmp | (1 << 24));
	tmp = gunit_iosf_read32(DPLL_IOSF_EP, PLLA_DWORD7_2);
	gunit_iosf_write32(DPLL_IOSF_EP, PLLA_DWORD7_2, tmp | (1 << 24));

	/* Set HDMI lane CML clock */
	gunit_iosf_write32(DPLL_IOSF_EP, DPLL_CML_CLK1, 0x07760018);
	gunit_iosf_write32(DPLL_IOSF_EP, DPLL_CML_CLK2, 0x00400888);

	/* Swing settings */
	gunit_iosf_write32(DPLL_IOSF_EP, TX_SWINGS_1, 0x00000000);
	gunit_iosf_write32(DPLL_IOSF_EP, TX_SWINGS_2, 0x2B245555);
	gunit_iosf_write32(DPLL_IOSF_EP, TX_SWINGS_3, 0x5578B83A);
	gunit_iosf_write32(DPLL_IOSF_EP, TX_SWINGS_4, 0x0C782040);
	//gunit_iosf_write32(DPLL_IOSF_EP, TX_SWINGS_5, 0x2B247878);
	gunit_iosf_write32(DPLL_IOSF_EP, TX_SWINGS_6, 0x00030000);
	gunit_iosf_write32(DPLL_IOSF_EP, TX_SWINGS_7, 0x00004000);
	gunit_iosf_write32(DPLL_IOSF_EP, TX_SWINGS_1, 0x80000000);

	/* Wait until DPLL is locked */
	ret = hdmi_read32(IPS_DPLL_B);
	ret &= 0x8000;
	while ((retry++ < 1000) && (ret != 0x8000)) {
		usleep_range(500, 1000);
		ret = hdmi_read32(IPS_DPLL_B);
		ret &= 0x8000;
	}

	if (ret != 0x8000) {
		pr_err("%s: DPLL failed to lock, exit...\n", __func__);
		return;
	}
}

/**
 * Description: programs dpll clocks, enables dpll and waits
 *		till it locks with DSI PLL
 *
 * @dev:	hdmi_device_t
 * @dclk:	refresh rate dot clock in kHz of current mode
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments
 */
otm_hdmi_ret_t	ips_hdmi_crtc_mode_set_program_dpll(hdmi_device_t *dev,
							unsigned long dclk)
{
	int n, p1, p2, m1, m2;
	uint32_t target_dclk;

	pr_debug("enter %s\n", __func__);

	if (__ips_hdmi_get_divider_selector(dclk,
			&target_dclk, &m1, &m2, &n, &p1, &p2)) {
		__ips_hdmi_set_program_dpll(n, p1, p2, m1, m2);
		dev->clock_khz = target_dclk;
		return OTM_HDMI_SUCCESS;
	} else
		return OTM_HDMI_ERR_INVAL;
}

/**
 * Description: get pixel clock range
 *
 * @pc_min:	minimum pixel clock
 * @pc_max:	maximum pixel clock
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_FAILED on NULL input arguments.
 */
otm_hdmi_ret_t ips_get_pixel_clock_range(unsigned int *pc_min,
					unsigned int *pc_max)
{
	if (!pc_min || !pc_max)
		return OTM_HDMI_ERR_FAILED;

	*pc_min = IPS_MIN_PIXEL_CLOCK;
	*pc_max = IPS_MAX_PIXEL_CLOCK;
	return OTM_HDMI_SUCCESS;
}

/**
 * Returns if the given values is preferred mode or not
 * @hdisplay	: width
 * @vdisplay	: height
 * @refresh	: refresh rate
 *
 * Returns true if preferred mode else false
 */
bool ips_hdmi_is_preferred_mode(int hdisplay, int vdisplay, int refresh)
{
	if (hdisplay == IPS_PREFERRED_HDISPLAY &&
		vdisplay == IPS_PREFERRED_VDISPLAY &&
		refresh == IPS_PREFERRED_REFRESH_RATE)
		return true;
	else
		return false;
}

/**
 * Description: restore HDMI display registers and enable display
 *
 * @dev:	hdmi_device_t
 *
 * Returns:	none
 */
void ips_hdmi_restore_and_enable_display(hdmi_device_t *dev)
{

	int i;

	if (NULL == dev)  {
		pr_debug("\n%s invalid argument\n", __func__);
		return;
	}
	if (dev->reg_state.valid == false) {
		pr_debug("\nhdmi no data to restore\n");
		return;
	}

	/*make sure VGA plane is off. it initializes to on after reset!*/
	hdmi_write32(IPIL_VGACNTRL, IPIL_VGA_DISP_DISABLE);

	ips_hdmi_crtc_mode_set_program_dpll(dev, dev->clock_khz);

	/* Restore mode */
	hdmi_write32(IPS_HTOTAL_B, dev->reg_state.saveHTOTAL_B);
	hdmi_write32(IPS_HBLANK_B, dev->reg_state.saveHBLANK_B);
	hdmi_write32(IPS_HSYNC_B, dev->reg_state.saveHSYNC_B);
	hdmi_write32(IPS_VTOTAL_B, dev->reg_state.saveVTOTAL_B);
	hdmi_write32(IPS_VBLANK_B, dev->reg_state.saveVBLANK_B);
	hdmi_write32(IPS_VSYNC_B, dev->reg_state.saveVSYNC_B);
	hdmi_write32(IPS_PIPEBSRC, dev->reg_state.savePIPEBSRC);
	hdmi_write32(IPS_DSPBSTAT, dev->reg_state.saveDSPBSTATUS);

	/*set up the plane*/
	hdmi_write32(IPS_DSPBSTRIDE, dev->reg_state.saveDSPBSTRIDE);
	hdmi_write32(IPS_DSPBLINOFF, dev->reg_state.saveDSPBLINOFF);
	hdmi_write32(IPS_DSPBTILEOFF, dev->reg_state.saveDSPBTILEOFF);
	hdmi_write32(IPS_DSPBSIZE, dev->reg_state.saveDSPBSIZE);
	hdmi_write32(IPS_DSPBPOS, dev->reg_state.saveDSPBPOS);
	hdmi_write32(IPS_DSPBSURF, dev->reg_state.saveDSPBSURF);

	hdmi_write32(IPS_PFIT_CONTROL, dev->reg_state.savePFIT_CONTROL);
	hdmi_write32(IPS_PFIT_PGM_RATIOS, dev->reg_state.savePFIT_PGM_RATIOS);
	hdmi_write32(IPS_HDMIPHYMISCCTL, dev->reg_state.saveHDMIPHYMISCCTL);
	hdmi_write32(IPS_HDMIB_CONTROL, dev->reg_state.saveHDMIB_CONTROL);

	/*enable the plane*/
	hdmi_write32(IPS_DSPBCNTR, dev->reg_state.saveDSPBCNTR);
	hdmi_write32(IPS_HDMIB_LANES02, dev->reg_state.saveHDMIB_DATALANES);
	hdmi_write32(IPS_HDMIB_LANES3, dev->reg_state.saveHDMIB_DATALANES);

	/*enable the pipe */
	hdmi_write32(IPS_PIPEBCONF, dev->reg_state.savePIPEBCONF);

	/* restore palette (gamma) */
	for (i = 0; i < 256; i++)
		hdmi_write32(IPS_PALETTE_B + (i<<2),
				dev->reg_state.save_palette_b[i]);

	dev->reg_state.valid = false;
}
