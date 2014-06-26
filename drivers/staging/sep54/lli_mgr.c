/*******************************************************************
* (c) Copyright 2011-2012 Discretix Technologies Ltd.              *
* This software is protected by copyright, international           *
* treaties and patents, and distributed under multiple licenses.   *
* Any use of this Software as part of the Discretix CryptoCell or  *
* Packet Engine products requires a commercial license.            *
* Copies of this Software that are distributed with the Discretix  *
* CryptoCell or Packet Engine product drivers, may be used in      *
* accordance with a commercial license, or at the user's option,   *
* used and redistributed under the terms and conditions of the GNU *
* General Public License ("GPL") version 2, as published by the    *
* Free Software Foundation.                                        *
* This program is distributed in the hope that it will be useful,  *
* but WITHOUT ANY LIABILITY AND WARRANTY; without even the implied *
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. *
* See the GNU General Public License version 2 for more details.   *
* You should have received a copy of the GNU General Public        *
* License version 2 along with this Software; if not, please write *
* to the Free Software Foundation, Inc., 59 Temple Place - Suite   *
* 330, Boston, MA 02111-1307, USA.                                 *
* Any copy or reproduction of this Software, as permitted under    *
* the GNU General Public License version 2, must include this      *
* Copyright Notice as well as any other notices provided under     *
* the said license.                                                *
********************************************************************/

 /*!
  * \file lli_mgr.c
  * \brief LLI logic: Bulding MLLI tables from user virtual memory buffers
  */

#define SEP_LOG_CUR_COMPONENT SEP_LOG_MASK_LLI_MGR

#include <linux/slab.h>
#include <linux/dmapool.h>
#include <linux/pagemap.h>
#include "sep_ctx.h"
#include "dx_driver.h"
#include "sep_log.h"
#include "lli_mgr.h"

/* Limitation of DLLI buffer size due to size of "SIZE" field */
/* Set this to 0 in order to disable DLLI support */
/*#define DLLI_BUF_LIMIT \
	((1UL << SEP_SW_DESC_CRYPTO_OP_IFT_SIZE_BIT_SIZE) - 1)*/
/* For now limit to the size planned for DLLI_AUX_BUF_LIMIT because we
   must always have both din and dout DLLI or MLLI. When mixing would be
   supported we can increase this limit to the one in comment, above */
#define DLLI_BUF_LIMIT 2048

/* The following size defines up to which size we would optimize for DLLI
   buffer descriptor even for non-contiguous buffers. If not physically
   contiguous (or not cache aligned on platforms with no cache coherency) an
   auxilliary buffer would be allocated and the data copied to/from it. */
#define DLLI_AUX_BUF_LIMIT 2048
/* Note: The value of DLLI_AUX_BUF_LIMIT is tuned based on emipirical tests
   on our system so the memcpy overhead does not "consume" the performance
   benefit of using DLLI */

#if DLLI_AUX_BUF_LIMIT > DLLI_BUF_LIMIT
#error DLLI_AUX_BUF_LIMIT is too large. May be at most 64KB-1
#endif

#if (SEP_SUPPORT_SHA > 256)
#define MAX_CRYPTO_BLOCK_LOG2 7
#else
#define MAX_CRYPTO_BLOCK_LOG2 6
#endif
#define MAX_CRYPTO_BLOCK_SIZE (1 << MAX_CRYPTO_BLOCK_LOG2)
#define MAX_CRYPTO_BLOCK_MASK (MAX_CRYPTO_BLOCK_SIZE - 1)

#define SEP_LLI_ENTRY_BYTE_SIZE (SEP_LLI_ENTRY_WORD_SIZE * sizeof(u32))

/* Index of first LLI which encodes data buffer
   (after "next VA" and "next DMA") */
#define FIRST_DATA_LLI_INDEX 2

/* Overhead for tables linked list:
 * one entry for FW linked list + one for host/kernel linked list
 * (required due to difference between dma_addr and kernel virt. addr.) +
 * last entry is reserved to the protected entry with the stop bit */
#define SEP_MLLI_LINK_TO_NEXT_OVERHEAD 3

/* macro to set/get link-to-next virtual address for next MLLI
 * This macro relies on availability of an extra LLI entry per MLLI table.
 * It uses the space of the first entry so the "SeP" table start after it.
 */
#define SEP_MLLI_SET_NEXT_VA(cur_mlli_p, next_mlli_p) \
do { \
	u32 __phys_ptr_ = virt_to_phys(next_mlli_p) & (DMA_BIT_MASK(32));\
	SEP_LLI_SET(cur_mlli_p , ADDR, __phys_ptr_);\
} while (0)
#define SEP_MLLI_SET_NEXT_VA_NULL(cur_mlli_start) \
		SEP_MLLI_SET_NEXT_VA(mlli_table_p, 0)
#define SEP_MLLI_GET_NEXT_VA(cur_mlli_start) \
	SEP_LLI_GET((cur_mlli_start), ADDR) == 0 ? 0 :  ((u32 *)phys_to_virt(SEP_LLI_GET((cur_mlli_start), ADDR)));\


#define CACHE_LINE_MASK (L1_CACHE_BYTES - 1)

/* Number of data bytes to gather at last LLI of for end of Din buffer */
#define DIN_LAST_LLI_GATHER_SIZE 32

/* Select the client buffer amount to copy into aux. buffers (head/tail) */
#ifdef CONFIG_NOT_COHERENT_CACHE
#if (DIN_LAST_LLI_GATHER_SIZE > L1_CACHE_BYTES)
#define EDGE_BUFS_POOL_ITEM_SIZE DIN_LAST_LLI_GATHER_SIZE
#else
#define EDGE_BUFS_POOL_ITEM_SIZE L1_CACHE_BYTES
#endif
#else	/* Coherent cache - only tail buffer required per CC requirements */
#define EDGE_BUFS_POOL_ITEM_SIZE DIN_LAST_LLI_GATHER_SIZE
#endif

/* Similar to for_each_sg but no need for nents - runs until NULL */
#define for_each_valid_sg(sglist, cur_sge)	\
	for (cur_sge = (sglist); cur_sge != NULL; cur_sge = sg_next(cur_sge))

/**
 * struct llimgr_obj - The LLI manager object (exposed as llimgr_h)
 * @dev:	The associated device context (for DMA operations)
 * @mlli_cache:	DMA coherent memory pool for the MLLI tables
 * @edge_bufs_pool:	Pool for auxilliary buffers used instead of user buffer
 *			start/end. Used for last LLI data mirroring to fulfil
 *			requirement of 32B on last LLI.
 *			In case of a non-coherent cache and a data buffer
 *			which starts/ends unaligned to cache line we should
 *			allocate external buffer to be used as the first/last
 *			LLI entry instead of the "tail". This is required to
 *			avoid cache incoherency due to access by other process
 *			entities to the cache lines where the data is. This is
 *			required only for the output buffer where the same cache
 *			line is accessed by the host processor while SeP should
 *			DMA output data into it.
 * @dlli_bufs_pool:	Pool for client buffers up to DLLI_AUX_BUF_LIMIT which
 *			are not phys. contig. and copied into it in order to be
 *			physically contiguous, thus suitable for DLLI access.
 * @max_lli_num:	Maximum LLI entries number in MLLI table.
 * @max_data_per_mlli:	Maximum bytes of data mapped by each MLLI table.
 *
 */
struct llimgr_obj {
	struct device *dev;
	struct dma_pool *mlli_cache;
	struct dma_pool *edge_bufs_pool;
	struct dma_pool *dlli_bufs_pool;
	unsigned int max_lli_num;
	unsigned long max_data_per_mlli;
};

/* Iterator state for building the MLLI tables list */
struct mlli_tables_list_iterator {
	u32 *prev_mlli_table_p;
	u32 *cur_mlli_table_p;
	dma_addr_t cur_mlli_dma_addr;
	unsigned int next_lli_idx; /* Data LLI (After FIRST_DATA_LLI_INDEX) */
	unsigned long cur_mlli_accum_data; /* Accumulated in current MLLI */
};

static void cleanup_mlli_tables_list(struct llimgr_obj *llimgr_p,
				     struct mlli_tables_list *mlli_tables_ptr,
				     int is_data_dirty);
static inline unsigned int get_sgl_nents(struct scatterlist *sgl);

/**
 * llimgr_create() - Create LLI-manager object
 * @dev:	 Device context
 * @mlli_table_size:	 The maximum size of an MLLI table in bytes
 *
 * Returns llimgr_h Created object handle or LLIMGR_NULL_HANDLE if failed
 */
void *llimgr_create(struct device *dev, unsigned long mlli_table_size)
{
	struct llimgr_obj *new_llimgr_p;
	unsigned int num_of_full_page_llis;

	new_llimgr_p = kmalloc(sizeof(struct llimgr_obj), GFP_KERNEL);
	if (new_llimgr_p == NULL)
		return LLIMGR_NULL_HANDLE;
	new_llimgr_p->dev = dev;
	/* create dma "coherent" memory pool for MLLI tables */
	new_llimgr_p->mlli_cache = dma_pool_create("dx_sep_mlli_tables", dev,
						   mlli_table_size,
						   L1_CACHE_BYTES, 0);
	if (new_llimgr_p->mlli_cache == NULL) {
		pr_err("Failed creating DMA pool for MLLI tables\n");
		goto create_failed_mlli_pool;
	}

	/* Create pool for holding buffer "tails" which share cache lines with
	 * other data buffers */
	new_llimgr_p->edge_bufs_pool = dma_pool_create("dx_sep_edge_bufs", dev,
						       EDGE_BUFS_POOL_ITEM_SIZE,
						       EDGE_BUFS_POOL_ITEM_SIZE,
						       0);
	if (new_llimgr_p->edge_bufs_pool == NULL) {
		pr_err("Failed creating DMA pool for edge buffers\n");
		goto create_failed_edge_bufs_pool;
	}

	new_llimgr_p->max_lli_num =
	    ((mlli_table_size / SEP_LLI_ENTRY_BYTE_SIZE) -
	     SEP_MLLI_LINK_TO_NEXT_OVERHEAD);
	num_of_full_page_llis = new_llimgr_p->max_lli_num;
	num_of_full_page_llis -= 2;/*First and last entries are partial pages */
	num_of_full_page_llis -= 1;	/* One less for end aux. buffer */
#ifdef CONFIG_NOT_COHERENT_CACHE
	num_of_full_page_llis -= 1;	/* One less for start aux. buffer */
#endif
	/* Always a multiple of PAGE_SIZE - assures that it is also a
	 * crypto block multiple. */
	new_llimgr_p->max_data_per_mlli = num_of_full_page_llis * PAGE_SIZE;

#if DLLI_AUX_BUF_LIMIT > 0
	new_llimgr_p->dlli_bufs_pool = dma_pool_create("dx_sep_dlli_bufs", dev,
						       DLLI_AUX_BUF_LIMIT,
						       DLLI_AUX_BUF_LIMIT, 0);
	if (new_llimgr_p->dlli_bufs_pool == NULL) {
		pr_err("Failed creating DMA pool for DLLI buffers\n");
		goto create_failed_dlli_bufs_pool;
	}
#endif

	return new_llimgr_p;

 create_failed_dlli_bufs_pool:
	dma_pool_destroy(new_llimgr_p->edge_bufs_pool);
 create_failed_edge_bufs_pool:
	dma_pool_destroy(new_llimgr_p->mlli_cache);
 create_failed_mlli_pool:
	kfree(new_llimgr_p);
	return LLIMGR_NULL_HANDLE;
}

