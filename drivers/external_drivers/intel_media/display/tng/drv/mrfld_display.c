/*
 * Copyright Â© 2011 Intel Corporation
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
 *	Jim Liu <jim.liu@intel.com>
 */

#include "android_hdmi.h"
#include "displayclass_interface.h"
#include "pwr_mgmt.h"

#define KEEP_UNUSED_CODE_S3D 0

/* Functions will be deleted after simulated on MDFLD_PLATFORM */

/* MRFLD_PLATFORM start */

#if KEEP_UNUSED_CODE_S3D
/**
 * Set up the HDMI Vendor Specific InfoFrame Packet and send it to HDMI display
 *
 */
static int mrfld_set_up_s3d_InfoFrame(struct drm_device *dev, enum
				      s3d_structure s3d_format)
{
	u8 vsif[12] = { 0x81, 0x01, 0x06, 0x00, 0x03, 0x0c,
		0x00, 0x40, 0x00, 0x00, 0x00, 0x00
	};
	u8 checksum = 0;
	u32 *p_vsif = (u32 *) vsif;
	u32 viddipctl_val = 0;
	u32 buf_size = 0;
	int i = 0;

	PSB_DEBUG_ENTRY("s3d_format = %d\n", s3d_format);

	/* Fill the 3d format in the HDMI Vendor Specific InfoFrame. */
	vsif[8] = s3d_format << 4;

	/* Get the buffer size in bytes */
	buf_size = vsif[2] + 3;

	/* Get the checksum byte. */
	for (i = 0; i < buf_size; i++)
		checksum += vsif[i];

	checksum = 0xff - checksum + 1;
	vsif[3] = checksum;

	/* Wait for 2 VSyncs. */
	mdelay(20);		/* msleep(20); */
	mdelay(20);		/* msleep(20); */
	/* psb_intel_wait_for_vblank(dev); */

	/* Wait for 3 HSync. */

	/* Disable the VS DIP type */
	viddipctl_val = REG_READ(VIDEO_DIP_CTL);
	viddipctl_val &= ~DIP_TYPE_VS;
	REG_WRITE(VIDEO_DIP_CTL, viddipctl_val);

	/* set the DIP buffer index to vendor specific. */
	viddipctl_val &= ~(DIP_BUFF_INDX_MASK |
			   DIP_RAM_ADDR_MASK | DIP_TX_FREQ_MASK);
	viddipctl_val |= DIP_BUFF_INDX_VS | EN_DIP;
	REG_WRITE(VIDEO_DIP_CTL, viddipctl_val);

	/* Get the buffer size in DWORD. */
	buf_size = (buf_size + 3) / 4;

	/* Write HDMI Vendor Specific InfoFrame. */
	for (i = 0; i < buf_size; i++)
		REG_WRITE(VIDEO_DIP_DATA, *(p_vsif++));

	/* Enable the DIP type and transmission frequency. */
	viddipctl_val |= DIP_TYPE_VS | DIP_TX_FREQ_2VSNC;
	REG_WRITE(VIDEO_DIP_CTL, viddipctl_val);

	return 0;
}
#endif /* if KEEP_UNUSED_CODE_S3D */

#if KEEP_UNUSED_CODE_S3D
/**
 * Disable sending the HDMI Vendor Specific InfoFrame Packet.
 *
 */
static int mrfld_disable_s3d_InfoFrame(struct drm_device *dev)
{
	u32 viddipctl_val = 0;

	PSB_DEBUG_ENTRY("\n");

	/* Wait for 2 VSyncs. */
	mdelay(20);		/* msleep(20); */
	mdelay(20);		/* msleep(20); */
	/* psb_intel_wait_for_vblank(dev); */

	/* Wait for 3 HSync. */

	/* Disable the VS DIP type */
	viddipctl_val = REG_READ(VIDEO_DIP_CTL);
	viddipctl_val &= ~DIP_TYPE_VS;
	REG_WRITE(VIDEO_DIP_CTL, viddipctl_val);

	return 0;
}
#endif /* if KEEP_UNUSED_CODE_S3D */

/**
 * Disable the pipe, plane and pll.
 *
 * Note: FIXME need to modify PLL
 */
void mrfld_disable_crtc(struct drm_device *dev, int pipe, bool plane_d)
{
	int dpll_reg = MRST_DPLL_A;
	int dspcntr_reg = DSPACNTR;
	int dspcntr_reg_d = DSPDCNTR;
	int dspsurf_reg = DSPASURF;
	int dspsurf_reg_d = DSPDSURF;
	int pipeconf_reg = PIPEACONF;
	u32 gen_fifo_stat_reg = GEN_FIFO_STAT_REG;
	u32 temp;

	PSB_DEBUG_ENTRY("pipe = %d\n", pipe);

	if (pipe != 1 && ((get_panel_type(dev, pipe) == JDI_7x12_VID) ||
			(get_panel_type(dev, pipe) == CMI_7x12_VID)))
		return;

	switch (pipe) {
	case 0:
		break;
	case 1:
		dpll_reg = MDFLD_DPLL_B;
		dspcntr_reg = DSPBCNTR;
		dspsurf_reg = DSPBSURF;
		pipeconf_reg = PIPEBCONF;
		break;
	case 2:
		dpll_reg = MRST_DPLL_A;
		dspcntr_reg = DSPCCNTR;
		dspsurf_reg = DSPCSURF;
		pipeconf_reg = PIPECCONF;
		gen_fifo_stat_reg = GEN_FIFO_STAT_REG + MIPIC_REG_OFFSET;
		break;
	default:
		DRM_ERROR("Illegal Pipe Number. \n");
		return;
	}

	if (pipe != 1)
		mdfld_dsi_gen_fifo_ready(dev, gen_fifo_stat_reg,
					 HS_CTRL_FIFO_EMPTY |
					 HS_DATA_FIFO_EMPTY);

	/* Disable display plane */
	temp = REG_READ(dspcntr_reg);
	if ((temp & DISPLAY_PLANE_ENABLE) != 0) {
		REG_WRITE(dspcntr_reg, temp & ~DISPLAY_PLANE_ENABLE);
		/* Flush the plane changes */
		REG_WRITE(dspsurf_reg, REG_READ(dspsurf_reg));
		REG_READ(dspsurf_reg);
	}

	/* Disable display plane D. */
	if (plane_d) {
		temp = REG_READ(dspcntr_reg_d);
		if ((temp & DISPLAY_PLANE_ENABLE) != 0) {
			REG_WRITE(dspcntr_reg_d, temp & ~DISPLAY_PLANE_ENABLE);
			/* Flush the plane changes */
			REG_WRITE(dspsurf_reg_d, REG_READ(dspsurf_reg_d));
			REG_READ(dspsurf_reg_d);
		}
	}

	/* Next, disable display pipes */
	temp = REG_READ(pipeconf_reg);
	if ((temp & PIPEACONF_ENABLE) != 0) {
		temp &= ~PIPEACONF_ENABLE;
		temp |= PIPECONF_PLANE_OFF | PIPECONF_CURSOR_OFF;
		REG_WRITE(pipeconf_reg, temp);
		REG_READ(pipeconf_reg);

		/* Wait for for the pipe disable to take effect. */
		mdfldWaitForPipeDisable(dev, pipe);
	}

	/* Disable PLLs. */
}

void mofd_update_fifo_size(struct drm_device *dev, bool hdmi_on)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct mdfld_dsi_config *dsi_config = dev_priv->dsi_configs[0];
	struct mdfld_dsi_hw_context *ctx = &dsi_config->dsi_hw_context;

	DRM_INFO("setting fifo size, hdmi_suspend: %d\n", hdmi_on);

	if (!hdmi_on) {
		/* no hdmi, 12KB for plane A D E F */
		REG_WRITE(DSPARB2, 0x90180);
		REG_WRITE(DSPARB, 0xc0300c0);
	} else {
		/* with hdmi, 10KB for plane A D E F; 8KB for plane B */
		REG_WRITE(DSPARB2, 0x981c0);
		REG_WRITE(DSPARB, 0x120480a0);
	}

	ctx->dsparb = REG_READ(DSPARB);
	ctx->dsparb2 = REG_READ(DSPARB2);
}

/**
 * Sets the power management mode of crtc including the pll pipe and plane.
 *
 */
static void mrfld_crtc_dpms(struct drm_crtc *crtc, int mode)
{
	struct drm_device *dev = crtc->dev;
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;
	struct psb_intel_crtc *psb_intel_crtc = to_psb_intel_crtc(crtc);
	int pipe = psb_intel_crtc->pipe;
	int dpll_reg = MRST_DPLL_A;
	int dspcntr_reg = DSPACNTR;
	int dspbase_reg = MRST_DSPABASE;
	int pipeconf_reg = PIPEACONF;
	u32 pipestat_reg = PIPEASTAT;
	u32 gen_fifo_stat_reg = GEN_FIFO_STAT_REG;
	u32 pipeconf = dev_priv->pipeconf;
	u32 dspcntr = dev_priv->dspcntr;
	u32 mipi_enable_reg = MIPIA_DEVICE_READY_REG;
	u32 temp;
	bool enabled;
	u32 power_island = 0;
	unsigned long irqflags;
	struct android_hdmi_priv *hdmi_priv = dev_priv->hdmi_priv;

	PSB_DEBUG_ENTRY("mode = %d, pipe = %d\n", mode, pipe);

#ifndef CONFIG_SUPPORT_TOSHIBA_MIPI_DISPLAY
	/**
	 * MIPI dpms
	 * NOTE: this path only works for TMD panel now. update it to
	 * support all MIPI panels later.
	 */
	if (pipe != 1 && (IS_MOFD(dev) ||
				(get_panel_type(dev, pipe) == TMD_VID) ||
				(get_panel_type(dev, pipe) == TMD_6X10_VID) ||
				(get_panel_type(dev, pipe) == CMI_7x12_VID) ||
				(get_panel_type(dev, pipe) == CMI_7x12_CMD) ||
				(get_panel_type(dev, pipe) == SHARP_10x19_CMD) ||
				(get_panel_type(dev, pipe) == SHARP_10x19_DUAL_CMD) ||
				(get_panel_type(dev, pipe) == SHARP_25x16_CMD) ||
				(get_panel_type(dev, pipe) == SDC_16x25_CMD) ||
				(get_panel_type(dev, pipe) == SDC_25x16_CMD) ||
				(get_panel_type(dev, pipe) == JDI_7x12_CMD) ||
				(get_panel_type(dev, pipe) == JDI_7x12_VID) ||
				(get_panel_type(dev, pipe) == SHARP_25x16_VID) ||
				(get_panel_type(dev, pipe) == JDI_25x16_CMD) ||
				(get_panel_type(dev, pipe) == JDI_25x16_VID))) {
		return;
	}
#endif

	power_island = pipe_to_island(pipe);

	if (!power_island_get(power_island))
		return;

	switch (pipe) {
	case 0:
		break;
	case 1:
		dpll_reg = DPLL_B;
		dspcntr_reg = DSPBCNTR;
		dspbase_reg = MRST_DSPBBASE;
		pipeconf_reg = PIPEBCONF;
		pipeconf = dev_priv->pipeconf1;
		dspcntr = dev_priv->dspcntr1;
		if (IS_MDFLD(dev))
			dpll_reg = MDFLD_DPLL_B;
		break;
	case 2:
		dpll_reg = MRST_DPLL_A;
		dspcntr_reg = DSPCCNTR;
		dspbase_reg = MDFLD_DSPCBASE;
		pipeconf_reg = PIPECCONF;
		pipestat_reg = PIPECSTAT;
		pipeconf = dev_priv->pipeconf2;
		dspcntr = dev_priv->dspcntr2;
		gen_fifo_stat_reg = GEN_FIFO_STAT_REG + MIPIC_REG_OFFSET;
		mipi_enable_reg = MIPIA_DEVICE_READY_REG + MIPIC_REG_OFFSET;
		break;
	default:
		DRM_ERROR("Illegal Pipe Number.\n");
		goto crtc_dpms_err;
	}

	/* XXX: When our outputs are all unaware of DPMS modes other than off
	 * and on, we should map those modes to DRM_MODE_DPMS_OFF in the CRTC.
	 */
	switch (mode) {
	case DRM_MODE_DPMS_ON:
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
		DCLockMutex();

		/* Enable the DPLL */
		temp = REG_READ(dpll_reg);

		if ((temp & DPLL_VCO_ENABLE) == 0) {
			/* When ungating power of DPLL, needs to wait 0.5us before enable the VCO */
			if (temp & MDFLD_PWR_GATE_EN) {
				temp &= ~MDFLD_PWR_GATE_EN;
				REG_WRITE(dpll_reg, temp);
				/* FIXME_MDFLD PO - change 500 to 1 after PO */
				udelay(500);
			}

			REG_WRITE(dpll_reg, temp);
			REG_READ(dpll_reg);
			/* FIXME_MDFLD PO - change 500 to 1 after PO */
			udelay(500);

			REG_WRITE(dpll_reg, temp | DPLL_VCO_ENABLE);
			REG_READ(dpll_reg);

#if 0				/* FIXME MRFLD */
			/**
			 * wait for DSI PLL to lock
			 * NOTE: only need to poll status of pipe 0 and pipe 1,
			 * since both MIPI pipes share the same PLL.
			 */
			while ((pipe != 2) && (timeout < 20000)
			       && !(REG_READ(pipeconf_reg) &
				    PIPECONF_DSIPLL_LOCK)) {
				udelay(150);
				timeout++;
			}
#endif				/* FIXME MRFLD */
		}

		/* Enable the plane */
		temp = REG_READ(dspcntr_reg);
		if ((temp & DISPLAY_PLANE_ENABLE) == 0) {
			REG_WRITE(dspcntr_reg, temp | DISPLAY_PLANE_ENABLE);
			/* Flush the plane changes */
			REG_WRITE(dspbase_reg, REG_READ(dspbase_reg));
		}

		/* Enable the pipe */
		temp = REG_READ(pipeconf_reg);
		if ((temp & PIPEACONF_ENABLE) == 0) {
			REG_WRITE(pipeconf_reg, pipeconf);

			/* Wait for for the pipe enable to take effect. */
			mdfldWaitForPipeEnable(dev, pipe);
		}

		/*workaround for sighting 3741701 Random X blank display */
		/*perform w/a in video mode only on pipe A or C */
		if ((pipe == 0 || pipe == 2) &&
		    (is_panel_vid_or_cmd(dev) == MDFLD_DSI_ENCODER_DPI)) {
			REG_WRITE(pipestat_reg, REG_READ(pipestat_reg));
			msleep(100);
			if (PIPE_VBLANK_STATUS & REG_READ(pipestat_reg)) {
				PSB_DEBUG_ENTRY("OK");
			} else {
				PSB_DEBUG_ENTRY("STUCK!!!!");
				/*shutdown controller */
				temp = REG_READ(dspcntr_reg);
				REG_WRITE(dspcntr_reg,
					  temp & ~DISPLAY_PLANE_ENABLE);
				REG_WRITE(dspbase_reg, REG_READ(dspbase_reg));
				/*mdfld_dsi_dpi_shut_down(dev, pipe); */
				REG_WRITE(0xb048, 1);
				msleep(100);
				temp = REG_READ(pipeconf_reg);
				temp &= ~PIPEACONF_ENABLE;
				REG_WRITE(pipeconf_reg, temp);
				msleep(100);	/*wait for pipe disable */
				/*printk(KERN_ALERT "70008 is %x\n", REG_READ(0x70008));
				   printk(KERN_ALERT "b074 is %x\n", REG_READ(0xb074)); */
				REG_WRITE(mipi_enable_reg, 0);
				msleep(100);
				PSB_DEBUG_ENTRY("70008 is %x\n",
						REG_READ(0x70008));
				PSB_DEBUG_ENTRY("b074 is %x\n",
						REG_READ(0xb074));
				REG_WRITE(0xb004, REG_READ(0xb004));
				/* try to bring the controller back up again */
				REG_WRITE(mipi_enable_reg, 1);
				temp = REG_READ(dspcntr_reg);
				REG_WRITE(dspcntr_reg,
					  temp | DISPLAY_PLANE_ENABLE);
				REG_WRITE(dspbase_reg, REG_READ(dspbase_reg));
				/*mdfld_dsi_dpi_turn_on(dev, pipe); */
				REG_WRITE(0xb048, 2);
				msleep(100);
				temp = REG_READ(pipeconf_reg);
				temp |= PIPEACONF_ENABLE;
				REG_WRITE(pipeconf_reg, temp);
			}
		}

		psb_intel_crtc_load_lut(crtc);

		if ((pipe == 1) && hdmi_priv)
			hdmi_priv->hdmi_suspended = false;

		psb_enable_vblank(dev, pipe);
		spin_lock_irqsave(&dev->vbl_lock, irqflags);
		dev->vblank_enabled[pipe] = 1;
		spin_unlock_irqrestore(&dev->vbl_lock, irqflags);

		DCAttachPipe(pipe);
		DC_MRFLD_onPowerOn(pipe);
		DCUnLockMutex();

		/* Give the overlay scaler a chance to enable
		   if it's on this pipe */
		/* psb_intel_crtc_dpms_video(crtc, true); TODO */

		break;
	case DRM_MODE_DPMS_OFF:
		DCLockMutex();

		/* Give the overlay scaler a chance to disable
		 * if it's on this pipe */
		/* psb_intel_crtc_dpms_video(crtc, FALSE); TODO */
		if (pipe != 1)
			mdfld_dsi_gen_fifo_ready(dev, gen_fifo_stat_reg,
						 HS_CTRL_FIFO_EMPTY |
						 HS_DATA_FIFO_EMPTY);

		/* Disable the VGA plane that we never use */
		REG_WRITE(VGACNTRL, VGA_DISP_DISABLE);

		/* Disable display plane */
		temp = REG_READ(dspcntr_reg);
		if ((temp & DISPLAY_PLANE_ENABLE) != 0) {
			REG_WRITE(dspcntr_reg, temp & ~DISPLAY_PLANE_ENABLE);
			/* Flush the plane changes */
			REG_WRITE(dspbase_reg, REG_READ(dspbase_reg));
			REG_READ(dspbase_reg);
		}

		/* Next, disable display pipes */
		temp = REG_READ(pipeconf_reg);
		if ((temp & PIPEACONF_ENABLE) != 0) {
			temp &= ~PIPEACONF_ENABLE;
			temp |= PIPECONF_PLANE_OFF | PIPECONF_CURSOR_OFF;
			REG_WRITE(pipeconf_reg, temp);
			REG_READ(pipeconf_reg);

			/* Wait for for the pipe disable to take effect. */
			mdfldWaitForPipeDisable(dev, pipe);
		}

		temp = REG_READ(dpll_reg);
		if (temp & DPLL_VCO_ENABLE) {
			if (((pipe != 1)
			     && !((REG_READ(PIPEACONF) | REG_READ(PIPECCONF)) &
				  PIPEACONF_ENABLE))
			    || (pipe == 1)) {
				temp &= ~(DPLL_VCO_ENABLE);
				REG_WRITE(dpll_reg, temp);
				REG_READ(dpll_reg);
				/* Wait for the clocks to turn off. */
				/* FIXME_MDFLD PO may need more delay */
				udelay(500);
#if 0				/* FIXME_MDFLD Check if we need to power gate the PLL */
				if (!(temp & MDFLD_PWR_GATE_EN)) {
					/* gating power of DPLL */
					REG_WRITE(dpll_reg,
						  temp | MDFLD_PWR_GATE_EN);
					/* FIXME_MDFLD PO - change 500 to 1 after PO */
					udelay(5000);
				}
#endif
			}
		}

		drm_handle_vblank(dev, pipe);

		/* Turn off vsync interrupt. */
		drm_vblank_off(dev, pipe);

		if ((pipe == 1) && hdmi_priv)
			hdmi_priv->hdmi_suspended = true;

		if (IS_ANN(dev))
			mofd_update_fifo_size(dev, false);

		/* Make the pending flip request as completed. */
		DCUnAttachPipe(pipe);
		DC_MRFLD_onPowerOff(pipe);
		DCUnLockMutex();
		break;
	}

	enabled = crtc->enabled && mode != DRM_MODE_DPMS_OFF;

crtc_dpms_err:
	power_island_put(power_island);
}

