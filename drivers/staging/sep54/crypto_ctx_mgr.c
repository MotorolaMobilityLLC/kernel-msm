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

/*! \file
 * This source file implements crypto context management services.
 */

#include <linux/device.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/sched.h>
#define SEP_LOG_CUR_COMPONENT SEP_LOG_MASK_CTX_MGR
#include "sep_log.h"
#include "dx_driver.h"
#include "crypto_ctx_mgr.h"

struct ctxmgr_cache_entry {
	struct crypto_ctx_uid ctx_id;/* Allocated context ID or CTX_INVALID_ID */
	unsigned long lru_time;	/* Monotonically incrementing counter for LRU */
};

struct sep_ctx_cache {
	unsigned long lru_clk;	/* Virtual clock counter */
	/* The virtual clock counter is incremented for each entry
	   allocation/reuse in order to provide LRU info */
	int cache_size;		/* Num. of cache entries */
	struct ctxmgr_cache_entry entries[1];	/*embedded entries */
	/* The "entries" element is only a start point for an array with
	   cache_size entries that starts in this location */
};

/* DES weak keys checking data */
static const u8 des_key_parity[] = {
	8, 1, 0, 8, 0, 8, 8, 0, 0, 8, 8, 0, 8, 0, 2, 8, 0, 8, 8, 0, 8, 0, 0, 8,
	    8,
	0, 0, 8, 0, 8, 8, 3,
	0, 8, 8, 0, 8, 0, 0, 8, 8, 0, 0, 8, 0, 8, 8, 0, 8, 0, 0, 8, 0, 8, 8, 0,
	    0,
	8, 8, 0, 8, 0, 0, 8,
	0, 8, 8, 0, 8, 0, 0, 8, 8, 0, 0, 8, 0, 8, 8, 0, 8, 0, 0, 8, 0, 8, 8, 0,
	    0,
	8, 8, 0, 8, 0, 0, 8,
	8, 0, 0, 8, 0, 8, 8, 0, 0, 8, 8, 0, 8, 0, 0, 8, 0, 8, 8, 0, 8, 0, 0, 8,
	    8,
	0, 0, 8, 0, 8, 8, 0,
	0, 8, 8, 0, 8, 0, 0, 8, 8, 0, 0, 8, 0, 8, 8, 0, 8, 0, 0, 8, 0, 8, 8, 0,
	    0,
	8, 8, 0, 8, 0, 0, 8,
	8, 0, 0, 8, 0, 8, 8, 0, 0, 8, 8, 0, 8, 0, 0, 8, 0, 8, 8, 0, 8, 0, 0, 8,
	    8,
	0, 0, 8, 0, 8, 8, 0,
	8, 0, 0, 8, 0, 8, 8, 0, 0, 8, 8, 0, 8, 0, 0, 8, 0, 8, 8, 0, 8, 0, 0, 8,
	    8,
	0, 0, 8, 0, 8, 8, 0,
	4, 8, 8, 0, 8, 0, 0, 8, 8, 0, 0, 8, 0, 8, 8, 0, 8, 5, 0, 8, 0, 8, 8, 0,
	    0,
	8, 8, 0, 8, 0, 6, 8,
};

/**
 * ctxmgr_get_ctx_size() - Get host context size for given algorithm class
 * @alg_class:	 Queries algorithm class
 *
 * Returns size_t Size in bytes of host context
 */
size_t ctxmgr_get_ctx_size(enum crypto_alg_class alg_class)
{
	switch (alg_class) {
	case ALG_CLASS_SYM_CIPHER:
		return sizeof(struct host_crypto_ctx_sym_cipher);
	case ALG_CLASS_AUTH_ENC:
		return sizeof(struct host_crypto_ctx_auth_enc);
	case ALG_CLASS_MAC:
		return sizeof(struct host_crypto_ctx_mac);
	case ALG_CLASS_HASH:
		return sizeof(struct host_crypto_ctx_hash);
	default:
		return 0;
	}
}

static size_t get_sep_ctx_offset(enum crypto_alg_class alg_class)
{
	switch (alg_class) {
	case ALG_CLASS_SYM_CIPHER:
		return offsetof(struct host_crypto_ctx_sym_cipher, sep_ctx);
	case ALG_CLASS_AUTH_ENC:
		return offsetof(struct host_crypto_ctx_auth_enc, sep_ctx);
	case ALG_CLASS_MAC:
		return offsetof(struct host_crypto_ctx_mac, sep_ctx);
	case ALG_CLASS_HASH:
		return offsetof(struct host_crypto_ctx_hash, sep_ctx);
	default:
		pr_err("Invalid algorith class = %d\n", alg_class);
		return 0;
	}

}

/**
 * get_hash_digest_size() - Get hash digest size (in octets) of given (SeP)
 *				hash mode
 * @hash_mode:
 *
 * Returns u32 Digest size in octets (0 if unknown hash mode)
 */
static u32 get_hash_digest_size(enum sep_hash_mode hash_mode)
{
	switch (hash_mode) {
	case SEP_HASH_SHA1:
		return 160 >> 3;	/* 160 bit */
	case SEP_HASH_SHA224:
		return 224 >> 3;
	case SEP_HASH_SHA256:
		return 256 >> 3;
	case SEP_HASH_SHA384:
		return 384 >> 3;
	case SEP_HASH_SHA512:
		return 512 >> 3;
	default:
		pr_err("Unknown hash mode %d\n", hash_mode);
	}
	return 0;
}

static u16 get_hash_block_size(enum dxdi_hash_type hash_type)
{

	switch (hash_type) {
	case DXDI_HASH_MD5:
		pr_err("MD5 not supported\n");
		break;
	case DXDI_HASH_SHA1:
	case DXDI_HASH_SHA224:
	case DXDI_HASH_SHA256:
		return 512 >> 3;
	case DXDI_HASH_SHA384:
	case DXDI_HASH_SHA512:
		return 1024 >> 3;
	default:
		pr_err("Invalid hash type %d", hash_type);
	}
	return 0;
}

enum sep_hash_mode get_sep_hash_mode(enum dxdi_hash_type hash_type)
{
	switch (hash_type) {
	case DXDI_HASH_MD5:
		pr_err("MD5 not supported\n");
		return SEP_HASH_NULL;
	case DXDI_HASH_SHA1:
		return SEP_HASH_SHA1;
	case DXDI_HASH_SHA224:
		return SEP_HASH_SHA224;
	case DXDI_HASH_SHA256:
		return SEP_HASH_SHA256;
	case DXDI_HASH_SHA384:
		return SEP_HASH_SHA384;
	case DXDI_HASH_SHA512:
		return SEP_HASH_SHA512;
	default:
		pr_err("Invalid hash type=%d\n", hash_type);
		return SEP_HASH_NULL;
	}
}

/**
 * ctxmgr_map_user_ctx() - Map given user context to kernel space + DMA
 * @ctx_info:	 User context info structure
 * @mydev:	 Associated device context
 * @alg_class:	If !ALG_CLASS_NONE, consider context of given class for
 *		size validation (used in uninitialized context mapping)
 *		When set to ALG_CLASS_NONE, the alg_class field of the
 *		host_crypto_ctx is used to verify mapped buffer size.
 * @user_ctx_ptr: Pointer to user space crypto context
 *
 * Returns int 0 for success
 */
int ctxmgr_map_user_ctx(struct client_crypto_ctx_info *ctx_info,
			struct device *mydev,
			enum crypto_alg_class alg_class,
			u32 __user *user_ctx_ptr)
{
	const unsigned long offset_in_page =
	    ((unsigned long)user_ctx_ptr & ~PAGE_MASK);
	const unsigned long dist_from_page_end = PAGE_SIZE - offset_in_page;
	size_t ctx_size = ctxmgr_get_ctx_size(alg_class);
	int pages_mapped;
	int rc;

#ifdef DEBUG
	if (ctx_info->user_ptr != NULL) {
		pr_err("User context already mapped to 0x%p\n",
			    ctx_info->user_ptr);
		return -EINVAL;
	}
#endif
	ctx_info->dev = mydev;

	/* If unknown class, verify that it is at least host_ctx size */
	/* (so we can access the alg_class field in it) */
	ctx_size = (ctx_size == 0) ? sizeof(struct host_crypto_ctx) : ctx_size;
	if (dist_from_page_end < ctx_size) {
		pr_err("Given user context that crosses a page (0x%p)\n",
			    user_ctx_ptr);
		return -EINVAL;
	}

	down_read(&current->mm->mmap_sem);
	pages_mapped = get_user_pages(current, current->mm,
				      (unsigned long)user_ctx_ptr, 1, 1, 0,
				      &ctx_info->ctx_page, 0);
	up_read(&current->mm->mmap_sem);
	if (pages_mapped < 1) {
		pr_err("Failed getting user page\n");
		return -ENOMEM;
	}

	/* Set pointer to correct offset in mapped page */
	ctx_info->ctx_kptr = (struct host_crypto_ctx *)
	    ((unsigned long)ctx_info->ctx_page |
	     ((unsigned long)user_ctx_ptr & ~PAGE_MASK));

	ctx_info->ctx_kptr = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (ctx_info->ctx_kptr == NULL) {
		SEP_LOG_ERR("Memory allocation failed\n");
		return -ENOMEM;
	}

	if (alg_class == ALG_CLASS_NONE) {
		size_t host_ctx_size = sizeof(struct host_crypto_ctx);
		/* Copy common header to get the alg class */
		if (copy_from_user(ctx_info->ctx_kptr,
				user_ctx_ptr, host_ctx_size)) {
			pr_err("Copy from user failed\n");
			rc = -EINVAL;
			goto copy_from_user_failed;
		}
		/* Verify actual context size with class saved in context */
		alg_class = ctx_info->ctx_kptr->alg_class;
		ctx_size = ctxmgr_get_ctx_size(alg_class);
		if (ctx_size == 0) {	/* Unknown class */
			pr_err("Unknown alg class\n");
			rc = -EINVAL;
			goto unknown_alg_class;
		}
		if (dist_from_page_end < ctx_size) {
			pr_err(
				    "Given user context that crosses a page (0x%p)\n",
				    user_ctx_ptr);
			rc = -EINVAL;
			goto ctx_cross_page;
		}
		/* Copy rest of the context when we know the actual size */
		if (copy_from_user((u8 *)ctx_info->ctx_kptr + host_ctx_size,
				(u8 *)user_ctx_ptr + host_ctx_size,
				ctx_size - host_ctx_size)) {
			pr_err("Copy from user failed\n");
			rc = -EINVAL;
			goto copy_from_user_failed;
		}
	}

	/* Map sep_ctx */
	ctx_info->sep_ctx_kptr = (struct sep_ctx_cache_entry *)
	    (((unsigned long)ctx_info->ctx_kptr) +
	     get_sep_ctx_offset(alg_class));
	ctx_info->sep_ctx_dma_addr = dma_map_single(mydev,
						    (void *)ctx_info->
						    sep_ctx_kptr,
						    sizeof(struct
							   sep_ctx_cache_entry),
						    DMA_BIDIRECTIONAL);
	if (dma_mapping_error(mydev, ctx_info->sep_ctx_dma_addr)) {
		pr_err("Mapping sep_ctx for DMA failed");
		rc = -ENOMEM;
		goto sep_ctx_dma_map_failed;
	}

	ctx_info->sep_cache_idx = -1;

	ctx_info->user_ptr = user_ctx_ptr;

	return 0;

 sep_ctx_dma_map_failed:
 copy_from_user_failed:
 ctx_cross_page:
 unknown_alg_class:
	kfree(ctx_info->ctx_kptr);
	ctx_info->ctx_kptr = NULL;
	page_cache_release(ctx_info->ctx_page);
	ctx_info->ctx_page = NULL;
	return rc;
}

/**
 * ctxmgr_unmap_user_ctx() - Unmap given currently mapped user context
 * @ctx_info:	 User context info structure
 */
