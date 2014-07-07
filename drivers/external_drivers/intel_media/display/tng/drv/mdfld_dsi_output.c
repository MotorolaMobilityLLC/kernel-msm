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
 * jim liu <jim.liu@intel.com>
 * Jackie Li<yaodong.li@intel.com>
 */

#include "mdfld_dsi_output.h"
#include "mdfld_dsi_dbi.h"
#include "mdfld_dsi_dpi.h"
#include "mdfld_output.h"
#include <asm/intel_scu_ipc.h>
#include "mdfld_dsi_pkg_sender.h"
#include <linux/freezer.h>
#include "psb_drv.h"
#include "mdfld_dsi_esd.h"
#include "mdfld_dsi_dbi_dsr.h"

#define ACTUAL_DRAIN_RATE_7x12 75
#define ACTUAL_DRAIN_RATE_10x19 150
#define ACTUAL_DRAIN_RATE_25x16 300
#define HDMI_SPRITE_DEADLINE 0x8D
#define HDMI_OVERLAY_DEADLINE 0xA4

#define KEEP_UNUSED_CODE 0

#define MDFLD_DSI_BRIGHTNESS_MAX_LEVEL 100

/* get the CABC LABC from command line. */
static int CABC_control = 1;
static int LABC_control = 1;

#ifdef MODULE
module_param(CABC_control, int, 0644);
module_param(LABC_control, int, 0644);
#else
static int __init parse_CABC_control(char *arg)
{
	/* CABC control can be passed in as a cmdline parameter */
	/* to enable this feature add CABC=1 to cmdline */
	/* to disable this feature add CABC=0 to cmdline */
	if (!arg)
		return -EINVAL;

	if (!strcasecmp(arg, "0"))
		CABC_control = 0;
	else if (!strcasecmp(arg, "1"))
		CABC_control = 1;

	return 0;
}
early_param("CABC", parse_CABC_control);

static int __init parse_LABC_control(char *arg)
{
	/* LABC control can be passed in as a cmdline parameter */
	/* to enable this feature add LABC=1 to cmdline */
	/* to disable this feature add LABC=0 to cmdline */
	if (!arg)
		return -EINVAL;

	if (!strcasecmp(arg, "0"))
		LABC_control = 0;
	else if (!strcasecmp(arg, "1"))
		LABC_control = 1;

	return 0;
}
early_param("LABC", parse_LABC_control);
#endif

#if KEEP_UNUSED_CODE
/**
 * make these MCS command global 
 * we don't need 'movl' everytime we send them.
 * FIXME: these datas were provided by OEM, we should get them from GCT.
 **/
static u32 mdfld_dbi_mcs_hysteresis[] = {
	0x42000f57, 0x8c006400, 0xff00bf00, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0x38000aff, 0x82005000, 0xff00ab00, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0x000000ff,
};
#endif /* if KEEP_UNUSED_CODE */

#if KEEP_UNUSED_CODE
static u32 mdfld_dbi_mcs_display_profile[] = {
	0x50281450, 0x0000c882, 0x00000000, 0x00000000,
	0x00000000,
};
#endif /* if KEEP_UNUSED_CODE */

#if KEEP_UNUSED_CODE
static u32 mdfld_dbi_mcs_kbbc_profile[] = {
	0x00ffcc60, 0x00000000, 0x00000000, 0x00000000,
};
#endif /* if KEEP_UNUSED_CODE */

#if KEEP_UNUSED_CODE
static u32 mdfld_dbi_mcs_gamma_profile[] = {
	0x81111158, 0x88888888, 0x88888888,
};
#endif /* if KEEP_UNUSED_CODE */

#if KEEP_UNUSED_CODE
/**
 * write hysteresis values.
 */
static void mdfld_dsi_write_hysteresis(struct mdfld_dsi_config *dsi_config,
		int pipe)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);

	if (!sender) {
		DRM_ERROR("No sender found\n");
		return;
	}

	mdfld_dsi_send_mcs_long_hs(sender,
			(u8 *) mdfld_dbi_mcs_hysteresis,
			68,
			MDFLD_DSI_SEND_PACKAGE);
}
#endif /* if KEEP_UNUSED_CODE */

#if KEEP_UNUSED_CODE
/**
 * write display profile values.
 */
static void mdfld_dsi_write_display_profile(struct mdfld_dsi_config *dsi_config,
		int pipe)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);

	if (!sender) {
		DRM_ERROR("No sender found\n");
		return;
	}

	mdfld_dsi_send_mcs_long_hs(sender,
			(u8 *) mdfld_dbi_mcs_display_profile,
			20,
			MDFLD_DSI_SEND_PACKAGE);
}
#endif /* if KEEP_UNUSED_CODE */

#if KEEP_UNUSED_CODE
/**
 * write KBBC profile values.
 */
static void mdfld_dsi_write_kbbc_profile(struct mdfld_dsi_config *dsi_config,
		int pipe)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);

	if (!sender) {
		DRM_ERROR("No sender found\n");
		return;
	}

	mdfld_dsi_send_mcs_long_hs(sender,
			(u8 *) mdfld_dbi_mcs_kbbc_profile,
			20,
			MDFLD_DSI_SEND_PACKAGE);
}
#endif /* if KEEP_UNUSED_CODE */