static int mrfld_crtc_mode_set(struct drm_crtc *crtc,
			       struct drm_display_mode *mode,
			       struct drm_display_mode *adjusted_mode,
			       int x, int y, struct drm_framebuffer *old_fb)
{
	struct drm_device *dev = crtc->dev;
	struct psb_intel_crtc *psb_intel_crtc = to_psb_intel_crtc(crtc);
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)dev->dev_private;
	struct mdfld_dsi_config *dsi_config = NULL;
	int pipe = psb_intel_crtc->pipe;

	PSB_DEBUG_ENTRY("pipe = 0x%x\n", pipe);

	switch (pipe) {
	case 0:
		dsi_config = dev_priv->dsi_configs[0];
		break;
	case 1:
		break;
	case 2:
		dsi_config = dev_priv->dsi_configs[1];
		break;
	default:
		DRM_ERROR("Illegal Pipe Number. \n");
		return 0;
	}

	if (pipe != 1) {
		int clk;

		if (dsi_config->lane_count)
			clk = adjusted_mode->clock / dsi_config->lane_count;
		else
			clk = adjusted_mode->clock;

		mrfld_setup_pll(dev, pipe, clk);

		return mdfld_crtc_dsi_mode_set(crtc, dsi_config, mode,
				adjusted_mode, x, y, old_fb);
	} else {
		if (IS_ANN(dev))
			mofd_update_fifo_size(dev, true);
		android_hdmi_crtc_mode_set(crtc, mode, adjusted_mode,
				x, y, old_fb);

		return 0;
	}
}


#if KEEP_UNUSED_CODE_S3D

/**
 * Perform display 3D mode set to half line interleaving 3D display with two buffers.
 *
 * FIXME modify the following function with option to disable PLL or not.
 *
 * Function return value: 0 if error, 1 if success.
 */
int mrfld_s3d_flip_surf_addr(struct drm_device *dev, int pipe, struct
			     mrfld_s3d_flip *ps3d_flip)
{
	/* register */
	u32 dspsurf_reg = DSPASURF;
	u32 dspsurf_reg_d = DSPDSURF;

	PSB_DEBUG_ENTRY("pipe = 0x%x \n", pipe);

	switch (pipe) {
	case 0:
		break;
	case 1:
		dspsurf_reg = DSPBSURF;
		break;
	case 2:
		dspsurf_reg = DSPCSURF;
		break;
	default:
		DRM_ERROR("Illegal Pipe Number. \n");
		return 0;
	}

	if (ps3d_flip->s3d_state & S3D_STATE_ENALBLED) {
		REG_WRITE(dspsurf_reg, ps3d_flip->uiAddr_l);
		REG_WRITE(dspsurf_reg_d, ps3d_flip->uiAddr_r);
	} else {
		REG_WRITE(dspsurf_reg, ps3d_flip->uiAddr_l);
	}

	return 1;
}

/*
extern void mdfld_dsi_tmd_drv_ic_init(struct mdfld_dsi_config *dsi_config,
				      int pipe);
*/

/**
 * Perform display 3D mode set to half line interleaving 3D display with two buffers.
 *
 * FIXME modify the following function with option to disable PLL or not.
 */
int mrfld_s3d_to_line_interleave_half(struct drm_device *dev, int pipe, struct
				      mrfld_s3d_flip *ps3d_flip)
{
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;
	struct mdfld_dsi_config *dsi_config = dev_priv->dsi_configs[0];

	/* register */
	u32 pipeconf_reg = PIPEACONF;
	u32 dspcntr_reg = DSPACNTR;
	u32 dspstride_reg = DSPASTRIDE;
	u32 dsplinoff_reg = DSPALINOFF;
	u32 dspsize_reg = DSPASIZE;
	u32 dspsurf_reg = DSPASURF;
	u32 mipi_reg = MIPI;

	u32 dspcntr_reg_d = DSPDCNTR;
	u32 dspstride_reg_d = DSPDSTRIDE;
	u32 dsplinoff_reg_d = DSPDLINOFF;
	u32 dspsize_reg_d = DSPDSIZE;
	u32 dspsurf_reg_d = DSPDSURF;

	/* values */
	u32 pipeconf_val = 0;
	u32 dspcntr_val = 0;
	u32 dspsize_val = 0;
	u32 mipi_val = 0;

	PSB_DEBUG_ENTRY("pipe = 0x%x \n", pipe);

	switch (pipe) {
	case 0:
		break;
	case 1:
		DRM_ERROR("HDMI doesn't support line interleave half. \n");
		return 0;
	case 2:
		dsi_config = dev_priv->dsi_configs[1];
		pipeconf_reg = PIPECCONF;
		dspcntr_reg = DSPCCNTR;
		dspstride_reg = DSPCSTRIDE;
		dsplinoff_reg = DSPCLINOFF;
		dspsize_reg = DSPCSIZE;
		dspsurf_reg = DSPCSURF;
		mipi_reg = MIPI_C;
		break;
	default:
		DRM_ERROR("Illegal Pipe Number. \n");
		return 0;
	}

	/* Save the related DC registers. */
	pipeconf_val = REG_READ(pipeconf_reg);
	dspcntr_val = REG_READ(dspcntr_reg);
	dspsize_val = REG_READ(dspsize_reg);
	mipi_val = REG_READ(mipi_reg);

	/* Disable pipe and port, don't disable the PLL. */
	mrfld_disable_crtc(dev, pipe, true);

	/* Set up line interleaving display 3D format. */
	dspsize_val = (((dspsize_val & 0xFFFF0000) + 0x00010000) / 2 -
		       0x00010000) | (dspsize_val & 0x0000FFFF);
	REG_WRITE(dspsize_reg, dspsize_val);
	REG_WRITE(dspsize_reg_d, dspsize_val);

	/* set up the frame buffer stride, offset and start. */
	REG_WRITE(dspstride_reg, ps3d_flip->pitch_l);
	REG_WRITE(dspstride_reg_d, ps3d_flip->pitch_r);
	REG_WRITE(dsplinoff_reg, 0);
	REG_WRITE(dsplinoff_reg_d, 0);
	REG_WRITE(dspsurf_reg, ps3d_flip->uiAddr_l);
	REG_WRITE(dspsurf_reg_d, ps3d_flip->uiAddr_r);

	/* Try to attach/de-attach Plane B to an existing swap chain,
	 * especially with another frame buffer inserted into GTT. */
	/* eError = MRSTLFBChangeSwapChainProperty(&Start, pipe); */

	if ((pipe == 0) || (pipe == 2)) {
		/*set up mipi port related registers */
		REG_WRITE(mipi_reg, mipi_val);

		/*setup MIPI adapter + MIPI IP registers */
		/* mdfld_dsi_controller_init(dsi_config, pipe); */
		mdelay(20);	/* msleep(20); */

		/* re-init the panel */
		dsi_config->drv_ic_inited = 0;
		/* mdfld_dsi_tmd_drv_ic_init(dsi_config, pipe); */
	}

	/*enable the plane */
	dspcntr_val &= ~(S3D_SPRITE_ORDER_BITS | S3D_SPRITE_INTERLEAVING_BITS);
	REG_WRITE(dspcntr_reg_d, dspcntr_val);
	dspcntr_val |= S3D_SPRITE_ORDER_A_FIRST | S3D_SPRITE_INTERLEAVING_LINE;
	REG_WRITE(dspcntr_reg, dspcntr_val);
	mdelay(20);		/* msleep(20); */
	/* psb_intel_wait_for_vblank(dev); */

	/*enable the pipe */
	REG_WRITE(pipeconf_reg, pipeconf_val);

	return 0;
}

/**
 * Perform 2d mode set back from half line interleaving 3D display.
 *
 */
int mrfld_s3d_from_line_interleave_half(struct drm_device *dev, int pipe, struct
					mrfld_s3d_flip *ps3d_flip)
{
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;
	struct mdfld_dsi_config *dsi_config = dev_priv->dsi_configs[0];

	/* register */
	u32 pipeconf_reg = PIPEACONF;
	u32 dspcntr_reg = DSPACNTR;
	u32 dspstride_reg = DSPASTRIDE;
	u32 dsplinoff_reg = DSPALINOFF;
	u32 dspsize_reg = DSPASIZE;
	u32 dspsurf_reg = DSPASURF;
	u32 mipi_reg = MIPI;

	/* values */
	u32 pipeconf_val = 0;
	u32 dspcntr_val = 0;
	u32 dspsize_val = 0;
	u32 mipi_val = 0;

	PSB_DEBUG_ENTRY("pipe = 0x%x \n", pipe);

	switch (pipe) {
	case 0:
		break;
	case 1:
		DRM_ERROR("HDMI doesn't support line interleave half. \n");
		return 0;
	case 2:
		dsi_config = dev_priv->dsi_configs[1];
		pipeconf_reg = PIPECCONF;
		dspcntr_reg = DSPCCNTR;
		dspstride_reg = DSPCSTRIDE;
		dsplinoff_reg = DSPCLINOFF;
		dspsize_reg = DSPCSIZE;
		dspsurf_reg = DSPCSURF;
		mipi_reg = MIPI_C;
		DRM_ERROR("HDMI doesn't support line interleave half. \n");
		return 0;
	default:
		DRM_ERROR("Illegal Pipe Number. \n");
		return 0;
	}

	/* Save the related DC registers. */
	pipeconf_val = REG_READ(pipeconf_reg);
	dspcntr_val = REG_READ(dspcntr_reg);
	dspsize_val = REG_READ(dspsize_reg);
	mipi_val = REG_READ(mipi_reg);

	/* Disable pipe and port, don't disable the PLL. */
	mrfld_disable_crtc(dev, pipe, true);

	/* restore to 2D display size. */
	REG_WRITE(dspsize_reg, (((dspsize_val & 0xFFFF0000) + 0x00010000) * 2 -
				0x00010000) | (dspsize_val & 0x0000FFFF));

	/* set up the frame buffer stride, offset and start. */
	REG_WRITE(dspstride_reg, ps3d_flip->pitch_l);
	REG_WRITE(dsplinoff_reg, 0);
	REG_WRITE(dspsurf_reg, ps3d_flip->uiAddr_l);

	/* Try to attach/de-attach Plane B to an existing swap chain,
	 * especially with another frame buffer inserted into GTT. */
	/* eError = MRSTLFBChangeSwapChainProperty(&Start, pipe); */

	if ((pipe == 0) || (pipe == 2)) {
		/*set up mipi port related registers */
		REG_WRITE(mipi_reg, mipi_val);

		/*setup MIPI adapter + MIPI IP registers */
		/* mdfld_dsi_controller_init(dsi_config, pipe); */
		mdelay(20);	/* msleep(20); */

		/* re-init the panel */
		dsi_config->drv_ic_inited = 0;
		/* mdfld_dsi_tmd_drv_ic_init(dsi_config, pipe); */
	}

	/*enable the plane */
	dspcntr_val &= ~(S3D_SPRITE_ORDER_BITS | S3D_SPRITE_INTERLEAVING_BITS);
	REG_WRITE(dspcntr_reg, dspcntr_val);
	mdelay(20);		/* msleep(20); */
	/* psb_intel_wait_for_vblank(dev); */

	/*enable the pipe */
	REG_WRITE(pipeconf_reg, pipeconf_val);

	return 0;
}

/**
 * Perform display 3D mode set to line interleaving 3D display with two buffers.
 *
 * FIXME: Assume the 3D buffer is the same as display resolution. Will re-visit
 * it for panel fitting mode.
 * Set up the pll, two times 2D clock.
 */
