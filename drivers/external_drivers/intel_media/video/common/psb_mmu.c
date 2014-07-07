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
#include <drm/drmP.h>
#ifdef CONFIG_DRM_VXD_BYT
#include "vxd_drv.h"
#else
#include "psb_drv.h"
#include "psb_reg.h"
#endif
#ifdef SUPPORT_VSP
#include "vsp.h"
#endif

/*
 * Code for the MSVDX/TOPAZ MMU:
 */

/*
 * clflush on one processor only:
 * clflush should apparently flush the cache line on all processors in an
 * SMP system.
 */

/*
 * kmap atomic:
 * The usage of the slots must be completely encapsulated within a spinlock, and
 * no other functions that may be using the locks for other purposed may be
 * called from within the locked region.
 * Since the slots are per processor, this will guarantee that we are the only
 * user.
 */

/*
 * TODO: Inserting ptes from an interrupt handler:
 * This may be desirable for some SGX functionality where the GPU can fault in
 * needed pages. For that, we need to make an atomic insert_pages function, that
 * may fail.
 * If it fails, the caller need to insert the page using a workqueue function,
 * but on average it should be fast.
 */

struct psb_mmu_driver {
	/* protects driver- and pd structures. Always take in read mode
	 * before taking the page table spinlock.
	 */
	struct rw_semaphore sem;

	/* protects page tables, directory tables and pt tables.
	 * and pt structures.
	 */
	spinlock_t lock;

	atomic_t needs_tlbflush;

	uint8_t __iomem *register_map;
	struct psb_mmu_pd *default_pd;
	/*uint32_t bif_ctrl;*/
	int has_clflush;
	int clflush_add;
	unsigned long clflush_mask;

	struct drm_psb_private *dev_priv;
	enum mmu_type_t mmu_type;
};

struct psb_mmu_pd;

struct psb_mmu_pt {
	struct psb_mmu_pd *pd;
	uint32_t index;
	uint32_t count;
	struct page *p;
	uint32_t *v;
};

struct psb_mmu_pd {
	struct psb_mmu_driver *driver;
	int hw_context;
	struct psb_mmu_pt **tables;
	struct page *p;
	struct page *dummy_pt;
	struct page *dummy_page;
	uint32_t pd_mask;
	uint32_t invalid_pde;
	uint32_t invalid_pte;
};

static inline uint32_t psb_mmu_pt_index(uint32_t offset)
{
	return (offset >> PSB_PTE_SHIFT) & 0x3FF;
}

static inline uint32_t psb_mmu_pd_index(uint32_t offset)
{
	return offset >> PSB_PDE_SHIFT;
}

#if defined(CONFIG_X86)
static inline void psb_clflush(volatile void *addr)
{
	__asm__ __volatile__("clflush (%0)\n" : : "r"(addr) : "memory");
}

static inline void psb_mmu_clflush(struct psb_mmu_driver *driver,
				   void *addr)
{
	if (!driver->has_clflush)
		return;

	mb();
	psb_clflush(addr);
	mb();
}

static void psb_page_clflush(struct psb_mmu_driver *driver, struct page* page)
{
	uint32_t clflush_add = driver->clflush_add >> PAGE_SHIFT;
	uint32_t clflush_count = PAGE_SIZE / clflush_add;
	int i;
	uint8_t *clf;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	clf = kmap_atomic(page, KM_USER0);
#else
	clf = kmap_atomic(page);
#endif
	mb();
	for (i = 0; i < clflush_count; ++i) {
		psb_clflush(clf);
		clf += clflush_add;
	}
	mb();
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	kunmap_atomic(clf, KM_USER0);
#else
	kunmap_atomic(clf);
#endif
}

static void psb_pages_clflush(struct psb_mmu_driver *driver, struct page *page[], unsigned long num_pages)
{
	int i;

	if (!driver->has_clflush)
		return ;

	for (i = 0; i < num_pages; i++)
		psb_page_clflush(driver, *page++);
}
#else

static inline void psb_mmu_clflush(struct psb_mmu_driver *driver,
				   void *addr)
{
	;
}

static void psb_pages_clflush(struct psb_mmu_driver *driver, struct page *page[], unsigned long num_pages)
{
	printk("Dumy psb_pages_clflush\n");
}

#endif

static void psb_mmu_flush_pd_locked(struct psb_mmu_driver *driver,
				    int force)
{
	if (atomic_read(&driver->needs_tlbflush) || force) {
		if (!driver->dev_priv)
			goto out;

		if (driver->mmu_type == IMG_MMU) {
			atomic_set(
				&driver->dev_priv->msvdx_mmu_invaldc,
				1);
#ifndef CONFIG_DRM_VXD_BYT
			atomic_set(
				&driver->dev_priv->topaz_mmu_invaldc,
				1);
#endif
		} else if (driver->mmu_type == VSP_MMU) {
#ifdef SUPPORT_VSP
			atomic_set(&driver->dev_priv->vsp_mmu_invaldc, 1);
#endif
		} else {
			DRM_ERROR("MMU: invalid MMU type %d\n",
				  driver->mmu_type);
		}
	}
out:
	atomic_set(&driver->needs_tlbflush, 0);
}

