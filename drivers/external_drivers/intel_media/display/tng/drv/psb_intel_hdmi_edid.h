/*
 * Copyright © 2006-2007 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *	
 */
/* chunfeng.zhao@intel.com
  */
#ifndef PSB_INTEL_HDMI_EDID_H
#define PSB_INTEL_HDMI_EDID_H

//#include "..\\..\\Common\\Platform.h"

////////////////////////////////////////////
//
// Max number of EDID extensions possible
//
////////////////////////////////////////////
#define MAX_EDID_EXTENSIONS				254	//Max EDID blocks minus Block 0
#define NUM_BASEEDID_STANDARD_TIMING	8
#define MAX_BASEEDID_DTD_BLOCKS			4

#define MAX_VIC_DEFINED					128

// New Macros for supporting EDID 1.4

// Macros for EDID Revision and Version
#define EDID_VERSION_1 0x01
#define EDID_REVISION_4 0x04

// Macros for CVT and GTF related support in Monitor descriptor
#define EDID14_CVT_TIMING_SUPPORTED 0x04
#define EDID14_DEFAULT_GTF_SUPPORTED 0x00
#define EDID14_SECONDARY_GTF_SUPPORTED 0x02

// Macros for display device data block in CEA.
#define EDID14_DISPLAY_DEVICE_DATA_TAG 0xFF
#define EDID14_DISPLAY_DEVICE_DATA_CHILD_TAG 0x02
#define EDID14_DISPLAY_DEVICE_DATA_LENGTH 0x20
#define EDID14_DISPLAY_PORT_INTERFACE 0x09

// Macros indicating digital interfaces supported by the display. 
#define EDID14_DVI_SUPPORTED 0x01
#define EDID14_DISPLAY_PORT_SUPPORTED 0x05
#define EDID14_HDMI_A_SUPPORTED 0x02
#define EDID14_HDMI_B_SUPPORTED 0x03

#define EDID14_MAX_MONITOR_DESCRIPTORS 0x03

// Macros related to EDID 1.4 Color Bit Depth support
#define EDID14_COLOR_BIT_DEPTH_UNDEFINED         0x00
#define EDID14_SIX_BITS_PER_PRIMARY_COLOR        0x06
#define EDID14_EIGHT_BITS_PER_PRIMARY_COLOR      0x08
#define EDID14_TEN_BITS_PER_PRIMARY_COLOR        0x0A
#define EDID14_TWELVE_BITS_PER_PRIMARY_COLOR     0x0C
#define EDID14_FOURTEEN_BITS_PER_PRIMARY_COLOR   0x0E
#define EDID14_SIXTEEN_BITS_PER_PRIMARY_COLOR    0x10
#define EDID14_INVALID_COLOR_BIT_DEPTH           0x07

// Macro for showing Color Bit Depth support for existing displays
#define EDID_EIGHT_BITS_PER_PRIMARY_COLOR        0x08

// Macro for Established Timings III Block descriptor 
#define EST_TIMINGS_III_BLOCK_TAG                0xF7
#define EST_TIMINGS_III_BLOCK_DATA_LENGTH        0x06

// Macro for indicating byte length
#define BYTE_LENGTH                              0x08

////////////////////////////////////////////
//
// Max number of EDID Blocks
//
////////////////////////////////////////////
#define MAX_EDID_BLOCKS					255	//According to E-EDID Standard doc.
#define EDID_BLOCK_SIZE					128

// Macros for EDID Revision and Version for EDID 1.3
#define EDID_VERSION_1_3 0x01
#define EDID_REVISION_1_3 0x03

////////////////////////////////////////////
// Base EDID header
////////////////////////////////////////////
static const unsigned char BASEEDID_Header[8] =
    { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };

// Display Range Limits Offset Flags.
// Applicable only from EDID 1.4 onwards
typedef union _edid_range_limits_flags {
	uint8_t ucRangeLimitOffsetFlags;	// Range Limits Offset Flags
	struct {
		uint8_t ucVerticalRateOffset:2;	// Vertical Rate Offset
		uint8_t ucHorizontalRateOffset:2;	// Horizontal Rate Offset
		uint8_t ucReserved:4;	// Reserved.
	};
} edid_range_limits_flags_t;

