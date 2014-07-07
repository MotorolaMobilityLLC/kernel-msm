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

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/printk.h>
#include <linux/delay.h>
#include <asm/intel-mid.h>

#include "psb_drv.h"
#include "gfx_ospm_ann.h"
#include "gfx_freq.h"
#include "pmu_tng.h"
#include "tng_wa.h"




#define	USE_GFX_PM_FUNC			0

/* WRAPPER Offset 0x160024 */
#define GFX_STATUS_OFFSET		0x24

#define GFX_POWER_UP(x) \
	pmu_nc_set_power_state(x, OSPM_ISLAND_UP, GFX_SS_PM0)

#define GFX_POWER_DOWN(x) \
	pmu_nc_set_power_state(x, OSPM_ISLAND_DOWN, GFX_SS_PM0)

extern IMG_BOOL gbSystemActivePMEnabled;
extern IMG_BOOL gbSystemActivePMInit;

enum GFX_ISLAND_STATUS {
	POWER_ON = 0,		/* No gating (clk or power) */
	CLOCK_GATED,		/* Clock Gating */
	SOFT_RESET,		/* Soft Reset */
	POWER_OFF,		/* Powered off or Power gated.*/
};

static int (*pSuspend_func)(void) = NULL;
static int (*pResume_func)(void) = NULL;

int is_tng_a0 = 0;
EXPORT_SYMBOL(is_tng_a0);

/**
  * gpu_freq_code_to_mhz() - Given frequency as a code (as defined for *_PM1
  * register), return frequency in mhz.
  * @freq_code_in - Input: A frequency code as specified for *_PM1 registers.
  * Function return value: corresponding frequency in MHz or < 0 if error.
  */
static int gpu_freq_code_to_mhz(int freq_code_in)
{
	int freq_mhz_out;

	switch (freq_code_in) {
	case IP_FREQ_100_00:
		freq_mhz_out = 100;
		break;
	case IP_FREQ_106_67:
		freq_mhz_out = 106;
		break;
	case IP_FREQ_133_30:
		freq_mhz_out = 133;
		break;
	case IP_FREQ_160_00:
		freq_mhz_out = 160;
		break;
	case IP_FREQ_177_78:
		freq_mhz_out = 177;
		break;
	case IP_FREQ_200_00:
		freq_mhz_out = 200;
		break;
	case IP_FREQ_213_33:
		freq_mhz_out = 213;
		break;
	case IP_FREQ_266_67:
		freq_mhz_out = 266;
		break;
	case IP_FREQ_320_00:
		freq_mhz_out = 320;
		break;
	case IP_FREQ_355_56:
		freq_mhz_out = 355;
		break;
	case IP_FREQ_400_00:
		freq_mhz_out = 400;
		break;
	case IP_FREQ_457_14:
		freq_mhz_out = 457;
		break;
	case IP_FREQ_533_33:
		freq_mhz_out = 533;
		break;
	case IP_FREQ_640_00:
		freq_mhz_out = 640;
		break;
	case IP_FREQ_800_00:
		freq_mhz_out = 800;
		break;
	default:
		printk(KERN_ALERT "%s: Invalid freq code: %#x\n", __func__,
			freq_code_in);
		return -EINVAL;
	}

	return freq_mhz_out;
}

/**
 * mrfl_pwr_cmd_gfx - Change graphics power state.
 * Change island power state in the require sequence.
 *
 * @gfx_mask: Mask of islands to be changed.
 * @new_state: 0 for power-off, 1 for power-on.
 */
