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

#include "mdfld_dsi_dpi.h"
#include "mdfld_output.h"
#include "mdfld_dsi_pkg_sender.h"
#include "psb_drv.h"
#include "mdfld_csc.h"
#include "psb_irq.h"
#include "dispmgrnl.h"
#include "mrfld_clock.h"
#include "android_hdmi.h"
#include "otm_hdmi.h"

#define KEEP_UNUSED_CODE 0

static
u16 mdfld_dsi_dpi_to_byte_clock_count(int pixel_clock_count,
		int num_lane, int bpp)
{
	return (u16)((pixel_clock_count * bpp) / (num_lane * 8));
}

/*
 * Calculate the dpi time basing on a given drm mode @mode
 * return 0 on success.
 * FIXME: I was using proposed mode value for calculation, may need to
 * use crtc mode values later
 */
int mdfld_dsi_dpi_timing_calculation(struct drm_device *dev,
		struct drm_display_mode *mode,
		struct mdfld_dsi_dpi_timing *dpi_timing,
		int num_lane, int bpp)
{
	int pclk_hsync, pclk_hfp, pclk_hbp, pclk_hactive;
	int pclk_vsync, pclk_vfp, pclk_vbp, pclk_vactive;
	if (!mode || !dpi_timing) {
		DRM_ERROR("Invalid parameter\n");
		return -EINVAL;
	}

	PSB_DEBUG_ENTRY("pclk %d, hdisplay %d, hsync_start %d, hsync_end %d," \
			"htotal %d\n",
			mode->clock, mode->hdisplay, mode->hsync_start,
			mode->hsync_end, mode->htotal);
	PSB_DEBUG_ENTRY("vdisplay %d, vsync_start %d, vsync_end %d," \
			"vtotal %d\n",
			mode->vdisplay, mode->vsync_start,
			mode->vsync_end, mode->vtotal);

	pclk_hactive = mode->hdisplay;
	pclk_hfp = mode->hsync_start - mode->hdisplay;
	pclk_hsync = mode->hsync_end - mode->hsync_start;
	pclk_hbp = mode->htotal - mode->hsync_end;

	pclk_vactive = mode->vdisplay;
	pclk_vfp = mode->vsync_start - mode->vdisplay;
	pclk_vsync = mode->vsync_end - mode->vsync_start;
	pclk_vbp = mode->vtotal - mode->vsync_end;
	/*
	 * byte clock counts were calculated by following formula
	 * bclock_count = pclk_count * bpp / num_lane / 8
	 */
	if (is_dual_dsi(dev)) {
		dpi_timing->hsync_count = pclk_hsync;
		dpi_timing->hbp_count = pclk_hbp;
		dpi_timing->hfp_count = pclk_hfp;
		dpi_timing->hactive_count = pclk_hactive / 2;
		dpi_timing->vsync_count = pclk_vsync;
		dpi_timing->vbp_count = pclk_vbp;
		dpi_timing->vfp_count = pclk_vfp;
	} else {
		dpi_timing->hsync_count =
			mdfld_dsi_dpi_to_byte_clock_count(pclk_hsync, num_lane, bpp);
		dpi_timing->hbp_count =
			mdfld_dsi_dpi_to_byte_clock_count(pclk_hbp, num_lane, bpp);
		dpi_timing->hfp_count =
			mdfld_dsi_dpi_to_byte_clock_count(pclk_hfp, num_lane, bpp);
		dpi_timing->hactive_count =
			mdfld_dsi_dpi_to_byte_clock_count(pclk_hactive, num_lane, bpp);

		dpi_timing->vsync_count =
			mdfld_dsi_dpi_to_byte_clock_count(pclk_vsync, num_lane, bpp);
		dpi_timing->vbp_count =
			mdfld_dsi_dpi_to_byte_clock_count(pclk_vbp, num_lane, bpp);
		dpi_timing->vfp_count =
			mdfld_dsi_dpi_to_byte_clock_count(pclk_vfp, num_lane, bpp);
	}
	PSB_DEBUG_ENTRY("DPI timings: %d, %d, %d, %d, %d, %d, %d\n",
			dpi_timing->hsync_count, dpi_timing->hbp_count,
			dpi_timing->hfp_count, dpi_timing->hactive_count,
			dpi_timing->vsync_count, dpi_timing->vbp_count,
			dpi_timing->vfp_count);

	return 0;
}

void mdfld_dsi_dpi_set_color_mode(struct mdfld_dsi_config *dsi_config , bool on)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	int err;
	u32  spk_pkg =  (on == true) ? MDFLD_DSI_DPI_SPK_COLOR_MODE_ON :
		MDFLD_DSI_DPI_SPK_COLOR_MODE_OFF;


	PSB_DEBUG_ENTRY("Turn  color mode %s  pkg value= %d...\n",
			(on ? "on" : "off"), spk_pkg);

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return ;
	}

	/*send turn on/off color mode packet*/
	err = mdfld_dsi_send_dpi_spk_pkg_hs(sender,
			spk_pkg);
	if (err) {
		DRM_ERROR("Failed to send turn on packet\n");
		return ;
	}
	PSB_DEBUG_ENTRY("Turn  color mode %s successful.\n",
			(on ? "on" : "off"));
	return;
}

static int __dpi_enter_ulps_locked(struct mdfld_dsi_config *dsi_config, int offset)
{
	struct mdfld_dsi_hw_registers *regs = &dsi_config->regs;
	struct mdfld_dsi_hw_context *ctx = &dsi_config->dsi_hw_context;
	struct drm_device *dev = dsi_config->dev;
	struct mdfld_dsi_pkg_sender *sender
		= mdfld_dsi_get_pkg_sender(dsi_config);
	if (!sender) {
		DRM_ERROR("pkg sender is NULL\n");
		return -EINVAL;
	}

	ctx->device_ready = REG_READ(regs->device_ready_reg + offset);

	if (ctx->device_ready & DSI_POWER_STATE_ULPS_MASK) {
		DRM_ERROR("Broken ULPS states\n");
		return -EINVAL;
	}

	if (offset != 0)
		sender->work_for_slave_panel = true;
	/*wait for all FIFOs empty*/
	mdfld_dsi_wait_for_fifos_empty(sender);
	sender->work_for_slave_panel = false;

	/*inform DSI host is to be put on ULPS*/
	ctx->device_ready |= DSI_POWER_STATE_ULPS_ENTER;
	REG_WRITE(regs->device_ready_reg + offset, ctx->device_ready);

	PSB_DEBUG_ENTRY("entered ULPS state\n");
	return 0;
}

