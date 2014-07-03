/**************************************************************************
 * Copyright (c) 2009, Intel Corporation.
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
 *    Benjamin Defnet <benjamin.r.defnet@intel.com>
 *    Rajesh Poornachandran <rajesh.poornachandran@intel.com>
 *    Yun(Mark) Tu <yun.tu@intel.com>
 *    Zhouzhou(Scott) Fang <zhouzhou.fang@intel.com>
 */
#include "psb_powermgmt.h"
#include "psb_drv.h"
#include "psb_intel_reg.h"
#include "psb_msvdx.h"
#include "pnw_topaz.h"
#include "mdfld_gl3.h"
#include "mdfld_dsi_dbi.h"
#include "mdfld_dsi_dbi_dpu.h"
#include "mdfld_dsi_dpi.h"
#include "android_hdmi.h"
#include "psb_intel_display.h"
#include "psb_irq.h"
#ifdef CONFIG_GFX_RTPM
#include <linux/pm_runtime.h>
#endif
#include <linux/atomic.h>

#include <linux/version.h>
#define SUPPORT_EARLY_SUSPEND 1
#include <asm/intel_scu_pmic.h>

#if SUPPORT_EARLY_SUSPEND
#include <linux/earlysuspend.h>
#endif /* if SUPPORT_EARLY_SUSPEND */

#include <linux/mutex.h>
#include <linux/gpio.h>
#include <linux/early_suspend_sysfs.h>
#include "mdfld_dsi_dbi_dsr.h"

#define SCU_CMD_VPROG2  0xe3

struct drm_device *gpDrmDevice;
EXPORT_SYMBOL(gpDrmDevice);
struct mutex g_ospm_mutex;

/* Lock strategy */
/*
 * we use both mutex lock and spin lock, for it
 * need synchronization between atomic context and process context
*/
static bool gbSuspendInProgress; /* default set as false */
static bool gbResumeInProgress; /* default set as false */
static bool pcihostSuspendInProgress;
bool gbSuspended; /* Indicate the host PCI suspened or not */
static int g_hw_power_status_mask;
static atomic_t g_display_access_count;
static atomic_t g_graphics_access_count;
atomic_t g_videoenc_access_count;
atomic_t g_videodec_access_count;


extern u32 DISP_PLANEB_STATUS;

void acquire_ospm_lock(void)
{
	mutex_lock(&g_ospm_mutex);
}

void release_ospm_lock(void)
{
	mutex_unlock(&g_ospm_mutex);
}

static void ospm_early_suspend();
static void ospm_late_resume();

#if SUPPORT_EARLY_SUSPEND
/*
 * gfx_early_suspend
 *
 */
static void gfx_early_suspend(struct early_suspend *h);
static void gfx_late_resume(struct early_suspend *h);

static struct early_suspend gfx_early_suspend_desc = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
	.suspend = gfx_early_suspend,
	.resume = gfx_late_resume,
};
#endif /* if SUPPORT_EARLY_SUSPEND */

static int ospm_runtime_pm_msvdx_suspend(struct drm_device *dev)
{
	int ret = 0;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;
	struct psb_video_ctx *pos, *n;
	int decode_ctx = 0, decode_running = 0;
	unsigned long irq_flags;

	PSB_DEBUG_PM("MSVDX: %s: enter in runtime pm.\n", __func__);

	if (!ospm_power_is_hw_on(OSPM_VIDEO_DEC_ISLAND))
		goto out;

	if (psb_get_power_state(OSPM_VIDEO_DEC_ISLAND) == 0) {
		PSB_DEBUG_PM("MSVDX: island already in power off state.\n");
		goto out;
	}

	if (atomic_read(&g_videodec_access_count)) {
		ret = -1;
		goto out;
	}

	if (psb_check_msvdx_idle(dev)) {
		ret = -2;
		goto out;
	}

	spin_lock_irqsave(&dev_priv->video_ctx_lock, irq_flags);
	list_for_each_entry_safe(pos, n, &dev_priv->video_ctx, head) {
		int entrypoint = pos->ctx_type & 0xff;
		if (entrypoint == VAEntrypointVLD ||
			entrypoint == VAEntrypointIZZ ||
			entrypoint == VAEntrypointIDCT ||
			entrypoint == VAEntrypointMoComp ||
			entrypoint == VAEntrypointDeblocking) {
			decode_ctx = 1;
			break;
		}
	}
	spin_unlock_irqrestore(&dev_priv->video_ctx_lock, irq_flags);

	/* have decode context, but not started, or is just closed */
	if (decode_ctx && msvdx_priv->msvdx_ctx)
		decode_running = 1;

#ifdef CONFIG_MDFD_VIDEO_DECODE
	psb_irq_uninstall_islands(gpDrmDevice, OSPM_VIDEO_DEC_ISLAND);

	if (decode_running)
		psb_msvdx_save_context(gpDrmDevice);
	MSVDX_NEW_PMSTATE(dev, msvdx_priv, PSB_PMSTATE_POWERDOWN);
#endif
	ospm_power_island_down(OSPM_VIDEO_DEC_ISLAND);

#ifdef CONFIG_MDFD_GL3
	/* Power off GL3 */
	if (IS_MDFLD(dev))
		ospm_power_island_down(OSPM_GL3_CACHE_ISLAND);
#endif

out:
	return ret;
}

static int ospm_runtime_pm_topaz_suspend(struct drm_device *dev)
{
	int ret = 0;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct pnw_topaz_private *pnw_topaz_priv = dev_priv->topaz_private;
	struct psb_video_ctx *pos, *n;
	int encode_ctx = 0, encode_running = 0;
	unsigned long irq_flags;

	if (!ospm_power_is_hw_on(OSPM_VIDEO_ENC_ISLAND))
		goto out;

	if (atomic_read(&g_videoenc_access_count)) {
		ret = -1;
		goto out;
	}

	if (IS_MDFLD(dev)) {
		if (pnw_check_topaz_idle(dev)) {
			ret = -2;
			goto out;
		}
	}

	spin_lock_irqsave(&dev_priv->video_ctx_lock, irq_flags);
	list_for_each_entry_safe(pos, n, &dev_priv->video_ctx, head) {
		int entrypoint = pos->ctx_type & 0xff;
		if (entrypoint == VAEntrypointEncSlice ||
		    entrypoint == VAEntrypointEncPicture) {
			encode_ctx = 1;
			break;
		}
	}
	spin_unlock_irqrestore(&dev_priv->video_ctx_lock, irq_flags);

	/* have encode context, but not started, or is just closed */
	if (encode_ctx && dev_priv->topaz_ctx)
		encode_running = 1;

#ifdef CONFIG_MDFD_VIDEO_DECODE
	psb_irq_uninstall_islands(gpDrmDevice, OSPM_VIDEO_ENC_ISLAND);

	if (IS_MDFLD(dev)) {
		if (encode_running)
			pnw_topaz_save_mtx_state(gpDrmDevice);
		PNW_TOPAZ_NEW_PMSTATE(dev, pnw_topaz_priv,
				PSB_PMSTATE_POWERDOWN);
	}
#endif
	ospm_power_island_down(OSPM_VIDEO_ENC_ISLAND);

#ifdef CONFIG_MDFD_GL3
	/* Power off GL3 */
	if (IS_MDFLD(dev))
		ospm_power_island_down(OSPM_GL3_CACHE_ISLAND);
#endif

out:
	return ret;
}

#ifdef CONFIG_GFX_RTPM
void psb_ospm_post_power_down()
{
	int ret;

	if (likely(!gpDrmDevice->pdev->dev.power.runtime_auto))
		return;

	if (ospm_power_is_hw_on(OSPM_VIDEO_ENC_ISLAND |
				OSPM_VIDEO_DEC_ISLAND |
				OSPM_GRAPHICS_ISLAND))
		return;

	PSB_DEBUG_PM("request runtime idle\n");

	ret = pm_request_idle(&gpDrmDevice->pdev->dev);

	if (ret) {
		PSB_DEBUG_PM("pm_request_idle fail, ret %d\n", ret);
		ret = pm_runtime_barrier(&gpDrmDevice->pdev->dev);
		if (!ret) {
			ret = pm_request_idle(&gpDrmDevice->pdev->dev);
			PSB_DEBUG_PM("pm_request_idle again, ret %d\n", ret);
		}
	}
}
#endif



static int ospm_runtime_pm_msvdx_resume(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;

	/*printk(KERN_ALERT "ospm_runtime_pm_msvdx_resume\n");*/

#ifdef CONFIG_MDFD_VIDEO_DECODE
	MSVDX_NEW_PMSTATE(dev, msvdx_priv, PSB_PMSTATE_POWERUP);

	psb_msvdx_restore_context(dev);
#endif

	return 0;
}

static int ospm_runtime_pm_topaz_resume(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct pnw_topaz_private *pnw_topaz_priv = dev_priv->topaz_private;
	struct psb_video_ctx *pos, *n;
	int encode_ctx = 0, encode_running = 0;
	unsigned long irq_flags;

	/*printk(KERN_ALERT "ospm_runtime_pm_topaz_resume\n");*/
	spin_lock_irqsave(&dev_priv->video_ctx_lock, irq_flags);
	list_for_each_entry_safe(pos, n, &dev_priv->video_ctx, head) {
		int entrypoint = pos->ctx_type & 0xff;
		if (entrypoint == VAEntrypointEncSlice ||
		    entrypoint == VAEntrypointEncPicture) {
			encode_ctx = 1;
			break;
		}
	}
	spin_unlock_irqrestore(&dev_priv->video_ctx_lock, irq_flags);

	/* have encode context, but not started, or is just closed */
	if (encode_ctx && dev_priv->topaz_ctx)
		encode_running = 1;

	if (encode_ctx)
		PSB_DEBUG_PM("Topaz: has encode context, running=%d\n",
			     encode_running);
	else
		PSB_DEBUG_PM("Topaz: no encode running\n");

#ifdef CONFIG_MDFD_VIDEO_DECODE
	if (IS_MDFLD(dev)) {
		if (encode_running) { /* has encode session running */
			psb_irq_uninstall_islands(gpDrmDevice, OSPM_VIDEO_ENC_ISLAND);
			pnw_topaz_restore_mtx_state(gpDrmDevice);
		}
		PNW_TOPAZ_NEW_PMSTATE(dev, pnw_topaz_priv, PSB_PMSTATE_POWERUP);
	}
#endif
	return 0;
}

