/*
 * Copyright (c) 2010, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Authors:
 *	jim liu <jim.liu@intel.com>
 */

#include <drm/drmP.h>
#include <linux/kernel.h>
#include "psb_drv.h"
#include "psb_intel_reg.h"
#include "mdfld_hdmi_audio_if.h"
#include "android_hdmi.h"

#ifdef CONFIG_SUPPORT_HDMI

/*
 * Audio register range 0x69000 to 0x69117
 */

#define IS_HDMI_AUDIO_REG(reg) ((reg >= 0x69000) && (reg < 0x69118))

/*
 *
 */
static struct android_hdmi_priv *hdmi_priv;

static void hdmi_suspend_work(struct work_struct *work)
{
	struct android_hdmi_priv *hdmi_priv =
		container_of(work, struct android_hdmi_priv, suspend_wq);
	struct drm_device *dev = hdmi_priv->dev;

	android_hdmi_suspend_display(dev);
}

void mid_hdmi_audio_init(struct android_hdmi_priv *p_hdmi_priv)
{
	hdmi_priv = p_hdmi_priv;
	INIT_WORK(&hdmi_priv->suspend_wq, hdmi_suspend_work);
}

/*
 * return whether HDMI audio device is busy.
*/
bool mid_hdmi_audio_is_busy(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	int hdmi_audio_busy = 0;
	hdmi_audio_event_t hdmi_audio_event;

	if (hdmi_state == 0) {
		/* HDMI is not connected, assuming audio device is idle. */
		return false;
	}

	if (dev_priv->had_interface) {
		hdmi_audio_event.type = HAD_EVENT_QUERY_IS_AUDIO_BUSY;
		hdmi_audio_busy = dev_priv->had_interface->query(
			dev_priv->had_pvt_data,
			hdmi_audio_event);
		return hdmi_audio_busy != 0;
	}
	return false;
}

/*
 * return whether HDMI audio device is suspended.
*/
bool mid_hdmi_audio_suspend(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	hdmi_audio_event_t hdmi_audio_event;
	int ret = 0;

	PSB_DEBUG_ENTRY("%s: hdmi_state %d", __func__, hdmi_state);
	if (hdmi_state == 0) {
		/* HDMI is not connected,
		*assuming audio device is suspended already.
		*/
		return true;
	}

	if (dev_priv->had_interface) {
		hdmi_audio_event.type = 0;
		ret = dev_priv->had_interface->suspend(
						dev_priv->had_pvt_data,
						hdmi_audio_event);
		return (ret == 0) ? true : false;
	}
	return true;
}

void mid_hdmi_audio_resume(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	PSB_DEBUG_ENTRY("%s: hdmi_state %d", __func__, hdmi_state);
	if (hdmi_state == 0) {
		/* HDMI is not connected,
		*  there is no need to resume audio device.
		*/
		return;
	}

	if (dev_priv->had_interface)
		dev_priv->had_interface->resume(dev_priv->had_pvt_data);
}

void mid_hdmi_audio_signal_event(
						struct drm_device *dev,
						enum had_event_type event)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	if (dev_priv->mdfld_had_event_callbacks)
		(*dev_priv->mdfld_had_event_callbacks)
				(event, dev_priv->had_pvt_data);
}


/**
 * mid_hdmi_audio_write:
 * used to write into display controller HDMI audio registers.
 *
 */
static int mid_hdmi_audio_write(uint32_t reg, uint32_t val)
{
	struct drm_device *dev = hdmi_priv->dev;
	int ret = 0;

	if (hdmi_priv->monitor_type == MONITOR_TYPE_DVI)
		return 0;

	if (!is_island_on(OSPM_DISPLAY_B) || !is_island_on(OSPM_DISPLAY_HDMI))
		return 0;

	if (IS_HDMI_AUDIO_REG(reg))
		REG_WRITE(reg, val);
	else
		ret = -EINVAL;

	return ret;
}

/**
 * mid_hdmi_audio_read:
 * used to get the register value read
 * from display controller HDMI audio registers.
 *
 */
static int mid_hdmi_audio_read(uint32_t reg, uint32_t *val)
{
	struct drm_device *dev = hdmi_priv->dev;
	int ret = 0;

	if (hdmi_priv->monitor_type == MONITOR_TYPE_DVI)
		return 0;

	if (!is_island_on(OSPM_DISPLAY_B) || !is_island_on(OSPM_DISPLAY_HDMI))
		return 0;

	if (IS_HDMI_AUDIO_REG(reg))
		*val = REG_READ(reg);
	else
		ret = -EINVAL;

	return ret;
}

