/**************************************************************************
 * Copyright (c) 2012, Intel Corporation.
 * All Rights Reserved.

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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Dale B. Stimson <dale.b.stimson@intel.com>
 *
 */


#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/delay.h>

#include <asm/intel-mid.h>

#include "pmu_tng.h"


#if (defined DEBUG_PM_CMD) && DEBUG_PM_CMD
const char *pm_cmd_reg_name(u32 reg_addr)
{
	const char *pstr;

	switch (reg_addr) {
	case GFX_SS_PM0:
		pstr = "GFX_SS_PM0";
		break;
	case GFX_SS_PM1:
		pstr = "GFX_SS_PM1";
		break;
	case VED_SS_PM0:
		pstr = "VED_SS_PM0";
		break;
	case VED_SS_PM1:
		pstr = "VED_SS_PM1";
		break;
	case VEC_SS_PM0:
		pstr = "VEC_SS_PM0";
		break;
	case VEC_SS_PM1:
		pstr = "VEC_SS_PM1";
		break;
	case DSP_SS_PM:
		pstr = "DSP_SS_PM";
		break;
	case VSP_SS_PM0:
		pstr = "VSP_SS_PM0";
		break;
	case VSP_SS_PM1:
		pstr = "VSP_SS_PM1";
		break;
	case MIO_SS_PM:
		pstr = "MIO_SS_PM";
		break;
	case HDMIO_SS_PM:
		pstr = "HDMIO_SS_PM";
		break;
	case NC_PM_SSS:
		pstr = "NC_PM_SSS";
		break;
	default:
		pstr = "(unknown_pm_reg)";
		break;
	}

	return pstr;
}
#endif /* if (defined DEBUG_PM_CMD) && DEBUG_PM_CMD */


/**
 * pmu_set_power_state_tng() - Send power management cmd to punit and
 * wait for completion.
 *
 * This function implements Tangier/Merrifield punit-based power control.
 *
 * @reg_pm0 - Address of PM control register.  Example: GFX_SS_PM0
 *
 * @si_mask: Control bits.  "si" stands for "sub-islands".
 * Bit mask specifying of one or more of the power islands to be affected.
 * Each power island is a two bit field.  These bits are set for every bit
 * in each power island to be affected by this command.
 * For each island, either 0 or all 2 of its bits may be specified, but it
 * is an error to specify only 1 of its bits.
 *
 * @ns_mask: "ns" stands for "new state".
 * New state for bits specified by si_mask.
 * Bits in ns_mask that are not set in si_mask are ignored.
 * Mask of new power state for the power islands specified by si_mask.
 * These bits are 0b00 for full power off and 0b11 for full power on.
 * Note that other values may be specified (0b01 and 0b10).
 *
 * Bit field values:
 *   TNG_SSC_I0    0b00      - i0 - power on, no clock or p[ower gating
 *   TNG_SSC_I1    0b01      - i1 - clock gated
 *   TNG_SSC_I2    0b01      - i2 - soft reset
 *   TNG_SSC_D3    0b11      - d3 - power off, hw state not retained
 *
 * NOTE: Bit mask ns_mask is inverted from the *actual* hardware register
 * values being used for power control.  This convention was adopted so that
 * the API accepts 0b11 for full power-on and 0b00 for full power-off.
 *
 * Function return value: 0 if success, or -error_value.
 *
 * Example calls (ignoring return status):
 * Turn on all gfx islands:
 *   si_mask = GFX_SLC_LDO_SSC | GFX_SLC_SSC | GFX_SDKCK_SSC | GFX_RSCD_SSC;
 *   ns_mask = GFX_SLC_LDO_SSC | GFX_SLC_SSC | GFX_SDKCK_SSC | GFX_RSCD_SSC;
 *   pmu_set_power_state_tng(GFX_SS_PM0, this_mask, new_state);
 * Turn on all gfx islands:  (Another way):
 *   si_mask = GFX_SLC_LDO_SSC | GFX_SLC_SSC | GFX_SDKCK_SSC | GFX_RSCD_SSC;
 *   ns_mask = 0xFFFFFFFF;
 *   pmu_set_power_state_tng(GFX_SS_PM0, this_mask, new_state);
 * Turn off all gfx islands:
 *   si_mask = GFX_SLC_LDO_SSC | GFX_SLC_SSC | GFX_SDKCK_SSC | GFX_RSCD_SSC;
 *   ns_mask = 0;
 *   pmu_set_power_state_tng(GFX_SS_PM0, this_mask, new_state);
 *
 * Replaces (for Tangier):
 *    int pmu_nc_set_power_state(int islands, int state_type, int reg_type);
 */
int pmu_set_power_state_tng(u32 reg_pm0, u32 si_mask, u32 ns_mask)
{
	u32 pwr_cur;
	u32 pwr_val;
	int tcount;

#if (defined DEBUG_PM_CMD) && DEBUG_PM_CMD
	u32 pwr_prev;
	int pwr_stored;
#endif

	ns_mask &= si_mask;

#if (defined DEBUG_PM_CMD) && DEBUG_PM_CMD
	printk(KERN_ALERT "%s(\"%s\"=%#x, %#x, %#x);\n", __func__,
		pm_cmd_reg_name(reg_pm0), reg_pm0, si_mask, ns_mask);
#endif

	pwr_cur = intel_mid_msgbus_read32(PUNIT_PORT, reg_pm0);

#if (defined DEBUG_PM_CMD) && DEBUG_PM_CMD
	printk(KERN_ALERT "%s: before: %s: read: %#x\n",
		__func__, pm_cmd_reg_name(reg_pm0), pwr_cur);
#endif
	/*  Return if already in desired state. */
	if ((((pwr_cur >> SSC_TO_SSS_SHIFT) ^ ns_mask) & si_mask) == 0)
		return 0;

	pwr_val = (pwr_cur & ~si_mask) | ns_mask;
	intel_mid_msgbus_write32(PUNIT_PORT, reg_pm0, pwr_val);

#if (defined DEBUG_PM_CMD) && DEBUG_PM_CMD
	printk(KERN_ALERT "%s: %s: write: %#x\n",
		__func__, pm_cmd_reg_name(reg_pm0), pwr_val);
	pwr_prev = 0;
	pwr_stored = 0;
#endif

	for (tcount = 0; ; tcount++) {
		if (tcount > 50) {
			WARN(1, "%s: P-Unit PM action request timeout",
				__func__);
			return -EBUSY;
		}
		pwr_cur = intel_mid_msgbus_read32(PUNIT_PORT, reg_pm0);

#if (defined DEBUG_PM_CMD) && DEBUG_PM_CMD
		if (!pwr_stored || (pwr_prev != pwr_cur)) {
			printk(KERN_ALERT
				"%s: tries=%d: %s: read: %#x\n",
				__func__, tcount,
				pm_cmd_reg_name(reg_pm0),
				pwr_cur);
			pwr_stored = 1;
			pwr_prev = pwr_cur;
		}
#endif

		if ((((pwr_cur >> SSC_TO_SSS_SHIFT) ^ ns_mask) & si_mask) == 0)
			break;
		udelay(10);
	}

	return 0;
}
