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


#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/types.h>

#include "otm_hdmi.h"
#include "hdmi_internal.h"
#include "ips_hdmi.h"
#include "mfld_hdmi_reg.h"
#include "ipil_internal.h"
#include "ipil_utils.h"

/**
 * Description: disable video infoframe
 *
 * @dev:        hdmi_device_t
 * @type:       type of infoframe packet
 *
 * Returns:     OTM_HDMI_ERR_NULL_ARG on NULL parameters
 *              OTM_HDMI_ERR_INVAL on invalid packet type
 *              OTM_HDMI_SUCCESS on success
 */
otm_hdmi_ret_t ips_hdmi_disable_vid_infoframe(hdmi_device_t *dev,
					unsigned int type)
{
	uint32_t vid_dip_ctl = 0;
	uint32_t dip_type = 0;
	uint32_t index = 0;

	if (!dev)
		return OTM_HDMI_ERR_NULL_ARG;

	/* Enable Particular Packet Type */
	switch (type) {
	case HDMI_PACKET_AVI:
		dip_type = IPS_HDMI_EN_DIP_TYPE_AVI;
		index = IPS_HDMI_DIP_BUFF_INDX_AVI;
		break;
	case HDMI_PACKET_VS:
		dip_type = IPS_HDMI_EN_DIP_TYPE_VS;
		index = IPS_HDMI_DIP_BUFF_INDX_VS;
		break;
	case HDMI_PACKET_SPD:
		dip_type = IPS_HDMI_EN_DIP_TYPE_SPD;
		index = IPS_HDMI_DIP_BUFF_INDX_SPD;
		break;
	default:
		return OTM_HDMI_ERR_INVAL;
	}

	/* Disable DIP type & set the buffer index & reset access address */
	vid_dip_ctl = hdmi_read32(IPS_HDMI_VID_DIP_CTL_ADDR);
	vid_dip_ctl &= ~(dip_type |
			IPS_HDMI_DIP_BUFF_INDX_MASK |
			IPS_HDMI_DIP_ACCESS_ADDR_MASK |
			IPS_HDMI_DIP_TRANSMISSION_FREQ_MASK);
	vid_dip_ctl |= (index |
			IPS_HDMI_VID_PORT_B_SELECT |
			IPS_HDMI_VID_EN_DIP);
	hdmi_write32(IPS_HDMI_VID_DIP_CTL_ADDR, vid_dip_ctl);

	return OTM_HDMI_SUCCESS;
}

/**
 * Description: enable video infoframe
 *
 * @dev:        hdmi_device_t
 * @type:       type of infoframe packet
 * @pkt:        infoframe packet data
 * @freq:       number of times packet needs to be sent
 *
 * Returns:     OTM_HDMI_ERR_NULL_ARG on NULL parameters
 *              OTM_HDMI_ERR_INVAL on invalid packet type
 *              OTM_HDMI_SUCCESS on success
 */
