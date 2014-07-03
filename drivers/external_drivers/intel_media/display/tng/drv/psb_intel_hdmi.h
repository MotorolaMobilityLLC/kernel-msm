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
 *	Chunfeng Zhao <chunfeng.zhao@intel.com>
 *	Jim Liu <jim.liu@intel.com>
 */

#ifndef __PSB_INTEL_HDMI_H__
#define __PSB_INTEL_HDMI_H__

/*
 * HDMI Parameters
 */

#define HDMI_DEVICE_NAME "ABC_VEND"
#define HDMI_DEVICE_DESC "XZ05 PC VIDEO"

#define HDMI_MAX_PIXEL_REPETITION 0x04	// On Cantiga only upto 4X pixel repetition is supported
#define HDMI_HBR_AUDIO_SAMPLE_RATE 192000	// 192kHz is the sample rate corresponding to the HBR audio formats
#define HDMI_AUDIO_CLOCK_PACKET_RATE 1500	// Audio clock packet rate of 1.5kHz has to be considered while calculating audio BW

#define HDMI_BAR_INFO_LENGTH 8	// 8 bytes of barinfo

#define HDMI_MONITOR_NAME_LENGTH 20

// BaseLineDataLength.
// Total size is in multiple of 4 bytes. i.e, 80/4 = 20
#define HDMI_EELD_BASELINE_DATA_LENGTH 0x14

// Header = 4, Baseline Data = 80 and Vendor (INTEL) specific = 2 as per EELD spec
// 4 + 80 + = 84
#define HDMI_EELD_SIZE 84

struct mid_intel_hdmi_priv;
extern void mdfld_hdcp_init(struct mid_intel_hdmi_priv *p_hdmi_priv);
extern void mdfld_msic_init(struct mid_intel_hdmi_priv *p_hdmi_priv);
//
// HDMI command types
//
typedef enum {
	HDMI_COMMAND_GET,
	HDMI_COMMAND_SET
} hdmi_command_t;

#define HDMI_AVI_FLAG_ITCONTENT 0x00800000
#define HDMI_AVI_FLAG_RGB_QUANT_RANGE 0x00040000
#define HDMI_AVI_FLAG_SCAN_INFO 0x00000001
#define HDMI_AVI_FLAG_BAR_INFO 0x00000010
//
// CEA-861b definitions
//
#define HDMI_CEA_VERSION             0x00
#define HDMI_ELD_VERSION             0x01
#define HDMI_EELD_VERSION            0x02
#define HDMI_BASE_ELD_SIZE           0x0E
#define HDMI_CEA_EDID_HEADER_SIZE    0x04
#define HDMI_EELD_CEA_EDID_VERSION   0x03
#define HDMI_CEA_EDID_BLOCK_SIZE     128

//
//Basic Audio support definitions
//

#define HDMI_BASIC_AUDIO_SUPPORTED                0x40
#define HDMI_CEA_EXTENSION_BLOCK_BYTE_3            3
#define HDMI_FL_AND_FR_SPEAKERS_CONNECTED        0x1

//
// HDMI buffer/information types
//
typedef enum {
	// Non-standard or non-HDMI type
	HDMI_ELD_TYPE = 0x00,	// ELD buffer type
	HDMI_EELD_TYPE = 0x01,	// EELD buffer type

	// Per HDMI Spec, refer Table 2-1 in HDMI EDS 
	// or Table 5-8 in HDMI spec
	HDMI_VS_TYPE = 0x81,	// Vendor-Specific InfoFrame type
	HDMI_AVI_TYPE = 0x82,	// AVI InfoFrame type
	HDMI_SPD_TYPE = 0x83,	// SPD InfoFrame type
	HDMI_AUDIO_TYPE = 0x84,	// Audio InfoFrame type
	HDMI_MS_TYPE = 0x85,	// MPEG Source InfoFrame type

	// Non-standard or non-HDMI types
	HDMI_PR_PE_TYPE = 0x86,	// Pixel Replication & Pixel Encoding(colorimetry) type
	HDMI_AUDIO_CAPS_TYPE = 0x87,	// Encoder Audio Capabilities type
	HDMI_AUDIO_ENABLE_FLAGS_TYPE = 0x88	// Flags for enabling / disabling audio
} hdmi_info_type_t;

//
// InfoFrame Version Information
//
typedef enum {
	HDMI_VS_VERSION = 1,	// Vendor-Specific InfoFrame Version 1
	HDMI_AVI_VERSION = 1,	// AVI InfoFrame Version 1
	HDMI_AVI_VERSION2 = 2,	// AVI InfoFrame Version 2
	HDMI_SPD_VERSION = 1,	// SPD InfoFrame Version 1
	HDMI_AUDIO_VERSION = 1,	// Audio InfoFrame Version 1
	HDMI_MS_VERSION = 1	// MPEG Source InfoFrame Version 1
} infoframe_version_t;