////////////////////////////////////////////
//
//      18-byte DTD block
//  Refer Table 3.16, 3.17 & 3.18 of 
//  EDID spec
//
////////////////////////////////////////////
typedef struct _edid_dtd_timing {
#pragma pack(1)

	int16_t wPixelClock;	// Pixel clock / 10000

	uint8_t ucHA_low;	// Lower 8 bits of H. active pixels
	uint8_t ucHBL_low;	// Lower 8 bits of H. blanking
	union {
		uint8_t ucHAHBL_high;
		struct {
			uint8_t ucHBL_high:4;	// Upper 4 bits of H. blanking
			uint8_t ucHA_high:4;	// Upper 4 bits of H. active pixels
		};
	};

	uint8_t ucVA_low;	// Lower 8 bits of V. active lines
	uint8_t ucVBL_low;	// Lower 8 bits of V. blanking
	union {
		uint8_t ucVAVBL_high;
		struct {
			uint8_t ucVBL_high:4;	// Upper 4 bits of V. blanking
			uint8_t ucVA_high:4;	// Upper 4 bits of V. active pixels
		};
	};

	uint8_t ucHSO_low;	// Lower 8 bits of H. sync offset
	uint8_t ucHSPW_low;	// Lower 8 bits of H. sync pulse width
	union {
		uint8_t ucVSOVSPW_low;
		struct {
			uint8_t ucVSPW_low:4;	// Lower 4 bits of V. sync pulse width
			uint8_t ucVSO_low:4;	// Lower 4 bits of V. sync offset
		};
	};
	union {
		uint8_t ucHSVS_high;
		struct {
			uint8_t ucVSPW_high:2;	// Upper 2 bits of V. sync pulse width
			uint8_t ucVSO_high:2;	// Upper 2 bits of V. sync offset
			uint8_t ucHSPW_high:2;	// Upper 2 bits of H. sync pulse width
			uint8_t ucHSO_high:2;	// Upper 2 bits of H. sync offset
		};
	};

	uint8_t ucHIS_low;	// Lower 8 bits of H. image size in mm
	uint8_t ucVIS_low;	// Lower 8 bits of V. image size in mm
	union {
		uint8_t ucHISVIS_high;
		struct {
			uint8_t ucVIS_high:4;	// Upper 4 bits of V. image size
			uint8_t ucHIS_high:4;	// Upper 4 bits of H. image size
		};
	};

	uint8_t ucHBorder;	// H. border in pixels
	uint8_t ucVBorder;	// V. border in pixels

	union {
		uint8_t ucFlags;	// Hsync & Vsync polarity, etc. flags
		struct {
			uint8_t ucStereo1:1;	// Stereo definition with bit[6:5]
			uint8_t ucHSync_Pol:1;	// Hsync polarity (0: Neg, 1: Pos)
			uint8_t ucVSync_Pol:1;	// Vsync polarity (0: Neg, 1: Pos)
			uint8_t ucSync_Conf:2;	// Sync configuration
			// 00 : Analog composite
			// 01 : Bipolar analog composite
			// 00 : Digital composite
			// 00 : Digital separate
			uint8_t ucStereo2:2;	// Stereo definition
			// 00 : Normal display, no stereo 
			// xx : Stereo definition with bit0
			uint8_t ucInterlaced:1;	// Interlaced / Non-interlaced
			// 0 : Non-interlaced
			// 1 : Interlaced
		};
	};

#pragma pack()
} edid_dtd_timing_t;

////////////////////////////////////////////
//
//      Standard timing identification
//  Refer Table 3.15 of EDID spec
//
////////////////////////////////////////////
typedef union _edid_std_timing {
	uint16_t usStdTiming;

	struct {
#pragma pack(1)
		uint8_t ucHActive;	// (HActive/8) - 31;
		struct {
			uint8_t ucRefreshRate:6;	// Refresh Rate - 60
			uint8_t ucAspectRatio:2;	// Aspect ratio (HActive/VActive)
			// 00:  1:1 Aspect ratio
			// 01:  4:3 Aspect ratio
			// 10:  5:4 Aspect ratio
			// 11: 16:9 Aspect ratio
		};
	};
#pragma pack()

} edid_std_timing_t;
////////////////////////////////////////////////////////
// Aspect Ratio def's as per Edid 1.3 Standard Timings
////////////////////////////////////////////////////////
#define EDID_STD_ASPECT_RATIO_16_10   0x0
#define EDID_STD_ASPECT_RATIO_4_3     0x1
#define EDID_STD_ASPECT_RATIO_5_4     0x2
#define EDID_STD_ASPECT_RATIO_16_9    0x3

////////////////////////////////////////////
//
//      Monitor range limits
//
////////////////////////////////////////////
typedef struct _monitor_range_limits {
#pragma pack(1)

	uint8_t ucMin_vert_rate;	//Min Vertical Rate,in Hz
	uint8_t ucMax_vert_rate;	//Max Vertical Rate, in Hz
	uint8_t ucMin_horz_rate;	//Min Horizontal Rate, in Hz
	uint8_t ucMax_horz_rate;	//Max Horizontal Rate, in Hz
	uint8_t ucMax_pixel_clock;	//Max Pixel Clock,Value/10 Mhz
	uint8_t ucTiming_formula_support;	//00 - No Secondary Timing Formula Supported
	//02 - Secondary GTF Curve Supported
	//In EDID 1.4, this may indicate CVT support as well
	//If timing_formula_support is 02
	uint8_t ucReserved;	//00h
	uint8_t ucStart_freq;	//Horizontal Freq, Value/2, KHz
	uint8_t ucByte_C;	//C*2
	uint8_t ucLSB_M;	//LSB of M Value
	uint8_t ucMSB_M;	//MSB of M Value
	uint8_t ucByte_K;	//K Value
	uint8_t ucByte_J;	//J*2

#pragma pack()
} monitor_range_limits_t;