static int __dpi_exit_ulps_locked(struct mdfld_dsi_config *dsi_config, int offset)
{
	struct mdfld_dsi_hw_registers *regs = &dsi_config->regs;
	struct mdfld_dsi_hw_context *ctx = &dsi_config->dsi_hw_context;
	struct drm_device *dev = dsi_config->dev;

	ctx->device_ready = REG_READ(regs->device_ready_reg + offset);

	/*enter ULPS EXIT state*/
	ctx->device_ready &= ~DSI_POWER_STATE_ULPS_MASK;
	ctx->device_ready |= DSI_POWER_STATE_ULPS_EXIT;
	REG_WRITE(regs->device_ready_reg + offset, ctx->device_ready);

	/*wait for 1ms as spec suggests*/
	mdelay(1);

	/*clear ULPS state*/
	ctx->device_ready &= ~DSI_POWER_STATE_ULPS_MASK;
	REG_WRITE(regs->device_ready_reg + offset, ctx->device_ready);

	PSB_DEBUG_ENTRY("exited ULPS state\n");
	return 0;
}

static void __dpi_set_properties(struct mdfld_dsi_config *dsi_config,
			enum enum_ports port)
{
	struct mdfld_dsi_hw_registers *regs;
	struct mdfld_dsi_hw_context *ctx;
	struct drm_device *dev;
	int offset = 0;

	regs = &dsi_config->regs;
	ctx = &dsi_config->dsi_hw_context;
	dev = dsi_config->dev;

	if (port == PORT_C)
		offset = 0x800;
	/*D-PHY parameter*/
	REG_WRITE(regs->dphy_param_reg + offset, ctx->dphy_param);

	/*Configure DSI controller*/
	REG_WRITE(regs->mipi_control_reg + offset, ctx->mipi_control);
	REG_WRITE(regs->intr_en_reg + offset, ctx->intr_en);
	REG_WRITE(regs->hs_tx_timeout_reg + offset, ctx->hs_tx_timeout);
	REG_WRITE(regs->lp_rx_timeout_reg + offset, ctx->lp_rx_timeout);
	REG_WRITE(regs->turn_around_timeout_reg + offset,
			ctx->turn_around_timeout);
	REG_WRITE(regs->device_reset_timer_reg + offset,
			ctx->device_reset_timer);
	REG_WRITE(regs->high_low_switch_count_reg + offset,
			ctx->high_low_switch_count);
	REG_WRITE(regs->init_count_reg + offset, ctx->init_count);
	REG_WRITE(regs->eot_disable_reg + offset, (REG_READ(regs->eot_disable_reg) & ~DSI_EOT_DISABLE_MASK) | (ctx->eot_disable & DSI_EOT_DISABLE_MASK));
	REG_WRITE(regs->lp_byteclk_reg + offset, ctx->lp_byteclk);
	REG_WRITE(regs->clk_lane_switch_time_cnt_reg + offset,
			ctx->clk_lane_switch_time_cnt);
	REG_WRITE(regs->video_mode_format_reg + offset, ctx->video_mode_format);
	REG_WRITE(regs->dsi_func_prg_reg + offset, ctx->dsi_func_prg);

	/*DSI timing*/
	REG_WRITE(regs->dpi_resolution_reg + offset, ctx->dpi_resolution);
	REG_WRITE(regs->hsync_count_reg + offset, ctx->hsync_count);
	REG_WRITE(regs->hbp_count_reg + offset, ctx->hbp_count);
	REG_WRITE(regs->hfp_count_reg + offset, ctx->hfp_count);
	REG_WRITE(regs->hactive_count_reg + offset, ctx->hactive_count);
	REG_WRITE(regs->vsync_count_reg + offset, ctx->vsync_count);
	REG_WRITE(regs->vbp_count_reg + offset, ctx->vbp_count);
	REG_WRITE(regs->vfp_count_reg + offset, ctx->vfp_count);

}

static int __dpi_config_port(struct mdfld_dsi_config *dsi_config,
			struct panel_funcs *p_funcs, enum enum_ports port)
{
	struct mdfld_dsi_hw_registers *regs;
	struct mdfld_dsi_hw_context *ctx;
	struct drm_device *dev;
	int offset = 0;

	if (!dsi_config)
		return -EINVAL;

	regs = &dsi_config->regs;
	ctx = &dsi_config->dsi_hw_context;
	dev = dsi_config->dev;

	if (port == PORT_C)
		offset = 0x800;

	/*exit ULPS state*/
	__dpi_exit_ulps_locked(dsi_config, offset);

	/*Enable DSI Controller*/
	REG_WRITE(regs->device_ready_reg + offset, ctx->device_ready | BIT0);

	/*set low power output hold*/
	if (port == PORT_C)
		offset = 0x1000;
	REG_WRITE(regs->mipi_reg + offset, (ctx->mipi | BIT16));

	return 0;
}