void ctxmgr_unmap_user_ctx(struct client_crypto_ctx_info *ctx_info)
{
	size_t ctx_size;

	if (ctx_info->ctx_kptr == NULL) {
		/* This is a valid case since we invoke this function in some
		   error cases without knowing if context was mapped or not */
		pr_debug("Context not mapped\n");
		return;
	}

	ctx_size = ctxmgr_get_ctx_size(ctx_info->ctx_kptr->alg_class);

	dma_unmap_single(ctx_info->dev, ctx_info->sep_ctx_dma_addr,
			 sizeof(struct sep_ctx_cache_entry), DMA_BIDIRECTIONAL);
	ctx_info->sep_ctx_dma_addr = 0;

	if (copy_to_user(ctx_info->user_ptr, ctx_info->ctx_kptr, ctx_size))
		pr_err("Copy to user failed\n");

	kfree(ctx_info->ctx_kptr);
	ctx_info->ctx_kptr = NULL;

	if (!PageReserved(ctx_info->ctx_page))
		SetPageDirty(ctx_info->ctx_page);
	page_cache_release(ctx_info->ctx_page);
	ctx_info->ctx_page = NULL;

	ctx_info->sep_cache_idx = -1;

	ctx_info->user_ptr = NULL;

}

/**
 * ctxmgr_map_kernel_ctx() - Map given kernel context + clone SeP context into
 *				Privately allocated DMA buffer
 *				(required for async. ops. on the same context)
 * @ctx_info:	Client crypto context info structure
 * @mydev:	Associated device context
 * @alg_class:	If !ALG_CLASS_NONE, consider context of given class for
 *		size validation (used in uninitialized context mapping)
 *		When set to ALG_CLASS_NONE, the alg_class field of the
 *		host_crypto_ctx is used to verify mapped buffer size.
 * @kernel_ctx_p:	Pointer to kernel space crypto context
 * @sep_ctx_p:	Pointer to (private) SeP context. If !NULL the embedded sep
 *		context is copied into this buffer.
 *		Set to NULL to use the one embedded in host_crypto_ctx.
 * @sep_ctx_dma_addr:	DMA address of private SeP context (if sep_ctx_p!=NULL)
 *
 * Returns int 0 for success
 */
int ctxmgr_map_kernel_ctx(struct client_crypto_ctx_info *ctx_info,
			  struct device *mydev,
			  enum crypto_alg_class alg_class,
			  struct host_crypto_ctx *kernel_ctx_p,
			  struct sep_ctx_cache_entry *sep_ctx_p,
			  dma_addr_t sep_ctx_dma_addr)
{
	int rc = 0;
	size_t embedded_sep_ctx_offset = get_sep_ctx_offset(alg_class);
	struct sep_ctx_cache_entry *embedded_sep_ctx_p =
	    (struct sep_ctx_cache_entry *)(((unsigned long)kernel_ctx_p) +
					   embedded_sep_ctx_offset);

	if (embedded_sep_ctx_offset == 0)
		return -EINVAL;
	if (sep_ctx_p == NULL)	/* Default context is the one inside */
		sep_ctx_p = embedded_sep_ctx_p;

	pr_debug("kernel_ctx_p=%p\n", kernel_ctx_p);

	ctx_info->dev = mydev;
	/* These fields are only relevant for user space context mapping */
	ctx_info->user_ptr = NULL;
	ctx_info->ctx_page = NULL;
	ctx_info->ctx_kptr = kernel_ctx_p;
	ctx_info->sep_ctx_kptr = sep_ctx_p;
	/* We assume that the CryptoAPI context of kernel is allocated using
	   the slab pools, thus aligned to one of the standard blocks and
	   does not cross page boundary (It is required to be physically
	   contiguous for SeP DMA access) */
	if ((((unsigned long)sep_ctx_p + sizeof(struct sep_ctx_cache_entry))
	     >> PAGE_SHIFT) != ((unsigned long)sep_ctx_p >> PAGE_SHIFT)) {
		pr_err("SeP context cross page boundary start=0x%lx len=0x%zX\n",
		       (unsigned long)sep_ctx_p,
		       sizeof(struct sep_ctx_cache_entry));
		return -EINVAL;
	}

	/* Map sep_ctx if embedded in given host context */
	/* (otherwise, assumed to be cache coherent DMA buffer) */
	if (sep_ctx_p == embedded_sep_ctx_p) {
		ctx_info->sep_ctx_dma_addr =
				dma_map_single(mydev, sep_ctx_p,
					       sizeof(struct
						      sep_ctx_cache_entry),
					       DMA_BIDIRECTIONAL);
		if (dma_mapping_error(mydev, ctx_info->sep_ctx_dma_addr)) {
			pr_err("Mapping sep_ctx for DMA failed");
			rc = -ENOMEM;
		}
	} else {
		ctx_info->sep_ctx_dma_addr = sep_ctx_dma_addr;
		/* Clone base context into external SeP context buffer */
		memcpy(sep_ctx_p, embedded_sep_ctx_p,
		       sizeof(struct sep_ctx_cache_entry));
	}

	ctx_info->sep_cache_idx = -1;

	return rc;
}

/**
 * ctxmgr_unmap_kernel_ctx() - Unmap given currently mapped kernel context
 * @ctx_info:	 User context info structure
 */
void ctxmgr_unmap_kernel_ctx(struct client_crypto_ctx_info *ctx_info)
{
	struct sep_ctx_cache_entry *embedded_sep_ctx_p;
	size_t embedded_sep_ctx_offset;

	if (ctx_info == NULL) {
		pr_err("Context not mapped\n");
		return;
	}

#ifdef DEBUG
	if (ctx_info->ctx_kptr == NULL) {
		/* This is a valid case since we invoke this function in some
		   error cases without knowing if context was mapped or not */
		pr_debug("Context not mapped\n");
		return;
	}
#endif

	embedded_sep_ctx_offset =
	    get_sep_ctx_offset(ctx_info->ctx_kptr->alg_class);

#ifdef DEBUG
	if (embedded_sep_ctx_offset == 0) {
		pr_err("Invalid algorithm class\n");
		return;
	}
#endif

	pr_debug("kernel_ctx_ptr=%p\n", ctx_info->ctx_kptr);
	embedded_sep_ctx_p = (struct sep_ctx_cache_entry *)
	    (((unsigned long)ctx_info->ctx_kptr) + embedded_sep_ctx_offset);

	if (embedded_sep_ctx_p == ctx_info->sep_ctx_kptr) {
		dma_unmap_single(ctx_info->dev, ctx_info->sep_ctx_dma_addr,
				 sizeof(struct sep_ctx_cache_entry),
				 DMA_BIDIRECTIONAL);
	}

	ctx_info->sep_ctx_kptr = NULL;
	ctx_info->sep_ctx_dma_addr = 0;
	ctx_info->ctx_kptr = NULL;
	ctx_info->sep_cache_idx = -1;
	ctx_info->dev = NULL;
}

/**
 * get_blk_rem_buf() - Get a pointer to the (hash) block remainder buffer
 *			structure
 * @ctx_info:
 *
 * Returns struct hash_block_remainder*
 */
static struct hash_block_remainder *get_blk_rem_buf(struct
						    client_crypto_ctx_info
						    *ctx_info)
{
	struct host_crypto_ctx_hash *hash_ctx_p =
	    (struct host_crypto_ctx_hash *)ctx_info->ctx_kptr;
	struct host_crypto_ctx_mac *mac_ctx_p =
	    (struct host_crypto_ctx_mac *)ctx_info->ctx_kptr;
	struct hash_block_remainder *blk_rem_p = NULL;

#ifdef DEBUG
	if (ctx_info->ctx_kptr == NULL) {
		pr_err("Context not mapped\n");
		SEP_DRIVER_BUG();
	}
	if ((hash_ctx_p->alg_class != ALG_CLASS_HASH) &&
	    ctxmgr_get_mac_type(ctx_info) != DXDI_MAC_HMAC) {
		pr_err("Not a hash/HMAC context\n");
		SEP_DRIVER_BUG();
	}
#endif

	/* Get the correct block remainder buffer structure */
	if (hash_ctx_p->alg_class == ALG_CLASS_HASH)
		blk_rem_p = &hash_ctx_p->hash_tail;
	else			/* HMAC */
		blk_rem_p = &mac_ctx_p->hmac_tail;

	return blk_rem_p;
}

/**
 * ctxmgr_map2dev_hash_tail() - Map hash data tail buffer in the host context
 *				for DMA to device
 * @ctx_info:
 * @mydev:
 *
 * Returns int 0 on success
 */
int ctxmgr_map2dev_hash_tail(struct client_crypto_ctx_info *ctx_info,
			     struct device *mydev)
{
	struct hash_block_remainder *blk_rem_p = get_blk_rem_buf(ctx_info);

	if (blk_rem_p->size > 0) {
		ctx_info->hash_tail_dma_addr = dma_map_single(mydev,
							      (void *)
							      blk_rem_p->data,
							      blk_rem_p->size,
							      DMA_TO_DEVICE);
		if (dma_mapping_error(mydev, ctx_info->hash_tail_dma_addr)) {
			pr_err("Mapping hash_tail for DMA failed");
			return -ENOMEM;
		}
	}

	return 0;
}

/**
 * ctxmgr_unmap2dev_hash_tail() - Unmap hash data tail buffer from DMA to device
 * @ctx_info:
 * @mydev:
 *
 */
void ctxmgr_unmap2dev_hash_tail(struct client_crypto_ctx_info *ctx_info,
				struct device *mydev)
{
	struct hash_block_remainder *blk_rem_p = get_blk_rem_buf(ctx_info);

	if (blk_rem_p->size > 0) {
		dma_unmap_single(mydev, ctx_info->hash_tail_dma_addr,
				 blk_rem_p->size, DMA_TO_DEVICE);
	}
}

/**
 * ctxmgr_set_ctx_state() - Set context state
 * @ctx_info:	User context info structure
 * @state:	State to set in context
 *
 * Returns void
 */
void ctxmgr_set_ctx_state(struct client_crypto_ctx_info *ctx_info,
			  const enum host_ctx_state state)
{

#ifdef DEBUG
	if (ctx_info->ctx_kptr == NULL) {
		pr_err("Context not mapped\n");
		SEP_DRIVER_BUG();
	}
#endif
	ctx_info->ctx_kptr->state = state;
}

/**
 * ctxmgr_get_ctx_state() - Get context state
 * @ctx_info:	 User context info structure
 *
 * Returns Current context state
 */
enum host_ctx_state ctxmgr_get_ctx_state(const struct client_crypto_ctx_info
					 *ctx_info)
{

#ifdef DEBUG
	if (ctx_info->ctx_kptr == NULL) {
		pr_err("Context not mapped\n");
		SEP_DRIVER_BUG();
	}
#endif
	return ctx_info->ctx_kptr->state;
}

/**
 * ctxmgr_set_ctx_id() - Allocate unique ID for (initialized) user context
 * @ctx_info:	 Client crypto context info structure
 * @ctx_id:	 The unique ID allocated for given context
 *
 * Allocate unique ID for (initialized) user context
 * (Assumes invoked within session mutex so no need for counter protection)
 */
void ctxmgr_set_ctx_id(struct client_crypto_ctx_info *ctx_info,
		       const struct crypto_ctx_uid ctx_id)
{

#ifdef DEBUG
	if (ctx_info->ctx_kptr == NULL) {
		pr_err("Context not mapped\n");
		SEP_DRIVER_BUG();
	}
	pr_debug("ctx_id=0x%16llX for ctx@0x%p\n",
		      ctx_id, (ctx_info->user_ptr == NULL) ?
		      (void *)ctx_info->ctx_kptr : (void *)ctx_info->user_ptr);
#endif
	memcpy(&ctx_info->ctx_kptr->uid, &ctx_id,
		sizeof(struct crypto_ctx_uid));
}

/**
 * ctxmgr_get_ctx_id() - Return the unique ID for current user context
 * @ctx_info:	 User context info structure
 *
 * Returns Allocated ID (or CTX_ID_INVALID if none)
 */
struct crypto_ctx_uid ctxmgr_get_ctx_id(struct client_crypto_ctx_info *ctx_info)
{

#ifdef DEBUG
	if (ctx_info->ctx_kptr == NULL) {
		pr_err("Context not mapped\n");
		SEP_DRIVER_BUG();
	}
#endif
	return ctx_info->ctx_kptr->uid;
}

/**
 * ctxmgr_get_session_id() - Return the session ID of given context ID
 * @ctx_info:
 *
 * Return the session ID of given context ID
 * This may be used to validate ID and verify that it was not tampered
 * in a manner that can allow access to a session of another process
 * Returns u32
 */
u64 ctxmgr_get_session_id(struct client_crypto_ctx_info *ctx_info)
{
#ifdef DEBUG
	if (ctx_info->ctx_kptr == NULL) {
		pr_err("Context not mapped\n");
		SEP_DRIVER_BUG();
	}
#endif
	/* session ID is the 32 MS-bits of the context ID */
	return ctx_info->ctx_kptr->uid.addr;
}