////////////////////////////////////////////
//
// Color point
//
////////////////////////////////////////////
typedef struct _color_point {
#pragma pack(1)

	uint8_t ucWhite_point_index_number_1;
	uint8_t ucWhite_low_bits_1;
	uint8_t ucWhite_x_1;
	uint8_t ucWhite_y_1;
	uint8_t ucWhite_gamma_1;
	uint8_t ucWhite_point_index_number_2;
	uint8_t ucWhite_low_bits_2;
	uint8_t ucWhite_x_2;
	uint8_t ucWhite_y_2;
	uint8_t ucWhite_gamma_2;
	uint8_t ucByte_15;
	uint8_t ucByte_16_17[2];

#pragma pack()
} color_point_t;

////////////////////////////////////////////
//
//      Monitor description descriptor
//  Refer Table 3.19 & 3.20 of EDID spec
//
////////////////////////////////////////////
#define BASEEDID_MONITORSN_MDDATATYPE			0xFF
#define BASEEDID_ASCIISTRING_MDDATATYPE			0xFE
#define BASEEDID_MONITORRANGELIMIT_MDDATATYPE	0xFD
#define BASEEDID_MONITORNAME_MDDATATYPE			0xFC
#define BASEEDID_COLORPOINT_MDDATATYPE			0xFB
#define BASEEDID_STDTIMINGS_MDDATATYPE			0xFA

// Structure definition for Established Timings III monitor block
typedef struct _est_timings_iii_block {
#pragma pack(1)
	// The first byte will show the VESA DMTS Standard Version. 
	// The following six bytes will have the Timings Bit Mask.
	// Right now only 6 bytes are used for this!!!
	// Rest is reserved.
	uint8_t ucVesaDMTVersion;	//Byte 0 indicating the VESA DMT Version.
	uint8_t ucTimingBitMask[6];	// Next 6 bytes indicating the Timing Bit Mask Bytes used in Est Timing III.
	uint8_t bReserved[6];	//Next 6 bytes are reserved
#pragma pack()
} est_timings_iii_block_t;

typedef struct _monitor_descriptor {
#pragma pack(1)

	int16_t wFlag;		// = 0000 when block is used as descriptor
	uint8_t ucFlag0;	// Reserved

	uint8_t ucDataTypeTag;

	uint8_t ucFlag1;	// 00 for descriptor

	union {

		// Monitor S/N (ucDataTypeTag = FF)
		uint8_t ucMonitorSerialNumber[13];

		// ASCII string (ucDataTypeTag = FE)
		uint8_t ucASCIIString[13];

		// Monitor range limit (ucDataTypeTag = FD)
		monitor_range_limits_t MonitorRangeLimits;

		// Monitor name (ucDataTypeTag = FC)
		uint8_t ucMonitorName[13];

		// Color point (ucDataTypeTag = FB)
		color_point_t ColorPoint;

		// ESTABLISHED TIMINGS III BLOCK = F7 (Added for EDID 1.4)
		est_timings_iii_block_t stEstTimingsIIIBlock;

		// Standard timings (ucDataTypeTag = FA)
		struct {
			edid_std_timing_t ExtraStdTiming[6];
			uint8_t ucFixedValueOfA0;	// Should be 0xA0
		};

		// Manufacturer specific value (ucDataTypeTag = 0F-00)
		uint8_t ucMfgSpecificData[13];
	};

#pragma pack()
} monitor_descriptor_t;

////////////////////////////////////////////
//
//      EDID PnP ID fields
//
////////////////////////////////////////////
typedef union _baseedid_pnpid {
	uint8_t VendorProductID[10];	// Vendor / Product identification

	struct {
		uint8_t ManufacturerID[2];	// Bytes 8, 9: Manufacturer ID 
		uint8_t ProductID[2];	// Bytes 10, 11: Product ID
		uint8_t SerialNumber[4];	// Bytes 12-15: Serial numbers
		uint8_t WeekOfManufacture;	// Byte 16: Week of manufacture
		uint8_t YearOfManufacture;	// Byte 17: Year of manufacture
	};
} baseedid_pnpid_t;

//
// Chromaticity structure
// Table 3.12 of Base Block for details
//
typedef struct _baseedid_chromaticity_block {
	union {
		uint8_t RedGreenLowBits;	// Byte 1
		struct {
			uint8_t ucGreenYLowBits:2;	// bit 1:0
			uint8_t ucGreenXLowBits:2;	// bit 3:2
			uint8_t ucRedYLowBits:2;	// bit 5:4
			uint8_t ucRedXLowBits:2;	// bit 7:6
		};
	};

	union {
		uint8_t ucBlueWhiteLowBits;	// Byte 2
		struct {
			uint8_t ucWhiteYLowBits:2;	// bit 1:0
			uint8_t ucWhiteXLowBits:2;	// bit 3:2
			uint8_t ucBlueYLowBits:2;	// bit 5:4
			uint8_t ucBlueXLowBits:2;	// bit 7:6
		};
	};

	uint8_t ucRedXUpperBits;	// bit 9:2          Byte 3
	uint8_t ucRedYUpperBits;	// bit 9:2          Byte 4

	uint8_t ucGreenXUpperBits;	// bit 9:2        Byte 5
	uint8_t ucGreenYUpperBits;	// bit 9:2        Byte 6

	uint8_t ucBlueXUpperBits;	// bit 9:2         Byte 7
	uint8_t ucBlueYUpperBits;	// bit 9:2         Byte 8

	uint8_t ucWhiteXUpperBits;	// bit 9:2        Byte 9
	uint8_t ucWhiteYUpperBits;	// bit 9:2        Byte 10
} baseedid_chromaticity_block_t;

