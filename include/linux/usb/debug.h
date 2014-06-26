/*
 * <linux/usb/debug.h> -- USB Debug Class definitions.
 *
 * Copyright (C) 2008-2010, Intel Corporation.
 *
 * This software is distributed under the terms of the GNU General Public
 * License ("GPL") version 2, as published by the Free Software Foundation.
 *
 */

#ifndef __LINUX_USB_DEBUG_H
#define __LINUX_USB_DEBUG_H

#include <linux/types.h>

/* Debug Interface Subclass Codes */
#define USB_SUBCLASS_DVC_GP		0x05
#define USB_SUBCLASS_DVC_DFX		0x06
#define USB_SUBCLASS_DVC_TRACE		0x07
#define USB_SUBCLASS_DEBUG_CONTROL	0x08

/* Debug Interface Function Protocol */
#define DC_PROTOCOL_VENDOR			0x00
#define DC_PROTOCOL_LAUTERBACH			0x01
#define DC_PROTOCOL_ITP			0x02

/* Debug Class-Specific Interface Descriptor Subtypes */
#define DC_UNDEFINED			0x00
#define DC_INPUT_CONNECTION		0x01
#define DC_OUTPUT_CONNECTION		0x02
#define DC_DEBUG_UNIT			0x03
#define DC_DEBUG_ATTRIBUTES		0x04 /* revision: per SAS */

/* Debug-Class Input/Output Connection Type */
#define DC_CONNECTION_USB			0x00
#define DC_CONNECTION_JTAG			0x01
#define DC_CONNECTION_DEBUG_DATA_CONTROL	0x02
#define DC_CONNECTION_DEBUG_DATA		0x03
#define DC_CONNECTION_DEBUG_CONTROL		0x04

/*
 * Debug-class (rev 0.88r2) section 4.4.3
 * Attibute  Descriptor, bmControl
 */
#define DC_CTL_SET_CFG_DATA_SG			(1 << 0)
#define DC_CTL_SET_CFG_DATA			(1 << 1)
#define DC_CTL_GET_CFG_DATA			(1 << 2)
#define DC_CTL_SET_CFG_ADDR			(1 << 3)
#define DC_CTL_GET_CFG_ADDR			(1 << 4)
#define DC_CTL_SET_ALT_STACK			(1 << 5)
#define DC_CTL_GET_ALT_STACK			(1 << 6)
#define DC_CTL_SET_OP_MODE			(1 << 7)
#define DC_CTL_GET_OP_MODE			(1 << 8)
#define DC_CTL_SET_TRACE_CFG			(1 << 9)
#define DC_CTL_GET_TRACE_CFG			(1 << 10)
#define DC_CTL_SET_BUFF_INFO			(1 << 11)
#define DC_CTL_GET_BUFF_INFO			(1 << 12)
#define DC_CTL_SET_RESET			(1 << 13)

/* Debug-class (rev 0.88r2) section 4.4.6
 * Unit/Input/Output connection Descriptors,
 * dTraceFormat
 */
#define DC_TRACE_NOT_FORMATED_PASSTHROUGH   0x00000000
#define DC_TRACE_NOT_FORMATED_HEADER        0x00000001
#define DC_TRACE_NOT_FORMATED_FOOTER        0x00000002
#define DC_TRACE_NOT_FORMATED_GUID          0x00000005
#define DC_TRACE_NOT_FORMATED_UTF8          0x00000006
#define DC_TRACE_INTEL_FORMATED_VENDOR      0x01000000
#define DC_TRACE_MIPI_FORMATED_STPV1        0x80000000
#define DC_TRACE_MIPI_FORMATED_STPV2        0x80000001
#define DC_TRACE_MIPI_FORMATED_TWP          0x80000100
#define DC_TRACE_MIPI_FORMATED_OST          0x80001000
#define DC_TRACE_NEXUS_FORMATED             0x81000000

/* Debug-class (rev 0.88r2) section 4.4.6
 * Unit connection Descriptors, dDebugUnitType
 */
