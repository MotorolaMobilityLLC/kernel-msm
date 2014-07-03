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

#ifndef _EDID_INTERNAL_H
#define _EDID_INTERNAL_H

#include "edid.h"

#define EDID_STD_TIMINGS_NUM 8
#define BLOCK_MAP_SIZE 126

/*
 * Structure representing timing descriptor in EDID native format
 * See Table 3.16 of EDID STD and Table 50 of CEA-861-C documents for details
 * Order of fields MUST match documentation
 */
typedef struct {
	unsigned short pixel_clock;
	unsigned char h_active;
	unsigned char h_blanking;
	unsigned char h_active_blanking_hb;
	unsigned char v_active;
	unsigned char v_blanking;
	unsigned char v_active_blanking_hb;
	unsigned char h_sync_offset;
	unsigned char h_sync_pulse_width;
	unsigned char vs_offset_pulse_width;
	unsigned char offset_pulse_width_hb;
	unsigned char h_image_size;
	unsigned char v_image_size;
	unsigned char hv_image_size;
	unsigned char h_border;
	unsigned char v_border;
	unsigned char flags;
} timing_descriptor_t;

/*
 * Structure representing any 18 bytes descriptor
 * See Table 3.19 and 3.20 of EDID STD for details
 * Order of fields MUST match documentation
 */
typedef struct {
	unsigned short flag_required;
	unsigned char flag_reserved;
	unsigned char data_type_tag;
	unsigned char flag;
	unsigned char payload[13];
} generic_descriptor_t;

/*
 * Structure representing EDID CEA extention block
 * See Table 56, Table 26 and section A.2.13 of CEA-861-C for details
 */
typedef struct {
	unsigned char tag;
	unsigned char revision;
	unsigned char content_offset;
	unsigned char flags;
	unsigned char data[124];
} extention_block_cea_t;

/*
 * EDID Block 0
 */
typedef struct {
	unsigned long long signature;
	unsigned short manufacturer_id;
	unsigned short product_code;
	unsigned int serial_number;
	unsigned char manufacture_week;
	unsigned char manufacture_year;
	unsigned char version;
	unsigned char revision;
	unsigned char video_input_definition;
	unsigned char max_horz_image_size;
	unsigned char max_vert_image_size;
	unsigned char gamma;
	unsigned char feature_support;
	unsigned char rg_lowbits;
	unsigned char bw_lowbits;
	unsigned char red_x;
	unsigned char red_y;
	unsigned char green_x;
	unsigned char green_y;
	unsigned char blue_x;
	unsigned char blue_y;
	unsigned char white_x;
	unsigned char white_y;
	unsigned char est_timing_1;
	unsigned char est_timing_2;
	unsigned char est_timing_3;
	unsigned short std_timings[EDID_STD_TIMINGS_NUM];
	unsigned char td_1[sizeof(generic_descriptor_t)];
	unsigned char td_2[sizeof(generic_descriptor_t)];
	unsigned char md_1[sizeof(generic_descriptor_t)];
	unsigned char md_2[sizeof(generic_descriptor_t)];
	unsigned char num_extentions;
} edid_block_zero_t;

/*
 * EDID block map
 */
typedef struct {
	unsigned char tag;
	unsigned char map[BLOCK_MAP_SIZE];
	unsigned short checksum;
} edid_block_map_t;

/*
 * Detailed printing routines prototypes
 */
void print_monitor_descriptor_undecoded(generic_descriptor_t *md,
					printf_t print);

void print_timing_descriptor_undecoded(timing_descriptor_t *td,
				       printf_t print);

void print_block_zero_undecoded(edid_block_zero_t *ebz, printf_t print);

#endif /* _EDID_INTERNAL_H */