#if 0
static void psb_mmu_flush_pd(struct psb_mmu_driver *driver, int force)
{
	down_write(&driver->sem);
	psb_mmu_flush_pd_locked(driver, force);
	up_write(&driver->sem);
}
#endif

static void psb_virtual_addr_clflush(struct psb_mmu_driver *driver,
			void *vaddr, uint32_t num_pages)
{
	int i, j;
	uint8_t *clf = (uint8_t*)vaddr;
	uint32_t clflush_add = (driver->clflush_add * sizeof(uint32_t)) >> PAGE_SHIFT;
	uint32_t clflush_count = PAGE_SIZE / clflush_add;

	DRM_INFO("clflush pages %d\n", num_pages);
	mb();
	for (i = 0; i < num_pages; ++i) {
		for (j = 0; j < clflush_count; ++j) {
			psb_clflush(clf);
			clf += clflush_add;
		}
	}
	mb();
}

void psb_mmu_flush(struct psb_mmu_driver *driver, int rc_prot)
{
	if (rc_prot)
		down_write(&driver->sem);

	if (!driver->dev_priv)
		goto out;

	if (driver->mmu_type == IMG_MMU) {
		atomic_set(&driver->dev_priv->msvdx_mmu_invaldc, 1);
#ifndef CONFIG_DRM_VXD_BYT
		atomic_set(&driver->dev_priv->topaz_mmu_invaldc, 1);
#endif
	} else if (driver->mmu_type == VSP_MMU) {
#ifdef SUPPORT_VSP
		atomic_set(&driver->dev_priv->vsp_mmu_invaldc, 1);
#endif
	} else {
		DRM_ERROR("MMU: invalid MMU type %d\n", driver->mmu_type);
	}
out:
	if (rc_prot)
		up_write(&driver->sem);
}

void psb_mmu_set_pd_context(struct psb_mmu_pd *pd, int hw_context)
{
	/*ttm_tt_cache_flush(&pd->p, 1);*/
	psb_pages_clflush(pd->driver, &pd->p, 1);
	down_write(&pd->driver->sem);
	wmb();
	psb_mmu_flush_pd_locked(pd->driver, 1);
	pd->hw_context = hw_context;
	up_write(&pd->driver->sem);

}

static inline unsigned long psb_pd_addr_end(unsigned long addr,
		unsigned long end)
{

	addr = (addr + PSB_PDE_MASK + 1) & ~PSB_PDE_MASK;
	return (addr < end) ? addr : end;
}

static inline uint32_t psb_mmu_mask_pte(uint32_t pfn, int type)
{
	uint32_t mask = PSB_PTE_VALID;

	if (type & PSB_MMU_CACHED_MEMORY)
		mask |= PSB_PTE_CACHED;
	if (type & PSB_MMU_RO_MEMORY)
		mask |= PSB_PTE_RO;
	if (type & PSB_MMU_WO_MEMORY)
		mask |= PSB_PTE_WO;

	return (pfn << PAGE_SHIFT) | mask;
}

#ifdef SUPPORT_VSP
static inline uint32_t vsp_mmu_mask_pte(uint32_t pfn, int type)
{
	return (pfn & VSP_PDE_MASK) | VSP_PTE_VALID;
}
#endif

struct psb_mmu_pd *psb_mmu_alloc_pd(struct psb_mmu_driver *driver,
				    int trap_pagefaults, int invalid_type) {
	struct psb_mmu_pd *pd = kmalloc(sizeof(*pd), GFP_KERNEL);
	uint32_t *v;
	int i;

	if (!pd)
		return NULL;

	pd->p = alloc_page(GFP_DMA32);
	if (!pd->p)
		goto out_err1;
	pd->dummy_pt = alloc_page(GFP_DMA32);
	if (!pd->dummy_pt)
		goto out_err2;
	pd->dummy_page = alloc_page(GFP_DMA32);
	if (!pd->dummy_page)
		goto out_err3;

	if (!trap_pagefaults) {
		if (driver->mmu_type == IMG_MMU) {
			pd->invalid_pde =
				psb_mmu_mask_pte(page_to_pfn(pd->dummy_pt),
						 invalid_type);
			pd->invalid_pte =
				psb_mmu_mask_pte(page_to_pfn(pd->dummy_page),
						 invalid_type);
		} else if (driver->mmu_type == VSP_MMU) {
#ifdef SUPPORT_VSP
			pd->invalid_pde =
				vsp_mmu_mask_pte(page_to_pfn(pd->dummy_pt),
						 invalid_type);
			pd->invalid_pte =
				vsp_mmu_mask_pte(page_to_pfn(pd->dummy_page),
						 invalid_type);
#endif
		} else {
			DRM_ERROR("MMU: invalid MMU type %d\n",
				  driver->mmu_type);
			goto out_err4;
		}
	} else {
		pd->invalid_pde = 0;
		pd->invalid_pte = 0;
	}

	v = kmap(pd->dummy_pt);
	if (!v)
		goto out_err4;
	for (i = 0; i < (PAGE_SIZE / sizeof(uint32_t)); ++i)
		v[i] = pd->invalid_pte;

	kunmap(pd->dummy_pt);

	v = kmap(pd->p);
	if (!v)
		goto out_err4;
	for (i = 0; i < (PAGE_SIZE / sizeof(uint32_t)); ++i)
		v[i] = pd->invalid_pde;

	kunmap(pd->p);

	v = kmap(pd->dummy_page);
	if (!v)
		goto out_err4;
	clear_page(v);
	kunmap(pd->dummy_page);

	pd->tables = vmalloc_user(sizeof(struct psb_mmu_pt *) * 1024);
	if (!pd->tables)
		goto out_err4;

	pd->hw_context = -1;
	pd->pd_mask = PSB_PTE_VALID;
	pd->driver = driver;

	return pd;

out_err4:
	__free_page(pd->dummy_page);
out_err3:
	__free_page(pd->dummy_pt);
out_err2:
	__free_page(pd->p);
out_err1:
	kfree(pd);
	return NULL;
}