#ifdef USE_GFX_INTERNAL_PM_FUNC
static int mrfl_pwr_cmd_gfx(u32 gfx_mask, int new_state)
{
	/*
	 * pwrtab - gfx pwr sub-islands in required power-up order and
	 * in reverse of required power-down order.
	 */
	static const u32 pwrtab[] = {
		GFX_SLC_LDO_SHIFT,
		GFX_SLC_SHIFT,
		GFX_SDKCK_SHIFT,
		GFX_RSCD_SHIFT,
	};
	const int pwrtablen = ARRAY_SIZE(pwrtab);
	int i;
	int j;
	int ret;
	u32 ns_mask;
	u32 done_mask;
	u32 this_mask;
	u32 pwr_state_prev;

	pwr_state_prev = intel_mid_msgbus_read32(PUNIT_PORT, GFX_SS_PM0);

	if (new_state == OSPM_ISLAND_UP)
		ns_mask = TNG_COMPOSITE_I0;
	else
		ns_mask = TNG_COMPOSITE_D3;

	/*  Call underlying function separately for each step in the
	    power sequence. */
	done_mask = 0;
	for (i = 0; i < pwrtablen ; i++) {
		if (new_state == OSPM_ISLAND_UP)
			j = i;
		else
			j = pwrtablen - i - 1;

		done_mask |= TNG_SSC_MASK << pwrtab[j];
		this_mask = gfx_mask & done_mask;
		if (this_mask) {
		/*  FIXME - if (new_state == 0), check for required
			    conditions per the SAS. */
			ret = pmu_set_power_state_tng(GFX_SS_PM0,
					this_mask, ns_mask);
			if (ret)
			return ret;
		}

		/**
		 * If turning some power on, and the power to be on includes SLC,
		 * and SLC was not previously on, then setup some registers.
		 */
		if ((new_state == OSPM_ISLAND_UP)
			&& (pwrtab[j] == GFX_SLC_SHIFT)
			&& ((pwr_state_prev >> GFX_SLC_SHIFT) != TNG_SSC_I0))
		{
			/* TNG A0 workarounds */
			if (IS_TNG_A0(dev))
				apply_TNG_A0_workarounds(OSPM_GRAPHICS_ISLAND, 1);

			/* ANN A0 workarounds */
			if (IS_ANN(dev))
				apply_ANN_A0_workarounds(OSPM_GRAPHICS_ISLAND, 1);
		}

		if ((gfx_mask & ~done_mask) == 0)
			break;
	}

	return 0;
}
#endif

/**
 * pm_cmd_freq_wait() - Wait for frequency valid via specified register.
 * Optionally, return realized frequency to caller.
 * @reg_freq: The frequency control register.  One of *_PM1.
 * @freq_code_rlzd - If non-NULL, pointer to receive the realized Tangier
 * frequency code.
 */