//
// InfoFrame Payload Length in bytes
//
typedef enum {
	HDMI_VS_MAX_LENGTH = 27,	// Vendor-Specific InfoFrame Payload Length, including IEEE reg ID
	HDMI_AVI_LENGTH = 13,	// AVI InfoFrame Payload Length
	HDMI_SPD_LENGTH = 25,	// SPD InfoFrame Payload Length
	HDMI_AUDIO_LENGTH = 10,	// Audio InfoFrame Payload Length
	HDMI_MS_LENGTH = 10,	// MPEG Source InfoFrame Payload Length
	HDMI_PR_PE_LENGTH = 4,	// Length of PR_PE_TYPE
	HDMI_AUDIO_CAPS_LENGTH = 4	// Length of AUDIO_CAPS_TYPE
} infoframe_length_t;

//
// InfoFrame TOTAL Length in bytes (includes header + payload)
//
typedef enum {
	HDMI_VS_MAX_TOTAL_LENGTH = HDMI_VS_MAX_LENGTH + 4,	// Max Total size of Vendor-Specific InfoFrame
	HDMI_AVI_TOTAL_LENGTH = HDMI_AVI_LENGTH + 5,	// Total size of AVI InfoFrame
	HDMI_SPD_TOTAL_LENGTH = HDMI_SPD_LENGTH + 4,	// Total size of SPD InfoFrame
	HDMI_AUDIO_TOTAL_LENGTH = HDMI_AUDIO_LENGTH + 4,	// Total size of Audio InfoFrame
	HDMI_MS_TOTAL_LENGTH = HDMI_MS_LENGTH + 4,	// Total size of MPEG Source InfoFrame
} infoframe_total_length_t;

//
// Pixel Replication multipliers
//
typedef enum {
	HDMI_PR_ONE = 0,	// No repetition (ie., pixel sent once)
	HDMI_PR_TWO,		// Pixel sent 2 times (ie.,repeated once)
	HDMI_PR_THREE,		// Pixel sent 3 times
	HDMI_PR_FOUR,		// Pixel sent 4 times
	HDMI_PR_FIVE,		// Pixel sent 5 times
	HDMI_PR_SIX,		// Pixel sent 6 times
	HDMI_PR_SEVEN,		// Pixel sent 7 times
	HDMI_PR_EIGHT,		// Pixel sent 8 times
	HDMI_PR_NINE,		// Pixel sent 9 times
	HDMI_PR_TEN		// Pixel sent 10 times
} hdmi_pixel_replication_t;

//
// Pixel encoding modes
//
//typedef typedef enum  {
  //  HDMI_RGB256 = 0x01,
  //  HDMI_RGB220 = 0x02,
  //  HDMI_YCrCb422 = 0x04,
  //  HDMI_YCrCb444 = 0x08
//}HDMI_COLORIMETRY;

//
// Pixel encoding modes
//
typedef enum {
	HDMI_COLORIMETRY_RGB256 = 0x01,
	HDMI_COLORIMETRY_RGB220 = 0x02,
	HDMI_COLORIMETRY_YCrCb422 = 0x04,
	HDMI_COLORIMETRY_YCrCb444 = 0x08
} hdmi_colorimetry_t;

//
// AVI InfoFrame definitions - start
//
// Scan Info
typedef enum {
	HDMI_AVI_SCAN_NODATA = 0,	// No data
	HDMI_AVI_SCAN_OVERSCAN = 1,	// Overscanned (TV)
	HDMI_AVI_SCAN_UNDERSCAN = 2,	// Underscanned (Computer)
	HDMI_AVI_SCAN_FUTURE = 3	// Future
} avi_scan_info_t;

// Bar Info
typedef enum {
	HDMI_AVI_BAR_INVALID = 0,	// Bar data not valid
	HDMI_AVI_BAR_VALID_VERTICAL = 1,	// Vertical Bar data valid
	HDMI_AVI_BAR_VALID_HORIZONTAL = 2,	// Horizontal Bar data valid
	HDMI_AVI_BAR_VALID_BOTH = 3	// Vertical & Horizontal Bar data valid
} avi_bar_info_t;

// Active Format Information
typedef enum {
	HDMI_AVI_AFI_INVALID = 0,	// No data
	HDMI_AVI_AFI_VALID = 1	// Active Format Information valid
} avi_fi_info_t;

// AVI Pixel Encoding modes
typedef enum {
	HDMI_AVI_RGB_MODE = 0,	// RGB pixel encoding mode
	HDMI_AVI_YCRCB422_MODE = 1,	// YCrCb 4:2:2 mode
	HDMI_AVI_YCRCB444_MODE = 2,	// YCrCb 4:4:4 mode
	HDMI_AVI_FUTURE_MODE = 3	// Future mode
} avi_encoding_mode_t;