/**
 * ctxmgr_get_alg_class() - Get algorithm class of context
 * @ctx_info:	 User context info structure
 *
 * Returns Current algorithm class of context
 */
enum crypto_alg_class ctxmgr_get_alg_class(const struct client_crypto_ctx_info
					   *ctx_info)
{

#ifdef DEBUG
	if (ctx_info->ctx_kptr == NULL) {
		pr_err("Context not mapped\n");
		SEP_DRIVER_BUG();
	}
#endif
	return ctx_info->ctx_kptr->alg_class;
}

/**
 * ctxmgr_get_crypto_blk_size() - Get the crypto-block length of given context
 *					in octets
 * @ctx_info:	 User context info structure
 *
 * Returns u32 Cypto-block size in bytes, 0 if invalid/unsupported alg.
 */
u32 ctxmgr_get_crypto_blk_size(struct client_crypto_ctx_info *ctx_info)
{
	u32 cblk_size = 0;
#ifdef DEBUG
	if (ctx_info->ctx_kptr == NULL) {
		pr_err("Context not mapped\n");
		SEP_DRIVER_BUG();
	}
#endif

	switch (ctx_info->ctx_kptr->alg_class) {

	case ALG_CLASS_SYM_CIPHER:{
			enum dxdi_sym_cipher_type cipher_type =
			    ((struct host_crypto_ctx_sym_cipher *)
			     ctx_info->ctx_kptr)->props.cipher_type;
			if ((cipher_type >= _DXDI_SYMCIPHER_AES_FIRST) &&
			    (cipher_type <= _DXDI_SYMCIPHER_AES_LAST)) {
				cblk_size = SEP_AES_BLOCK_SIZE;
			} else
			    if (((cipher_type >= _DXDI_SYMCIPHER_DES_FIRST) &&
				 (cipher_type <= _DXDI_SYMCIPHER_DES_LAST)) ||
					    ((cipher_type >=
					      _DXDI_SYMCIPHER_C2_FIRST) &&
					     (cipher_type <=
					      _DXDI_SYMCIPHER_C2_LAST))) {
				/* DES and C2 have the same block size */
				cblk_size = SEP_DES_BLOCK_SIZE;
			} else {
				pr_err("Invalid sym.cipher type %d",
					    cipher_type);
			}
			break;	/*ALG_CLASS_SYM_CIPHER */
		}

	case ALG_CLASS_AUTH_ENC:{
			enum dxdi_auth_enc_type ae_type =
			    ((struct host_crypto_ctx_auth_enc *)
			     ctx_info->ctx_kptr)->props.ae_type;
			if (ae_type == DXDI_AUTHENC_AES_CCM)
				cblk_size = SEP_AES_BLOCK_SIZE;
			else
				pr_err("Invalid auth.enc. type %d",
					    ae_type);
			break;
		}

	case ALG_CLASS_MAC:{
			struct host_crypto_ctx_mac *ctx_p =
			    ((struct host_crypto_ctx_mac *)ctx_info->ctx_kptr);
			const enum dxdi_mac_type mac_type =
			    ctx_p->props.mac_type;
			switch (mac_type) {	/* switch for block size */
			case DXDI_MAC_HMAC:
				cblk_size =
				    get_hash_block_size(ctx_p->props.
							alg_specific.hmac.
							hash_type);
				break;
			case DXDI_MAC_AES_CMAC:
			case DXDI_MAC_AES_XCBC_MAC:
			case DXDI_MAC_AES_MAC:
				cblk_size = SEP_AES_BLOCK_SIZE;
				break;
			default:
				pr_err("Invalid MAC type %d\n",
					    ctx_p->props.mac_type);
			}
			break;	/* ALG_CLASS_MAC */
		}

	case ALG_CLASS_HASH:{
			enum dxdi_hash_type hash_type =
			    ((struct host_crypto_ctx_hash *)
			     ctx_info->ctx_kptr)->hash_type;
			cblk_size = get_hash_block_size(hash_type);
			break;
		}

	default:
		pr_err("Invalid algorithm class %d\n",
			    ctx_info->ctx_kptr->alg_class);

	}			/*switch alg_class */

	return cblk_size;
}

/**
 * ctxmgr_is_valid_adata_size() - Validate additional/associated data for
 * @ctx_info:
 * @adata_size:
 *
 * Validate additional/associated data for
 * auth/enc algs.
 * Returns bool
 */
bool ctxmgr_is_valid_adata_size(struct client_crypto_ctx_info *ctx_info,
				unsigned long adata_size)
{
	struct host_crypto_ctx_auth_enc *host_ctx_p =
	    (struct host_crypto_ctx_auth_enc *)ctx_info->ctx_kptr;

	if (ctx_info->ctx_kptr->alg_class != ALG_CLASS_AUTH_ENC)
		return false;

	if ((adata_size != host_ctx_p->props.adata_size) || (adata_size == 0))
		return false;

	return true;
}

/**
 * ctxmgr_is_valid_data_unit_size() - Validate given data unit for given
 *					alg./mode
 * @ctx_info:
 * @data_unit_size:
 * @is_finalize:
 *
 * Returns bool true is valid.
 */
bool ctxmgr_is_valid_size(struct client_crypto_ctx_info *ctx_info,
				    unsigned long data_unit_size,
				    bool is_finalize)
{
	if (!is_finalize && (data_unit_size == 0)) {
		/* None allow 0 data for intermediate processing blocks */
		pr_err("Given 0 B for intermediate processing!");
		return false;
	}

	switch (ctx_info->ctx_kptr->alg_class) {

	case ALG_CLASS_SYM_CIPHER:{
		struct host_crypto_ctx_sym_cipher *host_ctx_p =
		    (struct host_crypto_ctx_sym_cipher *)ctx_info->
		    ctx_kptr;
		struct sep_ctx_cipher *aes_ctx_p;

		switch (host_ctx_p->props.cipher_type) {
		case DXDI_SYMCIPHER_AES_XTS:
			if (host_ctx_p->props.alg_specific.aes_xts.
			    data_unit_size == 0) {
				/* Initialize on first data unit if not
				   provided by the user */
				if (data_unit_size < 32) {
					pr_err(
						"AES-XTS data unit size too small (%lu). Must be at least 32B\n",
						data_unit_size);
					return false;
				}
				host_ctx_p->props.alg_specific.aes_xts.
				    data_unit_size = data_unit_size;
				aes_ctx_p = (struct sep_ctx_cipher *)
				    &(host_ctx_p->sep_ctx);
				aes_ctx_p->data_unit_size =
				    cpu_to_le32(host_ctx_p->
						props.
						alg_specific.aes_xts.
						data_unit_size);
				break;
			} else if (((is_finalize) &&
				    (data_unit_size > 0)) ||
					    (!is_finalize)) {
				/* finalize which is not empty must be
				   consistent with prev. data unit */
				if (host_ctx_p->props.
				    alg_specific.aes_xts.
				    data_unit_size != data_unit_size) {
					pr_err("Data unit mismatch. was %u. now %lu.\n",
					       host_ctx_p->props.alg_specific.
					       aes_xts.data_unit_size,
					       data_unit_size);
					return false;
				}
			}
			break;
		case DXDI_SYMCIPHER_AES_CTR:
			if (!is_finalize) {	/* !finalize */
				if (!IS_MULT_OF(data_unit_size,
						SEP_AES_BLOCK_SIZE)) {
					pr_err(
						"Data unit size (%lu) is not AES block multiple\n",
						data_unit_size);
					return false;
				}
			}
			break;
		case DXDI_SYMCIPHER_AES_ECB:
		case DXDI_SYMCIPHER_AES_CBC:
			if (!IS_MULT_OF
			    (data_unit_size, SEP_AES_BLOCK_SIZE)) {
				pr_err(
					"Data unit size (%lu) is not AES block multiple\n",
					data_unit_size);
				return false;
			}
			break;
		case DXDI_SYMCIPHER_DES_ECB:
		case DXDI_SYMCIPHER_DES_CBC:
			if (!IS_MULT_OF
			    (data_unit_size, SEP_DES_BLOCK_SIZE)) {
				pr_err(
					"Data unit size (%lu) is not DES block multiple\n",
					data_unit_size);
				return false;
			}
			break;
		case DXDI_SYMCIPHER_C2_ECB:
		case DXDI_SYMCIPHER_C2_CBC:
			if (!IS_MULT_OF
			    (data_unit_size, SEP_C2_BLOCK_SIZE)) {
				pr_err(
					"Data unit size (%lu) is not C2 block multiple\n",
					data_unit_size);
				return false;
			}
			break;
		default:
			pr_err("Invalid cipher type %d\n",
				    host_ctx_p->props.cipher_type);
			return false;
		}

		break;	/*ALG_CLASS_SYM_CIPHER */
	}

	case ALG_CLASS_AUTH_ENC:{
		enum dxdi_auth_enc_type ae_type =
		    ((struct host_crypto_ctx_auth_enc *)
		     ctx_info->ctx_kptr)->props.ae_type;
		if (ae_type == DXDI_AUTHENC_AES_CCM) {
			if (!is_finalize) {	/* !finalize */
				if (!IS_MULT_OF(data_unit_size,
						SEP_AES_BLOCK_SIZE)) {
					pr_err(
						    "Data unit size (%lu) is not AES block multiple\n",
						    data_unit_size);
					return false;
				}
			}
		} else {
			pr_err("Invalid auth.enc. type %d",
				    ae_type);
			return false;
		}
		break;
	}

	case ALG_CLASS_MAC:{
		struct host_crypto_ctx_mac *ctx_p =
		    ((struct host_crypto_ctx_mac *)ctx_info->ctx_kptr);
		const enum dxdi_mac_type mac_type =
		    ctx_p->props.mac_type;
		switch (mac_type) {	/* switch for block size */
		case DXDI_MAC_HMAC:
			break;	/* Any data unit size is allowed */
		case DXDI_MAC_AES_CMAC:
		case DXDI_MAC_AES_XCBC_MAC:
			if (!is_finalize) {
				if (!IS_MULT_OF(data_unit_size,
						SEP_AES_BLOCK_SIZE)) {
					pr_err(
						    "Data unit size (%lu) is not AES block multiple\n",
						    data_unit_size);
					return false;
				}
			}
			break;
		case DXDI_MAC_AES_MAC:
			if (!IS_MULT_OF
			    (data_unit_size, SEP_AES_BLOCK_SIZE)) {
				pr_err(
					    "Data unit size (%lu) is not AES block multiple\n",
					    data_unit_size);
				return false;
			}
			break;
		default:
			pr_err("Invalid MAC type %d\n",
				    ctx_p->props.mac_type);
		}

		ctx_p->client_data_size += data_unit_size;
		break;	/* ALG_CLASS_MAC */
	}

	case ALG_CLASS_HASH:{
		break;	/* Any data unit size is allowed for hash */
	}

	default:
		pr_err("Invalid algorithm class %d\n",
			    ctx_info->ctx_kptr->alg_class);

	}			/*switch alg_class */

	return true;		/* passed validations */
}

/**
 * ctxmgr_get_sym_cipher_type() - Returns the sym cipher specific type.
 * @ctx_info:	 The context info object of the sym cipher alg.
 *
 * Returns enum dxdi_sym_cipher_type The sym cipher type.
 */
enum dxdi_sym_cipher_type ctxmgr_get_sym_cipher_type(const struct
						     client_crypto_ctx_info
						     *ctx_info)
{
#ifdef DEBUG
	if (ctx_info->ctx_kptr == NULL) {
		pr_err("Context not mapped\n");
		SEP_DRIVER_BUG();
	}
#endif

	return ((struct host_crypto_ctx_sym_cipher *)
		ctx_info->ctx_kptr)->props.cipher_type;
}

/**
 * ctxmgr_get_mac_type() - Returns the mac specific type.
 * @ctx_info:	 The context info object of the mac alg.
 *
 * Returns enum dxdi_mac_type The mac type.
 */
