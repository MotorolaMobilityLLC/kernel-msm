/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2011 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  Contact Information:

  Intel Corporation
  2200 Mission College Blvd.
  Santa Clara, CA  95054

  BSD LICENSE

  Copyright(c) 2011 Intel Corporation. All rights reserved.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/
#include "otm_hdmi_types.h"

#include <asm/io.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>
#include "otm_hdmi.h"
#include "ipil_hdmi.h"
#include "ps_hdmi.h"
#include "psb_drv.h"

#include <asm/intel_scu_pmic.h>
#include "psb_powermgmt.h"

/* Implementation of the Medfield specific PCI driver for receiving
 * Hotplug and other device status signals.
 * In Medfield platform, the HPD and OCP signals are delivered to the
 * display sub-system from the MSIC chip.
 */

/* Constants */
#define PS_HDMI_HPD_PCI_DRIVER_NAME "Medfield HDMI MSIC Driver"

/* Globals */
static hdmi_context_t *g_context;

#define PS_HDMI_MMIO_RESOURCE 0
#define PS_VDC_OFFSET 0x00000000
#define PS_VDC_SIZE 0x000080000
#define PS_MSIC_PCI_DEVICE_ID 0x0831
#define PS_MSIC_VRINT_ADDR 0xFFFF7FCB
#define PS_MSIC_VRINT_IOADDR_LEN 0x02

#define PS_HDMI_OCP_STATUS			(1 << 2)
#define PS_HDMI_HPD_STATUS_BIT			(1 << 3)

#define PS_MSIC_VCC330CNT			0xd3
#define PS_VCC330_OFF				0x24
#define PS_VCC330_ON				0x37
#define PS_MSIC_VHDMICNT			0xde
#define PS_VHDMI_OFF				0x24
#define PS_VHDMI_ON				0xa4
#define PS_VHDMI_DB_30MS			0x60
#define PS_MSIC_HDMI_STATUS_CMD                 0x281
#define PS_MSIC_HDMI_STATUS                     (1 << 0)
#define PS_MSIC_IRQLVL1_MASK                    0x21
#define PS_VREG_MASK                            (1 << 5)


otm_hdmi_ret_t ps_hdmi_pci_dev_init(void *context, struct pci_dev *pdev)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	int result = 0;
	unsigned int vdc_start;
	uint32_t pci_address = 0;
	uint8_t pci_dev_revision = 0;
	hdmi_context_t *ctx = NULL;

	if (pdev == NULL || context == NULL) {
		rc = OTM_HDMI_ERR_INTERNAL;
		goto exit;
	}
	ctx = (hdmi_context_t *)context;

	pr_debug("get resource start\n");
	result = pci_read_config_dword(pdev, 16, &vdc_start);
	if (result != 0) {
		rc = OTM_HDMI_ERR_FAILED;
		goto exit;
	}
	pci_address = vdc_start + PS_VDC_OFFSET;

	pr_debug("map IO region\n");
	/* Map IO region and save its length */
	ctx->io_length = PS_VDC_SIZE;
	ctx->io_address = ioremap_cache(pci_address, ctx->io_length);
	if (!ctx->io_address) {
		rc = OTM_HDMI_ERR_FAILED;
		goto exit;
	}

	pr_debug("get PCI dev revision\n");
	result = pci_read_config_byte(pdev, 8, &pci_dev_revision);
	if (result != 0) {
		rc = OTM_HDMI_ERR_FAILED;
		goto exit;
	}
	ctx->dev.id = pci_dev_revision;
	/* Store this context for use by MSIC PCI driver */
	g_context = ctx;
exit:
	return rc;
}

otm_hdmi_ret_t ps_hdmi_pci_dev_deinit(void *context)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	hdmi_context_t *ctx = NULL;

	if (context == NULL) {
		rc = OTM_HDMI_ERR_INTERNAL;
		goto exit;
	}
	ctx = (hdmi_context_t *)context;

	/* unmap IO region */
	iounmap(ctx->io_address) ;

exit:
	return rc;
}

otm_hdmi_ret_t ps_hdmi_i2c_edid_read(void *ctx, unsigned int sp,
				  unsigned int offset, void *buffer,
				  unsigned int size)
{
	hdmi_context_t *context = (hdmi_context_t *)ctx;

	char *src = context->edid_raw + sp * SEGMENT_SIZE + offset;
	memcpy(buffer, src, size);

	return OTM_HDMI_SUCCESS;
}

