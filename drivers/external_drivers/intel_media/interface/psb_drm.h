/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 * Copyright (c) 2008, Tungsten Graphics Inc.  Cedar Park, TX., USA.
 * All Rights Reserved.
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
 **************************************************************************/

#ifndef _PSB_DRM_H_
#define _PSB_DRM_H_

#if defined(__linux__) && !defined(__KERNEL__)
#include <stdbool.h>
#include <stdint.h>
#include <linux/types.h>
#include "drm_mode.h"
#endif



/*
 * Menlow/MRST graphics driver package version
 * a.b.c.xxxx
 * a - Product Family: 5 - Linux
 * b - Major Release Version: 0 - non-Gallium (Unbuntu)
 *                            1 - Gallium (Moblin2)
 *                            2 - IMG     (Moblin2)
 *                            3 - IMG     (Meego)
 *                            4 - IMG     (Android)
 * c - Hotfix Release
 * xxxx - Graphics internal build #
 */
#define PSB_PACKAGE_VERSION             "5.6.0.1202"

#define DRM_PSB_SAREA_MAJOR             0
#define DRM_PSB_SAREA_MINOR             2
#define PSB_FIXED_SHIFT                 16

#define PSB_NUM_PIPE                    3

/*
 * Public memory types.
 */

#define DRM_PSB_MEM_MMU                 TTM_PL_PRIV1
#define DRM_PSB_FLAG_MEM_MMU            TTM_PL_FLAG_PRIV1

#define TTM_PL_CI                       TTM_PL_PRIV0
#define TTM_PL_FLAG_CI                  TTM_PL_FLAG_PRIV0
#define TTM_PL_RAR                      TTM_PL_PRIV2
#define TTM_PL_FLAG_RAR                 TTM_PL_FLAG_PRIV2
#define TTM_PL_IMR                      TTM_PL_PRIV2
#define TTM_PL_FLAG_IMR                 TTM_PL_FLAG_PRIV2

#define DRM_PSB_MEM_MMU_TILING          TTM_PL_PRIV3
#define DRM_PSB_FLAG_MEM_MMU_TILING     TTM_PL_FLAG_PRIV3


/*Status of the command sent to the gfx device.*/
typedef enum {
	DRM_CMD_SUCCESS,
	DRM_CMD_FAILED,
	DRM_CMD_HANG
} drm_cmd_status_t;

struct drm_psb_scanout {
	uint32_t buffer_id;	/* DRM buffer object ID */
	uint32_t rotation;	/* Rotation as in RR_rotation definitions */
	uint32_t stride;	/* Buffer stride in bytes */
	uint32_t depth;		/* Buffer depth in bits (NOT) bpp */
	uint32_t width;		/* Buffer width in pixels */
	uint32_t height;	/* Buffer height in lines */
	int32_t transform[3][3];	/* Buffer composite transform */
	/* (scaling, rot, reflect) */
};

#define DRM_PSB_SAREA_OWNERS            16
#define DRM_PSB_SAREA_OWNER_2D          0
#define DRM_PSB_SAREA_OWNER_3D          1
#define DRM_PSB_SAREA_SCANOUTS          3

struct drm_psb_sarea {
	/* Track changes of this data structure */

	uint32_t major;
	uint32_t minor;

	/* Last context to touch part of hw */
	uint32_t ctx_owners[DRM_PSB_SAREA_OWNERS];

	/* Definition of front- and rotated buffers */
	uint32_t num_scanouts;
	struct drm_psb_scanout scanouts[DRM_PSB_SAREA_SCANOUTS];

	int planeA_x;
	int planeA_y;
	int planeA_w;
	int planeA_h;
	int planeB_x;
	int planeB_y;
	int planeB_w;
	int planeB_h;
	/* Number of active scanouts */
	uint32_t num_active_scanouts;
};

#define PSB_RELOC_MAGIC                 0x67676767
#define PSB_RELOC_SHIFT_MASK            0x0000FFFF
#define PSB_RELOC_SHIFT_SHIFT           0
#define PSB_RELOC_ALSHIFT_MASK          0xFFFF0000
#define PSB_RELOC_ALSHIFT_SHIFT         16

/* Offset of the indicated buffer*/
#define PSB_RELOC_OP_OFFSET             0

struct drm_psb_reloc {
	uint32_t reloc_op;
	uint32_t where;		/* offset in destination buffer */
	uint32_t buffer;	/* Buffer reloc applies to */
	uint32_t mask;		/* Destination format: */
	uint32_t shift;		/* Destination format: */
	uint32_t pre_add;	/* Destination format: */
	uint32_t background;	/* Destination add */
	uint32_t dst_buffer;	/* Destination buffer. Index into buffer_list */
	uint32_t arg0;		/* Reloc-op dependant */
	uint32_t arg1;
};


#define PSB_GPU_ACCESS_READ             (1ULL << 32)
#define PSB_GPU_ACCESS_WRITE            (1ULL << 33)
#define PSB_GPU_ACCESS_MASK             (PSB_GPU_ACCESS_READ | PSB_GPU_ACCESS_WRITE)

#define PSB_BO_FLAG_COMMAND             (1ULL << 52)

#define PSB_ENGINE_2D                   2
#define PSB_ENGINE_DECODE               0
#define PSB_ENGINE_VIDEO                0
#define LNC_ENGINE_ENCODE               1
#ifdef MERRIFIELD
#define PSB_NUM_ENGINES                 7
#else
#define PSB_NUM_ENGINES                 2
#endif
#define VSP_ENGINE_VPP                  6

/*
 * For this fence class we have a couple of
 * fence types.
 */

#define _PSB_FENCE_EXE_SHIFT            0
#define _PSB_FENCE_FEEDBACK_SHIFT       4

#define _PSB_FENCE_TYPE_EXE             (1 << _PSB_FENCE_EXE_SHIFT)
#define _PSB_FENCE_TYPE_FEEDBACK        (1 << _PSB_FENCE_FEEDBACK_SHIFT)

