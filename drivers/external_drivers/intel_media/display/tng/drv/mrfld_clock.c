/*
 * Copyright Â© 2011 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *	Jim Liu <jim.liu@intel.com>	
 */

#include <drm/drmP.h>
#include "psb_fb.h"
#include "psb_drv.h"
#include "psb_intel_drv.h"
#include "psb_intel_reg.h"
#include "psb_intel_display.h"
#include "mrfld_clock.h"
#ifdef CONFIG_SUPPORT_MIPI
#include "mdfld_dsi_output.h"
#endif
#include <asm/intel-mid.h>

#define MRFLD_LIMT_DPLL_19	    0
#define MRFLD_LIMT_DPLL_25	    1
#define MRFLD_LIMT_DPLL_83	    2
#define MRFLD_LIMT_DPLL_100	    3
#define MRFLD_LIMT_DSIPLL_19	    4
#define MRFLD_LIMT_DSIPLL_25	    5
#define MRFLD_LIMT_DSIPLL_83	    6
#define MRFLD_LIMT_DSIPLL_100	    7

#define MRFLD_DOT_MIN		  19750
#define MRFLD_DOT_MAX		  120000
#define MRFLD_DPLL_M_MIN_19	    113
#define MRFLD_DPLL_M_MAX_19	    155
#define MRFLD_DPLL_P1_MIN_19	    2
#define MRFLD_DPLL_P1_MAX_19	    10
#define MRFLD_DPLL_M_MIN_25	    101
#define MRFLD_DPLL_M_MAX_25	    130
#define MRFLD_DPLL_P1_MIN_25	    2
#define MRFLD_DPLL_P1_MAX_25	    10
#define MRFLD_DPLL_M_MIN_83	    64
#define MRFLD_DPLL_M_MAX_83	    64
#define MRFLD_DPLL_P1_MIN_83	    2
#define MRFLD_DPLL_P1_MAX_83	    2
#define MRFLD_DPLL_M_MIN_100	    64
#define MRFLD_DPLL_M_MAX_100	    64
#define MRFLD_DPLL_P1_MIN_100	    2
#define MRFLD_DPLL_P1_MAX_100	    2
#define MRFLD_DSIPLL_M_MIN_19	    131
#define MRFLD_DSIPLL_M_MAX_19	    175
#define MRFLD_DSIPLL_P1_MIN_19	    3
#define MRFLD_DSIPLL_P1_MAX_19	    8
#define MRFLD_DSIPLL_M_MIN_25	    97
#define MRFLD_DSIPLL_M_MAX_25	    140
#define MRFLD_DSIPLL_P1_MIN_25	    3
#define MRFLD_DSIPLL_P1_MAX_25	    9
#define MRFLD_DSIPLL_M_MIN_83	    33
#define MRFLD_DSIPLL_M_MAX_83	    92
#define MRFLD_DSIPLL_P1_MIN_83	    2
#define MRFLD_DSIPLL_P1_MAX_83	    3
#define MRFLD_DSIPLL_M_MIN_100	    97
#define MRFLD_DSIPLL_M_MAX_100	    140
#define MRFLD_DSIPLL_P1_MIN_100	    3
#define MRFLD_DSIPLL_P1_MAX_100	    9

