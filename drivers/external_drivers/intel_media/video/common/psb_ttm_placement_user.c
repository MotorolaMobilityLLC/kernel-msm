/**************************************************************************
 *
 * Copyright (c) 2006-2008 Tungsten Graphics, Inc., Cedar Park, TX., USA
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
/*
 * Authors: Thomas Hellstrom <thomas-at-tungstengraphics-dot-com>
 */

#include "psb_ttm_placement_user.h"
#include "ttm/ttm_bo_driver.h"
#include "ttm/ttm_object.h"
#include "psb_ttm_userobj_api.h"
#include "ttm/ttm_lock.h"
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/dma-buf.h>
#include "drmP.h"
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
#include "drm.h"
#else
#include <uapi/drm/drm.h>
#endif


struct ttm_bo_user_object {
	struct ttm_base_object base;
	struct ttm_buffer_object bo;
};

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
static size_t pl_bo_size;
#endif

static uint32_t psb_busy_prios[] = {
	TTM_PL_FLAG_TT | TTM_PL_FLAG_WC | TTM_PL_FLAG_UNCACHED,
	TTM_PL_FLAG_PRIV0, /* CI */
	TTM_PL_FLAG_PRIV2, /* IMR */
	TTM_PL_FLAG_PRIV1, /* DRM_PSB_MEM_MMU */
	TTM_PL_FLAG_SYSTEM
};

const struct ttm_placement default_placement = {0, 0, 0, NULL, 5, psb_busy_prios};

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
static size_t ttm_pl_size(struct ttm_bo_device *bdev, unsigned long num_pages)
{
	size_t page_array_size =
		(num_pages * sizeof(void *) + PAGE_SIZE - 1) & PAGE_MASK;

	if (unlikely(pl_bo_size == 0)) {
		pl_bo_size = bdev->glob->ttm_bo_extra_size +
			     ttm_round_pot(sizeof(struct ttm_bo_user_object));
	}

	return bdev->glob->ttm_bo_size + 2 * page_array_size;
}
#endif

static struct ttm_bo_user_object *ttm_bo_user_lookup(struct ttm_object_file
		*tfile, uint32_t handle) {
	struct ttm_base_object *base;

	base = ttm_base_object_lookup(tfile, handle);
	if (unlikely(base == NULL)) {
		printk(KERN_ERR "Invalid buffer object handle 0x%08lx.\n",
		       (unsigned long)handle);
		return NULL;
	}

	if (unlikely(base->object_type != ttm_buffer_type)) {
		ttm_base_object_unref(&base);
		printk(KERN_ERR "Invalid buffer object handle 0x%08lx.\n",
		       (unsigned long)handle);
		return NULL;
	}

	return container_of(base, struct ttm_bo_user_object, base);
}

struct ttm_buffer_object *ttm_buffer_object_lookup(struct ttm_object_file
		*tfile, uint32_t handle) {
	struct ttm_bo_user_object *user_bo;
	struct ttm_base_object *base;

	user_bo = ttm_bo_user_lookup(tfile, handle);
	if (unlikely(user_bo == NULL))
		return NULL;

	(void)ttm_bo_reference(&user_bo->bo);
	base = &user_bo->base;
	ttm_base_object_unref(&base);
	return &user_bo->bo;
}

static void ttm_bo_user_destroy(struct ttm_buffer_object *bo)
{
	struct ttm_bo_user_object *user_bo =
		container_of(bo, struct ttm_bo_user_object, bo);

	ttm_mem_global_free(bo->glob->mem_glob, bo->acc_size);
	kfree(user_bo);
}