void ospm_apm_power_down_msvdx(struct drm_device *dev, int force_off)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;

	mutex_lock(&g_ospm_mutex);
	if (force_off)
		goto power_off;
	if (!ospm_power_is_hw_on(OSPM_VIDEO_DEC_ISLAND)) {
		PSB_DEBUG_PM("g_hw_power_status_mask: msvdx in power off.\n");
		goto out;
	}

	if (psb_get_power_state(OSPM_VIDEO_DEC_ISLAND) == 0) {
		PSB_DEBUG_PM("pmu_nc_get_power_state: msvdx in power off.\n");
		goto out;
	}

	if (atomic_read(&g_videodec_access_count)) {
		PSB_DEBUG_PM("g_videodec_access_count has been set.\n");
		goto out;
	}

#ifdef CONFIG_MDFD_VIDEO_DECODE
	if (psb_check_msvdx_idle(dev))
		goto out;

	psb_msvdx_save_context(dev);

#endif

power_off:
	ospm_power_island_down(OSPM_VIDEO_DEC_ISLAND);
#ifdef CONFIG_MDFD_GL3
	/* Power off GL3 */
	ospm_power_island_down(OSPM_GL3_CACHE_ISLAND);
#endif
	MSVDX_NEW_PMSTATE(dev, msvdx_priv, PSB_PMSTATE_POWERDOWN);

out:
	mutex_unlock(&g_ospm_mutex);
	return;
}

void ospm_apm_power_down_topaz(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct pnw_topaz_private *pnw_topaz_priv = dev_priv->topaz_private;

	mutex_lock(&g_ospm_mutex);

	if (!ospm_power_is_hw_on(OSPM_VIDEO_ENC_ISLAND))
		goto out;


	if (atomic_read(&g_videoenc_access_count))
		goto out;

#ifdef CONFIG_MDFD_VIDEO_DECODE
	if (IS_MDFLD(dev))
		if (pnw_check_topaz_idle(dev))
			goto out;
	if (IS_MDFLD(dev)) {
		psb_irq_uninstall_islands(dev, OSPM_VIDEO_ENC_ISLAND);
		pnw_topaz_save_mtx_state(gpDrmDevice);
		PNW_TOPAZ_NEW_PMSTATE(dev, pnw_topaz_priv, PSB_PMSTATE_POWERDOWN);
	}
	ospm_power_island_down(OSPM_VIDEO_ENC_ISLAND);
#endif

#ifdef CONFIG_MDFD_GL3
	/* Power off GL3 */
	if (IS_MDFLD(dev))
		ospm_power_island_down(OSPM_GL3_CACHE_ISLAND);
#endif

out:
	mutex_unlock(&g_ospm_mutex);
	return;
}

static ssize_t early_suspend_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (!strncmp(buf, EARLY_SUSPEND_ON, EARLY_SUSPEND_STATUS_LEN))
		ospm_early_suspend();
	else if (!strncmp(buf, EARLY_SUSPEND_OFF, EARLY_SUSPEND_STATUS_LEN))
		ospm_late_resume();

	return count;
}
static DEVICE_EARLY_SUSPEND_ATTR(early_suspend_store);

/*
 * ospm_power_init
 *
 * Description: Initialize this ospm power management module
 */
void ospm_power_init(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = (struct drm_psb_private *)dev->dev_private;
	unsigned long flags;

	gpDrmDevice = dev;

	mutex_init(&g_ospm_mutex);
	spin_lock_init(&dev_priv->ospm_lock);

	spin_lock_irqsave(&dev_priv->ospm_lock, flags);
	g_hw_power_status_mask = OSPM_ALL_ISLANDS;
	spin_unlock_irqrestore(&dev_priv->ospm_lock, flags);

	atomic_set(&g_display_access_count, 0);
	atomic_set(&g_graphics_access_count, 0);
	atomic_set(&g_videoenc_access_count, 0);
	atomic_set(&g_videodec_access_count, 0);

	device_create_file(&dev->pdev->dev, &dev_attr_early_suspend);

	register_early_suspend_device(&gpDrmDevice->pdev->dev);

#if SUPPORT_EARLY_SUSPEND
	register_early_suspend(&gfx_early_suspend_desc);
#endif /* if SUPPORT_EARLY_SUSPEND */

#ifdef OSPM_STAT
	dev_priv->graphics_state = PSB_PWR_STATE_ON;
	dev_priv->gfx_last_mode_change = jiffies;
	dev_priv->gfx_on_time = 0;
	dev_priv->gfx_off_time = 0;
#endif
}

/*
 * ospm_power_uninit
 *
 * Description: Uninitialize this ospm power management module
 */
void ospm_power_uninit(void)
{
	device_remove_file(&gpDrmDevice->pdev->dev, &dev_attr_early_suspend);
	unregister_early_suspend_device(&gpDrmDevice->pdev->dev);

#if SUPPORT_EARLY_SUSPEND
    unregister_early_suspend(&gfx_early_suspend_desc);
#endif /* if SUPPORT_EARLY_SUSPEND */
    mutex_destroy(&g_ospm_mutex);
#ifdef CONFIG_GFX_RTPM
	pm_runtime_get_noresume(&gpDrmDevice->pdev->dev);
#endif
}

/*
 *  mdfld_adjust_display_fifo
 *
 * Update display fifo setting to avoid hdmi flicker
 */
static void mdfld_adjust_display_fifo(struct drm_device *dev)
{
	u32 temp;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct mdfld_dsi_config *dsi_config = dev_priv->dsi_configs[0];
	struct drm_display_mode *mode = dsi_config->fixed_mode;

	if (IS_CTP(dev)) {
		/* Set proper high priority configuration to avoid overlay
		 * block memory self-refresh entry */
		temp = REG_READ(G_HP_CONTROL);
		REG_WRITE(G_HP_CONTROL,
			HP_REQUESTORS_STATUS_OVERRIDE_MODE | temp);
		if (mode &&
		    ((mode->hdisplay >= 1920 && mode->vdisplay >= 1080) ||
		     (mode->hdisplay >= 1080 && mode->vdisplay >= 1920))) {
			if ((mode->hdisplay == 1920 &&
			     mode->vdisplay == 1080) ||
			    (mode->hdisplay == 1080 &&
			     mode->vdisplay == 1920)) {
				/* setting for 1080p panel */
				REG_WRITE(DSPARB, 0x0005F8C0);
				REG_WRITE(DSPFW1, 0x0F0F1010);
				REG_WRITE(DSPFW2, 0x5F2F0F0F);
				REG_WRITE(DSPFW4, 0x07071010);
			} else {
				/* setting for panel bigger than 1080p */
				REG_WRITE(DSPARB, 0x0005F8D4);
				REG_WRITE(DSPFW1, 0x0F0F1010);
				REG_WRITE(DSPFW2, 0x5F2F0F0F);
				REG_WRITE(DSPFW4, 0x07071010);
			}
		} else {
			/* setting for panel smaller than 1080p, f.e 720p */
			REG_WRITE(DSPARB, 0x0005E480);
			REG_WRITE(DSPFW1, 0x0F0F103F);
			REG_WRITE(DSPFW4, 0x0707101F);
		}

		REG_WRITE(MI_ARB, 0x0);
	}

	temp = REG_READ(DSPARB);
	PSB_DEBUG_ENTRY("gfx_hdmi_setting: DSPARB = 0x%x", temp);

	temp = REG_READ(DSPFW1);
	PSB_DEBUG_ENTRY("gfx_hdmi_setting: DSPFW1 = 0x%x", temp);

	temp = REG_READ(DSPFW4);
	PSB_DEBUG_ENTRY("gfx_hdmi_setting: DSPFW4 = 0x%x", temp);

	temp = REG_READ(MI_ARB);
	PSB_DEBUG_ENTRY("gfx_hdmi_setting: MI_ARB = 0x%x", temp);

}

/*
* ospm_post_init
*
* Description: Power gate unused GFX & Display islands.
*/
void ospm_post_init(struct drm_device *dev)
{
	u32 dc_islands  = 0;
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)dev->dev_private;
	unsigned long flags;
	u32 all_display_islands;

	mutex_lock(&g_ospm_mutex);
	/* need disable power for msvdx and topaz in init stage */
	ospm_power_island_down(OSPM_VIDEO_DEC_ISLAND | OSPM_VIDEO_ENC_ISLAND);

#ifndef CONFIG_MDFD_GL3
	ospm_power_island_down(OSPM_GL3_CACHE_ISLAND);
#endif
	/*Save & Power gate un-used display islands.*/
	mdfld_save_display(dev);

	if (!(dev_priv->panel_desc & DISPLAY_A))
		dc_islands |= OSPM_DISPLAY_A_ISLAND;

	if (!(dev_priv->panel_desc & DISPLAY_B))
		dc_islands |= OSPM_DISPLAY_B_ISLAND;

	if (!(dev_priv->panel_desc & DISPLAY_C))
		dc_islands |= OSPM_DISPLAY_C_ISLAND;

	if (!(dev_priv->panel_desc))
		dc_islands |= OSPM_MIPI_ISLAND;

	DRM_INFO("%s dc_islands: %x to be powered OFF\n", __func__, dc_islands);

	spin_lock_irqsave(&dev_priv->ospm_lock, flags);
	/*
	If pmu_nc_set_power_state fails then accessing HW
	reg would result in a crash - IERR/Fabric error.
	*/
	if (pmu_nc_set_power_state(dc_islands,
		OSPM_ISLAND_DOWN, OSPM_REG_TYPE))
		BUG();

	all_display_islands = (OSPM_DISPLAY_A_ISLAND |
	OSPM_DISPLAY_B_ISLAND |
	OSPM_DISPLAY_C_ISLAND |
	OSPM_MIPI_ISLAND);
	if ((dc_islands & all_display_islands) == all_display_islands)
		g_hw_power_status_mask &= ~OSPM_DISPLAY_ISLAND;

	spin_unlock_irqrestore(&dev_priv->ospm_lock, flags);

/* if HDMI is disabled in the kernel .config, then we want to
disable these MSIC power rails permanently.  */
#ifndef CONFIG_SUPPORT_HDMI
	if (IS_MDFLD_OLD(dev)) {
		/* turn off HDMI power rails */
		intel_scu_ipc_iowrite8(MSIC_VHDMICNT, VHDMI_OFF);
		intel_scu_ipc_iowrite8(MSIC_VCC330CNT, VCC330_OFF);
	}
	if (IS_CTP(dev)) {
		/* turn off HDMI power rails */
		intel_scu_ipc_iowrite8(MSIC_VCC330CNT, VCC330_OFF);
	}