enum dxdi_mac_type ctxmgr_get_mac_type(const struct client_crypto_ctx_info
				       *ctx_info)
{
	struct host_crypto_ctx_mac *mac_ctx =
	    (struct host_crypto_ctx_mac *)ctx_info->ctx_kptr;
#ifdef DEBUG
	if (ctx_info->ctx_kptr == NULL) {
		pr_err("Context not mapped\n");
		SEP_DRIVER_BUG();
	}
#endif

	if (mac_ctx->alg_class == ALG_CLASS_MAC)
		return mac_ctx->props.mac_type;
	else
		return DXDI_MAC_NONE;
}

/**
 * ctxmgr_get_hash_type() - Returns the hash specific type.
 * @ctx_info:	 The context info object of the hash alg.
 *
 * Returns dxdi_hash_type The hash type.
 */
enum dxdi_hash_type ctxmgr_get_hash_type(const struct client_crypto_ctx_info
					 *ctx_info)
{
#ifdef DEBUG
	if (ctx_info->ctx_kptr == NULL) {
		pr_err("Context not mapped\n");
		SEP_DRIVER_BUG();
	}
#endif

	return ((struct host_crypto_ctx_hash *)ctx_info->ctx_kptr)->hash_type;
}

/**
 * ctxmgr_save_hash_blk_remainder() - Save hash block tail data in given
 *	context. The data is taken from the save4next chunk of given client
 *	buffer.
 * @ctx_info:	 Client context info structure (HASH's or HMAC's)
 * @client_dma_buf_p:	A client DMA buffer object. Data is taken from the
 *			save4next chunk of this buffer.
 * @append_data:	When true, given data is appended to existing
 *
 * Returns 0 on success
 */
int ctxmgr_save_hash_blk_remainder(struct client_crypto_ctx_info *ctx_info,
				   struct client_dma_buffer *client_dma_buf_p,
				   bool append_data)
{
	u16 copy_offset;
	int rc;
	struct hash_block_remainder *blk_rem_p = get_blk_rem_buf(ctx_info);

	copy_offset = append_data ? blk_rem_p->size : 0;
	rc = llimgr_copy_from_client_buf_save4next(client_dma_buf_p,
						   blk_rem_p->data +
						   copy_offset,
						   HASH_BLK_SIZE_MAX -
						   copy_offset);
	if (likely(rc >= 0)) {	/* rc is the num. of bytes copied */
		blk_rem_p->size = copy_offset + rc;
		pr_debug("Accumalted %u B at offset %u\n",
			      rc, copy_offset);
		rc = 0;	/* Caller of this function expects 0 on success */
	} else {
		pr_err("Failed copying hash block tail from user\n");
	}
	return rc;
}

/**
 * ctxmgr_get_hash_blk_remainder_buf() - Get DMA info for hash block remainder
 *	buffer from given context
 * @ctx_info:			User context info structure
 * @hash_blk_tail_dma_p:	Returned tail buffer DMA address
 *
 * Note: This function must be invoked only when tail_buf is mapped2dev
 *	(using ctxmgr_map2dev_hash_tail)
 * Returns u16 Number of valid bytes/octets in tail buffer
 */
u16 ctxmgr_get_hash_blk_remainder_buf(struct client_crypto_ctx_info *
					   ctx_info,
					   dma_addr_t *
					   hash_blk_remainder_dma_p)
{
	struct hash_block_remainder *blk_rem_p = get_blk_rem_buf(ctx_info);

	*hash_blk_remainder_dma_p = ctx_info->hash_tail_dma_addr;
	return blk_rem_p->size;
}

/**
 * ctxmgr_get_digest_or_mac() - Get the digest/MAC result when applicable
 * @ctx_info:		User context info structure
 * @digest_or_mac:	Pointer to digest/MAC buffer
 *
 * Returns Returned digest/MAC size
 */
u32 ctxmgr_get_digest_or_mac(struct client_crypto_ctx_info *ctx_info,
				  u8 *digest_or_mac)
{
	u8 *digest_mac_source;
	u32 digest_or_mac_size;

	digest_or_mac_size = ctxmgr_get_digest_or_mac_ptr(ctx_info,
							  &digest_mac_source);
	if (digest_mac_source != NULL)
		memcpy(digest_or_mac, digest_mac_source, digest_or_mac_size);
	return digest_or_mac_size;
}

/**
 * ctxmgr_get_digest_or_mac_ptr() - Get the digest/MAC pointer in SeP context
 * @ctx_info:		User context info structure
 * @digest_or_mac_pp:	Returned pointer to digest/MAC buffer
 *
 * Returns Returned digest/MAC size
 * This function may be used for the caller to reference the result instead
 * of always copying
 */
u32 ctxmgr_get_digest_or_mac_ptr(struct client_crypto_ctx_info *ctx_info,
				      u8 **digest_or_mac_pp)
{
	struct sep_ctx_cache_entry *sep_ctx_p;
	u32 digest_or_mac_size = 0;

#ifdef DEBUG
	if (ctx_info->ctx_kptr == NULL) {
		pr_err("Context not mapped\n");
		SEP_DRIVER_BUG();
	}
#endif
	*digest_or_mac_pp = NULL;	/* default */
	sep_ctx_p = ctx_info->sep_ctx_kptr;
	switch (le32_to_cpu(sep_ctx_p->alg)) {
	case SEP_CRYPTO_ALG_HMAC:
	/* HMAC context holds the MAC (digest) in the same place as
	   HASH context */
	case SEP_CRYPTO_ALG_HASH:{
		struct sep_ctx_hash *hash_ctx_p =
		    (struct sep_ctx_hash *)sep_ctx_p;
		digest_or_mac_size =
		    get_hash_digest_size(le32_to_cpu(hash_ctx_p->mode));
		*digest_or_mac_pp = hash_ctx_p->digest;
		break;
	}
	case SEP_CRYPTO_ALG_AES:{
		struct sep_ctx_cipher *aes_ctx_p =
		    (struct sep_ctx_cipher *)sep_ctx_p;
		switch (le32_to_cpu(aes_ctx_p->mode)) {
		case SEP_CIPHER_CBC_MAC:
		case SEP_CIPHER_XCBC_MAC:
		case SEP_CIPHER_CMAC:
			digest_or_mac_size = SEP_AES_BLOCK_SIZE;
		/* The AES MACs are returned in the block_state field */
			*digest_or_mac_pp = aes_ctx_p->block_state;
			break;
		default:
			break;	/* No MAC for others */
		}
		break;
	}
	case SEP_CRYPTO_ALG_AEAD:{
		struct sep_ctx_aead *aead_ctx_p =
		    (struct sep_ctx_aead *)sep_ctx_p;

		if (le32_to_cpu(aead_ctx_p->mode) == SEP_CIPHER_CCM) {
			digest_or_mac_size =
			    le32_to_cpu(aead_ctx_p->tag_size);
			*digest_or_mac_pp = aead_ctx_p->mac_state;
		} else {
			pr_err(
				    "Invalid mode (%d) for SEP_CRYPTO_ALG_AEAD\n",
				    le32_to_cpu(aead_ctx_p->mode));
		}
		break;
	}
	default:
		;		/* No MAC/digest for the other algorithms */
	}

	return digest_or_mac_size;
}

static int set_sep_ctx_alg_mode(struct client_crypto_ctx_info *ctx_info,
				const enum dxdi_sym_cipher_type cipher_type)
{
	struct sep_ctx_cipher *aes_ctx_p;
	struct sep_ctx_cipher *des_ctx_p;
	struct sep_ctx_c2 *c2_ctx_p;

	if (ctx_info == NULL) {
		pr_err("Context not mapped\n");
		return -EINVAL;
	}

	aes_ctx_p = (struct sep_ctx_cipher *)ctx_info->sep_ctx_kptr;
	des_ctx_p = (struct sep_ctx_cipher *)ctx_info->sep_ctx_kptr;
	c2_ctx_p = (struct sep_ctx_c2 *)ctx_info->sep_ctx_kptr;

	switch (cipher_type) {
	case DXDI_SYMCIPHER_AES_ECB:
	case DXDI_SYMCIPHER_AES_CBC:
	case DXDI_SYMCIPHER_AES_CTR:
	case DXDI_SYMCIPHER_AES_XTS:
		aes_ctx_p->alg = cpu_to_le32(SEP_CRYPTO_ALG_AES);
		break;
	case DXDI_SYMCIPHER_DES_ECB:
	case DXDI_SYMCIPHER_DES_CBC:
		des_ctx_p->alg = cpu_to_le32(SEP_CRYPTO_ALG_DES);
		break;
	case DXDI_SYMCIPHER_C2_ECB:
	case DXDI_SYMCIPHER_C2_CBC:
		c2_ctx_p->alg = cpu_to_le32(SEP_CRYPTO_ALG_C2);
		break;
	case DXDI_SYMCIPHER_RC4:
		return -ENOSYS;	/* Not supported via this API (only MSGs) */
	default:
		return -EINVAL;
	}

	/* mode specific initializations */
	switch (cipher_type) {
	case DXDI_SYMCIPHER_AES_ECB:
		aes_ctx_p->mode = cpu_to_le32(SEP_CIPHER_ECB);
		break;
	case DXDI_SYMCIPHER_AES_CBC:
		aes_ctx_p->mode = cpu_to_le32(SEP_CIPHER_CBC);
		break;
	case DXDI_SYMCIPHER_AES_CTR:
		aes_ctx_p->mode = cpu_to_le32(SEP_CIPHER_CTR);
		break;
	case DXDI_SYMCIPHER_AES_XTS:
		aes_ctx_p->mode = cpu_to_le32(SEP_CIPHER_XTS);
		break;
	case DXDI_SYMCIPHER_DES_ECB:
		des_ctx_p->mode = cpu_to_le32(SEP_CIPHER_ECB);
		break;
	case DXDI_SYMCIPHER_DES_CBC:
		des_ctx_p->mode = cpu_to_le32(SEP_CIPHER_CBC);
		break;
	case DXDI_SYMCIPHER_C2_ECB:
		c2_ctx_p->mode = cpu_to_le32(SEP_C2_ECB);
		break;
	case DXDI_SYMCIPHER_C2_CBC:
		c2_ctx_p->mode = cpu_to_le32(SEP_C2_CBC);
		break;
	case DXDI_SYMCIPHER_RC4:
		return -ENOSYS;	/* Not supported via this API (only RPC) */
	default:
		return -EINVAL;
	}

	return 0;
}

/**
 * ctxmgr_set_symcipher_iv_user() - Set IV of symcipher context given in
 *					user space pointer
 * @user_ctx_ptr:	 A user space pointer to the host context
 * @iv_ptr:	 The IV to set
 *
 * Returns int
 */
int ctxmgr_set_symcipher_iv_user(u32 __user *user_ctx_ptr,
				 u8 *iv_ptr)
{
	struct host_crypto_ctx_sym_cipher __user *host_ctx_p =
	    (struct host_crypto_ctx_sym_cipher __user *)user_ctx_ptr;
	struct sep_ctx_cipher *aes_ctx_p =
	    (struct sep_ctx_cipher *)&host_ctx_p->sep_ctx;
	enum dxdi_sym_cipher_type cipher_type;

	/* Copy cypher type from user context */
	if (copy_from_user(&cipher_type, &host_ctx_p->props.cipher_type,
			   sizeof(enum dxdi_sym_cipher_type))) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}

	if ((cipher_type != DXDI_SYMCIPHER_AES_CBC) &&
	    (cipher_type != DXDI_SYMCIPHER_AES_CTR) &&
	    (cipher_type != DXDI_SYMCIPHER_AES_XTS)) {
		return -EINVAL;
	}

	if (copy_to_user(aes_ctx_p->block_state, iv_ptr, SEP_AES_IV_SIZE) ||
	    copy_to_user(host_ctx_p->props.alg_specific.aes_cbc.iv, iv_ptr,
		     SEP_AES_IV_SIZE)) {
		pr_err("Failed writing input parameters");
		return -EFAULT;
	}

	return 0;
}

/**
 * ctxmgr_get_symcipher_iv_user() - Read current "IV"
 * @user_ctx_ptr:	 A user space pointer to the host context
 * @iv_ptr:	 Where to return the read IV
 *
 * Read current "IV" (block state - not the actual set IV during initialization)
 * This function works directly over a user space context
 * Returns int
 */
