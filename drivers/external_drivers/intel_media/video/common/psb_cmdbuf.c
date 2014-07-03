/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 * Copyright (c) 2008, Tungsten Graphics, Inc. Cedar Park, TX. USA.
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

#include <drm/drmP.h>
#ifdef CONFIG_DRM_VXD_BYT
#include "vxd_drv.h"
#include "vxd_drm.h"
#else
#include "psb_drv.h"
#include "psb_drm.h"
#include "psb_reg.h"
#ifdef MERRIFIELD
#include "tng_topaz.h"
#include "pwr_mgmt.h"
#else
#include "pnw_topaz.h"
#include "psb_powermgmt.h"
#endif
#include "psb_intel_reg.h"
#endif

#include "psb_msvdx.h"

#ifdef SUPPORT_MRST
#include "lnc_topaz.h"
#endif

#ifdef SUPPORT_VSP
#include "vsp.h"
#endif

#include "ttm/ttm_bo_api.h"
#include "ttm/ttm_execbuf_util.h"
#include "psb_ttm_userobj_api.h"
#include "ttm/ttm_placement.h"
#include "psb_video_drv.h"

static inline int psb_same_page(unsigned long offset,
				unsigned long offset2)
{
	return (offset & PAGE_MASK) == (offset2 & PAGE_MASK);
}

#if 0
static inline unsigned long psb_offset_end(unsigned long offset,
		unsigned long end)
{
	offset = (offset + PAGE_SIZE) & PAGE_MASK;
	return (end < offset) ? end : offset;
}
#endif

static void psb_idle_engine(struct drm_device *dev, int engine)
{
	/*Fix me add video engile support*/
	return;
}

struct psb_dstbuf_cache {
	unsigned int dst;
	struct ttm_buffer_object *dst_buf;
	unsigned long dst_offset;
	uint32_t *dst_page;
	unsigned int dst_page_offset;
	struct ttm_bo_kmap_obj dst_kmap;
	bool dst_is_iomem;
};

static int psb_check_presumed(struct psb_validate_req *req,
			      struct ttm_buffer_object *bo,
			      struct psb_validate_arg __user *data,
			      int *presumed_ok)
{
	struct psb_validate_req __user *user_req = &(data->d.req);

	*presumed_ok = 0;

	if (bo->mem.mem_type == TTM_PL_SYSTEM) {
		*presumed_ok = 1;
		return 0;
	}

	if (unlikely(!(req->presumed_flags & PSB_USE_PRESUMED)))
		return 0;

	if (bo->offset == req->presumed_gpu_offset) {
		*presumed_ok = 1;
		return 0;
	}

	return __put_user(req->presumed_flags & ~PSB_USE_PRESUMED,
			  &user_req->presumed_flags);
}


static void psb_unreference_buffers(struct psb_context *context)
{
	struct ttm_validate_buffer *entry, *next;
	struct psb_validate_buffer *vbuf;
	struct list_head *list = &context->validate_list;

	list_for_each_entry_safe(entry, next, list, head) {
		vbuf =
			container_of(entry, struct psb_validate_buffer, base);
		list_del(&entry->head);
		ttm_bo_unref(&entry->bo);
	}

	/*
	list = &context->kern_validate_list;

	list_for_each_entry_safe(entry, next, list, head) {
		vbuf =
			container_of(entry, struct psb_validate_buffer, base);
		list_del(&entry->head);
		ttm_bo_unref(&entry->bo);
	}
	*/
}

static int psb_lookup_validate_buffer(struct drm_file *file_priv,
				      uint64_t data,
				      struct psb_validate_buffer *item)
{
	struct ttm_object_file *tfile = psb_fpriv(file_priv)->tfile;

	item->user_val_arg =
		(struct psb_validate_arg __user *)(unsigned long) data;

	if (unlikely(copy_from_user(&item->req, &item->user_val_arg->d.req,
				    sizeof(item->req)) != 0)) {
		DRM_ERROR("Lookup copy fault.\n");
		return -EFAULT;
	}

	item->base.bo =
		ttm_buffer_object_lookup(tfile, item->req.buffer_handle);

	if (unlikely(item->base.bo == NULL)) {
		DRM_ERROR("Bo lookup fault.\n");
		return -EINVAL;
	}

	return 0;
}

static int psb_reference_buffers(struct drm_file *file_priv,
				 uint64_t data,
				 struct psb_context *context)
{
	struct psb_validate_buffer *item;
	int ret;