#endif
	mdfld_adjust_display_fifo(dev);

	mutex_unlock(&g_ospm_mutex);

}

/*
 * mdfld_save_display_registers
 *
 * Description: We are going to suspend so save current display
 * register state.
 *
 */
static int mdfld_save_display_registers (struct drm_device *dev, int pipe)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	int i;

	/* regester */
	u32 dpll_reg = MDFLD_DPLL_B;
	u32 fp_reg = MDFLD_DPLL_DIV0;
	u32 pipeconf_reg = PIPEBCONF;
	u32 htot_reg = HTOTAL_B;
	u32 hblank_reg = HBLANK_B;
	u32 hsync_reg = HSYNC_B;
	u32 vtot_reg = VTOTAL_B;
	u32 vblank_reg = VBLANK_B;
	u32 vsync_reg = VSYNC_B;
	u32 pipesrc_reg = PIPEBSRC;
	u32 dspstride_reg = DSPBSTRIDE;
	u32 dsplinoff_reg = DSPBLINOFF;
	u32 dsptileoff_reg = DSPBTILEOFF;
	u32 dspsize_reg = DSPBSIZE;
	u32 dsppos_reg = DSPBPOS;
	u32 dspsurf_reg = DSPBSURF;
	u32 dspcntr_reg = DSPBCNTR;
	u32 dspstatus_reg = PIPEBSTAT;
	u32 palette_reg = PALETTE_B;
	u32 color_coef_reg = PIPEB_COLOR_COEF0;


	/* values */
	u32 *dpll_val = &dev_priv->saveDPLL_B;
	u32 *fp_val = &dev_priv->saveFPB0;
	u32 *pipeconf_val = &dev_priv->savePIPEBCONF;
	u32 *htot_val = &dev_priv->saveHTOTAL_B;
	u32 *hblank_val = &dev_priv->saveHBLANK_B;
	u32 *hsync_val = &dev_priv->saveHSYNC_B;
	u32 *vtot_val = &dev_priv->saveVTOTAL_B;
	u32 *vblank_val = &dev_priv->saveVBLANK_B;
	u32 *vsync_val = &dev_priv->saveVSYNC_B;
	u32 *pipesrc_val = &dev_priv->savePIPEBSRC;
	u32 *dspstride_val = &dev_priv->saveDSPBSTRIDE;
	u32 *dsplinoff_val = &dev_priv->saveDSPBLINOFF;
	u32 *dsptileoff_val = &dev_priv->saveDSPBTILEOFF;
	u32 *dspsize_val = &dev_priv->saveDSPBSIZE;
	u32 *dsppos_val = &dev_priv->saveDSPBPOS;
	u32 *dspsurf_val = &dev_priv->saveDSPBSURF;
	u32 *dspcntr_val = &dev_priv->saveDSPBCNTR;
	u32 *dspstatus_val = &dev_priv->saveDSPBSTATUS;
	u32 *palette_val = dev_priv->save_palette_b;
	u32 *color_coef = dev_priv->save_color_coef_b;

	PSB_DEBUG_ENTRY("\n");

	if (pipe != 1)
		return 0;

	/* Pipe & plane A info */
	*dpll_val = REG_READ(dpll_reg);
	*fp_val = REG_READ(fp_reg);
	*pipeconf_val = REG_READ(pipeconf_reg);
	*htot_val = REG_READ(htot_reg);
	*hblank_val = REG_READ(hblank_reg);
	*hsync_val = REG_READ(hsync_reg);
	*vtot_val = REG_READ(vtot_reg);
	*vblank_val = REG_READ(vblank_reg);
	*vsync_val = REG_READ(vsync_reg);
	*pipesrc_val = REG_READ(pipesrc_reg);
	*dspstride_val = REG_READ(dspstride_reg);
	*dsplinoff_val = REG_READ(dsplinoff_reg);
	*dsptileoff_val = REG_READ(dsptileoff_reg);
	*dspsize_val = REG_READ(dspsize_reg);
	*dsppos_val = REG_READ(dsppos_reg);
	*dspsurf_val = REG_READ(dspsurf_reg);
	*dspcntr_val = REG_READ(dspcntr_reg);
	*dspstatus_val = REG_READ(dspstatus_reg);

	/*save palette (gamma) */
	for (i = 0; i < 256; i++)
		palette_val[i] = REG_READ(palette_reg + (i<<2));

	/*save color_coef (chrome) */
	for (i = 0; i < 6; i++)
		color_coef[i] = REG_READ(color_coef_reg + (i<<2));


	dev_priv->savePFIT_CONTROL = REG_READ(PFIT_CONTROL);
	dev_priv->savePFIT_PGM_RATIOS = REG_READ(PFIT_PGM_RATIOS);

	dev_priv->saveHDMIPHYMISCCTL = REG_READ(HDMIPHYMISCCTL);
	dev_priv->saveHDMIB_CONTROL = REG_READ(HDMIB_CONTROL);
	dev_priv->saveDATALANES_B = REG_READ(HDMIB_LANES02);
	return 0;
}

/*
 * mdfld_save_cursor_overlay_registers
 *
 * Description: We are going to suspend so save current cursor and overlay display
 * register state.
 */
static int mdfld_save_cursor_overlay_registers(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;

	/*save cursor regs*/
	dev_priv->saveDSPACURSOR_CTRL = PSB_RVDC32(CURACNTR);
	dev_priv->saveDSPACURSOR_BASE = PSB_RVDC32(CURABASE);
	dev_priv->saveDSPACURSOR_POS = PSB_RVDC32(CURAPOS);

	dev_priv->saveDSPBCURSOR_CTRL = PSB_RVDC32(CURBCNTR);
	dev_priv->saveDSPBCURSOR_BASE = PSB_RVDC32(CURBBASE);
	dev_priv->saveDSPBCURSOR_POS = PSB_RVDC32(CURBPOS);

	dev_priv->saveDSPCCURSOR_CTRL = PSB_RVDC32(CURCCNTR);
	dev_priv->saveDSPCCURSOR_BASE = PSB_RVDC32(CURCBASE);
	dev_priv->saveDSPCCURSOR_POS = PSB_RVDC32(CURCPOS);

	/* HW overlay */
	dev_priv->saveOV_OVADD = PSB_RVDC32(OV_OVADD);
	dev_priv->saveOV_OGAMC0 = PSB_RVDC32(OV_OGAMC0);
	dev_priv->saveOV_OGAMC1 = PSB_RVDC32(OV_OGAMC1);
	dev_priv->saveOV_OGAMC2 = PSB_RVDC32(OV_OGAMC2);
	dev_priv->saveOV_OGAMC3 = PSB_RVDC32(OV_OGAMC3);
	dev_priv->saveOV_OGAMC4 = PSB_RVDC32(OV_OGAMC4);
	dev_priv->saveOV_OGAMC5 = PSB_RVDC32(OV_OGAMC5);

	dev_priv->saveOV_OVADD_C = PSB_RVDC32(OV_OVADD + OV_C_OFFSET);
	dev_priv->saveOV_OGAMC0_C = PSB_RVDC32(OV_OGAMC0 + OV_C_OFFSET);
	dev_priv->saveOV_OGAMC1_C = PSB_RVDC32(OV_OGAMC1 + OV_C_OFFSET);
	dev_priv->saveOV_OGAMC2_C = PSB_RVDC32(OV_OGAMC2 + OV_C_OFFSET);
	dev_priv->saveOV_OGAMC3_C = PSB_RVDC32(OV_OGAMC3 + OV_C_OFFSET);
	dev_priv->saveOV_OGAMC4_C = PSB_RVDC32(OV_OGAMC4 + OV_C_OFFSET);
	dev_priv->saveOV_OGAMC5_C = PSB_RVDC32(OV_OGAMC5 + OV_C_OFFSET);

	return 0;
}

/**
 * @dev: DRM device
 * @pipe: DC pipe
 *
 * restore registers of display controller. It's just for HDMI, as for MIPI
 * pipe, use early suspend to save/restore dc registers.
 */
