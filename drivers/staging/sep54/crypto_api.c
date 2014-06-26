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

/* \file cryto_api.c - Implementation of wrappers for Linux Crypto API */
#define SEP_LOG_CUR_COMPONENT SEP_LOG_MASK_CRYPTO_API

#include <linux/module.h>
#include <linux/crypto.h>
#include <crypto/algapi.h>
#include <crypto/hash.h>
#include <crypto/internal/hash.h>
#include <crypto/sha.h>
#include <linux/workqueue.h>
#include "dx_driver_abi.h"
#include "dx_driver.h"
#include "sep_power.h"
#include "crypto_ctx_mgr.h"
#include "crypto_api.h"
#include "sepapp.h"
#include "sep_applets.h"
#include "dx_sepapp_kapi.h"

#include <linux/sched.h>

#define CRYPTO_API_QID 0
/* Priority assigned to our algorithms implementation */
#define DX_CRYPTO_PRIO (300 + (100 * CRYPTO_API_QID))

#define SYMCIPHER_ALG_NAME_LEN 8	/* Format: "mod(alg)" */
#define SYMCIPHER_ALG_NAME_MODE_OFFSET 0
#define SYMCIPHER_ALG_NAME_MODE_SIZE 3
#define SYMCIPHER_ALG_NAME_ALG_OFFSET 4
#define SYMCIPHER_ALG_NAME_ALG_SIZE 3

#define CMD_DO_CRYPTO 7
#define DISK_ENC_APP_UUID "INTEL DISK ENC01"

/**
 * struct async_digest_req_ctx - Context for async. digest algorithms requests
 * @host_ctx:	Host crypto context allocated per request
 * @result:	Where to copy the digest result.
 *		When NULL the result is retained in the sep_ctx until "final"
 *		and this field holds the pointer to its location.
 * @async_req:	The generic async request context for completion notification
 */
struct async_digest_req_ctx {
	union {
		struct host_crypto_ctx_hash hash_ctx;
		struct host_crypto_ctx_mac mac_ctx;
	} host_ctx;
	u8 *result;
	struct async_req_ctx async_req;
};

/* Client context for the Crypto API operations */
/* To be initialized by sep_setup */
static struct sep_client_ctx crypto_api_ctx;
static struct dma_pool *sep_ctx_pool;

/* Functions from the main driver code that are shared with this module */
int prepare_data_for_sep(struct sep_op_ctx *op_ctx,
			 u8 __user *data_in,
			 struct scatterlist *sgl_in,
			 u8 __user *data_out,
			 struct scatterlist *sgl_out,
			 u32 data_in_size,
			 enum crypto_data_intent data_intent);

/* Local (static) functions */
static void release_symcipher_ctx(struct sep_op_ctx *op_ctx,
				  u8 *iv_crypto);

/****************************************/
/* Block cipher algorithms declarations */
/****************************************/
static int symcipher_set_key(struct crypto_ablkcipher *tfm,
			     const u8 *key, unsigned int keylen);
static int symcipher_encrypt(struct ablkcipher_request *req);
static int symcipher_decrypt(struct ablkcipher_request *req);
static int symcipher_ctx_init(struct crypto_tfm *tfm);
static void crypto_ctx_cleanup(struct crypto_tfm *tfm);

/* Template for block ciphers */
static struct crypto_alg blkcipher_algs_base = {
	.cra_priority = DX_CRYPTO_PRIO,
	.cra_flags = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_ctxsize = sizeof(struct host_crypto_ctx_sym_cipher),
	.cra_alignmask = 0,	/* Cannot use this due to bug in kernel */
	.cra_type = &crypto_ablkcipher_type,
	.cra_module = THIS_MODULE,
	.cra_u = {
		  .ablkcipher = {
				 .setkey = symcipher_set_key,
				 .encrypt = symcipher_encrypt,
				 .decrypt = symcipher_decrypt}
		  },
	.cra_init = symcipher_ctx_init,
	.cra_exit = crypto_ctx_cleanup
};

/* Block cipher specific attributes */
static struct crypto_alg dx_ablkcipher_algs[] = {
	{			/* xxx(aes) */
	 .cra_name = "xxx(aes)",
	 .cra_driver_name = MODULE_NAME "-aes-xxx",
	 .cra_blocksize = SEP_AES_BLOCK_SIZE,
	 .cra_u = {
		   .ablkcipher = {
				  .min_keysize = SEP_AES_128_BIT_KEY_SIZE,
				  .max_keysize = SEP_AES_256_BIT_KEY_SIZE,
				  .ivsize = SEP_AES_IV_SIZE}
		   }
	 },
#ifdef USE_SEP54_AES
	{			/* ecb(aes) */
	 .cra_name = "ecb(aes)",
	 .cra_driver_name = MODULE_NAME "-aes-ecb",
	 .cra_blocksize = SEP_AES_BLOCK_SIZE,
	 .cra_u = {
		   .ablkcipher = {
				  .min_keysize = SEP_AES_128_BIT_KEY_SIZE,
				  .max_keysize = SEP_AES_256_BIT_KEY_SIZE,
				  .ivsize = SEP_AES_IV_SIZE}
		   }
	 },
	{			/* cbc(aes) */
	 .cra_name = "cbc(aes)",
	 .cra_driver_name = MODULE_NAME "-aes-cbc",
	 .cra_blocksize = SEP_AES_BLOCK_SIZE,
	 .cra_u = {
		   .ablkcipher = {
				  .min_keysize = SEP_AES_128_BIT_KEY_SIZE,
				  .max_keysize = SEP_AES_256_BIT_KEY_SIZE,
				  .ivsize = SEP_AES_IV_SIZE}
		   }
	 },
	{			/* ctr(aes) */
	 .cra_name = "ctr(aes)",
	 .cra_driver_name = MODULE_NAME "-aes-ctr",
	 .cra_blocksize = SEP_AES_BLOCK_SIZE,
	 .cra_u = {
		   .ablkcipher = {
				  .min_keysize = SEP_AES_128_BIT_KEY_SIZE,
				  .max_keysize = SEP_AES_256_BIT_KEY_SIZE,
				  .ivsize = SEP_AES_IV_SIZE}
		   }
	 },
	{			/* xts(aes) */
	 .cra_name = "xts(aes)",
	 .cra_driver_name = MODULE_NAME "-aes-xts",
	 .cra_blocksize = SEP_AES_BLOCK_SIZE,
	 .cra_u = {
			/* AES-XTS uses two keys, so the key size is doubled */
		   .ablkcipher = {
				  .min_keysize = SEP_AES_128_BIT_KEY_SIZE * 2,
				  .max_keysize = SEP_AES_256_BIT_KEY_SIZE * 2,
				  .ivsize = SEP_AES_IV_SIZE}
		   }
	 },
#endif /* USE_SEP54_AES */
	{			/* ecb(des) */
	 .cra_name = "ecb(des)",
	 .cra_driver_name = MODULE_NAME "-des-ecb",
	 .cra_blocksize = SEP_DES_BLOCK_SIZE,
	 .cra_u = {
		   .ablkcipher = {
				  .min_keysize = SEP_DES_ONE_KEY_SIZE,
				  .max_keysize = SEP_DES_ONE_KEY_SIZE,
				  .ivsize = SEP_DES_IV_SIZE}
		   }
	 },
	{			/* cbc(des) */
	 .cra_name = "cbc(des)",
	 .cra_driver_name = MODULE_NAME "-des-cbc",
	 .cra_blocksize = SEP_DES_BLOCK_SIZE,
	 .cra_u = {
		   .ablkcipher = {
				  .min_keysize = SEP_DES_ONE_KEY_SIZE,
				  .max_keysize = SEP_DES_ONE_KEY_SIZE,
				  .ivsize = SEP_DES_IV_SIZE}
		   }
	 },
	{			/* ecb(des3_ede) */
	 .cra_name = "ecb(des3_ede)",
	 .cra_driver_name = MODULE_NAME "-des3-ecb",
	 .cra_blocksize = SEP_DES_BLOCK_SIZE,
	 .cra_u = {
		   .ablkcipher = {
				  .min_keysize = SEP_DES_TRIPLE_KEY_SIZE,
				  .max_keysize = SEP_DES_TRIPLE_KEY_SIZE,
				  .ivsize = SEP_DES_IV_SIZE}
		   }
	 },
	{			/* cbc(des3_ede) */
	 .cra_name = "cbc(des3_ede)",
	 .cra_driver_name = MODULE_NAME "-des3-cbc",
	 .cra_blocksize = SEP_DES_BLOCK_SIZE,
	 .cra_u = {
		   .ablkcipher = {
				  .min_keysize = SEP_DES_TRIPLE_KEY_SIZE,
				  .max_keysize = SEP_DES_TRIPLE_KEY_SIZE,
				  .ivsize = SEP_DES_IV_SIZE}
		   }
	 }
};				/* ablkcipher_algs[] */