static const struct mrst_limit_t mrfld_limits[] = {
	{			/* MRFLD_LIMT_DPLL_19 */
	 .dot = {.min = MRFLD_DOT_MIN,.max = MRFLD_DOT_MAX},
	 .m = {.min = MRFLD_DPLL_M_MIN_19,.max = MRFLD_DPLL_M_MAX_19},
	 .p1 = {.min = MRFLD_DPLL_P1_MIN_19,.max = MRFLD_DPLL_P1_MAX_19},
	 },
	{			/* MRFLD_LIMT_DPLL_25 */
	 .dot = {.min = MRFLD_DOT_MIN,.max = MRFLD_DOT_MAX},
	 .m = {.min = MRFLD_DPLL_M_MIN_25,.max = MRFLD_DPLL_M_MAX_25},
	 .p1 = {.min = MRFLD_DPLL_P1_MIN_25,.max = MRFLD_DPLL_P1_MAX_25},
	 },
	{			/* MRFLD_LIMT_DPLL_83 */
	 .dot = {.min = MRFLD_DOT_MIN,.max = MRFLD_DOT_MAX},
	 .m = {.min = MRFLD_DPLL_M_MIN_83,.max = MRFLD_DPLL_M_MAX_83},
	 .p1 = {.min = MRFLD_DPLL_P1_MIN_83,.max = MRFLD_DPLL_P1_MAX_83},
	 },
	{			/* MRFLD_LIMT_DPLL_100 */
	 .dot = {.min = MRFLD_DOT_MIN,.max = MRFLD_DOT_MAX},
	 .m = {.min = MRFLD_DPLL_M_MIN_100,.max = MRFLD_DPLL_M_MAX_100},
	 .p1 = {.min = MRFLD_DPLL_P1_MIN_100,.max = MRFLD_DPLL_P1_MAX_100},
	 },
	{			/* MRFLD_LIMT_DSIPLL_19 */
	 .dot = {.min = MRFLD_DOT_MIN,.max = MRFLD_DOT_MAX},
	 .m = {.min = MRFLD_DSIPLL_M_MIN_19,.max = MRFLD_DSIPLL_M_MAX_19},
	 .p1 = {.min = MRFLD_DSIPLL_P1_MIN_19,.max = MRFLD_DSIPLL_P1_MAX_19},
	 },
	{			/* MRFLD_LIMT_DSIPLL_25 */
	 .dot = {.min = MRFLD_DOT_MIN,.max = MRFLD_DOT_MAX},
	 .m = {.min = MRFLD_DSIPLL_M_MIN_25,.max = MRFLD_DSIPLL_M_MAX_25},
	 .p1 = {.min = MRFLD_DSIPLL_P1_MIN_25,.max = MRFLD_DSIPLL_P1_MAX_25},
	 },
	{			/* MRFLD_LIMT_DSIPLL_83 */
	 .dot = {.min = MRFLD_DOT_MIN,.max = MRFLD_DOT_MAX},
	 .m = {.min = MRFLD_DSIPLL_M_MIN_83,.max = MRFLD_DSIPLL_M_MAX_83},
	 .p1 = {.min = MRFLD_DSIPLL_P1_MIN_83,.max = MRFLD_DSIPLL_P1_MAX_83},
	 },
	{			/* MRFLD_LIMT_DSIPLL_100 */
	 .dot = {.min = MRFLD_DOT_MIN,.max = MRFLD_DOT_MAX},
	 .m = {.min = MRFLD_DSIPLL_M_MIN_100,.max = MRFLD_DSIPLL_M_MAX_100},
	 .p1 = {.min = MRFLD_DSIPLL_P1_MIN_100,.max = MRFLD_DSIPLL_P1_MAX_100},
	 },
};

