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


#include <linux/spinlock.h>
#include <asm/intel-mid.h>
#include <asm/intel_scu_ipc.h>

#include <linux/pm_runtime.h>

#include "psb_drv.h"
#include "pmu_tng.h"
#include "tng_wa.h"
#include "pwr_mgmt.h"
#include "gfx_rtpm.h"
#ifdef CONFIG_MOOREFIELD
#include "gfx_ospm_ann.h"
#else
#include "gfx_ospm.h"
#endif
#include "dc_ospm.h"
#include "dc_maxfifo.h"
#include "video_ospm.h"
#include "early_suspend.h"
#include "early_suspend_sysfs.h"


struct _ospm_data_ *g_ospm_data;
struct drm_device *gpDrmDevice;

/* island, state, ref_count, init_func, power_func */
#ifdef CONFIG_MOOREFIELD
struct ospm_power_island island_list[] = {
	{OSPM_DISPLAY_A, OSPM_POWER_OFF, {0}, ospm_disp_a_init, NULL},
	{OSPM_DISPLAY_B, OSPM_POWER_OFF, {0}, ospm_disp_b_init, NULL},
	{OSPM_DISPLAY_C, OSPM_POWER_OFF, {0}, ospm_disp_c_init, NULL},
	{OSPM_DISPLAY_MIO, OSPM_POWER_OFF, {0}, ospm_mio_init, NULL},
	{OSPM_DISPLAY_HDMI, OSPM_POWER_OFF, {0}, ospm_hdmi_init, NULL},
	{OSPM_GRAPHICS_ISLAND, OSPM_POWER_OFF, {0}, ospm_rscd_init, NULL},
	{OSPM_SIDEKICK_ISLAND, OSPM_POWER_OFF, {0}, ospm_sidekick_init, NULL},
	{OSPM_SLC_ISLAND, OSPM_POWER_OFF, {0}, ospm_slc_init, NULL},
	{OSPM_SLC_LDO_ISLAND, OSPM_POWER_OFF, {0}, ospm_slc_ldo_init, NULL},
	{OSPM_VIDEO_VPP_ISLAND, OSPM_POWER_OFF, {0}, ospm_vsp_init, NULL},
	{OSPM_VIDEO_DEC_ISLAND, OSPM_POWER_OFF, {0}, ospm_ved_init, NULL},
	{OSPM_VIDEO_ENC_ISLAND, OSPM_POWER_OFF, {0}, ospm_vec_init, NULL},
};
#else
struct ospm_power_island island_list[] = {
	{OSPM_DISPLAY_A, OSPM_POWER_OFF, {0}, ospm_disp_a_init, NULL},
	{OSPM_DISPLAY_B, OSPM_POWER_OFF, {0}, ospm_disp_b_init, NULL},
	{OSPM_DISPLAY_C, OSPM_POWER_OFF, {0}, ospm_disp_c_init, NULL},
	{OSPM_DISPLAY_MIO, OSPM_POWER_OFF, {0}, ospm_mio_init, NULL},
	{OSPM_DISPLAY_HDMI, OSPM_POWER_OFF, {0}, ospm_hdmi_init, NULL},
	{OSPM_GRAPHICS_ISLAND, OSPM_POWER_OFF, {0}, ospm_gfx_init, NULL},
	{OSPM_SLC_ISLAND, OSPM_POWER_OFF, {0}, ospm_slc_init, NULL},
	{OSPM_VIDEO_VPP_ISLAND, OSPM_POWER_OFF, {0}, ospm_vsp_init, NULL},
	{OSPM_VIDEO_DEC_ISLAND, OSPM_POWER_OFF, {0}, ospm_ved_init, NULL},
	{OSPM_VIDEO_ENC_ISLAND, OSPM_POWER_OFF, {0}, ospm_vec_init, NULL},
};
#endif

