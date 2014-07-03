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
#include "pnw_topaz.h"
#include "psb_intel_reg.h"
#include "psb_powermgmt.h"

#include "mdfld_dsi_dbi_dpu.h"
#include "mdfld_dsi_pkg_sender.h"
#include "mdfld_dsi_dbi_dsr.h"

#ifdef CONFIG_MDFD_GL3
#include "mdfld_gl3.h"
#endif

#include "psb_irq.h"

#include <linux/time.h>
#include <linux/history_record.h>

extern int drm_psb_smart_vsync;
extern atomic_t g_videoenc_access_count;
extern atomic_t g_videodec_access_count;
extern struct workqueue_struct *te_wq;
extern struct workqueue_struct *vsync_wq;

/* inline functions */
static inline u32
psb_pipestat(int pipe)
{
	if (pipe == 0)
		return PIPEASTAT;
	if (pipe == 1)
		return PIPEBSTAT;
	if (pipe == 2)
		return PIPECSTAT;
	BUG();
	return 0;
}

static inline u32
mid_pipe_event(int pipe)
{
	if (pipe == 0)
		return _PSB_PIPEA_EVENT_FLAG;
	if (pipe == 1)
		return _MDFLD_PIPEB_EVENT_FLAG;
	if (pipe == 2)
		return _MDFLD_PIPEC_EVENT_FLAG;
	BUG();
	return 0;
}

static inline u32
mid_pipe_vsync(int pipe)
{
	if (pipe == 0)
		return _PSB_VSYNC_PIPEA_FLAG;
	if (pipe == 1)
		return _PSB_VSYNC_PIPEB_FLAG;
	if (pipe == 2)
		return _MDFLD_PIPEC_VBLANK_FLAG;
	BUG();
	return 0;
}

static inline u32
mid_pipeconf(int pipe)
{
	if (pipe == 0)
		return PIPEACONF;
	if (pipe == 1)
		return PIPEBCONF;
	if (pipe == 2)
		return PIPECCONF;
	BUG();
	return 0;
}

void
psb_enable_pipestat(struct drm_psb_private *dev_priv, int pipe, u32 mask)
{
	if ((dev_priv->pipestat[pipe] & mask) != mask) {
		u32 reg = psb_pipestat(pipe);
		dev_priv->pipestat[pipe] |= mask;
		/* Enable the interrupt, clear any pending status */
		if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, OSPM_UHB_ONLY_IF_ON)) {
			u32 writeVal = PSB_RVDC32(reg);
			/* Don't clear other interrupts */
			writeVal &= (PIPE_EVENT_MASK | PIPE_VBLANK_MASK);
			writeVal |= (mask | (mask >> 16));
			PSB_WVDC32(writeVal, reg);
			(void) PSB_RVDC32(reg);
			ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
		}
	}
}

void
psb_disable_pipestat(struct drm_psb_private *dev_priv, int pipe, u32 mask)
{
	if ((dev_priv->pipestat[pipe] & mask) != 0) {
		u32 reg = psb_pipestat(pipe);
		dev_priv->pipestat[pipe] &= ~mask;
		if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, OSPM_UHB_ONLY_IF_ON)) {
			if ((mask == PIPE_VBLANK_INTERRUPT_ENABLE) ||
					(mask == PIPE_TE_ENABLE))
				wake_up_interruptible(&dev_priv->vsync_queue);

			u32 writeVal = PSB_RVDC32(reg);
			/* Don't clear other interrupts */
			writeVal &= (PIPE_EVENT_MASK | PIPE_VBLANK_MASK);
			writeVal &= ~mask;
			PSB_WVDC32(writeVal, reg);
			(void) PSB_RVDC32(reg);
			ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
		}
	}
}

void
mid_enable_pipe_event(struct drm_psb_private *dev_priv, int pipe)
{
	if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, OSPM_UHB_ONLY_IF_ON)) {
		u32 pipe_event = mid_pipe_event(pipe);
		dev_priv->vdc_irq_mask |= pipe_event;
		PSB_WVDC32(~dev_priv->vdc_irq_mask, PSB_INT_MASK_R);
		PSB_WVDC32(dev_priv->vdc_irq_mask, PSB_INT_ENABLE_R);
		ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	}
}

void
mid_disable_pipe_event(struct drm_psb_private *dev_priv, int pipe)
{
	if (dev_priv->pipestat[pipe] == 0) {
		if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, OSPM_UHB_ONLY_IF_ON)) {
			u32 pipe_event = mid_pipe_event(pipe);
			dev_priv->vdc_irq_mask &= ~pipe_event;
			PSB_WVDC32(~dev_priv->vdc_irq_mask, PSB_INT_MASK_R);
			PSB_WVDC32(dev_priv->vdc_irq_mask, PSB_INT_ENABLE_R);
			ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
		}
	}
}
/*
 * to sync the mipi and hdmi irq to ensure they flush the same buffer
 *
 */
