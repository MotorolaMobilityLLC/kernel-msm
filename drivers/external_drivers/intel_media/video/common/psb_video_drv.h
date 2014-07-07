/*
 * Copyright (c) 2008, Intel Corporation
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
 *    Eric Anholt <eric@anholt.net>
 *    Fei Jiang <fei.jiang@intel.com>
 *
 **/
#ifndef _PSB_CMDBUF_H_
#define _PSB_CMDBUF_H_

#ifdef CONFIG_DRM_VXD_BYT
#include "vxd_drv.h"
#include "vxd_drm.h"
#else
#include "psb_drv.h"
#endif

#include "ttm/ttm_execbuf_util.h"

#ifdef MERRIFIELD
#include "bufferclass_interface.h"
#include "pvrsrv_interface.h"
#endif

struct drm_psb_private;

/*
 * TTM driver private offsets used for mmap.
 */
#ifdef CONFIG_DRM_VXD_BYT
/* need distinguish between gem mmap and ttm mmap */
#define DRM_PSB_FILE_PAGE_OFFSET ((0x100000000ULL >> PAGE_SHIFT) * 18)
#else
#define DRM_PSB_FILE_PAGE_OFFSET (0x100000000ULL >> PAGE_SHIFT)
#endif

#ifdef MERRIFIELD
#define PSB_OBJECT_HASH_ORDER 13
#define PSB_FILE_OBJECT_HASH_ORDER 12
#define PSB_BO_HASH_ORDER 12
#else
#define PSB_OBJECT_HASH_ORDER 9
#define PSB_FILE_OBJECT_HASH_ORDER 8
#define PSB_BO_HASH_ORDER 8
#endif

#define PSB_TT_PRIV0_LIMIT	 (512*1024*1024)
#define PSB_TT_PRIV0_PLIMIT	 (PSB_TT_PRIV0_LIMIT >> PAGE_SHIFT)
#define PSB_NUM_VALIDATE_BUFFERS 2048

#ifdef MERRIFIELD
/*
 * For Merrifield, the VSP memory must be in 1GB.
 *	TT size is 256M or 512M
 *	MMU size is 256M
 *	TILING size is 256M
 */
#define PSB_MEM_MMU_START		0x00000000
#define PSB_MEM_TT_START		0x10000000
#define PSB_MEM_MMU_TILING_START	0x30000000
#else
#define PSB_MEM_MMU_START		0x00000000
#define PSB_MEM_TT_START		0x30000000
#define PSB_MEM_IMR_START		0x2C000000
#define PSB_MEM_MMU_TILING_START	0x50000000

#endif

#define PSB_MAX_RELOC_PAGES 1024

/* MMU type */
enum mmu_type_t {
	IMG_MMU = 1,
	VSP_MMU = 2
};

/* psb_cmdbuf.c */
int psb_cmdbuf_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *file_priv);
void psb_fence_or_sync(struct drm_file *file_priv,
			      uint32_t engine,
			      uint32_t fence_types,
			      uint32_t fence_flags,
			      struct list_head *list,
			      struct psb_ttm_fence_rep *fence_arg,
			      struct ttm_fence_object **fence_p);

/* psb_fence.c */
void psb_fence_handler(struct drm_device *dev, uint32_t class);
int psb_fence_emit_sequence(struct ttm_fence_device *fdev,
				   uint32_t fence_class,
				   uint32_t flags, uint32_t *sequence,
				   unsigned long *timeout_jiffies);
void psb_fence_error(struct drm_device *dev,
			    uint32_t class,
			    uint32_t sequence, uint32_t type, int error);
int psb_ttm_fence_device_init(struct ttm_fence_device *fdev);

/*
 * psb_ttm_glue.c
 */
int psb_ttm_fault(struct vm_area_struct *vma, struct vm_fault *vmf);
int psb_fence_signaled_ioctl(struct drm_device *dev, void *data,
				    struct drm_file *file_priv);
int psb_verify_access(struct ttm_buffer_object *bo,
			     struct file *filp);
ssize_t psb_ttm_read(struct file *filp, char __user *buf,
			    size_t count, loff_t *f_pos);
ssize_t psb_ttm_write(struct file *filp, const char __user *buf,
			     size_t count, loff_t *f_pos);