// AVI Active Format Aspect Ratio
typedef enum {
	HDMI_AVI_AFAR_SAME = 8,	// same as picture aspect ratio
	HDMI_AVI_AFAR_4_3 = 9,	// 4:3 center
	HDMI_AVI_AFAR_16_9 = 10,	// 16:9 center
	HDMI_AVI_AFAR_14_9 = 11	// 14:9 center
} avi_afar_info_t;

// AVI Picture Aspect Ratio
typedef enum {
	HDMI_AVI_PAR_NODATA = 0,	// No Data
	HDMI_AVI_PAR_4_3 = 1,	// 4:3
	HDMI_AVI_PAR_16_9 = 2,	// 16:9
	HDMI_AVI_PAR_FUTURE = 3	// Future
} avi_par_info_t;

// AVI Colorimetry Information
typedef enum {
	HDMI_AVI_COLOR_NODATA = 0,	// No data
	HDMI_AVI_COLOR_ITU601 = 1,	// SMPTE 170M, ITU601
	HDMI_AVI_COLOR_ITU709 = 2,	// ITU709
	HDMI_AVI_COLOR_FUTURE = 3	// Future
} avi_color_info_t;

// AVI Non-uniform Picture Scaling Info
typedef enum {
	HDMI_AVI_SCALING_NODATA = 0,	// No scaling
	HDMI_AVI_SCALING_HORIZONTAL = 1,	// horizontal scaling
	HDMI_AVI_SCALING_VERTICAL = 2,	// vertical scaling
	HDMI_AVI_SCALING_BOTH = 3	// horizontal & vertical scaling
} avi_scaling_infp_t;

// AVI RGB Quantization Range
typedef enum {
	HDMI_AVI_RGBQUANT_DEFAULT = 0,	// Default value
	HDMI_AVI_RGBQUANT_LIMITED = 1,	// Limited Range
	HDMI_AVI_RGBQUANT_FULL = 2,	// Full Range
	HDMI_AVI_RGBQUANT_FUTURE = 3	// Future use
} avi_rgbquant_range_t;

// AVI IT Content
typedef enum {
	HDMI_AVI_ITC_NODATA = 0,	// No Data
	HDMI_AVI_ITC_ITCONTENT = 1	//IT Content
} avi_it_content_t;

//
// AVI InfoFrame definitions - end
//

//
// SPD InfoFrame definitions - start
//
// SPD InfoFrame Data Byte 25, refer Table-17 in CEA-861b
typedef enum {
	HDMI_SPD_SRC_UNKNOWN = 0x00,	// unknown
	HDMI_SPD_SRC_DIGITAL_STB = 0x01,	// Digital STB
	HDMI_SPD_SRC_DVD = 0x02,	// DVD
	HDMI_SPD_SRC_DVHS = 0x03,	// D-VHS
	HDMI_SPD_SRC_HDD_VIDEO = 0x04,	// HDD Video
	HDMI_SPD_SRC_DVC = 0x05,	// DVC
	HDMI_SPD_SRC_DSC = 0x06,	// DSC
	HDMI_SPD_SRC_VCD = 0x07,	// Video CD
	HDMI_SPD_SRC_GAME = 0x08,	// Game
	HDMI_SPD_SRC_PC = 0x09	// PC General
} spd_src_type_t;

// SPD InfoFrame Vendor Name & Descriptor Length in bytes
typedef enum {
	HDMI_SPD_VNAME_LENGTH = 8,	// SPD Vendor Name Length in bytes
	HDMI_SPD_VDESC_LENGTH = 16,	// SPD Vendor Descriptor Length in bytes
} spd_namedesc_length_info_t;

//
// SPD InfoFrame definitions - end
//

//
// InfoFrame Packet Header - generic
//
typedef struct _if_header {
	uint8_t type;		// InfoFrame Type
	uint8_t version;	// InfoFrame Version
	uint8_t length;		// InfoFrame Length
	uint8_t ecc;		// ECC Parity
} if_header_t;

