/*
 * Copyright (c) 2010, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Authors:
 *	jim liu <jim.liu@intel.com>
 */

#ifndef MDFLD_HDCP_H
#define MDFLD_HDCP_H

#include "mdfld_hdcp_if.h"

#define MAX_HDCP_DEVICES 127
#define KSV_SIZE           5
#define V_SIZE            20

#define HDCP_MAX_RETRY_STATUS 1500

#define HDCP_100MS_DELAY 100	//

#define HDCP_BKSV_BYTES 5

typedef struct _hdcp_priv_data {
	int enabled;
	int drmFD;
	int output_id;
} hdcp_priv_data_t;
typedef struct _sqwordt {
	union {
		unsigned long long quad_part;	//ULONGLONG QuadPart;
		struct {
			unsigned long low_part;
			unsigned long high_part;
		} u;
		struct {
			uint8_t byte[8];
		};
	};
} sqword_tt; //need_check_later
// A quadword size member 
//
typedef struct _hqword {
	union {
		struct {
			uint64_t major_part:40;	// lower 40 bits
			uint64_t unused1:24;
		};
		struct {
			unsigned major_part_low:32;	// lower 32 bits
			unsigned major_part_high:8;	// lower  bits
			unsigned unused2:24;
		};
		struct {
			uint8_t byte[8];
		};
	};
} hqword_t;

// HDCP related definitions are kept here for common usability between 
// Integrated and External SDVO based HDCP operations
//I2C Address for HDCP cummnication with Receiver
#define RX_ADDRESS      0x74000000	// HDCP Port I2C Address (Single Link)
				    //  shifted for call back function

//I2C Subaddress Defines - As per the HDCP Spec
// Downstream spec does not specify which is MSB and LSB?
#define RX_BKSV_0       0x00	// BKSV[7:0]
#define RX_BKSV_1       0x01	// BKSV[15:8]
#define RX_BKSV_2       0x02	// BKSV[23:16]
#define RX_BKSV_3       0x03	// BKSV[31:24]
#define RX_BKSV_4       0x04	// BKSV[39:32]
#define RX_RI_HIGH      0x08	// Ri'[7:0]
#define RX_RI_LOW       0x09	// Ri'[15:8]
#define RX_AKSV_0       0x10	// AKSV[7:0]
#define RX_AKSV_1       0x11	// AKSV[15:8]
#define RX_AKSV_2       0x12	// AKSV[23:16]
#define RX_AKSV_3       0x13	// AKSV[31:24]
#define RX_AKSV_4       0x14	// AKSV[39:32]... write this byte last
#define RX_AINFO        0x15	// Receiver register to inform it to enable 1.1 features
#define RX_AN_0         0x18	// An[7:0]
#define RX_AN_1         0x19	// An[15:8]
#define RX_AN_2         0x1A	// An[23:16]
#define RX_AN_3         0x1B	// An[31:24]
#define RX_AN_4         0x1C	// An[39:32]
#define RX_AN_5         0x1D	// An[47:40]
#define RX_AN_6         0x1E	// An[55:48]
#define RX_AN_7         0x1F	// An[63:56]
#define RX_VPRIME_H0_0  0x20	// V'[7:0]
#define RX_VPRIME_H0_1  0x21	// V'[15:8]
#define RX_VPRIME_H0_2  0x22	// V'[23:16]
#define RX_VPRIME_H0_3  0x23	// V'[31:24]
#define RX_VPRIME_H1_0  0x24	// V'[39:32]
#define RX_VPRIME_H1_1  0x25	// V'[47:40]
#define RX_VPRIME_H1_2  0x26	// V'[55:48]
#define RX_VPRIME_H1_3  0x27	// V'[63:56]
#define RX_VPRIME_H2_0  0x28	// V'[71:64]
#define RX_VPRIME_H2_1  0x29	// V'[79:72]
#define RX_VPRIME_H2_2  0x2A	// V'[87:80]
#define RX_VPRIME_H2_3  0x2B	// V'[95:88]
#define RX_VPRIME_H3_0  0x2C	// V'[103:96]
#define RX_VPRIME_H3_1  0x2D	// V'[111:104]
#define RX_VPRIME_H3_2  0x2E	// V'[119:112]
#define RX_VPRIME_H3_3  0x2F	// V'[127:120]
#define RX_VPRIME_H4_0  0x30	// V'[135:128]
#define RX_VPRIME_H4_1  0x31	// V'[143:136]
#define RX_VPRIME_H4_2  0x32	// V'[151:144]
#define RX_VPRIME_H4_3  0x33	// V'[159:152]
#define RX_BCAPS        0x40	// [7] RSVD, [6] Repeater, [5] Ready, [4] Fast, [3:2] RSVD, [1] Features, [0] Fast_reauthentication
#define RX_BSTATUS_0    0x41	// [7] MAX_DEVS_EXCEEDED, [6:0] DEVICE_COUNT
#define RX_BSTATUS_1    0x42	// [15:14] RSVD, [13] HDMI_RSVD, [12] HDMI_MODE, [11] MAX_CASCADE_EXCEEDED, [10:8] DEPTH
#define RX_KSV_FIFO     0x43

