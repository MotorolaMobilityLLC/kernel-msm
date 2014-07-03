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
 *    Javier Torres Castillo <javier.torres.castillo@intel.com>
 */
#if !defined DEVFREQ_DEBUG_H
#define  DEVFREQ_DEBUG_H
#include <linux/kernel.h>
#define DF_RGX_DEV    "dfrgx"
#define DFRGX_ALERT    KERN_ALERT DF_RGX_DEV ": "
#define DFRGX_DEBUG_MASK	0x01
#define DFRGX_DEBUG_HIGH	0x01
#define DFRGX_DEBUG_MED		0x02
#define DFRGX_DEBUG_LOW		0x04

#define DFRGX_DEBUG 0

#if (defined DFRGX_DEBUG) && DFRGX_DEBUG
#define DFRGX_DPF(mask, ...) if (mask & DFRGX_DEBUG_MASK) \
		{ \
			printk(DFRGX_ALERT __VA_ARGS__); \
		}
#else
#define DFRGX_DPF(mask, ...)
#endif
#endif /*DEVFREQ_DEBUG_H*/