static int pm_cmd_freq_wait(u32 reg_freq, u32 *freq_code_rlzd)
{
	int tcount;
	u32 freq_val;

	for (tcount = 0; ; tcount++) {
		freq_val = intel_mid_msgbus_read32(PUNIT_PORT, reg_freq);
		if ((freq_val & IP_FREQ_VALID) == 0)
			break;
		if (tcount > 1500) {
			WARN(1, "%s: P-Unit freq request wait timeout",
				__func__);
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


/**
 * pm_cmd_freq_set() - Set operating frequency via specified register.
 * Optionally, return realized frequency to caller.
 * @reg_freq: The frequency control register.  One of *_PM1.
 * @freq_code: Tangier frequency code.
 * @p_freq_code_rlzd - If non-NULL, pointer to receive the realized Tangier
 * frequency code.
 */
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


/**
 * pm_cmd_freq_from_code() - Set operating frequency via specified register.
 * Optionally, return realized frequency to caller.
 * @reg_freq: The frequency control register.  One of *_PM1.
 * @freq_code: Tangier frequency code.
 * @function return value: - <0 if error, or frequency in MHz.
 */
int gpu_freq_set_from_code(int freq_code)
{
	u32 freq_realized_code;
	int rva;

	rva = pm_cmd_freq_set(GFX_SS_PM1, freq_code, &freq_realized_code);
	if (rva < 0)
		return rva;

	return gpu_freq_code_to_mhz(freq_realized_code);
}
EXPORT_SYMBOL(gpu_freq_set_from_code);


/**
  * gpu_freq_mhz_to_code() - Given frequency in MHz, return frequency code
  * used for frequency control.
  * Always pick the code less than equal to the integer MHz value.
  * @freq_mhz_in - Input: A MHz frequency specification.
  * @*p_freq_out - Out: The quantized MHz frequency specification.
  * Function return value: frequency code as in register definition.
  */
int gpu_freq_mhz_to_code(int freq_mhz_in, int *p_freq_out)
{
	int freq_code;
	int freq_out;

	if (freq_mhz_in >= 800) {
		freq_code = IP_FREQ_800_00;	/* 800.00 */
		freq_out = 800;
	} else if (freq_mhz_in >= 640) {
		freq_code = IP_FREQ_640_00;	/* 640.00 */
		freq_out = 640;
	} else if (freq_mhz_in >= 533) {
		freq_code = IP_FREQ_533_33;	/* 533.33 */
		freq_out = 533;
	} else if (freq_mhz_in >= 457) {
		freq_code = IP_FREQ_457_14;	/* 457.14 */
		freq_out = 457;
	} else if (freq_mhz_in >= 400) {
		freq_code = IP_FREQ_400_00;	/* 400.00 */
		freq_out = 400;
	} else if (freq_mhz_in >= 355) {
		freq_code = IP_FREQ_355_56;	/* 355.56 */
		freq_out = 355;
	} else if (freq_mhz_in >= 320) {
		freq_code = IP_FREQ_320_00;	/* 320.00 */
		freq_out = 320;
	} else if (freq_mhz_in >= 266) {
		freq_code = IP_FREQ_266_67;	/* 266.67 */
		freq_out = 266;
	} else if (freq_mhz_in >= 213) {
		freq_code = IP_FREQ_213_33;	/* 213.33 */
		freq_out = 213;
	} else if (freq_mhz_in >= 200) {
		freq_code = IP_FREQ_200_00;	/* 200.00 */
		freq_out = 200;
	} else if (freq_mhz_in >= 177) {
		freq_code = IP_FREQ_177_78;	/* 177.78 */
		freq_out = 177;
	} else if (freq_mhz_in >= 160) {
		freq_code = IP_FREQ_160_00;	/* 160.00 */
		freq_out = 160;
	} else if (freq_mhz_in >= 133) {
		freq_code = IP_FREQ_133_30;	/* 133.30 */
		freq_out = 133;
	} else if (freq_mhz_in >= 106) {
		freq_code = IP_FREQ_106_67;	/* 106.67 */
		freq_out = 106;
	} else {
		freq_code = IP_FREQ_100_00;	/* 100.00 */
		freq_out = 100;
	}

	*p_freq_out = freq_out;

	return freq_code;
}
EXPORT_SYMBOL(gpu_freq_mhz_to_code);

void gpu_freq_set_suspend_func(int (*suspend_func)(void))
{
	pSuspend_func = suspend_func;
	PSB_DEBUG_PM("OSPM: suspend \n");
}
EXPORT_SYMBOL(gpu_freq_set_suspend_func);

void gpu_freq_set_resume_func(int (*resume_func)(void))
{
	pResume_func = resume_func;
	PSB_DEBUG_PM("OSPM: Resume \n");
}
EXPORT_SYMBOL(gpu_freq_set_resume_func);

/***********************************************************
 * All Graphics Island
 ***********************************************************/

/**
 * ospm_rscd_power_up
 *
 * Power up Rascal/Dust islands
 * Sequence & flow from SAS
 */
static bool ospm_rscd_power_up(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	bool ret;
	int error = 0;

	if (pResume_func) {
		error = (*pResume_func)();
		if (error) {
			PSB_DEBUG_PM("OSPM: Could not resume DFRGX");
			return false;
		}
	}

	PSB_DEBUG_PM("Pre-power-up status = 0x%08x\n",
		intel_mid_msgbus_read32(PUNIT_PORT, NC_PM_SSS));

	ret = GFX_POWER_UP(PMU_RSCD);

	PSB_DEBUG_PM("Post-power-up status = 0x%08x\n",
		intel_mid_msgbus_read32(PUNIT_PORT, NC_PM_SSS));

	/* If APM is enabled, then we need to make sure that the IRQs
	 * are installed. It is possible the the GUnit has been turned
	 * off and the IER and IMR registers have lost their state.
	 * So we need to enable interrupts after powering on.
	 * If the IRQs are not turned on, the interrupt sent from RGX
	 * to indicate that it is done with processing is lost. RGX
	 * island would then remain ON.
	 */
	psb_irq_preinstall_islands(dev, OSPM_GRAPHICS_ISLAND);
	psb_irq_postinstall_islands(dev, OSPM_GRAPHICS_ISLAND);

	return !ret;
}

/**
 * ospm_rscd_power_down
 *
 * Power down Rascal/Dust islands
 * Sequence & flow from SAS
 */
static bool ospm_rscd_power_down(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	bool ret;
	int error = 0;

	PSB_DEBUG_PM("OSPM: ospm_gfx_power_down \n");

	if (pSuspend_func) {
	error = (*pSuspend_func)();
		if (error) {
			PSB_DEBUG_PM("OSPM :Could not suspend DFRGX");
			return false;
		}
	}

	PSB_DEBUG_PM("Pre-power-off Status = 0x%08x\n",
		intel_mid_msgbus_read32(PUNIT_PORT, NC_PM_SSS));

	/* If APM is enabled, we can turn off the RGX interrupts. This is
	 * kind of a no-op but still better coding to turn of IRQs for
	 * devices/ components that are turned off
	 */
	psb_irq_uninstall_islands(dev, OSPM_GRAPHICS_ISLAND);
	synchronize_irq(dev->pdev->irq);

	/* power down every thing */
	ret = GFX_POWER_DOWN(PMU_RSCD);

	PSB_DEBUG_PM("Post-power-off Status = 0x%08x\n",
		intel_mid_msgbus_read32(PUNIT_PORT, NC_PM_SSS));

	return !ret;
}

/**
 * ospm_rscd_init
 *
 * Rascal/Dut power island init
 */
void ospm_rscd_init(struct drm_device *dev,
		struct ospm_power_island *p_island)
{
	if (IS_TNG_A0(dev))
		is_tng_a0 = 1;

	PSB_DEBUG_PM("%s\n", __func__);
	p_island->p_funcs->power_up = ospm_rscd_power_up;
	p_island->p_funcs->power_down = ospm_rscd_power_down;
	p_island->p_dependency = get_island_ptr(NC_PM_SSS_GFX_SDKCK);
}

/**
 * ospm_sidekick_power_up
 *
 * Power up Sidekick island
 * Sequence & flow from SAS
 */
static bool ospm_sidekick_power_up(struct drm_device *dev,
		struct ospm_power_island *p_island)
{
	bool ret;

	PSB_DEBUG_PM("Pre-power-up status = 0x%08x\n",
			intel_mid_msgbus_read32(PUNIT_PORT, NC_PM_SSS));

	ret = GFX_POWER_UP(PMU_SDKCK);

	PSB_DEBUG_PM("Post-power-up status = 0x%08x\n",
			intel_mid_msgbus_read32(PUNIT_PORT, NC_PM_SSS));

#if 0
	/* SLC flush and invalidate */
	if (!ret)
	{
		uint32_t reg, data, count;

		reg = 0x100100 - RGX_OFFSET;
		data = RGX_REG_READ(reg);
		RGX_REG_WRITE(reg, data | (1 << 27));
		RGX_REG_WRITE(reg, data);

		/* write 1 to RGX_CR_SLC_CTRL_FLUSH_INVAL */
		reg = 0x103818 - RGX_OFFSET;
		data = 1;
		RGX_REG_WRITE(reg, data);

		count = 0;
		/* Poll RGX_CR_SLC_STATUS0 */
		reg = 0x103820 - RGX_OFFSET;
		do {
			udelay(500);
			data = RGX_REG_READ(reg);
		} while ((data & 0x2) && (count++ < 10000));

		if (unlikely(count >= 10000))
			PSB_DEBUG_PM("SLC: flush and invalide timeout\n" );
	}
#endif

	if (!ret && IS_TNG_B0(dev)) {
		uint32_t reg, data;
		/* soc.gfx_wrapper.gclip_control.aes_bypass_disable = 1*/
		reg = 0x160020 - GFX_WRAPPER_OFFSET;
		data = WRAPPER_REG_READ(reg);

		data |= 0x80;
		WRAPPER_REG_WRITE(reg, data);
	}

#if 0
	/* SLC hash set */
	if (!ret)
	{
		uint32_t reg, data;
		reg = 0x103800 - RGX_OFFSET;
		data = 0x200001;
		RGX_REG_WRITE(reg, data);
	}
#endif

	return !ret;
}

/**
 * ospm_sidekick_power_down
 *
 * Power down Sidekick island
 * Sequence & flow from SAS
 */
static bool ospm_sidekick_power_down(struct drm_device *dev,
		struct ospm_power_island *p_island)
{
	bool ret;

	PSB_DEBUG_PM("Pre-power-off Status = 0x%08x\n",
			intel_mid_msgbus_read32(PUNIT_PORT, NC_PM_SSS));

	/* power down every thing */
	ret = GFX_POWER_DOWN(PMU_SDKCK);

	PSB_DEBUG_PM("Post-power-off Status = 0x%08x\n",
			intel_mid_msgbus_read32(PUNIT_PORT, NC_PM_SSS));

	return !ret;
}

/**
 * ospm_sidekick_init
 *
 * Sidekick power island init
 */
void ospm_sidekick_init(struct drm_device *dev,
		struct ospm_power_island *p_island)
{
	PSB_DEBUG_PM("%s\n", __func__);
	p_island->p_funcs->power_up = ospm_sidekick_power_up;
	p_island->p_funcs->power_down = ospm_sidekick_power_down;
	p_island->p_dependency = get_island_ptr(NC_PM_SSS_GFX_SLC);
}

static void ospm_pnp_settings(struct drm_device *dev)
{
	uint32_t reg, data;

	reg = 0x160008 - GFX_WRAPPER_OFFSET;
	data = 0x0;
	WRAPPER_REG_WRITE(reg, data);

	reg = 0x160028 - GFX_WRAPPER_OFFSET;
	data = WRAPPER_REG_READ(reg);
	/*
	GCILP_ARB_CONTROL[3:0] = SLCRD_WEIGHT = 3
	GCILP_ARB_CONTROL[7:4] = SLCWR_WEIGHT = 3
	GCILP_ARB_CONTROL[11:8] = VED_WEIGHT = 3
	GCILP_ARB_CONTROL[15:12] = VEC_WEIGHT = 3
	GCILP_ARB_CONTROL[19:16] = VSP_WEIGHT = 3
	GCILP_ARB_CONTROL[23:20] = FIRST_ARB_WEIGHT = 3
	GCILP_ARB_CONTROL[31] = ARB_MODE = 0
	*/
	data |= 0x333333;
	WRAPPER_REG_WRITE(reg, data);

	return ;
}

/**
 * ospm_slc_power_up
 *
 * Power up slc islands
 * Sequence & flow from SAS
 */
static bool ospm_slc_power_up(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	bool ret;

	PSB_DEBUG_PM("%s: Pre-power-off Status = 0x%08x\n",
		__func__,
		intel_mid_msgbus_read32(PUNIT_PORT, NC_PM_SSS));

	ret = GFX_POWER_UP(PMU_SLC);

	if (!ret && IS_ANN(dev))
		apply_ANN_A0_workarounds(OSPM_GRAPHICS_ISLAND, 1);

	ospm_pnp_settings(dev);

	PSB_DEBUG_PM("Post-power-up status = 0x%08x\n",
		intel_mid_msgbus_read32(PUNIT_PORT, NC_PM_SSS));

	return !ret;

	if (!ret) {
		uint32_t reg, data;

		/* soc.gfx_wrapper.gbypassenable_sw = 1 */
		reg = 0x160854 - GFX_WRAPPER_OFFSET;
		data = WRAPPER_REG_READ(reg);
		data |= 0x100; /*Bypass SLC for VEC*/
		WRAPPER_REG_WRITE(reg, data);
	}

	return !ret;
}

/**
 * ospm_slc_power_down
 *
 * Power down SLC islands
 * Sequence & flow from SAS
 */
static bool ospm_slc_power_down(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	bool ret;

	PSB_DEBUG_PM("%s: Pre-power-off Status = 0x%08x\n",
		__func__,
		intel_mid_msgbus_read32(PUNIT_PORT, NC_PM_SSS));

	ret = GFX_POWER_DOWN(PMU_SLC);

	PSB_DEBUG_PM("Post-power-off Status = 0x%08x\n",
		intel_mid_msgbus_read32(PUNIT_PORT, NC_PM_SSS));

	return !ret;
}

/**
 * ospm_slc_init
 *
 * SLC power island init
 */
void ospm_slc_init(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	if (IS_TNG_A0(dev))
		is_tng_a0 = 1;

	PSB_DEBUG_PM("%s\n", __func__);
	p_island->p_funcs->power_up = ospm_slc_power_up;
	p_island->p_funcs->power_down = ospm_slc_power_down;
	p_island->p_dependency = get_island_ptr(NC_PM_SSS_GFX_SLC_LDO);
}

/**
 * ospm_slc_ldo_power_up
 *
 * Power up SLC LDO island
 * Sequence & flow from SAS
 */
static bool ospm_slc_ldo_power_up(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	bool ret;

	PSB_DEBUG_PM("%s: Pre-power-off Status = 0x%08x\n",
		__func__,
		intel_mid_msgbus_read32(PUNIT_PORT, NC_PM_SSS));

	ret = GFX_POWER_UP(PMU_LDO);

	PSB_DEBUG_PM("Post-power-up status = 0x%08x\n",
		intel_mid_msgbus_read32(PUNIT_PORT, NC_PM_SSS));

	return !ret;
}

/**
 * ospm_slc_ldo_power_down
 *
 * Power down SLC LDO island
 * Sequence & flow from SAS
 */
static bool ospm_slc_ldo_power_down(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	bool ret;

	PSB_DEBUG_PM("%s: Pre-power-off Status = 0x%08x\n",
		__func__,
		intel_mid_msgbus_read32(PUNIT_PORT, NC_PM_SSS));

	ret = GFX_POWER_DOWN(PMU_LDO);

	PSB_DEBUG_PM("Post-power-off Status = 0x%08x\n",
		intel_mid_msgbus_read32(PUNIT_PORT, NC_PM_SSS));

	return !ret;
}

/**
 * ospm_slc_ldo_init
 *
 * SLC LDO power island init
 */
void ospm_slc_ldo_init(struct drm_device *dev,
			struct ospm_power_island *p_island)
{
	if (IS_TNG_A0(dev))
		is_tng_a0 = 1;

	PSB_DEBUG_PM("%s\n", __func__);
	p_island->p_funcs->power_up = ospm_slc_ldo_power_up;
	p_island->p_funcs->power_down = ospm_slc_ldo_power_down;
	p_island->p_dependency = NULL;
}