/**
 * llimgr_destroy() - Destroy (free resources of) given LLI-manager object
 * @llimgr:	 LLI-manager object handle
 *
 */
void llimgr_destroy(void *llimgr)
{
	struct llimgr_obj *llimgr_p = (struct llimgr_obj *)llimgr;

#if DLLI_AUX_BUF_LIMIT > 0
	dma_pool_destroy(llimgr_p->dlli_bufs_pool);
#endif
	dma_pool_destroy(llimgr_p->edge_bufs_pool);
	dma_pool_destroy(llimgr_p->mlli_cache);
	kfree(llimgr_p);
}

/*****************************************/
/* Auxilliary buffers handling functions */
/*****************************************/

/**
 * calc_aux_bufs_size() - Calculate required aux. buffers for given user buffer
 * @buf_start:	A pointer value at buffer start (used to calculate alignment)
 * @buf_size:	User buffer size in bytes
 * @data_direction:	DMA direction
 * @last_blk_with_prelast:	Last crypto block must be in the same LLI and
 *				pre-last block.
 * @crypto_block_size:	The Crypto-block size in bytes
 * @start_aux_buf_size_p:	Returned required aux. buffer size at start
 * @end_aux_buf_size_p:	Returned required aux. buffers size at end
 *
 * Returns void
 */
static void calc_aux_bufs_size(const unsigned long buf_start,
			       unsigned long buf_size,
			       enum dma_data_direction data_direction,
			       unsigned long *start_aux_buf_size_p,
			       unsigned long *end_aux_buf_size_p)
{
#ifdef CONFIG_NOT_COHERENT_CACHE
	const bool is_dout = ((data_direction == DMA_BIDIRECTIONAL) ||
			      (data_direction == DMA_FROM_DEVICE));
#endif				/*CONFIG_NOT_COHERENT_CACHE */

	/* Calculate required aux. buffers: cache line tails + last w/prelast */
	*start_aux_buf_size_p = 0;
	*end_aux_buf_size_p = 0;

	if (buf_size == 0)
		return;

#ifdef CONFIG_NOT_COHERENT_CACHE
	/* start of buffer unaligned to cache line... */
	if ((is_dout) && (buf_start & CACHE_LINE_MASK)) {
		*start_aux_buf_size_p =	/* Remainder to end of cache line */
		    L1_CACHE_BYTES - (buf_start & CACHE_LINE_MASK);
		/* But not more than buffer size */
		if (*start_aux_buf_size_p > buf_size)
			*start_aux_buf_size_p = buf_size;
	}
#endif				/*CONFIG_NOT_COHERENT_CACHE */

	/* last 32 B must always be on last LLI entry */
	/* Put 32 B or the whole buffer if smaller than 32 B */
	*end_aux_buf_size_p = buf_size >= DIN_LAST_LLI_GATHER_SIZE ?
	    DIN_LAST_LLI_GATHER_SIZE : buf_size;
	if ((*end_aux_buf_size_p + *start_aux_buf_size_p) > buf_size) {
		/* End aux. buffer covers part of
		 * start aux. buffer - leave only remainder in
		 * start aux. buffer. */
		*start_aux_buf_size_p = buf_size - *end_aux_buf_size_p;
	}
#ifdef DEBUG
	if (((*end_aux_buf_size_p + *start_aux_buf_size_p) > buf_size) ||
	    (*start_aux_buf_size_p > EDGE_BUFS_POOL_ITEM_SIZE) ||
	    (*end_aux_buf_size_p > EDGE_BUFS_POOL_ITEM_SIZE)) {
		pr_err(
			    "Invalid aux. buffer sizes: buf_size=%lu B, start_aux=%lu B, end_aux=%lu B\n",
			    buf_size, *start_aux_buf_size_p,
			    *end_aux_buf_size_p);
	} else {
		pr_debug
		    ("buf_size=%lu B, start_aux=%lu B, end_aux=%lu B\n",
		     buf_size, *start_aux_buf_size_p, *end_aux_buf_size_p);
	}
#endif
}

#ifdef DEBUG
static void dump_client_buf_pages(const struct client_dma_buffer
				  *client_dma_buf_p)
{
	int i;
	struct scatterlist *sgentry;

	if (client_dma_buf_p->user_buf_ptr != NULL) {
		pr_debug(
			      "Client DMA buffer %p maps %lu B over %d pages at user_ptr=0x%p (dma_dir=%d):\n",
			      client_dma_buf_p, client_dma_buf_p->buf_size,
			      client_dma_buf_p->num_of_pages,
			      client_dma_buf_p->user_buf_ptr,
			      client_dma_buf_p->dma_direction);
	} else {
		pr_debug("Client DMA buffer %p maps %lu B (dma_dir=%d):\n",
			      client_dma_buf_p, client_dma_buf_p->buf_size,
			      client_dma_buf_p->dma_direction);
	}

	if (client_dma_buf_p->user_pages != NULL) {
		pr_debug("%d user_pages:\n",
			      client_dma_buf_p->num_of_pages);
		for (i = 0; i < client_dma_buf_p->num_of_pages; i++) {
			pr_debug("%d. phys_addr=0x%08lX\n", i,
				      page_to_pfn(client_dma_buf_p->
						  user_pages[i]) << PAGE_SHIFT);
		}
	}
#if 0
	pr_debug("sg_head:\n");
	i = 0;
	for_each_valid_sg(client_dma_buf_p->sg_head, sgentry) {
		pr_debug("%d. phys_addr=0x%08llX len=0x%08X\n", i,
			      sg_phys(sgentry), sgentry->length);
		i++;
	}
#endif
	pr_debug("sg_main:\n");
	i = 0;
	for_each_valid_sg(client_dma_buf_p->sg_main, sgentry) {
		pr_debug("%d. dma_addr=0x%08llX len=0x%08X\n", i,
			(long long unsigned int)sg_dma_address(sgentry),
			sg_dma_len(sgentry));
		i++;
	}
	pr_debug("sg_tail:\n");
	i = 0;
	for_each_valid_sg(client_dma_buf_p->sg_tail, sgentry) {
		pr_debug("%d. phys_addr=0x%08llX len=0x%08X\n", i,
				(long long unsigned int)sg_phys(sgentry),
				sgentry->length);
		i++;
	}
	pr_debug("sg_save4next:\n");
	i = 0;
	for_each_valid_sg(client_dma_buf_p->sg_save4next, sgentry) {
		pr_debug("%d. phys_addr=0x%08llX len=0x%08X\n", i,
				(long long unsigned int)sg_phys(sgentry),
				sgentry->length);
		i++;
	}

}
#endif /*DEBUG*/
/**
 * create_sg_list() - Allocate/create S/G list for given page array,
 * @page_array:	 The source pages array
 * @offset_in_first_page:	 Offset in bytes in the first page of page_array
 * @sg_data_size:	 Number of bytes to include in the create S/G list
 * @new_sg_list_p:	 The allocated S/G list buffer
 * @next_page_p:	 The next page to map (for incremental list creation)
 * @next_page_offset_p:	 The offset to start in next page to map
 *	(The list is allocated by this func. and should be freed by the caller)
 *
 * Allocate/create S/G list for given page array,
 * starting at given offset in the first page spanning across given sg_data_size
 * Returns int 0 for success
 */
static int create_sg_list(struct page **pages_array,
			  unsigned long offset_in_first_page,
			  unsigned long sg_data_size,
			  struct scatterlist **new_sg_list_p,
			  struct page ***next_page_p,
			  unsigned long *next_page_offset_p)
{
	const unsigned long end_offset =
	    offset_in_first_page + sg_data_size - 1;
	const unsigned long num_of_sg_ents = (end_offset >> PAGE_SHIFT) + 1;
	const unsigned long size_of_first_page = (num_of_sg_ents == 1) ?
	    sg_data_size : (PAGE_SIZE - offset_in_first_page);
	const unsigned long size_of_last_page = (end_offset & ~PAGE_MASK) + 1;
	struct scatterlist *cur_sge;
	int i;

	if (sg_data_size == 0) {	/* Empty S/G list */
		*new_sg_list_p = NULL;
		*next_page_p = pages_array;
		*next_page_offset_p = offset_in_first_page;
		return 0;
	}

	*new_sg_list_p =
	    kmalloc(sizeof(struct scatterlist) * num_of_sg_ents, GFP_KERNEL);
	if (unlikely(*new_sg_list_p == NULL)) {
		pr_err("Failed allocating sglist array for %lu entries\n",
			    num_of_sg_ents);
		return -ENOMEM;
	}

	/* Set default for next table assuming full pages */
	*next_page_p = pages_array + num_of_sg_ents;
	*next_page_offset_p = 0;

	sg_init_table(*new_sg_list_p, num_of_sg_ents);
	cur_sge = *new_sg_list_p;
	/* First page is partial
	 * - May start in middle of page
	 * - May end in middle of page if single page */
	sg_set_page(cur_sge, pages_array[0],
		    size_of_first_page, offset_in_first_page);
	/* Handle following (whole) pages, but last (which may be partial) */
	for (i = 1; i < (num_of_sg_ents - 1); i++) {
		cur_sge = sg_next(cur_sge);
		if (unlikely(cur_sge == NULL)) {
			pr_err(
				    "Reached end of sgl before (%d) num_of_sg_ents (%lu)\n",
				    i, num_of_sg_ents);
			kfree(*new_sg_list_p);
			*new_sg_list_p = NULL;
			return -EINVAL;
		}
		sg_set_page(cur_sge, pages_array[i], PAGE_SIZE, 0);
	}
	/* Handle last (partial?) page */
	if (num_of_sg_ents > 1) {
		/* only if was not handled already as first */
		cur_sge = sg_next(cur_sge);
		if (unlikely(cur_sge == NULL)) {
			pr_err(
				    "Cannot put last page in given num_of_sg_ents (%lu)\n",
				    num_of_sg_ents);
			kfree(*new_sg_list_p);
			*new_sg_list_p = NULL;
			return -EINVAL;
		}
		sg_set_page(cur_sge,
			    pages_array[num_of_sg_ents - 1],
			    size_of_last_page, 0);
		if (size_of_last_page < PAGE_SIZE) {
			(*next_page_p)--; /* Last page was not fully consumed */
			*next_page_offset_p = size_of_last_page;
		}
	} else {		/* First was last */
		if ((offset_in_first_page + size_of_first_page) < PAGE_SIZE) {
			(*next_page_p)--; /* Page was not fully consumed */
			*next_page_offset_p =
			    (offset_in_first_page + size_of_first_page);
		}
	}

	return 0;
}

