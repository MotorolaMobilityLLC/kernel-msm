/**************************************************************************
 * Copyright (c) 2012, Intel Corporation.
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
 */

#ifndef __INTEL_MEDIA_ISLAND_MANAGEMENT_H__
#define __INTEL_MEDIA_ISLAND_MANAGEMENT_H__

#include <linux/types.h>
#include <drm/drmP.h>
#include <linux/intel_mid_pm.h>
#include "gfx_rtpm.h"
#include "pmu_tng.h"

/* TNG Power Islands defination
 * Subsystem status bits for NC_PM_SSS.  Status of all North Cluster IPs.
 * These correspond to the bits below.
 */
#define	OSPM_DISPLAY_A		NC_PM_SSS_DPA
#define	OSPM_DISPLAY_B		NC_PM_SSS_DPB
#define	OSPM_DISPLAY_C		NC_PM_SSS_DPC
#define	OSPM_DISPLAY_MIO	NC_PM_SSS_MIO
#define	OSPM_DISPLAY_HDMI	NC_PM_SSS_HDMIO
#define	OSPM_VIDEO_VPP_ISLAND	NC_PM_SSS_VSP
#define	OSPM_VIDEO_DEC_ISLAND	NC_PM_SSS_VED
#define	OSPM_VIDEO_ENC_ISLAND	NC_PM_SSS_VEC

enum POWER_ISLAND_STATE {
	OSPM_POWER_OFF = 0,	/* power island/device OFF */
	OSPM_POWER_ON,		/* power island/device ON */
};

/* All Graphics Islands */
#define	OSPM_GRAPHICS_ISLAND	NC_PM_SSS_GFX_RSCD

#ifdef CONFIG_MOOREFIELD
#define	OSPM_SIDEKICK_ISLAND	NC_PM_SSS_GFX_SDKCK
#define	OSPM_SLC_ISLAND		NC_PM_SSS_GFX_SLC
#define	OSPM_SLC_LDO_ISLAND	NC_PM_SSS_GFX_SLC_LDO
#else
/* All SLC Islands */
#define	OSPM_SLC_ISLAND	(NC_PM_SSS_GFX_SLC | \
				NC_PM_SSS_GFX_SDKCK | \
				NC_PM_SSS_GFX_SLC_LDO)
#endif

/* All Display Islands */
#define OSPM_DISPLAY_ISLAND	(OSPM_DISPLAY_A |\
				OSPM_DISPLAY_B |\
				OSPM_DISPLAY_C |\
				OSPM_DISPLAY_MIO |\
				OSPM_DISPLAY_HDMI)

/* All Video Islands */
#define OSPM_VIDEO_ISLAND	(OSPM_VIDEO_VPP_ISLAND |\
				OSPM_VIDEO_DEC_ISLAND |\
				OSPM_VIDEO_ENC_ISLAND)

/* All Island for Intel Media */
#define OSPM_ALL_ISLANDS	(OSPM_GRAPHICS_ISLAND |\
				OSPM_VIDEO_ISLAND |\
				OSPM_DISPLAY_ISLAND)

struct power_ops;

/* Generic defination of a power island */
struct ospm_power_island {
	/* bit for identifying power island */
	u32 island;
	/* power state of the island state */
	enum POWER_ISLAND_STATE island_state;
	/* Ref count for the Power island */
	atomic_t ref_count;
	void (*init_func)(struct drm_device *, struct ospm_power_island *);
	/* power island up/down functions */
	/* Function pointer must be initilized in init_func above */
	struct power_ops *p_funcs;

	/* we MUST power/on off the dependent island */
	/* before modifying power state of "this" island */
	/* dependency (if any) must be set in the init function */
	struct ospm_power_island *p_dependency;
};

/* power island up/down functions */
struct power_ops {
	bool (*power_up)(struct drm_device *,
			struct ospm_power_island *);
	bool (*power_down)(struct drm_device *,
			struct ospm_power_island *);
};

struct _ospm_data_ {
	struct mutex ospm_lock;

	/* drm device */
	struct drm_device	*dev;
};

/* get pointer to the island */
/* use it to get array item for setting dependency */
struct ospm_power_island *get_island_ptr(u32 hw_island);
bool ospm_power_suspend(void);
void ospm_power_resume(void);
void ospm_power_init(struct drm_device *dev);
void ospm_power_uninit(void);

/* Power up */
bool power_island_get(u32 hw_island);
/* Power down */
bool power_island_put(u32 hw_island);

/* Check the state of the island */
bool is_island_on(u32 hw_island);

/* Get Display island from the pipe */
u32 pipe_to_island(u32 pipe);

/* FIXME: hkpatel */
/*** LEGACY SUPPORT ****/
/*** REMOVE ONCE CONVERTED ALL FUNCTIONS TO NEW ARCH */

/* Panel presence */
#define DISPLAY_A	0x1
#define DISPLAY_B	0x2
#define DISPLAY_C	0x4

#define OSPM_UHB_ONLY_IF_ON        0
#define OSPM_UHB_FORCE_POWER_ON    1

/* Legacy Function for support */
bool ospm_power_using_hw_begin(int hw_island, u32 usage);
void ospm_power_using_hw_end(int hw_island);

void ospm_apm_power_down_msvdx(struct drm_device *dev, int on);
bool ospm_power_is_hw_on(u32 hw_island);
void ospm_apm_power_down_topaz(struct drm_device *dev);
void ospm_apm_power_down_vsp(struct drm_device *dev);
int ospm_runtime_pm_allow(struct drm_device *dev);
void ospm_runtime_pm_forbid(struct drm_device *dev);

#endif		/* __INTEL_MEDIA_ISLAND_MANAGEMENT_H__ */