int ctxmgr_get_symcipher_iv_user(u32 __user *user_ctx_ptr,
				 u8 *iv_ptr)
{
	struct host_crypto_ctx_sym_cipher __user *host_ctx_p =
	    (struct host_crypto_ctx_sym_cipher __user *)user_ctx_ptr;
	struct sep_ctx_cipher *aes_ctx_p =
	    (struct sep_ctx_cipher *)&host_ctx_p->sep_ctx;
	enum dxdi_sym_cipher_type cipher_type;

	/* Copy cypher type from user context */
	if (copy_from_user(&cipher_type, &host_ctx_p->props.cipher_type,
			   sizeof(enum dxdi_sym_cipher_type))) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}

	if ((cipher_type != DXDI_SYMCIPHER_AES_CBC) &&
	    (cipher_type != DXDI_SYMCIPHER_AES_CTR) &&
	    (cipher_type != DXDI_SYMCIPHER_AES_XTS)) {
		return -EINVAL;
	}

	if (copy_from_user(iv_ptr, aes_ctx_p->block_state, SEP_AES_IV_SIZE)) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}

	return 0;
}

/**
 * get_symcipher_iv_info() - Return the IV location in given context
 *				(based on symcipher type)
 * @ctx_info:
 * @host_ctx_iv:	Returned IV pointer in host props field
 * @sep_ctx_iv:		Returned IV state block in sep context
 * @iv_size:	Size of IV in bytes (0 if IV is not applicable for this alg.)
 *
 */
static void get_symcipher_iv_info(struct client_crypto_ctx_info *ctx_info,
				  u8 **host_ctx_iv, u8 **sep_ctx_iv,
				  unsigned long *iv_size)
{
	struct host_crypto_ctx_sym_cipher *host_ctx_p =
	    (struct host_crypto_ctx_sym_cipher *)ctx_info->ctx_kptr;

	switch (host_ctx_p->props.cipher_type) {
	case DXDI_SYMCIPHER_AES_CBC:
	case DXDI_SYMCIPHER_AES_CTR:
	case DXDI_SYMCIPHER_AES_XTS:
		*sep_ctx_iv = ((struct sep_ctx_cipher *)
			       ctx_info->sep_ctx_kptr)->block_state;
		*iv_size = SEP_AES_IV_SIZE;
		break;
	case DXDI_SYMCIPHER_DES_CBC:
		*sep_ctx_iv = ((struct sep_ctx_cipher *)
			       ctx_info->sep_ctx_kptr)->block_state;
		*iv_size = SEP_DES_IV_SIZE;
		break;
	default:
		*sep_ctx_iv = NULL;
		*iv_size = 0;
	}

	switch (host_ctx_p->props.cipher_type) {
	case DXDI_SYMCIPHER_AES_CBC:
		*host_ctx_iv = host_ctx_p->props.alg_specific.aes_cbc.iv;
		break;
	case DXDI_SYMCIPHER_AES_CTR:
		*host_ctx_iv = host_ctx_p->props.alg_specific.aes_ctr.cntr;
		break;
	case DXDI_SYMCIPHER_AES_XTS:
		*host_ctx_iv =
		    host_ctx_p->props.alg_specific.aes_xts.init_tweak;
		break;
	case DXDI_SYMCIPHER_DES_CBC:
		*host_ctx_iv = host_ctx_p->props.alg_specific.des_cbc.iv;
		break;
	default:
		*host_ctx_iv = NULL;
	}

}

/**
 * ctxmgr_set_symcipher_iv() - Set IV for given block symcipher algorithm
 * @ctx_info:	Context to update
 * @iv:		New IV
 *
 * Returns int 0 if changed IV, -EINVAL for error
 * (given cipher type does not have IV)
 */
int ctxmgr_set_symcipher_iv(struct client_crypto_ctx_info *ctx_info,
			    u8 *iv)
{
	u8 *host_ctx_iv;
	u8 *sep_ctx_iv;
	unsigned long iv_size = 0;

	get_symcipher_iv_info(ctx_info, &host_ctx_iv, &sep_ctx_iv, &iv_size);
	if (iv_size > 0 && host_ctx_iv != NULL && sep_ctx_iv != NULL) {
		memcpy(sep_ctx_iv, iv, iv_size);
		memcpy(host_ctx_iv, iv, iv_size);
	}

	return (iv_size > 0) ? 0 : -EINVAL;
}

/**
 * ctxmgr_get_symcipher_iv() - Return given cipher IV
 * @ctx_info:	Context to query
 * @iv_user:	The IV given by the user on last ctxmgr_set_symcipher_iv
 * @iv_current:	The current IV state block
 * @iv_size_p:	[I/O] The given buffers size and returns actual IV size
 *
 * Return given cipher IV - Original IV given by user and current state "IV"
 * The given IV buffers must be large enough to accomodate the IVs
 * Returns int 0 on success, -ENOMEM if given iv_size is too small
 */
int ctxmgr_get_symcipher_iv(struct client_crypto_ctx_info *ctx_info,
			    u8 *iv_user, u8 *iv_current,
			    u8 *iv_size_p)
{
	u8 *host_ctx_iv;
	u8 *sep_ctx_iv;
	unsigned long iv_size;
	int rc = 0;

	get_symcipher_iv_info(ctx_info, &host_ctx_iv, &sep_ctx_iv, &iv_size);
	if (iv_size > 0) {
		if (*iv_size_p < iv_size) {
			rc = -ENOMEM;
		} else {
			if (iv_current != NULL && sep_ctx_iv != NULL)
				memcpy(iv_current, sep_ctx_iv, iv_size);
			if (iv_user != NULL && host_ctx_iv != NULL)
				memcpy(iv_user, host_ctx_iv, iv_size);
		}
	}

	/* Always return IV size for informational purposes */
	*iv_size_p = iv_size;
	return rc;
}

/**
 * ctxmgr_set_symcipher_direction() - Set the operation direction for given
 *					symcipher context
 * @ctx_info:		Context to update
 * @dxdi_direction:	Requested cipher direction
 *
 * Returns int 0 on success
 */
int ctxmgr_set_symcipher_direction(struct client_crypto_ctx_info *ctx_info,
				   enum dxdi_cipher_direction dxdi_direction)
{
	struct host_crypto_ctx_sym_cipher *host_ctx_p =
	    (struct host_crypto_ctx_sym_cipher *)ctx_info->ctx_kptr;
	enum sep_crypto_direction sep_direction;
	struct sep_ctx_cipher *aes_ctx_p;
	struct sep_ctx_cipher *des_ctx_p;
	struct sep_ctx_c2 *c2_ctx_p;

	/* Translate direction from driver ABI to SeP ABI */
	if (dxdi_direction == DXDI_CDIR_ENC) {
		sep_direction = SEP_CRYPTO_DIRECTION_ENCRYPT;
	} else if (dxdi_direction == DXDI_CDIR_DEC) {
		sep_direction = SEP_CRYPTO_DIRECTION_DECRYPT;
	} else {
		pr_err("Invalid direction=%d\n", dxdi_direction);
		return -EINVAL;
	}

	switch (host_ctx_p->props.cipher_type) {
	case DXDI_SYMCIPHER_AES_ECB:
	case DXDI_SYMCIPHER_AES_CBC:
	case DXDI_SYMCIPHER_AES_CTR:
	case DXDI_SYMCIPHER_AES_XTS:
		aes_ctx_p = (struct sep_ctx_cipher *)ctx_info->sep_ctx_kptr;
		aes_ctx_p->direction = cpu_to_le32(sep_direction);
		break;

	case DXDI_SYMCIPHER_DES_ECB:
	case DXDI_SYMCIPHER_DES_CBC:
		des_ctx_p = (struct sep_ctx_cipher *)ctx_info->sep_ctx_kptr;
		des_ctx_p->direction = cpu_to_le32(sep_direction);
		break;

	case DXDI_SYMCIPHER_C2_ECB:
	case DXDI_SYMCIPHER_C2_CBC:
		c2_ctx_p = (struct sep_ctx_c2 *)ctx_info->sep_ctx_kptr;
		c2_ctx_p->direction = cpu_to_le32(sep_direction);
		break;

	case DXDI_SYMCIPHER_RC4:
		pr_err("Invoked for RC4!\n");
		return -ENOSYS;	/* Not supported via this API (only RPC) */
	default:
		pr_err("Invalid symcipher type %d\n",
			    host_ctx_p->props.cipher_type);
		return -EINVAL;
	}

	host_ctx_p->props.direction = dxdi_direction;

	return 0;
}

/**
 * ctxmgr_get_symcipher_direction() - Return the operation direction of given
 *					symcipher context
 * @ctx_info:	Context to query
 *
 * Returns enum dxdi_cipher_direction (<0 on error)
 */
enum dxdi_cipher_direction ctxmgr_get_symcipher_direction(struct
							  client_crypto_ctx_info
							  *ctx_info)
{
	struct host_crypto_ctx_sym_cipher *host_ctx_p =
	    (struct host_crypto_ctx_sym_cipher *)ctx_info->ctx_kptr;
	enum sep_crypto_direction sep_direction;
	enum dxdi_cipher_direction dxdi_direction;
	struct sep_ctx_cipher *aes_ctx_p;
	struct sep_ctx_cipher *des_ctx_p;
	struct sep_ctx_c2 *c2_ctx_p;

	switch (host_ctx_p->props.cipher_type) {
	case DXDI_SYMCIPHER_AES_ECB:
	case DXDI_SYMCIPHER_AES_CBC:
	case DXDI_SYMCIPHER_AES_CTR:
	case DXDI_SYMCIPHER_AES_XTS:
		aes_ctx_p = (struct sep_ctx_cipher *)ctx_info->sep_ctx_kptr;
		sep_direction = le32_to_cpu(aes_ctx_p->direction);
		break;

	case DXDI_SYMCIPHER_DES_ECB:
	case DXDI_SYMCIPHER_DES_CBC:
		des_ctx_p = (struct sep_ctx_cipher *)ctx_info->sep_ctx_kptr;
		sep_direction = le32_to_cpu(des_ctx_p->direction);
		break;

	case DXDI_SYMCIPHER_C2_ECB:
	case DXDI_SYMCIPHER_C2_CBC:
		c2_ctx_p = (struct sep_ctx_c2 *)ctx_info->sep_ctx_kptr;
		sep_direction = le32_to_cpu(c2_ctx_p->direction);
		break;

	case DXDI_SYMCIPHER_RC4:
		pr_err("Invoked for RC4!\n");
		return -ENOSYS;	/* Not supported via this API (only RPC) */
	default:
		pr_err("Invalid symcipher type %d\n",
			    host_ctx_p->props.cipher_type);
		return -EINVAL;
	}

	/* Translate direction from driver ABI to SeP ABI */
	if (sep_direction == SEP_CRYPTO_DIRECTION_ENCRYPT) {
		dxdi_direction = DXDI_CDIR_ENC;
	} else if (sep_direction == SEP_CRYPTO_DIRECTION_DECRYPT) {
		dxdi_direction = DXDI_CDIR_DEC;
	} else {
		pr_err("Invalid (sep) direction=%d\n", sep_direction);
		dxdi_direction = -EINVAL;
	}

	return dxdi_direction;
}

/*
 * RFC2451: Weak DES key check
 * Returns: 1 (weak), 0 (not weak)
 */
/**
 * is_weak_des_key() - Validate DES weak keys per RFC2451 (section 2.3)
 * @key:
 *
 * @keylen: 8 or 24 bytes
 *
 *
 * Validate DES weak keys per RFC2451 (section 2.3)
 * (weak key validation based on DX_CRYPTO_DES.c of cc52_crypto)
 * Returns bool "true" for weak keys
 */
static bool is_weak_des_key(const u8 *key, unsigned int keylen)
{
	u32 n, w;
	u64 *k1, *k2, *k3;	/* For 3DES/2DES checks */

	if (keylen > 8) {/* For 3DES/2DES only validate no key repeatition */
		k1 = (u64 *)key;
		k2 = k1 + 1;
		if (*k1 == *k2)
			return true;
		if (keylen > 16) {	/* 3DES */
			k3 = k2 + 1;
			if (*k2 == *k3)
				return true;
		}
	}

	/* Only for single-DES, check weak keys */
	n = des_key_parity[key[0]];
	n <<= 4;
	n |= des_key_parity[key[1]];
	n <<= 4;
	n |= des_key_parity[key[2]];
	n <<= 4;
	n |= des_key_parity[key[3]];
	n <<= 4;
	n |= des_key_parity[key[4]];
	n <<= 4;
	n |= des_key_parity[key[5]];
	n <<= 4;
	n |= des_key_parity[key[6]];
	n <<= 4;
	n |= des_key_parity[key[7]];
	w = 0x88888888L;

	/* 1 in 10^10 keys passes this test */
	if (!((n - (w >> 3)) & w)) {
		switch (n) {
		case 0x11111111:
		case 0x13131212:
		case 0x14141515:
		case 0x16161616:
		case 0x31312121:
		case 0x33332222:
		case 0x34342525:
		case 0x36362626:
		case 0x41415151:
		case 0x43435252:
		case 0x44445555:
		case 0x46465656:
		case 0x61616161:
		case 0x63636262:
		case 0x64646565:
		case 0x66666666:
			return true;
		}
	}
	return false;
}