void psb_mmu_free_pt(struct psb_mmu_pt *pt)
{
	__free_page(pt->p);
	kfree(pt);
}

void psb_mmu_free_pagedir(struct psb_mmu_pd *pd)
{
	struct psb_mmu_driver *driver = pd->driver;
	struct psb_mmu_pt *pt;
	int i;

	down_write(&driver->sem);
	if (pd->hw_context != -1)
		psb_mmu_flush_pd_locked(driver, 1);

	/* Should take the spinlock here, but we don't need to do that
	   since we have the semaphore in write mode. */

	for (i = 0; i < 1024; ++i) {
		pt = pd->tables[i];
		if (pt)
			psb_mmu_free_pt(pt);
	}

	vfree(pd->tables);
	__free_page(pd->dummy_page);
	__free_page(pd->dummy_pt);
	__free_page(pd->p);
	kfree(pd);
	up_write(&driver->sem);
}

static struct psb_mmu_pt *psb_mmu_alloc_pt(struct psb_mmu_pd *pd)
{
	struct psb_mmu_pt *pt = kmalloc(sizeof(*pt), GFP_KERNEL);
	void *v;
	uint32_t clflush_add = pd->driver->clflush_add >> PAGE_SHIFT;
	uint32_t clflush_count = PAGE_SIZE / clflush_add;
	spinlock_t *lock = &pd->driver->lock;
	uint8_t *clf;
	uint32_t *ptes;
	int i;

	if (!pt)
		return NULL;

	pt->p = alloc_page(GFP_DMA32);
	if (!pt->p) {
		kfree(pt);
		return NULL;
	}

	spin_lock(lock);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	v = kmap_atomic(pt->p, KM_USER0);
#else
	v = kmap_atomic(pt->p);
#endif
	clf = (uint8_t *) v;
	ptes = (uint32_t *) v;
	for (i = 0; i < (PAGE_SIZE / sizeof(uint32_t)); ++i)
		*ptes++ = pd->invalid_pte;


#if defined(CONFIG_X86)
	if (pd->driver->has_clflush && pd->hw_context != -1) {
		mb();
		for (i = 0; i < clflush_count; ++i) {
			psb_clflush(clf);
			clf += clflush_add;
		}
		mb();
	}
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	kunmap_atomic(v, KM_USER0);
#else
	kunmap_atomic(v);
#endif
	spin_unlock(lock);

	pt->count = 0;
	pt->pd = pd;
	pt->index = 0;

	return pt;
}

struct psb_mmu_pt *psb_mmu_pt_alloc_map_lock(struct psb_mmu_pd *pd,
		unsigned long addr) {
	uint32_t index = psb_mmu_pd_index(addr);
	struct psb_mmu_pt *pt;
	uint32_t *v;
	spinlock_t *lock = &pd->driver->lock;
	struct psb_mmu_driver *driver = pd->driver;

	spin_lock(lock);
	pt = pd->tables[index];
	while (!pt) {
		spin_unlock(lock);
		pt = psb_mmu_alloc_pt(pd);
		if (!pt)
			return NULL;
		spin_lock(lock);

		if (pd->tables[index]) {
			spin_unlock(lock);
			psb_mmu_free_pt(pt);
			spin_lock(lock);
			pt = pd->tables[index];
			continue;
		}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
		v = kmap_atomic(pd->p, KM_USER0);
#else
		v = kmap_atomic(pd->p);
#endif
		pd->tables[index] = pt;
		if (driver->mmu_type == IMG_MMU)
			v[index] = (page_to_pfn(pt->p) << 12) |
				pd->pd_mask;
#ifdef SUPPORT_VSP
		else if (driver->mmu_type == VSP_MMU)
			v[index] = (page_to_pfn(pt->p));
#endif
		else
			DRM_ERROR("MMU: invalid MMU type %d\n",
				  driver->mmu_type);

		pt->index = index;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
		kunmap_atomic((void *) v, KM_USER0);
#else
		kunmap_atomic((void *) v);
#endif

		if (pd->hw_context != -1) {
			psb_mmu_clflush(pd->driver, (void *) &v[index]);
			atomic_set(&pd->driver->needs_tlbflush, 1);
		}
	}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	pt->v = kmap_atomic(pt->p, KM_USER0);
#else
	pt->v = kmap_atomic(pt->p);
#endif
	return pt;
}

