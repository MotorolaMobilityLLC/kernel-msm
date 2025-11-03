// SPDX-License-Identifier: GPL-2.0-only
/* Copyright 2025 Moto LLC */

#ifndef __LLM_H__
#define __LLM_H__

#define DBUF_CACHE_IN_USAGE	 0x1
#define DBUF_CACHE_IN_ALLOC	 0x2
#define DBUF_CACHE_DIRECT_RECLAIM   0x4

#define LLMHEAP_PATH_MAX 512

struct llmheap_buf_cache {
	unsigned long flags;
	struct list_head list;
	struct rcu_head rcu;
	u64 pos;
	unsigned long remained_pages;
	unsigned long total_pages;
	unsigned long allocated_pages;
	unsigned long read_pages;
	unsigned long nr_reclaimed;
	spinlock_t lock;
	struct work_struct aio_worker;
	bool stop_aio_worker;
	int is_destroying;
	struct page **pages;
	struct dma_buf *dbuf;
	char bin_path[LLMHEAP_PATH_MAX];
	struct file *bin_file;
	int fd;
	int idx;
};

struct llm_load_dma_buf {
	u64 file_offs;
	u64 size;
	int fd;
	int dma_buf_fd;
	char bin_path[LLMHEAP_PATH_MAX];
};

/* in/out data from user */
struct llm_file_info {
	int idx; /* out */
	/* in */
	int fd;
	__u64 pos; /* unit: byte */
	__u64 bin_len;
	char bin_path[LLMHEAP_PATH_MAX];
};



#define LLMHEAP_FLAGS BIT(63)

#define DESTROY_STAGE1	1
#define DESTROY_STAGE2	2




DECLARE_STATIC_KEY_TRUE(llmheap_enable);
DECLARE_STATIC_KEY_FALSE(cache_enable);

static inline bool llmheap_enabled(void)
{
	return static_branch_likely(&llmheap_enable);
}

static inline bool cache_enabled(void)
{
	return static_branch_likely(&cache_enable);
}

int llmheap_dev_init(void);
int llm_heap_fs_init(void);
int llm_cache_init(void);
struct llmheap_buf_cache *find_llm_cache_by_path(char *path, loff_t pos, unsigned long pages, int idx, bool allocate);
struct llmheap_buf_cache *create_llm_cache_by_idx(int param_idx, unsigned long len);
struct llmheap_buf_cache *find_llm_cache_by_idx(int param_idx);

int llm_cache_drop_all(void);

long llmheap_handle_deposit_file_info(unsigned long arg);
int llmheap_handle_withdraw_file_info(int param_idx);

unsigned long llm_cache_shrink(gfp_t gfp_mask, unsigned long nr_to_scan,
					struct llmheap_buf_cache *target_cache);
void llm_cache_destroy(struct llmheap_buf_cache *target_cache);

long llmheap_handle_load_data(unsigned long arg);

int cache_stat_info(char *buf,  int size);

#endif  /* __LLM_H__ */