/* This is used for sg_table which is derived from user-pointer */
static void ttm_tt_free_user_pages(struct ttm_buffer_object *bo)
{
	struct page *page;
	struct page **pages = NULL;
	int i, ret;
/*
	struct page **pages_to_wb;

	pages_to_wb = kmalloc(ttm->num_pages * sizeof(struct page *),
			GFP_KERNEL);

	if (pages_to_wb && ttm->caching_state != tt_cached) {
		int num_pages_wb = 0;

		for (i = 0; i < ttm->num_pages; ++i) {
			page = ttm->pages[i];
			if (page == NULL)
				continue;
			pages_to_wb[num_pages_wb++] = page;
		}

		if (set_pages_array_wb(pages_to_wb, num_pages_wb))
			printk(KERN_ERR TTM_PFX "Failed to set pages to wb\n");

	} else if (NULL == pages_to_wb) {
		printk(KERN_ERR TTM_PFX
		       "Failed to allocate memory for set wb operation.\n");
	}

*/
	pages = kzalloc(bo->num_pages * sizeof(struct page *), GFP_KERNEL);
	if (unlikely(pages == NULL)) {
		printk(KERN_ERR "TTM bo free: kzalloc failed\n");
		return ;
	}

	ret = drm_prime_sg_to_page_addr_arrays(bo->sg, pages,
						 NULL, bo->num_pages);
	if (ret) {
		printk(KERN_ERR "sg to pages: kzalloc failed\n");
		return ;
	}

	for (i = 0; i < bo->num_pages; ++i) {
		page = pages[i];
		if (page == NULL)
			continue;

		put_page(page);
	}
	/* kfree(pages_to_wb); */
	kfree(pages);
}

/* This is used for sg_table which is derived from user-pointer */
static void ttm_ub_bo_user_destroy(struct ttm_buffer_object *bo)
{
	struct ttm_bo_user_object *user_bo =
		container_of(bo, struct ttm_bo_user_object, bo);

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 3, 0))
	if (bo->sg) {
		ttm_tt_free_user_pages(bo);
		sg_free_table(bo->sg);
		kfree(bo->sg);
		bo->sg = NULL;
	}
#endif

	ttm_mem_global_free(bo->glob->mem_glob, bo->acc_size);
	kfree(user_bo);
}

static void ttm_bo_user_release(struct ttm_base_object **p_base)
{
	struct ttm_bo_user_object *user_bo;
	struct ttm_base_object *base = *p_base;
	struct ttm_buffer_object *bo;

	*p_base = NULL;

	if (unlikely(base == NULL))
		return;

	user_bo = container_of(base, struct ttm_bo_user_object, base);
	bo = &user_bo->bo;
	ttm_bo_unref(&bo);
}

static void ttm_bo_user_ref_release(struct ttm_base_object *base,
				    enum ttm_ref_type ref_type)
{
	struct ttm_bo_user_object *user_bo =
		container_of(base, struct ttm_bo_user_object, base);
	struct ttm_buffer_object *bo = &user_bo->bo;

	switch (ref_type) {
	case TTM_REF_SYNCCPU_WRITE:
		ttm_bo_synccpu_write_release(bo);
		break;
	default:
		BUG();
	}
}

static void ttm_pl_fill_rep(struct ttm_buffer_object *bo,
			    struct ttm_pl_rep *rep)
{
	struct ttm_bo_user_object *user_bo =
		container_of(bo, struct ttm_bo_user_object, bo);

	rep->gpu_offset = bo->offset;
	rep->bo_size = bo->num_pages << PAGE_SHIFT;
	rep->map_handle = bo->addr_space_offset;
	rep->placement = bo->mem.placement;
	rep->handle = user_bo->base.hash.key;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	rep->sync_object_arg = (uint32_t)(unsigned long)bo->sync_obj_arg;
#endif
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
/* FIXME Copy from upstream TTM */
static inline size_t ttm_bo_size(struct ttm_bo_global *glob,
				 unsigned long num_pages)
{
	size_t page_array_size = (num_pages * sizeof(void *) + PAGE_SIZE - 1) &
				 PAGE_MASK;

	return glob->ttm_bo_size + 2 * page_array_size;
}
#endif /* if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)) */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
/* FIXME Copy from upstream TTM "ttm_bo_create", upstream TTM does not export this, so copy it here */
static int ttm_bo_create_private(struct ttm_bo_device *bdev,
				 unsigned long size,
				 enum ttm_bo_type type,
				 struct ttm_placement *placement,
				 uint32_t page_alignment,
				 unsigned long buffer_start,
				 bool interruptible,
				 struct file *persistent_swap_storage,
				 struct ttm_buffer_object **p_bo)
{
	struct ttm_buffer_object *bo;
	struct ttm_mem_global *mem_glob = bdev->glob->mem_glob;
	int ret;