/**
 * split_sg_list() - Split SG list at given offset.
 * @sgl_to_split:	The SG list to split
 * @split_sg_list:	Returned new SG list which starts with second half
 *			of split entry.
 * @split_offset:	Size in bytes of entry part to leave on original list.
 *
 * Split SG list at given offset.
 * The entry is shortened at given length and the remainder is added to
 * a new SG entry that is chained to the original list following.
 * Returns int 0 on success
 */
static int split_sg_list(struct scatterlist *sgl_to_split,
			 struct scatterlist **split_sg_list,
			 unsigned long split_offset)
{
	struct scatterlist *cur_sge = sgl_to_split;

	/* Scan list until consuming enough for first part to fit in cur_sge */
	while ((cur_sge != NULL) && (split_offset > cur_sge->length)) {
		split_offset -= cur_sge->length;
		cur_sge = sg_next(cur_sge);
	}
	/* After the loop above, split_offset is actually the offset within
	 * cur_sge */

	if (cur_sge == NULL)
		return -ENOMEM;	/* SG list too short for given first_part_len */

	if (split_offset < cur_sge->length) {
		/* Split entry */
		*split_sg_list =
		    kmalloc(sizeof(struct scatterlist) * 2, GFP_KERNEL);
		if (*split_sg_list == NULL) {
			pr_err("Failed allocating SGE for split entry\n");
			return -ENOMEM;
		}
		sg_init_table(*split_sg_list, 2);
		sg_set_page(*split_sg_list, sg_page(cur_sge),
			    cur_sge->length - split_offset,
			    cur_sge->offset + split_offset);
		/* Link to second SGE */
		sg_chain(*split_sg_list, 2, sg_next(cur_sge));
		cur_sge->length = split_offset;
	} else {		/* Split at entry boundary */
		*split_sg_list = sg_next(cur_sge);
		sg_mark_end(cur_sge);
	}

	return 0;
}

/**
 * link_sg_lists() - Link back split S/G list
 * @first_sgl:	The first chunk s/g list
 * @second_sgl:	The second chunk s/g list
 *
 * Returns Unified lists list head
 */
static struct scatterlist *link_sg_lists(struct scatterlist *first_sgl,
					 struct scatterlist *second_sgl)
{
	struct scatterlist *second_sgl_second = NULL;
	struct scatterlist *first_sgl_last;
	struct scatterlist *cur_sge;

	if (first_sgl == NULL)
		return second_sgl;	/* Second list is the "unified" list */
	if (second_sgl == NULL)
		return first_sgl;	/* Nothing to link back */
	/* Seek end of first s/g list */
	first_sgl_last = NULL;	/* To save last s/g entry */
	for_each_valid_sg(first_sgl, cur_sge)
	    first_sgl_last = cur_sge;
	if ((sg_page(first_sgl_last) == sg_page(second_sgl)) &&
	    ((first_sgl_last->offset + first_sgl_last->length) ==
	     second_sgl->offset)) {
		/* Case of entry split */
		/* Restore first entry length */
		first_sgl_last->length += second_sgl->length;
		/* Save before freeing */
		second_sgl_second = sg_next(second_sgl);
		kfree(second_sgl);
	}
	/* This entry was allocated by split_sg_list */
	/* else, list was split on entry boundary */
	if (second_sgl_second != NULL) {
		/*
		 * Restore link to following entries
		 * Clear chain termination flag to link back to next sge
		 * Unfortunately there is no direct function to do this
		 * so we rely on implementation detail (all flags cleared)
		 */
		first_sgl_last->page_link =
		    (unsigned long)sg_page(first_sgl_last);
	}
	return first_sgl;
}

/**
 * cleanup_client_dma_buf() - Cleanup client_dma_buf resources (S/G lists,
 *				pages array, aux. bufs)
 * @client_dma_buf_p:

 *
 * Returns void
 */
static void cleanup_client_dma_buf(struct llimgr_obj *llimgr_p,
				   struct client_dma_buffer *client_dma_buf_p)
{
	struct page *cur_page;
	int i;
	const bool is_outbuf =
	    (client_dma_buf_p->dma_direction == DMA_FROM_DEVICE) ||
	    (client_dma_buf_p->dma_direction == DMA_BIDIRECTIONAL);

	/* User space buffer */
	if (client_dma_buf_p->user_buf_ptr != NULL) {
#ifdef CONFIG_NOT_COHERENT_CACHE
		if (client_dma_buf_p->sg_head != NULL)
			kfree(client_dma_buf_p->sg_head);
#endif
		if (client_dma_buf_p->sg_main != NULL)
			kfree(client_dma_buf_p->sg_main);
		if (client_dma_buf_p->sg_tail != NULL)
			kfree(client_dma_buf_p->sg_tail);
		if (client_dma_buf_p->sg_save4next != NULL)
			kfree(client_dma_buf_p->sg_save4next);
		/* Unmap pages that were mapped/locked */
		if (client_dma_buf_p->user_pages != NULL) {
			for (i = 0; i < client_dma_buf_p->num_of_pages; i++) {
				cur_page = client_dma_buf_p->user_pages[i];
				/* Mark dirty for pages written by HW/DMA */
				if (is_outbuf && !PageReserved(cur_page))
					SetPageDirty(cur_page);
				page_cache_release(cur_page);	/* Unlock */
			}
			kfree(client_dma_buf_p->user_pages);
		}

	} else {
		/* (kernel) given s/g list */
		/* Fix S/G list back to what was given */
#ifdef CONFIG_NOT_COHERENT_CACHE
		if (client_dma_buf_p->sg_head != NULL) {
			if (client_dma_buf_p->sg_main != NULL) {
				client_dma_buf_p->sg_main =
				    link_sg_lists(client_dma_buf_p->sg_head,
						  client_dma_buf_p->sg_main);
			} else {
				client_dma_buf_p->sg_tail =
				    link_sg_lists(client_dma_buf_p->sg_head,
						  client_dma_buf_p->sg_tail);
			}
			/* Linked to next */
			client_dma_buf_p->sg_head = NULL;
		}
#endif
		client_dma_buf_p->sg_tail =
		    link_sg_lists(client_dma_buf_p->sg_main,
				  client_dma_buf_p->sg_tail);
		client_dma_buf_p->sg_save4next =
		    link_sg_lists(client_dma_buf_p->sg_tail,
				  client_dma_buf_p->sg_save4next);
	}

	/* Free aux. buffers */
	if (client_dma_buf_p->buf_end_aux_buf_va != NULL) {
		if (client_dma_buf_p->buf_end_aux_buf_size <=
		    EDGE_BUFS_POOL_ITEM_SIZE) {
			dma_pool_free(llimgr_p->edge_bufs_pool,
				      client_dma_buf_p->buf_end_aux_buf_va,
				      client_dma_buf_p->buf_end_aux_buf_dma);
		} else {	/* From DLLI buffers pool */
			dma_pool_free(llimgr_p->dlli_bufs_pool,
				      client_dma_buf_p->buf_end_aux_buf_va,
				      client_dma_buf_p->buf_end_aux_buf_dma);
		}
	}
#ifdef CONFIG_NOT_COHERENT_CACHE
	if (client_dma_buf_p->buf_start_aux_buf_va != NULL)
		dma_pool_free(llimgr_p->edge_bufs_pool,
			      client_dma_buf_p->buf_start_aux_buf_va,
			      client_dma_buf_p->buf_start_aux_buf_dma);
#endif
	CLEAN_DMA_BUFFER_INFO(client_dma_buf_p);
}

/**
 * is_sgl_phys_contig() - Check if given scatterlist is physically contig.
 *
 * @sgl:		Checked scatter/gather list
 * @data_size_p:	Size of phys. contig. portion of given sgl
 */
static bool is_sgl_phys_contig(struct scatterlist *sgl,
			       unsigned long *data_size_p)
{
	struct scatterlist *cur_sge = sgl;
	struct scatterlist *next_sge;

	*data_size_p = 0;
	for (cur_sge = sgl; cur_sge != NULL; cur_sge = next_sge) {
		(*data_size_p) += cur_sge->length;
		next_sge = sg_next(cur_sge);
		if ((next_sge != NULL) &&
		    /* Check proximity of current entry to next entry */
		    ((page_to_phys(sg_page(cur_sge)) + cur_sge->length) !=
		     (page_to_phys(sg_page(next_sge)) + next_sge->offset))) {
			/* End of cur_sge does not reach start of next_sge */
			return false;
		}
	}
	/* If we passed the loop then data is phys. contig. */
	return true;
}

/**
 * is_pages_phys_contig() - Check if given pages are phys. contig.
 *
 * @pages_list:		Array of pages
 * @num_of_pages:	Number of pages in provided pages_list
 */
static bool is_pages_phys_contig(struct page *pages_list[],
				 unsigned int num_of_pages)
{
	int i;

	for (i = 0; i < (num_of_pages - 1); i++) {
		if ((page_to_phys(pages_list[i]) + PAGE_SIZE) !=
		    page_to_phys(pages_list[i + 1]))
			return false;
	}
	/* If reached here then all pages are following each other */
	return true;
}

/**
 * user_buf_to_client_dma_buf() - Apply given user_buf_ptr (user space buffer)
 *				into client_dma_buffer
 * @llimgr_p:
 * @client_dma_buf_p:	 Client DMA object
 *
 * Apply given user_buf_ptr (user space buffer) into client_dma_buffer
 * This function should be invoked after caller has set in the given
 * client_dma_buf_p object the user_buf_ptr, buf_size and dma_direction
 * Returns int 0 for success
 */