static unsigned char vrint_data;

bool ps_hdmi_power_rails_on(void)
{
	int ret = 0;
	pr_debug("Entered %s\n", __func__);

	if (vrint_data == 0) {
		/* If it is not invoked in response to hot plug event,
		 * then simply a NOP as power rails are never turned off.
		 */
		pr_debug("%s: NOP as there is no HPD.\n", __func__);
		return true;
	}
	/* Turn on HDMI power rails. These will be on in all non-S0iX
	 * states so that HPD and connection status will work. VCC330
	 * will have ~1.7mW usage during idle states when the display
	 * is active
	 */
	ret = intel_scu_ipc_iowrite8(PS_MSIC_VCC330CNT, PS_VCC330_ON);
	if (ret) {
		pr_debug("%s: Failed to power on VCC330.\n", __func__);
		return false;
	}

	if (vrint_data & PS_HDMI_OCP_STATUS) {
		/* When there occurs overcurrent in MSIC HDMI HDP,
		 * need to reset VHDMIEN by clearing to 0 then set to 1
		 */
		ret = intel_scu_ipc_iowrite8(PS_MSIC_VHDMICNT,
					PS_VHDMI_OFF);
		if (ret) {
			pr_debug("%s: Failed to power off VHDMI.\n", __func__);
			goto err;
		}
		vrint_data = 0;
	}


	/* MSIC documentation requires that there be a 500us
	 * delay after enabling VCC330 before you can enable
	 * VHDMI
	 */
	usleep_range(500, 1000);

	/* Extend VHDMI switch de-bounce time, to avoid
	 * redundant MSIC VREG/HDMI interrupt during HDMI
	 * cable plugged in/out
	 */
	ret = intel_scu_ipc_iowrite8(PS_MSIC_VHDMICNT,
				PS_VHDMI_ON |
				PS_VHDMI_DB_30MS);
	if (ret) {
		pr_debug("%s: Failed to power on VHDMI.\n", __func__);
		goto err;
	}

	return true;

err:
	ret = intel_scu_ipc_iowrite8(PS_MSIC_VCC330CNT, PS_VCC330_OFF);
	if (ret) {
		pr_debug("%s: Failed to power off VCC330 during clean up.\n",
				__func__);
		/* Fall through */
	}
	return false;
}


bool ps_hdmi_power_rails_off(void)
{
	/* VCC330 must stay on always for HPD. */
	return true;
}

/* enable/disable HPD */
bool ps_hdmi_enable_hpd(bool enable)
{
}

bool ps_hdmi_power_islands_on()
{
	/*
	 * If pmu_nc_set_power_state fails then accessing HW
	 * reg would result in a crash - IERR/Fabric error.
	 */
	if (pmu_nc_set_power_state(OSPM_DISPLAY_B_ISLAND,
				OSPM_ISLAND_UP, OSPM_REG_TYPE))
		BUG();

	return true;
}

void ps_hdmi_power_islands_off()
{
}

void ps_hdmi_vblank_control(struct drm_device *dev, bool on)
{
	if (on)
		psb_enable_vblank(dev, 1);
	else
		psb_disable_vblank(dev, 1);
}

/*
 * ps_hdmi_get_cable_status - Get HDMI cable connection status
 * @context: hdmi device context
 *
 * Returns - boolean state.
 * true - HDMI cable connected
 * false - HDMI cable disconnected
 */
bool ps_hdmi_get_cable_status(void *context)
{
	hdmi_context_t *ctx = (hdmi_context_t *)context;
	u8 data = 0;

	if (ctx == NULL)
		return false;

	/* Read HDMI cable status from MSIC chip */
	intel_scu_ipc_ioread8(PS_MSIC_HDMI_STATUS_CMD, &data);
	if (data & PS_MSIC_HDMI_STATUS)
		ctx->is_connected = true;
	else
		ctx->is_connected = false;
	return ctx->is_connected;
}

/**
 * hdmi interrupt handler (upper half).
 * @irq:	irq number
 * @data:	data for the interrupt handler
 *
 * Returns:	IRQ_HANDLED on NULL input arguments, and if the
 *			interrupt is not HDMI HPD interrupts.
 *		IRQ_WAKE_THREAD if this is a HDMI HPD interrupt.
 * hdmi interrupt handler (upper half). handles the interrupts
 * by reading hdmi status register and waking up bottom half if needed.
 */