#define DX_ABLKCIPHER_NUM \
	(sizeof(dx_ablkcipher_algs) / sizeof(struct crypto_alg))

static const enum dxdi_sym_cipher_type dx_algs_cipher_types[] = {
	DXDI_SYMCIPHER_AES_XXX,
#ifdef USE_SEP54_AES
	DXDI_SYMCIPHER_AES_ECB,
	DXDI_SYMCIPHER_AES_CBC,
	DXDI_SYMCIPHER_AES_CTR,
	DXDI_SYMCIPHER_AES_XTS,
#endif
	DXDI_SYMCIPHER_DES_ECB,
	DXDI_SYMCIPHER_DES_CBC,
	DXDI_SYMCIPHER_DES_ECB,
	DXDI_SYMCIPHER_DES_CBC,
};

/*********************************************/
/* Digest (hash/MAC) algorithms declarations */
/*********************************************/
static int digest_tfm_init(struct crypto_tfm *tfm);
static int digest_init(struct ahash_request *req);
static int digest_update(struct ahash_request *req);
static int digest_final(struct ahash_request *req);
static int digest_finup(struct ahash_request *req);
static int digest_integrated(struct ahash_request *req);
static int mac_setkey(struct crypto_ahash *tfm,
		      const u8 *key, unsigned int keylen);

/* Save set key in tfm ctx */
struct mac_key_data {
	u32 key_size;	/* In octets */
	u8 key[DXDI_MAC_KEY_SIZE_MAX];
};

/* Description of a digest (hash/MAC) algorithm */
struct dx_digest_alg {
	enum dxdi_hash_type hash_type;
	enum dxdi_mac_type mac_type;
	struct ahash_alg ahash;
};

/* Common attributes for all the digest (hash/MAC) algorithms */
static struct ahash_alg digest_algs_base = {
	.init = digest_init,
	.update = digest_update,
	.final = digest_final,
	.finup = digest_finup,
	.digest = digest_integrated,
	.halg.base = {
		      .cra_type = &crypto_ahash_type,
		      .cra_priority = DX_CRYPTO_PRIO,
		      .cra_flags = CRYPTO_ALG_TYPE_AHASH | CRYPTO_ALG_ASYNC,
		      .cra_alignmask = 0,
		      .cra_module = THIS_MODULE,
		      .cra_init = digest_tfm_init}
};

/* Algorithm specific attributes */
static struct dx_digest_alg dx_digest_algs[] = {
#ifdef USE_SEP54_AHASH
	{			/* sha1 */
	 .hash_type = DXDI_HASH_SHA1,
	 .mac_type = DXDI_MAC_NONE,
	 .ahash = {
		   .halg.base = {
				 .cra_name = "sha1",
				 .cra_driver_name = MODULE_NAME "-sha1",
				 .cra_blocksize = SHA1_BLOCK_SIZE},
		   .halg.digestsize = SHA1_DIGEST_SIZE,
		   .halg.statesize = SHA1_BLOCK_SIZE}
	 },
	{			/* sha224 */
	 .hash_type = DXDI_HASH_SHA224,
	 .mac_type = DXDI_MAC_NONE,
	 .ahash = {
		   .halg.base = {
				 .cra_name = "sha224",
				 .cra_driver_name = MODULE_NAME "-sha224",
				 .cra_blocksize = SHA224_BLOCK_SIZE},
		   .halg.digestsize = SHA224_DIGEST_SIZE,
		   .halg.statesize = SHA224_BLOCK_SIZE}
	 },
	{			/* sha256 */
	 .hash_type = DXDI_HASH_SHA256,
	 .mac_type = DXDI_MAC_NONE,
	 .ahash = {
		   .halg.base = {
				 .cra_name = "sha256",
				 .cra_driver_name = MODULE_NAME "-sha256",
				 .cra_blocksize = SHA256_BLOCK_SIZE},
		   .halg.digestsize = SHA256_DIGEST_SIZE,
		   .halg.statesize = SHA256_BLOCK_SIZE}
	 },
	{			/* hmac(sha1) */
	 .hash_type = DXDI_HASH_SHA1,
	 .mac_type = DXDI_MAC_HMAC,
	 .ahash = {
		   .setkey = mac_setkey,
		   .halg.base = {
				 .cra_name = "hmac(sha1)",
				 .cra_driver_name = MODULE_NAME "-hmac-sha1",
				 .cra_blocksize = SHA1_BLOCK_SIZE,
				 .cra_ctxsize = sizeof(struct mac_key_data)
				 },
		   .halg.digestsize = SHA1_DIGEST_SIZE,
		   .halg.statesize = SHA1_BLOCK_SIZE}
	 },
	{			/* hmac(sha224) */
	 .hash_type = DXDI_HASH_SHA224,
	 .mac_type = DXDI_MAC_HMAC,
	 .ahash = {
		   .setkey = mac_setkey,
		   .halg.base = {
				 .cra_name = "hmac(sha224)",
				 .cra_driver_name = MODULE_NAME "-hmac-sha224",
				 .cra_blocksize = SHA224_BLOCK_SIZE,
				 .cra_ctxsize = sizeof(struct mac_key_data)
				 },
		   .halg.digestsize = SHA224_DIGEST_SIZE,
		   .halg.statesize = SHA224_BLOCK_SIZE}
	 },
	{			/* hmac(sha256) */
	 .hash_type = DXDI_HASH_SHA256,
	 .mac_type = DXDI_MAC_HMAC,
	 .ahash = {
		   .setkey = mac_setkey,
		   .halg.base = {
				 .cra_name = "hmac(sha256)",
				 .cra_driver_name = MODULE_NAME "-hmac-sha256",
				 .cra_blocksize = SHA256_BLOCK_SIZE,
				 .cra_ctxsize = sizeof(struct mac_key_data)
				 },
		   .halg.digestsize = SHA256_DIGEST_SIZE,
		   .halg.statesize = SHA256_BLOCK_SIZE}
	 },
#ifdef USE_SEP54_AES
	{			/* xcbc(aes) */
	 .hash_type = DXDI_HASH_NONE,
	 .mac_type = DXDI_MAC_AES_XCBC_MAC,
	 .ahash = {
		   .setkey = mac_setkey,
		   .halg.base = {
				 .cra_name = "xcbc(aes)",
				 .cra_driver_name = MODULE_NAME "-aes-xcbc",
				 .cra_blocksize = SEP_AES_BLOCK_SIZE,
				 .cra_ctxsize = sizeof(struct mac_key_data)
				 },
		   .halg.digestsize = SEP_AES_BLOCK_SIZE,
		   .halg.statesize = SEP_AES_BLOCK_SIZE}
	 },
	{			/* cmac(aes) */
	 .hash_type = DXDI_HASH_NONE,
	 .mac_type = DXDI_MAC_AES_CMAC,
	 .ahash = {
		   .setkey = mac_setkey,
		   .halg.base = {
				 .cra_name = "cmac(aes)",
				 .cra_driver_name = MODULE_NAME "-aes-cmac",
				 .cra_blocksize = SEP_AES_BLOCK_SIZE,
				 .cra_ctxsize = sizeof(struct mac_key_data)
				 },
		   .halg.digestsize = SEP_AES_BLOCK_SIZE,
		   .halg.statesize = SEP_AES_BLOCK_SIZE}
	 }
#endif /* USE_SEP54_AES */
#endif /* USE_SEP54_AHASH */
};				/*dx_ahash_algs[] */