/**
 * in_atomic_or_interrupt() - Return non-zero if in atomic context.
 * Problems with this code:
 * - Function in_atomic is not guaranteed to detect the atomic state entered
 *   by acquisition of a spinlock (and indeed does so only if CONFIG_PREEMPT).
 *   For a discussion on the use of in_atomic and why is it considered (in
 *   general) problematic, see: http://lwn.net/Articles/274695/
 * - Therefore, scripts/checkpatch.pl will complain about use of function
 *   in_atomic in non-core kernel.  For this reason, the several uses of
 *   in_atomic in this file were centralized here (so only one warning).
 *
 * Note: The test herein was originally:
 *   in_atomic() || in_interrupt()
 * but the test for in_interrupt() is redundant with the in_atomic test.
 */
#if !defined CONFIG_PREEMPT
#error Function in_atomic (in general) requires CONFIG_PREEMPT
#endif

#undef OSPM_DEBUG_INFO
#ifdef OSPM_DEBUG_INFO
const char *get_island_name(u32 hw_island)
{
	const char *pstr;

	switch (hw_island) {
	case OSPM_DISPLAY_A:
		pstr = "DISP A ";
		break;
	case OSPM_DISPLAY_B:
		pstr = "DISP B ";
		break;
	case OSPM_DISPLAY_C:
		pstr = "DISP C ";
		break;
	case OSPM_DISPLAY_MIO:
		pstr = "MIO    ";
		break;
	case OSPM_DISPLAY_HDMI:
		pstr = "HDMI   ";
		break;
	case OSPM_VIDEO_VPP_ISLAND:
		pstr = "VSP    ";
		break;
	case OSPM_VIDEO_DEC_ISLAND:
		pstr = "VED    ";
		break;
	case OSPM_VIDEO_ENC_ISLAND:
		pstr = "VEC    ";
		break;
	case OSPM_GRAPHICS_ISLAND:
		pstr = "GFX    ";
		break;
	default:
		pstr = "(unknown hw_island)";
		break;
	}

	return pstr;
}

static void dump_ref_count(u32 hw_island)
{
	int i = 0;
	int ref_value = 0;
	struct ospm_power_island *p_island = NULL;

	PSB_DEBUG_PM("*** power island refrence count. ***\n");

	for (i = 0; i < ARRAY_SIZE(island_list); i++) {
		if (hw_island & island_list[i].island) {
			p_island = &island_list[i];
			ref_value = atomic_read(&p_island->ref_count);
			printk(KERN_ALERT
				"*** %s: %d\n",
				get_island_name(island_list[i].island),
				ref_value);
		}
	}

	OSPM_DPF("%s: ************************************\n");
}
#endif	/* OSPM_DEBUG_INFO */

/**
 * ospm_suspend_pci
 *
 * Description: Suspend the pci device saving state and disabling
 * as necessary.
 */
static void ospm_suspend_pci(struct drm_device *dev)
{
	struct pci_dev *pdev = dev->pdev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	int bsm, vbt, bgsm;

	PSB_DEBUG_PM("%s\n", __func__);

	pci_read_config_dword(pdev, 0x5C, &bsm);
	dev_priv->saveBSM = bsm;
	pci_read_config_dword(pdev, 0xFC, &vbt);
	dev_priv->saveVBT = vbt;
	pci_read_config_dword(pdev, 0x70, &bgsm);
	dev_priv->saveBGSM = bgsm;
	pci_read_config_dword(pdev, PSB_PCIx_MSI_ADDR_LOC, &dev_priv->msi_addr);
	pci_read_config_dword(pdev, PSB_PCIx_MSI_DATA_LOC, &dev_priv->msi_data);
	pci_save_state(pdev);
	pci_set_power_state(pdev, PCI_D3hot);
}

/**
 * ospm_resume_pci
 *
 * Description: Resume the pci device restoring state and enabling
 * as necessary.
 */
