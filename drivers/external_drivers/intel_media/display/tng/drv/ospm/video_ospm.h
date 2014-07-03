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
 *    Hitesh K. Patel <hitesh.k.patel@intel.com>
 */

#ifndef _INTEL_MEDIA_VIDEO_OSPM_H_
#define _INTEL_MEDIA_VIDEO_OSPM_H_

#include "pwr_mgmt.h"
#include "psb_msvdx.h"
#include "vsp.h"

#define PMU_VPP			0x1
#define PMU_ENC			0x1
#define PMU_DEC			0x1

static bool  need_set_ved_freq = true;

void ospm_vsp_init(struct drm_device *dev,
			struct ospm_power_island *p_island);

void ospm_ved_init(struct drm_device *dev,
			struct ospm_power_island *p_island);

void ospm_vec_init(struct drm_device *dev,
			struct ospm_power_island *p_island);

int psb_msvdx_get_ved_freq(u32 reg_freq);

int psb_msvdx_set_ved_freq(u32 freq_code);

void psb_set_freq_control_switch(bool config_value);

#endif	/* _INTEL_MEDIA_VIDEO_OSPM_H_*/
