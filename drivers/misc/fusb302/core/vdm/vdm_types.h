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
 * The software supplied herewith by Fairchild Semiconductor (the �Company�)
 * is supplied to you, the Company's customer, for exclusive use with its
 * USB Type C / USB PD products.  The software is owned by the Company and/or
 * its supplier, and is protected under applicable copyright laws.
 * All rights are reserved. Any use in violation of the foregoing restrictions
 * may subject the user to criminal sanctions under applicable laws, as well
 * as to civil liability for the breach of the terms and conditions of this
 * license.
 *
 * THIS SOFTWARE IS PROVIDED IN AN �AS IS� CONDITION. NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
 *****************************************************************************/

/*
 * This file contains various object definitions for VDM.
 */
#ifdef FSC_HAVE_VDM

#ifndef __VDM_TYPES_H__
#define __VDM_TYPES_H__

#include "../platform.h"

#define STRUCTURED_VDM_VERSION 0
#define MAX_NUM_SVIDS 11
#define MAX_MODES_PER_SVID 6

// enumeration of SVIDs
typedef enum {
	VID_UNASSIGNED = 0x0000,
	PD_SID = 0xFF00,
	DP_SID = 0xFF01
	    /* Define Standard/Vendor IDs Here... */
} Svid;

// All VDMs are either structured or unstructured -
// capture that with this enumeration
typedef enum {
	UNSTRUCTURED_VDM = 0b0,
	STRUCTURED_VDM = 0b1,
} VdmType;

// Structured VDM Versions
typedef enum {
	V1P0 = 0b00
	    /* Define future Structured VDM Versions here... */
} SvdmVersion;

// Object Position field for referring to objects in a VDM Message
typedef enum {
	INDEX1 = 0b001,
	INDEX2 = 0b010,
	INDEX3 = 0b011,
	INDEX4 = 0b100,
	INDEX5 = 0b101,
	INDEX6 = 0b110,
	EXIT_ALL = 0b111,
} ObjPos;

// Command Type field enumeration
typedef enum {
	INITIATOR = 0b00,
	RESPONDER_ACK = 0b01,
	RESPONDER_NAK = 0b10,
	RESPONDER_BUSY = 0b11,
} CmdType;

// Command field enumeration
typedef enum {
	DISCOVER_IDENTITY = 1,
	DISCOVER_SVIDS = 2,
	DISCOVER_MODES = 3,
	ENTER_MODE = 4,
	EXIT_MODE = 5,
	ATTENTION = 6,

	/* Define SVID-Specific Commands Here... */
} Command;

// internal form factor of an Unstructured VDM Header
typedef struct {
	Svid svid:16;
	VdmType vdm_type:1;
	FSC_U16 info:15;
} UnstructuredVdmHeader;

// internal form factor of a Structured VDM Header
typedef struct {
	Svid svid:16;
	VdmType vdm_type:1;
	SvdmVersion svdm_version:2;
	ObjPos obj_pos:3;
	CmdType cmd_type:2;
	Command command:5;
} StructuredVdmHeader;

// Product Type field in ID Header
typedef enum {
	UNDEFINED = 0b000,
	HUB = 0b001,
	PERIPHERAL = 0b010,
	PASSIVE_CABLE = 0b011,
	ACTIVE_CABLE = 0b100,
	AMA = 0b101,		// Alternative Mode Adapter
} ProductType;

// internal form factor of an ID Header
typedef struct {
	FSC_BOOL usb_host_data_capable:1;
	FSC_BOOL usb_device_data_capable:1;
	ProductType product_type:3;
	FSC_BOOL modal_op_supported:1;
	FSC_U16 usb_vid:16;
} IdHeader;

// internal form factor for Cert Stat VDO
typedef struct {
	FSC_U32 test_id:20;
} CertStatVdo;

// internal form factor for Product VDO
typedef struct {
	FSC_U16 usb_product_id:16;
	FSC_U16 bcd_device:16;
} ProductVdo;

// enumeration of what I'm calling 'Cable To Letter Type'
// ie. Type-C to Type-A/B/C
typedef enum {
	TO_TYPE_A = 0b00,
	TO_TYPE_B = 0b01,
	TO_TYPE_C = 0b10,
} CableToType;

// enumeration of other Type-C to [something] TODO Come up with a better type name...
typedef enum {
	PLUG = 0b0,
	RECEPTACLE = 0b1,
} CableToPr;

// enumeration of cable latency values
// in nanoseconds (max) eg NS20 means 10ns-20ns latency.
typedef enum {
	NS10 = 0b0001,
	NS20 = 0b0010,
	NS30 = 0b0011,
	NS40 = 0b0100,
	NS50 = 0b0101,
	NS60 = 0b0110,
	NS70 = 0b0111,
	NS1000 = 0b1000,
	NS2000 = 0b1001,
	NS3000 = 0b1010,
} CableLatency;