static int mdfld_restore_display_registers(struct drm_device *dev, int pipe)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	int i = 0;
	u32 dpll = 0;

	/* regester */
	u32 dpll_reg = MDFLD_DPLL_B;
	u32 fp_reg = MDFLD_DPLL_DIV0;
	u32 pipeconf_reg = PIPEBCONF;
	u32 htot_reg = HTOTAL_B;
	u32 hblank_reg = HBLANK_B;
	u32 hsync_reg = HSYNC_B;
	u32 vtot_reg = VTOTAL_B;
	u32 vblank_reg = VBLANK_B;
	u32 vsync_reg = VSYNC_B;
	u32 pipesrc_reg = PIPEBSRC;
	u32 dspstride_reg = DSPBSTRIDE;
	u32 dsplinoff_reg = DSPBLINOFF;
	u32 dsptileoff_reg = DSPBTILEOFF;
	u32 dspsize_reg = DSPBSIZE;
	u32 dsppos_reg = DSPBPOS;
	u32 dspsurf_reg = DSPBSURF;
	u32 dspcntr_reg = DSPBCNTR;
	u32 palette_reg = PALETTE_B;
	u32 dspstatus_reg = PIPEBSTAT;
	u32 color_coef_reg = PIPEB_COLOR_COEF0;

	/* values */
	u32 dpll_val = dev_priv->saveDPLL_B & ~DPLL_VCO_ENABLE;
	u32 fp_val = dev_priv->saveFPB0;
	u32 pipeconf_val = dev_priv->savePIPEBCONF;
	u32 htot_val = dev_priv->saveHTOTAL_B;
	u32 hblank_val = dev_priv->saveHBLANK_B;
	u32 hsync_val = dev_priv->saveHSYNC_B;
	u32 vtot_val = dev_priv->saveVTOTAL_B;
	u32 vblank_val = dev_priv->saveVBLANK_B;
	u32 vsync_val = dev_priv->saveVSYNC_B;
	u32 pipesrc_val = dev_priv->savePIPEBSRC;
	u32 dspstride_val = dev_priv->saveDSPBSTRIDE;
	u32 dsplinoff_val = dev_priv->saveDSPBLINOFF;
	u32 dsptileoff_val = dev_priv->saveDSPBTILEOFF;
	u32 dspsize_val = dev_priv->saveDSPBSIZE;
	u32 dsppos_val = dev_priv->saveDSPBPOS;
	u32 dspsurf_val = dev_priv->saveDSPBSURF;
	u32 dspcntr_val = dev_priv->saveDSPBCNTR & ~DISPLAY_PLANE_ENABLE;
	u32 dspstatus_val = dev_priv->saveDSPBSTATUS;
	u32 *palette_val = dev_priv->save_palette_b;
	u32 *color_coef = dev_priv->save_color_coef_b;

	PSB_DEBUG_ENTRY("\n");

	if (pipe != 1)
		return 0;

	/*make sure VGA plane is off. it initializes to on after reset!*/
	REG_WRITE(VGACNTRL, 0x80000000);

	dpll = REG_READ(dpll_reg);

	if (!(dpll & DPLL_VCO_ENABLE)) {
		/**
		 * When ungating power of DPLL, needs to wait 0.5us
		 * before enable the VCO
		 */
		if (dpll & MDFLD_PWR_GATE_EN) {
			dpll &= ~MDFLD_PWR_GATE_EN;
			REG_WRITE(dpll_reg, dpll);
			ndelay(500);
		}

		REG_WRITE(fp_reg, fp_val);
		REG_WRITE(dpll_reg, dpll_val);
		ndelay(500);

		dpll_val |= DPLL_VCO_ENABLE;
		REG_WRITE(dpll_reg, dpll_val);
		REG_READ(dpll_reg);
	}

	/* Restore mode */
	REG_WRITE(htot_reg, htot_val);
	REG_WRITE(hblank_reg, hblank_val);
	REG_WRITE(hsync_reg, hsync_val);
	REG_WRITE(vtot_reg, vtot_val);
	REG_WRITE(vblank_reg, vblank_val);
	REG_WRITE(vsync_reg, vsync_val);
	REG_WRITE(pipesrc_reg, pipesrc_val);
	REG_WRITE(dspstatus_reg, dspstatus_val);

	/*set up the plane*/
	REG_WRITE(dspstride_reg, dspstride_val);
	REG_WRITE(dsplinoff_reg, dsplinoff_val);
	REG_WRITE(dsptileoff_reg, dsptileoff_val);
	REG_WRITE(dspsize_reg, dspsize_val);
	REG_WRITE(dsppos_reg, dsppos_val);
	REG_WRITE(dspsurf_reg, dspsurf_val);

	REG_WRITE(PFIT_CONTROL, dev_priv->savePFIT_CONTROL);
	REG_WRITE(PFIT_PGM_RATIOS, dev_priv->savePFIT_PGM_RATIOS);
	REG_WRITE(HDMIPHYMISCCTL, dev_priv->saveHDMIPHYMISCCTL);
	REG_WRITE(HDMIB_CONTROL, dev_priv->saveHDMIB_CONTROL);
	REG_WRITE(HDMIB_LANES02, dev_priv->saveDATALANES_B);
	REG_WRITE(HDMIB_LANES3, dev_priv->saveDATALANES_B);

	/*save color_coef (chrome) */
	for (i = 0; i < 6; i++)
		REG_WRITE(color_coef_reg + (i<<2), color_coef[i]);

	/* restore palette (gamma) */
	for (i = 0; i < 256; i++)
		REG_WRITE(palette_reg + (i<<2), palette_val[i]);

	/*enable the plane*/
	REG_WRITE(dspcntr_reg, dspcntr_val);

	/*enable the pipe*/
	REG_WRITE(pipeconf_reg, pipeconf_val);
	if (pipeconf_val & PIPEBCONF_ENABLE)
		intel_wait_for_pipe_enable_disable(dev, pipe, true);

	return 0;
}

/*
 * mdfld_restore_cursor_overlay_registers
 *
 * Description: We are going to resume so restore cursor and overlay register state.
 */
static int mdfld_restore_cursor_overlay_registers(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;

	/*Enable Cursor A*/
	REG_WRITE(CURACNTR, dev_priv->saveDSPACURSOR_CTRL);
	REG_WRITE(CURAPOS, dev_priv->saveDSPACURSOR_POS);
	REG_WRITE(CURABASE, dev_priv->saveDSPACURSOR_BASE);

	REG_WRITE(CURBCNTR, dev_priv->saveDSPBCURSOR_CTRL);
	REG_WRITE(CURBPOS, dev_priv->saveDSPBCURSOR_POS);
	REG_WRITE(CURBBASE, dev_priv->saveDSPBCURSOR_BASE);

	REG_WRITE(CURCCNTR, dev_priv->saveDSPCCURSOR_CTRL);
	REG_WRITE(CURCPOS, dev_priv->saveDSPCCURSOR_POS);
	REG_WRITE(CURCBASE, dev_priv->saveDSPCCURSOR_BASE);

	/* restore HW overlay */
	REG_WRITE(OV_OVADD, dev_priv->saveOV_OVADD);
	REG_WRITE(OV_OGAMC0, dev_priv->saveOV_OGAMC0);
	REG_WRITE(OV_OGAMC1, dev_priv->saveOV_OGAMC1);
	REG_WRITE(OV_OGAMC2, dev_priv->saveOV_OGAMC2);
	REG_WRITE(OV_OGAMC3, dev_priv->saveOV_OGAMC3);
	REG_WRITE(OV_OGAMC4, dev_priv->saveOV_OGAMC4);
	REG_WRITE(OV_OGAMC5, dev_priv->saveOV_OGAMC5);

	REG_WRITE(OV_OVADD + OV_C_OFFSET, dev_priv->saveOV_OVADD_C);
	REG_WRITE(OV_OGAMC0 + OV_C_OFFSET, dev_priv->saveOV_OGAMC0_C);
	REG_WRITE(OV_OGAMC1 + OV_C_OFFSET, dev_priv->saveOV_OGAMC1_C);
	REG_WRITE(OV_OGAMC2 + OV_C_OFFSET, dev_priv->saveOV_OGAMC2_C);
	REG_WRITE(OV_OGAMC3 + OV_C_OFFSET, dev_priv->saveOV_OGAMC3_C);
	REG_WRITE(OV_OGAMC4 + OV_C_OFFSET, dev_priv->saveOV_OGAMC4_C);
	REG_WRITE(OV_OGAMC5 + OV_C_OFFSET, dev_priv->saveOV_OGAMC5_C);

	return 0;
}

/*
 *  mdfld_save_display
 *
 * Description: Save display status before DPMS OFF for RuntimePM
 */
void mdfld_save_display(struct drm_device *dev)
{
	PSB_DEBUG_ENTRY("\n");

	mdfld_save_cursor_overlay_registers(dev);
}

/*
 * powermgmt_suspend_display
 *
 * Description: Suspend the display hardware saving state and disabling
 * as necessary.
 */
void ospm_suspend_display(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;

	PSB_DEBUG_ENTRY("\n");

	if (!ospm_power_is_hw_on(OSPM_DISPLAY_ISLAND)) {
		DRM_INFO("%s: Exit because island is down\n", __func__);
		return;
	}

	mdfld_save_cursor_overlay_registers(dev);

	if (dev_priv->panel_desc & DISPLAY_A) {
		mdfld_save_display_registers(dev, 0);
		mdfld_disable_crtc(dev, 0);
	}
	if (dev_priv->panel_desc & DISPLAY_B) {
		android_hdmi_suspend_display(dev);
	}
	if (dev_priv->panel_desc & DISPLAY_C) {
		mdfld_save_display_registers(dev, 2);
		mdfld_disable_crtc(dev, 2);
	}

	/*save performance state*/
	dev_priv->savePERF_MODE = REG_READ(MRST_PERF_MODE);
	dev_priv->saveCLOCKGATING = REG_READ(PSB_GFX_CLOCKGATING);
	dev_priv->saveVED_CG_DIS = REG_READ(PSB_MSVDX_CLOCKGATING);
	dev_priv->saveVEC_CG_DIS = REG_READ(PSB_TOPAZ_CLOCKGATING);

#ifdef CONFIG_MDFD_GL3
	dev_priv->saveGL3_CTL = REG_READ(MDFLD_GL3_CONTROL);
	dev_priv->saveGL3_USE_WRT_INVAL = REG_READ(MDFLD_GL3_USE_WRT_INVAL);
#endif

	ospm_power_island_down(OSPM_DISPLAY_ISLAND);
}

/*
 * ospm_resume_display
 *
 * Description: Resume the display hardware restoring state and enabling
 * as necessary.
 */
void ospm_resume_display(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct psb_gtt *pg = dev_priv->pg;

	PSB_DEBUG_ENTRY("\n");

	if (ospm_power_is_hw_on(OSPM_DISPLAY_ISLAND)) {
		DRM_INFO("%s: Exit because hw on\n", __func__);
		return;
	}

	/*restore performance mode*/
	REG_WRITE(MRST_PERF_MODE, dev_priv->savePERF_MODE);
	REG_WRITE(PSB_GFX_CLOCKGATING, dev_priv->saveCLOCKGATING);
	REG_WRITE(PSB_MSVDX_CLOCKGATING, dev_priv->saveVED_CG_DIS);
	REG_WRITE(PSB_TOPAZ_CLOCKGATING, dev_priv->saveVEC_CG_DIS);
#ifdef CONFIG_MDFD_GL3
	REG_WRITE(MDFLD_GL3_CONTROL, dev_priv->saveGL3_CTL);
	REG_WRITE(MDFLD_GL3_USE_WRT_INVAL, dev_priv->saveGL3_USE_WRT_INVAL);
#endif

	/* turn on the display power island */
	ospm_power_island_up(OSPM_DISPLAY_ISLAND);

	REG_WRITE(PSB_PGETBL_CTL, pg->pge_ctl | _PSB_PGETBL_ENABLED);
	pci_write_config_word(pdev, PSB_GMCH_CTRL,
			pg->gmch_ctrl | _PSB_GMCH_ENABLED);

	if (dev_priv->panel_desc & DISPLAY_A)
		mdfld_restore_display_registers(dev, 0);
	if (dev_priv->panel_desc & DISPLAY_C)
		mdfld_restore_display_registers(dev, 2);

	/*
	 * Don't restore Display B registers during resuming, if HDMI
	 * isn't turned on before suspending.
	 */
	if (dev_priv->panel_desc & DISPLAY_B) {
		android_hdmi_resume_display(dev);
		/*devices connect status will be changed
		  when system suspend,re-detect once here*/
		if (android_hdmi_is_connected(dev))
			mid_hdmi_audio_resume(dev);
	}
	mdfld_restore_cursor_overlay_registers(dev);

	mdfld_adjust_display_fifo(dev);
}