	while (likely(data != 0)) {
		if (unlikely(context->used_buffers >=
			     PSB_NUM_VALIDATE_BUFFERS)) {
			DRM_ERROR("Too many buffers "
				  "on validate list.\n");
			ret = -EINVAL;
			goto out_err0;
		}

		item = &context->buffers[context->used_buffers];

		ret = psb_lookup_validate_buffer(file_priv, data, item);
		if (unlikely(ret != 0))
			goto out_err0;

		item->base.reserved = 0;
		list_add_tail(&item->base.head, &context->validate_list);
		context->used_buffers++;
		data = item->req.next;
	}
	return 0;

out_err0:
	psb_unreference_buffers(context);
	return ret;
}

/* Todo: can simplified the code:
 * set_val_flags is hard code as (read | write) in user space
 * clr_flags is set as NULL
 * only one fence type is needed */
static int psb_placement_fence_type(struct ttm_buffer_object *bo,
						uint64_t set_val_flags,
						uint64_t clr_val_flags,
						uint32_t new_fence_class,
						uint32_t *new_fence_type)
{
	int ret;
	uint32_t n_fence_type;

	/*
	uint32_t set_flags = set_val_flags & 0xFFFFFFFF;
	uint32_t clr_flags = clr_val_flags & 0xFFFFFFFF;
	*/
	struct ttm_fence_object *old_fence;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	uint32_t old_fence_type;
#endif
	struct ttm_placement placement;

	if (unlikely
	    (!(set_val_flags &
	       (PSB_GPU_ACCESS_READ | PSB_GPU_ACCESS_WRITE)))) {
		DRM_ERROR
		("GPU access type (read / write) is not indicated.\n");
		return -EINVAL;
	}

	/* User space driver doesn't set any TTM placement flags in set_val_flags or clr_val_flags */
	placement.num_placement = 0;/* FIXME  */
	placement.num_busy_placement = 0;
	placement.fpfn = 0;
	placement.lpfn = 0;
	ret = psb_ttm_bo_check_placement(bo, &placement);
	if (unlikely(ret != 0))
		return ret;

	switch (new_fence_class) {
	default:
		n_fence_type = _PSB_FENCE_TYPE_EXE;
	}

	*new_fence_type = n_fence_type;
	old_fence = (struct ttm_fence_object *) bo->sync_obj;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	old_fence_type = (uint32_t)(unsigned long) bo->sync_obj_arg;

	if (old_fence && ((new_fence_class != old_fence->fence_class) ||
			  ((n_fence_type ^ old_fence_type) &
			   old_fence_type))) {
#else
	if (old_fence && ((new_fence_class != old_fence->fence_class))) {
#endif
		ret = ttm_bo_wait(bo, 0, 1, 0);
		if (unlikely(ret != 0))
			return ret;
	}
	/*
	bo->proposed_flags = (bo->proposed_flags | set_flags)
		& ~clr_flags & TTM_PL_MASK_MEMTYPE;
	*/
	return 0;
}

#if 0
int psb_validate_kernel_buffer(struct psb_context *context,
			       struct ttm_buffer_object *bo,
			       uint32_t fence_class,
			       uint64_t set_flags, uint64_t clr_flags)
{
	struct psb_validate_buffer *item;
	uint32_t cur_fence_type;
	int ret;

	if (unlikely(context->used_buffers >= PSB_NUM_VALIDATE_BUFFERS)) {
		DRM_ERROR("Out of free validation buffer entries for "
			  "kernel buffer validation.\n");
		return -ENOMEM;
	}

	item = &context->buffers[context->used_buffers];
	item->user_val_arg = NULL;
	item->base.reserved = 0;

	ret = ttm_bo_reserve(bo, 1, 0, 1, context->val_seq);
	if (unlikely(ret != 0))
		goto out_unlock;

	spin_lock(&bo->lock);
	ret = psb_placement_fence_type(bo, set_flags, clr_flags, fence_class,
				       &cur_fence_type);
	if (unlikely(ret != 0)) {
		ttm_bo_unreserve(bo);
		goto out_unlock;
	}

	item->base.bo = ttm_bo_reference(bo);
	item->base.new_sync_obj_arg = (void *)(unsigned long) cur_fence_type;
	item->base.reserved = 1;

	list_add_tail(&item->base.head, &context->kern_validate_list);
	context->used_buffers++;
	/*
	ret = ttm_bo_validate(bo, 1, 0, 0);
	if (unlikely(ret != 0))
		goto out_unlock;
	*/
	item->offset = bo->offset;
	item->flags = bo->mem.placement;
	context->fence_types |= cur_fence_type;

out_unlock:
	spin_unlock(&bo->lock);
	return ret;
}
#endif

static int psb_validate_buffer_list(struct drm_file *file_priv,
				    uint32_t fence_class,
				    struct psb_context *context,
				    int *po_correct,
				    struct psb_mmu_driver *psb_mmu)
{
	struct psb_validate_buffer *item;
	struct ttm_buffer_object *bo;
	int ret;
	struct psb_validate_req *req;
	uint32_t fence_types = 0;
	uint32_t cur_fence_type;
	struct ttm_validate_buffer *entry;
	struct list_head *list = &context->validate_list;
	struct ttm_placement placement;
	uint32_t flags;

	*po_correct = 1;

	list_for_each_entry(entry, list, head) {
		item = container_of(entry, struct psb_validate_buffer, base);
		bo = entry->bo;
		item->ret = 0;
		req = &item->req;

		spin_lock(&bo->bdev->fence_lock);
		ret = psb_placement_fence_type(bo,
					       req->set_flags,
					       req->clear_flags,
					       fence_class,
					       &cur_fence_type);
		if (unlikely(ret != 0))
			goto out_unlock;

		flags = item->req.pad64 | TTM_PL_FLAG_WC | TTM_PL_FLAG_UNCACHED;
		placement.num_placement = 1;
		placement.placement = &flags;
		placement.num_busy_placement = 1;
		placement.busy_placement = &flags;
		placement.fpfn = 0;
		placement.lpfn = 0;

		spin_unlock(&bo->bdev->fence_lock);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
		ret = ttm_bo_validate(bo, &placement, 1, 0, 0);
#else
		ret = ttm_bo_validate(bo, &placement, 1, 0);
#endif
		/* spin_lock(&bo->lock); */
		/* mem and offset field of bo is protected by ::reserve
		 * this function is called in reserve */
		if (unlikely(ret != 0))
			goto out_err;

#ifndef CONFIG_DRM_VXD_BYT
		if ((item->req.unfence_flag & PSB_MEM_CLFLUSH)) {
			ret = psb_ttm_bo_clflush(psb_mmu, bo);
			if (unlikely(!ret))
				PSB_DEBUG_WARN("clflush bo fail\n");
		}
#endif

		fence_types |= cur_fence_type;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
		entry->new_sync_obj_arg = (void *)
					  (unsigned long) cur_fence_type;
#endif

		item->offset = bo->offset;
		item->flags = bo->mem.placement;
		/* spin_unlock(&bo->lock); */

		ret = psb_check_presumed(&item->req, bo, item->user_val_arg,
					&item->po_correct);
		if (unlikely(ret != 0))
			goto out_err;

		if (unlikely(!item->po_correct))
			*po_correct = 0;

		item++;
	}

	context->fence_types |= fence_types;

	return 0;
out_unlock:
	spin_unlock(&bo->bdev->fence_lock);

out_err:
	/* spin_unlock(&bo->lock); */
	item->ret = ret;
	return ret;
}

static void psb_clear_dstbuf_cache(struct psb_dstbuf_cache *dst_cache)
{
	if (dst_cache->dst_page) {
		ttm_bo_kunmap(&dst_cache->dst_kmap);
		dst_cache->dst_page = NULL;
	}
	dst_cache->dst_buf = NULL;
	dst_cache->dst = ~0;
}

static int psb_update_dstbuf_cache(struct psb_dstbuf_cache *dst_cache,
				   struct psb_validate_buffer *buffers,
				   unsigned int dst,
				   unsigned long dst_offset)
{
	int ret;

	PSB_DEBUG_GENERAL("Destination buffer is %d.\n", dst);

	if (unlikely(dst != dst_cache->dst || NULL == dst_cache->dst_buf)) {
		psb_clear_dstbuf_cache(dst_cache);
		dst_cache->dst = dst;
		dst_cache->dst_buf = buffers[dst].base.bo;
	}

	if (unlikely
	    (dst_offset > dst_cache->dst_buf->num_pages * PAGE_SIZE)) {
		DRM_ERROR("Relocation destination out of bounds.\n");
		return -EINVAL;
	}

	if (!psb_same_page(dst_cache->dst_offset, dst_offset) ||
	    NULL == dst_cache->dst_page) {
		if (NULL != dst_cache->dst_page) {
			ttm_bo_kunmap(&dst_cache->dst_kmap);
			dst_cache->dst_page = NULL;
		}

		ret = ttm_bo_kmap(dst_cache->dst_buf,
				dst_offset >> PAGE_SHIFT, 1,
				&dst_cache->dst_kmap);
		if (ret) {
			DRM_ERROR("Could not map destination buffer for "
				  "relocation.\n");
			return ret;
		}

		dst_cache->dst_page =
			ttm_kmap_obj_virtual(&dst_cache->dst_kmap,
					     &dst_cache->dst_is_iomem);
		dst_cache->dst_offset = dst_offset & PAGE_MASK;
		dst_cache->dst_page_offset = dst_cache->dst_offset >> 2;
	}
	return 0;
}

static int psb_apply_reloc(struct drm_psb_private *dev_priv,
			   uint32_t fence_class,
			   const struct drm_psb_reloc *reloc,
			   struct psb_validate_buffer *buffers,
			   int num_buffers,
			   struct psb_dstbuf_cache *dst_cache,
			   int no_wait, int interruptible)
{
	uint32_t val;
	uint32_t background;
	unsigned int index;
	int ret;
	unsigned int shift;
	unsigned int align_shift;
	struct ttm_buffer_object *reloc_bo;


	PSB_DEBUG_GENERAL("Reloc type %d\n"
			  "\t where 0x%04x\n"
			  "\t buffer 0x%04x\n"
			  "\t mask 0x%08x\n"
			  "\t shift 0x%08x\n"
			  "\t pre_add 0x%08x\n"
			  "\t background 0x%08x\n"
			  "\t dst_buffer 0x%08x\n"
			  "\t arg0 0x%08x\n"
			  "\t arg1 0x%08x\n",
			  reloc->reloc_op,
			  reloc->where,
			  reloc->buffer,
			  reloc->mask,
			  reloc->shift,
			  reloc->pre_add,
			  reloc->background,
			  reloc->dst_buffer, reloc->arg0, reloc->arg1);

	if (unlikely(reloc->buffer >= num_buffers)) {
		DRM_ERROR("Illegal relocation buffer %d.\n",
			  reloc->buffer);
		return -EINVAL;
	}

	if (buffers[reloc->buffer].po_correct)
		return 0;

	if (unlikely(reloc->dst_buffer >= num_buffers)) {
		DRM_ERROR
		("Illegal destination buffer for relocation %d.\n",
		 reloc->dst_buffer);
		return -EINVAL;
	}

	ret = psb_update_dstbuf_cache(dst_cache, buffers,
					reloc->dst_buffer,
					reloc->where << 2);
	if (ret)
		return ret;

	reloc_bo = buffers[reloc->buffer].base.bo;

	if (unlikely(reloc->pre_add > (reloc_bo->num_pages << PAGE_SHIFT))) {
		DRM_ERROR("Illegal relocation offset add.\n");
		return -EINVAL;
	}

	switch (reloc->reloc_op) {
	case PSB_RELOC_OP_OFFSET:
		val = reloc_bo->offset + reloc->pre_add;
		break;
	default:
		DRM_ERROR("Unimplemented relocation.\n");
		return -EINVAL;
	}

	shift =
		(reloc->shift & PSB_RELOC_SHIFT_MASK) >> PSB_RELOC_SHIFT_SHIFT;
	align_shift =
		(reloc->
		 shift & PSB_RELOC_ALSHIFT_MASK) >> PSB_RELOC_ALSHIFT_SHIFT;

	val = ((val >> align_shift) << shift);
	index = reloc->where - dst_cache->dst_page_offset;

	background = reloc->background;
	val = (background & ~reloc->mask) | (val & reloc->mask);
	dst_cache->dst_page[index] = val;

	PSB_DEBUG_GENERAL("Reloc buffer %d index 0x%08x, value 0x%08x\n",
			  reloc->dst_buffer, index,
			  dst_cache->dst_page[index]);

	return 0;
}

static int psb_ok_to_map_reloc(struct drm_psb_private *dev_priv,
			       unsigned int num_pages)
{
	int ret = 0;

	spin_lock(&dev_priv->reloc_lock);
	if (dev_priv->rel_mapped_pages + num_pages <= PSB_MAX_RELOC_PAGES) {
		dev_priv->rel_mapped_pages += num_pages;
		ret = 1;
	}
	spin_unlock(&dev_priv->reloc_lock);
	return ret;
}

static int psb_fixup_relocs(struct drm_file *file_priv,
			    uint32_t fence_class,
			    unsigned int num_relocs,
			    unsigned int reloc_offset,
			    uint32_t reloc_handle,
			    struct psb_context *context,
			    int no_wait, int interruptible)
{
	struct drm_device *dev = file_priv->minor->dev;
	struct ttm_object_file *tfile = psb_fpriv(file_priv)->tfile;
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct ttm_buffer_object *reloc_buffer = NULL;
	unsigned int reloc_num_pages;
	unsigned int reloc_first_page;
	unsigned int reloc_last_page;
	struct psb_dstbuf_cache dst_cache;
	struct drm_psb_reloc *reloc;
	struct ttm_bo_kmap_obj reloc_kmap;
	bool reloc_is_iomem;
	int count;
	int ret = 0;
	int registered = 0;
	uint32_t num_buffers = context->used_buffers;

	if (num_relocs == 0)
		return 0;

	memset(&dst_cache, 0, sizeof(dst_cache));
	memset(&reloc_kmap, 0, sizeof(reloc_kmap));

	reloc_buffer = ttm_buffer_object_lookup(tfile, reloc_handle);
	if (!reloc_buffer)
		goto out;

	if (unlikely(atomic_read(&reloc_buffer->reserved) != 1)) {
		DRM_ERROR("Relocation buffer was not on validate list.\n");
		ret = -EINVAL;
		goto out;
	}

	reloc_first_page = reloc_offset >> PAGE_SHIFT;
	reloc_last_page =
		(reloc_offset +
		 num_relocs * sizeof(struct drm_psb_reloc)) >> PAGE_SHIFT;
	reloc_num_pages = reloc_last_page - reloc_first_page + 1;
	reloc_offset &= ~PAGE_MASK;

	if (reloc_num_pages > PSB_MAX_RELOC_PAGES) {
		DRM_ERROR("Relocation buffer is too large\n");
		ret = -EINVAL;
		goto out;
	}

	DRM_WAIT_ON(ret, dev_priv->rel_mapped_queue, 3 * DRM_HZ,
		    (registered =
			     psb_ok_to_map_reloc(dev_priv, reloc_num_pages)));

	if (ret == -EINTR) {
		ret = -ERESTART;
		goto out;
	}
	if (ret) {
		DRM_ERROR("Error waiting for space to map "
			  "relocation buffer.\n");
		goto out;
	}

	ret = ttm_bo_kmap(reloc_buffer, reloc_first_page,
			  reloc_num_pages, &reloc_kmap);

	if (ret) {
		DRM_ERROR("Could not map relocation buffer.\n"
			  "\tReloc buffer id 0x%08x.\n"
			  "\tReloc first page %d.\n"
			  "\tReloc num pages %d.\n",
			  reloc_handle, reloc_first_page, reloc_num_pages);
		goto out;
	}

	reloc = (struct drm_psb_reloc *)
		((unsigned long)
		 ttm_kmap_obj_virtual(&reloc_kmap,
				      &reloc_is_iomem) + reloc_offset);

	for (count = 0; count < num_relocs; ++count) {
		ret = psb_apply_reloc(dev_priv, fence_class,
				      reloc, context->buffers,
				      num_buffers, &dst_cache,
				      no_wait, interruptible);
		if (ret)
			goto out1;
		reloc++;
	}

out1:
	ttm_bo_kunmap(&reloc_kmap);
out:
	if (registered) {
		spin_lock(&dev_priv->reloc_lock);
		dev_priv->rel_mapped_pages -= reloc_num_pages;
		spin_unlock(&dev_priv->reloc_lock);
		DRM_WAKEUP(&dev_priv->rel_mapped_queue);
	}

	psb_clear_dstbuf_cache(&dst_cache);
	if (reloc_buffer)
		ttm_bo_unref(&reloc_buffer);
	return ret;
}

void psb_fence_or_sync(struct drm_file *file_priv,
		       uint32_t engine,
		       uint32_t fence_types,
		       uint32_t fence_flags,
		       struct list_head *list,
		       struct psb_ttm_fence_rep *fence_arg,
		       struct ttm_fence_object **fence_p)
{
	struct drm_device *dev = file_priv->minor->dev;
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct ttm_fence_device *fdev = &dev_priv->fdev;
	int ret;
	struct ttm_fence_object *fence;
	struct ttm_object_file *tfile = psb_fpriv(file_priv)->tfile;
	uint32_t handle;
	struct ttm_validate_buffer *entry, *next;
	struct ttm_bo_global *glob = dev_priv->bdev.glob;

	ret = ttm_fence_user_create(fdev, tfile,
				    engine, fence_types,
				    TTM_FENCE_FLAG_EMIT, &fence, &handle);
	if (ret) {

		/*
		 * Fence creation failed.
		 * Fall back to synchronous operation and idle the engine.
		 */

		psb_idle_engine(dev, engine);
		if (!(fence_flags & DRM_PSB_FENCE_NO_USER)) {

			/*
			 * Communicate to user-space that
			 * fence creation has failed and that
			 * the engine is idle.
			 */

			fence_arg->handle = ~0;
			fence_arg->error = ret;
		}

		ttm_eu_backoff_reservation(list);
		if (fence_p)
			*fence_p = NULL;
		return;
	}

#ifndef CONFIG_DRM_VXD_BYT
	spin_lock(&glob->lru_lock);
	list_for_each_entry_safe(entry, next, list, head) {
		struct psb_validate_buffer *vbuf =
			container_of(entry, struct psb_validate_buffer,
				     base);
		if (vbuf->req.unfence_flag & PSB_NOT_FENCE) {
			list_del(&entry->head);
			ttm_bo_unreserve_locked(entry->bo);
			ttm_bo_unref(&entry->bo);
			entry->reserved = false;
		}
	}
	spin_unlock(&glob->lru_lock);
#endif

	ttm_eu_fence_buffer_objects(list, fence);
	if (!(fence_flags & DRM_PSB_FENCE_NO_USER)) {
		struct ttm_fence_info info = ttm_fence_get_info(fence);
		fence_arg->handle = handle;
		fence_arg->fence_class = ttm_fence_class(fence);
		fence_arg->fence_type = ttm_fence_types(fence);
		fence_arg->signaled_types = info.signaled_types;
		fence_arg->error = 0;
	} else {
		ret = ttm_ref_object_base_unref(tfile, handle,
						ttm_fence_type);
		if (ret)
			DRM_ERROR("Failed to unref buffer object.\n");
	}

	if (fence_p)
		*fence_p = fence;
	else if (fence)
		ttm_fence_object_unref(&fence);
}


#if 0
static int psb_dump_page(struct ttm_buffer_object *bo,
			 unsigned int page_offset, unsigned int num)
{
	struct ttm_bo_kmap_obj kmobj;
	int is_iomem;
	uint32_t *p;
	int ret;
	unsigned int i;

	ret = ttm_bo_kmap(bo, page_offset, 1, &kmobj);
	if (ret)
		return ret;

	p = ttm_kmap_obj_virtual(&kmobj, &is_iomem);
	for (i = 0; i < num; ++i)
		PSB_DEBUG_GENERAL("0x%04x: 0x%08x\n", i, *p++);

	ttm_bo_kunmap(&kmobj);
	return 0;
}
#endif

static int psb_handle_copyback(struct drm_device *dev,
			       struct psb_context *context,
			       int ret)
{
	int err = ret;
	struct ttm_validate_buffer *entry;
	struct psb_validate_arg arg;
	struct list_head *list = &context->validate_list;

	if (ret) {
		ttm_eu_backoff_reservation(list);
		/* ttm_eu_backoff_reservation(&context->kern_validate_list); */
	}

	/* Todo: actually user space driver didn't hanlde the rep info */
	if (ret != -EAGAIN && ret != -EINTR && ret != -ERESTART) {
		list_for_each_entry(entry, list, head) {
			struct psb_validate_buffer *vbuf =
				container_of(entry, struct psb_validate_buffer,
					     base);
			arg.handled = 1;
			arg.ret = vbuf->ret;
			if (!arg.ret) {
				struct ttm_buffer_object *bo = entry->bo;
				/* spin_lock(&bo->lock); */
				/* offset and mem field of bo is protected by reserve */
				ret = ttm_bo_reserve(bo, 1, 0, 0, 0);
				if (unlikely(ret != 0))
					arg.ret = -EFAULT;
				arg.d.rep.gpu_offset = bo->offset;
				arg.d.rep.placement = bo->mem.placement;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
				arg.d.rep.fence_type_mask =
					(uint32_t)(unsigned long)
					entry->new_sync_obj_arg;
#else
				arg.d.rep.fence_type_mask = _PSB_FENCE_TYPE_EXE;
#endif
				ttm_bo_unreserve(bo);
				/* spin_unlock(&bo->lock); */
			}

			if (__copy_to_user(vbuf->user_val_arg,
					   &arg, sizeof(arg)))
				err = -EFAULT;

			if (arg.ret)
				break;
		}
	}

	return err;
}

int psb_cmdbuf_ioctl(struct drm_device *dev, void *data,
		     struct drm_file *file_priv)
{
	struct drm_psb_cmdbuf_arg *arg = data;
	int ret = 0;
	struct ttm_object_file *tfile = psb_fpriv(file_priv)->tfile;
	struct ttm_buffer_object *cmd_buffer = NULL;
	struct psb_ttm_fence_rep fence_arg;
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct psb_mmu_driver *mmu = NULL;
	struct msvdx_private *msvdx_priv = NULL;
#ifdef SUPPORT_VSP
	struct vsp_private *vsp_priv = NULL;
#endif
	struct psb_video_ctx *pos = NULL;
	struct psb_video_ctx *n = NULL;
	struct psb_video_ctx *msvdx_ctx = NULL;
	unsigned long irq_flags;
	if (dev_priv == NULL)
		return -EINVAL;
	mmu = dev_priv->mmu;
	msvdx_priv = dev_priv->msvdx_private;

#if defined(MERRIFIELD)
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;
#endif
	int engine, po_correct;
	int found = 0;
	struct psb_context *context = NULL;

#ifdef SUPPORT_VSP
	vsp_priv = dev_priv->vsp_private;
#endif

#if defined(MERRIFIELD)
	if (drm_topaz_cmdpolicy != PSB_CMDPOLICY_PARALLEL) {
		wait_event_interruptible(topaz_priv->cmd_wq, \
			(atomic_read(&topaz_priv->cmd_wq_free) == 1));
		atomic_set(&topaz_priv->cmd_wq_free, 0);
	}
#endif

	ret = ttm_read_lock(&dev_priv->ttm_lock, true);
	if (unlikely(ret != 0))
		return ret;

	if (arg->engine == PSB_ENGINE_DECODE) {
		if (msvdx_priv->fw_loaded_by_punit)
			psb_msvdx_check_reset_fw(dev);
#ifndef MERRIFIELD
		if (!ospm_power_using_video_begin(OSPM_VIDEO_DEC_ISLAND)) {
			ret = -EBUSY;
			goto out_err0;
		}
#endif
		ret = mutex_lock_interruptible(&msvdx_priv->msvdx_mutex);
		if (unlikely(ret != 0))
			goto out_err0;

		msvdx_priv->tfile = tfile;
		context = &dev_priv->decode_context;
	} else if (arg->engine == LNC_ENGINE_ENCODE) {
#ifndef CONFIG_DRM_VXD_BYT

		if (dev_priv->topaz_disabled) {
			ret = -ENODEV;
			goto out_err0;
		}
#ifndef MERRIFIELD
		if (!ospm_power_using_video_begin(OSPM_VIDEO_ENC_ISLAND)) {
			ret = -EBUSY;
			goto out_err0;
		}
#endif
		ret = mutex_lock_interruptible(&dev_priv->cmdbuf_mutex);
		if (unlikely(ret != 0))
			goto out_err0;
		context = &dev_priv->encode_context;
#endif
	} else if (arg->engine == VSP_ENGINE_VPP) {
#ifdef SUPPORT_VSP
		ret = mutex_lock_interruptible(&vsp_priv->vsp_mutex);
		if (unlikely(ret != 0))
			goto out_err0;

		context = &dev_priv->vsp_context;
#endif
	} else {
		ret = -EINVAL;
		goto out_err0;
	}

	if (context == NULL) {
		ret = -EINVAL;
		goto out_err0;
	}

	context->used_buffers = 0;
	context->fence_types = 0;
	if (!list_empty(&context->validate_list)) {
		DRM_ERROR("context->validate_list is not null.\n");
		ret = -EINVAL;
		goto out_err1;
	}

	/* BUG_ON(!list_empty(&context->kern_validate_list)); */

	if (unlikely(context->buffers == NULL)) {
		context->buffers = vmalloc(PSB_NUM_VALIDATE_BUFFERS *
					   sizeof(*context->buffers));
		if (unlikely(context->buffers == NULL)) {
			ret = -ENOMEM;
			goto out_err1;
		}
	}

	ret = psb_reference_buffers(file_priv,
				    arg->buffer_list,
				    context);
	if (unlikely(ret != 0))
		goto out_err1;

	/* Not used in K3 */
	/* context->val_seq = atomic_add_return(1, &dev_priv->val_seq); */

	ret = ttm_eu_reserve_buffers(&context->validate_list);
	if (unlikely(ret != 0))
		goto out_err2;

	engine = arg->engine;
	ret = psb_validate_buffer_list(file_priv, engine,
				       context, &po_correct, mmu);
	if (unlikely(ret != 0))
		goto out_err3;

	if (!po_correct) {
		ret = psb_fixup_relocs(file_priv, engine, arg->num_relocs,
				       arg->reloc_offset,
				       arg->reloc_handle, context, 0, 1);
		if (unlikely(ret != 0))
			goto out_err3;

	}

	cmd_buffer = ttm_buffer_object_lookup(tfile, arg->cmdbuf_handle);
	if (unlikely(cmd_buffer == NULL)) {
		ret = -EINVAL;
		goto out_err4;
	}

	spin_lock_irqsave(&dev_priv->video_ctx_lock, irq_flags);
	list_for_each_entry_safe(pos, n, &dev_priv->video_ctx, head) {
		if (pos->filp == file_priv->filp) {
			int entrypoint = pos->ctx_type & 0xff;

		PSB_DEBUG_GENERAL("cmds for profile %d, entrypoint %d\n",
					(pos->ctx_type >> 8) & 0xff,
					(pos->ctx_type & 0xff));

#ifndef CONFIG_DRM_VXD_BYT
			if (entrypoint == VAEntrypointEncSlice ||
			    entrypoint == VAEntrypointEncPicture)
				dev_priv->topaz_ctx = pos;
			else
#endif
			if (entrypoint != VAEntrypointVideoProc ||
				arg->engine == PSB_ENGINE_DECODE)
				msvdx_ctx = pos;
			found = 1;
			break;
		}
	}
	spin_unlock_irqrestore(&dev_priv->video_ctx_lock, irq_flags);

	if (!found) {
		PSB_DEBUG_WARN("WARN: video ctx is not found.\n");
		goto out_err4;
	}

	switch (arg->engine) {
	case PSB_ENGINE_DECODE:
		ret = psb_cmdbuf_video(file_priv, &context->validate_list,
				       context->fence_types, arg,
				       cmd_buffer, &fence_arg, msvdx_ctx);
		if (unlikely(ret != 0))
			goto out_err4;
		break;
#ifndef CONFIG_DRM_VXD_BYT
	case LNC_ENGINE_ENCODE:
#ifdef MERRIFIELD
		if (IS_MRFLD(dev))
			ret = tng_cmdbuf_video(
				file_priv, &context->validate_list,
				context->fence_types, arg,
				cmd_buffer, &fence_arg);
#else
		if (IS_MDFLD(dev))
			ret = pnw_cmdbuf_video(
				file_priv, &context->validate_list,
				context->fence_types, arg,
				cmd_buffer, &fence_arg);
#endif

		if (unlikely(ret != 0))
			goto out_err4;
		break;
#endif
	case VSP_ENGINE_VPP:
#ifdef SUPPORT_VSP
		ret = vsp_cmdbuf_vpp(file_priv, &context->validate_list,
				     context->fence_types, arg,
				     cmd_buffer, &fence_arg);

		if (unlikely(ret != 0))
			goto out_err4;
		break;
#endif
	default:
		DRM_ERROR
		("Unimplemented command submission mechanism (%x).\n",
		 arg->engine);
		ret = -EINVAL;
		goto out_err4;
	}

	if (!(arg->fence_flags & DRM_PSB_FENCE_NO_USER)) {
		ret = copy_to_user((void __user *)
				   ((unsigned long) arg->fence_arg),
				   &fence_arg, sizeof(fence_arg));
	}

out_err4:
	if (cmd_buffer)
		ttm_bo_unref(&cmd_buffer);
out_err3:
	ret = psb_handle_copyback(dev, context, ret);
out_err2:
	psb_unreference_buffers(context);
out_err1:
	if (arg->engine == PSB_ENGINE_DECODE)
		mutex_unlock(&msvdx_priv->msvdx_mutex);
	if (arg->engine == LNC_ENGINE_ENCODE)
		mutex_unlock(&dev_priv->cmdbuf_mutex);
#ifdef SUPPORT_VSP
	if (arg->engine == VSP_ENGINE_VPP)
		mutex_unlock(&vsp_priv->vsp_mutex);
#endif
out_err0:
	ttm_read_unlock(&dev_priv->ttm_lock);
#ifndef MERRIFIELD
	if (arg->engine == PSB_ENGINE_DECODE)
		ospm_power_using_video_end(OSPM_VIDEO_DEC_ISLAND);
#endif
#ifndef CONFIG_DRM_VXD_BYT
#ifndef MERRIFIELD
	if (arg->engine == LNC_ENGINE_ENCODE)
		ospm_power_using_video_end(OSPM_VIDEO_ENC_ISLAND);
#endif
#endif
	return ret;
}
