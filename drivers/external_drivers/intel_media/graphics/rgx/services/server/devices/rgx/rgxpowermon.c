/**************************************************************************
 * Copyright (c) 2013, Intel Corporation.
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
 *    Javier Torres Castillo <javier.torres.castillo@intel.com>
 */

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/printk.h>
#include <linux/delay.h>
#include <asm/intel-mid.h>
#include "rgxpowermon.h"

#define UINT32_MAX		(0xFFFFFFFF)
#define PUNIT_PORT              0x04	/* Constant from pmu*/
#define GFX_SS_PM1              0x31	/* Power monitor reg*/

#define GFX_POWEROFF_POS		(23)
#define GFX_POWEROFF_INTENT_MASK	(UINT32_MAX & (0x1 << GFX_POWEROFF_POS))
#define GFX_POWEROFF_CLEAR_INTENT	(UINT32_MAX ^ GFX_POWEROFF_INTENT_MASK)

#define PUNIT_BUSY_POS			(31)
#define PUNIT_BUSY_MASK			(UINT32_MAX & (0x1 << PUNIT_BUSY_POS))
#define PUNIT_CLEAR_BUSY		(UINT32_MAX ^ PUNIT_BUSY_MASK)

#define POLLING_TIMES			(5)
#define POLLING_INTERVAL		(100)

#define REGULAR_POLLING_TIMES			(50)
#define REGULAR_POLLING_INTERVAL		(1)

#define CLEAR_REG			(1)
#define SET_REG				(0)

#define RGX_POW_MON_DEBUG	0

static int punit_read_reg_gfxsspm1(int attempts, int interval_us, u32 *p_reg_value)
{
	int ui_count;

	*p_reg_value = 0;

	if(!p_reg_value)
		return -EINVAL;

	for (ui_count = 0; ; ui_count++) {

		*p_reg_value = intel_mid_msgbus_read32(PUNIT_PORT, GFX_SS_PM1);

		if(*p_reg_value) {
#if defined(RGX_POW_MON_DEBUG) && RGX_POW_MON_DEBUG
			printk(KERN_ALERT "punit_read_reg_gfxsspm1: read reg value: %0x\n", *p_reg_value);
#endif
			break;
		}
		if (ui_count >= attempts)
			return -EBUSY;

		udelay(interval_us);
	}

	return 0;
}

static int punit_write_reg_gfxsspm1(int clear, u32 mask)
{
	int rva = 1;
	u32 read_reg_value;
	u32 write_reg_value;

	rva = punit_read_reg_gfxsspm1(REGULAR_POLLING_TIMES, REGULAR_POLLING_INTERVAL, &read_reg_value);
	if (rva < 0) {
		return 0;
	}

	/* Intent not submitted yet*/
	if(clear)
		write_reg_value = read_reg_value & mask;
	else
		write_reg_value = read_reg_value | mask;

	intel_mid_msgbus_write32(PUNIT_PORT, GFX_SS_PM1, write_reg_value);

	rva = punit_read_reg_gfxsspm1(REGULAR_POLLING_TIMES, REGULAR_POLLING_INTERVAL, &read_reg_value);

#if defined(RGX_POW_MON_DEBUG) && RGX_POW_MON_DEBUG
	printk(KERN_ALERT "punit_write_reg_gfxsspm1: read: %0x, requested: %0x\n", read_reg_value, write_reg_value);
#endif
	if (rva < 0 || write_reg_value != read_reg_value) {
		return 0;
	}

	return 1;
}

/* Will return IMG_TRUE if punit is busy or if it times out*/
static int rgx_is_punit_busy(void)
{
	int b_punit_busy = 1;
	u32 read_gfx_sspm1;
	int rva = punit_read_reg_gfxsspm1(POLLING_TIMES,
				POLLING_INTERVAL,
				&read_gfx_sspm1);

	/* Only in success We check, if -EBUSY it means there
	* was a timeout
	*/
	if (!rva) {
		/* Intent not submitted yet*/
		if ((read_gfx_sspm1 & PUNIT_BUSY_MASK) == 0) {
			b_punit_busy = 0;
		}
	}

	return b_punit_busy;
}

void rgx_powermeter_poweron(void)
{
	/* Clear reg, GFX_SSPM1[23] = 0*/
	int is_reg_written;
#if defined(RGX_POW_MON_DEBUG) && RGX_POW_MON_DEBUG
	printk(KERN_ALERT "punit_rgx_powermeter_poweron-----\n");
#endif
	is_reg_written = punit_write_reg_gfxsspm1(CLEAR_REG, GFX_POWEROFF_CLEAR_INTENT);
}

int rgx_powermeter_poweroff(void)
{
	int is_reg_written;
	int b_punit_busy;

#if defined(RGX_POW_MON_DEBUG) && RGX_POW_MON_DEBUG
	printk(KERN_ALERT "punit_rgx_powermeter_poweroff-----\n");
#endif
	is_reg_written = punit_write_reg_gfxsspm1(SET_REG, GFX_POWEROFF_INTENT_MASK);
	b_punit_busy = rgx_is_punit_busy();
	/*If we set 1 to GFX_SSPM1[23] and GFX_SSPM1[31] is 1
	* We need to abort the power-off
	*/
	return ((!b_punit_busy) && is_reg_written);
}

