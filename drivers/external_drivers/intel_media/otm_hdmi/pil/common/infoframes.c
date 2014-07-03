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


#include <linux/kernel.h>
#include <linux/types.h>
#include "otm_hdmi.h"
#include "ipil_hdmi.h"

#include "hdmi_internal.h"
#include "hdmi_timings.h"
#include "infoframes_api.h"


/**
 * This enumeration represents possible color space settings found in GBD
 * Current driver logic assumes we only support colorimetries that can be
 * advertised in EDID: xvYCC601 and xvYCC709
 */
enum {
	GBD_CS_ITU_BT709 = 0,
	GBD_CS_XVYCC601 = 1,
	GBD_CS_XVYCC709 = 2,
	GBD_CS_XYZ = 3,
	GBD_CS_RESERVED = 4,
};

static int __compute_check_sum(otm_hdmi_packet_t *packet)
{
	uint8_t i = 0;
	uint8_t sum = 0;

	for (i = 0; i < 3; i++)
		sum += packet->header[i];
	for (i = 1; i < 28; i++)
		sum += packet->data[i];

	packet->data[0] = (uint8_t)(0xFF - sum + 1);

	return (int)packet->data[0];
}

/**
 * Note: higher level ensures that input value is valid
 */
static int __pfconvert(otm_hdmi_output_pixel_format_t pf)
{
	int rc = 0;	/* init to RGB444 */

	switch (pf) {
	case OTM_HDMI_OPF_RGB444:
		rc = 0;
		break;
	case OTM_HDMI_OPF_YUV422:
		rc = 1;
		break;
	case OTM_HDMI_OPF_YUV444:
		rc = 2;
		break;
	default:
		rc = 0;
		break;
	}

	return rc;
}

otm_hdmi_ret_t hdmi_packet_check_type(otm_hdmi_packet_t *p,
					hdmi_packet_type_t type)
{
	return ((p && p->header[0] == type) ?
		OTM_HDMI_SUCCESS : OTM_HDMI_ERR_FAILED);
}

/*
 * Description: set avi infoframe based on mode
 *
 * @context:	hdmi_context
 * @mode:	mode requested
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments
 */
