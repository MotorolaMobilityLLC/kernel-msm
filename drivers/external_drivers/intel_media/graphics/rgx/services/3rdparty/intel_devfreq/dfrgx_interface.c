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
 *    Javier Torres Castillo <javier.torres.castillo@intel.com>
 */


#include <linux/module.h>

#include "dfrgx_interface.h"

static struct dfrgx_interface_s dfrgx_interface;


/**
 * dfrgx_interface_set_data() - Provide some gburst data for hooks
 * inside the graphics driver.
 * @dfrgx_interface_in: Data to allow callback to dfrgx burst module.
 *
 * Also, the symbol dependency will establish a load order dependency for
 * the case where both the graphics driver  and the dfrgx driver are modules,
 * ensuring that the graphics driver is loaded and initialized before gburst.
 */
void dfrgx_interface_set_data(struct dfrgx_interface_s *dfrgx_interface_in)
{
	dfrgx_interface = *dfrgx_interface_in;
}


/*  Leave this export in place, even if built-in, as it allows easy compilation
    testing of gburst as a module. */
EXPORT_SYMBOL(dfrgx_interface_set_data);


/**
 * gburst_interface_power_state_set() - gfx drv calls to indicate power state.
 * @st_on: 1 if power coming on, 0 if power going off.
 */
void dfrgx_interface_power_state_set(int st_on)
{
	if (dfrgx_interface.dfrgx_power_state_set && dfrgx_interface.dfrgx_priv)
		dfrgx_interface.dfrgx_power_state_set(dfrgx_interface.dfrgx_priv, st_on);
}