	size_t acc_size =
		ttm_bo_size(bdev->glob, (size + PAGE_SIZE - 1) >> PAGE_SHIFT);
	ret = ttm_mem_global_alloc(mem_glob, acc_size, false, false);
	if (unlikely(ret != 0))
		return ret;

	bo = kzalloc(sizeof(*bo), GFP_KERNEL);

	if (unlikely(bo == NULL)) {
		ttm_mem_global_free(mem_glob, acc_size);
		return -ENOMEM;
	}

	ret = ttm_bo_init(bdev, bo, size, type, placement, page_alignment,
			  buffer_start, interruptible,
			  persistent_swap_storage, acc_size, NULL);
	if (likely(ret == 0))
		*p_bo = bo;

	return ret;
}
#endif /* if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)) */

int psb_ttm_bo_check_placement(struct ttm_buffer_object *bo,
			       struct ttm_placement *placement)
{
	int i;

	for (i = 0; i < placement->num_placement; i++) {
		if (!capable(CAP_SYS_ADMIN)) {
			if (placement->placement[i] & TTM_PL_FLAG_NO_EVICT) {
				printk(KERN_ERR TTM_PFX "Need to be root to "
				       "modify NO_EVICT status.\n");
				return -EINVAL;
			}
		}
	}
	for (i = 0; i < placement->num_busy_placement; i++) {
		if (!capable(CAP_SYS_ADMIN)) {
			if (placement->busy_placement[i] & TTM_PL_FLAG_NO_EVICT) {
				printk(KERN_ERR TTM_PFX "Need to be root to "
				       "modify NO_EVICT status.\n");
				return -EINVAL;
			}
		}
	}
	return 0;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
int ttm_buffer_object_create(struct ttm_bo_device *bdev,
			     unsigned long size,
			     enum ttm_bo_type type,
			     uint32_t flags,
			     uint32_t page_alignment,
			     unsigned long buffer_start,
			     bool interruptible,
			     struct file *persistent_swap_storage,
			     struct ttm_buffer_object **p_bo)
{
	struct ttm_placement placement = default_placement;
	int ret;

	if ((flags & TTM_PL_MASK_CACHING) == 0)
		flags |= TTM_PL_FLAG_WC | TTM_PL_FLAG_UNCACHED;

	placement.num_placement = 1;
	placement.placement = &flags;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
	ret = ttm_bo_create_private(bdev, size, type, &placement,
		page_alignment, buffer_start, interruptible,
		persistent_swap_storage, p_bo);
#else
	ret = ttm_bo_create(bdev, size, type, &placement, page_alignment,
		buffer_start, interruptible, persistent_swap_storage, p_bo);
#endif

	return ret;
}
#else
int ttm_buffer_object_create(struct ttm_bo_device *bdev,
			     unsigned long size,
			     enum ttm_bo_type type,
			     uint32_t flags,
			     uint32_t page_alignment,
			     bool interruptible,
			     struct file *persistent_swap_storage,
			     struct ttm_buffer_object **p_bo)
{
	struct ttm_placement placement = default_placement;
	int ret;

	if ((flags & TTM_PL_MASK_CACHING) == 0)
		flags |= TTM_PL_FLAG_WC | TTM_PL_FLAG_UNCACHED;

	placement.num_placement = 1;
	placement.placement = &flags;

	ret = ttm_bo_create(bdev, size, type, &placement, page_alignment,
		interruptible, persistent_swap_storage, p_bo);

	return ret;
}
#endif


int ttm_pl_create_ioctl(struct ttm_object_file *tfile,
			struct ttm_bo_device *bdev,
			struct ttm_lock *lock, void *data)
{
	union ttm_pl_create_arg *arg = data;
	struct ttm_pl_create_req *req = &arg->req;
	struct ttm_pl_rep *rep = &arg->rep;
	struct ttm_buffer_object *bo;
	struct ttm_buffer_object *tmp;
	struct ttm_bo_user_object *user_bo;
	uint32_t flags;
	int ret = 0;
	struct ttm_mem_global *mem_glob = bdev->glob->mem_glob;
	struct ttm_placement placement = default_placement;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
	size_t acc_size =
		ttm_pl_size(bdev, (req->size + PAGE_SIZE - 1) >> PAGE_SHIFT);
#else
	size_t acc_size = ttm_bo_acc_size(bdev, req->size,
		sizeof(struct ttm_buffer_object));
#endif
	ret = ttm_mem_global_alloc(mem_glob, acc_size, false, false);
	if (unlikely(ret != 0))
		return ret;

