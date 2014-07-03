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

#ifndef _MRFLD_CLOCK_H_
#define _MRFLD_CLOCK_H_

#define HDMIPHY_PORT			0x13
#define CCK_PORT			0x14
#define DSI_PLL_CTRL_REG		0x48
#define _DSI_LDO_EN			(1 << 30)
#define _P1_POST_DIV_MASK		(0x1ff << 17)
#define _DSI_CCK_PLL_SELECT		(1 << 11)
#define _DSI_MUX_SEL_CCK_DSI0		(1 << 10)
#define _DSI_MUX_SEL_CCK_DSI1		(1 << 9)
#define _CLK_EN_PLL_DSI0		(1 << 8)
#define _CLK_EN_PLL_DSI1		(1 << 7)
#define _CLK_EN_CCK_DSI0		(1 << 6)
#define _CLK_EN_CCK_DSI1		(1 << 5)
#define _CLK_EN_MASK			(0xf << 5)
#define _DSI_PLL_LOCK			(1 << 0)
#define DSI_PLL_DIV_REG			0x4C
#define FUSE_OVERRIDE_FREQ_CNTRL_REG3	0x54
#define FUSE_OVERRIDE_FREQ_CNTRL_REG5	0x68
#define CKDP_DIV2_ENABLE	(1 << 12)
#define CKDP2X_ENABLE		(1 << 11)
#define CKESC_GATE_EN		(1 << 10)
#define CKDP1X_GATE_EN		(1 << 9)
#define CKDP2X_GATE_EN		(1 << 8)
#define DISPLAY_FRE_EN		(1 << 7)
#define DISPLAY_FREQ_FOR_200	4
#define DISPLAY_FREQ_FOR_333	2
#define DPLL_STAGER_CTL_REG1		0x0230
#define DPLL_STAGER_CTL_REG2		0x0430
#define DPLL_DIV_REG			0x800C
#define PLL_CTL_IN_MISC_TLDRV_REG	0x8014
#define PLL_AFC_MISC_REG		0x801C
#define LPF_COEFF_REG			0x8048
#define GLOBAL_RCOMP_REG		0x80E0

struct psb_intel_range_t {
	int min, max;
};

struct mrst_limit_t {
	struct psb_intel_range_t dot, m, p1;
};

struct mrst_clock_t {
	/* derived values */
	int dot;
	int m;
	int p1;
};

void enable_HFPLL(struct drm_device *dev);
bool enable_DSIPLL(struct drm_device *dev);
bool disable_DSIPLL(struct drm_device *dev);

#endif
