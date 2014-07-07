/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
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
 * Authors: Thomas Hellstrom <thomas-at-tungstengraphics.com>
 */
#include <ttm/ttm_placement.h>
#include <ttm/ttm_execbuf_util.h>
#include <ttm/ttm_page_alloc.h>
#include "psb_ttm_fence_api.h"
#include <drm/drmP.h>
#ifdef CONFIG_DRM_VXD_BYT
#include "vxd_drv.h"
#else
#include "psb_drv.h"
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
struct drm_psb_ttm_backend {
	struct ttm_backend base;
	struct page **pages;
	dma_addr_t *dma_addrs;
	unsigned int desired_tile_stride;
	unsigned int hw_tile_stride;
	int mem_type;
	unsigned long offset;
	unsigned long num_pages;
};

#else

/*  ttm removal of "backend" -- struct ttm_backend changed to struct ttm_tt.
    Members now in struct ttm instead of struct ttm_backend:
	pages
	num_pages
    */

struct drm_psb_ttm_tt_s {
	struct ttm_dma_tt ttm_dma;
	unsigned int desired_tile_stride;
	unsigned int hw_tile_stride;
	int mem_type;
	unsigned long offset;
};
#endif

static int psb_move_blit(struct ttm_buffer_object *bo,
			 bool evict, bool no_wait,
			 struct ttm_mem_reg *new_mem)
{
	BUG();
	return 0;
}

/*
 * Flip destination ttm into GATT,
 * then blit and subsequently move out again.
 */
static int psb_move_flip(struct ttm_buffer_object *bo,
			 bool evict, bool interruptible, bool no_wait,
			 struct ttm_mem_reg *new_mem)
{
	/*struct ttm_bo_device *bdev = bo->bdev;*/
	struct ttm_mem_reg tmp_mem;
	int ret;
	struct ttm_placement placement;
	uint32_t flags = TTM_PL_FLAG_TT;

	tmp_mem = *new_mem;
	tmp_mem.mm_node = NULL;

	placement.fpfn = 0;
	placement.lpfn = 0;
	placement.num_placement = 1;
	placement.placement = &flags;
	placement.num_busy_placement = 0; /* FIXME */
	placement.busy_placement = NULL;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	ret = ttm_bo_mem_space(bo, &placement, &tmp_mem, interruptible, false, no_wait);
#else
	ret = ttm_bo_mem_space(bo, &placement, &tmp_mem, interruptible, no_wait);
#endif
	if (ret)
		return ret;
	ret = ttm_tt_bind(bo->ttm, &tmp_mem);
	if (ret)
		goto out_cleanup;
	ret = psb_move_blit(bo, true, no_wait, &tmp_mem);
	if (ret)
		goto out_cleanup;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	ret = ttm_bo_move_ttm(bo, evict, false, no_wait, new_mem);
#else
	ret = ttm_bo_move_ttm(bo, evict, no_wait, new_mem);
#endif
out_cleanup:
	if (tmp_mem.mm_node) {
		/*spin_lock(&bdev->lru_lock);*/ /* lru_lock is removed from upstream TTM */
		drm_mm_put_block(tmp_mem.mm_node);
		tmp_mem.mm_node = NULL;
		/*spin_unlock(&bdev->lru_lock);*/
	}
	return ret;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
static int drm_psb_tbe_populate(struct ttm_backend *backend,
				unsigned long num_pages,
				struct page **pages,
				struct page *dummy_read_page,
				dma_addr_t *dma_addrs)
{
	struct drm_psb_ttm_backend *psb_be =
		container_of(backend, struct drm_psb_ttm_backend, base);

	psb_be->pages = pages;
	psb_be->dma_addrs = dma_addrs; /* Not concretely implemented by TTM yet*/
	return 0;
}
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
static void drm_psb_tbe_clear(struct ttm_backend *backend)
{
	struct drm_psb_ttm_backend *psb_be =
		container_of(backend, struct drm_psb_ttm_backend, base);

	psb_be->pages = NULL;
	psb_be->dma_addrs = NULL;
	return;
}

static int drm_psb_tbe_unbind(struct ttm_backend *backend)
#else
static int drm_psb_tbe_unbind(struct ttm_tt *ttm)
#endif
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
	struct ttm_bo_device *bdev = backend->bdev;
	struct drm_psb_private *dev_priv =
		container_of(bdev, struct drm_psb_private, bdev);
	struct drm_psb_ttm_backend *psb_be =
		container_of(backend, struct drm_psb_ttm_backend, base);
	struct psb_mmu_pd *pd = psb_mmu_get_default_pd(dev_priv->mmu);
#ifdef SUPPORT_VSP
	struct psb_mmu_pd *vsp_pd = psb_mmu_get_default_pd(dev_priv->vsp_mmu);
#endif /* SUPPORT_VSP */
#else
	struct ttm_dma_tt *ttm_dma;
	struct ttm_bo_device *bdev;
	struct drm_psb_private *dev_priv;
	struct drm_psb_ttm_tt_s *psb_be;
	struct psb_mmu_pd *pd;
	struct psb_mmu_pd *vsp_pd;

