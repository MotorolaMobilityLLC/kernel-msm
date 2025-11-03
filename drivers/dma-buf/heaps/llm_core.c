// SPDX-License-Identifier: GPL-2.0-only
/* Copyright 2025 Moto LLC */

#define pr_fmt(fmt) "llm: " fmt

#include <linux/list.h>
#include <linux/shrinker.h>
#include <linux/spinlock.h>
#include <linux/swap.h>
#include <linux/vmalloc.h>
#include <linux/sched/signal.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/idr.h>
#include <linux/rcupdate.h>
#include <linux/namei.h>
#include "llm_heap.h"
#include "llm.h"



#define MAX_DATA_SLOT 32

struct idx_slots {
	void* slots[MAX_DATA_SLOT];
	pid_t owner[MAX_DATA_SLOT];
	int cur_idx;
	struct mutex i_mutex;
};



static DEFINE_SPINLOCK(llm_cache_lock);
LIST_HEAD(llm_cache_list);

static struct idx_slots filename_idx_arr  = {
	.slots = {0},
	.cur_idx = 0,
	.i_mutex = __MUTEX_INITIALIZER(filename_idx_arr.i_mutex),
};

static struct llmheap_buf_cache *create_llm_cache(char *path,
						loff_t pos,
						loff_t size);

static struct llmheap_buf_cache *find_and_prep_destroy_dmabuf_cache_by_path(char *path);

static void aio_work(struct work_struct *work);

#define LLMHEAP_PATH_MAX 512



int deposit_data(void *data)
{
	int idx;
	if (!data) return -1;
	mutex_lock(&filename_idx_arr.i_mutex);
	idx = filename_idx_arr.cur_idx;
	if (filename_idx_arr.slots[idx])
	{
		kfree(filename_idx_arr.slots[idx]);
	}
	filename_idx_arr.slots[idx] = data;
	filename_idx_arr.owner[idx] = current->pid;
	filename_idx_arr.cur_idx++;
	filename_idx_arr.cur_idx = filename_idx_arr.cur_idx%MAX_DATA_SLOT;
	mutex_unlock(&filename_idx_arr.i_mutex);
	return idx;
}
void *withraw_data(int idx, bool deleted)
{
	void * data  = NULL;
	if ((idx < MAX_DATA_SLOT && idx >=0 ))
	{
		mutex_lock(&filename_idx_arr.i_mutex);
		if ( filename_idx_arr.owner[idx] == current->pid)
		{
			data = filename_idx_arr.slots[idx];
			if (deleted)
				filename_idx_arr.slots[idx] = NULL;
		}
		mutex_unlock(&filename_idx_arr.i_mutex);
	}
	return data;
}

long llmheap_handle_deposit_file_info(unsigned long arg)
{
	struct llm_file_info *param_data;
	unsigned int in_size;
	int ret = 0;

	in_size = sizeof(struct llm_file_info);

	param_data = kmalloc(in_size, GFP_KERNEL);
	if (!param_data)
		return -ENOMEM;

	if (copy_from_user(param_data, (void __user *)arg, in_size) != 0) {
		ret = -EFAULT;
		goto out_free_param_data;
	}


	if (unlikely(strlen(param_data->bin_path) >= LLMHEAP_PATH_MAX)) {
		pr_err("llmheap: WARNING: path:%s len:%ld >= %d \n",
			param_data->bin_path, strlen(param_data->bin_path),
			(unsigned int)LLMHEAP_PATH_MAX);
		kfree(param_data);
		return -EINVAL;
	}

	param_data->idx = deposit_data(param_data);

	pr_debug("idx:%d %llx %llx \n",param_data->idx, param_data->pos, param_data->bin_len);

	if (copy_to_user((void __user *)arg, param_data, in_size) != 0) {
		ret = -EFAULT;
		goto out_free_idx;
	}

	return 0;

out_free_idx:
	withraw_data(param_data->idx, true);
out_free_param_data:
	kfree(param_data);

	return ret;
}