#define PSB_FEEDBACK_OP_VISTEST         (1 << 0)

struct drm_psb_extension_rep {
	int32_t exists;
	uint32_t driver_ioctl_offset;
	uint32_t sarea_offset;
	uint32_t major;
	uint32_t minor;
	uint32_t pl;
};

#define DRM_PSB_EXT_NAME_LEN            128

union drm_psb_extension_arg {
	char extension[DRM_PSB_EXT_NAME_LEN];
	struct drm_psb_extension_rep rep;
};

#define PSB_NOT_FENCE                (1 << 0)
#define PSB_MEM_CLFLUSH                (1 << 1)

struct psb_validate_req {
	uint64_t set_flags;
	uint64_t clear_flags;
	uint64_t next;
	uint64_t presumed_gpu_offset;
	uint32_t buffer_handle;
	uint32_t presumed_flags;
	uint32_t pad64;
	uint32_t unfence_flag;
};

struct psb_validate_rep {
	uint64_t gpu_offset;
	uint32_t placement;
	uint32_t fence_type_mask;
};

#define PSB_USE_PRESUMED                (1 << 0)

struct psb_validate_arg {
	uint64_t handled;
	uint64_t ret;
	union {
		struct psb_validate_req req;
		struct psb_validate_rep rep;
	} d;
};


#define DRM_PSB_FENCE_NO_USER           (1 << 0)

struct psb_ttm_fence_rep {
	uint32_t handle;
	uint32_t fence_class;
	uint32_t fence_type;
	uint32_t signaled_types;
	uint32_t error;
};

typedef struct drm_psb_cmdbuf_arg {
	uint64_t buffer_list;	/* List of buffers to validate */
	uint64_t fence_arg;

	uint32_t cmdbuf_handle;	/* 2D Command buffer object or, */
	uint32_t cmdbuf_offset;	/* rasterizer reg-value pairs */
	uint32_t cmdbuf_size;

	uint32_t reloc_handle;	/* Reloc buffer object */
	uint32_t reloc_offset;
	uint32_t num_relocs;

	/* Not implemented yet */
	uint32_t fence_flags;
	uint32_t engine;

} drm_psb_cmdbuf_arg_t;

typedef struct drm_psb_pageflip_arg {
	uint32_t flip_offset;
	uint32_t stride;
} drm_psb_pageflip_arg_t;

typedef enum {
	LNC_VIDEO_DEVICE_INFO,
	LNC_VIDEO_GETPARAM_IMR_INFO,
	LNC_VIDEO_GETPARAM_CI_INFO,
	LNC_VIDEO_FRAME_SKIP,
	IMG_VIDEO_DECODE_STATUS,
	IMG_VIDEO_NEW_CONTEXT,
	IMG_VIDEO_RM_CONTEXT,
	IMG_VIDEO_UPDATE_CONTEXT,
	IMG_VIDEO_MB_ERROR,
	IMG_VIDEO_SET_DISPLAYING_FRAME,
	IMG_VIDEO_GET_DISPLAYING_FRAME,
	IMG_VIDEO_GET_HDMI_STATE,
	IMG_VIDEO_SET_HDMI_STATE,
	PNW_VIDEO_QUERY_ENTRY,
	IMG_DISPLAY_SET_WIDI_EXT_STATE,
	IMG_VIDEO_IED_STATE
} lnc_getparam_key_t;

struct drm_lnc_video_getparam_arg {
	uint64_t key;
	uint64_t arg;	/* argument pointer */
	uint64_t value;	/* feed back pointer */
};

struct drm_video_displaying_frameinfo {
	uint32_t buf_handle;
	uint32_t width;
	uint32_t height;
	uint32_t size; /* buffer size */
	uint32_t format; /* fourcc */
	uint32_t luma_stride; /* luma stride */
	uint32_t chroma_u_stride; /* chroma stride */
	uint32_t chroma_v_stride;
	uint32_t luma_offset; /* luma offset from the beginning of the memory */
	uint32_t chroma_u_offset; /* UV offset from the beginning of the memory */
	uint32_t chroma_v_offset;
	uint32_t reserved;
};

/*
 * Feedback components:
 */

/*
 * Vistest component. The number of these in the feedback buffer
 * equals the number of vistest breakpoints + 1.
 * This is currently the only feedback component.
 */

struct drm_psb_vistest {
	uint32_t vt[8];
};

struct drm_psb_sizes_arg {
	uint32_t ta_mem_size;
	uint32_t mmu_size;
	uint32_t pds_size;
	uint32_t rastgeom_size;
	uint32_t tt_size;
	uint32_t vram_size;
};

struct drm_psb_hist_status_arg {
	uint32_t buf[32];
};

struct drm_psb_dpst_lut_arg {
	uint8_t lut[256];
	int output_id;
};

struct mrst_timing_info {
	uint16_t pixel_clock;
	uint8_t hactive_lo;
	uint8_t hblank_lo;
	uint8_t hblank_hi:4;
	uint8_t hactive_hi:4;
	uint8_t vactive_lo;
	uint8_t vblank_lo;
	uint8_t vblank_hi:4;
	uint8_t vactive_hi:4;
	uint8_t hsync_offset_lo;
	uint8_t hsync_pulse_width_lo;
	uint8_t vsync_pulse_width_lo:4;
	uint8_t vsync_offset_lo:4;
	uint8_t vsync_pulse_width_hi:2;
	uint8_t vsync_offset_hi:2;
	uint8_t hsync_pulse_width_hi:2;
	uint8_t hsync_offset_hi:2;
	uint8_t width_mm_lo;
	uint8_t height_mm_lo;
	uint8_t height_mm_hi:4;
	uint8_t width_mm_hi:4;
	uint8_t hborder;
	uint8_t vborder;
	uint8_t unknown0:1;
	uint8_t hsync_positive:1;
	uint8_t vsync_positive:1;
	uint8_t separate_sync:2;
	uint8_t stereo:1;
	uint8_t unknown6:1;
	uint8_t interlaced:1;
} __attribute__((packed));