otm_hdmi_ret_t ips_hdmi_enable_vid_infoframe(hdmi_device_t *dev,
		unsigned int type, otm_hdmi_packet_t *pkt, unsigned int freq)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	uint32_t vid_dip_ctl = 0;
	uint32_t index = 0;
	uint32_t dip_type = 0;
	uint32_t dip_data = 0;

	if (!dev || !pkt)
		return OTM_HDMI_ERR_NULL_ARG;

	if (freq > HDMI_DIP_SEND_ATLEAST_EVERY_OTHER_VSYNC)
		return OTM_HDMI_ERR_INVAL;

	rc = ips_hdmi_disable_vid_infoframe(dev, type);
	if (rc != OTM_HDMI_SUCCESS)
		return rc;

	/* Add delay for any Pending transmissions ~ 2 VSync + 3 HSync */
	msleep_interruptible(32 + 8);

	/* Enable Particular Packet Type */
	switch (type) {
	case HDMI_PACKET_AVI:
		dip_type = IPS_HDMI_EN_DIP_TYPE_AVI;
		index = IPS_HDMI_DIP_BUFF_INDX_AVI;
		break;
	case HDMI_PACKET_VS:
		dip_type = IPS_HDMI_EN_DIP_TYPE_VS;
		index = IPS_HDMI_DIP_BUFF_INDX_VS;
		break;
	case HDMI_PACKET_SPD:
		dip_type = IPS_HDMI_EN_DIP_TYPE_SPD;
		index = IPS_HDMI_DIP_BUFF_INDX_SPD;
		break;
	default:
		return OTM_HDMI_ERR_INVAL;
	}

	/* Disable DIP type & set the buffer index & reset access address */
	vid_dip_ctl = hdmi_read32(IPS_HDMI_VID_DIP_CTL_ADDR);
	vid_dip_ctl &= ~(dip_type |
			IPS_HDMI_DIP_BUFF_INDX_MASK |
			IPS_HDMI_DIP_ACCESS_ADDR_MASK |
			IPS_HDMI_DIP_TRANSMISSION_FREQ_MASK);
	vid_dip_ctl |= (index |
			IPS_HDMI_VID_PORT_B_SELECT |
			IPS_HDMI_VID_EN_DIP);
	hdmi_write32(IPS_HDMI_VID_DIP_CTL_ADDR, vid_dip_ctl);

	/* Write Packet Data */
	dip_data = 0;
	dip_data = (pkt->header[0] << 0) |
		   (pkt->header[1] << 8) |
		   (pkt->header[2] << 16);
	hdmi_write32(IPS_HDMI_VIDEO_DIP_DATA_ADDR, dip_data);

	for (index = 0; index < (HDMI_DIP_PACKET_DATA_LEN/4); index++) {
		dip_data = pkt->data32[index];
		hdmi_write32(IPS_HDMI_VIDEO_DIP_DATA_ADDR, dip_data);
	}

	/* Enable Packet Type & Transmission Frequency */
	vid_dip_ctl = hdmi_read32(IPS_HDMI_VID_DIP_CTL_ADDR);
	vid_dip_ctl &= ~IPS_HDMI_VID_PORT_SELECT_MASK;
	vid_dip_ctl |= (IPS_HDMI_VID_PORT_B_SELECT | IPS_HDMI_VID_EN_DIP);
	vid_dip_ctl |= (dip_type | (freq << IPS_HDMI_DIP_TX_FREQ_SHIFT));
	pr_debug("vid_dip_ctl %x\n", vid_dip_ctl);
	hdmi_write32(IPS_HDMI_VID_DIP_CTL_ADDR, vid_dip_ctl);

	return rc;
}

/**
 * Description: disable all infoframes
 *
 * @dev:	hdmi_device
 *
 * Returns:     OTM_HDMI_ERR_NULL_ARG on NULL parameters
 *		OTM_HDMI_SUCCESS on success
 */
otm_hdmi_ret_t ips_hdmi_disable_all_infoframes(hdmi_device_t *dev)
{
	if (!dev)
		return OTM_HDMI_ERR_NULL_ARG;

	/* Disable Video Related Infoframes */
	hdmi_write32(IPS_HDMI_VID_DIP_CTL_ADDR, 0x0);
	/* Disable Audio Related Infoframes */
	hdmi_write32(IPS_HDMI_AUD_DIP_CTL_ADDR, 0x0);

	/* TODO: Disable other infoframes? */

	return OTM_HDMI_SUCCESS;
}

/**
 * Description: save HDMI display registers
 *
 * @dev:	hdmi_device_t
 *
 * Returns:	none
 */