#if KEEP_UNUSED_CODE
/**
 * write gamma setting.
 */
static void mdfld_dsi_write_gamma_setting(struct mdfld_dsi_config *dsi_config,
		int pipe)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);

	if (!sender) {
		DRM_ERROR("No sender found\n");
		return;
	}

	mdfld_dsi_send_mcs_long_hs(sender,
			(u8 *) mdfld_dbi_mcs_gamma_profile,
			3,
			MDFLD_DSI_SEND_PACKAGE);
}
#endif /* if KEEP_UNUSED_CODE */

/**
 * Check and see if the generic control or data buffer is empty and ready.
 */
void mdfld_dsi_gen_fifo_ready(struct drm_device *dev, u32 gen_fifo_stat_reg,
		u32 fifo_stat)
{
	u32 GEN_BF_time_out_count = 0;

	/* Check MIPI Adatper command registers */
	for (GEN_BF_time_out_count = 0; GEN_BF_time_out_count < GEN_FB_TIME_OUT;
			GEN_BF_time_out_count++) {
		if ((REG_READ(gen_fifo_stat_reg) & fifo_stat) == fifo_stat)
			break;
		udelay(100);
	}

	if (GEN_BF_time_out_count == GEN_FB_TIME_OUT)
		DRM_ERROR("%s: Timeout. gen_fifo_stat_reg = 0x%x.\n", __func__,
				gen_fifo_stat_reg);
}

/**
 * Manage the mipi display brightness.
 * TODO: refine this interface later
 */
void mdfld_dsi_brightness_control(struct drm_device *dev, int pipe, int level)
{
	struct drm_psb_private *dev_priv;
	struct mdfld_dsi_config *dsi_config;
	struct mdfld_dsi_dpi_output *dpi_output;
	struct mdfld_dsi_dbi_output *dbi_output;
	struct mdfld_dsi_encoder *encoder;
	struct panel_funcs *p_funcs;
	u32 power_island = 0;

	if (!dev || (pipe != 0 && pipe != 2)) {
		DRM_ERROR("Invalid parameter\n");
		return;
	}

	dev_priv = dev->dev_private;

	if (pipe)
		dsi_config = dev_priv->dsi_configs[1];
	else
		dsi_config = dev_priv->dsi_configs[0];

	if (!dsi_config) {
		PSB_DEBUG_ENTRY("No dsi config found on pipe %d\n", pipe);
		return;
	}

	encoder = dsi_config->encoders[dsi_config->type];

	if (dsi_config->type == MDFLD_DSI_ENCODER_DBI) {
		dbi_output = MDFLD_DSI_DBI_OUTPUT(encoder);
		p_funcs = dbi_output ? dbi_output->p_funcs : NULL;
	} else if (dsi_config->type == MDFLD_DSI_ENCODER_DPI) {
		dpi_output = MDFLD_DSI_DPI_OUTPUT(encoder);
		p_funcs = dpi_output ? dpi_output->p_funcs : NULL;
	} else {
		DRM_ERROR("Invalid parameter\n");
		return;
	}

	if (!p_funcs || !p_funcs->set_brightness) {
		DRM_INFO("Cannot set panel brightness\n");
		return;
	}

	power_island = pipe_to_island(dsi_config->pipe);

	if (power_island & (OSPM_DISPLAY_A | OSPM_DISPLAY_C))
		power_island |= OSPM_DISPLAY_MIO;

	if (!power_island_get(power_island))
		return;

	mutex_lock(&dsi_config->context_lock);

	mdfld_dsi_dsr_forbid_locked(dsi_config);

	if (!dsi_config->dsi_hw_context.panel_on)
		goto set_brightness_out;

	if (p_funcs->set_brightness(dsi_config, level))
		DRM_ERROR("Failed to set panel brightness\n");

set_brightness_out:
	mdfld_dsi_dsr_allow_locked(dsi_config);
	mutex_unlock(&dsi_config->context_lock);
	power_island_put(power_island);
}

int mdfld_dsi_get_panel_status(struct mdfld_dsi_config *dsi_config,
		u8 dcs,
		u8 *data,
		u8 transmission,
		u32 len)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	int ret = 0;

	if (!sender || !data) {
		DRM_ERROR("Invalid parameter\n");
		return -EINVAL;
	}

	if (transmission == MDFLD_DSI_HS_TRANSMISSION) {
		ret = mdfld_dsi_read_mcs_hs(sender, dcs, data, len);
		if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
			ret = -EIO;
		return ret;
	} else if (transmission == MDFLD_DSI_LP_TRANSMISSION) {
		ret = mdfld_dsi_read_mcs_lp(sender, dcs, data, len);
		if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
			ret = -EIO;
		return ret;
	} else
		return -EINVAL;
}

int mdfld_dsi_get_power_mode(struct mdfld_dsi_config *dsi_config,
		u8 *mode,
		u8 transmission)
{
	if (!dsi_config || !mode) {
		DRM_ERROR("Invalid parameter\n");
		return -EINVAL;
	}

	return mdfld_dsi_get_panel_status(dsi_config, 0x0A, mode,
			transmission, 1);
}