static void ann_dc_setup(struct mdfld_dsi_config *dsi_config)
{
	struct drm_device *dev = dsi_config->dev;
	struct mdfld_dsi_hw_registers *regs = &dsi_config->regs;
	struct mdfld_dsi_hw_context *ctx = &dsi_config->dsi_hw_context;


	DRM_INFO("restore some registers to default value\n");

	power_island_get(OSPM_DISPLAY_B | OSPM_DISPLAY_C);

	REG_WRITE(DSPCLK_GATE_D, 0x0);
	REG_WRITE(RAMCLK_GATE_D, 0xc0000 | (1 << 11)); // FIXME: delay 1us for RDB done signal
	REG_WRITE(PFIT_CONTROL, 0x20000000);
	REG_WRITE(DSPIEDCFGSHDW, 0x0);
	REG_WRITE(DSPARB2, 0x000A0200);
	REG_WRITE(DSPARB, 0x18040080);
	REG_WRITE(DSPFW1, 0x0F0F3F3F);
	REG_WRITE(DSPFW2, 0x5F2F0F3F);
	REG_WRITE(DSPFW3, 0x0);
	REG_WRITE(DSPFW4, 0x07071F1F);
	REG_WRITE(DSPFW5, 0x2F17071F);
	REG_WRITE(DSPFW6, 0x00001F3F);
	REG_WRITE(DSPFW7, 0x1F3F1F3F);
	REG_WRITE(DSPSRCTRL, 0x00080100);
	REG_WRITE(DSPCHICKENBIT, 0x20);
	REG_WRITE(FBDC_CHICKEN, 0x0C0C0C0C);
	REG_WRITE(CURACNTR, 0x0);
	REG_WRITE(CURBCNTR, 0x0);
	REG_WRITE(CURCCNTR, 0x0);
	REG_WRITE(IEP_OVA_CTRL, 0x0);
	REG_WRITE(IEP_OVA_CTRL, 0x0);

	REG_WRITE(DSPBCNTR, 0x0);
	REG_WRITE(DSPCCNTR, 0x0);
	REG_WRITE(DSPDCNTR, 0x0);
	REG_WRITE(DSPECNTR, 0x0);
	REG_WRITE(DSPFCNTR, 0x0);
	REG_WRITE(GCI_CTRL, REG_READ(GCI_CTRL) | 1);

	power_island_put(OSPM_DISPLAY_B | OSPM_DISPLAY_C);

	DRM_INFO("setup drain latency\n");

	REG_WRITE(regs->ddl1_reg, ctx->ddl1);
	REG_WRITE(regs->ddl2_reg, ctx->ddl2);
	REG_WRITE(regs->ddl3_reg, ctx->ddl3);
	REG_WRITE(regs->ddl4_reg, ctx->ddl4);
}

/**
 * Power on sequence for video mode MIPI panel.
 * NOTE: do NOT modify this function
 */
static int __dpi_panel_power_on(struct mdfld_dsi_config *dsi_config,
		struct panel_funcs *p_funcs, bool first_boot)
{
	u32 val = 0;
	struct mdfld_dsi_hw_registers *regs;
	struct mdfld_dsi_hw_context *ctx;
	struct drm_psb_private *dev_priv;
	struct drm_device *dev;
	int retry, reset_count = 10;
	int i;
	int err = 0;
	u32 power_island = 0;
	int offset = 0;

	PSB_DEBUG_ENTRY("\n");

	if (!dsi_config)
		return -EINVAL;

	regs = &dsi_config->regs;
	ctx = &dsi_config->dsi_hw_context;
	dev = dsi_config->dev;
	dev_priv = dev->dev_private;
	power_island = pipe_to_island(dsi_config->pipe);
	if (power_island & (OSPM_DISPLAY_A | OSPM_DISPLAY_C))
		power_island |= OSPM_DISPLAY_MIO;
	if (is_dual_dsi(dev))
		power_island |= OSPM_DISPLAY_C;

	if (!power_island_get(power_island))
		return -EAGAIN;
	if (android_hdmi_is_connected(dev) && first_boot)
			otm_hdmi_power_islands_on();

reset_recovery:
	--reset_count;
	/*HW-Reset*/
	if (p_funcs && p_funcs->reset)
		p_funcs->reset(dsi_config);

	/*
	 * Wait for DSI PLL locked on pipe, and only need to poll status of pipe
	 * A as both MIPI pipes share the same DSI PLL.
	 */
	if (dsi_config->pipe == 0) {
		retry = 20000;
		while (!(REG_READ(regs->pipeconf_reg) & PIPECONF_DSIPLL_LOCK) &&
				--retry)
			udelay(150);
		if (!retry) {
			DRM_ERROR("PLL failed to lock on pipe\n");
			err = -EAGAIN;
			goto power_on_err;
		}
	}

	if (IS_ANN(dev)) {
		/* FIXME: reset the DC registers for ANN A0 */
		ann_dc_setup(dsi_config);
	}

	__dpi_set_properties(dsi_config, PORT_A);

	/* update 0x650c[0] = 1 to fixed arbitration pattern
	 * it is found display TLB request be blocked by display plane
	 * memory requests, never goes out. This causes display controller
	 * uses stale TLB data to do memory translation, getting wrong
	 * memory address for data, and causing the flickering issue.
	 */
	REG_WRITE(GCI_CTRL, REG_READ(GCI_CTRL) | 1);

	/*Setup pipe timing*/
	REG_WRITE(regs->htotal_reg, ctx->htotal);
	REG_WRITE(regs->hblank_reg, ctx->hblank);
	REG_WRITE(regs->hsync_reg, ctx->hsync);
	REG_WRITE(regs->vtotal_reg, ctx->vtotal);
	REG_WRITE(regs->vblank_reg, ctx->vblank);
	REG_WRITE(regs->vsync_reg, ctx->vsync);
	REG_WRITE(regs->pipesrc_reg, ctx->pipesrc);

	REG_WRITE(regs->dsppos_reg, ctx->dsppos);
	REG_WRITE(regs->dspstride_reg, ctx->dspstride);

	/*Setup plane*/
	REG_WRITE(regs->dspsize_reg, ctx->dspsize);
	REG_WRITE(regs->dspsurf_reg, ctx->dspsurf);
	REG_WRITE(regs->dsplinoff_reg, ctx->dsplinoff);
	REG_WRITE(regs->vgacntr_reg, ctx->vgacntr);

	/*restore color_coef (chrome) */
	for (i = 0; i < 6; i++)
		REG_WRITE(regs->color_coef_reg + (i<<2), csc_setting_save[i]);

