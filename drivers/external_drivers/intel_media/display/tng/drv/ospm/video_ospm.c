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
#include "pmu_tng.h"
#include "video_ospm.h"
#include "vsp.h"
#include "psb_msvdx.h"
#include "tng_topaz.h"
#include "tng_wa.h"
#include <asm/intel-mid.h>
#include "pmu_tng.h"

static bool need_set_ved_freq = true;

static int pm_cmd_freq_get(u32 reg_freq);
static int pm_cmd_freq_set(u32 reg_freq, u32 freq_code, u32 *p_freq_code_rlzd);
static int pm_cmd_freq_wait(u32 reg_freq, u32 *freq_code_rlzd);
static void pm_cmd_power_set(int pm_reg, int pm_mask) __attribute__((unused));

static void vsp_set_max_frequency(struct drm_device *dev);
static void vsp_set_default_frequency(struct drm_device *dev);

extern struct drm_device *gpDrmDevice;
/***********************************************************
 * vsp islands
 ***********************************************************/
/**
 * vsp_power_up
 *
 * Power up island
 */
static bool vsp_power_up(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	bool ret = true;
	int pm_ret = 0;

	if (p_island->island_state == OSPM_POWER_ON)
		return true;
	/*
	 * This workarounds are only needed for TNG A0/A1 silicon.
	 * Any TNG SoC which is newer than A0/A1 won't need this.
	 */
	if (IS_TNG_A0(dev))
	{
		apply_TNG_A0_workarounds(OSPM_VIDEO_VPP_ISLAND, 1);
	}

#ifndef USE_GFX_INTERNAL_PM_FUNC
	pm_ret = pmu_nc_set_power_state(PMU_VPP, OSPM_ISLAND_UP, VSP_SS_PM0);
#else
	pm_ret = pmu_set_power_state_tng(VSP_SS_PM0, VSP_SSC, TNG_COMPOSITE_I0);
#endif

	if (pm_ret) {
		PSB_DEBUG_PM("VSP: pmu_nc_set_power_state ON failed!\n");
		return false;
	}

	if (drm_vsp_burst)
		vsp_set_max_frequency(dev);

	psb_irq_preinstall_islands(dev, OSPM_VIDEO_VPP_ISLAND);
	psb_irq_postinstall_islands(dev, OSPM_VIDEO_VPP_ISLAND);

	PSB_DEBUG_PM("Power ON VSP!\n");
	return ret;
}

/**
 * vsp_power_down
 *
 * Power down island
 */
static bool vsp_power_down(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	bool ret = true;
	int pm_ret = 0;

	/* whether the VSP is idle */
	if (psb_check_vsp_idle(dev)) {
		PSB_DEBUG_PM("The VSP isn't in idle!\n");
		return false;
	}

	psb_irq_uninstall_islands(dev, OSPM_VIDEO_VPP_ISLAND);

	/* save VSP registers */
	psb_vsp_save_context(dev);

	if (drm_vsp_burst)
		vsp_set_default_frequency(dev);

#ifndef USE_GFX_INTERNAL_PM_FUNC
	pm_ret = pmu_nc_set_power_state(PMU_VPP, OSPM_ISLAND_DOWN, VSP_SS_PM0);
#else
	pm_ret = pmu_set_power_state_tng(VSP_SS_PM0, VSP_SSC, TNG_COMPOSITE_D3);
#endif

	if (pm_ret) {
		PSB_DEBUG_PM("VSP: pmu_nc_set_power_state OFF failed!\n");
		return false;
	}

	PSB_DEBUG_PM("Power OFF VSP!\n");
	return ret;
}

/**
 * ospm_vsp_init
 *
 * initilize
 */
void ospm_vsp_init(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	PSB_DEBUG_ENTRY("\n");

	p_island->p_funcs->power_up = vsp_power_up;
	p_island->p_funcs->power_down = vsp_power_down;
	p_island->p_dependency = get_island_ptr(NC_PM_SSS_GFX_SLC);
}

/***********************************************************
 * slc workaround for ved
 ***********************************************************/
/**
 * apply_ved_slc_workaround
 *
 * bypass SLC for ved if there is ctx that needs the workaround
 */
#define GFX_WRAPPER_GBYPASSENABLE_SW 0x160854
static void apply_ved_slc_workaround(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;

	if (atomic_read(&msvdx_priv->vc1_workaround_ctx)) {
		uint32_t reg, data;

		/* soc.gfx_wrapper.gbypassenable_sw = 1 */
		reg = GFX_WRAPPER_GBYPASSENABLE_SW - GFX_WRAPPER_OFFSET;
		data = WRAPPER_REG_READ(reg);
		data |= 0x1; /* Disable Bypass SLC for VED on MOFD */
		WRAPPER_REG_WRITE(reg, data);
	}
}

