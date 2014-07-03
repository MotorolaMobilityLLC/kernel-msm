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
#ifndef _OTM_HDMI_DEFS_H
#define _OTM_HDMI_DEFS_H


#include <linux/types.h>

#include "otm_hdmi_types.h"

/* Definition for debug print format */
#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt)	"[otm_hdmi]: " fmt
#endif

/**
 Existing port driver IDs with room for user specified port drivers.
 When communicating with a specific port driver the port id must be passed.
*/
typedef enum {
	OTM_HDMI_MIN_SUPPORTED_DRIVERS = 0, /* Port Driver IDs start at this
						value */

	OTM_HDMI_ID_INTTVENC = OTM_HDMI_MIN_SUPPORTED_DRIVERS, /* CVBS TV
								encoder */
	OTM_HDMI_ID_INTTVENC_COMPONENT,	/* Component TV encoder */
	OTM_HDMI_ID_HDMI,		/* HDMI */
	OTM_HDMI_ID_USER_MIN,	/* Begin user defined drivers */
	OTM_HDMI_ID_USER_MAX = 8,	/* End user defined drivers */

	OTM_HDMI_MAX_SUPPORTED_DRIVERS	/* Maximum number of port drivers. */
} otm_hdmi_id_t;

/**
Defined port driver attributes.  Which ones are supported (and exactly how)
can vary from one port driver to another.  See the Intel� CE Media Processors
GDL 3.0 Programming Guide for details on the attributes supported by
each port driver.

The following Attributes require "elevated" privileges:
OTM_HDMI_ATTR_ID_HDCP
OTM_HDMI_ATTR_ID_HDCP_AUTO_MUTE
OTM_HDMI_ATTR_ID_POWER
OTM_HDMI_ATTR_ID_MUTE
OTM_HDMI_ATTR_ID_HDCP_AUTO_PHY
OTM_HDMI_ATTR_ID_HDCP_AUTO_RETRY
OTM_HDMI_ATTR_ID_HDCP_RI_RETRY
OTM_HDMI_ATTR_ID_HDCP_1P1
OTM_HDMI_ATTR_ID_HDCP_ENCRYPT
*/

/* *NOTE* Extend pd_lib.c::__pd_attr_get_name() when adding new entries */
typedef enum {
	OTM_HDMI_MIN_SUPPORTED_ATTRIBUTES = 0,	/* Attribute ID enum's start
						at this value */

	OTM_HDMI_ATTR_ID_HDCP = OTM_HDMI_MIN_SUPPORTED_ATTRIBUTES, /* HDCP
								control */
	OTM_HDMI_ATTR_ID_HDCP_AUTO_MUTE, /* HDCP auto mute */
	OTM_HDMI_ATTR_ID_HDCP_STATUS, /*HDCP status */
	OTM_HDMI_ATTR_ID_HDCP_1P1, /* HDCP 1.1 Pj check control */
	OTM_HDMI_ATTR_ID_COLOR_SPACE_INPUT, /*Input colorspace */
	OTM_HDMI_ATTR_ID_PIXEL_FORMAT_OUTPUT, /* Output colorspace */
	OTM_HDMI_ATTR_ID_PIXEL_DEPTH, /* Depth of outgoing pixels */
	OTM_HDMI_ATTR_ID_BG_COLOR, /* Color for HDCP failure & video mute */
	OTM_HDMI_ATTR_ID_CABLE_STATUS, /* Cable status */
	OTM_HDMI_ATTR_ID_PAR, /* Picture aspect ratio */
	OTM_HDMI_ATTR_ID_FAR, /* Format aspect ratio */
	OTM_HDMI_ATTR_ID_USE_EDID, /* TV timings source */
	OTM_HDMI_ATTR_ID_SLOW_DDC, /* DDC bus speed */
	OTM_HDMI_ATTR_ID_AUDIO_CLOCK, /* Audio clock */
	OTM_HDMI_ATTR_ID_COLOR_SPACE_EXT, /* Extended colorimetry */
	OTM_HDMI_ATTR_ID_OUTPUT_CLAMP, /* Clamp output in [16,235] when it is */
	/* RGB color space. In YCbCr output */
	/* this attribute is ignored. */
	OTM_HDMI_ATTR_ID_BRIGHTNESS, /* Brightness Level */
	OTM_HDMI_ATTR_ID_CONTRAST, /* Contrast Level */
	OTM_HDMI_ATTR_ID_HUE, /* Hue Angle */
	OTM_HDMI_ATTR_ID_SATURATION, /* Saturation Level */
	OTM_HDMI_ATTR_ID_ACP, /* Analog Content Protection */
	OTM_HDMI_ATTR_ID_CC, /* Closed Captioning */
	OTM_HDMI_ATTR_ID_OUTPUT_DITHER,	/* Dither 12-bit->10/8bit conversion */
	OTM_HDMI_ATTR_ID_SHARPNESS_HLUMA, /* Horizontal Luma filter */
	OTM_HDMI_ATTR_ID_SHARPNESS_HCHROMA, /* Horizontal Chroma Filter */
	OTM_HDMI_ATTR_ID_BLANK_LEVEL, /* Sync pulse level */
	OTM_HDMI_ATTR_ID_BLACK_LEVEL, /* Black Level */
	OTM_HDMI_ATTR_ID_BURST_LEVEL, /* Burst Level */
	OTM_HDMI_ATTR_ID_HDCP_DELAY,	/* Delay of HDCP start, waiting for TMDS to be available */
	OTM_HDMI_ATTR_ID_HDCP_RI_RETRY,	/* RI retry delay */
	OTM_HDMI_ATTR_ID_DVI,	/* DVI enforcement */
	OTM_HDMI_ATTR_ID_TVOUT_TYPE, /* Current DAC configuration */
	OTM_HDMI_ATTR_ID_HDCP_ENCRYPT, /* HDCP encryption control */
	OTM_HDMI_ATTR_ID_3CH_SYNC, /* 3 Channel sync */
	OTM_HDMI_ATTR_ID_SD_OPTION, /* Alternate SD mode (e.g.: PAL-M, */
	/* PAL-N, etc.) */
	OTM_HDMI_ATTR_ID_RGB,	/* RGB / YPbPr output selection */
	OTM_HDMI_ATTR_ID_CGMS_MODE, /* Current Copy Generation mode */
	OTM_HDMI_ATTR_ID_NO_SYNC, /* Sync removal from green (y) signal */
	OTM_HDMI_ATTR_ID_YC_DELAY, /* Luma vs Chroma delay */
	OTM_HDMI_ATTR_ID_POWER,	/* Disable DAC output */
	OTM_HDMI_ATTR_ID_NAME,	/* Driver name */
	OTM_HDMI_ATTR_ID_VERSION_MAJOR,	/* Driver major version */
	OTM_HDMI_ATTR_ID_VERSION_MINOR,	/* Driver minor version */
	OTM_HDMI_ATTR_ID_DEBUG,	/* Debug log */
	OTM_HDMI_ATTR_ID_BUILD_DATE, /* Driver Build date */
	OTM_HDMI_ATTR_ID_BUILD_TIME, /* Driver Build time */
	OTM_HDMI_ATTR_ID_DISPLAY_PIPE, /* Display Pipeline assigned */
	OTM_HDMI_ATTR_ID_SVIDEO, /* Assignment of component Pb & Pr */
	/* DACs for S-Video output */
	OTM_HDMI_ATTR_ID_AUDIO_STATUS, /* Status of audio playback */
	OTM_HDMI_ATTR_ID_TMDS_DELAY, /* HPD to TMDS assert delay */
	OTM_HDMI_ATTR_ID_MUTE, /* Mute */
	OTM_HDMI_ATTR_ID_PURE_VIDEO, /* Pure video indication control */
	OTM_HDMI_ATTR_ID_HDCP_AUTO_PHY, /* PHY blinking on HDCP Ri/Pj failure */
	OTM_HDMI_ATTR_ID_CALIBRATION, /* Calibration value */
	OTM_HDMI_ATTR_ID_HDCP_AUTO_RETRY, /* Automatic HDCP retry control */
	OTM_HDMI_ATTR_ID_INTERNAL_TEST,	/* CVBS/Component internal test */

	/* EXTENDED */
	OTM_HDMI_ATTR_ID_USER_MIN, /* Start of user defined attributes */
	OTM_HDMI_ATTR_ID_USER_MAX = 100, /* Max user defined attribute */
	OTM_HDMI_MAX_SUPPORTED_ATTRIBUTES /* End of attribute IDs;
						must be last */
} otm_hdmi_attribute_id_t;