//
// AVI InfoFrame structure
//
typedef union _avi_if {
	uint8_t avi_buf[HDMI_AVI_TOTAL_LENGTH];
#pragma pack(1)
	struct {
		if_header_t avi_if_header;	// AVI header data
		uint8_t chksum;	//checksum
		union {
			uint8_t byte1;
			struct {
				uint8_t scan_info:2;	// scan information
				uint8_t bar_info:2;	// bar information
				uint8_t format:1;	// active format information
				uint8_t enc_mode:2;	// pixel encoding (RGB or YCrCb)
				uint8_t b1rsvd:1;	// reserved
			} byte1_bits;
		};
		union {
			uint8_t byte2;
			struct {
				uint8_t afar:4;	// Active Format Aspect Ratio
				uint8_t par:2;	// Picture Aspect Ratio
				uint8_t colorimetry:2;	// colorimetry
			} byte2_bits;
		};
		union {
			uint8_t byte3;
			struct {
				uint8_t scaling_info:2;	// Scaling information
				uint8_t rgbquant_range:2;	// RGB Quantization Range
				uint8_t ext_colorimetry:3;	//Extended Colorimetry
				uint8_t it_content:1;	//IT Content
			} byte3_bits;
		};
		union {
			uint8_t byte4;
			struct {
				uint8_t vic:7;	// Video Identification code (refer Table 13 in CEA-861b)
				uint8_t b4rsvd:1;	// reserved
			} byte4_bits;
		};
		union {
			uint8_t byte5;
			struct {
				uint8_t pr:4;	// pixel repetition (refer Table 15 in CEA-861b)
				uint8_t b5rsvd:4;	// reserved
			} byte5_bits;
		};
		uint8_t byte6;	// end of top bar(lower), set to "00"
		uint8_t byte7;	// end of top bar(upper), set to "00"
		uint8_t byte8;	// start of bottom bar(lower), set to "00"
		uint8_t byte9;	// start of bottom bar(upper), set to "00"
		uint8_t byte10;	// end of left bar(lower), set to "00"
		uint8_t byte11;	// end of left bar(upper), set to "00"
		uint8_t byte12;	// start of right bar(lower), set to "00"
		uint8_t byte13;	// start of right bar(upper), set to "00"
	} avi_info;
#pragma pack()
} avi_if_t;

//
// SPD InfoFrame structure
//
typedef union _spd_if {
	uint8_t spd_buf[HDMI_SPD_TOTAL_LENGTH];
#pragma pack(1)
	struct {
		if_header_t spd_if_header;	// SPD header data
		uint8_t name[8];	// Vendor Name, 8 characters
		uint8_t desc[16];	// Product Description, 16 characters
		uint8_t sdi;	// Source Device Information
	};
#pragma pack()
} spd_if_t;

//
// Vendor Specific InfoFrame structure
//
typedef union _vs_if {
	uint8_t vs_buf[HDMI_VS_MAX_TOTAL_LENGTH];
#pragma pack(1)
	struct {
		if_header_t vs_if_header;	// VS header data
		uint8_t ieee_reg_id[3];	// 3-byte IEEE registration ID
		uint8_t pay_load[24];	// Payload bytes
	};
#pragma pack()
} vs_if_t;

//
// AVI Infoframe structure for customization
//

typedef struct _avi_infoframe_custom {
	//GUID        guid;                   // GUID
	int32_t command;	// Command
	int32_t flags;		// Flags
	uint32_t type_code;	// Type code of AVI Infoframe
	uint32_t version;	// Version of AVI Infoframe
	uint32_t length;	// Length of AVI Info Frame 
	uint8_t r3r0_valid;	// Reserved
	uint8_t it_content;	// IT Content
	uint8_t bar_info[8];	// Reserved
	int32_t active_format_aspect_ratio;	// Reserved 
	int32_t non_uniform_scaling;	// Reserved 
	int32_t rgb_ycc_indicator;	// Reserved 
	int32_t ext_colorimetry;	// Reserved 
	int32_t pixel_factor;	// Reserved 
	int32_t bar_info_valid;	// Reserved 
	int32_t colorimetry;	// Reserved 
	int32_t aspect_ratio;	// Reserved 
	int32_t quant_range;	// Quantization Range
	int32_t video_code;	// Reserved 
	int32_t scan_info;	// Scan Information
} avi_infoframe_custom_t;

//
// LinearPCM Consolidated Audio Data(CAD) structure
//
typedef union _lpcm_cad {
	uint8_t value;
	struct {
		uint8_t maxch_cp_on:3;	// Max channels-1 supported with CP turned ON
		uint8_t maxch_cp_off:3;	// Max channels-1 supported with CP turned OFF
		uint8_t sp_20bit:1;	// 20-bit sample support
		uint8_t sp_24bit:1;	// 24-bit sample support
	};
} lpcm_cad_t;