/**
 * ctxmgr_set_symcipher_key() - Set a symcipher context key
 * @ctx_info:	Context to update
 * @key_size:	Size of key in bytes
 * @key:	New key pointer
 *
 * Set a symcipher context key
 * After invoking this function the context should be reinitialized by SeP
 * (set its state to "partial init" if not done in this sequence)
 * Returns int 0 on success, -EINVAL Invalid key len, -EPERM Forbidden/weak key
 */
int ctxmgr_set_symcipher_key(struct client_crypto_ctx_info *ctx_info,
			     u8 key_size, const u8 *key)
{
	struct sep_ctx_cipher *aes_ctx_p = NULL;
	struct sep_ctx_cipher *des_ctx_p = NULL;
	struct sep_ctx_c2 *c2_ctx_p = NULL;
	struct dxdi_sym_cipher_props *props =
	    &((struct host_crypto_ctx_sym_cipher *)ctx_info->ctx_kptr)->props;

	/* Update the respective sep context fields if valid */
	switch (props->cipher_type) {
	case DXDI_SYMCIPHER_AES_ECB:
	case DXDI_SYMCIPHER_AES_CBC:
	case DXDI_SYMCIPHER_AES_CTR:
	case DXDI_SYMCIPHER_AES_XTS:
		aes_ctx_p = (struct sep_ctx_cipher *)ctx_info->sep_ctx_kptr;
		/* Validate key size before copying */
		if (props->cipher_type == DXDI_SYMCIPHER_AES_XTS) {
			/* XTS has two keys of either 128b or 256b */
			if ((key_size != 32) && (key_size != 64)) {
				pr_err(
					"Invalid key size for AES-XTS (%u bits)\n",
					key_size * 8);
				return -EINVAL;
			}
			/* Divide by two (we have two keys of the same size) */
			key_size >>= 1;
			/* copy second half of the double-key as XEX-key */
			memcpy(aes_ctx_p->xex_key, key + key_size, key_size);
			/* Always clear data_unit_size on key change
			   (Assumes new data source with possibly different
			   data unit size). The actual data unit size
			   would be concluded on the next data processing. */
			props->alg_specific.aes_xts.data_unit_size = 0;
			aes_ctx_p->data_unit_size = cpu_to_le32(0);
		} else {	/* AES engine support 128b/192b/256b keys */
			if ((key_size != 16) &&
			    (key_size != 24) && (key_size != 32)) {
				pr_err(
					"Invalid key size for AES (%u bits)\n",
					key_size * 8);
				return -EINVAL;
			}
		}
		memcpy(aes_ctx_p->key, key, key_size);
		aes_ctx_p->key_size = cpu_to_le32(key_size);
		break;

	case DXDI_SYMCIPHER_DES_ECB:
	case DXDI_SYMCIPHER_DES_CBC:
		if (is_weak_des_key(key, key_size)) {
			pr_info("Weak DES key.\n");
			return -EPERM;
		}
		des_ctx_p = (struct sep_ctx_cipher *)ctx_info->sep_ctx_kptr;
		des_ctx_p->key_size = cpu_to_le32(key_size);
		if ((key_size != 8) && (key_size != 16) && (key_size != 24)) {
			/* Avoid copying a key too large */
			pr_err("Invalid key size for DES (%u bits)\n",
				    key_size * 8);
			return -EINVAL;
		}
		memcpy(des_ctx_p->key, key, key_size);
		break;

	case DXDI_SYMCIPHER_C2_ECB:
	case DXDI_SYMCIPHER_C2_CBC:
		c2_ctx_p = (struct sep_ctx_c2 *)ctx_info->sep_ctx_kptr;
		c2_ctx_p->key_size = cpu_to_le32(key_size);
		if (key_size != SEP_C2_KEY_SIZE_MAX) {
			/* Avoid copying a key too large */
			pr_err("Invalid key size for C2 (%u bits)\n",
				    key_size * 8);
			return -EINVAL;
		}
		memcpy(c2_ctx_p->key, key, key_size);
		break;

	case DXDI_SYMCIPHER_RC4:
		return -ENOSYS;	/* Not supported via this API (only MSGs) */
	default:
		return -EINVAL;
	}

	/* If reached here then all validations passed */
	/* Update in props of host context */
	memcpy(props->key, key, key_size);
	props->key_size = key_size;

	return 0;
}

/**
 * ctxmgr_init_symcipher_ctx_no_props() - Initialize symcipher context when full
 *					props are not available, yet.
 * @ctx_info:		Context to init.
 * @cipher_type:	Cipher type for context
 *
 * Initialize symcipher context when full props are not available, yet.
 * Later set_key and set_iv may update the context.
 * Returns int 0 on success
 */
int ctxmgr_init_symcipher_ctx_no_props(struct client_crypto_ctx_info *ctx_info,
				       enum dxdi_sym_cipher_type cipher_type)
{
	struct host_crypto_ctx_sym_cipher *host_ctx_p =
	    (struct host_crypto_ctx_sym_cipher *)ctx_info->ctx_kptr;

	/* Initialize host context part with just cipher type */
	host_ctx_p->alg_class = ALG_CLASS_SYM_CIPHER;
	memset(&(host_ctx_p->props), 0, sizeof(struct dxdi_sym_cipher_props));
	host_ctx_p->props.cipher_type = cipher_type;
	host_ctx_p->is_encrypted = false;

	/* Clear SeP context before setting specific fields */
	/* This helps in case SeP-FW assumes zero on uninitialized fields */
	memset(ctx_info->sep_ctx_kptr, 0, sizeof(struct sep_ctx_cache_entry));

	/* with only cipher_type we can initialize just the alg/mode fields */
	return set_sep_ctx_alg_mode(ctx_info, cipher_type);
}

/**
 * ctxmgr_init_symcipher_ctx() - Initialize symCipher context based on given
 *				properties.
 * @ctx_info:	 User context mapping info.
 * @props:	 The initialization properties
 * @postpone_init:	Return "true" if INIT on SeP should be postponed
 *			to first processing (e.g, in AES-XTS)
 * @error_info:	Error info
 *
 * Returns 0 on success, otherwise on error
 */
int ctxmgr_init_symcipher_ctx(struct client_crypto_ctx_info *ctx_info,
			      struct dxdi_sym_cipher_props *props,
			      bool *postpone_init, u32 *error_info)
{
	struct host_crypto_ctx_sym_cipher *host_ctx_p =
	    (struct host_crypto_ctx_sym_cipher *)ctx_info->ctx_kptr;
	struct sep_ctx_cipher *aes_ctx_p = NULL;
	struct sep_ctx_cipher *des_ctx_p = NULL;
	int rc;

#ifdef DEBUG
	if (ctx_info->ctx_kptr == NULL) {
		pr_err("Context not mapped\n");
		SEP_DRIVER_BUG();
	}
#endif

	*postpone_init = false;	/* default */
	*error_info = DXDI_ERROR_NULL;	/* assume no error */

	/* Initialize host context part */
	host_ctx_p->alg_class = ALG_CLASS_SYM_CIPHER;
	memcpy(&(host_ctx_p->props), props,
	       sizeof(struct dxdi_sym_cipher_props));
	host_ctx_p->is_encrypted = false;

	/* Clear SeP context before setting specific fields */
	/* This helps in case SeP-FW assumes zero on uninitialized fields */
	memset(&(host_ctx_p->sep_ctx), 0, sizeof(host_ctx_p->sep_ctx));

	rc = set_sep_ctx_alg_mode(ctx_info, props->cipher_type);
	if (rc != 0) {
		*error_info = DXDI_ERROR_INVAL_MODE;
		return rc;
	}

	rc = ctxmgr_set_symcipher_direction(ctx_info, props->direction);
	if (rc != 0) {
		*error_info = DXDI_ERROR_INVAL_DIRECTION;
		return rc;
	}

	rc = ctxmgr_set_symcipher_key(ctx_info, props->key_size, props->key);
	if (rc != 0) {
		*error_info = DXDI_ERROR_INVAL_KEY_SIZE;
		return rc;
	}

	/* mode specific initializations */
	switch (props->cipher_type) {
	case DXDI_SYMCIPHER_AES_CBC:
		aes_ctx_p = (struct sep_ctx_cipher *)ctx_info->sep_ctx_kptr;
		memcpy(aes_ctx_p->block_state, props->alg_specific.aes_cbc.iv,
		       SEP_AES_IV_SIZE);
		break;
	case DXDI_SYMCIPHER_AES_CTR:
		aes_ctx_p = (struct sep_ctx_cipher *)ctx_info->sep_ctx_kptr;
		memcpy(aes_ctx_p->block_state, props->alg_specific.aes_ctr.cntr,
		       SEP_AES_IV_SIZE);
		break;
	case DXDI_SYMCIPHER_AES_XTS:
		aes_ctx_p = (struct sep_ctx_cipher *)ctx_info->sep_ctx_kptr;
		memcpy(aes_ctx_p->block_state,
		       props->alg_specific.aes_xts.init_tweak, SEP_AES_IV_SIZE);
		aes_ctx_p->data_unit_size =
		    cpu_to_le32(props->alg_specific.aes_xts.data_unit_size);
		/* update in context because was cleared by
		   ctxmgr_set_symcipher_key */
		host_ctx_p->props.alg_specific.aes_xts.data_unit_size =
		    props->alg_specific.aes_xts.data_unit_size;
		if (props->alg_specific.aes_xts.data_unit_size == 0)
			*postpone_init = true;
		break;
	case DXDI_SYMCIPHER_DES_CBC:
		des_ctx_p = (struct sep_ctx_cipher *)ctx_info->sep_ctx_kptr;
		memcpy(des_ctx_p->block_state, props->alg_specific.des_cbc.iv,
		       SEP_DES_IV_SIZE);
		break;
	case DXDI_SYMCIPHER_C2_CBC:
		/*C2 reset interval is not supported, yet */
		*error_info = DXDI_ERROR_UNSUP;
		return -ENOSYS;
	case DXDI_SYMCIPHER_RC4:
		*error_info = DXDI_ERROR_UNSUP;
		return -ENOSYS;	/* Not supported via this API (only MSGs) */
	default:
		break;	/* No specific initializations for other modes */
	}

	return 0;
}

/**
 * ctxmgr_init_auth_enc_ctx() - Initialize Authenticated Encryption class
 *				context
 * @ctx_info:
 * @props:
 * @error_info:	Error info
 *
 * Returns int
 */