/**
 * mid_hdmi_audio_rmw:
 * used to update the masked bits in display
 * controller HDMI audio registers .
 *
 */
static int mid_hdmi_audio_rmw(uint32_t reg,
				uint32_t val, uint32_t mask)
{
	struct drm_device *dev = hdmi_priv->dev;
	int ret = 0;
	uint32_t val_tmp = 0;

	if (!is_island_on(OSPM_DISPLAY_B) || !is_island_on(OSPM_DISPLAY_HDMI))
		return 0;

	if (IS_HDMI_AUDIO_REG(reg)) {
		val_tmp = (val & mask) | (REG_READ(reg) & ~mask);
		REG_WRITE(reg, val_tmp);
	} else {
		ret = -EINVAL;
	}

	return ret;
}

/**
 * mid_hdmi_audio_get_caps:
 * used to return the HDMI audio capabilities.
 * e.g. resolution, frame rate.
 */
static int mid_hdmi_audio_get_caps(
						enum had_caps_list get_element,
						void *capabilities)
{
	struct drm_device *dev = hdmi_priv->dev;
	int ret = 0;

	PSB_DEBUG_ENTRY("\n");

	switch (get_element) {
	case HAD_GET_ELD:
		ret = android_hdmi_get_eld(dev, capabilities);
		break;
	case HAD_GET_SAMPLING_FREQ:
	{
		uint32_t val;
		val = android_hdmi_get_dpll_clock(dev);
		memcpy(capabilities, &val, sizeof(uint32_t));
		break;
	}
	default:
		break;
	}

	return ret;
}

/**
 * mid_hdmi_audio_set_caps:
 * used to set the HDMI audio capabilities.
 * e.g. Audio INT.
 */
static int mid_hdmi_audio_set_caps(
				enum had_caps_list set_element,
				void *capabilties)
{
	struct drm_device *dev = hdmi_priv->dev;
	struct drm_psb_private *dev_priv =
				(struct drm_psb_private *) dev->dev_private;
	int ret = 0;
	u32 hdmib;
	u32 int_masks = 0;

	PSB_DEBUG_ENTRY("\n");

	if (!is_island_on(OSPM_DISPLAY_B) || !is_island_on(OSPM_DISPLAY_HDMI))
		return -EINVAL;

	switch (set_element) {
	case HAD_SET_ENABLE_AUDIO:
		if (hdmi_priv->hdmi_audio_enabled) {
			pr_err("OSPM: %s: hdmi audio has been enabled\n", __func__);
			return 0;
		}

		hdmib = REG_READ(hdmi_priv->hdmib_reg);

		if (hdmib & HDMIB_PORT_EN)
			hdmib |= HDMIB_AUDIO_ENABLE;

		REG_WRITE(hdmi_priv->hdmib_reg, hdmib);
		REG_READ(hdmi_priv->hdmib_reg);
		hdmi_priv->hdmi_audio_enabled = true;
		break;
	case HAD_SET_DISABLE_AUDIO:
		if (!hdmi_priv->hdmi_audio_enabled) {
			pr_err("OSPM: %s: hdmi audio has been disabled\n", __func__);
			return 0;
		}
		hdmib = REG_READ(hdmi_priv->hdmib_reg) & ~HDMIB_AUDIO_ENABLE;
		REG_WRITE(hdmi_priv->hdmib_reg, hdmib);
		REG_READ(hdmi_priv->hdmib_reg);

		hdmi_priv->hdmi_audio_enabled = false;
		if (dev_priv->early_suspended) {
			/* suspend hdmi display if device has been suspended */
			schedule_work(&hdmi_priv->suspend_wq);
		}
		break;
	case HAD_SET_ENABLE_AUDIO_INT:
		if (*((u32 *)capabilties) & HDMI_AUDIO_UNDERRUN)
			int_masks |= PIPE_HDMI_AUDIO_UNDERRUN;

		if (*((u32 *)capabilties) & HDMI_AUDIO_BUFFER_DONE)
			int_masks |= PIPE_HDMI_AUDIO_BUFFER_DONE;

		dev_priv->hdmi_audio_interrupt_mask |= int_masks;
		mid_irq_enable_hdmi_audio(dev);
		break;
	case HAD_SET_DISABLE_AUDIO_INT:
		if (*((u32 *)capabilties) & HDMI_AUDIO_UNDERRUN)
			int_masks |= PIPE_HDMI_AUDIO_UNDERRUN;

		if (*((u32 *)capabilties) & HDMI_AUDIO_BUFFER_DONE)
			int_masks |= PIPE_HDMI_AUDIO_BUFFER_DONE;

		dev_priv->hdmi_audio_interrupt_mask &= ~int_masks;

		if (dev_priv->hdmi_audio_interrupt_mask)
			mid_irq_enable_hdmi_audio(dev);
		else
			mid_irq_disable_hdmi_audio(dev);
		break;
	default:
		break;
	}

	return ret;
}