	flags = req->placement;
	user_bo = kzalloc(sizeof(*user_bo), GFP_KERNEL);
	if (unlikely(user_bo == NULL)) {
		ttm_mem_global_free(mem_glob, acc_size);
		return -ENOMEM;
	}

	bo = &user_bo->bo;
	ret = ttm_read_lock(lock, true);
	if (unlikely(ret != 0)) {
		ttm_mem_global_free(mem_glob, acc_size);
		kfree(user_bo);
		return ret;
	}

	placement.num_placement = 1;
	placement.placement = &flags;

	if ((flags & TTM_PL_MASK_CACHING) == 0)
		flags |=  TTM_PL_FLAG_WC | TTM_PL_FLAG_UNCACHED;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	ret = ttm_bo_init(bdev, bo, req->size,
			  ttm_bo_type_device, &placement,
			  req->page_alignment, 0, true,
			  NULL, acc_size, NULL, &ttm_bo_user_destroy);
#else
	ret = ttm_bo_init(bdev, bo, req->size,
			  ttm_bo_type_device, &placement,
			  req->page_alignment, true,
			  NULL, acc_size, NULL, &ttm_bo_user_destroy);
#endif
	ttm_read_unlock(lock);
	/*
	 * Note that the ttm_buffer_object_init function
	 * would've called the destroy function on failure!!
	 */

	if (unlikely(ret != 0))
		goto out;

	tmp = ttm_bo_reference(bo);
	ret = ttm_base_object_init(tfile, &user_bo->base,
				   flags & TTM_PL_FLAG_SHARED,
				   ttm_buffer_type,
				   &ttm_bo_user_release,
				   &ttm_bo_user_ref_release);
	if (unlikely(ret != 0))
		goto out_err;

	ret = ttm_bo_reserve(bo, true, false, false, 0);
	if (unlikely(ret != 0))
		goto out_err;
	ttm_pl_fill_rep(bo, rep);
	ttm_bo_unreserve(bo);
	ttm_bo_unref(&bo);
out:
	return 0;
out_err:
	ttm_bo_unref(&tmp);
	ttm_bo_unref(&bo);
	return ret;
}

int ttm_pl_ub_create_ioctl(struct ttm_object_file *tfile,
			   struct ttm_bo_device *bdev,
			   struct ttm_lock *lock, void *data)
{
	union ttm_pl_create_ub_arg *arg = data;
	struct ttm_pl_create_ub_req *req = &arg->req;
	struct ttm_pl_rep *rep = &arg->rep;
	struct ttm_buffer_object *bo;
	struct ttm_buffer_object *tmp;
	struct ttm_bo_user_object *user_bo;
	uint32_t flags;
	int ret = 0;
	struct ttm_mem_global *mem_glob = bdev->glob->mem_glob;
	struct ttm_placement placement = default_placement;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
	size_t acc_size =
		ttm_pl_size(bdev, (req->size + PAGE_SIZE - 1) >> PAGE_SHIFT);
#else
	size_t acc_size = ttm_bo_acc_size(bdev, req->size,
		sizeof(struct ttm_buffer_object));
#endif
	if (req->user_address & ~PAGE_MASK) {
		printk(KERN_ERR "User pointer buffer need page alignment\n");
		return -EFAULT;
	}

	ret = ttm_mem_global_alloc(mem_glob, acc_size, false, false);
	if (unlikely(ret != 0))
		return ret;