#define DC_UNIT_TYPE_DFX		0x00
#define DC_UNIT_TYPE_SELECT		0x01
#define DC_UNIT_TYPE_TRACE_ROUTE	0x02
#define DC_UNIT_TYPE_TRACE_PROC	0x03
#define DC_UNIT_TYPE_TRACE_GEN		0x04
#define DC_UNIT_TYPE_TRACE_SINK	0x05
#define DC_UNIT_TYPE_CONTROL		0x06
#define DC_UNIT_TYPE_VENDOR		0x40

/* Debug-class (rev 0.88r2) section 4.4.6
 * Unit connection Descriptors, dDebugUnitSubType
 */
#define DC_UNIT_SUBTYPE_NO              0x00
#define DC_UNIT_SUBTYPE_CPU             0x01
#define DC_UNIT_SUBTYPE_GFX             0x02
#define DC_UNIT_SUBTYPE_VIDEO		0x03
#define DC_UNIT_SUBTYPE_IMAGING	0x04
#define DC_UNIT_SUBTYPE_AUDIO		0x05
#define DC_UNIT_SUBTYPE_MODEM		0x06
#define DC_UNIT_SUBTYPE_BLUETOOTH	0x07
#define DC_UNIT_SUBTYPE_PWR_MGT	0x08
#define DC_UNIT_SUBTYPE_SECURITY	0x09
#define DC_UNIT_SUBTYPE_SENSOR		0x0A
#define DC_UNIT_SUBTYPE_BUSWATCH	0x0B
#define DC_UNIT_SUBTYPE_GPS		0x0C
#define DC_UNIT_SUBTYPE_TRACEZIP	0x0D
#define DC_UNIT_SUBTYPE_TAPCTL		0x0E
#define DC_UNIT_SUBTYPE_MEMACC		0x0F
#define DC_UNIT_SUBTYPE_SWLOGGER	0x40
#define DC_UNIT_SUBTYPE_SWROUTER	0x41
#define DC_UNIT_SUBTYPE_SWDRIVER	0x42
#define DC_UNIT_SUBTYPE_VENDOR		0x80

/* USB DBG requests values */
#define DC_REQUEST_SET_CONFIG_DATA		0x01
#define DC_REQUEST_SET_CONFIG_DATA_SINGLE	0x02
#define DC_REQUEST_SET_CONFIG_ADDRESS		0x03
#define DC_REQUEST_SET_ALT_STACK		0x04
#define DC_REQUEST_SET_OPERATING		0x05
#define DC_REQUEST_SET_TRACE			0x08
#define DC_REQUEST_SET_BUFFER_INFO		0x09
#define DC_REQUEST_SET_RESET			0x0A
#define DC_REQUEST_GET_CONFIG_DATA		0x81
#define DC_REQUEST_GET_CONFIG_DATA_SINGLE	0x82
#define DC_REQUEST_GET_CONFIG_ADDRESS		0x83
#define DC_REQUEST_GET_ALT_STACK		0x84
#define DC_REQUEST_GET_OPERATING		0x85
#define DC_REQUEST_GET_TRACE			0x86
#define DC_REQUEST_GET_INFO			0x87
#define DC_REQUEST_GET_ERROR			0x88
#define DC_REQUEST_GET_BUFFER_INFO		0x89

/* Debug-Class Debug-Attributes Descriptor */
struct dc_debug_attributes_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__u8  bDescriptorSubtype;
	__le16 bcdDC;
	__le16 wTotalLength;
	__u8  bmSupportedFeatures;
	__u8  bControlSize;		/* n */
	__u8  bmControl[0];		/* [n] */
	__le16 wAuxDataSize;		/* m */
	__le32 dInputBufferSize;
	__le32 dOutputBufferSize;
	__le64 qBaseAddress;
	__le64 hGlobalID[2];
	__u8  Supplementary[0];		/* [m-32] */
} __attribute__((__packed__));

