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

#include "psb_drv.h"
#include "dc_ospm.h"
#include "pmu_tng.h"
#include "tng_wa.h"
#include "mrfld_clock.h"
#include <asm/intel-mid.h>

/***********************************************************
+ * Sideband implementation
+ ***********************************************************/
#define	SB_PCKT		0x2100
#define	SB_DATA		0x2104
#define	SB_ADDR		0x2108
#define	SB_STATUS	0x210C

#define MIO_SB_ADDR	0x3b
#define	MIO_ON		0x00
#define	MIO_OFF		0x03

extern struct drm_device *gpDrmDevice;

void sb_write_packet(bool pwr_on)
{
	struct drm_device *dev = gpDrmDevice;
	u32 ulData = MIO_ON;

	if (!pwr_on)
		ulData = MIO_OFF;

	REG_WRITE(SB_ADDR, MIO_SB_ADDR);
	REG_WRITE(SB_DATA, ulData);
	REG_WRITE(SB_PCKT, 0x00070410);

}


/***********************************************************
 * display A Island implementation
 ***********************************************************/

/**
 * disp_a_power_up
 *
 * Power up island : return true if success
 */
static bool disp_a_power_up(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	bool ret;

	enable_HFPLL(dev);

#ifndef USE_GFX_INTERNAL_PM_FUNC
	ret = pmu_nc_set_power_state(PMU_DISP_A, OSPM_ISLAND_UP, DSP_SS_PM);
#else
	ret = pmu_set_power_state_tng(DSP_SS_PM, DPA_SSC, TNG_COMPOSITE_I0);
#endif

	/*
	 * This workarounds are only needed for TNG A0/A1 silicon.
	 * Any TNG SoC which is newer than A0/A1 won't need this.
	 */
	if (IS_TNG_A0(dev))
	{
		if (!ret)
			apply_TNG_A0_workarounds(OSPM_DISPLAY_ISLAND, 0);
	}

	PSB_DEBUG_PM("Power on island %x, returned %d\n", p_island->island, ret);

#ifdef CONFIG_SUPPORT_MIPI
	/* FIXME: Can we move dpst out of ospm code? */
	psb_dpst_diet_restore(dev);
#endif
	return !ret;
}

/**
 * disp_a_power_down
 *
 * Power down island : return true if success
 */
static bool disp_a_power_down(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	bool ret;

#ifdef CONFIG_SUPPORT_MIPI
	psb_dpst_diet_save(dev);
#endif

#ifndef USE_GFX_INTERNAL_PM_FUNC
	ret = pmu_nc_set_power_state(PMU_DISP_A, OSPM_ISLAND_DOWN, DSP_SS_PM);
#else
	ret = pmu_set_power_state_tng(DSP_SS_PM, DPA_SSC, TNG_COMPOSITE_D3);
#endif

	PSB_DEBUG_PM("Power off island %x, returned %d\n", p_island->island, ret);

	return !ret;
}

/**
 * ospm_disp_a_init
 *
 * initilize
 */
void ospm_disp_a_init(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	p_island->p_funcs->power_up = disp_a_power_up;
	p_island->p_funcs->power_down = disp_a_power_down;
	p_island->p_dependency = NULL;
}

/***********************************************************
 * display B Island implementation
 ***********************************************************/
/**
 * disp_b_power_up
 *
 * Power up island : return true if success
 */
static bool disp_b_power_up(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	bool ret;

#ifndef USE_GFX_INTERNAL_PM_FUNC
	ret = pmu_nc_set_power_state(PMU_DISP_B, OSPM_ISLAND_UP, DSP_SS_PM);
#else
	ret = pmu_set_power_state_tng(DSP_SS_PM, DPB_SSC, TNG_COMPOSITE_I0);
#endif
	PSB_DEBUG_PM("Power on island %x, returned %d\n", p_island->island, ret);

	return !ret;
}

/**
 * disp_b_power_down
 *
 * Power down island : return true if success
 */
static bool disp_b_power_down(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	bool ret;

#ifndef USE_GFX_INTERNAL_PM_FUNC
	ret = pmu_nc_set_power_state(PMU_DISP_B, OSPM_ISLAND_DOWN, DSP_SS_PM);
#else
	ret = pmu_set_power_state_tng(DSP_SS_PM, DPB_SSC, TNG_COMPOSITE_D3);
#endif
	PSB_DEBUG_PM("Power off island %x, returned %d\n", p_island->island, ret);

	return !ret;
}

/**
 * ospm_disp_b_init
 *
 * initilize
 */
void ospm_disp_b_init(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	p_island->p_funcs->power_up = disp_b_power_up;
	p_island->p_funcs->power_down = disp_b_power_down;
	p_island->p_dependency = get_island_ptr(OSPM_DISPLAY_A);
}

/***********************************************************
 * display C Island implementation
 ***********************************************************/
/**
 * disp_c_power_up
 *
 * Power up island : return true if success
 */
static bool disp_c_power_up(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	bool ret;

#ifndef USE_GFX_INTERNAL_PM_FUNC
	ret = pmu_nc_set_power_state(PMU_DISP_C, OSPM_ISLAND_UP, DSP_SS_PM);
#else
	ret = pmu_set_power_state_tng(DSP_SS_PM, DPC_SSC, TNG_COMPOSITE_I0);
#endif

	PSB_DEBUG_PM("Power on island %x, returned %d\n", p_island->island, ret);

	return !ret;
}