////////////////////////////////////////////
//
//      128-byte EDID 1.x block0 structure
//
////////////////////////////////////////////
typedef struct _baseedid_1_x {
#pragma pack(1)

	//
	// Header: 8 bytes (Table 3.3 of EDID spec)
	char Header[8];		// EDID1.x header "0 FFh FFh FFh FFh FFh FFh 0"

	//
	// Vendor/Product ID: 10 bytes (Table 3.4, 3.5 & 3.6 of EDID spec)
	//baseedid_pnpid_t;
	union {
		uint8_t VendorProductID[10];	// Vendor / Product identification
		struct {
			uint8_t ManufacturerID[2];	// Bytes 8, 9: Manufacturer ID 
			uint8_t ProductID[2];	// Bytes 10, 11: Product ID
			uint8_t SerialNumber[4];	// Bytes 12-15: Serial numbers
			uint8_t WeekOfManufacture;	// Byte 16: Week of manufacture
			uint8_t YearOfManufacture;	// Byte 17: Year of manufacture
		};
	};

	//
	// EDID structure Version/Revision: 2 bytes (Table 3.7 of EDID spec)
	uint8_t ucVersion;	// EDID version no.
	uint8_t ucRevision;	// EDID revision no.

	//
	// Basic display parameters & features: 5 bytes (Table 3.8 of EDID spec)
	union {
		uint8_t ucVideoInput;	// Video input definition (Refer Table 3.9 of EDID spec)

		struct {
			uint8_t ucSyncInput:4;	// Sync input supported (iff ucDigitInput = 0)
			uint8_t ucSetup:1;	// Display setup (iff ucDigitInput = 0)
			uint8_t ucSigLevStd:2;	// Signal level Standard (iff ucDigitInput = 0)

			uint8_t ucDigitInput:1;	// 1: Digital input; 0: Analog input
		};
	};

	// Image size (Table 3.10 of EDID spec)
	uint8_t ucMaxHIS;	// Maximum H. image size in cm
	uint8_t ucMaxVIS;	// Maximum V. image size in cm

	// Gamma (display transfer characteristic)
	uint8_t ucGamma;	// Display gamma value  [= (gamma*100)-100]

	// Feature support (Table 3.11 of EDID spec)
	union {
		uint8_t ucDMPSFeature;	// DPMS feature support

		struct {
			uint8_t ucGTFSupport:1;	// GTF timing support (1: Yes)
			uint8_t ucPTM:1;	// Preferred timing is 1st DTD (1: Yes) [Must if EDID >= 1.3]
			uint8_t ucColorSpace:1;	// Use STD color space (1:Yes) [If set ColorChars should match sRGB values in EDID spec Appendix A]
			uint8_t ucDispType:2;	// Display type
			// 00: Monochrome
			// 01: R/G/B color display
			// 10: Non R/G/B multicolor display
			// 11: Undefined
			uint8_t ucActiveOff:1;	// Active off (Display consumes less power/blanks out when it receives an out of range timing)
			uint8_t ucSuspend:1;	// Suspend      (Refer VESA DPMS spec)
			uint8_t ucStandBy:1;	// Stand-by     (Refer VESA DPMS spec)
		};
	};

	//
	// Phosphor or Filter Chromaticity: 10 bytes
	uint8_t ColorChars[10];	// Color characteristics        (Refer Table 3.12 of EDID spec)

	//
	// Established timings: 3 bytes (Table 3.14 of EDID spec)
	union {
		uint8_t EstTiming1;
		struct {
			uint8_t bSupports800x600_60:1;
			uint8_t bSupports800x600_56:1;
			uint8_t bSupports640x480_75:1;
			uint8_t bSupports640x480_72:1;
			uint8_t bSupports640x480_67:1;
			uint8_t bSupports640x480_60:1;
			uint8_t bSupports720x400_88:1;
			uint8_t bSupports720x400_70:1;
		};
	};
	union {
		uint8_t EstTiming2;
		struct {
			uint8_t bSupports1280x1024_75:1;
			uint8_t bSupports1024x768_75:1;
			uint8_t bSupports1024x768_70:1;
			uint8_t bSupports1024x768_60:1;
			uint8_t bSupports1024x768_87i:1;
			uint8_t bSupports832x624_75:1;
			uint8_t bSupports800x600_75:1;
			uint8_t bSupports800x600_72:1;
		};
	};
	union {
		uint8_t MfgTimings;
		struct {
			uint8_t bMfgReservedTimings:7;
			uint8_t bSupports1152x870_75:1;
		};
	};

	//
	// Standard timings: 8 bytes (Table 3.15 of EDID spec)
	edid_std_timing_t StdTiming[NUM_BASEEDID_STANDARD_TIMING];	// 8 Standard timing support

	//
	// Detailed timing section - 72 bytes (4*18 bytes)
	union {
		edid_dtd_timing_t DTD[MAX_BASEEDID_DTD_BLOCKS];	// Four DTD data blocks

		monitor_descriptor_t MonitorInfo[MAX_BASEEDID_DTD_BLOCKS];
	};

	uint8_t ucNumExtBlocks;	// Number of extension EDID blocks
	uint8_t ucChecksum;	// Checksum of the EDID block

#pragma pack()
} baseedid_1_x_t;

