/**
 * file vsp.h
 * Author: Binglin Chen <binglin.chen@intel.com>
 *
 */

/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
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

#ifndef _VSP_FW_H_
#define _VSP_FW_H_

#pragma pack(4)
#define VssProcPipelineMaxNumFilters 5
#define VSS_PROC_MAX_INPUT_PICTURES  1
#define VSS_PROC_MAX_OUTPUT_PICTURES 4

/* Application IDs for applications that use secure boot
 * and context-switching
 * */
#define VSP_APP_ID_NONE 0
#define VSP_APP_ID_FRC_VPP 1
#define VSP_APP_ID_VP8_ENC 2
#define VSP_APP_ID_WIDI_ENC 3

enum VssProcFilterType {
	VssProcFilterDenoise,
	VssProcFilterSharpening,
	VssProcFilterColorEnhancement,
	VssProcFilterFrameRateConversion
};

enum VssDenoiseType {
	VssProcDegrain,
	VssProcDeblock
};

enum VssFrcQuality {
	/* VssFrcLowQuality, */
	VssFrcMediumQuality,
	VssFrcHighQuality
};

enum VssFrcConversionRate {
	VssFrc2xConversionRate,
	VssFrc2_5xConversionRate,
	VssFrc4xConversionRate,
	VssFrc1_25xConversionRate
};

struct VssProcPipelineParameterBuffer {
	unsigned int      num_filters;
	enum VssProcFilterType filter_pipeline[VssProcPipelineMaxNumFilters];
	unsigned int intermediate_buffer_base;
	unsigned int intermediate_buffer_size;
};

struct VssProcSharpenParameterBuffer {
	int quality;
	/* to make multiple of 32 bytes*/
	unsigned int _pad[7];
};

struct VssProcDenoiseParameterBuffer {
	enum VssDenoiseType     type;
	int                value_thr;
	int                cnt_thr;
	int                coef;
	int                temp_thr1;
	int                temp_thr2;
	/* to make multiple of 32 bytes*/
	int                _pad[2];
};

struct VssProcColorEnhancementParameterBuffer {
	int                temp_detect;
	int                temp_correct;
	int                clip_thr;
	int                mid_thr;
	int                luma_amm;
	int                chroma_amm;
	/* to make multiple of 32 bytes*/
	int                _pad[2];
};

struct VssProcFrcParameterBuffer {
	enum VssFrcQuality quality;
	enum VssFrcConversionRate conversion_rate;
	/* to make multiple of 32 bytes*/
	int  _pad[6];
};

/* Set the rotation angle */
#define VSP_ROTATION_NONE 0
#define VSP_ROTATION_90   90
#define VSP_ROTATION_180  180
#define VSP_ROTATION_270  270

struct VssProcPicture {
	unsigned int surface_id;
	/* send interupt when input or output surface is ready */
	unsigned int irq;
	unsigned int base;
	unsigned int height;
	unsigned int width;
	unsigned int rot_angle;
	unsigned int stride;
	/* frame raw format */
	unsigned int format;
	/* flag indicating if frame is stored in tiled format */
	unsigned int tiled;
	/* to make multiple of 32 bytes*/
	int _pad[7];
};

struct VssProcPictureParameterBuffer {
	unsigned int num_input_pictures;
	unsigned int num_output_pictures;
	/* to make multiple of 32 bytes*/
	int          _pad[6];
	struct VssProcPicture input_picture[VSS_PROC_MAX_INPUT_PICTURES];
	struct VssProcPicture output_picture[VSS_PROC_MAX_OUTPUT_PICTURES];
};

union VssProcBuffer {
	struct VssProcPipelineParameterBuffer         pipeline;
	struct VssProcSharpenParameterBuffer          sharpen_base;
	struct VssProcDenoiseParameterBuffer          denoiser_base;
	struct VssProcColorEnhancementParameterBuffer enhancer_base;
	struct VssProcFrcParameterBuffer              frc;
	struct VssProcPictureParameterBuffer          picture;
};