/*
 * ospm_suspend_pci
 *
 * Description: Suspend the pci device saving state and disabling
 * as necessary.
 */
void ospm_suspend_pci(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	struct drm_psb_private *dev_priv = dev->dev_private;
	int bsm, vbt;

	if (gbSuspended)
		return;

	PSB_DEBUG_PM("ospm_suspend_pci\n");

	pci_save_state(pdev);
	pci_read_config_dword(pdev, 0x5C, &bsm);
	dev_priv->saveBSM = bsm;
	pci_read_config_dword(pdev, 0xFC, &vbt);
	dev_priv->saveVBT = vbt;
	pci_read_config_dword(pdev, PSB_PCIx_MSI_ADDR_LOC, &dev_priv->msi_addr);
	pci_read_config_dword(pdev, PSB_PCIx_MSI_DATA_LOC, &dev_priv->msi_data);

	pci_disable_device(pdev);
	pci_set_power_state(pdev, PCI_D3hot);

	gbSuspended = true;
}

/*
 * ospm_resume_pci
 *
 * Description: Resume the pci device restoring state and enabling
 * as necessary.
 */
static bool ospm_resume_pci(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	struct drm_psb_private *dev_priv = dev->dev_private;
	int ret = 0;

	if (!gbSuspended)
		return true;

	PSB_DEBUG_PM("ospm_resume_pci.\n");

	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);
	pci_write_config_dword(pdev, 0x5c, dev_priv->saveBSM);
	pci_write_config_dword(pdev, 0xFC, dev_priv->saveVBT);
	/* retoring MSI address and data in PCIx space */
	pci_write_config_dword(pdev, PSB_PCIx_MSI_ADDR_LOC, dev_priv->msi_addr);
	pci_write_config_dword(pdev, PSB_PCIx_MSI_DATA_LOC, dev_priv->msi_data);
	ret = pci_enable_device(pdev);

	if (ret != 0)
		printk(KERN_ALERT "ospm_resume_pci: pci_enable_device failed: %d\n", ret);
	else {
		if (IS_MDFLD(dev)) {
			/*restore performance mode*/
			PSB_WVDC32(dev_priv->savePERF_MODE, MRST_PERF_MODE);
			PSB_WVDC32(dev_priv->saveCLOCKGATING,
				PSB_GFX_CLOCKGATING);
			PSB_WVDC32(dev_priv->saveVED_CG_DIS,
					PSB_MSVDX_CLOCKGATING);
			PSB_WVDC32(dev_priv->saveVEC_CG_DIS,
					PSB_TOPAZ_CLOCKGATING);
#ifdef CONFIG_MDFD_GL3
			PSB_WVDC32(dev_priv->saveGL3_CTL, MDFLD_GL3_CONTROL);
			PSB_WVDC32(dev_priv->saveGL3_USE_WRT_INVAL,
					MDFLD_GL3_USE_WRT_INVAL);
#endif
		}
		gbSuspended = false;
	}

	return !gbSuspended;
}

static void ospm_early_suspend()
{
	struct drm_psb_private *dev_priv = gpDrmDevice->dev_private;
	struct drm_device *dev = dev_priv->dev;
	struct drm_encoder *encoder;
	struct drm_encoder_helper_funcs *enc_funcs;

	if (!(drm_psb_use_cases_control & PSB_SUSPEND_ENABLE))
		return ;

	PSB_DEBUG_ENTRY("\n");

	dev_priv->b_dsr_enable_status = dev_priv->b_dsr_enable;
	if (dev_priv->b_dsr_enable) {
		dev_priv->exit_idle(dev,
				MDFLD_DSR_2D_3D,
				NULL,
				0);
		dev_priv->b_dsr_enable = false;
	}

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
	}


	gbdispstatus = false;
	dev_priv->early_suspended = true;

	mutex_unlock(&dev->mode_config.mutex);

#ifdef CONFIG_GFX_RTPM
	pm_runtime_allow(&gpDrmDevice->pdev->dev);
#endif
}

static void ospm_late_resume()
{
	struct drm_psb_private *dev_priv = gpDrmDevice->dev_private;
	struct drm_device *dev = dev_priv->dev;
	struct drm_encoder *encoder;
	struct drm_encoder_helper_funcs *enc_funcs;

	if (!(drm_psb_use_cases_control & PSB_SUSPEND_ENABLE))
		return ;

	PSB_DEBUG_ENTRY("\n");

	/* protect early_suspend with dpms and mode config */
	mutex_lock(&dev->mode_config.mutex);

	dev_priv->early_suspended = false;

#ifdef CONFIG_GFX_RTPM
	pm_runtime_forbid(&gpDrmDevice->pdev->dev);
	mutex_lock(&g_ospm_mutex);
	ospm_resume_pci(gpDrmDevice->pdev);
	ospm_resume_display(gpDrmDevice->pdev);
	psb_irq_preinstall_islands(gpDrmDevice, OSPM_DISPLAY_ISLAND);
	psb_irq_postinstall_islands(gpDrmDevice, OSPM_DISPLAY_ISLAND);
	mutex_unlock(&g_ospm_mutex);
#endif

	list_for_each_entry(encoder,
			&dev->mode_config.encoder_list,
			head) {
		enc_funcs = encoder->helper_private;
		if (!drm_helper_encoder_in_use(encoder))
			continue;
		if (enc_funcs && enc_funcs->restore)
			enc_funcs->restore(encoder);
	}

	gbdispstatus = true;
	dev_priv->b_dsr_enable = dev_priv->b_dsr_enable_status;

	if (lastFailedBrightness > 0)
		psb_set_brightness(NULL);

	mutex_unlock(&dev->mode_config.mutex);
}

#if SUPPORT_EARLY_SUSPEND
static void gfx_early_suspend(struct early_suspend *h)
{
	ospm_early_suspend();
}
#endif /* if SUPPORT_EARLY_SUSPEND */

#if SUPPORT_EARLY_SUSPEND
static void gfx_late_resume(struct early_suspend *h)
{
	ospm_late_resume();
}
#endif /* if SUPPORT_EARLY_SUSPEND */

/*
 * ospm_power_suspend
 *
 * Description: OSPM is telling our driver to suspend so save state
 * and power down all hardware.
 */
int ospm_power_suspend(struct pci_dev *pdev, pm_message_t state)
{
	int ret = 0;
	int graphics_access_count;
	int videoenc_access_count;
	int videodec_access_count;
	int display_access_count;
	struct drm_psb_private *dev_priv = gpDrmDevice->dev_private;
	unsigned long flags;
	bool hdmi_audio_suspend = true;

	if (gbSuspendInProgress || gbResumeInProgress) {
		PSB_DEBUG_PM(KERN_ALERT "%s: system BUSY\n", __func__);
		return  -EBUSY;
	}

	PSB_DEBUG_PM("enter ospm_power_suspend\n");

	mutex_lock(&g_ospm_mutex);
	if (!gbSuspended) {
		hdmi_audio_suspend = mid_hdmi_audio_suspend(dev_priv->dev);
		/* Turn on suspending first before check the access count */
		spin_lock_irqsave(&dev_priv->ospm_lock, flags);
		gbSuspendInProgress = true;
		spin_unlock_irqrestore(&dev_priv->ospm_lock, flags);

		graphics_access_count = atomic_read(&g_graphics_access_count);
		videoenc_access_count = atomic_read(&g_videoenc_access_count);
		videodec_access_count = atomic_read(&g_videodec_access_count);
		display_access_count = atomic_read(&g_display_access_count);

		if (graphics_access_count
			|| videoenc_access_count
			|| videodec_access_count
			|| display_access_count
			|| (hdmi_audio_suspend == false))
			ret = -EBUSY;
		if (!ret) {
			if (ospm_runtime_pm_msvdx_suspend(gpDrmDevice) != 0)
				ret = -EBUSY;

			if (ospm_runtime_pm_topaz_suspend(gpDrmDevice) != 0)
				ret = -EBUSY;

			if (!ret) {
				ospm_suspend_display(gpDrmDevice);

				/* When suspend, the gfx island may increase
				** its access count, hence the PCI host
				** shouldn't be power off
				*/
				spin_lock_irqsave(&dev_priv->ospm_lock, flags);
				graphics_access_count =
					atomic_read(&g_graphics_access_count);
				if (!graphics_access_count) {
					pcihostSuspendInProgress = true;
					spin_unlock_irqrestore(
						&dev_priv->ospm_lock, flags);
					ospm_suspend_pci(pdev);
					pcihostSuspendInProgress = false;
				} else {
					spin_unlock_irqrestore(
						&dev_priv->ospm_lock, flags);
					ret = -EBUSY;
				}
			}
		} else {
			PSB_DEBUG_PM("ospm_power_suspend: device busy:");
			PSB_DEBUG_PM("SGX %d Enc %d Dec %d Display %d\n",
				graphics_access_count, videoenc_access_count,
				videodec_access_count, display_access_count);
		}
		spin_lock_irqsave(&dev_priv->ospm_lock, flags);
		gbSuspendInProgress = false;
		spin_unlock_irqrestore(&dev_priv->ospm_lock, flags);
	}


	mutex_unlock(&g_ospm_mutex);
	return ret;
}

void ospm_power_graphics_island_up(int hw_islands)
{
	unsigned long flags;
	unsigned long irqflags;
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) gpDrmDevice->dev_private;


	if (hw_islands) {
		spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);
		/*
		If pmu_nc_set_power_state fails then accessing HW
		reg would result in a crash - IERR/Fabric error.
		*/
		spin_lock_irqsave(&dev_priv->ospm_lock, flags);
		PSB_DEBUG_PM("power on gfx_islands: 0x%x\n", hw_islands);
		if (pmu_nc_set_power_state(hw_islands,
					   OSPM_ISLAND_UP, APM_REG_TYPE))
			BUG();
		if (hw_islands & OSPM_GRAPHICS_ISLAND)
			atomic_inc(&g_graphics_access_count);
		g_hw_power_status_mask |= hw_islands;
		psb_irq_preinstall_graphics_islands(gpDrmDevice,
							OSPM_GRAPHICS_ISLAND);
		psb_irq_postinstall_graphics_islands(gpDrmDevice,
							OSPM_GRAPHICS_ISLAND);
		spin_unlock_irqrestore(&dev_priv->ospm_lock, flags);
		spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
	}
}

/*
 * ospm_power_island_up
 *
 * Description: Restore power to the specified island(s) (powergating)
 */