int mrfld_s3d_to_line_interleave(struct drm_device *dev, int pipe, struct
				 mrfld_s3d_flip *ps3d_flip)
{
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;
	struct mdfld_dsi_config *dsi_config = dev_priv->dsi_configs[0];

	/* register */
	u32 pipeconf_reg = PIPEACONF;
	u32 pipesrc_reg = PIPEASRC;
	u32 vtot_reg = VTOTAL_A;
	u32 vblank_reg = VBLANK_A;
	u32 vsync_reg = VSYNC_A;

	u32 dspcntr_reg = DSPACNTR;
	u32 dspstride_reg = DSPASTRIDE;
	u32 dsplinoff_reg = DSPALINOFF;
	u32 dspsize_reg = DSPASIZE;
	u32 dspsurf_reg = DSPASURF;
	u32 mipi_reg = MIPI;

	u32 dspcntr_reg_d = DSPDCNTR;
	u32 dspstride_reg_d = DSPDSTRIDE;
	u32 dsplinoff_reg_d = DSPDLINOFF;
	u32 dspsize_reg_d = DSPDSIZE;
	u32 dspsurf_reg_d = DSPDSURF;

	/* values */
	u32 pipeconf_val = 0;
	u32 pipesrc_val = 0;
	u32 vtot_val = 0;
	u32 vblank_val = 0;
	u32 vsync_val = 0;

	u32 dspcntr_val = 0;
	u32 dspsize_val = 0;
	u32 mipi_val = 0;
	u32 temp = 0;
	u32 temp1 = 0;

	PSB_DEBUG_ENTRY("pipe = 0x%x \n", pipe);

	switch (pipe) {
	case 0:
		break;
	case 1:
		pipeconf_reg = PIPEBCONF;
		pipesrc_reg = PIPEBSRC;
		vtot_reg = VTOTAL_B;
		vblank_reg = VBLANK_B;
		vsync_reg = VSYNC_B;
		dspcntr_reg = DSPBCNTR;
		dspstride_reg = DSPBSTRIDE;
		dsplinoff_reg = DSPBLINOFF;
		dspsize_reg = DSPBSIZE;
		dspsurf_reg = DSPBSURF;
		break;
	case 2:
		dsi_config = dev_priv->dsi_configs[1];
		pipeconf_reg = PIPECCONF;
		pipesrc_reg = PIPECSRC;
		vtot_reg = VTOTAL_C;
		vblank_reg = VBLANK_C;
		vsync_reg = VSYNC_C;
		dspcntr_reg = DSPCCNTR;
		dspstride_reg = DSPCSTRIDE;
		dsplinoff_reg = DSPCLINOFF;
		dspsize_reg = DSPCSIZE;
		dspsurf_reg = DSPCSURF;
		mipi_reg = MIPI_C;
		break;
	default:
		DRM_ERROR("Illegal Pipe Number. \n");
		return 0;
	}

	/* Save the related DC registers. */
	pipeconf_val = REG_READ(pipeconf_reg);
	pipesrc_val = REG_READ(pipesrc_reg);
	vtot_val = REG_READ(vtot_reg);
	vblank_val = REG_READ(vblank_reg);
	vsync_val = REG_READ(vsync_reg);

	dspcntr_val = REG_READ(dspcntr_reg);
	dspsize_val = REG_READ(dspsize_reg);
	mipi_val = REG_READ(mipi_reg);

	/* Disable pipe and port. */
	mrfld_disable_crtc(dev, pipe, true);

	/* Set up line interleaving display 3D format. */
	/* Get the 3D pipe source. */
	REG_WRITE(pipesrc_reg, (((pipesrc_val & 0x0000ffff) + 1) * 2 - 1) |
		  (pipesrc_val & 0xFFFF0000));

	/* Get the 3D Vactive and Vtotal. */
	temp = ((vtot_val & 0x0000FFFF) + 1) * 2 - 1;
	temp1 = ((vtot_val & 0xFFFF0000) + 0x00010000) * 2 - 0x00010000;
	REG_WRITE(vtot_reg, temp1 | temp);

	/* Get the 3D Vblank. */
	temp = ((vblank_val & 0x0000FFFF) + 1) * 2 - 1;
	temp1 = ((vblank_val & 0xFFFF0000) + 0x00010000) * 2 - 0x00010000;
	REG_WRITE(vblank_reg, temp1 | temp);

	/* Get the 3D Vsync */
	temp = ((vsync_val & 0x0000FFFF) + 1) * 2 - 1;
	temp1 = ((vsync_val & 0xFFFF0000) + 0x00010000) * 2 - 0x00010000;
	REG_WRITE(vsync_reg, temp1 | temp);

	/* set up plane related registers */
	REG_WRITE(dspsize_reg_d, dspsize_val);

	/* set up the frame buffer stride, offset and start. */
	REG_WRITE(dspstride_reg, ps3d_flip->pitch_l);
	REG_WRITE(dspstride_reg_d, ps3d_flip->pitch_r);
	REG_WRITE(dsplinoff_reg, 0);
	REG_WRITE(dsplinoff_reg_d, 0);
	REG_WRITE(dspsurf_reg, ps3d_flip->uiAddr_l);
	REG_WRITE(dspsurf_reg_d, ps3d_flip->uiAddr_r);

	/* Try to attach/de-attach Plane B to an existing swap chain,
	 * especially with another frame buffer inserted into GTT. */
	/* eError = MRSTLFBChangeSwapChainProperty(&Start, pipe); */

	if ((pipe == 0) || (pipe == 2)) {
		/*set up mipi port related registers */
		REG_WRITE(mipi_reg, mipi_val);

		/*setup MIPI adapter + MIPI IP registers */
		/* mdfld_dsi_controller_init(dsi_config, pipe); */
		mdelay(20);	/* msleep(20); */

		/* re-init the panel */
		dsi_config->drv_ic_inited = 0;
		/* mdfld_dsi_tmd_drv_ic_init(dsi_config, pipe); */
	}

	/*enable the plane */
	dspcntr_val &= ~(S3D_SPRITE_ORDER_BITS | S3D_SPRITE_INTERLEAVING_BITS);
	REG_WRITE(dspcntr_reg_d, dspcntr_val);
	dspcntr_val |= S3D_SPRITE_ORDER_A_FIRST | S3D_SPRITE_INTERLEAVING_LINE;
	REG_WRITE(dspcntr_reg, dspcntr_val);
	mdelay(20);		/* msleep(20); */
	/* psb_intel_wait_for_vblank(dev); */

	/*enable the pipe */
	REG_WRITE(pipeconf_reg, pipeconf_val);

	/* set up Vendor Specific InfoFrame for 3D format. */
	if (pipe == 1)
		mrfld_set_up_s3d_InfoFrame(dev, S3D_LINE_ALTERNATIVE);

	return 0;
}

/**
 * Perform 2d mode set back from line interleaving 3D display.
 *
 */
int mrfld_s3d_from_line_interleave(struct drm_device *dev, int pipe, struct
				   mrfld_s3d_flip *ps3d_flip)
{
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;
	struct mdfld_dsi_config *dsi_config = dev_priv->dsi_configs[0];

	/* register */
	u32 pipeconf_reg = PIPEACONF;
	u32 pipesrc_reg = PIPEASRC;
	u32 vtot_reg = VTOTAL_A;
	u32 vblank_reg = VBLANK_A;
	u32 vsync_reg = VSYNC_A;

	u32 dspcntr_reg = DSPACNTR;
	u32 dspstride_reg = DSPASTRIDE;
	u32 dsplinoff_reg = DSPALINOFF;
	u32 dspsize_reg = DSPASIZE;
	u32 dspsurf_reg = DSPASURF;
	u32 mipi_reg = MIPI;

	/* values */
	u32 pipeconf_val = 0;
	u32 pipesrc_val = 0;
	u32 vtot_val = 0;
	u32 vblank_val = 0;
	u32 vsync_val = 0;

	u32 dspcntr_val = 0;
	u32 dspsize_val = 0;
	u32 mipi_val = 0;
	u32 temp = 0;
	u32 temp1 = 0;

	PSB_DEBUG_ENTRY("pipe = 0x%x \n", pipe);

	switch (pipe) {
	case 0:
		break;
	case 1:
		pipeconf_reg = PIPEBCONF;
		pipesrc_reg = PIPEBSRC;
		vtot_reg = VTOTAL_B;
		vblank_reg = VBLANK_B;
		vsync_reg = VSYNC_B;
		dspcntr_reg = DSPBCNTR;
		dspstride_reg = DSPBSTRIDE;
		dsplinoff_reg = DSPBLINOFF;
		dspsize_reg = DSPBSIZE;
		dspsurf_reg = DSPBSURF;
		break;
	case 2:
		dsi_config = dev_priv->dsi_configs[1];
		pipeconf_reg = PIPECCONF;
		pipesrc_reg = PIPECSRC;
		vtot_reg = VTOTAL_C;
		vblank_reg = VBLANK_C;
		vsync_reg = VSYNC_C;
		pipeconf_reg = PIPECCONF;
		dspcntr_reg = DSPCCNTR;
		dspstride_reg = DSPCSTRIDE;
		dsplinoff_reg = DSPCLINOFF;
		dspsize_reg = DSPCSIZE;
		dspsurf_reg = DSPCSURF;
		mipi_reg = MIPI_C;
		break;
	default:
		DRM_ERROR("Illegal Pipe Number. \n");
		return 0;
	}

	/* Save the related DC registers. */
	pipeconf_val = REG_READ(pipeconf_reg);
	pipesrc_val = REG_READ(pipesrc_reg);
	vtot_val = REG_READ(vtot_reg);
	vblank_val = REG_READ(vblank_reg);
	vsync_val = REG_READ(vsync_reg);

	dspcntr_val = REG_READ(dspcntr_reg);
	dspsize_val = REG_READ(dspsize_reg);
	mipi_val = REG_READ(mipi_reg);

	/* Disable pipe and port. */
	mrfld_disable_crtc(dev, pipe, true);

	/* Set up the pll, half of 3D clock. */
	/* mrfld_setup_pll (dev, pipe, adjusted_mode->clock); */

	/* set up pipe related registers */
	/* Get the 2D pipe source. */
	REG_WRITE(pipesrc_reg, (((pipesrc_val & 0x0000ffff) + 1) / 2 - 1) |
		  (pipesrc_val & 0xFFFF0000));

	/* Get the 2D Vactive and Vtotal. */
	temp = ((vtot_val & 0x0000FFFF) + 1) / 2 - 1;
	temp1 = ((vtot_val & 0xFFFF0000) + 0x00010000) / 2 - 0x00010000;
	REG_WRITE(vtot_reg, temp1 | temp);

	/* Get the 2D Vblank. */
	temp = ((vblank_val & 0x0000FFFF) + 1) / 2 - 1;
	temp1 = ((vblank_val & 0xFFFF0000) + 0x00010000) / 2 - 0x00010000;
	REG_WRITE(vblank_reg, temp1 | temp);

	/* Get the 2D Vsync */
	temp = ((vsync_val & 0x0000FFFF) + 1) / 2 - 1;
	temp1 = ((vsync_val & 0xFFFF0000) + 0x00010000) / 2 - 0x00010000;
	REG_WRITE(vsync_reg, temp1 | temp);

	/* set up the frame buffer stride, offset and start. */
	REG_WRITE(dspstride_reg, ps3d_flip->pitch_l);
	REG_WRITE(dsplinoff_reg, 0);
	REG_WRITE(dspsurf_reg, ps3d_flip->uiAddr_l);

	/* Try to attach/de-attach Plane B to an existing swap chain,
	 * especially with another frame buffer inserted into GTT. */
	/* eError = MRSTLFBChangeSwapChainProperty(&Start, pipe); */

	if ((pipe == 0) || (pipe == 2)) {
		/*set up mipi port related registers */
		REG_WRITE(mipi_reg, mipi_val);

		/*setup MIPI adapter + MIPI IP registers */
		/* mdfld_dsi_controller_init(dsi_config, pipe); */
		mdelay(20);	/* msleep(20); */

		/* re-init the panel */
		dsi_config->drv_ic_inited = 0;
		/* mdfld_dsi_tmd_drv_ic_init(dsi_config, pipe); */
	}

	/*enable the plane */
	dspcntr_val &= ~(S3D_SPRITE_ORDER_BITS | S3D_SPRITE_INTERLEAVING_BITS);
	REG_WRITE(dspcntr_reg, dspcntr_val);
	mdelay(20);		/* msleep(20); */
	/* psb_intel_wait_for_vblank(dev); */

	/*enable the pipe */
	REG_WRITE(pipeconf_reg, pipeconf_val);

	/* set up Vendor Specific InfoFrame for 3D format. */
	if (pipe == 1)
		mrfld_disable_s3d_InfoFrame(dev);

	return 0;
}

/**
 * Perform display 3D mode set to frame packing 3D display.
 *
 * FIXME: Assume the 3D buffer is the same as display resolution. Will re-visit
 * it for panel fitting mode.
 * Set up the pll, two times 2D clock.
 */
int mrfld_s3d_to_frame_packing(struct drm_device *dev, int pipe, struct
			       mrfld_s3d_flip *ps3d_flip)
{
	/* register */
	u32 pipeconf_reg = PIPEBCONF;
	u32 pipesrc_reg = PIPEBSRC;
	u32 vtot_reg = VTOTAL_B;
	u32 vblank_reg = VBLANK_B;
	u32 vsync_reg = VSYNC_B;

	u32 dspcntr_reg = DSPBCNTR;
	u32 dspstride_reg = DSPBSTRIDE;
	u32 dsplinoff_reg = DSPBLINOFF;
	u32 dspsize_reg = DSPBSIZE;
	u32 dspsurf_reg = DSPBSURF;

	u32 dspcntr_reg_d = DSPDCNTR;
	u32 dspstride_reg_d = DSPDSTRIDE;
	u32 dsplinoff_reg_d = DSPDLINOFF;
	u32 dspsize_reg_d = DSPDSIZE;
	u32 dspsurf_reg_d = DSPDSURF;
	u32 dsppos_reg_d = DSPDPOS;

	/* values */
	u32 pipeconf_val = 0;
	u32 pipesrc_val = 0;
	u32 vtot_val = 0;
	u32 vblank_val = 0;
	u32 vsync_val = 0;

	u32 dspcntr_val = 0;
	u32 dspsize_val = 0;
	u32 dsppos_val = 0;
	u32 temp = 0;
	u32 temp1 = 0;
	u32 temp2 = 0;
	u32 temp3 = 0;

	PSB_DEBUG_ENTRY("pipe = 0x%x \n", pipe);

	if (pipe != 1) {
		DRM_ERROR("Illegal Pipe Number. \n");
		return 0;
	}

	/* Save the related DC registers. */
	pipeconf_val = REG_READ(pipeconf_reg);
	pipesrc_val = REG_READ(pipesrc_reg);
	vtot_val = REG_READ(vtot_reg);
	vblank_val = REG_READ(vblank_reg);
	vsync_val = REG_READ(vsync_reg);

	dspcntr_val = REG_READ(dspcntr_reg);
	dspsize_val = REG_READ(dspsize_reg);

	/* Disable pipe and port. */
	mrfld_disable_crtc(dev, pipe, true);

	/* Set up the pll, two times 2D clock. */
	/* mrfld_setup_pll (dev, pipe, adjusted_mode->clock); */

	/* Set up frame packing display 3D format. */

	/* set up pipe related registers */
	/* Get the Vblank and Vborder period. */
	temp = ((vtot_val & 0xFFFF0000) >> 16) - (vtot_val & 0x0000FFFF);

	/* Get the 3D pipe source. */
	REG_WRITE(pipesrc_reg,
		  (((pipesrc_val & 0x0000ffff) + 1) * 2 + temp -
		   1) | (pipesrc_val & 0xFFFF0000));
	dsppos_val = temp + (pipesrc_val & 0x0000ffff) + 1;
	dsppos_val <<= 16;

	/* Get the 3D Vactive. */
	temp += ((vtot_val & 0x0000FFFF) + 1) * 2 - 1;

	/* Get the 3D Vtotal. */
	temp1 = ((vtot_val & 0xFFFF0000) + 0x00010000) * 2 - 0x00010000;

	REG_WRITE(vtot_reg, temp1 | temp);

	/* Get the 3D Vblank. */
	temp2 = (vblank_val & 0x0000FFFF) - (vtot_val & 0x0000FFFF);
	temp3 = (vtot_val & 0xFFFF0000) - (vblank_val & 0xFFFF0000);
	REG_WRITE(vblank_reg, (temp1 - temp3) | (temp + temp2));

	/* Get the 3D Vsync */
	temp2 = (vsync_val & 0x0000FFFF) - (vtot_val & 0x0000FFFF);
	temp3 = (vtot_val & 0xFFFF0000) - (vsync_val & 0xFFFF0000);
	REG_WRITE(vsync_reg, (temp1 - temp3) | (temp + temp2));

	/* set up plane related registers */
	REG_WRITE(dspsize_reg_d, dspsize_val);
	REG_WRITE(dsppos_reg_d, dsppos_val);

	/* set up the frame buffer stride, offset and start. */
	REG_WRITE(dspstride_reg, ps3d_flip->pitch_l);
	REG_WRITE(dspstride_reg_d, ps3d_flip->pitch_r);
	REG_WRITE(dsplinoff_reg, 0);
	REG_WRITE(dsplinoff_reg_d, 0);
	REG_WRITE(dspsurf_reg, ps3d_flip->uiAddr_l);
	REG_WRITE(dspsurf_reg_d, ps3d_flip->uiAddr_r);

	/* Try to attach/de-attach Plane B to an existing swap chain,
	 * especially with another frame buffer inserted into GTT. */
	/* eError = MRSTLFBChangeSwapChainProperty(&Start, pipe); */

	/*enable the plane */
	dspcntr_val &= ~(S3D_SPRITE_ORDER_BITS | S3D_SPRITE_INTERLEAVING_BITS);
	REG_WRITE(dspcntr_reg, dspcntr_val);
	REG_WRITE(dspcntr_reg_d, dspcntr_val);
	mdelay(20);		/* msleep(20); */
	/* psb_intel_wait_for_vblank(dev); */

	/*enable the pipe */
	REG_WRITE(pipeconf_reg, pipeconf_val);

	/* set up Vendor Specific InfoFrame for 3D format. */
	if (pipe == 1)
		mrfld_set_up_s3d_InfoFrame(dev, S3D_FRAME_PACKING);

	return 0;
}

/**
 * Perform 2d mode set back from frame packing 3D display.
 *
 */