int llmheap_handle_withdraw_file_info(int param_idx)
{
	struct llm_file_info *param_data;

	param_data = withraw_data(param_idx, true);
	if (!param_data)
		return -ENODEV;

	/* release param_data */
	kfree(param_data);

	pr_debug("free idx :%d! \n", param_idx);
	return 0;
}

struct llmheap_buf_cache *find_llm_cache_by_idx(int param_idx)
{
	struct llm_file_info *param_data;
	struct llmheap_buf_cache* llm_cache;

	param_data = withraw_data(param_idx, false);
	if (!param_data)
		return NULL;

	llm_cache =  find_llm_cache_by_path(param_data->bin_path, param_data->pos, DIV_ROUND_UP(param_data->bin_len, PAGE_SIZE), param_idx, true);
	if ( llm_cache) {
		llm_cache->fd = param_data->fd;
	}
	return llm_cache;
}

struct llmheap_buf_cache *create_llm_cache_by_idx(int param_idx, unsigned long len)
{
	struct llm_file_info *param_data;
	struct llmheap_buf_cache *llm_cache;

	param_data = withraw_data(param_idx, false);
	if (!param_data)
		return NULL;
	if (param_data->bin_len != len) return NULL;
	llm_cache = create_llm_cache(param_data->bin_path, param_data->pos,
				 param_data->bin_len);
	if (llm_cache) {
		INIT_WORK(&llm_cache->aio_worker, aio_work);
		llm_cache->fd = param_data->fd;
		llm_cache->idx = param_idx;
	}
	return llm_cache;
}


static int llm_cache_drop_by_path(char *path)
{
	struct llmheap_buf_cache *llm_cache;

retry:
	llm_cache = find_and_prep_destroy_dmabuf_cache_by_path(path);
	if (!llm_cache) {
		pr_err_ratelimited("%s failed to find llm_cache or another destroying is ongoing for %s\n",
			__func__, path);
		return 0;
	}
	if (IS_ERR(llm_cache)) {
		/* wait for releasing */
		schedule_timeout_uninterruptible(HZ / 15);
		goto retry;
	}

	llm_cache_shrink(0, LONG_MAX, llm_cache);
	/* arrive here, remained pages should have been zero */
	WARN_ON(llm_cache->remained_pages);

	llm_cache_destroy(llm_cache);

	return 0;
}

int llm_cache_drop_all(void)
{
	struct llmheap_buf_cache *llm_cache;
	char path[LLMHEAP_PATH_MAX];

	/* drop caches and metadata till llm_cache_list is empty */
again:
	memset(path, 0, LLMHEAP_PATH_MAX);
	rcu_read_lock();
	list_for_each_entry_rcu(llm_cache, &llm_cache_list, list) {
		if (!(llm_cache->flags & DBUF_CACHE_IN_USAGE)) {
			strncpy(path, llm_cache->bin_path, LLMHEAP_PATH_MAX - 1);
			break;
		}
	}
	rcu_read_unlock();

	if (path[0] != 0) {
		llm_cache_drop_by_path(path);
		goto again;
	}

	return 0;
}