int ospm_power_island_up(int hw_islands)
{
	u32 dc_islands  = 0;
	u32 gfx_islands = hw_islands;
	unsigned long flags;
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) gpDrmDevice->dev_private;
	int ret = 0;

	if (hw_islands & OSPM_DISPLAY_ISLAND) {
		/*Power-up required islands only*/
		if (dev_priv->panel_desc & DISPLAY_A)
			dc_islands |= OSPM_DISPLAY_A_ISLAND;

		if (dev_priv->panel_desc & DISPLAY_B)
			dc_islands |= OSPM_DISPLAY_B_ISLAND;

		if (dev_priv->panel_desc & DISPLAY_C)
			dc_islands |= OSPM_DISPLAY_C_ISLAND;

		if (dev_priv->panel_desc)
			dc_islands |= OSPM_MIPI_ISLAND;

		/*
		If pmu_nc_set_power_state fails then accessing HW
		reg would result in a crash - IERR/Fabric error.
		*/
		spin_lock_irqsave(&dev_priv->ospm_lock, flags);

		PSB_DEBUG_PM("power up display islands 0x%x.\n", dc_islands);
		if (pmu_nc_set_power_state(dc_islands,
			OSPM_ISLAND_UP, OSPM_REG_TYPE))
			BUG();
		g_hw_power_status_mask |= OSPM_DISPLAY_ISLAND;
		spin_unlock_irqrestore(&dev_priv->ospm_lock, flags);

		/* handle other islands */
		gfx_islands = hw_islands & ~OSPM_DISPLAY_ISLAND;
	}

	if (gfx_islands) {
		/*
		If pmu_nc_set_power_state fails then accessing HW
		reg would result in a crash - IERR/Fabric error.
		*/
		spin_lock_irqsave(&dev_priv->ospm_lock, flags);
		PSB_DEBUG_PM("power on gfx_islands: 0x%x\n", gfx_islands);
		ret = pmu_nc_set_power_state(gfx_islands,
					   OSPM_ISLAND_UP, APM_REG_TYPE);
		if (ret) {
			PSB_DEBUG_PM("pmu_nc_set_power_state fails, ret is %d\n", ret);
			spin_unlock_irqrestore(&dev_priv->ospm_lock, flags);
			return ret;
		}
		if (gfx_islands & OSPM_GRAPHICS_ISLAND)
			atomic_inc(&g_graphics_access_count);
		g_hw_power_status_mask |= gfx_islands;
		spin_unlock_irqrestore(&dev_priv->ospm_lock, flags);
	}

	return 0;
}

/*
 * ospm_power_resume
 */
int ospm_power_resume(struct pci_dev *pdev)
{
	if (gbSuspendInProgress || gbResumeInProgress) {
		DRM_INFO("%s: suspend/resume in progress\n", __func__);
		return 0;
	}

	PSB_DEBUG_ENTRY("\n");

	mutex_lock(&g_ospm_mutex);

	gbResumeInProgress = true;

	ospm_resume_pci(pdev);

	ospm_resume_display(gpDrmDevice->pdev);
	psb_irq_preinstall_islands(gpDrmDevice, OSPM_DISPLAY_ISLAND);
	psb_irq_postinstall_islands(gpDrmDevice, OSPM_DISPLAY_ISLAND);

	gbResumeInProgress = false;

	mutex_unlock(&g_ospm_mutex);

	return 0;
}

/*
 * ospm_power_island_down_video
 *
 * Description: Cut power to the specified video island(s) (powergating)
 * If pmu_nc_set_power_state fails then accessing HW
 * reg would result in a crash - IERR/Fabric error.
 */
static void ospm_power_island_down_video(int video_islands)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) gpDrmDevice->dev_private;
	unsigned long flags;
	PSB_DEBUG_PM("MSVDX: power off video island %d.\n", video_islands);
	spin_lock_irqsave(&dev_priv->ospm_lock, flags);
	if (video_islands & OSPM_VIDEO_DEC_ISLAND) {
		if (pmu_nc_set_power_state(OSPM_VIDEO_DEC_ISLAND,
				OSPM_ISLAND_DOWN, APM_REG_TYPE))
			BUG();
		g_hw_power_status_mask &= ~OSPM_VIDEO_DEC_ISLAND;
	}

	if (video_islands & OSPM_VIDEO_ENC_ISLAND) {
		if (pmu_nc_set_power_state(OSPM_VIDEO_ENC_ISLAND,
				OSPM_ISLAND_DOWN, APM_REG_TYPE))
			BUG();
		g_hw_power_status_mask &= ~OSPM_VIDEO_ENC_ISLAND;
	}
	spin_unlock_irqrestore(&dev_priv->ospm_lock, flags);
}

void ospm_power_graphics_island_down(int hw_islands)
{
	u32 dc_islands = 0;
	u32 gfx_islands = hw_islands;

	unsigned long flags;
	unsigned long irqflags;
	struct mdfld_dsi_config *dsi_config;
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) gpDrmDevice->dev_private;

	if (gfx_islands) {
		spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);
		spin_lock_irqsave(&dev_priv->ospm_lock, flags);
		/* both graphics and GL3 based on graphics_access count */
		if (gfx_islands & OSPM_GL3_CACHE_ISLAND) {
#ifdef CONFIG_MDFD_GL3
			/*
			Make sure both GFX & Video aren't
			using GL3
			*/
			if (atomic_read(&g_graphics_access_count) ||
					(g_hw_power_status_mask &
					(OSPM_VIDEO_DEC_ISLAND |
					OSPM_VIDEO_ENC_ISLAND |
					OSPM_GRAPHICS_ISLAND)) ||
					(drm_psb_gl3_enable == 0)) {
				gfx_islands &=  ~OSPM_GL3_CACHE_ISLAND;
				if (!gfx_islands) {
					spin_unlock_irqrestore(
					&dev_priv->ospm_lock, flags);
					spin_unlock_irqrestore(
					&dev_priv->irqmask_lock, irqflags);
					return ;
				}
			}
#endif
		}
		if (gfx_islands & OSPM_GRAPHICS_ISLAND) {
			if (atomic_read(&g_graphics_access_count))
				gfx_islands &=  ~OSPM_GRAPHICS_ISLAND;
			else
				psb_irq_uninstall_graphics_islands(gpDrmDevice,
							OSPM_GRAPHICS_ISLAND);
		}

		/*
		If pmu_nc_set_power_state fails then accessing HW
		reg would result in a crash - IERR/Fabric error.
		*/
		PSB_DEBUG_PM("power off gfx/gl3 island 0x%x.\n", gfx_islands);
		g_hw_power_status_mask &= ~gfx_islands;
		if (pmu_nc_set_power_state(gfx_islands,
			OSPM_ISLAND_DOWN, APM_REG_TYPE))
			BUG();

		spin_unlock_irqrestore(&dev_priv->ospm_lock, flags);
		spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);

		/* From the test, after enter DSR level 1, only GFX island
		** has chance to power on and leave PCI host power ungated
		** Because after SGX complete a buffer, it will trigger
		** PROCESS_QUEUES command to SGX even if there are no more
		** 3D thing to do, hence power on SGX and PCI. Because there are
		** nothing remain to flip, exit_dsr doesn't be called,
		** so PCI host remain power ungated.
		** here just give another chance to enter DSR
		** Note:
		*/
#if 0
		if (gfx_islands & OSPM_GRAPHICS_ISLAND) {
			dsi_config = dev_priv->dsi_configs[0];
			mdfld_dsi_dsr_forbid(dsi_config);
			mdfld_dsi_dsr_allow(dsi_config);
		}
#endif
	}
}
EXPORT_SYMBOL(ospm_power_graphics_island_down);

/*
 * ospm_power_island_down
 *
 * Description: Cut power to the specified island(s) (powergating)
 */
void ospm_power_island_down(int hw_islands)
{
	u32 dc_islands = 0;
	u32 gfx_islands = hw_islands;
	int video_islands = hw_islands &
		(OSPM_VIDEO_DEC_ISLAND | OSPM_VIDEO_ENC_ISLAND);
	unsigned long flags;
	struct mdfld_dsi_config *dsi_config;
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) gpDrmDevice->dev_private;

	PSB_DEBUG_PM("power down hw_islands: %x\n", hw_islands);

	if (video_islands) {
		ospm_power_island_down_video(video_islands);
		hw_islands = hw_islands &
			~(OSPM_VIDEO_DEC_ISLAND | OSPM_VIDEO_ENC_ISLAND);
	}

	if (hw_islands & OSPM_DISPLAY_ISLAND) {
		/*Power gate all display islands.*/
		dc_islands |= (OSPM_DISPLAY_A_ISLAND |
				OSPM_DISPLAY_B_ISLAND |
				OSPM_DISPLAY_C_ISLAND |
				OSPM_MIPI_ISLAND);

		/*
		If pmu_nc_set_power_state fails then accessing HW
		reg would result in a crash - IERR/Fabric error.
		*/
		spin_lock_irqsave(&dev_priv->ospm_lock, flags);
		/*last chance of canceling the power off*/
		/*
		 * if (atomic_read(&g_display_access_count))
		 *	goto unlock;
		 */

		PSB_DEBUG_PM("power off display island\n");
		g_hw_power_status_mask &= ~OSPM_DISPLAY_ISLAND;
		if (pmu_nc_set_power_state(dc_islands,
					   OSPM_ISLAND_DOWN, OSPM_REG_TYPE))
			BUG();
		spin_unlock_irqrestore(&dev_priv->ospm_lock, flags);

		/* handle other islands */
		gfx_islands = hw_islands & ~OSPM_DISPLAY_ISLAND;
	}

	if (gfx_islands) {
		spin_lock_irqsave(&dev_priv->ospm_lock, flags);
		/* both graphics and GL3 based on graphics_access count */
		if (gfx_islands & OSPM_GL3_CACHE_ISLAND) {
#ifdef CONFIG_MDFD_GL3
			/*
			Make sure both GFX & Video aren't
			using GL3
			*/
			if (atomic_read(&g_graphics_access_count) ||
					(g_hw_power_status_mask &
					(OSPM_VIDEO_DEC_ISLAND |
					OSPM_VIDEO_ENC_ISLAND |
					OSPM_GRAPHICS_ISLAND)) ||
					(drm_psb_gl3_enable == 0)) {
				gfx_islands &=  ~OSPM_GL3_CACHE_ISLAND;
				if (!gfx_islands) {
					spin_unlock_irqrestore(
						&dev_priv->ospm_lock, flags);
					return ;
				}
			}
#endif
		}
		if (gfx_islands & OSPM_GRAPHICS_ISLAND) {
			if (atomic_read(&g_graphics_access_count))
				gfx_islands &=  ~OSPM_GRAPHICS_ISLAND;
		}

		/*
		If pmu_nc_set_power_state fails then accessing HW
		reg would result in a crash - IERR/Fabric error.
		*/
		PSB_DEBUG_PM("power off gfx/gl3 island 0x%x.\n", gfx_islands);
		g_hw_power_status_mask &= ~gfx_islands;
		if (pmu_nc_set_power_state(gfx_islands,
			OSPM_ISLAND_DOWN, APM_REG_TYPE))
			BUG();

		spin_unlock_irqrestore(&dev_priv->ospm_lock, flags);

		/* From the test, after enter DSR level 1, only GFX island
		** has chance to power on and leave PCI host power ungated
		** Because after SGX complete a buffer, it will trigger
		** PROCESS_QUEUES command to SGX even if there are no more
		** 3D thing to do, hence power on SGX and PCI. Because there are
		** nothing remain to flip, exit_dsr doesn't be called,
		** so PCI host remain power ungated.
		** here just give another chance to enter DSR
		** Note:
		*/
#if 0
		if (gfx_islands & OSPM_GRAPHICS_ISLAND) {
			dsi_config = dev_priv->dsi_configs[0];
			mdfld_dsi_dsr_forbid(dsi_config);
			mdfld_dsi_dsr_allow(dsi_config);
		}
#endif
	}
}
EXPORT_SYMBOL(ospm_power_island_down);