#define MRFLD_M_MIN	    21
#define MRFLD_M_MAX	    180
static const u32 mrfld_m_converts[] = {
/* M configuration table from 9-bit LFSR table */
	224, 368, 440, 220, 366, 439, 219, 365, 182, 347,	/* 21 - 30 */
	173, 342, 171, 85, 298, 149, 74, 37, 18, 265,	/* 31 - 40 */
	388, 194, 353, 432, 216, 108, 310, 155, 333, 166,	/* 41 - 50 */
	83, 41, 276, 138, 325, 162, 337, 168, 340, 170,	/* 51 - 60 */
	341, 426, 469, 234, 373, 442, 221, 110, 311, 411,	/* 61 - 70 */
	461, 486, 243, 377, 188, 350, 175, 343, 427, 213,	/* 71 - 80 */
	106, 53, 282, 397, 354, 227, 113, 56, 284, 142,	/* 81 - 90 */
	71, 35, 273, 136, 324, 418, 465, 488, 500, 506,	/* 91 - 100 */
	253, 126, 63, 287, 399, 455, 483, 241, 376, 444,	/* 101 - 110 */
	478, 495, 503, 251, 381, 446, 479, 239, 375, 443,	/* 111 - 120 */
	477, 238, 119, 315, 157, 78, 295, 147, 329, 420,	/* 121 - 130 */
	210, 105, 308, 154, 77, 38, 275, 137, 68, 290,	/* 131 - 140 */
	145, 328, 164, 82, 297, 404, 458, 485, 498, 249,	/* 141 - 150 */
	380, 190, 351, 431, 471, 235, 117, 314, 413, 206,	/* 151 - 160 */
	103, 51, 25, 12, 262, 387, 193, 96, 48, 280,	/* 161 - 170 */
	396, 198, 99, 305, 152, 76, 294, 403, 457, 228,	/* 171 - 180 */
};
static const struct mrst_limit_t *mrfld_limit(struct drm_device *dev, int pipe)
{
	const struct mrst_limit_t *limit = NULL;
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;

	if ((pipe == 0) || (pipe == 2)) {
		if ((dev_priv->ksel == KSEL_CRYSTAL_19)
		    || (dev_priv->ksel == KSEL_BYPASS_19))
			limit = &mrfld_limits[MRFLD_LIMT_DSIPLL_19];
		else if (dev_priv->ksel == KSEL_BYPASS_25)
			limit = &mrfld_limits[MRFLD_LIMT_DSIPLL_25];
		else if ((dev_priv->ksel == KSEL_BYPASS_83_100)
			 && (dev_priv->core_freq == 166))
			limit = &mrfld_limits[MRFLD_LIMT_DSIPLL_83];
		else if ((dev_priv->ksel == KSEL_BYPASS_83_100) &&
			 (dev_priv->core_freq == 100
			  || dev_priv->core_freq == 200))
			limit = &mrfld_limits[MRFLD_LIMT_DSIPLL_100];
	} else if (pipe == 1) {
		if ((dev_priv->ksel == KSEL_CRYSTAL_19)
		    || (dev_priv->ksel == KSEL_BYPASS_19))
			limit = &mrfld_limits[MRFLD_LIMT_DPLL_19];
		else if (dev_priv->ksel == KSEL_BYPASS_25)
			limit = &mrfld_limits[MRFLD_LIMT_DPLL_25];
		else if ((dev_priv->ksel == KSEL_BYPASS_83_100)
			 && (dev_priv->core_freq == 166))
			limit = &mrfld_limits[MRFLD_LIMT_DPLL_83];
		else if ((dev_priv->ksel == KSEL_BYPASS_83_100) &&
			 (dev_priv->core_freq == 100
			  || dev_priv->core_freq == 200))
			limit = &mrfld_limits[MRFLD_LIMT_DPLL_100];
	} else {
		limit = NULL;
		PSB_DEBUG_ENTRY("mrfld_limit Wrong display type. \n");
	}

	return limit;
}

/** Derive the pixel clock for the given refclk and divisors. */
static void mrfld_clock(int refclk, struct mrst_clock_t *clock)
{
	clock->dot = (refclk * clock->m) / clock->p1;
}

/**
 * Returns a set of divisors for the desired target clock with the given refclk,
 * or FALSE.  
 */
static bool
mrfld_find_best_PLL(struct drm_device *dev, int pipe, int target, int refclk,
		    struct mrst_clock_t *best_clock)
{
	struct mrst_clock_t clock;
	const struct mrst_limit_t *limit = mrfld_limit(dev, pipe);
	int err = target;

	if (!limit) {
		DRM_ERROR("limit is NULL\n");
		return false;
	}

	memset(best_clock, 0, sizeof(*best_clock));

	PSB_DEBUG_ENTRY
	    ("target = %d, m_min = %d, m_max = %d, p_min = %d, p_max = %d. \n",
	     target, limit->m.min, limit->m.max, limit->p1.min, limit->p1.max);

	for (clock.m = limit->m.min; clock.m <= limit->m.max; clock.m++) {
		for (clock.p1 = limit->p1.min; clock.p1 <= limit->p1.max;
		     clock.p1++) {
			int this_err;

			mrfld_clock(refclk, &clock);

			this_err = abs(clock.dot - target);
			if (this_err < err) {
				*best_clock = clock;
				err = this_err;
			}
		}
	}
	PSB_DEBUG_ENTRY("mdfldFindBestPLL target = %d,"
			"m = %d, p = %d. \n", target, best_clock->m,
			best_clock->p1);
	PSB_DEBUG_ENTRY("mdfldFindBestPLL err = %d.\n", err);

