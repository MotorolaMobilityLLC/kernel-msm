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
 *   Vinil Cheeramvelill <vinil.cheeramvelil@intel.com>
 */

#ifndef __DC_MAXFIFO_H__
#define __DC_MAXFIFO_H__

#include "psb_drv.h"


int dc_maxfifo_init(struct drm_device *dev);
void maxfifo_report_repeat_frame_interrupt(struct drm_device * dev);
bool enter_maxfifo_mode(struct drm_device *dev);
bool exit_maxfifo_mode(struct drm_device *dev);
bool enter_s0i1_display_mode(struct drm_device *dev);
bool exit_s0i1_display_mode(struct drm_device *dev);
void enable_repeat_frame_intr(struct drm_device *dev);
void disable_repeat_frame_intr(struct drm_device *dev);
#ifndef ENABLE_HW_REPEAT_FRAME
void maxfifo_timer_stop(struct drm_device *dev);
void maxfifo_timer_start(struct drm_device *dev);
#endif

int dc_maxfifo_uninit(struct drm_device *dev);

#endif
