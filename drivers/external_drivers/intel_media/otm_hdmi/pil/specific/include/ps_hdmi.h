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

#ifndef _PLATFORM_SPEC_H
#define _PLATFORM_SPEC_H

#include <linux/interrupt.h>
#include <linux/pci.h>
#include <asm/intel_scu_ipc.h>
#include "otm_hdmi_types.h"
#include "otm_hdmi_defs.h"
#include "edid.h"

otm_hdmi_ret_t ps_hdmi_i2c_edid_read(void *ctx, unsigned int sp,
				  unsigned int offset, void *buffer,
				  unsigned int size);

otm_hdmi_ret_t ps_hdmi_pci_dev_init(void *context, struct pci_dev *pdev);

otm_hdmi_ret_t ps_hdmi_pci_dev_deinit(void *context);

/*
 * ps_hdmi_get_cable_status - Get HDMI cable connection status
 * @context: hdmi device context
 *
 * Returns - boolean state.
 * true - HDMI cable connected
 * false - HDMI cable disconnected
 */
bool ps_hdmi_get_cable_status(void *context);

/* turn on HDMI power rails not used for HPD */
bool ps_hdmi_power_rails_on(void);

bool ps_hdmi_hdcp_power_islands_on(void);


/* turn off HDMI power rails not used for HPD */
bool ps_hdmi_power_rails_off(void);

/* turn on HDMI power islands */
bool ps_hdmi_power_islands_on(void);

/* turn off HDMI power islands */
void ps_hdmi_power_islands_off(void);

/* enable IRQ and CPD_HPD */
bool ps_hdmi_enable_hpd(bool enable);

/* control HDMI vblank interrupt */
void ps_hdmi_vblank_control(struct drm_device *dev, bool on);

/* get HDMI hotplug pin number */
int ps_hdmi_get_hpd_pin(void);

/* override the hdmi hpd cable status */
void ps_hdmi_override_cable_status(bool state, bool auto_state);


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
irqreturn_t ps_hdmi_irq_handler(int irq, void *data);

/* PCI Driver registration function */
int ps_hdmi_hpd_register_driver(void);

/* PCI Driver Cleanup function */
int ps_hdmi_hpd_unregister_driver(void);

/**
 * notify security component of hdcp and hdmi cable status
 *
 * @hdcp	HDCP status: true if phase1 is enabled
 * @cable	HDMI connection status: true if connected
 *
 * Returns:	none
 */
void ps_hdmi_update_security_hdmi_hdcp_status(bool hdcp, bool cable);

#endif /* _PLATFORM_SPEC_H */
