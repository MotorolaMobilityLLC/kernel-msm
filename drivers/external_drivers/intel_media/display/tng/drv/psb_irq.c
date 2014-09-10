/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
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
 * Intel funded Tungsten Graphics (http://www.tungstengraphics.com) to
 * develop this driver.
 *
 **************************************************************************/
/*
 */

#include <drm/drmP.h>
#include "psb_drv.h"
#include "psb_reg.h"
#include "psb_msvdx.h"
#include "tng_topaz.h"

#ifdef SUPPORT_VSP
#include "vsp.h"
#endif

#include "psb_intel_reg.h"
#include "pwr_mgmt.h"
#include "dc_maxfifo.h"

#include "psb_irq.h"
#include "psb_intel_hdmi.h"

#ifdef CONFIG_SUPPORT_MIPI
#include "mdfld_dsi_dbi_dsr.h"
#include "mdfld_dsi_dbi_dpu.h"
#include "mdfld_dsi_pkg_sender.h"
#endif

#define KEEP_UNUSED_CODE 0

extern struct drm_device *gpDrmDevice;
extern int drm_psb_smart_vsync;
/*
 * inline functions
 */

static inline void update_te_counter(struct drm_device *dev, uint32_t pipe)
{
#ifdef CONFIG_SUPPORT_MIPI
	struct mdfld_dsi_pkg_sender *sender;
	struct mdfld_dsi_dbi_output **dbi_outputs;
	struct mdfld_dsi_dbi_output *dbi_output;
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)dev->dev_private;
	struct mdfld_dbi_dsr_info *dsr_info = dev_priv->dbi_dsr_info;

	if (!dsr_info)
		return;

	dbi_outputs = dsr_info->dbi_outputs;
	dbi_output = pipe ? dbi_outputs[1] : dbi_outputs[0];
	sender = mdfld_dsi_encoder_get_pkg_sender(&dbi_output->base);
	mdfld_dsi_report_te(sender);
#else
	return;
#endif
}

static inline u32 psb_pipestat(int pipe)
{
	if (pipe == 0)
		return PIPEASTAT;
	if (pipe == 1)
		return PIPEBSTAT;
	if (pipe == 2)
		return PIPECSTAT;
	BUG();
	/* This should be unreachable */
	return 0;
}

static inline u32 mid_pipe_event(int pipe)
{
	if (pipe == 0)
		return _PSB_PIPEA_EVENT_FLAG;
	if (pipe == 1)
		return _MDFLD_PIPEB_EVENT_FLAG;
	if (pipe == 2)
		return _MDFLD_PIPEC_EVENT_FLAG;
	BUG();
	/* This should be unreachable */
	return 0;
}

static inline u32 mid_pipe_vsync(int pipe)
{
	if (pipe == 0)
		return _PSB_VSYNC_PIPEA_FLAG;
	if (pipe == 1)
		return _PSB_VSYNC_PIPEB_FLAG;
	if (pipe == 2)
		return _MDFLD_PIPEC_VBLANK_FLAG;
	BUG();
	/* This should be unreachable */
	return 0;
}

static inline u32 mid_pipeconf(int pipe)
{
	if (pipe == 0)
		return PIPEACONF;
	if (pipe == 1)
		return PIPEBCONF;
	if (pipe == 2)
		return PIPECCONF;
	BUG();
	/* This should be unreachable */
	return 0;
}

void psb_enable_pipestat(struct drm_psb_private *dev_priv, int pipe, u32 mask)
{
	struct drm_device *dev = dev_priv->dev;
	u32 write_val;

	u32 reg = psb_pipestat(pipe);
	dev_priv->pipestat[pipe] |= mask;
	/* Enable the interrupt, clear any pending status */
	write_val = PSB_RVDC32(reg);

	write_val |= (mask | (mask >> 16));
	PSB_WVDC32(write_val, reg);
	(void)PSB_RVDC32(reg);
}

void psb_recover_pipestat(struct drm_psb_private *dev_priv, int pipe, u32 mask)
{
	struct drm_device *dev = dev_priv->dev;
	if ((dev_priv->pipestat[pipe] & mask) == mask) {
		u32 reg = psb_pipestat(pipe);
		u32 write_val = PSB_RVDC32(reg);
		write_val |= (mask | (mask >> 16));
		PSB_WVDC32(write_val, reg);
		(void)PSB_RVDC32(reg);
	}
}

void psb_disable_pipestat(struct drm_psb_private *dev_priv, int pipe, u32 mask)
{
	struct drm_device *dev = dev_priv->dev;
	u32 reg = psb_pipestat(pipe);
	u32 write_val;

	dev_priv->pipestat[pipe] &= ~mask;
	write_val = PSB_RVDC32(reg);
	write_val &= ~mask;
	PSB_WVDC32(write_val, reg);
	(void)PSB_RVDC32(reg);
}

void mid_enable_pipe_event(struct drm_psb_private *dev_priv, int pipe)
{
	struct drm_device *dev = dev_priv->dev;
	u32 pipe_event = mid_pipe_event(pipe);
	/* S0i1-Display registers can only be programmed after the pipe events
	 * are disabled. Otherwise MSI from the display controller can reach
	 * the Punit without SCU knowing about it. We have to prevent this
	 * scenario. So, if we about to enable the pipe_event, check if the
	 * we have already entered the S0i1-Display mode. If so clear it
	 * and set the state back to ready.
	 */
	exit_s0i1_display_mode(dev);
	dev_priv->vdc_irq_mask |= pipe_event;
	PSB_WVDC32(~dev_priv->vdc_irq_mask, PSB_INT_MASK_R);
	PSB_WVDC32(dev_priv->vdc_irq_mask, PSB_INT_ENABLE_R);
}