/*
 * the following def should be moved to otm_htmi_internel.h later
 */
#define PD_NAME "pd_hdmi"

/** @ingroup disp_mode
 * Defines supported color spaces
 *
 */
typedef enum {
	/* Normally used for Standard Definition YCbCr content */
	OTM_HDMI_COLOR_SPACE_BT601 = 0,
	/* Normally used for High Definition YCbCr content */
	OTM_HDMI_COLOR_SPACE_BT709,
	/* Used for all RGB pixel formats */
	OTM_HDMI_COLOR_SPACE_RGB,
	/* Number of entries in this enumeration */
	OTM_HDMI_COLOR_SPACE_COUNT
} otm_hdmi_color_space_t;

/** @ingroup disp_mode
 * Display (pipe) ids.  The Intel CE Media Processors have two displays.
 */
typedef enum {
	/* [Pipe A] Main display/HDMI */
	OTM_HDMI_DISPLAY_ID_0 = 0,
	/* [Pipe B] Secondary display/Composite */
	OTM_HDMI_DISPLAY_ID_1,
	/* Undefined Pipe */
	OTM_HDMI_DISPLAY_ID_UNDEFINED
} otm_hdmi_display_id_t;

/*
 *
 * Attribute usage flags.
 */
/* TODO: Move OTM_HDMI_ATTR_FLAG_SUPPORTED since it's internal to PD */
typedef enum {
	OTM_HDMI_ATTR_FLAG_WRITE = 0x1, /* Attribute can be written */
	OTM_HDMI_ATTR_FLAG_SUPPORTED = 0x2,
				      /* Attribute is supported on this port
					driver. FOR INTERNAL USE ONLY. */
	OTM_HDMI_ATTR_FLAG_INTERNAL = 0x4,
				      /* Attribute is invisible to outside
					world. FOR INTERNAL USE ONLY */
} otm_hdmi_attribute_flag_t;

/*
 * Attribute flags used internally to override / extend certain behavior
 * NOTE: Make sure values don't collide with otm_hdmi_attribute_flag_t
 */
#define OTM_HDMI_ATTR_FLAG_FORCED 0x8000 /* Read only is ignored */

/**
    Attribute types
*/
typedef enum {
	OTM_HDMI_ATTR_TYPE_UINT, /* Attribute is of type #unsigned int. */
	OTM_HDMI_ATTR_TYPE_BOOLEAN, /* Attribute is of type #bool. */
	OTM_HDMI_ATTR_TYPE_STRING /* Attribute is a read-only 0-terminated
					ASCII string. */
} otm_hdmi_attribute_type_t;

/**
   Maximum size of PD string attributes
*/
#define OTM_HDMI_MAX_STRING_LENGTH 16

/**
     This structure represents port driver attribute
*/
typedef struct {
	otm_hdmi_attribute_id_t id;	/* Global attribute ID. */
	otm_hdmi_attribute_type_t type;	/* Data type of attribute. */
	otm_hdmi_attribute_flag_t flags;/* Access permissions and
						internal use*/

	char name[OTM_HDMI_MAX_STRING_LENGTH + 1];

	union	/* Attribute data dependent on attribute type */
	{
		struct {
			unsigned int value_default; /* default value */
			unsigned int value_min;	/* minimum value */
			unsigned int value_max;	/* maximum value */
			unsigned int value;	/* current value */
		} _uint;

		struct {
			bool value_default; /* default value */
			bool value; /* current value */
		} _bool;

		struct {
			/* current value */
			char value[OTM_HDMI_MAX_STRING_LENGTH + 1];
		} string;

	} content;
} otm_hdmi_attribute_t;

/**
    The Internal TV Encoders can support several different TV standards when
    they are used in Standard Definition (SD) resolutions.  The entries in
    this enumeration are values that can be used to set the
    OTM_HDMI_ATR_ID_SD_OPTION attribute to specify the standard to be used
    for SD.
*/
typedef enum {
	TV_STD_UNDEFINED = 0, /* Use Default per resolution */
	TV_STD_NTSC = 0, /* Use NTSC for 720x480i mode. */
	TV_STD_PAL = 0,	 /* Use PAL for 720x576i mode. */
	TV_STD_NTSC_J = 1, /* Use NTSC-J (Japan) for 720x480i mode. */
	TV_STD_PAL_M = 2, /* Use PAL-M (Brazil) for 720x480i mode. */
	TV_STD_PAL_N = 3, /* Use PAL-N (Argentina) for 720x576i mode. */
	TV_STD_MAX /* The number of IDs in this enumeration. */
} otm_hdmi_sd_option_t;

/*
 * Unique IDs for [end user -> port driver] communication
 */
/*
 * Command codes for the otm_hdmi_port_send() function.
 */