static int mipi_hdmi_vsync_check(struct drm_device *dev, uint32_t pipe)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	struct mdfld_dsi_config *dsi_config = dev_priv->dsi_configs[0];
	static int pipe_surf[2];
	int pipea_stat = 0;
	int pipeb_stat = 0;
	int pipeb_ctl = 0;
	int pipeb_cntr = 0;
	unsigned long irqflags;

	/*check whether need to sync*/
	if (dev_priv->vsync_te_working[0] == false ||
		dev_priv->vsync_te_working[1] == false)
		return 1;

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);
	if (dev_priv->bhdmiconnected && dsi_config->dsi_hw_context.panel_on) {
		if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, OSPM_UHB_ONLY_IF_ON)) {
			pipea_stat = REG_READ(psb_pipestat(0));
			pipeb_stat = REG_READ(psb_pipestat(1));
			pipeb_ctl = REG_READ(HDMIB_CONTROL);
			pipeb_cntr = REG_READ(DSPBCNTR);
			pipe_surf[pipe] = REG_READ(DSPASURF);
			ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
		} else {
			spin_unlock_irqrestore(&dev_priv->irqmask_lock,
							irqflags);
			return 1;
		}

		/* PSB_DEBUG_ENTRY("[vsync irq] pipe : 0x%x, regsurf: 0x%x !\n", pipe, pipe_surf[pipe]); */

		if ((pipea_stat & PIPE_VBLANK_INTERRUPT_ENABLE)
		    && (pipeb_ctl & HDMIB_PORT_EN)
		    && (pipeb_cntr & DISPLAY_PLANE_ENABLE)
		    && (pipeb_stat & PIPE_VBLANK_INTERRUPT_ENABLE)) {
			if (pipe_surf[0] == pipe_surf[1]) {
				pipe_surf[0] = MAX_NUM;
				pipe_surf[1] = MAX_NUM;
			} else {
				spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
		/*PSB_DEBUG_ENTRY("Probable Display Buffer LOCK!\n");*/
				return 0;
			}
		}
	}
	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);

	return 1;
}

/* Function to check if HDMI and MIPI are presenting the same buffer.
 * If MIPI is faster than HDMI (different refresh rates), then throttle
 * the MIPI buffers by simply dropping them.
 * This function should be called from the local display's Vblank/TE
 * interrupt handler. This version of function is required to handle
 * the sync check on CTP, on which local display works on the TE interrupt
 * handler.
 */
static int mipi_te_hdmi_vsync_check(struct drm_device *dev, uint32_t pipe)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	struct mdfld_dsi_config *dsi_config = dev_priv->dsi_configs[0];
	int pipea_stat, pipeb_stat, pipeb_ctl, pipeb_cntr, pipeb_config;
	static int pipe_surf[2];
	unsigned long irqflags;

	/*check whether need to sync*/
	if (dev_priv->vsync_te_working[0] == false ||
		dev_priv->vsync_te_working[1] == false)
		return 1;

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);
	if (dev_priv->bhdmiconnected && dsi_config->dsi_hw_context.panel_on) {
		if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND,
					      OSPM_UHB_ONLY_IF_ON)) {
			pipea_stat = REG_READ(psb_pipestat(0));
			pipeb_stat = REG_READ(psb_pipestat(1));
			pipeb_ctl = REG_READ(HDMIB_CONTROL);
			pipeb_cntr = REG_READ(DSPBCNTR);
			pipeb_config = REG_READ(PIPEBCONF);
			pipe_surf[pipe] = REG_READ(DSPASURF);
			ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
		} else {
			spin_unlock_irqrestore(&dev_priv->irqmask_lock,
					irqflags);
			return 1;
		}

		if ((pipea_stat & PIPE_TE_ENABLE)
			&& (pipeb_config & PIPEBCONF_ENABLE)
			&& (pipeb_ctl & HDMIB_PORT_EN)
			&& (pipeb_cntr & DISPLAY_PLANE_ENABLE)
			&& (pipeb_stat & PIPE_VBLANK_INTERRUPT_ENABLE)) {
			if (pipe_surf[0] == pipe_surf[1]) {
				pipe_surf[0] = MAX_NUM;
				pipe_surf[1] = MAX_NUM;
			} else {
				spin_unlock_irqrestore(&dev_priv->irqmask_lock,
						       irqflags);
				return 0;
			}
		}
	}
	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);

	return 1;
}

/**
 * Check if we can disable vblank for video MIPI display
 *
 */
static void mid_check_vblank(struct drm_device *dev, uint32_t pipe)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	unsigned long irqflags;
	static unsigned long cnt;

	if (drm_psb_smart_vsync == 0) {
		if ((cnt++) % 600 == 0)
			PSB_DEBUG_ENTRY("[vsync irq] 600 times pipe : 0x%x!\n", pipe);
		return ;
	}

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);
	if (dev_priv->dsr_idle_count > 50) {
		dev_priv->b_is_in_idle = true;
	} else
		dev_priv->dsr_idle_count++;
	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
}

u32 intel_vblank_count(struct drm_device *dev, int pipe)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)dev->dev_private;

	return atomic_read(&dev_priv->vblank_count[pipe]);
}

/**
 * Display controller interrupt handler for vsync/vblank.
 *
 */
static void mid_vblank_handler(struct drm_device *dev, uint32_t pipe)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
#if 0
	/*doesn't use it for now, leave code here for reference*/
	if (pipe == 1)
		psb_flip_hdmi(dev, pipe);
#endif
	drm_handle_vblank(dev, pipe);

	if (is_cmd_mode_panel(dev)) {
		if (!mipi_te_hdmi_vsync_check(dev, pipe))
			return;
	} else {
		if (!mipi_hdmi_vsync_check(dev, pipe))
			return;
	}

	if (dev_priv->psb_vsync_handler)
		(*dev_priv->psb_vsync_handler)(dev, pipe);
}

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

static void mdfld_vsync_event(struct drm_device *dev, uint32_t pipe)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)dev->dev_private;

	if (dev_priv)
		wake_up_interruptible(&dev_priv->vsync_queue);
}