void mid_disable_pipe_event(struct drm_psb_private *dev_priv, int pipe)
{
	struct drm_device *dev = dev_priv->dev;
	if (dev_priv->pipestat[pipe] == 0) {
		u32 pipe_event = mid_pipe_event(pipe);
		dev_priv->vdc_irq_mask &= ~pipe_event;
		PSB_WVDC32(~dev_priv->vdc_irq_mask, PSB_INT_MASK_R);
		PSB_WVDC32(dev_priv->vdc_irq_mask, PSB_INT_ENABLE_R);
		/* S0i1-Display registers can only be programmed after the pipe events
		 * are disabled. Otherwise MSI from the display controller can reach
		 * the Punit without SCU knowing about it. We have to prevent this
		 * scenario. So, if we are disabling the pipe_event, check if are
		 * ready to enter S0i1-Display mode. If so clear it
		 * and set the state back to entered.
		 */
		enter_s0i1_display_mode(dev);
	}
}

#if KEEP_UNUSED_CODE
/**
 * Check if we can disable vblank for video MIPI display
 *
 */
static void mid_check_vblank(struct drm_device *dev, uint32_t pipe)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	unsigned long irqflags;
	static unsigned long cnt = 0;

	if (drm_psb_smart_vsync == 0) {
		if ((cnt++) % 600 == 0) {
			PSB_DEBUG_ENTRY("[vsync irq] 600 times !\n");
		}
		return;
	}

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);
	if (dev_priv->dsr_idle_count > 50)
		dev_priv->b_is_in_idle = true;
	else
		dev_priv->dsr_idle_count++;
	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
}
#endif /* if KEEP_UNUSED_CODE */

u32 intel_vblank_count(struct drm_device *dev, int pipe)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)dev->dev_private;

	return atomic_read(&dev_priv->vblank_count[pipe]);
}

/**
 *  Display controller interrupt handler for vsync/vblank.
 *
 *  Modified to handle the midi to hdmi clone 7/13/2012
 *      williamx.f.schmidt@intel.com
 */
static void mid_vblank_handler(struct drm_device *dev, uint32_t pipe)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;

	if (dev_priv->psb_vsync_handler)
		(*dev_priv->psb_vsync_handler)(dev, pipe);
}

#ifdef CONFIG_SUPPORT_HDMI
/**
 * Display controller interrupt handler for pipe hdmi audio underrun.
 *
 */
void hdmi_do_audio_underrun_wq(struct work_struct *work)
{
	struct drm_psb_private *dev_priv = container_of(work,
			struct drm_psb_private, hdmi_audio_underrun_wq);
	void *had_pvt_data = dev_priv->had_pvt_data;
	enum had_event_type event_type = HAD_EVENT_AUDIO_BUFFER_UNDERRUN;

	if (dev_priv->mdfld_had_event_callbacks)
		(*dev_priv->mdfld_had_event_callbacks)(event_type,
							had_pvt_data);
}

/**
 * Display controller interrupt handler for pipe hdmi audio buffer done.
 *
 */
void hdmi_do_audio_bufferdone_wq(struct work_struct *work)
{
	struct drm_psb_private *dev_priv = container_of(work,
			struct drm_psb_private, hdmi_audio_bufferdone_wq);

	if (dev_priv->mdfld_had_event_callbacks)
		(*dev_priv->mdfld_had_event_callbacks)
		    (HAD_EVENT_AUDIO_BUFFER_DONE, dev_priv->had_pvt_data);
}

/**
 * Display controller tasklet entry function for pipe hdmi audio buffer done.
 *
 */

void hdmi_audio_bufferdone_tasklet_func(unsigned long data)
{
	struct drm_psb_private *dev_priv = (struct drm_psb_private *)data;

	if (dev_priv->mdfld_had_event_callbacks)
		(*dev_priv->mdfld_had_event_callbacks)
		    (HAD_EVENT_AUDIO_BUFFER_DONE, dev_priv->had_pvt_data);
}

#endif

void psb_te_timer_func(unsigned long data)
{
	/*
	   struct drm_psb_private * dev_priv = (struct drm_psb_private *)data;
	   struct drm_device *dev = (struct drm_device *)dev_priv->dev;
	   uint32_t pipe = dev_priv->cur_pipe;
	   drm_handle_vblank(dev, pipe);
	   if( dev_priv->psb_vsync_handler != NULL)
	   (*dev_priv->psb_vsync_handler)(dev,pipe);
	 */
}

void mdfld_vsync_event_work(struct work_struct *work)
{
	struct drm_psb_private *dev_priv =
		container_of(work, struct drm_psb_private, vsync_event_work);
	int pipe = dev_priv->vsync_pipe;
	struct drm_device *dev = dev_priv->dev;

	mid_vblank_handler(dev, pipe);

	/* TODO: to report vsync event to HWC. */
	/*report vsync event*/
	/* mdfld_vsync_event(dev, pipe); */
}

#ifdef CONFIG_SUPPORT_MIPI
void mdfld_te_handler_work(struct work_struct *work)
{
	struct drm_psb_private *dev_priv =
		container_of(work, struct drm_psb_private, te_work);
	int pipe = dev_priv->te_pipe;
	struct drm_device *dev = dev_priv->dev;
	struct mdfld_dsi_config *dsi_config = NULL;

	if (dev_priv->b_async_flip_enable) {
		if (dev_priv->psb_vsync_handler != NULL)
			(*dev_priv->psb_vsync_handler)(dev, pipe);

		dsi_config = (pipe == 0) ? dev_priv->dsi_configs[0] :
			dev_priv->dsi_configs[1];
		mdfld_dsi_dsr_report_te(dsi_config);
	} else {
#ifdef CONFIG_MID_DSI_DPU
		mdfld_dpu_update_panel(dev);
#else
		mdfld_dbi_update_panel(dev, pipe);
#endif

		if (dev_priv->psb_vsync_handler != NULL)
			(*dev_priv->psb_vsync_handler)(dev, pipe);
	}
}
#endif
/**
 * Display controller interrupt handler for pipe event.
 *
 */
static void mid_pipe_event_handler(struct drm_device *dev, uint32_t pipe)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;

	uint32_t pipe_stat_val = 0;
	uint32_t pipe_stat_reg = psb_pipestat(pipe);
	uint32_t pipe_enable = dev_priv->pipestat[pipe];
	uint32_t pipe_status = dev_priv->pipestat[pipe] >> 16;
	unsigned long irq_flags;
	uint32_t read_count = 0;