typedef enum {
	OTM_HDMI_SEND_CC = 0,
	/* Closed Captioning data; see #otm_hdmi_cc_data_t */
	OTM_HDMI_SEND_PAL_WSS,
	/* Pal Wide Screen signaling; See #otm_hdmi_wss_data_t */
	OTM_HDMI_SEND_CGMS_A,
	/* CGMS-A for NTSC and ATSC formats; See #otm_hdmi_cgms_data_t */
	OTM_HDMI_SEND_CGMS_A_TYPE_B,
	/* CGMS-A for CEA 770.2 and 770.3; see #otm_hdmi_cgms_type_b_data_t */
	OTM_HDMI_SEND_HDMI_AUDIO_CTRL,
	/* HDMI audio data; See #otm_hdmi_audio_ctrl_t */
	OTM_HDMI_SEND_HDMI_HDCP_SRM,
	/* HDCP System Renewability Message */
	OTM_HDMI_SEND_HDMI_PACKET,
	/* Generic HDMI packet; See #otm_hdmi_packet_info_t */
	OTM_HDMI_SEND_HDMI_PHY,
	/* Set HDMI PHY electrical properties */
	OTM_HDMI_SEND_TELETEXT,
	/* Teletext data to be re inserted into the vbi */
	OTM_HDMI_SEND_USER_MIN,
	/* External (non-Intel) port drivers may define command codes
		starting with this value.
	*/
	OTM_HDMI_SEND_USER_MAX = 30
	/* External (non-Intel) port drivers may define command codes up to
		this value.
	*/
} otm_hdmi_otm_hdmi_send_t;

/* Unique IDs for [port driver -> end user] communication */
/**
    Command codes for retrieving port driver extended information
    via otm_hdmi_port_recv().
*/
typedef enum {
	OTM_HDMI_RECV_HDMI_AUDIO_CTRL = 0, /* Audio control information
					see #otm_hdmi_audio_ctrl_t       */
	OTM_HDMI_RECV_HDMI_SINK_INFO,  /* HDMI sink information
					see #otm_hdmi_sink_info_t        */
	OTM_HDMI_RECV_HDMI_EDID_BLOCK, /* 128 bytes of raw EDID
					see #otm_hdmi_edid_block_t       */
	OTM_HDMI_RECV_HDMI_HDCP_INFO,  /* HDCP information        */
	OTM_HDMI_RECV_HDMI_HDCP_KSVS,  /* HDCP keys selection vectors      */
	OTM_HDMI_RECV_HDMI_PHY,	     /* PHY settings                     */
	OTM_HDMI_RECV_HDMI_NATIVE_MODE,/* Native modes query               */
	OTM_HDMI_RECV_USER_MIN,	     /* Begin user defined command codes */
	OTM_HDMI_RECV_USER_MAX = 30    /* End user defined command codes   */
} otm_hdmi_otm_hdmi_receive_t;

/* Output pixel format */
/**
    Attribute values for the HDMI output pixel format.
    See #OTM_HDMI_ATTR_ID_PIXEL_FORMAT_OUTPUT.
*/
typedef enum {
	OTM_HDMI_OPF_RGB444 = 0,	/* RGB 4:4:4 Output */
	OTM_HDMI_OPF_YUV422,	/* YUV 4:2:2 Output */
	OTM_HDMI_OPF_YUV444,	/* YUV 4:4:4 Output */
	OTM_HDMI_OPF_COUNT	/* Number of output pixel formats + 1 */
} otm_hdmi_output_pixel_format_t;

/* Output pixel depth */
/**
    Attribute values for the HDMI output pixel depth.
    See #OTM_HDMI_ATTR_ID_PIXEL_DEPTH.
*/
typedef enum {
	OTM_HDMI_OPD_24BIT = 0,	/* 24 bits per pixel */
	OTM_HDMI_OPD_30BIT,	/* 30 bits per pixel */
	OTM_HDMI_OPD_36BIT,	/* 36 bits per pixel */
	OTM_HDMI_PIXEL_DEPTH_COUNT /* Number of supported pixel depths + 1 */
} otm_hdmi_output_pixel_depth_t;

/* Picture Aspect Ratio infoframe code */
/**
    Attribute values for the HDMI Picture Aspect Ratio information sent via
    AVI infoframes. See #OTM_HDMI_ATTR_ID_PAR .
*/
typedef enum {
	OTM_HDMI_PAR_NO_DATA = 0x00,	/* No aspect ratio specified */
	OTM_HDMI_PAR_4_3 = 0x01,	/* 4:3 aspect ratio */
	OTM_HDMI_PAR_16_9 = 0x02,	/* 16:9 aspect ratio */
} otm_hdmi_par_t;

/* Format Aspect Ratio infoframe code */
/**
    Attribute values for the HDMI Format Aspect Ratio information sent via
    AVI infoframes. See #OTM_HDMI_ATTR_ID_FAR.
*/
typedef enum {
	OTM_HDMI_FAR_16_9_TOP = 0x02, /* box 16:9 (top) */
	OTM_HDMI_FAR_14_9_TOP = 0x03, /* box 14:9 (top) */
	OTM_HDMI_FAR_G_14_9_CENTER = 0x04, /* box > 16:9 (center) */
	OTM_HDMI_FAR_SAME_AS_PAR = 0x08, /* As encoded frame */
	OTM_HDMI_FAR_4_3_CENTER = 0x09,	/* 4:3 center */
	OTM_HDMI_FAR_16_9_CENTER = 0x0A, /* 16:9 center */
	OTM_HDMI_FAR_14_9_CENTER = 0x0B, /* 14:9 center */
	OTM_HDMI_FAR_4_3_SP_14_9 = 0x0D, /* 4:3 with s&p 14:9 center */
	OTM_HDMI_FAR_16_9_SP_14_9 = 0x0E, /* 16:9 with s&p 14:9 center */
	OTM_HDMI_FAR_16_9_SP_4_3 = 0x0F, /* 4:3 with s&p 4:3 center */
} otm_hdmi_far_t;

/* V B I   S E R V I C E S */
/**
    When inserting VBI information into the analog TV signal, this enumeration
    is used to indicate the field into which the information should be inserted.
*/
typedef enum {
	VBI_FIELD_ID_ODD = 1,	/* Odd field (field 1).   */
	VBI_FIELD_ID_EVEN = 2,	/* Even field (field 2).  */
	VBI_FIELD_ID_UNDEFINED = 3
				/* This value should be passed when the
				display is in a progressive (frame) mode.
				*/
} otm_hdmi_vbi_fieldid_t;