	flags = req->placement;
	user_bo = kzalloc(sizeof(*user_bo), GFP_KERNEL);
	if (unlikely(user_bo == NULL)) {
		ttm_mem_global_free(mem_glob, acc_size);
		return -ENOMEM;
	}
	ret = ttm_read_lock(lock, true);
	if (unlikely(ret != 0)) {
		ttm_mem_global_free(mem_glob, acc_size);
		kfree(user_bo);
		return ret;
	}
	bo = &user_bo->bo;

	placement.num_placement = 1;
	placement.placement = &flags;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))

/*  For kernel 3.0, use the desired type. */
#define TTM_HACK_WORKAROUND_ttm_bo_type_user ttm_bo_type_user

#else
/*  TTM_HACK_WORKAROUND_ttm_bo_type_user -- Hack for porting,
    as ttm_bo_type_user is no longer implemented.
    This will not result in working code.
    FIXME - to be removed. */

#warning warning: ttm_bo_type_user no longer supported

/*  For kernel 3.3+, use the wrong type, which will compile but not work. */
#define TTM_HACK_WORKAROUND_ttm_bo_type_user ttm_bo_type_kernel

#endif

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 3, 0))
		/* Handle frame buffer allocated in user space, Convert
		  user space virtual address into pages list */
		unsigned int page_nr = 0;
		struct vm_area_struct *vma = NULL;
		struct sg_table *sg = NULL;
		unsigned long num_pages = 0;
		struct page **pages = 0;

		num_pages = (req->size + PAGE_SIZE - 1) >> PAGE_SHIFT;
		pages = kzalloc(num_pages * sizeof(struct page *), GFP_KERNEL);
		if (unlikely(pages == NULL)) {
			printk(KERN_ERR "kzalloc pages failed\n");
			return -ENOMEM;
		}

		down_read(&current->mm->mmap_sem);
		vma = find_vma(current->mm, req->user_address);
		if (unlikely(vma == NULL)) {
			up_read(&current->mm->mmap_sem);
			kfree(pages);
			printk(KERN_ERR "find_vma failed\n");
			return -EFAULT;
		}
		unsigned long before_flags = vma->vm_flags;
		if (vma->vm_flags & (VM_IO | VM_PFNMAP))
			vma->vm_flags = vma->vm_flags & ((~VM_IO) & (~VM_PFNMAP));
		page_nr = get_user_pages(current, current->mm,
					 req->user_address,
					 (int)(num_pages), 1, 0, pages,
					 NULL);
		vma->vm_flags = before_flags;
		up_read(&current->mm->mmap_sem);

		/* can be written by caller, not forced */
		if (unlikely(page_nr < num_pages)) {
			kfree(pages);
			pages = 0;
			printk(KERN_ERR "get_user_pages err.\n");
			return -ENOMEM;
		}
		sg = drm_prime_pages_to_sg(pages, num_pages);
		if (unlikely(sg == NULL)) {
			kfree(pages);
			printk(KERN_ERR "drm_prime_pages_to_sg err.\n");
			return -ENOMEM;
		}
		kfree(pages);
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0))
	ret = ttm_bo_init(bdev,
			  bo,
			  req->size,
			  TTM_HACK_WORKAROUND_ttm_bo_type_user,
			  &placement,
			  req->page_alignment,
			  req->user_address,
			  true,
			  NULL,
			  acc_size,
			  NULL,
			  &ttm_bo_user_destroy);
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	ret = ttm_bo_init(bdev,
			  bo,
			  req->size,
			  ttm_bo_type_sg,
			  &placement,
			  req->page_alignment,
			  req->user_address,
			  true,
			  NULL,
			  acc_size,
			  sg,
			  &ttm_ub_bo_user_destroy);
#else
	ret = ttm_bo_init(bdev,
			  bo,
			  req->size,
			  ttm_bo_type_sg,
			  &placement,
			  req->page_alignment,
			  true,
			  NULL,
			  acc_size,
			  sg,
			  &ttm_ub_bo_user_destroy);