	ttm_dma = container_of(ttm, struct ttm_dma_tt, ttm);
	psb_be = container_of(ttm_dma, struct drm_psb_ttm_tt_s, ttm_dma);
	bdev = ttm->bdev;
	dev_priv = container_of(bdev, struct drm_psb_private, bdev);
	pd = psb_mmu_get_default_pd(dev_priv->mmu);
#ifdef SUPPORT_VSP
	vsp_pd = psb_mmu_get_default_pd(dev_priv->vsp_mmu);
#endif /* SUPPORT_VSP */
#endif

#ifndef CONFIG_DRM_VXD_BYT
	if (psb_be->mem_type == TTM_PL_TT) {
		uint32_t gatt_p_offset =
			(psb_be->offset - dev_priv->pg->mmu_gatt_start) >> PAGE_SHIFT;

		(void) psb_gtt_remove_pages(dev_priv->pg, gatt_p_offset,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
					    psb_be->num_pages,
#else
					    ttm->num_pages,
#endif
					    psb_be->desired_tile_stride,
					    psb_be->hw_tile_stride, 0);
	}
#endif

	psb_mmu_remove_pages(pd, psb_be->offset,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
			     psb_be->num_pages,
#else
			     ttm->num_pages,
#endif
			     psb_be->desired_tile_stride,
			     psb_be->hw_tile_stride);
#ifdef SUPPORT_VSP
	psb_mmu_remove_pages(vsp_pd, psb_be->offset,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0))
			     psb_be->num_pages,
#else
			     ttm->num_pages,
#endif
			     psb_be->desired_tile_stride,
			     psb_be->hw_tile_stride);
#endif
	return 0;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
static int drm_psb_tbe_bind(struct ttm_backend *backend,
			    struct ttm_mem_reg *bo_mem)
#else
static int drm_psb_tbe_bind(struct ttm_tt *ttm,
			    struct ttm_mem_reg *bo_mem)