/**

    This enumeration is used to specify values for the #OTM_HDMI_ATTR_ID_ACP
    attribute (the Analog Copy Protection mode).
*/
typedef enum {
	ACP_MODE_OFF,		/* ACP Off */
	ACP_MODE_PSP,		/* Pseudo Sync Pulse+No Color Stripes */
	ACP_MODE_PSP_CS_2_LINES, /* Pseudo Sync Pulse+Color Stripes (2 lines)*/
	ACP_MODE_PSP_CS_4_LINES, /* Pseudo Sync Pulse+Color Stripes (4 lines)*/
	ACP_MODE_PSP_ALT, /* Pseudo Sync Pulse + No Color Stripes */
	/* Type 1 Supplement 1 BPP configuration */
	ACP_MODE_PSP_CS_2_LINES_ALT,	/* Pseudo Sync Pulse +
						Color Stripes (2 lines) */
	/* Type 2 Supplement 1 BPP configuration */
	ACP_MODE_PSP_CS_4_LINES_ALT	/* Pseudo Sync Pulse +
						Color Stripes (4 lines) */
	    /* Type 3 Supplement 1 BPP configuration */
} otm_hdmi_acp_mode_t;

/**
    This enumeration specifies values for CGMS-A copy permission states to be
    inserted into the analog TV signal. See the #otm_hdmi_cgms_data_t
    data structure.
*/
typedef enum {
	CGMS_A_COPY_FREELY = 1,	/* Unlimited Copies can be made */
	CGMS_A_COPY_NOMORE = 2,	/* Copy has already been made (was reserved) */
	CGMS_A_COPY_ONCE = 3,	/* One copy can be made */
	CGMS_A_COPY_NEVER = 4,	/* No copies can be made */
	CGMS_A_NO_DATA = 5	/* No data. Word 1 will be 1111 */
} otm_hdmi_cgms_copy_t;

/**

    This enumeration specifies values for CGMS-A aspect ratios to be inserted
    into the analog TV signal. See the #otm_hdmi_cgms_data_t data structure.
*/
typedef enum {
	CGMS_A_4_3 = 1,	 /* Normal 4:3 aspect ratio */
	CGMS_A_4_3_LB = 2, /* 4:3 aspect ratio letterboxed */
	CGMS_A_16_9 = 3	 /* 16:9 aspect ratio (Not available at 480i/576i) */
} otm_hdmi_cgms_aspect_t;

/**

    This enumeration specifies values for CGMS-A aspect ratios to be inserted
    into the analog TV signal. See the #otm_hdmi_cgms_data_t data structure.
*/
typedef enum {
	CGMS_A_SCAN_NODATA = 0,	/* No Data */
	CGMS_A_SCAN_OVER = 1,	/* Overscanned */
	CGMS_A_SCAN_UNDER = 2,	/* Underscanned */
	CGMS_A_SCAN_RESERVED = 3
} otm_hdmi_cgms_scan_t;

/**

    This enumeration specifies values for CGMS-A aspect ratios to be inserted
    into the analog TV signal. See the #otm_hdmi_cgms_data_t data structure.
*/
typedef enum {
	CGMS_A_CSC_NO_DATA = 0,	/* No Data */
	CGMS_A_CSC_BT601 = 1,	/* BT601 */
	CGMS_A_CSC_BT709 = 2,	/* BT709 */
	CGMS_A_CSC_RESERVED = 3	/* Reserved */
} otm_hdmi_cgms_colorimetery;

/**
    This enumeration specifies values for Wide Screen Signalling aspect ration
    information to be inserted into the analog TV signal. See the

*/
typedef enum {
	/* PAL specific Modes */
	WSS_4_3_FF = 0,		/* 4:3 Full Format */
	WSS_14_9_LB_C = 1,	/* 14:9 Letterbox, Centered */
	WSS_14_9_LB_T = 2,	/* 14:9 Letterbox, Top */
	WSS_16_9_LB_C = 3,	/* 16:9 Letterbox, Centered */
	WSS_16_9_LB_T = 4,	/* 16:9 Letterbox, Top */
	WSS_G_16_9_LB_C = 5,	/* 16:9 Letterbox, Centered */
	WSS_14_9_FF = 6,	/* 14:9 Full Format */
	WSS_16_9_ANAMORPHIC = 7, /*16:9 Anamorphic */
} otm_hdmi_wss_aspect_t;

/**
    This enumeration specifies values for Wide Screen Signalling camera mode
    information to be inserted into the analog TV signal. See the

*/
typedef enum {
	WSS_CAMERA_MODE = 0,	/* Camera Mode */
	WSS_FILM_MODE = 1,	/* Film Mode */
} otm_hdmi_wss_camera_t;

/**
    This enumeration specifies values for Wide Screen Signalling color encoding
    information to be inserted into the analog TV signal. See the

*/
typedef enum {
	WSS_CE_NORMAL_PAL = 10,	/* Normal PAL Colors */
	WSS_CE_COLOR_PLUS = 11,	/* Motion Adaptive Color Plus */
} otm_hdmi_wss_ce_t;

/**
    This enumeration specifies values to indicate  Wide Screen Signalling
    helpers state, to be inserted into the analog TV signal. See the

*/
typedef enum {
	WSS_HELPERS_NOT_PRESENT = 1,	/* No Helper */
	WSS_HELPERS_PRESENT = 2,	/* Modulated helper */

} otm_hdmi_wss_helpers_t;

/**
    This enumeration specifies values for Wide Screen Signalling open subtitles
    state, to be inserted into the analog TV signal.
    See the #otm_hdmi_wss_data_t data structure.
*/
typedef enum {
	WSS_OPEN_SUBTITLES_NO = 1,	/* No open subtitles */
	WSS_OPEN_SUBTITLES_INSIDE = 2,	/* Subtitles in active image area*/
	WSS_OPEN_SUBTITLES_OUTSIDE = 3,	/* Subtitles out of active image area*/
} otm_hdmi_wss_opensub_t;

/**
    This enumeration specifies values for Wide Screen Signalling surround sound
    state, to be inserted into the analog TV signal.
    See the #otm_hdmi_wss_data_t data structure.
*/
typedef enum {
	WSS_SURROUND_NO = 1,	/* No surround sound information */
	WSS_SURROUND_YES = 2,	/* Surround sound present */
} otm_hdmi_wss_surround_t;

/**
    This enumeration contains the data type identifier for the WSS information
    to pass to the TV encoder to be inserted into the analog TV signal.
*/
typedef enum {
	WSS_NO_COPYRIGHT = 1,	/* No Copyright asserted or status unknown */
	WSS_COPYRIGHT = 2,	/* Copyright Asserted */
} otm_hdmi_wss_copyright_t;

