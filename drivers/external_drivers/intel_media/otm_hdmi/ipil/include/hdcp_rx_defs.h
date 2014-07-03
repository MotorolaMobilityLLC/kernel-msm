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

#ifndef HDCP_RX_DEFS_H
#define HDCP_RX_DEFS_H

#include "hdmi_internal.h"

#define HDCP_PRIMARY_I2C_ADDR	0x3A

#define HDCP_MAX_RETRY_STATUS	(1500)
#define HDCP_MAX_DEVICES        (127)

#define HDCP_KSV_SIZE		0x05
#define HDCP_KSV_HAMMING_WT	(20)
#define HDCP_AN_SIZE		0x08
#define HDCP_RI_SIZE		0x02
#define HDCP_PJ_SIZE		0x01
#define HDCP_V_H_SIZE		(20)


#define HDCP_RX_BKSV_ADDR	0x00
#define HDCP_RX_RI_ADDR		0x08
#define HDCP_RX_PJ_ADDR		0x0A
#define HDCP_RX_AKSV_ADDR	0x10

#define HDCP_RX_AINFO_ADDR	0x15
#define HDCP_RX_AINFO_SIZE	0x01

#define HDCP_RX_AN_ADDR		0x18

#define HDCP_RX_V_H_ADDR	0x20

#define HDCP_RX_V_H0_ADDR	0x20
#define HDCP_RX_V_H0_SIZE	0x04

#define HDCP_RX_V_H1_ADDR	0x24
#define HDCP_RX_V_H1_SIZE	0x04

#define HDCP_RX_V_H2_ADDR	0x28
#define HDCP_RX_V_H2_SIZE	0x04

#define HDCP_RX_V_H3_ADDR	0x2C
#define HDCP_RX_V_H3_SIZE	0x04

#define HDCP_RX_V_H4_ADDR	0x30
#define HDCP_RX_V_H4_SIZE	0x04

#define HDCP_RX_BCAPS_ADDR	0x40
#define HDCP_RX_BCAPS_SIZE	0x01

#define HDCP_RX_BSTATUS_ADDR	0x41
#define HDCP_RX_BSTATUS_SIZE	0x02

#define HDCP_RX_KSV_FIFO_ADDR	0x43

struct hdcp_rx_bcaps_t {
	union {
		uint8_t value;
		struct {
			uint8_t fast_reauthentication:1 ;
			uint8_t b1_1_features:1 ;
			uint8_t reserved:2 ;
			uint8_t fast_transfer:1 ;
			uint8_t ksv_fifo_ready:1 ;
			uint8_t is_repeater:1 ;
			uint8_t hdmi_reserved:1 ;
		};
	};
};

struct hdcp_rx_bstatus_t {
	union {
		uint16_t value;
		struct {
			uint16_t device_count:7 ;
			uint16_t max_devs_exceeded:1 ;
			uint16_t depth:3 ;
			uint16_t max_cascade_exceeded:1 ;
			uint16_t hdmi_mode:1 ;
			uint16_t reserved2:1 ;
			uint16_t rsvd:2 ;
		};
	};
};


#endif /* HDCP_RX_DEFS_H */