void mdfld_vsync_event_work(struct work_struct *work)
{
	struct drm_psb_private *dev_priv =
		container_of(work, struct drm_psb_private, vsync_event_work);
	int pipe = dev_priv->vsync_pipe;
	struct drm_device *dev = dev_priv->dev;

	dev_priv->vsync_te_worker_ts[pipe] = cpu_clock(0);

	mid_vblank_handler(dev, pipe);

	/*report vsync event*/
	mdfld_vsync_event(dev, pipe);
}

void mdfld_te_handler_work(struct work_struct *work)
{
	struct drm_psb_private *dev_priv =
		container_of(work, struct drm_psb_private, te_work);
	int pipe = 0;
	struct drm_device *dev = dev_priv->dev;

	dev_priv->vsync_te_worker_ts[pipe] = cpu_clock(0);

	/*report vsync event*/
	mdfld_vsync_event(dev, pipe);

	drm_handle_vblank(dev, pipe);

	if (dev_priv->b_async_flip_enable) {
		if (mipi_te_hdmi_vsync_check(dev, pipe)) {
			if (dev_priv->psb_vsync_handler != NULL)
				(*dev_priv->psb_vsync_handler)(dev, pipe);
		}

		mdfld_dsi_dsr_report_te(dev_priv->dsi_configs[0]);
	} else {
#ifdef CONFIG_MDFD_DSI_DPU
		mdfld_dpu_update_panel(dev);
#else
		mdfld_dbi_update_panel(dev, pipe);
#endif
		if (dev_priv->psb_vsync_handler != NULL)
			(*dev_priv->psb_vsync_handler)(dev, pipe);
	}
}

static void update_te_counter(struct drm_device *dev, uint32_t pipe)
{
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
}
static void get_use_cases_control_info()
{
	static int last_use_cases_control = -1;
	if (drm_psb_use_cases_control != last_use_cases_control) {
		last_use_cases_control = drm_psb_use_cases_control;
		DRM_INFO("new update of use cases\n");
		if (!(drm_psb_use_cases_control & PSB_SUSPEND_ENABLE))
			DRM_INFO("BIT0 suspend/resume  disabled\n");
		if (!(drm_psb_use_cases_control & PSB_BRIGHTNESS_ENABLE))
			DRM_INFO("BIT1 brighness setting disabled\n");
		if (!(drm_psb_use_cases_control & PSB_ESD_ENABLE))
			DRM_INFO("BIT2 ESD  disabled\n");
		if (!(drm_psb_use_cases_control & PSB_DPMS_ENABLE))
			DRM_INFO("BIT3 DPMS  disabled\n");
		if (!(drm_psb_use_cases_control & PSB_DSR_ENABLE))
			DRM_INFO("BIT4 DSR disabled\n");
		if (!(drm_psb_use_cases_control & PSB_VSYNC_OFF_ENABLE))
			DRM_INFO("BIT5 VSYNC off  disabled\n");
	}
}

/**
 * Display controller interrupt handler for pipe event.
 *
 */
#define WAIT_STATUS_CLEAR_LOOP_COUNT 0xffff
static void mid_pipe_event_handler(struct drm_device *dev, uint32_t pipe)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;

	uint32_t pipe_stat_val;
	uint32_t pipe_stat_val_raw;
	uint32_t pipe_stat_reg = psb_pipestat(pipe);
	uint32_t pipe_enable;
	uint32_t pipe_status;
	uint32_t i = 0;
	unsigned long irq_flags;
	struct mdfld_dsi_pkg_sender *sender;
	struct mdfld_dsi_dbi_output **dbi_outputs;
	struct mdfld_dsi_dbi_output *dbi_output;
	struct mdfld_dbi_dsr_info *dsr_info = dev_priv->dbi_dsr_info;

	spin_lock_irqsave(&dev_priv->irqmask_lock, irq_flags);

	pipe_enable = dev_priv->pipestat[pipe];
	pipe_status = dev_priv->pipestat[pipe] >> 16;

	pipe_stat_val = PSB_RVDC32(pipe_stat_reg);
	pipe_stat_val_raw = pipe_stat_val;
	pipe_stat_val &= pipe_enable | pipe_status;
	pipe_stat_val &= pipe_stat_val >> 16;

	/* clear the 2nd level interrupt status bits.
	 * We must keep spinlock to protect disable / enable status
	 * safe in the same register (assuming that other do that also)
	 * Clear only bits that we are going to serve this time.
	 */
	/**
	* FIXME: shouldn't use while loop here. However, the interrupt
	* status 'sticky' bits cannot be cleared by setting '1' to that
	* bit once...
	*/

	for (i = 0; i < WAIT_STATUS_CLEAR_LOOP_COUNT; i++) {
		PSB_WVDC32(pipe_stat_val_raw, pipe_stat_reg);
		(void) PSB_RVDC32(pipe_stat_reg);

		if ((PSB_RVDC32(pipe_stat_reg) & pipe_stat_val) == 0)
			break;
	}
	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irq_flags);

	if (i == WAIT_STATUS_CLEAR_LOOP_COUNT)
		DRM_ERROR("%s, can't clear the status bits in pipe_stat_reg, its value = 0x%x. \n",
			  __FUNCTION__, PSB_RVDC32(pipe_stat_reg));

	/* if pipe is HDMI, but hdmi is plugged out, should not handle
	* HDMI reg any more here */
	if (pipe == 1 && !dev_priv->bhdmiconnected)
		return;