int ctxmgr_init_auth_enc_ctx(struct client_crypto_ctx_info *ctx_info,
			     struct dxdi_auth_enc_props *props,
			     u32 *error_info)
{
	struct host_crypto_ctx_auth_enc *host_ctx_p =
	    (struct host_crypto_ctx_auth_enc *)ctx_info->ctx_kptr;
	struct sep_ctx_aead *aead_ctx_p = NULL;
	enum sep_crypto_direction direction;

#ifdef DEBUG
	if (ctx_info->ctx_kptr == NULL) {
		pr_err("Context not mapped\n");
		SEP_DRIVER_BUG();
	}
#endif
	*error_info = DXDI_ERROR_NULL;

	/* Initialize host context part */
	host_ctx_p->alg_class = ALG_CLASS_AUTH_ENC;
	host_ctx_p->is_adata_processed = 0;
	memcpy(&(host_ctx_p->props), props, sizeof(struct dxdi_auth_enc_props));
	host_ctx_p->is_encrypted = false;

	/* Clear SeP context before setting specific fields */
	/* This helps in case SeP-FW assumes zero on uninitialized fields */
	memset(&(host_ctx_p->sep_ctx), 0, sizeof(host_ctx_p->sep_ctx));

	/* Translate direction from driver ABI to SeP ABI */
	if (props->direction == DXDI_CDIR_ENC) {
		direction = SEP_CRYPTO_DIRECTION_ENCRYPT;
	} else if (props->direction == DXDI_CDIR_DEC) {
		direction = SEP_CRYPTO_DIRECTION_DECRYPT;
	} else {
		pr_err("Invalid direction=%d\n", props->direction);
		*error_info = DXDI_ERROR_INVAL_DIRECTION;
		return -EINVAL;
	}

	/* initialize SEP context */
	aead_ctx_p = (struct sep_ctx_aead *)&(host_ctx_p->sep_ctx);
	aead_ctx_p->alg = cpu_to_le32(SEP_CRYPTO_ALG_AEAD);
	aead_ctx_p->direction = cpu_to_le32(direction);
	aead_ctx_p->header_size = cpu_to_le32(props->adata_size);
	if (props->nonce_size > SEP_AES_BLOCK_SIZE) {
		pr_err("Invalid nonce size=%u\n", aead_ctx_p->nonce_size);
		*error_info = DXDI_ERROR_INVAL_NONCE_SIZE;
		return -EINVAL;
	}
	aead_ctx_p->nonce_size = cpu_to_le32(props->nonce_size);
	memcpy(aead_ctx_p->nonce, props->nonce, props->nonce_size);
	if (props->tag_size > SEP_AES_BLOCK_SIZE) {
		pr_err("Invalid tag_size size=%u\n", aead_ctx_p->tag_size);
		*error_info = DXDI_ERROR_INVAL_TAG_SIZE;
		return -EINVAL;
	}
	aead_ctx_p->tag_size = cpu_to_le32(props->tag_size);
	aead_ctx_p->text_size = cpu_to_le32(props->text_size);
	if ((props->key_size != 16) &&
	    (props->key_size != 24) && (props->key_size != 32)) {
		pr_err("Invalid key size for AEAD (%u bits)\n",
			    props->key_size * 8);
		*error_info = DXDI_ERROR_INVAL_KEY_SIZE;
		return -EINVAL;
	}
	memcpy(aead_ctx_p->key, props->key, props->key_size);
	aead_ctx_p->key_size = cpu_to_le32(props->key_size);

	/* mode specific initializations */
	switch (props->ae_type) {
	case DXDI_AUTHENC_AES_CCM:
		aead_ctx_p->mode = cpu_to_le32(SEP_CIPHER_CCM);
		break;
	case DXDI_AUTHENC_AES_GCM:
		*error_info = DXDI_ERROR_UNSUP;
		return -ENOSYS;	/* Not supported */
	default:
		*error_info = DXDI_ERROR_UNSUP;
		return -EINVAL;
	}

	return 0;
}

/**
 * ctxmgr_init_mac_ctx() - Initialize context for MAC algorithm
 * @ctx_info:
 * @props:
 * @error_info:	Error info
 *
 * Returns int
 */
int ctxmgr_init_mac_ctx(struct client_crypto_ctx_info *ctx_info,
			struct dxdi_mac_props *props, u32 *error_info)
{
	struct host_crypto_ctx_mac *host_ctx_p =
	    (struct host_crypto_ctx_mac *)ctx_info->ctx_kptr;
	struct sep_ctx_cipher *aes_ctx_p = NULL;
	struct sep_ctx_hmac *hmac_ctx_p = NULL;
	enum dxdi_hash_type hash_type;
	enum sep_hash_mode hash_mode;

#ifdef DEBUG
	if (ctx_info->ctx_kptr == NULL) {
		pr_err("Context not mapped\n");
		SEP_DRIVER_BUG();
	}
#endif

	*error_info = DXDI_ERROR_NULL;

	/* Initialize host context part */
	host_ctx_p->alg_class = ALG_CLASS_MAC;
	host_ctx_p->client_data_size = 0;
	memcpy(&(host_ctx_p->props), props, sizeof(struct dxdi_mac_props));
	host_ctx_p->hmac_tail.size = 0;
	host_ctx_p->is_encrypted = false;

	/* Clear SeP context before setting specific fields */
	/* This helps in case SeP-FW assumes zero on uninitialized fields */
	memset(&(host_ctx_p->sep_ctx), 0, sizeof(host_ctx_p->sep_ctx));

	switch (props->mac_type) {
	case DXDI_MAC_HMAC:
		hmac_ctx_p = (struct sep_ctx_hmac *)&(host_ctx_p->sep_ctx);
		hmac_ctx_p->alg = cpu_to_le32(SEP_CRYPTO_ALG_HMAC);
		hash_type = props->alg_specific.hmac.hash_type;
		hash_mode = get_sep_hash_mode(hash_type);
		if (hash_mode == SEP_HASH_NULL) {
			*error_info = DXDI_ERROR_INVAL_MODE;
			return -EINVAL;
		}
		if (get_hash_block_size(hash_type) > SEP_HMAC_BLOCK_SIZE_MAX) {
			pr_err(
				    "Given hash type (%d) is not supported for HMAC\n",
				    hash_type);
			*error_info = DXDI_ERROR_UNSUP;
			return -EINVAL;
		}
		hmac_ctx_p->mode = cpu_to_le32(hash_mode);
		hmac_ctx_p->k0_size = cpu_to_le32(props->key_size);
		if (props->key_size > SEP_HMAC_BLOCK_SIZE_MAX) {
			pr_err("Invalid key size %u bits\n",
				    props->key_size * 8);
			*error_info = DXDI_ERROR_INVAL_KEY_SIZE;
			return -EINVAL;
		}
		memcpy(&(hmac_ctx_p->k0), props->key, props->key_size);
		break;

	case DXDI_MAC_AES_MAC:
	case DXDI_MAC_AES_CMAC:
	case DXDI_MAC_AES_XCBC_MAC:
		aes_ctx_p = (struct sep_ctx_cipher *)&(host_ctx_p->sep_ctx);
		aes_ctx_p->alg = cpu_to_le32(SEP_CRYPTO_ALG_AES);
		aes_ctx_p->direction =
		    cpu_to_le32(SEP_CRYPTO_DIRECTION_ENCRYPT);
		aes_ctx_p->key_size = cpu_to_le32(props->key_size);
		if ((props->key_size > SEP_AES_KEY_SIZE_MAX) ||
		    ((props->mac_type == DXDI_MAC_AES_XCBC_MAC) &&
		     (props->key_size != SEP_AES_128_BIT_KEY_SIZE))) {
			/* Avoid copying a key too large */
			pr_err("Invalid key size for MAC (%u bits)\n",
				    props->key_size * 8);
			*error_info = DXDI_ERROR_INVAL_KEY_SIZE;
			return -EINVAL;
		}
		memcpy(aes_ctx_p->key, props->key, props->key_size);
		break;

	default:
		*error_info = DXDI_ERROR_UNSUP;
		return -EINVAL;
	}

	/* AES mode specific initializations */
	switch (props->mac_type) {
	case DXDI_MAC_AES_MAC:
		aes_ctx_p->mode = cpu_to_le32(SEP_CIPHER_CBC_MAC);
		memcpy(aes_ctx_p->block_state, props->alg_specific.aes_mac.iv,
		       SEP_AES_IV_SIZE);
		break;
	case DXDI_MAC_AES_CMAC:
		aes_ctx_p->mode = cpu_to_le32(SEP_CIPHER_CMAC);
		break;
	case DXDI_MAC_AES_XCBC_MAC:
		aes_ctx_p->mode = cpu_to_le32(SEP_CIPHER_XCBC_MAC);
		break;
	default:
		/* Invalid type was already handled in previous "switch" */
		break;
	}

	return 0;
}

/**
 * ctxmgr_init_hash_ctx() - Initialize hash context
 * @ctx_info:	 User context mapping info.
 * @hash_type:	 Assigned hash type
 * @error_info:	Error info
 *
 * Returns int 0 on success, -EINVAL, -ENOSYS
 */
int ctxmgr_init_hash_ctx(struct client_crypto_ctx_info *ctx_info,
			 enum dxdi_hash_type hash_type, u32 *error_info)
{
	struct host_crypto_ctx_hash *host_ctx_p =
	    (struct host_crypto_ctx_hash *)ctx_info->ctx_kptr;
	struct sep_ctx_hash *sep_ctx_p;
	enum sep_hash_mode hash_mode;

#ifdef DEBUG
	if (ctx_info->ctx_kptr == NULL) {
		pr_err("Context not mapped\n");
		SEP_DRIVER_BUG();
	}
#endif

	*error_info = DXDI_ERROR_NULL;

	/* Limit to hash types supported by SeP */
	if ((hash_type != DXDI_HASH_SHA1) &&
	    (hash_type != DXDI_HASH_SHA224) &&
	    (hash_type != DXDI_HASH_SHA256)) {
		pr_err("Unsupported hash type %d\n", hash_type);
		*error_info = DXDI_ERROR_UNSUP;
		return -ENOSYS;
	}

	/* Initialize host context part */
	host_ctx_p->alg_class = ALG_CLASS_HASH;
	host_ctx_p->hash_type = hash_type;
	host_ctx_p->hash_tail.size = 0;
	host_ctx_p->is_encrypted = false;

	/* Initialize SeP/FW context part */
	sep_ctx_p = (struct sep_ctx_hash *)&(host_ctx_p->sep_ctx);
	/* Clear SeP context before setting specific fields */
	/* This helps in case SeP-FW assumes zero on uninitialized fields */
	memset(sep_ctx_p, 0, sizeof(struct sep_ctx_hash));
	sep_ctx_p->alg = cpu_to_le32(SEP_CRYPTO_ALG_HASH);
	hash_mode = get_sep_hash_mode(hash_type);
	if (hash_mode == SEP_HASH_NULL) {
		*error_info = DXDI_ERROR_INVAL_MODE;
		return -EINVAL;
	}
	sep_ctx_p->mode = cpu_to_le32(hash_mode);

	return 0;
}

/**
 * ctxmgr_set_sep_cache_idx() - Set the index of this context in the sep_cache
 * @ctx_info:	 User context info structure
 * @sep_cache_idx:	 The allocated index in SeP cache for this context
 *
 * Returns void
 */
void ctxmgr_set_sep_cache_idx(struct client_crypto_ctx_info *ctx_info,
			      int sep_cache_idx)
{
#ifdef DEBUG
	if (ctx_info->ctx_kptr == NULL) {
		pr_err("Context not mapped\n");
		SEP_DRIVER_BUG();
	}
#endif
	ctx_info->sep_cache_idx = sep_cache_idx;
}

/**
 * ctxmgr_get_sep_cache_idx() - Get the index of this context in the sep_cache
 * @ctx_info:	 User context info structure
 *
 * Returns The allocated index in SeP cache for this context
 */
int ctxmgr_get_sep_cache_idx(struct client_crypto_ctx_info *ctx_info)
{
#ifdef DEBUG
	if (ctx_info->ctx_kptr == NULL) {
		pr_err("Context not mapped\n");
		SEP_DRIVER_BUG();
	}
#endif
	return ctx_info->sep_cache_idx;
}

#ifdef DEBUG
static void dump_sep_aes_ctx(struct sep_ctx_cipher *ctx_p)
{
	pr_debug("Alg.=AES , Mode=%d , Direction=%d , Key size=%d\n",
		      le32_to_cpu(ctx_p->mode),
		      le32_to_cpu(ctx_p->direction),
		      le32_to_cpu(ctx_p->key_size));
	dump_byte_array("Key", ctx_p->key, le32_to_cpu(ctx_p->key_size));
	dump_byte_array("block_state",
			ctx_p->block_state, sizeof(ctx_p->block_state));
	if (le32_to_cpu(ctx_p->mode) == SEP_CIPHER_XTS) {
		pr_debug("data_unit_size=%u\n",
			      le32_to_cpu(ctx_p->data_unit_size));
		dump_byte_array("XEX-Key",
				ctx_p->xex_key, le32_to_cpu(ctx_p->key_size));
	}
}

static void dump_sep_aead_ctx(struct sep_ctx_aead *ctx_p)
{
	pr_debug(
		      "Alg.=AEAD, Mode=%d, Direction=%d, Key size=%d, header size=%d, nonce size=%d, tag size=%d, text size=%d\n",
		      le32_to_cpu(ctx_p->mode),
		      le32_to_cpu(ctx_p->direction),
		      le32_to_cpu(ctx_p->key_size),
		      le32_to_cpu(ctx_p->header_size),
		      le32_to_cpu(ctx_p->nonce_size),
		      le32_to_cpu(ctx_p->tag_size),
		      le32_to_cpu(ctx_p->text_size));
	dump_byte_array("Key", ctx_p->key, le32_to_cpu(ctx_p->key_size));
	dump_byte_array("block_state",
			ctx_p->block_state, sizeof(ctx_p->block_state));
	dump_byte_array("mac_state",
			ctx_p->mac_state, sizeof(ctx_p->mac_state));
	dump_byte_array("nonce", ctx_p->nonce, le32_to_cpu(ctx_p->nonce_size));
}