#define DX_DIGEST_NUM \
	(sizeof(dx_digest_algs) / sizeof(struct dx_digest_alg))

static void crypto_ctx_cleanup(struct crypto_tfm *tfm)
{
	struct host_crypto_ctx *host_ctx_p = crypto_tfm_ctx(tfm);
	struct device *mydev = crypto_api_ctx.drv_data->sep_data->dev;
	struct client_crypto_ctx_info _ctx_info;
	struct client_crypto_ctx_info *ctx_info = &_ctx_info;
	int rc;

	pr_debug("Cleaning context @%p for %s\n",
		      host_ctx_p, crypto_tfm_alg_name(tfm));

	rc = ctxmgr_map_kernel_ctx(ctx_info, mydev, host_ctx_p->alg_class,
				   (struct host_crypto_ctx *)host_ctx_p, NULL,
				   0);
	if (rc != 0) {
		pr_err("Failed mapping context @%p (rc=%d)\n",
			    host_ctx_p, rc);
		return;
	}

	/* New TEE method */
	if (!memcmp(crypto_tfm_alg_name(tfm), "xxx(aes)", 8)) {
		if (dx_sepapp_session_close(host_ctx_p->sctx,
						host_ctx_p->sess_id))
			BUG(); /* TODO */
		dx_sepapp_context_free(host_ctx_p->sctx);
	}

	ctxmgr_set_ctx_state(ctx_info, CTX_STATE_UNINITIALIZED);

	ctxmgr_unmap_kernel_ctx(ctx_info);
}

/**
 * dispatch_crypto_op() - Dispatch (async.) CRYPTO_OP descriptor operation
 * @op_ctx:		Operation context
 * @may_backlog:	If software queue is full, may be put in backlog queue
 * @do_init:		Initialize given crypto context
 * @proc_mode:		Processing mode code
 * @keep_in_cache:	Retain crypto context in cache after dispatching req.
 *
 * Returns -EINPROGRESS on success to dispatch into the SW desc. q.
 * Returns -EBUSY if may_backlog==true and the descriptor was enqueued in the
 * the backlog queue.
 * Returns -ENOMEM if queue is full and cannot enqueue in the backlog queue
 */
static int dispatch_crypto_op(struct sep_op_ctx *op_ctx, bool may_backlog,
			      bool do_init, enum sep_proc_mode proc_mode,
			      bool keep_in_cache)
{
	struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;
	struct client_crypto_ctx_info *ctx_info = &op_ctx->ctx_info;
	int sep_ctx_load_req;
	struct crypto_ctx_uid ctx_id = ctxmgr_get_ctx_id(ctx_info);
	int rc;
	struct sep_sw_desc desc;

	/* Start critical section -
	   cache allocation must be coupled to descriptor enqueue */
	mutex_lock(&drvdata->desc_queue_sequencer);
#ifdef SEP_RUNTIME_PM
	dx_sep_pm_runtime_get();
#endif
	ctxmgr_set_sep_cache_idx(ctx_info,
				 ctxmgr_sep_cache_alloc(drvdata->sep_cache,
							ctx_id,
							&sep_ctx_load_req));
	desc_q_pack_crypto_op_desc(&desc, op_ctx, sep_ctx_load_req, do_init,
				   proc_mode);
	/* op_state must be updated before dispatching descriptor */
	rc = desc_q_enqueue(drvdata->desc_queue, &desc, may_backlog);
	if (unlikely(IS_DESCQ_ENQUEUE_ERR(rc)))
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
	if ((!keep_in_cache) || unlikely(IS_DESCQ_ENQUEUE_ERR(rc)))
		ctxmgr_sep_cache_invalidate(drvdata->sep_cache, ctx_id,
					    CRYPTO_CTX_ID_SINGLE_MASK);
#ifdef SEP_RUNTIME_PM
	dx_sep_pm_runtime_put();
#endif
	mutex_unlock(&drvdata->desc_queue_sequencer);
	return rc;
}

/**
 * process_digest_fin() - Process finilization event for hash operation
 *
 * @digest_req:	The async. digest request context
 *
 */
static void process_digest_fin(struct async_digest_req_ctx *digest_req)
{
	u8 digest_size;
	u8 *digest_ptr;
	struct sep_op_ctx *op_ctx = &digest_req->async_req.op_ctx;
#ifdef DEBUG
	struct crypto_ahash *ahash_tfm;
#endif

	if (op_ctx->op_type == SEP_OP_CRYPTO_FINI) {
		/* Handle digest copy back to "result" */
		digest_size =
		    ctxmgr_get_digest_or_mac_ptr(&op_ctx->ctx_info,
						 &digest_ptr);
		if (unlikely(digest_ptr == NULL)) {
			pr_err("Failed fetching digest/MAC\n");
			return;
		}
		if (digest_req->result != NULL)
			memcpy(digest_req->result, digest_ptr, digest_size);
		else	/* Save pointer to result (to be copied on "final") */
			digest_req->result = digest_ptr;
#ifdef DEBUG
		dump_byte_array("digest", digest_req->result, digest_size);
		ahash_tfm =
		    crypto_ahash_reqtfm(container_of
					(digest_req->async_req.initiating_req,
					 struct ahash_request, base));
		if (digest_size != crypto_ahash_digestsize(ahash_tfm))
			pr_err("Read digest of %u B. Expected %u B.\n",
				    digest_size,
				    crypto_ahash_digestsize(ahash_tfm));
#endif
	}
	crypto_op_completion_cleanup(op_ctx);
	ctxmgr_unmap_kernel_ctx(&op_ctx->ctx_info);
}

static void dx_crypto_api_handle_op_completion(struct work_struct *work)
{
	struct async_req_ctx *areq_ctx =
	    container_of(work, struct async_req_ctx, comp_work);
	struct sep_op_ctx *op_ctx = &areq_ctx->op_ctx;
	struct ablkcipher_request *ablkcipher_req;
	struct crypto_async_request *initiating_req = areq_ctx->initiating_req;
	int err = 0;
	u8 *req_info_p;/* For state persistency in caller's context (IV) */

	pr_debug("req=%p op_ctx=%p\n", initiating_req, op_ctx);
	if (op_ctx == NULL) {
		pr_err("Invalid work context (%p)\n", work);
		return;
	}

	if (op_ctx->op_state == USER_OP_COMPLETED) {

		if (unlikely(op_ctx->error_info != 0)) {
			pr_err("SeP crypto-op failed (sep_rc=0x%08X)\n",
				    op_ctx->error_info);
		}
		switch (crypto_tfm_alg_type(initiating_req->tfm)) {
		case CRYPTO_ALG_TYPE_ABLKCIPHER:
			/* Resolve to "info" (IV, etc.) for given alg_type */
			crypto_op_completion_cleanup(op_ctx);
			ablkcipher_req = (struct ablkcipher_request *)
			    container_of(initiating_req,
					 struct ablkcipher_request, base);
			req_info_p = ablkcipher_req->info;
			release_symcipher_ctx(op_ctx, req_info_p);
			break;
		case CRYPTO_ALG_TYPE_AHASH:
			process_digest_fin(container_of(areq_ctx,
					struct async_digest_req_ctx,
					async_req));
			break;
		default:
			pr_err("Unsupported alg_type (%d)\n",
				    crypto_tfm_alg_type(initiating_req->tfm));
		}
		/* Save ret_code info before cleaning op_ctx */
		err = -(op_ctx->error_info);
		if (unlikely(err == -EINPROGRESS)) {
			/* SeP error code collides with EINPROGRESS */
			pr_err("Invalid SeP error code 0x%08X\n",
				    op_ctx->error_info);
			err = -EINVAL;	/* fallback */
		}
		op_ctx_fini(op_ctx);
	} else if (op_ctx->op_state == USER_OP_INPROC) {
		/* Report with the callback the dispatch from backlog to
		   the actual processing in the SW descriptors queue
		   (Returned -EBUSY when the request was dispatched) */
		err = -EINPROGRESS;
	} else {
		pr_err("Invalid state (%d) for op_ctx %p\n",
			    op_ctx->op_state, op_ctx);
		BUG();
	}
	if (likely(initiating_req->complete != NULL))
		initiating_req->complete(initiating_req, err);
	else
		pr_err("Async. operation has no completion callback.\n");
}

