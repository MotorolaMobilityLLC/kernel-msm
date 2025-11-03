// SPDX-License-Identifier: GPL-2.0-only
/* Copyright 2025 Moto LLC */

#define pr_fmt(fmt) "llm: " fmt

#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/bvec.h>
#include <linux/kthread.h>
#include <linux/ctype.h>
#include "llm_heap.h"
#include "llm.h"


MODULE_IMPORT_NS(DMA_BUF);

#define AIIO_THREADS 4
#define BATCH_IO 128
#define BATCH_SIZE (BATCH_IO * PAGE_SIZE)

struct llmheap_io {
	struct file *filp;
	loff_t pos;
	unsigned long len;
	unsigned long comp_len;
	struct bio_vec bvec[BATCH_IO];
	unsigned long page_pos;
	unsigned long max_pages;
};


static struct page **io_pages;
static struct task_struct *tasks[AIIO_THREADS];
static struct llmheap_io io[AIIO_THREADS + 1];
static DEFINE_MUTEX(llmheap_mutex);

static int llmheap_do_batch_io(struct llmheap_io *io)
{
	int ret = 0;
	for (unsigned long len = 0; len < io->len; len += BATCH_SIZE) {
		struct bio_vec *bvec = io->bvec;
		struct iov_iter iter;
		struct kiocb kiocb;
		ret = 0;

retry:
		for (int i = 0; i < BATCH_IO; i++) {
			bvec[i].bv_page = io_pages[io->page_pos + len/ PAGE_SIZE + i];
			bvec[i].bv_offset = 0;
			bvec[i].bv_len = PAGE_SIZE;
			// if ((io->max_pages) <= (io->page_pos + len/ PAGE_SIZE + i))
			// {
			// 	pr_info("%s max %ld  page_pos %ld i %d len %ld", __func__, io->max_pages, io->page_pos, i, len);
			// }
		}

		init_sync_kiocb(&kiocb, io->filp);
		kiocb.ki_flags |= IOCB_NOWAIT;
		kiocb.ki_pos = io->pos + len;
		iov_iter_bvec(&iter, ITER_DEST, bvec, BATCH_IO, BATCH_SIZE);
		ret = io->filp->f_op->read_iter(&kiocb, &iter);
		io->comp_len += BATCH_SIZE;
		if (ret < 0 && ret != -EAGAIN) {
			pr_err("%s-%s io error aIt %lld len %ldret:%d\n",
				__func__, current->comm, io->pos, len, ret);
			goto out;
		}
		if ((ret == -EAGAIN )|| (( ret > 0 ) && (ret != BATCH_SIZE ))) {
			schedule_timeout_uninterruptible(1);
			pr_err_ratelimited("%s IO retry %lld:%ld:%ld:%d  \n", __func__, io->pos, io->page_pos, len, ret);
			goto retry;
		}
	}

out:
	return ret >= 0? 0:ret;
}

static int llmheap_io_task(void *data)
{
	struct llmheap_io *io = (struct llmheap_io *)data;

	return llmheap_do_batch_io(io);
}