#ifdef CONFIG_SUPPORT_MIPI
	uint32_t i = 0;
	uint32_t mipi_intr_stat_val = 0;
	uint32_t mipi_intr_stat_reg = 0;
	struct mdfld_dsi_config *dsi_config = NULL;
	struct mdfld_dsi_hw_registers *regs = NULL;
	struct mdfld_dsi_hw_context *ctx = NULL;
	u32 val = 0;
#endif

	spin_lock_irqsave(&dev_priv->irqmask_lock, irq_flags);

#ifdef CONFIG_SUPPORT_MIPI
	if (pipe == MDFLD_PIPE_A)
		mipi_intr_stat_reg = MIPIA_INTR_STAT_REG;

	if ((mipi_intr_stat_reg) && (is_panel_vid_or_cmd(dev) == MDFLD_DSI_ENCODER_DPI)) {
		mipi_intr_stat_val = PSB_RVDC32(mipi_intr_stat_reg);
		PSB_WVDC32(mipi_intr_stat_val, mipi_intr_stat_reg);
		if (mipi_intr_stat_val & DPI_FIFO_UNDER_RUN) {
			dev_priv->pipea_dpi_underrun_count++;
			/* ignore the first dpi underrun after dpi panel power on */
			if (dev_priv->pipea_dpi_underrun_count > 1)
				DRM_INFO("Display pipe A received a DPI_FIFO_UNDER_RUN event\n");
		}

		mipi_intr_stat_reg = MIPIA_INTR_STAT_REG + MIPIC_REG_OFFSET;

		mipi_intr_stat_val = PSB_RVDC32(mipi_intr_stat_reg);
		PSB_WVDC32(mipi_intr_stat_val, mipi_intr_stat_reg);
		if (mipi_intr_stat_val & DPI_FIFO_UNDER_RUN) {
			dev_priv->pipec_dpi_underrun_count++;
			/* ignore the first dpi underrun after dpi panel power on */
			if (dev_priv->pipec_dpi_underrun_count > 1)
				DRM_INFO("Display pipe C received a DPI_FIFO_UNDER_RUN event\n");
		}
	}
#else
	if (pipe != 1) {
		spin_unlock_irqrestore(&dev_priv->irqmask_lock, irq_flags);
		DRM_ERROR("called with other than HDMI PIPE %d\n", pipe);
		return;
	}
#endif
	pipe_stat_val = PSB_RVDC32(pipe_stat_reg);
	/* Sometimes We read 0 from HW - keep reading until we get non-zero */
	while ((!pipe_stat_val) && (read_count < 1000)) {
		pipe_stat_val = REG_READ(pipe_stat_reg);
		read_count ++;
	}

#ifdef CONFIG_SUPPORT_MIPI
#ifdef ENABLE_HW_REPEAT_FRAME
	/* In the case of the repeated frame count interrupt, we need to disable
	   the repeat_frame_count_threshold register as otherwise we keep getting
	   an interrupt
	*/
	if ((pipe == MDFLD_PIPE_A) &&
               (pipe_stat_val & PIPE_REPEATED_FRAME_STATUS)) {
		PSB_DEBUG_PM("Frame count interrupt Before Clearing "
				"PipeAStat Reg=0x%8x Val=0x%8x\n",
			pipe_stat_reg, pipe_stat_val);
		PSB_WVDC32(PIPEA_REPEAT_FRM_CNT_TRESHOLD_DISABLE,
				PIPEA_REPEAT_FRM_CNT_TRESHOLD_REG);
	}
#endif
#endif

	/* clear the 2nd level interrupt status bits */
	PSB_WVDC32(pipe_stat_val, pipe_stat_reg);
	pipe_stat_val &= pipe_enable | pipe_status;
	pipe_stat_val &= pipe_stat_val >> 16;

	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irq_flags);

#ifdef CONFIG_SUPPORT_MIPI
	if ((pipe_stat_val & PIPE_DPST_EVENT_STATUS) &&
	    (dev_priv->psb_dpst_state != NULL)) {
		uint32_t pwm_reg = 0;
		uint32_t hist_reg = 0;
		struct dpst_guardband guardband_reg;
		struct dpst_ie_histogram_control ie_hist_cont_reg;

		hist_reg = PSB_RVDC32(HISTOGRAM_INT_CONTROL);

		/* Determine if this is histogram or pwm interrupt */
		if ((hist_reg & HISTOGRAM_INT_CTRL_CLEAR) &&
				(hist_reg & HISTOGRAM_INTERRUPT_ENABLE)) {
			/* Notify UM of histogram interrupt */
			psb_dpst_notify_change_um(DPST_EVENT_HIST_INTERRUPT,
						  dev_priv->psb_dpst_state);

			/* disable dpst interrupts */
			guardband_reg.data = PSB_RVDC32(HISTOGRAM_INT_CONTROL);
			guardband_reg.interrupt_enable = 0;
			guardband_reg.interrupt_status = 1;
			PSB_WVDC32(guardband_reg.data, HISTOGRAM_INT_CONTROL);

			ie_hist_cont_reg.data = PSB_RVDC32(HISTOGRAM_LOGIC_CONTROL);
			ie_hist_cont_reg.ie_histogram_enable = 0;
			PSB_WVDC32(ie_hist_cont_reg.data, HISTOGRAM_LOGIC_CONTROL);
		}
		pwm_reg = PSB_RVDC32(PWM_CONTROL_LOGIC);
		if ((pwm_reg & PWM_PHASEIN_INT_ENABLE) &&
		    !(pwm_reg & PWM_PHASEIN_ENABLE)) {
			/* Notify UM of the phase complete */
			psb_dpst_notify_change_um(DPST_EVENT_PHASE_COMPLETE,
						  dev_priv->psb_dpst_state);

			/* Temporarily get phase mngr ready to generate
			 * another interrupt until this can be moved to
			 * user mode */
			/* PSB_WVDC32(pwm_reg | 0x80010100 | PWM_PHASEIN_ENABLE,
			   PWM_CONTROL_LOGIC); */
		}
	}
#endif

	if (pipe_stat_val & PIPE_VBLANK_STATUS) {
		dev_priv->vsync_pipe = pipe;
		drm_handle_vblank(dev, pipe);
		queue_work(dev_priv->vsync_wq, &dev_priv->vsync_event_work);
	}