static void ospm_resume_pci(struct drm_device *dev)
{
	struct pci_dev *pdev = dev->pdev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	int mmadr, ret = 0;

	PSB_DEBUG_PM("%s\n", __func__);

	if (dev_priv->saveBGSM != 0)
		pci_write_config_dword(pdev, 0x70, dev_priv->saveBGSM);

	if (dev_priv->saveBSM != 0)
		pci_write_config_dword(pdev, 0x5c, dev_priv->saveBSM);

	if (dev_priv->saveVBT != 0)
		pci_write_config_dword(pdev, 0xFC, dev_priv->saveVBT);

	/* retoring MSI address and data in PCIx space */
	if (dev_priv->msi_addr != 0)
		pci_write_config_dword(pdev, PSB_PCIx_MSI_ADDR_LOC,
				dev_priv->msi_addr);

	if (dev_priv->msi_data != 0)
		pci_write_config_dword(pdev, PSB_PCIx_MSI_DATA_LOC,
				dev_priv->msi_data);

	ret = pci_enable_device(pdev);

	/* FixMe, Change should be removed Once bz 115181 is fixed */
	pci_read_config_dword(pdev, 0x10, &mmadr);
	if (mmadr == 0) {
		pr_err("GFX OSPM : Bad PCI config\n");
		BUG();
	}

	if (ret != 0)
		PSB_DEBUG_PM("pci_enable_device failed: %d\n", ret);
}

void rtpm_suspend_pci(void)
{
	ospm_suspend_pci(g_ospm_data->dev);
}

void rtpm_resume_pci(void)
{
	ospm_resume_pci(g_ospm_data->dev);
}


/**
 * get_island_ptr
 *
 * get pointer to the island
 * use it to get array item for setting dependency
 *
 * Although island values are defined as bit mask values,
 * this function only supports having a single bit set
 * in this parameter.
 */
struct ospm_power_island *get_island_ptr(u32 hw_island)
{
	struct ospm_power_island *p_island = NULL;
	int i = 0;

	/* got through islands array to find the island */
	while ((i < ARRAY_SIZE(island_list) && (!p_island))) {
		/* do we have the island? */
		if (hw_island & island_list[i].island) {
			/* Found it */
			p_island = &island_list[i];
			break;
		}

		i++;
	}

	if (i == ARRAY_SIZE(island_list))
		PSB_DEBUG_PM("island %x not found\n", hw_island);

	return p_island;
}

static bool power_down_island(struct ospm_power_island *p_island);

/**
 * power_up_island
 *
 * Description: Power up the island and all of it's dependent islands
 */
static bool power_up_island(struct ospm_power_island *p_island)
{
	bool ret = true;

	/* handle the dependency first */
	if (p_island->p_dependency) {
		/* Power up dependent island */
		ret = power_up_island(p_island->p_dependency);
		if (!ret)
			return ret;
	}

	/* if successfully handled dependency */
	if (!atomic_read(&p_island->ref_count)) {
		/* power on the island */
		PSB_DEBUG_PM("Power up island %x\n", p_island->island);
		ret = p_island->p_funcs->power_up(g_ospm_data->dev, p_island);
		if (ret)
			p_island->island_state = OSPM_POWER_ON;
		else {
			PSB_DEBUG_PM("Power up island %x failed!\n", p_island->island);
			if (p_island->p_dependency)
				power_down_island(p_island->p_dependency);
			return ret;
		}
	}

	/* increment the ref count */
	atomic_inc(&p_island->ref_count);

	return ret;
}

/**
 * power_down_island
 *
 * Description: Power down the island and all of it's dependent islands
 */