static struct psb_mmu_pt *psb_mmu_pt_map_lock(struct psb_mmu_pd *pd,
		unsigned long addr) {
	uint32_t index = psb_mmu_pd_index(addr);
	struct psb_mmu_pt *pt;
	spinlock_t *lock = &pd->driver->lock;

	spin_lock(lock);
	pt = pd->tables[index];
	if (!pt) {
		spin_unlock(lock);
		return NULL;
	}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	pt->v = kmap_atomic(pt->p, KM_USER0);
#else
	pt->v = kmap_atomic(pt->p);
#endif
	return pt;
}

static void psb_mmu_pt_unmap_unlock(struct psb_mmu_pt *pt)
{
	struct psb_mmu_pd *pd = pt->pd;
	uint32_t *v;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	kunmap_atomic(pt->v, KM_USER0);
#else
	kunmap_atomic(pt->v);
#endif
	if (pt->count == 0) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
		v = kmap_atomic(pd->p, KM_USER0);
#else
		v = kmap_atomic(pd->p);
#endif
		v[pt->index] = pd->invalid_pde;
		pd->tables[pt->index] = NULL;

		if (pd->hw_context != -1) {
			psb_mmu_clflush(pd->driver,
					(void *) &v[pt->index]);
			atomic_set(&pd->driver->needs_tlbflush, 1);
		}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
		kunmap_atomic(pt->v, KM_USER0);
#else
		kunmap_atomic(pt->v);
#endif
		spin_unlock(&pd->driver->lock);
		psb_mmu_free_pt(pt);
		return;
	}
	spin_unlock(&pd->driver->lock);
}

static inline void psb_mmu_set_pte(struct psb_mmu_pt *pt,
				   unsigned long addr, uint32_t pte)
{
	pt->v[psb_mmu_pt_index(addr)] = pte;
}

static inline void psb_mmu_invalidate_pte(struct psb_mmu_pt *pt,
		unsigned long addr)
{
	pt->v[psb_mmu_pt_index(addr)] = pt->pd->invalid_pte;
}

#if 0
static uint32_t psb_mmu_check_pte_locked(struct psb_mmu_pd *pd,
		uint32_t mmu_offset)
{
	uint32_t *v;
	uint32_t pfn;

	v = kmap_atomic(pd->p, KM_USER0);
	if (!v) {
		printk(KERN_INFO "Could not kmap pde page.\n");
		return 0;
	}
	pfn = v[psb_mmu_pd_index(mmu_offset)];
	/*      printk(KERN_INFO "pde is 0x%08x\n",pfn); */
	kunmap_atomic(v, KM_USER0);
	if (((pfn & 0x0F) != PSB_PTE_VALID)) {
		printk(KERN_INFO "Strange pde at 0x%08x: 0x%08x.\n",
		       mmu_offset, pfn);
	}
	v = ioremap(pfn & 0xFFFFF000, 4096);
	if (!v) {
		printk(KERN_INFO "Could not kmap pte page.\n");
		return 0;
	}
	pfn = v[psb_mmu_pt_index(mmu_offset)];
	/* printk(KERN_INFO "pte is 0x%08x\n",pfn); */
	iounmap(v);
	if (((pfn & 0x0F) != PSB_PTE_VALID)) {
		printk(KERN_INFO "Strange pte at 0x%08x: 0x%08x.\n",
		       mmu_offset, pfn);
	}
	return pfn >> PAGE_SHIFT;
}

static void psb_mmu_check_mirrored_gtt(struct psb_mmu_pd *pd,
				       uint32_t mmu_offset,
				       uint32_t gtt_pages)
{
	uint32_t start;
	uint32_t next;

	printk(KERN_INFO "Checking mirrored gtt 0x%08x %d\n",
	       mmu_offset, gtt_pages);
	down_read(&pd->driver->sem);
	start = psb_mmu_check_pte_locked(pd, mmu_offset);
	mmu_offset += PAGE_SIZE;
	gtt_pages -= 1;
	while (gtt_pages--) {
		next = psb_mmu_check_pte_locked(pd, mmu_offset);
		if (next != start + 1) {
			printk(KERN_INFO
			       "Ptes out of order: 0x%08x, 0x%08x.\n",
			       start, next);
		}
		start = next;
		mmu_offset += PAGE_SIZE;
	}
	up_read(&pd->driver->sem);
}

