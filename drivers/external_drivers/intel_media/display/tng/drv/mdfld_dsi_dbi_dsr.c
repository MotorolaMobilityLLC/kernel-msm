/*
 * Copyright © 2012 Intel Corporation
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
#include "mdfld_dsi_dbi.h"
#include "mdfld_dsi_dbi_dsr.h"
#include "mdfld_dsi_pkg_sender.h"

#define DSR_COUNT 15

static int exit_dsr_locked(struct mdfld_dsi_config *dsi_config)
{
	int err = 0;
	struct drm_device *dev;
	if (!dsi_config)
		return -EINVAL;

	dev = dsi_config->dev;
	err =  __dbi_power_on(dsi_config);

	DC_MRFLD_onPowerOn(dsi_config->pipe);

	return err;
}

static int enter_dsr_locked(struct mdfld_dsi_config *dsi_config, int level)
{
	struct mdfld_dsi_hw_registers *regs;
	struct mdfld_dsi_hw_context *ctx;
	struct drm_psb_private *dev_priv;
	struct drm_device *dev;
	struct mdfld_dsi_pkg_sender *sender;
	int err;
	pm_message_t state;

	PSB_DEBUG_ENTRY("mdfld_dsi_dsr: enter dsr\n");

	if (!dsi_config)
		return -EINVAL;

	regs = &dsi_config->regs;
	ctx = &dsi_config->dsi_hw_context;
	dev = dsi_config->dev;
	dev_priv = dev->dev_private;

	sender = mdfld_dsi_get_pkg_sender(dsi_config);
	if (!sender) {
		DRM_ERROR("Failed to get dsi sender\n");
		return -EINVAL;
	}

	if (level < DSR_EXITED) {
		DRM_ERROR("Why to do this?");
		return -EINVAL;
	}

	if (level > DSR_ENTERED_LEVEL0) {
		/**
		 * TODO: require OSPM interfaces to tell OSPM module that
		 * display controller is ready to be power gated.
		 * OSPM module needs to response this request ASAP.
		 * NOTE: it makes no sense to have display controller islands
		 * & pci power gated here directly. OSPM module is the only one
		 * who can power gate/ungate power islands.
		 * FIXME: since there's no ospm interfaces for acquiring
		 * suspending DSI related power islands, we have to call OSPM
		 * interfaces to power gate display islands and pci right now,
		 * which should NOT happen in this way!!!
		 */
		if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND,
			OSPM_UHB_FORCE_POWER_ON)) {
			DRM_ERROR("Failed power on display island\n");
			return -EINVAL;
		}

		PSB_DEBUG_ENTRY("mdfld_dsi_dsr: entering DSR level 1\n");

		err = mdfld_dsi_wait_for_fifos_empty(sender);
		if (err) {
			DRM_ERROR("mdfld_dsi_dsr: FIFO not empty\n");
			ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
			return err;
		}
		ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);

		/*suspend whole PCI host and related islands
		** if failed at this try, revive te for another chance
		*/
		state.event = 0;
		if (ospm_power_suspend()) {
			/* Only display island is powered off then
			 ** need revive the whole TE
			 */
			if (!ospm_power_is_hw_on(OSPM_DISPLAY_ISLAND))
				exit_dsr_locked(dsi_config);

			return -EINVAL;
		}
		/*
		 *suspend pci
		 *FIXME: should I do it here?
		 *how about decoder/encoder is working??
		 *OSPM should check the refcout of each islands before
		 *actually power off PCI!!!
		 *need invoke this in the same context, we need deal with
		 *DSR lock later for suspend PCI may go to sleep!!!
		 */
		/*ospm_suspend_pci(dev->pdev);*/

		PSB_DEBUG_ENTRY("mdfld_dsi_dsr: entered\n");
		return 0;
	}

	PSB_DEBUG_ENTRY("mdfld_dsi_dsr: entering DSR level 0\n");

	err = mdfld_dsi_wait_for_fifos_empty(sender);
	if (err) {
		DRM_ERROR("mdfld_dsi_dsr: FIFO not empty\n");
		return err;
	}

	/*
	 * To set the vblank_enabled to false with drm_vblank_off(), as
	 * vblank_disable_and_save() would be scheduled late (<= 5s), and it
	 * would cause drm_vblank_get() fail to turn on vsync interrupt
	 * immediately.
	 */
	drm_vblank_off(dev, dsi_config->pipe);

	DC_MRFLD_onPowerOff(dsi_config->pipe);

	/*turn off dbi interface put in ulps*/
	__dbi_power_off(dsi_config);

	PSB_DEBUG_ENTRY("entered\n");
	return 0;
}