#ifdef CONFIG_SUPPORT_MIPI
	if (pipe_stat_val & PIPE_TE_STATUS) {
		dev_priv->te_pipe = pipe;
		update_te_counter(dev, pipe);
		drm_handle_vblank(dev, pipe);
		queue_work(dev_priv->vsync_wq, &dev_priv->te_work);
	}

	if (pipe == drm_psb_set_gamma_pipe && drm_psb_set_gamma_pending) {
		dsi_config = dev_priv->dsi_configs[pipe];
		regs = &dsi_config->regs;
		ctx = &dsi_config->dsi_hw_context;

		for (i = 0; i < 256; i++)
			REG_WRITE(regs->palette_reg + i*4, gamma_setting_save[i] );

		val = REG_READ(regs->pipeconf_reg);
		val |= (PIPEACONF_GAMMA);
		REG_WRITE(regs->pipeconf_reg, val);
		ctx->pipeconf = val;
		REG_WRITE(regs->dspcntr_reg, REG_READ(regs->dspcntr_reg) |
				DISPPLANE_GAMMA_ENABLE);
		ctx->dspcntr = REG_READ(regs->dspcntr_reg) | DISPPLANE_GAMMA_ENABLE;
		REG_READ(regs->dspcntr_reg);
		drm_psb_set_gamma_pending = 0 ;
		drm_psb_set_gamma_pipe = MDFLD_PIPE_MAX;
	}

	if (pipe == 0) { /* only for pipe A */
		if (pipe_stat_val & PIPE_FRAME_DONE_STATUS)
			wake_up_interruptible(&dev_priv->eof_wait);
	}

#ifdef ENABLE_HW_REPEAT_FRAME
	if ((pipe == MDFLD_PIPE_A) &&
               (pipe_stat_val & PIPE_REPEATED_FRAME_STATUS)) {
		maxfifo_report_repeat_frame_interrupt(dev);
	}
#endif
#endif

#ifdef CONFIG_SUPPORT_HDMI
	if (pipe == 1) { /* HDMI is only on PIPE B */
		if (pipe_stat_val & PIPE_HDMI_AUDIO_UNDERRUN_STATUS)
			schedule_work(&dev_priv->hdmi_audio_underrun_wq);

		if (pipe_stat_val & PIPE_HDMI_AUDIO_BUFFER_DONE_STATUS)
			tasklet_schedule(&dev_priv->hdmi_audio_bufferdone_tasklet);
	}
#endif
}

/**
 * Display controller interrupt handler.
 */
static void psb_vdc_interrupt(struct drm_device *dev, uint32_t vdc_stat)
{

	if (vdc_stat & _PSB_PIPEA_EVENT_FLAG) {
		mid_pipe_event_handler(dev, 0);
	}

	if (vdc_stat & _MDFLD_PIPEB_EVENT_FLAG) {
		mid_pipe_event_handler(dev, 1);
	}

	if (vdc_stat & _MDFLD_PIPEC_EVENT_FLAG) {
		mid_pipe_event_handler(dev, 2);
	}
}

/**
 * Registration function for RGX irq handler
 * When we get a RGX irq, we call the handler function
 * We do not want RGX to register their own irq_handler
 * since they would be getting interrupted for all
 * gunit interrupts (display controller, video encoder,
 * video decoder etc)
 * This is because we just have a single PCI device for
 * all of "graphics".
 */

void register_rgx_irq_handler(int (*pfn_rgxIrqHandler) (void *), void * pData)
{
	if (gpDrmDevice){
		struct drm_psb_private *dev_priv =
		    (struct drm_psb_private *)gpDrmDevice->dev_private;
		dev_priv->pfn_rgxIrqHandler = pfn_rgxIrqHandler;
		dev_priv->prgx_irqData = pData;
	}
}

irqreturn_t psb_irq_handler(DRM_IRQ_ARGS)
{
	struct drm_device *dev = (struct drm_device *)arg;
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;

	uint32_t vdc_stat, dsp_int = 0, sgx_int = 0, msvdx_int = 0;
	uint32_t topaz_int = 0, vsp_int = 0;
	int handled = 0;
	unsigned long irq_flags;

	/*      PSB_DEBUG_ENTRY("\n"); */

	spin_lock_irqsave(&dev_priv->irqmask_lock, irq_flags);

	vdc_stat = PSB_RVDC32(PSB_INT_IDENTITY_R);

	if (vdc_stat & _MDFLD_DISP_ALL_IRQ_FLAG) {
		PSB_DEBUG_IRQ("Got DISP interrupt\n");
		dsp_int = 1;
	}

	if (vdc_stat & _PSB_IRQ_SGX_FLAG) {
		PSB_DEBUG_IRQ("Got SGX interrupt\n");
		sgx_int = 1;
	}
	if (vdc_stat & _PSB_IRQ_MSVDX_FLAG) {
		PSB_DEBUG_IRQ("Got MSVDX interrupt\n");
		msvdx_int = 1;
	}

	if (vdc_stat & _LNC_IRQ_TOPAZ_FLAG) {
		PSB_DEBUG_IRQ("Got TOPAX interrupt\n");
		topaz_int = 1;
	}

	if (vdc_stat & _TNG_IRQ_VSP_FLAG) {
		PSB_DEBUG_IRQ("Got VSP interrupt\n");
		vsp_int = 1;
	}

	vdc_stat &= dev_priv->vdc_irq_mask;
	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irq_flags);

	if (dsp_int && ospm_power_is_hw_on(OSPM_DISPLAY_ISLAND)) {
		psb_vdc_interrupt(dev, vdc_stat);
		handled = 1;
	}

	if (msvdx_int && (IS_FLDS(dev)
			  || ospm_power_is_hw_on(OSPM_VIDEO_DEC_ISLAND))) {
		psb_msvdx_interrupt(dev);
		handled = 1;
	}
	if ((topaz_int)) {
		tng_topaz_interrupt(dev);
		handled = 1;
	}
#ifdef SUPPORT_VSP
	if (vsp_int) {
		vsp_interrupt(dev);
		handled = 1;
		vdc_stat &= ~_TNG_IRQ_VSP_FLAG;
	}