struct gct_r10_timing_info {
	uint16_t pixel_clock;
	uint32_t hactive_lo:8;
	uint32_t hactive_hi:4;
	uint32_t hblank_lo:8;
	uint32_t hblank_hi:4;
	uint32_t hsync_offset_lo:8;
	uint16_t hsync_offset_hi:2;
	uint16_t hsync_pulse_width_lo:8;
	uint16_t hsync_pulse_width_hi:2;
	uint16_t hsync_positive:1;
	uint16_t rsvd_1:3;
	uint8_t  vactive_lo:8;
	uint16_t vactive_hi:4;
	uint16_t vblank_lo:8;
	uint16_t vblank_hi:4;
	uint16_t vsync_offset_lo:4;
	uint16_t vsync_offset_hi:2;
	uint16_t vsync_pulse_width_lo:4;
	uint16_t vsync_pulse_width_hi:2;
	uint16_t vsync_positive:1;
	uint16_t rsvd_2:3;
} __attribute__((packed));

struct mrst_panel_descriptor_v1 {
	uint32_t Panel_Port_Control; /* 1 dword, Register 0x61180 if LVDS */
	/* 0x61190 if MIPI */
	uint32_t Panel_Power_On_Sequencing;/*1 dword,Register 0x61208,*/
	uint32_t Panel_Power_Off_Sequencing;/*1 dword,Register 0x6120C,*/
	uint32_t Panel_Power_Cycle_Delay_and_Reference_Divisor;/* 1 dword */
	/* Register 0x61210 */
	struct mrst_timing_info DTD;/*18 bytes, Standard definition */
	uint16_t Panel_Backlight_Inverter_Descriptor;/* 16 bits, as follows */
	/* Bit 0, Frequency, 15 bits,0 - 32767Hz */
	/* Bit 15, Polarity, 1 bit, 0: Normal, 1: Inverted */
	uint16_t Panel_MIPI_Display_Descriptor;
	/*16 bits, Defined as follows: */
	/* if MIPI, 0x0000 if LVDS */
	/* Bit 0, Type, 2 bits, */
	/* 0: Type-1, */
	/* 1: Type-2, */
	/* 2: Type-3, */
	/* 3: Type-4 */
	/* Bit 2, Pixel Format, 4 bits */
	/* Bit0: 16bpp (not supported in LNC), */
	/* Bit1: 18bpp loosely packed, */
	/* Bit2: 18bpp packed, */
	/* Bit3: 24bpp */
	/* Bit 6, Reserved, 2 bits, 00b */
	/* Bit 8, Minimum Supported Frame Rate, 6 bits, 0 - 63Hz */
	/* Bit 14, Reserved, 2 bits, 00b */
} __attribute__((packed));

struct mrst_panel_descriptor_v2 {
	uint32_t Panel_Port_Control; /* 1 dword, Register 0x61180 if LVDS */
	/* 0x61190 if MIPI */
	uint32_t Panel_Power_On_Sequencing;/*1 dword,Register 0x61208,*/
	uint32_t Panel_Power_Off_Sequencing;/*1 dword,Register 0x6120C,*/
	uint8_t Panel_Power_Cycle_Delay_and_Reference_Divisor;/* 1 byte */
	/* Register 0x61210 */
	struct mrst_timing_info DTD;/*18 bytes, Standard definition */
	uint16_t Panel_Backlight_Inverter_Descriptor;/*16 bits, as follows*/
	/*Bit 0, Frequency, 16 bits, 0 - 32767Hz*/
	uint8_t Panel_Initial_Brightness;/* [7:0] 0 - 100% */
	/*Bit 7, Polarity, 1 bit,0: Normal, 1: Inverted*/
	uint16_t Panel_MIPI_Display_Descriptor;
	/*16 bits, Defined as follows: */
	/* if MIPI, 0x0000 if LVDS */
	/* Bit 0, Type, 2 bits, */
	/* 0: Type-1, */
	/* 1: Type-2, */
	/* 2: Type-3, */
	/* 3: Type-4 */
	/* Bit 2, Pixel Format, 4 bits */
	/* Bit0: 16bpp (not supported in LNC), */
	/* Bit1: 18bpp loosely packed, */
	/* Bit2: 18bpp packed, */
	/* Bit3: 24bpp */
	/* Bit 6, Reserved, 2 bits, 00b */
	/* Bit 8, Minimum Supported Frame Rate, 6 bits, 0 - 63Hz */
	/* Bit 14, Reserved, 2 bits, 00b */
} __attribute__((packed));

union mrst_panel_rx {
	struct {
		uint16_t NumberOfLanes:2; /*Num of Lanes, 2 bits,0 = 1 lane,*/
		/* 1 = 2 lanes, 2 = 3 lanes, 3 = 4 lanes. */
		uint16_t MaxLaneFreq:3; /* 0: 100MHz, 1: 200MHz, 2: 300MHz, */
		/*3: 400MHz, 4: 500MHz, 5: 600MHz, 6: 700MHz, 7: 800MHz.*/
		uint16_t SupportedVideoTransferMode:2; /*0: Non-burst only */
		/* 1: Burst and non-burst */
		/* 2/3: Reserved */
		uint16_t HSClkBehavior:1; /*0: Continuous, 1: Non-continuous*/
		uint16_t DuoDisplaySupport:1; /*1 bit,0: No, 1: Yes*/
		uint16_t ECC_ChecksumCapabilities:1; /*1 bit,0: No, 1: Yes*/
		uint16_t BidirectionalCommunication:1; /*1 bit,0: No, 1: Yes */
		uint16_t Rsvd:5; /*5 bits,00000b */
	} panelrx;
	uint16_t panel_receiver;
} __attribute__((packed));