#ifdef CONFIG_CTP_DPST
	if ((pipe_stat_val & (PIPE_DPST_EVENT_STATUS)) &&
	    (dev_priv->psb_dpst_state != NULL)) {
		uint32_t pwm_reg = 0;
		uint32_t hist_reg = 0;
		u32 irqCtrl = 0;
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
		dev_priv->vsync_te_irq_ts[pipe] = cpu_clock(0);
		dev_priv->vsync_te_working[pipe] = true;
		dev_priv->vsync_pipe = pipe;
		atomic_inc(&dev_priv->vblank_count[pipe]);
		queue_work(vsync_wq, &dev_priv->vsync_event_work);
	}

	if (pipe_stat_val & PIPE_TE_STATUS) {
		/*update te sequence on this pipe*/
		dev_priv->vsync_te_irq_ts[pipe] = cpu_clock(0);
		dev_priv->vsync_te_working[pipe] = true;
		update_te_counter(dev, pipe);
		atomic_inc(&dev_priv->vblank_count[pipe]);
		queue_work(te_wq, &dev_priv->te_work);

	}

	if (pipe_stat_val & PIPE_HDMI_AUDIO_UNDERRUN_STATUS)
		mid_hdmi_audio_signal_event(dev, HAD_EVENT_AUDIO_BUFFER_UNDERRUN);

	if (pipe_stat_val & PIPE_HDMI_AUDIO_BUFFER_DONE_STATUS)
		mid_hdmi_audio_signal_event(dev, HAD_EVENT_AUDIO_BUFFER_DONE);

	get_use_cases_control_info();
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

	if (vdc_stat & _MDFLD_MIPIA_FLAG) {
		/* mid_mipi_event_handler(dev, 0); */
	}

	if (vdc_stat & _MDFLD_MIPIC_FLAG) {
		/* mid_mipi_event_handler(dev, 2); */
	}
}

/**
 * Medfield Gl3 Cache interrupt handler.
 */
#ifdef CONFIG_MDFD_GL3
static void mdfld_gl3_interrupt(struct drm_device *dev, uint32_t vdc_stat)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	uint32_t gl3_err_status = 0;
	unsigned long irq_flags;

	gl3_err_status = MDFLD_GL3_READ(MDFLD_GCL_ERR_STATUS);
	if (gl3_err_status & _MDFLD_GL3_ECC_FLAG) {
		gl3_flush();
		gl3_reset();
	}
	spin_lock_irqsave(&dev_priv->irqmask_lock, irq_flags);
	dev_priv->vdc_irq_mask &= ~_MDFLD_GL3_IRQ_FLAG;
	PSB_WVDC32(~dev_priv->vdc_irq_mask, PSB_INT_MASK_R);
	PSB_WVDC32(dev_priv->vdc_irq_mask, PSB_INT_ENABLE_R);
	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irq_flags);
}
#endif

irqreturn_t psb_irq_handler(DRM_IRQ_ARGS)
{
	struct drm_device *dev = (struct drm_device *) arg;
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;

	uint32_t vdc_stat, dsp_int = 0, sgx_int = 0, msvdx_int = 0, topaz_int = 0;
	int handled = 0;
	unsigned long irq_flags;
	struct saved_history_record *precord = NULL;

#ifdef CONFIG_MDFD_GL3
	uint32_t gl3_int = 0;
#endif

	/*	PSB_DEBUG_ENTRY("\n"); */

	spin_lock_irqsave(&dev_priv->irqmask_lock, irq_flags);
	vdc_stat = PSB_RVDC32(PSB_INT_IDENTITY_R);

	precord = get_new_history_record();
	if (precord) {
		precord->type = 5;
		precord->record_value.vdc_stat = vdc_stat;
	}

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

#ifdef CONFIG_MDFD_GL3
	if (vdc_stat & _MDFLD_GL3_IRQ_FLAG) {
		PSB_DEBUG_IRQ("Got GL3 interrupt\n");
		gl3_int = 1;
	}
#endif

	vdc_stat &= dev_priv->vdc_irq_mask;
	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irq_flags);

	/*
		Ignore interrupt if sub-system is already
		powered gated; nothing needs to be done,
		when HW is already power-gated
		- saftey check to avoid illegal HW access.
	*/
	if (dsp_int) {
		if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND,
			OSPM_UHB_ONLY_IF_ON)) {
			psb_vdc_interrupt(dev, vdc_stat);
			handled = 1;
			ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
		} else
			DRM_INFO("get dsp int while it's off\n");
	}

#ifdef CONFIG_MDFD_VIDEO_DECODE
	if (msvdx_int) {
		if (ospm_power_is_hw_on(OSPM_VIDEO_DEC_ISLAND)) {
			atomic_inc(&g_videodec_access_count);
			psb_msvdx_interrupt(dev);
			handled = 1;
			atomic_dec(&g_videodec_access_count);
		} else
			DRM_INFO("get msvdx int while it's off\n");
	}
	if ((IS_MDFLD(dev) && topaz_int)) {
		if (ospm_power_is_hw_on(OSPM_VIDEO_ENC_ISLAND)) {
			atomic_inc(&g_videoenc_access_count);
			pnw_topaz_interrupt(dev);
			handled = 1;
			atomic_dec(&g_videoenc_access_count);
		} else
			DRM_INFO("get mdfld topaz int while it's off\n");
	}
