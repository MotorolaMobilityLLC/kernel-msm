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

#include <linux/mutex.h>
#include <asm/intel-mid.h>
#include <asm/intel_scu_ipc.h>

#include <linux/pm_runtime.h>
#include "psb_drv.h"
#include "pwr_mgmt.h"

extern struct ospm_power_island island_list[9];
extern struct drm_device *gpDrmDevice;

int rtpm_suspend(struct device *dev)
{
	struct drm_psb_private *dev_priv = gpDrmDevice->dev_private;
	PSB_DEBUG_PM("%s\n", __func__);

	rtpm_suspend_pci();
	if (pm_qos_request_active(&dev_priv->s0ix_qos))
		pm_qos_remove_request(&dev_priv->s0ix_qos);

	return 0;
}

int rtpm_resume(struct device *dev)
{
	struct drm_psb_private *dev_priv = gpDrmDevice->dev_private;
	PSB_DEBUG_PM("%s\n", __func__);
	pm_qos_add_request(&dev_priv->s0ix_qos,
			PM_QOS_CPU_DMA_LATENCY, CSTATE_EXIT_LATENCY_S0i1 - 1);
	/* No OPs of GFX/VED/VEC/VSP/DISP */
	rtpm_resume_pci();

	return 0;
}

int rtpm_idle(struct device *dev)
{
	int ref_count = 0;
	int i;

	PSB_DEBUG_PM("%s\n", __func__);

	for (i = 0; i < ARRAY_SIZE(island_list); i++)
		ref_count += atomic_read(&island_list[i].ref_count);

	if (ref_count) {
		PSB_DEBUG_PM("%s return busy\n", __func__);
		return -EBUSY;
	} else
		return 0;
}

int rtpm_allow(struct drm_device *dev)
{
	PSB_DEBUG_PM("%s\n", __func__);
	pm_runtime_allow(&dev->pdev->dev);
	return 0;
}

void rtpm_forbid(struct drm_device *dev)
{
	PSB_DEBUG_PM("%s\n", __func__);
	pm_runtime_forbid(&dev->pdev->dev);
	return;
}

void rtpm_init(struct drm_device *dev)
{
	rtpm_allow(dev);
}

void rtpm_uninit(struct drm_device *dev)
{
	pm_runtime_get_noresume(&dev->pdev->dev);
}