static int user_buf_to_client_dma_buf(struct llimgr_obj *llimgr_p,
				      struct client_dma_buffer
				      *client_dma_buf_p)
{
	u8 __user const *user_buf_ptr = client_dma_buf_p->user_buf_ptr;
	unsigned long buf_size = client_dma_buf_p->buf_size;
	const enum dma_data_direction dma_direction =
	    client_dma_buf_p->dma_direction;
	unsigned long buf_end = (unsigned long)user_buf_ptr + buf_size - 1;
	const int num_of_pages =
	    (buf_end >> PAGE_SHIFT) -
	    ((unsigned long)user_buf_ptr >> PAGE_SHIFT) + 1;
	const unsigned long offset_in_first_page =
	    (unsigned long)user_buf_ptr & ~PAGE_MASK;
	const bool is_inbuf = (dma_direction == DMA_TO_DEVICE) ||
	    (dma_direction == DMA_BIDIRECTIONAL);
	const bool is_outbuf = (dma_direction == DMA_FROM_DEVICE) ||
	    (dma_direction == DMA_BIDIRECTIONAL);
	unsigned long head_buf_size = 0, tail_buf_size = 0, main_buf_size = 0;
	struct page **cur_page_p;
	unsigned long cur_page_offset;
	int rc = 0;

	/* Verify permissions */
	if (is_inbuf && !access_ok(ACCESS_READ, user_buf_ptr, buf_size)) {
		pr_err("No read access to data buffer at %p\n",
			    user_buf_ptr);
		return -EFAULT;
	}
	if (is_outbuf && !access_ok(ACCESS_WRITE, user_buf_ptr, buf_size)) {
		pr_err("No write access to data buffer at %p\n",
			    user_buf_ptr);
		return -EFAULT;
	}
	client_dma_buf_p->user_pages =
	    kmalloc(sizeof(struct page *)*num_of_pages, GFP_KERNEL);
	if (unlikely(client_dma_buf_p->user_pages == NULL)) {
		pr_err("Failed allocating user_pages array for %d pages\n",
			    num_of_pages);
		return -ENOMEM;
	}
	/* Get user pages structure (also increment ref. count... lock) */
	client_dma_buf_p->num_of_pages = get_user_pages_fast((unsigned long)
							     user_buf_ptr,
							     num_of_pages,
							     is_outbuf,
							     client_dma_buf_p->
							     user_pages);
	if (client_dma_buf_p->num_of_pages != num_of_pages) {
		pr_warn(
			     "Failed to lock all user pages (locked %d, requested lock = %d)\n",
			     client_dma_buf_p->num_of_pages, num_of_pages);
		rc = -ENOMEM;
	}
	/* Leave only currently processed data (remainder in sg_save4next */
	buf_size -= client_dma_buf_p->save4next_size;
	buf_end -= client_dma_buf_p->save4next_size;
	/* Decide on type of mapping: MLLI, DLLI or DLLI after copy */
	if (buf_size <= DLLI_BUF_LIMIT) {
		/* Check if possible to map buffer directly as DLLI */
		if (
#ifdef CONFIG_NOT_COHERENT_CACHE
			   /* For systems with incoherent cache the buffer
			    * must be cache line aligned to be cosidered
			    * for this case
			    */
			   (((unsigned long)user_buf_ptr & CACHE_LINE_MASK) ==
			    0) &&
				((buf_end & CACHE_LINE_MASK) ==
				 CACHE_LINE_MASK) &&
#endif
			   is_pages_phys_contig(client_dma_buf_p->user_pages,
						client_dma_buf_p->
						num_of_pages)) {
			pr_debug(
				      "Mapping user buffer @%p (0x%08lX B) to DLLI directly\n",
				      client_dma_buf_p->user_buf_ptr, buf_size);
			main_buf_size = buf_size;/* Leave 0 for tail_buf_size */
			/* 0 for the tail buffer indicates that we use this
			 * optimization, because in any other case there must
			 * be some data in the tail buffer (if buf_size>0) */
		} else if (buf_size <= DLLI_AUX_BUF_LIMIT) {
			pr_debug(
				      "Mapping user buffer @%p (0x%08lX B) to DLLI via aux. buffer\n",
				      client_dma_buf_p->user_buf_ptr, buf_size);
			tail_buf_size = buf_size;
			/* All data goes to "tail" in order to piggy-back over
			 * the aux. buffers logic for copying data in/out of the
			 * temp. DLLI DMA buffer */
		}
	}
	if ((main_buf_size + tail_buf_size) == 0) {
		/* If none of the optimizations was applied... */
		calc_aux_bufs_size((unsigned long)user_buf_ptr, buf_size,
				   dma_direction, &head_buf_size,
				   &tail_buf_size);
		main_buf_size = buf_size - head_buf_size - tail_buf_size;
	}

	/* Create S/G list */
	cur_page_p = client_dma_buf_p->user_pages;
	cur_page_offset = offset_in_first_page;
#ifdef CONFIG_NOT_COHERENT_CACHE
	if (likely(rc == 0)) {
		/* Create S/G list for head (aux.) buffer */
		rc = create_sg_list(cur_page_p, cur_page_offset,
				    head_buf_size, &client_dma_buf_p->sg_head,
				    &cur_page_p, &cur_page_offset);
	}
#endif
	/* Create S/G list for buffer "body" - to be used for DMA */
	if (likely(rc == 0)) {
		rc = create_sg_list(cur_page_p, cur_page_offset,
				    main_buf_size, &client_dma_buf_p->sg_main,
				    &cur_page_p, &cur_page_offset);
	}
	/* Create S/G list for tail (aux.) buffer */
	if (likely(rc == 0)) {
		rc = create_sg_list(cur_page_p, cur_page_offset,
				    tail_buf_size, &client_dma_buf_p->sg_tail,
				    &cur_page_p, &cur_page_offset);
	}
	/* Create S/G list for save4next buffer */
	if (likely(rc == 0)) {
		rc = create_sg_list(cur_page_p, cur_page_offset,
				    client_dma_buf_p->save4next_size,
				    &client_dma_buf_p->sg_save4next,
				    &cur_page_p, &cur_page_offset);
	}

	if (unlikely(rc != 0)) {
		cleanup_client_dma_buf(llimgr_p, client_dma_buf_p);
	} else {
		/* Save head/tail sizes */
#ifdef CONFIG_NOT_COHERENT_CACHE
		client_dma_buf_p->buf_start_aux_buf_size = head_buf_size;
#endif
		client_dma_buf_p->buf_end_aux_buf_size = tail_buf_size;
	}

	return rc;
}

/**
 * client_sgl_to_client_dma_buf() - Create head/main/tail sg lists from given
 *					sg list
 * @llimgr_p:
 * @sgl:
 * @client_dma_buf_p:
 *
 * Returns int
 */
static int client_sgl_to_client_dma_buf(struct llimgr_obj *llimgr_p,
					struct scatterlist *sgl,
					struct client_dma_buffer
					*client_dma_buf_p)
{
	const unsigned long buf_size = client_dma_buf_p->buf_size -
	    client_dma_buf_p->save4next_size;
	const enum dma_data_direction dma_direction =
	    client_dma_buf_p->dma_direction;
	unsigned long sgl_phys_contig_size;	/* Phys. contig. part size */
	unsigned long head_buf_size = 0, tail_buf_size = 0;
	unsigned long main_buf_size = 0;
	unsigned long last_sgl_size = 0;
	struct scatterlist *last_sgl = NULL;	/* sgl to split of save4next */
	int rc;

	pr_debug("sgl=%p nbytes=%lu save4next=%lu client_dma_buf=%p\n",
		      sgl, client_dma_buf_p->buf_size,
		      client_dma_buf_p->save4next_size, client_dma_buf_p);

	if (buf_size == 0) {	/* all goes to save4next (if anything) */
		client_dma_buf_p->sg_save4next = sgl;
		return 0;
	}

	/* Decide on type of mapping: MLLI, DLLI or DLLI after copy */
	if (buf_size <= DLLI_BUF_LIMIT) {
		/* Check if possible to map buffer directly as DLLI */
		if (
#ifdef CONFIG_NOT_COHERENT_CACHE
			   /*
			    * For systems with incoherent cache the
			    * buffer must be cache line aligned to be
			    * cosidered for this case
			    */
			   ((sgl->offset & CACHE_LINE_MASK) == 0) &&
			   (((sgl->offset + buf_size) &
			     CACHE_LINE_MASK) == 0) &&
#endif
			   is_sgl_phys_contig(sgl, &sgl_phys_contig_size)) {
			pr_debug(
				      "Mapping sgl buffer (0x%08lX B) to DLLI directly\n",
				      buf_size);
			main_buf_size = buf_size;
			/* Leave 0 for tail_buf_size
			 * 0 for the tail buffer indicates that we use this
			 * optimization, because in any other case there must
			 * be some data in the tail buffer (if buf_size>0)
			 */
		} else if (buf_size <= DLLI_AUX_BUF_LIMIT) {
			pr_debug(
				      "Mapping sgl buffer (0x%08lX B) to DLLI via aux. buffer\n",
				      buf_size);
			tail_buf_size = buf_size;
			/* All data goes to "tail" in order to piggy-back
			 * over the aux. buffers logic for copying data
			 * in/out of the temp. DLLI DMA buffer
			 */
		}
	}
	if ((main_buf_size + tail_buf_size) == 0) {
		/* If none of the optimizations was applied... */
		/* Use first SG entry for start alignment */
		calc_aux_bufs_size((unsigned long)sgl->offset, buf_size,
				   dma_direction, &head_buf_size,
				   &tail_buf_size);
		main_buf_size = buf_size - head_buf_size - tail_buf_size;
	}

#ifdef CONFIG_NOT_COHERENT_CACHE
	if (head_buf_size > 0) {
		client_dma_buf_p->sg_head = sgl;
		rc = split_sg_list(client_dma_buf_p->sg_head,
				   &client_dma_buf_p->sg_main, head_buf_size);
		if (unlikely(rc != 0)) {
			pr_err("Failed splitting sg_head-sg_main\n");
			cleanup_client_dma_buf(llimgr_p, client_dma_buf_p);
			return rc;
		}
		last_sgl_size = head_buf_size;
		last_sgl = client_dma_buf_p->sg_head;
	} else
#endif
		/* Initilize sg_main to given sgl */
		client_dma_buf_p->sg_main = sgl;

	if (tail_buf_size > 0) {
		if (main_buf_size > 0) {
			rc = split_sg_list(client_dma_buf_p->sg_main,
					   &client_dma_buf_p->sg_tail,
					   main_buf_size);
			if (unlikely(rc != 0)) {
				pr_err("Fail:splitting sg_main-sg_tail\n");
				cleanup_client_dma_buf(llimgr_p,
						       client_dma_buf_p);
				return rc;
			}
		} else {	/* All data moved to sg_tail */
			client_dma_buf_p->sg_tail = client_dma_buf_p->sg_main;
			client_dma_buf_p->sg_main = NULL;
		}
		last_sgl_size = tail_buf_size;
		last_sgl = client_dma_buf_p->sg_tail;
	} else if (main_buf_size > 0) {	/* main only */
		last_sgl_size = main_buf_size;
		last_sgl = client_dma_buf_p->sg_main;
	}

	/* Save head/tail sizes */
#ifdef CONFIG_NOT_COHERENT_CACHE
	client_dma_buf_p->buf_start_aux_buf_size = head_buf_size;
#endif
	client_dma_buf_p->buf_end_aux_buf_size = tail_buf_size;

	if (client_dma_buf_p->save4next_size > 0) {
		if (last_sgl != NULL) {
			rc = split_sg_list(last_sgl,
					   &client_dma_buf_p->sg_save4next,
					   last_sgl_size);
			if (unlikely(rc != 0)) {
				pr_err("Failed splitting sg_save4next\n");
				cleanup_client_dma_buf(llimgr_p,
						       client_dma_buf_p);
				return rc;
			}
		} else {	/* Whole buffer goes to save4next */
			client_dma_buf_p->sg_save4next = sgl;
		}
	}
	return 0;
}

/**
 * llimgr_register_client_dma_buf() - Register given client buffer for DMA
 *					operation.
 * @llimgr:	 The LLI manager object handle
 * @user_buf_ptr:	 Pointer in user space of the user buffer
 * @sgl:	Client provided s/g list. user_buf_ptr is assumed NULL if this
 *		list is given (!NULL).
 * @buf_size:	 The user buffer size in bytes (incl. save4next). May be 0.
 * @save4next_size:	Amount from buffer end to save for next op.
 *			(split into seperate sgl). May be 0.
 * @dma_direction:	The DMA direction this buffer would be used for
 * @client_dma_buf_p:	Pointer to the user DMA buffer "object"
 *
 * Register given client buffer for DMA operation.
 * If user_buf_ptr!=NULL and sgl==NULL it locks the user pages and creates
 * head/main/tail s/g lists. If sgl!=NULL is splits it into head/main/tail
 * s/g lists.
 * Returns 0 for success
 */