static void mdfld_dsi_connector_save(struct drm_connector *connector)
{
	PSB_DEBUG_ENTRY("\n");
}

static void mdfld_dsi_connector_restore(struct drm_connector *connector)
{
	PSB_DEBUG_ENTRY("\n");
}

static enum drm_connector_status mdfld_dsi_connector_detect(
	struct drm_connector *connector, bool force)
{
	struct psb_intel_output *psb_output =
		to_psb_intel_output(connector);
	struct mdfld_dsi_connector *dsi_connector =
		MDFLD_DSI_CONNECTOR(psb_output);
	(void) force;

	PSB_DEBUG_ENTRY("\n");

	return dsi_connector->status;
}

static int mdfld_dsi_connector_set_property(struct drm_connector *connector,
		struct drm_property *property,
		uint64_t value)
{
	struct drm_encoder *encoder = connector->encoder;
	struct backlight_device *psb_bd;
	struct drm_encoder_helper_funcs *pEncHFuncs = NULL;
	struct psb_intel_crtc *psb_crtc = NULL;
	bool bTransitionFromToCentered;
	uint64_t curValue;

	PSB_DEBUG_ENTRY("\n");

	if (!strcmp(property->name, "scaling mode") && encoder) {
		psb_crtc = to_psb_intel_crtc(encoder->crtc);
		if (!psb_crtc)
			goto set_prop_error;

		switch (value) {
		case DRM_MODE_SCALE_FULLSCREEN:
			break;
		case DRM_MODE_SCALE_CENTER:
			break;
		case DRM_MODE_SCALE_NO_SCALE:
			break;
		case DRM_MODE_SCALE_ASPECT:
			break;
		default:
			goto set_prop_error;
		}

		if (drm_object_property_get_value(&connector->base, property,
					&curValue))
			goto set_prop_error;

		if (curValue == value)
			goto set_prop_done;

		if (drm_object_property_set_value(&connector->base, property,
					value))
			goto set_prop_error;

		bTransitionFromToCentered =
			(curValue == DRM_MODE_SCALE_NO_SCALE) ||
			(value == DRM_MODE_SCALE_NO_SCALE);

		if (psb_crtc->saved_mode.hdisplay != 0 &&
				psb_crtc->saved_mode.vdisplay != 0) {
			if (bTransitionFromToCentered) {
				if (!drm_crtc_helper_set_mode(encoder->crtc,
							&psb_crtc->saved_mode,
							encoder->crtc->x,
							encoder->crtc->y,
							encoder->crtc->fb))
					goto set_prop_error;
			} else {
				pEncHFuncs = encoder->helper_private;
				pEncHFuncs->mode_set(encoder,
						&psb_crtc->saved_mode,
						&psb_crtc->saved_adjusted_mode);
			}
		}
	} else if (!strcmp(property->name, "backlight") && encoder) {
		PSB_DEBUG_ENTRY("backlight level = %d\n", (int)value);
		if (drm_object_property_set_value(&connector->base, property,
					value))
			goto set_prop_error;
		else {
			PSB_DEBUG_ENTRY("set brightness to %d", (int)value);
			psb_bd = psb_get_backlight_device();
			if (psb_bd) {
				psb_bd->props.brightness = value;
				psb_set_brightness(psb_bd);
			}
		}
	}
set_prop_done:
	return 0;
set_prop_error:
	return -1;
}

static void mdfld_dsi_connector_destroy(struct drm_connector *connector)
{
	struct psb_intel_output *psb_output = to_psb_intel_output(connector);
	struct mdfld_dsi_connector *dsi_connector =
		MDFLD_DSI_CONNECTOR(psb_output);
	struct mdfld_dsi_config *dsi_config =
		mdfld_dsi_get_config(dsi_connector);
	struct mdfld_dsi_pkg_sender *sender;

	PSB_DEBUG_ENTRY("\n");

	if (!dsi_connector)
		return;

	drm_sysfs_connector_remove(connector);
	drm_connector_cleanup(connector);

	mdfld_dsi_dsr_destroy(dsi_config);

	sender = dsi_connector->pkg_sender;

	mdfld_dsi_error_detector_exit(dsi_connector);

	mdfld_dsi_pkg_sender_destroy(sender);

	kfree(dsi_connector);
}

static int mdfld_dsi_connector_get_modes(struct drm_connector *connector)
{
	struct psb_intel_output *psb_output = to_psb_intel_output(connector);
	struct mdfld_dsi_connector *dsi_connector =
		MDFLD_DSI_CONNECTOR(psb_output);
	struct mdfld_dsi_config *dsi_config =
		mdfld_dsi_get_config(dsi_connector);
	struct drm_display_mode *fixed_mode;
	struct drm_display_mode *dup_mode = NULL;
	struct drm_device *dev;

	if (!dsi_config) {
		DRM_ERROR("dsi_config is NULL\n");
		return MODE_ERROR;
	}
	fixed_mode = dsi_config->fixed_mode;
	dev = connector->dev;

	PSB_DEBUG_ENTRY("\n");

	connector->display_info.min_vfreq = 0;
	connector->display_info.max_vfreq = 200;
	connector->display_info.min_hfreq = 0;
	connector->display_info.max_hfreq = 200;

	if (fixed_mode) {
		PSB_DEBUG_ENTRY("fixed_mode %dx%d\n",
				fixed_mode->hdisplay, fixed_mode->vdisplay);

		dup_mode = drm_mode_duplicate(dev, fixed_mode);
		drm_mode_probed_add(connector, dup_mode);
		return 1;
	}

	DRM_ERROR("Didn't get any modes!\n");

	return 0;
}