/****************************************************/
/* Block cipher algorithms                          */
/****************************************************/

/**
 * get_symcipher_tfm_cipher_type() - Get cipher type of given symcipher
 *					transform
 * @tfm:
 *
 * Returns enum dxdi_sym_cipher_type (DXDI_SYMCIPHER_NONE for invalid)
 */
static enum dxdi_sym_cipher_type get_symcipher_tfm_cipher_type(struct crypto_tfm
							       *tfm)
{
	const int alg_index = tfm->__crt_alg - dx_ablkcipher_algs;

	if ((alg_index < 0) || (alg_index >= DX_ABLKCIPHER_NUM)) {
		pr_err("Unknown alg: %s\n", crypto_tfm_alg_name(tfm));
		return DXDI_SYMCIPHER_NONE;
	}

	return dx_algs_cipher_types[alg_index];
}

static int symcipher_ctx_init(struct crypto_tfm *tfm)
{
	struct ablkcipher_tfm *ablktfm = &tfm->crt_ablkcipher;
	struct host_crypto_ctx_sym_cipher *host_ctx_p = crypto_tfm_ctx(tfm);
	struct device *mydev = crypto_api_ctx.drv_data->sep_data->dev;
	struct client_crypto_ctx_info _ctx_info;
	struct client_crypto_ctx_info *ctx_info = &_ctx_info;
	enum dxdi_sym_cipher_type cipher_type =
	    get_symcipher_tfm_cipher_type(tfm);
	int rc;

	pr_debug("Initializing context @%p for %s (%d)\n",
		      host_ctx_p, crypto_tfm_alg_name(tfm), cipher_type);
#ifdef SEP_RUNTIME_PM
	dx_sep_pm_runtime_get();
#endif
	ablktfm->reqsize += sizeof(struct async_req_ctx);
	rc = ctxmgr_map_kernel_ctx(ctx_info, mydev, ALG_CLASS_SYM_CIPHER,
				   (struct host_crypto_ctx *)host_ctx_p, NULL,
				   0);
	if (rc != 0) {
		pr_err("Failed mapping context (rc=%d)\n", rc);
#ifdef SEP_RUNTIME_PM
		dx_sep_pm_runtime_put();
#endif
		return rc;
	}
	/* Allocate a new Crypto context ID */
	ctxmgr_set_ctx_id(ctx_info, alloc_crypto_ctx_id(&crypto_api_ctx));
	rc = ctxmgr_init_symcipher_ctx_no_props(ctx_info, cipher_type);
	if (unlikely(rc != 0)) {
		pr_err("Failed initializing context\n");
		ctxmgr_set_ctx_state(ctx_info, CTX_STATE_UNINITIALIZED);
	} else {
		ctxmgr_set_ctx_state(ctx_info, CTX_STATE_PARTIAL_INIT);
	}
	ctxmgr_unmap_kernel_ctx(ctx_info);

	/* New TEE method */
	if (!memcmp(crypto_tfm_alg_name(tfm), "xxx(aes)", 8)) {
		u8 uuid[16] = DISK_ENC_APP_UUID;
		enum dxdi_sep_module ret_origin;

		host_ctx_p->sctx = dx_sepapp_context_alloc();
		if (unlikely(!host_ctx_p->sctx)) {
			rc = -ENOMEM;
			goto init_end;
		}

		rc = dx_sepapp_session_open(host_ctx_p->sctx,
				uuid, 0, NULL, NULL, &host_ctx_p->sess_id,
				&ret_origin);
		if (unlikely(rc != 0))
			dx_sepapp_context_free(host_ctx_p->sctx);
	}

init_end:
#ifdef SEP_RUNTIME_PM
	dx_sep_pm_runtime_put();
#endif
	return rc;
}

/**
 * symcipher_set_key() - Set key for given symmetric cipher context
 * @tfm:
 * @key:
 * @keylen:
 *
 * Set key for given symmetric cipher context
 * Setting a key implies initialization of the context
 * Returns int
 */
static int symcipher_set_key(struct crypto_ablkcipher *tfm,
			     const u8 *key, unsigned int keylen)
{
	struct host_crypto_ctx_sym_cipher *host_ctx_p =
	    crypto_tfm_ctx(crypto_ablkcipher_tfm(tfm));
	struct device *mydev = crypto_api_ctx.drv_data->sep_data->dev;
	struct client_crypto_ctx_info _ctx_info;
	struct client_crypto_ctx_info *ctx_info = &_ctx_info;
	enum dxdi_sym_cipher_type cipher_type =
	    get_symcipher_tfm_cipher_type(crypto_ablkcipher_tfm(tfm));
	u32 tfm_flags = crypto_ablkcipher_get_flags(tfm);
	int rc;

	if (cipher_type == DXDI_SYMCIPHER_NONE)
		return -EINVAL;

	if (keylen > DXDI_SYM_KEY_SIZE_MAX) {
		pr_err("keylen=%u > %u\n", keylen, DXDI_SYM_KEY_SIZE_MAX);
		tfm_flags |= CRYPTO_TFM_RES_BAD_KEY_LEN;
		crypto_ablkcipher_set_flags(tfm, tfm_flags);
		return -EINVAL;
	}

	pr_debug("alg=%s (%d) , keylen=%u\n",
		      crypto_tfm_alg_name(crypto_ablkcipher_tfm(tfm)),
		      cipher_type, keylen);

	rc = ctxmgr_map_kernel_ctx(ctx_info, mydev, ALG_CLASS_SYM_CIPHER,
				   (struct host_crypto_ctx *)host_ctx_p, NULL,
				   0);
	if (unlikely(rc != 0)) {
		pr_err("Failed mapping context (rc=%d)\n", rc);
		return rc;
	}

	if (ctxmgr_get_ctx_state(ctx_info) == CTX_STATE_UNINITIALIZED) {
		pr_err("Invoked for uninitialized context @%p\n",
			    host_ctx_p);
		rc = -EINVAL;
	} else {		/* Modify algorithm key */
		rc = ctxmgr_set_symcipher_key(ctx_info, keylen, key);
		if (rc != 0) {
			if (rc == -EINVAL) {
				pr_info("Invalid keylen=%u\n", keylen);
				tfm_flags |= CRYPTO_TFM_RES_BAD_KEY_LEN;
			} else if (rc == -EPERM) {
				pr_info("Invalid/weak key\n");
				tfm_flags |= CRYPTO_TFM_RES_WEAK_KEY;
			} else {
				pr_err("Unknown key setting error (%d)\n",
					    rc);
			}
		}
	}

	ctxmgr_unmap_kernel_ctx(ctx_info);
	crypto_ablkcipher_set_flags(tfm, tfm_flags);
	return rc;
}

/**
 * prepare_symcipher_ctx_for_processing() - Prepare crypto context resources
 *						before dispatching an operation
 *
 * @op_ctx:	The associate operation context (from async req ctx)
 * @host_ctx_p:	The host context to use (from tfm)
 * @iv_crypto:	A pointer to IV (from req->info)
 * @direction:	Requested cipher direction
 */
