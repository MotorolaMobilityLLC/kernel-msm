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


#include <linux/version.h>
#include <linux/gfp.h>
#include <linux/sched.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0))
#include <linux/sched/rt.h>
#endif
#include "mdfld_dsi_esd.h"
#include "mdfld_dsi_dbi.h"
#include "mdfld_dsi_pkg_sender.h"
#include "mdfld_dsi_dbi_dsr.h"

#define MDFLD_ESD_SLEEP_MSECS	8000

/**
 * esd detection
 */
static bool intel_dsi_dbi_esd_detection(struct mdfld_dsi_config *dsi_config)
{
	int ret;
	u32 data = 0;

	PSB_DEBUG_ENTRY("esd\n");

	ret = mdfld_dsi_get_power_mode(dsi_config,
			(u8 *) &data,
			MDFLD_DSI_HS_TRANSMISSION);
	/**
	 * if FIFO is not empty, need do ESD, ret equals -EIO means
	 * FIFO is abnormal.
	 */
	if ((ret == -EIO) || ((ret == 1) && ((data & 0x14) != 0x14)))
		return true;

	return false;
}

static int __esd_thread(void *data)
{
	struct mdfld_dsi_dbi_output *dbi_output = NULL;
	struct panel_funcs *p_funcs  = NULL;
	struct mdfld_dsi_config *dsi_config;
	struct mdfld_dsi_error_detector *err_detector =
		(struct mdfld_dsi_error_detector *)data;
	struct drm_device *dev = err_detector->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;

	dsi_config = dev_priv->dsi_configs[0];
	if (!dsi_config)
		return -EINVAL;

	set_freezable();

	while (!kthread_should_stop()) {
		wait_event_freezable(err_detector->esd_thread_wq,
			(dsi_config->dsi_hw_context.panel_on ||
			 kthread_should_stop()));

		dbi_output = dev_priv->dbi_output;

		if (!dbi_output)
			goto esd_exit;

		mutex_lock(&dsi_config->context_lock);

		p_funcs = dbi_output->p_funcs;
		if (dsi_config->dsi_hw_context.panel_on &&
			!mdfld_dsi_dsr_in_dsr_locked(dsi_config)) {
			/*forbid DSR during detection & resume*/
			mdfld_dsi_dsr_forbid_locked(dsi_config);

			if (intel_dsi_dbi_esd_detection(dsi_config)) {
				DRM_INFO("%s: error detected\n", __func__);
				schedule_work(&dev_priv->reset_panel_work);
			}

			mdfld_dsi_dsr_allow_locked(dsi_config);
		}
		mutex_unlock(&dsi_config->context_lock);
esd_exit:
		schedule_timeout_interruptible(
			msecs_to_jiffies(MDFLD_ESD_SLEEP_MSECS));
	}

	DRM_INFO("%s: ESD exited\n", __func__);
	return 0;
}

/**
 * Wake up the error detector
 */
void mdfld_dsi_error_detector_wakeup(struct mdfld_dsi_connector *dsi_connector)
{
	struct mdfld_dsi_error_detector *err_detector;

	if (!dsi_connector || !dsi_connector->err_detector)
		return;

	err_detector = dsi_connector->err_detector;
	wake_up_interruptible(&err_detector->esd_thread_wq);
}

/**
 * @dev: DRM device
 * @dsi_connector:
 *
 * Initialize DSI error detector
 */
int mdfld_dsi_error_detector_init(struct drm_device *dev,
		struct mdfld_dsi_connector *dsi_connector)
{
	struct mdfld_dsi_error_detector *err_detector;
	const char *fmt = "%s";
	struct task_struct *p;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1};

	if (!dsi_connector || !dev) {
		DRM_ERROR("Invalid parameter\n");
		return -EINVAL;
	}

	if (dsi_connector->err_detector)
		return 0;

	/*create a new error detector*/
	err_detector = kzalloc(sizeof(struct mdfld_dsi_error_detector),
				GFP_KERNEL);
	if (!err_detector) {
		DRM_ERROR("Failed to allocate ESD\n");
		return -ENOMEM;
	}

	/*init detector thread wait queue*/
	init_waitqueue_head(&err_detector->esd_thread_wq);

	/*init detector thread*/
	p = kthread_create(__esd_thread, err_detector, fmt, "dsi_esd", 0);
	if (IS_ERR(p)) {
		DRM_ERROR("Failed to create ESD thread\n");
		goto esd_thread_err;
	}
	/*use FIFO scheduler*/
	sched_setscheduler_nocheck(p, SCHED_FIFO, &param);

	err_detector->esd_thread = p;
	err_detector->dev = dev;

	/*attach it to connector*/
	dsi_connector->err_detector = err_detector;

	/*time to start detection*/
	wake_up_process(p);

	DRM_INFO("%s: started\n", __func__);

	return 0;
esd_thread_err:
	kfree(err_detector);
	return -EAGAIN;
}

void mdfld_dsi_error_detector_exit(struct mdfld_dsi_connector *dsi_connector)
{
	struct mdfld_dsi_error_detector *err_detector;

	if (!dsi_connector || !dsi_connector->err_detector)
		return;

	err_detector = dsi_connector->err_detector;

	/*stop & destroy detector thread*/
	if (err_detector->esd_thread) {
		kthread_stop(err_detector->esd_thread);
		err_detector->esd_thread = NULL;
	}

	/*delete it*/
	kfree(err_detector);

	dsi_connector->err_detector = NULL;
}