static bool power_down_island(struct ospm_power_island *p_island)
{
	bool ret = true;

	if (atomic_dec_return(&p_island->ref_count) < 0) {
		DRM_ERROR("Island %x, UnExpect RefCount %d\n",
				p_island->island,
				p_island->ref_count);
		dump_stack();
		goto power_down_err;
	}

	/* check to see if island is turned off */
	if (!atomic_read(&p_island->ref_count)) {
		/* power on the island */
		PSB_DEBUG_PM("Power down island %x\n", p_island->island);
		ret = p_island->p_funcs->power_down(
				g_ospm_data->dev,
				p_island);

		/* set the island state */
		if (ret)
			p_island->island_state = OSPM_POWER_OFF;
		else
			goto power_down_err;
	}

	/* handle the dependency later */
	if (p_island->p_dependency) {
		/* Power down dependent island */
		ret = power_down_island(p_island->p_dependency);
		if (!ret)
			goto power_down_err;
	}

	return ret;

power_down_err:
	atomic_inc(&p_island->ref_count);
	ret = false;
	return ret;
}

static bool any_island_on(void)
{
	struct ospm_power_island *p_island = NULL;
	u32 i = 0;
	bool ret = false;

	for (i = 0; i < ARRAY_SIZE(island_list); i++) {
		p_island = &island_list[i];

		if (atomic_read(&p_island->ref_count) > 0) {
			ret = true;
			break;
		}
	}

	return ret;
}

/**
 * power_island_get
 *
 * Description: Notify PowerMgmt module that you will be accessing the
 * specified island's hw so don't power it off.  If the island is not
 * powered up, it will power it on.
 *
 */
bool power_island_get(u32 hw_island)
{
	u32 i = 0;
	bool ret = true, first_island = false;
	int pm_ret;
	struct ospm_power_island *p_island;
	struct drm_psb_private *dev_priv = g_ospm_data->dev->dev_private;

	mutex_lock(&g_ospm_data->ospm_lock);

	if (!any_island_on()) {
		PSB_DEBUG_PM("Resuming PCI\n");
		/* Here, we use runtime pm framework to suit
		 * S3 PCI suspend/resume
		 */
		wake_lock(&dev_priv->ospm_wake_lock);
		pm_ret = pm_runtime_get_sync(&g_ospm_data->dev->pdev->dev);
		if (pm_ret < 0) {
			ret = false;
			PSB_DEBUG_PM("pm_runtime_get_sync failed 0x%x.\n",
				&g_ospm_data->dev->pdev->dev);
			goto out_err;
		}
		first_island = true;

	}

	for (i = 0; i < ARRAY_SIZE(island_list); i++) {
		if (hw_island & island_list[i].island) {
			p_island = &island_list[i];
			ret = power_up_island(p_island);
			if (!ret) {
				PSB_DEBUG_PM("power up failed %x\n",
					island_list[i].island);
				goto out_err;
			}
		}
	}

out_err:
	if (ret && first_island)
		pm_qos_remove_request(&dev_priv->s0ix_qos);
	mutex_unlock(&g_ospm_data->ospm_lock);

	return ret;
}

/**
 * power_island_put
 *
 * Description: Notify PowerMgmt module that you are done accessing the
 * specified island's hw so feel free to power it off.  Note that this
 * function doesn't actually power off the islands.
 */
bool power_island_put(u32 hw_island)
{
	bool ret = true;
	u32 i = 0;
	struct drm_psb_private *dev_priv = g_ospm_data->dev->dev_private;
	struct ospm_power_island *p_island;

	mutex_lock(&g_ospm_data->ospm_lock);

	for (i = 0; i < ARRAY_SIZE(island_list); i++) {
		if (hw_island & island_list[i].island) {
			/* Power down the island if needed */
			p_island = &island_list[i];
			ret = power_down_island(p_island);
			if (!ret) {
				PSB_DEBUG_PM("power down failed %x\n",
					island_list[i].island);
			}
		}
	}

out_err:
	/* Check to see if we need to suspend PCI */
	if (!any_island_on()) {
		PSB_DEBUG_PM("Suspending PCI\n");
		/* Here, we use runtime pm framework to suit
		 * S3 PCI suspend/resume
		 */
		pm_qos_add_request(&dev_priv->s0ix_qos,
				PM_QOS_CPU_DMA_LATENCY, CSTATE_EXIT_LATENCY_S0i1 - 1);
		pm_runtime_put_sync_suspend(&g_ospm_data->dev->pdev->dev);
		wake_unlock(&dev_priv->ospm_wake_lock);
	}
	mutex_unlock(&g_ospm_data->ospm_lock);

	return ret;
}