static int prepare_symcipher_ctx_for_processing(struct sep_op_ctx *op_ctx,
						struct
						host_crypto_ctx_sym_cipher
						*host_ctx_p,
						u8 *iv_crypto,
						enum dxdi_cipher_direction
						direction)
{
	struct client_crypto_ctx_info *ctx_info = &op_ctx->ctx_info;
	struct device *mydev = crypto_api_ctx.drv_data->sep_data->dev;
	struct sep_ctx_cache_entry *sep_ctx_p;
	dma_addr_t sep_ctx_dma_addr;
	int rc;

	sep_ctx_p = dma_pool_alloc(sep_ctx_pool, GFP_KERNEL, &sep_ctx_dma_addr);
	if (sep_ctx_p == NULL) {
		pr_err("Failed allocating SeP context buffer\n");
		return -ENOMEM;
	}
	rc = ctxmgr_map_kernel_ctx(ctx_info, mydev,
				   ALG_CLASS_SYM_CIPHER,
				   (struct host_crypto_ctx *)host_ctx_p,
				   sep_ctx_p, sep_ctx_dma_addr);
	if (unlikely(rc != 0)) {
		pr_err("Failed mapping context (rc=%d)\n", rc);
	} else {
		ctxmgr_set_symcipher_iv(ctx_info, iv_crypto);
		rc = ctxmgr_set_symcipher_direction(ctx_info, direction);
		if (unlikely(rc != 0)) {
			pr_err("Failed setting direction %d (rc=%d)\n",
				    direction, rc);
		}
	}

	if (unlikely(rc != 0)) {
		/* Invalidate context on error */
		ctxmgr_set_ctx_state(ctx_info, CTX_STATE_UNINITIALIZED);
		ctxmgr_unmap_kernel_ctx(ctx_info);
		dma_pool_free(sep_ctx_pool, sep_ctx_p, sep_ctx_dma_addr);
#ifdef DEBUG
	} else {		/* Context was changed by host */
		ctxmgr_dump_sep_ctx(ctx_info);
#endif
		/* No need to dma_sync - sep_ctx is dma coherent mem. */
	}

	return rc;
}

/**
 * release_symcipher_ctx() - Sync. back crypto context (IV) to desc->info)
 *				to be able to track
 * @op_ctx:	The associated operation context (from async req ctx)
 * @iv_crypto:	The Crypto API IV buffer (req->info)
 *
 * Sync. back crypto context (IV) to desc->info) to be able to track
 * IV changes, then unmap the context.
 */
static void release_symcipher_ctx(struct sep_op_ctx *op_ctx,
				  u8 *iv_crypto)
{
	struct client_crypto_ctx_info *ctx_info = &op_ctx->ctx_info;
	u8 iv_sep_ctx[DXDI_AES_BLOCK_SIZE];
	u8 iv_size = DXDI_AES_BLOCK_SIZE;	/* Init. to max. */
	struct sep_ctx_cache_entry *sep_ctx_p = ctx_info->sep_ctx_kptr;
	dma_addr_t sep_ctx_dma_addr = ctx_info->sep_ctx_dma_addr;
	int rc;

	if (iv_crypto != NULL) {	/* Save IV (block state) */
		rc = ctxmgr_get_symcipher_iv(ctx_info, NULL, iv_sep_ctx,
					     &iv_size);
		if (likely(rc == 0)) {
			if (iv_size > 0) {
				if (unlikely(iv_crypto == NULL)) {
					pr_err(
						    "iv_crypto==NULL when iv_size==%u\n",
						    iv_size);
				} else {
					memcpy(iv_crypto, iv_sep_ctx, iv_size);
				}
			}
		} else {
			pr_err("Fail: getting IV information for ctx@%p\n",
				    ctx_info->ctx_kptr);
		}
	}

	ctxmgr_unmap_kernel_ctx(ctx_info);
	dma_pool_free(sep_ctx_pool, sep_ctx_p, sep_ctx_dma_addr);
}

/**
 * symcipher_process() - Process (encrypt/decrypt) data for given block-cipher
 *			algorithm
 * @req:	The async. request structure
 * @direction:	Cipher operation direction
 *
 */
static int symcipher_process(struct ablkcipher_request *req,
			     enum dxdi_cipher_direction direction)
{
	struct async_req_ctx *areq_ctx = ablkcipher_request_ctx(req);
	struct sep_op_ctx *op_ctx = &areq_ctx->op_ctx;
	struct crypto_ablkcipher *tfm = crypto_ablkcipher_reqtfm(req);
	struct host_crypto_ctx_sym_cipher *host_ctx_p =
	    crypto_ablkcipher_ctx(tfm);
	int rc;

	pr_debug("alg=%s %scrypt (req=%p, op_ctx=%p, host_ctx=%p)\n",
		      crypto_tfm_alg_name(crypto_ablkcipher_tfm(tfm)),
		      direction == DXDI_CDIR_ENC ? "en" : "de",
		      req, op_ctx, host_ctx_p);

	/* Initialize async. req. context */
	areq_ctx->initiating_req = &req->base;

	/* Old method */
	if (memcmp(crypto_tfm_alg_name(crypto_ablkcipher_tfm(tfm)),
			"xxx(aes)", 8)) {
		INIT_WORK(&areq_ctx->comp_work,
				dx_crypto_api_handle_op_completion);
		op_ctx_init(op_ctx, &crypto_api_ctx);
		op_ctx->op_type = SEP_OP_CRYPTO_PROC;
		op_ctx->comp_work = &areq_ctx->comp_work;

		rc = prepare_symcipher_ctx_for_processing(op_ctx,
				host_ctx_p, req->info,
				direction);
		if (unlikely(rc != 0)) {
			op_ctx_fini(op_ctx);
			return rc;
		}

		rc = prepare_data_for_sep(op_ctx, NULL, req->src,
					  NULL, req->dst,
					  req->nbytes, CRYPTO_DATA_TEXT);
		if (unlikely(rc != 0)) {
			SEP_LOG_ERR(
				    "Failed preparing DMA buffers (rc=%d, err_info=0x%08X\n)\n",
				    rc, op_ctx->error_info);
			if (op_ctx->error_info == DXDI_ERROR_INVAL_DATA_SIZE) {
				SEP_LOG_ERR("Invalid data unit size %u\n",
						req->nbytes);
				req->base.flags |=
						CRYPTO_TFM_RES_BAD_BLOCK_LEN;
			}
		} else {		/* Initiate processing */
			/* Async. block cipher op. cannot reuse cache entry
			   bacause the IV is set on every operation. Invalidate
			   before releasing the sequencer (that's "async"
			   invalidation) */
			rc = dispatch_crypto_op(op_ctx,
					req->base.
					flags & CRYPTO_TFM_REQ_MAY_BACKLOG,
					true /*init. */ , SEP_PROC_MODE_PROC_T,
					false /*cache */);
		}
		if (unlikely(IS_DESCQ_ENQUEUE_ERR(rc))) { /* Dispatch failure */
			crypto_op_completion_cleanup(op_ctx);
			release_symcipher_ctx(op_ctx, req->info);
			op_ctx_fini(op_ctx);
		}
	} else {
		/* New method vith TEE api */
		struct dxdi_sepapp_kparams *cmd_params =
			kzalloc(sizeof(struct dxdi_sepapp_kparams), GFP_KERNEL);
		enum dxdi_sep_module ret_origin;
		struct scatterlist sg_iv;
		u8 iv[SEP_AES_IV_SIZE];

		if (cmd_params == NULL)
			return -ENOMEM;

		memcpy(iv, req->info, SEP_AES_IV_SIZE);
		sg_init_one(&sg_iv, iv, SEP_AES_IV_SIZE);

		cmd_params->params_types[0] = DXDI_SEPAPP_PARAM_VAL;
		cmd_params->params[0].val.data[0] = direction;
		cmd_params->params[0].val.data[1] = 0;
		cmd_params->params[0].val.copy_dir = DXDI_DATA_TO_DEVICE;

		cmd_params->params_types[1] = DXDI_SEPAPP_PARAM_MEMREF;
		cmd_params->params[1].kmemref.dma_direction =
					DXDI_DATA_TO_DEVICE;
		cmd_params->params[1].kmemref.sgl = &sg_iv;
		cmd_params->params[1].kmemref.nbytes = SEP_AES_IV_SIZE;

		cmd_params->params_types[2] = DXDI_SEPAPP_PARAM_MEMREF;
		cmd_params->params[2].kmemref.dma_direction =
					DXDI_DATA_TO_DEVICE;
		cmd_params->params[2].kmemref.sgl = req->src;
		cmd_params->params[2].kmemref.nbytes = req->nbytes;

		cmd_params->params_types[3] = DXDI_SEPAPP_PARAM_MEMREF;
		cmd_params->params[3].kmemref.dma_direction =
					DXDI_DATA_FROM_DEVICE;
		cmd_params->params[3].kmemref.sgl = req->dst;
		cmd_params->params[3].kmemref.nbytes = req->nbytes;

		rc = async_sepapp_command_invoke(host_ctx_p->sctx,
					host_ctx_p->sess_id, CMD_DO_CRYPTO,
					cmd_params, &ret_origin, areq_ctx);
	}