static int mdfld_dsi_connector_mode_valid(struct drm_connector *connector,
		struct drm_display_mode *mode)
{
	struct psb_intel_output *psb_output = to_psb_intel_output(connector);
	struct mdfld_dsi_connector *dsi_connector =
		MDFLD_DSI_CONNECTOR(psb_output);
	struct mdfld_dsi_config *dsi_config =
		mdfld_dsi_get_config(dsi_connector);
	struct drm_display_mode *fixed_mode;

	if (!dsi_config) {
		DRM_ERROR("dsi_config is NULL\n");
		return MODE_ERROR;
	}
	fixed_mode = dsi_config->fixed_mode;

	PSB_DEBUG_ENTRY("mode %p, fixed mode %p\n", mode, fixed_mode);

	if (mode->flags & DRM_MODE_FLAG_DBLSCAN)
		return MODE_NO_DBLESCAN;

	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		return MODE_NO_INTERLACE;

	/** FIXME: current DC has no fitting unit, reject any mode setting
	 * request will figure out a way to do up-scaling(pannel fitting) later
	 **/
	if (fixed_mode) {
		if (mode->hdisplay != fixed_mode->hdisplay)
			return MODE_PANEL;

		if (mode->vdisplay != fixed_mode->vdisplay)
			return MODE_PANEL;
	}

	PSB_DEBUG_ENTRY("ok\n");

	return MODE_OK;
}

static void mdfld_dsi_connector_dpms(struct drm_connector *connector, int mode)
{
	/*first, execute dpms*/
	drm_helper_connector_dpms(connector, mode);
}

static struct drm_encoder *
mdfld_dsi_connector_best_encoder(struct drm_connector *connector)
{
	struct psb_intel_output *psb_output = to_psb_intel_output(connector);
	struct mdfld_dsi_connector *dsi_connector =
		MDFLD_DSI_CONNECTOR(psb_output);
	struct mdfld_dsi_config *dsi_config =
		mdfld_dsi_get_config(dsi_connector);
	struct mdfld_dsi_encoder *encoder = NULL;

	if (!dsi_config) {
		DRM_ERROR("dsi_config is NULL\n");
		return NULL;
	}

	PSB_DEBUG_ENTRY("config type %d\n", dsi_config->type);

	if (dsi_config->type == MDFLD_DSI_ENCODER_DBI)
		encoder = dsi_config->encoders[MDFLD_DSI_ENCODER_DBI];
	else if (dsi_config->type == MDFLD_DSI_ENCODER_DPI)
		encoder = dsi_config->encoders[MDFLD_DSI_ENCODER_DPI];

	PSB_DEBUG_ENTRY("get encoder %p\n", encoder);

	if (!encoder) {
		DRM_ERROR("Invalid encoder for type %d\n", dsi_config->type);
		return NULL;
	}

	dsi_config->encoder = encoder;

	return &encoder->base;
}

/*DSI connector funcs*/
static const struct drm_connector_funcs mdfld_dsi_connector_funcs = {
	.dpms = mdfld_dsi_connector_dpms,
	.save = mdfld_dsi_connector_save,
	.restore = mdfld_dsi_connector_restore,
	.detect = mdfld_dsi_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.set_property = mdfld_dsi_connector_set_property,
	.destroy = mdfld_dsi_connector_destroy,
};

/*DSI connector helper funcs*/
static const
struct drm_connector_helper_funcs mdfld_dsi_connector_helper_funcs = {
	.get_modes = mdfld_dsi_connector_get_modes,
	.mode_valid = mdfld_dsi_connector_mode_valid,
	.best_encoder = mdfld_dsi_connector_best_encoder,
};

static int mdfld_dsi_get_default_config(struct drm_device *dev,
		struct mdfld_dsi_config *config, int pipe)
{
	if (!dev || !config) {
		DRM_ERROR("Invalid parameters");
		return -EINVAL;
	}

	config->bpp = 24;
	config->type = is_panel_vid_or_cmd(dev);
	config->lane_count = 2;
	config->lane_config = MDFLD_DSI_DATA_LANE_2_2;

	config->channel_num = 0;
	config->video_mode = MDFLD_DSI_VIDEO_BURST_MODE;

	return 0;
}