////////////////////////////////////////////
//
//      128-byte EDID 1.4 block0 structure
//  EDID 1.4 block0 structure is different from 1.3 block0
//  Thats why this new structure has been added 
//  Changes are commented in the structure itself
//
////////////////////////////////////////////
typedef struct _baseedid_1_4 {
#pragma pack(1)

	//
	// Header: 8 bytes (Table 3.3 of EDID spec)
	char Header[8];		// EDID1.x header "0 FFh FFh FFh FFh FFh FFh 0"

	//
	// Vendor/Product ID: 10 bytes (Table 3.4, 3.5 & 3.6 of EDID spec)
	union {
		uint8_t VendorProductID[10];	// Vendor / Product identification
		struct {
			uint8_t ManufacturerID[2];	// Bytes 8, 9: Manufacturer ID 
			uint8_t ProductID[2];	// Bytes 10, 11: Product ID
			uint8_t SerialNumber[4];	// Bytes 12-15: Serial numbers
			uint8_t WeekOfManufacture;	// Byte 16: Week of manufacture
			uint8_t YearOfManufacture;	// Byte 17: Year of manufacture
		};
	};

	//
	// EDID structure Version/Revision: 2 bytes (Table 3.7 of EDID spec)
	uint8_t ucVersion;	// EDID version no.
	uint8_t ucRevision;	// EDID revision no.

	//
	// Basic display parameters & features: 5 bytes (Table 3.8 of EDID spec)
	union {
		uint8_t ucVideoInput;	// Video input definition (Refer Table 3.9 of EDID spec)

		struct {
			uint8_t ucSyncInput:4;	// Sync input supported (iff ucDigitInput = 0)
			uint8_t ucSetup:1;	// Display setup (iff ucDigitInput = 0)
			uint8_t ucSigLevStd:2;	// Signal level Standard (iff ucDigitInput = 0)

			uint8_t ucDigitInput:1;	// 1: Digital input; 0: Analog input
		};
		// This structure has been introduced to reflect the changes in EDID 1.4 spec
		// This sturcture shows new meaning of VIDEO INPUT DEFINITION when input is digital 
		struct {
			uint8_t ucDigitalVideoInterface:4;	// Digital Video Interface Standard Supported.
			uint8_t ucColorBitDepth:3;	// Color Bit Depth. 
			// 0 0 0 -- Color Bit Depth is undefined
			// 0 0 1 -- 6 Bits per Primary Color
			// 0 1 0 -- 8 Bits per Primary Color
			// 0 1 1 -- 10 Bits per Primary Color
			// 1 0 0 -- 12 Bits per Primary Color
			// 1 0 1 -- 14 Bits per Primary Color
			// 1 1 0 -- 16 Bits per Primary Color
			// 1 1 1 -- Reserved (Do Not Use)
			uint8_t bIsDigitalVideoSignalInterface:1;	// Bit 7
		};
	};

	// As per the EDID spec 1.4, the following two fields can be aspect ratios as well. 
	union {
		uint8_t ucMaxHIS;	// Maximum H. image size in cm 
		uint8_t ucARLandscape;	// Landscape Aspect raio as per EDID 1.4 spec
	};
	union {
		uint8_t ucMaxVIS;	// Maximum V. image size in cm 
		uint8_t ucARPortrait;	// Portrait Aspect raio as per EDID 1.4 spec
	};

	// Gamma (display transfer characteristic)
	uint8_t ucGamma;	// Display gamma value  [= (gamma*100)-100]

	// Feature support (Table 3.11 of EDID spec)
	union {
		uint8_t ucDMPSFeature;	// DPMS feature support

		struct {
			uint8_t ucContinuousDisplay:1;	// Display is continuous or non-continuous (1: Yes)
			uint8_t ucPTM:1;	// Preferred timing mode indicates native pixel format and native RR. 
			uint8_t ucColorSpace:1;	// Use STD color space (1:Yes) [If set ColorChars should match sRGB values in EDID spec Appendix A]
			uint8_t ucDispType:2;	// Display type
			// 00: Monochrome
			// 01: R/G/B color display
			// 10: Non R/G/B multicolor display
			// 11: Undefined
			uint8_t ucActiveOff:1;	// Active off (Display consumes less power/blanks out when it receives an out of range timing)
			uint8_t ucSuspend:1;	// Suspend      (Refer VESA DPMS spec)
			uint8_t ucStandBy:1;	// Stand-by     (Refer VESA DPMS spec)
		};

		struct {
			uint8_t bReserved0:1;
			uint8_t bReserved1:1;
			uint8_t bReserved2:1;
			uint8_t ucColorEncodingFormat:2;	// Supported Color Encoding Format if Video Input is digital
			// 00: RGB 4:4:4
			// 01: RGB 4:4:4 & YCrCb 4:4:4
			// 10: RGB 4:4:4 & YCrCb 4:2:2
			// 11: RGB 4:4:4 & YCrCb 4:4:4 & YCrCb 4:2:2
			uint8_t bReserved3:1;
			uint8_t bReserved4:1;
			uint8_t bReserved5:1;
		};
	};

	//
	// Phosphor or Filter Chromaticity: 10 bytes
	uint8_t ColorChars[10];	// Color characteristics        (Refer Table 3.12 of EDID spec)

	//
	// Established timings: 3 bytes (Table 3.14 of EDID spec)
	union {
		uint8_t EstTiming1;
		struct {
			uint8_t bSupports800x600_60:1;
			uint8_t bSupports800x600_56:1;
			uint8_t bSupports640x480_75:1;
			uint8_t bSupports640x480_72:1;
			uint8_t bSupports640x480_67:1;
			uint8_t bSupports640x480_60:1;
			uint8_t bSupports720x400_88:1;
			uint8_t bSupports720x400_70:1;
		};
	};
	union {
		uint8_t EstTiming2;
		struct {
			uint8_t bSupports1280x1024_75:1;
			uint8_t bSupports1024x768_75:1;
			uint8_t bSupports1024x768_70:1;
			uint8_t bSupports1024x768_60:1;
			uint8_t bSupports1024x768_87i:1;
			uint8_t bSupports832x624_75:1;
			uint8_t bSupports800x600_75:1;
			uint8_t bSupports800x600_72:1;
		};
	};
	union {
		uint8_t MfgTimings;
		struct {
			uint8_t bMfgReservedTimings:7;
			uint8_t bSupports1152x870_75:1;
		};
	};

	//
	// Standard timings: 8 bytes (Table 3.15 of EDID spec)
	edid_std_timing_t StdTiming[NUM_BASEEDID_STANDARD_TIMING];	// 8 Standard timing support

	// Detailed timing section - 72 bytes (4*18 bytes)
	// As per the new spec 1.4, the first Detailed Timing Section should contain the PREFERED TIMING BLOCK
	edid_dtd_timing_t PreferedTimingMode;
	// The rest 54 bytes of the Detailed Timing Section. 
	union {
		edid_dtd_timing_t DTD[MAX_BASEEDID_DTD_BLOCKS - 1];	// Three DTD data blocks

		monitor_descriptor_t MonitorInfo[MAX_BASEEDID_DTD_BLOCKS - 1];	// Three Monitor Descriptor blocks
	};

	uint8_t ucNumExtBlocks;	// Number of extension EDID blocks
	uint8_t ucChecksum;	// Checksum of the EDID block

#pragma pack()
} baseedid_1_4_t;

