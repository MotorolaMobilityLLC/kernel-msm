#ifndef __FSC_VDM_DEFS_H__
#define __FSC_VDM_DEFS_H__

#include "vdm_types.h"

// definition/configuration object - these are all the things that the system needs to configure.
// TODO: sub divide this... at least into an 'Identity Information' struct so we can request specifically IdHeader info from the system.
typedef struct {
	bool data_capable_as_usb_host:1;
	bool data_capable_as_usb_device:1;
	ProductType product_type:3;
	bool modal_operation_supported:1;
	u16 usb_vendor_id:16;
	u32 test_id:20;		// for Cert Stat VDO, "allocated by USB-IF during certification"
	u16 usb_product_id:16;
	u16 bcd_device:16;
	u8 cable_hw_version:4;
	u8 cable_fw_version:4;
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

	// TODO: Find out if it's possible to unify these two fields? They are nearly identical. - Gabe
	UsbSsSupport usb_ss_supp:3;
	AmaUsbSsSupport ama_usb_ss_supp:3;

	// TODO:        What's a good limit on total number of SVIDs?
	//                      Or should we allow an arbitrarily large amount...?
	unsigned int num_svids;
	u16 svids[MAX_NUM_SVIDS];	// TODO: Do we need different lists of SVIDs we respond with for different SOPs?
	unsigned int num_modes_for_svid[MAX_NUM_SVIDS];

	// TODO: A lot of potential wasted memory here...
	u32 modes[MAX_NUM_SVIDS][MAX_MODES_PER_SVID];
} VendorDefinition;

#endif // header guard