static int mdfld_dsi_regs_init(struct mdfld_dsi_config *dsi_config,
		int pipe)
{
	struct mdfld_dsi_hw_registers *regs;
	u32 reg_offset;

	if (!dsi_config) {
		DRM_ERROR("dsi_config is null\n");
		return -EINVAL;
	}

	regs = &dsi_config->regs;

	regs->vgacntr_reg = VGACNTRL;
	regs->dpll_reg = MRST_DPLL_A;
	regs->fp_reg = MRST_FPA0;

	regs->ovaadd_reg = OV_OVADD;
	regs->ovcadd_reg = OVC_OVADD;
	regs->ddl1_reg = DDL1;
	regs->ddl2_reg = DDL2;
	regs->ddl3_reg = DDL3;
	regs->ddl4_reg = DDL4;

	if (pipe == 0) {
		regs->dspcntr_reg = DSPACNTR;
		regs->dspsize_reg = DSPASIZE;
		regs->dspsurf_reg = DSPASURF;
		regs->dsplinoff_reg = DSPALINOFF;
		regs->dsppos_reg = DSPAPOS;
		regs->dspstride_reg = DSPASTRIDE;
		regs->color_coef_reg = PIPEA_COLOR_COEF0;
		regs->htotal_reg = HTOTAL_A;
		regs->hblank_reg = HBLANK_A;
		regs->hsync_reg = HSYNC_A;
		regs->vtotal_reg = VTOTAL_A;
		regs->vblank_reg = VBLANK_A;
		regs->vsync_reg = VSYNC_A;
		regs->pipesrc_reg = PIPEASRC;
		regs->pipeconf_reg = PIPEACONF;
		regs->pipestat_reg = PIPEASTAT;
		regs->mipi_reg = MIPI;
		regs->palette_reg = PALETTE_A;
		regs->gamma_red_max_reg = GAMMA_RED_MAX_A;
		regs->gamma_green_max_reg = GAMMA_GREEN_MAX_A;
		regs->gamma_blue_max_reg = GAMMA_BLUE_MAX_A;
		reg_offset = 0;
	} else if (pipe == 2) {
		regs->dspcntr_reg = DSPCCNTR;
		regs->dspsize_reg = DSPCSIZE;
		regs->dspsurf_reg = DSPCSURF;
		regs->dsplinoff_reg = DSPCLINOFF;
		regs->dsppos_reg = DSPCPOS;
		regs->dspstride_reg = DSPCSTRIDE;
		regs->color_coef_reg = PIPEC_COLOR_COEF0;
		regs->htotal_reg = HTOTAL_C;
		regs->hblank_reg = HBLANK_C;
		regs->hsync_reg = HSYNC_C;
		regs->vtotal_reg = VTOTAL_C;
		regs->vblank_reg = VBLANK_C;
		regs->vsync_reg = VSYNC_C;
		regs->pipesrc_reg = PIPECSRC;
		regs->pipeconf_reg = PIPECCONF;
		regs->pipestat_reg = PIPECSTAT;
		regs->mipi_reg = MIPI_C;
		regs->palette_reg = PALETTE_C;
		regs->gamma_red_max_reg = GAMMA_RED_MAX_C;
		regs->gamma_green_max_reg = GAMMA_GREEN_MAX_C;
		regs->gamma_blue_max_reg = GAMMA_BLUE_MAX_C;

		reg_offset = MIPIC_REG_OFFSET;
	} else {
		DRM_ERROR("Wrong pipe\n");
		return -EINVAL;
	}

	regs->device_ready_reg = MIPIA_DEVICE_READY_REG + reg_offset;
	regs->intr_stat_reg = MIPIA_INTR_STAT_REG + reg_offset;
	regs->intr_en_reg = MIPIA_INTR_EN_REG + reg_offset;
	regs->dsi_func_prg_reg = MIPIA_DSI_FUNC_PRG_REG + reg_offset;
	regs->hs_tx_timeout_reg = MIPIA_HS_TX_TIMEOUT_REG + reg_offset;
	regs->lp_rx_timeout_reg = MIPIA_LP_RX_TIMEOUT_REG + reg_offset;
	regs->turn_around_timeout_reg =
		MIPIA_TURN_AROUND_TIMEOUT_REG + reg_offset;
	regs->device_reset_timer_reg =
		MIPIA_DEVICE_RESET_TIMER_REG + reg_offset;
	regs->dpi_resolution_reg = MIPIA_DPI_RESOLUTION_REG + reg_offset;
	regs->dbi_fifo_throttle_reg =
		MIPIA_DBI_FIFO_THROTTLE_REG + reg_offset;
	regs->hsync_count_reg = MIPIA_HSYNC_COUNT_REG + reg_offset;
	regs->hbp_count_reg = MIPIA_HBP_COUNT_REG + reg_offset;
	regs->hfp_count_reg = MIPIA_HFP_COUNT_REG + reg_offset;
	regs->hactive_count_reg = MIPIA_HACTIVE_COUNT_REG + reg_offset;
	regs->vsync_count_reg = MIPIA_VSYNC_COUNT_REG + reg_offset;
	regs->vbp_count_reg = MIPIA_VBP_COUNT_REG + reg_offset;
	regs->vfp_count_reg = MIPIA_VFP_COUNT_REG + reg_offset;
	regs->high_low_switch_count_reg =
		MIPIA_HIGH_LOW_SWITCH_COUNT_REG + reg_offset;
	regs->dpi_control_reg = MIPIA_DPI_CONTROL_REG + reg_offset;
	regs->dpi_data_reg = MIPIA_DPI_DATA_REG + reg_offset;
	regs->init_count_reg = MIPIA_INIT_COUNT_REG + reg_offset;
	regs->max_return_pack_size_reg =
		MIPIA_MAX_RETURN_PACK_SIZE_REG + reg_offset;
	regs->video_mode_format_reg =
		MIPIA_VIDEO_MODE_FORMAT_REG + reg_offset;
	regs->eot_disable_reg = MIPIA_EOT_DISABLE_REG + reg_offset;
	regs->lp_byteclk_reg = MIPIA_LP_BYTECLK_REG + reg_offset;
	regs->lp_gen_data_reg = MIPIA_LP_GEN_DATA_REG + reg_offset;
	regs->hs_gen_data_reg = MIPIA_HS_GEN_DATA_REG + reg_offset;
	regs->lp_gen_ctrl_reg = MIPIA_LP_GEN_CTRL_REG + reg_offset;
	regs->hs_gen_ctrl_reg = MIPIA_HS_GEN_CTRL_REG + reg_offset;
	regs->gen_fifo_stat_reg = MIPIA_GEN_FIFO_STAT_REG + reg_offset;
	regs->hs_ls_dbi_enable_reg =
		MIPIA_HS_LS_DBI_ENABLE_REG + reg_offset;
	regs->dphy_param_reg = MIPIA_DPHY_PARAM_REG + reg_offset;
	regs->dbi_bw_ctrl_reg = MIPIA_DBI_BW_CTRL_REG + reg_offset;
	regs->clk_lane_switch_time_cnt_reg =
		MIPIA_CLK_LANE_SWITCH_TIME_CNT_REG + reg_offset;

	regs->mipi_control_reg = MIPIA_CONTROL_REG + reg_offset;
	regs->mipi_data_addr_reg = MIPIA_DATA_ADD_REG + reg_offset;
	regs->mipi_data_len_reg = MIPIA_DATA_LEN_REG + reg_offset;
	regs->mipi_cmd_addr_reg = MIPIA_CMD_ADD_REG + reg_offset;
	regs->mipi_cmd_len_reg = MIPIA_CMD_LEN_REG + reg_offset;
	regs->histogram_intr_ctrl_reg = HISTOGRAM_INT_CONTROL;
	regs->histogram_logic_ctrl_reg = HISTOGRAM_LOGIC_CONTROL;
	regs->aimg_enhance_bin_reg = HISTOGRAM_BIN_DATA;
	//regs->lvds_port_ctrl_reg = LVDS_PORT_CTRL;

	return 0;
}