//
// CEA Short Audio Descriptor
// 
typedef struct _cea_861b_adb {
#pragma pack(1)
	union {
		uint8_t byte1;
		struct {
			uint8_t max_channels:3;	// Bits[0-2]
			uint8_t audio_format_code:4;	// Bits[3-6], see AUDIO_FORMAT_CODES
			uint8_t b1reserved:1;	// Bit[7] - reserved
		};
	};
	union {
		uint8_t byte2;
		struct {
			uint8_t sp_rate_32kHz:1;	// Bit[0] sample rate = 32kHz
			uint8_t sp_rate_44kHz:1;	// Bit[1] sample rate = 44kHz
			uint8_t sp_rate_48kHz:1;	// Bit[2] sample rate = 48kHz
			uint8_t sp_rate_88kHz:1;	// Bit[3] sample rate = 88kHz
			uint8_t sp_rate_96kHz:1;	// Bit[4] sample rate = 96kHz
			uint8_t sp_rate_176kHz:1;	// Bit[5] sample rate = 176kHz
			uint8_t sp_rate_192kHz:1;	// Bit[6] sample rate = 192kHz
			uint8_t sp_rate_b2reserved:1;	// Bit[7] - reserved
		};
	};
	union {
		uint8_t byte3;	// maximum bit rate divided by 8kHz
		// following is the format of 3rd byte for uncompressed(LPCM) audio
		struct {
			uint8_t bit_rate_16bit:1;	// Bit[0]
			uint8_t bit_rate_20bit:1;	// Bit[1]
			uint8_t bit_rate_24bit:1;	// Bit[2]
			uint8_t bit_rate_b3reserved:5;	// Bits[3-7]
		};
	};
#pragma pack()
} cea_861b_adb_t;

//
// Enhanced EDID Like Data aka EELD structure
//
typedef union _hdmi_eeld {
	uint8_t eeld[HDMI_EELD_SIZE];
#pragma pack(1)
	struct {
		// Byte[0] = ELD Version Number
		union {
			uint8_t byte0;
			struct {
				uint8_t reserved:3;	// Reserf
				uint8_t eld_ver:5;	// ELD Version Number
				//  00000b - reserved
				//  00001b - first rev
				//  00010b:11111b - reserved for future
			};
		};

		// Byte[1] = Vendor Version Field
		union {
			uint8_t vendor_version;
			struct {
				uint8_t reserved1:3;
				uint8_t veld_ver:5;	// Version number of the ELD extension.
				// This value is provisioned and unique to each vendor.
			};
		};

		// Byte[2] = Baseline Lenght field
		uint8_t baseline_eld_length;	// Length of the Baseline structure divided by Four.

		// Byte [3] = Reserved for future use
		uint8_t byte3;

		// Starting of the BaseLine EELD structure
		// Byte[4] = Monitor Name Length 
		union {
			uint8_t byte4;
			struct {
				uint8_t mnl:5;
				uint8_t cea_edid_rev_id:3;
			};
		};

		// Byte[5] = Capabilities
		union {
			uint8_t capabilities;
			struct {
				uint8_t hdcp:1;	// Indicates HDCP support
				uint8_t ai_support:1;	// Inidcates AI support
				uint8_t connection_type:2;	// Indicates Connection type 
				// 00 - HDMI
				// 01 - DP
				// 10 -11  Reserved for future connection types
				uint8_t sadc:4;	// Indicates number of 3 bytes Short Audio Descriptors. 
			};
		};

		// Byte[6] = Audio Synch Delay
		uint8_t audio_synch_delay;	// Amount of time reported by the sink that the video trails audio in milliseconds.

		// Byte[7] = Speaker Allocation Block
		union {
			uint8_t speaker_allocation_block;
			struct {
				uint8_t flr:1;	// Front Left and Right channels
				uint8_t lfe:1;	// Low Frequency Effect channel
				uint8_t fc:1;	// Center transmission channel
				uint8_t rlr:1;	// Rear Left and Right channels
				uint8_t rc:1;	// Rear Center channel
				uint8_t flrc:1;	// Front left and Right of Center transmission channels
				uint8_t rlrc:1;	// Rear left and Right of Center transmission channels
				uint8_t reserved3:1;	// Reserved
			};
		};

		// Byte[8 - 15] - 8 Byte port identification value
		uint8_t port_id_value[8];

		// Byte[16 - 17] - 2 Byte Manufacturer ID
		uint8_t manufacturer_id[2];

		// Byte[18 - 19] - 2 Byte Product ID
		uint8_t product_id[2];

		// Byte [20-83] - 64 Bytes of BaseLine Data
		uint8_t mn_sand_sads[64];	// This will include
		// - ASCII string of Monitor name
		// - List of 3 byte SADs
		// - Zero padding

		// Vendor ELD Block should continue here!
		// No Vendor ELD block defined as of now. 
	};
#pragma pack()
} hdmi_eeld_t;

//
// Data structure for misc HDMI data
//
typedef struct _misc_hdmi_data {
	int32_t colorimetry:4;	// 
	int32_t pr:4;		// pixel repetition value
	int32_t reserved:24;	// reserved bits
} misc_hdmi_data_t;

