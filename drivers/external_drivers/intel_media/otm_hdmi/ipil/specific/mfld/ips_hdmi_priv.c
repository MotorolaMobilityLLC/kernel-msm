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


#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/types.h>

#include "otm_hdmi.h"
#include "hdmi_internal.h"
#include "ips_hdmi.h"
#include "mfld_hdmi_reg.h"
#include "ipil_internal.h"
#include "ipil_utils.h"

#define IPS_MIN_PIXEL_CLOCK 25174	/* 640x480@59.94Hz */
#define IPS_MAX_PIXEL_CLOCK 74250	/* 1920x1080@30Hz */
#define IPS_PREFERRED_HDISPLAY 1920
#define IPS_PREFERRED_VDISPLAY 1080
#define IPS_PREFERRED_REFRESH_RATE 30

/* Clock Related Definitions
 * Min/Max value based on DPLL parameter interface table
 * from Penwell Display HAS
 */
#define IPS_DOT_MIN		19750
#define IPS_DOT_MAX		120000

#define IPS_DPLL_M_MIN_19	105
#define IPS_DPLL_M_MAX_19	197
#define IPS_DPLL_P1_MIN_19	2
#define IPS_DPLL_P1_MAX_19	10
#define IPS_LIMIT_DPLL_19	0
#define IPS_VCO_SEL		(1 << 16)

#define IPS_M_MIN		21

static const struct ips_clock_limits_t ips_clock_limits[] = {
	{	/* CRYSTAL_19 */
	 .dot = {.min = IPS_DOT_MIN, .max = IPS_DOT_MAX},
	 .m = {.min = IPS_DPLL_M_MIN_19, .max = IPS_DPLL_M_MAX_19},
	 .p1 = {.min = IPS_DPLL_P1_MIN_19, .max = IPS_DPLL_P1_MAX_19},
	 },
};

static const u32 ips_m_converts[] = {
/* M configuration table from 9-bit LFSR table */
	224, 368, 440, 220, 366, 439, 219, 365, 182, 347, /* 21 - 30 */
	173, 342, 171, 85, 298, 149, 74, 37, 18, 265,   /* 31 - 40 */
	388, 194, 353, 432, 216, 108, 310, 155, 333, 166, /* 41 - 50 */
	83, 41, 276, 138, 325, 162, 337, 168, 340, 170, /* 51 - 60 */
	341, 426, 469, 234, 373, 442, 221, 110, 311, 411, /* 61 - 70 */
	461, 486, 243, 377, 188, 350, 175, 343, 427, 213, /* 71 - 80 */
	106, 53, 282, 397, 354, 227, 113, 56, 284, 142, /* 81 - 90 */
	71, 35, 273, 136, 324, 418, 465, 488, 500, 506, /* 91 - 100 */
	253, 126, 63, 287, 399, 455, 483, 241, 376, 444, /* 101 - 110 */
	478, 495, 503, 251, 381, 446, 479, 239, 375, 443, /* 111 - 120 */
	477, 238, 119, 315, 157, 78, 295, 147, 329, 420, /* 121 - 130 */
	210, 105, 308, 154, 77, 38, 275, 137, 68, 290, /* 131 - 140 */
	145, 328, 164, 82, 297, 404, 458, 485, 498, 249, /* 141 - 150 */
	380, 190, 351, 431, 471, 235, 117, 314, 413, 206, /* 151 - 160 */
	103, 51, 25, 12, 262, 387, 193, 96, 48, 280, /* 161 - 170 */
	396, 198, 99, 305, 152, 76, 294, 403, 457, 228, /* 171 - 180 */
	114, 313, 156, 334, 423, 467, 489, 244, 378, 445, /*181 - 190 */
	222, 367, 183, 91, 45, 22, 11, 261, 130, 321, /* 191 - 200 */
};

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

/*
 * Derive the pixel clock for the given refclk and
 * divisors for 8xx chips.
 */
static void __ips_hdmi_derive_dot_clock(int refclk, struct ips_clock_t *clock)
{
	clock->dot = (refclk * clock->m) / clock->p1;
}

static const struct ips_clock_limits_t *__ips_hdmi_clk_limits(void)
{
	const struct ips_clock_limits_t *limit = NULL;

	/*
	 * CRYSTAL_19 is enabled for medfield.
	 * Expand this logic for other types.
	 */
	limit = &ips_clock_limits[IPS_LIMIT_DPLL_19];
	return limit;
}

static bool __ips_hdmi_find_bestPll(int target, int refclk,
					struct ips_clock_t *best_clock)
{
	struct ips_clock_t clock;
	const struct ips_clock_limits_t *limit = __ips_hdmi_clk_limits();
	int err = target;

	memset(best_clock, 0, sizeof(*best_clock));
	for (clock.m = limit->m.min; clock.m <= limit->m.max; clock.m++) {
		for (clock.p1 = limit->p1.min; clock.p1 <= limit->p1.max;
		     clock.p1++) {
			int this_err;

			__ips_hdmi_derive_dot_clock(refclk, &clock);

			this_err = abs(clock.dot - target);
			if (this_err < err) {
				*best_clock = clock;
				err = this_err;
			}
		}
	}
	return err != target;
}

/**
 * Description: gets the best dpll clock value based on
 *		current timing mode clock.
 *
 * @clk:	refresh rate dot clock in kHz of current mode
 * @pdpll, pfp:	will be set to adjusted dpll values.
 * @pclock_khz:	tmds clk value for the best pll and is needed for audio.
 *		This field has to be moved into OTM audio
 *		interfaces when implemented
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments.
 */