static void dsr_power_off_work(struct work_struct *work)
{
	DRM_INFO("mdfld_dsi_dsr: power off work\n");
}

static void dsr_power_on_work(struct work_struct *work)
{
	DRM_INFO("mdfld_dsi_dsr: power on work\n");
}

int mdfld_dsi_dsr_update_panel_fb(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_dsr *dsr;
	struct mdfld_dsi_pkg_sender *sender;
	int err = 0;

	if (!dsi_config) {
		DRM_ERROR("Invalid parameter\n");
		return -EINVAL;
	}

	dsr = dsi_config->dsr;

	if (!IS_ANN(dev)) {
		/*if no dsr attached, return 0*/
		if (!dsr)
			return 0;
	}

	PSB_DEBUG_ENTRY("\n");

	if (dsi_config->type == MDFLD_DSI_ENCODER_DPI)
		return 0;
	mutex_lock(&dsi_config->context_lock);

	if (!dsi_config->dsi_hw_context.panel_on) {
		PSB_DEBUG_ENTRY(
		"if screen off, update fb is not allowed\n");
		err = -EINVAL;
		goto update_fb_out;
	}

	/*no pending fb updates, go ahead to send out write_mem_start*/
	PSB_DEBUG_ENTRY("send out write_mem_start\n");
	sender = mdfld_dsi_get_pkg_sender(dsi_config);
	if (!sender) {
		DRM_ERROR("No sender\n");
		err = -EINVAL;
		goto update_fb_out;
	}

	err = mdfld_dsi_send_dcs(sender, write_mem_start,
				NULL, 0, CMD_DATA_SRC_PIPE,
				MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("Failed to send write_mem_start");
		err = -EINVAL;
		goto update_fb_out;
	}

	/*clear free count*/
	dsr->free_count = 0;

update_fb_out:
	mutex_unlock(&dsi_config->context_lock);
	return err;
}

int mdfld_dsi_dsr_report_te(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_dsr *dsr;
	struct drm_psb_private *dev_priv;
	struct drm_device *dev;
	int err = 0;
	int dsr_level;

	if (!dsi_config) {
		DRM_ERROR("Invalid parameter\n");
		return -EINVAL;
	}

	dsr = dsi_config->dsr;

	/*if no dsr attached, return 0*/
	if (!dsr)
		return 0;

	/*
	 * TODO: check HDMI & WIDI connection state here, then setup
	 * dsr_level accordingly.
	 */
	dev = dsi_config->dev;
	dev_priv = dev->dev_private;

	/*
	 * FIXME: when hdmi connected with no audio output, we still can
	 * power gate DSI related islands, how to check whether HDMI audio
	 * is active or not.
	 * Currently, we simply enter DSR LEVEL0 when HDMI is connected
	 */
	dsr_level = DSR_ENTERED_LEVEL0;

	mutex_lock(&dsi_config->context_lock);

	if (!dsr->dsr_enabled)
		goto report_te_out;

	/*if panel is off, then forget it*/
	if (!dsi_config->dsi_hw_context.panel_on)
		goto report_te_out;

	if (dsr_level <= dsr->dsr_state)
		goto report_te_out;
	else if (++dsr->free_count > DSR_COUNT && !dsr->ref_count) {
		/*reset free count*/
		dsr->free_count = 0;
		/*enter dsr*/
		err = enter_dsr_locked(dsi_config, dsr_level);
		if (err) {
			PSB_DEBUG_ENTRY("Failed to enter DSR\n");
			goto report_te_out;
		}
		dsr->dsr_state = dsr_level;
	}
report_te_out:
	mutex_unlock(&dsi_config->context_lock);
	return err;
}

int mdfld_dsi_dsr_forbid_locked(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_dsr *dsr;
	int err = 0;

	if (!dsi_config) {
		DRM_ERROR("Invalid parameter\n");
		return -EINVAL;
	}

	dsr = dsi_config->dsr;

	/*if no dsr attached, return 0*/
	if (!dsr) {
		return 0;
	}
	/*exit dsr if necessary*/
	if (!dsr->dsr_enabled)
		goto forbid_out;

	PSB_DEBUG_ENTRY("\n");

	/*if reference count is not 0, it means dsr was forbidden*/
	if (dsr->ref_count) {
		dsr->ref_count++;
		goto forbid_out;
	}

	/*exited dsr if current dsr state is DSR_ENTERED*/
	if (dsr->dsr_state > DSR_EXITED) {
		err = exit_dsr_locked(dsi_config);
		if (err) {
			DRM_ERROR("Failed to exit DSR\n");
			goto forbid_out;
		}
		dsr->dsr_state = DSR_EXITED;
	}
	dsr->ref_count++;
forbid_out:
	return err;
}