int llimgr_register_client_dma_buf(void *llimgr,
				   u8 __user *user_buf_ptr,
				   struct scatterlist *sgl,
				   const unsigned long buf_size,
				   const unsigned long save4next_size,
				   const enum dma_data_direction dma_direction,
				   struct client_dma_buffer *client_dma_buf_p)
{
	struct llimgr_obj *llimgr_p = (struct llimgr_obj *)llimgr;
	int rc, tmp;

	CLEAN_DMA_BUFFER_INFO(client_dma_buf_p);

	if (buf_size == 0) {	/* Handle empty buffer */
		pr_debug("buf_size == 0\n");
		return 0;
	}

	if ((user_buf_ptr == NULL) && (sgl == NULL)) {
		pr_err("NULL user_buf_ptr/sgl\n");
		return -EINVAL;
	}
	if ((user_buf_ptr != NULL) && (sgl != NULL)) {
		pr_err("Provided with dual buffer info (both user+sgl)\n");
		return -EINVAL;
	}

	/* Init. basic/common attributes */
	client_dma_buf_p->user_buf_ptr = user_buf_ptr;
	client_dma_buf_p->buf_size = buf_size;
	client_dma_buf_p->save4next_size = save4next_size;
	client_dma_buf_p->dma_direction = dma_direction;

	if (user_buf_ptr != NULL) {
		rc = user_buf_to_client_dma_buf(llimgr_p, client_dma_buf_p);
	} else {
		rc = client_sgl_to_client_dma_buf(llimgr_p, sgl,
						  client_dma_buf_p);
	}
	if (unlikely(rc != 0))
		return rc;
	/* Since sg_main may be large and we need its nents for each
	 * dma_map_sg/dma_unmap_sg operation, we count its nents once and
	 * save the result.
	 * (for the other sgl's in the object we can count when accessed) */
	client_dma_buf_p->sg_main_nents =
	    get_sgl_nents(client_dma_buf_p->sg_main);

	/* Allocate auxilliary buffers for sg_head/sg_tail copies */
#ifdef CONFIG_NOT_COHERENT_CACHE
	if ((likely(rc == 0)) &&
	    (client_dma_buf_p->buf_start_aux_buf_size > 0)) {
		client_dma_buf_p->buf_start_aux_buf_va =
		    dma_pool_alloc(llimgr_p->edge_bufs_pool, GFP_KERNEL,
				   &client_dma_buf_p->buf_start_aux_buf_dma);
		if (unlikely(client_dma_buf_p->buf_start_aux_buf_va == NULL)) {
			pr_err("Fail alloc from edge_bufs_pool, head\n");
			rc = -ENOMEM;
		} else {
			pr_debug("start_aux: va=%p dma=0x%08llX\n",
				      client_dma_buf_p->buf_start_aux_buf_va,
				      client_dma_buf_p->buf_start_aux_buf_dma);
		}
	}
#endif
	if ((likely(rc == 0)) && (client_dma_buf_p->buf_end_aux_buf_size > 0)) {
#ifdef DEBUG
		if (client_dma_buf_p->buf_end_aux_buf_size >
				DLLI_AUX_BUF_LIMIT) {
			pr_err("end_aux_buf size too large = 0x%08lX\n",
				    client_dma_buf_p->buf_end_aux_buf_size);
			return -EINVAL;
		}
#endif
		if (client_dma_buf_p->buf_end_aux_buf_size <=
		    EDGE_BUFS_POOL_ITEM_SIZE) {
			client_dma_buf_p->buf_end_aux_buf_va =
			    dma_pool_alloc(llimgr_p->edge_bufs_pool, GFP_KERNEL,
					   &client_dma_buf_p->
					   buf_end_aux_buf_dma);
		} else {
			/* Allocate from the dedicated DLLI buffers pool */
			client_dma_buf_p->buf_end_aux_buf_va =
			    dma_pool_alloc(llimgr_p->dlli_bufs_pool, GFP_KERNEL,
					   &client_dma_buf_p->
					   buf_end_aux_buf_dma);
		}
		if (unlikely(client_dma_buf_p->buf_end_aux_buf_va == NULL)) {
			pr_err("Fail:allocating from aux. buf for tail\n");
			rc = -ENOMEM;
		} else {
			pr_debug("end_aux: va=%p dma=0x%08llX\n",
				client_dma_buf_p->buf_end_aux_buf_va,
				(long long unsigned int)
				client_dma_buf_p->buf_end_aux_buf_dma);
		}
	}

	/* Map the main sglist (head+tail would not be used for DMA) */
	if (likely(rc == 0) && (client_dma_buf_p->sg_main != NULL)) {
		tmp = dma_map_sg(llimgr_p->dev, client_dma_buf_p->sg_main,
				 client_dma_buf_p->sg_main_nents,
				 dma_direction);
		if (unlikely(tmp == 0)) {
			pr_err("dma_map_sg failed\n");
			rc = -ENOMEM;
		}
	}

#ifdef DEBUG
	if (likely(rc == 0))
		dump_client_buf_pages(client_dma_buf_p);
#endif

	if (unlikely(rc != 0)) {	/* Error cases cleanup */
		cleanup_client_dma_buf(llimgr_p, client_dma_buf_p);
	}

	return rc;
}

/**
 * llimgr_deregister_client_dma_buf() - Unmap given user DMA buffer
 * @llimgr:

 * @client_dma_buf_p:	 User DMA buffer object
 *
 * Unmap given user DMA buffer (flush and unlock pages)
 * (this function can handle client_dma_buffer of size 0)
 */
void llimgr_deregister_client_dma_buf(void *llimgr,
				      struct client_dma_buffer
				      *client_dma_buf_p)
{
	struct llimgr_obj *llimgr_p = (struct llimgr_obj *)llimgr;

	/* Cleanup DMA mappings */
	if (client_dma_buf_p->sg_main != NULL) {
		dma_unmap_sg(llimgr_p->dev, client_dma_buf_p->sg_main,
			     client_dma_buf_p->sg_main_nents,
			     client_dma_buf_p->dma_direction);
	}
	cleanup_client_dma_buf(llimgr_p, client_dma_buf_p);
}

/**
 * get_sgl_nents() - Get (count) the number of entries in given s/g list
 *	(used in order to be able to invoke sg_copy_to/from_buffer)
 * @sgl:	Counted s/g list entries
 */
static inline unsigned int get_sgl_nents(struct scatterlist *sgl)
{
	int cnt = 0;
	struct scatterlist *cur_sge;
	for_each_valid_sg(sgl, cur_sge)
	    cnt++;
	return cnt;
}

/**
 * copy_to_from_aux_buf() - Copy to/from given aux.buffer from/to given s/g list
 * @to_buf:	 "TRUE" for copying to the given buffer from sgl
 * @sgl:	 The S/G list data source/target
 * @to_buf:	 Target/source buffer
 * @buf_len:	 Buffer length
 *
 * Returns int
 */
static inline int copy_to_from_aux_buf(bool to_buf,
				       struct scatterlist *sgl, void *buf_p,
				       size_t buf_len)
{
	size_t copied_cnt;
	unsigned int nents = get_sgl_nents(sgl);

	if (to_buf)
		copied_cnt = sg_copy_to_buffer(sgl, nents, buf_p, buf_len);
	else
		copied_cnt = sg_copy_from_buffer(sgl, nents, buf_p, buf_len);

	if (copied_cnt < buf_len) {
		pr_err("Failed copying %s buf of %zu B\n",
			    to_buf ? "to" : "from", buf_len);
		return -ENOMEM;
	}

	return 0;
}

/**
 * sync_client_dma_buf() - Sync. pages before DMA to device at given offset in
 *				the user buffer
 * @dev:	 Associated device structure
 * @client_dma_buf_p:	 The user DMA buffer object
 * @for_device:	 Set to "true" to sync before device DMA op.
 * @dma_direction:	 DMA direction for sync.
 *
 * Returns int 0 for success
 */
static int sync_client_dma_buf(struct device *dev,
			       struct client_dma_buffer *client_dma_buf_p,
			       const bool for_device,
			       const enum dma_data_direction dma_direction)
{
	const bool is_from_device = ((dma_direction == DMA_BIDIRECTIONAL) ||
				     (dma_direction == DMA_FROM_DEVICE));
	const bool is_to_device = ((dma_direction == DMA_BIDIRECTIONAL) ||
				   (dma_direction == DMA_TO_DEVICE));
	int rc;

	pr_debug("DMA buf %p (0x%08lX B) for %s\n",
		      client_dma_buf_p, client_dma_buf_p->buf_size,
		      for_device ? "device" : "cpu");

	if (for_device) {
		/* Copy out aux. buffers if required */
		/* We should copy before dma_sync_sg_for_device */
		if (is_to_device) {
#ifdef CONFIG_NOT_COHERENT_CACHE
			if (client_dma_buf_p->sg_head != NULL) {
				rc = copy_to_from_aux_buf(true,
						  client_dma_buf_p->sg_head,
						  client_dma_buf_p->
						  buf_start_aux_buf_va,
						  client_dma_buf_p->
						  buf_start_aux_buf_size);
				if (rc != 0)
					return rc;
			}
#endif
			if (client_dma_buf_p->sg_tail != NULL) {
				rc = copy_to_from_aux_buf(true,
						  client_dma_buf_p->sg_tail,
						  client_dma_buf_p->
						  buf_end_aux_buf_va,
						  client_dma_buf_p->
						  buf_end_aux_buf_size);
				if (rc != 0)
					return rc;
			}
		}
		if (client_dma_buf_p->sg_main != NULL) {
			dma_sync_sg_for_device(dev, client_dma_buf_p->sg_main,
					       client_dma_buf_p->sg_main_nents,
					       dma_direction);
		}

	} else {		/* for CPU */
		if (client_dma_buf_p->sg_main != NULL) {
			dma_sync_sg_for_cpu(dev, client_dma_buf_p->sg_main,
					    client_dma_buf_p->sg_main_nents,
					    dma_direction);
		}
		/* Copy back from aux. buffers */
		/* We should copy after dma_sync_sg_for_cpu */
		if (is_from_device) {
#ifdef CONFIG_NOT_COHERENT_CACHE
			if (client_dma_buf_p->sg_head != NULL) {
				rc = copy_to_from_aux_buf(false,
						  client_dma_buf_p->sg_head,
						  client_dma_buf_p->
						  buf_start_aux_buf_va,
						  client_dma_buf_p->
						  buf_start_aux_buf_size);
				if (rc != 0)
					return rc;
			}
#endif
			if (client_dma_buf_p->sg_tail != NULL) {
				rc = copy_to_from_aux_buf(false,
						  client_dma_buf_p->sg_tail,
						  client_dma_buf_p->
						  buf_end_aux_buf_va,
						  client_dma_buf_p->
						  buf_end_aux_buf_size);
				if (rc != 0)
					return rc;
			}
		}
	}

	return 0;
}