void psb_mmu_mirror_gtt(struct psb_mmu_pd *pd,
			uint32_t mmu_offset, uint32_t gtt_start,
			uint32_t gtt_pages)
{
	uint32_t *v;
	uint32_t start = psb_mmu_pd_index(mmu_offset);
	struct psb_mmu_driver *driver = pd->driver;
	int num_pages = gtt_pages;

	down_read(&driver->sem);
	spin_lock(&driver->lock);

	v = kmap_atomic(pd->p, KM_USER0);
	v += start;

	while (gtt_pages--) {
		*v++ = gtt_start | pd->pd_mask;
		gtt_start += PAGE_SIZE;
	}

	/*ttm_tt_cache_flush(&pd->p, num_pages);*/
	psb_pages_clflush(pd->driver, &pd->p, num_pages);
	kunmap_atomic(v, KM_USER0);
	spin_unlock(&driver->lock);

	if (pd->hw_context != -1)
		atomic_set(&pd->driver->needs_tlbflush, 1);

	up_read(&pd->driver->sem);
	psb_mmu_flush_pd(pd->driver, 0);
}
#endif

struct psb_mmu_pd *psb_mmu_get_default_pd(struct psb_mmu_driver *driver)
{
	struct psb_mmu_pd *pd;

	/* down_read(&driver->sem); */
	pd = driver->default_pd;
	/* up_read(&driver->sem); */

	return pd;
}

/* Returns the physical address of the PD shared by sgx/msvdx */
uint32_t psb_get_default_pd_addr(struct psb_mmu_driver *driver)
{
	struct psb_mmu_pd *pd;

	pd = psb_mmu_get_default_pd(driver);
	return page_to_pfn(pd->p) << PAGE_SHIFT;
}

void psb_mmu_driver_takedown(struct psb_mmu_driver *driver)
{
	psb_mmu_free_pagedir(driver->default_pd);
	kfree(driver);
}

struct psb_mmu_driver *psb_mmu_driver_init(uint8_t __iomem * registers,
					int trap_pagefaults,
					int invalid_type,
					struct drm_psb_private *dev_priv,
					enum mmu_type_t mmu_type) {
	struct psb_mmu_driver *driver;

	driver = kmalloc(sizeof(*driver), GFP_KERNEL);
	if (!driver)
		return NULL;

	driver->dev_priv = dev_priv;
	driver->mmu_type = mmu_type;

	driver->default_pd = psb_mmu_alloc_pd(driver, trap_pagefaults,
					      invalid_type);
	if (!driver->default_pd)
		goto out_err1;

	spin_lock_init(&driver->lock);
	init_rwsem(&driver->sem);
	down_write(&driver->sem);
	driver->register_map = registers;
	atomic_set(&driver->needs_tlbflush, 1);

	driver->has_clflush = 0;

#if defined(CONFIG_X86)
	if (boot_cpu_has(X86_FEATURE_CLFLSH)) {
		uint32_t tfms, misc, cap0, cap4, clflush_size;

		/*
		 * clflush size is determined at kernel setup for x86_64
		 *  but not for i386. We have to do it here.
		 */

		cpuid(0x00000001, &tfms, &misc, &cap0, &cap4);
		clflush_size = ((misc >> 8) & 0xff) * 8;
		driver->has_clflush = 1;
		driver->clflush_add =
			PAGE_SIZE * clflush_size / sizeof(uint32_t);
		driver->clflush_mask = driver->clflush_add - 1;
		driver->clflush_mask = ~driver->clflush_mask;
	}
#endif

	up_write(&driver->sem);
	return driver;

out_err1:
	kfree(driver);
	return NULL;
}

#if defined(CONFIG_X86)
static void psb_mmu_flush_ptes(struct psb_mmu_pd *pd,
			       unsigned long address, uint32_t num_pages,
			       uint32_t desired_tile_stride,
			       uint32_t hw_tile_stride)
{
	struct psb_mmu_pt *pt;
	uint32_t rows = 1;
	uint32_t i;
	unsigned long addr;
	unsigned long end;
	unsigned long next;
	unsigned long add;
	unsigned long row_add;
	unsigned long clflush_add = pd->driver->clflush_add;
	unsigned long clflush_mask = pd->driver->clflush_mask;

	if (!pd->driver->has_clflush) {
		/*ttm_tt_cache_flush(&pd->p, num_pages);*/
		psb_pages_clflush(pd->driver, &pd->p, num_pages);
		return;
	}

	if (hw_tile_stride)
		rows = num_pages / desired_tile_stride;
	else
		desired_tile_stride = num_pages;

	add = desired_tile_stride << PAGE_SHIFT;
	row_add = hw_tile_stride << PAGE_SHIFT;
	mb();
	for (i = 0; i < rows; ++i) {
		addr = address;
		end = addr + add;

		do {
			next = psb_pd_addr_end(addr, end);
			pt = psb_mmu_pt_map_lock(pd, addr);
			if (!pt)
				continue;
			do {
				psb_clflush(&pt->v
					    [psb_mmu_pt_index(addr)]);
			} while (addr +=
					 clflush_add,
				 (addr & clflush_mask) < next);

			psb_mmu_pt_unmap_unlock(pt);
		} while (addr = next, next != end);
		address += row_add;
	}
	mb();
}
#else
static void psb_mmu_flush_ptes(struct psb_mmu_pd *pd,
			       unsigned long address, uint32_t num_pages,
			       uint32_t desired_tile_stride,
			       uint32_t hw_tile_stride)
{
	drm_ttm_cache_flush(&pd->p, num_pages);
}
#endif