void ips_hdmi_save_display_registers(hdmi_device_t *dev)
{
	int i;
	if (NULL == dev)  {
		pr_debug("\n%s invalid argument\n", __func__);
		return;
	}

	dev->reg_state.saveDPLL = hdmi_read32(IPS_DPLL_B);
	dev->reg_state.saveFPA0 = hdmi_read32(IPS_DPLL_DIV0);
	dev->reg_state.savePIPEBCONF = hdmi_read32(IPS_PIPEBCONF);
	dev->reg_state.saveHTOTAL_B = hdmi_read32(IPS_HTOTAL_B);
	dev->reg_state.saveHBLANK_B = hdmi_read32(IPS_HBLANK_B);
	dev->reg_state.saveHSYNC_B = hdmi_read32(IPS_HSYNC_B);
	dev->reg_state.saveVTOTAL_B = hdmi_read32(IPS_VTOTAL_B);
	dev->reg_state.saveVBLANK_B = hdmi_read32(IPS_VBLANK_B);
	dev->reg_state.saveVSYNC_B = hdmi_read32(IPS_VSYNC_B);
	dev->reg_state.savePIPEBSRC = hdmi_read32(IPS_PIPEBSRC);
	dev->reg_state.saveDSPBSTRIDE = hdmi_read32(IPS_DSPBSTRIDE);
	dev->reg_state.saveDSPBLINOFF = hdmi_read32(IPS_DSPBLINOFF);
	dev->reg_state.saveDSPBTILEOFF = hdmi_read32(IPS_DSPBTILEOFF);
	dev->reg_state.saveDSPBSIZE = hdmi_read32(IPS_DSPBSIZE);
	dev->reg_state.saveDSPBPOS = hdmi_read32(IPS_DSPBPOS);
	dev->reg_state.saveDSPBSURF = hdmi_read32(IPS_DSPBSURF);
	dev->reg_state.saveDSPBCNTR = hdmi_read32(IPS_DSPBCNTR);
	dev->reg_state.saveDSPBSTATUS = hdmi_read32(IPS_DSPBSTAT);

	/*save palette (gamma) */
	for (i = 0; i < 256; i++)
		dev->reg_state.save_palette_b[i] =
			hdmi_read32(IPS_PALETTE_B + (i<<2));

	dev->reg_state.savePFIT_CONTROL = hdmi_read32(IPS_PFIT_CONTROL);
	dev->reg_state.savePFIT_PGM_RATIOS = hdmi_read32(IPS_PFIT_PGM_RATIOS);
	dev->reg_state.saveHDMIPHYMISCCTL = hdmi_read32(IPS_HDMIPHYMISCCTL);
	dev->reg_state.saveHDMIB_CONTROL = hdmi_read32(IPS_HDMIB_CONTROL);
	dev->reg_state.saveHDMIB_DATALANES = hdmi_read32(IPS_HDMIB_LANES02);

	dev->reg_state.valid = true;
}

/**
 * Description:	saves HDMI data island packets
 *
 * @dev:	hdmi_device_t
 *
 * Returns:	none
 */
void ips_hdmi_save_data_island(hdmi_device_t *dev)
{
	uint32_t index = 0;
	uint32_t reg_val = 0;

	if (NULL == dev)  {
		pr_debug("\n%s invalid argument\n", __func__);
		return;
	}

	/* Save AVI Infoframe Data */
	reg_val = hdmi_read32(IPS_HDMI_VID_DIP_CTL_ADDR);
	if ((reg_val & IPS_HDMI_VID_EN_DIP) &&
	    (reg_val & IPS_HDMI_EN_DIP_TYPE_AVI)) {
		/* set DIP buffer index to AVI */
		reg_val &= ~IPS_HDMI_DIP_BUFF_INDX_MASK;
		reg_val |= IPS_HDMI_DIP_BUFF_INDX_AVI;
		hdmi_write32(IPS_HDMI_VID_DIP_CTL_ADDR, reg_val);
		/* set DIP RAM Access Address to 0 */
		reg_val &= ~IPS_HDMI_DIP_ACCESS_ADDR_MASK;
		hdmi_write32(IPS_HDMI_VID_DIP_CTL_ADDR, reg_val);
		reg_val = hdmi_read32(IPS_HDMI_VID_DIP_CTL_ADDR);
		/* copy transmission frequency */
		dev->avi.freq =
			((reg_val & IPS_HDMI_DIP_TRANSMISSION_FREQ_MASK) >>
				IPS_HDMI_DIP_TX_FREQ_SHIFT);
		/* copy header */
		reg_val = hdmi_read32(IPS_HDMI_VIDEO_DIP_DATA_ADDR);
		dev->avi.pkt.header[0] = reg_val & 0xFF;
		dev->avi.pkt.header[1] = (reg_val >> 8) & 0xFF;
		dev->avi.pkt.header[2] = (reg_val >> 16) & 0xFF;
		/* copy data */
		for (index = 0; index < (HDMI_DIP_PACKET_DATA_LEN/4); index++) {
			dev->avi.pkt.data32[index] =
				hdmi_read32(IPS_HDMI_VIDEO_DIP_DATA_ADDR);
		}
		/* set data valid */
		dev->avi.valid = true;
	} else
		dev->avi.valid = false;
}

