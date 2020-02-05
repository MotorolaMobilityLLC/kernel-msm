// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Linaro 2012
 * Author: <benjamin.gaignard@linaro.org> for ST-Ericsson.
 *
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/cma.h>
#include <linux/scatterlist.h>
#include <linux/highmem.h>
#include <linux/delay.h>

#include "../../../mm/cma.h"
#include "ion.h"

/*
 * struct rcma: struct is somewhat similar to that of CMA's.
 * This contains the basic information required to similarly
 * manage the heap like how CMA does today (i.e. a bitmap).
 */
struct rcma {
	unsigned long  base_pfn;
	unsigned long  count;
	unsigned long *bitmap;
	unsigned int   order_per_bit; /* Order of pages represented by one bit */
	struct mutex   lock;
};

struct ion_reserved_cma_heap {
	struct ion_heap  heap;
	struct rcma      rcma;
	struct cma      *cma_area;
};

#define to_rcma_heap(x) container_of(x, struct ion_reserved_cma_heap, heap)

static inline unsigned long rcma_bitmap_maxno(struct rcma *rcma)
{
	return rcma->count >> rcma->order_per_bit;
}

static unsigned long rcma_bitmap_aligned_mask(const struct rcma *rcma,
					     unsigned int align_order)
{
	if (align_order <= rcma->order_per_bit)
		return 0;
	return (1UL << (align_order - rcma->order_per_bit)) - 1;
}

/*
 * Find the offset of the base PFN from the specified align_order.
 * The value returned is represented in order_per_bits.
 */
static unsigned long rcma_bitmap_aligned_offset(const struct rcma *rcma,
					       unsigned int align_order)
{
	return (rcma->base_pfn & ((1UL << align_order) - 1))
		>> rcma->order_per_bit;
}

static unsigned long rcma_bitmap_pages_to_bits(const struct rcma *rcma,
					      unsigned long pages)
{
	return ALIGN(pages, 1UL << rcma->order_per_bit) >> rcma->order_per_bit;
}

static void rcma_clear_bitmap(struct rcma *rcma, unsigned long pfn,
			     unsigned int count)
{
	unsigned long bitmap_no, bitmap_count;

	bitmap_no = (pfn - rcma->base_pfn) >> rcma->order_per_bit;
	bitmap_count = rcma_bitmap_pages_to_bits(rcma, count);

	mutex_lock(&rcma->lock);
	bitmap_clear(rcma->bitmap, bitmap_no, bitmap_count);
	mutex_unlock(&rcma->lock);
}

/**
 * rcma_alloc() - allocate pages from contiguous area
 * @rcma:   Contiguous memory region for which the allocation is performed.
 * @count: Requested number of pages.
 * @align: Requested alignment of pages (in PAGE_SIZE order).
 * @no_warn: Avoid printing message about failed allocation
 *
 * This function allocates part of contiguous memory on specific
 * contiguous memory area.
 */
struct page *rcma_alloc(struct rcma *rcma, size_t count, unsigned int align,
		       bool no_warn)
{
	unsigned long mask, offset;
	unsigned long pfn = -1;
	unsigned long start = 0;
	unsigned long bitmap_maxno, bitmap_no, bitmap_count;
	struct page *page = NULL;
	int ret = -ENOMEM;

	if (!rcma || !rcma->count)
		return NULL;

	pr_debug("%s(rcma %p, count %zu, align %d)\n", __func__, (void *)rcma,
		 count, align);

	if (!count)
		return NULL;

	mask = rcma_bitmap_aligned_mask(rcma, align);
	offset = rcma_bitmap_aligned_offset(rcma, align);
	bitmap_maxno = rcma_bitmap_maxno(rcma);
	bitmap_count = rcma_bitmap_pages_to_bits(rcma, count);

	if (bitmap_count > bitmap_maxno)
		return NULL;

	mutex_lock(&rcma->lock);
	bitmap_no = bitmap_find_next_zero_area_off(rcma->bitmap,
			bitmap_maxno, start, bitmap_count, mask,
			offset);
	if (bitmap_no >= bitmap_maxno) {
		mutex_unlock(&rcma->lock);
	} else {
		bitmap_set(rcma->bitmap, bitmap_no, bitmap_count);
		/*
		 * It's safe to drop the lock here. We've marked this region for
		 * our exclusive use.
		 */
		mutex_unlock(&rcma->lock);

		pfn = rcma->base_pfn + (bitmap_no << rcma->order_per_bit);
		page = pfn_to_page(pfn);
		ret = 0;
	}

	if (ret && !no_warn) {
		pr_err("%s: alloc failed, req-size: %zu pages, ret: %d\n",
			__func__, rcma->count, ret);
	}

	pr_debug("%s(): returned %p\n", __func__, page);
	return page;
}

/**
 * rcma_release() - release allocated pages
 * @rcma:   Contiguous memory region for which the allocation is performed.
 * @pages: Allocated pages.
 * @count: Number of allocated pages.
 *
 * This function releases memory allocated by alloc_cma().
 * It returns false when provided pages do not belong to contiguous area and
 * true otherwise.
 */
bool rcma_release(struct rcma *rcma, const struct page *pages, unsigned int count)
{
	unsigned long pfn;

	if (!rcma || !pages)
		return false;

	pfn = page_to_pfn(pages);

	if (pfn < rcma->base_pfn || pfn >= rcma->base_pfn + rcma->count)
		return false;

	VM_BUG_ON(pfn + count > rcma->base_pfn + rcma->count);

	rcma_clear_bitmap(rcma, pfn, count);

	return true;
}