enum VssProcCommandType {
	VssProcPipelineParameterCommand =         0xFFFE,
	VssProcSharpenParameterCommand =          0xFFFD,
	VssProcDenoiseParameterCommand =          0xFFFC,
	VssProcColorEnhancementParameterCommand = 0xFFFB,
	VssProcFrcParameterCommand =              0xFFFA,
	VssProcPictureCommand =                   0xFFF9,
	VspFencePictureParamCommand =             0xEBEC,
	VspSetContextCommand =                    0xEBED,
	Vss_Sys_STATE_BUF_COMMAND =		  0xEBEE,
	VspFenceComposeCommand =		  0xEBEF
};

#define VSP_CMD_QUEUE_SIZE (64)
#define VSP_ACK_QUEUE_SIZE (64)

/*
 * Command types and data structure.
 * Each command has a type. Depending on the type there is some kind
 * of data in external memory,
 * The VSS will use its DMA to load data from the buffer into local memory.
 */
struct vss_command_t {
	unsigned int       context;
	unsigned int       type;
	unsigned int       buffer;
	unsigned int       size;
	unsigned int       buffer_id;
	unsigned int       irq;
	unsigned int       reserved6;
	unsigned int       reserved7;
};

struct vss_response_t {
	unsigned int       context;
	unsigned int       type;
	unsigned int       buffer;
	unsigned int       size;
	unsigned int       vss_cc;
	unsigned int       reserved5;
	unsigned int       reserved6;
	unsigned int       reserved7;
};

/* Default initial values for vsp-command and vsp-response
* Using those avoids the risk of uninitialized warnings when
* the definition changes.
*/
#define VSP_COMMAND_INITIALIZER {0, 0, 0, 0, 0, 0, 0, 0}
#define VSP_RESPONSE_INITIALIZER {0, 0, 0, 0, 0, 0, 0, 0}

/*
 * Response types
 */
enum VssResponseType {
	VssIdleResponse               = 0x80010000,
	VssErrorResponse              = 0x80020000,
	VssEndOfSequenceResponse      = 0x80030000,
	VssCommandBufferReadyResponse = 0x80040000,
	VssInputSurfaceReadyResponse  = 0x80050000,
	VssOutputSurfaceReadyResponse = 0x80060000,
	VssVp8encSetSequenceParametersResponse = 150,
	VssVp8encEncodeFrameResponse
};

enum VssStatus {
	VssOK                         = 0x8001,
	VssInvalidCommandType         = 0x8002,
	VssInvalidCommandArgument     = 0x8003,
	VssInvalidProcPictureCommand  = 0x8004,
	VssInvalidDdrAddress          = 0x8005,
	VssInvalidSequenceParameters_VP8 = 0x1,
	VssInvalidPictureParameters_VP8  = 0x2,
	VssContextMustBeDestroyed_VP8    = 0x3,
	VssInitFailure_VP8               = 0x5,
	VssCorruptFrame                  = 0x6,
	VssCorruptFramecontinue_VP8      = 0x7
};

enum FrcResponseType {
	VssOutputSurfaceFreeResponse = 0x0000F001,
	VssOutputSurfaceCrcResponse  = 0x0000F002
};

enum vsp_format {
	VSP_NV12,
	VSP_YV12,
	VSP_UYVY,
	VSP_YUY2,
	VSP_NV11,
	VSP_NV16,
	VSP_IYUV,
	VSP_TYPE_ERROR
};

struct vsp_data {
	unsigned int fw_state;
	unsigned int uninit_req;
};

#define VSP_SECURE_BOOT_MAGIC_NR 0xb0070001

enum vsp_processor {
	vsp_sp0 = 0,
	vsp_sp1 = 1,
	vsp_vp0 = 2,
	vsp_vp1 = 3,
	vsp_mea = 4
};

/**
* Header-data/struct by PUnit to start VSP boot-processor
* This struct is mapped directly into the header of the multi-application-blob
*
* For each value that is to be written to the VSP, the register-address to
* write to is listed directly after the value to be written.
*
* Entries that contain values can be written directly into the VSP-system.
* Offsets need to have the secure-boot-header-address added and then be written
* into the VSP
*
* boot_start_value should always be the last value written. (Since it starts
* the VSP)
*/
struct vsp_secure_boot_header {
	/* Magic number to identify header version */
	unsigned int magic_number;

