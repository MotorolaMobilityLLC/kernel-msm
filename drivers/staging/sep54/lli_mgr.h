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

/*!                                                                           *
 * \file lli_mgr.h                                                            *
 * \brief LLI logic: API definition                                           *
 *                                                                            */

#ifndef __LLI_MGR_H__
#define __LLI_MGR_H__

#include <linux/dma-mapping.h>
#include <linux/device.h>
#include "sep_lli.h"

#define LLIMGR_NULL_HANDLE NULL

/* Using this macros assures correct initialization in case of future changes */
#define MLLI_TABLES_LIST_INIT(mlli_tables_list_ptr) do {		\
	memset((mlli_tables_list_ptr), 0,				\
	       sizeof(struct mlli_tables_list));			\
} while (0)		/* Executed once */

/**
 * llimgr_is_same_mlli_tables() - Tell if given tables list are pointing to the
 *					same tables list
 * @llimgr:
 * @mlli_tables1:	 First MLLI tables list object
 * @mlli_tables2:	 Second MLLI tables list object
 *
 * Tell if given tables list are pointing to the same tables list
 * (used to identify in-place operation)
 */
#define llimgr_is_same_mlli_tables(llimgr, mlli_tables1, mlli_tables2)   \
	((mlli_tables1)->user_memref == (mlli_tables2)->user_memref)

/* Clean struct client_dma_buffer buffer info */
#define CLEAN_DMA_BUFFER_INFO(_client_dma_buf_p) \
	memset(_client_dma_buf_p, 0, sizeof(struct client_dma_buffer))

/**
 * struct client_dma_buffer - Client DMA buffer object
 * @buf_size: buffer size in bytes
 * @user_buf_ptr:	Pointer to start of buffer in user space. May be NULL
 *			for buf_size==0 or if the user is the kernel (given
 *			scatterlist)
 * @num_of_pages:	Number of pages in user_pages array
 * @user_pages:		Locked user pages (for user space buffer)
 * @dma_direction:	DMA direction over given buffer mapping
 * @sg_head:		S/G list of buffer header (memcopied)
 * @sg_main:		S/G list of buffer body (for DMA)
 * @sg_tail:		S/G list of buffer tail (memcopied)
 * @sg_main_nents:	Num. of S/G entries for sg_main
 * @sg_save4next:	S/G list of buffer chunk past "tail" for copying to
 *			side buffer for next operation (for hash block remains)
 * @sg_save4next_nents:	Num. of S/G entries for sg_save4next
 * @save4next_size:	Size of data in sg_save4next
 * @buf_end_aux_buf_va:	Tail aux. buffer virtual address
 * @buf_end_aux_buf_dma:	DMA address of buf_end_aux_buf_va
 * @buf_end_aux_buf_size: Number of bytes copied in tail aux. buffer
 * @buf_start_aux_buf_va:	Header aux. buffer virtual address
 * @buf_start_aux_buf_dma:	DMA address of buf_start_aux_buf_va
 * @buf_start_aux_buf_size: Number of bytes copied in header aux. buffer
 */
struct client_dma_buffer {
	unsigned long buf_size;
	/* User buffer info. */
	u8 __user *user_buf_ptr;
	int num_of_pages;
	struct page **user_pages;

	/*
	 * DMA mapping info (either created for user space pages
	 * or retrieved from kernel client)
	 */
	enum dma_data_direction dma_direction;
#ifdef CONFIG_NOT_COHERENT_CACHE
	struct scatterlist *sg_head;
#endif
	struct scatterlist *sg_main;
	struct scatterlist *sg_tail;
	struct scatterlist *sg_save4next;

	unsigned int sg_main_nents;
	unsigned long save4next_size;
	/* Auxilliary driver buffer for sg_head and sg_tail copies */
	void *buf_end_aux_buf_va;
	dma_addr_t buf_end_aux_buf_dma;
	unsigned long buf_end_aux_buf_size;
#ifdef CONFIG_NOT_COHERENT_CACHE
	/* Currently only cache line at buffer start requires aux. buffers */
	void *buf_start_aux_buf_va;
	dma_addr_t buf_start_aux_buf_dma;
	unsigned long buf_start_aux_buf_size;
#endif				/*CONFIG_NOT_COHERENT_CACHE */

};