	return err != target;
}

/**
 * Set up the display clock 
 *
 */
void mrfld_setup_pll(struct drm_device *dev, int pipe, int clk)
{
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;
	int refclk = 0;
	int clk_n = 0, clk_p2 = 0, clk_byte = 1, m_conv = 0, clk_tmp = 0;
	struct mrst_clock_t clock;
	bool ok;
	u32 pll = 0, fp = 0;
	bool is_mipi = false, is_mipi2 = false, is_hdmi = false;

#ifdef CONFIG_SUPPORT_MIPI
	struct mdfld_dsi_config *dsi_config = NULL;
	struct mdfld_dsi_hw_context *ctx = NULL;

	PSB_DEBUG_ENTRY("pipe = 0x%x\n", pipe);

	if (pipe == 0)
		dsi_config = dev_priv->dsi_configs[0];
	else if (pipe == 2)
		dsi_config = dev_priv->dsi_configs[1];

	if ((pipe != 1) && !dsi_config) {
		DRM_ERROR("Invalid DSI config\n");
		return;
	}

	if (pipe != 1) {
		ctx = &dsi_config->dsi_hw_context;

		mutex_lock(&dsi_config->context_lock);
	}

	switch (pipe) {
	case 0:
		is_mipi = true;
		break;
	case 1:
		is_hdmi = true;
		break;
	case 2:
		is_mipi2 = true;
		break;
	}
#else
	if (pipe == 1)
		is_hdmi = true;
	else
		return;
#endif

	if ((dev_priv->ksel == KSEL_CRYSTAL_19)
	    || (dev_priv->ksel == KSEL_BYPASS_19)) {
		refclk = 19200;

		if (is_mipi || is_mipi2) {
			clk_n = 1, clk_p2 = 8;
		} else if (is_hdmi) {
			clk_n = 1, clk_p2 = 10;
		}
	} else if (dev_priv->ksel == KSEL_BYPASS_25) {
		refclk = 25000;

		if (is_mipi || is_mipi2) {
			clk_n = 1, clk_p2 = 8;
		} else if (is_hdmi) {
			clk_n = 1, clk_p2 = 10;
		}
	} else if ((dev_priv->ksel == KSEL_BYPASS_83_100)
		   && (dev_priv->core_freq == 166)) {
		refclk = 83000;

		if (is_mipi || is_mipi2) {
			clk_n = 4, clk_p2 = 8;
		} else if (is_hdmi) {
			clk_n = 4, clk_p2 = 10;
		}
	} else if ((dev_priv->ksel == KSEL_BYPASS_83_100) &&
		   (dev_priv->core_freq == 100 || dev_priv->core_freq == 200)) {
		refclk = 100000;
		if (is_mipi || is_mipi2) {
			clk_n = 4, clk_p2 = 8;
		} else if (is_hdmi) {
			clk_n = 4, clk_p2 = 10;
		}
	}

	if (is_mipi || is_mipi2)
		clk_byte = 3;

	clk_tmp = clk * clk_n * clk_p2 * clk_byte;

	PSB_DEBUG_ENTRY("clk = %d, clk_n = %d, clk_p2 = %d. \n", clk, clk_n,
			clk_p2);
	PSB_DEBUG_ENTRY("clk = %d, clk_tmp = %d, clk_byte = %d. \n", clk,
			clk_tmp, clk_byte);

	ok = mrfld_find_best_PLL(dev, pipe, clk_tmp, refclk, &clock);
	dev_priv->tmds_clock_khz = clock.dot / (clk_n * clk_p2 * clk_byte);

#ifdef CONFIG_SUPPORT_MIPI
	/*
	 * FIXME: Hard code the divisors' value for JDI panel, and need to
	 * calculate them according to the DSI PLL HAS spec.
	 */
	if (pipe != 1) {
		switch(get_panel_type(dev, pipe)) {
		case SDC_16x25_CMD:
				clock.p1 = 3;
				clock.m = 126;
				break;
		case SHARP_10x19_CMD:
				clock.p1 = 3;
				clock.m = 137;
				break;
		case SHARP_10x19_DUAL_CMD:
				clock.p1 = 3;
				clock.m = 125;
				break;
		case CMI_7x12_CMD:
				clock.p1 = 4;
				clock.m = 120;
				break;
		case SDC_25x16_CMD:
		case JDI_25x16_CMD:
		case SHARP_25x16_CMD:
				clock.p1 = 3;
				clock.m = 138;
				break;
		case SHARP_25x16_VID:
		case JDI_25x16_VID:
				clock.p1 = 3;
				clock.m = 162;
				break;
		case JDI_7x12_VID:
				clock.p1 = 5;
				clk_n = 1;
				clock.m = 144;
				break;
		default:
			/* for JDI_7x12_CMD */
				clock.p1 = 4;
				clock.m = 142;
				break;
		}
		clk_n = 1;
	}
#endif

	if (!ok) {
		DRM_ERROR("mdfldFindBestPLL fail in mrfld_crtc_mode_set.\n");
	} else {
		m_conv = mrfld_m_converts[(clock.m - MRFLD_M_MIN)];

		PSB_DEBUG_ENTRY("dot clock = %d,"
				"m = %d, p1 = %d, m_conv = %d. \n", clock.dot,
				clock.m, clock.p1, m_conv);
	}

	/* Write the N1 & M1 parameters into DSI_PLL_DIV_REG */
	fp = (clk_n / 2) << 16;
	fp |= m_conv;

#ifdef CONFIG_SUPPORT_MIPI
	if (is_mipi) {
		/* Enable DSI PLL clocks for DSI0 rather than CCK. */
		pll |= _CLK_EN_PLL_DSI0;
		pll &= ~_CLK_EN_CCK_DSI0;
		/* Select DSI PLL as the source of the mux input clocks. */
		pll &= ~_DSI_MUX_SEL_CCK_DSI0;
	}

	if (is_mipi2 || is_dual_dsi(dev)) {
		/* Enable DSI PLL clocks for DSI1 rather than CCK. */
		pll |= _CLK_EN_PLL_DSI1;
		pll &= ~_CLK_EN_CCK_DSI1;
		/* Select DSI PLL as the source of the mux input clocks. */
		pll &= ~_DSI_MUX_SEL_CCK_DSI1;
	}
#endif

	if (is_hdmi)
		pll |= MDFLD_VCO_SEL;

	/* compute bitmask from p1 value */
	pll |= (1 << (clock.p1 - 2)) << 17;

#ifdef CONFIG_SUPPORT_MIPI
	if (pipe != 1) {
		ctx->dpll = pll;
		ctx->fp = fp;
		mutex_unlock(&dsi_config->context_lock);
	}
#endif
}