static void dump_sep_des_ctx(struct sep_ctx_cipher *ctx_p)
{
	pr_debug("Alg.=DES, Mode=%d, Direction=%d, Key size=%d\n",
		      le32_to_cpu(ctx_p->mode),
		      le32_to_cpu(ctx_p->direction),
		      le32_to_cpu(ctx_p->key_size));
	dump_byte_array("Key", ctx_p->key, le32_to_cpu(ctx_p->key_size));
	dump_byte_array("IV", ctx_p->block_state, SEP_DES_IV_SIZE);
}

static void dump_sep_c2_ctx(struct sep_ctx_c2 *ctx_p)
{
	pr_debug("Alg.=C2, Mode=%d, Direction=%d, KeySz=%d, ResetInt.=%d",
		      le32_to_cpu(ctx_p->mode),
		      le32_to_cpu(ctx_p->direction),
		      le32_to_cpu(ctx_p->key_size),
		      0 /* reset_interval (CBC) not implemented, yet */);
	dump_byte_array("Key", ctx_p->key, le32_to_cpu(ctx_p->key_size));
}

static const char *hash_mode_str(enum sep_hash_mode mode)
{
	switch (mode) {
	case SEP_HASH_SHA1:
		return "SHA1";
	case SEP_HASH_SHA224:
		return "SHA224";
	case SEP_HASH_SHA256:
		return "SHA256";
	case SEP_HASH_SHA384:
		return "SHA384";
	case SEP_HASH_SHA512:
		return "SHA512";
	default:
		return "(unknown)";
	}
}

static void dump_sep_hash_ctx(struct sep_ctx_hash *ctx_p)
{
	pr_debug("Alg.=Hash , Mode=%s\n",
		      hash_mode_str(le32_to_cpu(ctx_p->mode)));
}

static void dump_sep_hmac_ctx(struct sep_ctx_hmac *ctx_p)
{
	/* Alg./Mode of HMAC is identical to HASH */
	pr_debug("Alg.=HMAC , Mode=%s\n",
		      hash_mode_str(le32_to_cpu(ctx_p->mode)));
	pr_debug("K0 size = %u B\n", le32_to_cpu(ctx_p->k0_size));
	dump_byte_array("K0", ctx_p->k0, le32_to_cpu(ctx_p->k0_size));
}

/**
 * ctxmgr_dump_sep_ctx() - Dump SeP context data
 * @ctx_info:
 *
 */
void ctxmgr_dump_sep_ctx(const struct client_crypto_ctx_info *ctx_info)
{
	struct sep_ctx_cache_entry *sep_ctx_p = ctx_info->sep_ctx_kptr;
	enum sep_crypto_alg alg;
	int ctx_idx;

	/* For combined mode call recursively for each sub-context */
	if (ctx_info->ctx_kptr->alg_class == ALG_CLASS_COMBINED) {
		for (ctx_idx = 0; (ctx_info[ctx_idx].ctx_kptr != NULL) &&
		     (ctx_idx < DXDI_COMBINED_NODES_MAX); ctx_idx++) {
			ctxmgr_dump_sep_ctx(&ctx_info[ctx_idx]);
		}
		return;
	}

	alg = (enum sep_crypto_alg)le32_to_cpu(sep_ctx_p->alg);

	pr_debug("SeP crypto context at %p: Algorithm=%d\n",
		      sep_ctx_p, alg);
	switch (alg) {
	case SEP_CRYPTO_ALG_NULL:
		break;		/* Nothing to dump */
	case SEP_CRYPTO_ALG_AES:
		dump_sep_aes_ctx((struct sep_ctx_cipher *)sep_ctx_p);
		break;
	case SEP_CRYPTO_ALG_AEAD:
		dump_sep_aead_ctx((struct sep_ctx_aead *)sep_ctx_p);
		break;
	case SEP_CRYPTO_ALG_DES:
		dump_sep_des_ctx((struct sep_ctx_cipher *)sep_ctx_p);
		break;
	case SEP_CRYPTO_ALG_C2:
		dump_sep_c2_ctx((struct sep_ctx_c2 *)sep_ctx_p);
		break;
	case SEP_CRYPTO_ALG_HASH:
		dump_sep_hash_ctx((struct sep_ctx_hash *)sep_ctx_p);
		break;
	case SEP_CRYPTO_ALG_HMAC:
		dump_sep_hmac_ctx((struct sep_ctx_hmac *)sep_ctx_p);
		break;
	default:
		pr_debug("(Unsupported algorithm dump - %d)\n", alg);
	}

}
#endif

/**
 * ctxmgr_sync_sep_ctx() - Sync. SeP context to device (flush from cache...)
 * @ctx_info:	 User context info structure
 * @mydev:	 Associated device context
 *
 * Returns void
 */
void ctxmgr_sync_sep_ctx(const struct client_crypto_ctx_info *ctx_info,
			 struct device *mydev)
{
	size_t embedded_sep_ctx_offset =
	    get_sep_ctx_offset(ctx_info->ctx_kptr->alg_class);
	struct sep_ctx_cache_entry *embedded_sep_ctx_p =
	    (struct sep_ctx_cache_entry *)
	    (((unsigned long)ctx_info->ctx_kptr) + embedded_sep_ctx_offset);

#ifdef DEBUG
	if (ctx_info->sep_ctx_dma_addr == 0) {
		pr_err("Context not mapped\n");
		SEP_DRIVER_BUG();
	}
	if (embedded_sep_ctx_offset == 0)
		pr_err("Invalid alg. class for algorithm\n");
#endif

	/* Only the embedded SeP context requires sync (it is in user memory)
	   Otherwise, it is a cache coherent DMA buffer.                      */
	if (ctx_info->sep_ctx_kptr == embedded_sep_ctx_p) {
		dma_sync_single_for_device(mydev, ctx_info->sep_ctx_dma_addr,
					   SEP_CTX_SIZE, DMA_BIDIRECTIONAL);
	}
}

/**
 * ctxmgr_get_sep_ctx_dma_addr() - Return DMA address of SeP (FW) area of
 *					the context
 * @ctx_info:	 User context info structure
 *
 * Returns DMA address of SeP (FW) area of the context
 */
dma_addr_t ctxmgr_get_sep_ctx_dma_addr(const struct client_crypto_ctx_info
				       *ctx_info)
{
#ifdef DEBUG
	if (ctx_info->sep_ctx_dma_addr == 0) {
		pr_err("Context not mapped\n");
		SEP_DRIVER_BUG();
	}
#endif
	return ctx_info->sep_ctx_dma_addr;
}

/**
 * ctxmgr_sep_cache_create() - Create a SeP (FW) cache manager of given num.
 *				of entries
 * @num_of_entries:	 = Number of entries available in cache
 *
 * Returns void * handle (NULL on failure)
 */
void *ctxmgr_sep_cache_create(int num_of_entries)
{
	struct sep_ctx_cache *new_cache;
	int i;

	/* Allocate the sep_ctx_cache + additional entries beyond the one
	 * that is included in the sep_ctx_cache structure */
	new_cache = kmalloc(sizeof(struct sep_ctx_cache) +
			    (num_of_entries - 1) *
			    sizeof(struct ctxmgr_cache_entry), GFP_KERNEL);
	if (new_cache == NULL) {
		pr_err("Failed allocating SeP cache of %d entries\n",
			    num_of_entries);
		return NULL;
	}

	/* Initialize */
	for (i = 0; i < num_of_entries; i++)
		new_cache->entries[i].ctx_id.addr = CTX_ID_INVALID;

	new_cache->cache_size = num_of_entries;

	new_cache->lru_clk = 0;

	return (void *)new_cache;
}

/**
 * ctxmgr_sep_cache_destroy() - Destory SeP (FW) cache manager object
 * @sep_cache:	 = The cache object
 *
 * Returns void
 */
void ctxmgr_sep_cache_destroy(void *sep_cache)
{
	struct sep_ctx_cache *this_cache = (struct sep_ctx_cache *)sep_cache;

	kfree(this_cache);
}

/**
 * ctxmgr_sep_cache_get_size() - Get cache size (entries count)
 * @sep_cache:
 *
 * Returns int Number of cache entries available
 */
int ctxmgr_sep_cache_get_size(void *sep_cache)
{
	struct sep_ctx_cache *this_cache = sep_cache;

	return this_cache->cache_size;
}

/**
 * ctxmgr_sep_cache_alloc() - Allocate a cache entry of given SeP context cache
 * @sep_cache:	 The cache object
 * @ctx_id:	 The host crypto. context ID
 * @load_required_p:	Pointed int is set to !0 if a cache load is required
 *			(i.e., if item already loaded in cache it would be 0)
 *
 * Returns cache index
 */
int ctxmgr_sep_cache_alloc(void *sep_cache,
			   struct crypto_ctx_uid ctx_id, int *load_required_p)
{
	struct sep_ctx_cache *this_cache = (struct sep_ctx_cache *)sep_cache;
	int i;
	int chosen_idx = 0;	/* first candidate... */

	*load_required_p = 1;	/* until found assume a load is required */

	/* First search for given ID or free/older entry  */
	for (i = 0; i < this_cache->cache_size; i++) {
		if (this_cache->entries[i].ctx_id.addr == ctx_id.addr
		    && this_cache->entries[i].ctx_id.cntr == ctx_id.cntr) {
			/* if found */
			chosen_idx = i;
			*load_required_p = 0;
			break;
		}
		/* else... if no free entry, replace candidate with invalid
		   or older entry */
		if (this_cache->entries[chosen_idx].ctx_id.addr
			!= CTX_ID_INVALID) {
			if ((this_cache->entries[i].ctx_id.addr
				== CTX_ID_INVALID) ||
			    (this_cache->entries[chosen_idx].lru_time >
			     this_cache->entries[i].lru_time)) {
				/* Found free OR older entry */
				chosen_idx = i;
			}
		}
	}

	/* Record allocation + update LRU "timestamp" */
	this_cache->entries[chosen_idx].ctx_id.addr = ctx_id.addr;
	this_cache->entries[chosen_idx].ctx_id.cntr = ctx_id.cntr;
	this_cache->entries[chosen_idx].lru_time = this_cache->lru_clk++;

#ifdef DEBUG
	if (this_cache->lru_clk == 0xFFFFFFFF) {
		pr_err("Reached lru_clk limit!\n");
		SEP_DRIVER_BUG();
		/* If this limit is found to be a practical real life
		   case, a few workarounds may be used:
		   1. Use a larger (64b) lru_clk
		   2. Invalidate the whole cache before wrapping to 0
		   3. Ignore this case - old contexts would persist over newer
		   until they are all FINALIZEd and invalidated "manually".
		   4. "shift down" existing timestamps so the lowest would be 0
		   5. "pack" timestamps to be 0,1,2,... based on exisitng order
		   and set lru_clk to the largest (which is the num. of
		   entries)
		 */
	}
#endif

	return chosen_idx;
}

/**
 * ctxmgr_sep_cache_invalidate() - Invalidate cache entry for given context ID
 * @sep_cache:	 The cache object
 * @ctx_id:	 The host crypto. context ID
 * @id_mask:	 A bit mask to be used when comparing the ID
 *                (to be used for a set of entries from the same client)
 *
 * Returns void
 */
void ctxmgr_sep_cache_invalidate(void *sep_cache,
				 struct crypto_ctx_uid ctx_id,
				 u64 id_mask)
{
	struct sep_ctx_cache *this_cache = (struct sep_ctx_cache *)sep_cache;
	int i;

	/* Search for given ID */
	for (i = 0; i < this_cache->cache_size; i++) {
		if ((this_cache->entries[i].ctx_id.addr) == ctx_id.addr) {
			/* When invalidating single, check also counter */
			if (id_mask == CRYPTO_CTX_ID_SINGLE_MASK
			    && this_cache->entries[i].ctx_id.cntr
			       != ctx_id.cntr)
				continue;
			this_cache->entries[i].ctx_id.addr = CTX_ID_INVALID;
		}
	}

}