/*
 * Returns the panel fixed mode from configuration. 
 */
struct drm_display_mode *
mdfld_dsi_get_configuration_mode(struct mdfld_dsi_config *dsi_config, int pipe)
{
	struct drm_device *dev = dsi_config->dev;
	struct drm_display_mode *mode;
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	struct mrst_timing_info *ti = &dev_priv->gct_data.DTD;
	bool use_gct = false;
	if (IS_CTP(dev))
		use_gct = true;

	PSB_DEBUG_ENTRY("\n");

	mode = kzalloc(sizeof(*mode), GFP_KERNEL);
	if (!mode)
		return NULL;

	if (use_gct) {
		PSB_DEBUG_ENTRY("gct find MIPI panel. \n");

		mode->hdisplay = (ti->hactive_hi << 8) | ti->hactive_lo;
		mode->vdisplay = (ti->vactive_hi << 8) | ti->vactive_lo;
		mode->hsync_start = mode->hdisplay +
				    ((ti->hsync_offset_hi << 8) |
				     ti->hsync_offset_lo);
		mode->hsync_end = mode->hsync_start +
				  ((ti->hsync_pulse_width_hi << 8) |
				   ti->hsync_pulse_width_lo);
		mode->htotal = mode->hdisplay + ((ti->hblank_hi << 8) |
				ti->hblank_lo);
		mode->vsync_start = mode->vdisplay +
			((ti->vsync_offset_hi << 8) |
			 ti->vsync_offset_lo);
		mode->vsync_end = mode->vsync_start +
			((ti->vsync_pulse_width_hi << 8) |
			 ti->vsync_pulse_width_lo);
		mode->vtotal = mode->vdisplay +
			       ((ti->vblank_hi << 8) | ti->vblank_lo);
		mode->clock = ti->pixel_clock * 10;

		PSB_DEBUG_ENTRY("hdisplay is %d\n", mode->hdisplay);
		PSB_DEBUG_ENTRY("vdisplay is %d\n", mode->vdisplay);
		PSB_DEBUG_ENTRY("HSS is %d\n", mode->hsync_start);
		PSB_DEBUG_ENTRY("HSE is %d\n", mode->hsync_end);
		PSB_DEBUG_ENTRY("htotal is %d\n", mode->htotal);
		PSB_DEBUG_ENTRY("VSS is %d\n", mode->vsync_start);
		PSB_DEBUG_ENTRY("VSE is %d\n", mode->vsync_end);
		PSB_DEBUG_ENTRY("vtotal is %d\n", mode->vtotal);
		PSB_DEBUG_ENTRY("clock is %d\n", mode->clock);
	} else {
		if (dsi_config->type == MDFLD_DSI_ENCODER_DPI) {
			mode->hdisplay = 864;
			mode->vdisplay = 480;
			mode->hsync_start = 873;
			mode->hsync_end = 876;
			mode->htotal = 887;
			mode->vsync_start = 487;
			mode->vsync_end = 490;
			mode->vtotal = 499;
			mode->clock = 33264;
		} else if (dsi_config->type == MDFLD_DSI_ENCODER_DBI) {
			mode->hdisplay = 864;
			mode->vdisplay = 480;
			mode->hsync_start = 872;
			mode->hsync_end = 876;
			mode->htotal = 884;
			mode->vsync_start = 482;
			mode->vsync_end = 494;
			mode->vtotal = 486;
			mode->clock = 25777;
		}
	}

	drm_mode_set_name(mode);
	drm_mode_set_crtcinfo(mode, 0);

	mode->type |= DRM_MODE_TYPE_PREFERRED;

	return mode;
}