int psb_fence_finish_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file_priv);
int psb_fence_unref_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
int psb_pl_waitidle_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
int psb_pl_setstatus_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file_priv);
int psb_pl_synccpu_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv);
int psb_pl_unref_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv);
int psb_pl_reference_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file_priv);
int psb_pl_create_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file_priv);
int psb_pl_ub_create_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file_priv);
int psb_extension_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file_priv);
int psb_ttm_global_init(struct drm_psb_private *dev_priv);
void psb_ttm_global_release(struct drm_psb_private *dev_priv);
int psb_getpageaddrs_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file_priv);
int psb_video_getparam(struct drm_device *dev, void *data,
			      struct drm_file *file_priv);
void psb_remove_videoctx(struct drm_psb_private *dev_priv, struct file *filp);

/*
 * psb_mmu.c
 */
struct psb_mmu_driver *psb_mmu_driver_init(uint8_t __iomem *registers,
		int trap_pagefaults, int invalid_type,
		struct drm_psb_private *dev_priv, enum mmu_type_t mmu_type);
void psb_mmu_driver_takedown(struct psb_mmu_driver *driver);
struct psb_mmu_pd *psb_mmu_get_default_pd(struct psb_mmu_driver
							*driver);
struct psb_mmu_pd *psb_mmu_alloc_pd(struct psb_mmu_driver *driver,
						int trap_pagefaults,
						int invalid_type);
void psb_mmu_free_pagedir(struct psb_mmu_pd *pd);
void psb_mmu_flush(struct psb_mmu_driver *driver, int rc_prot);
void psb_mmu_remove_pfn_sequence(struct psb_mmu_pd *pd,
					unsigned long address,
					uint32_t num_pages);
int psb_mmu_insert_pfn_sequence(struct psb_mmu_pd *pd,
						uint32_t start_pfn,
						unsigned long address,
						uint32_t num_pages, int type);
void psb_mmu_set_pd_context(struct psb_mmu_pd *pd, int hw_context);
int psb_mmu_insert_pages(struct psb_mmu_pd *pd, struct page **pages,
				unsigned long address, uint32_t num_pages,
				uint32_t desired_tile_stride,
				uint32_t hw_tile_stride, int type);
void psb_mmu_remove_pages(struct psb_mmu_pd *pd,
				 unsigned long address, uint32_t num_pages,
				 uint32_t desired_tile_stride,
				 uint32_t hw_tile_stride);
#if 0
void psb_mmu_pgtable_dump(struct drm_device *dev);
extern void psb_mmu_mirror_gtt(struct psb_mmu_pd *pd, uint32_t mmu_offset,
			       uint32_t gtt_start, uint32_t gtt_pages);
extern int psb_mmu_virtual_to_pfn(struct psb_mmu_pd *pd, uint32_t virtual,
				  unsigned long *pfn);
#endif
int psb_ttm_bo_clflush(struct psb_mmu_driver *mmu,
			struct ttm_buffer_object *bo);

int tng_securefw(struct drm_device *dev, char *fw_basename, char *island_name, int imrl_reg);
int tng_rawfw(struct drm_device *dev, uint8_t *name);


/* Currently defined profiles */
enum VAProfile {
	VAProfileNone                   = -1,
	VAProfileMPEG2Simple		= 0,
	VAProfileMPEG2Main		= 1,
	VAProfileMPEG4Simple		= 2,
	VAProfileMPEG4AdvancedSimple	= 3,
	VAProfileMPEG4Main		= 4,
	VAProfileH264Baseline		= 5,
	VAProfileH264Main		= 6,
	VAProfileH264High		= 7,
	VAProfileVC1Simple		= 8,
	VAProfileVC1Main		= 9,
	VAProfileVC1Advanced		= 10,
	VAProfileH263Baseline		= 11,
	VAProfileJPEGBaseline           = 12,
	VAProfileH264ConstrainedBaseline = 13,
	VAProfileVP8Version0_3          = 14,
	VAProfileMax
};

