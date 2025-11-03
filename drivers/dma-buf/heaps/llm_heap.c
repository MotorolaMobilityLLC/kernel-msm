// SPDX-License-Identifier: GPL-2.0-only
/* Copyright 2025 Moto LLC */

#define pr_fmt(fmt) "llm: " fmt


#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/bvec.h>
#include <linux/kthread.h>
#include <linux/namei.h>

#include "qcom_sg_ops.h"
#include "llm.h"
#include "llm_heap.h"



void *find_or_create_llm_cache(unsigned long *len)
{
	struct llmheap_buf_cache *llm_cache = NULL;
	/* decode param_idx from len */
	unsigned long real_len = *len & (SZ_4G - 1);
	/* Node: (1 << 63) for identify encode! */
	unsigned int param_idx = (*len >> 48) & (~(1 << 15));

	if (*len & LLMHEAP_FLAGS ) {
		*len = real_len;

		if (!llmheap_enabled()) {
			pr_err("!!!!FIXME: call llm_cache allocation after disabling llmheap\n");
			llmheap_handle_withdraw_file_info(param_idx);
			return ERR_PTR(-ENOMEM);
		}

		/* case: use dbuf cache */
		llm_cache = find_llm_cache_by_idx(param_idx);
		if (!llm_cache) {
			/* first: llm_cache->dbuf set null */
			llm_cache = create_llm_cache_by_idx(param_idx, real_len);
			if (!llm_cache) {
				pr_err("fix: fail to create_llm_cache\n");
				llmheap_handle_withdraw_file_info(param_idx);
				return ERR_PTR(-ENOMEM);
			}
		} else if (IS_ERR(llm_cache)) {
			llmheap_handle_withdraw_file_info(param_idx);
			goto out;
		}
		llm_cache->idx = param_idx;
        llm_cache->allocated_pages = llm_cache->remained_pages;
		pr_debug("allocated pages %ld rem:%ld\n", llm_cache->allocated_pages, llm_cache->remained_pages);
		llmheap_handle_withdraw_file_info(param_idx);

		llm_cache->bin_file = fget(llm_cache->fd);
		if (IS_ERR_OR_NULL(llm_cache->bin_file)) {
			pr_err("failed to open %s %p \n", llm_cache->bin_path, llm_cache->bin_file);
			llm_cache->bin_file = NULL;
		}else if (S_ISREG(file_inode(llm_cache->bin_file)->i_mode))
        {
		    schedule_work(&llm_cache->aio_worker);
        }else {
            pr_err(" not file and released it %d\n", llm_cache->fd);
            fput(llm_cache->bin_file);
            llm_cache->bin_file = NULL;
        }
	}

out:
	return llm_cache;
}

