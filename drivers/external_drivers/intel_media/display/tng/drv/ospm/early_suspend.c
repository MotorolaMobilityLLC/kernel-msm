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

#ifdef CONFIG_HAS_EARLYSUSPEND

#include <linux/earlysuspend.h>
#include <linux/mutex.h>
#include "psb_drv.h"
#include "early_suspend.h"
#include "android_hdmi.h"
#include "gfx_rtpm.h"

static struct drm_device *g_dev;

static void gfx_early_suspend(struct early_suspend *h)
{
	struct drm_psb_private *dev_priv = g_dev->dev_private;
	struct drm_device *dev = dev_priv->dev;
	struct drm_encoder *encoder;
	struct drm_encoder_helper_funcs *enc_funcs;

	PSB_DEBUG_PM("%s\n", __func__);

	flush_workqueue(dev_priv->power_wq);

	/* protect early_suspend with dpms and mode config */
	mutex_lock(&dev->mode_config.mutex);

	list_for_each_entry(encoder,
			&dev->mode_config.encoder_list,
			head) {
		enc_funcs = encoder->helper_private;
		if (!drm_helper_encoder_in_use(encoder))
			continue;
		if (enc_funcs && enc_funcs->save)
			enc_funcs->save(encoder);

		if (encoder->encoder_type == DRM_MODE_ENCODER_TMDS) {
			DCLockMutex();
			drm_handle_vblank(dev, 1);

			/* Turn off vsync interrupt. */
			drm_vblank_off(dev, 1);

			/* Make the pending flip request as completed. */
			DCUnAttachPipe(1);
			DC_MRFLD_onPowerOff(1);
			DCUnLockMutex();
		}
	}

	/* Suspend hdmi
	 * Note: hotplug detection is disabled if audio is not playing
	 */
	android_hdmi_suspend_display(dev);

	ospm_power_suspend();
	dev_priv->early_suspended = true;

	mutex_unlock(&dev->mode_config.mutex);
}

static void gfx_late_resume(struct early_suspend *h)
{
	struct drm_psb_private *dev_priv = g_dev->dev_private;
	struct drm_device *dev = dev_priv->dev;
	struct drm_encoder *encoder;
	struct drm_encoder_helper_funcs *enc_funcs;

	PSB_DEBUG_PM("%s\n", __func__);

	/* protect early_suspend with dpms and mode config */
	mutex_lock(&dev->mode_config.mutex);

	dev_priv->early_suspended = false;
	ospm_power_resume();

	list_for_each_entry(encoder,
			&dev->mode_config.encoder_list,
			head) {
		enc_funcs = encoder->helper_private;
		if (!drm_helper_encoder_in_use(encoder))
			continue;
		if (enc_funcs && enc_funcs->save)
			enc_funcs->restore(encoder);
	}

	/* Resume HDMI */
	android_hdmi_resume_display(dev);

	/*
	 * Devices connect status will be changed
	 * when system suspend,re-detect once here.
	 */
	if (android_hdmi_is_connected(dev)) {
		DCAttachPipe(1);
		DC_MRFLD_onPowerOn(1);
		mid_hdmi_audio_resume(dev);
	}

	mutex_unlock(&dev->mode_config.mutex);
}

static struct early_suspend intel_media_early_suspend = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
	.suspend = gfx_early_suspend,
	.resume = gfx_late_resume,
};

void intel_media_early_suspend_init(struct drm_device *dev)
{
	g_dev = dev;
	register_early_suspend(&intel_media_early_suspend);
}

void intel_media_early_suspend_uninit(void)
{
	unregister_early_suspend(&intel_media_early_suspend);
}

#endif