	/* Offset to text section of boot-program in blob */
	unsigned int boot_text_offset;
	/* iCache base-address of boot-processor */
	unsigned int boot_text_reg;

	/* Value of icache-control-bits to write to boot-processor */
	unsigned int boot_icache_value;
	/* status&control register of boot-processor */
	unsigned int boot_icache_reg;

	/* Value of program counter to write to boot-processor */
	/* address of main-function in boot-program */
	unsigned int boot_pc_value;
	/* pc-start-register of boot-processor */
	unsigned int boot_pc_reg;

	/* Offset of multi-application-header in blob */
	unsigned int ma_header_offset;
	unsigned int ma_header_reg;

	/* Value to write to start the boot-processor */
	unsigned int boot_start_value;
	/* status&control register of boot-processor */
	unsigned int boot_start_reg;
};

#define VSP_MULTI_APP_MAGIC_NR 0xb10b0005
/*
 * Note: application index/id 0 is reserved.
 * So the maximum number of applications is one less than listed here.
 * */
#define VSP_MULTI_APP_MAX_APPS 16

/*
 * With a 1MB state-buffer in IMR and a 50k context-buffer-size, we could run
 * * max 20 apps. Using 32 as a nice round number of maximum nr of contexts.
 * * Actual maximum allowed contexts is currently less, since context-buffer-size
 * * is larger than 50k.
 * */
#define VSP_MULTI_APP_MAX_CONTEXTS 32
#define VSP_API_GENERIC_CONTEXT_ID (0xffffffff)
/*
 * Struct used by VSP-boot-processor to start the correct application
 * Read from header in firmware ma-blob.
 * Address of the header is communicated by p-unit.
 *
 * Note: this is a VIED internal header
 */
struct vsp_multi_app_blob_data {
	unsigned int magic_number;
	unsigned int offset_from_start;
	/** State buffer address in virtual memory, default location on TNG B0 and ANN
	 * * is 0xA0000000 (2.5GB memory offset, master port 2, 2nd IMR region) */
	unsigned int imr_state_buffer_addr;
	/** Size of state-buffer in IMR (in bytes). Default state buffer size for TNG
	 * * B0 and ANN is 1 MB */
	unsigned int imr_state_buffer_size;
	/** default context-buffer size of apps in this blob (each app also has it's
	 * context-size in it's header. */
	unsigned int apps_default_context_buffer_size;
	/**
	* Address of genboot-helper-program in blob (relative to start of this header)
	*/
	unsigned int genboot_helper_prog_offset;
	/*
	 * * This table contains a zero (offset of zero) for unused entries
	 * * Offsets here are relative to the start-address of this header.
	 */
	unsigned int application_blob_offsets[VSP_MULTI_APP_MAX_APPS];
};

/*
 * Struct for the settings of a single context. Normally placed in an array in
 * the multi-app header in IMR
 *
 * Context-id is determined by the position in the array, so it is not stored in
 * the struct itself.
 *
 * State_buffer_size and state_buffer_addr are currently not stored, since they
 * can/will be determined automatically based on generic IMR parameters.
 *
 * Usage field is the last field, so that it gets written last during a memory
 * transfer.
 */
struct vsp_multi_app_context_settings {
	unsigned int app_id;  /* Which app this context belongs to */
	unsigned int usage; /* Indicates if this context is in use */
};

/*
 * Datastructure placed at the beginning of the VSP IMR state-save region.
 * */
struct vsp_multi_app_imr_header {
	/*
	 * Usage field (32-bit), set to 0 by Chaabi during system bootup, set to 1
	 * by VSP if it is safe for PUnit to perform a restart without power-cycle.
	 * Set to any other value by VSP if VSP is running.
	 * */
	unsigned int vsp_and_imr_state;
	/* Reserved field for 256-bit alignment of header */
	unsigned int reserved_1;
	/* Reserved field for 256-bit alignment of header */
	unsigned int reserved_2;
	/* Reserved field for 256-bit alignment of header */
	unsigned int reserved_3;
	/* Reserved field for 256-bit alignment of header */
	unsigned int reserved_4;
	/* Reserved field for 256-bit alignment of header */
	unsigned int reserved_5;
	/* Reserved field for 256-bit alignment of header */
	unsigned int reserved_6;
	/* Reserved field for 256-bit alignment of header */
	unsigned int reserved_7;
	/* Settings of all active/available contexts */
	struct vsp_multi_app_context_settings context_settings[VSP_MULTI_APP_MAX_CONTEXTS];
};