	return rc;
}

/**
 * symcipher_encrypt() - Encrypt for given sym-cipher context
 * @req: The async. request structure
 *
 */
static int symcipher_encrypt(struct ablkcipher_request *req)
{
	return symcipher_process(req, DXDI_CDIR_ENC);
}

/**
 * symcipher_encrypt() - Decrypt for given sym-cipher context
 * @req: The async. request structure
 *
 */
static int symcipher_decrypt(struct ablkcipher_request *req)
{
	return symcipher_process(req, DXDI_CDIR_DEC);
}

static int ablkcipher_algs_init(void)
{
	int i, rc;
	/* scratchpad to build crypto_alg from template + alg.specific data */
	struct crypto_alg alg_spad;

	/* Create block cipher algorithms from base + specs via scratchpad */
	for (i = 0; i < DX_ABLKCIPHER_NUM; i++) {
		/* Get base template */
		memcpy(&alg_spad, &blkcipher_algs_base,
		       sizeof(struct crypto_alg));
		/* Get alg. specific attributes over base */
		strcpy(alg_spad.cra_name, dx_ablkcipher_algs[i].cra_name);
		strcpy(alg_spad.cra_driver_name,
		       dx_ablkcipher_algs[i].cra_driver_name);
		alg_spad.cra_blocksize = dx_ablkcipher_algs[i].cra_blocksize;
		alg_spad.cra_u.ablkcipher.min_keysize =
		    dx_ablkcipher_algs[i].cra_u.ablkcipher.min_keysize;
		alg_spad.cra_u.ablkcipher.max_keysize =
		    dx_ablkcipher_algs[i].cra_u.ablkcipher.max_keysize;
		alg_spad.cra_u.ablkcipher.ivsize =
		    dx_ablkcipher_algs[i].cra_u.ablkcipher.ivsize;
		/* Copy scratchpad to real entry */
		memcpy(&dx_ablkcipher_algs[i], &alg_spad,
		       sizeof(struct crypto_alg));
		/* The list must be initialized in place (pointers based) */
		INIT_LIST_HEAD(&dx_ablkcipher_algs[i].cra_list);
	}

	/* Register algs */
	pr_debug("Registering CryptoAPI blkciphers:\n");
	for (i = 0, rc = 0; (i < DX_ABLKCIPHER_NUM) && (rc == 0); i++) {
		pr_debug("%d. %s (__crt_alg=%p)\n", i,
			      dx_ablkcipher_algs[i].cra_name,
			      &dx_ablkcipher_algs[i]);
		rc = crypto_register_alg(&dx_ablkcipher_algs[i]);
		if (rc != 0)
			break;
	}
	/* Failure: cleanup algorithms that already registered */
	if (rc != 0) {
		pr_err("Failed registering %s\n",
			    dx_ablkcipher_algs[i].cra_name);
		if (i > 0)
			for (; i >= 0; i--)
				crypto_unregister_alg(
						&dx_ablkcipher_algs[i]);
	}
	return rc;
}

static void ablkcipher_algs_exit(void)
{
	int i;

	for (i = 0; i < DX_ABLKCIPHER_NUM; i++)
		crypto_unregister_alg(&dx_ablkcipher_algs[i]);
}

/****************************************************/
/* Digest (hash/MAC) algorithms                     */
/****************************************************/

static struct dx_digest_alg *get_digest_alg(struct crypto_tfm *tfm)
{
	struct hash_alg_common *halg_common =
	    container_of(tfm->__crt_alg, struct hash_alg_common, base);
	struct ahash_alg *this_ahash =
	    container_of(halg_common, struct ahash_alg, halg);
	struct dx_digest_alg *this_digest_alg =
	    container_of(this_ahash, struct dx_digest_alg, ahash);
	int alg_index = this_digest_alg - dx_digest_algs;

	/* Verify that the tfm is valid (inside our dx_digest_algs array) */
	if ((alg_index < 0) || (alg_index >= DX_DIGEST_NUM)) {
		pr_err("Invalid digest tfm @%p\n", tfm);
		return NULL;
	}
	return this_digest_alg;
}

/**
 * prepare_digest_context_for_processing() - Prepare the crypto context of
 *	async. hash/mac operation. Initialize context if requested.
 *
 * @req:		Crypto request context
 * @do_init:		When "true" the given context is initialized
 */
static int prepare_digest_context_for_processing(struct ahash_request *req,
						 bool do_init)
{
	struct crypto_ahash *ahash_tfm = crypto_ahash_reqtfm(req);
	struct crypto_tfm *tfm = &ahash_tfm->base;
	struct dx_digest_alg *digest_alg = get_digest_alg(tfm);
	struct mac_key_data *key_data = crypto_tfm_ctx(tfm);/* For MACS only */
	struct async_digest_req_ctx *req_ctx = ahash_request_ctx(req);
	struct device *mydev = crypto_api_ctx.drv_data->sep_data->dev;
	struct sep_op_ctx *op_ctx = &req_ctx->async_req.op_ctx;
	struct client_crypto_ctx_info *ctx_info;
	enum dxdi_mac_type mac_type;
	struct dxdi_mac_props mac_props;	/* For MAC init. */
#ifdef DEBUG
	enum host_ctx_state ctx_state;
#endif
	int error_info;
	int rc;

	if (unlikely(digest_alg == NULL))
		return -EINVAL;

	pr_debug("op_ctx=%p op_state=%d\n", op_ctx, op_ctx->op_state);
	ctx_info = &op_ctx->ctx_info;
	mac_type = digest_alg->mac_type;

	if (!do_init) {
		/* Verify given request context was initialized */
		if (req_ctx->async_req.initiating_req == NULL) {
			pr_err(
				    "Invoked for uninitialized async. req. context\n");
			return -EINVAL;
		}
		/* Verify this request context that is not in use */
		if (op_ctx->op_state != USER_OP_NOP) {
			pr_err("Invoked for context in use!\n");
			return -EINVAL;
			/*
			 * We do not return -EBUSY because this is a valid
			 * return code for async crypto operations that
			 * indicates the given request was actually dispatched.
			 */
		}
	}
	op_ctx_init(op_ctx, &crypto_api_ctx);
	op_ctx->comp_work = &req_ctx->async_req.comp_work;
	rc = ctxmgr_map_kernel_ctx(ctx_info, mydev,
				   (mac_type != DXDI_MAC_NONE) ?
				   ALG_CLASS_MAC : ALG_CLASS_HASH,
				   (struct host_crypto_ctx *)&req_ctx->host_ctx,
				   NULL, 0);
	if (rc != 0) {
		pr_err("Failed mapping context (rc=%d)\n", rc);
		return rc;
	}
	if (do_init) {
		/* Allocate a new Crypto context ID */
		ctxmgr_set_ctx_id(ctx_info,
				  alloc_crypto_ctx_id(&crypto_api_ctx));
		if (mac_type == DXDI_MAC_NONE) {	/* Hash alg. */
			rc = ctxmgr_init_hash_ctx(ctx_info,
						  digest_alg->hash_type,
						  &error_info);
		} else {	/* MAC */
			mac_props.mac_type = mac_type;
			mac_props.key_size = key_data->key_size;
			memcpy(mac_props.key, key_data->key,
			       key_data->key_size);
			if (mac_type == DXDI_MAC_HMAC)
				mac_props.alg_specific.hmac.hash_type =
				    digest_alg->hash_type;
			rc = ctxmgr_init_mac_ctx(ctx_info, &mac_props,
						 &error_info);
		}
		if (unlikely(rc != 0)) {
			pr_err("Failed initializing context\n");
			ctxmgr_set_ctx_state(ctx_info, CTX_STATE_UNINITIALIZED);
		} else {
			ctxmgr_set_ctx_state(ctx_info, CTX_STATE_PARTIAL_INIT);
			/* Init. the async. request context */
			req_ctx->async_req.initiating_req = &req->base;
			INIT_WORK(&req_ctx->async_req.comp_work,
				  dx_crypto_api_handle_op_completion);
			req_ctx->result = NULL;
		}
#ifdef DEBUG
	} else {		/* Should have been initialized before */
		ctx_state = ctxmgr_get_ctx_state(ctx_info);
		if (ctx_state != CTX_STATE_INITIALIZED) {
			pr_err("Invoked for context in state %d!\n",
				    ctx_state);
			rc = -EINVAL;
		}
#endif		 /*DEBUG*/
	}
	if (likely(rc == 0)) {
#ifdef DEBUG
		ctxmgr_dump_sep_ctx(ctx_info);
#endif
		/* Flush sep_ctx out of host cache */
		ctxmgr_sync_sep_ctx(ctx_info, mydev);
	}
	return rc;
}