	/* restore palette (gamma) */
	for (i = 0; i < 256; i++)
		REG_WRITE(regs->palette_reg + (i<<2), gamma_setting_save[i]);

	/* restore dpst setting */
	if (dev_priv->psb_dpst_state) {
		dpstmgr_reg_restore_locked(dev, dsi_config);
		psb_enable_pipestat(dev_priv, 0, PIPE_DPST_EVENT_ENABLE);
	}

	if (__dpi_config_port(dsi_config, p_funcs, PORT_A) != 0) {
		if (!reset_count) {
				err = -EAGAIN;
				goto power_on_err;
			}
			DRM_ERROR("Failed to init dsi controller, reset it!\n");
			goto reset_recovery;
	}
	if (is_dual_dsi(dev)) {
		__dpi_set_properties(dsi_config, PORT_C);
		__dpi_config_port(dsi_config, p_funcs, PORT_C);
	}

	/**
	 * Different panel may have different ways to have
	 * drvIC initialized. Support it!
	 */
	if (p_funcs && p_funcs->drv_ic_init) {
		if (p_funcs->drv_ic_init(dsi_config)) {
			if (!reset_count) {
				err = -EAGAIN;
				goto power_on_err;
			}

			DRM_ERROR("Failed to init dsi controller, reset it!\n");
			goto reset_recovery;
		}
	}

	/*Enable MIPI Port A*/
	offset = 0x0;
	REG_WRITE(regs->mipi_reg + offset, (ctx->mipi | BIT31));
	REG_WRITE(regs->dpi_control_reg + offset, BIT1);
	if (is_dual_dsi(dev)) {
		/*Enable MIPI Port C*/
		offset = 0x1000;
		REG_WRITE(regs->mipi_reg + offset, (ctx->mipi | BIT31));
		offset = 0x800;
		REG_WRITE(regs->dpi_control_reg + offset, BIT1);
	}
	/**
	 * Different panel may have different ways to have
	 * panel turned on. Support it!
	 */
	if (p_funcs && p_funcs->power_on)
		if (p_funcs->power_on(dsi_config)) {
			DRM_ERROR("Failed to power on panel\n");
			err = -EAGAIN;
			goto power_on_err;
		}

	if (IS_ANN(dev)) {
		REG_WRITE(regs->ddl1_reg, ctx->ddl1);
		REG_WRITE(regs->ddl2_reg, ctx->ddl2);
		REG_WRITE(regs->ddl3_reg, ctx->ddl3);
		REG_WRITE(regs->ddl4_reg, ctx->ddl4);

		REG_WRITE(DSPARB2, ctx->dsparb2);
		REG_WRITE(DSPARB, ctx->dsparb);
	}

	/*Enable pipe*/
	val = ctx->pipeconf;
	val &= ~0x000c0000;
	/**
	 * Frame Start occurs on third HBLANK
	 * after the start of VBLANK
	 */
	val |= BIT31 | BIT28;
	REG_WRITE(regs->pipeconf_reg, val);
	/*Wait for pipe enabling,when timing generator
	  is wroking */
	if (REG_READ(regs->mipi_reg) & BIT31) {
		retry = 10000;
		while (--retry && !(REG_READ(regs->pipeconf_reg) & BIT30))
			udelay(3);

		if (!retry) {
			DRM_ERROR("Failed to enable pipe\n");
			err = -EAGAIN;
			goto power_on_err;
		}
	}
	/*enable plane*/
	val = ctx->dspcntr | BIT31;
	REG_WRITE(regs->dspcntr_reg, val);

	if (p_funcs && p_funcs->set_brightness) {
		if (p_funcs->set_brightness(dsi_config,
				ctx->lastbrightnesslevel))
			DRM_ERROR("Failed to set panel brightness\n");
	} else {
		DRM_ERROR("Failed to set panel brightness\n");
	}
	if (p_funcs && p_funcs->drv_set_panel_mode)
		p_funcs->drv_set_panel_mode(dsi_config);

	psb_enable_vblank(dev, dsi_config->pipe);
	return err;

power_on_err:
	power_island_put(power_island);
	return err;
}

/**
 * Power off sequence for video mode MIPI panel.
 * NOTE: do NOT modify this function
 */
static int __dpi_panel_power_off(struct mdfld_dsi_config *dsi_config,
		struct panel_funcs *p_funcs)
{
	u32 val = 0;
	u32 tmp = 0;
	struct mdfld_dsi_hw_registers *regs;
	struct mdfld_dsi_hw_context *ctx;
	struct drm_device *dev;
	struct drm_psb_private *dev_priv;
	int retry;
	int i;
	int err = 0;
	u32 power_island = 0;
	int offset = 0;

	PSB_DEBUG_ENTRY("\n");

	if (!dsi_config)
		return -EINVAL;

	regs = &dsi_config->regs;
	ctx = &dsi_config->dsi_hw_context;
	dev = dsi_config->dev;
	dev_priv = dev->dev_private;

	ctx->dsparb = REG_READ(DSPARB);
	ctx->dsparb2 = REG_READ(DSPARB2);

	/* Don't reset brightness to 0.*/
	ctx->lastbrightnesslevel = psb_brightness;

	tmp = REG_READ(regs->pipeconf_reg);
        ctx->dspcntr = REG_READ(regs->dspcntr_reg);

	/*save color_coef (chrome) */
	for (i = 0; i < 6; i++)
		ctx->color_coef[i] = REG_READ(regs->color_coef_reg + (i<<2));

	/* save palette (gamma) */
	for (i = 0; i < 256; i++)
		ctx->palette[i] = REG_READ(regs->palette_reg + (i<<2));

	/*
	 * Couldn't disable the pipe until DRM_WAIT_ON signaled by last
	 * vblank event when playing video, otherwise the last vblank event
	 * will lost when pipe disabled before vblank interrupt coming
	 * sometimes.
	 */

	/*Disable panel*/
	val = ctx->dspcntr;
	REG_WRITE(regs->dspcntr_reg, (val & ~BIT31));
	/*Disable overlay & cursor panel assigned to this pipe*/
	REG_WRITE(regs->pipeconf_reg, (tmp | (0x000c0000)));