/***********************************************************
 * ved islands
 ***********************************************************/
/**
 * ved_power_up
 *
 * Power up island
 */
static bool ved_power_up(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	bool ret = true;
	int pm_ret = 0;
	unsigned int pci_device = dev->pci_device & 0xffff;
	/* struct drm_psb_private *dev_priv = dev->dev_private; */

	PSB_DEBUG_PM("powering up ved\n");
	apply_ved_slc_workaround(dev);

#ifndef USE_GFX_INTERNAL_PM_FUNC
	pm_ret = pmu_nc_set_power_state(PMU_DEC, OSPM_ISLAND_UP, VED_SS_PM0);
#else
	pm_ret = pmu_set_power_state_tng(VED_SS_PM0, VED_SSC, TNG_COMPOSITE_I0);
#endif

	if (pm_ret) {
		PSB_DEBUG_PM("power up ved failed\n");
		return false;
	}

	/* iowrite32(0xffffffff, dev_priv->ved_wrapper_reg + 0); */

	if (need_set_ved_freq && (pci_device != 0x1182)) {
		if (!psb_msvdx_set_ved_freq(IP_FREQ_320_00))
			PSB_DEBUG_PM("MSVDX: Set VED frequency to " \
				"320MHZ after power up\n");
	}

	return ret;
}

/**
 * ved_power_down
 *
 * Power down island
 */
static bool ved_power_down(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	bool ret = true;
	int pm_ret = 0;
	unsigned int pci_device = dev->pci_device & 0xffff;

	/* Need to implement force_off */
	PSB_DEBUG_PM("powering down ved\n");

	/*
	if (psb_check_msvdx_idle(dev))
		return false;
	*/

	psb_msvdx_save_context(dev);


	if (need_set_ved_freq && (pci_device != 0x1182)) {
		if (!psb_msvdx_set_ved_freq(IP_FREQ_200_00))
			PSB_DEBUG_PM("MSVDX: Set VED frequency to " \
				"200MHZ before power down\n");
	}


#ifndef USE_GFX_INTERNAL_PM_FUNC
	pm_ret = pmu_nc_set_power_state(PMU_DEC, OSPM_ISLAND_DOWN, VED_SS_PM0);
#else
	pm_ret = pmu_set_power_state_tng(VED_SS_PM0, VED_SSC, TNG_COMPOSITE_D3);
#endif

	if (pm_ret) {
		PSB_DEBUG_PM("power down ved failed\n");
		return false;
	}

	return ret;
}

/**
 * ospm_ved_init
 *
 * initilize
 */
void ospm_ved_init(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	PSB_DEBUG_ENTRY("\n");

	p_island->p_funcs->power_up = ved_power_up;
	p_island->p_funcs->power_down = ved_power_down;
	p_island->p_dependency = get_island_ptr(NC_PM_SSS_GFX_SLC);
}

/***********************************************************
 * vec islands
 ***********************************************************/
static u32 vec_get_max_freq(struct drm_device *dev)
{
	unsigned int pci_device = dev->pci_device & 0xffff;
	u32 max_freq = IP_FREQ_320_00;

	if ((pci_device == 0x1180) ||
		(pci_device == 0x1181)) {
		max_freq = IP_FREQ_400_00;
		PSB_DEBUG_PM("vec 1180 1181 maximum freq is 400\n");
	} else if (pci_device == 0x1182) {
		max_freq = IP_FREQ_266_67;
		PSB_DEBUG_PM("vec 1182 maximum freq is 400\n");
	} else if (pci_device == 0x1480) {
		max_freq = IP_FREQ_400_00;
		PSB_DEBUG_PM("vec 1480 maximum freq is 400\n");
	} else {
		DRM_ERROR("invalid pci device id %x\n", pci_device);
	}
	return max_freq;
}

/**
 * vec_power_up
 *
 * Power up island
 */
