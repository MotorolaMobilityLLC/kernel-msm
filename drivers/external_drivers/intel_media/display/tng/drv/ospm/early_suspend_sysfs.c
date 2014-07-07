/**************************************************************************
 * Copyright (c) 2014, Intel Corporation.
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
 *    Sathya Kuppuswamy <sathyanarayanan.kuppuswamy@intel.com>
 */

#include <linux/mutex.h>
#include <linux/early_suspend_sysfs.h>
#include "psb_drv.h"
#include "early_suspend.h"
#include "android_hdmi.h"
#include "gfx_rtpm.h"

static struct drm_device *g_dev;

static void gfx_early_suspend(void)
{
	struct drm_psb_private *dev_priv = g_dev->dev_private;
	struct drm_device *dev = dev_priv->dev;
	struct drm_encoder *encoder;
	struct drm_encoder_helper_funcs *enc_funcs;

	PSB_DEBUG_PM("%s\n", __func__);

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

static void gfx_late_resume(void)
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

static ssize_t early_suspend_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (!strncmp(buf, EARLY_SUSPEND_ON, EARLY_SUSPEND_STATUS_LEN))
		gfx_early_suspend();
	else if (!strncmp(buf, EARLY_SUSPEND_OFF, EARLY_SUSPEND_STATUS_LEN))
		gfx_late_resume();

	return count;
}

static DEVICE_EARLY_SUSPEND_ATTR(early_suspend_store);

void intel_media_early_suspend_sysfs_init(struct drm_device *dev)
{
	g_dev = dev;
	device_create_file(&dev->pdev->dev, &dev_attr_early_suspend);
	register_early_suspend_device(&dev->pdev->dev);
}

void intel_media_early_suspend_sysfs_uninit(struct drm_device *dev)
{
	device_remove_file(&dev->pdev->dev, &dev_attr_early_suspend);
	unregister_early_suspend_device(&dev->pdev->dev);
}