#endif
	if (sgx_int) {
		BUG_ON(!dev_priv->pvr_ops);
		if (dev_priv->pvr_ops->SYSPVRServiceSGXInterrupt(dev) != 0)
				handled = 1;
	}


#ifdef CONFIG_MDFD_GL3
	if (gl3_int && ospm_power_is_hw_on(OSPM_GL3_CACHE_ISLAND)) {
		mdfld_gl3_interrupt(dev, vdc_stat);
		handled = 1;
	}
#endif

	PSB_WVDC32(vdc_stat, PSB_INT_IDENTITY_R);
	(void) PSB_RVDC32(PSB_INT_IDENTITY_R);
	DRM_READMEMORYBARRIER();

	if (!handled)
		return IRQ_NONE;

	return IRQ_HANDLED;
}

void psb_irq_preinstall(struct drm_device *dev)
{
	psb_irq_preinstall_islands(dev, OSPM_ALL_ISLANDS);
}

void psb_irq_preinstall_graphics_islands(struct drm_device *dev, int hw_islands)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	unsigned long irqflags;


	if (hw_islands & OSPM_GRAPHICS_ISLAND)
		dev_priv->vdc_irq_mask |= _PSB_IRQ_SGX_FLAG;
	/*This register is safe even if display island is off*/
	PSB_WVDC32(~dev_priv->vdc_irq_mask, PSB_INT_MASK_R);

}

/**
 * FIXME: should I remove display irq enable here??
 */
void psb_irq_preinstall_islands(struct drm_device *dev, int hw_islands)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	unsigned long irqflags;

	PSB_DEBUG_IRQ("\n");

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);

	if (hw_islands & OSPM_DISPLAY_ISLAND) {
		if (ospm_power_is_hw_on(OSPM_DISPLAY_ISLAND)) {
			if (IS_POULSBO(dev))
				PSB_WVDC32(0xFFFFFFFF, PSB_HWSTAM);
			if (dev->vblank_enabled[0])
				dev_priv->vdc_irq_mask |= _PSB_PIPEA_EVENT_FLAG;
			if (dev->vblank_enabled[1])
				dev_priv->vdc_irq_mask |= _MDFLD_PIPEB_EVENT_FLAG;
			if (dev->vblank_enabled[2])
				dev_priv->vdc_irq_mask |= _MDFLD_PIPEC_EVENT_FLAG;
		}
	}
	if (hw_islands & OSPM_GRAPHICS_ISLAND) {
		dev_priv->vdc_irq_mask |= _PSB_IRQ_SGX_FLAG;
	}

#ifdef CONFIG_MDFD_GL3
	if (hw_islands & OSPM_GL3_CACHE_ISLAND)
		dev_priv->vdc_irq_mask |= _MDFLD_GL3_IRQ_FLAG;
#endif
	if (hw_islands & OSPM_VIDEO_DEC_ISLAND)
		if (ospm_power_is_hw_on(OSPM_VIDEO_DEC_ISLAND))
			dev_priv->vdc_irq_mask |= _PSB_IRQ_MSVDX_FLAG;

	if (hw_islands & OSPM_VIDEO_ENC_ISLAND)
		if (ospm_power_is_hw_on(OSPM_VIDEO_ENC_ISLAND))
			dev_priv->vdc_irq_mask |= _LNC_IRQ_TOPAZ_FLAG;

	/*This register is safe even if display island is off*/
	PSB_WVDC32(~dev_priv->vdc_irq_mask, PSB_INT_MASK_R);

	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
}

int psb_irq_postinstall(struct drm_device *dev)
{
	return psb_irq_postinstall_islands(dev, OSPM_ALL_ISLANDS);
}

int psb_irq_postinstall_graphics_islands(struct drm_device *dev, int hw_islands)
{

	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	unsigned long irqflags;

	PSB_DEBUG_IRQ("\n");


	/*This register is safe even if display island is off*/
	PSB_WVDC32(dev_priv->vdc_irq_mask, PSB_INT_ENABLE_R);


	return 0;
}

int psb_irq_postinstall_islands(struct drm_device *dev, int hw_islands)
{

	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	unsigned long irqflags;

	PSB_DEBUG_IRQ("\n");

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);

	/*This register is safe even if display island is off*/
	PSB_WVDC32(dev_priv->vdc_irq_mask, PSB_INT_ENABLE_R);

	if (hw_islands & OSPM_DISPLAY_ISLAND) {
		if (true/*powermgmt_is_hw_on(dev->pdev, PSB_DISPLAY_ISLAND)*/) {
			if (IS_POULSBO(dev))
				PSB_WVDC32(0xFFFFFFFF, PSB_HWSTAM);

			if (dev->vblank_enabled[0]) {
				if (dev_priv->platform_rev_id != MDFLD_PNW_A0 &&
				    is_cmd_mode_panel(dev)) {
#if 0 /* FIXME need to revisit it */
					psb_enable_pipestat(dev_priv, 0,
							    PIPE_TE_ENABLE);
#endif
				} else
					psb_enable_pipestat(dev_priv, 0,
							    PIPE_VBLANK_INTERRUPT_ENABLE);
			} else {
				if (dev_priv->platform_rev_id != MDFLD_PNW_A0 &&
				    is_cmd_mode_panel(dev)) {
#if 0 /* FIXME need to revisit it */
					psb_disable_pipestat(dev_priv, 0,
							     PIPE_TE_ENABLE);
#endif
				} else
					psb_disable_pipestat(dev_priv, 0,
							     PIPE_VBLANK_INTERRUPT_ENABLE);
			}

			if (dev->vblank_enabled[1])
				psb_enable_pipestat(dev_priv, 1,
						    PIPE_VBLANK_INTERRUPT_ENABLE);
			else
				psb_disable_pipestat(dev_priv, 1,
						     PIPE_VBLANK_INTERRUPT_ENABLE);

			if (dev->vblank_enabled[2]) {
				if (dev_priv->platform_rev_id != MDFLD_PNW_A0 &&
				    is_cmd_mode_panel(dev)) {
#if 0 /* FIXME need to revisit it */
					psb_enable_pipestat(dev_priv, 2,
							    PIPE_TE_ENABLE);
#endif
				} else
					psb_enable_pipestat(dev_priv, 2,
							    PIPE_VBLANK_INTERRUPT_ENABLE);
			} else {
				if (dev_priv->platform_rev_id != MDFLD_PNW_A0 &&
				    is_cmd_mode_panel(dev)) {
#if 0 /* FIXME need to revisit it */
					psb_disable_pipestat(dev_priv, 2,
							     PIPE_TE_ENABLE);
#endif
				} else
					psb_disable_pipestat(dev_priv, 2,
							     PIPE_VBLANK_INTERRUPT_ENABLE);
			}
		}
	}