#endif

	if (sgx_int) {
		if (dev_priv->pfn_rgxIrqHandler){
			handled = dev_priv->pfn_rgxIrqHandler(
					dev_priv->prgx_irqData) ? 1 : 0;
		}
	}

	PSB_WVDC32(vdc_stat, PSB_INT_IDENTITY_R);
	(void)PSB_RVDC32(PSB_INT_IDENTITY_R);
	DRM_READMEMORYBARRIER();

	if (!handled)
		return IRQ_NONE;

	return IRQ_HANDLED;
}

void psb_irq_preinstall(struct drm_device *dev)
{
	psb_irq_preinstall_islands(dev, OSPM_ALL_ISLANDS);
}

/**
 * FIXME: should I remove display irq enable here??
 */
void psb_irq_preinstall_islands(struct drm_device *dev, int hw_islands)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	unsigned long irqflags;

	PSB_DEBUG_IRQ("\n");

	if (dev_priv->b_dsr_enable)
		dev_priv->exit_idle(dev, MDFLD_DSR_2D_3D, 0, true);

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);

	if (hw_islands & OSPM_DISPLAY_ISLAND) {
		if (ospm_power_is_hw_on(OSPM_DISPLAY_ISLAND)) {
			if (dev->vblank_enabled[0])
				dev_priv->vdc_irq_mask |= _PSB_PIPEA_EVENT_FLAG;
			if (dev->vblank_enabled[1])
				dev_priv->vdc_irq_mask |=
				    _MDFLD_PIPEB_EVENT_FLAG;
			if (dev->vblank_enabled[2])
				dev_priv->vdc_irq_mask |=
				    _MDFLD_PIPEC_EVENT_FLAG;
		}
	}
	if (hw_islands & OSPM_GRAPHICS_ISLAND) {
		dev_priv->vdc_irq_mask |= _PSB_IRQ_SGX_FLAG;
	}

	if (hw_islands & OSPM_VIDEO_DEC_ISLAND)
		if (IS_MID(dev) && ospm_power_is_hw_on(OSPM_VIDEO_DEC_ISLAND))
			dev_priv->vdc_irq_mask |= _PSB_IRQ_MSVDX_FLAG;

	if (hw_islands & OSPM_VIDEO_ENC_ISLAND)
		if (IS_MID(dev) && ospm_power_is_hw_on(OSPM_VIDEO_ENC_ISLAND))
			dev_priv->vdc_irq_mask |= _LNC_IRQ_TOPAZ_FLAG;

	if (hw_islands & OSPM_VIDEO_VPP_ISLAND)
		if (IS_MID(dev) && ospm_power_is_hw_on(OSPM_VIDEO_VPP_ISLAND))
			dev_priv->vdc_irq_mask |= _TNG_IRQ_VSP_FLAG;

	/*This register is safe even if display island is off*/
	PSB_WVDC32(~dev_priv->vdc_irq_mask, PSB_INT_MASK_R);

	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
}

int psb_irq_postinstall(struct drm_device *dev)
{
	return psb_irq_postinstall_islands(dev, OSPM_ALL_ISLANDS);
}

int psb_irq_postinstall_islands(struct drm_device *dev, int hw_islands)
{

	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	unsigned long irqflags;

	PSB_DEBUG_IRQ("\n");

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);

	/*This register is safe even if display island is off */
	PSB_WVDC32(dev_priv->vdc_irq_mask, PSB_INT_ENABLE_R);

	if (IS_MID(dev) && !dev_priv->topaz_disabled)
		if (hw_islands & OSPM_VIDEO_ENC_ISLAND)
			if (ospm_power_is_hw_on(OSPM_VIDEO_ENC_ISLAND)) {
				tng_topaz_enableirq(dev);

			}

	if (hw_islands & OSPM_VIDEO_DEC_ISLAND)
		psb_msvdx_enableirq(dev);

#ifdef SUPPORT_VSP
	if (hw_islands & OSPM_VIDEO_VPP_ISLAND)
		vsp_enableirq(dev);
#endif

	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);

	return 0;
}

void psb_irq_uninstall(struct drm_device *dev)
{
	psb_irq_uninstall_islands(dev, OSPM_ALL_ISLANDS);
}

void psb_irq_uninstall_islands(struct drm_device *dev, int hw_islands)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	unsigned long irqflags;

	PSB_DEBUG_IRQ("\n");

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);

	if (hw_islands & OSPM_DISPLAY_ISLAND)
		dev_priv->vdc_irq_mask &= _PSB_IRQ_SGX_FLAG |
					  _PSB_IRQ_MSVDX_FLAG |
					  _LNC_IRQ_TOPAZ_FLAG |
					  _TNG_IRQ_VSP_FLAG;

	/*TODO: remove follwoing code */
	if (hw_islands & OSPM_GRAPHICS_ISLAND) {
		dev_priv->vdc_irq_mask &= ~_PSB_IRQ_SGX_FLAG;
	}

	if ((hw_islands & OSPM_VIDEO_DEC_ISLAND) && IS_MID(dev))
		dev_priv->vdc_irq_mask &= ~_PSB_IRQ_MSVDX_FLAG;

	if ((hw_islands & OSPM_VIDEO_ENC_ISLAND) && IS_MID(dev))
		dev_priv->vdc_irq_mask &= ~_LNC_IRQ_TOPAZ_FLAG;

	if ((hw_islands & OSPM_VIDEO_VPP_ISLAND) && IS_MID(dev))
		dev_priv->vdc_irq_mask &= ~_TNG_IRQ_VSP_FLAG;

	/*These two registers are safe even if display island is off*/
	PSB_WVDC32(~dev_priv->vdc_irq_mask, PSB_INT_MASK_R);
	PSB_WVDC32(dev_priv->vdc_irq_mask, PSB_INT_ENABLE_R);

	wmb();

	/*This register is safe even if display island is off */
	PSB_WVDC32(PSB_RVDC32(PSB_INT_IDENTITY_R), PSB_INT_IDENTITY_R);

	if (IS_MID(dev) && !dev_priv->topaz_disabled)
		if (hw_islands & OSPM_VIDEO_ENC_ISLAND)
			if (ospm_power_is_hw_on(OSPM_VIDEO_ENC_ISLAND)) {
				tng_topaz_disableirq(dev);
			}

	if (hw_islands & OSPM_VIDEO_DEC_ISLAND)
		if (ospm_power_is_hw_on(OSPM_VIDEO_DEC_ISLAND))
			psb_msvdx_disableirq(dev);