inline struct llmheap_buf_cache *find_llm_cache_by_path(char *path, loff_t pos, unsigned long pages, int idx, bool allocate)
{
	struct llmheap_buf_cache *llm_cache = NULL, *target_cache = NULL;

	if (!path)
		return NULL;

	rcu_read_lock();
	list_for_each_entry_rcu(llm_cache, &llm_cache_list, list) {
		if (!strcmp(llm_cache->bin_path, path)
			&& (llm_cache->pos == pos) && ( llm_cache->total_pages == pages)) {
			spin_lock(&llm_cache->lock);
			if (!allocate) {
				if (llm_cache->is_destroying) {
					pr_err("!!!search %s during destroying\n", path);
					spin_unlock(&llm_cache->lock);
					continue;
				}else if ( llm_cache->idx == idx && !(llm_cache->flags & DBUF_CACHE_IN_ALLOC )){
					pr_debug("find llm_cache for %s allocate:%d\n", path, allocate);
					target_cache = llm_cache;
					llm_cache->flags |= DBUF_CACHE_IN_ALLOC;
					spin_unlock(&llm_cache->lock);
					break;
				}
			}else {
				/* dbuf has not been released or llm_cache is destroying */
				if (llm_cache->flags & (DBUF_CACHE_IN_USAGE | DBUF_CACHE_DIRECT_RECLAIM)) {
					pr_info("heap in used %p\n",llm_cache);
					spin_unlock(&llm_cache->lock);
					continue;
				}
				if (llm_cache->is_destroying) {
					pr_debug("%s allocation during destoying\n", path);
					spin_unlock(&llm_cache->lock);
					continue;
				}
								
				if (!llm_cache->pages)
				{
					llm_cache->pages = __vmalloc_array(llm_cache->total_pages, sizeof(struct page *), GFP_KERNEL);
					if (!llm_cache->pages)
						continue;
					memset(llm_cache->pages, 0, llm_cache->total_pages * sizeof(struct page *));
					llm_cache->read_pages = llm_cache->remained_pages = 0;
					pr_debug("relocated pages %p", llm_cache->pages);
				}
				llm_cache->flags |= DBUF_CACHE_IN_USAGE;
				llm_cache->allocated_pages = 0;
				llm_cache->stop_aio_worker = 0;
				llm_cache->read_pages = llm_cache->remained_pages;
				pr_debug("got llm_cache %p for %s allocate:%d\n",llm_cache, path, allocate);
				target_cache = llm_cache;
				spin_unlock(&llm_cache->lock);
				break;
			}
		spin_unlock(&llm_cache->lock);
		}
	}
	rcu_read_unlock();
	return target_cache;
}

static struct llmheap_buf_cache *find_and_prep_destroy_dmabuf_cache_by_path(char *path)
{
	struct llmheap_buf_cache *llm_cache = NULL;

	if (!path)
		return NULL;

	rcu_read_lock();
	list_for_each_entry_rcu(llm_cache, &llm_cache_list, list) {
		if (!strcmp(llm_cache->bin_path, path)) {
			spin_lock(&llm_cache->lock);
			if (llm_cache->flags & DBUF_CACHE_IN_USAGE) {
				pr_err_ratelimited("ERROR!!!%s destroy llm_cache %s before releasing\n",
					__func__, llm_cache->bin_path);
				spin_unlock(&llm_cache->lock);
				continue;
			}

			if (llm_cache->is_destroying) {
				pr_err("ERROR!!!%s search %s during destoying\n", __func__, path);
				spin_unlock(&llm_cache->lock);
				llm_cache = NULL;
				continue;
			} else {
				llm_cache->is_destroying = DESTROY_STAGE1;
			}
			spin_unlock(&llm_cache->lock);
			if (llm_cache){
				break;
			}
		}
	}
	rcu_read_unlock();

	return llm_cache;
}

#define ASYNC_BATCH_IO 32
#define ASYNC_BATCH_SIZE (ASYNC_BATCH_IO * PAGE_SIZE)

struct llmheap_aio {
	struct file *filp;
	loff_t pos;
	unsigned long len;
	struct page **pages;
	struct bio_vec bvec[ASYNC_BATCH_IO];
	unsigned long page_pos;
	unsigned long max_pages;
};