int mdfld_dsi_dsr_forbid(struct mdfld_dsi_config *dsi_config)
{
	int err = 0;

	if (!dsi_config)
		return -EINVAL;

	mutex_lock(&dsi_config->context_lock);

	err = mdfld_dsi_dsr_forbid_locked(dsi_config);

	mutex_unlock(&dsi_config->context_lock);

	return err;
}

int mdfld_dsi_dsr_allow_locked(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_dsr *dsr;

	if (!dsi_config) {
		DRM_ERROR("Invalid parameter\n");
		return -EINVAL;
	}

	dsr = dsi_config->dsr;

	/*if no dsr attached, return 0*/
	if (!dsr) {
		return 0;
	}

	if (!dsr->dsr_enabled)
		goto allow_out;

	if (!dsr->ref_count) {
		DRM_ERROR("Reference count is 0\n");
		goto allow_out;
	}

	PSB_DEBUG_ENTRY("\n");

	dsr->ref_count--;
allow_out:
	return 0;
}

int mdfld_dsi_dsr_allow(struct mdfld_dsi_config *dsi_config)
{
	int err = 0;

	if (!dsi_config)
		return -EINVAL;

	mutex_lock(&dsi_config->context_lock);

	err = mdfld_dsi_dsr_allow_locked(dsi_config);

	mutex_unlock(&dsi_config->context_lock);

	return err;
}

void mdfld_dsi_dsr_enable(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_dsr *dsr;

	PSB_DEBUG_ENTRY("\n");

	if (!dsi_config) {
		DRM_ERROR("Invalid parameter\n");
		return;
	}

	dsr = dsi_config->dsr;

	/*if no dsr attached, return 0*/
	if (!dsr)
		return;

	/*lock dsr*/
	mutex_lock(&dsi_config->context_lock);

	dsr->dsr_enabled = 1;
	dsr->dsr_state = DSR_EXITED;

	mutex_unlock(&dsi_config->context_lock);
}

int mdfld_dsi_dsr_in_dsr_locked(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_dsr *dsr;
	int in_dsr = 0;

	PSB_DEBUG_ENTRY("\n");

	if (!dsi_config) {
		DRM_ERROR("Invalid parameter\n");
		goto get_state_out;
	}

	dsr = dsi_config->dsr;

	/*if no dsr attached, return 0*/
	if (!dsr)
		goto get_state_out;

	if (dsr->dsr_state > DSR_EXITED)
		in_dsr = 1;

get_state_out:
	return in_dsr;
}

/**
 * init dsr structure
 */
int mdfld_dsi_dsr_init(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_dsr *dsr;

	PSB_DEBUG_ENTRY("\n");

	if (!dsi_config) {
		DRM_ERROR("Invalid parameter\n");
		return -EINVAL;
	}

	/*check panel type*/
	if (dsi_config->type == MDFLD_DSI_ENCODER_DPI) {
		DRM_INFO("%s: Video mode panel, disabling DSR\n", __func__);
		return 0;
	}

	dsr = kzalloc(sizeof(struct mdfld_dsi_dsr), GFP_KERNEL);
	if (!dsr) {
		DRM_ERROR("No memory\n");
		return -ENOMEM;
	}

	/*init reference count*/
	dsr->ref_count = 0;

	/*init free count*/
	dsr->free_count = 0;

	/*init dsr enabled*/
	dsr->dsr_enabled = 0;

	/*set dsr state*/
	dsr->dsr_state = DSR_INIT;

	/*init power on/off works*/
	INIT_WORK(&dsr->power_off_work, dsr_power_off_work);
	INIT_WORK(&dsr->power_on_work, dsr_power_on_work);

	/*init dsi config*/
	dsr->dsi_config = dsi_config;

	dsi_config->dsr = dsr;

	PSB_DEBUG_ENTRY("successfully\n");

	return 0;
}

/**
 * destroy dsr structure
 */
void mdfld_dsi_dsr_destroy(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_dsr *dsr;

	PSB_DEBUG_ENTRY("\n");

	dsr = dsi_config->dsr;

	if (!dsr)
		kfree(dsr);

	dsi_config->dsr = 0;
}