static bool vec_power_up(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	int pm_ret = 0;
	u32 freq_code = 0;
	u32 freq_max = 0;

	PSB_DEBUG_PM("powering up vec\n");
#ifndef USE_GFX_INTERNAL_PM_FUNC
	pm_ret = pmu_nc_set_power_state(PMU_ENC, OSPM_ISLAND_UP, VEC_SS_PM0);
#else
	pm_ret = pmu_set_power_state_tng(VEC_SS_PM0, VEC_SSC, TNG_COMPOSITE_I0);
#endif

	if (pm_ret) {
		PSB_DEBUG_PM("power up vec failed\n");
		return false;
	}

	freq_max = vec_get_max_freq(dev);

	if (drm_vec_force_up_freq < 0) {
		drm_vec_force_up_freq = 0;
		freq_code = freq_max;
	} else {
		if (freq_max < drm_vec_force_up_freq)
			freq_code = drm_vec_force_up_freq;
		else
			freq_code = freq_max;
	}

	if(!tng_topaz_set_vec_freq(freq_code))
		PSB_DEBUG_PM("TOPAZ: Set VEC freq by code %d\n", freq_code);
	else {
		PSB_DEBUG_PM("TOPAZ: Fail to set VEC freq by code %d!\n",
			freq_code);
		/*return false;*/
	}

	if (drm_topaz_cgpolicy != PSB_CGPOLICY_ON)
		tng_topaz_CG_disable(dev);

	PSB_DEBUG_PM("powering up vec done\n");

	return true;
}

/**
 * vec_power_down
 *
 * Power down island
 */
static bool vec_power_down(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	int pm_ret = 0;
	int freq_code = 0;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;

	/* Avoid handle the previous context's power down request */
	topaz_priv->power_down_by_release = 0;

	PSB_DEBUG_PM("TOPAZ: powering down vec\n");

	tng_topaz_save_mtx_state(dev);

	if (drm_vec_force_down_freq < 0)
		drm_vec_force_down_freq = 0;

	if (!drm_vec_force_down_freq)
		freq_code = IP_FREQ_200_00;
	else
		freq_code = drm_vec_force_down_freq;

	if(!tng_topaz_set_vec_freq(freq_code))
		PSB_DEBUG_PM("TOPAZ: Set VEC freq by code %d\n", freq_code);
	else {
		PSB_DEBUG_PM("TOPAZ: Fail to set VEC freq by code %d!\n",
			freq_code);
		/*return false;*/
	}

#ifndef USE_GFX_INTERNAL_PM_FUNC
	pm_ret = pmu_nc_set_power_state(PMU_ENC, \
		OSPM_ISLAND_DOWN, VEC_SS_PM0);
#else
	pm_ret = pmu_set_power_state_tng(VEC_SS_PM0, VEC_SSC, TNG_COMPOSITE_D3);
#endif

	if (pm_ret) {
		DRM_ERROR("Power down ved failed\n");
		return false;
	}

	PSB_DEBUG_PM("TOPAZ: powering down vec done\n");

	return true;
}

/**
 * ospm_vec_init
 *
 * initilize
 */
void ospm_vec_init(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	PSB_DEBUG_ENTRY("\n");

	p_island->p_funcs->power_up = vec_power_up;
	p_island->p_funcs->power_down = vec_power_down;
	p_island->p_dependency = get_island_ptr(NC_PM_SSS_GFX_SLC);
}

static int pm_cmd_freq_wait(u32 reg_freq, u32 *freq_code_rlzd)
{
	int tcount;
	u32 freq_val;

	for (tcount = 0; ; tcount++) {
		freq_val = intel_mid_msgbus_read32(PUNIT_PORT, reg_freq);
		if ((freq_val & IP_FREQ_VALID) == 0)
			break;
		if (tcount > 1500) {
			DRM_ERROR("P-Unit freq request wait timeout %x",
				freq_val);
			return -EBUSY;
		}
		udelay(1);
	}

	if (freq_code_rlzd) {
		*freq_code_rlzd = ((freq_val >> IP_FREQ_STAT_POS) &
			IP_FREQ_MASK);
	}

	return 0;
}

static int pm_cmd_freq_get(u32 reg_freq)
{
	u32 freq_val;
	int freq_code=0;

	pm_cmd_freq_wait(reg_freq, NULL);

	freq_val = intel_mid_msgbus_read32(PUNIT_PORT, reg_freq);
	freq_code =(int)((freq_val>>IP_FREQ_STAT_POS) & ~IP_FREQ_VALID);
	return freq_code;
}