#ifdef CONFIG_MDFD_VIDEO_DECODE
	if (!dev_priv->topaz_disabled)
		if (hw_islands & OSPM_VIDEO_ENC_ISLAND)
			if (ospm_power_is_hw_on(OSPM_VIDEO_ENC_ISLAND)) {
				if (IS_MDFLD(dev))
					pnw_topaz_enableirq(dev);
			}

	if (hw_islands & OSPM_VIDEO_DEC_ISLAND)
		if (true/*powermgmt_is_hw_on(dev->pdev, PSB_VIDEO_DEC_ISLAND)*/)
			psb_msvdx_enableirq(dev);
#endif
	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);

	return 0;
}

void psb_irq_uninstall(struct drm_device *dev)
{
	psb_irq_uninstall_islands(dev, OSPM_ALL_ISLANDS);
}

void psb_irq_uninstall_graphics_islands(struct drm_device *dev, int hw_islands)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	unsigned long irqflags;

	if (hw_islands & OSPM_GRAPHICS_ISLAND)
		dev_priv->vdc_irq_mask &= ~_PSB_IRQ_SGX_FLAG;

	/*These two registers are safe even if display island is off*/
	PSB_WVDC32(~dev_priv->vdc_irq_mask, PSB_INT_MASK_R);
	PSB_WVDC32(dev_priv->vdc_irq_mask, PSB_INT_ENABLE_R);

	wmb();

	/*This register is safe even if display island is off*/
	PSB_WVDC32(PSB_RVDC32(PSB_INT_IDENTITY_R), PSB_INT_IDENTITY_R);

}

void psb_irq_uninstall_islands(struct drm_device *dev, int hw_islands)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	unsigned long irqflags;

	PSB_DEBUG_IRQ("\n");

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);

	if (hw_islands & OSPM_DISPLAY_ISLAND) {
		if (true/*powermgmt_is_hw_on(dev->pdev, PSB_DISPLAY_ISLAND)*/) {
			if (IS_POULSBO(dev))
				PSB_WVDC32(0xFFFFFFFF, PSB_HWSTAM);

			if (dev->vblank_enabled[0]) {
				if (dev_priv->platform_rev_id != MDFLD_PNW_A0 &&
				    is_cmd_mode_panel(dev)) {
#if 0 /* FIXME need to revisit it */
					psb_disable_pipestat(dev_priv, 0,
							     PIPE_TE_ENABLE);
#endif
				} else
					psb_disable_pipestat(dev_priv, 0,
							     PIPE_VBLANK_INTERRUPT_ENABLE);
			}

			if (dev->vblank_enabled[1])
				psb_disable_pipestat(dev_priv, 1,
						     PIPE_VBLANK_INTERRUPT_ENABLE);

			if (dev->vblank_enabled[2]) {
				if (dev_priv->platform_rev_id != MDFLD_PNW_A0 &&
				    is_cmd_mode_panel(dev)) {
#if 0 /* FIXME need to revisit it */
					psb_disable_pipestat(dev_priv, 2,
							     PIPE_TE_ENABLE);
#endif
				} else
					psb_disable_pipestat(dev_priv, 2,
							     PIPE_VBLANK_INTERRUPT_ENABLE);
			}
		}
		dev_priv->vdc_irq_mask &= _PSB_IRQ_SGX_FLAG |
					  _PSB_IRQ_MSVDX_FLAG |
					  _LNC_IRQ_TOPAZ_FLAG;
#ifdef CONFIG_MDFD_GL3
		/* Duplicate code can be removed.
		dev_priv->vdc_irq_mask &= _MDFLD_GL3_IRQ_FLAG;
		*/
#endif
	}
	/*TODO: remove follwoing code*/
	if (hw_islands & OSPM_GRAPHICS_ISLAND) {
		dev_priv->vdc_irq_mask &= ~_PSB_IRQ_SGX_FLAG;
	}
	if (hw_islands & OSPM_GL3_CACHE_ISLAND) {
#ifdef CONFIG_MDFD_GL3
		dev_priv->vdc_irq_mask &= ~_MDFLD_GL3_IRQ_FLAG;
#endif
	}

	if ((hw_islands & OSPM_VIDEO_DEC_ISLAND))
		dev_priv->vdc_irq_mask &= ~_PSB_IRQ_MSVDX_FLAG;

	if ((hw_islands & OSPM_VIDEO_ENC_ISLAND))
		dev_priv->vdc_irq_mask &= ~_LNC_IRQ_TOPAZ_FLAG;

	/*These two registers are safe even if display island is off*/
	PSB_WVDC32(~dev_priv->vdc_irq_mask, PSB_INT_MASK_R);
	PSB_WVDC32(dev_priv->vdc_irq_mask, PSB_INT_ENABLE_R);

	wmb();

	/*This register is safe even if display island is off*/
	PSB_WVDC32(PSB_RVDC32(PSB_INT_IDENTITY_R), PSB_INT_IDENTITY_R);