/**
    This enumeration specifies values for Wide Screen Signalling copy
    restriction state, to be inserted into the analog TV signal. See the

*/
typedef enum {
	WSS_COPY_NO_REST = 1,	/* Copying not restricted */
	WSS_COPY_RESTRICTED = 2	/* Copying Restricted */
} otm_hdmi_wss_copy_t;

/**

    This data structure is used to pass closed captioning information to the
    display driver.  The driver will pass this information to the TV encoder to
    be inserted into the analog TV signal.
*/
typedef struct {
	otm_hdmi_vbi_fieldid_t pd_vbi_field_id; /* Field ID identifier */
	unsigned char data_length;
				/* Number of valid closed caption data bytes
				passed; must be an even number, with a
				maximum value of 8.
				*/
	unsigned char ccdata[8];
				/* Array containing the closed caption data
				to be inserted.
				*/
} otm_hdmi_cc_data_t;

/**
    This data structure is used to pass PAL Wide Screen signaling from an
    application to the display driver.  The driver will pass this
    information to the TV encoder to be inserted into the PAL analog TV signal.

    Teletext is not supported in silicon. Teletext in subtitle always is 0.

    Standard in use:
    ETSI EN 300 294 V1.4.1 2003-2004
*/
typedef struct {
	bool enabled; /* OTM_HDMI_TRUE => Enabled */
	otm_hdmi_wss_aspect_t aspect; /* Aspect Ratio */
	otm_hdmi_wss_camera_t cam_mode;	/* Camera Mode */
	otm_hdmi_wss_ce_t color_enc; /* Color Encoding */
	otm_hdmi_wss_helpers_t helpers;	/* Helpers Present */
	otm_hdmi_wss_opensub_t open_sub; /* Open Subtitles */
	otm_hdmi_wss_surround_t surround; /* Surround sound */
	otm_hdmi_wss_copyright_t copyright; /* Copyright assertion */
	otm_hdmi_wss_copy_t copy_rest;	/* Copy Restriction */
} otm_hdmi_wss_data_t;

/**
    This data structure is used to pass Copy Generation Management System
    (Analog) information from the application to the display driver.  The driver
    will pass this information to the TV encoder to be inserted into the analog
    TV signal.

    XDS CEA-608 based CGMS-A should be passed using the Closed Captioning API.
    See #otm_hdmi_cc_data_t

    Standard is use: IEC 61880 480i Line20, EIA/CEA-805 480p Line 41,
			720p Line 24 , and 1080i Line 19
*/
typedef struct {
	bool enabled; /* OTM_HDMI_TRUE => Enabled */
	otm_hdmi_cgms_copy_t copyGen; /* CGMS-A data see #otm_hdmi_cgms_copy_t*/
	otm_hdmi_cgms_aspect_t aspect; /* Wide Screen signaling. */
	otm_hdmi_acp_mode_t mv;	/* APS */
	bool analog_src; /* Analog Source Bit */
} otm_hdmi_cgms_data_t;

/**
    This enumeration is used to specify values for the #OTM_HDMI_ATTR_ID_SVIDEO
    attribute
*/
typedef enum {
	OTM_HDMI_TVOUT_TYPE_COMPOSITE, /* Composite only */
	OTM_HDMI_TVOUT_TYPE_SVIDEO, /* S-Video only */
	OTM_HDMI_TVOUT_TYPE_COMPONENT, /* Reserved for internal use */
	OTM_HDMI_TVOUT_TYPE_CVBSSV, /* Composite and S-video */
} otm_hdmi_tvout_type_t;

/**

*   This data structure is used to pass Copy Generation
*   Management System (Analog) Type B information from the
*   application to the display driver.  The driver will pass
*   this information to the TV encoder to be inserted into the
*   analog TV signal.

    XDS CEA-608 based CGMS-A should be passed using the Closed Captioning API.
    See #otm_hdmi_cc_data_t

    Standard is use: EIA/CEA-805 480p Line 40,
			720p Line 23 , and 1080i Line 18, 581
*/
typedef struct {
	bool enabled;	/* OTM_HDMI_TRUE => Enabled */
	otm_hdmi_cgms_aspect_t aspect;	/* Wide Screen signaling IEC 61880. */
	bool analog_src;	/* Analog Source Bit */
	bool activeFormatValid;
	bool barDataValid;
	otm_hdmi_cgms_scan_t scanData;
	otm_hdmi_cgms_colorimetery colorimetery;
	unsigned char activeFormat;
	bool RCI;
	otm_hdmi_acp_mode_t mv;	/* APS */
	otm_hdmi_cgms_copy_t copyGen;	/* CGMS-A data
					see #otm_hdmi_cgms_copy_t */
	unsigned short lineEndTop;
	unsigned short lineStartBottom;
	unsigned short pixelEndLeft;
	unsigned short pixelStartRight;
} otm_hdmi_cgms_type_b_data_t;

/* Defines the size of the Teletext data packet. */
#define TELETEXT_NUM_LINES 32
#define TELETEXT_LINE_LENGTH 46

/**

*   This data structure is used as part of otm_hdmi_teletext_data_t
*   structure to more easily fill out the PES header.
*
*   The packet structure is for a single Teletext line. This is
*   a convenience structure that describes the PES data header
*   defined in section 4.3 ETSI EN 300-472.
*
*   Only data_unit_id's of Teletext Data (0x02) and Teletext
*   subtitles (0x03) will be processed by the driver. Stuffing
*   (0xFF) will be ignored. A value of 0x00 will generate a
*   warning.
*
*   Line numbers with a field id of 0 will have 313 added.
*   Line numbers outside the VBI will generate a warning.
*/
typedef struct {
	unsigned char type:8;	/* Data unit ID */
	unsigned char length:8;	/* Length is required to be 44. */
	unsigned char line:5;	/* VBI line */
	unsigned char field:1;	/* VBI Field, 1 is bottom field */
	unsigned char:2;	/* padding */
	unsigned char framing_code:8;	/* Framing Code is required
						to be 0xE4. */
	unsigned char data[42];	/* Teletext data */
} teletext_packet_t;

/**

*   This data structure is used to pass Teletext and Subtitle
*   information to be reinserted into the VBI of the PAL signal
*
*   The packet structure contains up to two field's teletext
*   lines with a 4-byte PES data header as defined in section
*   4.3 ETSI EN 300-472.
*
*   Only data_unit_id's of Teletext Data (0x02) and Teletext
*   subtitles (0x03) will be processed by the driver. Stuffing
*   (0xFF) will be ignored. A value of 0x00 will generate a
*   warning.
*
*   Line numbers with a field id of 0 will have 313 added.
*   Line numbers outside the VBI will generate a warning.
*/
typedef struct {
	bool enabled;	/* OTM_HDMI_TRUE => Enabled */
	union {
		teletext_packet_t packet[TELETEXT_NUM_LINES];
		unsigned char
		 raw_data[TELETEXT_NUM_LINES][TELETEXT_LINE_LENGTH];
	};
} otm_hdmi_teletext_data_t;