static struct  hdmi_audio_registers_ops mid_hdmi_audio_reg_ops = {
	.hdmi_audio_read_register = mid_hdmi_audio_read,
	.hdmi_audio_write_register = mid_hdmi_audio_write,
	.hdmi_audio_read_modify = mid_hdmi_audio_rmw,
};

static struct hdmi_audio_query_set_ops mid_hdmi_audio_get_set_ops = {
	.hdmi_audio_get_caps = mid_hdmi_audio_get_caps,
	.hdmi_audio_set_caps = mid_hdmi_audio_set_caps,
};

int mid_hdmi_audio_setup(
	had_event_call_back audio_callbacks,
	struct hdmi_audio_registers_ops *reg_ops,
	struct hdmi_audio_query_set_ops *query_ops)
{
	struct drm_device *dev = hdmi_priv->dev;
	struct drm_psb_private *dev_priv =
				(struct drm_psb_private *) dev->dev_private;
	int ret = 0;

	reg_ops->hdmi_audio_read_register =
			(mid_hdmi_audio_reg_ops.hdmi_audio_read_register);
	reg_ops->hdmi_audio_write_register =
			(mid_hdmi_audio_reg_ops.hdmi_audio_write_register);
	reg_ops->hdmi_audio_read_modify =
			(mid_hdmi_audio_reg_ops.hdmi_audio_read_modify);
	query_ops->hdmi_audio_get_caps =
			mid_hdmi_audio_get_set_ops.hdmi_audio_get_caps;
	query_ops->hdmi_audio_set_caps =
			mid_hdmi_audio_get_set_ops.hdmi_audio_set_caps;

	dev_priv->mdfld_had_event_callbacks = audio_callbacks;

	return ret;
}
EXPORT_SYMBOL(mid_hdmi_audio_setup);

int mid_hdmi_audio_register(struct snd_intel_had_interface *driver,
							void *had_data)
{
	struct drm_device *dev = hdmi_priv->dev;
	struct drm_psb_private *dev_priv =
				(struct drm_psb_private *) dev->dev_private;
	dev_priv->had_pvt_data = had_data;
	dev_priv->had_interface = driver;

	if (hdmi_priv->monitor_type == MONITOR_TYPE_DVI)
		return 0;

	/* The Audio driver is loading now and we need to notify
	 * it if there is an HDMI device attached */
	DRM_INFO("%s: Scheduling HDMI audio work queue\n", __func__);
	schedule_work(&dev_priv->hdmi_audio_wq);

	return 0;
}
EXPORT_SYMBOL(mid_hdmi_audio_register);


#else /* CONFIG_SUPPORT_HDMI - HDMI is not supported. */

bool mid_hdmi_audio_is_busy(struct drm_device *dev)
{
	/* always in idle state */
	return false;
}

bool mid_hdmi_audio_suspend(struct drm_device *dev)
{
	/* always in suspend state */
	return true;
}

void mid_hdmi_audio_resume(struct drm_device *dev)
{
}

void mid_hdmi_audio_signal_event(struct drm_device *dev,
					enum had_event_type event)
{
}

void mid_hdmi_audio_init(struct android_hdmi_priv *hdmi_priv)
{
	DRM_INFO("%s: HDMI is not supported.\n", __func__);
}

int mid_hdmi_audio_setup(
	had_event_call_back audio_callbacks,
	struct hdmi_audio_registers_ops *reg_ops,
	struct hdmi_audio_query_set_ops *query_ops)
{
	DRM_ERROR("%s: HDMI is not supported.\n", __func__);
	return -ENODEV;
}
EXPORT_SYMBOL(mid_hdmi_audio_setup);

int mid_hdmi_audio_register(struct snd_intel_had_interface *driver,
				void *had_data)
{
	DRM_ERROR("%s: HDMI is not supported.\n", __func__);
	return -ENODEV;
}
EXPORT_SYMBOL(mid_hdmi_audio_register);

#endif