// enumeration of Cable Termination Type
typedef enum {
	BOTH_PASSIVE_NO_VCONN = 0b00,	// note: NO_VCONN means VCONN not _required_
	BOTH_PASSIVE_VCONN = 0b01,	// VCONN required
	ONE_EACH_VCONN = 0b10,
	BOTH_ACTIVE_VCONN = 0b11,
} CableTermType;

// enumeration for configurability of SS Directionality
typedef enum {
	FIXED = 0b0,
	CONFIGURABLE = 0b1,
} SsDirectionality;

// enumeration for VBUS Current Handling Capability
typedef enum {
	VBUS_1P5A = 0b00,	// 1.5 Amps
	VBUS_3A = 0b01,
	VBUS_5A = 0b10,
} VbusCurrentHandlingCapability;

// enumeration for VBUS through cable-ness
typedef enum {
	NO_VBUS_THRU_CABLE = 0b0,
	VBUS_THRU_CABLE = 0b1,
} VbusThruCable;

// enumeration for SOP'' presence
typedef enum {
	SOP2_NOT_PRESENT = 0b0,
	SOP2_PRESENT = 0b1,
} Sop2Presence;

// enumeration for USB Superspeed Signaling Support
typedef enum {
	USB2P0_ONLY = 0b000,
	USB3P1_GEN1 = 0b001,
	USB3P1_GEN1N2 = 0b010,	// Gen 1 and Gen 2
} UsbSsSupport;

// internal form factor for Cable VDO
typedef struct {
	FSC_U8 cable_hw_version:4;
	FSC_U8 cable_fw_version:4;
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
	UsbSsSupport usb_ss_supp:3;
} CableVdo;

// enumeration for power needed by adapter for full functionality
typedef enum {
	VCONN_1W = 0b000,
	VCONN_1P5W = 0b001,
	VCONN_2W = 0b010,
	VCONN_3W = 0b011,
	VCONN_4W = 0b100,
	VCONN_5W = 0b101,
	VCONN_6W = 0b110,
} VConnFullPower;

// enumeration for VCONN being required by an adapter
typedef enum {
	VCONN_NOT_REQUIRED = 0b0,
	VCONN_REQUIRED = 0b1,
} VConnRequirement;

// enumeration for VBUS being required by an adapter
typedef enum {
	VBUS_NOT_REQUIRED = 0b0,
	VBUS_REQUIRED = 0b1,
} VBusRequirement;

// enumeration for USB Superspeed Signaling support by an AMA...
// very similar to the USB SS Support for Cable VDO, but a little different in the PD spec.
typedef enum {
	AMA_USB2P0_ONLY = 0b000,
	AMA_GEN1 = 0b001,
	AMA_GEN1N2 = 0b010,
	AMA_USB2P0_BILLBOARD = 0b011,
} AmaUsbSsSupport;

// internal form factor for Alternate Mode Adapter VDO
typedef struct {
	FSC_U8 cable_hw_version:4;
	FSC_U8 cable_fw_version:4;
	SsDirectionality sstx1_dir_supp:1;
	SsDirectionality sstx2_dir_supp:1;
	SsDirectionality ssrx1_dir_supp:1;
	SsDirectionality ssrx2_dir_supp:1;
	VConnFullPower vconn_full_power:3;
	VConnRequirement vconn_requirement:1;
	VBusRequirement vbus_requirement:1;
	AmaUsbSsSupport usb_ss_supp:3;
} AmaVdo;

// internal form factor for an SVID VDO 
typedef struct {
	FSC_U16 SVID0:16;
	FSC_U16 SVID1:16;
} SvidVdo;

// 'Identity' Object - this is how we'll store data received from Discover Identity messages
typedef struct {
	FSC_BOOL nack;		// set to true to nack a Discover Identity
	// TODO: also put BUSY answer in here

	IdHeader id_header;
	CertStatVdo cert_stat_vdo;

	FSC_BOOL has_product_vdo;
	ProductVdo product_vdo;

	FSC_BOOL has_cable_vdo;
	CableVdo cable_vdo;

	FSC_BOOL has_ama_vdo;
	AmaVdo ama_vdo;
} Identity;

// SVID Info object - give the system an easy struct to pass info to us through a callback
typedef struct {
	FSC_BOOL nack;		// set to true to NACK the Discover SVIDs
	// TODO: incorporate BUSY
	FSC_U32 num_svids;
	FSC_U16 svids[MAX_NUM_SVIDS];
} SvidInfo;

// Modes Info object  give the system an easy struct to pass info to us through a callback
typedef struct {
	FSC_BOOL nack;		// set to true to NACK the Discover SVIDs
	// TODO: incorporate BUSY
	FSC_U16 svid;
	FSC_U32 num_modes;
	FSC_U32 modes[MAX_MODES_PER_SVID];
} ModesInfo;

#endif // header guard

#endif // FSC_HAVE_VDM