/**
 * disp_c_power_down
 *
 * Power down island : return true if success
 */
static bool disp_c_power_down(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	bool ret;

#ifndef USE_GFX_INTERNAL_PM_FUNC
	ret = pmu_nc_set_power_state(PMU_DISP_C, OSPM_ISLAND_DOWN, DSP_SS_PM);
#else
	ret = pmu_set_power_state_tng(DSP_SS_PM, DPC_SSC, TNG_COMPOSITE_D3);
#endif

	PSB_DEBUG_PM("Power off island %x, returned %d\n", p_island->island, ret);

	return !ret;
}

/**
 * ospm_disp_c_init
 *
 * initilize
 */
void ospm_disp_c_init(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	p_island->p_funcs->power_up = disp_c_power_up;
	p_island->p_funcs->power_down = disp_c_power_down;
	p_island->p_dependency = get_island_ptr(OSPM_DISPLAY_A);
}

/***********************************************************
 * display MIO Island implementation
 ***********************************************************/
/**
 * mio_power_up
 *
 * Power up island : return true if success
 */
static bool mio_power_up(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	bool ret = false;

#ifdef CONFIG_SUPPORT_MIPI
	if (!enable_DSIPLL(dev)){
		DRM_ERROR("Not Powering up MIO since DSI PLL could not be locked");
		return ret;
	}
#endif

	if (IS_TNG_A0(dev))
	{
		sb_write_packet(true);
		udelay(50);
		sb_write_packet(false);
		udelay(50);
		sb_write_packet(true);
		udelay(50);
		PSB_DEBUG_PM("%s:using sideband to powerup MIO\n", __func__);
	} else {
#ifndef USE_GFX_INTERNAL_PM_FUNC
	ret = pmu_nc_set_power_state(PMU_MIO, OSPM_ISLAND_UP, MIO_SS_PM);
	ret = pmu_nc_set_power_state(PMU_MIO, OSPM_ISLAND_DOWN, MIO_SS_PM);
	ret = pmu_nc_set_power_state(PMU_MIO, OSPM_ISLAND_UP, MIO_SS_PM);
#else
	ret = pmu_set_power_state_tng(MIO_SS_PM, MIO_SSC, TNG_COMPOSITE_I0);
	ret = pmu_set_power_state_tng(MIO_SS_PM, MIO_SSC, TNG_COMPOSITE_D3);
	ret = pmu_set_power_state_tng(MIO_SS_PM, MIO_SSC, TNG_COMPOSITE_I0);
#endif
	}

	PSB_DEBUG_PM("Power on island %x, returned %d\n", p_island->island, ret);

	return !ret;
}

/**
 * mio_power_down
 *
 * Power down island : return true if success
 */
static bool mio_power_down(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	bool ret;
#ifdef CONFIG_SUPPORT_MIPI
	if (!disable_DSIPLL(dev)){
		DRM_ERROR("Skipping MIO power down ad DSI PLL could not be unlocked\n");
		return false;
	}
#endif

#ifndef USE_GFX_INTERNAL_PM_FUNC
	ret = pmu_nc_set_power_state(PMU_MIO, OSPM_ISLAND_DOWN, MIO_SS_PM);
#else
	ret = pmu_set_power_state_tng(MIO_SS_PM, MIO_SSC, TNG_COMPOSITE_D3);
#endif

	PSB_DEBUG_PM("Power off island %x, returned %d\n", p_island->island, ret);

	return !ret;
}

/**
 * ospm_mio_init
 *
 * initilize
 */
void ospm_mio_init(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	p_island->p_funcs->power_up = mio_power_up;
	p_island->p_funcs->power_down = mio_power_down;
	p_island->p_dependency = get_island_ptr(OSPM_DISPLAY_A);
}

/***********************************************************
 * display HDMI Island implementation
 ***********************************************************/
/**
 * hdmi_power_up
 *
 * Power up island : return true if success
 */
static bool hdmi_power_up(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	bool ret;

#ifndef USE_GFX_INTERNAL_PM_FUNC
	ret = pmu_nc_set_power_state(PMU_HDMI, OSPM_ISLAND_UP, HDMIO_SS_PM);
#else
	ret = pmu_set_power_state_tng(HDMIO_SS_PM, HDMIO_SSC, TNG_COMPOSITE_I0);
#endif

	PSB_DEBUG_PM("Power on island %x, returned %d\n", p_island->island, ret);

	return !ret;
}

/**
 * hdmi_power_down
 *
 * Power down island : return true if success
 */
static bool hdmi_power_down(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	bool ret;

#ifndef USE_GFX_INTERNAL_PM_FUNC
	ret = pmu_nc_set_power_state(PMU_HDMI, OSPM_ISLAND_DOWN, HDMIO_SS_PM);
#else
	ret = pmu_set_power_state_tng(HDMIO_SS_PM, HDMIO_SSC, TNG_COMPOSITE_D3);
#endif
	PSB_DEBUG_PM("Power off island %x, returned %d\n", p_island->island, ret);

	return !ret;
}

/**
 * ospm_hdmi_init
 *
 * initilize
 */
void ospm_hdmi_init(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	p_island->p_funcs->power_up = hdmi_power_up;
	p_island->p_funcs->power_down = hdmi_power_down;
	p_island->p_dependency = get_island_ptr(OSPM_DISPLAY_A);
}