struct gct_ioctl_arg {
	uint8_t bpi; /* boot panel index, number of panel used during boot */
	uint8_t pt; /* panel type, 4 bit field, 0=lvds, 1=mipi */
	struct mrst_timing_info DTD; /* timing info for the selected panel */
	uint32_t Panel_Port_Control;
	uint32_t PP_On_Sequencing;/*1 dword,Register 0x61208,*/
	uint32_t PP_Off_Sequencing;/*1 dword,Register 0x6120C,*/
	uint32_t PP_Cycle_Delay;
	uint16_t Panel_Backlight_Inverter_Descriptor;
	uint16_t Panel_MIPI_Display_Descriptor;
} __attribute__((packed));


struct gct_timing_desc_block {
	uint16_t clock;
	uint16_t hactive:12;
	uint16_t hblank:12;
	uint16_t hsync_start:10;
	uint16_t hsync_end:10;
	uint16_t hsync_polarity:1;
	uint16_t h_reversed:3;
	uint16_t vactive:12;
	uint16_t vblank:12;
	uint16_t vsync_start:6;
	uint16_t vsync_end:6;
	uint16_t vsync_polarity:1;
	uint16_t v_reversed:3;
} __packed;

struct gct_display_desc_block {
	uint8_t type:2;
	uint8_t pxiel_format:4;
	uint8_t mode:2;
	uint8_t frame_rate:6;
	uint8_t reserved:2;
} __attribute__((packed));

struct gct_dsi_desc_block {
	uint8_t lane_count:2;
	uint8_t lane_frequency:3;
	uint8_t transfer_mode:2;
	uint8_t hs_clock_behavior:1;
	uint8_t duo_display_support:1;
	uint8_t ecc_caps:1;
	uint8_t bdirect_support:1;
	uint8_t reversed:5;
} __packed;

struct gct_bkl_desc_block {
	uint16_t frequency;
	uint8_t max_brightness:7;
	uint8_t polarity:1;
} __packed;

struct gct_r20_clock_desc {
	uint8_t pre_divisor:2;
	uint16_t divisor:9;
	uint8_t post_divisor:4;
	uint8_t pll_bypass:1;
	uint8_t cck_clock_divisor:1;
	uint8_t reserved:7;
} __packed;

struct gct_r20_panel_info {
	uint16_t width;
	uint16_t height;
} __packed;

struct gct_r20_panel_mode {
	uint8_t mode:1;
	uint16_t reserved:15;
} __packed;

struct gct_r20_dsi_desc {
	uint8_t num_dsi_lanes:2;
	uint16_t reserved:14;
} __packed;

struct gct_r20_panel_desc {
	uint8_t panel_name[16];
	struct gct_timing_desc_block timing;
	struct gct_r20_clock_desc clock_desc;
	struct gct_r20_panel_info panel_info;
	struct gct_r20_panel_mode panel_mode;
	struct gct_r20_dsi_desc dsi_desc;
	uint32_t early_init_seq;
	uint32_t late_init_seq;
} __packed;

struct gct_r11_panel_desc {
	uint8_t panel_name[16];
	struct gct_timing_desc_block timing;
	struct gct_display_desc_block display;
	struct gct_dsi_desc_block dsi;
	struct gct_bkl_desc_block bkl;
	uint32_t early_init_seq;
	uint32_t late_init_seq;
} __packed;

struct gct_r10_panel_desc {
	struct gct_timing_desc_block timing;
	struct gct_display_desc_block display;
	struct gct_dsi_desc_block dsi;
	struct gct_bkl_desc_block bkl;
	uint32_t early_init_seq;
	uint32_t late_init_seq;
	uint8_t reversed[4];
} __packed;

struct intel_mid_vbt {
	char signature[4];		/*4 bytes,"$GCT" */
	uint8_t revision;		/*1 byte GCT version*/
	uint8_t checksum;		/*1 byte checksum*/
	uint16_t size;			/*2 byte size of checksumed data*/
	uint8_t num_of_panel_desc;	/*1 byte number of panel descriptor*/
	uint8_t primary_panel_idx;	/*1 byte primary panel descriptor idx*/
	uint8_t secondary_panel_idx;	/*1 byte secondary panel desc idx*/
	uint8_t splash_flag;		/*1 byte bit 0 is to disable splash*/
	uint8_t reserved[4];		/*[0..1] relates to GPU burst for R20*/
	void *panel_descs;
} __packed;

struct mrst_vbt {
	char Signature[4];	/*4 bytes,"$GCT" */
	uint8_t Revision;	/*1 byte */
	uint8_t Size;		/*1 byte */
	uint8_t Checksum;	/*1 byte,Calculated */
	void *mrst_gct;
} __attribute__ ((packed));

struct mrst_gct_v1 { /* expect this table to change per customer request*/
	union { /*8 bits,Defined as follows: */
		struct {
			uint8_t PanelType:4; /*4 bits, Bit field for panels*/
			/* 0 - 3: 0 = LVDS, 1 = MIPI*/
			/*2 bits,Specifies which of the*/
			uint8_t BootPanelIndex:2;
			/* 4 panels to use by default*/
			uint8_t BootMIPI_DSI_RxIndex:2; /*Specifies which of*/
			/* the 4 MIPI DSI receivers to use*/
		} PD;
		uint8_t PanelDescriptor;
	};
	struct mrst_panel_descriptor_v1 panel[4];/*panel descrs,38 bytes each*/
	union mrst_panel_rx panelrx[4]; /* panel receivers*/
} __attribute__((packed));