	/*Disable pipe*/
	val = REG_READ(regs->pipeconf_reg);
	ctx->pipeconf = val;
	REG_WRITE(regs->pipeconf_reg, (val & ~BIT31));

	/*wait for pipe disabling,
	  pipe synchronization plus , only avaiable when
	  timer generator is working*/
	if (REG_READ(regs->mipi_reg) & BIT31) {
		retry = 100000;
		while (--retry && (REG_READ(regs->pipeconf_reg) & BIT30))
			udelay(5);

		if (!retry) {
			DRM_ERROR("Failed to disable pipe\n");
			if (IS_MOFD(dev)) {
				/*
				 * FIXME: turn off the power island directly
				 * although failed to disable pipe.
				 */
				err = 0;
			} else
				err = -EAGAIN;
			goto power_off_err;
		}
	}

	/**
	 * Different panel may have different ways to have
	 * panel turned off. Support it!
	 */
	if (p_funcs && p_funcs->power_off) {
		if (p_funcs->power_off(dsi_config)) {
			DRM_ERROR("Failed to power off panel\n");
			err = -EAGAIN;
			goto power_off_err;
		}
	}

	/*Disable MIPI port*/
	REG_WRITE(regs->mipi_reg, (REG_READ(regs->mipi_reg) & ~BIT31));

	/*clear Low power output hold*/
	REG_WRITE(regs->mipi_reg, (REG_READ(regs->mipi_reg) & ~BIT16));

	/*Disable DSI controller*/
	REG_WRITE(regs->device_ready_reg, (ctx->device_ready & ~BIT0));

	/*enter ULPS*/
	__dpi_enter_ulps_locked(dsi_config, offset);

	if (is_dual_dsi(dev)) {
		offset = 0x1000;
		/*Disable MIPI port*/
		REG_WRITE(regs->mipi_reg, (REG_READ(regs->mipi_reg) & ~BIT31));

		/*clear Low power output hold*/
		REG_WRITE(regs->mipi_reg, (REG_READ(regs->mipi_reg) & ~BIT16));
		offset = 0x800;
		/*Disable DSI controller*/
		REG_WRITE(regs->device_ready_reg, (ctx->device_ready & ~BIT0));

		/*enter ULPS*/
		__dpi_enter_ulps_locked(dsi_config, offset);
		offset = 0x0;
	}

power_off_err:
	power_island = pipe_to_island(dsi_config->pipe);

	if (power_island & (OSPM_DISPLAY_A | OSPM_DISPLAY_C))
		power_island |= OSPM_DISPLAY_MIO;

	if (is_dual_dsi(dev))
		power_island |= OSPM_DISPLAY_C;

	if (!power_island_put(power_island))
		return -EINVAL;

	return err;
}

#if KEEP_UNUSED_CODE
/**
 * Send TURN_ON package to dpi panel to turn it on
 */
static int mdfld_dsi_dpi_panel_turn_on(struct mdfld_dsi_config *dsi_config,
		struct panel_funcs *p_funcs)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	struct mdfld_dsi_hw_context *ctx;
	int err = 0;

	err = mdfld_dsi_send_dpi_spk_pkg_hs(sender,
			MDFLD_DSI_DPI_SPK_TURN_ON);
	/*According HW DSI spec, here need wait for 100ms*/
	/*To optimize dpms flow, move sleep out of mutex*/
	/* msleep(100); */

	ctx = &dsi_config->dsi_hw_context;
	if (p_funcs->set_brightness(dsi_config, ctx->lastbrightnesslevel))
		DRM_ERROR("Failed to set panel brightness\n");

	return err;
}
#endif /* if KEEP_UNUSED_CODE */

#if KEEP_UNUSED_CODE
/**
 * Send SHUT_DOWN package to dpi panel to turn if off
 */
static int mdfld_dsi_dpi_panel_shut_down(struct mdfld_dsi_config *dsi_config,
		struct panel_funcs *p_funcs)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	struct mdfld_dsi_hw_context *ctx;
	int err = 0;

	ctx = &dsi_config->dsi_hw_context;
	ctx->lastbrightnesslevel = psb_brightness;
	if (p_funcs->set_brightness(dsi_config, 0))
		DRM_ERROR("Failed to set panel brightness\n");

	err = mdfld_dsi_send_dpi_spk_pkg_hs(sender,
			MDFLD_DSI_DPI_SPK_SHUT_DOWN);
	/*According HW DSI spec, here need wait for 100ms*/
	/*To optimize dpms flow, move sleep out of mutex*/
	/* msleep(100); */

	return err;
}
#endif /* if KEEP_UNUSED_CODE */

/**
 * Setup Display Controller to turn on/off a video mode panel.
 * Most of the video mode MIPI panel should follow the power on/off
 * sequence in this function.
 * NOTE: do NOT modify this function for new panel Enabling. Register
 * new panel function callbacks to make this function available for a
 * new video mode panel
 */
