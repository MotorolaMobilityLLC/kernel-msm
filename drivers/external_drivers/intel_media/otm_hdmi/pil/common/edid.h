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

#ifndef _EDID_H
#define _EDID_H


#include <linux/types.h>
#include <linux/i2c.h>

#include "otm_hdmi.h"

#define MAX_TIMINGS 100
#define MAX_CAPS 30
#define SEGMENT_SIZE 128
#define MAX_EDID_BLOCKS 5
#define MAX_DATA_BLOCK_SIZE 32

/**
 * Different types representing pointers to a function
 */
typedef otm_hdmi_ret_t (*i2c_read_t)(void *client_data, unsigned int sp,
				     unsigned int offset, void *buffer,
				     unsigned int size);

typedef int (*printf_t) (const char *fmt, ...);

/**
 * Structure with parsed EDID information returned to end user
 */
typedef struct {
	bool hdmi;	/* HDMI or DVI device */
	unsigned int num_timings;	/* Number of supported timings */
	otm_hdmi_timing_t timings[MAX_TIMINGS];	/* Supported timings */
	unsigned int order[MAX_TIMINGS];	/* VIC order of descovery */
	unsigned int num_caps;	/* Number of supported audiocaps */
	otm_hdmi_audio_cap_t audio_caps[MAX_CAPS];	/* Supported audio caps */
	unsigned char short_audio_descriptor_data[MAX_DATA_BLOCK_SIZE]; /* payload of CEA 861 audio data block holding SADs*/
	unsigned int short_audio_descriptor_count;  /* number of Short Audio Descriptor (SAD) */

	unsigned int speaker_map;	/* Speaker layout */
	unsigned short manufacturer_id;	/* Manufacturer ID */
	unsigned short product_code;	/* Product code */
	unsigned int product_sn;	/* Serial number */
	unsigned int product_year;	/* Year of product manufacture */
	unsigned int product_week;	/* Week of product manufacture */
	unsigned char product_name[14];	/* Product name */
	bool ycbcr444;	/* YCbCr444 support */
	bool ycbcr422;	/* YCbCr422 support */
	unsigned int spa;	/* CEC source physical address */
	bool dc_30;	/* 30 bit Deep Color support */
	bool dc_36;	/* 36 bit Deep Color support */
	bool dc_48;	/* 48 bit Deep Color support */
	bool dc_y444;	/* YCbCr444 support with DC */
	bool xvycc601;	/* xvYCC BT601 Colorimetry */
	bool xvycc709;	/* xvYCC BT709 Colorimetry */
	bool supports_ai;	/* Aux audio info support */
	unsigned int max_tmds_clock;
	bool latency_present;
	bool latency_int_present;
	bool hdmi_video_present;
	int latency_video;
	int latency_audio;
	int latency_video_interlaced;
	int latency_audio_interlaced;
	bool enabled_3d;
	bool supports_60Hz;
	bool supports_50Hz;
	unsigned char max_horz_image_size;
	unsigned char max_vert_image_size;
	int native_idx;
	bool rgb_quant_selectable;	/*rgb quant mode selectable */
	bool ycc_quant_selectable;	/*ycc quant mode selectable */

	const otm_hdmi_timing_t **ref_timings;	/* INPUT: reference timings
						   table */
	unsigned int num_ref_timings;	/* INPUT: size of ref timings table */
} edid_info_t;

otm_hdmi_ret_t edid_parse(edid_info_t *edid_info, i2c_read_t data_read,
			  void *cd);

otm_hdmi_ret_t edid_extension_parse(struct i2c_adapter *adapter,
			 edid_info_t *edid_info, unsigned char *edid);

void print_pd_timing(const otm_hdmi_timing_t *pdt, unsigned int order);
void print_audio_capability(otm_hdmi_audio_cap_t *adb);
void print_speaker_layout(unsigned int layout);
void print_raw_block(unsigned char *buffer, int size);

int find_timing(const otm_hdmi_timing_t *set, int size,
		const otm_hdmi_timing_t *e);

int edid_parse_pd_timing_from_cea_block(edid_info_t *edid_info,
					unsigned char *buffer,
					otm_hdmi_timing_t *pdts);
#endif /* _EDID_H */