void psb_mmu_remove_pfn_sequence(struct psb_mmu_pd *pd,
				 unsigned long address, uint32_t num_pages)
{
	struct psb_mmu_pt *pt;
	unsigned long addr;
	unsigned long end;
	unsigned long next;
	unsigned long f_address = address;

	down_read(&pd->driver->sem);

	addr = address;
	end = addr + (num_pages << PAGE_SHIFT);

	do {
		next = psb_pd_addr_end(addr, end);
		pt = psb_mmu_pt_alloc_map_lock(pd, addr);
		if (!pt)
			goto out;
		do {
			psb_mmu_invalidate_pte(pt, addr);
			--pt->count;
		} while (addr += PAGE_SIZE, addr < next);
		psb_mmu_pt_unmap_unlock(pt);

	} while (addr = next, next != end);

out:
	if (pd->hw_context != -1)
		psb_mmu_flush_ptes(pd, f_address, num_pages, 1, 1);

	up_read(&pd->driver->sem);

	if (pd->hw_context != -1)
		psb_mmu_flush(pd->driver, 0);

	return;
}

void psb_mmu_remove_pages(struct psb_mmu_pd *pd, unsigned long address,
			  uint32_t num_pages, uint32_t desired_tile_stride,
			  uint32_t hw_tile_stride)
{
	struct psb_mmu_pt *pt;
	uint32_t rows = 1;
	uint32_t i;
	unsigned long addr;
	unsigned long end;
	unsigned long next;
	unsigned long add;
	unsigned long row_add;
	unsigned long f_address = address;

	if (hw_tile_stride)
		rows = num_pages / desired_tile_stride;
	else
		desired_tile_stride = num_pages;

	add = desired_tile_stride << PAGE_SHIFT;
	row_add = hw_tile_stride << PAGE_SHIFT;

	/* down_read(&pd->driver->sem); */

	/* Make sure we only need to flush this processor's cache */

	for (i = 0; i < rows; ++i) {

		addr = address;
		end = addr + add;

		do {
			next = psb_pd_addr_end(addr, end);
			pt = psb_mmu_pt_map_lock(pd, addr);
			if (!pt)
				continue;
			do {
				psb_mmu_invalidate_pte(pt, addr);
				--pt->count;

			} while (addr += PAGE_SIZE, addr < next);
			psb_mmu_pt_unmap_unlock(pt);

		} while (addr = next, next != end);
		address += row_add;
	}
	if (pd->hw_context != -1)
		psb_mmu_flush_ptes(pd, f_address, num_pages,
				   desired_tile_stride, hw_tile_stride);

	/* up_read(&pd->driver->sem); */

	if (pd->hw_context != -1)
		psb_mmu_flush(pd->driver, 0);
}

int psb_mmu_insert_pfn_sequence(struct psb_mmu_pd *pd, uint32_t start_pfn,
				unsigned long address, uint32_t num_pages,
				int type)
{
	struct psb_mmu_pt *pt;
	struct psb_mmu_driver *driver = pd->driver;
	uint32_t pte;
	unsigned long addr;
	unsigned long end;
	unsigned long next;
	unsigned long f_address = address;
	int ret = 0;

	down_read(&pd->driver->sem);

	addr = address;
	end = addr + (num_pages << PAGE_SHIFT);

	do {
		next = psb_pd_addr_end(addr, end);
		pt = psb_mmu_pt_alloc_map_lock(pd, addr);
		if (!pt) {
			ret = -ENOMEM;
			goto out;
		}
		do {
			if (driver->mmu_type == IMG_MMU) {
				pte = psb_mmu_mask_pte(start_pfn++, type);
#ifdef SUPPORT_VSP
			} else if (driver->mmu_type == VSP_MMU) {
				pte = vsp_mmu_mask_pte(start_pfn++, type);
#endif
			} else {
				DRM_ERROR("MMU: mmu type invalid %d\n",
					  driver->mmu_type);
				ret = -EINVAL;
				goto out;
			}

			psb_mmu_set_pte(pt, addr, pte);
			pt->count++;
		} while (addr += PAGE_SIZE, addr < next);
		psb_mmu_pt_unmap_unlock(pt);

	} while (addr = next, next != end);

out:
	if (pd->hw_context != -1)
		psb_mmu_flush_ptes(pd, f_address, num_pages, 1, 1);

	up_read(&pd->driver->sem);

	if (pd->hw_context != -1)
		psb_mmu_flush(pd->driver, 1);

	return ret;
}