/**
 * Description: disable HDMI display
 *
 * @dev:	hdmi_device_t
 *
 * Returns:	none
 */
void ips_disable_hdmi(hdmi_device_t *dev)
{
	int count = 0;
	u32 temp;

	if (NULL == dev)  {
		pr_debug("\n%s invalid argument\n", __func__);
		return;
	}

	/* Disable display plane */
	temp = hdmi_read32(IPS_DSPBCNTR);
	if ((temp & IPIL_DSP_PLANE_ENABLE) != 0) {
		hdmi_write32(IPS_DSPBCNTR, temp & ~IPIL_DSP_PLANE_ENABLE);
		/* Flush the plane changes */
		hdmi_write32(IPS_DSPBSURF, hdmi_read32(IPS_DSPBSURF));
		hdmi_read32(IPS_DSPBSURF);
	}

	/* Next, disable display pipes */
	temp = hdmi_read32(IPS_PIPEBCONF);
	if ((temp & IPIL_PIPEACONF_ENABLE) != 0) {
		temp &= ~IPIL_PIPEACONF_ENABLE;
		temp |= IPIL_PIPECONF_PLANE_OFF | IPIL_PIPECONF_CURSOR_OFF;
		hdmi_write32(IPS_PIPEBCONF, temp);
		hdmi_read32(IPS_PIPEBCONF);

		/* Wait for for the pipe disable to take effect. */
		for (count = 0; count < 1000; count++) {
			temp = hdmi_read32(IPS_PIPEBCONF);
			if (!(temp & IPIL_PIPEACONF_PIPE_STATE))
				break;

			udelay(20);
		}
	}

}

/**
 * Description: restore HDMI data island packets
 *
 * @dev:	hdmi_device_t
 *
 * Returns:	none
 */
void ips_hdmi_restore_data_island(hdmi_device_t *dev)
{
	if (NULL == dev)  {
		pr_debug("\n%s invalid argument\n", __func__);
		return;
	}

	/* restore AVI infoframe */
	if (dev->avi.valid) {
		if (ips_hdmi_enable_vid_infoframe(dev, HDMI_PACKET_AVI,
				&dev->avi.pkt, dev->avi.freq) !=
				OTM_HDMI_SUCCESS)
			pr_debug("\nfailed to program avi infoframe\n");
		dev->avi.valid = false;
	}
}

/**
 * Description: destroys any saved HDMI data
 *
 * @dev:        hdmi_device_t
 *
 * Returns: none
 */
void ips_hdmi_destroy_saved_data(hdmi_device_t *dev)
{
	if (NULL != dev) {
		dev->reg_state.valid = false;
		dev->avi.valid = false;
	}
}

/**
 * Description: disable all planes on pipe
 *
 * @pipe:    pipe ID
 * @enable : true to enable planes; false to disable planes
 *
 */
void ips_enable_planes_on_pipe(uint32_t pipe, bool enable)
{
	uint32_t  temp;

	/* Currently, only handle planes on pipe B */
	switch (pipe) {
	case 1:
		temp = hdmi_read32(IPS_PIPEBCONF);

		if (enable)
			temp &= ~IPIL_PIPECONF_PLANE_OFF;
		else
			temp |= IPIL_PIPECONF_PLANE_OFF;

		hdmi_write32(IPS_PIPEBCONF, temp);

		break;
	default:
		break;
	}

	return;
}