#endif

	/*
	 * Note that the ttm_buffer_object_init function
	 * would've called the destroy function on failure!!
	 */
	ttm_read_unlock(lock);
	if (unlikely(ret != 0))
		goto out;

	tmp = ttm_bo_reference(bo);
	ret = ttm_base_object_init(tfile, &user_bo->base,
				   flags & TTM_PL_FLAG_SHARED,
				   ttm_buffer_type,
				   &ttm_bo_user_release,
				   &ttm_bo_user_ref_release);
	if (unlikely(ret != 0))
		goto out_err;

	ret = ttm_bo_reserve(bo, true, false, false, 0);
	if (unlikely(ret != 0))
		goto out_err;
	ttm_pl_fill_rep(bo, rep);
	ttm_bo_unreserve(bo);
	ttm_bo_unref(&bo);
out:
	return 0;
out_err:
	ttm_bo_unref(&tmp);
	ttm_bo_unref(&bo);
	return ret;
}

int ttm_pl_reference_ioctl(struct ttm_object_file *tfile, void *data)
{
	union ttm_pl_reference_arg *arg = data;
	struct ttm_pl_rep *rep = &arg->rep;
	struct ttm_bo_user_object *user_bo;
	struct ttm_buffer_object *bo;
	struct ttm_base_object *base;
	int ret;

	user_bo = ttm_bo_user_lookup(tfile, arg->req.handle);
	if (unlikely(user_bo == NULL)) {
		printk(KERN_ERR "Could not reference buffer object.\n");
		return -EINVAL;
	}

	bo = &user_bo->bo;
	ret = ttm_ref_object_add(tfile, &user_bo->base, TTM_REF_USAGE, NULL);
	if (unlikely(ret != 0)) {
		printk(KERN_ERR
		       "Could not add a reference to buffer object.\n");
		goto out;
	}

	ret = ttm_bo_reserve(bo, true, false, false, 0);
	if (unlikely(ret != 0))
		goto out;
	ttm_pl_fill_rep(bo, rep);
	ttm_bo_unreserve(bo);

out:
	base = &user_bo->base;
	ttm_base_object_unref(&base);
	return ret;
}

int ttm_pl_unref_ioctl(struct ttm_object_file *tfile, void *data)
{
	struct ttm_pl_reference_req *arg = data;

	return ttm_ref_object_base_unref(tfile, arg->handle, TTM_REF_USAGE);
}

int ttm_pl_synccpu_ioctl(struct ttm_object_file *tfile, void *data)
{
	struct ttm_pl_synccpu_arg *arg = data;
	struct ttm_bo_user_object *user_bo;
	struct ttm_buffer_object *bo;
	struct ttm_base_object *base;
	bool existed;
	int ret;

	switch (arg->op) {
	case TTM_PL_SYNCCPU_OP_GRAB:
		user_bo = ttm_bo_user_lookup(tfile, arg->handle);
		if (unlikely(user_bo == NULL)) {
			printk(KERN_ERR
			       "Could not find buffer object for synccpu.\n");
			return -EINVAL;
		}
		bo = &user_bo->bo;
		base = &user_bo->base;
		ret = ttm_bo_synccpu_write_grab(bo,
						arg->access_mode &
						TTM_PL_SYNCCPU_MODE_NO_BLOCK);
		if (unlikely(ret != 0)) {
			ttm_base_object_unref(&base);
			goto out;
		}
		ret = ttm_ref_object_add(tfile, &user_bo->base,
					 TTM_REF_SYNCCPU_WRITE, &existed);
		if (existed || ret != 0)
			ttm_bo_synccpu_write_release(bo);
		ttm_base_object_unref(&base);
		break;
	case TTM_PL_SYNCCPU_OP_RELEASE:
		ret = ttm_ref_object_base_unref(tfile, arg->handle,
						TTM_REF_SYNCCPU_WRITE);
		break;
	default:
		ret = -EINVAL;
		break;
	}
out:
	return ret;
}

int ttm_pl_setstatus_ioctl(struct ttm_object_file *tfile,
			   struct ttm_lock *lock, void *data)
{
	union ttm_pl_setstatus_arg *arg = data;
	struct ttm_pl_setstatus_req *req = &arg->req;
	struct ttm_pl_rep *rep = &arg->rep;
	struct ttm_buffer_object *bo;
	struct ttm_bo_device *bdev;
	struct ttm_placement placement = default_placement;
	uint32_t flags[2];
	int ret;