//*****************************************************
//*****************************************************
//
// DATA STRUCTURES AND DEFINITIONS FOR CE-EXTENSION
//
//*****************************************************
//*****************************************************

/////////////////////////////////
//
//CE - Extension Block Structure
//
/////////////////////////////////
typedef struct _ce_edid {
	uint8_t ucTag;
	uint8_t ucRevision;
	uint8_t ucDTDOffset;
	uint8_t ucCapabilty;
	uint8_t data[123];
	uint8_t ucCheckSum;
} ce_edid_t;

////////////////////////////////////////////
//
//CE - Video Capability Data block structure
//
////////////////////////////////////////////
typedef union _video_cap_data_block {
	uint8_t ucValue;
	struct {
		uint8_t ucCEScanBehavior:2;	// Indicates scan behavior of CE mode
		uint8_t ucITScanBehavior:2;	// Indicates scan behavior of IT mode
		uint8_t ucPTScanBehavior:2;	// Indicates scan behavior of Preferred mode
		uint8_t ucQuantRangeSelectable:1;	// Indicates if RGB Quantization Range can be overridden
		uint8_t ucReserved:1;
	};
} video_cap_data_block_t;

////////////////////////////////////////////
//
//CEA Extn Block Byte3 structure
//
////////////////////////////////////////////
typedef union _cea_ext_capability {
	uint8_t ucValue;
	struct {
		uint8_t ucTotalNativeDTDs:4;	// Total number of DTDs in extension block
		uint8_t ucSupportsYCBCR422:1;	// Indicates support for YCBCR 4:2:2
		uint8_t ucSupportsYCBCR444:1;	// Indicates support for YCBCR 4:4:4
		uint8_t ucSupportsBasicAudio:1;	// Indicates support for Basic audio
		uint8_t ucUnderscansITFormats:1;	// Indicates underscan behavior of IT formats
	};
} cea_ext_capability_t;

////////////////////////////////////////////
//
//CE - Video Capability Data block structure
//
////////////////////////////////////////////
typedef enum {
	FORMAT_NOT_SUPPORTED = 0,	// Format is not supported
	ALWAYS_OVERSCANNED = 1,	// Format is always overscanned
	ALWAYS_UNDERSCANNED = 2,	// Format is always underscanned
	SUPPORTS_OVER_AND_UNDERSCAN = 3	// Sink supports both overscan and underscan
} cea_scan_behavior_t;