/**
 * is_island_on
 *
 * Description: checks to see if the island is up
 * returns true if hw_island is ON
 * returns false if hw_island is OFF
 */
bool is_island_on(u32 hw_island)
{
	/* get the power island */
	struct ospm_power_island *p_island = get_island_ptr(hw_island);
	bool island_on = false;

	if (!p_island) {
		DRM_ERROR("p_island is NULL\n");
		return false;
	}

	/* TODO: add lock here. */
	island_on = (p_island->island_state == OSPM_POWER_ON) ? true : false;

	return island_on;
}

u32 pipe_to_island(u32 pipe)
{
	u32 power_island = 0;

	switch (pipe) {
	case 0:
		power_island = OSPM_DISPLAY_A;
		break;
	case 1:
		power_island = OSPM_DISPLAY_B;
		break;
	case 2:
		power_island = OSPM_DISPLAY_C;
		break;
	default:
		DRM_ERROR("%s: invalid pipe %u\n", __func__, pipe);
		return 0;
	}

	return power_island;
}

/**
 * ospm_power_init
 *
 * Description: Initialize this ospm power management module
 */
void ospm_power_init(struct drm_device *dev)
{
	u32 i = 0;
	struct drm_psb_private *dev_priv = dev->dev_private;
	/* allocate ospm data */
	g_ospm_data = kmalloc(sizeof(struct _ospm_data_), GFP_KERNEL);
	if (!g_ospm_data)
		goto out_err;

	mutex_init(&g_ospm_data->ospm_lock);
	g_ospm_data->dev = dev;
	gpDrmDevice = dev;

	wake_lock_init(&dev_priv->ospm_wake_lock, WAKE_LOCK_SUSPEND,
			"ospm_wake_lock");
	/* initilize individual islands */
	for (i = 0; i < ARRAY_SIZE(island_list); i++) {
		island_list[i].p_funcs = kmalloc(sizeof(struct power_ops),
						GFP_KERNEL);
		if ((island_list[i].p_funcs) && (island_list[i].init_func)) {
			island_list[i].init_func(dev, &island_list[i]);
			atomic_set(&island_list[i].ref_count, 0);

			switch (island_list[i].island){
			case OSPM_DISPLAY_A:
			case OSPM_DISPLAY_C:
			case OSPM_DISPLAY_MIO:
				atomic_set(&island_list[i].ref_count, 1);
				island_list[i].island_state = OSPM_POWER_ON;
				if (island_list[i].p_dependency) {
					atomic_inc(&island_list[i].p_dependency->ref_count);
					island_list[i].island_state = OSPM_POWER_ON;
				}

				break;
			default:
				break;
			}
		}
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	/* register early_suspend runtime pm */
	intel_media_early_suspend_init(dev);
#endif
	intel_media_early_suspend_sysfs_init(dev);
	dc_maxfifo_init(dev);
	rtpm_init(dev);
out_err:
	return;
}

/**
 * ospm_power_uninit
 *
 * Description: Uninitialize this ospm power management module
 */
void ospm_power_uninit(void)
{
	int i;
	PSB_DEBUG_PM("%s\n", __func__);

	rtpm_uninit(gpDrmDevice);

#ifdef CONFIG_HAS_EARLYSUSPEND
	/* un-init early suspend */
	intel_media_early_suspend_uninit();
#endif
	intel_media_early_suspend_sysfs_uninit(gpDrmDevice);

	/* Do we need to turn off all islands? */
	power_island_put(OSPM_ALL_ISLANDS);

	for (i = 0; i < ARRAY_SIZE(island_list); i++)
		kfree(island_list[i].p_funcs);

	kfree(g_ospm_data);
}

/**
 * ospm_power_suspend
 *
 * Description: suspend all islands
 */
bool ospm_power_suspend(void)
{
	PSB_DEBUG_PM("%s\n", __func__);

	return true;
}

/**
 * ospm_power_resume
 *
 * Description: resume previously suspended islands.
 */
void ospm_power_resume(void)
{
	PSB_DEBUG_PM("%s\n", __func__);
}

/* FIXME: hkpatel */
/*** LEGACY SUPPORT ****/
/*** REMOVE ONCE CONVERTED ALL FUNCTIONS TO NEW ARCH */

/* Legacy Function for support */
bool ospm_power_using_hw_begin(int hw_island, u32 usage)
{
	bool ret = true;

	/*
	 * FIXME: make ospm_power_using_hw_begin used for Display islands only
	 * take effect for DSPB/HDMIO islands, becaused it's called by the OTM
	 * HDMI codes and not to impact CTP/MDFLD. But eventually need to
	 * replace hw_begin() with power_island_get() in OTM HDMI.
	 */
	if (hw_island == OSPM_DISPLAY_ISLAND)
		hw_island = OSPM_DISPLAY_B | OSPM_DISPLAY_HDMI;

	ret = power_island_get(hw_island);

	return ret;
}
EXPORT_SYMBOL(ospm_power_using_hw_begin);

bool ospm_power_is_hw_on(u32 hw_island)
{
	return is_island_on(hw_island);
}
EXPORT_SYMBOL(ospm_power_is_hw_on);

void ospm_power_using_hw_end(int hw_island)
{
	/*
	 * FIXME: make ospm_power_using_hw_end used for Display islands only
	 * take effect for DSPB/HDMIO islands, becaused it's called by the OTM
	 * HDMI codes and not to impact CTP/MDFLD. But eventually need to
	 * replace hw_end() with power_island_put() in OTM HDMI.
	 */
	if (hw_island == OSPM_DISPLAY_ISLAND)
		hw_island = OSPM_DISPLAY_B | OSPM_DISPLAY_HDMI;

	power_island_put(hw_island);
}
EXPORT_SYMBOL(ospm_power_using_hw_end);

void ospm_apm_power_down_msvdx(struct drm_device *dev, int force_off)
{
	unsigned long irq_flags;
	int ret, frame_finished = 0;
	int seq_flag = 0, shp_ctx_count = 0;
	struct ospm_power_island *p_island;
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;
	struct psb_video_ctx *pos, *n;


	p_island = get_island_ptr(OSPM_VIDEO_DEC_ISLAND);

	if (!p_island) {
		DRM_ERROR("p_island is NULL\n");
		return;
	}

	if (force_off) {
		if (!ospm_power_is_hw_on(OSPM_VIDEO_DEC_ISLAND)) {
			PSB_DEBUG_PM("msvdx in power off.\n");
			return;
		}

		mutex_lock(&g_ospm_data->ospm_lock);
		ret = p_island->p_funcs->power_down(
			g_ospm_data->dev,
			p_island);

		/* set the island state */
		if (ret)
			p_island->island_state = OSPM_POWER_OFF;

		mutex_unlock(&g_ospm_data->ospm_lock);

		/* MSVDX_NEW_PMSTATE(dev, msvdx_priv, PSB_PMSTATE_POWERDOWN); */

		mutex_lock(&g_ospm_data->ospm_lock);
		ret = p_island->p_funcs->power_up(
			g_ospm_data->dev,
			p_island);

		/* set the island state */
		if (ret)
			p_island->island_state = OSPM_POWER_ON;

		mutex_unlock(&g_ospm_data->ospm_lock);

		power_island_put(OSPM_VIDEO_DEC_ISLAND);
		return;
	}

	if (!ospm_power_is_hw_on(OSPM_VIDEO_DEC_ISLAND))
		PSB_DEBUG_PM("msvdx in power off.\n");

	psb_msvdx_dequeue_send(dev);

#ifdef CONFIG_SLICE_HEADER_PARSING
	spin_lock_irqsave(&dev_priv->video_ctx_lock, irq_flags);
	list_for_each_entry_safe(pos, n, &dev_priv->video_ctx, head){
		if (pos->slice_extract_flag){
			shp_ctx_count++;
		}
		if (pos->frame_end) {
			frame_finished = 1;
			pos->frame_end = 0;
			break;
		}
	}
	spin_unlock_irqrestore(&dev_priv->video_ctx_lock, irq_flags);

	if (shp_ctx_count == 0 || frame_finished)
		power_island_put(OSPM_VIDEO_DEC_ISLAND);
#else
	power_island_put(OSPM_VIDEO_DEC_ISLAND);
#endif

	return;
}

void ospm_apm_power_down_topaz(struct drm_device *dev)
{
	int ret;
	struct ospm_power_island *p_island;

	PSB_DEBUG_PM("Power down VEC...\n");
	p_island = get_island_ptr(OSPM_VIDEO_ENC_ISLAND);

	if (!p_island) {
		DRM_ERROR("p_island is NULL\n");
		return;
	}

	mutex_lock(&g_ospm_data->ospm_lock);

	if (!ospm_power_is_hw_on(OSPM_VIDEO_ENC_ISLAND))
		goto out;

	if (atomic_read(&p_island->ref_count)) {
		PSB_DEBUG_PM("vec ref_count has been set(%d), bypass\n",
			     atomic_read(&p_island->ref_count));
		goto out;
	}

	ret = p_island->p_funcs->power_down(
			g_ospm_data->dev,
			p_island);

	/* set the island state */
	if (ret)
		p_island->island_state = OSPM_POWER_OFF;

	PSB_DEBUG_PM("Power down VEC done\n");
out:
	mutex_unlock(&g_ospm_data->ospm_lock);
	return;
}

void ospm_apm_power_down_vsp(struct drm_device *dev)
{
	int ret;
	struct ospm_power_island *p_island;
	unsigned long flags;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct vsp_private *vsp_priv = dev_priv->vsp_private;

	PSB_DEBUG_PM("Power down VPP...\n");
	p_island = get_island_ptr(OSPM_VIDEO_VPP_ISLAND);

	if (!p_island) {
		DRM_ERROR("p_island is NULL\n");
		return;
	}

	mutex_lock(&g_ospm_data->ospm_lock);

	if (!ospm_power_is_hw_on(OSPM_VIDEO_VPP_ISLAND))
		goto out;

	if (atomic_read(&p_island->ref_count))
		PSB_DEBUG_PM("VPP ref_count has been set(%d), bypass\n",
			     atomic_read(&p_island->ref_count));

	if (vsp_priv->vsp_cmd_num > 0) {
		VSP_DEBUG("command in VSP, by pass\n");
		goto out;
	}

	ret = p_island->p_funcs->power_down(
		g_ospm_data->dev,
		p_island);

	/* set the island state */
	if (ret) {
		p_island->island_state = OSPM_POWER_OFF;
		atomic_set(&p_island->ref_count, 0);
	}

	/* handle the dependency */
	if (p_island->p_dependency) {
		/* Power down dependent island */
		power_down_island(p_island->p_dependency);
	}

	if (!any_island_on()) {
		PSB_DEBUG_PM("Suspending PCI\n");
		pm_runtime_put(&g_ospm_data->dev->pdev->dev);
		wake_unlock(&dev_priv->ospm_wake_lock);
	}

	PSB_DEBUG_PM("Power down VPP done\n");
out:
	mutex_unlock(&g_ospm_data->ospm_lock);
	return;
}

int ospm_runtime_pm_allow(struct drm_device *dev)
{
	pm_runtime_allow(&dev->pdev->dev);
	return 0;
}

void ospm_runtime_pm_forbid(struct drm_device *dev)
{
	pm_runtime_forbid(&dev->pdev->dev);
}