static int digest_req_dispatch(struct ahash_request *req,
			       bool do_init, bool is_last,
			       struct scatterlist *src, unsigned int nbytes)
{
	struct crypto_ahash *ahash_tfm = crypto_ahash_reqtfm(req);
	struct dx_digest_alg *digest_alg = get_digest_alg(&ahash_tfm->base);
	struct async_digest_req_ctx *req_ctx =
	    (struct async_digest_req_ctx *)ahash_request_ctx(req);
	struct sep_op_ctx *op_ctx = &req_ctx->async_req.op_ctx;
	struct client_crypto_ctx_info *ctx_info = &op_ctx->ctx_info;
	int rc;

	if (digest_alg == NULL)
		return -EINVAL;
	if ((!do_init) && (req_ctx->result != NULL)) {
		/* already finalized (AES based MACs) */
		if (unlikely(nbytes > 0)) {
			pr_err("Invoked with %u B after finalized\n",
				    nbytes);
			return -EINVAL;
		}
		if (is_last) {
			/* Fetch saved result */
			memcpy(req->result, req_ctx->result,
			       SEP_AES_BLOCK_SIZE);
			return 0;
		}
	}
	rc = prepare_digest_context_for_processing(req, do_init);
	if (unlikely(rc != 0))
		return rc;
	/* Preapre req_ctx->result */
	if (is_last) {		/* Plain finalization */
		req_ctx->result = req->result;
	} else if (((digest_alg->mac_type == DXDI_MAC_AES_XCBC_MAC) ||
		    (digest_alg->mac_type == DXDI_MAC_AES_CMAC)) &&
		   (!IS_MULT_OF(nbytes, SEP_AES_BLOCK_SIZE))) {
		/* Handle special case of AES based MAC update when not AES
		   block multiple --> dispatch as final update */
		is_last = true;
		/* req_Ctx->result remains NULL. This would cause setting
		   it to the result location in the sep context,
		   when completed */
	}

	op_ctx->op_type = is_last ? SEP_OP_CRYPTO_FINI :
	    do_init ? SEP_OP_CRYPTO_INIT : SEP_OP_CRYPTO_PROC;
	if (op_ctx->op_type != SEP_OP_CRYPTO_INIT) {
		rc = prepare_data_for_sep(op_ctx, NULL, src, NULL, NULL, nbytes,
					  is_last ? CRYPTO_DATA_TEXT_FINALIZE :
					  CRYPTO_DATA_TEXT);
		if (rc == -ENOTBLK) {
			/* Data was accumulated but less than a hash block */
			/* Complete operation immediately */
			rc = 0;
			goto digest_proc_exit;
		}
		if (unlikely(rc != 0)) {
			pr_err("Failed mapping client DMA buffer.\n");
			goto digest_proc_exit;
		}
	}
	rc = dispatch_crypto_op(op_ctx,
				req->base.flags & CRYPTO_TFM_REQ_MAY_BACKLOG,
				do_init,
				is_last ? SEP_PROC_MODE_FIN : do_init ?
				SEP_PROC_MODE_NOP : SEP_PROC_MODE_PROC_T,
				true /*cache */);
	if (unlikely(IS_DESCQ_ENQUEUE_ERR(rc))) {
		pr_err("Failed dispatching CRYPTO_OP (rc=%d)\n", rc);
		crypto_op_completion_cleanup(op_ctx);
		ctxmgr_unmap_kernel_ctx(ctx_info);
		op_ctx_fini(op_ctx);
	}
	return rc;
/* Exit when there is no pending request (error or not enough data) */
 digest_proc_exit:
	ctxmgr_unmap_kernel_ctx(ctx_info);
	op_ctx_fini(op_ctx);
	return rc;
}

static int digest_init(struct ahash_request *req)
{
	pr_debug("\n");
	return digest_req_dispatch(req, true, false, NULL, 0);
}

static int digest_update(struct ahash_request *req)
{
	pr_debug("nbytes=%u\n", req->nbytes);
	if (req->nbytes == 0)
		return 0;	/* Nothing to do (but valid for 0 data MACs */

	return digest_req_dispatch(req, false, false, req->src, req->nbytes);
}

static int digest_final(struct ahash_request *req)
{
	pr_debug("\n");
	return digest_req_dispatch(req, false, true, NULL, 0);
}

static int digest_finup(struct ahash_request *req)
{
	pr_debug("nbytes=%u\n", req->nbytes);
	return digest_req_dispatch(req, false, true, req->src, req->nbytes);
}

static int digest_integrated(struct ahash_request *req)
{
	pr_debug("nbytes=%u\n", req->nbytes);
	return digest_req_dispatch(req, true, true, req->src, req->nbytes);
}

/**
 * do_hash_sync() - Do integrated hash operation synchronously
 *
 * @hash_type:		The hash type used for this HMAC
 * @data_in:		The input data
 * @data_len:		Size data_in in bytes
 * @digest:		The hash result
 * @digest_len_p:	Returned digest size
 *
 * This function is used to shorten long HMAC keys.
 */
static int do_hash_sync(enum dxdi_hash_type hash_type,
			const u8 *data_in, unsigned int data_len,
			u8 *digest, unsigned int *digest_len_p)
{
	int rc;
	struct queue_drvdata *drvdata = crypto_api_ctx.drv_data;
	struct host_crypto_ctx_hash host_ctx;
	struct sep_op_ctx op_ctx;
	struct client_crypto_ctx_info *ctx_info_p = &op_ctx.ctx_info;
	struct scatterlist din_sgl;

	op_ctx_init(&op_ctx, &crypto_api_ctx);
	rc = ctxmgr_map_kernel_ctx(ctx_info_p, drvdata->sep_data->dev,
				   ALG_CLASS_HASH,
				   (struct host_crypto_ctx *)&host_ctx, NULL,
				   0);
	if (rc != 0) {
		pr_err("Failed mapping crypto context (rc=%d)\n", rc);
		op_ctx_fini(&op_ctx);
		return rc;
	}
	/* Allocate a new Crypto context ID */
	ctxmgr_set_ctx_id(ctx_info_p, alloc_crypto_ctx_id(op_ctx.client_ctx));
	/* Algorithm class specific initialization */
	rc = ctxmgr_init_hash_ctx(ctx_info_p, hash_type, &op_ctx.error_info);
	if (rc != 0)
		goto unmap_ctx_and_exit;
	ctxmgr_set_ctx_state(ctx_info_p, CTX_STATE_PARTIAL_INIT);
	op_ctx.op_type = SEP_OP_CRYPTO_FINI;	/* Integrated is also fin. */
	sg_init_one(&din_sgl, data_in, data_len);
	rc = prepare_data_for_sep(&op_ctx, NULL, &din_sgl, NULL, NULL,
				  data_len, CRYPTO_DATA_TEXT_FINALIZE);
	if (rc != 0)
		goto unmap_ctx_and_exit;

#ifdef DEBUG
	ctxmgr_dump_sep_ctx(ctx_info_p);
#endif
	/* Flush sep_ctx out of host cache */
	ctxmgr_sync_sep_ctx(ctx_info_p, drvdata->sep_data->dev);
	rc = dispatch_crypto_op(&op_ctx, true, true, SEP_PROC_MODE_FIN, false);
	if (likely(!IS_DESCQ_ENQUEUE_ERR(rc))) {
		rc = 0;	/* Clear valid return code from dispatch_crypto_op */
		wait_for_completion(&op_ctx.ioctl_op_compl);
		if (likely(op_ctx.error_info == 0)) {
			*digest_len_p =
			    ctxmgr_get_digest_or_mac(ctx_info_p, digest);
		}
	}
	crypto_op_completion_cleanup(&op_ctx);

 unmap_ctx_and_exit:
	ctxmgr_unmap_kernel_ctx(ctx_info_p);
	return rc;
}