static int __mdfld_dsi_dpi_set_power(struct drm_encoder *encoder, bool on)
{
	struct mdfld_dsi_encoder *dsi_encoder;
	struct mdfld_dsi_connector *dsi_connector;
	struct mdfld_dsi_dpi_output *dpi_output;
	struct mdfld_dsi_config *dsi_config;
	struct panel_funcs *p_funcs;
	int pipe;
	struct drm_device *dev;
	struct drm_psb_private *dev_priv;

	if (!encoder) {
		DRM_ERROR("Invalid encoder\n");
		return -EINVAL;
	}

	dsi_encoder = MDFLD_DSI_ENCODER(encoder);
	dpi_output = MDFLD_DSI_DPI_OUTPUT(dsi_encoder);
	dsi_config = mdfld_dsi_encoder_get_config(dsi_encoder);
	p_funcs = dpi_output->p_funcs;
	pipe = mdfld_dsi_encoder_get_pipe(dsi_encoder);
	dsi_connector = mdfld_dsi_encoder_get_connector(dsi_encoder);
	if (!dsi_connector) {
		DRM_ERROR("dsi_connector is NULL\n");
		return -EINVAL;
	}

	dev = dsi_config->dev;
	dev_priv = dev->dev_private;

	if (dsi_connector->status != connector_status_connected)
		return 0;

	mutex_lock(&dsi_config->context_lock);

	if (dpi_output->first_boot && on) {
		if (dsi_config->dsi_hw_context.panel_on) {
			if (IS_ANN(dev))
				ann_dc_setup(dsi_config);

			psb_enable_vblank(dev, dsi_config->pipe);

			/* don't need ISLAND c for non dual-dsi panel */
			if (!is_dual_dsi(dev))
				power_island_put(OSPM_DISPLAY_C);

			DRM_INFO("skip panle power setting for first boot! "
				 "panel is already powered on\n");
			goto fun_exit;
		}
		if (android_hdmi_is_connected(dev))
			otm_hdmi_power_islands_off();
		/* power down islands turned on by firmware */
		power_island_put(OSPM_DISPLAY_A | OSPM_DISPLAY_C |
				 OSPM_DISPLAY_MIO);
	}

	switch (on) {
	case true:
		/* panel is already on */
		if (dsi_config->dsi_hw_context.panel_on)
			goto fun_exit;
		if (__dpi_panel_power_on(dsi_config, p_funcs, dpi_output->first_boot)) {
			DRM_ERROR("Faild to turn on panel\n");
			goto set_power_err;
		}
		dsi_config->dsi_hw_context.panel_on = 1;

		/* for every dpi panel power on, clear the dpi underrun count */
		dev_priv->pipea_dpi_underrun_count = 0;
		dev_priv->pipec_dpi_underrun_count = 0;

		break;
	case false:
		if (!dsi_config->dsi_hw_context.panel_on &&
			!dpi_output->first_boot)
			goto fun_exit;
		if (__dpi_panel_power_off(dsi_config, p_funcs)) {
			DRM_ERROR("Faild to turn off panel\n");
			goto set_power_err;
		}
		dsi_config->dsi_hw_context.panel_on = 0;
		break;
	default:
		break;
	}

fun_exit:
	mutex_unlock(&dsi_config->context_lock);
	PSB_DEBUG_ENTRY("successfully\n");
	return 0;
set_power_err:
	mutex_unlock(&dsi_config->context_lock);
	PSB_DEBUG_ENTRY("unsuccessfully!!!!\n");
	return -EAGAIN;
}

void mdfld_dsi_dpi_set_power(struct drm_encoder *encoder, bool on)
{
	struct mdfld_dsi_encoder *dsi_encoder = MDFLD_DSI_ENCODER(encoder);
	struct mdfld_dsi_config *dsi_config =
		mdfld_dsi_encoder_get_config(dsi_encoder);
	struct drm_device *dev;
	struct drm_psb_private *dev_priv;
	struct mdfld_dsi_dpi_output *dpi_output = NULL;
	u32 mipi_reg = MIPI;
	u32 pipeconf_reg = PIPEACONF;
	int pipe;

	if (!dsi_config) {
		DRM_ERROR("dsi_config is NULL\n");
		return;
	}

	pipe = mdfld_dsi_encoder_get_pipe(dsi_encoder);
	dev = dsi_config->dev;
	dev_priv = dev->dev_private;

	PSB_DEBUG_ENTRY("set power %s on pipe %d\n", on ? "On" : "Off", pipe);

	dpi_output = MDFLD_DSI_DPI_OUTPUT(dsi_encoder);

	if (pipe)
		if (!(dev_priv->panel_desc & DISPLAY_B) ||
				!(dev_priv->panel_desc & DISPLAY_C))
			return;

	if (pipe) {
		mipi_reg = MIPI_C;
		pipeconf_reg = PIPECCONF;
	}

	/**
	 * if TMD panel call new power on/off sequences instead.
	 * NOTE: refine TOSHIBA panel code later
	 */
	__mdfld_dsi_dpi_set_power(encoder, on);
}

static
void mdfld_dsi_dpi_dpms(struct drm_encoder *encoder, int mode)
{
	struct mdfld_dsi_encoder *dsi_encoder;
	struct mdfld_dsi_config *dsi_config;
	struct drm_device *dev;
	struct drm_psb_private *dev_priv;
	struct mdfld_dsi_dpi_output *dpi_output;
	struct panel_funcs *p_funcs;

	dsi_encoder = MDFLD_DSI_ENCODER(encoder);
	dsi_config = mdfld_dsi_encoder_get_config(dsi_encoder);
	if (!dsi_config) {
		DRM_ERROR("dsi_config is NULL\n");
		return;
	}
	dev = dsi_config->dev;
	dev_priv = dev->dev_private;

	dpi_output = MDFLD_DSI_DPI_OUTPUT(dsi_encoder);
	p_funcs = dpi_output->p_funcs;

	PSB_DEBUG_ENTRY("%s\n", (mode == DRM_MODE_DPMS_ON ? "on" :
		DRM_MODE_DPMS_STANDBY == mode ? "standby" : "off"));

	mutex_lock(&dev_priv->dpms_mutex);
	DCLockMutex();

	if (mode == DRM_MODE_DPMS_ON) {
		mdfld_dsi_dpi_set_power(encoder, true);
		DCAttachPipe(dsi_config->pipe);
		DC_MRFLD_onPowerOn(dsi_config->pipe);

#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
		{
			struct mdfld_dsi_hw_context *ctx =
				&dsi_config->dsi_hw_context;
			struct backlight_device bd;
			bd.props.brightness = ctx->lastbrightnesslevel;
			psb_set_brightness(&bd);
		}
#endif
	} else if (mode == DRM_MODE_DPMS_STANDBY) {
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
		struct mdfld_dsi_hw_context *ctx = &dsi_config->dsi_hw_context;
		struct backlight_device bd;
		ctx->lastbrightnesslevel = psb_get_brightness(&bd);
		bd.props.brightness = 0;
		psb_set_brightness(&bd);
#endif
		/* Make the pending flip request as completed. */
		DCUnAttachPipe(dsi_config->pipe);
		msleep(50);
		DC_MRFLD_onPowerOff(dsi_config->pipe);
		msleep(50);
	} else {
		mdfld_dsi_dpi_set_power(encoder, false);

		drm_handle_vblank(dev, dsi_config->pipe);

		/* Turn off TE interrupt. */
		drm_vblank_off(dev, dsi_config->pipe);

		/* Make the pending flip request as completed. */
		DCUnAttachPipe(dsi_config->pipe);
		DC_MRFLD_onPowerOff(dsi_config->pipe);
	}

	DCUnLockMutex();
	mutex_unlock(&dev_priv->dpms_mutex);
}

