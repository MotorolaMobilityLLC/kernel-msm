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

#if !defined DFRGX_INTERFACE_H
#define DFRGX_INTERFACE_H

struct df_rgx_data_s;

typedef void (*dfrgx_power_state_set_f)(struct df_rgx_data_s *dfrgx_data, int st_on);

/**
 * struct gburst_interface_s -
 * @dfrgx_priv: Private data handle, opaque to the other driver.
 * @dfrgx_power_state_set: Function to callback when dev power changes.
 */
struct dfrgx_interface_s {
	struct df_rgx_data_s  *dfrgx_priv;
	dfrgx_power_state_set_f dfrgx_power_state_set;
};


/**
 * dfrgx_interface_set_data() - Provide some dfrgx data for hooks
 * inside the graphics driver.
 * Also, the symbol dependency will establish a load order dependency for
 * the case where both the graphics driver and the dfrgx driver are modules,
 * ensuring that the graphics driver is loaded and initialized before dfrgx.
 */
void dfrgx_interface_set_data(struct dfrgx_interface_s *gb_interface);


/**
 * dfrgx_interface_power_state_set() - Indicate that power is off (0) or on (1).
 * This is a hook called from the low-level device driver.
 */
void dfrgx_interface_power_state_set(int st_on);


#endif /* if !defined DFRGX_INTERFACE_H */