int mrfld_s3d_from_frame_packing(struct drm_device *dev, int pipe, struct
				 mrfld_s3d_flip *ps3d_flip)
{
	/* register */
	u32 pipeconf_reg = PIPEBCONF;
	u32 pipesrc_reg = PIPEBSRC;
	u32 vtot_reg = VTOTAL_B;
	u32 vblank_reg = VBLANK_B;
	u32 vsync_reg = VSYNC_B;

	u32 dspcntr_reg = DSPBCNTR;
	u32 dspstride_reg = DSPBSTRIDE;
	u32 dsplinoff_reg = DSPBLINOFF;
	u32 dspsize_reg = DSPBSIZE;
	u32 dspsurf_reg = DSPBSURF;

	/* values */
	u32 pipeconf_val = 0;
	u32 pipesrc_val = 0;
	u32 vtot_val = 0;
	u32 vblank_val = 0;
	u32 vsync_val = 0;

	u32 dspcntr_val = 0;
	u32 dspsize_val = 0;
	u32 temp = 0;
	u32 temp1 = 0;
	u32 temp2 = 0;
	u32 temp3 = 0;

	PSB_DEBUG_ENTRY("pipe = 0x%x \n", pipe);

	if (pipe != 1) {
		DRM_ERROR("Illegal Pipe Number. \n");
		return 0;
	}

	/* Save the related DC registers. */
	pipeconf_val = REG_READ(pipeconf_reg);
	pipesrc_val = REG_READ(pipesrc_reg);
	vtot_val = REG_READ(vtot_reg);
	vblank_val = REG_READ(vblank_reg);
	vsync_val = REG_READ(vsync_reg);

	dspcntr_val = REG_READ(dspcntr_reg);
	dspsize_val = REG_READ(dspsize_reg);

	/* Disable pipe and port. */
	mrfld_disable_crtc(dev, pipe, true);

	/* Set up the pll, half of 3D clock. */
	/* mrfld_setup_pll (dev, pipe, adjusted_mode->clock); */

	/* set up pipe related registers */
	/* Get the Vblank and Vborder period. */
	temp = ((vtot_val & 0xFFFF0000) >> 16) - (vtot_val & 0x0000FFFF);

	/* Get the 2D pipe source. */
	temp1 = ((pipesrc_val & 0x0000ffff) + 1 - temp) / 2 - 1;
	REG_WRITE(pipesrc_reg, temp1 | (pipesrc_val & 0xFFFF0000));

	/* Get the 2D Vactive. */
	temp = ((vtot_val & 0x0000FFFF) + 1 - temp) / 2 - 1;

	/* Get the 2D Vtotal. */
	temp1 = ((vtot_val & 0xFFFF0000) + 0x00010000) / 2 - 0x00010000;

	REG_WRITE(vtot_reg, temp1 | temp);

	/* Get the 2D Vblank. */
	temp2 = (vblank_val & 0x0000FFFF) - (vtot_val & 0x0000FFFF);
	temp3 = (vtot_val & 0xFFFF0000) - (vblank_val & 0xFFFF0000);
	REG_WRITE(vblank_reg, (temp1 - temp3) | (temp + temp2));

	/* Get the 3D Vsync */
	temp2 = (vsync_val & 0x0000FFFF) - (vtot_val & 0x0000FFFF);
	temp3 = (vtot_val & 0xFFFF0000) - (vsync_val & 0xFFFF0000);
	REG_WRITE(vblank_reg, (temp1 - temp3) | (temp + temp2));

	/* set up plane related registers */
	/* set up the frame buffer stride, offset and start. */
	REG_WRITE(dspstride_reg, ps3d_flip->pitch_l);
	REG_WRITE(dsplinoff_reg, 0);
	REG_WRITE(dspsurf_reg, ps3d_flip->uiAddr_l);

	/* Try to attach/de-attach Plane B to an existing swap chain,
	 * especially with another frame buffer inserted into GTT. */
	/* eError = MRSTLFBChangeSwapChainProperty(&Start, pipe); */

	/*enable the plane */
	dspcntr_val &= ~(S3D_SPRITE_ORDER_BITS | S3D_SPRITE_INTERLEAVING_BITS);
	REG_WRITE(dspcntr_reg, dspcntr_val);
	mdelay(20);		/* msleep(20); */
	/* psb_intel_wait_for_vblank(dev); */

	/*enable the pipe */
	REG_WRITE(pipeconf_reg, pipeconf_val);

	/* set up Vendor Specific InfoFrame for 3D format. */
	if (pipe == 1)
		mrfld_disable_s3d_InfoFrame(dev);

	return 0;
}

/**
 * Perform display 3D mode set to top_and_bottom 3D display.
 *
 * FIXME: Assume the 3D buffer is the same as display resolution. Will re-visit
 * it for panel fitting mode.
 */
int mrfld_s3d_to_top_and_bottom(struct drm_device *dev, int pipe, struct
				mrfld_s3d_flip *ps3d_flip)
{
	/* register */
	u32 pipeconf_reg = PIPEBCONF;

	u32 dspcntr_reg = DSPBCNTR;
	u32 dspstride_reg = DSPBSTRIDE;
	u32 dsplinoff_reg = DSPBLINOFF;
	u32 dspsize_reg = DSPBSIZE;
	u32 dspsurf_reg = DSPBSURF;

	u32 dspcntr_reg_d = DSPDCNTR;
	u32 dspstride_reg_d = DSPDSTRIDE;
	u32 dsplinoff_reg_d = DSPDLINOFF;
	u32 dspsize_reg_d = DSPDSIZE;
	u32 dspsurf_reg_d = DSPDSURF;
	u32 dsppos_reg_d = DSPDPOS;

	/* values */
	u32 pipeconf_val = 0;

	u32 dspcntr_val = 0;
	u32 dspsize_val = 0;
	u32 dsppos_val = 0;

	PSB_DEBUG_ENTRY("pipe = 0x%x \n", pipe);

	if (pipe != 1) {
		DRM_ERROR("Illegal Pipe Number. \n");
		return 0;
	}

	/* Save the related DC registers. */
	pipeconf_val = REG_READ(pipeconf_reg);
	dspcntr_val = REG_READ(dspcntr_reg);
	dspsize_val = REG_READ(dspsize_reg);

	/* Disable pipe and port, don't disable the PLL. */
	mrfld_disable_crtc(dev, pipe, true);

	/* set up plane related registers */
	dsppos_val = ((dspsize_val & 0xFFFF0000) + 0x00010000) / 2;
	dspsize_val = (((dspsize_val & 0xFFFF0000) + 0x00010000) / 2 -
		       0x00010000) | (dspsize_val & 0x0000FFFF);
	REG_WRITE(dspsize_reg, dspsize_val);
	REG_WRITE(dspsize_reg_d, dspsize_val);
	REG_WRITE(dsppos_reg_d, dsppos_val);

	/* set up the frame buffer stride, offset and start. */
	REG_WRITE(dspstride_reg, ps3d_flip->pitch_l);
	REG_WRITE(dspstride_reg_d, ps3d_flip->pitch_r);
	REG_WRITE(dsplinoff_reg, 0);
	REG_WRITE(dsplinoff_reg_d, 0);
	REG_WRITE(dspsurf_reg, ps3d_flip->uiAddr_l);
	REG_WRITE(dspsurf_reg_d, ps3d_flip->uiAddr_r);

	/* Try to attach/de-attach Plane B to an existing swap chain,
	 * especially with another frame buffer inserted into GTT. */
	/* eError = MRSTLFBChangeSwapChainProperty(&Start, pipe); */

	/*enable the plane */
	dspcntr_val &= ~(S3D_SPRITE_ORDER_BITS | S3D_SPRITE_INTERLEAVING_BITS);
	REG_WRITE(dspcntr_reg, dspcntr_val);
	REG_WRITE(dspcntr_reg_d, dspcntr_val);
	mdelay(20);		/* msleep(20); */
	/* psb_intel_wait_for_vblank(dev); */

	/*enable the pipe */
	REG_WRITE(pipeconf_reg, pipeconf_val);

	/* set up Vendor Specific InfoFrame for 3D format. */
	if (pipe == 1)
		mrfld_set_up_s3d_InfoFrame(dev, S3D_TOP_AND_BOTTOM);

	return 0;
}

/**
 * Perform 2d mode set back from top_and_bottom 3D display.
 *
 */
int mrfld_s3d_from_top_and_bottom(struct drm_device *dev, int pipe, struct
				  mrfld_s3d_flip *ps3d_flip)
{
	/* register */
	u32 pipeconf_reg = PIPEBCONF;

	u32 dspcntr_reg = DSPBCNTR;
	u32 dspstride_reg = DSPBSTRIDE;
	u32 dsplinoff_reg = DSPBLINOFF;
	u32 dspsize_reg = DSPBSIZE;
	u32 dspsurf_reg = DSPBSURF;

	/* values */
	u32 pipeconf_val = 0;
	u32 dspcntr_val = 0;
	u32 dspsize_val = 0;

	PSB_DEBUG_ENTRY("pipe = 0x%x \n", pipe);

	if (pipe != 1) {
		DRM_ERROR("Illegal Pipe Number. \n");
		return 0;
	}

	/* Save the related DC registers. */
	pipeconf_val = REG_READ(pipeconf_reg);
	dspcntr_val = REG_READ(dspcntr_reg);
	dspsize_val = REG_READ(dspsize_reg);

	/* Disable pipe and port, don't disable the PLL. */
	mrfld_disable_crtc(dev, pipe, true);

	/* set up plane related registers */
	dspsize_val = (((dspsize_val & 0xFFFF0000) + 0x00010000) * 2 -
		       0x00010000) | (dspsize_val & 0x0000FFFF);
	REG_WRITE(dspsize_reg, dspsize_val);

	/* set up plane related registers */
	/* set up the frame buffer stride, offset and start. */
	REG_WRITE(dspstride_reg, ps3d_flip->pitch_l);
	REG_WRITE(dsplinoff_reg, 0);
	REG_WRITE(dspsurf_reg, ps3d_flip->uiAddr_l);

	/* Try to attach/de-attach Plane B to an existing swap chain,
	 * especially with another frame buffer inserted into GTT. */
	/* eError = MRSTLFBChangeSwapChainProperty(&Start, pipe); */

	/*enable the plane */
	dspcntr_val &= ~(S3D_SPRITE_ORDER_BITS | S3D_SPRITE_INTERLEAVING_BITS);
	REG_WRITE(dspcntr_reg, dspcntr_val);
	mdelay(20);		/* msleep(20); */
	/* psb_intel_wait_for_vblank(dev); */

	/*enable the pipe */
	REG_WRITE(pipeconf_reg, pipeconf_val);

	/* set up Vendor Specific InfoFrame for 3D format. */
	if (pipe == 1)
		mrfld_disable_s3d_InfoFrame(dev);

	return 0;
}

/**
 * Perform display 3D mode set to full side_by_side 3D display.
 *
 * FIXME: Assume the 3D buffer is the same as display resolution. Will re-visit
 * it for panel fitting mode.
 * Set up the pll, two times 2D clock.
 */
int mrfld_s3d_to_full_side_by_side(struct drm_device *dev, int pipe, struct
				   mrfld_s3d_flip *ps3d_flip)
{
	/* register */
	u32 pipeconf_reg = PIPEBCONF;
	u32 pipesrc_reg = PIPEBSRC;
	u32 htot_reg = HTOTAL_B;
	u32 hblank_reg = HBLANK_B;
	u32 hsync_reg = HSYNC_B;

	u32 dspcntr_reg = DSPBCNTR;
	u32 dspstride_reg = DSPBSTRIDE;
	u32 dsplinoff_reg = DSPBLINOFF;
	u32 dspsize_reg = DSPBSIZE;
	u32 dspsurf_reg = DSPBSURF;

	u32 dspcntr_reg_d = DSPDCNTR;
	u32 dspstride_reg_d = DSPDSTRIDE;
	u32 dsplinoff_reg_d = DSPDLINOFF;
	u32 dspsize_reg_d = DSPDSIZE;
	u32 dspsurf_reg_d = DSPDSURF;
	u32 dsppos_reg_d = DSPDPOS;

	/* values */
	u32 pipeconf_val = 0;
	u32 pipesrc_val = 0;
	u32 htot_val = 0;
	u32 hblank_val = 0;
	u32 hsync_val = 0;

	u32 dspcntr_val = 0;
	u32 dspsize_val = 0;
	u32 dsppos_val = 0;
	u32 temp = 0;
	u32 temp1 = 0;

	PSB_DEBUG_ENTRY("pipe = 0x%x \n", pipe);

	if (pipe != 1) {
		DRM_ERROR("Illegal Pipe Number. \n");
		return 0;
	}

	/* Save the related DC registers. */
	pipeconf_val = REG_READ(pipeconf_reg);
	pipesrc_val = REG_READ(pipesrc_reg);
	htot_val = REG_READ(htot_reg);
	hblank_val = REG_READ(hblank_reg);
	hsync_val = REG_READ(hsync_reg);

	dspcntr_val = REG_READ(dspcntr_reg);
	dspsize_val = REG_READ(dspsize_reg);

	/* Disable pipe and port. */
	mrfld_disable_crtc(dev, pipe, true);

	/* Set up the pll, two times 2D clock. */
	/* mrfld_setup_pll (dev, pipe, adjusted_mode->clock); */

	/* Set up full side-by-side display 3D format. */

	/* set up pipe related registers */
	/* Get the 3D pipe source. */
	REG_WRITE(pipesrc_reg, (pipesrc_val & 0x0000ffff) |
		  (((pipesrc_val & 0xFFFF0000) + 0x00010000) * 2 - 0x00010000));

	/* Get the 3D Hactive and Htotal. */
	temp = ((htot_val & 0x0000FFFF) + 1) * 2 - 1;
	temp1 = ((htot_val & 0xFFFF0000) + 0x00010000) * 2 - 0x00010000;
	REG_WRITE(htot_reg, temp1 | temp);

	/* Get the 3D Hblank. */
	temp = ((hblank_val & 0x0000FFFF) + 1) * 2 - 1;
	temp1 = ((hblank_val & 0xFFFF0000) + 0x00010000) * 2 - 0x00010000;
	REG_WRITE(hblank_reg, temp1 | temp);

	/* Get the 3D Hsync */
	temp = ((hsync_val & 0x0000FFFF) + 1) * 2 - 1;
	temp1 = ((hsync_val & 0xFFFF0000) + 0x00010000) * 2 - 0x00010000;
	REG_WRITE(hsync_reg, temp1 | temp);

	/* set up plane related registers */
	REG_WRITE(dspsize_reg_d, dspsize_val);
	dsppos_val = (dspsize_val & 0x0000ffff) + 1;
	REG_WRITE(dsppos_reg_d, dsppos_val);

	/* set up the frame buffer stride, offset and start. */
	REG_WRITE(dspstride_reg, ps3d_flip->pitch_l);
	REG_WRITE(dspstride_reg_d, ps3d_flip->pitch_r);
	REG_WRITE(dsplinoff_reg, 0);
	REG_WRITE(dsplinoff_reg_d, 0);
	REG_WRITE(dspsurf_reg, ps3d_flip->uiAddr_l);
	REG_WRITE(dspsurf_reg_d, ps3d_flip->uiAddr_r);

	/* Try to attach/de-attach Plane B to an existing swap chain,
	 * especially with another frame buffer inserted into GTT. */
	/* eError = MRSTLFBChangeSwapChainProperty(&Start, pipe); */

	/*enable the plane */
	dspcntr_val &= ~(S3D_SPRITE_ORDER_BITS | S3D_SPRITE_INTERLEAVING_BITS);
	REG_WRITE(dspcntr_reg, dspcntr_val);
	REG_WRITE(dspcntr_reg_d, dspcntr_val);
	mdelay(20);		/* msleep(20); */
	/* psb_intel_wait_for_vblank(dev); */

	/*enable the pipe */
	REG_WRITE(pipeconf_reg, pipeconf_val);

	/* set up Vendor Specific InfoFrame for 3D format. */
	if (pipe == 1)
		mrfld_set_up_s3d_InfoFrame(dev, S3D_SIDE_BY_SIDE_FULL);

	return 0;
}

/**
 * Perform 2d mode set back from full side_by_side 3D display.
 *
 */
int mrfld_s3d_from_full_side_by_side(struct drm_device *dev, int pipe, struct
				     mrfld_s3d_flip *ps3d_flip)
{
	/* register */
	u32 pipeconf_reg = PIPEBCONF;
	u32 pipesrc_reg = PIPEBSRC;
	u32 htot_reg = HTOTAL_B;
	u32 hblank_reg = HBLANK_B;
	u32 hsync_reg = HSYNC_B;

	u32 dspcntr_reg = DSPBCNTR;
	u32 dspstride_reg = DSPBSTRIDE;
	u32 dsplinoff_reg = DSPBLINOFF;
	u32 dspsize_reg = DSPBSIZE;
	u32 dspsurf_reg = DSPBSURF;

	/* values */
	u32 pipeconf_val = 0;
	u32 pipesrc_val = 0;
	u32 htot_val = 0;
	u32 hblank_val = 0;
	u32 hsync_val = 0;

	u32 dspcntr_val = 0;
	u32 dspsize_val = 0;
	u32 temp = 0;
	u32 temp1 = 0;

	PSB_DEBUG_ENTRY("pipe = 0x%x \n", pipe);

	if (pipe != 1) {
		DRM_ERROR("Illegal Pipe Number. \n");
		return 0;
	}

	/* Save the related DC registers. */
	pipeconf_val = REG_READ(pipeconf_reg);
	pipesrc_val = REG_READ(pipesrc_reg);
	htot_val = REG_READ(htot_reg);
	hblank_val = REG_READ(hblank_reg);
	hsync_val = REG_READ(hsync_reg);

	dspcntr_val = REG_READ(dspcntr_reg);
	dspsize_val = REG_READ(dspsize_reg);

	/* Disable pipe and port. */
	mrfld_disable_crtc(dev, pipe, true);

	/* Set up the pll, half of 3D clock. */
	/* mrfld_setup_pll (dev, pipe, adjusted_mode->clock); */

	/* set up pipe related registers */
	/* Get the 2D pipe source. */
	REG_WRITE(pipesrc_reg, (pipesrc_val & 0x0000ffff) |
		  (((pipesrc_val & 0xFFFF0000) + 0x00010000) / 2 - 0x00010000));

	/* Get the 2D Hactive and Htotal. */
	temp = ((htot_val & 0x0000FFFF) + 1) / 2 - 1;
	temp1 = ((htot_val & 0xFFFF0000) + 0x00010000) / 2 - 0x00010000;
	REG_WRITE(htot_reg, temp1 | temp);

	/* Get the 2D Hblank. */
	temp = ((hblank_val & 0x0000FFFF) + 1) / 2 - 1;
	temp1 = ((hblank_val & 0xFFFF0000) + 0x00010000) / 2 - 0x00010000;
	REG_WRITE(hblank_reg, temp1 | temp);

	/* Get the 2D Hsync */
	temp = ((hsync_val & 0x0000FFFF) + 1) / 2 - 1;
	temp1 = ((hsync_val & 0xFFFF0000) + 0x00010000) / 2 - 0x00010000;
	REG_WRITE(hsync_reg, temp1 | temp);

	/* set up plane related registers */
	/* set up the frame buffer stride, offset and start. */
	REG_WRITE(dspstride_reg, ps3d_flip->pitch_l);
	REG_WRITE(dsplinoff_reg, 0);
	REG_WRITE(dspsurf_reg, ps3d_flip->uiAddr_l);

	/* Try to attach/de-attach Plane B to an existing swap chain,
	 * especially with another frame buffer inserted into GTT. */
	/* eError = MRSTLFBChangeSwapChainProperty(&Start, pipe); */

	/*enable the plane */
	dspcntr_val &= ~(S3D_SPRITE_ORDER_BITS | S3D_SPRITE_INTERLEAVING_BITS);
	REG_WRITE(dspcntr_reg, dspcntr_val);
	mdelay(20);		/* msleep(20); */
	/* psb_intel_wait_for_vblank(dev); */

	/*enable the pipe */
	REG_WRITE(pipeconf_reg, pipeconf_val);

	/* set up Vendor Specific InfoFrame for 3D format. */
	if (pipe == 1)
		mrfld_disable_s3d_InfoFrame(dev);

	return 0;
}