#endif
{
	int type;
	int ret;
	struct ttm_bo_device *bdev;
	struct drm_psb_private *dev_priv;
	struct psb_mmu_pd *pd;
#ifdef SUPPORT_VSP
	struct psb_mmu_pd *vsp_pd;
#endif
	struct ttm_mem_type_manager *man;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
	struct drm_psb_ttm_backend *psb_be;
#else
	struct ttm_dma_tt *ttm_dma;
	struct drm_psb_ttm_tt_s *psb_be;
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
	bdev = backend->bdev;
	psb_be = container_of(backend, struct drm_psb_ttm_backend, base);
#else
	ttm_dma = container_of(ttm, struct ttm_dma_tt, ttm);
	psb_be = container_of(ttm_dma, struct drm_psb_ttm_tt_s, ttm_dma);
	bdev = ttm->bdev;
#endif

	dev_priv = container_of(bdev, struct drm_psb_private, bdev);
	pd = psb_mmu_get_default_pd(dev_priv->mmu);
#ifdef SUPPORT_VSP
	vsp_pd = psb_mmu_get_default_pd(dev_priv->vsp_mmu);
#endif
	man = &bdev->man[bo_mem->mem_type];

	ret = 0;
	psb_be->mem_type = bo_mem->mem_type;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
	psb_be->num_pages = bo_mem->num_pages;
#else
	ttm->num_pages = bo_mem->num_pages;
#endif

	psb_be->desired_tile_stride = 0;
	psb_be->hw_tile_stride = 0;
	psb_be->offset = (bo_mem->start << PAGE_SHIFT) +
			 man->gpu_offset;

	type = (bo_mem->placement & TTM_PL_FLAG_CACHED) ?
				PSB_MMU_CACHED_MEMORY : 0;

#ifndef CONFIG_DRM_VXD_BYT
	if (psb_be->mem_type == TTM_PL_TT) {
		uint32_t gatt_p_offset =
			(psb_be->offset - dev_priv->pg->mmu_gatt_start) >> PAGE_SHIFT;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
		ret = psb_gtt_insert_pages(dev_priv->pg,
					   psb_be->pages,
					   gatt_p_offset,
					   psb_be->num_pages,
					   psb_be->desired_tile_stride,
					   psb_be->hw_tile_stride, type);
#else
		ret = psb_gtt_insert_pages(dev_priv->pg,
					   ttm->pages,
					   gatt_p_offset,
					   ttm->num_pages,
					   psb_be->desired_tile_stride,
					   psb_be->hw_tile_stride, type);
#endif
	}
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
	ret = psb_mmu_insert_pages(pd,
		psb_be->pages,
		psb_be->offset, psb_be->num_pages,
		psb_be->desired_tile_stride,
		psb_be->hw_tile_stride, type);
#else
	ret = psb_mmu_insert_pages(pd,
		ttm->pages,
		psb_be->offset, ttm->num_pages,
		psb_be->desired_tile_stride,
		psb_be->hw_tile_stride, type);
#endif
	if (ret)
		goto out_err;

#ifdef SUPPORT_VSP
	ret = psb_mmu_insert_pages(vsp_pd,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0))
				   psb_be->pages,
				   psb_be->offset,
				   psb_be->num_pages,
#else
				   ttm->pages,
				   psb_be->offset,
				   ttm->num_pages,
#endif
				   psb_be->desired_tile_stride,
				   psb_be->hw_tile_stride, type);
	if (ret)
		goto out_err;
#endif

	return 0;
out_err:
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
	drm_psb_tbe_unbind(backend);
#else
	drm_psb_tbe_unbind(ttm);
#endif
	return ret;

}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
static void drm_psb_tbe_destroy(struct ttm_backend *backend)
{
	struct drm_psb_ttm_backend *psb_be =
		container_of(backend, struct drm_psb_ttm_backend, base);

	if (backend)
		kfree(psb_be);
}
#else
static void drm_psb_tbe_destroy(struct ttm_tt *ttm)
{
	struct ttm_dma_tt *ttm_dma;
	struct drm_psb_ttm_tt_s *psb_be;

	ttm_dma = container_of(ttm, struct ttm_dma_tt, ttm);
	psb_be = container_of(ttm_dma, struct drm_psb_ttm_tt_s, ttm_dma);

	ttm_dma_tt_fini(ttm_dma);
	if (ttm)
		kfree(psb_be);
 }
#endif