/* H D M I   S P E C I F I C   D A T A   T Y P E S */

/**
   This structure defines the HDMI audio data blocks.
*/
typedef struct {
	unsigned int format;
	unsigned int max_channels;
	unsigned int fs;
	unsigned int ss_bitrate;
} otm_hdmi_audio_cap_t;

/**
    A CEC Source Physical Address.
*/
typedef struct {
	unsigned char a;
	unsigned char b;
	unsigned char c;
	unsigned char d;
} otm_hdmi_src_phys_addr_t;

/**
    This data structure represents additional sink details not-available through
    port attributes
*/
typedef struct {
	unsigned short manufac_id; /* Sink manufacturer ID */
	unsigned short product_code; /* Sink product code */
	bool hdmi; /* Sink is HDMI */
	bool ycbcr444; /* Sink supports YCbCr444 */
	bool ycbcr422; /* Sink supports YCbCr422 */
	otm_hdmi_src_phys_addr_t spa; /* CEC source physical address a.b.c.d*/
	unsigned int speaker_map; /* Speaker allocation map */
	bool dc_30; /* Sink supports 30-bit color */
	bool dc_36; /* Sink supports 36-bit color */
	bool dc_y444; /* Sink supports YCbCr444 in supported DC modes*/
	bool xvycc601; /* Sink supports xvYCC BT601 Colorimetry */
	bool xvycc709; /* Sink supports xvYCC BT709 Colorimetry */
	bool supports_ai; /* Sink supports aux audio information */
	unsigned int max_tmds_clock; /* Sink MAX TMDS clock in MHz. */
	bool latency_present; /* Sink lipsync data valid */
	bool latency_int_present; /* Sink Interlaced lipsync
						data valid */
	bool hdmi_video_present; /* Sink Supports HDMI spec.
						video modes */
	int latency_video; /* progressive modes video latency */
	int latency_audio; /* progressive modes audio latency */
	int latency_video_interlaced; /* interlaced modes video latency */
	int latency_audio_interlaced; /* interlaced modes audio latency */
	bool enabled_3d; /* Sink supports 3D video modes */
	bool num_modes_3d; /* DEPRECATED; */
	unsigned char max_horz_image_size; /* Sink's screen size in cm */
	unsigned char max_vert_image_size; /* Sink's screen size in cm */
	unsigned char name[14];	/* Sink's name */
	bool rgb_quant_full; /* RGB quantization mode selectable */
	bool ycc_quant_full; /* YCC quantization mode selectable */
} otm_hdmi_sink_info_t;

/**
    This data structure represents 128 byte EDID block
*/
typedef struct {
	unsigned char index; /* Block number to read */
	unsigned char data[128]; /* Block contents */
} otm_hdmi_edid_block_t;

/**
    This enumeration defines the command IDs for the HDMI audio commands.
    See #otm_hdmi_audio_ctrl_t.
*/
typedef enum {
	OTM_HDMI_AUDIO_START, /* Start audio playback */
	OTM_HDMI_AUDIO_STOP, /* Stop audio playback */
	OTM_HDMI_AUDIO_SET_FORMAT, /* Set audio format */
	OTM_HDMI_AUDIO_GET_CAPS, /* Retrieve descriptor of audio blocks */
	OTM_HDMI_AUDIO_WRITE, /* For driver internal use only */
	OTM_HDMI_AUDIO_SET_CHANNEL_STATUS, /* Set channel status info */
	OTM_HDMI_AUDIO_GET_CHANNEL_STATUS, /* Get channel status info */
} otm_hdmi_audio_cmd_id_t;

/**
    This enumeration defines IDs for different HDMI audio formats.
*/
/* IMPORTANT: DO NOT change order!!! */
typedef enum {
	OTM_HDMI_AUDIO_FORMAT_UNDEFINED = 0x00,
	OTM_HDMI_AUDIO_FORMAT_PCM,
	OTM_HDMI_AUDIO_FORMAT_AC3,
	OTM_HDMI_AUDIO_FORMAT_MPEG1,
	OTM_HDMI_AUDIO_FORMAT_MP3,
	OTM_HDMI_AUDIO_FORMAT_MPEG2,
	OTM_HDMI_AUDIO_FORMAT_AAC,
	OTM_HDMI_AUDIO_FORMAT_DTS,
	OTM_HDMI_AUDIO_FORMAT_ATRAC,
	OTM_HDMI_AUDIO_FORMAT_OBA,
	OTM_HDMI_AUDIO_FORMAT_DDP,
	OTM_HDMI_AUDIO_FORMAT_DTSHD,
	OTM_HDMI_AUDIO_FORMAT_MLP,
	OTM_HDMI_AUDIO_FORMAT_DST,
	OTM_HDMI_AUDIO_FORMAT_WMA_PRO,
} otm_hdmi_audio_fmt_t;

/**
    This enumeration defines IDs for different HDMI audio sampling frequencies.
*/
typedef enum {
	OTM_HDMI_AUDIO_FS_32_KHZ = 0x01,
	OTM_HDMI_AUDIO_FS_44_1_KHZ = 0x02,
	OTM_HDMI_AUDIO_FS_48_KHZ = 0x04,
	OTM_HDMI_AUDIO_FS_88_2_KHZ = 0x08,
	OTM_HDMI_AUDIO_FS_96_KHZ = 0x10,
	OTM_HDMI_AUDIO_FS_176_4_KHZ = 0x20,
	OTM_HDMI_AUDIO_FS_192_KHZ = 0x40,
} otm_hdmi_audio_fs_t;

/**
    This enumeration defines IDs for different HDMI audio sample sizes.
*/
typedef enum {
	OTM_HDMI_AUDIO_SS_UNDEFINED = 0x00,	/* Undefined value */
	OTM_HDMI_AUDIO_SS_16 = 0x01,	/* 16 bits */
	OTM_HDMI_AUDIO_SS_20 = 0x02,	/* 20 bits */
	OTM_HDMI_AUDIO_SS_24 = 0x04,	/* 24 bits */
} otm_hdmi_audio_ss_t;