int psb_mmu_insert_pages(struct psb_mmu_pd *pd, struct page **pages,
			 unsigned long address, uint32_t num_pages,
			 uint32_t desired_tile_stride,
			 uint32_t hw_tile_stride, int type)
{
	struct psb_mmu_pt *pt;
	struct psb_mmu_driver *driver = pd->driver;
	uint32_t rows = 1;
	uint32_t i;
	uint32_t pte;
	unsigned long addr;
	unsigned long end;
	unsigned long next;
	unsigned long add;
	unsigned long row_add;
	unsigned long f_address = address;
	int ret = 0;

	if (hw_tile_stride) {
		if (num_pages % desired_tile_stride != 0)
			return -EINVAL;
		rows = num_pages / desired_tile_stride;
	} else {
		desired_tile_stride = num_pages;
	}

	add = desired_tile_stride << PAGE_SHIFT;
	row_add = hw_tile_stride << PAGE_SHIFT;

	down_read(&pd->driver->sem);

	for (i = 0; i < rows; ++i) {

		addr = address;
		end = addr + add;

		do {
			next = psb_pd_addr_end(addr, end);
			pt = psb_mmu_pt_alloc_map_lock(pd, addr);
			if (!pt) {
				ret = -ENOMEM;
				goto out;
			}
			do {
				if (driver->mmu_type == IMG_MMU) {
					pte = psb_mmu_mask_pte(
						page_to_pfn(*pages++),
						type);
#ifdef SUPPORT_VSP
				} else if (driver->mmu_type == VSP_MMU) {
					pte = vsp_mmu_mask_pte(
						page_to_pfn(*pages++),
						type);
#endif
				} else {
					DRM_ERROR("MMU: mmu type invalid %d\n",
						  driver->mmu_type);
					ret = -EINVAL;
					goto out;
				}

				psb_mmu_set_pte(pt, addr, pte);
				pt->count++;
			} while (addr += PAGE_SIZE, addr < next);
			psb_mmu_pt_unmap_unlock(pt);

		} while (addr = next, next != end);

		address += row_add;
	}
out:
	if (pd->hw_context != -1)
		psb_mmu_flush_ptes(pd, f_address, num_pages,
				   desired_tile_stride, hw_tile_stride);

	up_read(&pd->driver->sem);

	if (pd->hw_context != -1)
		psb_mmu_flush(pd->driver, 1);

	return ret;
}

#if 0 /*comented out, only used in mmu test now*/
void psb_mmu_enable_requestor(struct psb_mmu_driver *driver, uint32_t mask)
{
	mask &= _PSB_MMU_ER_MASK;
	psb_iowrite32(driver,
		      psb_ioread32(driver, PSB_CR_BIF_CTRL) & ~mask,
		      PSB_CR_BIF_CTRL);
	(void) psb_ioread32(driver, PSB_CR_BIF_CTRL);
}

void psb_mmu_disable_requestor(struct psb_mmu_driver *driver,
			       uint32_t mask)
{
	mask &= _PSB_MMU_ER_MASK;
	psb_iowrite32(driver, psb_ioread32(driver, PSB_CR_BIF_CTRL) | mask,
		      PSB_CR_BIF_CTRL);
	(void) psb_ioread32(driver, PSB_CR_BIF_CTRL);
}

int psb_mmu_virtual_to_pfn(struct psb_mmu_pd *pd, uint32_t virtual,
			   unsigned long *pfn)
{
	int ret;
	struct psb_mmu_pt *pt;
	uint32_t tmp;
	spinlock_t *lock = &pd->driver->lock;

	down_read(&pd->driver->sem);
	pt = psb_mmu_pt_map_lock(pd, virtual);
	if (!pt) {
		uint32_t *v;

		spin_lock(lock);
		v = kmap_atomic(pd->p, KM_USER0);
		tmp = v[psb_mmu_pd_index(virtual)];
		kunmap_atomic(v, KM_USER0);
		spin_unlock(lock);

		if (tmp != pd->invalid_pde || !(tmp & PSB_PTE_VALID) ||
		    !(pd->invalid_pte & PSB_PTE_VALID)) {
			ret = -EINVAL;
			goto out;
		}
		ret = 0;
		*pfn = pd->invalid_pte >> PAGE_SHIFT;
		goto out;
	}
	tmp = pt->v[psb_mmu_pt_index(virtual)];
	if (!(tmp & PSB_PTE_VALID)) {
		ret = -EINVAL;
	} else {
		ret = 0;
		*pfn = tmp >> PAGE_SHIFT;
	}
	psb_mmu_pt_unmap_unlock(pt);
out:
	up_read(&pd->driver->sem);
	return ret;
}