irqreturn_t ps_hdmi_irq_handler(int irq, void *data)
{
	/* Read interrupt status register */
	if (g_context != NULL) {
		vrint_data = readb(g_context->dev.irq_io_address);

		/* handle HDMI HPD interrupts. */
		if (vrint_data & (PS_HDMI_HPD_STATUS_BIT|PS_HDMI_OCP_STATUS))
			return IRQ_WAKE_THREAD;
	}
	return IRQ_HANDLED;
}

/* Power management functions */

/*
 * Platform specific resume function after deep-sleep
 * This function is used to carry out any specific actviity
 * to aid HDMI IP resume in the context of system resume.
 * This function will always be scheduled to execute after
 * the system has finished resuming.
 */
void ps_post_resume_wq(struct work_struct *work)
{
	hdmi_context_t *ctx = container_of(work,
					   hdmi_context_t,
					   post_resume_work);
	int ret = 0;

	pr_debug("Entered %s\n", __func__);
	if (ctx == NULL) {
		pr_err("%s: NULL context!\n", __func__);
		return;
	}

	/* While going to suspend state, the HPD interrupts from MSIC
	 * were masked. During the resume, we do not immediately unmask
	 * the interrupt to avoid race between the resultant hotplug
	 * handlers and system resume activity. Instead, we simply turn
	 * on the HDMI MSIC power rails and schedule this function to be
	 * called after the system finishes a complete resume. At this
	 * time, it is safe to re-enable HPD interrupts.
	 */
	ret = intel_scu_ipc_update_register(PS_MSIC_IRQLVL1_MASK, 0x0,
					    PS_VREG_MASK);
	if (ret) {
		pr_debug("%s: Failed to unmask VREG IRQ.\n",
			__func__);
		goto exit;
	}

exit:
	pr_debug("Exiting %s\n", __func__);
}

static int ps_hdmi_hpd_suspend(struct device *dev)
{
	int ret = 0;

	pr_debug("Entered %s\n", __func__);

	/* suspend process is irreversible */
	ret = intel_scu_ipc_update_register(PS_MSIC_IRQLVL1_MASK, 0xff,
					    PS_VREG_MASK);
	if (ret) {
		pr_debug("%s: Failed to mask VREG IRQ.\n",
			  __func__);
	}

	ret = intel_scu_ipc_iowrite8(PS_MSIC_VHDMICNT, PS_VHDMI_OFF);
	if (ret) {
		pr_debug("%s: Failed to power off VHDMI.\n",
			  __func__);
	}

	ret = intel_scu_ipc_iowrite8(PS_MSIC_VCC330CNT, PS_VCC330_OFF);
	if (ret) {
		pr_debug("%s: Failed to power off VCC330.\n",
			  __func__);
	}

	pr_debug("Exiting %s\n", __func__);
	return ret;
}

static int ps_hdmi_hpd_resume(struct device *dev)
{
	int ret = 0;

	pr_debug("Entered %s\n", __func__);

	ret = intel_scu_ipc_iowrite8(PS_MSIC_VCC330CNT, PS_VCC330_ON);
	if (ret) {
		pr_debug("%s: Failed to power on VCC330.\n",
			  __func__);
		goto err;
	}

	/* MSIC documentation requires that there be a 500us delay
	   after enabling VCC330 before you can enable VHDMI */
	usleep_range(500, 1000);

	ret = intel_scu_ipc_iowrite8(PS_MSIC_VHDMICNT,
				     PS_VHDMI_ON | PS_VHDMI_DB_30MS);
	if (ret) {
		pr_debug("%s: Failed to power on VHDMI.\n",
			  __func__);
		goto err;
	}

	/* We schedule a delayed wok item to be executed only after the
	 * the full system has resumed.
	 */
	queue_work(g_context->post_resume_wq, &g_context->post_resume_work);

	pr_debug("Exiting %s\n", __func__);
	return ret;

err:
	pr_debug("Exiting %s\n", __func__);
	return ret;
}

int ps_hdmi_get_hpd_pin(void)
{
	return 0;
}

