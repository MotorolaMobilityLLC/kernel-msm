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

#ifndef MDFLD_HDCP_IF_H
#define MDFLD_HDCP_IF_H

// Constants
#define CP_HDCP_KEY_SELECTION_VECTOR_SIZE       5

// Protection level (HDCP)
typedef enum _cp_protection_level_hdcp {
	CP_PROTECTION_LEVEL_HDCP_OFF = 0,
	CP_PROTECTION_LEVEL_HDCP_ON = 1,
} cp_protection_level_hdcp_t;

// Protection type
typedef enum _cp_protection_type {
	CP_PROTECTION_TYPE_UNKNOWN = 0x80000000,
	CP_PROTECTION_TYPE_NONE = 0x00000000,
	CP_PROTECTION_TYPE_HDCP = 0x00000001,
	CP_PROTECTION_TYPE_MASK = 0x80000001,
} cp_protection_type_t;

typedef enum _cp_status {
	STATUS_UNSUCCESSFUL = 0x80000000,
	STATUS_SUCCESS = 0x00000000,
	STATUS_NOT_SUPPORTED = 0x00000001,
	STATUS_INVALID_DEVICE_REQUEST = 0x00000002,
	STATUS_REVOKED_HDCP_DEVICE_ATTACHED = 0x00000003,
	STATUS_DATA_ERROR = 0x00000004,
	STATUS_PENDING = 0x00000005,
	STATUS_INVALID_PARAMETER = 0x00000006,
} cp_status_t;

// KSV
typedef struct _ksv_t {
	uint8_t ab_ksv[CP_HDCP_KEY_SELECTION_VECTOR_SIZE];
} ksv_t;

// HDCP
typedef struct _hdcp_data {
	uint32_t ksv_list_length;	// Length of the revoked KSV list (set)
	//ksv_t                       aksv;               // KSV of attached device
	//ksv_t                       bksv;               // KSV of attached device
	ksv_t *ksv_list;	// List of revoked KSVs (set)
	int perform_second_step;	// True when the second authentication step is requested (get)
	int is_repeater;	// True when a repeater is attached to the connector (get and set)
} hdcp_data_t;

// CP Parameters
typedef struct _cp_parameters {
	uint32_t protect_type_mask;	// Protection type mask (get and set)
	uint32_t level;		// Protection level (get and set)
	hdcp_data_t hdcp;	// HDCP specific data (get and set)
} cp_parameters_t;

extern uint32_t hdcp_set_cp_data(cp_parameters_t * cp);
extern uint32_t hdcp_get_cp_data(cp_parameters_t * cp);
#endif				/* MDFLD_HDCP_IF_H */
