/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2012 Intel Corporation. All rights reserved.

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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/string.h>
#include "otm_hdmi.h"
#include "ipil_hdmi.h"
#include "ps_hdmi.h"
#include <asm/intel_scu_pmic.h>
#include <asm/intel-mid.h>
#include "pwr_mgmt.h"

/* Implementation of the Moorefield specific PCI driver for receiving
 * Hotplug and other device status signals.
 * In Moorefield platform, the HPD and OCP signals are delivered to the
 * display sub-system using the TI TPD Companion chip.
 */

/* Constants */
#define PS_HDMI_HPD_PCI_DRIVER_NAME "Moorefield HDMI HPD Driver"

/* Globals */
static hdmi_context_t *g_context = NULL;

#define PS_HDMI_MMIO_RESOURCE 0
#define PS_VDC_OFFSET 0x00000000
#define PS_VDC_SIZE 0x000080000
#define PS_MSIC_PCI_DEVICE_ID 0x11A6

#define PS_MSIC_HPD_GPIO_PIN 16
#define PS_MSIC_LS_EN_GPIO_PIN 67
#define PS_MSIC_HPD_GPIO_PIN_NAME "HDMI_HPD"
#define PS_MSIC_LS_EN_GPIO_PIN_NAME "HDMI_LS_EN"


/* For Moorefield, it is required that SW pull up or pull down the
 * LS_OE GPIO pin based on cable status. This is needed before
 * performing any EDID read operation on Moorefield.
 */
static void __ps_gpio_configure_edid_read(void)
{
	static int old_pin_value  = -1;
	int new_pin_value;
	hdmi_context_t *ctx = g_context;

	if (ctx == NULL) {
		pr_err("%s failed due to internal error\n", __func__);
		return;
	}

	new_pin_value = gpio_get_value(ctx->gpio_hpd_pin);
	if (new_pin_value == old_pin_value)
		return;

	old_pin_value = new_pin_value;

	if (new_pin_value == 0)
		gpio_set_value(ctx->gpio_ls_en_pin, 0);
	else
		gpio_set_value(ctx->gpio_ls_en_pin, 1);
	pr_debug("%s: MSIC_LS_OE pin = %d (%d)\n", __func__,
		 gpio_get_value(ctx->gpio_ls_en_pin), new_pin_value);
}

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
	ctx->io_address = ioremap(pci_address, ctx->io_length);
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
	ctx->is_connected_overridden = true;

	/* Handle Moorefield specific GPIO configuration
	 * to enable EDID reads
	 */
	ctx->gpio_hpd_pin = get_gpio_by_name(PS_MSIC_HPD_GPIO_PIN_NAME);
	if (-1 == ctx->gpio_hpd_pin) {
		ctx->gpio_hpd_pin = PS_MSIC_HPD_GPIO_PIN;
		pr_debug("get_gpio_by_name failed! Use default pin %d\n",
				PS_MSIC_HPD_GPIO_PIN);
	}

	ctx->gpio_ls_en_pin = get_gpio_by_name(PS_MSIC_LS_EN_GPIO_PIN_NAME);
	if (-1 == ctx->gpio_ls_en_pin) {
		pr_debug("get_gpio_by_name failed! Use default pin %d\n",
				PS_MSIC_LS_EN_GPIO_PIN);
		ctx->gpio_ls_en_pin = PS_MSIC_LS_EN_GPIO_PIN;
	}

	if (gpio_request(ctx->gpio_ls_en_pin, "HDMI_LS_EN")) {
		pr_err("%s: Unable to request gpio %d\n", __func__,
		       ctx->gpio_ls_en_pin);
		rc = OTM_HDMI_ERR_FAILED;
		goto exit;
	}

	if (!gpio_is_valid(ctx->gpio_ls_en_pin)) {
		pr_err("%s: Unable to validate gpio %d\n", __func__,
		       ctx->gpio_ls_en_pin);
		rc = OTM_HDMI_ERR_FAILED;
		goto exit;
	}

	if (gpio_direction_output(ctx->gpio_ls_en_pin, 0)) {
		pr_err("%s: Failed to set GPIO %d as output\n",
			 __func__, ctx->gpio_ls_en_pin);
		goto exit;
	}
	/* Set the GPIO based on cable status */
	__ps_gpio_configure_edid_read();
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
	iounmap(ctx->io_address);

	/* Free GPIO resources */
	gpio_free(ctx->gpio_ls_en_pin);
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

bool ps_hdmi_power_rails_on(void)
{
	pr_debug("Entered %s\n", __func__);
	return true;
}

bool ps_hdmi_power_rails_off(void)
{
	pr_debug("Entered %s\n", __func__);
	return 0;
}