static int mac_setkey(struct crypto_ahash *tfm,
		      const u8 *key, unsigned int keylen)
{
	struct dx_digest_alg *digest_alg = get_digest_alg(&tfm->base);
	u32 tfm_flags = crypto_ahash_get_flags(tfm);
	struct mac_key_data *key_data = crypto_tfm_ctx(&tfm->base);
	int rc = 0;

	if (unlikely(digest_alg == NULL))
		return -EINVAL;
	if (unlikely(digest_alg->mac_type == DXDI_MAC_NONE)) {
		pr_err("Given algorithm which is not MAC\n");
		return -EINVAL;
	}
	/* Pre-process HMAC key if larger than hash block size */
	if ((digest_alg->hash_type != DXDI_HASH_NONE) &&
	    (keylen > digest_alg->ahash.halg.base.cra_blocksize)) {
		rc = do_hash_sync(digest_alg->hash_type, key, keylen,
				  key_data->key, &key_data->key_size);
		if (unlikely(rc != 0))
			pr_err("Failed digesting key of %u bytes\n",
			       keylen);
		if (key_data->key_size != digest_alg->ahash.halg.digestsize)
			pr_err("Returned digest size is %u != %u (expected)\n",
			       key_data->key_size,
			       digest_alg->ahash.halg.digestsize);
	} else {		/* No need to digest the key */
		/* Verify that the key size for AES based MACs is not too
		   large. */
		if ((digest_alg->hash_type == DXDI_HASH_NONE) &&
		    (keylen > SEP_AES_KEY_SIZE_MAX)) {
			pr_err("Invalid key size %u for %s\n",
			       keylen,
			       digest_alg->ahash.halg.base.cra_name);
			tfm_flags |= CRYPTO_TFM_RES_BAD_KEY_LEN;
			crypto_ahash_set_flags(tfm, tfm_flags);
			rc = -EINVAL;
		} else {
			key_data->key_size = keylen;
			memcpy(&key_data->key, key, keylen);
		}
	}

	return rc;
}

/**
 * digest_tfm_init() - Initialize tfm with our reqsize (to accomodate context)
 *	(cra_init entry point)
 * @tfm:
 */
static int digest_tfm_init(struct crypto_tfm *tfm)
{
	struct crypto_ahash *ahash_tfm =
	    container_of(tfm, struct crypto_ahash, base);

	ahash_tfm->reqsize = sizeof(struct async_digest_req_ctx);
	return 0;
}

static int digest_algs_init(void)
{
	int i, rc;

	/* Create hash algorithms from base + specs via scratchpad */
	for (i = 0; i < DX_DIGEST_NUM; i++) {
		/* Apply template values into given algorithms */
		dx_digest_algs[i].ahash.init = digest_algs_base.init;
		dx_digest_algs[i].ahash.update = digest_algs_base.update;
		dx_digest_algs[i].ahash.final = digest_algs_base.final;
		dx_digest_algs[i].ahash.finup = digest_algs_base.finup;
		dx_digest_algs[i].ahash.digest = digest_algs_base.digest;
		dx_digest_algs[i].ahash.halg.base.cra_type =
		    digest_algs_base.halg.base.cra_type;
		dx_digest_algs[i].ahash.halg.base.cra_priority =
		    digest_algs_base.halg.base.cra_priority;
		dx_digest_algs[i].ahash.halg.base.cra_flags =
		    digest_algs_base.halg.base.cra_flags;
		dx_digest_algs[i].ahash.halg.base.cra_module =
		    digest_algs_base.halg.base.cra_module;
		dx_digest_algs[i].ahash.halg.base.cra_init =
		    digest_algs_base.halg.base.cra_init;
		INIT_LIST_HEAD(&dx_digest_algs[i].ahash.halg.base.cra_list);
	}

	/* Register algs */
	pr_debug("Registering CryptoAPI digest algorithms:\n");
	for (i = 0, rc = 0; (i < DX_DIGEST_NUM) && (rc == 0); i++) {
		pr_debug("%d. %s (__crt_alg=%p)\n", i,
			      dx_digest_algs[i].ahash.halg.base.cra_name,
			      &dx_digest_algs[i].ahash);
		rc = crypto_register_ahash(&dx_digest_algs[i].ahash);
		if (rc != 0)
			break;
	}
	if (unlikely(rc != 0)) {
		/* Failure: cleanup algorithms that already registered */
		pr_err("Failed registering %s\n",
			    dx_digest_algs[i].ahash.halg.base.cra_name);
		if (i > 0)
			for (; i >= 0; i--)
				crypto_unregister_ahash(
						&dx_digest_algs[i].ahash);
	}
	return rc;
}

static void digest_algs_exit(void)
{
	int i;

	for (i = 0; i < DX_DIGEST_NUM; i++)
		crypto_unregister_ahash(&dx_digest_algs[i].ahash);
}

/****************************************************/
int dx_crypto_api_init(struct sep_drvdata *drvdata)
{
	/* Init. return code of each init. function to know which one to
	   cleanup (only those with rc==0) */
	int rc_ablkcipher_init = -EINVAL;
	int rc_digest_init = -EINVAL;
	int rc;

	init_client_ctx(drvdata->queue + CRYPTO_API_QID, &crypto_api_ctx);

	sep_ctx_pool = dma_pool_create("dx_sep_ctx",
				       crypto_api_ctx.drv_data->sep_data->dev,
				       sizeof(struct sep_ctx_cache_entry),
				       L1_CACHE_BYTES, 0);
	if (sep_ctx_pool == NULL) {
		pr_err("Failed allocating pool for SeP contexts\n");
		rc = -ENOMEM;
		goto init_error;
	}
	rc_ablkcipher_init = ablkcipher_algs_init();
	if (unlikely(rc_ablkcipher_init != 0)) {
		rc = rc_ablkcipher_init;
		goto init_error;
	}
	rc_digest_init = digest_algs_init();
	if (unlikely(rc_digest_init != 0)) {
		rc = rc_digest_init;
		goto init_error;
	}

	return 0;

 init_error:
	if (rc_ablkcipher_init == 0)
		ablkcipher_algs_exit();
	if (sep_ctx_pool != NULL)
		dma_pool_destroy(sep_ctx_pool);
	cleanup_client_ctx(drvdata->queue + CRYPTO_API_QID, &crypto_api_ctx);
	return rc;
}

void dx_crypto_api_fini(void)
{
	digest_algs_exit();
	ablkcipher_algs_exit();
	dma_pool_destroy(sep_ctx_pool);
	cleanup_client_ctx(crypto_api_ctx.drv_data, &crypto_api_ctx);
}