/* ION Reserved CMA heap operations functions */
static int ion_reserved_cma_allocate(struct ion_heap *heap,
				struct ion_buffer *buffer,
			    unsigned long len,
			    unsigned long flags)
{
	struct ion_reserved_cma_heap *rcma_heap = to_rcma_heap(heap);
	struct sg_table *table;
	struct page *pages;
	unsigned long size = PAGE_ALIGN(len);
	unsigned long nr_pages = size >> PAGE_SHIFT;
	unsigned long align = get_order(size);
	int ret;
	struct device *dev = heap->priv;

	if (align > CONFIG_CMA_ALIGNMENT)
		align = CONFIG_CMA_ALIGNMENT;

	pages = rcma_alloc(&(rcma_heap->rcma), nr_pages, align, false);
	if (!pages)
		return -ENOMEM;

	if (!(flags & ION_FLAG_SECURE)) {
		if (PageHighMem(pages)) {
			unsigned long nr_clear_pages = nr_pages;
			struct page *page = pages;

			while (nr_clear_pages > 0) {
				void *vaddr = kmap_atomic(page);

				memset(vaddr, 0, PAGE_SIZE);
				kunmap_atomic(vaddr);
				page++;
				nr_clear_pages--;
			}
		} else {
			memset(page_address(pages), 0, size);
		}
	}

	if (MAKE_ION_ALLOC_DMA_READY ||
	    (flags & ION_FLAG_SECURE) ||
	     (!ion_buffer_cached(buffer)))
		ion_pages_sync_for_device(dev, pages, size,
					  DMA_BIDIRECTIONAL);

	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		goto err;

	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto free_mem;

	sg_set_page(table->sgl, pages, size, 0);

	buffer->priv_virt = pages;
	buffer->sg_table = table;
	return 0;

free_mem:
	kfree(table);
err:
	rcma_release(&(rcma_heap->rcma), pages, nr_pages);
	return -ENOMEM;
}

static void ion_reserved_cma_free(struct ion_buffer *buffer)
{
	struct ion_reserved_cma_heap *rcma_heap = to_rcma_heap(buffer->heap);
	struct page *pages = buffer->priv_virt;
	unsigned long nr_pages = PAGE_ALIGN(buffer->size) >> PAGE_SHIFT;

	rcma_release(&(rcma_heap->rcma), pages, nr_pages);

	/* release sg table */
	sg_free_table(buffer->sg_table);
	kfree(buffer->sg_table);
}

static struct ion_heap_ops ion_reserved_cma_ops = {
	.allocate = ion_reserved_cma_allocate,
	.free = ion_reserved_cma_free,
	.map_user = ion_heap_map_user,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
};

struct ion_heap *ion_reserved_cma_heap_create(struct ion_platform_heap *data)
{
	struct ion_reserved_cma_heap *rcma_heap;
	struct device *dev = (struct device *)data->priv;
	struct page *pages;
	size_t nr_pages;
	int bitmap_size;

	if (!dev->cma_area)
		return ERR_PTR(-EINVAL);

	rcma_heap = kzalloc(sizeof(*rcma_heap), GFP_KERNEL);
	if (!rcma_heap)
		return ERR_PTR(-ENOMEM);

	rcma_heap->heap.ops = &ion_reserved_cma_ops;

	/*
	 * get device from private heaps data, later it will be
	 * used to make the link with reserved CMA memory
	 */
	rcma_heap->cma_area = dev->cma_area;
	rcma_heap->heap.type = ION_HEAP_TYPE_DMA_RESERVED;

	nr_pages = rcma_heap->cma_area->count;
	//nr_pages = cma_get_size(rcma_heap->cma_area) >> PAGE_SHIFT;
	pages = cma_alloc(rcma_heap->cma_area, nr_pages, CONFIG_CMA_ALIGNMENT,
				false);
	if (!pages) {
		kfree(rcma_heap);
		return ERR_PTR(-ENOMEM);
	}

	rcma_heap->rcma.base_pfn = page_to_pfn(pages);
	rcma_heap->rcma.count = nr_pages;
	rcma_heap->rcma.order_per_bit = rcma_heap->cma_area->order_per_bit;

	bitmap_size = BITS_TO_LONGS(rcma_bitmap_maxno(&(rcma_heap->rcma))) *
						sizeof(long);
	rcma_heap->rcma.bitmap = kzalloc(bitmap_size, GFP_KERNEL);
	if (!rcma_heap->rcma.bitmap) {
		cma_release(rcma_heap->cma_area, pages, nr_pages);
		kfree(rcma_heap);
		return ERR_PTR(-ENOMEM);
	}

	return &rcma_heap->heap;
}

void ion_reserved_cma_heap_destroy(struct ion_heap *heap)
{
	struct ion_reserved_cma_heap *rcma_heap =
		container_of(heap, struct ion_reserved_cma_heap, heap);
	struct page *pages = pfn_to_page(rcma_heap->rcma.base_pfn);
	size_t nr_pages = rcma_heap->rcma.count;

	if (!pages) {
		cma_release(rcma_heap->cma_area, pages, nr_pages);
	}

	if (!rcma_heap->rcma.bitmap) {
		kfree(rcma_heap->rcma.bitmap);
	}

	kfree(rcma_heap);
}