static int llmheap_do_batch_io(struct llmheap_aio *io, struct llmheap_buf_cache *llm_cache)
{
	for (unsigned long len = 0; len < io->len; len += ASYNC_BATCH_SIZE) {
		struct bio_vec *bvec = io->bvec;
		struct iov_iter iter;
		struct kiocb kiocb;
		int nr = (io->len - len) >= ASYNC_BATCH_SIZE ?  ASYNC_BATCH_IO : (io->len - len) / PAGE_SIZE;
		int ret;
		int cnt = 0;

retry:

		for (int i = 0; i < nr; i++) {
			bvec[i].bv_page = io->pages[io->page_pos + len/PAGE_SIZE +  i];

			if (!bvec[i].bv_page)
				pr_err("error for %s pos: %ld allocated_pages %ld read_pages:%lx nr_reclaimed:%ld flags:%lx\n",
					llm_cache->bin_path,
					(unsigned long)((io->pos + len)/ PAGE_SIZE + i),
					llm_cache->allocated_pages,
					llm_cache->read_pages,
					llm_cache->nr_reclaimed,
					llm_cache->flags);
			bvec[i].bv_offset = 0;
			bvec[i].bv_len = PAGE_SIZE;
		}

		init_sync_kiocb(&kiocb, io->filp);
		kiocb.ki_flags |= IOCB_NOWAIT;
		kiocb.ki_pos = io->pos + len;
		iov_iter_bvec(&iter, ITER_DEST, bvec, nr, nr * PAGE_SIZE);

		ret = io->filp->f_op->read_iter(&kiocb, &iter);
		if (ret < 0 && ret != -EAGAIN) {
			pr_err("%s-%s io error at %llx ret:%d\n",
				__func__, current->comm, io->pos, ret);
			return ret;
		}
		if ((ret == -EAGAIN )|| (( ret > 0 ) && (ret != nr* PAGE_SIZE ))) {
			WARN_ON_ONCE(cnt++> 1000);
			schedule_timeout_uninterruptible(1);
			pr_err_ratelimited("AIO retry %lld:%ld:%ld:%d \n",io->pos, io->page_pos, len, ret);
			goto retry;
		}
	}

	return 0;
}

static void aio_work(struct work_struct *work)
{
	unsigned long pos, offs;
	struct llmheap_buf_cache *llm_cache = container_of(work, struct llmheap_buf_cache, aio_worker);
	int ret;

	if (llm_cache->remained_pages == llm_cache->total_pages) {
		pr_debug("nothing llm_cache:%lx v:%ld r:%ld t:%ld\n",
			(unsigned long)llm_cache, llm_cache->remained_pages, llm_cache->read_pages,
			llm_cache->total_pages);
		return;
	}

	offs = llm_cache->pos / PAGE_SIZE;
	pos = offs + llm_cache->remained_pages;

	pr_info("start aio: p:%lx pos:%lx offs:%lx v:%ld r:%ld t:%ld a:%ld\n",
		(unsigned long)llm_cache, pos, offs, llm_cache->remained_pages, llm_cache->read_pages,
		llm_cache->total_pages, llm_cache->allocated_pages);

	while (1) {
		if (llm_cache->allocated_pages >= llm_cache->remained_pages &&
		    llm_cache->allocated_pages  - (pos - offs) >= ASYNC_BATCH_IO) {
			struct llmheap_aio io = {
				.filp = llm_cache->bin_file,
				.pos = pos * PAGE_SIZE,
				.len = ASYNC_BATCH_SIZE,
				.pages = llm_cache->pages,
				.page_pos = llm_cache->read_pages,
				.max_pages = llm_cache->total_pages,
			};
			ret = llmheap_do_batch_io(&io, llm_cache);
			if (ret) {
				pr_err("%s failed to read pos:%lx len:%lx\n", __func__, pos, io.len);
				break;
			}
			pos += ASYNC_BATCH_IO;
			llm_cache->read_pages += ASYNC_BATCH_IO;
		} else if (llm_cache->allocated_pages == llm_cache->total_pages &&
			   (pos - offs) < llm_cache->total_pages) {
			int nr = llm_cache->total_pages - (pos - offs);
			struct llmheap_aio io = {
				.filp = llm_cache->bin_file,
				.pos = pos * PAGE_SIZE,
				.len = nr * PAGE_SIZE,
				.pages = llm_cache->pages,
				.page_pos = llm_cache->read_pages,
				.max_pages = llm_cache->total_pages,
			};
			// pr_info("%s allocated:%lx pos:%lx len:%lx nr:%d\n", __func__, llm_cache->allocated_pages, pos, io.len, nr);
			ret = llmheap_do_batch_io(&io, llm_cache);
			if (ret) {
				pr_err("%s failed to read pos:%lx len:%lx\n", __func__, pos, io.len);
				break;
			}
			pos += nr;
			llm_cache->read_pages += nr;
		} else {
			schedule_timeout_uninterruptible(2);
		}

		if (llm_cache->total_pages - (pos - offs) == 0 ||
		    llm_cache->stop_aio_worker) {
			pr_info("end aio, v:%ld r:%ld t:%ld a:%ld\n",
				llm_cache->remained_pages, llm_cache->read_pages,
				llm_cache->total_pages, llm_cache->allocated_pages);
			break;
		}
	}
}