/**
    Enumeration of the different audio speaker allocation options defined in the
    CEA-861D specification.
*/
typedef enum {
	OTM_HDMI_AUDIO_SPEAKER_MAP_FLFR = 0x0001,
	OTM_HDMI_AUDIO_SPEAKER_MAP_LFE = 0x0002,
	OTM_HDMI_AUDIO_SPEAKER_MAP_FC = 0x0004,
	OTM_HDMI_AUDIO_SPEAKER_MAP_RLRR = 0x0008,
	OTM_HDMI_AUDIO_SPEAKER_MAP_RC = 0x0010,
	OTM_HDMI_AUDIO_SPEAKER_MAP_FLCFRC = 0x0020,
	OTM_HDMI_AUDIO_SPEAKER_MAP_RLCRRC = 0x0040,
	OTM_HDMI_AUDIO_SPEAKER_MAP_FLWFRW = 0x0080,
	OTM_HDMI_AUDIO_SPEAKER_MAP_FLHFRH = 0x0100,
	OTM_HDMI_AUDIO_SPEAKER_MAP_TC = 0x0200,
	OTM_HDMI_AUDIO_SPEAKER_MAP_FCH = 0x0400,
} otm_hdmi_audio_speaker_map_t;

/**
    This structure represents different audio commands
*/
typedef struct {
	otm_hdmi_audio_cmd_id_t cmd_id;	/* Audio command type */

	union /* Audio command details */
	{
		struct	/* Arguments for #OTM_HDMI_AUDIO_SET_FORMAT command. */
		{
			otm_hdmi_audio_fmt_t fmt; /* Audio format */
			otm_hdmi_audio_fs_t fs;	/* Sampling frequency */
			unsigned int ch; /* Number of channels */
			otm_hdmi_audio_ss_t ss;	/* Sample size [in bits] */
			otm_hdmi_audio_speaker_map_t map; /* Speaker allocation
								map */
		} _set_config;

		struct /* Arguments for #OTM_HDMI_AUDIO_GET_CAPS command. */
		{
			unsigned int index; /* Capability number */
			otm_hdmi_audio_cap_t cap; /* Capability content */
		} _get_caps;

		struct /* Arguments for #OTM_HDMI_AUDIO_WRITE command */
		{
			unsigned int samples; /* Audio samples buffer address*/
			unsigned int silence; /* Audio silence buffer address*/
			unsigned int size; /* Audio data buffer size */
			unsigned int id; /* Audio buffer ID */
			bool sync; /* Type of write operation */
		} _write;

		struct /* Arguments for #OTM_HDMI_AUDIO_STOP command */
		{
			bool sync; /* Type of stop request */
		} _stop;

		struct /* Arguments for
			#OTM_HDMI_AUDIO_SET_CHANNEL_STATUS command */
		{
			unsigned int lsw; /* Least significant word of
						ch status */
			unsigned int msw; /* Most significant word of
						ch status */
		} _channel_status;

	} data;

} otm_hdmi_audio_ctrl_t;

#define HDMI_DIP_PACKET_HEADER_LEN	3
#define HDMI_DIP_PACKET_DATA_LEN	28

/**
    This structure represents generic HDMI packet
*/
typedef struct {
	uint8_t header[HDMI_DIP_PACKET_HEADER_LEN];
	union {
		uint8_t data[HDMI_DIP_PACKET_DATA_LEN];
		uint32_t data32[HDMI_DIP_PACKET_DATA_LEN/4];
	};
} otm_hdmi_packet_t;

/**
    This structure represents HDMI packet slot number
*/
typedef enum {
	OTM_HDMI_PACKET_SLOT_0,
	OTM_HDMI_PACKET_SLOT_1,
	OTM_HDMI_PACKET_SLOT_AVI,
} otm_hdmi_packet_slot_t;

/**
    This structure is used to submit data via #OTM_HDMI_SEND_HDMI_PACKET service
    provided by #otm_hdmi_port_send
*/
typedef struct {
	otm_hdmi_packet_t packet;
	otm_hdmi_packet_slot_t slot;
} otm_hdmi_packet_info_t;

/**
    This enumeration is used to specify values for the
    #OTM_HDMI_ATTR_ID_USE_EDID attribute
*/
typedef enum {
	OTM_HDMI_USE_EDID_NONE = 0,
	OTM_HDMI_USE_EDID_REAL = 1,
	OTM_HDMI_USE_EDID_SAFE = 2,
} otm_hdmi_use_edid_t;

/**
*   This enumeration represents YC Delay amounts
*/
typedef enum {
	OTM_HDMI_YC_DELAY_NONE,	/* No YC delay */
	OTM_HDMI_YC_DELAY_ADVANCE, /* Y 0.5 Pixel Advance delay */
	OTM_HDMI_YC_DELAY_MINUS	/* Y 1.0 Pixel delay */
} otm_hdmi_yc_delay_t;

/**
*   This enumeration represents vswing equalization values
*/
typedef enum {
	OTM_HDMI_EQUALIZE_NONE,	/* Equalization disabled */
	OTM_HDMI_EQUALIZE_10,	/* Equalization 10%, not supported on CE3100 */
	OTM_HDMI_EQUALIZE_20,	/* Equalization 20% */
	OTM_HDMI_EQUALIZE_30,	/* Equalization 30%, not supported on CE3100 */
	OTM_HDMI_EQUALIZE_40,	/* Equalization 40% */
	OTM_HDMI_EQUALIZE_50,	/* Equalization 50%, not supported on CE3100 */
	OTM_HDMI_EQUALIZE_60,	/* Equalization 60% */
	OTM_HDMI_EQUALIZE_70,	/* Equalization 70%, not supported on CE3100 */
	OTM_HDMI_EQUALIZE_80,	/* Equalization 80% */
} otm_hdmi_equalize_t;

/**
*   This enumeration represents transmit level amplitude values
*/
typedef enum {
	OTM_HDMI_TRANSMIT_LEVEL_300, /* 300 mV, not supported on CE3100 */
	OTM_HDMI_TRANSMIT_LEVEL_325, /* 325 mV, not supported on CE3100 */
	OTM_HDMI_TRANSMIT_LEVEL_350, /* 350 mV, not supported on CE3100 */
	OTM_HDMI_TRANSMIT_LEVEL_375, /* 375 mV, not supported on CE3100 */
	OTM_HDMI_TRANSMIT_LEVEL_400, /* 400 mV, not supported on CE3100 */
	OTM_HDMI_TRANSMIT_LEVEL_425, /* 425 mV, not supported on CE3100 */
	OTM_HDMI_TRANSMIT_LEVEL_450, /* 450 mV */
	OTM_HDMI_TRANSMIT_LEVEL_475, /* 475 mV, not supported on CE3100 */
	OTM_HDMI_TRANSMIT_LEVEL_500, /* 500 mV */
	OTM_HDMI_TRANSMIT_LEVEL_525, /* 525 mV, not supported on CE3100 */
	OTM_HDMI_TRANSMIT_LEVEL_550, /* 550 mV */
	OTM_HDMI_TRANSMIT_LEVEL_575, /* 575 mV, not supported on CE3100 */
	OTM_HDMI_TRANSMIT_LEVEL_600, /* 600 mV */
	OTM_HDMI_TRANSMIT_LEVEL_625, /* 625 mV, not supported on CE3100 */
	OTM_HDMI_TRANSMIT_LEVEL_650, /* 650 mV, not supported on CE3100 */
	OTM_HDMI_TRANSMIT_LEVEL_675, /* 675 mV, not supported on CE3100 */
} otm_hdmi_transmit_level_t;