/**
 * Set up the HDMI display clock 
 *
 */
void mrfld_setup_dpll(struct drm_device *dev, int clk)
{
	int clk_n = 0, clk_m1 = 0, clk_m2 = 0, clk_p1 = 0, clk_p2 = 0;
	int dpll_div = 0;
	int pllctl = 0, tldrv = 0, pllin = 0, pllmisc = 0;
	u32 dpll_tmp = 0;
	u32 pll = 0;

	pll = MRFLD_CRI_ICK_PLL | MRFLD_INPUT_REF_SSC;
	REG_WRITE(MDFLD_DPLL_B, pll);
	udelay(500);		/* revisit it for exact delay. */

	pll |= MRFLD_EXT_CLK_BUF_EN | MRFLD_REF_CLK_EN | MRFLD_CMNRST;
	REG_WRITE(MDFLD_DPLL_B, pll);

	/* Main PLL Configuration. */
	clk_n = 1;
	clk_m1 = 2;
	clk_m2 = 145;
	clk_p1 = 3;
	clk_p2 = 5;
	dpll_div = clk_m2 | (clk_m1 << 8) | (clk_n << 12) | (clk_p2 << 16) |
	    (clk_p1 << 21);
	intel_mid_msgbus_write32(HDMIPHY_PORT, DPLL_DIV_REG, dpll_div);

	/* Set up LCPLL in Digital Mode. */
	pllctl = 0;		/* idthsen reset to 0 for display operation. */
	tldrv = 0xCC;
	pllin = 0x73;		/* pllrefsel selects alt core ref clock(19.2MHz). */
	pllmisc = 0x0D;		/* Digital mode for LCPLL, pllrefselorden set. */
	dpll_tmp = pllctl | (tldrv << 8) | (pllin << 16) | (pllmisc << 24);
	intel_mid_msgbus_write32(HDMIPHY_PORT, PLL_CTL_IN_MISC_TLDRV_REG,
		dpll_tmp);

	/* Program Co-efficients for LCPLL in Digital Mode. */
	dpll_tmp = 0x001f0077;
	intel_mid_msgbus_write32(HDMIPHY_PORT, LPF_COEFF_REG, dpll_tmp);

	/* Enable DPLL VCO. */
	pll |= DPLL_VCO_ENABLE;
	REG_WRITE(MDFLD_DPLL_B, pll);

	/* Enable DCLP to core. */
	dpll_tmp = 0x00030101;	/* FIXME need to read_mask_write. */
	intel_mid_msgbus_write32(HDMIPHY_PORT, PLL_AFC_MISC_REG, dpll_tmp);

	/* Disable global rcomp. */
	dpll_tmp = 0x07010101;	/* FIXME need to read_mask_write. */
	intel_mid_msgbus_write32(HDMIPHY_PORT, GLOBAL_RCOMP_REG, dpll_tmp);

	/* Stagger Programming */
	dpll_tmp = 0x00401f00;
	intel_mid_msgbus_write32(HDMIPHY_PORT, DPLL_STAGER_CTL_REG1, dpll_tmp);

	dpll_tmp = 0x00541f00;
	intel_mid_msgbus_write32(HDMIPHY_PORT, DPLL_STAGER_CTL_REG2, dpll_tmp);
}

