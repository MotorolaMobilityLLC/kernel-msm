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

#ifndef __IPS_HDMI_H
#define __IPS_HDMI_H

#include <linux/interrupt.h>
#include <linux/types.h>
#include "otm_hdmi_types.h"
#include "hdmi_internal.h"

#define PCI_DEVICE_HDMI 0x080D
#define PCI_LENGTH_HDMI 0x7000

#define IPS_DPLL_B           (0x0f018)
#define IPS_DPLL_DIV0        (0x0f048)
#define IPS_PIPEBCONF        (0x71008)
#define IPS_HTOTAL_B         (0x61000)
#define IPS_HBLANK_B         (0x61004)
#define IPS_HSYNC_B          (0x61008)
#define IPS_VTOTAL_B         (0x6100c)
#define IPS_VBLANK_B         (0x61010)
#define IPS_VSYNC_B          (0x61014)
#define IPS_PIPEBSRC         (0x6101c)
#define IPS_DSPBSTRIDE       (0x71188)
#define IPS_DSPBLINOFF       (0X71184)
#define IPS_DSPBTILEOFF      (0x711A4)
#define IPS_DSPBSIZE         (0x71190)
#define IPS_DSPBPOS          (0x7118C)
#define IPS_DSPBSURF         (0x7119C)
#define IPS_DSPBCNTR         (0x71180)
#define IPS_DSPBSTAT         (0x71024)
#define IPS_PALETTE_B        (0x0a800)
#define IPS_PFIT_CONTROL     (0x61230)
#define IPS_PFIT_PGM_RATIOS  (0x61234)
#define IPS_HDMIPHYMISCCTL   (0x61134)
#define IPS_HDMIB_CONTROL    (0x61140)
#define IPS_HDMIB_LANES02    (0x61120)
#define IPS_HDMIB_LANES3     (0x61124)
#define IPS_PFIT_ENABLE		(1 << 31)

/* HSYNC and VSYNC Polarity Mask Bits */
#define IPS_HSYNC_POLARITY_MASK (1 << 3)
#define IPS_VSYNC_POLARITY_MASK (1 << 4)

struct ips_clock_t {
	int dot;
	int m;
	int p1;
};

struct ips_range_t {
	int min, max;
};

struct ips_clock_limits_t {
	struct ips_range_t dot, m, p1;
};

/**
 * Description: disable video infoframe
 *
 * @dev:	hdmi_device_t
 * @type:       type of infoframe packet
 *
 * Returns:     OTM_HDMI_ERR_NULL_ARG on NULL parameters
 *		OTM_HDMI_ERR_INVAL on invalid packet type
 *		OTM_HDMI_SUCCESS on success
 */
otm_hdmi_ret_t ips_hdmi_disable_vid_infoframe(hdmi_device_t *dev,
					unsigned int type);

/**
 * Description: enable video infoframe
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
otm_hdmi_ret_t ips_hdmi_enable_vid_infoframe(hdmi_device_t *dev,
					unsigned int type,
					otm_hdmi_packet_t *pkt,
					unsigned int freq);

/**
 * Description: disable all infoframes
 *
 * @dev:	hdmi_device_t
 *
 * Returns:	OTM_HDMI_ERR_NULL_ARG on NULL parameters
 *		OTM_HDMI_SUCCESS on success
 */
otm_hdmi_ret_t ips_hdmi_disable_all_infoframes(hdmi_device_t *dev);

/**
 * Description: gets the best dpll clock value based on
 *		current timing mode clock.
 *
 * @clk:		refresh rate dot clock in kHz of current mode
 * @pdpll, pfp:		will be set to adjusted dpll values.
 * @pclock_khz:		tmds clk value for the best pll and is needed for audio.
 *			This field has to be moved into OTM audio
 *			interfaces when implemented
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments.
 */
otm_hdmi_ret_t ips_hdmi_get_adjusted_clk(unsigned long clk,
				u32 *pdpll, u32 *pfp, uint32_t *pclock_khz);

/**
 * Description: restore HDMI display registers and enable display
 *
 * @dev:        hdmi_device_t
 *
 * Returns: none
 */
void ips_hdmi_restore_and_enable_display(hdmi_device_t *dev);
void ips_hdmi_restore_data_island(hdmi_device_t *dev);

/**
 * Description: save HDMI display registers
 *
 * @dev:        hdmi_device_t
 *
 * Returns: none
 */
void ips_hdmi_save_display_registers(hdmi_device_t *dev);
void ips_hdmi_save_data_island(hdmi_device_t *dev);

/**
 * Description: destroys any saved HDMI data
 *
 * @dev:        hdmi_device_t
 *
 * Returns: none
 */
void ips_hdmi_destroy_saved_data(hdmi_device_t *dev);

/**
 * Description: disable HDMI display
 *
 * @dev:        hdmi_device_t
 *
 * Returns: none
 */
void ips_disable_hdmi(hdmi_device_t *dev);

/**
 * Description: get pixel clock range
 *
 * @pc_min:	minimum pixel clock
 * @pc_max:	maximum pixel clock
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_FAILED on NULL input arguments.
 */
otm_hdmi_ret_t ips_get_pixel_clock_range(unsigned int *pc_min,
						unsigned int *pc_max);

/**
 * Returns if the given values is preferred mode or not
 * @hdisplay	: width
 * @vdisplay	: height
 * @refresh	: refresh rate
 *
 * Returns true if preferred mode else false
 */
bool ips_hdmi_is_preferred_mode(int hdisplay, int vdisplay, int refresh);

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
otm_hdmi_ret_t	ips_hdmi_crtc_mode_set_program_dpll(hdmi_device_t *dev,
							unsigned long dclk);

/**
 * Description: disable all planes on pipe
 *
 * @pipe:    pipe ID
 * @enable : true to enable planes; false to disable planes
 *
 */
void ips_enable_planes_on_pipe(uint32_t pipe, bool enable);

#endif /* __IPS_HDMI_H */