long llmheap_handle_load_data(unsigned long arg)
{
	struct llm_load_dma_buf buf;
	struct dma_buf *dbuf;
	loff_t pos, size;
	struct file *bin_file;
	int idx = -2;
	int ret;

	struct llmheap_buf_cache *llm_cache;
	unsigned long off;

	ret = copy_from_user(&buf, (void __user *)arg, sizeof(buf));
	if (ret) {
		pr_err("%s fail to get arg\n", __func__);
		return -EFAULT;
	}

	if (unlikely(strlen(buf.bin_path) >= LLMHEAP_PATH_MAX)) {
		pr_err("llmheap: WARNING: func:%s path:%s len:%ld >= %d \n",
			__func__, buf.bin_path, strlen(buf.bin_path),
			(unsigned int)LLMHEAP_PATH_MAX);
		return -EINVAL;
	}

	/* reset ret=0 */
	ret = 0;
	if (buf.size & LLM_HEAP_FLAG) {
		idx = (buf.size >> 48) & (~(1 << 15));
		buf.size = buf.size & (SZ_4G - 1);
	}

	pos = buf.file_offs;
	buf.bin_path[LLMHEAP_PATH_MAX - 1] = 0;

	bin_file = fget(buf.fd);
	if (IS_ERR_OR_NULL(bin_file)) {
		pr_debug("%s failed to open (%s): %ld\n", __func__, buf.bin_path, PTR_ERR(bin_file));
		return -EINVAL;
	}else if (!S_ISREG(file_inode(bin_file)->i_mode))
	{
		pr_err("not file %d\n", buf.fd);
		fput(bin_file);
		return -EBADF;
	}
	size = i_size_read(file_inode(bin_file));
	if (size > SZ_4G || pos >= size ) {
		pr_info("exceed size  %lld\n", size);
		fput(bin_file);
		return -EINVAL;
	}
	buf.size = min_t(unsigned long, buf.size, size - pos);

	dbuf = dma_buf_get(buf.dma_buf_fd);
	if (IS_ERR(dbuf)) {
		pr_err("fail to dma_buf_get\n");
		fput(bin_file);
		return PTR_ERR(dbuf);
	}

	mutex_lock(&llmheap_mutex);

	llm_cache = find_llm_cache_by_path(buf.bin_path, pos,DIV_ROUND_UP(buf.size, PAGE_SIZE), idx, false);


	if (likely(llm_cache)) {
		spin_lock(&llm_cache->lock);
		io_pages = llm_cache->pages;
		llm_cache->stop_aio_worker = 1;
		spin_unlock(&llm_cache->lock);
		flush_work(&llm_cache->aio_worker);
		if (!IS_ERR_OR_NULL(llm_cache->bin_file))
			fput(llm_cache->bin_file);
		llm_cache->bin_file = NULL;
		spin_lock(&llm_cache->lock);

		off =  llm_cache->read_pages << PAGE_SHIFT;

		if (off < buf.size) {
			// pos = max_t(unsigned long, pos, off);
			// pr_info("pos %lld filesize %ld off\n", pos, size, off);
			pos = pos + off;
			buf.size = min_t(unsigned long, buf.size, buf.size - off);
		} else {
			pos = buf.size - 1;
			buf.size = 0;
		}

		if (!(llm_cache->flags & DBUF_CACHE_IN_USAGE)) {
			pr_err("!!!  %s whose dma-buf has been closed\n",
				buf.bin_path);
			spin_unlock(&llm_cache->lock);
			mutex_unlock(&llmheap_mutex);
			dma_buf_put(dbuf);
			fput(bin_file);
			return -EINVAL;
		}

		if (!buf.size) {
			llm_cache->stop_aio_worker = 1;
			spin_unlock(&llm_cache->lock);
			flush_work(&llm_cache->aio_worker);
			if (!IS_ERR_OR_NULL(llm_cache->bin_file))
				fput(llm_cache->bin_file);
			llm_cache->bin_file = NULL;
			mutex_unlock(&llmheap_mutex);
			pr_info("no read: cache:%lx v:%ld r:%ld t:%ld\n",
				(unsigned long)llm_cache, llm_cache->remained_pages, llm_cache->read_pages,
				llm_cache->total_pages);
			dma_buf_put(dbuf);
			fput(bin_file);
			if (!cache_enabled())
				llm_cache->flags |= DBUF_CACHE_DIRECT_RECLAIM;
			return 0;
		}
		spin_unlock(&llm_cache->lock);

	} else {
		pr_err("ERROR!!! cache for %s \n",  buf.bin_path);
		mutex_unlock(&llmheap_mutex);
		dma_buf_put(dbuf);
		fput(bin_file);
		return -EINVAL;
	}

	pr_info("start read: heap:%lx v:%ld r:%ld t:%ld\n",
		(unsigned long)llm_cache, llm_cache->remained_pages, llm_cache->read_pages,
		llm_cache->total_pages);

	do {
		unsigned long remained_len = buf.size % (AIIO_THREADS * BATCH_SIZE);
		unsigned long len = buf.size - remained_len;
		int i = 0;

		if (len > 0) {
			for (i = 0; i < AIIO_THREADS; i++) {
				io[i].filp = bin_file;
				io[i].len = len / AIIO_THREADS;
				io[i].comp_len = 0;
				io[i].pos = pos + i * len / AIIO_THREADS;
				io[i].page_pos = llm_cache->read_pages + (i* len/AIIO_THREADS) / PAGE_SIZE;
				io[i].max_pages = llm_cache->total_pages;
				tasks[i] = kthread_create(llmheap_io_task, &io[i], "llmheap_io/%d", i);
				if (IS_ERR(tasks[i])) {
					pr_err("failed to create io thread%d\n", i);
					break;
				}
			}
			if (i != AIIO_THREADS) {
				for (int j = 0; j < i; j++) {
					kthread_stop(tasks[j]);
					tasks[j] = NULL;
				}
				spin_lock(&llm_cache->lock);
				llm_cache->flags |= DBUF_CACHE_DIRECT_RECLAIM;
				spin_unlock(&llm_cache->lock);

				ret = PTR_ERR(tasks[i]);
				tasks[i] = NULL;
				goto out;
			}
			for (i = 0; i < AIIO_THREADS; i++) {
				get_task_struct(tasks[i]);
				wake_up_process(tasks[i]);
			}

			pos += len;
			llm_cache->read_pages += (len/PAGE_SIZE);
		}

		i = AIIO_THREADS;
		len = remained_len - (remained_len % BATCH_SIZE);
		if (len > 0) {
			io[i].filp = bin_file;
			io[i].len = len;
			io[i].comp_len = 0;
			io[i].pos = pos;
			io[i].page_pos = llm_cache->read_pages;
			io[i].max_pages = llm_cache->total_pages;
			ret = llmheap_do_batch_io(&io[i]);
			if (ret < 0) {
				spin_lock(&llm_cache->lock);
				llm_cache->flags |= DBUF_CACHE_DIRECT_RECLAIM;
				spin_unlock(&llm_cache->lock);
				pr_err("%s-%s io error at %llx ret:%d\n",
						__func__, current->comm, io->pos, ret);
				goto out;
			}
			pos += len;
			llm_cache->read_pages += (len/PAGE_SIZE);
		}

		remained_len %= BATCH_SIZE;
		if (remained_len > 0) {
			int npages = DIV_ROUND_UP(remained_len, PAGE_SIZE);
			struct bio_vec *bvec = io[i].bvec;
			struct iov_iter iter;
			struct kiocb kiocb;

io_retry:
			for (i = 0; i < npages; i++) {
				bvec[i].bv_page = io_pages[llm_cache->read_pages +  i];
				bvec[i].bv_offset = 0;
				bvec[i].bv_len = PAGE_SIZE;
				// if ((llm_cache->total_pages ) <= ( llm_cache->read_pages +  i ))
				// {
				// 	pr_info("%s r max %ld i %d", __func__, llm_cache->total_pages , i);
				// }
			}
			init_sync_kiocb(&kiocb, bin_file);
			kiocb.ki_pos = pos;
			kiocb.ki_flags |= IOCB_NOWAIT;
			iov_iter_bvec(&iter, ITER_DEST, bvec, npages, npages * PAGE_SIZE);
			ret = bin_file->f_op->read_iter(&kiocb, &iter);
			if (ret < 0 && ret != -EAGAIN) {
				spin_lock(&llm_cache->lock);
				llm_cache->flags |= DBUF_CACHE_DIRECT_RECLAIM;
				spin_unlock(&llm_cache->lock);

				pr_err("%s-%s io error at %llx ret:%d\n",
						__func__, current->comm, io->pos, ret);
				goto out;
			}
			if ((ret == -EAGAIN )|| (( ret > 0 ) && (ret != npages * PAGE_SIZE ))) {
				schedule_timeout_uninterruptible(1);
				pr_err_ratelimited("%s IO retry %lld:%ld:%d(pg):%d  \n", __func__, io->pos, llm_cache->read_pages, npages,ret);
				goto io_retry;
			}
			llm_cache->read_pages += npages;
		}
	} while (0);

out:
	schedule_timeout_uninterruptible(1);
	for (int i = 0; i < AIIO_THREADS && tasks[i]; i++) {
		int task_ret;
		int cnt = 0;

		/* make sure io threads have started */
		while (!READ_ONCE(io[i].comp_len) && cnt++ < 500)
			schedule_timeout_uninterruptible(1);
		WARN_ON(cnt >= 500);

		task_ret = kthread_stop(tasks[i]);
		put_task_struct(tasks[i]);
		tasks[i] = NULL;
		if (task_ret) {
			if (llm_cache) {
				spin_lock(&llm_cache->lock);
				llm_cache->flags |= DBUF_CACHE_DIRECT_RECLAIM;
				spin_unlock(&llm_cache->lock);
			}
			ret = task_ret;
			pr_err("thread%i io failed, ret:%d\n", i, ret);
		}
	}
	mutex_unlock(&llmheap_mutex);

	pr_info("end read: heap:%lx v:%ld r:%ld t:%ld ret:%d\n",
		(unsigned long)llm_cache, llm_cache->remained_pages, llm_cache->read_pages,
		llm_cache->total_pages, ret);
	dma_buf_put(dbuf);
	fput(bin_file);
	if (!cache_enabled()) {
		llm_cache->flags |= DBUF_CACHE_DIRECT_RECLAIM;
		vfree(llm_cache->pages);
		llm_cache->pages = NULL;
	}
	return ret >= 0 ? 0 : ret;
}


int llmheap_dev_init(void)
{
	int ret;

	ret = llm_cache_init();
	if (ret) {
		pr_err("Failed to dmabuf_cache_init\n");
		return ret;
	}

	pr_info("llmheap registered.\n");
	return 0;
}