//
// Audio capability structure
//
typedef struct _device_audio_caps {
	int32_t npl_design:8;	// max number of audio packets device can
	// deliver per line
	int32_t k0:8;		// The overhead(in pixels) per line requied
	// by device for setting up audio packets when
	// CP is disabled
	int32_t k1:8;		// The overhead(in pixels) per line requied
	// by device for setting up audio packets when
	// CP is enabled
	// Misc data
	int32_t pr:4;		// Pixel Replication value
	int32_t is_hdcp:1;	// Driver, Device and Receiver support HDCP
	int32_t is_rptr:1;	// Receiver is HDCP repeater
	int32_t reserved:2;	// reserved bits
} device_audio_caps_t;

typedef struct _audio_enable_flags {
	int32_t is_hdmi_display:1;	//1 if HDMI display, 0 if not HDMI display
	int32_t is_eld_valid:1;	//1 if ELD valid, 0 if ELD not valid
	int32_t reserved1:30;
} audio_enable_flags_t;

//
// Data structure to exchange HDMI data through GetSetParameters interface
//
typedef struct _hdmi_parameters {
	//GUID              Guid;
	hdmi_command_t command;
	uint8_t type;
	uint8_t size;
	union {
		hdmi_eeld_t eeld_buffer;
		avi_if_t avi_infoframe;
		spd_if_t spd_infoframe;
		vs_if_t vs_infoframe;
		union {
			int32_t gen_data;
			device_audio_caps_t audio_caps;
			misc_hdmi_data_t misc_data;
			audio_enable_flags_t fl_audio_enable_flags;
		};
	};
} hdmi_parameters_t;

//
// Audio format codes
//
typedef enum {
	AUDIO_LPCM = 0x0001,	// Linear PCM (eg. IEC60958)
	AUDIO_AC3 = 0x0002,	// AC-3
	AUDIO_MPEG1 = 0x0003,	// MPEG1 (Layers 1 & 2)
	AUDIO_MP3 = 0x0004,	// MP3   (MPEG1 Layer 3)
	AUDIO_MPEG2 = 0x0005,	// MPEG2 (multichannel)
	AUDIO_AAC = 0x0006,	// AAC
	AUDIO_DTS = 0x0007,	// DTS
	AUDIO_ATRAC = 0x0008,	// ATRAC
	AUDIO_OBA = 0x0009,	// One Bit Audio
	AUDIO_DOLBY_DIGITAL = 0x000A,	// Dolby Digital
	AUDIO_DTS_HD = 0x000B,	// DTS-HD
	AUDIO_MAT = 0x000C,	// MAT (MLP)
	AUDIO_DST = 0x000D,	// DST
	AUDIO_WMA_PRO = 0x000E	// WMA Pro
} audio_format_codes_t;

//
// Data structure for byte #6 to 8 which has fixed definition
//
typedef struct _vsdb_char6_to_char8 {
#pragma pack(1)

	union {
		uint8_t byte1;
		struct {
			uint8_t dvi_dual:1;	// Bit[0]
			uint8_t b1reserved:2;	// Bits[1-2]
			uint8_t dcy444:1;	// Bit[3] YCBCR 4:4:4 in Deep Color modes.
			uint8_t dc30bit:1;	//Bit[4]
			uint8_t dc36bit:1;	//Bit[5]
			uint8_t dc48bit:1;	//Bit[6]
			uint8_t supports_ai:1;	// Bit[7]
		};
	};

	uint8_t max_tmds_clock;

	union {
		uint8_t byte3;
		struct {
			uint8_t b3reserved:6;	// Bit[0-5] reserved
			uint8_t i_latency_field_present:1;	// Bit[6]
			uint8_t latency_field_present:1;	// Bits[7]
		};
	};

#pragma pack()
} vsdb_byte6_to_byte8_t;

//
// Gamut metadata structure
//
// Note : The data is written in big endian format

#define HDMI_GBD_PKT_TYPE 0x0A
#define HDMI_GBD_P0_DATA_SIZE 27
#define HDMI_MAX_VERTICES_DATA 25
#define HDMI_MAX_FACET_DATA 25

typedef enum {
	VERTICES_AND_FACETS = 0,
	RGB_MIN_MAX_RANGE = 1
} gbd_format_flag_t;

typedef enum {
	GBD_8BIT_PRECISION = 0,
	GBD_10BIT_PRECISION = 1,
	GBD_12BIT_PRECISION = 2
} gbd_color_precision_t;

typedef enum {
	RGB_BT709 = 0,
	XVY_CC601 = 1,
	XVY_CC709 = 2,
	RESERVED_COLORSPACE
} gbd_color_space_t;

typedef enum {
	MIN_RED_INDEX = 0,
	MAX_RED_INDEX = 1,
	MIN_GREEN_INDEX = 2,
	MAX_GREEN_INDEX = 3,
	MIN_BLUE_INDEX = 4,
	MAX_BLUE_INDEX = 5,
	MAX_RANGE_DATA_INDEX_LIMIT = 6
} gbd_rgb_range_data_index_t;