	bo = ttm_buffer_object_lookup(tfile, req->handle);
	if (unlikely(bo == NULL)) {
		printk(KERN_ERR
		       "Could not find buffer object for setstatus.\n");
		return -EINVAL;
	}

	bdev = bo->bdev;

	ret = ttm_read_lock(lock, true);
	if (unlikely(ret != 0))
		goto out_err0;

	ret = ttm_bo_reserve(bo, true, false, false, 0);
	if (unlikely(ret != 0))
		goto out_err1;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	ret = ttm_bo_wait_cpu(bo, false);
	if (unlikely(ret != 0))
		goto out_err2;
#endif

	flags[0] = req->set_placement;
	flags[1] = req->clr_placement;

	placement.num_placement = 2;
	placement.placement = flags;

	/* spin_lock(&bo->lock); */ /* Already get reserve lock */

	ret = psb_ttm_bo_check_placement(bo, &placement);
	if (unlikely(ret != 0))
		goto out_err2;

	placement.num_placement = 1;
	flags[0] = (req->set_placement | bo->mem.placement) & ~req->clr_placement;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	ret = ttm_bo_validate(bo, &placement, true, false, false);
#else
	ret = ttm_bo_validate(bo, &placement, true, false);
#endif
	if (unlikely(ret != 0))
		goto out_err2;

	ttm_pl_fill_rep(bo, rep);
out_err2:
	/* spin_unlock(&bo->lock); */
	ttm_bo_unreserve(bo);
out_err1:
	ttm_read_unlock(lock);
out_err0:
	ttm_bo_unref(&bo);
	return ret;
}

static int psb_ttm_bo_block_reservation(struct ttm_buffer_object *bo, bool interruptible,
					bool no_wait)
{
	int ret;

	while (unlikely(atomic_cmpxchg(&bo->reserved, 0, 1) != 0)) {
		if (no_wait)
			return -EBUSY;
		else if (interruptible) {
			ret = wait_event_interruptible
			      (bo->event_queue, atomic_read(&bo->reserved) == 0);
			if (unlikely(ret != 0))
				return -ERESTART;
		} else {
			wait_event(bo->event_queue,
				   atomic_read(&bo->reserved) == 0);
		}
	}
	return 0;
}

static void psb_ttm_bo_unblock_reservation(struct ttm_buffer_object *bo)
{
	atomic_set(&bo->reserved, 0);
	wake_up_all(&bo->event_queue);
}

int ttm_pl_waitidle_ioctl(struct ttm_object_file *tfile, void *data)
{
	struct ttm_pl_waitidle_arg *arg = data;
	struct ttm_buffer_object *bo;
	int ret;

	bo = ttm_buffer_object_lookup(tfile, arg->handle);
	if (unlikely(bo == NULL)) {
		printk(KERN_ERR "Could not find buffer object for waitidle.\n");
		return -EINVAL;
	}

	ret =
		psb_ttm_bo_block_reservation(bo, true,
					     arg->mode & TTM_PL_WAITIDLE_MODE_NO_BLOCK);
	if (unlikely(ret != 0))
		goto out;
	spin_lock(&bo->bdev->fence_lock);
	ret = ttm_bo_wait(bo,
			  arg->mode & TTM_PL_WAITIDLE_MODE_LAZY,
			  true, arg->mode & TTM_PL_WAITIDLE_MODE_NO_BLOCK);
	spin_unlock(&bo->bdev->fence_lock);
	psb_ttm_bo_unblock_reservation(bo);
out:
	ttm_bo_unref(&bo);
	return ret;
}

int ttm_pl_verify_access(struct ttm_buffer_object *bo,
			 struct ttm_object_file *tfile)
{
	struct ttm_bo_user_object *ubo;

	/*
	 * Check bo subclass.
	 */

	if (unlikely(bo->destroy != &ttm_bo_user_destroy
		&& bo->destroy != &ttm_ub_bo_user_destroy))
		return -EPERM;

	ubo = container_of(bo, struct ttm_bo_user_object, bo);
	if (likely(ubo->base.shareable || ubo->base.tfile == tfile))
		return 0;

	return -EPERM;
}