static struct llmheap_buf_cache *create_llm_cache(char *path,
						loff_t pos,
						loff_t size)
{
	unsigned int nr_pages;
	struct llmheap_buf_cache *llm_cache;

	if (!path)
		return NULL;

	/* create new llm_cache */
	llm_cache = kzalloc(sizeof(*llm_cache), GFP_KERNEL);
	if (!llm_cache)
		return NULL;
	memset(llm_cache, 0, sizeof(*llm_cache));


	nr_pages = DIV_ROUND_UP(size, PAGE_SIZE);
	llm_cache->pages = __vmalloc_array(nr_pages, sizeof(struct page *), GFP_KERNEL);
	if (!llm_cache->pages )
	{
		kfree(llm_cache);
		return NULL;
	}
	llm_cache->pos = pos;
	llm_cache->total_pages = nr_pages;
	strncpy(llm_cache->bin_path, path, sizeof(llm_cache->bin_path) - 1);
	memset(llm_cache->pages, 0, nr_pages * sizeof(struct page *));
	spin_lock_init(&llm_cache->lock);

	llm_cache->flags |= DBUF_CACHE_IN_USAGE;
	smp_mb();

	spin_lock(&llm_cache_lock);
	list_add_tail_rcu(&llm_cache->list, &llm_cache_list);
	spin_unlock(&llm_cache_lock);

	pr_debug("create heap %p for %s pg:%ld n", llm_cache, path, llm_cache->total_pages);

	return llm_cache;
}




/********************* shrink func *********************/
void llm_cache_destroy(struct llmheap_buf_cache *target_cache)
{
	struct llmheap_buf_cache *sync_llm_cache = NULL;

	spin_lock(&llm_cache_lock);
	if (target_cache) {
		spin_lock(&target_cache->lock);
		if (!(target_cache->flags & DBUF_CACHE_IN_USAGE)
		    && !target_cache->remained_pages &&
		    target_cache->is_destroying ==  DESTROY_STAGE1) {
			target_cache->is_destroying = DESTROY_STAGE2;
			sync_llm_cache = target_cache;
			list_del_rcu(&target_cache->list);
		}
		spin_unlock(&target_cache->lock);
	}
	spin_unlock(&llm_cache_lock);
	if (sync_llm_cache) {
		synchronize_rcu();
		if (sync_llm_cache->pages)
			vfree(sync_llm_cache->pages);
		kfree(sync_llm_cache);
	}
}

unsigned long llm_cache_shrink(gfp_t gfp_mask, unsigned long nr_to_scan,
				 struct llmheap_buf_cache *target_cache)
{
	unsigned long nr_total = 0;
	struct page *page, *head;
	unsigned int order, nr;
	struct llmheap_buf_cache *llm_cache, *llm_cache_dropped = NULL;
	int only_scan = 0;
	unsigned long last_valid_idx, idx;
	char path[LLMHEAP_PATH_MAX];
	bool found_shrink = false;

	if (!nr_to_scan)
		only_scan = 1;