static
bool mdfld_dsi_dpi_mode_fixup(struct drm_encoder *encoder,
		const struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode)
{
	struct mdfld_dsi_encoder *dsi_encoder = MDFLD_DSI_ENCODER(encoder);
	struct mdfld_dsi_config *dsi_config =
		mdfld_dsi_encoder_get_config(dsi_encoder);
	struct drm_display_mode *fixed_mode;

	if (!dsi_config) {
		DRM_ERROR("dsi_config is NULL\n");
		return false;
	}

	fixed_mode = dsi_config->fixed_mode;

	PSB_DEBUG_ENTRY("\n");

	if (fixed_mode) {
		adjusted_mode->hdisplay = fixed_mode->hdisplay;
		adjusted_mode->hsync_start = fixed_mode->hsync_start;
		adjusted_mode->hsync_end = fixed_mode->hsync_end;
		adjusted_mode->htotal = fixed_mode->htotal;
		adjusted_mode->vdisplay = fixed_mode->vdisplay;
		adjusted_mode->vsync_start = fixed_mode->vsync_start;
		adjusted_mode->vsync_end = fixed_mode->vsync_end;
		adjusted_mode->vtotal = fixed_mode->vtotal;
		adjusted_mode->clock = fixed_mode->clock;
		drm_mode_set_crtcinfo(adjusted_mode, CRTC_INTERLACE_HALVE_V);
	}

	return true;
}

static
void mdfld_dsi_dpi_prepare(struct drm_encoder *encoder)
{
	PSB_DEBUG_ENTRY("\n");
}

static
void mdfld_dsi_dpi_commit(struct drm_encoder *encoder)
{
	struct mdfld_dsi_encoder *dsi_encoder;
	struct mdfld_dsi_dpi_output *dpi_output;

	PSB_DEBUG_ENTRY("\n");

	dsi_encoder = MDFLD_DSI_ENCODER(encoder);
	dpi_output = MDFLD_DSI_DPI_OUTPUT(dsi_encoder);

	/*Everything is ready, commit DSI hw context to HW*/
	__mdfld_dsi_dpi_set_power(encoder, true);
	dpi_output->first_boot = 0;
}

/**
 * Setup DPI timing for video mode MIPI panel.
 * NOTE: do NOT modify this function
 */
static void __mdfld_dsi_dpi_set_timing(struct mdfld_dsi_config *config,
		struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode)
{
	struct mdfld_dsi_dpi_timing dpi_timing;
	struct mdfld_dsi_hw_context *ctx;
	struct drm_device *dev = config->dev;

	if (!config) {
		DRM_ERROR("Invalid parameters\n");
		return;
	}

	mode = adjusted_mode;
	ctx = &config->dsi_hw_context;

	mutex_lock(&config->context_lock);

	/*dpi resolution*/
	if (is_dual_dsi(dev))
		ctx->dpi_resolution = (mode->vdisplay << 16 | (mode->hdisplay / 2));
	else
		ctx->dpi_resolution = (mode->vdisplay << 16 | mode->hdisplay);

	/*Calculate DPI timing*/
	mdfld_dsi_dpi_timing_calculation(dev, mode, &dpi_timing,
			config->lane_count,
			config->bpp);

	/*update HW context with new DPI timings*/
	ctx->hsync_count = dpi_timing.hsync_count;
	ctx->hbp_count = dpi_timing.hbp_count;
	ctx->hfp_count = dpi_timing.hfp_count;
	ctx->hactive_count = dpi_timing.hactive_count;
	ctx->vsync_count = dpi_timing.vsync_count;
	ctx->vbp_count = dpi_timing.vbp_count;
	ctx->vfp_count = dpi_timing.vfp_count;

	mutex_unlock(&config->context_lock);
}

static
void mdfld_dsi_dpi_mode_set(struct drm_encoder *encoder,
		struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode)
{
	struct mdfld_dsi_encoder *dsi_encoder = MDFLD_DSI_ENCODER(encoder);
	struct mdfld_dsi_config *dsi_config =
		mdfld_dsi_encoder_get_config(dsi_encoder);
	int pipe = mdfld_dsi_encoder_get_pipe(dsi_encoder);


	PSB_DEBUG_ENTRY("set mode %dx%d on pipe %d",
			mode->hdisplay, mode->vdisplay, pipe);

	/**
	 * if TMD panel call new power on/off sequences instead.
	 * NOTE: refine TOSHIBA panel code later
	 */
	if (!dsi_config) {
		DRM_ERROR("Invalid dsi config\n");
		return;
	}

	__mdfld_dsi_dpi_set_timing(dsi_config, mode, adjusted_mode);
	mdfld_dsi_set_drain_latency(encoder, adjusted_mode);
}

static
void mdfld_dsi_dpi_save(struct drm_encoder *encoder)
{
	struct mdfld_dsi_encoder *dsi_encoder;
	struct mdfld_dsi_config *dsi_config;
	struct drm_device *dev;
	int pipe;

	if (!encoder)
		return;

	PSB_DEBUG_ENTRY("\n");

	dsi_encoder = MDFLD_DSI_ENCODER(encoder);
	dsi_config = mdfld_dsi_encoder_get_config(dsi_encoder);
	dev = dsi_config->dev;
	pipe = mdfld_dsi_encoder_get_pipe(dsi_encoder);

	DCLockMutex();

	/* give time to the last flip to take effective,
	 * if we disable hardware too quickly, overlay hardware may crash,
	 * causing pipe hang next time when we try to use overlay
	 */
	msleep(50);
	DC_MRFLD_onPowerOff(pipe);
	msleep(50);
	__mdfld_dsi_dpi_set_power(encoder, false);

	drm_handle_vblank(dev, pipe);

	/* Turn off vsync interrupt. */
	drm_vblank_off(dev, pipe);

	/* Make the pending flip request as completed. */
	DCUnAttachPipe(pipe);
	DCUnLockMutex();
}