#define DC_DEBUG_ATTR_DESCR(name)		 \
	dc_debug_attributes_descriptor_##name

#define DECLARE_DC_DEBUG_ATTR_DESCR(name, n, m)	\
struct DC_DEBUG_ATTR_DESCR(name) {		\
	__u8  bLength;					\
	__u8  bDescriptorType;				\
	__u8  bDescriptorSubtype;			\
	__le16 bcdDC;					\
	__le16 wTotalLength;				\
	__u8  bmSupportedFeatures;			\
	__u8  bControlSize;				\
	__u8  bmControl[n];				\
	__le16 wAuxDataSize;				\
	__le32 dInputBufferSize;			\
	__le32 dOutputBufferSize;			\
	__le64 qBaseAddress;				\
	__le64 hGlobalID[2];				\
	__u8  Supplementary[m-32];			\
} __attribute__((__packed__));

#define DC_DBG_ATTRI_SIZE(n, m)		(9 + (n) + 2 + (m))

/* Debug-Class Input Connection Descriptor */
struct dc_input_connection_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__u8  bDescriptorSubtype;
	__u8  bConnectionID;
	__u8  bConnectionType;
	__u8  bAssocConnection;
	__u8  iConnection;
	__le32 dTraceFormat;
	__le32 dStreamID;
} __attribute__((__packed__));

#define DC_INPUT_CONNECTION_SIZE	15

/* Debug-Class Output Connection Descriptor */
struct dc_output_connection_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__u8  bDescriptorSubtype;
	__u8  bConnectionID;
	__u8  bConnectionType;
	__u8  bAssocConnection;
	__le16 wSourceID;
	__u8  iConnection;
} __attribute__((__packed__));

#define DC_OUTPUT_CONNECTION_SIZE	9

/* Debug-Class Debug-Unit Descriptor */
struct dc_debug_unit_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__u8  bDescriptorSubtype;
	__u8  bUnitID;
	__u8  bDebugUnitType;
	__u8  bDebugSubUnitType;
	__u8  bAliasUnitID;
	__u8  bNrInPins;		/* p */
	__le16 wSourceID[0];		/* [p] */
	__u8  bNrOutPins;		/* q */
	__le32 dTraceFormat[0];		/* [q] */
	__le32 dStreamID;
	__u8  bControlSize;		/* n */
	__u8  bmControl[0];		/* [n] */
	__le16 wAuxDataSize;		/* m */
	__le64 qBaseAddress;
	__le64 hIPID[2];
	__u8  Supplementary[0];		/* [m-24] */
	__u8  iDebugUnitType;
} __attribute__((__packed__));

#define DC_DEBUG_UNIT_DESCRIPTOR(p, q, n, m)		\
	dc_debug_unit_descriptor_##p_##q##n_##m

#define DECLARE_DC_DEBUG_UNIT_DESCRIPTOR(p, q, n, m)	\
struct DC_DEBUG_UNIT_DESCRIPTOR(p, q, n, m) {		\
	__u8  bLength;					\
	__u8  bDescriptorType;				\
	__u8  bDescriptorSubtype;			\
	__u8  bUnitID;					\
	__u8  bDebugUnitType;				\
	__u8  bDebugSubUnitType;			\
	__u8  bAliasUnitID;				\
	__u8  bNrInPins;				\
	__le16 wSourceID[p];				\
	__u8  bNrOutPins;				\
	__le32 dTraceFormat[q];			\
	__le32 dStreamID;				\
	__u8  bControlSize;				\
	__u8  bmControl[n];				\
	__le16 wAuxDataSize;				\
	__le64 qBaseAddress;				\
	__le64 hIPID[2];				\
	__u8  Supplementary[m-24];			\
	__u8  iDebugUnitType;				\
} __attribute__((__packed__));

#define DC_DBG_UNIT_SIZE(p, q, n, m)	\
(8 + (p * 2) + 1 + (q * 4) + 5 + (n) + 2 + (m) + 1)

#endif /* __LINUX_USB_DEBUG_H */