//
// App needs to feel the data in this structure
//
typedef struct _gbd_p0_hdmi_1_3 {
	uint8_t enable;		// Enable/Disable GBD profile sending
	gbd_format_flag_t format_flag;	// uses GBD_FORMAT_FLAG_EN, this defines the gamut data format
	gbd_color_precision_t color_precision;	// uses GBD_COLOR_PRECISION, this is the bit precision of GBD vertex and range data
	gbd_color_space_t color_space;	// uses GBD_COLOR_SPACE_EN, this defines the color space being represented

	union {
		// If bFormatFlag is 0
		struct {
			uint8_t facet_mode;	// spec supports 0 alone right now
			uint16_t num_vertices;	// Number of vertices 
			uint16_t num_facets;	// Number of faces

			// For 4 vertices of 12bits size is 18
			// Max possible with 0 facets and 28 bytes of GBD is 28-5=23 bytes
			uint16_t vertices_data[HDMI_MAX_VERTICES_DATA];	// Vertices data representation
			uint16_t facets_data[HDMI_MAX_FACET_DATA];	// kept it as input data but to be defined based on future spec
		} vertices_facets_data;

		// If eFormatFlag is 1
		struct {
			uint16_t rgb_primary_data[MAX_RANGE_DATA_INDEX_LIMIT];
		} rgb_range_data;
	};

} gbd_p0_hdmi_1_3_t;

#define HDMI_GBD_MAX_SEQ_NUM_INDEX 16

// various GBD profiles
typedef enum {
	P0_PROFILE = 0,
	P1_PROFILE = 1,
	P2_PROFILE = 2,
	P3_PROFILE = 3,
	INVALID_PROFILE
} gbd_profile_type_t;

// various packet transmission options
typedef enum {
	INTERMEDIATE_PKT_IN_SEQ = 0,
	FIRST_PKT_IN_SEQ = 1,
	LAST_PKT_IN_SEQ = 2,
	ONLY_PKT_IN_SEQ = 3
} gbd_pkt_seq_t;

//
// Packet header defn as per HDMI spec
//
typedef struct _gamut_pkt_header {
	uint8_t pkt_type;	// Defines the pkt type
	union {
		uint8_t field_byte;
		struct {
			uint8_t affected_gamut_info:4;	// BIT 3:0
			uint8_t gbd_profile:3;	// BIT 6:4 ; uses GBD_PROFILE_TYPE_EN
			uint8_t next_field:1;	// BIT7
		};
	};

	union {
		uint8_t gbd_seq_info;
		struct {
			uint8_t current_gamut_info:4;	// BIT 3:0
			uint8_t packet_seq:2;	// BIT 5:4 ; use GBD_PKT_SEQ_EN
			uint8_t reserved2:1;	// BIT 6
			uint8_t no_current_gbd:1;	// BIT 7
		};
	};
} gamut_pkt_header_t;

//
// Gamut structure contains data in following format
// 
typedef struct _gamut_metadata_struct {
#pragma pack(1)
	gamut_pkt_header_t pkt_hdr;	// Gamut Metadata header data
	union {
		uint8_t byte1;
		struct {
			uint8_t gbd_color_space:3;
			// Note: GBD buffer is formatted based upon the color precision
			// 8 bit precision : 1 sign bit, 2 bits of integer, 5 bits of fraction
			// 10 bit precision : 1 sign bit, 2 bits of integer, 7 bits of fraction
			// 12 bit precision : 1 sign bit, 2 bits of integer, 9 bits of fraction
			uint8_t gbd_color_precision:2;
			uint8_t reserved3:1;
			uint8_t facet_mode:1;	// 0 - No facet info in GBD; 1 - Facet info in GBD
			uint8_t format_flag:1;	// uses GBD_FORMAT_FLAG_EN
		};
	};

	// For P0 profile below is the syntax in which data will be filled
	// If Format is YUV
	// char 2 : Higher 8 bits of number of vertices
	// char 3 : Lower 8 bits of number of vertices
	// char 4 to VSIZE+2 : Vertex data of size VSIZE, 
	// where VSIZE = 3*number of vertices*GBD color precision/8 + 0.99999
	// char VSIZE+3: Higher 8 bits of number of facets
	// char VSIZE+4: Lower 8 bits of number of facets
	// char VSIZE+5 to VSIZE+FSIZE+4 : Facet data
	// where VSIZE = number of facet data
	uint8_t gbd_data[HDMI_GBD_P0_DATA_SIZE];	// data will be filled

#pragma pack()
} gamut_metadata_st_t;

struct hdmi_edid_info {
	char monitor_name[HDMI_MONITOR_NAME_LENGTH];
	char *edid_info;
};