/* enable/disable IRQ and CPD_HPD */
bool ps_hdmi_enable_hpd(bool enable)
{
	u8 pin = 0;

	pr_debug("Entered %s: %s\n", __func__, enable ? "enable" : "disable");

	/* see ShadyCove PMIC spec and board schema */
	/* PRx uses GPIO0 for CT_CP_HPD */
	pin = 0x7e;

	if (enable)
		intel_scu_ipc_iowrite8(pin, 0x31);
	else
		intel_scu_ipc_iowrite8(pin, 0x30);
	return true;
}

bool ps_hdmi_power_islands_on(void)
{
	/* power on display island C to use overlay C and sprite D planes */
	return ospm_power_using_hw_begin(
			OSPM_DISPLAY_B | OSPM_DISPLAY_HDMI,
			OSPM_UHB_FORCE_POWER_ON);
}

void ps_hdmi_power_islands_off(void)
{
	ospm_power_using_hw_end(
		OSPM_DISPLAY_HDMI | OSPM_DISPLAY_B);
}

void ps_hdmi_vblank_control(struct drm_device *dev, bool on)
{
	/* Won't force turning on/off vblank interrupt for MRFLD. */
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
	if (ctx == NULL)
		return false;

	/* Read HDMI cable status from GPIO */
	/* For Moorefield, it is required that SW pull up or pull down the
	 * LS_OE GPIO pin based on cable status. This is needed before
	 * performing any EDID read operation on Moorefield.
	 */
	__ps_gpio_configure_edid_read();

	if (g_context->override_cable_state)
		return g_context->is_connected_overridden;

	if (gpio_get_value(ctx->gpio_hpd_pin) == 0)
		ctx->is_connected =  false;
	else
		ctx->is_connected = true;
	return ctx->is_connected;
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
	/* Note: do nothing since not clear if mrfld needs this or not */
	return;
}

/**
 * hdmi interrupt handler (top half).
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
	if (g_context == NULL)
		return IRQ_HANDLED;

	return IRQ_WAKE_THREAD;
}

/* Power management functions */
static int ps_hdmi_hpd_suspend(struct device *dev)
{
	pr_debug("Entered %s\n", __func__);
	ps_hdmi_power_rails_off();
	return 0;
}

static int ps_hdmi_hpd_resume(struct device *dev)
{
	pr_debug("Entered %s\n", __func__);
	ps_hdmi_power_rails_on();
	return 0;
}

/* get HDMI hotplug pin number */
int ps_hdmi_get_hpd_pin(void)
{
	if (g_context == NULL)
		return 0;

	return g_context->gpio_hpd_pin;
}

/* override the hdmi hpd cable status */
void ps_hdmi_override_cable_status(bool state, bool auto_state)
{
	if (g_context == NULL)
		return;

	g_context->override_cable_state = auto_state;

	if (state)
		g_context->is_connected_overridden = true;
	else
		g_context->is_connected_overridden = false;
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

	/* Perform the GPIO configuration */
	result = gpio_request(ctx->gpio_hpd_pin, "hdmi_hpd");
	if (result) {
		pr_debug("%s: Failed to request GPIO %d for kbd IRQ\n",
			 __func__, ctx->gpio_hpd_pin);
		goto exit2;
	}

	result = gpio_direction_input(ctx->gpio_hpd_pin);
	if (result) {
		pr_debug("%s: Failed to set GPIO %d as input\n",
			 __func__, ctx->gpio_hpd_pin);
		goto exit3;
	}

	ctx->irq_number = gpio_to_irq(ctx->gpio_hpd_pin);
	pr_debug("%s: IRQ number assigned = %d\n", __func__, ctx->irq_number);

	result = irq_set_irq_type(ctx->irq_number, IRQ_TYPE_EDGE_BOTH);
	if (result) {
		pr_debug("%s: Failed to set HDMI HPD IRQ type for IRQ %d\n",
			 __func__, ctx->irq_number);
		goto exit3;
	}

	/* This is unused on Moorefield platform, since we use GPIO */
	ctx->dev.irq_io_address = 0;

	result = request_threaded_irq(ctx->irq_number, ps_hdmi_irq_handler,
				      ctx->hpd_callback, IRQF_SHARED,
				      PS_HDMI_HPD_PCI_DRIVER_NAME,
				      ctx->hpd_data);
	if (result) {
		pr_debug("%s: Register irq interrupt %d failed\n",
			 __func__, ctx->irq_number);
		goto exit3;
	}
	return result;

exit3:
	gpio_free(ctx->gpio_hpd_pin);
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
	pci_unregister_driver(&ps_hdmi_hpd_driver);
	return 0;
}