static int pm_cmd_freq_set(u32 reg_freq, u32 freq_code, u32 *p_freq_code_rlzd)
{
	u32 freq_val;
	u32 freq_code_realized;
	int rva;

	rva = pm_cmd_freq_wait(reg_freq, NULL);
	if (rva < 0) {
		printk(KERN_ALERT "%s: pm_cmd_freq_wait 1 failed\n", __func__);
		return rva;
	}

	freq_val = IP_FREQ_VALID | freq_code;
	intel_mid_msgbus_write32(PUNIT_PORT, reg_freq, freq_val);

	rva = pm_cmd_freq_wait(reg_freq, &freq_code_realized);
	if (rva < 0) {
		printk(KERN_ALERT "%s: pm_cmd_freq_wait 2 failed\n", __func__);
		return rva;
	}

	if (p_freq_code_rlzd)
		*p_freq_code_rlzd = freq_code_realized;

	return rva;
}

static void vsp_set_max_frequency(struct drm_device *dev)
{
	unsigned int pci_device = dev->pci_device & 0xffff;
	u32 freq_code_rlzd;
	u32 freq_code, max_freq_code;
	u32 freq, max_freq;
	int ret;

	freq_code = 0;
	max_freq_code = 0;
	if (pci_device == 0x1180) {
		max_freq_code = IP_FREQ_457_14;
		PSB_DEBUG_PM("vsp maximum freq is 457\n");
	} else if (pci_device == 0x1181) {
		max_freq_code = IP_FREQ_400_00;
		PSB_DEBUG_PM("vsp maximum freq is 400\n");
	} else if (pci_device == 0x1480) {
		max_freq_code = IP_FREQ_400_00;
		PSB_DEBUG_PM("vsp maximum freq for ANN A0 is 400\n");
	} else if (pci_device == 0x1182) {
		PSB_DEBUG_PM("Max freq is the default freq 200MHZ for SKU3 \n");
		max_freq_code = IP_FREQ_200_00;
	} else {
		DRM_ERROR("invalid pci device id %x\n", pci_device);
		return;
	}

	// according to the latest scheme, set VSP max frequency to 400MHZ
	freq_code = IP_FREQ_400_00;

	if (drm_vsp_force_up_freq)
		freq_code = drm_vsp_force_up_freq;

	freq = 1600 * 2 / (freq_code + 1);
	max_freq = 1600 * 2 / (max_freq_code + 1);
	VSP_DEBUG("try to set %dMHZ, max freq is %dMHZ\n", freq, max_freq);
	if (freq > max_freq)
		freq_code = max_freq_code;

	ret = pm_cmd_freq_set(VSP_SS_PM1, freq_code, &freq_code_rlzd);
	if (ret < 0) {
		DRM_ERROR("failed to set freqency, current is %x\n",
			  freq_code_rlzd);
	}

	PSB_DEBUG_PM("set maximum frequency\n");
	return;
}

static void vsp_set_default_frequency(struct drm_device *dev)
{
	u32 freq_code_rlzd;
	int ret;
	u32 freq_code;

	freq_code = IP_FREQ_200_00;

	if (drm_vsp_force_down_freq)
		freq_code = drm_vsp_force_down_freq;

	ret = pm_cmd_freq_set(VSP_SS_PM1, freq_code, &freq_code_rlzd);
	if (ret < 0) {
		DRM_ERROR("failed to set freqency, current is %x\n",
			  freq_code_rlzd);
	}

	PSB_DEBUG_PM("set default frequency\n");
	return;
}

int psb_msvdx_set_ved_freq(u32 freq_code)
{
       u32 freq_code_rlzd;
       int ret;

       ret = pm_cmd_freq_set(VED_SS_PM1, freq_code, &freq_code_rlzd);
       if (ret < 0) {
               DRM_ERROR("failed to set freqency, current is %x\n",
                               freq_code_rlzd);
       }

       return ret;
}

int psb_msvdx_get_ved_freq(u32 reg_freq)
{
	return  pm_cmd_freq_get(reg_freq);
}

void psb_set_freq_control_switch(bool config_value)
{
	need_set_ved_freq = config_value;
}

static void pm_cmd_power_set(int pm_reg, int pm_mask)
{
	intel_mid_msgbus_write32(0x04, pm_reg, pm_mask);
	udelay(500);

	if (pm_reg == VEC_SS_PM0 && !(pm_mask & 0x3)) {
		PSB_DEBUG_PM("Power up VEC, delay another 1500 us\n");
		udelay(1500);
	}

	pm_mask = intel_mid_msgbus_read32(0x04, pm_reg);
	PSB_DEBUG_PM("pwr_mask read: reg=0x%x pwr_mask=0x%x \n", pm_reg, pm_mask);
}