void enable_HFPLL(struct drm_device *dev)
{
	uint32_t pll_select = 0, ctrl_reg5 = 0;
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)dev->dev_private;

	/* Enable HFPLL for command mode panel */
	if (dev_priv->bUseHFPLL) {
			pll_select = intel_mid_msgbus_read32(CCK_PORT,
						DSI_PLL_CTRL_REG);
			ctrl_reg5 = intel_mid_msgbus_read32(CCK_PORT,
						FUSE_OVERRIDE_FREQ_CNTRL_REG5);

			pll_select &= ~(_DSI_MUX_SEL_CCK_DSI1 |
					_DSI_MUX_SEL_CCK_DSI0);

			intel_mid_msgbus_write32(CCK_PORT, DSI_PLL_CTRL_REG,
					pll_select | _DSI_CCK_PLL_SELECT);
			ctrl_reg5 |= (1 << 7) | 0xF;

#ifdef CONFIG_SUPPORT_MIPI
			if (get_panel_type(dev, 0) == SHARP_10x19_CMD)
				ctrl_reg5 = 0x1f87;
#endif
			intel_mid_msgbus_write32(CCK_PORT,
					FUSE_OVERRIDE_FREQ_CNTRL_REG5,
					ctrl_reg5);
	}
}

#ifdef CONFIG_SUPPORT_MIPI
bool enable_DSIPLL(struct drm_device *dev)
{
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;
	struct mdfld_dsi_config *dsi_config = NULL;
	struct mdfld_dsi_hw_context *ctx = NULL;
	u32 guit_val = 0x0;
	u32 retry;

	if (!dev_priv)
		goto err_out;
	dsi_config = dev_priv->dsi_configs[0];
	if (!dsi_config)
		goto err_out;
	ctx = &dsi_config->dsi_hw_context;

	if (IS_ANN(dev)) {
		int dspfreq;

		if ((get_panel_type(dev, 0) == JDI_7x12_CMD) ||
			(get_panel_type(dev, 0) == JDI_7x12_VID))
			dspfreq = DISPLAY_FREQ_FOR_200;
		else
			dspfreq = DISPLAY_FREQ_FOR_333;

		intel_mid_msgbus_write32(CCK_PORT,
			FUSE_OVERRIDE_FREQ_CNTRL_REG5,
			CKESC_GATE_EN | CKDP1X_GATE_EN | DISPLAY_FRE_EN
			| dspfreq);
	}

	/* Prepare DSI  PLL register before enabling */
	intel_mid_msgbus_write32(CCK_PORT, DSI_PLL_DIV_REG, 0);
	guit_val = intel_mid_msgbus_read32(CCK_PORT, DSI_PLL_CTRL_REG);
	guit_val &= ~(DPLL_VCO_ENABLE | _DSI_LDO_EN
			|_CLK_EN_MASK | _DSI_MUX_SEL_CCK_DSI0 | _DSI_MUX_SEL_CCK_DSI1);
	intel_mid_msgbus_write32(CCK_PORT,
					DSI_PLL_CTRL_REG, guit_val);
	udelay(1);
	/* Program PLL */

	/*first set up the dpll and fp variables
	 * dpll - will contain the following information
	 *      - the clock source - DSI vs HFH vs LFH PLL
	 * 	- what clocks should be running DSI0, DSI1
	 *      - and the divisor.
	 *
	 */

	intel_mid_msgbus_write32(CCK_PORT, DSI_PLL_DIV_REG, ctx->fp);
	guit_val &= ~_P1_POST_DIV_MASK;	/*clear the divisor bit*/
	/* the ctx->dpll contains the divisor that we need to use as well as which clocks
	 * need to start up */
	guit_val |= ctx->dpll;
	guit_val &= ~_DSI_LDO_EN;	/* We want to clear the LDO enable when programming*/
	guit_val |=  DPLL_VCO_ENABLE;	/* Enable the DSI PLL */

	/* For the CD clock (clock used by Display controller), we need to set
	 * the DSI_CCK_PLL_SELECT bit (bit 11). This should already be set. But
	 * setting it just in case
	 */
	if (dev_priv->bUseHFPLL)
		guit_val |= _DSI_CCK_PLL_SELECT;

	intel_mid_msgbus_write32(CCK_PORT, DSI_PLL_CTRL_REG, guit_val);

	/* Wait for DSI PLL lock */
	retry = 10000;
	guit_val = intel_mid_msgbus_read32(CCK_PORT, DSI_PLL_CTRL_REG);
	while (((guit_val & _DSI_PLL_LOCK) != _DSI_PLL_LOCK) && (--retry)) {
		udelay(3);
		guit_val = intel_mid_msgbus_read32(CCK_PORT, DSI_PLL_CTRL_REG);
		if (!retry%1000)
			DRM_ERROR("DSI PLL taking too long to lock"
				"- retry count=%d\n", 10000-retry);
	}
	if (retry == 0) {
		DRM_ERROR("DSI PLL fails to lock\n");
		return false;
	}

	return true;
err_out:
	return false;

}

bool disable_DSIPLL(struct drm_device * dev)
{
	u32 val, guit_val;

	/* Disable PLL*/
	intel_mid_msgbus_write32(CCK_PORT, DSI_PLL_DIV_REG, 0);

	val = intel_mid_msgbus_read32(CCK_PORT, DSI_PLL_CTRL_REG);
	val &= ~_CLK_EN_MASK;
	intel_mid_msgbus_write32(CCK_PORT, DSI_PLL_CTRL_REG, val);
	udelay(1);
	val &= ~DPLL_VCO_ENABLE;
	val |= _DSI_LDO_EN;
	intel_mid_msgbus_write32(CCK_PORT, DSI_PLL_CTRL_REG, val);
	udelay(1);

	guit_val = intel_mid_msgbus_read32(CCK_PORT, DSI_PLL_CTRL_REG);
	if ((guit_val & _DSI_PLL_LOCK) == _DSI_PLL_LOCK ) {
		DRM_ERROR("DSI PLL fails to Unlock\n");
		return false;
	}
	return true;
}
#endif