struct mrst_gct_v2 { /* expect this table to change per customer request*/
	union { /*8 bits,Defined as follows: */
		struct {
			uint8_t PanelType:4; /*4 bits, Bit field for panels*/
			/* 0 - 3: 0 = LVDS, 1 = MIPI*/
			/*2 bits,Specifies which of the*/
			uint8_t BootPanelIndex:2;
			/* 4 panels to use by default*/
			uint8_t BootMIPI_DSI_RxIndex:2; /*Specifies which of*/
			/* the 4 MIPI DSI receivers to use*/
		} PD;
		uint8_t PanelDescriptor;
	};
	struct mrst_panel_descriptor_v2 panel[4];/*panel descrs,38 bytes each*/
	union mrst_panel_rx panelrx[4]; /* panel receivers*/
} __attribute__((packed));

#define PSB_DC_CRTC_SAVE                0x01
#define PSB_DC_CRTC_RESTORE             0x02
#define PSB_DC_OUTPUT_SAVE              0x04
#define PSB_DC_OUTPUT_RESTORE           0x08
#define PSB_DC_CRTC_MASK                0x03
#define PSB_DC_OUTPUT_MASK              0x0C

struct drm_psb_dc_state_arg {
	uint32_t flags;
	uint32_t obj_id;
};

struct drm_psb_mode_operation_arg {
	uint32_t obj_id;
	uint16_t operation;
	struct drm_mode_modeinfo mode;
	void *data;
};

struct drm_psb_stolen_memory_arg {
	uint32_t base;
	uint32_t size;
};

/*Display Register Bits*/
#define REGRWBITS_PFIT_CONTROLS         (1 << 0)
#define REGRWBITS_PFIT_AUTOSCALE_RATIOS (1 << 1)
#define REGRWBITS_PFIT_PROGRAMMED_SCALE_RATIOS  (1 << 2)
#define REGRWBITS_PIPEASRC              (1 << 3)
#define REGRWBITS_PIPEBSRC              (1 << 4)
#define REGRWBITS_VTOTAL_A              (1 << 5)
#define REGRWBITS_VTOTAL_B              (1 << 6)
#define REGRWBITS_DSPACNTR              (1 << 8)
#define REGRWBITS_DSPBCNTR              (1 << 9)
#define REGRWBITS_DSPCCNTR              (1 << 10)
#define REGRWBITS_SPRITE_UPDATE         (1 << 11)
#define REGRWBITS_PIPEASTAT             (1 << 12)
#define REGRWBITS_INT_MASK              (1 << 13)
#define REGRWBITS_INT_ENABLE            (1 << 14)
#define REGRWBITS_DISPLAY_ALL           (1 << 15)
/*Overlay Register Bits*/
#define OV_REGRWBITS_OVADD              (1 << 0)
#define OV_REGRWBITS_OGAM_ALL           (1 << 1)

#define OVC_REGRWBITS_OVADD             (1 << 2)
#define OVC_REGRWBITS_OGAM_ALL          (1 << 3)

#define OV_REGRWBITS_WAIT_FLIP          (1 << 4)
#define OVC_REGRWBITS_WAIT_FLIP         (1 << 5)
#define OVSTATUS_REGRBIT_OVR_UPDT       (1 << 6)

/*sprite update fields*/
#define SPRITE_UPDATE_SURFACE           (0x00000001UL)
#define SPRITE_UPDATE_CONTROL           (0x00000002UL)
#define SPRITE_UPDATE_POSITION          (0x00000004UL)
#define SPRITE_UPDATE_SIZE              (0x00000008UL)
#define SPRITE_UPDATE_WAIT_VBLANK       (0X00000010UL)
#define SPRITE_UPDATE_CONSTALPHA        (0x00000020UL)
#define SPRITE_UPDATE_ALL               (0x0000003fUL)

/*vsync operation*/
#define VSYNC_ENABLE                    (1 << 0)
#define VSYNC_DISABLE                   (1 << 1)
#define VSYNC_WAIT                      (1 << 2)
#define GET_VSYNC_COUNT                 (1 << 3)
struct intel_overlay_context {
	uint32_t index;
	uint32_t pipe;
	uint32_t ovadd;
};

struct intel_sprite_context {
	uint32_t update_mask;
	/*plane index 0-A, 1-B, 2-C,etc*/
	uint32_t index;
	uint32_t pipe;

	uint32_t cntr;
	uint32_t linoff;
	uint32_t stride;
	uint32_t pos;
	uint32_t size;
	uint32_t keyminval;
	uint32_t keymask;
	uint32_t surf;
	uint32_t keymaxval;
	uint32_t tileoff;
	uint32_t contalpa;
};

/* dependent macros*/
#define INTEL_SPRITE_PLANE_NUM          3
#define INTEL_OVERLAY_PLANE_NUM         2
#define INTEL_DISPLAY_PLANE_NUM         5
/* Medfield */
#define INTEL_MDFLD_SPRITE_PLANE_NUM    3
#define INTEL_MDFLD_OVERLAY_PLANE_NUM   2
#define INTEL_MDFLD_CURSOR_PLANE_NUM    3
#define INTEL_MDFLD_DISPLAY_PLANE_NUM   8
#define INTEL_MDFLD_DISPLAY_PIPE_NUM    3
/* Clovertrail+ */
#define INTEL_CTP_SPRITE_PLANE_NUM      2
#define INTEL_CTP_OVERLAY_PLANE_NUM     1
#define INTEL_CTP_CURSOR_PLANE_NUM      2
#define INTEL_CTP_DISPLAY_PLANE_NUM     5
#define INTEL_CTP_DISPLAY_PIPE_NUM      2

#define INVALID_INDEX                   0xffffffff

struct mdfld_plane_contexts {
	uint32_t active_primaries;
	uint32_t active_sprites;
	uint32_t active_overlays;
	struct intel_sprite_context primary_contexts[INTEL_SPRITE_PLANE_NUM];
	struct intel_sprite_context sprite_contexts[INTEL_SPRITE_PLANE_NUM];
	struct intel_overlay_context overlay_contexts[INTEL_OVERLAY_PLANE_NUM];
};

struct drm_psb_vsync_set_arg {
	uint32_t vsync_operation_mask;