/////////////////////////////////
//
// #defines required for CE Etxn
//
/////////////////////////////////
#define CEA_EXT_TAG 0x02
#define CEA_EXT_SUPPORTED_VERSION 0x03
#define CEA_EXT_861_REVISION 0x01

#define CEA_USE_EXTENDED_TAG   0x7

#define CEA_AUDIO_DATABLOCK         0x1
#define CEA_VIDEO_DATABLOCK         0x2
#define CEA_VENDOR_DATABLOCK        0x3
#define CEA_SPEAKER_DATABLOCK       0x4
#define CEA_VIDEO_CAP_DATABLOCK     0x0

#define CEA_DATABLOCK_TAG_MASK                  0xE0
#define CEA_DATABLOCK_LENGTH_MASK               0x1F
#define CEA_SHORT_VIDEO_DESCRIPTOR_CODE_MASK    0x7F
#define CEA_NATIVE_FORMAT_BIT_MASK              0x80

#define CEA_HDMI_IEEE_REG_ID    0x00000C03
#define CEA_EDID_HEADER_SIZE    0x04

// Extended Data block type
// This bit definitions are as per CE 861-D spec
#define CEA_COLORIMETRY_DATABLOCK   0x5
#define CE_COLORIMETRY_MD0_MASK BIT0
#define CE_COLORIMETRY_MD1_MASK BIT1
#define CE_COLORIMETRY_MD2_MASK BIT3
#if 0				/* for future reference */
//==================================================================================
//==================================================================================
//      DATA Structure definitions for VTB parsing.....
//  Reference VESA Documents are VTB Extension(Release A) & CVT standard version 1.1
//===================================================================================
//      #defines for VTB-EXT
//===================================================================================

#define VTB_EXT_TAG	0x10
#define	VTB_EXT_SUPPORTED_VERSION 0x03

#define	VTB_MAX_DTD_TIMINGS			 6
#define	VTB_MAX_CVT_TIMINGS			40
#define	VTB_MAX_STANDARD_TIMINGS	61

#define VTB_DTD_OFFSET		5
#define VTB_DTD_SIZE		18
#define	VTB_CVT_SIZE		3
#define VTB_ST_SIZE			2

// This struct is for VTB Extension block.
typedef struct _VTB_EXT {
	uint8_t ucTag;
	uint8_t ucVersion;
	uint8_t ulNumDTD;
	uint8_t ulNumCVT;
	uint8_t ulNumST;
	uint8_t DATA[122];
	uint8_t ucChecksum;
} VTB_EXT, *PVTB_EXT;

// Following struct is for CVT descriptor (Version 1.1)
typedef struct _VTB_CVT_TIMING {
#pragma pack(1)

	uint8_t ucVA_low;	// Lower 8 bits of Vertical size. This Vsize = (vertical active lines/2)-1. 
	//      Range for VA lines is 2 to 8192. CVT supprts only an even no. of active lines per frame.

	union {
		uint8_t ucVA_high_AR;
		struct {

			uint8_t ucReserved00:2;	//Bits 1-0 are reserved and set to 00h
			uint8_t ucAspectRatio:2;	//      Aspect Ratio specifier bits.
			// 00:   4:3 Aspect ratio
			// 01:  16:9 Aspect ratio
			// 10:  16:10 Aspect ratio
			// 11: Undefined (Reserved)

			uint8_t ucVA_high:4;	//      Upper 4 bits of Vertical Size.
		};
	};

	union {
		uint8_t ucRefresh_Rate_Bits;
		struct {

			uint8_t ucRR_60Hz_RB:1;	// When set, indicates 60Hz support with Reduced Blanking.
			uint8_t ucRR_85Hz:1;	//                              ||         85Hz                         ||                                                                                              .
			uint8_t ucRR_75Hz:1;	//                              ||         75Hz                         ||                                                                                              .
			uint8_t ucRR_60Hz:1;	//                              ||         60Hz                         ||                                                                                              .
			uint8_t ucRR_50Hz:1;	// When set, indicates 50Hz Refrash Rate with CRT Blanking supports specified pixel format.
			uint8_t ucPreferredRefresh_Rate:2;	// Preferred Refresh Rate specifier bits.
			// 00:  50 Hz
			// 01:  60 Hz (this means either CRT blanking or Reduced Blanking whichever is supported. 
			//                              If both are supported, then RB is preferred.)
			// 10:  75 Hz
			// 11:  85 Hz   

			uint8_t ucReserved0:1;	// This bit is reserved and set to '0'.

		};
	};
#pragma	pack()
} VTB_CVT_TIMING, *PVTB_CVT_TIMING;