#ifdef SUPPORT_VSP
	if (hw_islands & OSPM_VIDEO_VPP_ISLAND)
		if (ospm_power_is_hw_on(OSPM_VIDEO_VPP_ISLAND))
			vsp_disableirq(dev);
#endif
	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
}

void psb_irq_turn_on_dpst(struct drm_device *dev)
{
#ifdef CONFIG_SUPPORT_MIPI
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	struct mdfld_dsi_config *dsi_config = NULL;
	struct mdfld_dsi_hw_context *ctx = NULL;
	unsigned long irqflags;

	if(!dev_priv)
		return;

	dsi_config = dev_priv->dsi_configs[0];
	if(!dsi_config)
		return;

	ctx = &dsi_config->dsi_hw_context;

	/* TODO: use DPST spinlock */
	/* FIXME: revisit the power island when touching the DPST feature. */
	if (power_island_get(OSPM_DISPLAY_A)) {

		PSB_WVDC32(ctx->histogram_logic_ctrl, HISTOGRAM_LOGIC_CONTROL);
		PSB_WVDC32(ctx->histogram_intr_ctrl, HISTOGRAM_INT_CONTROL);

		spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);
		psb_enable_pipestat(dev_priv, 0, PIPE_DPST_EVENT_ENABLE);
		spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
/*
		PSB_WVDC32(BIT31, HISTOGRAM_LOGIC_CONTROL);
		hist_reg = PSB_RVDC32(HISTOGRAM_LOGIC_CONTROL);
		ctx->histogram_logic_ctrl = hist_reg;
		PSB_WVDC32(BIT31, HISTOGRAM_INT_CONTROL);
		hist_reg = PSB_RVDC32(HISTOGRAM_INT_CONTROL);
		ctx->histogram_intr_ctrl = hist_reg;

		PSB_WVDC32(0x80010100, PWM_CONTROL_LOGIC);
		pwm_reg = PSB_RVDC32(PWM_CONTROL_LOGIC);
		PSB_WVDC32(pwm_reg | PWM_PHASEIN_ENABLE |
			   PWM_PHASEIN_INT_ENABLE, PWM_CONTROL_LOGIC);
		pwm_reg = PSB_RVDC32(PWM_CONTROL_LOGIC);

		hist_reg = PSB_RVDC32(HISTOGRAM_INT_CONTROL);
		PSB_WVDC32(hist_reg | HISTOGRAM_INT_CTRL_CLEAR,
			   HISTOGRAM_INT_CONTROL);
		pwm_reg = PSB_RVDC32(PWM_CONTROL_LOGIC);
		PSB_WVDC32(pwm_reg | 0x80010100 | PWM_PHASEIN_ENABLE,
			   PWM_CONTROL_LOGIC);
*/

		power_island_put(OSPM_DISPLAY_A);
	}
#else
	return;
#endif
}

int psb_irq_enable_dpst(struct drm_device *dev)
{
	/* enable DPST */
	//mid_enable_pipe_event(dev_priv, 0);
	psb_irq_turn_on_dpst(dev);

	return 0;
}

void psb_irq_turn_off_dpst(struct drm_device *dev)
{
#ifdef CONFIG_SUPPORT_MIPI
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	struct mdfld_dsi_config *dsi_config = NULL;
	unsigned long irqflags;

	if (!dev_priv)
		return;
	dsi_config = dev_priv->dsi_configs[0];
	if(!dsi_config)
		return;

	/* TODO: use DPST spinlock */
	/* FIXME: revisit the power island when touching the DPST feature. */
	if (power_island_get(OSPM_DISPLAY_A)) {

               PSB_WVDC32(PSB_RVDC32(HISTOGRAM_INT_CONTROL) & 0x7fffffff,
			HISTOGRAM_INT_CONTROL);

		spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);
		psb_disable_pipestat(dev_priv, 0, PIPE_DPST_EVENT_ENABLE);
		spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);

/*
		pwm_reg = PSB_RVDC32(PWM_CONTROL_LOGIC);
		PSB_WVDC32(pwm_reg & !(PWM_PHASEIN_INT_ENABLE),
			   PWM_CONTROL_LOGIC);
		pwm_reg = PSB_RVDC32(PWM_CONTROL_LOGIC);
*/

		power_island_put(OSPM_DISPLAY_A);
	}
#else
	return;
#endif
}

int psb_irq_disable_dpst(struct drm_device *dev)
{
	//mid_disable_pipe_event(dev_priv, 0);
	psb_irq_turn_off_dpst(dev);

	return 0;
}

#ifdef PSB_FIXME
static int psb_vblank_do_wait(struct drm_device *dev,
			      unsigned int *sequence, atomic_t * counter)
{
	unsigned int cur_vblank;
	int ret = 0;
	DRM_WAIT_ON(ret, dev->vbl_queue, 3 * DRM_HZ,
		    (((cur_vblank = atomic_read(counter))
		      - *sequence) <= (1 << 23)));
	*sequence = cur_vblank;

	return ret;
}
#endif

/*
 * It is used to enable VBLANK interrupt
 */
int psb_enable_vblank(struct drm_device *dev, int pipe)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	unsigned long irqflags;
	uint32_t reg_val = 0;
	uint32_t pipeconf_reg = mid_pipeconf(pipe);
#ifdef CONFIG_SUPPORT_MIPI
	mdfld_dsi_encoder_t encoder_type;

	PSB_DEBUG_ENTRY("\n");

	encoder_type = is_panel_vid_or_cmd(dev);
	if (IS_MRFLD(dev) && (encoder_type == MDFLD_DSI_ENCODER_DBI) &&
			(pipe != 1))
		return mdfld_enable_te(dev, pipe);
#else
	if (pipe != 1)
		return -EINVAL;