/**
 * struct mlli_tables_list - MLLI tables list object
 * @link_to_first_table:	"link" to first table (one extra entry for link
 *				to first table we are using LLI entry in order
 *				to implement the list head using the same data
 *				structure used to link subsequent tables. This
 *				helps avoiding special code for the first table
 *				linking.
 * @table_count:	The total number of fragmented tables. table_count is
 *			only 16b following the size allocated for it in the
 *			SW descriptor.
 * @user_memref:	Referenced client DMA buffer
 * @data_direction:	This operation DMA direction
 *
 */
struct mlli_tables_list {
	u32 link_to_first_table[2 * SEP_LLI_ENTRY_WORD_SIZE];
	u16 table_count;
	struct client_dma_buffer *user_memref;
	enum dma_data_direction data_direction;
};

/**
 * checks if the given MLLI tables list object points to a Direct LLI buffer.
 * @mlli_tables_p:	a pointer to a table list
 *
 * Returns true if DLLI buffer, false otherwise. If mlli_tables_p points to
 * an empty data buffer this function will return true
 */
static inline bool llimgr_mlli_is_dlli(struct mlli_tables_list *mlli_tables_p)
{
	return (mlli_tables_p->user_memref != NULL) &&
	    (mlli_tables_p->table_count == 0);
}

/**
 * llimgr_create() - Create LLI-manager object
 * @dev:	 Device context
 * @mlli_table_size:	 The maximum size of an MLLI table in bytes
 *
 * Returns llimgr_h Created object handle or LLIMGR_NULL_HANDLE if failed
 */
void *llimgr_create(struct device *dev, unsigned long mlli_table_size);

/**
 * llimgr_destroy() - Destroy (free resources of) given LLI-manager object
 * @llimgr:	 LLI-manager object handle
 *
 */
void llimgr_destroy(void *llimgr);

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
				   struct client_dma_buffer *client_dma_buf_p);

/**
 * llimgr_deregister_client_dma_buf() - Unmap given user DMA buffer
 *					(flush and unlock pages)
 * @llimgr:

 * @client_dma_buf_p:	 User DMA buffer object
 *
 */
void llimgr_deregister_client_dma_buf(void *llimgr,
				      struct client_dma_buffer
				      *client_dma_buf_p);

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
					  unsigned long buf_len);

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
		       struct client_dma_buffer *user_memref,
		       dma_addr_t prepend_data,
		       unsigned long prepend_data_size);

/**
 * llimgr_destroy_mlli() - Cleanup resources of given MLLI tables list object
 *				(if has any tables)
 * @llimgr:
 * @mlli_tables_p:
 *
 */
void llimgr_destroy_mlli(void *llimgr,
			 struct mlli_tables_list *mlli_tables_p);

/**
 * llimgr_mlli_to_seprpc_memref() - Convert given MLLI tables list into a
 *					SeP RPC memory reference format
 * @mlli_tables_p:	 The source MLLI table
 * @memref_p:	 The destination RPC memory reference
 *
 */
void llimgr_mlli_to_seprpc_memref(struct mlli_tables_list *mlli_tables_p,
				  struct seprpc_memref *memref_p);

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
			       u16 *num_of_tables_p);

#ifdef DEBUG
/**
 * llimgr_dump_mlli_tables_list() - Dump all the MLLI tables in a given tables
 *					list
 * @mlli_tables_list_p:	 Pointer to tables list structure
 *
 */
void llimgr_dump_mlli_tables_list(struct mlli_tables_list *mlli_tables_list_p);
#else
#define llimgr_dump_mlli_tables_list(mlli_tables_list_p) do {} while (0)
#endif /*DEBUG*/
#endif /*__LLI_MGR_H__*/
