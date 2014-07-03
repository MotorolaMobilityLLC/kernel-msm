/*
 * Copyright © 2006-2007 Intel Corporation
 *
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *	
 */
/* chunfeng.zhao@intel.com
  */

#ifndef PSB_INTEL_HDMI_REG_H
#define PSB_INTEL_HDMI_REG_H

//////////////////////////////////////////
//
// Integrated HDMI specific registers
//
/////////////////////////////////////////

#define RESERVED2(x,y)  x##y
#define RESERVED1(x,y)  RESERVED2(x,y)
#define RANDOMNUMBER	__LINE__	// __COUNTER__
#define UNIQUENAME(ValueName) RESERVED1(ValueName, RANDOMNUMBER)

/** Requird for HDMI operation */
#define   HDMI_NULL_PACKETS_DURING_VSYNC (1 << 9)
#define   HDMI_BORDER_ENABLE		(1 << 7)
#define   HDMI_AUDIO_ENABLE		(1 << 6)
/** New with 965, default is to be set */
#define   HDMI_VSYNC_ACTIVE_HIGH	(1 << 4)
/** New with 965, default is to be set */
#define   HDMI_HSYNC_ACTIVE_HIGH	(1 << 3)
#define   HDMIB_PCIE_CONCURRENCY	(1 << 3)
#define   HDMI_DETECTED			(1 << 2)
//
//AUDIO configuration register
//
#define MDFLD_AUD_CONFIG_REG 0x69000
#define MDFLD_AUD_CONFIG_REG_RESERVED_BITS	BITRANGE(31,25)
typedef union _mdfld_aud_config {
	uint32_t value;

	struct {
		const uint32_t disable_ncts:1;	//Bit 0
		uint32_t lay_out:1;	//Bit 1 (0 - layout 0 1 - layout 1)
		uint32_t format:2;	/*Bit [3:2] 
					 * 00 - l-PCM or IEC 61937 
					 * 01 - High bit rate IEC 61937 stream packet)
					 * 10 - Not supported
					 * 11 - Not supported
					 */
		uint32_t num_audio_ch:2;	/*Bit [5:4]
						 * 00 - 2 channels(stereo)
						 * 01 - 3 or 4 channels
						 * 10 - 5 or 6 channels
						 * 11 - 7 or 8 channels
						 */
		uint32_t UNIQUENAME(Reserved):1;	//Bit 6
		uint32_t b_bit_enabled:1;	/* Bit 7 (0 - B bit set only for sub-packet 0
						 *        1 - B bit set for all valid sub packet)
						 */
		uint32_t sample_flat_bit:1;	//Bit 8
		uint32_t validity_bit:1;	//Bit 9 (1 - set V bit in sub-frame 0 - clear V bit(debugging, testing))
		uint32_t user_bit:1;	//Bit 10 (1 - set U bit in sub frame 0 - clear U bit(default)
		uint32_t underrun_packet_bit:1;	//Bit 11 (1 - send underrun packet 0 - send null packet)
		uint32_t UNIQUENAME(Reserved):20;	//Bit [31:12]
	};
} mdfld_aud_config_t;

//
// Audio control state register
//
#define MDFLD_AUD_CNTL_ST_REG 0x69060
#define MDFLD_AUD_CNTL_ST_RESERVED_BITS	(BITRANGE(14,4) | BITRANGE(31,25))
// Note => DIP : Data Island Packet
typedef union _mdfld_aud_cntl {
	uint32_t value;

	struct {
		uint32_t dip_ram_access_address:4;	// bit 3:0
		uint32_t UNIQUENAME(Reserved):11;	// bit 14:4
		uint32_t cp_ready:1;	// bit 15 
		uint32_t video_dip_trans_freq:2;	// bit 17:16
		uint32_t dip_buffer_index:3;	// bit 20:18
		uint32_t enable_dip_type:4;	// bit 24:21
		uint32_t UNIQUENAME(Reserved):7;	// bit 31:25
	};

} mdfld_aud_cntl_t;

// HDMI Audio Data Island Packet Data
//
#define MDFLD_HDMI_AUDPAC_DATA_REG 0x69114

typedef union _mdfld_hdmi_audpac_data {
	uint32_t value;

	struct {
		uint32_t audio_dip_data:32;	// bit 31:0
	};
} mdfld_hdmi_audpac_data_t;

#endif				// PSB_INTEL_HDMI_REG_H