/*
 * MIPI output init
 * @dev drm device
 * @pipe pipe number. 0 or 2
 * @config 
 * 
 * Do the initialization of a MIPI output, including create DRM mode objects
 * initialization of DSI output on @pipe 
 */
int mdfld_dsi_output_init(struct drm_device *dev,
		int pipe,
		struct mdfld_dsi_config *config,
		struct panel_funcs *p_funcs)
{
	struct mdfld_dsi_config *dsi_config;
	struct mdfld_dsi_connector *dsi_connector;
	struct psb_intel_output *psb_output;
	struct drm_connector *connector;
	struct mdfld_dsi_encoder *encoder;
	struct drm_psb_private *dev_priv;
	struct panel_info dsi_panel_info;
	u32 width_mm, height_mm;

	PSB_DEBUG_ENTRY("init DSI output on pipe %d\n", pipe);

	if (!dev || ((pipe != 0) && (pipe != 2))) {
		DRM_ERROR("Invalid parameter\n");
		return -EIO;
	}

	dev_priv = dev->dev_private;

	/*create a new connetor*/
	dsi_connector = kzalloc(sizeof(struct mdfld_dsi_connector), GFP_KERNEL);
	if (!dsi_connector) {
		DRM_ERROR("No memory");
		return -ENOMEM;
	}

	dsi_connector->pipe =  pipe;

	/*set DSI config*/
	if (config)
		dsi_config = config;
	else {
		dsi_config = kzalloc(sizeof(struct mdfld_dsi_config),
				GFP_KERNEL);
		if (!dsi_config) {
			DRM_ERROR("cannot allocate memory for DSI config\n");
			goto dsi_init_err0;
		}

		mdfld_dsi_get_default_config(dev, dsi_config, pipe);
	}

	/*init DSI regs*/
	mdfld_dsi_regs_init(dsi_config, pipe);

	/*init DSI HW context lock*/
	mutex_init(&dsi_config->context_lock);

	dsi_connector->private = dsi_config;

	dsi_config->pipe = pipe;
	dsi_config->changed = 1;
	dsi_config->dev = dev;

	/*init fixed mode basing on DSI config type*/
	dsi_panel_info.panel_180_rotation = false;
	if (dsi_config->type == MDFLD_DSI_ENCODER_DBI) {
		dsi_config->fixed_mode = p_funcs->get_config_mode();
		p_funcs->get_panel_info(pipe, &dsi_panel_info);
	} else if (dsi_config->type == MDFLD_DSI_ENCODER_DPI) {
		dsi_config->fixed_mode = p_funcs->get_config_mode();
		p_funcs->get_panel_info(pipe, &dsi_panel_info);
	}

	width_mm = dsi_panel_info.width_mm;
	height_mm = dsi_panel_info.height_mm;
	dev_priv->panel_180_rotation = dsi_panel_info.panel_180_rotation;

	dsi_config->mode = dsi_config->fixed_mode;
	dsi_config->connector = dsi_connector;

	if (!dsi_config->fixed_mode) {
		DRM_ERROR("No pannel fixed mode was found\n");
		goto dsi_init_err0;
	}

	if (pipe && dev_priv->dsi_configs[0])
		dev_priv->dsi_configs[1] = dsi_config;
	else if (pipe == 0)
		dev_priv->dsi_configs[0] = dsi_config;
	else {
		DRM_ERROR("Trying to init MIPI1 before MIPI0\n");
		goto dsi_init_err0;
	}

	/*init drm connector object*/
	psb_output = &dsi_connector->base;

	psb_output->type = (pipe == 0) ? INTEL_OUTPUT_MIPI : INTEL_OUTPUT_MIPI2;

	connector = &psb_output->base;
	drm_connector_init(dev, connector,
			&mdfld_dsi_connector_funcs,
			DRM_MODE_CONNECTOR_DSI);

	drm_connector_helper_add(connector, &mdfld_dsi_connector_helper_funcs);

	connector->display_info.subpixel_order = SubPixelHorizontalRGB;
	connector->display_info.width_mm = width_mm;
	connector->display_info.height_mm = height_mm;
	connector->interlace_allowed = false;
	connector->doublescan_allowed = false;

	/*attach properties*/
	drm_object_attach_property(&connector->base,
			dev->mode_config.scaling_mode_property,
			DRM_MODE_SCALE_FULLSCREEN);