void ps_hdmi_override_cable_status(bool state, bool auto_state)
{
	return;
}


/* PCI probe function */
static int ps_hdmi_hpd_probe(struct pci_dev *pdev,
				       const struct pci_device_id *id)
{
	int result = 0;
	hdmi_context_t *ctx = g_context;

	if (pdev == NULL || ctx == NULL) {
		pr_err("%s: called with NULL device or context\n", __func__);
		result = -EINVAL;
		return result;
	}

	/* Verify probe is called for the intended device */
	if (pdev->device != PS_MSIC_PCI_DEVICE_ID) {
		pr_err("%s: called for wrong device id = 0x%x\n", __func__,
		       pdev->device);
		result = -EINVAL;
		goto exit;
	}

	pr_debug("pci_enable_device for 0x%x\n",
					PS_MSIC_PCI_DEVICE_ID);
	result = pci_enable_device(pdev);
	if (result) {
		pr_err("%s: Failed to enable MSIC PCI device = 0x%x\n",
		       __func__, PS_MSIC_PCI_DEVICE_ID);
		goto exit;
	}

	/* Map IO region for IRQ registers */
	ctx->dev.irq_io_address = ioremap_nocache(PS_MSIC_VRINT_ADDR,
						  PS_MSIC_VRINT_IOADDR_LEN);
	if (!ctx->dev.irq_io_address) {
		pr_err("%s: Failed to map IO region for MSIC IRQ\n", __func__);
		result = -ENOMEM;
		goto exit2;
	}

	ctx->irq_number = pdev->irq;
	pr_debug("%s: IRQ number assigned = %d\n", __func__, pdev->irq);

	result = request_threaded_irq(ctx->irq_number, ps_hdmi_irq_handler,
				      ctx->hpd_callback, IRQF_SHARED,
				      PS_HDMI_HPD_PCI_DRIVER_NAME,
				      ctx->hpd_data);
	if (result) {
		pr_debug("%s: Register irq interrupt %d failed\n",
			 __func__, ctx->irq_number);
		goto exit3;
	}

	/* Create Freezable workqueue for post resume HPD operations */
	ctx->post_resume_wq = create_freezable_workqueue("MFLD Post-Resume WQ");
	if (!ctx->post_resume_wq) {
		pr_debug("%s: Failed to create post-resume workqueue\n",
			 __func__);
		goto exit3;
	}

	INIT_WORK(&ctx->post_resume_work, ps_post_resume_wq);
	return result;

exit3:
	iounmap(ctx->dev.irq_io_address);
exit2:
	pci_disable_device(pdev);
exit:
	pci_dev_put(pdev);
	return result;
}

/* PCI driver related structures */
static DEFINE_PCI_DEVICE_TABLE(ps_hdmi_hpd_pci_id) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, PS_MSIC_PCI_DEVICE_ID) },
	{ 0 }
};

static const struct dev_pm_ops ps_hdmi_hpd_pm_ops = {
	.suspend = ps_hdmi_hpd_suspend,
	.resume = ps_hdmi_hpd_resume,
};

static struct pci_driver ps_hdmi_hpd_driver = {
	.name = PS_HDMI_HPD_PCI_DRIVER_NAME,
	.id_table = ps_hdmi_hpd_pci_id,
	.probe = ps_hdmi_hpd_probe,
	.driver.pm = &ps_hdmi_hpd_pm_ops,
};

/* PCI Driver registration function */
int ps_hdmi_hpd_register_driver(void)
{
	pr_debug("%s: Registering PCI driver for HDMI HPD\n", __func__);
	return pci_register_driver(&ps_hdmi_hpd_driver);
}

/* PCI Driver Cleanup function */
int ps_hdmi_hpd_unregister_driver(void)
{
	/* unmap IO region */
	iounmap((void *)g_context->dev.irq_io_address);
	pci_unregister_driver(&ps_hdmi_hpd_driver);

	return 0;
}

/**
 * notify security component of hdcp and hdmi cable status
 *
 * @hdcp	HDCP status: true if phase1 is enabled
 * @cable	HDMI connection status: true if connected
 *
 * Returns:	none
 */
void ps_hdmi_update_security_hdmi_hdcp_status(bool hdcp, bool cable)
{
	/* Note: do nothing since not clear if mfld needs this or not */
	return;
}