typedef enum _mdfld_hdcp_rx_data_type_enum {
	RX_TYPE_BKSV_DATA = 0,
	RX_TYPE_BCAPS = 1,
	RX_TYPE_BSTATUS = 2,
	RX_TYPE_REPEATER_KSV_LIST = 3,
	RX_TYPE_REPEATER_PRIME_V = 4,
	RX_TYPE_RI_DATA = 5,
	RX_TYPE_BINFO = 6
} mdfld_hdcp_rx_data_type_en;

typedef struct _hdcp_bstatus {
	unsigned device_count:7;	// [6:0] Total Number of Receiver Devices (excluding repeaters) attached
	unsigned max_devices_exceeded:1;	// [7] Topology Error. Greater than 127 devices attached
	unsigned repeater_depth:3;	// [10:8] Repeater depth 
	unsigned max_cascade_exceeded:1;	// [11] Topology Error. Greater than 7 levels of Repeater attached
	unsigned reserved:20;	// [31:12] Reserved for future expansion
} hdcp_bstatus_t;

//
// BCAPS
//
typedef union _hdcp_rx_bcaps {
	uint8_t value;
	struct {
		uint8_t fast_reauthantication:1;	// bit 0
		uint8_t b1_1features_supported:1;	// bit 1
		uint8_t reserved:2;	// bi 3:2
		uint8_t fast_transfer:1;	// bit 4 ( TRUE  = transfer speed at 400 kHz FALSE = transfer speed at 100 Khz)
		uint8_t ksv_fifo_ready:1;	// bit 5
		uint8_t is_reapeater:1;	// bit 6
		uint8_t reserved1:1;	// bit 7
	};
} hdcp_rx_bcaps_t;

//
// BSTATUS
//
typedef union _hdcp_rx_bstatus {
	uint16_t value;
	struct {
		uint16_t device_count:7;	// bit 6:0
		uint16_t max_devs_exceeded:1;	// bit 7
		uint16_t depth:3;	// bit 10:8
		uint16_t max_cascade_exceeded:1;	// bit 11
		uint16_t rx_in_hdmi_mode:1;	// bit 12
		uint16_t rserved:3;	// bit 15:13
	};
} hdcp_rx_bstatus_t;

// HDCP authentication step
typedef enum _hdcp_authentication_step {
	HDCP_AUTHENTICATION_STEP_NONE = 0,
	HDCP_AUTHENTICATION_STEP_1 = 1,
	HDCP_AUTHENTICATION_STEP_2 = 2,
} hdcp_authentication_step_t;

// KSV_GET
typedef struct _aksv_get {
	uint8_t uc_aksv[CP_HDCP_KEY_SELECTION_VECTOR_SIZE];
} aksv_get_t;

int hdcp_init(void **data, int drmFD, int output_id);
int hdcp_uninit(void *data);
int hdcp_query(void);
int hdcp_is_valid_bksv(uint8_t * buffer, uint32_t size);
int hdcp_is_enabled(void);
int hdcp_enable(int enable);
int read_hdcp_port(uint32_t read_request_type, uint8_t * buffer, int size);
#endif				/* MDFLD_HDCP_H */