void psb_mmu_test(struct psb_mmu_driver *driver, uint32_t offset)
{
	struct page *p;
	unsigned long pfn;
	int ret = 0;
	struct psb_mmu_pd *pd;
	uint32_t *v;
	uint32_t *vmmu;

	pd = driver->default_pd;
	if (!pd)
		printk(KERN_WARNING "Could not get default pd\n");


	p = alloc_page(GFP_DMA32);

	if (!p) {
		printk(KERN_WARNING "Failed allocating page\n");
		return;
	}

	v = kmap(p);
	memset(v, 0x67, PAGE_SIZE);

	pfn = (offset >> PAGE_SHIFT);

	ret = psb_mmu_insert_pages(pd, &p, pfn << PAGE_SHIFT, 1, 0, 0, 0);
	if (ret) {
		printk(KERN_WARNING "Failed inserting mmu page\n");
		goto out_err1;
	}

	/* Ioremap the page through the GART aperture */

	vmmu = ioremap(pfn << PAGE_SHIFT, PAGE_SIZE);
	if (!vmmu) {
		printk(KERN_WARNING "Failed ioremapping page\n");
		goto out_err2;
	}

	/* Read from the page with mmu disabled. */
	printk(KERN_INFO "Page first dword is 0x%08x\n", ioread32(vmmu));

	/* Enable the mmu for host accesses and read again. */
	psb_mmu_enable_requestor(driver, _PSB_MMU_ER_HOST);

	printk(KERN_INFO "MMU Page first dword is (0x67676767) 0x%08x\n",
	       ioread32(vmmu));
	*v = 0x15243705;
	printk(KERN_INFO "MMU Page new dword is (0x15243705) 0x%08x\n",
	       ioread32(vmmu));
	iowrite32(0x16243355, vmmu);
	(void) ioread32(vmmu);
	printk(KERN_INFO "Page new dword is (0x16243355) 0x%08x\n", *v);

	printk(KERN_INFO "Int stat is 0x%08x\n",
	       psb_ioread32(driver, PSB_CR_BIF_INT_STAT));
	printk(KERN_INFO "Fault is 0x%08x\n",
	       psb_ioread32(driver, PSB_CR_BIF_FAULT));

	/* Disable MMU for host accesses and clear page fault register */
	psb_mmu_disable_requestor(driver, _PSB_MMU_ER_HOST);
	iounmap(vmmu);
out_err2:
	psb_mmu_remove_pages(pd, pfn << PAGE_SHIFT, 1, 0, 0);
out_err1:
	kunmap(p);
	__free_page(p);
}
#endif

/*
void psb_mmu_pgtable_dump(struct drm_device *dev)
{

	struct drm_psb_private *dev_priv = dev->dev_private;
	struct psb_mmu_pd *pd = psb_mmu_get_default_pd(dev_priv->mmu);
	struct psb_mmu_pt *pt;
	int i, j;
	uint32_t flags;
	uint32_t *v;

	spinlock_t *lock = &pd->driver->lock;
	down_read(&pd->driver->sem);
	spin_lock_irqsave(lock, flags);
	v = kmap_atomic(pd->p, KM_USER0);
	if (!v) {
		printk(KERN_INFO "%s: Kmap pg fail, abort\n", __func__);
		return;
	}

	printk(KERN_INFO "%s: start dump mmu page table\n", __func__);
	for (i = 0; i < 1024; i++) {
		pt = pd->tables[i];
		if (!pt) {
			printk(KERN_INFO "pt[%d] is NULL, 0x%08x\n", i, v[i]);
			continue;
		}
		printk(KERN_INFO "pt[%d] is 0x%08x\n", i, v[i]);
		pt->v = kmap_atomic(pt->p, KM_USER0);
		if (!(pt->v)) {
			printk(KERN_INFO "%s: Kmap fail, abort\n", __func__);
			break;
		}
		for (j = 0; j < 1024; j++) {
			if (!(j%16))
				printk(KERN_INFO "pte%d:", j);
			uint32_t pte = pt->v[j];
			printk("%08xh ", pte);
			//if ((j%16) == 15)
				//printk(KERN_INFO "\n");
		}
		kunmap_atomic(pt->v, KM_USER0);
	}
	spin_unlock_irqrestore(lock, flags);
	up_read(&pd->driver->sem);
	kunmap_atomic((void *) v, KM_USER0);
	printk(KERN_INFO "%s: finish dump mmu page table\n", __func__);
}
*/

int psb_ttm_bo_clflush(struct psb_mmu_driver *mmu,
			struct ttm_buffer_object *bo)
{
	int ret = 0;
	bool is_iomem;
	void *addr;
	struct ttm_bo_kmap_obj bo_kmap;

	if (unlikely(!mmu || !bo)) {
		DRM_ERROR("NULL pointer, mmu:%p bo:%p\n", mmu, bo);
		return 1;
	}

	/*map surface parameters*/
	ret = ttm_bo_kmap(bo, 0, bo->num_pages,
                          &bo_kmap);
        if (ret) {
                DRM_ERROR("ttm_bo_kmap failed: %d.\n", ret);
                return ret;
        }

	addr = (void *)ttm_kmap_obj_virtual(&bo_kmap, &is_iomem);
	if (unlikely(!addr)) {
		DRM_ERROR("failed to ttm_kmap_obj_virtual\n");
		ret = 1;
	}

	psb_virtual_addr_clflush(mmu, addr, bo->num_pages);

	ttm_bo_kunmap(&bo_kmap);
	return ret;
}