enum vsp_imr_state{
	/** State when no data for VSP is initialized */
	vsp_imr_uninitialized = 0,
	/** State where datastructures are initialized, but no VSP is running */
	vsp_imr_safe_to_resume = 1,
	/** State where datastructures are initialized and VSP(-API) is running */
	vsp_imr_initialized = 2,
	/** State where datastructures are initialized and VSP(-APP) is running */
	vsp_imr_app_is_running = 3
};

enum vsp_ctrl_reg_addr {
	VSP_SETTING_ADDR_REG      = 3,
	VSP_SECBOOT_DEBUG_REG     = 4,
	VSP_ENTRY_KIND_REG        = 5,
	VSP_POWER_SAVING_MODE_REG = 6,
	VSP_MMU_TLB_SOFT_INVALIDATE_REG = 7,
	VSP_CMD_QUEUE_RD_REG      = 12,
	VSP_CMD_QUEUE_WR_REG      = 13,
	VSP_ACK_QUEUE_RD_REG      = 14,
	VSP_ACK_QUEUE_WR_REG      = 15
};

struct vsp_ctrl_reg {
	unsigned int reserved_2;

	/* setting address from host to firmware */
	unsigned int setting_addr;

	/* used for sending debug-status from firmware to host */
	unsigned int secboot_debug;

	/* entry type from host to firmware
	 * If it contains vsp_exit, uninitialize the firmware
	 */
	unsigned int entry_kind;

	/* set the power-saving-mode setting */
	unsigned int power_saving_mode;

	/* config reg to request firmware to perform an MMU TLB invalidate.
	* MMU TLB invalidation for VSP on TNG needs to be done through firmware
	* due to a hardware bug that could trigger if TLB invalidation is done
	* while VSP DMA is not idle.
	*/
	unsigned int mmu_tlb_soft_invalidate;

	unsigned int reserved_8;
	unsigned int reserved_9;
	unsigned int reserved_10;
	unsigned int reserved_11;

	/* used for the command and response queues */
	unsigned int cmd_rd;
	unsigned int cmd_wr;
	unsigned int ack_rd;
	unsigned int ack_wr;
};

/* constant parameters passed from host to firmware,
 * address of this struct is passed via config reg
 * struct is written to ddr in vsp_init call, destroyed upon uninit
 */
struct vsp_settings_t {
	/* Extra field to align to 256 bit (for DMA) */
	unsigned int reserved0;
	unsigned int command_queue_size;
	unsigned int command_queue_addr;
	unsigned int response_queue_size;
	unsigned int response_queue_addr;
	/* Extra field to align to 256 bit (for DMA) */
	unsigned int reserved5;
	/* Extra field to align to 256 bit (for DMA) */
	unsigned int reserved6;
	unsigned int reserved7;
};

/**
* The host should only modify the vsp_context_settings_entry when the usage
* field is vsp_context_unused or vsp_context_uninit. The host can do the
* following state-transitions for the usage field:
* 1) vsp_context_unused->vsp_context_starting: start a new stream/context.
* After this transition, the host can submit commands into the command-queue
* for the context-id associated with this vsp_context_settings entry.
* 2) vsp_context_deinit->vsp_context_unused: destroy resources (free state
* buffer) from the no longer needed context and mark the context as being
* unused.
*
* The VSP will only modify the vsp_context_settings_entry when the usage
* field is vsp_context_starting or vsp_context_in_use. The VSP will do the
* following state-transitions for the usage field:
* 3) vsp_context_starting->vsp_context_in_use: Perform initialisation of
* state-buffers and other VSP-side initialisation required to start a new
* stream/context. This is typically done when the first command for this
* context is received from the host.
* 4) vsp_context_in_use->vsp_context_deinit: Mark a context as being no longer
* used by the VSP. The VSP will no longer access any resource used by this
* context after this transition. This transition is done after an
* end-of-stream response or similar response to the host to indicate that an
* application finished for a specific context.
*/
enum vsp_context_usage {
	vsp_context_unused   = 0,
	vsp_context_deinit   = 1,
	vsp_context_starting = 16,
	vsp_context_in_use   = 17
};