#define HDMI_EDID_INFO(nm, info) \
	.monitor_name = nm, .edid_info = info

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34))
#define MSIC_IRQLVL1_STATUS 0x02
#define VREG_STATUS (1 << 5)
#define MSIC_VRINT_STATUS 0x0d
#define HDMI_HPD_STATUS (1 << 3)
#define HDMI_OCP_STATUS (1 << 2)
#define VR_OCP_STATUS (1 << 1)
#define VR_OVP_STATUS (1 << 0)
#define MSIC_VRINT_MASK 0x1c
#define HDMI_HPD_MASK (1 << 3)
#define HDMI_OCP_MASK (1 << 2)
#define VR_OCP_MASK (1 << 1)
#define VR_OVP_MASK (1 << 0)
#define MSIC_IRQLVL1_MASK 0x21
#define VREG_MASK (1 << 5)
#define MSIC_VCC330CNT 0xd3
#define VCC330_OFF 0x24
#define VCC330_ON 0x37
#define MSIC_VHDMICNT 0xde
#define VHDMI_OFF 0x24
#define VHDMI_ON 0xa4
#define VHDMI_DB_30MS 0x60
#define MSIC_HDMI_STATUS 0x281
#define HPD_SIGNAL_STATUS (1 << 0)
#endif

/* HDMI Video Timing */
struct hdmi_video_format_timing {
	u32 video_code;
	u32 hdisplay;
	u32 vdisplay;
	u32 refresh;
	bool bInterlace;
	u32 hpolarity;
	u32 vpolarity;
	avi_afar_info_t afar;
};

/* start of S3D header */
#define HDMI_S3D_CEA_EDID_VERSION	0x03
#define HDMI_S3D_INFO_VERSION		0x01
#define CEA_EDID_EXTENSION_TAG		0x02
#define CEA_VDB_VIC_MASK		0x7f
#define HDMI_VSDB_MASK			0x00ffffff
#define HDMI_VSDB			0x00000c03
#define HDMI_VIDEO_PRESENT_OFFSET	0x8
#define HDMI_VIDEO_PRESENT_BIT		(1 << 5)
#define I_LATENCY_FIELDS_PRESENT_BIT	(1 << 6)
#define LATENCY_FIELDS_PRESENT_BIT	(1 << 7)
#define HDMI_3D_PRESENT_BIT		(1 << 7)
#define HDMI_3D_MULTI_PRESENT_BITS	(0x3 << 5)
#define HDMI_3D_MULTI_PRESENT_BITS_POS	0x5
#define HDMI_3D_LEN_BITS		0x1f
#define HDMI_VIC_LEN_BITS		(0x7 << 5)
#define HDMI_VIC_LEN_BITS_POS		0x5
#define HDMI_3D_STRUCTURE_BITS		0xf
#define HDMI_2D_VIC_ORDER_BITS		(0xf << 4)
#define HDMI_2D_VIC_ORDER_BITS_POS	0x4
#define RESERVED_BITS			0xf
#define HDMI_3D_DETAILS_BITS		(0xf << 4)
#define HDMI_3D_DETAILS_BITS_POS	0x4
#define HDMI_3D_STRUCTURE_EXTRA_BIT	0x8

#define HDMI_3D_MANDATORY_1080P24	(S3D_FRAME_PACKING_BIT | S3D_TOP_AND_BOTTOM_BIT)
#define HDMI_3D_MANDATORY_720P		(S3D_FRAME_PACKING_BIT | S3D_TOP_AND_BOTTOM_BIT)
#define HDMI_3D_MANDATORY_1080I		S3D_SIDE_BY_SIDE_HALF_BIT

/* Display 3D format data structure. */
typedef struct _hdmi_3d_info {
	uint8_t cea_edid_rev_id;
	uint8_t s3d_info_ver;
	uint16_t s3d_structure_all[16];
	/* Mandatory 3D display is supported by the HDMI display. */
	bool s3d_mandatory;
	bool s3d_vic;
} hdmi_3d_info_t;

/* end of S3D header */

struct mid_intel_hdmi_priv {
	u32 hdmib_reg;
	u32 save_HDMIB;
	bool has_hdmi_sink;
	/* Should set this when detect hotplug */
	bool hdmi_device_connected;
	struct mdfld_hdmi_i2c *i2c_bus;
	/* EELD packet holder */
	hdmi_eeld_t eeld;
	u32 hdmi_eeld_size;
	cea_861b_adb_t lpcm_sad;
	bool is_hdcp_supported;
	struct i2c_adapter *hdmi_i2c_adapter;	/* for control functions */
	hdmi_3d_info_t s3d_info;
	uint8_t vdb_vic[16];
	struct drm_device *dev;
	struct drm_display_mode *mimic_mode;
	struct drm_display_mode *edid_preferred_mode;
};

#endif				//__IHDMI_H__