static
void mdfld_dsi_dpi_restore(struct drm_encoder *encoder)
{
	struct mdfld_dsi_encoder *dsi_encoder;
	struct mdfld_dsi_config *dsi_config;
	struct drm_device *dev;
	int pipe;

	if (!encoder)
		return;

	PSB_DEBUG_ENTRY("\n");

	dsi_encoder = MDFLD_DSI_ENCODER(encoder);
	dsi_config = mdfld_dsi_encoder_get_config(dsi_encoder);
	dev = dsi_config->dev;
	pipe = mdfld_dsi_encoder_get_pipe(dsi_encoder);

	DCLockMutex();
	__mdfld_dsi_dpi_set_power(encoder, true);

	DCAttachPipe(pipe);
	DC_MRFLD_onPowerOn(pipe);
	DCUnLockMutex();
}

static const
struct drm_encoder_helper_funcs dsi_dpi_generic_encoder_helper_funcs = {
	.save = mdfld_dsi_dpi_save,
	.restore = mdfld_dsi_dpi_restore,
	.dpms = mdfld_dsi_dpi_dpms,
	.mode_fixup = mdfld_dsi_dpi_mode_fixup,
	.prepare = mdfld_dsi_dpi_prepare,
	.mode_set = mdfld_dsi_dpi_mode_set,
	.commit = mdfld_dsi_dpi_commit,
};

static const
struct drm_encoder_funcs dsi_dpi_generic_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

/*
 * Init DSI DPI encoder. 
 * Allocate an mdfld_dsi_encoder and attach it to given @dsi_connector
 * return pointer of newly allocated DPI encoder, NULL on error
 */
struct mdfld_dsi_encoder *mdfld_dsi_dpi_init(struct drm_device *dev,
		struct mdfld_dsi_connector *dsi_connector,
		struct panel_funcs *p_funcs)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct mdfld_dsi_dpi_output *dpi_output = NULL;
	struct mdfld_dsi_config *dsi_config;
	struct drm_connector *connector = NULL;
	struct drm_encoder *encoder = NULL;
	struct drm_display_mode *fixed_mode = NULL;
	int pipe;
	int ret;

	PSB_DEBUG_ENTRY("[DISPLAY] %s\n", __func__);

	if (!dsi_connector || !p_funcs) {
		DRM_ERROR("Invalid parameters\n");
		return NULL;
	}
	dsi_config = mdfld_dsi_get_config(dsi_connector);
	pipe = dsi_connector->pipe;

	/*detect panel connection stauts*/
	if (p_funcs->detect) {
		ret = p_funcs->detect(dsi_config);
		if (ret) {
			DRM_INFO("Detecting Panel %d, Not connected\n", pipe);
			dsi_connector->status = connector_status_disconnected;
		} else {
			PSB_DEBUG_ENTRY("Panel %d is connected\n", pipe);
			dsi_connector->status = connector_status_connected;
		}

		if (dsi_connector->status == connector_status_disconnected &&
				pipe == 0) {
			DRM_ERROR("Primary panel disconnected\n");
			return NULL;
		}
	} else {
		/*use the default config*/
		if (pipe == 0)
			dsi_connector->status = connector_status_connected;
		else
			dsi_connector->status = connector_status_disconnected;
	}
	/*init DSI controller*/
	if (p_funcs->dsi_controller_init)
		p_funcs->dsi_controller_init(dsi_config);
	/**
	 * TODO: can we keep these code out of display driver as
	 * it will make display driver hard to be maintained
	 */
	if (dsi_connector->status == connector_status_connected) {
		if (pipe == 0)
			dev_priv->panel_desc |= DISPLAY_A;
		if (pipe == 2)
			dev_priv->panel_desc |= DISPLAY_C;
	}

	dpi_output = kzalloc(sizeof(struct mdfld_dsi_dpi_output), GFP_KERNEL);
	if (!dpi_output) {
		DRM_ERROR("No memory\n");
		return NULL;
	}

	dpi_output->dev = dev;
	dpi_output->p_funcs = p_funcs;
	dpi_output->first_boot = 1;
	/*get fixed mode*/
	fixed_mode = dsi_config->fixed_mode;

	/*create drm encoder object*/
	connector = &dsi_connector->base.base;
	encoder = &dpi_output->base.base;
	drm_encoder_init(dev,
			encoder,
			&dsi_dpi_generic_encoder_funcs,
			DRM_MODE_ENCODER_DSI);
	drm_encoder_helper_add(encoder,
			&dsi_dpi_generic_encoder_helper_funcs);

	/*attach to given connector*/
	drm_mode_connector_attach_encoder(connector, encoder);
	connector->encoder = encoder;

	/*set possible crtcs and clones*/
	if (dsi_connector->pipe) {
		encoder->possible_crtcs = (1 << 2);
		encoder->possible_clones = (1 << 1);
	} else {
		encoder->possible_crtcs = (1 << 0);
		encoder->possible_clones = (1 << 0);
	}

	dev_priv->dsr_fb_update = 0;
	dev_priv->b_dsr_enable = false;
	dev_priv->exit_idle = NULL;
#if defined(CONFIG_MDFLD_DSI_DPU) || defined(CONFIG_MDFLD_DSI_DSR)
	dev_priv->b_dsr_enable_config = true;
#endif /*CONFIG_MDFLD_DSI_DSR*/

	PSB_DEBUG_ENTRY("successfully\n");

	return &dpi_output->base;
}