/* default initializer to initialize vsp_settings struct
 * (including the extra alignment fields)
 */
#define VSP_SETTINGS_INITIALIZER {0, 0, 0, 0, 0, 0, 0, 0}

/* values passed via VSP_ENTRY_TYPE_REG
 * vsp_entry_booted is the default value, it means no init or resume has been
 * communicated by the host yet.
 * vsp_entry_init and vsp_entry_resume are used for initial starting of the
 * system and for resuming after a suspend/power-down.
*/
enum vsp_entry_kind {
	vsp_entry_booted   = 0,
	vsp_entry_init     = 1,
	vsp_entry_resume   = 2,
	vsp_exit           = 3
};

/* values passed via VSP_POWER_SAVING_MODE_REG */
enum vsp_power_saving_mode {
	vsp_always_on              = 0,
	vsp_suspend_on_empty_queue = 1,
	vsp_hw_idle_on_empty_queue = 2,
	vsp_suspend_and_hw_idle_on_empty_queue
};

/****************************
 * VP8enc data structures
 ****************************/

/**
 * Picture data structure. Currently the same as frc
 */
struct VssProcPictureVP8 {
	uint32_t surface_id;
	uint32_t irq;  /* send interupt when input or output surface is ready */
	uint32_t base; /* pointer to luma picture in DDR */
	uint32_t base_uv; /* pointer to chroma picture in DDR */
	uint32_t height;
	uint32_t width;
	uint32_t stride;
	uint32_t format; /* frame raw format */
};

/**
 * Enumeration for recon_buffer_mode
 */
typedef enum {
	vss_vp8enc_seq_param_recon_buffer_mode_per_seq = 0, /* send 4 ref/recon frame buffers at seq lvl */
	vss_vp8enc_seq_param_recon_buffer_mode_per_pic,     /* send 1 recon frame buffer per picture */
	vss_vp8enc_seq_param_recon_buffer_mode_cnt          /* nr of modes */
} vss_vp8enc_seq_param_recon_buffer_mode_t;

/**
 * Sequence parameter data structure.
 */
struct VssVp8encSequenceParameterBuffer {
	uint32_t frame_width;
	uint32_t frame_height;
	uint32_t frame_rate;
	uint32_t error_resilient;
	uint32_t num_token_partitions;
	uint32_t kf_mode;
	uint32_t kf_min_dist;
	uint32_t kf_max_dist;
	uint32_t rc_target_bitrate;
	uint32_t rc_min_quantizer;
	uint32_t rc_max_quantizer;
	uint32_t rc_undershoot_pct;
	uint32_t rc_overshoot_pct;
	uint32_t rc_end_usage;
	uint32_t rc_buf_sz;
	uint32_t rc_buf_initial_sz;
	uint32_t rc_buf_optimal_sz;
	uint32_t max_intra_rate;
	uint32_t cyclic_intra_refresh;
	uint32_t concatenate_partitions;
	uint32_t recon_buffer_mode;
	uint32_t ts_number_layers;
	uint32_t ts_target_bitrate[3];
	uint32_t ts_rate_decimator[3];
	uint32_t ts_periodicity;
	uint8_t ts_layer_id[32];
	struct VssProcPictureVP8 ref_frame_buffers[4];
};

struct VssVp8encEncodedFrame {
	uint32_t frame_size;
	uint32_t status;
	uint32_t partitions;
	uint32_t partition_size[9];
	uint32_t partition_start[9];
	uint32_t segments;
	uint32_t quantizer[4];
	uint32_t frame_flags;
	uint32_t partition_id;
	uint32_t buffer_level;
	uint32_t quality;
	uint32_t surfaceId_of_ref_frame[4];
	uint32_t reserved[15];
	uint32_t coded_data[1];
};

/**
 * Encode frame command buffer
 */