bool llm_cache_release(void *buffer)
{
    if (!buffer)
        return false;
    struct qcom_sg_buffer *qcom_buffer = (struct qcom_sg_buffer*)buffer;

	struct llmheap_buf_cache *llm_cache = (struct llmheap_buf_cache *)qcom_buffer->dbuf;
	if (!llm_cache)
		return false;


	if (!llm_cache->stop_aio_worker) {
		int i;

		struct sg_table *table;
		struct scatterlist *sg;
		spin_lock(&llm_cache->lock);
		llm_cache->dbuf = NULL;
		llm_cache->stop_aio_worker = 1;
		spin_unlock(&llm_cache->lock);

		flush_work(&llm_cache->aio_worker);
        if (!IS_ERR_OR_NULL(llm_cache->bin_file))
				fput(llm_cache->bin_file);
		llm_cache->bin_file = NULL;

		spin_lock(&llm_cache->lock);
		llm_cache->idx = -1;
		llm_cache->fd = -1;
		llm_cache->read_pages = 0;
		llm_cache->allocated_pages = 0;
		llm_cache->remained_pages = 0;
		if (llm_cache->pages)
			memset(llm_cache->pages, 0, llm_cache->total_pages * sizeof(struct page *));
		// llm_cache->flags &= ~DBUF_CACHE_IN_USAGE;
		llm_cache->flags = 0;

		pr_debug("io for %p v:%ld t:%ld a:%ld r:%ld w:%d\n",
				llm_cache, llm_cache->remained_pages, llm_cache->total_pages,
				llm_cache->allocated_pages, llm_cache->read_pages, llm_cache->stop_aio_worker);
		spin_unlock(&llm_cache->lock);

		table = &qcom_buffer->sg_table;
		for_each_sgtable_sg(table, sg, i) {
			struct page *page = sg_page(sg);
			// put_page(page);
			__free_pages(page, compound_order(page));
		}
	} else {
		if (llm_cache->flags & DBUF_CACHE_DIRECT_RECLAIM) {
			int i;

			struct sg_table *table;
			struct scatterlist *sg;

			spin_lock(&llm_cache->lock);

			pr_debug("free for %p v:%ld t:%ld a:%ld r:%ld w:%d\n",
				llm_cache, llm_cache->remained_pages, llm_cache->total_pages,
				llm_cache->allocated_pages, llm_cache->read_pages, llm_cache->stop_aio_worker);
			llm_cache->dbuf = NULL;
			llm_cache->read_pages = 0;
			llm_cache->allocated_pages = 0;
			llm_cache->remained_pages = 0;
			llm_cache->idx = -1;
			llm_cache->fd = -1;
			if (llm_cache->pages)
				memset(llm_cache->pages, 0, llm_cache->total_pages * sizeof(struct page *));
			llm_cache->flags = 0;
			// llm_cache->flags &= ~DBUF_CACHE_DIRECT_RECLAIM;
			// llm_cache->flags &= ~DBUeF_CACHE_IN_USAGE;
			spin_unlock(&llm_cache->lock);
			table = &qcom_buffer->sg_table;
			for_each_sgtable_sg(table, sg, i) {
				struct page *page = sg_page(sg);
				__free_pages(page, compound_order(page));
			}
			return true;
		}

		spin_lock(&llm_cache->lock);
		pr_debug("release for %p v:%ld t:%ld a:%ld r:%ld w:%d\n",
				llm_cache, llm_cache->remained_pages, llm_cache->total_pages,
				llm_cache->allocated_pages, llm_cache->read_pages, llm_cache->stop_aio_worker);
		llm_cache->idx = -1;
		llm_cache->fd = -1;
		llm_cache->remained_pages = llm_cache->total_pages;
		llm_cache->dbuf = NULL;
		// llm_cache->flags &= ~DBUF_CACHE_IN_USAGE;
		llm_cache->flags = 0;
		/* we have only one NUMA node */
		if (llm_cache->pages)
			mod_node_page_state(page_pgdat(llm_cache->pages[0]),
					NR_KERNEL_MISC_RECLAIMABLE, llm_cache->total_pages);
		spin_unlock(&llm_cache->lock);
	}

	return true;
}

/********************* dma-buf cache operation set * *********************/
struct page *get_page_from_llm_cache(void *dbuf,unsigned long idx)
{
	struct llmheap_buf_cache *llm_cache = (struct llmheap_buf_cache *)dbuf;
	if (llm_cache && llm_cache->remained_pages > 0 &&
	    idx < llm_cache->remained_pages)
		return llm_cache->pages[idx];

	return NULL;
}


void llm_cache_add_pages(void *dbuf,  struct page *page)
{
	int j;
	unsigned long nr;
	unsigned long idx;
	struct llmheap_buf_cache *llm_cache = (struct llmheap_buf_cache *)dbuf;

	if (!llm_cache)
		return;

	idx = llm_cache->allocated_pages;
	/* All pages from sglist of dma-buf are added to llm_cache->pages */

	nr = compound_nr(page);
	if ((idx + nr) > llm_cache->total_pages)
	{
		pr_info("%p exceed page %lx %lx %lx", llm_cache, llm_cache->total_pages, idx ,nr);
		return;
	}

	spin_lock(&llm_cache->lock);
	nr = compound_nr(page);
	if (!llm_cache->pages[idx]) {
		for (j = 0; j < nr; j++)
			llm_cache->pages[idx + j] = page + j;
	}
	llm_cache->allocated_pages += nr;
	spin_unlock(&llm_cache->lock);
}

void llm_cache_init_dbuf( void *dbuf, struct dma_buf *dmabuf)
{
	struct llmheap_buf_cache *llm_cache = (struct llmheap_buf_cache*)dbuf;
	if (!llmheap_enabled() || !llm_cache)
		return;

	/* reset/record llm_cache->dbuf */
	spin_lock(&llm_cache->lock);
	llm_cache->dbuf = dmabuf;
	mod_node_page_state(page_pgdat(llm_cache->pages[0]),
			NR_KERNEL_MISC_RECLAIMABLE, -llm_cache->remained_pages);
	spin_unlock(&llm_cache->lock);
}


int llmheap_init(void)
{
    llmheap_dev_init();
    return llm_heap_fs_init();
}
