/*
 * Copyright © 2010 Intel Corporation
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
 * Jackie Li<yaodong.li@intel.com>
 */
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/wait.h>

#include "psb_drv.h"
#include "mdfld_dsi_output.h"

struct mdfld_dsi_error_detector {
	struct drm_device *dev;

	struct task_struct *esd_thread;
	wait_queue_head_t esd_thread_wq;
};

int mdfld_dsi_error_detector_init(struct drm_device *dev,
	struct mdfld_dsi_connector *dsi_connector);
void mdfld_dsi_error_detector_exit(struct mdfld_dsi_connector *dsi_connector);
void mdfld_dsi_error_detector_wakeup(struct mdfld_dsi_connector *dsi_connector);