/*
 * ospm_power_is_hw_on
 *
 * Description: do an instantaneous check for if the specified islands
 * are on.  Only use this in cases where you know the g_state_change_mutex
 * is already held such as in irq install/uninstall.  Otherwise, use
 * ospm_power_using_hw_begin().
 */
bool ospm_power_is_hw_on(int hw_islands)
{
	unsigned long flags;
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) gpDrmDevice->dev_private;
	bool ret = false;
	spin_lock_irqsave(&dev_priv->ospm_lock, flags);
	ret = ((g_hw_power_status_mask & hw_islands)
			== hw_islands) ? true : false;
	spin_unlock_irqrestore(&dev_priv->ospm_lock, flags);
	return ret;
}
EXPORT_SYMBOL(ospm_power_is_hw_on);

/* For video case, we only force enable hw in process context.
 * Protected by g_ospm_mutex */
bool ospm_power_using_video_begin(int video_island)
{
	bool ret = true;
	bool island_is_on = true;
	struct pci_dev *pdev = gpDrmDevice->pdev;
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)gpDrmDevice->dev_private;
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;
	bool  already_increase = false;

	PSB_DEBUG_PM("MSVDX: need power on island 0x%x.\n", video_island);

	if (!(video_island & (OSPM_VIDEO_DEC_ISLAND | OSPM_VIDEO_ENC_ISLAND)))
		return false;
#ifdef CONFIG_GFX_RTPM
	/* if system suspend is in progress, do NOT allow system resume. if
	 * runtime_status is RPM_SUSPENDING, and here call pm_runtime_get will
	 * call rpm_resume indirectly, it causes defferred_resume be set to
	 * ture, so at the end of rpm_suspend(), rpm_resume() will be called.
	 * it will block system from entering s0ix */
	if (gbSuspendInProgress) {
		DRM_INFO("%s: suspend in progress,"
			"call pm_runtime_get_noresume\n", __func__);
		pm_runtime_get_noresume(&pdev->dev);
	} else {
		pm_runtime_get(&pdev->dev);
	}


	/* Taking this lock is very important to keep consistent with
	*runtime framework */
	spin_lock_irq(&pdev->dev.power.lock);
recheck:
	if (pdev->dev.power.runtime_status == RPM_SUSPENDING) {
		DEFINE_WAIT(wait);
		/* Wait for the other suspend running to finish */
		for (;;) {
			prepare_to_wait(&pdev->dev.power.wait_queue, &wait,
				TASK_UNINTERRUPTIBLE);
			if (pdev->dev.power.runtime_status != RPM_SUSPENDING)
				break;
			spin_unlock_irq(&pdev->dev.power.lock);
			schedule();
			spin_lock_irq(&pdev->dev.power.lock);
		}
		finish_wait(&pdev->dev.power.wait_queue, &wait);
		goto recheck;
	}
	/* Because !force_on has been done above, so here is force_on case
	**it must be process context in current code base, so it will power on
	** island defintely, so increase access_count here to prevent another
	** suspending thread run async
	*/
	switch (video_island) {
	case OSPM_VIDEO_ENC_ISLAND:
		atomic_inc(&g_videoenc_access_count);
		break;
	case OSPM_VIDEO_DEC_ISLAND:
		atomic_inc(&g_videodec_access_count);
		break;
	}
	already_increase = true;
	spin_unlock_irq(&pdev->dev.power.lock);