/* Currently defined entrypoints */
enum VAEntrypoint {
	VAEntrypointVLD		= 1,
	VAEntrypointIZZ		= 2,
	VAEntrypointIDCT	= 3,
	VAEntrypointMoComp	= 4,
	VAEntrypointDeblocking	= 5,
	VAEntrypointEncSlice	= 6,	/* slice level encode */
	VAEntrypointEncPicture  = 7,    /* pictuer encode, JPEG, etc */
	VAEntrypointVideoProc   = 10,   /* video pre/post processing */
	VAEntrypointMax
};

#define VA_RT_FORMAT_PROTECTED	0x80000000


/**
 *struct psb_context
 *
 *@buffers:	 array of pre-allocated validate buffers.
 *@used_buffers: number of buffers in @buffers array currently in use.
 *@validate_buffer: buffers validated from user-space.
 *@kern_validate_buffers : buffers validated from kernel-space.
 *@fence_flags : Fence flags to be used for fence creation.
 *
 *This structure is used during execbuf validation.
 */
struct psb_context {
	struct psb_validate_buffer *buffers;
	uint32_t used_buffers;
	struct list_head validate_list;
	/* not used:
	 * struct list_head kern_validate_list;
	 * uint32_t val_seq; */
	uint32_t fence_types;
};

struct psb_validate_buffer {
	struct ttm_validate_buffer base;
	struct psb_validate_req req;
	int ret;
	struct psb_validate_arg __user *user_val_arg;
	uint32_t flags;
	uint32_t offset;
	int po_correct;
};

#define	LOG2_WB_FIFO_SIZE	(5)
#define	WB_FIFO_SIZE		(1 << (LOG2_WB_FIFO_SIZE))

struct adaptive_intra_refresh_info_type{
       int8_t *air_table;
       int32_t air_per_frame;
       int16_t air_skip_cnt;
       uint16_t air_scan_pos;
       int32_t sad_threshold;
};

struct psb_video_ctx {
	struct list_head head;
	struct file *filp; /* DRM device file pointer */
	uint64_t ctx_type; /* (msvdx_tile&0xff)<<16|profile<<8|entrypoint */
	/* todo: more context specific data for multi-context support */
	/* Write back buffer object */
	struct ttm_buffer_object *wb_bo;
	struct ttm_bo_kmap_obj wb_bo_kmap;
	uint32_t *wb_addr[WB_FIFO_SIZE];
	/* Setvideo buffer object */
	struct ttm_buffer_object *mtx_ctx_bo;
	struct ttm_bo_kmap_obj mtx_ctx_kmap;
	uint32_t setv_addr;

	/* CIR parameters */
	struct ttm_buffer_object *cir_input_ctrl_bo;
	struct ttm_bo_kmap_obj cir_input_ctrl_kmap;
	uint32_t *cir_input_ctrl_addr;
	uint32_t pseudo_rand_seed;
	int32_t last_cir_index;

	/* AIR parameters */
	struct adaptive_intra_refresh_info_type air_info;
	struct ttm_buffer_object *bufs_f_p_out_params_bo;
	struct ttm_bo_kmap_obj bufs_f_p_out_params_kmap;
	uint32_t *bufs_f_p_out_params_addr;
	struct ttm_buffer_object *bufs_f_p_out_best_mp_param_bo;
	struct ttm_bo_kmap_obj bufs_f_p_out_best_mp_param_kmap;
	uint32_t *bufs_f_p_out_best_mp_param_addr;

	/* Save state registers */
	uint32_t *bias_reg;

	uint32_t status;
	uint32_t codec;
	uint32_t frame_count;
	/* Firmware data section offset and size */
	uint32_t mtx_debug_val;
	uint32_t mtx_bank_size;
	uint32_t mtx_ram_size;

	bool handle_sequence_needed;
	uint32_t cur_sequence;

	uint32_t low_cmd_count;
	uint32_t high_cmd_count;

	uint32_t enc_ctx_param;
	uint32_t enc_ctx_addr;
#ifdef CONFIG_SLICE_HEADER_PARSING
	uint32_t slice_extract_flag;
	uint32_t frame_boundary;
	uint32_t frame_end_seq;
	uint32_t frame_end;
	uint32_t copy_cmd_send;
#endif
};

#ifdef MERRIFIELD
struct psb_fpriv *psb_fpriv(struct drm_file *file_priv);
#endif

#endif