otm_hdmi_ret_t otm_hdmi_infoframes_set_avi(void *context,
					otm_hdmi_timing_t *mode)
{
	hdmi_context_t *ctx = (hdmi_context_t *)context;
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	unsigned int type = HDMI_PACKET_AVI;
	unsigned int freq = HDMI_DIP_SEND_EVERY_VSYNC;
	int cs = GBD_CS_XVYCC601;
	otm_hdmi_packet_t avi_pkt;
	unsigned int pf, vic;
	bool p, ext;
	otm_hdmi_par_t par = PD_ATTR_UINT(ATTRS[OTM_HDMI_ATTR_ID_PAR]);

	if (!context || !mode) {
		pr_debug("\ninvalid arguments\n");
		return OTM_HDMI_ERR_NULL_ARG;
	}

	/* Set header to AVI */
	avi_pkt.header[0] = 0x82;
	avi_pkt.header[1] = 0x02;
	avi_pkt.header[2] = 0x0D;
	/* Clear payload section */
	memset(avi_pkt.data, 0, sizeof(avi_pkt.data));

	/* RGB, Active Format Info valid, no bars */
	/* use underscan as HDMI video is composed with all
	 * active pixels and lines with or without border
	 */
	avi_pkt.data[1] = 0x12;
	/* Set color component sample format */
	pf = __pfconvert(PD_ATTR_UINT
			 (ATTRS[OTM_HDMI_ATTR_ID_PIXEL_FORMAT_OUTPUT]));
	avi_pkt.data[1] |= pf << 5;
	/* Colorimetry */
	ext = PD_ATTR_BOOL(ATTRS[OTM_HDMI_ATTR_ID_COLOR_SPACE_EXT]);
	avi_pkt.data[2] =
	    (ext ? 3 : (!pf ? 0 : ((mode->width <= 720) ? 0x01 : 0x02))) << 6;

	/* Fill PAR for all supported modes
	 * This is required for passing compliance tests
	 */

	if (mode->mode_info_flags & OTM_HDMI_PAR_16_9)
		par = OTM_HDMI_PAR_16_9;
	else if (mode->mode_info_flags & OTM_HDMI_PAR_4_3)
		par = OTM_HDMI_PAR_4_3;

	pr_debug("%s: Selecting PAR %d for mode vic %lu\n", __func__,
		 par, mode->metadata);

	avi_pkt.data[2] |= par << 4;
	/* Fill FAR */
	avi_pkt.data[2] |= PD_ATTR_UINT(ATTRS[OTM_HDMI_ATTR_ID_FAR]);

	/* Get extended colorimetry from slot 1 */
	if (hdmi_packet_check_type(&ctx->pi_1.packet, HDMI_PACKET_GAMUT) ==
	    OTM_HDMI_SUCCESS)
		cs = ctx->pi_1.packet.data[0] & 0x07;
	/* Get extended colorimetry from slot 0 */
	else if (hdmi_packet_check_type(&ctx->pi_0.packet, HDMI_PACKET_GAMUT) ==
		 OTM_HDMI_SUCCESS)
		cs = ctx->pi_0.packet.data[0] & 0x07;

	/* Fill extended colorimetry */
	avi_pkt.data[3] = ((cs == GBD_CS_XVYCC601) ? 0 : 1) << 4;

	/* Fill quantization range */
	if (ctx->edid_int.rgb_quant_selectable
	    && PD_ATTR_UINT(ATTRS[OTM_HDMI_ATTR_ID_PIXEL_FORMAT_OUTPUT]) ==
	    OTM_HDMI_OPF_RGB444)
		avi_pkt.data[3] |=
			PD_ATTR_BOOL(ATTRS[OTM_HDMI_ATTR_ID_OUTPUT_CLAMP]) ?
			(0x01 << 2) : (0x02 << 2);

	/* Only support RGB output, 640x480: full range Q0=1, Q1=0
	* other timing: limited range Q0=0, Q1=1 */
	avi_pkt.data[3] &= ~OTM_HDMI_COLOR_RANGE_MASK;
	if (mode->width == 640 && mode->height == 480)
		avi_pkt.data[3] |= 0x02 << 2;
	else
		avi_pkt.data[3] |= 0x01 << 2;

	/* Fill Video Identification Code [adjust VIC according to PAR] */
	vic = mode->metadata;
	avi_pkt.data[4] = vic;

	/* Fill pixel repetition value: 2x for 480i and 546i */
	p = ((mode->mode_info_flags & PD_SCAN_INTERLACE) == 0);
	avi_pkt.data[5] = ((mode->width == 720) && !p) ? 0x01 : 0x00;
	/* Fill quantization range */
	if (ctx->edid_int.ycc_quant_selectable
	    && (PD_ATTR_UINT(ATTRS[OTM_HDMI_ATTR_ID_PIXEL_FORMAT_OUTPUT]) ==
							OTM_HDMI_OPF_YUV444 ||
		PD_ATTR_UINT(ATTRS[OTM_HDMI_ATTR_ID_PIXEL_FORMAT_OUTPUT]) ==
							OTM_HDMI_OPF_YUV422))
		avi_pkt.data[5] |=
			PD_ATTR_BOOL(ATTRS[OTM_HDMI_ATTR_ID_OUTPUT_CLAMP]) ?
			(0x00 << 6) : (0x01 << 6);

	/* Compute and fill checksum */
	avi_pkt.data[0] = __compute_check_sum(&avi_pkt);

	/* Enable AVI infoframe */
	rc = ipil_hdmi_enable_infoframe(&ctx->dev, type, &avi_pkt, freq);

	return rc;
}

/*
 * Description: disable all infoframes
 *
 * @context:        hdmi_context
 *
 * Returns:     OTM_HDMI_ERR_NULL_ARG on NULL parameters
 *              OTM_HDMI_SUCCESS on success
*/
otm_hdmi_ret_t otm_hdmi_disable_all_infoframes(void *context)
{
	hdmi_context_t *ctx = (hdmi_context_t *)context;
	if (!ctx)
		return OTM_HDMI_ERR_NULL_ARG;

	return ipil_hdmi_disable_all_infoframes(&ctx->dev);
}