// This struct is for storing extracted Info from CVT descriptor....
// This is defined by author.....not based on CVT specs.
typedef struct _CVT_INFO {
	ULONG ulYRes;
	ULONG ulXRes;
	ULONG ulRRate[5];	//As max 5 Refresh Rates can be supported.
	BOOLEAN bRed_Blank_Req[5];
	BOOLEAN bPreferred_RR[5];	//To set flag for Preffered RR
	ULONG ulNumRates;	//Number of Refresh rates Supported. (Max. 5)
} CVT_INFO, *PCVT_INFO;
#endif
// This structure is for stroing the Display device Data retreived from CEA block
// This is defined as per the Display Device Data Block standard.
typedef struct _display_device_data {
#pragma pack (1)
	union {
		uint8_t ucTagAndLength;	// Data Block Tag and Block Length. should be 0xFF
		struct {
			uint8_t ucLength:5;
			uint8_t ucTag:3;
		};
	};
	uint8_t ucChildTag;	// Child tag required as per CEA spec  should be 0x02
	union {
		uint8_t ucInterfaceType;
		struct {
			uint8_t ucNumOfChannels:4;	// Number of channels supported
			uint8_t ucInterfaceCode:4;	// Interface code 
		};
	};
	union {
		uint8_t ucVerAndRel;
		struct {
			uint8_t ucRelease:4;	// Release 
			uint8_t ucVersion:4;	// Version.
		};
	};
	uint8_t ucContentProtectionSuppFlag;	// Flag indicating support for content protection. 
	union {
		uint16_t usClockFrequency;	// Clock Frequency
		struct {
			uint16_t usMinClockFrequency:6;	// First 6 bits indicates Min frequency
			uint16_t usMaxClockFrequency:10;	// Next 10 bits indicates Max frequency
		};
	};
	union {
		uint8_t ucNativePixelFormat[4];	// Pixel Format
		struct {
			uint8_t ucHorizontalPixelCntLower;	// Lower byte value of the Horizontal pixel count
			uint8_t ucHorizontalPixelCntUpper;	// Upper byte value of the Horizontal pixel count
			uint8_t ucVerticalPixelCntLower;	//  Lower byte value of the vertical pixel count
			uint8_t ucVerticalPixelCntUpper;	// Upper byte value of the vertical pixel count
		};
	};
	uint8_t ucAspectRatio;	// Byte indicating Aspect ratio. 
	union {
		uint8_t ucOrientationAndRotation;
		struct {
			uint8_t ucScanDirection:2;	// Scan direction.
			uint8_t ucZeroPixelLocation:2;	// Zero Pixel Location.
			uint8_t ucRotationCapability:2;	// Indicates rotation capability
			uint8_t ucDefaultOrientation:2;	// Default Orientation.
		};
	};
	uint8_t ucSubPixelInfo;	// Sub-Pixle Information.
	uint8_t ucHorizontalPitch;	// Horizontal Pitch
	uint8_t ucVerticalPitch;	// Vertical Pitch
	union {
		uint8_t ucMiscDisplayCapabilities;
		struct {
			uint8_t bReserved:3;
			uint8_t ucDeinterlacing:1;	// indicates deinterlacing support
			uint8_t ucOverdriverNotRecommended:1;
			uint8_t ucDirectDrive:1;	// indicates DirectDrive support
			uint8_t ucDithering:2;	// indicates Dithering support.
		};
	};
	union {
		uint8_t ucAudioFlags;	// Flags indicating Audio details
		struct {
			uint8_t bReserved1:4;
			uint8_t ucAudioInputOverride:1;	// Indicates Audio Input Override
			uint8_t ucSeparateAudioInputs:1;	// Indicates Separate Audio Inputs
			uint8_t ucAudioInputOnVideoInterface:1;	// Shows whether Audio input is through the video interface.
		};
	};
	union {
		uint8_t ucAudioDelayFlags;	// Audio Delay Flags
		struct {
			uint8_t ucAudioDelay:7;	// Absolute offset between the audio and video signals.
			uint8_t ucAudioSign:1;	// Indicates positive or negative delay.
		};
	};
	union {
		uint8_t ucFrameRateAndModeConversion;
		struct {
			uint8_t ucFrameRateRange:6;	//Device Frame rate Range
			uint8_t ucFrameRateConversion:2;	//00 � No dedicated rate conversion hardware is provided;
			//01 � The display provides a single frame buffer
			//10 � The display provides double-buffering
			//11- The display provides frame-rate conversion involving interframe interpolation
		};
	};
	uint8_t ucDeviceNativeRate;	// Device Native Frame rate
	union {
		uint8_t ucColorBitDepth;	// Color bit depth
		struct {
			uint8_t ucDisplayDeviceColBitDepth:4;	// Color bit depth of the display device
			uint8_t ucInterfaceColBitDepth:4;	// color bit depth supported by the interface.h
		};
	};
	uint8_t ucAddPrimaryChromaticities[8];	// Additional Primary Chromaticities.
	union {
		uint8_t ucResponseTimeFlags;
		struct {
			uint8_t ucResponseTime:7;	// Time for transition.
			uint8_t ucBlackToWhite:1;	// if 1, then transition from black to white
			// if 0, then transition from white to black
		};
	};
	union {
		uint8_t ucOverscanInformation;
		struct {
			uint8_t ucVerticalPercentage:4;	// Percentage of Overscan in vertical direction.
			uint8_t ucHorizontalPercentage:4;	// Percentage of Overscan in horizontal direction.
		};
	};
#pragma pack()
} display_device_data_t;

//=========================================================================
//=========================================================================
// #defines for Block Map Ext.
//=========================================================================
//=========================================================================
#define BLOCK_MAP_EXT_TAG 0xF0

#endif				// EDIDSTRUCTS_H