	struct {
		uint32_t pipe;
		int vsync_pipe;
		int vsync_count;
		uint64_t timestamp;
	} vsync;
};

struct drm_psb_dc_info {
	uint32_t pipe_count;

	uint32_t primary_plane_count;
	uint32_t sprite_plane_count;
	uint32_t overlay_plane_count;
	uint32_t cursor_plane_count;
};

struct drm_psb_register_rw_arg {
	uint32_t b_force_hw_on;

	uint32_t display_read_mask;
	uint32_t display_write_mask;
	struct {
		uint32_t pfit_controls;
		uint32_t pfit_autoscale_ratios;
		uint32_t pfit_programmed_scale_ratios;
		uint32_t pipeasrc;
		uint32_t pipebsrc;
		uint32_t vtotal_a;
		uint32_t vtotal_b;
		uint32_t dspcntr_a;
		uint32_t dspcntr_b;
		uint32_t pipestat_a;
		uint32_t int_mask;
		uint32_t int_enable;
	} display;

	uint32_t overlay_read_mask;
	uint32_t overlay_write_mask;

	struct {
		uint32_t OVADD;
		uint32_t OGAMC0;
		uint32_t OGAMC1;
		uint32_t OGAMC2;
		uint32_t OGAMC3;
		uint32_t OGAMC4;
		uint32_t OGAMC5;
		uint32_t IEP_ENABLED;
		uint32_t IEP_BLE_MINMAX;
		uint32_t IEP_BSSCC_CONTROL;
		uint32_t index;
		uint32_t b_wait_vblank;
		uint32_t b_wms;
		uint32_t buffer_handle;
	} overlay;

	uint32_t vsync_operation_mask;

	struct {
		uint32_t pipe;
		int vsync_pipe;
		int vsync_count;
		uint64_t timestamp;
	} vsync;

	uint32_t sprite_enable_mask;
	uint32_t sprite_disable_mask;

	struct {
		uint32_t dspa_control;
		uint32_t dspa_key_value;
		uint32_t dspa_key_mask;
		uint32_t dspc_control;
		uint32_t dspc_stride;
		uint32_t dspc_position;
		uint32_t dspc_linear_offset;
		uint32_t dspc_size;
		uint32_t dspc_surface;
	} sprite;

	uint32_t subpicture_enable_mask;
	uint32_t subpicture_disable_mask;
	struct {
		uint32_t CursorADDR;
		uint32_t xPos;
		uint32_t yPos;
		uint32_t CursorSize;
	} cursor;
	uint32_t cursor_enable_mask;
	uint32_t cursor_disable_mask;

	uint32_t plane_enable_mask;
	uint32_t plane_disable_mask;

	uint32_t get_plane_state_mask;

	struct {
		uint32_t type;
		uint32_t index;
		uint32_t ctx;
	} plane;
};

enum {
	PSB_DC_PLANE_ENABLED,
	PSB_DC_PLANE_DISABLED,
};

enum {
	PSB_GTT_MAP_TYPE_MEMINFO = 0,
	PSB_GTT_MAP_TYPE_BCD,
	PSB_GTT_MAP_TYPE_BCD_INFO,
	PSB_GTT_MAP_TYPE_VIRTUAL,
};

struct psb_gtt_mapping_arg {
	uint32_t type;
	void *hKernelMemInfo;
	uint32_t offset_pages;
	uint32_t page_align;
	uint32_t bcd_device_id;
	uint32_t bcd_buffer_id;
	uint32_t bcd_buffer_count;
	uint32_t bcd_buffer_stride;
	uint32_t vaddr;
	uint32_t size;
};

struct drm_psb_getpageaddrs_arg {
	uint64_t handle;
	uint64_t page_addrs;
	uint64_t gtt_offset;
};


#define MAX_SLICES_PER_PICTURE 72
struct  psb_msvdx_mb_region {
	uint32_t start;
	uint32_t end;
};

typedef struct drm_psb_msvdx_decode_status {
	uint32_t num_region;
	struct psb_msvdx_mb_region mb_regions[MAX_SLICES_PER_PICTURE];
} drm_psb_msvdx_decode_status_t;


enum {
	IDLE_CTRL_ENABLE = 0,
	IDLE_CTRL_DISABLE,
	IDLE_CTRL_ENTER,
	IDLE_CTRL_EXIT
};

struct drm_psb_idle_ctrl {
	uint32_t cmd;
	uint32_t value;
};

/* Controlling the kernel modesetting buffers */

#define DRM_PSB_KMS_OFF                 0x00
#define DRM_PSB_KMS_ON                  0x01
#define DRM_PSB_VT_LEAVE                0x02
#define DRM_PSB_VT_ENTER                0x03
#define DRM_PSB_EXTENSION               0x06
#define DRM_PSB_SIZES                   0x07
#define DRM_PSB_FUSE_REG                0x08
#define DRM_PSB_VBT                     0x09
#define DRM_PSB_DC_STATE                0x0A
#define DRM_PSB_ADB                     0x0B
#define DRM_PSB_MODE_OPERATION          0x0C
#define DRM_PSB_STOLEN_MEMORY           0x0D
#define DRM_PSB_REGISTER_RW             0x0E
#define DRM_PSB_GTT_MAP                 0x0F
#define DRM_PSB_GTT_UNMAP               0x10
#define DRM_PSB_GETPAGEADDRS            0x11
/**
 * NOTE: Add new commands here, but increment
 * the values below and increment their
 * corresponding defines where they're
 * defined elsewhere.
 */
#define DRM_PVR_RESERVED1               0x12
#define DRM_PVR_RESERVED2               0x13
#define DRM_PVR_RESERVED3               0x14
#define DRM_PVR_RESERVED4               0x15
#define DRM_PVR_RESERVED5               0x16

