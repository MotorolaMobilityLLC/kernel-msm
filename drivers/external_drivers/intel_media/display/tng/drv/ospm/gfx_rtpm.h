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

#ifndef _INTEL_MEDIA_RUNTIME_PM_H_
#define _INTEL_MEDIA_RUNTIME_PM_H_

#include <linux/types.h>
#include <drm/drmP.h>

void rtpm_init(struct drm_device *dev);
void rtpm_uninit(struct drm_device *dev);

/*
* GFX-Runtime PM callbacks
*/
int rtpm_suspend(struct device *dev);
int rtpm_resume(struct device *dev);
int rtpm_idle(struct device *dev);
int rtpm_allow(struct drm_device *dev);
void rtpm_forbid(struct drm_device *dev);
void rtpm_suspend_pci(void);
void rtpm_resume_pci(void);
#endif /* _INTEL_MEDIA_RUNTIME_PM_H_ */