#endif
	reg_val = REG_READ(pipeconf_reg);

	if (!(reg_val & PIPEACONF_ENABLE)) {
		DRM_ERROR("%s: pipe %d is disabled %#x\n",
			  __func__, pipe, reg_val);
		return -EINVAL;
	}

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);

	mid_enable_pipe_event(dev_priv, pipe);
	psb_enable_pipestat(dev_priv, pipe, PIPE_VBLANK_INTERRUPT_ENABLE);

	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
	PSB_DEBUG_ENTRY("%s: Enabled VBlank for pipe %d\n", __func__, pipe);

	return 0;
}

/*
 * It is used to disable VBLANK interrupt
 */
void psb_disable_vblank(struct drm_device *dev, int pipe)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	unsigned long irqflags;
#ifdef CONFIG_SUPPORT_MIPI
	mdfld_dsi_encoder_t encoder_type;

	PSB_DEBUG_ENTRY("\n");

	encoder_type = is_panel_vid_or_cmd(dev);
	if (IS_MRFLD(dev) && (encoder_type == MDFLD_DSI_ENCODER_DBI) &&
			(pipe != 1)) {
		mdfld_disable_te(dev, pipe);
		return;
	}
#else
	if (pipe != 1)
		return;
#endif
	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);

	psb_disable_pipestat(dev_priv, pipe, PIPE_VBLANK_INTERRUPT_ENABLE);
	mid_disable_pipe_event(dev_priv, pipe);

	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
	PSB_DEBUG_ENTRY("%s: Disabled VBlank for pipe %d\n", __func__, pipe);
}

/* Called from drm generic code, passed a 'crtc', which
 * we use as a pipe index
 */
u32 psb_get_vblank_counter(struct drm_device *dev, int pipe)
{
	uint32_t high_frame = PIPEAFRAMEHIGH;
	uint32_t low_frame = PIPEAFRAMEPIXEL;
	uint32_t pipeconf_reg = PIPEACONF;
	uint32_t reg_val = 0;
	uint32_t high1 = 0, high2 = 0, low = 0, count = 0;

	switch (pipe) {
	case 0:
		break;
	case 1:
		high_frame = PIPEBFRAMEHIGH;
		low_frame = PIPEBFRAMEPIXEL;
		pipeconf_reg = PIPEBCONF;
		break;
	case 2:
		high_frame = PIPECFRAMEHIGH;
		low_frame = PIPECFRAMEPIXEL;
		pipeconf_reg = PIPECCONF;
		break;
	default:
		DRM_ERROR("%s, invalded pipe.\n", __func__);
		return 0;
	}

	reg_val = REG_READ(pipeconf_reg);

	if (!(reg_val & PIPEACONF_ENABLE)) {
		DRM_DEBUG("trying to get vblank count for disabled pipe %d\n",
			  pipe);
		goto psb_get_vblank_counter_exit;
	}

	/* we always get 0 reading these two registers on MOFD
	 * and reading these registers can causes UI freeze
	 * when connected with HDMI using 640x480p / 720x480p
	 */
	if (IS_MOFD(dev))
		return 0;

	/*
	 * High & low register fields aren't synchronized, so make sure
	 * we get a low value that's stable across two reads of the high
	 * register.
	 */
	do {
		high1 = ((REG_READ(high_frame) & PIPE_FRAME_HIGH_MASK) >>
			 PIPE_FRAME_HIGH_SHIFT);
		low = ((REG_READ(low_frame) & PIPE_FRAME_LOW_MASK) >>
		       PIPE_FRAME_LOW_SHIFT);
		high2 = ((REG_READ(high_frame) & PIPE_FRAME_HIGH_MASK) >>
			 PIPE_FRAME_HIGH_SHIFT);
	} while (high1 != high2);

	count = (high1 << 8) | low;

 psb_get_vblank_counter_exit:

	return count;
}

int intel_get_vblank_timestamp(struct drm_device *dev, int pipe,
		int *max_error,
		struct timeval *vblank_time,
		unsigned flags)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)dev->dev_private;
	struct drm_crtc *crtc;

	if (pipe < 0 || pipe >= dev_priv->num_pipe) {
		DRM_ERROR("Invalid crtc %d\n", pipe);
		return -EINVAL;
	}

	/* Get drm_crtc to timestamp: */
	crtc = psb_intel_get_crtc_from_pipe(dev, pipe);
	if (crtc == NULL) {
		DRM_ERROR("Invalid crtc %d\n", pipe);
		return -EINVAL;
	}

	if (!crtc->enabled) {
		DRM_DEBUG_KMS("crtc %d is disabled\n", pipe);
		return -EBUSY;
	}

	/* Helper routine in DRM core does all the work: */
	return drm_calc_vbltimestamp_from_scanoutpos(dev, pipe, max_error,
			vblank_time, flags,
			crtc);
}

int intel_get_crtc_scanoutpos(struct drm_device *dev, int pipe,
		int *vpos, int *hpos)
{
	u32 vbl = 0, position = 0;
	int vbl_start, vbl_end, vtotal;
	bool in_vbl = true;
	int pipeconf_reg = PIPEACONF;
	int vtot_reg = VTOTAL_A;
	int dsl_reg = PIPEADSL;
	int vblank_reg = VBLANK_A;
	int ret = 0;

	switch (pipe) {
	case 0:
		break;
	case 1:
		pipeconf_reg = PIPEBCONF;
		vtot_reg = VTOTAL_B;
		dsl_reg = PIPEBDSL;
		vblank_reg = VBLANK_B;
		break;
	case 2:
		pipeconf_reg = PIPECCONF;
		vtot_reg = VTOTAL_C;
		dsl_reg = PIPECDSL;
		vblank_reg = VBLANK_C;
		break;
	default:
		DRM_ERROR("Illegal Pipe Number.\n");
		return 0;
	}

	if (!REG_READ(pipeconf_reg)) {
		DRM_DEBUG_DRIVER("Failed to get scanoutpos in pipe %d\n", pipe);
		return 0;
	}

	/* Get vtotal. */
	vtotal = 1 + ((REG_READ(vtot_reg) >> 16) & 0x1fff);

	position = REG_READ(dsl_reg);

	/*
	 * Decode into vertical scanout position. Don't have
	 * horizontal scanout position.
	 */
	*vpos = position & 0x1fff;
	*hpos = 0;

	/* Query vblank area. */
	vbl = REG_READ(vblank_reg);

	/* Test position against vblank region. */
	vbl_start = vbl & 0x1fff;
	vbl_end = (vbl >> 16) & 0x1fff;

	if ((*vpos < vbl_start) || (*vpos > vbl_end))
		in_vbl = false;

	/* Inside "upper part" of vblank area? Apply corrective offset: */
	if (in_vbl && (*vpos >= vbl_start))
		*vpos = *vpos - vtotal;

	/* Readouts valid? */
	if (vbl > 0)
		ret |= DRM_SCANOUTPOS_VALID | DRM_SCANOUTPOS_ACCURATE;

	/* In vblank? */
	if (in_vbl)
		ret |= DRM_SCANOUTPOS_INVBL;

	return ret;
}