/**
 * llimgr_copy_from_client_buf_save4next() - Copy from the sg_save4next chunk
 *	of the client DMA buffer to given buffer.
 *	Used to save hash block remainder.
 *
 * @client_dma_buf_p:	The client DMA buffer with the save4next chunk
 * @to_buf:		Target buffer to copy to.
 * @buf_len:		Given buffer length (to avoid buffer overflow)
 *
 * Returns number of bytes copied or -ENOMEM if given buffer is too small
 */
int llimgr_copy_from_client_buf_save4next(struct client_dma_buffer
					  *client_dma_buf_p, u8 *to_buf,
					  unsigned long buf_len)
{
	int copied_cnt;
	struct scatterlist *sgl = client_dma_buf_p->sg_save4next;
	unsigned int nents;

	if (buf_len < client_dma_buf_p->save4next_size) {
		pr_err("Invoked for copying %lu B to a buffer of %lu B\n",
			    client_dma_buf_p->save4next_size, buf_len);
		copied_cnt = -ENOMEM;
	} else {
		nents = get_sgl_nents(sgl);
		if (nents > 0)
			copied_cnt = sg_copy_to_buffer(sgl, nents,
						       to_buf, buf_len);
		else		/* empty */
			copied_cnt = 0;
	}
	return copied_cnt;
}

#ifdef DEBUG
/**
 * dump_mlli_table() - Dump given MLLI table
 * @table_start_p:	 Pointer to allocated table buffer
 * @dma_addr:	 The table's DMA address as given to SeP
 * @table_size:	 The table size in bytes
 *
 */
static void dump_mlli_table(u32 *table_start_p, dma_addr_t dma_addr,
			    unsigned long table_size)
{
	u32 lli_spad[SEP_LLI_ENTRY_WORD_SIZE];
	int i;
	u32 *cur_entry_p;
	unsigned int num_of_entries = table_size / SEP_LLI_ENTRY_BYTE_SIZE;

	pr_debug("MLLI table at %p (dma_addr=0x%08X) with %u ent.:\n",
		      table_start_p, (unsigned int)dma_addr, num_of_entries);

	for (i = 0, cur_entry_p = table_start_p + SEP_LLI_ENTRY_WORD_SIZE;
	     i < num_of_entries; cur_entry_p += SEP_LLI_ENTRY_WORD_SIZE, i++) {
		/* LE to BE... */
		SEP_LLI_COPY_FROM_SEP(lli_spad, cur_entry_p);
		/*
		 * pr_debug("%02d: addr=0x%08lX , size=0x%08lX\n  %s\n", i,
		 * SEP_LLI_GET(lli_spad, ADDR),
		 * SEP_LLI_GET(lli_spad, SIZE),
		 */
		pr_debug("%02d: [0x%08X,0x%08X] %s\n", i,
			      lli_spad[0], lli_spad[1],
			      i == 0 ? "(next table)" : "");
	}
}

/**
 * llimgr_dump_mlli_tables_list() - Dump all the MLLI tables in a given tables
 *					list
 * @mlli_tables_list_p:	 Pointer to tables list structure
 *
 */
void llimgr_dump_mlli_tables_list(struct mlli_tables_list *mlli_tables_list_p)
{
	u32 lli_spad[SEP_LLI_ENTRY_WORD_SIZE];
	u32 *cur_table_p;
	u32 *next_table_p;
	u32 *link_entry_p;
	unsigned long next_table_size;
	dma_addr_t next_table_dma;
	u16 table_count = 0;

	/* This loop uses "cur_table_p" as the previous table that
	 * was already dumped */
	for (cur_table_p = mlli_tables_list_p->link_to_first_table;
	     cur_table_p != NULL; cur_table_p = next_table_p, table_count++) {

		if (table_count > mlli_tables_list_p->table_count) {
			pr_err(
				"MLLI tables list has more tables than table_cnt=%u. Stopping dump.\n",
				mlli_tables_list_p->table_count);
			break;
		}

		/* The SeP link entry is second in the buffer */
		link_entry_p = cur_table_p + SEP_LLI_ENTRY_WORD_SIZE;
		SEP_LLI_COPY_FROM_SEP(lli_spad, link_entry_p);/* LE to BE... */
		next_table_p = SEP_MLLI_GET_NEXT_VA(cur_table_p);
		next_table_dma = SEP_LLI_GET(lli_spad, ADDR);
		next_table_size = SEP_LLI_GET(lli_spad, SIZE);
		if (next_table_p != NULL)
			dump_mlli_table(next_table_p,
					next_table_dma, next_table_size);

	}

}
#endif /*DEBUG*/
/*****************************************/
/* MLLI tables construction functions    */
/*****************************************/
/**
 * set_dlli() - Set given mlli object to function as DLLI descriptor
 *
 * @mlli_tables_list_p:	The associated "MLLI" tables list
 * @dlli_addr:	The DMA address for the DLLI
 * @dlli_size:	The size in bytes of referenced data
 *
 * The MLLI tables list object represents DLLI when its table_count is 0
 * and the "link_to_first_table" actually points to the data.
 */
static inline void set_dlli(struct mlli_tables_list *mlli_tables_list_p,
			    u32 dlli_addr, u16 dlli_size)
{
	u32 lli_spad[SEP_LLI_ENTRY_WORD_SIZE];	/* LLI scratchpad */

	SEP_LLI_INIT(lli_spad);
	SEP_LLI_SET(lli_spad, ADDR, dlli_addr);
	SEP_LLI_SET(lli_spad, SIZE, dlli_size);
	SEP_LLI_COPY_TO_SEP(mlli_tables_list_p->link_to_first_table +
			    SEP_LLI_ENTRY_WORD_SIZE, lli_spad);
	mlli_tables_list_p->table_count = 0;
}

/**
 * set_last_lli() - Set "Last" bit on last LLI data entry
 * @last_lli_p:
 *
 */
static inline void set_last_lli(u32 *last_lli_p)
{
	u32 lli_spad[SEP_LLI_ENTRY_WORD_SIZE];	/* LLI scratchpad */

	SEP_LLI_COPY_FROM_SEP(lli_spad, last_lli_p);
	SEP_LLI_SET(lli_spad, LAST, 1);
	SEP_LLI_COPY_TO_SEP(last_lli_p, lli_spad);
}

/**
 * set_last_table() - Set table link to next as NULL (last table).
 * @mlli_table_p:
 *
 */
static inline void set_last_table(u32 *mlli_table_p)
{
	u32 lli_spad[SEP_LLI_ENTRY_WORD_SIZE];	/* LLI scratchpad */

	/* Set SeP link entry */
	SEP_LLI_INIT(lli_spad);
	SEP_LLI_SET(lli_spad, FIRST, 1);
	SEP_LLI_SET(lli_spad, LAST, 1);
	/* The rest of the field are zero from SEP_LLI_INIT */
	SEP_LLI_COPY_TO_SEP(mlli_table_p, lli_spad);
	/* Set NULL for next VA */
	SEP_MLLI_SET_NEXT_VA_NULL(mlli_table_p);
}

static inline void link_to_prev_mlli(struct mlli_tables_list_iterator
				     *mlli_iter_p)
{
	u32 lli_spad[SEP_LLI_ENTRY_WORD_SIZE];	/* LLI scratchpad */
	const u32 cur_mlli_table_size =
	    (mlli_iter_p->next_lli_idx + 1) * SEP_LLI_ENTRY_BYTE_SIZE;
	/* +1 for link entry at table start */

	SEP_LLI_INIT(lli_spad);
	SEP_LLI_SET(lli_spad, ADDR, mlli_iter_p->cur_mlli_dma_addr);
	SEP_LLI_SET(lli_spad, SIZE, cur_mlli_table_size);
	SEP_LLI_SET(lli_spad, FIRST, 1);
	SEP_LLI_SET(lli_spad, LAST, 1);
	SEP_LLI_COPY_TO_SEP(mlli_iter_p->prev_mlli_table_p +
			    SEP_LLI_ENTRY_WORD_SIZE, lli_spad);
	SEP_MLLI_SET_NEXT_VA(mlli_iter_p->prev_mlli_table_p,
			     mlli_iter_p->cur_mlli_table_p);
}

/**
 * terminate_mlli_tables_list() - "NULL" terminate the MLLI tables list and link
 *				to previous table if any.
 * @mlli_iter_p:	 MLLI tables list iterator
 *
 */
static inline void terminate_mlli_tables_list(struct mlli_tables_list_iterator
					      *mlli_iter_p)
{
	u32 *last_lli_p = mlli_iter_p->cur_mlli_table_p +
	    ((FIRST_DATA_LLI_INDEX + mlli_iter_p->next_lli_idx - 1) *
	     SEP_LLI_ENTRY_WORD_SIZE);

	if (mlli_iter_p->prev_mlli_table_p != NULL)
		link_to_prev_mlli(mlli_iter_p);

	if (mlli_iter_p->cur_mlli_table_p != NULL) {
		set_last_lli(last_lli_p);
		set_last_table(mlli_iter_p->cur_mlli_table_p);
	}

}

static int alloc_next_mlli(struct llimgr_obj *llimgr_p,
			   struct mlli_tables_list *mlli_tables_list_p,
			   struct mlli_tables_list_iterator *mlli_iter_p)
{
	u32 *last_lli_p = mlli_iter_p->cur_mlli_table_p +
	    ((FIRST_DATA_LLI_INDEX + mlli_iter_p->next_lli_idx - 1) *
	     SEP_LLI_ENTRY_WORD_SIZE);

	if (mlli_iter_p->prev_mlli_table_p != NULL) {
		/* "prev == NULL" means that we are on the stub link entry. */
		/* If we have "prev" it means that we already have one table */
		link_to_prev_mlli(mlli_iter_p);
		set_last_lli(last_lli_p);
	}

	mlli_iter_p->prev_mlli_table_p = mlli_iter_p->cur_mlli_table_p;

	/* Allocate MLLI table buffer from the pool */
	mlli_iter_p->cur_mlli_table_p =
	    dma_pool_alloc(llimgr_p->mlli_cache, GFP_KERNEL,
			   &mlli_iter_p->cur_mlli_dma_addr);
	if (mlli_iter_p->cur_mlli_table_p == NULL) {
		pr_err("Failed allocating MLLI table\n");
		return -ENOMEM;
	}

	/* Set DMA addr to the table start from SeP perpective */
	mlli_iter_p->cur_mlli_dma_addr += SEP_LLI_ENTRY_BYTE_SIZE;
	mlli_iter_p->next_lli_idx = 0;
	mlli_iter_p->cur_mlli_accum_data = 0;
	/* Set as last until linked to next (for keeping a valid tables list) */
	set_last_table(mlli_iter_p->cur_mlli_table_p);

	mlli_tables_list_p->table_count++;

	return 0;
}

/**
 * append_lli_to_mlli() - Set current LLI info and progress to next entry
 * @mlli_iter_p:
 * @dma_addr:
 * @dma_size:
 *
 * Returns void
 */