static struct ttm_backend_func psb_ttm_backend = {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
	.populate = drm_psb_tbe_populate,
	.clear = drm_psb_tbe_clear,
#endif
	.bind = drm_psb_tbe_bind,
	.unbind = drm_psb_tbe_unbind,
	.destroy = drm_psb_tbe_destroy,
};

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
static struct ttm_backend *drm_psb_tbe_init(struct ttm_bo_device *bdev)
{
	struct drm_psb_ttm_backend *psb_be;

	psb_be = kzalloc(sizeof(*psb_be), GFP_KERNEL);
	if (!psb_be)
		return NULL;
	psb_be->pages = NULL;
	psb_be->base.func = &psb_ttm_backend;
	psb_be->base.bdev = bdev;
	return &psb_be->base;
}
#else
static struct ttm_tt *drm_psb_ttm_tt_create(struct ttm_bo_device *bdev,
	unsigned long size, uint32_t page_flags, struct page *dummy_read_page)
{
	struct drm_psb_ttm_tt_s *psb_be;
	int rva;

#if __OS_HAS_AGP && 0
	if (this_is_an_agp_device)
		return ttm_agp_tt_populate(ttm);
#endif

	psb_be = kzalloc(sizeof(*psb_be), GFP_KERNEL);
	if (!psb_be)
		return NULL;

	psb_be->ttm_dma.ttm.func = &psb_ttm_backend;

	rva = ttm_dma_tt_init(&psb_be->ttm_dma, bdev, size, page_flags,
		dummy_read_page);
	if (rva < 0) {
		kfree(psb_be);
		return NULL;
	}

	return &psb_be->ttm_dma.ttm;
}

static int drm_psb_ttm_tt_populate(struct ttm_tt *ttm)
{
	struct ttm_dma_tt *ttm_dma;
	struct ttm_bo_device *bdev;
	struct drm_psb_private *dev_priv;
	struct drm_device *ddev;

	/*	The only use made of the structure pointed to
		by ddev is reference to these members:
			struct device *dev;
			struct pci_dev *pdev;
		*/
	ttm_dma = (struct ttm_dma_tt *) ttm;

	bdev = ttm->bdev;
	dev_priv = container_of(bdev, struct drm_psb_private, bdev);
	ddev = dev_priv->dev;

	if (ttm->state != tt_unpopulated)
		return 0;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 3, 0))
	{
		bool slave = !!(ttm->page_flags & TTM_PAGE_FLAG_SG);
		if (slave && ttm->sg) {
			drm_prime_sg_to_page_addr_arrays(ttm->sg, ttm->pages,
							 NULL, ttm->num_pages);
			ttm->state = tt_unbound;
			return 0;
		}
	}
#endif

#if __OS_HAS_AGP && 0
	if (this_is_an_agp_device)
		return ttm_agp_tt_populate(ttm);
#endif

#ifdef CONFIG_SWIOTLB
	if (swiotlb_nr_tbl())
		return ttm_dma_populate(ttm_dma, ddev->dev);
#endif

	return ttm_pool_populate(ttm);
}

static void drm_psb_ttm_tt_unpopulate(struct ttm_tt *ttm)
{
	struct ttm_dma_tt *ttm_dma;
	struct ttm_bo_device *bdev;
	struct drm_psb_private *dev_priv;
	struct drm_device *ddev;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 3, 0))
	bool slave = !!(ttm->page_flags & TTM_PAGE_FLAG_SG);

	if (slave)
		return;
#endif
	/*	The only use made of the structure pointed to
		by ddev is reference to these members:
			struct device *dev;
			struct pci_dev *pdev;
		*/
	ttm_dma = (struct ttm_dma_tt *) ttm;
	bdev = ttm->bdev;
	dev_priv = container_of(bdev, struct drm_psb_private, bdev);

	ddev = dev_priv->dev;

#if __OS_HAS_AGP && 0
	if (this_is_an_agp_device) {
		ttm_agp_tt_unpopulate(ttm);
		return;
	}
#endif