/*
 * It is used to enable TE interrupt
 */
int mdfld_enable_te(struct drm_device *dev, int pipe)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	unsigned long irqflags;
	uint32_t pipeconf_reg = mid_pipeconf(pipe);
	uint32_t retry = 0;

	while ((REG_READ(pipeconf_reg) & PIPEACONF_ENABLE) == 0) {
		retry++;
		if (retry > 10) {
			DRM_ERROR("%s: pipe %d is disabled\n", __func__, pipe);
			return -EINVAL;
		}
		udelay(3);
	}
	if (retry != 0)
		DRM_INFO("Take %d retries to get pipe %d config register\n", retry, pipe);

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);

	mid_enable_pipe_event(dev_priv, pipe);
	psb_enable_pipestat(dev_priv, pipe, PIPE_TE_ENABLE);
	psb_enable_pipestat(dev_priv, pipe, PIPE_FRAME_DONE_ENABLE);

	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
	PSB_DEBUG_ENTRY("%s: Enabled TE for pipe %d\n", __func__, pipe);

	return 0;
}

/*
 * It is used to recover TE interrupt in case pysical stat mismatch with logical stat
 */
int mdfld_recover_te(struct drm_device *dev, int pipe)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	unsigned long irqflags;
	uint32_t reg_val = 0;
	uint32_t pipeconf_reg = mid_pipeconf(pipe);

	reg_val = REG_READ(pipeconf_reg);

	if (!(reg_val & PIPEACONF_ENABLE))
		return -EINVAL;

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);

	psb_recover_pipestat(dev_priv, pipe, PIPE_TE_ENABLE);

	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);

	return 0;
}

/*
 * It is used to disable TE interrupt
 */
void mdfld_disable_te(struct drm_device *dev, int pipe)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	unsigned long irqflags;

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);

	mid_disable_pipe_event(dev_priv, pipe);
	psb_disable_pipestat(dev_priv, pipe, PIPE_FRAME_DONE_ENABLE);
	psb_disable_pipestat(dev_priv, pipe, 
		(PIPE_TE_ENABLE | PIPE_DPST_EVENT_ENABLE));

	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
	PSB_DEBUG_ENTRY("%s: Disabled TE for pipe %d\n", __func__, pipe);
}

#ifdef ENABLE_HW_REPEAT_FRAME
int mrfl_enable_repeat_frame_intr(struct drm_device *dev, int idle_frame_count)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	unsigned long irqflags;

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);

	/* Disable first to restart the count. This is for the
	 * scenario that we are changing the treshold count and the
	 * new count is lower than the new count - we don't want
	 * interrupt immediately
	 */
	PSB_WVDC32(PIPEA_CALCULATE_CRC_DISABLE, PIPEA_CALCULATE_CRC_REG);
	PSB_WVDC32(PIPEA_REPEAT_FRM_CNT_TRESHOLD_DISABLE,
			PIPEA_REPEAT_FRM_CNT_TRESHOLD_REG);
	/*Enable the CRC calculation*/
	PSB_WVDC32(PIPEA_CALCULATE_CRC_ENABLE, PIPEA_CALCULATE_CRC_REG);

	mid_enable_pipe_event(dev_priv, MDFLD_PIPE_A);
	/*Enable receiving the interrupt through pipestat register*/
	psb_enable_pipestat(dev_priv, MDFLD_PIPE_A, PIPE_REPEATED_FRAME_ENABLE);

	PSB_WVDC32(PIPEA_REPEAT_FRM_CNT_TRESHOLD_ENABLE | idle_frame_count,
		PIPEA_REPEAT_FRM_CNT_TRESHOLD_REG);

	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
	PSB_DEBUG_PM("Enabled Repeat Frame Interrupt\n");
	return 0;
}

void mrfl_disable_repeat_frame_intr(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	unsigned long irqflags;

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);
	PSB_WVDC32(PIPEA_CALCULATE_CRC_DISABLE, PIPEA_CALCULATE_CRC_REG);
	PSB_WVDC32(PIPEA_REPEAT_FRM_CNT_TRESHOLD_DISABLE,
			PIPEA_REPEAT_FRM_CNT_TRESHOLD_REG);
	psb_disable_pipestat(dev_priv, MDFLD_PIPE_A, PIPE_REPEATED_FRAME_ENABLE);
	PSB_WVDC32(PIPEA_CALCULATE_CRC_DISABLE, PIPEA_CALCULATE_CRC_REG);


	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
	PSB_DEBUG_PM("Disabled Repeat Frame Interrupt\n");
}
#endif

int mid_irq_enable_hdmi_audio(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	unsigned long irqflags;
	u32 reg_val = 0, mask = 0;

	reg_val = REG_READ(PIPEBCONF);
	if (!(reg_val & PIPEACONF_ENABLE))
		return -EINVAL;

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);

	/* enable HDMI audio interrupt */
	mid_enable_pipe_event(dev_priv, 1);
	dev_priv->pipestat[1] &= ~PIPE_HDMI_AUDIO_INT_MASK;
	mask = dev_priv->hdmi_audio_interrupt_mask;
	psb_enable_pipestat(dev_priv, 1, mask);

	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
	return 0;
}

int mid_irq_disable_hdmi_audio(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	unsigned long irqflags;

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);

	mid_disable_pipe_event(dev_priv, 1);
	psb_disable_pipestat(dev_priv, 1, PIPE_HDMI_AUDIO_INT_MASK);

	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
	return 0;
}