#define DRM_PSB_HIST_ENABLE             0x17
#define DRM_PSB_HIST_STATUS             0x18
#define DRM_PSB_UPDATE_GUARD            0x19
#define DRM_PSB_INIT_COMM               0x1A
#define DRM_PSB_DPST                    0x1B
#define DRM_PSB_GAMMA                   0x1C
#define DRM_PSB_DPST_BL                 0x1D

#define DRM_PVR_RESERVED6               0x1E

#define DRM_PSB_GET_PIPE_FROM_CRTC_ID   0x1F
#define DRM_PSB_DPU_QUERY               0x20
#define DRM_PSB_DPU_DSR_ON              0x21
#define DRM_PSB_DPU_DSR_OFF             0x22
#define DRM_PSB_HDMI_FB_CMD             0x23

/* HDCP IOCTLs */
#define DRM_PSB_QUERY_HDCP              0x24
#define DRM_PSB_VALIDATE_HDCP_KSV       0x25
#define DRM_PSB_GET_HDCP_STATUS         0x26
#define DRM_PSB_ENABLE_HDCP             0x27
#define DRM_PSB_DISABLE_HDCP            0x28
#define DRM_PSB_GET_HDCP_LINK_STATUS    0x2b
#define DRM_PSB_ENABLE_HDCP_REPEATER    0x2c
#define DRM_PSB_DISABLE_HDCP_REPEATER   0x2d
#define DRM_PSB_HDCP_REPEATER_PRESENT   0x2e

/* CSC IOCTLS */
#define DRM_PSB_CSC_GAMMA_SETTING       0x29
#define DRM_PSB_SET_CSC                 0x2a

/* IED session */
#define DRM_PSB_ENABLE_IED_SESSION      0x30
#define DRM_PSB_DISABLE_IED_SESSION     0x31

/* VSYNC IOCTLS */
#define DRM_PSB_VSYNC_SET               0x32

/* HDCP */
#define DRM_PSB_HDCP_DISPLAY_IED_OFF    0x33
#define DRM_PSB_HDCP_DISPLAY_IED_ON     0x34
#define DRM_PSB_QUERY_HDCP_DISPLAY_IED_CAPS 0x35

/* DPST LEVEL */
#define DRM_PSB_DPST_LEVEL              0x36

/* GET DC INFO IOCTLS */
#define DRM_PSB_GET_DC_INFO             0x37

/* Panel type query, 0: command mode, 1: video mode */
#define DRM_PSB_PANEL_QUERY             0x38

/* IDLE IOCTL*/
#define DRM_PSB_IDLE_CTRL               0x39

/****BEGIN HDMI TEST IOCTLS ****/
#define DRM_PSB_HDMITEST                0x3A

/* read an hdmi test register */
#define HT_READ                         1
/* write an hdmi test register */
#define HT_WRITE                        2
/* force power island on */
#define HT_FORCEON                      4

typedef struct tagHDMITESTREGREADWRITE {
	/* register offset */
	unsigned int reg;
	/* input/output value */
	unsigned int data;
	/* OR'ed combo of HT_xx flags */
	int mode;
} drm_psb_hdmireg_t, *drm_psb_hdmireg_p;

/**** END HDMI TEST IOCTLS ****/


/* GET PANEL ORIENTATION INFO */
#define DRM_PSB_PANEL_ORIENTATION       0x3B

/* Update cursor position, input is intel_dc_cursor_ctx */
#define DRM_PSB_UPDATE_CURSOR_POS       0x3C


/* Do not use IOCTL between 0x40 and 0x4F */
/* These will be reserved for OEM to use */
/* OEM IOCTLs */
#define DRM_OEM_RESERVED_START          0x40
#define DRM_OEM_RESERVED_END            0x4F


/*
 * TTM execbuf extension.
 */
#define DRM_PSB_TTM_START               0x50
#define DRM_PSB_TTM_END                 0x5F
#if defined(PDUMP)
#define DRM_PSB_CMDBUF                  (PVR_DRM_DBGDRV_CMD + 1)
#else
#define DRM_PSB_CMDBUF                  (DRM_PSB_TTM_START)
#endif
#define DRM_PSB_SCENE_UNREF             (DRM_PSB_CMDBUF + 1)
#define DRM_PSB_PLACEMENT_OFFSET        (DRM_PSB_SCENE_UNREF + 1)




#define DRM_PSB_DSR_ENABLE              0xfffffffe
#define DRM_PSB_DSR_DISABLE             0xffffffff

struct drm_psb_csc_matrix {
	int pipe;
	int64_t matrix[9];
}__attribute__((packed));

struct psb_drm_dpu_rect {
	int x, y;
	int width, height;
};

struct drm_psb_drv_dsr_off_arg {
	int screen;
	struct psb_drm_dpu_rect damage_rect;
};


struct drm_psb_dev_info_arg {
	uint32_t num_use_attribute_registers;
};
#define DRM_PSB_DEVINFO                 0x01
#define PSB_MODE_OPERATION_MODE_VALID   0x01
#define PSB_MODE_OPERATION_SET_DC_BASE  0x02

struct drm_psb_get_pipe_from_crtc_id_arg {
	/** ID of CRTC being requested **/
	uint32_t crtc_id;

	/** pipe of requested CRTC **/
	uint32_t pipe;
};
#define DRM_PSB_DISP_SAVE_HDMI_FB_HANDLE        1
#define DRM_PSB_DISP_GET_HDMI_FB_HANDLE         2
#define DRM_PSB_DISP_INIT_HDMI_FLIP_CHAIN       1
#define DRM_PSB_DISP_QUEUE_BUFFER               2
#define DRM_PSB_DISP_DEQUEUE_BUFFER             3
#define DRM_PSB_DISP_PLANEB_DISABLE             4
#define DRM_PSB_DISP_PLANEB_ENABLE              5
#define DRM_PSB_HDMI_OSPM_ISLAND_DOWN           6
#define DRM_PSB_HDMI_NOTIFY_HOTPLUG_TO_AUDIO    7