static inline void append_lli_to_mlli(struct mlli_tables_list_iterator
				      *mlli_iter_p, dma_addr_t dma_addr,
				      u32 data_size)
{
	u32 lli_spad[SEP_LLI_ENTRY_WORD_SIZE];	/* LLI scratchpad */
	u32 *next_lli_p;

	next_lli_p = mlli_iter_p->cur_mlli_table_p +
	    ((FIRST_DATA_LLI_INDEX + mlli_iter_p->next_lli_idx) *
	     SEP_LLI_ENTRY_WORD_SIZE);
	/* calc. includes first link entry */
	/* Create LLI entry */
	SEP_LLI_INIT(lli_spad);
	SEP_LLI_SET(lli_spad, ADDR, dma_addr);
	SEP_LLI_SET(lli_spad, SIZE, data_size);
	SEP_LLI_COPY_TO_SEP(next_lli_p, lli_spad);

	mlli_iter_p->next_lli_idx++;
	mlli_iter_p->cur_mlli_accum_data += data_size;
}

/**
 * append_data_to_mlli() - Append given DMA data chunk to given MLLI tables list
 * @llimgr_p:
 * @mlli_tables_list_p:
 * @mlli_iter_p:
 * @dma_addr:	 LLI entry ADDR
 * @dma_size:	 LLI entry SIZE
 *
 * Append given DMA data chunk to given MLLI tables list.
 * Based on given iterator the LLI entry may be added in the current table
 * or if table end reached, a new table would be allocated.
 * In the latter case the previous MLLI table would be linked to current.
 * If not all data fits into current table limit, it would be split in to
 * last LLI in this table and first LLI in the next table (assuming overall
 * DMA data is not more than max_data_per_mlli)
 * Returns int 0 for success
 */
static int append_data_to_mlli(struct llimgr_obj *llimgr_p,
			       struct mlli_tables_list *mlli_tables_list_p,
			       struct mlli_tables_list_iterator *mlli_iter_p,
			       dma_addr_t data_dma_addr, u32 data_size)
{
	u32 remaining_data_for_mlli;
	int rc;

#ifdef DEBUG
	if (data_size > llimgr_p->max_data_per_mlli) {
		pr_err(
			    "Given data size (%uB) is too large for MLLI (%luB)\n",
			    data_size, llimgr_p->max_data_per_mlli);
		return -EINVAL;
	}
#endif

	if (mlli_iter_p->next_lli_idx >= llimgr_p->max_lli_num) {
		/* Reached end of current MLLI table */
		rc = alloc_next_mlli(llimgr_p, mlli_tables_list_p, mlli_iter_p);
		if (rc != 0)
			return rc;
	}

	remaining_data_for_mlli =
	    llimgr_p->max_data_per_mlli - mlli_iter_p->cur_mlli_accum_data;

	if (data_size > remaining_data_for_mlli) {
		/* This chunk does not fit in this table */
		if (remaining_data_for_mlli > 0) {/* Space left in this MLLI */
			/* Add to this table first "half" of the chunk */
			append_lli_to_mlli(mlli_iter_p, data_dma_addr,
					   remaining_data_for_mlli);
			pr_debug("Splitting SG of %uB to %uB+%uB\n",
				      data_size, remaining_data_for_mlli,
				      data_size - remaining_data_for_mlli);
			/* Set the remainder to be pushed in the new table */
			data_dma_addr += remaining_data_for_mlli;
			data_size -= remaining_data_for_mlli;
		}
		rc = alloc_next_mlli(llimgr_p, mlli_tables_list_p, mlli_iter_p);
		if (rc != 0)
			return rc;
	}

	append_lli_to_mlli(mlli_iter_p, data_dma_addr, data_size);

	return 0;
}

/**
 * init_mlli_tables_list() - Initialize MLLI tables list object with user buffer
 *				information
 * @mlli_tables_list_p:
 * @dma_direction:
 * @user_buf_ptr:
 * @buf_size:
 *
 * Returns int 0 on success
 */
static int init_mlli_tables_list(struct llimgr_obj *llimgr_p,
				 struct mlli_tables_list *mlli_tables_list_p,
				 struct client_dma_buffer *client_memref,
				 enum dma_data_direction dma_direction)
{
	const bool is_inbuf = (dma_direction == DMA_TO_DEVICE) ||
	    (dma_direction == DMA_BIDIRECTIONAL);
	const bool is_outbuf = (dma_direction == DMA_FROM_DEVICE) ||
	    (dma_direction == DMA_BIDIRECTIONAL);
	const bool is_memref_inbuf =
	    (client_memref->dma_direction == DMA_TO_DEVICE) ||
	    (client_memref->dma_direction == DMA_BIDIRECTIONAL);
	const bool is_memref_outbuf =
	    (client_memref->dma_direction == DMA_FROM_DEVICE) ||
	    (client_memref->dma_direction == DMA_BIDIRECTIONAL);
	int rc;

#ifdef DEBUG
	/* Verify that given MLLI tables list is "clean" */
	if (mlli_tables_list_p->user_memref != NULL) {
		pr_err("Got \"dirty\" MLLI tables list!\n");
		return -EINVAL;
	}
#endif	 /*DEBUG*/
	    MLLI_TABLES_LIST_INIT(mlli_tables_list_p);
	if (client_memref->buf_size > 0) {
		/* Validate buffer access permissions */
		if (is_inbuf && !is_memref_inbuf) {
			pr_err("No read access (%d) to user buffer @ %p\n",
				    client_memref->dma_direction,
				    client_memref->user_buf_ptr);
			return -EFAULT;
		}
		if (is_outbuf && !is_memref_outbuf) {
			pr_err("No write access (%d), data buffer @ %p\n",
				    client_memref->dma_direction,
				    client_memref->user_buf_ptr);
			return -EFAULT;
		}

	}
	rc = sync_client_dma_buf(llimgr_p->dev,
				 client_memref, true /*for device */ ,
				 dma_direction);
	if (likely(rc == 0)) {
		/* Init. these fields only if the operations above succeeded */
		mlli_tables_list_p->user_memref = client_memref;
		mlli_tables_list_p->data_direction = dma_direction;
	}
	return rc;
}

/**
 * cleanup_mlli_tables_list() - Cleanup MLLI tables resources
 * @llimgr_p:	 LLI-manager pointer
 * @mlli_table_p:	The MLLI tables list object
 * @is_data_dirty:	If true (!0) the (output) data pages are marked as dirty
 *
 * Cleanup MLLI tables resources
 * This function may be invoked for partially constructed MLLI tables list
 * as tests for existence of released resources before trying to release.
 */
static void cleanup_mlli_tables_list(struct llimgr_obj *llimgr_p,
				     struct mlli_tables_list
				     *mlli_tables_list_p, int is_data_dirty)
{
	dma_addr_t cur_mlli_dma_addr;
	dma_addr_t next_mlli_dma_addr;
	u32 *cur_mlli_p;
	u32 *next_mlli_p;
	u32 *link_entry_p;
	u32 lli_spad[SEP_LLI_ENTRY_WORD_SIZE];

	pr_debug("mlli_tables_list_p=%p user_memref=%p table_count=%u\n",
		      mlli_tables_list_p, mlli_tables_list_p->user_memref,
		      mlli_tables_list_p->table_count);
	/* Initialize to the first MLLI table */
	if (mlli_tables_list_p->table_count > 0) {
		cur_mlli_p =
		    SEP_MLLI_GET_NEXT_VA(mlli_tables_list_p->
					 link_to_first_table);
		link_entry_p =
		    mlli_tables_list_p->link_to_first_table +
		    SEP_LLI_ENTRY_WORD_SIZE;
		/* LE to BE... */
		SEP_LLI_COPY_FROM_SEP(lli_spad, link_entry_p);
		/* Actual allocation DMA address is one entry before
		 * saved address */
		cur_mlli_dma_addr =
		    SEP_LLI_GET(lli_spad, ADDR) - SEP_LLI_ENTRY_BYTE_SIZE;
	} else {
		/*DLLI*/ cur_mlli_p = NULL;
		/* Skip the cleanup loop below */
	}

	/* Cleanup MLLI tables */
	while (cur_mlli_p != NULL) {
		pr_debug("Freeing MLLI table buffer at %p (%08llX)\n",
			cur_mlli_p, (long long unsigned int)cur_mlli_dma_addr);
		/* The link entry follows the first entry that holds next VA */
		link_entry_p = cur_mlli_p + SEP_LLI_ENTRY_WORD_SIZE;
		SEP_LLI_COPY_FROM_SEP(lli_spad, link_entry_p);/* LE to BE... */
		/* Save link pointers before freeing the table */
		next_mlli_p = SEP_MLLI_GET_NEXT_VA(cur_mlli_p);
		/* Actual allocation DMA address is one entry before
		 * saved address */
		next_mlli_dma_addr =
		    SEP_LLI_GET(lli_spad, ADDR) - SEP_LLI_ENTRY_BYTE_SIZE;
		dma_pool_free(llimgr_p->mlli_cache,
			      cur_mlli_p, cur_mlli_dma_addr);

		cur_mlli_p = next_mlli_p;
		cur_mlli_dma_addr = next_mlli_dma_addr;
	}

	if ((is_data_dirty) && (mlli_tables_list_p->user_memref != NULL))
		sync_client_dma_buf(llimgr_p->dev,
				    mlli_tables_list_p->user_memref,
				    false /*for CPU */ ,
				    mlli_tables_list_p->data_direction);

	/* Clear traces (pointers) of released resources */
	MLLI_TABLES_LIST_INIT(mlli_tables_list_p);

}

/**
 * process_as_dlli() - Consider given MLLI request as DLLI and update the MLLI
 *			object if possible. Otherwise return error.
 *
 * @mlli_tables_p:	Assciated MLLI object with client memref set.
 * @prepend_data:	Optional prepend data
 * @prepend_data_size:	Optional prepend data size
 */
static int process_as_dlli(struct mlli_tables_list *mlli_tables_p,
			   dma_addr_t prepend_data,
			   unsigned long prepend_data_size)
{
	struct client_dma_buffer *memref = mlli_tables_p->user_memref;
	u32 dma_size = memref->buf_size - memref->save4next_size;

	/* Prepend data only (or 0 data) case */
	if (memref->buf_size == 0) {
		/* Handle 0-sized buffer or prepend_data only */
		set_dlli(mlli_tables_p, prepend_data, prepend_data_size);
		return 0;
	}

	/* Cannot concatenate prepend_data to client data with DLLI */
	if (prepend_data_size > 0)
		return -EINVAL;

#ifdef CONFIG_NOT_COHERENT_CACHE
	/* None of the DLLI cases is possible with cache line alignment buf. */
	if (memref->sg_head != NULL)
		return -EINVAL;
#endif

	/* Physically contiguous buffer case */
	if (memref->sg_tail == NULL) {
		/* If no sg_tail it is an indication that sg_main is phys.
		 * contiguous - DLLI directly to client buffer */
		set_dlli(mlli_tables_p, sg_dma_address(memref->sg_main),
			 dma_size);
		return 0;
	}

	/* Small buffer copied to aux. buffer */
	if (memref->sg_main == NULL) {
		/* If not sg_main (i.e., only sg_tail) we can
		 * DLLI to the aux. buf. */
		set_dlli(mlli_tables_p, memref->buf_end_aux_buf_dma, dma_size);
		return 0;
	}

	return -EINVAL;		/* Not suitable for DLLI */
}