/**
 * Perform display 3D mode set to half side_by_side 3D display.
 *
 * FIXME: Assume the 3D buffer is the same as display resolution. Will re-visit
 * it for panel fitting mode.
 */
int mrfld_s3d_to_half_side_by_side(struct drm_device *dev, int pipe, struct
				   mrfld_s3d_flip *ps3d_flip)
{
	/* register */
	u32 pipeconf_reg = PIPEBCONF;
	u32 dspcntr_reg = DSPBCNTR;
	u32 dspstride_reg = DSPBSTRIDE;
	u32 dsplinoff_reg = DSPBLINOFF;
	u32 dspsize_reg = DSPBSIZE;
	u32 dspsurf_reg = DSPBSURF;

	u32 dspcntr_reg_d = DSPDCNTR;
	u32 dspstride_reg_d = DSPDSTRIDE;
	u32 dsplinoff_reg_d = DSPDLINOFF;
	u32 dspsize_reg_d = DSPDSIZE;
	u32 dspsurf_reg_d = DSPDSURF;
	u32 dsppos_reg_d = DSPDPOS;

	/* values */
	u32 pipeconf_val = 0;
	u32 dspcntr_val = 0;
	u32 dspsize_val = 0;
	u32 dsppos_val = 0;

	PSB_DEBUG_ENTRY("pipe = 0x%x \n", pipe);

	if (pipe != 1) {
		DRM_ERROR("Illegal Pipe Number. \n");
		return 0;
	}

	/* Save the related DC registers. */
	pipeconf_val = REG_READ(pipeconf_reg);
	dspcntr_val = REG_READ(dspcntr_reg);
	dspsize_val = REG_READ(dspsize_reg);

	/* Disable pipe and port, don't disable the PLL. */
	mrfld_disable_crtc(dev, pipe, true);

	/* Set up half side-by-side display 3D format. */

	/* set up plane related registers */
	dsppos_val = ((dspsize_val & 0x0000ffff) + 1) / 2;
	REG_WRITE(dsppos_reg_d, dsppos_val);

	dspsize_val = (dspsize_val & 0xffff0000) | (((dspsize_val & 0x0000ffff)
						     + 1) / 2 - 1);
	REG_WRITE(dspsize_reg, dspsize_val);
	REG_WRITE(dspsize_reg_d, dspsize_val);

	/* set up the frame buffer stride, offset and start. */
	REG_WRITE(dspstride_reg, ps3d_flip->pitch_l);
	REG_WRITE(dspstride_reg_d, ps3d_flip->pitch_r);
	REG_WRITE(dsplinoff_reg, 0);
	REG_WRITE(dsplinoff_reg_d, 0);
	REG_WRITE(dspsurf_reg, ps3d_flip->uiAddr_l);
	REG_WRITE(dspsurf_reg_d, ps3d_flip->uiAddr_r);

	/* Try to attach/de-attach Plane B to an existing swap chain,
	 * especially with another frame buffer inserted into GTT. */
	/* eError = MRSTLFBChangeSwapChainProperty(&Start, pipe); */

	/*enable the plane */
	dspcntr_val &= ~(S3D_SPRITE_ORDER_BITS | S3D_SPRITE_INTERLEAVING_BITS);
	REG_WRITE(dspcntr_reg, dspcntr_val);
	REG_WRITE(dspcntr_reg_d, dspcntr_val);
	mdelay(20);		/* msleep(20); */
	/* psb_intel_wait_for_vblank(dev); */

	/*enable the pipe */
	REG_WRITE(pipeconf_reg, pipeconf_val);

	/* set up Vendor Specific InfoFrame for 3D format. */
	if (pipe == 1)
		mrfld_set_up_s3d_InfoFrame(dev, S3D_SIDE_BY_SIDE_HALF);

	return 0;
}

/**
 * Perform 2d mode set back from half side_by_side 3D display.
 *
 */
int mrfld_s3d_from_half_side_by_side(struct drm_device *dev, int pipe, struct
				     mrfld_s3d_flip *ps3d_flip)
{
	/* register */
	u32 pipeconf_reg = PIPEBCONF;
	u32 dspcntr_reg = DSPBCNTR;
	u32 dspstride_reg = DSPBSTRIDE;
	u32 dsplinoff_reg = DSPBLINOFF;
	u32 dspsize_reg = DSPBSIZE;
	u32 dspsurf_reg = DSPBSURF;

	/* values */
	u32 pipeconf_val = 0;
	u32 dspcntr_val = 0;
	u32 dspsize_val = 0;

	PSB_DEBUG_ENTRY("pipe = 0x%x \n", pipe);

	if (pipe != 1) {
		DRM_ERROR("Illegal Pipe Number. \n");
		return 0;
	}

	/* Save the related DC registers. */
	pipeconf_val = REG_READ(pipeconf_reg);
	dspcntr_val = REG_READ(dspcntr_reg);
	dspsize_val = REG_READ(dspsize_reg);

	/* Disable pipe and port, don't disable the PLL. */
	mrfld_disable_crtc(dev, pipe, true);

	/* set up plane related registers */
	dspsize_val = (dspsize_val & 0xffff0000) | (((dspsize_val & 0x0000ffff)
						     + 1) * 2 - 1);
	REG_WRITE(dspsize_reg, dspsize_val);

	/* set up the frame buffer stride, offset and start. */
	REG_WRITE(dspstride_reg, ps3d_flip->pitch_l);
	REG_WRITE(dsplinoff_reg, 0);
	REG_WRITE(dspsurf_reg, ps3d_flip->uiAddr_l);

	/* Try to attach/de-attach Plane B to an existing swap chain,
	 * especially with another frame buffer inserted into GTT. */
	/* eError = MRSTLFBChangeSwapChainProperty(&Start, pipe); */

	/*enable the plane */
	dspcntr_val &= ~(S3D_SPRITE_ORDER_BITS | S3D_SPRITE_INTERLEAVING_BITS);
	REG_WRITE(dspcntr_reg, dspcntr_val);
	mdelay(20);		/* msleep(20); */
	/* psb_intel_wait_for_vblank(dev); */

	/*enable the pipe */
	REG_WRITE(pipeconf_reg, pipeconf_val);

	/* set up Vendor Specific InfoFrame for 3D format. */
	if (pipe == 1)
		mrfld_disable_s3d_InfoFrame(dev);

	return 0;
}

/**
 * Perform display 3D mode set to pixel alternative 3D display.
 *
 * FIXME: Assume the 3D buffer is the same as display resolution. Will re-visit
 * it for panel fitting mode.
 * Set up the pll, two times 2D clock.
 */
int mrfld_s3d_to_pixel_interleaving_full(struct drm_device *dev, int pipe, struct
					 mrfld_s3d_flip *ps3d_flip)
{
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;
	struct mdfld_dsi_config *dsi_config = dev_priv->dsi_configs[0];

	/* register */
	u32 pipeconf_reg = PIPEACONF;
	u32 pipesrc_reg = PIPEASRC;
	u32 htot_reg = HTOTAL_A;
	u32 hblank_reg = HBLANK_A;
	u32 hsync_reg = HSYNC_A;

	u32 dspcntr_reg = DSPACNTR;
	u32 dspstride_reg = DSPASTRIDE;
	u32 dsplinoff_reg = DSPALINOFF;
	u32 dspsize_reg = DSPASIZE;
	u32 dspsurf_reg = DSPASURF;
	u32 mipi_reg = MIPI;

	u32 dspcntr_reg_d = DSPDCNTR;
	u32 dspstride_reg_d = DSPDSTRIDE;
	u32 dsplinoff_reg_d = DSPDLINOFF;
	u32 dspsize_reg_d = DSPDSIZE;
	u32 dspsurf_reg_d = DSPDSURF;

	/* values */
	u32 pipeconf_val = 0;
	u32 pipesrc_val = 0;
	u32 htot_val = 0;
	u32 hblank_val = 0;
	u32 hsync_val = 0;

	u32 dspcntr_val = 0;
	u32 dspsize_val = 0;
	u32 mipi_val = 0;
	u32 temp = 0;
	u32 temp1 = 0;

	PSB_DEBUG_ENTRY("pipe = 0x%x \n", pipe);

	switch (pipe) {
	case 0:
		break;
	case 1:
		DRM_ERROR("HDMI doesn't support pixel interleave full. \n");
		return 0;
	case 2:
		dsi_config = dev_priv->dsi_configs[1];
		pipeconf_reg = PIPECCONF;
		pipesrc_reg = PIPECSRC;
		htot_reg = HTOTAL_C;
		hblank_reg = HBLANK_C;
		hsync_reg = HSYNC_C;
		dspcntr_reg = DSPCCNTR;
		dspstride_reg = DSPCSTRIDE;
		dsplinoff_reg = DSPCLINOFF;
		dspsize_reg = DSPCSIZE;
		dspsurf_reg = DSPCSURF;
		mipi_reg = MIPI_C;
		break;
	default:
		DRM_ERROR("Illegal Pipe Number. \n");
		return 0;
	}

	/* Save the related DC registers. */
	pipeconf_val = REG_READ(pipeconf_reg);
	pipesrc_val = REG_READ(pipesrc_reg);
	htot_val = REG_READ(htot_reg);
	hblank_val = REG_READ(hblank_reg);
	hsync_val = REG_READ(hsync_reg);

	dspcntr_val = REG_READ(dspcntr_reg);
	dspsize_val = REG_READ(dspsize_reg);
	mipi_val = REG_READ(mipi_reg);

	/* Disable pipe and port. */
	mrfld_disable_crtc(dev, pipe, true);

	/* Set up the pll, two times 2D clock. */
	/* mrfld_setup_pll (dev, pipe, adjusted_mode->clock); */

	/* Set up full pixel interleaving display 3D format. */

	/* set up pipe related registers */
	/* Get the 3D pipe source. */
	REG_WRITE(pipesrc_reg, (pipesrc_val & 0x0000ffff) |
		  (((pipesrc_val & 0xFFFF0000) + 0x00010000) * 2 - 0x00010000));

	/* Get the 3D Hactive and Htotal. */
	temp = ((htot_val & 0x0000FFFF) + 1) * 2 - 1;
	temp1 = ((htot_val & 0xFFFF0000) + 0x00010000) * 2 - 0x00010000;
	REG_WRITE(htot_reg, temp1 | temp);

	/* Get the 3D Hblank. */
	temp = ((hblank_val & 0x0000FFFF) + 1) * 2 - 1;
	temp1 = ((hblank_val & 0xFFFF0000) + 0x00010000) * 2 - 0x00010000;
	REG_WRITE(hblank_reg, temp1 | temp);

	/* Get the 3D Hsync */
	temp = ((hsync_val & 0x0000FFFF) + 1) * 2 - 1;
	temp1 = ((hsync_val & 0xFFFF0000) + 0x00010000) * 2 - 0x00010000;
	REG_WRITE(hsync_reg, temp1 | temp);

	/* set up plane related registers */
	REG_WRITE(dspsize_reg_d, dspsize_val);

	/* set up the frame buffer stride, offset and start. */
	REG_WRITE(dspstride_reg, ps3d_flip->pitch_l);
	REG_WRITE(dspstride_reg_d, ps3d_flip->pitch_r);
	REG_WRITE(dsplinoff_reg, 0);
	REG_WRITE(dsplinoff_reg_d, 0);
	REG_WRITE(dspsurf_reg, ps3d_flip->uiAddr_l);
	REG_WRITE(dspsurf_reg_d, ps3d_flip->uiAddr_r);

	/* Try to attach/de-attach Plane B to an existing swap chain,
	 * especially with another frame buffer inserted into GTT. */
	/* eError = MRSTLFBChangeSwapChainProperty(&Start, pipe); */

	if ((pipe == 0) || (pipe == 2)) {
		/*set up mipi port related registers */
		REG_WRITE(mipi_reg, mipi_val);

		/*setup MIPI adapter + MIPI IP registers */
		/* mdfld_dsi_controller_init(dsi_config, pipe); */
		mdelay(20);	/* msleep(20); */

		/* re-init the panel */
		dsi_config->drv_ic_inited = 0;
		/* mdfld_dsi_tmd_drv_ic_init(dsi_config, pipe); */
	}

	/*enable the plane */
	dspcntr_val &= ~(S3D_SPRITE_ORDER_BITS | S3D_SPRITE_INTERLEAVING_BITS);
	REG_WRITE(dspcntr_reg_d, dspcntr_val);
	dspcntr_val |= S3D_SPRITE_ORDER_A_FIRST | S3D_SPRITE_INTERLEAVING_PIXEL;
	REG_WRITE(dspcntr_reg, dspcntr_val);
	mdelay(20);		/* msleep(20); */
	/* psb_intel_wait_for_vblank(dev); */

	/*enable the pipe */
	REG_WRITE(pipeconf_reg, pipeconf_val);

	return 0;
}

/**
 * Perform display 2D mode set from pixel alternative 3D display.
 *
 */
