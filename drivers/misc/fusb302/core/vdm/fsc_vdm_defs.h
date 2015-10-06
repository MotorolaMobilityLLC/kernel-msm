/****************************************************************************
 * Company:         Fairchild Semiconductor
 *
 * Author           Date          Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * G. Noblesmith
 *
 *
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Software License Agreement:
 *
 * The software supplied herewith by Fairchild Semiconductor (the “Company”)
 * is supplied to you, the Company's customer, for exclusive use with its
 * USB Type C / USB PD products.  The software is owned by the Company and/or
 * its supplier, and is protected under applicable copyright laws.
 * All rights are reserved. Any use in violation of the foregoing restrictions
 * may subject the user to criminal sanctions under applicable laws, as well
 * as to civil liability for the breach of the terms and conditions of this
 * license.
 *
 * THIS SOFTWARE IS PROVIDED IN AN “AS IS” CONDITION. NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
 *****************************************************************************/

#ifndef __FSC_VDM_DEFS_H__
#define __FSC_VDM_DEFS_H__

#include "vdm_types.h"

// definition/configuration object - these are all the things that the system needs to configure.
typedef struct {
	BOOL data_capable_as_usb_host:1;
	BOOL data_capable_as_usb_device:1;
	ProductType product_type:3;
	BOOL modal_operation_supported:1;
	UINT16 usb_vendor_id:16;
	UINT32 test_id:20;	// for Cert Stat VDO, "allocated by USB-IF during certification"
	UINT16 usb_product_id:16;
	UINT16 bcd_device:16;
	UINT8 cable_hw_version:4;
	UINT8 cable_fw_version:4;
	CableToType cable_to_type:2;
	CableToPr cable_to_pr:1;
	CableLatency cable_latency:4;
	CableTermType cable_term:2;
	SsDirectionality sstx1_dir_supp:1;
	SsDirectionality sstx2_dir_supp:1;
	SsDirectionality ssrx1_dir_supp:1;
	SsDirectionality ssrx2_dir_supp:1;
	VbusCurrentHandlingCapability vbus_current_handling_cap:2;
	VbusThruCable vbus_thru_cable:1;
	Sop2Presence sop2_presence:1;

	VConnFullPower vconn_full_power:3;
	VConnRequirement vconn_requirement:1;
	VBusRequirement vbus_requirement:1;

	UsbSsSupport usb_ss_supp:3;
	AmaUsbSsSupport ama_usb_ss_supp:3;

	UINT32 num_svids;
	UINT16 svids[MAX_NUM_SVIDS];
	UINT32 num_modes_for_svid[MAX_NUM_SVIDS];

	// TODO: A lot of potential wasted memory here...
	UINT32 modes[MAX_NUM_SVIDS][MAX_MODES_PER_SVID];

} VendorDefinition;

#endif // header guard
