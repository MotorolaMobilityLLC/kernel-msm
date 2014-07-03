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

#ifndef MFLD_HDCP_REG_H
#define MFLD_HDCP_REG_H


/* Register Definitions */
#define MDFLD_HDMIB_CNTRL_REG		0x61140
#define MDFLD_HDMIB_HDCP_PORT_SEL	(0x1 << 5)

/* HDCP Config & Status */
#define MDFLD_HDCP_CONFIG_REG		0x61400
#define MDFLD_HDCP_STATUS_REG		0x61448
/* AN & AKSV */
#define MDFLD_HDCP_INIT_REG		0x61404
#define MDFLD_HDCP_AN_LOW_REG		0x61410
#define MDFLD_HDCP_AN_HI_REG		0x61414
#define MDFLD_HDCP_AKSV_HI_REG		0x61450
#define MDFLD_HDCP_AKSV_LOW_REG		0x61454
/* BKSV */
#define MDFLD_HDCP_BKSV_LOW_REG		0x61408
#define MDFLD_HDCP_BKSV_HI_REG		0x6140C
/* Rx-Ri */
#define MDFLD_HDCP_RECEIVER_RI_REG	0x61418
/* Repeater Control & Status */
#define MDFLD_HDCP_REP_REG		0x61444
/* Repeater SHA */
#define MDFLD_HDCP_VPRIME_H0		0x6142C
#define MDFLD_HDCP_VPRIME_H1		0x61430
#define MDFLD_HDCP_VPRIME_H2		0x61434
#define MDFLD_HDCP_VPRIME_H3		0x61438
#define MDFLD_HDCP_VPRIME_H4		0x6143C
#define MDFLD_HDCP_SHA1_IN		0x61440
/* Akey */
#define MDFLD_HDCP_AKEY_LO_REG		0x6141C
#define MDFLD_HDCP_AKEY_MED_REG		0x61420
#define MDFLD_HDCP_AKEY_HI_REG		0x61424

#define HDCP_CONVERT_ENDIANNESS(x)	(((x&0x000000ff)<<24)|\
					((x&0x0000ff00)<<8)|\
					((x&0x00ff0000)>>8)|\
					((x&0xff000000)>>24))

struct double_word_t {
	union {
		uint64_t value;
		struct {
			uint32_t low;
			uint32_t high;
		};
		struct {
			uint8_t byte[8];
		};
	};
};

struct sqword_t {
	union {
		unsigned long long quad_part;
		struct {
			unsigned long low_part;
			unsigned long high_part;
		} u;
		struct {
			uint8_t byte[8];
		};
	};
};

enum ips_hdcp_config_enum {
	HDCP_Off = 0,
	HDCP_CAPTURE_AN = 1,
	HDCP_DECRYPT_KEYS = 2,
	HDCP_AUTHENTICATE_AND_ENCRYPT = 3,
	HDCP_PULL_FUSE = 4,
	HDCP_UNIQUE_MCH_ID = 5,
	HDCP_ENCRYPT_KEYS = 6,
	HDCP_CYPHER_CHECK_MODE = 7,
	HDCP_FUSE_PULL_ENABLE = 0x20
};

struct ips_hdcp_config_reg_t {
		union {
		uint32_t value;
		struct {
			uint32_t hdcp_config:3;
			uint32_t reserved:29;
		};
	};
};

struct ips_hdcp_status_reg_t {
	union {
		uint32_t value;
		struct {
			uint32_t ainfo:8;
			uint32_t frame_count:8;
			uint32_t hdcp_on:1;
			uint32_t an_ready:1;
			uint32_t ri_ready:1;
			uint32_t ri_match:1;
			uint32_t encrypting:1;
			uint32_t ready_for_encr:1;
			uint32_t umch_id_ready:1;
			uint32_t mac_status:1;
			uint32_t fus_complete:1;
			uint32_t fus_success:1;
			uint32_t reserved:6;
		};
	};
};

/* Repeater Control register */
enum ips_hdcp_repeater_status_enum {
	HDCP_REPEATER_STATUS_IDLE = 0,
	HDCP_REPEATER_STATUS_BUSY = 1,
	HDCP_REPEATER_STATUS_RDY_NEXT_DATA = 2,
	HDCP_REPEATER_STATUS_COMPLETE_NO_MATCH = 4,
	HDCP_REPEATER_STATUS_COMPLETE_MATCH = 12
};

enum ips_hdcp_repeater_ctrl_enum {
	HDCP_REPEATER_CTRL_IDLE = 0,
	HDCP_REPEATER_32BIT_TEXT_IP = 1,
	HDCP_REPEATER_COMPLETE_SHA1 = 2,
	HDCP_REPEATER_24BIT_TEXT_8BIT_MO_IP = 4,
	HDCP_REPEATER_16BIT_TEXT_16BIT_MO_IP = 5,
	HDCP_REPEATER_8BIT_TEXT_24BIT_MO_IP = 6,
	HDCP_REPEATER_32BIT_MO_IP = 7
};

struct ips_hdcp_repeater_reg_t {
	union {
		uint32_t value;
		struct {
			uint32_t present:1;
			uint32_t control:3;
			uint32_t reserved1:12;
			const uint32_t status:4;
			uint32_t reserved2:12;
		};
	};
};

#endif /* MFLD_HDCP_REG_H */