otm_hdmi_ret_t __ips_hdmi_get_adjusted_clk(unsigned long clk,
					u32 *pdpll, u32 *pfp,
					uint32_t *pclock_khz)
{
	int refclk;
	int clk_n;
	int clk_p2;
	int clk_byte = 1;
	int m_conv = 0;
	int clk_tmp;
	u32 dpll, fp;
	bool ret;
	struct ips_clock_t clock;

	/* NULL checks */
	if (pdpll == NULL || pfp == NULL || pclock_khz == NULL) {
		pr_debug("\ninvalid argument\n");
		return OTM_HDMI_ERR_INVAL;
	}

	/* values corresponds to CRYSTAL_19, as this is enabled on mdfld */
	refclk = 19200;
	clk_n = 1;
	clk_p2 = 10;

	clk_tmp = clk * clk_n * clk_p2 * clk_byte;
	ret = __ips_hdmi_find_bestPll(clk_tmp, refclk, &clock);
	/*
	 * TODO: tmds clk value for the best pll found and is needed for audio.
	 * This field has to be moved into OTM audio interfaces
	 * when implemented.
	 */
	*pclock_khz = clock.dot / (clk_n * clk_p2 * clk_byte);
	if (ret)
		m_conv = ips_m_converts[(clock.m - IPS_M_MIN)];

	dpll = 0;
	dpll |= IPS_VCO_SEL;
	/* compute bitmask from p1 value */
	dpll |= (1 << (clock.p1 - 2)) << 17;

	fp = (clk_n / 2) << 16;
	fp |= m_conv;

	/* update the pointers */
	*pdpll = dpll;
	*pfp = fp;

	return OTM_HDMI_SUCCESS;
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
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	u32 dpll_adj, fp;
	u32 dpll;
	int timeout = 0;

	/* NULL checks */
	if (dev == NULL) {
		pr_debug("\ninvalid argument\n");
		return OTM_HDMI_ERR_INVAL;
	}

	rc = __ips_hdmi_get_adjusted_clk(dclk, &dpll_adj, &fp, &dev->clock_khz);
	dpll = hdmi_read32(IPIL_DPLL_B);
	if (dpll & IPIL_DPLL_VCO_ENABLE) {
		dpll &= ~IPIL_DPLL_VCO_ENABLE;
		hdmi_write32(IPIL_DPLL_B, dpll);
		hdmi_read32(IPIL_DPLL_B);

		/* reset M1, N1 & P1 */
		hdmi_write32(IPIL_DPLL_DIV0, 0);
		dpll &= ~IPIL_P1_MASK;
		hdmi_write32(IPIL_DPLL_B, dpll);
	}

	/*
	 * When ungating power of DPLL, needs to wait 0.5us
	 * before enable the VCO
	 */
	if (dpll & IPIL_PWR_GATE_EN) {
		dpll &= ~IPIL_PWR_GATE_EN;
		hdmi_write32(IPIL_DPLL_B, dpll);
		udelay(1);
	}

	dpll = dpll_adj;
	hdmi_write32(IPIL_DPLL_DIV0, fp);
	hdmi_write32(IPIL_DPLL_B, dpll);
	udelay(1);

	dpll |= IPIL_DPLL_VCO_ENABLE;
	hdmi_write32(IPIL_DPLL_B, dpll);
	hdmi_read32(IPIL_DPLL_B);

	/* wait for DSI PLL to lock */
	while ((timeout < 20000) && !(hdmi_read32(IPIL_PIPEBCONF) &
					IPIL_PIPECONF_PLL_LOCK)) {
		udelay(150);
		timeout++;
	}

	return OTM_HDMI_SUCCESS;
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
	u32 dpll = 0;
	u32 dpll_val;
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

	dpll = hdmi_read32(IPS_DPLL_B);
	if (!(dpll & IPIL_DPLL_VCO_ENABLE)) {
		/**
		 * When ungating power of DPLL, needs to wait 0.5us
		 * before enable the VCO
		 */
		if (dpll & IPIL_DPLL_PWR_GATE_EN) {
			dpll &= ~IPIL_DPLL_PWR_GATE_EN;
			hdmi_write32(IPS_DPLL_B, dpll);
			udelay(1);
		}

		hdmi_write32(IPS_DPLL_DIV0, dev->reg_state.saveFPA0);

		dpll_val = dev->reg_state.saveDPLL & ~IPIL_DPLL_VCO_ENABLE;
		hdmi_write32(IPS_DPLL_B, dpll_val);
		udelay(1);

		dpll_val |= IPIL_DPLL_VCO_ENABLE;
		hdmi_write32(IPS_DPLL_B, dpll_val);
		hdmi_read32(IPS_DPLL_B);

	}

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

	if (in_atomic() || in_interrupt()) {
		/*  udelay arg must be < 20000 */
		udelay(19999);
	} else
		msleep_interruptible(20);

	/*enable the pipe */
	hdmi_write32(IPS_PIPEBCONF, dev->reg_state.savePIPEBCONF);

	/* restore palette (gamma) */
	for (i = 0; i < 256; i++)
		hdmi_write32(IPS_PALETTE_B + (i<<2),
				dev->reg_state.save_palette_b[i]);

	dev->reg_state.valid = false;
}