#ifdef CONFIG_MDFD_VIDEO_DECODE
	if (!dev_priv->topaz_disabled)
		if (hw_islands & OSPM_VIDEO_ENC_ISLAND)
			if (ospm_power_is_hw_on(OSPM_VIDEO_ENC_ISLAND)) {
				if (IS_MDFLD(dev))
					pnw_topaz_disableirq(dev);
			}
	if (hw_islands & OSPM_VIDEO_DEC_ISLAND)
		if (ospm_power_is_hw_on(OSPM_VIDEO_DEC_ISLAND))
			psb_msvdx_disableirq(dev);
#endif
	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
}

#ifdef CONFIG_CTP_DPST
void psb_irq_turn_on_dpst(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	struct mdfld_dsi_config *dsi_config = NULL;
	struct mdfld_dsi_hw_context *ctx = NULL;
	u32 hist_reg;
	u32 pwm_reg;

	if (!dev_priv)
		return;

	dsi_config = dev_priv->dsi_configs[0];
	if (!dsi_config)
		return;
	ctx = &dsi_config->dsi_hw_context;

	mdfld_dsi_dsr_forbid(dsi_config);

	if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, OSPM_UHB_ONLY_IF_ON)) {
		PSB_WVDC32(BIT31, HISTOGRAM_LOGIC_CONTROL);
		hist_reg = PSB_RVDC32(HISTOGRAM_LOGIC_CONTROL);
		ctx->histogram_logic_ctrl = hist_reg;
		PSB_WVDC32(BIT31, HISTOGRAM_INT_CONTROL);
		hist_reg = PSB_RVDC32(HISTOGRAM_INT_CONTROL);
		ctx->histogram_intr_ctrl = hist_reg;

		PSB_WVDC32(0x80010100, PWM_CONTROL_LOGIC);
		pwm_reg = PSB_RVDC32(PWM_CONTROL_LOGIC);
		PSB_WVDC32(pwm_reg | PWM_PHASEIN_ENABLE | PWM_PHASEIN_INT_ENABLE,
			   PWM_CONTROL_LOGIC);
		pwm_reg = PSB_RVDC32(PWM_CONTROL_LOGIC);

		psb_enable_pipestat(dev_priv, 0, PIPE_DPST_EVENT_ENABLE);

		hist_reg = PSB_RVDC32(HISTOGRAM_INT_CONTROL);
		PSB_WVDC32(hist_reg | HISTOGRAM_INT_CTRL_CLEAR, HISTOGRAM_INT_CONTROL);
		pwm_reg = PSB_RVDC32(PWM_CONTROL_LOGIC);
		PSB_WVDC32(pwm_reg | 0x80010100 | PWM_PHASEIN_ENABLE, PWM_CONTROL_LOGIC);

		PSB_WVDC32(0x0, LVDS_PORT_CTRL);
		ctx->lvds_port_ctrl = 0x0;

		ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	}
	mdfld_dsi_dsr_allow(dsi_config);
}

int psb_irq_enable_dpst(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	unsigned long irqflags;

	PSB_DEBUG_ENTRY("\n");

	psb_irq_turn_on_dpst(dev);

	return 0;
}

void psb_irq_turn_off_dpst(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	struct mdfld_dsi_config *dsi_config = NULL;
	u32 hist_reg;
	u32 pwm_reg;

	if (!dev_priv)
		return;
	dsi_config = dev_priv->dsi_configs[0];
	if (!dsi_config)
		return;

	mdfld_dsi_dsr_forbid(dsi_config);

	if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND,
				OSPM_UHB_ONLY_IF_ON)) {
		PSB_WVDC32(0x00000000, HISTOGRAM_INT_CONTROL);
		hist_reg = PSB_RVDC32(HISTOGRAM_INT_CONTROL);

		psb_disable_pipestat(dev_priv, 0, PIPE_DPST_EVENT_ENABLE);

		pwm_reg = PSB_RVDC32(PWM_CONTROL_LOGIC);
		PSB_WVDC32(pwm_reg & !(PWM_PHASEIN_INT_ENABLE), PWM_CONTROL_LOGIC);
		pwm_reg = PSB_RVDC32(PWM_CONTROL_LOGIC);

		ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	}

	mdfld_dsi_dsr_allow(dsi_config);
}

int psb_irq_disable_dpst(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	unsigned long irqflags;

	PSB_DEBUG_ENTRY("\n");

	psb_irq_turn_off_dpst(dev);

	return 0;
}
#endif