#ifdef CONFIG_SWIOTLB
	if (swiotlb_nr_tbl()) {
		ttm_dma_unpopulate(ttm_dma, ddev->dev);
		return;
	}
#endif

	ttm_pool_unpopulate(ttm);
}
#endif

static int psb_invalidate_caches(struct ttm_bo_device *bdev,
				 uint32_t placement)
{
	return 0;
}

/*
 * MSVDX/TOPAZ GPU virtual space looks like this
 * (We currently use only one MMU context).
 * PSB_MEM_MMU_START: from 0x00000000~0xd000000, for generic buffers
 * TTM_PL_IMR: from 0xd0000000, for MFLD IMR buffers
 * TTM_PL_CI: from 0xe0000000+half GTT space, for MRST camear/video buffer sharing
 * TTM_PL_TT: from TTM_PL_CI+CI size, for buffers need to mapping into GTT
 */
static int psb_init_mem_type(struct ttm_bo_device *bdev, uint32_t type,
			     struct ttm_mem_type_manager *man)
{
	struct drm_psb_private *dev_priv =
		container_of(bdev, struct drm_psb_private, bdev);
#ifndef CONFIG_DRM_VXD_BYT
	struct psb_gtt *pg = dev_priv->pg;
#endif

	switch (type) {
	case TTM_PL_SYSTEM:
		man->flags = TTM_MEMTYPE_FLAG_MAPPABLE;
		man->available_caching = TTM_PL_FLAG_CACHED |
					 TTM_PL_FLAG_UNCACHED | TTM_PL_FLAG_WC;
		man->default_caching = TTM_PL_FLAG_CACHED;
		break;
	case DRM_PSB_MEM_MMU:
		man->func = &ttm_bo_manager_func;
		man->flags = TTM_MEMTYPE_FLAG_MAPPABLE |
			     TTM_MEMTYPE_FLAG_CMA;
		man->gpu_offset = PSB_MEM_MMU_START;
		man->available_caching = TTM_PL_FLAG_CACHED |
					 TTM_PL_FLAG_UNCACHED | TTM_PL_FLAG_WC;
		man->default_caching = TTM_PL_FLAG_WC;
		printk(KERN_INFO "[TTM] DRM_PSB_MEM_MMU heap: %lu\n",
		       man->gpu_offset);
		break;
#ifndef CONFIG_DRM_VXD_BYT
#if !defined(MERRIFIELD)
	case TTM_PL_IMR:	/* Unmappable IMR memory */
		man->func = &ttm_bo_manager_func;
		man->flags = TTM_MEMTYPE_FLAG_MAPPABLE |
			     TTM_MEMTYPE_FLAG_FIXED;
		man->available_caching = TTM_PL_FLAG_UNCACHED;
		man->default_caching = TTM_PL_FLAG_UNCACHED;
		man->gpu_offset = PSB_MEM_IMR_START;
		break;
#endif

	case TTM_PL_TT:	/* Mappable GATT memory */
		man->func = &ttm_bo_manager_func;
#ifdef PSB_WORKING_HOST_MMU_ACCESS
		man->flags = TTM_MEMTYPE_FLAG_MAPPABLE;
#else
		man->flags = TTM_MEMTYPE_FLAG_MAPPABLE |
			     TTM_MEMTYPE_FLAG_CMA;
#endif
		man->available_caching = TTM_PL_FLAG_CACHED |
					 TTM_PL_FLAG_UNCACHED | TTM_PL_FLAG_WC;
		man->default_caching = TTM_PL_FLAG_WC;
		man->gpu_offset =
		    pg->mmu_gatt_start + pg->gtt_video_start;
		printk(KERN_INFO "[TTM] TTM_PL_TT heap: 0x%lu\n",
		       man->gpu_offset);
		break;
#endif

	case DRM_PSB_MEM_MMU_TILING:
		man->func = &ttm_bo_manager_func;
		man->flags = TTM_MEMTYPE_FLAG_MAPPABLE |
			     TTM_MEMTYPE_FLAG_CMA;
		man->gpu_offset = PSB_MEM_MMU_TILING_START;
		man->available_caching = TTM_PL_FLAG_CACHED |
					 TTM_PL_FLAG_UNCACHED | TTM_PL_FLAG_WC;
		man->default_caching = TTM_PL_FLAG_WC;
		printk(KERN_INFO "[TTM] DRM_PSB_MEM_MMU_TILING heap: 0x%lu\n",
		       man->gpu_offset);
		break;

	default:
		DRM_ERROR("Unsupported memory type %u\n", (unsigned) type);
		return -EINVAL;
	}
	return 0;
}