	rcu_read_lock();
	list_for_each_entry_rcu(llm_cache, &llm_cache_list, list) {
		if (only_scan) {
			if (!(READ_ONCE(llm_cache->flags) & DBUF_CACHE_IN_USAGE)){
				nr_total += llm_cache->remained_pages;
				found_shrink = true;
			}
		} else {
			if (target_cache && llm_cache != target_cache)
				continue;

			while (nr_total < nr_to_scan) {
				spin_lock(&llm_cache->lock);
				if ((READ_ONCE(llm_cache->flags) & DBUF_CACHE_IN_USAGE) ||
				     READ_ONCE(llm_cache->remained_pages) == 0 ||
				     READ_ONCE(llm_cache->is_destroying) == DESTROY_STAGE2) {
					spin_unlock(&llm_cache->lock);
					if (!READ_ONCE(llm_cache->is_destroying) && (!(READ_ONCE(llm_cache->flags) & DBUF_CACHE_IN_USAGE)))
					{
						llm_cache_dropped = llm_cache;
					}
					break;
				}

				last_valid_idx = llm_cache->remained_pages - 1;
				page = NULL;
				if (llm_cache->pages)
					page = llm_cache->pages[last_valid_idx];					
				if (!page) {
					pr_info("last %ld remain %ld\n", last_valid_idx, llm_cache->remained_pages );
					continue;
				}
				head = compound_head(page);
				order = compound_order(head);
				nr = compound_nr(head);

				for (idx = last_valid_idx - nr + 1; idx <= last_valid_idx; idx++) {
					if (llm_cache->pages)
						llm_cache->pages[idx] = NULL;
				}
				llm_cache->remained_pages -= nr;
				llm_cache->nr_reclaimed += nr;

				spin_unlock(&llm_cache->lock);

				mod_node_page_state(page_pgdat(head), NR_KERNEL_MISC_RECLAIMABLE, -nr);
				__free_pages(head, order);

				nr_total += nr;
			}
		}
	}
	rcu_read_unlock();
	if (found_shrink ) {
		nr_total+=1;
	}else if (llm_cache_dropped)
	{
		memcpy(path, llm_cache_dropped->bin_path, LLMHEAP_PATH_MAX);
		if (path[0] != 0) {
			llm_cache_drop_by_path(path);
			nr_total+=1;
		}
	}
	return nr_total ? nr_total : SHRINK_STOP;
}

static unsigned long dmabuf_cache_shrink_count(struct shrinker *shrinker,
		struct shrink_control *sc)
{
	unsigned long ret;

	ret = llm_cache_shrink(sc->gfp_mask, 0, NULL);
	return (ret == SHRINK_STOP) ? 0 : ret;
}

static unsigned long dmabuf_cache_shrink_scan(struct shrinker *shrinker,
		struct shrink_control *sc)
{
	if (sc->nr_to_scan == 0)
		return 0;
	return llm_cache_shrink(sc->gfp_mask, sc->nr_to_scan, NULL);
}

struct shrinker dmabuf_cache_shrinker = {
	.count_objects = dmabuf_cache_shrink_count,
	.scan_objects = dmabuf_cache_shrink_scan,
	.seeks = DEFAULT_SEEKS,
	.batch = 0,
};

int cache_stat_info(char *buf,  int size)
{
	long i = 0, tot_size, valid_size, read_size;
	struct llmheap_buf_cache *llm_cache;
	int n=0;

	n += snprintf(buf, size,  "Cache Statistics:\n");
	rcu_read_lock();
	list_for_each_entry_rcu(llm_cache, &llm_cache_list, list) {
		tot_size = llm_cache->total_pages << PAGE_SHIFT;
		valid_size = llm_cache->remained_pages << PAGE_SHIFT;
		read_size = llm_cache->read_pages << PAGE_SHIFT;
		n += snprintf(buf+n,size-n, "cache:%ld path:%s pos:%llx(byte)-%llx(page) dma_buf:%lx\n",
				i++, llm_cache->bin_path, llm_cache->pos, llm_cache->pos >> PAGE_SHIFT,
				(unsigned long)llm_cache->dbuf);
		n += snprintf(buf+n, size -n, "  state: in_use:%d reclaimed:%lx\n",
				llm_cache->flags & DBUF_CACHE_IN_USAGE ? 1 : 0, llm_cache->nr_reclaimed);
		n += snprintf(buf+n, size-n, "  size: t:[%lx pg (%ld M)] v:[%lx pg (%ld M)] r:[%lx pg (%ld M)]\n\n",
				llm_cache->total_pages, tot_size / SZ_1M,
				llm_cache->remained_pages, valid_size / SZ_1M,
				llm_cache->read_pages, read_size / SZ_1M);
	}
	rcu_read_unlock();

	return n;
}


int llm_cache_init(void)
{
	shrinker_register(&dmabuf_cache_shrinker);
	return 0;
}