/**
 * llimgr_create_mlli() - Create MLLI tables list for given user buffer
 * @llimgr:
 * @mlli_tables_p:	 A pointer to MLLI tables list object
 * @dma_direction:	 The DMA direction of data flow
 * @user_memref:	 User DMA memory reference (locked pages, etc.)
 * @prepend_data:	 DMA address of data buffer to prepend before user data
 * @prepend_data_size:	 Size of prepend_data (0 if none)
 *
 * Returns int 0 on success
 */
int llimgr_create_mlli(void *llimgr,
		       struct mlli_tables_list *mlli_tables_p,
		       enum dma_data_direction dma_direction,
		       struct client_dma_buffer *client_memref,
		       dma_addr_t prepend_data, unsigned long prepend_data_size)
{
	struct llimgr_obj *llimgr_p = (struct llimgr_obj *)llimgr;
	unsigned long remaining_main_data;
	unsigned int remaining_main_sg_ents;
	struct mlli_tables_list_iterator mlli_iter;
	unsigned long client_dma_size;
	unsigned long cur_sge_len = 0;
	dma_addr_t cur_sge_addr;
	/* cur_lli_index initialized to end of "virtual" link table */
	struct scatterlist *cur_sg_entry;
	int rc;

	/* client_memref must exist even if no user data (buf_size == 0), i.e.,
	 * just prepend_data. */
	if (client_memref == NULL) {
		pr_err("Client memref is NULL.\n");
		return -EINVAL;
	}

	client_dma_size =
		client_memref->buf_size - client_memref->save4next_size;

	SEP_LOG_TRACE(
		      "buf @ 0x%08lX, size=0x%08lX B, prepend_size=0x%08lX B, dma_dir=%d\n",
		      (unsigned long)client_memref->user_buf_ptr,
		      client_memref->buf_size, prepend_data_size,
		      dma_direction);

	rc = init_mlli_tables_list(llimgr_p,
				   mlli_tables_p, client_memref, dma_direction);
	if (unlikely(rc != 0))
		return rc;	/* No resources to cleanup */

	rc = process_as_dlli(mlli_tables_p, prepend_data, prepend_data_size);
	if (rc == 0)		/* Mapped as DLLI */
		return 0;
	rc = 0;			/* In case checked below before updating */

	/* Initialize local state to empty list */
	mlli_iter.prev_mlli_table_p = NULL;
	mlli_iter.cur_mlli_table_p = mlli_tables_p->link_to_first_table;
	/* "First" table is the stub in struct mlli_tables_list, so we
	 * mark it as "full" by setting next_lli_idx to maximum */
	mlli_iter.next_lli_idx = llimgr_p->max_lli_num;
	mlli_iter.cur_mlli_accum_data = 0;

	if (prepend_data_size > 0) {
		rc = append_data_to_mlli(llimgr_p, mlli_tables_p, &mlli_iter,
					 prepend_data, prepend_data_size);
		if (unlikely(rc != 0)) {
			pr_err("Fail: add LLI entry for prepend_data\n");
			goto mlli_create_exit;	/* do cleanup */
		}
	}
#ifdef CONFIG_NOT_COHERENT_CACHE
	if (client_memref->buf_start_aux_buf_size > 0) {
		rc = append_data_to_mlli(llimgr_p, mlli_tables_p, &mlli_iter,
					 client_memref->buf_start_aux_buf_dma,
					 client_memref->buf_start_aux_buf_size);
		if (unlikely(rc != 0)) {
			pr_err("Fail: add LLI entry for start_aux_buf\n");
			goto mlli_create_exit;	/* do cleanup */
		}
	}
#endif

	/* Calculate amount of "main" data before last LLI */
	/* (round to crypto block multiple to avoid having MLLI table
	 * which is not last with non-crypto-block multiple */
	remaining_main_data =
	    (prepend_data_size + client_dma_size -
	     client_memref->buf_end_aux_buf_size) & ~MAX_CRYPTO_BLOCK_MASK;
	/* Now remove the data outside of the main buffer */
	remaining_main_data -= prepend_data_size;
#ifdef CONFIG_NOT_COHERENT_CACHE
	remaining_main_data -= client_memref->buf_start_aux_buf_size;
#endif
	cur_sg_entry = client_memref->sg_main;
	remaining_main_sg_ents = client_memref->sg_main_nents;

	/* construct MLLI tables for sg_main list */
	for (cur_sg_entry = client_memref->sg_main,
	     remaining_main_sg_ents = client_memref->sg_main_nents;
	     cur_sg_entry != NULL;
	     cur_sg_entry = sg_next(cur_sg_entry), remaining_main_sg_ents--) {
		/* Get current S/G entry length */
		cur_sge_len = sg_dma_len(cur_sg_entry);
		cur_sge_addr = sg_dma_address(cur_sg_entry);

		/* Reached end of "main" data which is multiple of largest
		 * crypto block? (consider split to next/last table).
		 * (Check if needs to skip to next table for 2nd half) */
		if ((remaining_main_data > 0) &&
		    (cur_sge_len >=
		     remaining_main_data) /*last "main" data */ &&
		    /* NOT at end of table (i.e.,starting a new table anyway) */
		    (llimgr_p->max_lli_num > mlli_iter.next_lli_idx) &&
		    /* Checks if remainig entries don't fit into this table */
		    (remaining_main_sg_ents -
		     ((cur_sge_len == remaining_main_data) ? 1 : 0) +
		     ((client_memref->buf_end_aux_buf_size > 0) ? 1 : 0)) >
		    (llimgr_p->max_lli_num - mlli_iter.next_lli_idx)) {
			/* "tail" would be in next/last table */
			/* Add last LLI for "main" data */
			rc = append_data_to_mlli(llimgr_p, mlli_tables_p,
						 &mlli_iter, cur_sge_addr,
						 remaining_main_data);
			if (unlikely(rc != 0)) {
				pr_err(
					    "Failed adding LLI entry for sg_main (last).\n");
				goto mlli_create_exit;	/* do cleanup */
			}
			cur_sge_len -= remaining_main_data;
			cur_sge_addr += remaining_main_data;
			/* Skip to next MLLI for tail data */
			rc = alloc_next_mlli(llimgr_p, mlli_tables_p,
					     &mlli_iter);
			if (unlikely(rc != 0)) {
				pr_err("Fail add MLLI table for tail.\n");
				goto mlli_create_exit;	/* do cleanup */
			}
		}

		if (likely(cur_sge_len > 0)) {
			/* When entry is split to next table, this would append
			 * the second half of it. */
			rc = append_data_to_mlli(llimgr_p, mlli_tables_p,
						 &mlli_iter, cur_sge_addr,
						 cur_sge_len);
			if (unlikely(rc != 0)) {
				pr_err("Fail add LLI entry for sg_main\n");
				goto mlli_create_exit;	/* do cleanup */
			}
		}
	}			/*for */

	if (remaining_main_sg_ents > 0) {
		pr_err("Remaining sg_ents>0 after end of S/G list!\n");
		rc = -EINVAL;
		goto mlli_create_exit;	/* do cleanup */
	}

	/* Append end aux. buffer */
	if (client_memref->buf_end_aux_buf_size > 0) {
		rc = append_data_to_mlli(llimgr_p, mlli_tables_p, &mlli_iter,
					 client_memref->buf_end_aux_buf_dma,
					 client_memref->buf_end_aux_buf_size);
		if (unlikely(rc != 0)) {
			pr_err("Fail: add LLI entry for end_aux_buf\n");
			goto mlli_create_exit;	/* do cleanup */
		}
	}

	terminate_mlli_tables_list(&mlli_iter);
	pr_debug("MLLI %u tables (rc=%d):\n",
		      mlli_tables_p->table_count, rc);
	llimgr_dump_mlli_tables_list(mlli_tables_p);

 mlli_create_exit:
	if (rc != 0) {
		/* The MLLI tables list are always consistent at bail-out points
		 * so we can use the simple cleanup function.                 */
		cleanup_mlli_tables_list(llimgr_p, mlli_tables_p, 0);
	}

	return rc;

}

/**
 * llimgr_destroy_mlli() - Cleanup resources of given MLLI tables list object
 *				(if has any tables)
 * @llimgr:
 * @mlli_tables_p:
 *
 */
void llimgr_destroy_mlli(void *llimgr,
			 struct mlli_tables_list *mlli_tables_p)
{
	struct llimgr_obj *llimgr_p = (struct llimgr_obj *)llimgr;
	const bool is_dirty =
	    (mlli_tables_p->data_direction == DMA_BIDIRECTIONAL) ||
	    (mlli_tables_p->data_direction == DMA_FROM_DEVICE);

	cleanup_mlli_tables_list(llimgr_p, mlli_tables_p, is_dirty);
}

/**
 * llimgr_mlli_to_seprpc_memref() - Convert given MLLI tables list into a SeP
 *					RPC memory reference format
 * @mlli_tables_p:	 The source MLLI table
 * @memref_p:	 The destination RPC memory reference
 *
 */
void llimgr_mlli_to_seprpc_memref(struct mlli_tables_list *mlli_tables_p,
				  struct seprpc_memref *memref_p)
{
	u32 xlli_addr;
	u16 xlli_size;
	u16 table_count;

	llimgr_get_mlli_desc_info(mlli_tables_p,
				  &xlli_addr, &xlli_size, &table_count);

	memref_p->ref_type = cpu_to_le32(table_count > 0 ?
					 SEPRPC_MEMREF_MLLI :
					 SEPRPC_MEMREF_DLLI);
	memref_p->location = cpu_to_le32(xlli_addr);
	memref_p->size = cpu_to_le32(xlli_size);
	memref_p->count = cpu_to_le32(table_count);
}

/**
 * llimgr_get_mlli_desc_info() - Get the MLLI info required for a descriptor.
 *
 * @mlli_tables_p:	The source MLLI table
 * @first_table_addr_p:	First table DMA address or data DMA address for DLLI
 * @first_table_size_p:	First table size in bytes or data size for DLLI
 * @num_of_tables:	Number of MLLI tables in the list (0 for DLLI)
 *
 * In case of DLLI, first_table_* refers to the client DMA buffer (DLLI info.)
 */
void llimgr_get_mlli_desc_info(struct mlli_tables_list *mlli_tables_p,
			       u32 *first_table_addr_p,
			       u16 *first_table_size_p,
			       u16 *num_of_tables_p)
{
	u32 link_lli_spad[SEP_LLI_ENTRY_WORD_SIZE];
	u32 *first_mlli_link_p;

	first_mlli_link_p = mlli_tables_p->link_to_first_table +
	    SEP_LLI_ENTRY_WORD_SIZE;
	SEP_LLI_COPY_FROM_SEP(link_lli_spad, first_mlli_link_p);
	/* Descriptor are read by direct-access which takes care of
	 * swapping from host endianess to SeP endianess, so we need
	 * to revert the endiness in the link LLI entry */
	*first_table_addr_p = SEP_LLI_GET(link_lli_spad, ADDR);
	*first_table_size_p = SEP_LLI_GET(link_lli_spad, SIZE);
	*num_of_tables_p = mlli_tables_p->table_count;
}