int mrfld_s3d_from_pixel_interleaving_full(struct drm_device *dev, int pipe, struct
					   mrfld_s3d_flip *ps3d_flip)
{
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;
	struct mdfld_dsi_config *dsi_config = dev_priv->dsi_configs[0];

	/* register */
	u32 pipeconf_reg = PIPEACONF;
	u32 pipesrc_reg = PIPEASRC;
	u32 htot_reg = HTOTAL_A;
	u32 hblank_reg = HBLANK_A;
	u32 hsync_reg = HSYNC_A;

	u32 dspcntr_reg = DSPACNTR;
	u32 dspstride_reg = DSPASTRIDE;
	u32 dsplinoff_reg = DSPALINOFF;
	u32 dspsize_reg = DSPASIZE;
	u32 dspsurf_reg = DSPASURF;
	u32 mipi_reg = MIPI;

	/* values */
	u32 pipeconf_val = 0;
	u32 pipesrc_val = 0;
	u32 htot_val = 0;
	u32 hblank_val = 0;
	u32 hsync_val = 0;

	u32 dspcntr_val = 0;
	u32 dspsize_val = 0;
	u32 mipi_val = 0;
	u32 temp = 0;
	u32 temp1 = 0;

	PSB_DEBUG_ENTRY("pipe = 0x%x \n", pipe);

	switch (pipe) {
	case 0:
		break;
	case 1:
		DRM_ERROR("HDMI doesn't support pixel interleave full. \n");
		return 0;
	case 2:
		dsi_config = dev_priv->dsi_configs[1];
		pipeconf_reg = PIPECCONF;
		pipesrc_reg = PIPECSRC;
		htot_reg = HTOTAL_C;
		hblank_reg = HBLANK_C;
		hsync_reg = HSYNC_C;
		dspcntr_reg = DSPCCNTR;
		dspstride_reg = DSPCSTRIDE;
		dsplinoff_reg = DSPCLINOFF;
		dspsize_reg = DSPCSIZE;
		dspsurf_reg = DSPCSURF;
		mipi_reg = MIPI_C;
		break;
	default:
		DRM_ERROR("Illegal Pipe Number. \n");
		return 0;
	}

	/* Save the related DC registers. */
	pipeconf_val = REG_READ(pipeconf_reg);
	pipesrc_val = REG_READ(pipesrc_reg);
	htot_val = REG_READ(htot_reg);
	hblank_val = REG_READ(hblank_reg);
	hsync_val = REG_READ(hsync_reg);

	dspcntr_val = REG_READ(dspcntr_reg);
	dspsize_val = REG_READ(dspsize_reg);
	mipi_val = REG_READ(mipi_reg);

	/* Disable pipe and port. */
	mrfld_disable_crtc(dev, pipe, true);

	/* Set up the pll, half of 3D clock. */
	/* mrfld_setup_pll (dev, pipe, adjusted_mode->clock); */

	/* set up pipe related registers */
	/* Get the 2D pipe source. */
	REG_WRITE(pipesrc_reg, (pipesrc_val & 0x0000ffff) |
		  (((pipesrc_val & 0xFFFF0000) + 0x00010000) / 2 - 0x00010000));

	/* Get the 2D Hactive and Htotal. */
	temp = ((htot_val & 0x0000FFFF) + 1) / 2 - 1;
	temp1 = ((htot_val & 0xFFFF0000) + 0x00010000) / 2 - 0x00010000;
	REG_WRITE(htot_reg, temp1 | temp);

	/* Get the 2D Hblank. */
	temp = ((hblank_val & 0x0000FFFF) + 1) / 2 - 1;
	temp1 = ((hblank_val & 0xFFFF0000) + 0x00010000) / 2 - 0x00010000;
	REG_WRITE(hblank_reg, temp1 | temp);

	/* Get the 2D Hsync */
	temp = ((hsync_val & 0x0000FFFF) + 1) / 2 - 1;
	temp1 = ((hsync_val & 0xFFFF0000) + 0x00010000) / 2 - 0x00010000;
	REG_WRITE(hsync_reg, temp1 | temp);

	/* set up plane related registers */
	/* set up the frame buffer stride, offset and start. */
	REG_WRITE(dspstride_reg, ps3d_flip->pitch_l);
	REG_WRITE(dsplinoff_reg, 0);
	REG_WRITE(dspsurf_reg, ps3d_flip->uiAddr_l);

	/* Try to attach/de-attach Plane B to an existing swap chain,
	 * especially with another frame buffer inserted into GTT. */
	/* eError = MRSTLFBChangeSwapChainProperty(&Start, pipe); */

	if ((pipe == 0) || (pipe == 2)) {
		/*set up mipi port related registers */
		REG_WRITE(mipi_reg, mipi_val);

		/*setup MIPI adapter + MIPI IP registers */
		/* mdfld_dsi_controller_init(dsi_config, pipe); */
		mdelay(20);	/* msleep(20); */

		/* re-init the panel */
		dsi_config->drv_ic_inited = 0;
		/* mdfld_dsi_tmd_drv_ic_init(dsi_config, pipe); */
	}

	/*enable the plane */
	dspcntr_val &= ~(S3D_SPRITE_ORDER_BITS | S3D_SPRITE_INTERLEAVING_BITS);
	REG_WRITE(dspcntr_reg, dspcntr_val);
	mdelay(20);		/* msleep(20); */
	/* psb_intel_wait_for_vblank(dev); */

	/*enable the pipe */
	REG_WRITE(pipeconf_reg, pipeconf_val);

	return 0;
}

/**
 * Perform display 3D mode set to pixel alternative 3D display with two
 * half-width L & R frame buffers.
 *
 */
int mrfld_s3d_to_pixel_interleaving_half(struct drm_device *dev, int pipe, struct
					 mrfld_s3d_flip *ps3d_flip)
{
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;
	struct mdfld_dsi_config *dsi_config = dev_priv->dsi_configs[0];

	/* register */
	u32 pipeconf_reg = PIPEACONF;
	u32 dspcntr_reg = DSPACNTR;
	u32 dspstride_reg = DSPASTRIDE;
	u32 dsplinoff_reg = DSPALINOFF;
	u32 dspsize_reg = DSPASIZE;
	u32 dspsurf_reg = DSPASURF;
	u32 mipi_reg = MIPI;

	u32 dspcntr_reg_d = DSPDCNTR;
	u32 dspstride_reg_d = DSPDSTRIDE;
	u32 dsplinoff_reg_d = DSPDLINOFF;
	u32 dspsize_reg_d = DSPDSIZE;
	u32 dspsurf_reg_d = DSPDSURF;

	/* values */
	u32 pipeconf_val = 0;
	u32 dspcntr_val = 0;
	u32 dspsize_val = 0;
	u32 mipi_val = 0;

	PSB_DEBUG_ENTRY("pipe = 0x%x \n", pipe);

	switch (pipe) {
	case 0:
		break;
	case 1:
		DRM_ERROR("HDMI doesn't support pixel interleave half. \n");
		return 0;
	case 2:
		dsi_config = dev_priv->dsi_configs[1];
		pipeconf_reg = PIPECCONF;
		dspcntr_reg = DSPCCNTR;
		dspstride_reg = DSPCSTRIDE;
		dsplinoff_reg = DSPCLINOFF;
		dspsize_reg = DSPCSIZE;
		dspsurf_reg = DSPCSURF;
		mipi_reg = MIPI_C;
		break;
	default:
		DRM_ERROR("Illegal Pipe Number. \n");
		return 0;
	}

	/* Save the related DC registers. */
	pipeconf_val = REG_READ(pipeconf_reg);
	dspcntr_val = REG_READ(dspcntr_reg);
	dspsize_val = REG_READ(dspsize_reg);
	mipi_val = REG_READ(mipi_reg);

	/* Disable pipe and port, don't disable the PLL. */
	mrfld_disable_crtc(dev, pipe, true);

	/* Set up half pixel interleaving display 3D format. */

	/* set up plane related registers */
	dspsize_val = (dspsize_val & 0xFFFF0000) | (((dspsize_val & 0x0000FFFF)
						     + 1) / 2 - 1);
	REG_WRITE(dspsize_reg, dspsize_val);
	REG_WRITE(dspsize_reg_d, dspsize_val);

	/* set up the frame buffer stride, offset and start. */
	REG_WRITE(dspstride_reg, ps3d_flip->pitch_l);
	REG_WRITE(dspstride_reg_d, ps3d_flip->pitch_r);
	REG_WRITE(dsplinoff_reg, 0);
	REG_WRITE(dsplinoff_reg_d, 0);
	REG_WRITE(dspsurf_reg, ps3d_flip->uiAddr_l);
	REG_WRITE(dspsurf_reg_d, ps3d_flip->uiAddr_r);

	/* Try to attach/de-attach Plane B to an existing swap chain,
	 * especially with another frame buffer inserted into GTT. */
	/* eError = MRSTLFBChangeSwapChainProperty(&Start, pipe); */

	if ((pipe == 0) || (pipe == 2)) {
		/*set up mipi port related registers */
		REG_WRITE(mipi_reg, mipi_val);

		/*setup MIPI adapter + MIPI IP registers */
		/* mdfld_dsi_controller_init(dsi_config, pipe); */
		mdelay(20);	/* msleep(20); */

		/* re-init the panel */
		dsi_config->drv_ic_inited = 0;
		/* mdfld_dsi_tmd_drv_ic_init(dsi_config, pipe); */
	}

	/*enable the plane */
	dspcntr_val &= ~(S3D_SPRITE_ORDER_BITS | S3D_SPRITE_INTERLEAVING_BITS);
	REG_WRITE(dspcntr_reg_d, dspcntr_val);
	dspcntr_val |= S3D_SPRITE_ORDER_A_FIRST | S3D_SPRITE_INTERLEAVING_PIXEL;
	REG_WRITE(dspcntr_reg, dspcntr_val);
	mdelay(20);		/* msleep(20); */
	/* psb_intel_wait_for_vblank(dev); */

	/*enable the pipe */
	REG_WRITE(pipeconf_reg, pipeconf_val);

	return 0;
}

/**
 * Perform display 2D mode set from pixel alternative 3D display with two
 * half-width L & R frame buffers.
 *
 */
int mrfld_s3d_from_pixel_interleaving_half(struct drm_device *dev, int pipe, struct
					   mrfld_s3d_flip *ps3d_flip)
{
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;
	struct mdfld_dsi_config *dsi_config = dev_priv->dsi_configs[0];

	/* register */
	u32 pipeconf_reg = PIPEACONF;
	u32 dspcntr_reg = DSPACNTR;
	u32 dspstride_reg = DSPASTRIDE;
	u32 dsplinoff_reg = DSPALINOFF;
	u32 dspsize_reg = DSPASIZE;
	u32 dspsurf_reg = DSPASURF;
	u32 mipi_reg = MIPI;

	/* values */
	u32 pipeconf_val = 0;
	u32 dspcntr_val = 0;
	u32 dspsize_val = 0;
	u32 mipi_val = 0;

	PSB_DEBUG_ENTRY("pipe = 0x%x \n", pipe);

	switch (pipe) {
	case 0:
		break;
	case 1:
		DRM_ERROR("HDMI doesn't support pixel interleave half. \n");
		return 0;
	case 2:
		dsi_config = dev_priv->dsi_configs[1];
		pipeconf_reg = PIPECCONF;
		dspcntr_reg = DSPCCNTR;
		dspstride_reg = DSPCSTRIDE;
		dsplinoff_reg = DSPCLINOFF;
		dspsize_reg = DSPCSIZE;
		dspsurf_reg = DSPCSURF;
		mipi_reg = MIPI_C;
		break;
	default:
		DRM_ERROR("Illegal Pipe Number. \n");
		return 0;
	}

	/* Save the related DC registers. */
	pipeconf_val = REG_READ(pipeconf_reg);
	dspcntr_val = REG_READ(dspcntr_reg);
	dspsize_val = REG_READ(dspsize_reg);
	mipi_val = REG_READ(mipi_reg);

	/* Disable pipe and port, don't disable the PLL. */
	mrfld_disable_crtc(dev, pipe, true);

	/* set up plane related registers */
	dspsize_val = (dspsize_val & 0xFFFF0000) | (((dspsize_val & 0x0000FFFF)
						     + 1) * 2 - 1);
	REG_WRITE(dspsize_reg, dspsize_val);
	/* set up the frame buffer stride, offset and start. */
	REG_WRITE(dspstride_reg, ps3d_flip->pitch_l);
	REG_WRITE(dsplinoff_reg, 0);
	REG_WRITE(dspsurf_reg, ps3d_flip->uiAddr_l);

	/* Try to attach/de-attach Plane B to an existing swap chain,
	 * especially with another frame buffer inserted into GTT. */
	/* eError = MRSTLFBChangeSwapChainProperty(&Start, pipe); */

	if ((pipe == 0) || (pipe == 2)) {
		/*set up mipi port related registers */
		REG_WRITE(mipi_reg, mipi_val);

		/*setup MIPI adapter + MIPI IP registers */
		/* mdfld_dsi_controller_init(dsi_config, pipe); */
		mdelay(20);	/* msleep(20); */

		/* re-init the panel */
		dsi_config->drv_ic_inited = 0;
		/* mdfld_dsi_tmd_drv_ic_init(dsi_config, pipe); */
	}

	/*enable the plane */
	dspcntr_val &= ~(S3D_SPRITE_ORDER_BITS | S3D_SPRITE_INTERLEAVING_BITS);
	REG_WRITE(dspcntr_reg, dspcntr_val);
	mdelay(20);		/* msleep(20); */
	/* psb_intel_wait_for_vblank(dev); */

	/*enable the pipe */
	REG_WRITE(pipeconf_reg, pipeconf_val);

	return 0;
}

/**
 * Check if the DSI display supports S3D. If so, report supported S3D formats
 *
 */
int mrfld_dsi_s3d_query(struct drm_device *dev, struct drm_psb_s3d_query
			*s3d_query)
{
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;
	struct mdfld_dsi_config *dsi_config = dev_priv->dsi_configs[0];
	uint32_t s3d_display_type = s3d_query->s3d_display_type;

	switch (s3d_display_type) {
	case S3D_MIPIA_DISPLAY:
		break;
	case S3D_MIPIC_DISPLAY:
		dsi_config = dev_priv->dsi_configs[1];
		break;
	default:
		DRM_ERROR("invalid parameters. \n");
		return -EINVAL;
	}

	if (dsi_config->s3d_format) {
		s3d_query->is_s3d_supported = 1;
		s3d_query->s3d_format = dsi_config->s3d_format;
	}

	return 0;
}

/**
 * Check if the display supports S3D. If so, report supported S3D formats.
 *
 */
int mrfld_s3d_query(struct drm_device *dev, struct drm_psb_s3d_query
		    *s3d_query)
{
	uint32_t s3d_display_type = s3d_query->s3d_display_type;

	switch (s3d_display_type) {
	case S3D_MIPIA_DISPLAY:
	case S3D_MIPIC_DISPLAY:
		return mrfld_dsi_s3d_query(dev, s3d_query);
	case S3D_HDMI_DISPLAY:
		return mrfld_hdmi_s3d_query(dev, s3d_query);
	default:
		DRM_ERROR("invalid parameters. \n");
		return -EINVAL;
	}
}

#endif /* if KEEP_UNUSED_CODE_S3D */

#if 0

/**
 * Perform display 3D mode set.
 *
 */
static int mrfld_s3d_crtc_mode_set(struct drm_crtc *crtc,
				   struct drm_display_mode *mode,
				   struct drm_display_mode *adjusted_mode,
				   int x, int y, struct drm_framebuffer *old_fb)
{
	struct drm_device *dev = crtc->dev;
	struct psb_intel_crtc *psb_intel_crtc = to_psb_intel_crtc(crtc);
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;
	int pipe = psb_intel_crtc->pipe;
	u32 dspcntr_reg = DSPACNTR;
	u32 pipeconf_reg = PIPEACONF;
	u32 htot_reg = HTOTAL_A;
	u32 hblank_reg = HBLANK_A;
	u32 hsync_reg = HSYNC_A;
	u32 vtot_reg = VTOTAL_A;
	u32 vblank_reg = VBLANK_A;
	u32 vsync_reg = VSYNC_A;
	u32 dspsize_reg = DSPASIZE;
	u32 dsppos_reg = DSPAPOS;
	u32 pipesrc_reg = PIPEASRC;
	u32 *pipeconf = &dev_priv->pipeconf;
	u32 *dspcntr = &dev_priv->dspcntr;
	struct drm_mode_config *mode_config = &dev->mode_config;
	struct psb_intel_output *psb_intel_output = NULL;
	uint64_t scalingType = DRM_MODE_SCALE_FULLSCREEN;
	struct drm_encoder *encoder;
	struct drm_connector *connector;

	PSB_DEBUG_ENTRY("pipe = 0x%x \n", pipe);

	switch (pipe) {
	case 0:
		break;
	case 1:
		dspcntr_reg = DSPBCNTR;
		pipeconf_reg = PIPEBCONF;
		htot_reg = HTOTAL_B;
		hblank_reg = HBLANK_B;
		hsync_reg = HSYNC_B;
		vtot_reg = VTOTAL_B;
		vblank_reg = VBLANK_B;
		vsync_reg = VSYNC_B;
		dspsize_reg = DSPBSIZE;
		dsppos_reg = DSPBPOS;
		pipesrc_reg = PIPEBSRC;
		pipeconf = &dev_priv->pipeconf1;
		dspcntr = &dev_priv->dspcntr1;
		break;
	case 2:
		dspcntr_reg = DSPCCNTR;
		pipeconf_reg = PIPECCONF;
		htot_reg = HTOTAL_C;
		hblank_reg = HBLANK_C;
		hsync_reg = HSYNC_C;
		vtot_reg = VTOTAL_C;
		vblank_reg = VBLANK_C;
		vsync_reg = VSYNC_C;
		dspsize_reg = DSPCSIZE;
		dsppos_reg = DSPCPOS;
		pipesrc_reg = PIPECSRC;
		pipeconf = &dev_priv->pipeconf2;
		dspcntr = &dev_priv->dspcntr2;
		break;
	default:
		DRM_ERROR("Illegal Pipe Number. \n");
		return 0;
	}

	memcpy(&psb_intel_crtc->saved_mode, mode,
	       sizeof(struct drm_display_mode));
	memcpy(&psb_intel_crtc->saved_adjusted_mode, adjusted_mode,
	       sizeof(struct drm_display_mode));

	list_for_each_entry(connector, &mode_config->connector_list, head) {
		if (!connector)
			continue;

		encoder = connector->encoder;

		if (!encoder)
			continue;

		if (encoder->crtc != crtc)
			continue;

		psb_intel_output = to_psb_intel_output(connector);

		PSB_DEBUG_ENTRY("output->type = 0x%x \n",
				psb_intel_output->type);

	}

	/* Disable the panel fitter if it was on our pipe */
	if (psb_intel_panel_fitter_pipe(dev) == pipe)
		REG_WRITE(PFIT_CONTROL, 0);

	/* pipesrc and dspsize control the size that is scaled from,
	 * which should always be the user's requested size.
	 */
	if (pipe == 1) {
		/* FIXME: To make HDMI display with 864x480 (TPO), 480x864 (PYR) or 480x854 (TMD), set the sprite
		 * width/height and souce image size registers with the adjusted mode for pipe B. */

		/* The defined sprite rectangle must always be completely contained within the displayable
		 * area of the screen image (frame buffer). */
		REG_WRITE(dspsize_reg,
			  ((MIN
			    (mode->crtc_vdisplay,
			     adjusted_mode->crtc_vdisplay) - 1) << 16)
			  |
			  (MIN
			   (mode->crtc_hdisplay,
			    adjusted_mode->crtc_hdisplay) - 1));
		/* Set the CRTC with encoder mode. */
		REG_WRITE(pipesrc_reg, ((mode->crtc_hdisplay - 1) << 16)
			  | (mode->crtc_vdisplay - 1));
	} else {
		REG_WRITE(dspsize_reg,
			  ((mode->crtc_vdisplay -
			    1) << 16) | (mode->crtc_hdisplay - 1));
		REG_WRITE(pipesrc_reg,
			  ((mode->crtc_hdisplay -
			    1) << 16) | (mode->crtc_vdisplay - 1));
	}

	REG_WRITE(dsppos_reg, 0);

	if (psb_intel_output)
		drm_object_property_get_value(&psb_intel_output->base->base,
						 dev->
						 mode_config.scaling_mode_property,
						 &scalingType);

	if (scalingType == DRM_MODE_SCALE_NO_SCALE) {
		/*Moorestown doesn't have register support for centering so we need to
		   mess with the h/vblank and h/vsync start and ends to get centering */
		int offsetX = 0, offsetY = 0;

		offsetX =
		    (adjusted_mode->crtc_hdisplay - mode->crtc_hdisplay) / 2;
		offsetY =
		    (adjusted_mode->crtc_vdisplay - mode->crtc_vdisplay) / 2;

		REG_WRITE(htot_reg, (mode->crtc_hdisplay - 1) |
			  ((adjusted_mode->crtc_htotal - 1) << 16));
		REG_WRITE(vtot_reg, (mode->crtc_vdisplay - 1) |
			  ((adjusted_mode->crtc_vtotal - 1) << 16));
		REG_WRITE(hblank_reg,
			  (adjusted_mode->crtc_hblank_start - offsetX -
			   1) | ((adjusted_mode->crtc_hblank_end - offsetX -
				  1) << 16));
		REG_WRITE(hsync_reg,
			  (adjusted_mode->crtc_hsync_start - offsetX -
			   1) | ((adjusted_mode->crtc_hsync_end - offsetX -
				  1) << 16));
		REG_WRITE(vblank_reg,
			  (adjusted_mode->crtc_vblank_start - offsetY -
			   1) | ((adjusted_mode->crtc_vblank_end - offsetY -
				  1) << 16));
		REG_WRITE(vsync_reg,
			  (adjusted_mode->crtc_vsync_start - offsetY -
			   1) | ((adjusted_mode->crtc_vsync_end - offsetY -
				  1) << 16));
	} else {
		REG_WRITE(htot_reg, (adjusted_mode->crtc_hdisplay - 1) |
			  ((adjusted_mode->crtc_htotal - 1) << 16));
		REG_WRITE(vtot_reg, (adjusted_mode->crtc_vdisplay - 1) |
			  ((adjusted_mode->crtc_vtotal - 1) << 16));
		REG_WRITE(hblank_reg, (adjusted_mode->crtc_hblank_start - 1) |
			  ((adjusted_mode->crtc_hblank_end - 1) << 16));
		REG_WRITE(hsync_reg, (adjusted_mode->crtc_hsync_start - 1) |
			  ((adjusted_mode->crtc_hsync_end - 1) << 16));
		REG_WRITE(vblank_reg, (adjusted_mode->crtc_vblank_start - 1) |
			  ((adjusted_mode->crtc_vblank_end - 1) << 16));
		REG_WRITE(vsync_reg, (adjusted_mode->crtc_vsync_start - 1) |
			  ((adjusted_mode->crtc_vsync_end - 1) << 16));
	}

	/* Flush the plane changes */
	{
		struct drm_crtc_helper_funcs *crtc_funcs = crtc->helper_private;
		crtc_funcs->mode_set_base(crtc, x, y, old_fb);
	}

	/* setup pipeconf */
	*pipeconf = PIPEACONF_ENABLE;

	/* Set up the display plane register */
	*dspcntr = REG_READ(dspcntr_reg);
	*dspcntr |= pipe << DISPPLANE_SEL_PIPE_POS;
	*dspcntr |= DISPLAY_PLANE_ENABLE;
	mrfld_setup_pll(dev, pipe, adjusted_mode->clock);

	if (pipe != 1)
		goto mrst_crtc_mode_set_exit;

	REG_WRITE(pipeconf_reg, *pipeconf);
	REG_READ(pipeconf_reg);

	REG_WRITE(dspcntr_reg, *dspcntr);
	psb_intel_wait_for_vblank(dev);

 mrst_crtc_mode_set_exit:

	return 0;
}