#endif

	/* It must be process context, will not be called in irq */
	mutex_lock(&g_ospm_mutex);

	island_is_on = ospm_power_is_hw_on(video_island);
	if (island_is_on)
		goto out;

	gbResumeInProgress = true;

	/* Because gfx island may resume pci silently,
	** so need sync with gfx
	*/
	if (ospm_resume_pci(pdev) == false) {
		ret = false;
		goto out;
	}

	switch (video_island) {
	case OSPM_VIDEO_DEC_ISLAND:
		if (!ospm_power_is_hw_on(OSPM_DISPLAY_ISLAND)) {
			/* printk(KERN_ALERT "%s power on display
			** for video decode use\n", __func__);
			*/
			ospm_resume_display(pdev);
			psb_irq_preinstall_islands(gpDrmDevice,
					OSPM_DISPLAY_ISLAND);
			psb_irq_postinstall_islands(gpDrmDevice,
					OSPM_DISPLAY_ISLAND);
		} else {
			/* printk(KERN_ALERT
			** "%s display is already on
			** for video decode use\n", __func__);
			*/
		}

		if (!ospm_power_is_hw_on(OSPM_VIDEO_DEC_ISLAND)) {
			/* printk(KERN_ALERT "%s power on video decode\n",
			** __func__);
			*/
			/*
			 * GL3 power island needs to be on for MSVDX working.
			 * We found this during enabling new MSVDX firmware
			 * uploading mechanism(by PUNIT) for Penwell D0.
			 */
#ifdef CONFIG_MDFD_GL3
			if (ospm_power_island_up(OSPM_GL3_CACHE_ISLAND | OSPM_VIDEO_DEC_ISLAND)) {
#else
			if (ospm_power_island_up(OSPM_VIDEO_DEC_ISLAND)) {
#endif
				ret = false;
				goto out;
			}
			if (msvdx_priv->fw_loaded_by_punit) {
				int reg_ret;
				reg_ret = psb_wait_for_register(dev_priv,
					MSVDX_COMMS_SIGNATURE,
					MSVDX_COMMS_SIGNATURE_VALUE,
					0xffffffff, 2000000, 5);
				if (reg_ret)
					PSB_DEBUG_WARN("WARN: load fw fail,\n"
					"MSVDX_COMMS_SIGNATURE reg is 0x%x,"
					"MSVDX_COMMS_FW_STATUS reg is 0x%x,"
					"MTX_ENABLE reg is 0x%x.\n",
					PSB_RMSVDX32(MSVDX_COMMS_SIGNATURE),
					PSB_RMSVDX32(MSVDX_COMMS_FW_STATUS),
					PSB_RMSVDX32(MTX_ENABLE_OFFSET));
			}
			ospm_runtime_pm_msvdx_resume(gpDrmDevice);
			psb_irq_preinstall_islands(gpDrmDevice,
				OSPM_VIDEO_DEC_ISLAND);
			psb_irq_postinstall_islands(gpDrmDevice,
				OSPM_VIDEO_DEC_ISLAND);
		} else {
#ifdef CONFIG_MDFD_GL3
			if (ospm_power_island_up(OSPM_GL3_CACHE_ISLAND)) {
				ret = false;
				goto out;
			}
#endif
		}

		break;
	case OSPM_VIDEO_ENC_ISLAND:
		if (IS_MRST(gpDrmDevice) &&
				(!ospm_power_is_hw_on(
					OSPM_DISPLAY_ISLAND))) {
			ospm_resume_display(pdev);
			psb_irq_preinstall_islands(gpDrmDevice,
				OSPM_DISPLAY_ISLAND);
			psb_irq_postinstall_islands(gpDrmDevice,
				OSPM_DISPLAY_ISLAND);
		}

		if (!ospm_power_is_hw_on(OSPM_VIDEO_ENC_ISLAND)) {
			/* printk(KERN_ALERT "%s power on video
			** encode\n", __func__);
			*/
#ifdef CONFIG_MDFD_GL3
			if (ospm_power_island_up(OSPM_VIDEO_ENC_ISLAND | OSPM_GL3_CACHE_ISLAND)) {
#else
			if (ospm_power_island_up(OSPM_VIDEO_ENC_ISLAND)) {
#endif
				ret = false;
				goto out;
			}
			ospm_runtime_pm_topaz_resume(gpDrmDevice);
			psb_irq_preinstall_islands(gpDrmDevice,
				OSPM_VIDEO_ENC_ISLAND);
			psb_irq_postinstall_islands(gpDrmDevice,
				OSPM_VIDEO_ENC_ISLAND);
		} else {
#ifdef CONFIG_MDFD_GL3
			if (ospm_power_island_up(OSPM_GL3_CACHE_ISLAND)) {
				ret = false;
				goto out;
			}
#endif
		}
		break;
	default:
		printk(KERN_ALERT "%s unknown island !!!!\n",
				__func__);
		break;
	}

out:
	if (!ret)
		printk(KERN_ALERT "%s: %d failed\n",
				__func__, video_island);

	gbResumeInProgress = false;

	if (ret) {
		if (!already_increase) {
			switch (video_island) {
			case OSPM_VIDEO_ENC_ISLAND:
				atomic_inc(&g_videoenc_access_count);
				break;
			case OSPM_VIDEO_DEC_ISLAND:
				atomic_inc(&g_videodec_access_count);
				break;
			}
		}
	}
#ifdef CONFIG_GFX_RTPM
	else {
		pm_runtime_put(&pdev->dev);
	}
#endif
	mutex_unlock(&g_ospm_mutex);
	return ret;
}

/*
 * ospm_power_using_hw_begin
 *
 * Description: Notify PowerMgmt module that you will be accessing the
 * specified island's hw so don't power it off.  If force_on is true,
 * this will power on the specified island if it is off.
 * Otherwise, this will return false and the caller is expected to not
 * access the hw.
 *
 * NOTE:The function doesn't support force_on in atomic context,
 * as there may sleep when resuming these islands. If u have to
 * resume in atomic context for these islands, u need revise ur
 * logic and move the resume to a process context. return true if
 * the island is on(no matter it's forced or already on) otherwise
 * false is returned.
 */
bool ospm_power_using_hw_begin(int hw_island, UHBUsage usage)
{
	bool ret = true;
	bool island_is_on = true;
	struct pci_dev *pdev = gpDrmDevice->pdev;
	IMG_UINT32 deviceID = 0;
	bool force_on = usage ? true : false;
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) gpDrmDevice->dev_private;
	unsigned long flags;

	if (!(hw_island & (OSPM_GRAPHICS_ISLAND | OSPM_DISPLAY_ISLAND |
		OSPM_GL3_CACHE_ISLAND)))
		return false;

#ifdef CONFIG_GFX_RTPM
	if (force_on)
		pm_runtime_get_sync(&pdev->dev);
	else
		pm_runtime_get_noresume(&pdev->dev);
#endif


	/* Only process context is allowed when in
	** force_on == true case. In force_on == false cases,
	** it may be in atomic or process context, so use spin_lock_irq
	*/
	if (!force_on) {
		spin_lock_irqsave(&dev_priv->ospm_lock, flags);
		island_is_on = ((g_hw_power_status_mask & hw_island)
			== hw_island) ? true : false;

		/* if island is off or another thread has been in
		** suspend progress, we should return false to
		** keep consistent, the caller should have dealt
		** with return value properly
		** Note: when in interrupt context, we should
		** always return true, for it has triggerred interrupt.
		*/
		if ((!island_is_on) ||
			((((hw_island == OSPM_DISPLAY_ISLAND) &&
			gbSuspendInProgress) ||
			((hw_island == OSPM_GRAPHICS_ISLAND) &&
			pcihostSuspendInProgress)) && (!in_interrupt()))) {
			spin_unlock_irqrestore(&dev_priv->ospm_lock,
				flags);
#ifdef CONFIG_GFX_RTPM
			pm_runtime_put(&pdev->dev);
#endif
			return false;
		}

		/* After sanity check can increase the access_count */
		if (hw_island == OSPM_DISPLAY_ISLAND)
			atomic_inc(&g_display_access_count);
		else if (hw_island == OSPM_GRAPHICS_ISLAND)
			atomic_inc(&g_graphics_access_count);

		spin_unlock_irqrestore(&dev_priv->ospm_lock, flags);
		return true;
	}

	/* Actually we can remove the codes below for following
	** mutex lock can keep race condition safe. These codes
	** exist only for facilitating gfx misuse force_on display
	** while taking spin lock. These codes can safely be removed
	** once gfx doesn't force on engines when taking spinlock
	*/
	spin_lock_irqsave(&dev_priv->ospm_lock, flags);
	island_is_on = ((g_hw_power_status_mask & hw_island)
			== hw_island) ? true : false;
	if (island_is_on && force_on &&
		((hw_island == OSPM_DISPLAY_ISLAND) &&
			!gbSuspendInProgress)) {
		atomic_inc(&g_display_access_count);
		spin_unlock_irqrestore(&dev_priv->ospm_lock, flags);
		return true;
	}
	spin_unlock_irqrestore(&dev_priv->ospm_lock, flags);

	BUG_ON(in_interrupt());
	mutex_lock(&g_ospm_mutex);

	spin_lock_irqsave(&dev_priv->ospm_lock, flags);
	island_is_on = ((g_hw_power_status_mask & hw_island)
			== hw_island) ? true : false;

	if (island_is_on && (hw_island == OSPM_GRAPHICS_ISLAND))
		atomic_inc(&g_graphics_access_count);

	spin_unlock_irqrestore(&dev_priv->ospm_lock, flags);

	if (island_is_on)
		goto increase_count;

	gbResumeInProgress = true;

	/* Because gfx island may resume pci silently,
	** so need sync with gfx
	*/
	ret = ospm_resume_pci(pdev);

	if (ret) {
		if (hw_island == OSPM_DISPLAY_ISLAND) {
			deviceID = gui32MRSTDisplayDeviceID;
			ospm_resume_display(pdev);
			psb_irq_preinstall_islands(gpDrmDevice,
				OSPM_DISPLAY_ISLAND);
			psb_irq_postinstall_islands(gpDrmDevice,
				OSPM_DISPLAY_ISLAND);
		} else if (hw_island == OSPM_GRAPHICS_ISLAND) {
				deviceID = gui32SGXDeviceID;
#ifdef CONFIG_MDFD_GL3
				ospm_power_graphics_island_up(
						OSPM_GRAPHICS_ISLAND |
						OSPM_GL3_CACHE_ISLAND);
#else
				ospm_power_graphics_island_up(
						OSPM_GRAPHICS_ISLAND);
#endif
		}
	}

	if (!ret)
		DRM_INFO("%s: %d failed\n", __func__, hw_island);

	gbResumeInProgress = false;

increase_count:
	if (ret) {
		if (hw_island == OSPM_DISPLAY_ISLAND)
			atomic_inc(&g_display_access_count);
	}
#ifdef CONFIG_GFX_RTPM
	else
		pm_runtime_put(&pdev->dev);
#endif
	mutex_unlock(&g_ospm_mutex);

	return ret;
}
EXPORT_SYMBOL(ospm_power_using_hw_begin);

/*
 * ospm_power_using_video_end
 *
 * Description: Notify PowerMgmt module that you are done accessing the
 * specified video island's hw so feel free to power it off.  Note that this
 * function doesn't actually power off the islands.
 */
void ospm_power_using_video_end(int video_island)
{
	PSB_DEBUG_PM("MSVDX: using video 0x%x end.\n", video_island);

	if (!(video_island & (OSPM_VIDEO_ENC_ISLAND | OSPM_VIDEO_DEC_ISLAND)))
		return;

	switch (video_island) {
	case OSPM_VIDEO_ENC_ISLAND:
		if (atomic_read(&g_videoenc_access_count) <= 0)
			DRM_ERROR("g_videoenc_access_count <=0.\n");
		else
			atomic_dec(&g_videoenc_access_count);
		break;
	case OSPM_VIDEO_DEC_ISLAND:
		if (atomic_read(&g_videodec_access_count) <= 0)
			DRM_ERROR("g_videodec_access_count <=0.\n");
		else
			atomic_dec(&g_videodec_access_count);
		break;
	}

#ifdef CONFIG_GFX_RTPM
	/* decrement runtime pm ref count */
	pm_runtime_put(&gpDrmDevice->pdev->dev);
#endif
}

/*
 * ospm_power_using_hw_end
 *
 * Description: Notify PowerMgmt module that you are done accessing the
 * specified island's hw so feel free to power it off.  Note that this
 * function doesn't actually power off the islands.
 */
void ospm_power_using_hw_end(int hw_island)
{
	if (!(hw_island & (OSPM_GRAPHICS_ISLAND | OSPM_DISPLAY_ISLAND |
		OSPM_GL3_CACHE_ISLAND)))
		return;

	switch (hw_island) {
	case OSPM_GRAPHICS_ISLAND:
		atomic_dec(&g_graphics_access_count);
		break;
	case OSPM_DISPLAY_ISLAND:
		atomic_dec(&g_display_access_count);
		break;
	}

#ifdef CONFIG_GFX_RTPM
	/* decrement runtime pm ref count */
	pm_runtime_put(&gpDrmDevice->pdev->dev);
#endif

	WARN_ON(atomic_read(&g_graphics_access_count) < 0);
	WARN_ON(atomic_read(&g_display_access_count) < 0);
}
EXPORT_SYMBOL(ospm_power_using_hw_end);

int ospm_runtime_pm_allow(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv;
	struct mdfld_dsi_config **dsi_configs;
	bool panel_on = false, panel_on2 = false;

	PSB_DEBUG_ENTRY("\n");

	dev_priv = (struct drm_psb_private *)dev->dev_private;
	dsi_configs = dev_priv->dsi_configs;

	if (dev_priv->rpm_enabled)
		return 0;

	if (dsi_configs[0])
		panel_on = dsi_configs[0]->dsi_hw_context.panel_on;
	if (dsi_configs[1])
		panel_on2 = dsi_configs[1]->dsi_hw_context.panel_on;

#ifdef CONFIG_GFX_RTPM
	if (!panel_on && !panel_on2) {
		pm_runtime_allow(&dev->pdev->dev);
		dev_priv->rpm_enabled = 1;
		DRM_INFO("Runtime PM enabled\n");
	}
#endif

	return 0;
}

void ospm_runtime_pm_forbid(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;

	PSB_DEBUG_ENTRY("\n");

#ifdef CONFIG_GFX_RTPM
	pm_runtime_forbid(&dev->pdev->dev);
#endif
	dev_priv->rpm_enabled = 0;
}

int psb_runtime_suspend(struct device *dev)
{
	pm_message_t state;
	int ret = 0;

	state.event = 0;

	PSB_DEBUG_PM("psb_runtime_suspend is called.\n");

	if (atomic_read(&g_graphics_access_count) ||
		atomic_read(&g_videoenc_access_count) ||
		(gbdispstatus == true) ||
		atomic_read(&g_videodec_access_count) ||
		atomic_read(&g_display_access_count))
		return -EBUSY;
	else
		ret = ospm_power_suspend(gpDrmDevice->pdev, state);

	return ret;
}

int psb_runtime_resume(struct device *dev)
{
	PSB_DEBUG_ENTRY("\n");

	/* Nop for GFX */
	return 0;
}

int psb_runtime_idle(struct device *dev)
{
	struct drm_psb_private *dev_priv = gpDrmDevice->dev_private;
	bool hdmi_audio_busy = false;

	PSB_DEBUG_ENTRY("\n");

	hdmi_audio_busy = mid_hdmi_audio_is_busy(dev_priv->dev);

	if (atomic_read(&g_graphics_access_count) ||
		atomic_read(&g_videoenc_access_count) ||
		atomic_read(&g_videodec_access_count) ||
		atomic_read(&g_display_access_count) ||
		(gbdispstatus == true) ||
		(hdmi_audio_busy == true))
		return -EBUSY;
	else
		return 0;
}