#ifdef PSB_FIXME
static int psb_vblank_do_wait(struct drm_device *dev,
			      unsigned int *sequence, atomic_t *counter)
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
		(struct drm_psb_private *) dev->dev_private;
	unsigned long irqflags;
	uint32_t reg_val = 0;
	uint32_t pipeconf_reg = mid_pipeconf(pipe);

	PSB_DEBUG_ENTRY("\n");

	if (IS_MDFLD(dev) && (dev_priv->platform_rev_id != MDFLD_PNW_A0) &&
	    is_cmd_mode_panel(dev) && (pipe != 1))
		return mdfld_enable_te(dev, pipe);

	if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, OSPM_UHB_ONLY_IF_ON)) {
		reg_val = REG_READ(pipeconf_reg);
		ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	}

	if (!(reg_val & PIPEACONF_ENABLE)) {
		DRM_ERROR("%s: Pipe config hasn't been enabled for pipe %d\n",
				__func__, pipe);
		return 0;
	}

	dev_priv->b_vblank_enable = true;

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);

	drm_psb_disable_vsync = 0;
	mid_enable_pipe_event(dev_priv, pipe);
	psb_enable_pipestat(dev_priv, pipe, PIPE_VBLANK_INTERRUPT_ENABLE);

	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);

	return 0;
}

/*
 * It is used to disable VBLANK interrupt
 */
void psb_disable_vblank(struct drm_device *dev, int pipe)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	unsigned long irqflags;

	if (!(drm_psb_use_cases_control & PSB_VSYNC_OFF_ENABLE))
		return;

	PSB_DEBUG_ENTRY("\n");

	if (IS_MDFLD(dev) && (dev_priv->platform_rev_id != MDFLD_PNW_A0) &&
	    is_cmd_mode_panel(dev) && (pipe != 1))
		mdfld_disable_te(dev, pipe);

	dev_priv->b_vblank_enable = false;

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);

	drm_psb_disable_vsync = 1;
	dev_priv->vsync_te_working[pipe] = false;
	mid_disable_pipe_event(dev_priv, pipe);
	psb_disable_pipestat(dev_priv, pipe, PIPE_VBLANK_INTERRUPT_ENABLE);
	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
}

/* Called from drm generic code, passed a 'crtc', which
 * we use as a pipe index
 */
u32 psb_get_vblank_counter(struct drm_device *dev, int pipe)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
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

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, false))
		return 0;

	if (pipe == 1 && dev_priv->bhdmiconnected == false) {
		DRM_ERROR("trying to get vblank count for power off pipe %d\n",
									pipe);
		goto psb_get_vblank_counter_exit;
	}

	reg_val = REG_READ(pipeconf_reg);

	if (!(reg_val & PIPEACONF_ENABLE)) {
		DRM_ERROR("trying to get vblank count for disabled pipe %d\n", pipe);
		goto psb_get_vblank_counter_exit;
	}

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

	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);

	return count;
}

/*
 * It is used to enable TE interrupt
 */
int mdfld_enable_te(struct drm_device *dev, int pipe)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	unsigned long irqflags;
	uint32_t reg_val = 0;
	uint32_t pipeconf_reg = mid_pipeconf(pipe);

	PSB_DEBUG_ENTRY("pipe = %d, \n", pipe);

	if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, OSPM_UHB_ONLY_IF_ON)) {
		reg_val = REG_READ(pipeconf_reg);
		ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	}

	if (!(reg_val & PIPEACONF_ENABLE)) {
		DRM_ERROR("%s: Pipe config hasn't been enabled for pipe %d\n",
				__func__, pipe);
		return 0;
	}

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);

	mid_enable_pipe_event(dev_priv, pipe);
	psb_enable_pipestat(dev_priv, pipe, PIPE_TE_ENABLE);
	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);

	return 0;
}

/*
 * It is used to disable TE interrupt
 */
void mdfld_disable_te(struct drm_device *dev, int pipe)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	unsigned long irqflags;
	struct mdfld_dsi_pkg_sender *sender;
	struct mdfld_dsi_config *dsi_config = dev_priv->dsi_configs[0];

	PSB_DEBUG_ENTRY("\n");

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);

	mid_disable_pipe_event(dev_priv, pipe);
	psb_disable_pipestat(dev_priv, pipe,
			(PIPE_TE_ENABLE | PIPE_DPST_EVENT_ENABLE));

	if (dsi_config) {
		/*
		 * reset te_seq, which make sure te_seq is really
		 * increased by next te enable.
		 * reset te_seq to 1 instead of 0 will make sure
		 * that last_screen_update and te_seq are alwasys
		 * unequal when exiting from DSR.
		 */
		sender = mdfld_dsi_get_pkg_sender(dsi_config);
		atomic64_set(&sender->last_screen_update, 0);
		atomic64_set(&sender->te_seq, 1);
		dev_priv->vsync_te_working[pipe] = false;
		atomic_set(&dev_priv->mipi_flip_abnormal, 0);
	}
	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
}

int mid_irq_enable_hdmi_audio(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	unsigned long irqflags;
	u32 reg_val = 0, mask = 0;

	if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, OSPM_UHB_ONLY_IF_ON)) {
		reg_val = REG_READ(PIPEBCONF);
		ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	}

	if (!(reg_val & PIPEACONF_ENABLE))
		return -EINVAL;

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);

	/* enable HDMI audio interrupt*/
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
		(struct drm_psb_private *) dev->dev_private;
	unsigned long irqflags;

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);

	mid_disable_pipe_event(dev_priv, 1);
	psb_disable_pipestat(dev_priv, 1, PIPE_HDMI_AUDIO_INT_MASK);

	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
	return 0;
}