/**
 * Perform display 3D mode set from half Top-and-Bottom to half Top-and-Bottom 3D display.
 *
 */
static int mrfld_s3d_half_top_and_bottom(struct drm_device *dev, int pipe)
{
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;

	/* regester */
	u32 dspsurf_reg = DSPASURF;

	/* values */
	u32 dspsurf_val = 0;

	PSB_DEBUG_ENTRY("pipe = 0x%x\n", pipe);

	dspsurf_val = REG_READ(dspsurf_reg);

	/* need to figure out the start. */
	REG_WRITE(dspsurf_reg, dspsurf_val);

	/* Try to attach/de-attach Plane B to an existing swap chain,
	 * especially with another frame buffer inserted into GTT. */
	/* eError = MRSTLFBChangeSwapChainProperty(&Start, pipe); */

	/* set up Vendor Specific InfoFrame for 3D format. */
	if (pipe == 1)
		mrfld_set_up_s3d_InfoFrame(dev, S3D_TOP_AND_BOTTOM);

	return 0;
}

/**
 * Perform display 3D mode set from two full source inputs to line
 * interleaving 3D display.
 *
 */
static int mrfld_s3d_line_interleaving(struct drm_device *dev, int pipe)
{
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;

	/* regester */
	u32 pipeconf_reg = PIPEBCONF;
	u32 pipesrc_reg = PIPEBSRC;
	u32 vtot_reg = VTOTAL_B;
	u32 vblank_reg = VBLANK_B;
	u32 vsync_reg = VSYNC_B;

	u32 dspcntr_reg = DSPBCNTR;
	u32 dspstride_reg = DSPBSTRIDE;
	u32 dsplinoff_reg = DSPBLINOFF;
	u32 dspsize_reg = DSPBSIZE;
	u32 dspsurf_reg = DSPBSURF;

	u32 dspcntr_reg_d = DSPDCNTR;
	u32 dspstride_reg_d = DSPDSTRIDE;
	u32 dsplinoff_reg_d = DSPDLINOFF;
	u32 dspsize_reg_d = DSPDSIZE;
	u32 dspsurf_reg_d = DSPDSURF;

	/* values */
	u32 pipeconf_val = 0;
	u32 pipesrc_val = 0;
	u32 vtot_val = 0;
	u32 vblank_val = 0;
	u32 vsync_val = 0;

	u32 dspcntr_val = 0;
	u32 dspstride_val = 0;
	u32 dsplinoff_val = 0;
	u32 dspsize_val = 0;
	u32 dspsurf_val = 0;

	PSB_DEBUG_ENTRY("pipe = 0x%x\n", pipe);

	/* Save the related DC registers. */
	pipeconf_val = REG_READ(pipeconf_reg);
	pipesrc_val = REG_READ(pipesrc_reg);
	vtot_val = REG_READ(vtot_reg);
	vblank_val = REG_READ(vblank_reg);
	vsync_val = REG_READ(vsync_reg);

	dspcntr_val = REG_READ(dspcntr_reg);
	dspstride_val = REG_READ(dspstride_reg);
	dsplinoff_val = REG_READ(dsplinoff_reg);
	dspsize_val = REG_READ(dspsize_reg);
	dspsurf_val = REG_READ(dspsurf_reg);

	/* Disable pipe and port. */
	mrfld_disable_crtc(dev, pipe, true);

	/* Set up the pll, two times 2D clock. */
	/* mrfld_setup_pll (dev, pipe, adjusted_mode->clock); */

	/* Set up from either full Top-and-Bottom or side-by-side to
	 * full side-by-side display 3D format.
	 */

	/* set up pipe related registers */
	REG_WRITE(pipesrc_reg, (((pipesrc_val & 0x0000FFFF) + 1) * 2 - 1) |
		  (pipesrc_val & 0xFFFF0000));
	REG_WRITE(vtot_reg, (vtot_val + 0x00010001) * 2 - 0x00010001);
	REG_WRITE(vblank_reg, (vblank_val + 0x00010001) * 2 - 0x00010001);
	REG_WRITE(vsync_reg, (vsync_val + 0x00010001) * 2 - 0x00010001);

	/* set up plane related registers */
	REG_WRITE(dspsize_reg_d, dspsize_val);

	/* need to figure out the stride, offset and start. */
	REG_WRITE(dspstride_reg, dspstride_val * 2);
	REG_WRITE(dspstride_reg_d, dspstride_val * 2);
	REG_WRITE(dsplinoff_reg, dsplinoff_val);
	REG_WRITE(dsplinoff_reg_d, dsplinoff_val + dspstride_val);
	REG_WRITE(dspsurf_reg, dspsurf_val);
	REG_WRITE(dspsurf_reg_d, dspsurf_val);

	/* Try to attach/de-attach Plane B to an existing swap chain,
	 * especially with another frame buffer inserted into GTT. */
	/* eError = MRSTLFBChangeSwapChainProperty(&Start, pipe); */

	/* send 3D info frame. */

	/*enable the plane */
	REG_WRITE(dspcntr_reg_d, dspcntr_val);
	dspcntr_val &= ~(S3D_SPRITE_ORDER_BITS | S3D_SPRITE_INTERLEAVING_BITS);
	dspcntr_val |= S3D_SPRITE_ORDER_A_FIRST | S3D_SPRITE_INTERLEAVING_LINE;
	REG_WRITE(dspcntr_reg, dspcntr_val);
	mdelay(20);		/* msleep(20); */
	/* psb_intel_wait_for_vblank(dev); */

	/*enable the pipe */
	REG_WRITE(pipeconf_reg, pipeconf_val);

	/* set up Vendor Specific InfoFrame for 3D format. */
	if (pipe == 1)
		mrfld_set_up_s3d_InfoFrame(dev, S3D_LINE_ALTERNATIVE);

	return 0;
}

/**
 * Perform display 3D mode set from full side-by-side to side-by-side 3D display.
 *
 */
static int mrfld_s3d_side_by_side(struct drm_device *dev, int pipe)
{
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;

	/* regester */
	u32 pipeconf_reg = PIPEBCONF;
	u32 pipesrc_reg = PIPEBSRC;
	u32 htot_reg = HTOTAL_B;
	u32 hblank_reg = HBLANK_B;
	u32 hsync_reg = HSYNC_B;

	u32 dspcntr_reg = DSPBCNTR;
	u32 dspstride_reg = DSPBSTRIDE;
	u32 dsplinoff_reg = DSPBLINOFF;
	u32 dspsize_reg = DSPBSIZE;
	u32 dspsurf_reg = DSPBSURF;

	u32 dspcntr_reg_d = DSPDCNTR;
	u32 dspstride_reg_d = DSPDSTRIDE;
	u32 dsplinoff_reg_d = DSPDLINOFF;
	u32 dspsize_reg_d = DSPDSIZE;
	u32 dspsurf_reg_d = DSPDSURF;

	/* values */
	u32 pipeconf_val = 0;
	u32 pipesrc_val = 0;
	u32 htot_val = 0;
	u32 hblank_val = 0;
	u32 hsync_val = 0;

	u32 dspcntr_val = 0;
	u32 dspstride_val = 0;
	u32 dsplinoff_val = 0;
	u32 dspsize_val = 0;
	u32 dspsurf_val = 0;

	PSB_DEBUG_ENTRY("pipe = 0x%x\n", pipe);

	/* Save the related DC registers. */
	pipeconf_val = REG_READ(pipeconf_reg);
	pipesrc_val = REG_READ(pipesrc_reg);
	htot_val = REG_READ(htot_reg);
	hblank_val = REG_READ(hblank_reg);
	hsync_val = REG_READ(hsync_reg);

	dspcntr_val = REG_READ(dspcntr_reg);
	dspstride_val = REG_READ(dspstride_reg);
	dsplinoff_val = REG_READ(dsplinoff_reg);
	dspsize_val = REG_READ(dspsize_reg);
	dspsurf_val = REG_READ(dspsurf_reg);

	/* Disable pipe and port. */
	mrfld_disable_crtc(dev, pipe, true);

	/* Set up the pll, two times 2D clock. */
	/* mrfld_setup_pll (dev, pipe, adjusted_mode->clock); */

	/* Set up from full side-by-side to full side-by-side display 3D format. */

	/* set up pipe related registers */
	REG_WRITE(pipesrc_reg, (pipesrc_val & 0x0000FFFF) |
		  (((pipesrc_val & 0xFFFF0000) + 0x00010000) * 2 - 0x00010000));
	REG_WRITE(htot_reg, (htot_val + 0x00010001) * 2 - 0x00010001);
	REG_WRITE(hblank_reg, (hblank_val + 0x00010001) * 2 - 0x00010001);
	REG_WRITE(hsync_reg, (hsync_val + 0x00010001) * 2 - 0x00010001);

	/* set up plane related registers */
	REG_WRITE(dspsize_reg_d, dspsize_val);

	/* need to figure out the stride, offset and start. */
	REG_WRITE(dspstride_reg, dspstride_val * 2);
	REG_WRITE(dspstride_reg_d, dspstride_val * 2);
	REG_WRITE(dsplinoff_reg, dsplinoff_val);
	REG_WRITE(dsplinoff_reg_d, dsplinoff_val + dspstride_val);
	REG_WRITE(dspsurf_reg, dspsurf_val);
	REG_WRITE(dspsurf_reg_d, dspsurf_val);

	/* Try to attach/de-attach Plane B to an existing swap chain,
	 * especially with another frame buffer inserted into GTT. */
	/* eError = MRSTLFBChangeSwapChainProperty(&Start, pipe); */

	/* send 3D info frame. */

	/*enable the plane */
	REG_WRITE(dspcntr_reg_d, dspcntr_val);
	dspcntr_val &= ~(S3D_SPRITE_ORDER_BITS | S3D_SPRITE_INTERLEAVING_BITS);
	dspcntr_val |= S3D_SPRITE_ORDER_A_FIRST | S3D_SPRITE_INTERLEAVING_LINE;
	REG_WRITE(dspcntr_reg, dspcntr_val);
	mdelay(20);		/* msleep(20); */
	/* psb_intel_wait_for_vblank(dev); */

	/*enable the pipe */
	REG_WRITE(pipeconf_reg, pipeconf_val);

	/* set up Vendor Specific InfoFrame for 3D format. */
	if (pipe == 1)
		mrfld_set_up_s3d_InfoFrame(dev, S3D_SIDE_BY_SIDE_FULL);

	return 0;
}

/**
 * Perform display 3D mode set from half side-by-side to half side-by-side 3D display.
 *
 */
static int mrfld_s3d_half_side_by_side(struct drm_device *dev, int pipe)
{
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;

	/* regester */
	u32 dspsurf_reg = DSPASURF;

	/* values */
	u32 dspsurf_val = 0;

	PSB_DEBUG_ENTRY("pipe = 0x%x\n", pipe);

	dspsurf_val = REG_READ(dspsurf_reg);

	/* need to figure out the start. */
	REG_WRITE(dspsurf_reg, dspsurf_val);

	/* Try to attach/de-attach Plane B to an existing swap chain,
	 * especially with another frame buffer inserted into GTT. */
	/* eError = MRSTLFBChangeSwapChainProperty(&Start, pipe); */

	/* set up Vendor Specific InfoFrame for 3D format. */
	if (pipe == 1)
		mrfld_set_up_s3d_InfoFrame(dev, S3D_SIDE_BY_SIDE_HALF);

	return 0;
}

/**
 * Perform display 3D mode set from full Top_Bottom to frame packing 3D display.
 *
 */
