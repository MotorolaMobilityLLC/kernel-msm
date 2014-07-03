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

#ifndef HDCP_API_H
#define HDCP_API_H

#include <linux/types.h>
#include "hdmi_internal.h"

/**
 * Description: function to update HPD status
 *
 * @hdmi_context handle hdmi_context
 * @hpd	 HPD high/low status
 *
 * Returns:	true on success
 *		false on failure
 */
bool otm_hdmi_hdcp_set_hpd_state(hdmi_context_t *hdmi_context,
					bool hpd);

/**
 * Description: function to update power save (suspend/resume) status
 *
 * @hdmi_context handle hdmi_context
 * @suspend	suspend/resume status
 *
 * Returns:	true on success
 *		false on failure
 */
bool otm_hdmi_hdcp_set_power_save(hdmi_context_t *hdmi_context,
					bool suspend);

/**
 * Description: function to update display_power_on status
 *
 * @hdmi_context handle hdmi_context
 * @display_power_on	display power on/off status
 *
 * Returns:	true on success
 *		false on failure
 */
bool otm_hdmi_hdcp_set_dpms(hdmi_context_t *hdmi_context,
					bool display_power_on);

/**
 * Description: Function to check HDCP encryption status
 *
 * @hdmi_context handle hdmi_context
 *
 * Returns:	true if encrypting
 *		else false
 */
bool otm_hdmi_hdcp_enc_status(hdmi_context_t *hdmi_context);

/**
 * Description: Function to check HDCP Phase3 Link status
 *
 * Returns:	true if link is verified Ri Matches
 *		else false
 */
bool otm_hdmi_hdcp_link_status(hdmi_context_t *hdmi_context);

/**
 * Description: Function to read BKSV and validate
 *
 * @hdmi_context handle hdmi_context
 * @bksv	 buffer to store bksv
 *
 * Returns:	true on success
 *		false on failure
 */
bool otm_hdmi_hdcp_read_validate_bksv(hdmi_context_t *hdmi_context,
					uint8_t *bksv);

/**
 * Description: function to enable HDCP
 *
 * @hdmi_context handle hdmi_context
 * @refresh_rate vertical refresh rate of the video mode
 *
 * Returns:	true on success
 *		false on failure
 */
bool otm_hdmi_hdcp_enable(hdmi_context_t *hdmi_context,
					int refresh_rate);

/**
 * Description: function to disable HDCP
 *
 * @hdmi_context handle hdmi_context
 *
 * Returns:	true on success
 *		false on failure
 */
bool otm_hdmi_hdcp_disable(hdmi_context_t *hdmi_context);

/**
 * Description: hdcp init function
 *
 * @hdmi_context handle hdmi_context
 * @ddc_rd_wr:  pointer to ddc read write function
 *
 * Returns:	true on success
 *		false on failure
 */
bool otm_hdmi_hdcp_init(hdmi_context_t *hdmi_context,
	int (*ddc_rd_wr)(bool, uint8_t, uint8_t, uint8_t *, int));

#endif /* HDCP_API_H */