/**
*   This enumeration represents termination impedance values
*/
typedef enum {
	OTM_HDMI_TERMINATION_OPEN, /* Open */
	OTM_HDMI_TERMINATION_677, /* 677 Ohm, not supported on CE3100 */
	OTM_HDMI_TERMINATION_398, /* 398 Ohm, not supported on CE3100 */
	OTM_HDMI_TERMINATION_250, /* 250 Ohm, not supported on CE3100 */
	OTM_HDMI_TERMINATION_200, /* 200 Ohm */
	OTM_HDMI_TERMINATION_100, /* 100 Ohm */
	OTM_HDMI_TERMINATION_88, /* 88 Ohm, not supported on CE3100 */
	OTM_HDMI_TERMINATION_78, /* 78 Ohm, not supported on CE3100 */
	OTM_HDMI_TERMINATION_72, /* 72 Ohm, not supported on CE3100 */
	OTM_HDMI_TERMINATION_67, /* 67 Ohm */
	OTM_HDMI_TERMINATION_65, /* 65 Ohm, not supported on CE3100 */
	OTM_HDMI_TERMINATION_50, /* 50 Ohm */
} otm_hdmi_termination_t;

/**
*   This enumeration represents current adjustment values
*/
typedef enum {
	OTM_HDMI_CURRENT_0MA,	/* 0 mA */
	OTM_HDMI_CURRENT_40MA,	/* 40 mA */
	OTM_HDMI_CURRENT_60MA,	/* 60 mA */
	OTM_HDMI_CURRENT_10MA,	/* 10 mA */
	OTM_HDMI_CURRENT_250MA,	/* 250 mA */
	OTM_HDMI_CURRENT_290MA,	/* 290 mA */
	OTM_HDMI_CURRENT_310MA,	/* 310 mA */
	OTM_HDMI_CURRENT_350MA,	/* 350 mA */
} otm_hdmi_current_t;

/**
*   This enumeration represents band gap resistor values
*/
typedef enum {
	OTM_HDMI_BGLVL_788, /* 0.788v not supported on CE4100 */
	OTM_HDMI_BGLVL_818, /* 0.818v not supported on CE4100 */
	OTM_HDMI_BGLVL_854, /* 0.854v not supported on CE4100
				[CE3100 default] */
	OTM_HDMI_BGLVL_891, /* 0.891v not supported on CE4100 */
	OTM_HDMI_BGLVL_820, /* 0.82v  not supported on CE3100
				[CE4100 default] */
	OTM_HDMI_BGLVL_800, /* 0.80v  not supported on CE3100 */
	OTM_HDMI_BGLVL_780, /* 0.78v  not supported on CE3100 */
	OTM_HDMI_BGLVL_760, /* 0.76v  not supported on CE3100 */
	OTM_HDMI_BGLVL_750, /* 0.75v  not supported on CE3100 */
	OTM_HDMI_BGLVL_720, /* 0.72v  not supported on CE3100 */
	OTM_HDMI_BGLVL_660, /* 0.66v  not supported on CE3100 */
	OTM_HDMI_BGLVL_600, /* 0.60v  not supported on CE3100 */
} otm_hdmi_bglvl_t;

/**
  This structure is used to submit data via #OTM_HDMI_SEND_HDMI_PACKET service
  provided by #otm_hdmi_port_send and is intended for adjustments of PHY clock
  and data lines.
*/
typedef struct {
	otm_hdmi_equalize_t clock_equalization;
				/**< Clock equalization percentage.  On CE3100,
				*   same value is used for both clock and data.
				*/
	otm_hdmi_equalize_t data_equalization;
				/**< Data equalization percentage.
				*   IGNORED ON CE3100
				*/
	otm_hdmi_transmit_level_t clock_transmit_level;
				/**< Clock transmit level in mV.  On CE3100,
				*   same value is used for both clock and data.
				*/
	otm_hdmi_transmit_level_t data_transmit_level;
				/**< Data transmit level in mV.
				*   IGNORED ON CE3100.
				*/
	otm_hdmi_termination_t clock_termination;
				/**< Clock termination in ohms. On CE3100, same
				*   value is used for both clock and data.
				*/
	otm_hdmi_termination_t data_termination;
				/**< Data termination in ohms.
				*   IGNORED ON CE3100.
				*/
	otm_hdmi_current_t clock_current;
				/**< Clock current in mA.  On CE3100, same value
				*   is used for both clock and data.
				*/
	otm_hdmi_current_t data_current;
				/**< Data current in mA.  IGNORED ON CE3100 */
	otm_hdmi_bglvl_t bandgap_level;
				/**< Same value used for both clock and data */
} otm_hdmi_phy_info_t;

/**
*   This enumeration represents different HDCP states
*/
typedef enum {
	OTM_HDMI_HDCP_STATUS_OFF, /* HDCP is disabled */
	OTM_HDMI_HDCP_STATUS_IN_PROGRESS, /* HDCP is enabled but not
						authenticated yet */
	OTM_HDMI_HDCP_STATUS_ON, /* HDCP is enabled and is authenticated */
} otm_hdmi_hdcp_status_t;

/**
*   This enumeration represents audio clock values with respect to which
*   internal audio divisor value is chosen
*/
typedef enum {
	OTM_HDMI_AUDIO_CLOCK_24,	/* Audio clock is running at 24MHz */
	OTM_HDMI_AUDIO_CLOCK_36,	/* Audio clock is running at 36Mhz */
	OTM_HDMI_AUDIO_CLOCK_16,	/* Audio clock is running at 16Mhz */
} otm_hdmi_audio_clock_t;

/**
*   This enumeration is used to specify values for the #OTM_HDMI_ATTR_ID_MUTE
*   attribute
*/
typedef enum {
	OTM_HDMI_MUTE_OFF = 0x0,	/* Unmute */
	OTM_HDMI_MUTE_VIDEO = 0x1,	/* Mute video only */
	OTM_HDMI_MUTE_AUDIO = 0x2,	/* Mute audio only */
	OTM_HDMI_MUTE_BOTH = 0x3,	/* Mute both audio and video */
} otm_hdmi_mute_t;

#endif /* _OTM_HDMI_DEFS_H */
