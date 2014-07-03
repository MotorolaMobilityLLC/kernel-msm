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
 *
 */
#ifndef _PSB_POWERMGMT_H_
#define _PSB_POWERMGMT_H_

#include <linux/pci.h>
#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <linux/intel_mid_pm.h>

#define OSPM_GRAPHICS_ISLAND	APM_GRAPHICS_ISLAND
#ifndef CONFIG_DRM_VXD_BYT
#define OSPM_VIDEO_DEC_ISLAND	APM_VIDEO_DEC_ISLAND
#endif
#define OSPM_VIDEO_ENC_ISLAND	APM_VIDEO_ENC_ISLAND
#define OSPM_GL3_CACHE_ISLAND	APM_GL3_CACHE_ISLAND
#define OSPM_DISPLAY_ISLAND	0x40

#ifdef CONFIG_MDFD_GL3
#define OSPM_ALL_ISLANDS	((OSPM_GRAPHICS_ISLAND) |\
				(OSPM_VIDEO_ENC_ISLAND) |\
				(OSPM_VIDEO_DEC_ISLAND) |\
				(OSPM_GL3_CACHE_ISLAND) |\
				(OSPM_DISPLAY_ISLAND))
#else
#define OSPM_ALL_ISLANDS	((OSPM_GRAPHICS_ISLAND) |\
				(OSPM_VIDEO_ENC_ISLAND) |\
				(OSPM_VIDEO_DEC_ISLAND) |\
				(OSPM_DISPLAY_ISLAND))
#endif
/* IPC message and command defines used to enable/disable mipi panel voltages */
#define IPC_MSG_PANEL_ON_OFF    0xE9
#define IPC_CMD_PANEL_ON        1
#define IPC_CMD_PANEL_OFF       0

/* Panel presence */
#define DISPLAY_A 0x1
#define DISPLAY_B 0x2
#define DISPLAY_C 0x4

extern bool gbSuspended;
extern int lastFailedBrightness;
extern struct drm_device *gpDrmDevice;

typedef enum _UHBUsage {
    OSPM_UHB_ONLY_IF_ON = 0,
    OSPM_UHB_FORCE_POWER_ON,
} UHBUsage;

struct mdfld_dsi_config;
void mdfld_save_display(struct drm_device *dev);
void mdfld_dsi_dpi_set_power(struct drm_encoder *encoder, bool on);
void mdfld_dsi_dbi_set_power(struct drm_encoder *encoder, bool on);

#ifndef CONFIG_DRM_VXD_BYT
/* extern int psb_check_msvdx_idle(struct drm_device *dev); */
/* Use these functions to power down video HW for D0i3 purpose  */
void ospm_apm_power_down_msvdx(struct drm_device *dev, int force_on);
void ospm_apm_power_down_topaz(struct drm_device *dev);
#endif

void ospm_power_init(struct drm_device *dev);
void ospm_post_init(struct drm_device *dev);
void ospm_power_uninit(void);
void ospm_subsystem_no_gating(struct drm_device *dev, int subsystem);
void ospm_subsystem_power_gate(struct drm_device *dev, int subsystem);

/*
 * OSPM will call these functions
 */
int ospm_power_suspend(struct pci_dev *pdev, pm_message_t state);
int ospm_power_resume(struct pci_dev *pdev);

/*
 * These are the functions the driver should use to wrap all hw access
 * (i.e. register reads and writes)
 */
bool ospm_power_using_hw_begin(int hw_island, UHBUsage usage);
void ospm_power_using_hw_end(int hw_island);

#ifndef CONFIG_DRM_VXD_BYT
bool ospm_power_using_video_begin(int hw_island);
void ospm_power_using_video_end(int hw_island);
#endif

/*
 * Use this function to do an instantaneous check for if the hw is on.
 * Only use this in cases where you know the g_state_change_mutex
 * is already held such as in irq install/uninstall and you need to
 * prevent a deadlock situation.  Otherwise use ospm_power_using_hw_begin().
 */
bool ospm_power_is_hw_on(int hw_islands);

/*
 * Power up/down different hw component rails/islands
 */
void mdfld_save_display(struct drm_device *dev);
void ospm_power_island_down(int hw_islands);
int ospm_power_island_up(int hw_islands);
void ospm_suspend_graphics(void);
void ospm_power_graphics_island_down(int hw_islands);
void ospm_power_graphics_island_up(int hw_islands);

/*
 * GFX-Runtime PM callbacks
 */
int psb_runtime_suspend(struct device *dev);
int psb_runtime_resume(struct device *dev);
int psb_runtime_idle(struct device *dev);
int ospm_runtime_pm_allow(struct drm_device *dev);
void ospm_runtime_pm_forbid(struct drm_device *dev);
void acquire_ospm_lock(void);
void release_ospm_lock(void);


/*
 * If vec/ved/gfx are idle, submit a request to execute the subsystem-level
 * idle callback for the device.
 */
#ifdef CONFIG_GFX_RTPM
extern void psb_ospm_post_power_down(void);
#endif
#endif /*_PSB_POWERMGMT_H_*/
