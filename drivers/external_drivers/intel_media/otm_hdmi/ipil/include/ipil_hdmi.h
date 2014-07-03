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

#ifndef __IPIL_HDMI_H
#define __IPIL_HDMI_H

#include <linux/interrupt.h>
#include <linux/types.h>
#include "otm_hdmi.h"

#include "hdmi_internal.h"
#include "hdmi_hal.h"


/**
 * Description: pass hdmi device information to lower layer
 * @dev:	hdmi_device_t
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_NULL_ARG on bad argument
 */
otm_hdmi_ret_t ipil_hdmi_set_hdmi_dev(hdmi_device_t *dev);

/**
 * Description: programs hdmi pipe src and size of the input.
 *
 * @dev:		hdmi_device_t
 * @scalingtype		scaling type (FULL_SCREEN, CENTER, NO_SCALE etc.)

 * @mode:		mode requested
 * @adjusted_mode:	adjusted mode
 * @fb_width, fb_height:allocated frame buffer dimensions
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments
 */
otm_hdmi_ret_t ipil_hdmi_crtc_mode_set_program_dspregs(hdmi_device_t *dev,
					int scalingtype,
					ipil_timings_t *mode,
					ipil_timings_t *adjusted_mode,
					int fb_width, int fb_height);

/**
 * Description: this is pre-modeset configuration. This can be
 *		resetting HDMI unit, disabling/enabling dpll etc
 *		on the need basis.
 *
 * @dev:	hdmi_device_t
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments
 */
otm_hdmi_ret_t ipil_hdmi_crtc_mode_set_prepare(hdmi_device_t *dev);

/**
 * Description: programs all the timing registers based on scaling type.
 *
 * @dev:		hdmi_device_t
 * @scalingtype:	scaling type (FULL_SCREEN, CENTER, NO_SCALE etc.)
 * @mode:		mode requested
 * @adjusted_mode:	adjusted mode
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments
 */
otm_hdmi_ret_t ipil_hdmi_crtc_mode_set_program_timings(hdmi_device_t *dev,
					int scalingtype,
					otm_hdmi_timing_t *mode,
					otm_hdmi_timing_t *adjusted_mode);

/**
 * Description: programs dpll clocks, enables dpll and waits
 *		till it locks with DSI PLL
 *
 * @dev:	hdmi_device_t
 * @dclk:	refresh rate dot clock in kHz of current mode
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments
 */
otm_hdmi_ret_t	ipil_hdmi_crtc_mode_set_program_dpll(hdmi_device_t *dev,
							unsigned long dclk);

/**
 * Description: configures the display plane register and enables
 *		pipeconf.
 *
 * @dev: hdmi_device_t
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments
 */
otm_hdmi_ret_t ipil_hdmi_crtc_mode_set_program_pipeconf(hdmi_device_t *dev);

/**
 * Description: enable infoframes
 *
 * @dev:	hdmi_device_t
 * @type:       type of infoframe packet
 * @pkt:	infoframe packet data
 * @freq:       number of times packet needs to be sent
 *
 * Returns:     OTM_HDMI_ERR_NULL_ARG on NULL parameters
 *		OTM_HDMI_ERR_INVAL on invalid packet type
 *		OTM_HDMI_SUCCESS on success
 */
otm_hdmi_ret_t ipil_hdmi_enable_infoframe(hdmi_device_t *dev,
					unsigned int type,
					otm_hdmi_packet_t *pkt,
					unsigned int freq);

/**
 * Description: disable particular infoframe
 *
 * @dev:	hdmi_device_t
 * @type:       type of infoframe packet
 *
 * Returns:     OTM_HDMI_ERR_NULL_ARG on NULL parameters
 *		OTM_HDMI_ERR_INVAL on invalid packet type
 *		OTM_HDMI_SUCCESS on success
 */
otm_hdmi_ret_t ipil_hdmi_disable_infoframe(hdmi_device_t *dev,
					unsigned int type);

/**
 * Description: disable all infoframes
 *
 * @dev:	hdmi_device_t
 *
 * Returns:     OTM_HDMI_ERR_NULL_ARG on NULL parameters
 *		OTM_HDMI_SUCCESS on success
 */
otm_hdmi_ret_t ipil_hdmi_disable_all_infoframes(hdmi_device_t *dev);

/**
 * Description: encoder mode set function for hdmi. enables phy.
 *		set correct polarity for the current mode, sets
 *		correct panel fitting.
 *
 *
 * @dev:		hdmi_device_t
 * @mode:		mode requested
 * @adjusted_mode:	adjusted mode
 * @is_monitor_hdmi:	is monitor type is hdmi or not
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments
 */
otm_hdmi_ret_t ipil_hdmi_enc_mode_set(hdmi_device_t *dev,
					otm_hdmi_timing_t *mode,
					otm_hdmi_timing_t *adjusted_mode,
					bool is_monitor_hdmi);
/**
 * Description: save HDMI display registers
 *
 * @dev:	hdmi_device_t
 *
 * Returns: none
 */
void ipil_hdmi_save_display_registers(hdmi_device_t *dev);
void ipil_hdmi_save_data_island(hdmi_device_t *dev);

/**
 * Description: restore HDMI display registers and enable display
 *
 * @dev:	hdmi_device_t
 *
 * Returns: none
 */
void ipil_hdmi_restore_and_enable_display(hdmi_device_t *dev);
void ipil_hdmi_restore_data_island(hdmi_device_t *dev);

/**
 * Description: destroys any saved HDMI data
 *
 * @dev:	hdmi_device_t
 *
 * Returns: none
 */
void ipil_hdmi_destroy_saved_data(hdmi_device_t *dev);

/**
 * Description: disable HDMI display
 *
 * @dev:	hdmi_device_t
 *
 * Returns: none
 */
void ipil_disable_hdmi(hdmi_device_t *dev);

/**
 * Description: get pixel clock range
 *
 * @pc_min:	minimum pixel clock
 * @pc_max:	maximum pixel clock
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_FAILED on NULL input arguments.
 */
otm_hdmi_ret_t ipil_get_pixel_clock_range(unsigned int *pc_min,
						unsigned int *pc_max);

/**
 * Returns if the given values is preferred mode or not
 * @hdisplay	: width
 * @vdisplay	: height
 * @refresh	: refresh rate
 *
 * Returns true if preferred mode else false
 */
bool ipil_hdmi_is_preferred_mode(int hdisplay, int vdisplay, int refresh);

/**
 * Description: disable all planes on pipe
 *
 * @pipe: pipe ID
 * @enable : true to enable planes; false to disable planes
 *
 */
void ipil_enable_planes_on_pipe(uint32_t pipe, bool enable);

#endif /* __IPIL_HDMI_H */