/*csc gamma setting*/
typedef enum {
	GAMMA,
	CSC,
	GAMMA_INITIA,
	GAMMA_SETTING,
	GAMMA_REG_SETTING,
	CSC_INITIA,
	CSC_CHROME_SETTING,
	CSC_SETTING,
	CSC_REG_SETTING
} setting_type;

typedef enum {
	/* gamma 0.5 */
	GAMMA_05 = 1,
	/* gamma 2.0 */
	GAMMA_20,
	/* gamma 0.5 + 2.0*/
	GAMMA_05_20,
	/* gamma 2.0 + 0.5*/
	GAMMA_20_05,
	/* gamma 1.0 */
	GAMMA_10
} gamma_mode;

#define CSC_REG_COUNT                   6
#define CHROME_COUNT                    16
#define CSC_COUNT                       9

struct csc_setting {
	uint32_t pipe;
	setting_type type;
	bool enable_state;
	uint32_t data_len;
	union {
		int csc_reg_data[CSC_REG_COUNT];
		int chrome_data[CHROME_COUNT];
		int64_t csc_data[CSC_COUNT];
	} data;
};
#define GAMMA_10_BIT_TABLE_COUNT        129

struct gamma_setting {
	uint32_t pipe;
	setting_type type;
	bool enable_state;
	gamma_mode initia_mode;
	uint32_t data_len;
	uint32_t gamma_tableX100[GAMMA_10_BIT_TABLE_COUNT];
};
struct drm_psb_csc_gamma_setting {
	setting_type type;
	union {
		struct csc_setting csc_data;
		struct gamma_setting gamma_data;
	} data;
}__attribute__((packed));
struct drm_psb_buffer_data {
	void *h_buffer;
};
struct drm_psb_flip_chain_data {
	void **h_buffer_array;
	unsigned int size;
};
struct drm_psb_disp_ctrl {
	uint32_t cmd;
	union {
		uint32_t data;
		struct drm_psb_buffer_data buf_data;
		struct drm_psb_flip_chain_data flip_chain_data;
	} u;
};

/* Merrifield driver specific interface */

#define S3D_MIPIA_DISPLAY               0
#define S3D_HDMI_DISPLAY                1
#define S3D_MIPIC_DISPLAY               2
#define S3D_WIDI_DISPLAY                0xFF

struct drm_psb_s3d_query {
	uint32_t s3d_display_type;
	uint32_t is_s3d_supported;
	uint32_t s3d_format;
	uint32_t mode_resolution_x;
	uint32_t mode_resolution_y;
	uint32_t mode_refresh_rate;
	uint32_t is_interleaving;
};

struct drm_psb_s3d_premodeset {
	uint32_t s3d_buffer_format;
};


typedef enum intel_dc_plane_types {
	DC_UNKNOWN_PLANE = 0,
	DC_SPRITE_PLANE = 1,
	DC_OVERLAY_PLANE,
	DC_PRIMARY_PLANE,
	DC_CURSOR_PLANE,
	DC_PLANE_MAX,
} DC_MRFLD_PLANE_TYPE;

#define SPRITE_UPDATE_SURFACE           (0x00000001UL)
#define SPRITE_UPDATE_CONTROL           (0x00000002UL)
#define SPRITE_UPDATE_POSITION          (0x00000004UL)
#define SPRITE_UPDATE_SIZE              (0x00000008UL)
#define SPRITE_UPDATE_WAIT_VBLANK       (0X00000010UL)
#define SPRITE_UPDATE_CONSTALPHA        (0x00000020UL)
#define SPRITE_UPDATE_ALL               (0x0000003fUL)
#define MRFLD_PRIMARY_COUNT             3

typedef struct intel_dc_overlay_ctx {
	uint32_t index;
	uint32_t pipe;
	uint32_t ovadd;
} DC_MRFLD_OVERLAY_CONTEXT;

typedef struct intel_dc_cursor_ctx {
	uint32_t index;
	uint32_t pipe;
	uint32_t cntr;
	uint32_t surf;
	uint32_t pos;
} DC_MRFLD_CURSOR_CONTEXT;

typedef struct intel_dc_sprite_ctx {
	uint32_t update_mask;
	/* plane index 0-A, 1-B, 2-C,etc */
	uint32_t index;
	uint32_t pipe;

	uint32_t cntr;
	uint32_t linoff;
	uint32_t stride;
	uint32_t pos;
	uint32_t size;
	uint32_t keyminval;
	uint32_t keymask;
	uint32_t surf;
	uint32_t keymaxval;
	uint32_t tileoff;
	uint32_t contalpa;
} DC_MRFLD_SPRITE_CONTEXT;

typedef struct intel_dc_primary_ctx {
	uint32_t update_mask;
	/* plane index 0-A, 1-B, 2-C,etc */
	uint32_t index;
	uint32_t pipe;
	uint32_t cntr;
	uint32_t linoff;
	uint32_t stride;
	uint32_t pos;
	uint32_t size;
	uint32_t keyminval;
	uint32_t keymask;
	uint32_t surf;
	uint32_t keymaxval;
	uint32_t tileoff;
	uint32_t contalpa;
} DC_MRFLD_PRIMARY_CONTEXT;

typedef struct intel_dc_plane_zorder {
	/* 3 primary planes */
	uint32_t forceBottom[3];
	/* 1 sprite plane */
	uint32_t abovePrimary;
} DC_MRFLD_DC_PLANE_ZORDER;

typedef struct intel_dc_plane_ctx {
	enum intel_dc_plane_types type;
	struct intel_dc_plane_zorder zorder;
	union {
		struct intel_dc_overlay_ctx ov_ctx;
		struct intel_dc_sprite_ctx sp_ctx;
		struct intel_dc_primary_ctx prim_ctx;
		struct intel_dc_cursor_ctx cs_ctx;
	} ctx;
} DC_MRFLD_SURF_CUSTOM;

#endif