struct VssVp8encPictureParameterBuffer {
	struct VssProcPictureVP8 input_frame;
	struct VssProcPictureVP8 recon_frame;

	uint32_t version;
	uint32_t pic_flags;
	uint32_t prev_frame_dropped;
	uint32_t cpuused;
	uint32_t sharpness;
	uint32_t num_token_partitions;
	uint32_t encoded_frame_size;
	uint32_t encoded_frame_base;
};

/**
 * Command enumeration
 */
enum VssVp8encCommandType {
	VssVp8encSetSequenceParametersCommand = 123,
	VssVp8encEncodeFrameCommand,
	VssVp8encEndOfSequenceCommand,
	VssVp8encInit,
	Vss_Sys_Ref_Frame_COMMAND
};

/*
 * Generic VSP commands
 *
 * Generic VSP commands should be sent with the context field set to
 * VSP_API_GENERIC_CONTEXT_ID.
 */
enum VssGenCommandType {
	/** Generic command to instruct the VSP to (create and) initialize a context.
	 * * The buffer field contains the context-id of the new context to initialize.
	 * The size-field contains the app-id for the new context to initialize
	 */
	VssGenInitializeContext       = 0xab01,
	/** Generic command to instruct the VSP to de-initialize and destroy a
	 * context. The buffer field contains the context-id of the context to
	 * de-initialize and destroy. The size-field should always be set to 0.
	 */
	VssGenDestroyContext          = 0xab02
};

/****************************
 * WiDi Compose data structures
 ****************************/
enum VssWiDi_ComposeCommandType {
	VssWiDi_ComposeSetSequenceParametersCommand = 200,
	VssWiDi_ComposeFrameCommand,
	VssWiDi_ComposeEndOfSequenceCommand
};

enum VssWiDi_ComposeResponseType {
	VssWiDi_ComposeSetSequenceParametersResponse = 250,
	VssWiDi_ComposeFrameResponse,
};

enum VssWiDi_ColorFormat {
	MonoChrome = 0,
	YUV_4_2_0,
	YUV_4_2_0_NV12,
	YUV_4_2_2,
	YUV_4_4_4
};
/**
 * WiDi Compose sequence parameter data structure.
 */
struct VssWiDi_ComposeSequenceParameterBuffer {
	unsigned int R_Buffer;
	unsigned int G_Buffer;
	unsigned int B_Buffer;
	unsigned int RGBA_Buffer;
	unsigned int Y_Buffer;
	unsigned int UV_Buffer;
	unsigned int U_Buffer;
	unsigned int V_Buffer;
	unsigned int A_Buffer;
	int ActualWidth;
	int ActualHeight;
	int ProcessedWidth;
	int ProcessedHeight;
	int TotalMBCount;
	int Stride;
	/*Video related*/
	int Video_IN_xsize;
	int Video_IN_ysize;
	int Video_IN_stride;
	int Video_IN_yuv_format; 

	unsigned int Video_IN_Y_Buffer;
	unsigned int Video_IN_UV_Buffer;
        unsigned int Video_IN_U_Buffer;
	unsigned int Video_IN_V_Buffer;
	int Video_OUT_xsize;
	int Video_OUT_ysize;
	int Video_OUT_stride;
	int Video_OUT_yuv_format; 

	unsigned int Video_OUT_Y_Buffer;
	unsigned int Video_OUT_UV_Buffer;
	unsigned int Video_OUT_V_Buffer;

	unsigned int scaled_width;
	unsigned int scaled_height;
	unsigned int scalefactor_dx;
	unsigned int scalefactor_dy;

	/*Blending related params*/
	int Is_Blending_Enabled;
	int ROI_width;
	int ROI_height;
	int ROI_x1;
	int ROI_y1;
	int ROI_x2;
	int ROI_y2;
	int alpha1;
	int alpha2;
	int Is_video_the_back_ground;
	int Is_source_1_image_available;
	int Is_source_2_image_available;
	int Is_alpha_channel_available;
	int Video_TotalMBCount;
	int CSC_FormatSelect; /* 0: YUV420NV12; 1: YUV444; */
	int CSC_InputFormatSelect; // 0: RGB Planar; 1: RGBA Interleaved
};

#pragma pack()
#endif