static void psb_evict_mask(struct ttm_buffer_object *bo, struct ttm_placement *placement)
{
	static uint32_t cur_placement;

	cur_placement = bo->mem.placement & ~TTM_PL_MASK_MEM;
	cur_placement |= TTM_PL_FLAG_SYSTEM;

	placement->fpfn = 0;
	placement->lpfn = 0;
	placement->num_placement = 1;
	placement->placement = &cur_placement;
	placement->num_busy_placement = 0;
	placement->busy_placement = NULL;

	/* all buffers evicted to system memory */
	/* return cur_placement | TTM_PL_FLAG_SYSTEM; */
}

static int psb_move(struct ttm_buffer_object *bo,
		    bool evict, bool interruptible,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
		    bool no_wait_reserve,
#endif
		    bool no_wait, struct ttm_mem_reg *new_mem)
{
	struct ttm_mem_reg *old_mem = &bo->mem;
#if (!defined(MERRIFIELD) && !defined(CONFIG_DRM_VXD_BYT))
	if ((old_mem->mem_type == TTM_PL_IMR) ||
	    (new_mem->mem_type == TTM_PL_IMR)) {
		if (old_mem->mm_node) {
			spin_lock(&bo->glob->lru_lock);
			drm_mm_put_block(old_mem->mm_node);
			spin_unlock(&bo->glob->lru_lock);
		}
		old_mem->mm_node = NULL;
		*old_mem = *new_mem;
	} else
#endif
	if (old_mem->mem_type == TTM_PL_SYSTEM) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
		return ttm_bo_move_memcpy(bo, evict, false, no_wait, new_mem);
#else
		return ttm_bo_move_memcpy(bo, evict, no_wait, new_mem);
#endif
	} else if (new_mem->mem_type == TTM_PL_SYSTEM) {
		int ret = psb_move_flip(bo, evict, interruptible,
					no_wait, new_mem);
		if (unlikely(ret != 0)) {
			if (ret == -ERESTART)
				return ret;
			else
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
				return ttm_bo_move_memcpy(bo, evict, false, no_wait,
#else
				return ttm_bo_move_memcpy(bo, evict, no_wait,
#endif
							  new_mem);
		}
	} else {
		if (psb_move_blit(bo, evict, no_wait, new_mem))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
			return ttm_bo_move_memcpy(bo, evict, false, no_wait,
#else
			return ttm_bo_move_memcpy(bo, evict, no_wait,
#endif
						  new_mem);
	}
	return 0;
}

int psb_verify_access(struct ttm_buffer_object *bo,
		      struct file *filp)
{
	struct drm_file *file_priv = (struct drm_file *)filp->private_data;

	if (capable(CAP_SYS_ADMIN))
		return 0;

	/* workaround drm authentification issue on Android for ttm_bo_mmap */
	/*
	if (unlikely(!file_priv->authenticated))
		return -EPERM;
	*/

	return ttm_pl_verify_access(bo, psb_fpriv(file_priv)->tfile);
}

static int psb_ttm_io_mem_reserve(struct ttm_bo_device *bdev, struct ttm_mem_reg *mem)
{
	struct ttm_mem_type_manager *man = &bdev->man[mem->mem_type];
	struct drm_psb_private *dev_priv =
		container_of(bdev, struct drm_psb_private, bdev);
#ifndef CONFIG_DRM_VXD_BYT
	struct psb_gtt *pg = dev_priv->pg;
#endif

	mem->bus.addr = NULL;
	mem->bus.offset = 0;
	mem->bus.size = mem->num_pages << PAGE_SHIFT;
	mem->bus.base = 0;
	mem->bus.is_iomem = false;
	if (!(man->flags & TTM_MEMTYPE_FLAG_MAPPABLE))
		return -EINVAL;
	switch (mem->mem_type) {
	case TTM_PL_SYSTEM:
		/* system memory */
		return 0;
#ifndef CONFIG_DRM_VXD_BYT
	case TTM_PL_TT:
		mem->bus.offset = mem->start << PAGE_SHIFT;
		mem->bus.base = pg->gatt_start;
		mem->bus.is_iomem = false; /* Don't know whether it is IO_MEM, this flag used in vm_fault handle */
		break;
#endif
	case DRM_PSB_MEM_MMU:
		mem->bus.offset = mem->start << PAGE_SHIFT;
		mem->bus.base = 0x00000000;
		break;
#ifndef CONFIG_DRM_VXD_BYT
#if !defined(MERRIFIELD)
	case TTM_PL_IMR:
		mem->bus.offset = mem->start << PAGE_SHIFT;
		mem->bus.base = dev_priv->imr_region_start;;
		mem->bus.is_iomem = true;
		break;
#endif
#endif
	case DRM_PSB_MEM_MMU_TILING:
		mem->bus.offset = mem->start << PAGE_SHIFT;
		mem->bus.base = 0x00000000;
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

#ifdef PSB_TTM_IO_MEM_FREE
static void psb_ttm_io_mem_free(struct ttm_bo_device *bdev, struct ttm_mem_reg *mem)
{
}
#endif

struct ttm_bo_driver psb_ttm_bo_driver = {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
	.create_ttm_backend_entry = &drm_psb_tbe_init,
#else
	.ttm_tt_create = &drm_psb_ttm_tt_create,
	.ttm_tt_populate = &drm_psb_ttm_tt_populate,
	.ttm_tt_unpopulate = &drm_psb_ttm_tt_unpopulate,
#endif
	.invalidate_caches = &psb_invalidate_caches,
	.init_mem_type = &psb_init_mem_type,
	.evict_flags = &psb_evict_mask,
	/* psb_move is used for IMR case */
	.move = &psb_move,
	.verify_access = &psb_verify_access,
	.sync_obj_signaled = &ttm_fence_sync_obj_signaled,
	.sync_obj_wait = &ttm_fence_sync_obj_wait,
	.sync_obj_flush = &ttm_fence_sync_obj_flush,
	.sync_obj_unref = &ttm_fence_sync_obj_unref,
	.sync_obj_ref = &ttm_fence_sync_obj_ref,

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
#if 0
	/* Begin -- Not currently used by this driver.
	   These will only be dispatched if non-NULL. */

	/* hook to notify driver about a driver move so it
	 * can do tiling things */
	/*  Only called if non-NULL */
	void (*move_notify)(struct ttm_buffer_object *bo,
			    struct ttm_mem_reg *new_mem);
	/* notify the driver we are taking a fault on this BO
	 * and have reserved it */
	int (*fault_reserve_notify)(struct ttm_buffer_object *bo);

	/**
	 * notify the driver that we're about to swap out this bo
	 */
	void (*swap_notify) (struct ttm_buffer_object *bo);

	/* End   -- Not currently used by this driver. */
#endif
#endif

	.io_mem_reserve = &psb_ttm_io_mem_reserve,
#ifdef PSB_TTM_IO_MEM_FREE
	.io_mem_free = &psb_ttm_io_mem_free
#endif
};
