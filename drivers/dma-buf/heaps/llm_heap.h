// SPDX-License-Identifier: GPL-2.0-only
/* Copyright 2025 Moto LLC */

#ifndef __LLM_HEAP_H__
#define __LLM_HEAP_H__

#include <linux/types.h>

#define LLM_HEAP_FLAG BIT(63)

int llmheap_init(void);
void *find_or_create_llm_cache(unsigned long *len);
bool llm_cache_release(void* buffer);
struct page *get_page_from_llm_cache(void* dbuf, unsigned long idx);
void llm_cache_add_pages( void *dbuf, struct page *page);
void llm_cache_init_dbuf( void *dbuf, struct dma_buf *dmabuf);

#endif /* __LLM_HEAP_H__ */