	drm_object_attach_property(&connector->base,
			dev_priv->backlight_property,
			MDFLD_DSI_BRIGHTNESS_MAX_LEVEL);

	/*init DSI package sender on this output*/
	if (mdfld_dsi_pkg_sender_init(dsi_connector, pipe)) {
		DRM_ERROR("Package Sender initialization failed on pipe %d\n",
				pipe);
		goto dsi_init_err0;
	}

#if 0
	/*init panel error detector*/
	if (mdfld_dsi_error_detector_init(dev, dsi_connector)) {
		DRM_ERROR("Failed to init dsi_error detector");
		goto dsi_init_err1;
	}
#endif

	/*create DBI & DPI encoders*/
	if (dsi_config->type == MDFLD_DSI_ENCODER_DBI) {
		encoder = mdfld_dsi_dbi_init(dev, dsi_connector, p_funcs);
		if (!encoder) {
			DRM_ERROR("Create DBI encoder failed\n");
			goto dsi_init_err2;
		}
		encoder->private = dsi_config;
		dsi_config->encoders[MDFLD_DSI_ENCODER_DBI] = encoder;

		if (pipe == 2)
			dev_priv->encoder2 = encoder;

		if (pipe == 0)
			dev_priv->encoder0 = encoder;
	} else if (dsi_config->type == MDFLD_DSI_ENCODER_DPI) {
		encoder = mdfld_dsi_dpi_init(dev, dsi_connector, p_funcs);
		if (!encoder) {
			DRM_ERROR("Create DPI encoder failed\n");
			goto dsi_init_err2;
		}
		encoder->private = dsi_config;
		dsi_config->encoders[MDFLD_DSI_ENCODER_DPI] = encoder;

		if (pipe == 2)
			dev_priv->encoder2 = encoder;

		if (pipe == 0)
			dev_priv->encoder0 = encoder;
	}

	drm_sysfs_connector_add(connector);

	/* DPST: TODO - get appropriate connector */
	if (dev_priv->dpst_connector == 0)
		dev_priv->dpst_connector = connector;

	/*init dsr*/
	if (mdfld_dsi_dsr_init(dsi_config))
		DRM_INFO("%s: Failed to initialize DSR\n", __func__);

	PSB_DEBUG_ENTRY("successfully\n");
	return 0;

	/*TODO: add code to destroy outputs on error*/
dsi_init_err2:
	mdfld_dsi_error_detector_exit(dsi_connector);

#if 0
dsi_init_err1:
#endif
	/*destroy sender*/
	mdfld_dsi_pkg_sender_destroy(dsi_connector->pkg_sender);

	drm_connector_cleanup(connector);

	if (dsi_config->fixed_mode)
		kfree(dsi_config->fixed_mode);

	if (dsi_config) {
		kfree(dsi_config);
		if (pipe)
			dev_priv->dsi_configs[1] = NULL;
		else
			dev_priv->dsi_configs[0] = NULL;
	}

dsi_init_err0:
	if (dsi_connector)
		kfree(dsi_connector);

	return -EIO;
}

void mdfld_dsi_set_drain_latency(struct drm_encoder *encoder,
		struct drm_display_mode *mode)
{
	struct mdfld_dsi_encoder *dsi_encoder = MDFLD_DSI_ENCODER(encoder);
	struct mdfld_dsi_config *dsi_config =
		mdfld_dsi_encoder_get_config(dsi_encoder);

	struct mdfld_dsi_hw_context *ctx;

	if (!dsi_config) {
                DRM_ERROR("Invalid parameters\n");
                return;
        }

	ctx = &dsi_config->dsi_hw_context;
	if (dsi_config->pipe == 0) {
		mutex_lock(&dsi_config->context_lock);
#if 0
		if ((mode->hdisplay == 720) && (mode->vdisplay == 1280))
			drain_rate = ACTUAL_DRAIN_RATE_7x12;
		else if ((mode->hdisplay == 1080) && (mode->vdisplay == 1920))
			drain_rate = ACTUAL_DRAIN_RATE_10x19;
		else if ((mode->hdisplay == 2560) && (mode->vdisplay == 1600))
			drain_rate = ACTUAL_DRAIN_RATE_25x16;
		if (drain_rate != 0) {
			value = ((64 * 32 / drain_rate) & 0xFF) | 0x80;
			ctx->ddl1 = value | (HDMI_SPRITE_DEADLINE << 8) |
					(value << 24);
			ctx->ddl2 = value | (HDMI_OVERLAY_DEADLINE << 8);
			ctx->ddl3 = 0;
			ctx->ddl4 = value | (value << 8);
			ctx->ddl1 = 0x83838383;
			ctx->ddl2 = 0x83838383;
			ctx->ddl3 = 0x83;
			ctx->ddl4 = 0x8383;

		}
#endif
		ctx->ddl1 = 0x86868686;
		ctx->ddl2 = 0x86868686;
		ctx->ddl3 = 0x86;
		ctx->ddl4 = 0x8686;

		/* init for 1st boot, 12KB for plane A D E F */
		ctx->dsparb = 0xc0300c0;
		ctx->dsparb2 = 0x90180;

		mutex_unlock(&dsi_config->context_lock);
	}

	return;
}