static int mrfld_s3d_frame_packing(struct drm_device *dev, int pipe)
{
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;

	/* regester */
	u32 pipeconf_reg = PIPEBCONF;
	u32 pipesrc_reg = PIPEBSRC;
	u32 vtot_reg = VTOTAL_B;
	u32 vblank_reg = VBLANK_B;
	u32 vsync_reg = VSYNC_B;

	u32 dspcntr_reg = DSPBCNTR;
	u32 dspstride_reg = DSPBSTRIDE;
	u32 dsplinoff_reg = DSPBLINOFF;
	u32 dspsize_reg = DSPBSIZE;
	u32 dspsurf_reg = DSPBSURF;

	/* values */
	u32 pipeconf_val = 0;
	u32 pipesrc_val = 0;
	u32 vtot_val = 0;
	u32 vblank_val = 0;
	u32 vsync_val = 0;

	u32 dspcntr_val = 0;
	u32 dspstride_val = 0;
	u32 dsplinoff_val = 0;
	u32 dspsize_val = 0;
	u32 dspsurf_val = 0;
	u32 temp = 0;
	u32 temp1 = 0;
	u32 temp2 = 0;
	u32 temp3 = 0;

	PSB_DEBUG_ENTRY("pipe = 0x%x \n", pipe);

	if (pipe != 1) {
		DRM_ERROR("Illegal Pipe Number. \n");
		return 0;
	}

	/* Save the related DC registers. */
	pipeconf_val = REG_READ(pipeconf_reg);
	pipesrc_val = REG_READ(pipesrc_reg);
	vtot_val = REG_READ(vtot_reg);
	vblank_val = REG_READ(vblank_reg);
	vsync_val = REG_READ(vsync_reg);

	dspcntr_val = REG_READ(dspcntr_reg);
	dspstride_val = REG_READ(dspstride_reg);
	dsplinoff_val = REG_READ(dsplinoff_reg);
	dspsize_val = REG_READ(dspsize_reg);
	dspsurf_val = REG_READ(dspsurf_reg);

	/* Disable pipe and port. */
	mrfld_disable_crtc(dev, pipe, true);

	/* Set up the pll, two times 2D clock. */
	/* mrfld_setup_pll (dev, pipe, adjusted_mode->clock); */

	/* Set up frame packing display 3D format. */

	/* set up pipe related registers */
	REG_WRITE(pipesrc_reg, (((pipesrc_val & 0x0000FFFF) + 1) * 2 - 1) |
		  (pipesrc_val & 0xFFFF0000));

	/* Get the 3D Vactive. */
	temp = ((vtot_val & 0xFFFF0000) >> 16) - (vtot_val & 0x0000FFFF);
	temp += ((vtot_val & 0x0000FFFF) + 1) * 2 - 1;

	/* Get the 3D Vtotal. */
	temp1 = ((vtot_val & 0xFFFF0000) + 0x00010000) * 2 - 0x00010000;

	REG_WRITE(vtot_reg, temp1 | temp);

	/* Get the 3D Vblank. */
	temp2 = (vblank_val & 0x0000FFFF) - (vtot_val & 0x0000FFFF);
	temp3 = (vtot_val & 0xFFFF0000) - (vblank_val & 0xFFFF0000);
	REG_WRITE(vblank_reg, (temp1 - temp3) | (temp + temp2));

	/* Get the 3D Vsync */
	temp2 = (vsync_val & 0x0000FFFF) - (vtot_val & 0x0000FFFF);
	temp3 = (vtot_val & 0xFFFF0000) - (vsync_val & 0xFFFF0000);
	REG_WRITE(vblank_reg, (temp1 - temp3) | (temp + temp2));

	/* set up plane related registers */
	REG_WRITE(dspsize_reg, (((dspsize_val & 0xFFFF0000) + 0x00010000) * 2 -
				0x00010000) | (dspsize_val & 0x0000FFFF));

	/* need to figure out the offset and start. */
	REG_WRITE(dsplinoff_reg, dsplinoff_val);
	REG_WRITE(dspsurf_reg, dspsurf_val);

	/* Try to attach/de-attach Plane B to an existing swap chain,
	 * especially with another frame buffer inserted into GTT. */
	/* eError = MRSTLFBChangeSwapChainProperty(&Start, pipe); */

	/* send 3D info frame. */

	/*enable the plane */
	REG_WRITE(dspcntr_reg, dspcntr_val);
	mdelay(20);		/* msleep(20); */
	/* psb_intel_wait_for_vblank(dev); */

	/*enable the pipe */
	REG_WRITE(pipeconf_reg, pipeconf_val);

	/* set up Vendor Specific InfoFrame for 3D format. */
	if (pipe == 1)
		mrfld_set_up_s3d_InfoFrame(dev, S3D_FRAME_PACKING);

	return 0;
}

/**
 * Perform display 3D mode set from half Top_Bottom to line interleaving 3D display.
 *
 */
static int mrfld_s3d_half_top_bottom_to_line_interleave(struct drm_device *dev,
							int pipe)
{
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;
	struct mdfld_dsi_config *dsi_config = dev_priv->dsi_configs[0];

	/* regester */
	u32 pipeconf_reg = PIPEACONF;

	u32 dspcntr_reg = DSPACNTR;
	u32 dspstride_reg = DSPASTRIDE;
	u32 dsplinoff_reg = DSPALINOFF;
	u32 dspsize_reg = DSPASIZE;
	u32 dspsurf_reg = DSPASURF;
	u32 mipi_reg = MIPI;

	u32 dspcntr_reg_d = DSPDCNTR;
	u32 dspstride_reg_d = DSPDSTRIDE;
	u32 dsplinoff_reg_d = DSPDLINOFF;
	u32 dspsize_reg_d = DSPDSIZE;
	u32 dspsurf_reg_d = DSPDSURF;

	/* values */
	u32 pipeconf_val = 0;

	u32 dspcntr_val = 0;
	u32 dspstride_val = 0;
	u32 dsplinoff_val = 0;
	u32 dspsize_val = 0;
	u32 dspsurf_val = 0;
	u32 mipi_val = 0;

	PSB_DEBUG_ENTRY("pipe = 0x%x \n", pipe);

	switch (pipe) {
	case 0:
		break;
	case 2:
		dsi_config = dev_priv->dsi_configs[1];
		pipeconf_reg = PIPECCONF;
		dspcntr_reg = DSPCCNTR;
		dspstride_reg = DSPCSTRIDE;
		dsplinoff_reg = DSPCLINOFF;
		dspsize_reg = DSPCSIZE;
		dspsurf_reg = DSPCSURF;
		mipi_reg = MIPI_C;
		break;
	default:
		DRM_ERROR("Illegal Pipe Number. \n");
		return 0;
	}

	/* Save the related DC registers. */
	pipeconf_val = REG_READ(pipeconf_reg);

	dspcntr_val = REG_READ(dspcntr_reg);
	dspstride_val = REG_READ(dspstride_reg);
	dsplinoff_val = REG_READ(dsplinoff_reg);
	dspsize_val = REG_READ(dspsize_reg);
	dspsurf_val = REG_READ(dspsurf_reg);
	mipi_val = REG_READ(mipi_reg);

	/* Disable pipe and port, don't disable the PLL. */
	/* FIXME modify the following function with option to disable PLL or
	 *  not.
	 *  */
	mrfld_disable_crtc(dev, pipe, true);

	/* Set up line interleaving display 3D format. */
	REG_WRITE(dspsize_reg, (((dspsize_val & 0xFFFF0000) + 0x00010000) / 2 -
				0x00010000) | (dspsize_val & 0x0000FFFF));
	REG_WRITE(dspsize_reg_d,
		  (((dspsize_val & 0xFFFF0000) + 0x00010000) / 2 -
		   0x00010000) | (dspsize_val & 0x0000FFFF));
	REG_WRITE(dspstride_reg_d, dspstride_val);

	/* need to figure out the offset and start. */
	REG_WRITE(dsplinoff_reg, dsplinoff_val);
	REG_WRITE(dsplinoff_reg_d, dsplinoff_val
		  + (((dspsize_val >> 16) + 1) / 2) * dspstride_val);
	REG_WRITE(dspsurf_reg, dspsurf_val);
	REG_WRITE(dspsurf_reg_d, dspsurf_val);

	/* Try to attach/de-attach Plane B to an existing swap chain,
	 * especially with another frame buffer inserted into GTT. */
	/* eError = MRSTLFBChangeSwapChainProperty(&Start, pipe); */

	/*set up mipi port related registers */
	REG_WRITE(mipi_reg, mipi_val);

	/*setup MIPI adapter + MIPI IP registers */
	/* mdfld_dsi_controller_init(dsi_config, pipe); */
	mdelay(20);		/* msleep(20); */

	/* re-init the panel */
	dsi_config->drv_ic_inited = 0;
	/* mdfld_dsi_tmd_drv_ic_init(dsi_config, pipe); */

	/*enable the plane */
	REG_WRITE(dspcntr_reg_d, dspcntr_val);
	dspcntr_val &= ~(S3D_SPRITE_ORDER_BITS | S3D_SPRITE_INTERLEAVING_BITS);
	dspcntr_val |= S3D_SPRITE_ORDER_A_FIRST | S3D_SPRITE_INTERLEAVING_LINE;
	REG_WRITE(dspcntr_reg, dspcntr_val);
	mdelay(20);		/* msleep(20); */
	/* psb_intel_wait_for_vblank(dev); */

	/*enable the pipe */
	REG_WRITE(pipeconf_reg, pipeconf_val);

	/* set up Vendor Specific InfoFrame for 3D format. */
	if (pipe == 1)
		mrfld_set_up_s3d_InfoFrame(dev, S3D_LINE_ALTERNATIVE);

	return 0;
}

/* MDFLD_PLATFORM start */
void mdfldWaitForPipeDisable(struct drm_device *dev, int pipe)
{
	int count, temp;
	u32 pipeconf_reg = PIPEACONF;

	switch (pipe) {
	case 0:
		break;
	case 1:
		pipeconf_reg = PIPEBCONF;
		break;
	case 2:
		pipeconf_reg = PIPECCONF;
		break;
	default:
		DRM_ERROR("Illegal Pipe Number. \n");
		return;
	}

	/* Wait for for the pipe disable to take effect. */
	for (count = 0; count < COUNT_MAX; count++) {
		temp = REG_READ(pipeconf_reg);
		if (!(temp & PIPEACONF_PIPE_STATE))
			break;

		udelay(20);
	}

	PSB_DEBUG_ENTRY("cout = %d. \n", count);
}

void mdfldWaitForPipeEnable(struct drm_device *dev, int pipe)
{
	int count, temp;
	u32 pipeconf_reg = PIPEACONF;

	switch (pipe) {
	case 0:
		break;
	case 1:
		pipeconf_reg = PIPEBCONF;
		break;
	case 2:
		pipeconf_reg = PIPECCONF;
		break;
	default:
		DRM_ERROR("Illegal Pipe Number. \n");
		return;
	}

	/* Wait for for the pipe enable to take effect. */
	for (count = 0; count < COUNT_MAX; count++) {
		temp = REG_READ(pipeconf_reg);
		if ((temp & PIPEACONF_PIPE_STATE))
			break;

		udelay(20);
	}

	PSB_DEBUG_ENTRY("cout = %d. \n", count);
}

static int mdfld_intel_crtc_cursor_set(struct drm_crtc *crtc,
				       struct drm_file *file_priv,
				       uint32_t handle,
				       uint32_t width, uint32_t height)
{
	struct drm_device *dev = crtc->dev;
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	struct psb_gtt *pg = dev_priv->pg;
	struct psb_intel_crtc *psb_intel_crtc = to_psb_intel_crtc(crtc);
	struct psb_intel_mode_device *mode_dev = psb_intel_crtc->mode_dev;
	int pipe = psb_intel_crtc->pipe;
	uint32_t control = CURACNTR;
	uint32_t base = CURABASE;
	uint32_t temp;
	size_t addr = 0;
	uint32_t page_offset;
	size_t size;
	void *bo;
	int ret;

	DRM_DEBUG("\n");

	switch (pipe) {
	case 0:
		break;
	case 1:
		control = CURBCNTR;
		base = CURBBASE;
		break;
	case 2:
		control = CURCCNTR;
		base = CURCBASE;
		break;
	default:
		DRM_ERROR("Illegal Pipe Number. \n");
		return -EINVAL;
	}

	/* Can't enalbe HW cursor on plane B/C. */
	if (pipe != 0)
		return 0;

	/* if we want to turn of the cursor ignore width and height */
	if (!handle) {
		DRM_DEBUG("cursor off\n");
		/* turn off the cursor */
		temp = 0;
		temp |= CURSOR_MODE_DISABLE;

		REG_WRITE(control, temp);
		REG_WRITE(base, 0);

		/* unpin the old bo */
		if (psb_intel_crtc->cursor_bo) {
			mode_dev->bo_unpin_for_scanout(dev,
						       psb_intel_crtc->
						       cursor_bo);
			psb_intel_crtc->cursor_bo = NULL;
		}
		return 0;
	}

	/* Currently we only support 64x64 cursors */
	if (width != 64 || height != 64) {
		DRM_ERROR("we currently only support 64x64 cursors\n");
		return -EINVAL;
	}

	bo = mode_dev->bo_from_handle(dev, file_priv, handle);
	if (!bo)
		return -ENOENT;

	ret = mode_dev->bo_pin_for_scanout(dev, bo);
	if (ret)
		return ret;
	size = mode_dev->bo_size(dev, bo);
	if (size < width * height * 4) {
		DRM_ERROR("buffer is to small\n");
		return -ENOMEM;
	}

	/*insert this bo into gtt */
//        DRM_INFO("%s: map meminfo for hw cursor. handle %x, pipe = %d \n", __FUNCTION__, handle, pipe);

	ret = psb_gtt_map_meminfo(dev, (IMG_HANDLE) handle, 0, &page_offset);
	if (ret) {
		DRM_ERROR("Can not map meminfo to GTT. handle 0x%x\n", handle);
		return ret;
	}

	addr = page_offset << PAGE_SHIFT;

	psb_intel_crtc->cursor_addr = addr;

	temp = 0;
	/* set the pipe for the cursor */
	temp |= (pipe << 28);
	temp |= CURSOR_MODE_64_ARGB_AX | MCURSOR_GAMMA_ENABLE;

	REG_WRITE(control, temp);
	REG_WRITE(base, addr);

	/* unpin the old bo */
	if (psb_intel_crtc->cursor_bo && psb_intel_crtc->cursor_bo != bo) {
		mode_dev->bo_unpin_for_scanout(dev, psb_intel_crtc->cursor_bo);
		psb_intel_crtc->cursor_bo = bo;
	}

	return 0;
}

static struct drm_device globle_dev;

void mdfld__intel_plane_set_alpha(int enable)
{
	struct drm_device *dev = &globle_dev;
	int dspcntr_reg = DSPACNTR;
	u32 dspcntr;

	dspcntr = REG_READ(dspcntr_reg);

	if (enable) {
		dspcntr &= ~DISPPLANE_32BPP_NO_ALPHA;
		dspcntr |= DISPPLANE_32BPP;
	} else {
		dspcntr &= ~DISPPLANE_32BPP;
		dspcntr |= DISPPLANE_32BPP_NO_ALPHA;
	}

	REG_WRITE(dspcntr_reg, dspcntr);
}

/**
 * Disable the pipe, plane and pll.
 *
 */
void mdfld_disable_crtc(struct drm_device *dev, int pipe)
{
	int dpll_reg = MRST_DPLL_A;
	int dspcntr_reg = DSPACNTR;
	int dspbase_reg = MRST_DSPABASE;
	int pipeconf_reg = PIPEACONF;
	u32 gen_fifo_stat_reg = GEN_FIFO_STAT_REG;
	u32 temp;

	PSB_DEBUG_ENTRY("pipe = %d \n", pipe);

	switch (pipe) {
	case 0:
		break;
	case 1:
		dpll_reg = MDFLD_DPLL_B;
		dspcntr_reg = DSPBCNTR;
		dspbase_reg = DSPBSURF;
		pipeconf_reg = PIPEBCONF;
		break;
	case 2:
		dpll_reg = MRST_DPLL_A;
		dspcntr_reg = DSPCCNTR;
		dspbase_reg = MDFLD_DSPCBASE;
		pipeconf_reg = PIPECCONF;
		gen_fifo_stat_reg = GEN_FIFO_STAT_REG + MIPIC_REG_OFFSET;
		break;
	default:
		DRM_ERROR("Illegal Pipe Number. \n");
		return;
	}

	if (pipe != 1)
		mdfld_dsi_gen_fifo_ready(dev, gen_fifo_stat_reg,
					 HS_CTRL_FIFO_EMPTY |
					 HS_DATA_FIFO_EMPTY);

	/* Disable display plane */
	temp = REG_READ(dspcntr_reg);
	if ((temp & DISPLAY_PLANE_ENABLE) != 0) {
		REG_WRITE(dspcntr_reg, temp & ~DISPLAY_PLANE_ENABLE);
		/* Flush the plane changes */
		REG_WRITE(dspbase_reg, REG_READ(dspbase_reg));
		REG_READ(dspbase_reg);
	}

	/* Next, disable display pipes */
	temp = REG_READ(pipeconf_reg);
	if ((temp & PIPEACONF_ENABLE) != 0) {
		temp &= ~PIPEACONF_ENABLE;
		temp |= PIPECONF_PLANE_OFF | PIPECONF_CURSOR_OFF;
		REG_WRITE(pipeconf_reg, temp);
		REG_READ(pipeconf_reg);

		/* Wait for for the pipe disable to take effect. */
		mdfldWaitForPipeDisable(dev, pipe);
	}

	temp = REG_READ(dpll_reg);
	if (temp & DPLL_VCO_ENABLE) {
		if (((pipe != 1)
		     && !((REG_READ(PIPEACONF) | REG_READ(PIPECCONF)) &
			  PIPEACONF_ENABLE))
		    || (pipe == 1)) {
			temp &= ~(DPLL_VCO_ENABLE);
			REG_WRITE(dpll_reg, temp);
			REG_READ(dpll_reg);
			/* Wait for the clocks to turn off. */
			/* FIXME_MDFLD PO may need more delay */
			udelay(500);

			if (!(temp & MDFLD_PWR_GATE_EN)) {
				/* gating power of DPLL */
				REG_WRITE(dpll_reg, temp | MDFLD_PWR_GATE_EN);
				/* FIXME_MDFLD PO - change 500 to 1 after PO */
				udelay(5000);
			}
		}
	}

}

/* MDFLD_PLATFORM end */
#endif
