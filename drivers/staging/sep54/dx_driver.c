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

#define SEP_LOG_CUR_COMPONENT SEP_LOG_MASK_MAIN

#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/sysctl.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/list.h>
#include <linux/slab.h>
/* cache.h required for L1_CACHE_ALIGN() and cache_line_size() */
#include <linux/cache.h>
#include <asm/byteorder.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <asm/cacheflush.h>
#include <linux/pagemap.h>
#include <linux/pci.h>
#include <linux/pm_runtime.h>
#include <linux/kthread.h>
#include <linux/genhd.h>
#include <linux/mmc/card.h>

#include <generated/autoconf.h>
#if defined(DEBUG) && defined(CONFIG_KGDB)
/* For setup_break option */
#include <linux/kgdb.h>
#endif

#include "sep_log.h"
#include "sep_init.h"
#include "desc_mgr.h"
#include "lli_mgr.h"
#include "sep_sysfs.h"
#include "dx_driver_abi.h"
#include "crypto_ctx_mgr.h"
#include "crypto_api.h"
#include "sep_request.h"
#include "dx_sep_kapi.h"
#if MAX_SEPAPP_SESSION_PER_CLIENT_CTX > 0
#include "sepapp.h"
#endif
#include "sep_power.h"
#include "sep_request_mgr.h"
#include "sep_applets.h"

/* Registers definitions from shared/hw/include */
#include "dx_reg_base_host.h"
#include "dx_host.h"
#ifdef DX_BASE_ENV_REGS
#include "dx_env.h"
#endif
#define DX_CC_HOST_VIRT	/* must be defined before including dx_cc_regs.h */
#include "dx_cc_regs.h"
#include "dx_init_cc_abi.h"
#include "dx_driver.h"

#ifdef CONFIG_COMPAT
#include "sep_compat_ioctl.h"
#endif

#if SEPAPP_UUID_SIZE != DXDI_SEPAPP_UUID_SIZE
#error Size mismatch of SEPAPP_UUID_SIZE and DXDI_SEPAPP_UUID_SIZE
#endif

#define DEVICE_NAME_PREFIX  "dx_sep_q"
#define SEP_DEVICES         SEP_MAX_NUM_OF_DESC_Q

#define DRIVER_NAME     MODULE_NAME
#define SEP_DEVICES     SEP_MAX_NUM_OF_DESC_Q

#ifdef SEP_PRINTF
#define SEP_PRINTF_H2S_GPR_OFFSET \
	HOST_SEP_GPR_REG_OFFSET(DX_SEP_HOST_PRINTF_GPR_IDX)
#define SEP_PRINTF_S2H_GPR_OFFSET \
	SEP_HOST_GPR_REG_OFFSET(DX_SEP_HOST_PRINTF_GPR_IDX)
/* Ack is allocated only upper 24 bits */
#define SEP_PRINTF_ACK_MAX 0xFFFFFF
/* Sync. host-SeP value */
#define SEP_PRINTF_ACK_SYNC_VAL SEP_PRINTF_ACK_MAX
#endif

static struct class *sep_class;

int q_num;			/* Initialized to 0 */
module_param(q_num, int, 0444);
MODULE_PARM_DESC(q_num, "Num. of active queues 1-2");

int sep_log_level = SEP_BASE_LOG_LEVEL;
module_param(sep_log_level, int, 0644);
MODULE_PARM_DESC(sep_log_level, "Log level: min ERR = 0, max TRACE = 4");

int sep_log_mask = SEP_LOG_MASK_ALL;
module_param(sep_log_mask, int, 0644);
MODULE_PARM_DESC(sep_log_mask, "Log components mask");

int disable_linux_crypto;
module_param(disable_linux_crypto, int, 0444);
MODULE_PARM_DESC(disable_linux_crypto,
		 "Set to !0 to disable registration with Linux CryptoAPI");

/* Parameters to override default sep cache memories reserved pages */
#ifdef ICACHE_SIZE_LOG2_DEFAULT
#include "dx_init_cc_defs.h"
int icache_size_log2 = ICACHE_SIZE_LOG2_DEFAULT;
module_param(icache_size_log2, int, 0444);
MODULE_PARM_DESC(icache_size_log2, "Size of Icache memory in log2(bytes)");

int dcache_size_log2 = DCACHE_SIZE_LOG2_DEFAULT;
module_param(dcache_size_log2, int, 0444);
MODULE_PARM_DESC(icache_size_log2, "Size of Dcache memory in log2(bytes)");
#endif

#ifdef SEP_BACKUP_BUF_SIZE
int sep_backup_buf_size = SEP_BACKUP_BUF_SIZE;
module_param(sep_backup_buf_size, int, 0444);
MODULE_PARM_DESC(sep_backup_buf_size,
		 "Size of backup buffer of SeP context (for warm-boot)");
#endif

/* Interrupt mask assigned to GPRs */
/* Used for run time lookup, where SEP_HOST_GPR_IRQ_MASK() cannot be used */
static const u32 gpr_interrupt_mask[] = {
	SEP_HOST_GPR_IRQ_MASK(0),
	SEP_HOST_GPR_IRQ_MASK(1),
	SEP_HOST_GPR_IRQ_MASK(2),
	SEP_HOST_GPR_IRQ_MASK(3),
	SEP_HOST_GPR_IRQ_MASK(4),
	SEP_HOST_GPR_IRQ_MASK(5),
	SEP_HOST_GPR_IRQ_MASK(6),
	SEP_HOST_GPR_IRQ_MASK(7)
};

u32 __iomem *security_cfg_reg;

#ifdef DEBUG
void dump_byte_array(const char *name, const u8 *the_array,
		     unsigned long size)
{
	int i, line_offset = 0;
	const u8 *cur_byte;
	char line_buf[80];

	line_offset = snprintf(line_buf, sizeof(line_buf), "%s[%lu]: ",
			       name, size);

	for (i = 0, cur_byte = the_array;
	     (i < size) && (line_offset < sizeof(line_buf)); i++, cur_byte++) {
		line_offset += snprintf(line_buf + line_offset,
					sizeof(line_buf) - line_offset,
					"%02X ", *cur_byte);
		if (line_offset > 75) {	/* Cut before line end */
			pr_debug("%s\n", line_buf);
			line_offset = 0;
		}
	}

	if (line_offset > 0)	/* Dump remainding line */
		pr_debug("%s\n", line_buf);

}

void dump_word_array(const char *name, const u32 *the_array,
		     unsigned long size_in_words)
{
	int i, line_offset = 0;
	const u32 *cur_word;
	char line_buf[80];

	line_offset = snprintf(line_buf, sizeof(line_buf), "%s[%lu]: ",
			       name, size_in_words);

	for (i = 0, cur_word = the_array;
	     (i < size_in_words) && (line_offset < sizeof(line_buf));
	     i++, cur_word++) {
		line_offset += snprintf(line_buf + line_offset,
					sizeof(line_buf) - line_offset,
					"%08X ", *cur_word);
		if (line_offset > 70) {	/* Cut before line end */
			pr_debug("%s\n", line_buf);
			line_offset = 0;
		}
	}

	if (line_offset > 0)	/* Dump remainding line */
		pr_debug("%s\n", line_buf);
}

#endif /*DEBUG*/
/**** SeP descriptor operations implementation functions *****/
/* (send descriptor, wait for completion, process result)    */
/**
 * send_crypto_op_desc() - Pack crypto op. descriptor and send
 * @op_ctx:
 * @sep_ctx_load_req:	Flag if context loading is required
 * @sep_ctx_init_req:	Flag if context init is required
 * @proc_mode:		Processing mode
 *
 * On failure desc_type is retained so process_desc_completion cleans up
 * resources anyway (error_info denotes failure to send/complete)
 * Returns int 0 on success
 */
static int send_crypto_op_desc(struct sep_op_ctx *op_ctx,
			       int sep_ctx_load_req, int sep_ctx_init_req,
			       enum sep_proc_mode proc_mode)
{
	const struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;
	struct sep_sw_desc desc;
	int rc;

	desc_q_pack_crypto_op_desc(&desc, op_ctx,
				   sep_ctx_load_req, sep_ctx_init_req,
				   proc_mode);
	/* op_state must be updated before dispatching descriptor */
	rc = desc_q_enqueue(drvdata->desc_queue, &desc, true);
	if (unlikely(IS_DESCQ_ENQUEUE_ERR(rc))) {
		/* Desc. sending failed - "signal" process_desc_completion */
		op_ctx->error_info = DXDI_ERROR_INTERNAL;
	} else {
		rc = 0;
	}

	return rc;
}

/**
 * send_combined_op_desc() - Pack combined or/and load operation descriptor(s)
 * @op_ctx:
 * @sep_ctx_load_req: Flag if context loading is required
 * @sep_ctx_init_req: Flag if context init is required
 * @proc_mode: Processing mode
 * @cfg_scheme: The SEP format configuration scheme claimed by the user
 *
 * On failure desc_type is retained so process_desc_completion cleans up
 * resources anyway (error_info denotes failure to send/complete)
 * Returns int 0 on success
 */
static int send_combined_op_desc(struct sep_op_ctx *op_ctx,
				 int *sep_ctx_load_req, int sep_ctx_init_req,
				 enum sep_proc_mode proc_mode,
				 u32 cfg_scheme)
{
	const struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;
	struct sep_sw_desc desc;
	int rc;

	/* transaction of two descriptors */
	op_ctx->pending_descs_cntr = 2;

	/* prepare load descriptor of combined associated contexts */
	desc_q_pack_load_op_desc(&desc, op_ctx, sep_ctx_load_req);

	/* op_state must be updated before dispatching descriptor */
	rc = desc_q_enqueue(drvdata->desc_queue, &desc, true);
	if (unlikely(IS_DESCQ_ENQUEUE_ERR(rc))) {
		/* Desc. sending failed - "signal" process_desc_completion */
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
		goto send_combined_op_exit;
	}

	/* prepare crypto descriptor for the combined scheme operation */
	desc_q_pack_combined_op_desc(&desc, op_ctx, 0/* contexts already loaded
						      * in prior descriptor */ ,
				     sep_ctx_init_req, proc_mode, cfg_scheme);
	rc = desc_q_enqueue(drvdata->desc_queue, &desc, true);
	if (unlikely(IS_DESCQ_ENQUEUE_ERR(rc))) {
		/*invalidate first descriptor (if still pending) */
		desc_q_mark_invalid_cookie(drvdata->desc_queue, (void *)op_ctx);
		/* Desc. sending failed - "signal" process_desc_completion */
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
	} else {
		rc = 0;
	}

 send_combined_op_exit:
	return rc;
}

/**
 * register_client_memref() - Register given client memory buffer reference
 * @client_ctx:		User context data
 * @user_buf_ptr:	Buffer address in user space. NULL if sgl!=NULL.
 * @sgl:		Scatter/gather list (for kernel buffers)
 *			NULL if user_buf_ptr!=NULL.
 * @buf_size:		Buffer size in bytes
 * @dma_direction:	DMA direction
 *
 * Returns int >= is the registered memory reference ID, <0 for error
 */
int register_client_memref(struct sep_client_ctx *client_ctx,
			   u8 __user *user_buf_ptr,
			   struct scatterlist *sgl,
			   const unsigned long buf_size,
			   const enum dma_data_direction dma_direction)
{
	int free_memref_idx, rc;
	struct registered_memref *regmem_p;

	if (unlikely((user_buf_ptr != NULL) && (sgl != NULL))) {
		pr_err("Both user_buf_ptr and sgl are given!\n");
		return -EINVAL;
	}

	/* Find free entry in user_memref */
	for (free_memref_idx = 0, regmem_p = &client_ctx->reg_memrefs[0];
	     free_memref_idx < MAX_REG_MEMREF_PER_CLIENT_CTX;
	     free_memref_idx++, regmem_p++) {
		mutex_lock(&regmem_p->buf_lock);
		if (regmem_p->ref_cnt == 0)
			break;	/* found free entry */
		mutex_unlock(&regmem_p->buf_lock);
	}
	if (unlikely(free_memref_idx == MAX_REG_MEMREF_PER_CLIENT_CTX)) {
		pr_warn("No free entry for user memory registration\n");
		free_memref_idx = -ENOMEM;/* Negative error code as index */
	} else {
		pr_debug("Allocated memref_idx=%d (regmem_p=%p)\n",
			      free_memref_idx, regmem_p);
		regmem_p->ref_cnt = 1;	/* Capture entry */
		/* Lock user pages for DMA and save pages info.
		 * (prepare for MLLI) */
		rc = llimgr_register_client_dma_buf(client_ctx->drv_data->
						    sep_data->llimgr,
						    user_buf_ptr, sgl, buf_size,
						    0, dma_direction,
						    &regmem_p->dma_obj);
		if (unlikely(rc < 0)) {
			/* Release entry */
			regmem_p->ref_cnt = 0;
			free_memref_idx = rc;
		}
		mutex_unlock(&regmem_p->buf_lock);
	}

	/* Return user_memref[] entry index as the memory reference ID */
	return free_memref_idx;
}

/**
 * free_user_memref() - Free resources associated with a user memory reference
 * @client_ctx:	 User context data
 * @memref_idx:	 Index of the user memory reference
 *
 * Free resources associated with a user memory reference
 * (The referenced memory may be locked user pages or allocated DMA-coherent
 *  memory mmap'ed to the user space)
 * Returns int !0 on failure (memref still in use or unknown)
 */
int free_client_memref(struct sep_client_ctx *client_ctx,
		       int memref_idx)
{
	struct registered_memref *regmem_p =
	    &client_ctx->reg_memrefs[memref_idx];
	int rc = 0;

	if (!IS_VALID_MEMREF_IDX(memref_idx)) {
		pr_err("Invalid memref ID %d\n", memref_idx);
		return -EINVAL;
	}

	mutex_lock(&regmem_p->buf_lock);

	if (likely(regmem_p->ref_cnt == 1)) {
		/* TODO: support case of allocated DMA-coherent buffer */
		llimgr_deregister_client_dma_buf(client_ctx->drv_data->
						 sep_data->llimgr,
						 &regmem_p->dma_obj);
		regmem_p->ref_cnt = 0;
	} else if (unlikely(regmem_p->ref_cnt == 0)) {
		pr_err("Invoked for free memref ID=%d\n", memref_idx);
		rc = -EINVAL;
	} else {		/* ref_cnt > 1 */
		pr_err(
			    "BUSY/Invalid memref to release: ref_cnt=%d, user_buf_ptr=%p\n",
			    regmem_p->ref_cnt, regmem_p->dma_obj.user_buf_ptr);
		rc = -EBUSY;
	}

	mutex_unlock(&regmem_p->buf_lock);

	return rc;
}

/**
 * acquire_dma_obj() - Get the memref object of given memref_idx and increment
 *			reference count of it
 * @client_ctx:	Associated client context
 * @memref_idx:	Required registered memory reference ID (index)
 *
 * Get the memref object of given memref_idx and increment reference count of it
 * The returned object must be released by invoking release_dma_obj() before
 * the object (memref) may be freed.
 * Returns struct user_dma_buffer The memref object or NULL for invalid
 */
struct client_dma_buffer *acquire_dma_obj(struct sep_client_ctx *client_ctx,
					  int memref_idx)
{
	struct registered_memref *regmem_p =
	    &client_ctx->reg_memrefs[memref_idx];
	struct client_dma_buffer *rc;

	if (!IS_VALID_MEMREF_IDX(memref_idx)) {
		pr_err("Invalid memref ID %d\n", memref_idx);
		return NULL;
	}

	mutex_lock(&regmem_p->buf_lock);
	if (regmem_p->ref_cnt < 1) {
		pr_err("Invalid memref (ID=%d, ref_cnt=%d)\n",
			    memref_idx, regmem_p->ref_cnt);
		rc = NULL;
	} else {
		regmem_p->ref_cnt++;
		rc = &regmem_p->dma_obj;
	}
	mutex_unlock(&regmem_p->buf_lock);

	return rc;
}

/**
 * release_dma_obj() - Release memref object taken with get_memref_obj
 *			(Does not free!)
 * @client_ctx:	Associated client context
 * @dma_obj:	The DMA object returned from acquire_dma_obj()
 *
 * Returns void
 */
void release_dma_obj(struct sep_client_ctx *client_ctx,
		     struct client_dma_buffer *dma_obj)
{
	struct registered_memref *regmem_p;
	int memref_idx;

	if (dma_obj == NULL)	/* Probably failed on acquire_dma_obj */
		return;
	/* Verify valid container */
	memref_idx = DMA_OBJ_TO_MEMREF_IDX(client_ctx, dma_obj);
	if (!IS_VALID_MEMREF_IDX(memref_idx)) {
		pr_err("Given DMA object is not registered\n");
		return;
	}
	/* Get container */
	regmem_p = &client_ctx->reg_memrefs[memref_idx];
	mutex_lock(&regmem_p->buf_lock);
	if (regmem_p->ref_cnt < 2) {
		pr_err("Invalid memref (ref_cnt=%d, user_buf_ptr=%p)\n",
			    regmem_p->ref_cnt, regmem_p->dma_obj.user_buf_ptr);
	} else {
		regmem_p->ref_cnt--;
	}
	mutex_unlock(&regmem_p->buf_lock);
}

/**
 * crypto_op_completion_cleanup() - Cleanup CRYPTO_OP descriptor operation
 *					resources after completion
 * @op_ctx:
 *
 * Returns int
 */
int crypto_op_completion_cleanup(struct sep_op_ctx *op_ctx)
{
	struct sep_client_ctx *client_ctx = op_ctx->client_ctx;
	struct queue_drvdata *drvdata = client_ctx->drv_data;
	void *llimgr = drvdata->sep_data->llimgr;
	struct client_crypto_ctx_info *ctx_info_p = &(op_ctx->ctx_info);
	enum sep_op_type op_type = op_ctx->op_type;
	u32 error_info = op_ctx->error_info;
	u32 ctx_info_idx;
	bool data_in_place;
	const enum crypto_alg_class alg_class =
	    ctxmgr_get_alg_class(&op_ctx->ctx_info);

	/* Resources cleanup on data processing operations (PROC/FINI) */
	if (op_type & (SEP_OP_CRYPTO_PROC | SEP_OP_CRYPTO_FINI)) {
		if ((alg_class == ALG_CLASS_HASH) ||
		    (ctxmgr_get_mac_type(ctx_info_p) == DXDI_MAC_HMAC)) {
			/* Unmap what was mapped in prepare_data_for_sep() */
			ctxmgr_unmap2dev_hash_tail(ctx_info_p,
						   drvdata->sep_data->dev);
			/* Save last data block tail (remainder of crypto-block)
			 * or clear that buffer after it was used if there is
			 * no new block remainder data */
			ctxmgr_save_hash_blk_remainder(ctx_info_p,
						       &op_ctx->din_dma_obj,
						       false);
		}
		data_in_place = llimgr_is_same_mlli_tables(llimgr,
							   &op_ctx->ift,
							   &op_ctx->oft);
		/* First free IFT resources */
		llimgr_destroy_mlli(llimgr, &op_ctx->ift);
		llimgr_deregister_client_dma_buf(llimgr, &op_ctx->din_dma_obj);
		/* Free OFT resources */
		if (data_in_place) {
			/* OFT already destroyed as IFT. Just clean it. */
			MLLI_TABLES_LIST_INIT(&op_ctx->oft);
			CLEAN_DMA_BUFFER_INFO(&op_ctx->din_dma_obj);
		} else {	/* OFT resources cleanup */
			llimgr_destroy_mlli(llimgr, &op_ctx->oft);
			llimgr_deregister_client_dma_buf(llimgr,
							 &op_ctx->dout_dma_obj);
		}
	}

	for (ctx_info_idx = 0;
	     ctx_info_idx < op_ctx->ctx_info_num;
	     ctx_info_idx++, ctx_info_p++) {
		if ((op_type & SEP_OP_CRYPTO_FINI) || (error_info != 0)) {
			/* If this was a finalizing descriptor, or any error,
			 * invalidate from cache */
			ctxmgr_sep_cache_invalidate(drvdata->sep_cache,
					ctxmgr_get_ctx_id(ctx_info_p),
					CRYPTO_CTX_ID_SINGLE_MASK);
		}
		/* Update context state */
		if ((op_type & SEP_OP_CRYPTO_FINI) ||
		    ((op_type & SEP_OP_CRYPTO_INIT) && (error_info != 0))) {
			/* If this was a finalizing descriptor,
			 * or a failing initializing descriptor: */
			ctxmgr_set_ctx_state(ctx_info_p,
					CTX_STATE_UNINITIALIZED);
		} else if (op_type & SEP_OP_CRYPTO_INIT)
			ctxmgr_set_ctx_state(ctx_info_p,
					CTX_STATE_INITIALIZED);
	}

	return 0;
}

/**
 * wait_for_sep_op_result() - Wait for outstanding SeP operation to complete and
 *				fetch SeP ret-code
 * @op_ctx:
 *
 * Wait for outstanding SeP operation to complete and fetch SeP ret-code
 * into op_ctx->sep_ret_code
 * Returns int
 */
int wait_for_sep_op_result(struct sep_op_ctx *op_ctx)
{
#ifdef DEBUG
	if (unlikely(op_ctx->op_state == USER_OP_NOP)) {
		pr_err("Operation context is inactive!\n");
		op_ctx->error_info = DXDI_ERROR_FATAL;
		return -EINVAL;
	}
#endif

	/* wait until crypto operation is completed.
	 * We cannot timeout this operation because hardware operations may
	 * be still pending on associated data buffers.
	 * Only system reboot can take us out of this abnormal state in a safe
	 * manner (avoiding data corruption) */
	wait_for_completion(&(op_ctx->ioctl_op_compl));
#ifdef DEBUG
	if (unlikely(op_ctx->op_state != USER_OP_COMPLETED)) {
		pr_err(
			    "Op. state is not COMPLETED after getting completion event (op_ctx=0x%p, op_state=%d)\n",
			    op_ctx, op_ctx->op_state);
		dump_stack();	/*SEP_DRIVER_BUG(); */
		op_ctx->error_info = DXDI_ERROR_FATAL;
		return -EINVAL;
	}
#endif

	return 0;
}

/**
 * get_num_of_ctx_info() - Count the number of valid contexts assigned for the
 *				combined operation.
 * @config:	 The user configuration scheme array
 *
 * Returns int
 */
static int get_num_of_ctx_info(struct dxdi_combined_props *config)
{
	int valid_ctx_n;

	for (valid_ctx_n = 0;
	     (valid_ctx_n < DXDI_COMBINED_NODES_MAX) &&
	     (config->node_props[valid_ctx_n].context != NULL)
	     ; valid_ctx_n++) {
		/*NOOP*/
	}

	return valid_ctx_n;
}

/***** Driver Interface implementation functions *****/

/**
 * format_sep_combined_cfg_scheme() - Encode the user configuration scheme to
 *					SeP format.
 * @config:	 The user configuration scheme array
 * @op_ctx:	 Operation context
 *
 * Returns u32
 */
static u32 format_sep_combined_cfg_scheme(struct dxdi_combined_props
					       *config,
					       struct sep_op_ctx *op_ctx)
{
	enum dxdi_input_engine_type engine_src = DXDI_INPUT_NULL;
	enum sep_engine_type engine_type = SEP_ENGINE_NULL;
	enum crypto_alg_class alg_class;
	u32 sep_cfg_scheme = 0;	/* the encoded config scheme */
	struct client_crypto_ctx_info *ctx_info_p = &op_ctx->ctx_info;
	enum dxdi_sym_cipher_type symc_type;
	int eng_idx, done = 0;
	int prev_direction = -1;

	/* encode engines connections into SEP format */
	for (eng_idx = 0;
	     (eng_idx < DXDI_COMBINED_NODES_MAX) && (!done);
	     eng_idx++, ctx_info_p++) {

		/* set engine source */
		engine_src = config->node_props[eng_idx].eng_input;

		/* set engine type */
		if (config->node_props[eng_idx].context != NULL) {
			int dir;
			alg_class = ctxmgr_get_alg_class(ctx_info_p);
			switch (alg_class) {
			case ALG_CLASS_HASH:
				engine_type = SEP_ENGINE_HASH;
				break;
			case ALG_CLASS_SYM_CIPHER:
				symc_type =
				    ctxmgr_get_sym_cipher_type(ctx_info_p);
				if ((symc_type == DXDI_SYMCIPHER_AES_ECB) ||
				    (symc_type == DXDI_SYMCIPHER_AES_CBC) ||
				    (symc_type == DXDI_SYMCIPHER_AES_CTR))
					engine_type = SEP_ENGINE_AES;
				else
					engine_type = SEP_ENGINE_NULL;

				dir =
				    (int)
				    ctxmgr_get_symcipher_direction(ctx_info_p);
				if (prev_direction == -1) {
					prev_direction = dir;
				} else {
					/* Only decrypt->encrypt operation */
					if (!
					    (prev_direction ==
					     SEP_CRYPTO_DIRECTION_DECRYPT &&
					     (dir ==
					      SEP_CRYPTO_DIRECTION_ENCRYPT))) {
						pr_err(
						    "Invalid direction combination %s->%s\n",
						    prev_direction ==
						    SEP_CRYPTO_DIRECTION_DECRYPT
						    ? "DEC" : "ENC",
						    dir ==
						    SEP_CRYPTO_DIRECTION_DECRYPT
						    ? "DEC" : "ENC");
						op_ctx->error_info =
						    DXDI_ERROR_INVAL_DIRECTION;
					}
				}
				break;
			default:
				engine_type = SEP_ENGINE_NULL;
				break;	/*unsupported alg class */
			}
		} else if (engine_src != DXDI_INPUT_NULL) {
			/* incase engine source is not NULL and NULL sub-context
			 * is passed then DOUT is -DOUT type */
			engine_type = SEP_ENGINE_DOUT;
			/* exit after props set */
			done = 1;
		} else {
			/* both context pointer & input type are
			 * NULL -we're done */
			break;
		}

		sep_comb_eng_props_set(&sep_cfg_scheme, eng_idx,
					  engine_src, engine_type);
	}

	return sep_cfg_scheme;
}

/**
 * init_crypto_context() - Initialize host crypto context
 * @op_ctx:
 * @context_buf:
 * @alg_class:
 * @props:	Pointer to configuration properties which match given alg_class:
 *		ALG_CLASS_SYM_CIPHER: struct dxdi_sym_cipher_props
 *		ALG_CLASS_AUTH_ENC: struct dxdi_auth_enc_props
 *		ALG_CLASS_MAC: struct dxdi_mac_props
 *		ALG_CLASS_HASH: enum dxdi_hash_type
 *
 * Returns int 0 if operation executed in SeP.
 * See error_info for actual results.
 */
static int init_crypto_context(struct sep_op_ctx *op_ctx,
			       u32 __user *context_buf,
			       enum crypto_alg_class alg_class, void *props)
{
	int rc;
	int sep_cache_load_req;
	struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;
	bool postpone_init = false;

	rc = ctxmgr_map_user_ctx(&op_ctx->ctx_info, drvdata->sep_data->dev,
				 alg_class, context_buf);
	if (rc != 0) {
		op_ctx->error_info = DXDI_ERROR_BAD_CTX;
		return rc;
	}
	op_ctx->op_type = SEP_OP_CRYPTO_INIT;
	ctxmgr_set_ctx_state(&op_ctx->ctx_info, CTX_STATE_UNINITIALIZED);
	/* Allocate a new Crypto context ID */
	ctxmgr_set_ctx_id(&op_ctx->ctx_info,
			  alloc_crypto_ctx_id(op_ctx->client_ctx));

	switch (alg_class) {
	case ALG_CLASS_SYM_CIPHER:
		rc = ctxmgr_init_symcipher_ctx(&op_ctx->ctx_info,
					       (struct dxdi_sym_cipher_props *)
					       props, &postpone_init,
					       &op_ctx->error_info);
		break;
	case ALG_CLASS_AUTH_ENC:
		rc = ctxmgr_init_auth_enc_ctx(&op_ctx->ctx_info,
					      (struct dxdi_auth_enc_props *)
					      props, &op_ctx->error_info);
		break;
	case ALG_CLASS_MAC:
		rc = ctxmgr_init_mac_ctx(&op_ctx->ctx_info,
					 (struct dxdi_mac_props *)props,
					 &op_ctx->error_info);
		break;
	case ALG_CLASS_HASH:
		rc = ctxmgr_init_hash_ctx(&op_ctx->ctx_info,
					  *((enum dxdi_hash_type *)props),
					  &op_ctx->error_info);
		break;
	default:
		pr_err("Invalid algorithm class %d\n", alg_class);
		op_ctx->error_info = DXDI_ERROR_UNSUP;
		rc = -EINVAL;
	}
	if (rc != 0)
		goto ctx_init_exit;

	/* After inialization above we are partially init. missing SeP init. */
	ctxmgr_set_ctx_state(&op_ctx->ctx_info, CTX_STATE_PARTIAL_INIT);
#ifdef DEBUG
	ctxmgr_dump_sep_ctx(&op_ctx->ctx_info);
#endif

	/* If not all the init. information is available at this time
	 * we postpone INIT in SeP to processing phase */
	if (postpone_init) {
		pr_debug("Init. postponed to processing phase\n");
		ctxmgr_unmap_user_ctx(&op_ctx->ctx_info);
		/* must be valid on "success" */
		op_ctx->error_info = DXDI_ERROR_NULL;
		return 0;
	}

	/* Flush out of host cache */
	ctxmgr_sync_sep_ctx(&op_ctx->ctx_info, drvdata->sep_data->dev);
	/* Start critical section -
	 * cache allocation must be coupled to descriptor enqueue */
	rc = mutex_lock_interruptible(&drvdata->desc_queue_sequencer);
	if (rc != 0) {
		pr_err("Failed locking descQ sequencer[%u]\n",
		       op_ctx->client_ctx->qid);
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
		goto ctx_init_exit;
	}

	ctxmgr_set_sep_cache_idx(&op_ctx->ctx_info,
				 ctxmgr_sep_cache_alloc(drvdata->sep_cache,
							ctxmgr_get_ctx_id
							(&op_ctx->ctx_info),
							&sep_cache_load_req));
	if (!sep_cache_load_req)
		pr_err("New context already in SeP cache?!");

	rc = send_crypto_op_desc(op_ctx,
				 1 /*always load on init */ , 1 /*INIT*/,
				 SEP_PROC_MODE_NOP);

	mutex_unlock(&drvdata->desc_queue_sequencer);
	if (likely(rc == 0))
		rc = wait_for_sep_op_result(op_ctx);

 ctx_init_exit:
	/* Cleanup resources and update context state */
	crypto_op_completion_cleanup(op_ctx);
	ctxmgr_unmap_user_ctx(&op_ctx->ctx_info);

	return rc;
}

/**
 * map_ctx_for_proc() - Map previously initialized crypto context before data
 *			processing
 * @op_ctx:
 * @context_buf:
 * @ctx_state:	 Returned current context state
 *
 * Returns int
 */
static int map_ctx_for_proc(struct sep_client_ctx *client_ctx,
			    struct client_crypto_ctx_info *ctx_info,
			    u32 __user *context_buf,
			    enum host_ctx_state *ctx_state_p)
{
	int rc;
	struct queue_drvdata *drvdata = client_ctx->drv_data;

	/* default state in case of error */
	*ctx_state_p = CTX_STATE_UNINITIALIZED;

	rc = ctxmgr_map_user_ctx(ctx_info, drvdata->sep_data->dev,
				 ALG_CLASS_NONE, context_buf);
	if (rc != 0) {
		pr_err("Failed mapping context\n");
		return rc;
	}
	if (ctxmgr_get_session_id(ctx_info) != (uintptr_t) client_ctx) {
		pr_err("Context ID is not associated with this session\n");
		rc = -EINVAL;
	}
	if (rc == 0)
		*ctx_state_p = ctx_info->ctx_kptr->state;

#ifdef DEBUG
	ctxmgr_dump_sep_ctx(ctx_info);
	/* Flush out of host cache */
	ctxmgr_sync_sep_ctx(ctx_info, drvdata->sep_data->dev);
#endif
	if (rc != 0)
		ctxmgr_unmap_user_ctx(ctx_info);

	return rc;
}

/**
 * init_combined_context() - Initialize Combined context
 * @op_ctx:
 * @config: Pointer to configuration scheme to be validate by the SeP
 *
 * Returns int 0 if operation executed in SeP.
 * See error_info for actual results.
 */
static int init_combined_context(struct sep_op_ctx *op_ctx,
				 struct dxdi_combined_props *config)
{
	struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;
	struct client_crypto_ctx_info *ctx_info_p = &op_ctx->ctx_info;
	int sep_ctx_load_req[DXDI_COMBINED_NODES_MAX];
	enum host_ctx_state ctx_state;
	int rc, ctx_idx, ctx_mapped_n = 0;

	op_ctx->op_type = SEP_OP_CRYPTO_INIT;

	/* no context to load -clear buffer */
	memset(sep_ctx_load_req, 0, sizeof(sep_ctx_load_req));

	/* set the number of active contexts */
	op_ctx->ctx_info_num = get_num_of_ctx_info(config);

	/* map each context in the configuration scheme */
	for (ctx_idx = 0; ctx_idx < op_ctx->ctx_info_num;
	     ctx_idx++, ctx_info_p++) {
		/* context already initialized by the user */
		rc = map_ctx_for_proc(op_ctx->client_ctx, ctx_info_p,
				      config->node_props[ctx_idx].context,
				      /*user ctx */ &ctx_state);
		if (rc != 0) {
			op_ctx->error_info = DXDI_ERROR_BAD_CTX;
			goto ctx_init_exit;
		}

		ctx_mapped_n++;	/*ctx mapped successfully */

		/* context must be initialzed */
		if (ctx_state != CTX_STATE_INITIALIZED) {
			pr_err(
				    "Given context [%d] in invalid state for processing -%d\n",
				    ctx_idx, ctx_state);
			op_ctx->error_info = DXDI_ERROR_BAD_CTX;
			rc = -EINVAL;
			goto ctx_init_exit;
		}
	}

	ctx_info_p = &op_ctx->ctx_info;

	/* Start critical section -
	 * cache allocation must be coupled to descriptor enqueue */
	rc = mutex_lock_interruptible(&drvdata->desc_queue_sequencer);
	if (rc == 0) {		/* Coupled sequence */
		for (ctx_idx = 0; ctx_idx < op_ctx->ctx_info_num;
		     ctx_idx++, ctx_info_p++) {
			ctxmgr_set_sep_cache_idx(ctx_info_p,
						 ctxmgr_sep_cache_alloc
						 (drvdata->sep_cache,
						  ctxmgr_get_ctx_id(ctx_info_p),
						  &sep_ctx_load_req[ctx_idx]));
		}

		rc = send_combined_op_desc(op_ctx,
					   sep_ctx_load_req /*nothing to load */
					   ,
					   1 /*INIT*/, SEP_PROC_MODE_NOP,
					   0 /*no scheme in init */);

		mutex_unlock(&drvdata->desc_queue_sequencer);
	} else {
		pr_err("Failed locking descQ sequencer[%u]\n",
			    op_ctx->client_ctx->qid);
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
	}
	if (rc == 0)
		rc = wait_for_sep_op_result(op_ctx);

	crypto_op_completion_cleanup(op_ctx);

 ctx_init_exit:
	ctx_info_p = &op_ctx->ctx_info;

	for (ctx_idx = 0; ctx_idx < ctx_mapped_n; ctx_idx++, ctx_info_p++)
		ctxmgr_unmap_user_ctx(ctx_info_p);

	return rc;
}

/**
 * prepare_adata_for_sep() - Generate MLLI tables for additional/associated data
 *				(input only)
 * @op_ctx:	 Operation context
 * @adata_in:
 * @adata_in_size:
 *
 * Returns int
 */
static inline int prepare_adata_for_sep(struct sep_op_ctx *op_ctx,
					u8 __user *adata_in,
					u32 adata_in_size)
{
	struct sep_client_ctx *client_ctx = op_ctx->client_ctx;
	void *llimgr = client_ctx->drv_data->sep_data->llimgr;
	struct dma_pool *spad_buf_pool =
	    client_ctx->drv_data->sep_data->spad_buf_pool;
	struct mlli_tables_list *ift_p = &op_ctx->ift;
	unsigned long a0_buf_size;
	u8 *a0_buf_p;
	int rc = 0;

	if (adata_in == NULL) {	/*data_out required for this alg_class */
		pr_err("adata_in==NULL for authentication\n");
		op_ctx->error_info = DXDI_ERROR_INVAL_DIN_PTR;
		return -EINVAL;
	}

	op_ctx->spad_buf_p = dma_pool_alloc(spad_buf_pool,
					    GFP_KERNEL,
					    &op_ctx->spad_buf_dma_addr);
	if (unlikely(op_ctx->spad_buf_p == NULL)) {
		pr_err("Failed allocating from spad_buf_pool for A0\n");
		return -ENOMEM;
	}
	a0_buf_p = op_ctx->spad_buf_p;

	/* format A0 (the first 2 words in the first block) */
	if (adata_in_size < ((1UL << 16) - (1UL << 8))) {
		a0_buf_size = 2;

		a0_buf_p[0] = (adata_in_size >> 8) & 0xFF;
		a0_buf_p[1] = adata_in_size & 0xFF;
	} else {
		a0_buf_size = 6;

		a0_buf_p[0] = 0xFF;
		a0_buf_p[1] = 0xFE;
		a0_buf_p[2] = (adata_in_size >> 24) & 0xFF;
		a0_buf_p[3] = (adata_in_size >> 16) & 0xFF;
		a0_buf_p[4] = (adata_in_size >> 8) & 0xFF;
		a0_buf_p[5] = adata_in_size & 0xFF;
	}

	/* Create IFT (MLLI table) */
	rc = llimgr_register_client_dma_buf(llimgr,
					    adata_in, NULL, adata_in_size, 0,
					    DMA_TO_DEVICE,
					    &op_ctx->din_dma_obj);
	if (likely(rc == 0)) {
		rc = llimgr_create_mlli(llimgr, ift_p, DMA_TO_DEVICE,
					&op_ctx->din_dma_obj,
					op_ctx->spad_buf_dma_addr, a0_buf_size);
		if (unlikely(rc != 0)) {
			llimgr_deregister_client_dma_buf(llimgr,
							 &op_ctx->din_dma_obj);
		}
	}

	return rc;
}

/**
 * prepare_cipher_data_for_sep() - Generate MLLI tables for cipher algorithms
 *				(input + output)
 * @op_ctx:	 Operation context
 * @data_in:	User space pointer for input data (NULL for kernel data)
 * @sgl_in:	Kernel buffers s/g list for input data (NULL for user data)
 * @data_out:	User space pointer for output data (NULL for kernel data)
 * @sgl_out:	Kernel buffers s/g list for output data (NULL for user data)
 * @data_in_size:
 *
 * Returns int
 */
static inline int prepare_cipher_data_for_sep(struct sep_op_ctx *op_ctx,
					      u8 __user *data_in,
					      struct scatterlist *sgl_in,
					      u8 __user *data_out,
					      struct scatterlist *sgl_out,
					      u32 data_in_size)
{
	struct sep_client_ctx *client_ctx = op_ctx->client_ctx;
	void *llimgr = client_ctx->drv_data->sep_data->llimgr;
	const bool is_data_inplace =
	    data_in != NULL ? (data_in == data_out) : (sgl_in == sgl_out);
	const enum dma_data_direction din_dma_direction =
	    is_data_inplace ? DMA_BIDIRECTIONAL : DMA_TO_DEVICE;
	int rc;

	/* Check parameters */
	if (data_in_size == 0) {	/* No data to prepare */
		return 0;
	}
	if ((data_out == NULL) && (sgl_out == NULL)) {
		/* data_out required for this alg_class */
		pr_err("data_out/sgl_out==NULL for enc/decryption\n");
		op_ctx->error_info = DXDI_ERROR_INVAL_DOUT_PTR;
		return -EINVAL;
	}

	/* Avoid partial overlapping of data_in with data_out */
	if (!is_data_inplace)
		if (data_in != NULL) {	/* User space buffer */
			if (((data_in < data_out) &&
			     ((data_in + data_in_size) > data_out)) ||
			    ((data_out < data_in) &&
			     ((data_out + data_in_size) > data_in))) {
				pr_err("Buffers partially overlap!\n");
				op_ctx->error_info =
				    DXDI_ERROR_DIN_DOUT_OVERLAP;
				return -EINVAL;
			}
		}
	/* else: TODO - scan s/g lists for overlapping */

	/* Create IFT + OFT (MLLI tables) */
	rc = llimgr_register_client_dma_buf(llimgr,
					    data_in, sgl_in, data_in_size, 0,
					    din_dma_direction,
					    &op_ctx->din_dma_obj);
	if (likely(rc == 0)) {
		rc = llimgr_create_mlli(llimgr, &op_ctx->ift, din_dma_direction,
					&op_ctx->din_dma_obj, 0, 0);
	}
	if (likely(rc == 0)) {
		if (is_data_inplace) {
			/* Mirror IFT data in OFT */
			op_ctx->dout_dma_obj = op_ctx->din_dma_obj;
			op_ctx->oft = op_ctx->ift;
		} else {	/* Create OFT */
			rc = llimgr_register_client_dma_buf(llimgr,
							    data_out, sgl_out,
							    data_in_size, 0,
							    DMA_FROM_DEVICE,
							    &op_ctx->
							    dout_dma_obj);
			if (likely(rc == 0)) {
				rc = llimgr_create_mlli(llimgr, &op_ctx->oft,
							DMA_FROM_DEVICE,
							&op_ctx->dout_dma_obj,
							0, 0);
			}
		}
	}

	if (unlikely(rc != 0)) {	/* Failure cleanup */
		/* No output MLLI to free in error case */
		if (!is_data_inplace) {
			llimgr_deregister_client_dma_buf(llimgr,
							 &op_ctx->dout_dma_obj);
		}
		llimgr_destroy_mlli(llimgr, &op_ctx->ift);
		llimgr_deregister_client_dma_buf(llimgr, &op_ctx->din_dma_obj);
	}

	return rc;
}

/**
 * prepare_hash_data_for_sep() - Prepare data for hash operation
 * @op_ctx:		Operation context
 * @is_finalize:	Is hash finialize operation (last)
 * @data_in:		Pointer to user buffer OR...
 * @sgl_in:		Gather list for kernel buffers
 * @data_in_size:	Data size in bytes
 *
 * Returns 0 on success
 */
static int prepare_hash_data_for_sep(struct sep_op_ctx *op_ctx,
				     bool is_finalize,
				     u8 __user *data_in,
				     struct scatterlist *sgl_in,
				     u32 data_in_size)
{
	struct sep_client_ctx *client_ctx = op_ctx->client_ctx;
	struct queue_drvdata *drvdata = client_ctx->drv_data;
	void *llimgr = drvdata->sep_data->llimgr;
	u32 crypto_blk_size;
	/* data size for processing this op. (incl. prev. block remainder) */
	u32 data_size4hash;
	/* amount of data_in to process in this op. */
	u32 data_in_save4next;
	u16 last_hash_blk_tail_size;
	dma_addr_t last_hash_blk_tail_dma;
	int rc;

	if ((data_in != NULL) && (sgl_in != NULL)) {
		pr_err("Given valid data_in+sgl_in!\n");
		return -EINVAL;
	}

	/*Hash block size required in order to buffer block remainders */
	crypto_blk_size = ctxmgr_get_crypto_blk_size(&op_ctx->ctx_info);
	if (crypto_blk_size == 0) {	/* Unsupported algorithm?... */
		op_ctx->error_info = DXDI_ERROR_UNSUP;
		return -ENOSYS;
	}

	/* Map for DMA the last block tail, if any */
	rc = ctxmgr_map2dev_hash_tail(&op_ctx->ctx_info,
				      drvdata->sep_data->dev);
	if (rc != 0) {
		pr_err("Failed mapping hash data tail buffer\n");
		return rc;
	}
	last_hash_blk_tail_size =
	    ctxmgr_get_hash_blk_remainder_buf(&op_ctx->ctx_info,
					      &last_hash_blk_tail_dma);
	data_size4hash = data_in_size + last_hash_blk_tail_size;
	if (!is_finalize) {
		/* Not last. Round to hash block size. */
		data_size4hash = (data_size4hash & ~(crypto_blk_size - 1));
		/* On the last hash op. take all that's left */
	}
	data_in_save4next = (data_size4hash > 0) ?
	    data_in_size - (data_size4hash - last_hash_blk_tail_size) :
	    data_in_size;
	rc = llimgr_register_client_dma_buf(llimgr,
					    data_in, sgl_in, data_in_size,
					    data_in_save4next, DMA_TO_DEVICE,
					    &op_ctx->din_dma_obj);
	if (unlikely(rc != 0)) {
		pr_err("Failed registering client buffer (rc=%d)\n", rc);
	} else {
		if ((!is_finalize) && (data_size4hash == 0)) {
			/* Not enough for even one hash block
			 * (all saved for next) */
			ctxmgr_unmap2dev_hash_tail(&op_ctx->ctx_info,
						   drvdata->sep_data->dev);
			/* Append to existing tail buffer */
			rc = ctxmgr_save_hash_blk_remainder(&op_ctx->ctx_info,
							    &op_ctx->
							    din_dma_obj,
							    true /*append */);
			if (rc == 0)	/* signal: not even one block */
				rc = -ENOTBLK;
		} else {
			rc = llimgr_create_mlli(llimgr, &op_ctx->ift,
						DMA_TO_DEVICE,
						&op_ctx->din_dma_obj,
						last_hash_blk_tail_dma,
						last_hash_blk_tail_size);
		}
	}

	if (unlikely(rc != 0)) {	/* Failed (or -ENOTBLK) */
		/* No harm if we invoke deregister if it was not registered */
		llimgr_deregister_client_dma_buf(llimgr, &op_ctx->din_dma_obj);
		/* Unmap hash block tail buffer */
		ctxmgr_unmap2dev_hash_tail(&op_ctx->ctx_info,
					   drvdata->sep_data->dev);
	}
	/* Hash block remainder would be copied into tail buffer only after
	 * operation completion, because this buffer is still in use for
	 * current operation */

	return rc;
}

/**
 * prepare_mac_data_for_sep() - Prepare data memory objects for (AES) MAC
 *				operations
 * @op_ctx:
 * @data_in:		Pointer to user buffer OR...
 * @sgl_in:		Gather list for kernel buffers
 * @data_in_size:
 *
 * Returns int
 */
static inline int prepare_mac_data_for_sep(struct sep_op_ctx *op_ctx,
					   u8 __user *data_in,
					   struct scatterlist *sgl_in,
					   u32 data_in_size)
{
	struct sep_client_ctx *client_ctx = op_ctx->client_ctx;
	void *llimgr = client_ctx->drv_data->sep_data->llimgr;
	int rc;

	rc = llimgr_register_client_dma_buf(llimgr,
					    data_in, sgl_in, data_in_size, 0,
					    DMA_TO_DEVICE,
					    &op_ctx->din_dma_obj);
	if (likely(rc == 0)) {
		rc = llimgr_create_mlli(llimgr, &op_ctx->ift,
					DMA_TO_DEVICE, &op_ctx->din_dma_obj, 0,
					0);
		if (rc != 0) {
			llimgr_deregister_client_dma_buf(llimgr,
							 &op_ctx->din_dma_obj);
		}
	}

	return rc;
}

/**
 * prepare_data_for_sep() - Prepare data for processing by SeP
 * @op_ctx:
 * @data_in:	User space pointer for input data (NULL for kernel data)
 * @sgl_in:	Kernel buffers s/g list for input data (NULL for user data)
 * @data_out:	User space pointer for output data (NULL for kernel data)
 * @sgl_out:	Kernel buffers s/g list for output data (NULL for user data)
 * @data_in_size:	 data_in buffer size (and data_out's if not NULL)
 * @data_intent:	 the purpose of the given data
 *
 * Prepare data for processing by SeP
 * (common flow for sep_proc_dblk and sep_fin_proc) .
 * Returns int
 */
int prepare_data_for_sep(struct sep_op_ctx *op_ctx,
			 u8 __user *data_in,
			 struct scatterlist *sgl_in,
			 u8 __user *data_out,
			 struct scatterlist *sgl_out,
			 u32 data_in_size,
			 enum crypto_data_intent data_intent)
{
	int rc;
	enum crypto_alg_class alg_class;

	if (data_intent == CRYPTO_DATA_ADATA) {
		/* additional/associated data */
		if (!ctxmgr_is_valid_adata_size(&op_ctx->ctx_info,
						data_in_size)) {
			op_ctx->error_info = DXDI_ERROR_INVAL_DATA_SIZE;
			return -EINVAL;
		}
	} else {
		/* cipher/text data */
		if (!ctxmgr_is_valid_size(&op_ctx->ctx_info,
					  data_in_size,
					  (data_intent ==
					   CRYPTO_DATA_TEXT_FINALIZE))) {
			op_ctx->error_info = DXDI_ERROR_INVAL_DATA_SIZE;
			return -EINVAL;
		}
	}

	pr_debug("data_in=0x%p/0x%p data_out=0x%p/0x%p data_in_size=%uB\n",
		      data_in, sgl_in, data_out, sgl_out, data_in_size);

	alg_class = ctxmgr_get_alg_class(&op_ctx->ctx_info);
	pr_debug("alg_class = %d\n", alg_class);
	switch (alg_class) {
	case ALG_CLASS_SYM_CIPHER:
		rc = prepare_cipher_data_for_sep(op_ctx,
						 data_in, sgl_in, data_out,
						 sgl_out, data_in_size);
		break;
	case ALG_CLASS_AUTH_ENC:
		if (data_intent == CRYPTO_DATA_ADATA) {
			struct host_crypto_ctx_auth_enc *aead_ctx_p =
			    (struct host_crypto_ctx_auth_enc *)op_ctx->ctx_info.
			    ctx_kptr;

			if (!aead_ctx_p->is_adata_processed) {
				rc = prepare_adata_for_sep(op_ctx,
							   data_in,
							   data_in_size);
				/* no more invocation to adata process
				 * is allowed */
				aead_ctx_p->is_adata_processed = 1;
			} else {
				/* additional data may be processed
				 * only once */
				return -EPERM;
			}
		} else {
			rc = prepare_cipher_data_for_sep(op_ctx,
							 data_in, sgl_in,
							 data_out, sgl_out,
							 data_in_size);
		}
		break;
	case ALG_CLASS_MAC:
	case ALG_CLASS_HASH:
		if ((alg_class == ALG_CLASS_MAC) &&
		    (ctxmgr_get_mac_type(&op_ctx->ctx_info) != DXDI_MAC_HMAC)) {
			/* Handle all MACs but HMAC */
			if (data_in_size == 0) {	/* No data to prepare */
				rc = 0;
				break;
			}
#if 0
			/* ignore checking the user out pointer due to crys api
			 * limitation */
			if (data_out != NULL) {
				pr_err("data_out!=NULL for MAC\n");
				return -EINVAL;
			}
#endif
			rc = prepare_mac_data_for_sep(op_ctx, data_in, sgl_in,
						      data_in_size);
			break;
		}

		/* else: HASH or HMAC require the same handling */
		rc = prepare_hash_data_for_sep(op_ctx,
					       (data_intent ==
						CRYPTO_DATA_TEXT_FINALIZE),
					       data_in, sgl_in, data_in_size);
		break;

	default:
		pr_err("Invalid algorithm class %d in context\n",
			    alg_class);
		/* probably context was corrupted since init. phase */
		op_ctx->error_info = DXDI_ERROR_BAD_CTX;
		rc = -EINVAL;
	}

	return rc;
}

/**
 * prepare_combined_data_for_sep() - Prepare combined data for processing by SeP
 * @op_ctx:
 * @data_in:
 * @data_out:
 * @data_in_size:	 data_in buffer size (and data_out's if not NULL)
 * @data_intent:	 the purpose of the given data
 *
 * Prepare combined data for processing by SeP
 * (common flow for sep_proc_dblk and sep_fin_proc) .
 * Returns int
 */
static int prepare_combined_data_for_sep(struct sep_op_ctx *op_ctx,
					 u8 __user *data_in,
					 u8 __user *data_out,
					 u32 data_in_size,
					 enum crypto_data_intent data_intent)
{
	int rc;

	if (data_intent == CRYPTO_DATA_TEXT) {
		/* restrict data unit size to the max block size multiple */
		if (!IS_MULT_OF(data_in_size, SEP_HASH_BLOCK_SIZE_MAX)) {
			pr_err(
				    "Data unit size (%u) is not HASH block multiple\n",
				    data_in_size);
			op_ctx->error_info = DXDI_ERROR_INVAL_DATA_SIZE;
			return -EINVAL;
		}
	} else if (data_intent == CRYPTO_DATA_TEXT_FINALIZE) {
		/* user may finalize with zero or AES block size multiple */
		if (!IS_MULT_OF(data_in_size, SEP_AES_BLOCK_SIZE)) {
			pr_err("Data size (%u), not AES block multiple\n",
				    data_in_size);
			op_ctx->error_info = DXDI_ERROR_INVAL_DATA_SIZE;
			return -EINVAL;
		}
	}

	pr_debug("data_in=0x%08lX data_out=0x%08lX data_in_size=%uB\n",
		      (unsigned long)data_in, (unsigned long)data_out,
		      data_in_size);

	pr_debug("alg_class = COMBINED\n");
	if (data_out == NULL)
		rc = prepare_mac_data_for_sep(op_ctx,
					      data_in, NULL, data_in_size);
	else
		rc = prepare_cipher_data_for_sep(op_ctx,
						 data_in, NULL, data_out, NULL,
						 data_in_size);

	return rc;
}

/**
 * sep_proc_dblk() - Process data block
 * @op_ctx:
 * @context_buf:
 * @data_block_type:
 * @data_in:
 * @data_out:
 * @data_in_size:
 *
 * Returns int
 */
static int sep_proc_dblk(struct sep_op_ctx *op_ctx,
			 u32 __user *context_buf,
			 enum dxdi_data_block_type data_block_type,
			 u8 __user *data_in,
			 u8 __user *data_out, u32 data_in_size)
{
	struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;
	int rc;
	int sep_cache_load_required;
	int sep_ctx_init_required = 0;
	enum crypto_alg_class alg_class;
	enum host_ctx_state ctx_state;

	if (data_in_size == 0) {
		pr_err("Got empty data_in\n");
		op_ctx->error_info = DXDI_ERROR_INVAL_DATA_SIZE;
		return -EINVAL;
	}

	rc = map_ctx_for_proc(op_ctx->client_ctx, &op_ctx->ctx_info,
			      context_buf, &ctx_state);
	if (rc != 0) {
		op_ctx->error_info = DXDI_ERROR_BAD_CTX;
		return rc;
	}
	op_ctx->op_type = SEP_OP_CRYPTO_PROC;
	if (ctx_state == CTX_STATE_PARTIAL_INIT) {
		/* case of postponed sep context init. */
		sep_ctx_init_required = 1;
		op_ctx->op_type |= SEP_OP_CRYPTO_INIT;
	} else if (ctx_state != CTX_STATE_INITIALIZED) {
		pr_err("Context in invalid state for processing %d\n",
			    ctx_state);
		op_ctx->error_info = DXDI_ERROR_BAD_CTX;
		rc = -EINVAL;
		goto unmap_ctx_and_return;
	}
	alg_class = ctxmgr_get_alg_class(&op_ctx->ctx_info);

	rc = prepare_data_for_sep(op_ctx, data_in, NULL, data_out, NULL,
				  data_in_size,
				  (data_block_type ==
				   DXDI_DATA_TYPE_TEXT ? CRYPTO_DATA_TEXT :
				   CRYPTO_DATA_ADATA));
	if (rc != 0) {
		if (rc == -ENOTBLK) {
			/* Did not accumulate even a single hash block */
			/* The data_in already copied to context, in addition
			 * to existing data. Report as success with no op. */
			op_ctx->error_info = DXDI_ERROR_NULL;
			rc = 0;
		}
		goto unmap_ctx_and_return;
	}

	if (sep_ctx_init_required) {
		/* If this flag is set it implies that we have updated
		 * parts of the sep_ctx structure during data preparation -
		 * need to sync. context to memory (from host cache...) */
#ifdef DEBUG
		ctxmgr_dump_sep_ctx(&op_ctx->ctx_info);
#endif
		ctxmgr_sync_sep_ctx(&op_ctx->ctx_info, drvdata->sep_data->dev);
	}

	/* Start critical section -
	 * cache allocation must be coupled to descriptor enqueue */
	rc = mutex_lock_interruptible(&drvdata->desc_queue_sequencer);
	if (rc == 0) {		/* coupled sequence */
		ctxmgr_set_sep_cache_idx(&op_ctx->ctx_info,
			 ctxmgr_sep_cache_alloc(drvdata->
						sep_cache,
						ctxmgr_get_ctx_id
						(&op_ctx->ctx_info),
						&sep_cache_load_required));
		rc = send_crypto_op_desc(op_ctx, sep_cache_load_required,
					 sep_ctx_init_required,
					 (data_block_type ==
					  DXDI_DATA_TYPE_TEXT ?
					  SEP_PROC_MODE_PROC_T :
					  SEP_PROC_MODE_PROC_A));
		mutex_unlock(&drvdata->desc_queue_sequencer);
	} else {
		pr_err("Failed locking descQ sequencer[%u]\n",
			    op_ctx->client_ctx->qid);
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
	}
	if (rc == 0)
		rc = wait_for_sep_op_result(op_ctx);

	crypto_op_completion_cleanup(op_ctx);

 unmap_ctx_and_return:
	ctxmgr_unmap_user_ctx(&op_ctx->ctx_info);

	return rc;
}

/**
 * sep_fin_proc() - Finalize processing of given context with given (optional)
 *			data
 * @op_ctx:
 * @context_buf:
 * @data_in:
 * @data_out:
 * @data_in_size:
 * @digest_or_mac_p:
 * @digest_or_mac_size_p:
 *
 * Returns int
 */
static int sep_fin_proc(struct sep_op_ctx *op_ctx,
			u32 __user *context_buf,
			u8 __user *data_in,
			u8 __user *data_out,
			u32 data_in_size,
			u8 *digest_or_mac_p,
			u8 *digest_or_mac_size_p)
{
	struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;
	int rc;
	int sep_cache_load_required;
	int sep_ctx_init_required = 0;
	enum host_ctx_state ctx_state;

	rc = map_ctx_for_proc(op_ctx->client_ctx, &op_ctx->ctx_info,
			      context_buf, &ctx_state);
	if (rc != 0) {
		op_ctx->error_info = DXDI_ERROR_BAD_CTX;
		return rc;
	}
	if (ctx_state == CTX_STATE_PARTIAL_INIT) {
		/* case of postponed sep context init. */
		sep_ctx_init_required = 1;
	} else if (ctx_state != CTX_STATE_INITIALIZED) {
		pr_err("Context in invalid state for finalizing %d\n",
			    ctx_state);
		op_ctx->error_info = DXDI_ERROR_BAD_CTX;
		rc = -EINVAL;
		goto data_prepare_err;
	}

	op_ctx->op_type = SEP_OP_CRYPTO_FINI;

	rc = prepare_data_for_sep(op_ctx, data_in, NULL, data_out, NULL,
				  data_in_size, CRYPTO_DATA_TEXT_FINALIZE);
	if (rc != 0)
		goto data_prepare_err;

	if (sep_ctx_init_required) {
		/* If this flag is set it implies that we have updated
		 * parts of the sep_ctx structure during data preparation -
		 * need to sync. context to memory (from host cache...) */
#ifdef DEBUG
		ctxmgr_dump_sep_ctx(&op_ctx->ctx_info);
#endif
		ctxmgr_sync_sep_ctx(&op_ctx->ctx_info, drvdata->sep_data->dev);
	}

	/* Start critical section -
	 * cache allocation must be coupled to descriptor enqueue */
	rc = mutex_lock_interruptible(&drvdata->desc_queue_sequencer);
	if (rc == 0) {		/* Coupled sequence */
		ctxmgr_set_sep_cache_idx(&op_ctx->ctx_info,
				 ctxmgr_sep_cache_alloc(drvdata->sep_cache,
					ctxmgr_get_ctx_id(&op_ctx->ctx_info),
					&sep_cache_load_required));
		rc = send_crypto_op_desc(op_ctx, sep_cache_load_required,
					 sep_ctx_init_required,
					 SEP_PROC_MODE_FIN);
		mutex_unlock(&drvdata->desc_queue_sequencer);
	} else {
		pr_err("Failed locking descQ sequencer[%u]\n",
			    op_ctx->client_ctx->qid);
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
	}
	if (rc == 0)
		rc = wait_for_sep_op_result(op_ctx);

	if ((rc == 0) && (op_ctx->error_info == 0)) {
		/* Digest or MAC are embedded in the SeP/FW context */
		*digest_or_mac_size_p =
		    ctxmgr_get_digest_or_mac(&op_ctx->ctx_info,
					     digest_or_mac_p);
		/* If above is not applicable for given algorithm,
		 *digest_or_mac_size_p would be set to 0          */
	} else {
		/* Nothing valid in digest_or_mac_p */
		*digest_or_mac_size_p = 0;
	}

	crypto_op_completion_cleanup(op_ctx);

 data_prepare_err:
	ctxmgr_unmap_user_ctx(&op_ctx->ctx_info);

	return rc;
}

/**
 * sep_combined_proc_dblk() - Process Combined operation block
 * @op_ctx:
 * @config:
 * @data_in:
 * @data_out:
 * @data_in_size:
 *
 * Returns int
 */
static int sep_combined_proc_dblk(struct sep_op_ctx *op_ctx,
				  struct dxdi_combined_props *config,
				  u8 __user *data_in,
				  u8 __user *data_out,
				  u32 data_in_size)
{
	int rc;
	int ctx_idx, ctx_mapped_n = 0;
	int sep_cache_load_required[DXDI_COMBINED_NODES_MAX];
	enum host_ctx_state ctx_state;
	struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;
	struct client_crypto_ctx_info *ctx_info_p = &(op_ctx->ctx_info);
	u32 cfg_scheme;

	if (data_in_size == 0) {
		pr_err("Got empty data_in\n");
		op_ctx->error_info = DXDI_ERROR_INVAL_DATA_SIZE;
		return -EINVAL;
	}

	/* assume nothing to load */
	memset(sep_cache_load_required, 0, sizeof(sep_cache_load_required));

	/* set the number of active contexts */
	op_ctx->ctx_info_num = get_num_of_ctx_info(config);

	/* map each context in the configuration scheme */
	for (ctx_idx = 0; ctx_idx < op_ctx->ctx_info_num;
	     ctx_idx++, ctx_info_p++) {
		/* context already initialized by the user */
		rc = map_ctx_for_proc(op_ctx->client_ctx,
				      ctx_info_p,
				      config->node_props[ctx_idx].context,
				      /*user ctx */ &ctx_state);
		if (rc != 0) {
			op_ctx->error_info = DXDI_ERROR_BAD_CTX;
			goto unmap_ctx_and_return;
		}

		ctx_mapped_n++;	/*ctx mapped successfully */

		/* context must be initialzed */
		if (ctx_state != CTX_STATE_INITIALIZED) {
			pr_err(
				    "Given context [%d] in invalid state for processing -%d\n",
				    ctx_idx, ctx_state);
			op_ctx->error_info = DXDI_ERROR_BAD_CTX;
			rc = -EINVAL;
			goto unmap_ctx_and_return;
		}
	}

	op_ctx->op_type = SEP_OP_CRYPTO_PROC;

	/* Construct SeP combined scheme */
	cfg_scheme = format_sep_combined_cfg_scheme(config, op_ctx);
	pr_debug("SeP Config. Scheme: 0x%08X\n", cfg_scheme);

	rc = prepare_combined_data_for_sep(op_ctx, data_in, data_out,
					   data_in_size, CRYPTO_DATA_TEXT);
	if (unlikely(rc != 0)) {
		SEP_LOG_ERR(
			    "Failed preparing DMA buffers (rc=%d, err_info=0x%08X\n)\n",
			    rc, op_ctx->error_info);
		return rc;
	}

	ctx_info_p = &(op_ctx->ctx_info);

	/* Start critical section -
	 * cache allocation must be coupled to descriptor enqueue */
	rc = mutex_lock_interruptible(&drvdata->desc_queue_sequencer);
	if (rc == 0) {		/* coupled sequence */
		for (ctx_idx = 0; ctx_idx < op_ctx->ctx_info_num;
		     ctx_idx++, ctx_info_p++) {
			ctxmgr_set_sep_cache_idx(ctx_info_p,
						 ctxmgr_sep_cache_alloc
						 (drvdata->sep_cache,
						  ctxmgr_get_ctx_id(ctx_info_p),
						  &sep_cache_load_required
						  [ctx_idx]));
		}

		rc = send_combined_op_desc(op_ctx,
					   sep_cache_load_required, 0 /*INIT*/,
					   SEP_PROC_MODE_PROC_T, cfg_scheme);
		mutex_unlock(&drvdata->desc_queue_sequencer);
	} else {
		pr_err("Failed locking descQ sequencer[%u]\n",
			    op_ctx->client_ctx->qid);
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
	}
	if (rc == 0)
		rc = wait_for_sep_op_result(op_ctx);

	crypto_op_completion_cleanup(op_ctx);

 unmap_ctx_and_return:
	ctx_info_p = &(op_ctx->ctx_info);

	for (ctx_idx = 0; ctx_idx < ctx_mapped_n; ctx_idx++, ctx_info_p++)
		ctxmgr_unmap_user_ctx(ctx_info_p);

	return rc;
}

/**
 * sep_combined_fin_proc() - Finalize Combined processing
 *			     with given (optional) data
 * @op_ctx:
 * @config:
 * @data_in:
 * @data_out:
 * @data_in_size:
 * @auth_data_p:
 * @auth_data_size_p:
 *
 * Returns int
 */
static int sep_combined_fin_proc(struct sep_op_ctx *op_ctx,
				 struct dxdi_combined_props *config,
				 u8 __user *data_in,
				 u8 __user *data_out,
				 u32 data_in_size,
				 u8 *auth_data_p,
				 u8 *auth_data_size_p)
{
	int rc;
	int ctx_idx, ctx_mapped_n = 0;
	int sep_cache_load_required[DXDI_COMBINED_NODES_MAX];
	enum host_ctx_state ctx_state;
	struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;
	struct client_crypto_ctx_info *ctx_info_p = &(op_ctx->ctx_info);
	u32 cfg_scheme;

	/* assume nothing to load */
	memset(sep_cache_load_required, 0, sizeof(sep_cache_load_required));

	/* set the number of active contexts */
	op_ctx->ctx_info_num = get_num_of_ctx_info(config);

	/* map each context in the configuration scheme */
	for (ctx_idx = 0; ctx_idx < op_ctx->ctx_info_num;
	     ctx_idx++, ctx_info_p++) {
		/* context already initialized by the user */
		rc = map_ctx_for_proc(op_ctx->client_ctx,
				      ctx_info_p,
				      config->node_props[ctx_idx].context,
				      /*user ctx */ &ctx_state);
		if (rc != 0) {
			op_ctx->error_info = DXDI_ERROR_BAD_CTX;
			goto unmap_ctx_and_return;
		}

		ctx_mapped_n++;	/*ctx mapped successfully */

		/* context must be initialzed */
		if (ctx_state != CTX_STATE_INITIALIZED) {
			pr_err(
				    "Given context [%d] in invalid state for processing -%d\n",
				    ctx_idx, ctx_state);
			rc = -EINVAL;
			op_ctx->error_info = DXDI_ERROR_BAD_CTX;
			goto unmap_ctx_and_return;
		}
	}

	op_ctx->op_type = SEP_OP_CRYPTO_FINI;

	/* Construct SeP combined scheme */
	cfg_scheme = format_sep_combined_cfg_scheme(config, op_ctx);
	pr_debug("SeP Config. Scheme: 0x%08X\n", cfg_scheme);

	rc = prepare_combined_data_for_sep(op_ctx, data_in, data_out,
					   data_in_size,
					   CRYPTO_DATA_TEXT_FINALIZE);
	if (rc != 0)
		goto unmap_ctx_and_return;

	ctx_info_p = &(op_ctx->ctx_info);

	/* Start critical section -
	 * cache allocation must be coupled to descriptor enqueue */
	rc = mutex_lock_interruptible(&drvdata->desc_queue_sequencer);
	if (rc == 0) {		/* Coupled sequence */
		for (ctx_idx = 0; ctx_idx < op_ctx->ctx_info_num;
		     ctx_idx++, ctx_info_p++) {
			ctxmgr_set_sep_cache_idx(ctx_info_p,
						 ctxmgr_sep_cache_alloc
						 (drvdata->sep_cache,
						  ctxmgr_get_ctx_id(ctx_info_p),
						  &sep_cache_load_required
						  [ctx_idx]));
		}
		rc = send_combined_op_desc(op_ctx,
					   sep_cache_load_required, 0 /*INIT*/,
					   SEP_PROC_MODE_FIN, cfg_scheme);

		mutex_unlock(&drvdata->desc_queue_sequencer);
	} else {
		pr_err("Failed locking descQ sequencer[%u]\n",
			    op_ctx->client_ctx->qid);
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
	}
	if (rc == 0)
		rc = wait_for_sep_op_result(op_ctx);

	if (auth_data_size_p != NULL) {
		if ((rc == 0) && (op_ctx->error_info == 0)) {
			ctx_info_p = &(op_ctx->ctx_info);
			ctx_info_p += op_ctx->ctx_info_num - 1;

			/* Auth data embedded in the last SeP/FW context */
			*auth_data_size_p =
			    ctxmgr_get_digest_or_mac(ctx_info_p,
						     auth_data_p);
		} else {	/* Failure */
			*auth_data_size_p = 0;	/* Nothing valid */
		}
	}

	crypto_op_completion_cleanup(op_ctx);

 unmap_ctx_and_return:
	ctx_info_p = &(op_ctx->ctx_info);

	for (ctx_idx = 0; ctx_idx < ctx_mapped_n; ctx_idx++, ctx_info_p++)
		ctxmgr_unmap_user_ctx(ctx_info_p);

	return rc;
}

/**
 * process_combined_integrated() - Integrated processing of
 *				   Combined data (init+proc+fin)
 * @op_ctx:
 * @props:	 Initialization properties (see init_context)
 * @data_in:
 * @data_out:
 * @data_in_size:
 * @auth_data_p:
 * @auth_data_size_p:
 *
 * Returns int
 */
static int process_combined_integrated(struct sep_op_ctx *op_ctx,
				       struct dxdi_combined_props *config,
				       u8 __user *data_in,
				       u8 __user *data_out,
				       u32 data_in_size,
				       u8 *auth_data_p,
				       u8 *auth_data_size_p)
{
	int rc;
	int ctx_idx, ctx_mapped_n = 0;
	int sep_cache_load_required[DXDI_COMBINED_NODES_MAX];
	enum host_ctx_state ctx_state;
	struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;
	struct client_crypto_ctx_info *ctx_info_p = &(op_ctx->ctx_info);
	u32 cfg_scheme;

	/* assume nothing to load */
	memset(sep_cache_load_required, 0, sizeof(sep_cache_load_required));

	/* set the number of active contexts */
	op_ctx->ctx_info_num = get_num_of_ctx_info(config);

	/* map each context in the configuration scheme */
	for (ctx_idx = 0; ctx_idx < op_ctx->ctx_info_num;
	     ctx_idx++, ctx_info_p++) {
		/* context already initialized by the user */
		rc = map_ctx_for_proc(op_ctx->client_ctx,
				      ctx_info_p,
				      config->node_props[ctx_idx].context,
				      &ctx_state);
		if (rc != 0) {
			op_ctx->error_info = DXDI_ERROR_BAD_CTX;
			goto unmap_ctx_and_return;
		}

		ctx_mapped_n++;	/*ctx mapped successfully */

		if (ctx_state != CTX_STATE_INITIALIZED) {
			pr_err(
				    "Given context [%d] in invalid state for processing 0x%08X\n",
				    ctx_idx, ctx_state);
			op_ctx->error_info = DXDI_ERROR_BAD_CTX;
			rc = -EINVAL;
			goto unmap_ctx_and_return;
		}
	}

	op_ctx->op_type = SEP_OP_CRYPTO_FINI;
	/* reconstruct combined scheme */
	cfg_scheme = format_sep_combined_cfg_scheme(config, op_ctx);
	pr_debug("SeP Config. Scheme: 0x%08X\n", cfg_scheme);

	rc = prepare_combined_data_for_sep(op_ctx, data_in, data_out,
					   data_in_size,
					   CRYPTO_DATA_TEXT_FINALIZE /*last */
					   );
	if (rc != 0)
		goto unmap_ctx_and_return;

	ctx_info_p = &(op_ctx->ctx_info);

	/* Start critical section -
	 * cache allocation must be coupled to descriptor enqueue */
	rc = mutex_lock_interruptible(&drvdata->desc_queue_sequencer);
	if (rc == 0) {		/* Coupled sequence */
		for (ctx_idx = 0; ctx_idx < op_ctx->ctx_info_num;
		     ctx_idx++, ctx_info_p++) {
			ctxmgr_set_sep_cache_idx(ctx_info_p,
						 ctxmgr_sep_cache_alloc
						 (drvdata->sep_cache,
						  ctxmgr_get_ctx_id(ctx_info_p),
						  &sep_cache_load_required
						  [ctx_idx]));
		}
		rc = send_combined_op_desc(op_ctx,
					   sep_cache_load_required, 1 /*INIT*/,
					   SEP_PROC_MODE_FIN, cfg_scheme);

		mutex_unlock(&drvdata->desc_queue_sequencer);
	} else {
		pr_err("Failed locking descQ sequencer[%u]\n",
			    op_ctx->client_ctx->qid);
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
	}
	if (rc == 0)
		rc = wait_for_sep_op_result(op_ctx);

	if (auth_data_size_p != NULL) {
		if ((rc == 0) && (op_ctx->error_info == 0)) {
			ctx_info_p = &(op_ctx->ctx_info);
			ctx_info_p += op_ctx->ctx_info_num - 1;

			/* Auth data embedded in the last SeP/FW context */
			*auth_data_size_p =
			    ctxmgr_get_digest_or_mac(ctx_info_p,
						     auth_data_p);
		} else {	/* Failure */
			*auth_data_size_p = 0;	/* Nothing valid */
		}
	}

	crypto_op_completion_cleanup(op_ctx);

 unmap_ctx_and_return:
	ctx_info_p = &(op_ctx->ctx_info);

	for (ctx_idx = 0; ctx_idx < ctx_mapped_n; ctx_idx++, ctx_info_p++)
		ctxmgr_unmap_user_ctx(ctx_info_p);

	return rc;
}

/**
 * process_integrated() - Integrated processing of data (init+proc+fin)
 * @op_ctx:
 * @context_buf:
 * @alg_class:
 * @props:	 Initialization properties (see init_context)
 * @data_in:
 * @data_out:
 * @data_in_size:
 * @digest_or_mac_p:
 * @digest_or_mac_size_p:
 *
 * Returns int
 */
static int process_integrated(struct sep_op_ctx *op_ctx,
			      u32 __user *context_buf,
			      enum crypto_alg_class alg_class,
			      void *props,
			      u8 __user *data_in,
			      u8 __user *data_out,
			      u32 data_in_size,
			      u8 *digest_or_mac_p,
			      u8 *digest_or_mac_size_p)
{
	int rc;
	int sep_cache_load_required;
	bool postpone_init;
	struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;

	rc = ctxmgr_map_user_ctx(&op_ctx->ctx_info, drvdata->sep_data->dev,
				 alg_class, context_buf);
	if (rc != 0) {
		op_ctx->error_info = DXDI_ERROR_BAD_CTX;
		return rc;
	}
	ctxmgr_set_ctx_state(&op_ctx->ctx_info, CTX_STATE_UNINITIALIZED);
	/* Allocate a new Crypto context ID */
	ctxmgr_set_ctx_id(&op_ctx->ctx_info,
			  alloc_crypto_ctx_id(op_ctx->client_ctx));

	/* Algorithm class specific initialization */
	switch (alg_class) {
	case ALG_CLASS_SYM_CIPHER:
		rc = ctxmgr_init_symcipher_ctx(&op_ctx->ctx_info,
					       (struct dxdi_sym_cipher_props *)
					       props, &postpone_init,
					       &op_ctx->error_info);
		/* postpone_init would be ignored because this is an integrated
		 * operation - all required data would be updated in the
		 * context before the descriptor is sent */
		break;
	case ALG_CLASS_AUTH_ENC:
		rc = ctxmgr_init_auth_enc_ctx(&op_ctx->ctx_info,
					      (struct dxdi_auth_enc_props *)
					      props, &op_ctx->error_info);
		break;
	case ALG_CLASS_MAC:
		rc = ctxmgr_init_mac_ctx(&op_ctx->ctx_info,
					 (struct dxdi_mac_props *)props,
					 &op_ctx->error_info);
		break;
	case ALG_CLASS_HASH:
		rc = ctxmgr_init_hash_ctx(&op_ctx->ctx_info,
					  *((enum dxdi_hash_type *)props),
					  &op_ctx->error_info);
		break;
	default:
		pr_err("Invalid algorithm class %d\n", alg_class);
		op_ctx->error_info = DXDI_ERROR_UNSUP;
		rc = -EINVAL;
	}
	if (rc != 0)
		goto unmap_ctx_and_exit;

	ctxmgr_set_ctx_state(&op_ctx->ctx_info, CTX_STATE_PARTIAL_INIT);
	op_ctx->op_type = SEP_OP_CRYPTO_FINI;	/* Integrated is also fin. */
	rc = prepare_data_for_sep(op_ctx, data_in, NULL, data_out, NULL,
				  data_in_size,
				  CRYPTO_DATA_TEXT_FINALIZE /*last */);
	if (rc != 0)
		goto unmap_ctx_and_exit;

#ifdef DEBUG
	ctxmgr_dump_sep_ctx(&op_ctx->ctx_info);
#endif
	/* Flush sep_ctx out of host cache */
	ctxmgr_sync_sep_ctx(&op_ctx->ctx_info, drvdata->sep_data->dev);

	/* Start critical section -
	 * cache allocation must be coupled to descriptor enqueue */
	rc = mutex_lock_interruptible(&drvdata->desc_queue_sequencer);
	if (rc == 0) {
		/* Allocate SeP context cache entry */
		ctxmgr_set_sep_cache_idx(&op_ctx->ctx_info,
				 ctxmgr_sep_cache_alloc(drvdata->sep_cache,
					ctxmgr_get_ctx_id(&op_ctx->ctx_info),
					&sep_cache_load_required));
		if (!sep_cache_load_required)
			pr_err("New context already in SeP cache?!");
		/* Send descriptor with combined load+init+fin */
		rc = send_crypto_op_desc(op_ctx, 1 /*load */ , 1 /*INIT*/,
					 SEP_PROC_MODE_FIN);
		mutex_unlock(&drvdata->desc_queue_sequencer);

	} else {		/* failed acquiring mutex */
		pr_err("Failed locking descQ sequencer[%u]\n",
			    op_ctx->client_ctx->qid);
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
	}

	if (rc == 0)
		rc = wait_for_sep_op_result(op_ctx);

	if (digest_or_mac_size_p != NULL) {
		if ((rc == 0) && (op_ctx->error_info == 0)) {
			/* Digest or MAC are embedded in the SeP/FW context */
			*digest_or_mac_size_p =
			    ctxmgr_get_digest_or_mac(&op_ctx->ctx_info,
						     digest_or_mac_p);
		} else {	/* Failure */
			*digest_or_mac_size_p = 0;	/* Nothing valid */
		}
	}

	/* Hash tail buffer is never used/mapped in integrated op -->
	 * no need to unmap */

	crypto_op_completion_cleanup(op_ctx);

 unmap_ctx_and_exit:
	ctxmgr_unmap_user_ctx(&op_ctx->ctx_info);

	return rc;
}

/**
 * process_integrated_auth_enc() - Integrated processing of aead
 * @op_ctx:
 * @context_buf:
 * @alg_class:
 * @props:
 * @data_header:
 * @data_in:
 * @data_out:
 * @data_header_size:
 * @data_in_size:
 * @mac_p:
 * @mac_size_p:
 *
 * Integrated processing of authenticate & encryption of data
 * (init+proc_a+proc_t+fin)
 * Returns int
 */
static int process_integrated_auth_enc(struct sep_op_ctx *op_ctx,
				       u32 __user *context_buf,
				       enum crypto_alg_class alg_class,
				       void *props,
				       u8 __user *data_header,
				       u8 __user *data_in,
				       u8 __user *data_out,
				       u32 data_header_size,
				       u32 data_in_size,
				       u8 *mac_p, u8 *mac_size_p)
{
	int rc;
	int sep_cache_load_required;
	struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;

	rc = ctxmgr_map_user_ctx(&op_ctx->ctx_info, drvdata->sep_data->dev,
				 alg_class, context_buf);
	if (rc != 0) {
		op_ctx->error_info = DXDI_ERROR_BAD_CTX;
		goto integ_ae_exit;
	}

	ctxmgr_set_ctx_state(&op_ctx->ctx_info, CTX_STATE_UNINITIALIZED);
	/* Allocate a new Crypto context ID */
	ctxmgr_set_ctx_id(&op_ctx->ctx_info,
			  alloc_crypto_ctx_id(op_ctx->client_ctx));

	/* initialization */
	rc = ctxmgr_init_auth_enc_ctx(&op_ctx->ctx_info,
				      (struct dxdi_auth_enc_props *)props,
				      &op_ctx->error_info);
	if (rc != 0) {
		ctxmgr_unmap_user_ctx(&op_ctx->ctx_info);
		op_ctx->error_info = DXDI_ERROR_BAD_CTX;
		goto integ_ae_exit;
	}

	ctxmgr_set_ctx_state(&op_ctx->ctx_info, CTX_STATE_PARTIAL_INIT);
	/* Op. type is to init. the context and process Adata */
	op_ctx->op_type = SEP_OP_CRYPTO_INIT | SEP_OP_CRYPTO_PROC;
	/* prepare additional/assoc data */
	rc = prepare_data_for_sep(op_ctx, data_header, NULL, NULL, NULL,
				  data_header_size, CRYPTO_DATA_ADATA);
	if (rc != 0) {
		ctxmgr_unmap_user_ctx(&op_ctx->ctx_info);
		goto integ_ae_exit;
	}
#ifdef DEBUG
	ctxmgr_dump_sep_ctx(&op_ctx->ctx_info);
#endif
	/* Flush sep_ctx out of host cache */
	ctxmgr_sync_sep_ctx(&op_ctx->ctx_info, drvdata->sep_data->dev);

	/* Start critical section -
	 * cache allocation must be coupled to descriptor enqueue */
	rc = mutex_lock_interruptible(&drvdata->desc_queue_sequencer);
	if (rc == 0) {
		/* Allocate SeP context cache entry */
		ctxmgr_set_sep_cache_idx(&op_ctx->ctx_info,
			 ctxmgr_sep_cache_alloc(drvdata->sep_cache,
					ctxmgr_get_ctx_id(&op_ctx->ctx_info),
					&sep_cache_load_required));
		if (!sep_cache_load_required)
			pr_err("New context already in SeP cache?!");
		/* Send descriptor with combined load+init+fin */
		rc = send_crypto_op_desc(op_ctx, 1 /*load */ , 1 /*INIT*/,
					 SEP_PROC_MODE_PROC_A);
		mutex_unlock(&drvdata->desc_queue_sequencer);

	} else {		/* failed acquiring mutex */
		pr_err("Failed locking descQ sequencer[%u]\n",
			    op_ctx->client_ctx->qid);
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
	}

	/* set status and cleanup last descriptor */
	if (rc == 0)
		rc = wait_for_sep_op_result(op_ctx);
	crypto_op_completion_cleanup(op_ctx);
	ctxmgr_unmap_user_ctx(&op_ctx->ctx_info);

	/* process text data only on adata success */
	if ((rc == 0) && (op_ctx->error_info == 0)) {/* Init+Adata succeeded */
		/* reset pending descriptor and preserve operation
		 * context for the finalize phase */
		op_ctx->pending_descs_cntr = 1;
		/* process & finalize operation with entire user data */
		rc = sep_fin_proc(op_ctx, context_buf, data_in,
				  data_out, data_in_size, mac_p, mac_size_p);
	}

 integ_ae_exit:
	return rc;
}

/**
 * dxdi_data_dir_to_dma_data_dir() - Convert from DxDI DMA direction type to
 *					Linux kernel DMA direction type
 * @dxdi_dir:	 DMA direction in DxDI encoding
 *
 * Returns enum dma_data_direction
 */
enum dma_data_direction dxdi_data_dir_to_dma_data_dir(enum dxdi_data_direction
						      dxdi_dir)
{
	switch (dxdi_dir) {
	case DXDI_DATA_BIDIR:
		return DMA_BIDIRECTIONAL;
	case DXDI_DATA_TO_DEVICE:
		return DMA_TO_DEVICE;
	case DXDI_DATA_FROM_DEVICE:
		return DMA_FROM_DEVICE;
	default:
		return DMA_NONE;
	}
}

/**
 * dispatch_sep_rpc() - Dispatch a SeP RPC descriptor and process results
 * @op_ctx:
 * @agent_id:
 * @func_id:
 * @mem_refs:
 * @rpc_params_size:
 * @rpc_params:
 *
 * Returns int
 */
static int dispatch_sep_rpc(struct sep_op_ctx *op_ctx,
			    u16 agent_id,
			    u16 func_id,
			    struct dxdi_memref mem_refs[],
			    unsigned long rpc_params_size,
			    struct seprpc_params __user *rpc_params)
{
	int i, rc = 0;
	struct sep_client_ctx *client_ctx = op_ctx->client_ctx;
	struct queue_drvdata *drvdata = client_ctx->drv_data;
	enum dma_data_direction dma_dir;
	unsigned int num_of_mem_refs;
	int memref_idx;
	struct client_dma_buffer *local_dma_objs[SEP_RPC_MAX_MEMREF_PER_FUNC];
	struct mlli_tables_list mlli_tables[SEP_RPC_MAX_MEMREF_PER_FUNC];
	struct sep_sw_desc desc;
	struct seprpc_params *rpc_msg_p;

	/* Verify RPC message size */
	if (unlikely(SEP_RPC_MAX_MSG_SIZE < rpc_params_size)) {
		pr_err("Given rpc_params is too big (%lu B)\n",
		       rpc_params_size);
		return -EINVAL;
	}

	op_ctx->spad_buf_p = dma_pool_alloc(drvdata->sep_data->spad_buf_pool,
					    GFP_KERNEL,
					    &op_ctx->spad_buf_dma_addr);
	if (unlikely(op_ctx->spad_buf_p == NULL)) {
		pr_err("Fail: alloc from spad_buf_pool for RPC message\n");
		return -ENOMEM;
	}
	rpc_msg_p = (struct seprpc_params *)op_ctx->spad_buf_p;

	/* Copy params to DMA buffer of message */
	rc = copy_from_user(rpc_msg_p, rpc_params, rpc_params_size);
	if (rc) {
		pr_err("Fail: copy RPC message from user at 0x%p, rc=%d\n",
			    rpc_params, rc);
		return -EFAULT;
	}
	/* Get num. of memory references in host endianess */
	num_of_mem_refs = le32_to_cpu(rpc_msg_p->num_of_memrefs);

	/* Handle user memory references - prepare DMA buffers */
	if (unlikely(num_of_mem_refs > SEP_RPC_MAX_MEMREF_PER_FUNC)) {
		pr_err("agent_id=%d func_id=%d: Invalid # of memref %u\n",
			    agent_id, func_id, num_of_mem_refs);
		return -EINVAL;
	}
	for (i = 0; i < num_of_mem_refs; i++) {
		/* Init. tables lists for proper cleanup */
		MLLI_TABLES_LIST_INIT(mlli_tables + i);
		local_dma_objs[i] = NULL;
	}
	for (i = 0; i < num_of_mem_refs; i++) {
		pr_debug(
			"memref[%d]: id=%d dma_dir=%d start/offset 0x%08x size %u\n",
			i, mem_refs[i].ref_id, mem_refs[i].dma_direction,
			mem_refs[i].start_or_offset, mem_refs[i].size);

		/* convert DMA direction to enum dma_data_direction */
		dma_dir =
		    dxdi_data_dir_to_dma_data_dir(mem_refs[i].dma_direction);
		if (unlikely(dma_dir == DMA_NONE)) {
			pr_err(
				    "agent_id=%d func_id=%d: Invalid DMA direction (%d) for memref %d\n",
				    agent_id, func_id,
				    mem_refs[i].dma_direction, i);
			rc = -EINVAL;
			break;
		}
		/* Temporary memory registration if needed */
		if (IS_VALID_MEMREF_IDX(mem_refs[i].ref_id)) {
			memref_idx = mem_refs[i].ref_id;
			if (unlikely(mem_refs[i].start_or_offset != 0)) {
				pr_err(
					    "Offset in memref is not supported for RPC.\n");
				rc = -EINVAL;
				break;
			}
		} else {
			memref_idx = register_client_memref(client_ctx,
							    (u8 __user *)
							    mem_refs[i].
							    start_or_offset,
							    NULL,
							    mem_refs[i].size,
							    dma_dir);
			if (unlikely(!IS_VALID_MEMREF_IDX(memref_idx))) {
				pr_err("Fail: temp memory registration\n");
				rc = -ENOMEM;
				break;
			}
		}
		/* MLLI table creation */
		local_dma_objs[i] = acquire_dma_obj(client_ctx, memref_idx);
		if (unlikely(local_dma_objs[i] == NULL)) {
			pr_err("Failed acquiring DMA objects.\n");
			rc = -ENOMEM;
			break;
		}
		if (unlikely(local_dma_objs[i]->buf_size != mem_refs[i].size)) {
			pr_err("RPC: Partial memory ref not supported.\n");
			rc = -EINVAL;
			break;
		}
		rc = llimgr_create_mlli(drvdata->sep_data->llimgr,
					mlli_tables + i, dma_dir,
					local_dma_objs[i], 0, 0);
		if (unlikely(rc != 0))
			break;
		llimgr_mlli_to_seprpc_memref(&(mlli_tables[i]),
					     &(rpc_msg_p->memref[i]));
	}

	op_ctx->op_type = SEP_OP_RPC;
	if (rc == 0) {
		/* Pack SW descriptor */
		desc_q_pack_rpc_desc(&desc, op_ctx, agent_id, func_id,
				     rpc_params_size,
				     op_ctx->spad_buf_dma_addr);
		op_ctx->op_state = USER_OP_INPROC;
		/* Enqueue descriptor */
		rc = desc_q_enqueue(drvdata->desc_queue, &desc, true);
		if (likely(!IS_DESCQ_ENQUEUE_ERR(rc)))
			rc = 0;
	}

	if (likely(rc == 0))
		rc = wait_for_sep_op_result(op_ctx);
	else
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;

	/* Process descriptor completion */
	if ((rc == 0) && (op_ctx->error_info == 0)) {
		/* Copy back RPC message buffer */
		rc = copy_to_user(rpc_params, rpc_msg_p, rpc_params_size);
		if (rc) {
			pr_err(
				    "Failed copying back RPC parameters/message to user at 0x%p (rc=%d)\n",
				    rpc_params, rc);
			rc = -EFAULT;
		}
	}
	op_ctx->op_state = USER_OP_NOP;
	for (i = 0; i < num_of_mem_refs; i++) {
		/* Can call for all - unitialized mlli tables would have
		 * table_count == 0 */
		llimgr_destroy_mlli(drvdata->sep_data->llimgr, mlli_tables + i);
		if (local_dma_objs[i] != NULL) {
			release_dma_obj(client_ctx, local_dma_objs[i]);
			memref_idx =
			    DMA_OBJ_TO_MEMREF_IDX(client_ctx,
						  local_dma_objs[i]);
			if (memref_idx != mem_refs[i].ref_id) {
				/* Memory reference was temp. registered */
				(void)free_client_memref(client_ctx,
							 memref_idx);
			}
		}
	}

	return rc;
}

#if defined(SEP_PRINTF) && defined(DEBUG)
/* Replace component mask */
#undef SEP_LOG_CUR_COMPONENT
#define SEP_LOG_CUR_COMPONENT SEP_LOG_MASK_SEP_PRINTF
void sep_printf_handler(struct sep_drvdata *drvdata)
{
	int cur_ack_cntr;
	u32 gpr_val;
	int i;

	/* Reduce interrupts by polling until no more characters */
	/* Loop for at most a line - to avoid inifite looping in wq ctx */
	for (i = 0; i < SEP_PRINTF_LINE_SIZE; i++) {

		gpr_val = READ_REGISTER(drvdata->cc_base +
					SEP_PRINTF_S2H_GPR_OFFSET);
		cur_ack_cntr = gpr_val >> 8;
		/*
		 * ack as soon as possible (data is already in local variable)
		 * let SeP push one more character until we finish processing
		 */
		WRITE_REGISTER(drvdata->cc_base + SEP_PRINTF_H2S_GPR_OFFSET,
			       cur_ack_cntr);
#if 0
		pr_debug("%d. GPR=0x%08X (cur_ack=0x%08X , last=0x%08X)\n",
			      i, gpr_val, cur_ack_cntr, drvdata->last_ack_cntr);
#endif
		if (cur_ack_cntr == drvdata->last_ack_cntr)
			break;

		/* Identify lost characters case */
		if (cur_ack_cntr >
		    ((drvdata->last_ack_cntr + 1) & SEP_PRINTF_ACK_MAX)) {
			/* NULL terminate */
			drvdata->line_buf[drvdata->cur_line_buf_offset] = 0;
			if (sep_log_mask & SEP_LOG_CUR_COMPONENT)
				pr_info("SeP(lost %d): %s",
				       cur_ack_cntr - drvdata->last_ack_cntr
				       - 1, drvdata->line_buf);
			drvdata->cur_line_buf_offset = 0;
		}
		drvdata->last_ack_cntr = cur_ack_cntr;

		drvdata->line_buf[drvdata->cur_line_buf_offset] =
		    gpr_val & 0xFF;

		/* Is end of line? */
		if ((drvdata->line_buf[drvdata->cur_line_buf_offset] == '\n') ||
		    (drvdata->line_buf[drvdata->cur_line_buf_offset] == 0) ||
		    (drvdata->cur_line_buf_offset == (SEP_PRINTF_LINE_SIZE - 1))
		    ) {
			/* NULL terminate */
			drvdata->line_buf[drvdata->cur_line_buf_offset + 1] = 0;
			if (sep_log_mask & SEP_LOG_CUR_COMPONENT)
				pr_info("SeP: %s", drvdata->line_buf);
			drvdata->cur_line_buf_offset = 0;
		} else {
			drvdata->cur_line_buf_offset++;
		}

	}

}

/* Restore component mask */
#undef SEP_LOG_CUR_COMPONENT
#define SEP_LOG_CUR_COMPONENT SEP_LOG_MASK_MAIN
#endif				/*SEP_PRINTF */

static int sep_interrupt_process(struct sep_drvdata *drvdata)
{
	u32 cause_reg = 0;
	int i;

	/* read the interrupt status */
	cause_reg = READ_REGISTER(drvdata->cc_base +
				  DX_CC_REG_OFFSET(HOST, IRR));

	if (cause_reg == 0) {
		/* pr_debug("Got interrupt with empty cause_reg\n"); */
		return IRQ_NONE;
	}
#if 0
	pr_debug("cause_reg=0x%08X gpr5=0x%08X\n", cause_reg,
		      READ_REGISTER(drvdata->cc_base +
				    SEP_PRINTF_S2H_GPR_OFFSET));
#endif
	/* clear interrupt */
	WRITE_REGISTER(drvdata->cc_base + DX_CC_REG_OFFSET(HOST, ICR),
		       cause_reg);

#ifdef SEP_PRINTF
	if (cause_reg & SEP_HOST_GPR_IRQ_MASK(DX_SEP_HOST_PRINTF_GPR_IDX)) {
#ifdef DEBUG
		sep_printf_handler(drvdata);
#else				/* Just ack to SeP so it does not stall */
		WRITE_REGISTER(drvdata->cc_base + SEP_PRINTF_H2S_GPR_OFFSET,
			       READ_REGISTER(drvdata->cc_base +
					     SEP_PRINTF_S2H_GPR_OFFSET) >> 8);
#endif
		/* handled */
		cause_reg &= ~SEP_HOST_GPR_IRQ_MASK(DX_SEP_HOST_PRINTF_GPR_IDX);
	}
#endif

	if (cause_reg & SEP_HOST_GPR_IRQ_MASK(DX_SEP_STATE_GPR_IDX)) {
		dx_sep_state_change_handler(drvdata);
		cause_reg &= ~SEP_HOST_GPR_IRQ_MASK(DX_SEP_STATE_GPR_IDX);
	}

	if (cause_reg & SEP_HOST_GPR_IRQ_MASK(DX_SEP_REQUEST_GPR_IDX)) {
		if (drvdata->
		    irq_mask & SEP_HOST_GPR_IRQ_MASK(DX_SEP_REQUEST_GPR_IDX)) {
			dx_sep_req_handler(drvdata);
		}
		cause_reg &= ~SEP_HOST_GPR_IRQ_MASK(DX_SEP_REQUEST_GPR_IDX);
	}

	/* Check interrupt flag for each queue */
	for (i = 0; cause_reg && i < drvdata->num_of_desc_queues; i++) {
		if (cause_reg & gpr_interrupt_mask[i]) {
			desc_q_process_completed(drvdata->queue[i].desc_queue);
			cause_reg &= ~gpr_interrupt_mask[i];
		}
	}

	return IRQ_HANDLED;
}

#ifdef SEP_INTERRUPT_BY_TIMER
static void sep_timer(unsigned long arg)
{
	struct sep_drvdata *drvdata = (struct sep_drvdata *)arg;

	(void)sep_interrupt_process(drvdata);

	mod_timer(&drvdata->delegate, jiffies + msecs_to_jiffies(10));
}
#else
irqreturn_t sep_interrupt(int irq, void *dev_id)
{
	struct sep_drvdata *drvdata =
	    (struct sep_drvdata *)dev_get_drvdata((struct device *)dev_id);

	if (drvdata->sep_suspended) {
		WARN(1, "sep_interrupt rise in suspend!");
		return IRQ_HANDLED;
	}

	return sep_interrupt_process(drvdata);
}
#endif

/***** IOCTL commands handlers *****/

static int sep_ioctl_get_ver_major(unsigned long arg)
{
	u32 __user *ver_p = (u32 __user *)arg;
	const u32 ver_major = DXDI_VER_MAJOR;

	return put_user(ver_major, ver_p);
}

static int sep_ioctl_get_ver_minor(unsigned long arg)
{
	u32 __user *ver_p = (u32 __user *)arg;
	const u32 ver_minor = DXDI_VER_MINOR;

	return put_user(ver_minor, ver_p);
}

static int sep_ioctl_get_sym_cipher_ctx_size(unsigned long arg)
{
	struct dxdi_get_sym_cipher_ctx_size_params __user *user_params =
	    (struct dxdi_get_sym_cipher_ctx_size_params __user *)arg;
	enum dxdi_sym_cipher_type sym_cipher_type;
	const u32 ctx_size = ctxmgr_get_ctx_size(ALG_CLASS_SYM_CIPHER);
	int err;

	err = __get_user(sym_cipher_type, &(user_params->sym_cipher_type));
	if (err)
		return err;

	if (((sym_cipher_type >= _DXDI_SYMCIPHER_AES_FIRST) &&
	     (sym_cipher_type <= _DXDI_SYMCIPHER_AES_LAST)) ||
	    ((sym_cipher_type >= _DXDI_SYMCIPHER_DES_FIRST) &&
	     (sym_cipher_type <= _DXDI_SYMCIPHER_DES_LAST)) ||
	    ((sym_cipher_type >= _DXDI_SYMCIPHER_C2_FIRST) &&
	     (sym_cipher_type <= _DXDI_SYMCIPHER_C2_LAST))
	    ) {
		pr_debug("sym_cipher_type=%u\n", sym_cipher_type);
		return put_user(ctx_size, &(user_params->ctx_size));
	} else {
		pr_err("Invalid cipher type=%u\n", sym_cipher_type);
		return -EINVAL;
	}
}

static int sep_ioctl_get_auth_enc_ctx_size(unsigned long arg)
{
	struct dxdi_get_auth_enc_ctx_size_params __user *user_params =
	    (struct dxdi_get_auth_enc_ctx_size_params __user *)arg;
	enum dxdi_auth_enc_type ae_type;
	const u32 ctx_size = ctxmgr_get_ctx_size(ALG_CLASS_AUTH_ENC);
	int err;

	err = __get_user(ae_type, &(user_params->ae_type));
	if (err)
		return err;

	if ((ae_type == DXDI_AUTHENC_NONE) || (ae_type > DXDI_AUTHENC_MAX)) {
		pr_err("Invalid auth-enc. type=%u\n", ae_type);
		return -EINVAL;
	}

	pr_debug("A.E. type=%u\n", ae_type);
	return put_user(ctx_size, &(user_params->ctx_size));
}

static int sep_ioctl_get_mac_ctx_size(unsigned long arg)
{
	struct dxdi_get_mac_ctx_size_params __user *user_params =
	    (struct dxdi_get_mac_ctx_size_params __user *)arg;
	enum dxdi_mac_type mac_type;
	const u32 ctx_size = ctxmgr_get_ctx_size(ALG_CLASS_MAC);
	int err;

	err = __get_user(mac_type, &(user_params->mac_type));
	if (err)
		return err;

	if ((mac_type == DXDI_MAC_NONE) || (mac_type > DXDI_MAC_MAX)) {
		pr_err("Invalid MAC type=%u\n", mac_type);
		return -EINVAL;
	}

	pr_debug("MAC type=%u\n", mac_type);
	return put_user(ctx_size, &(user_params->ctx_size));
}

static int sep_ioctl_get_hash_ctx_size(unsigned long arg)
{
	struct dxdi_get_hash_ctx_size_params __user *user_params =
	    (struct dxdi_get_hash_ctx_size_params __user *)arg;
	enum dxdi_hash_type hash_type;
	const u32 ctx_size = ctxmgr_get_ctx_size(ALG_CLASS_HASH);
	int err;

	err = __get_user(hash_type, &(user_params->hash_type));
	if (err)
		return err;

	if ((hash_type == DXDI_HASH_NONE) || (hash_type > DXDI_HASH_MAX)) {
		pr_err("Invalid hash type=%u\n", hash_type);
		return -EINVAL;
	}

	pr_debug("hash type=%u\n", hash_type);
	return put_user(ctx_size, &(user_params->ctx_size));
}

static int sep_ioctl_sym_cipher_init(struct sep_client_ctx *client_ctx,
				     unsigned long arg)
{
	struct dxdi_sym_cipher_init_params __user *user_init_params =
			(struct dxdi_sym_cipher_init_params __user *)arg;
	struct dxdi_sym_cipher_init_params init_params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
		offsetof(struct dxdi_sym_cipher_init_params, error_info);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&init_params, user_init_params, input_size)) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);

	rc = init_crypto_context(&op_ctx, init_params.context_buf,
				 ALG_CLASS_SYM_CIPHER, &(init_params.props));

	/* Even on SeP error the function above
	 * returns 0 (operation completed with no host side errors) */
	put_user(op_ctx.error_info, &(user_init_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_auth_enc_init(struct sep_client_ctx *client_ctx,
				   unsigned long arg)
{
	struct dxdi_auth_enc_init_params __user *user_init_params =
	    (struct dxdi_auth_enc_init_params __user *)arg;
	struct dxdi_auth_enc_init_params init_params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
	    offsetof(struct dxdi_sym_cipher_init_params, error_info);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&init_params, user_init_params, input_size)) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = init_crypto_context(&op_ctx, init_params.context_buf,
				 ALG_CLASS_AUTH_ENC, &(init_params.props));
	/* Even on SeP error the function above
	 * returns 0 (operation completed with no host side errors) */
	put_user(op_ctx.error_info, &(user_init_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_mac_init(struct sep_client_ctx *client_ctx,
			      unsigned long arg)
{
	struct dxdi_mac_init_params __user *user_init_params =
	    (struct dxdi_mac_init_params __user *)arg;
	struct dxdi_mac_init_params init_params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
	    offsetof(struct dxdi_mac_init_params, error_info);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&init_params, user_init_params, input_size)) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = init_crypto_context(&op_ctx, init_params.context_buf,
				 ALG_CLASS_MAC, &(init_params.props));
	/* Even on SeP error the function above
	 * returns 0 (operation completed with no host side errors) */
	put_user(op_ctx.error_info, &(user_init_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_hash_init(struct sep_client_ctx *client_ctx,
			       unsigned long arg)
{
	struct dxdi_hash_init_params __user *user_init_params =
	    (struct dxdi_hash_init_params __user *)arg;
	struct dxdi_hash_init_params init_params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
	    offsetof(struct dxdi_hash_init_params, error_info);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&init_params, user_init_params, input_size)) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = init_crypto_context(&op_ctx, init_params.context_buf,
				 ALG_CLASS_HASH, &(init_params.hash_type));
	/* Even on SeP error the function above
	 * returns 0 (operation completed with no host side errors) */
	put_user(op_ctx.error_info, &(user_init_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_proc_dblk(struct sep_client_ctx *client_ctx,
			       unsigned long arg)
{
	struct dxdi_process_dblk_params __user *user_dblk_params =
	    (struct dxdi_process_dblk_params __user *)arg;
	struct dxdi_process_dblk_params dblk_params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
	    offsetof(struct dxdi_process_dblk_params, error_info);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&dblk_params, user_dblk_params, input_size)) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = sep_proc_dblk(&op_ctx, dblk_params.context_buf,
			   dblk_params.data_block_type,
			   dblk_params.data_in, dblk_params.data_out,
			   dblk_params.data_in_size);
	/* Even on SeP error the function above
	 * returns 0 (operation completed with no host side errors) */
	put_user(op_ctx.error_info, &(user_dblk_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_fin_proc(struct sep_client_ctx *client_ctx,
			      unsigned long arg)
{
	struct dxdi_fin_process_params __user *user_fin_params =
	    (struct dxdi_fin_process_params __user *)arg;
	struct dxdi_fin_process_params fin_params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
	    offsetof(struct dxdi_fin_process_params, digest_or_mac);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&fin_params, user_fin_params, input_size)) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = sep_fin_proc(&op_ctx, fin_params.context_buf,
			  fin_params.data_in, fin_params.data_out,
			  fin_params.data_in_size,
			  fin_params.digest_or_mac,
			  &(fin_params.digest_or_mac_size));
	/* Even on SeP error the function above
	 * returns 0 (operation completed with no host side errors) */
	if (rc == 0) {
		/* Always copy back digest/mac size + error_info */
		/* (that's the reason for keeping them together)  */
		rc = put_user(fin_params.digest_or_mac_size,
			      &user_fin_params->digest_or_mac_size);
		rc += put_user(fin_params.error_info,
				 &user_fin_params->error_info);

		/* We always need to copy back the digest/mac size (even if 0)
		 * in order to indicate validity of digest_or_mac buffer */
	}
	if ((rc == 0) && (op_ctx.error_info == 0) &&
	    (fin_params.digest_or_mac_size > 0)) {
		if (likely(fin_params.digest_or_mac_size <=
			   DXDI_DIGEST_SIZE_MAX)) {
			/* Copy back digest/mac if valid */
			rc = copy_to_user(&(user_fin_params->digest_or_mac),
					    fin_params.digest_or_mac,
					    fin_params.digest_or_mac_size);
		} else {	/* Invalid digest/mac size! */
			pr_err("Got invalid digest/MAC size = %u",
				    fin_params.digest_or_mac_size);
			op_ctx.error_info = DXDI_ERROR_INVAL_DATA_SIZE;
			rc = -EINVAL;
		}
	}

	/* Even on SeP error the function above
	 * returns 0 (operation completed with no host side errors) */
	put_user(op_ctx.error_info, &(user_fin_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_combined_init(struct sep_client_ctx *client_ctx,
				   unsigned long arg)
{
	struct dxdi_combined_init_params __user *user_init_params =
	    (struct dxdi_combined_init_params __user *)arg;
	struct dxdi_combined_init_params init_params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
	    offsetof(struct dxdi_combined_init_params, error_info);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&init_params, user_init_params, input_size)) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = init_combined_context(&op_ctx, &(init_params.props));
	/* Even on SeP error the function above
	 * returns 0 (operation completed with no host side errors) */
	put_user(op_ctx.error_info, &(user_init_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_combined_proc_dblk(struct sep_client_ctx *client_ctx,
					unsigned long arg)
{
	struct dxdi_combined_proc_dblk_params __user *user_dblk_params =
	    (struct dxdi_combined_proc_dblk_params __user *)arg;
	struct dxdi_combined_proc_dblk_params dblk_params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
	    offsetof(struct dxdi_combined_proc_dblk_params, error_info);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&dblk_params, user_dblk_params, input_size)) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = sep_combined_proc_dblk(&op_ctx, &dblk_params.props,
				    dblk_params.data_in, dblk_params.data_out,
				    dblk_params.data_in_size);
	/* Even on SeP error the function above
	 * returns 0 (operation completed with no host side errors) */
	put_user(op_ctx.error_info, &(user_dblk_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_combined_fin_proc(struct sep_client_ctx *client_ctx,
				       unsigned long arg)
{
	struct dxdi_combined_proc_params __user *user_fin_params =
	    (struct dxdi_combined_proc_params __user *)arg;
	struct dxdi_combined_proc_params fin_params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
	    offsetof(struct dxdi_combined_proc_params, error_info);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&fin_params, user_fin_params, input_size)) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = sep_combined_fin_proc(&op_ctx, &fin_params.props,
				   fin_params.data_in, fin_params.data_out,
				   fin_params.data_in_size,
				   fin_params.auth_data,
				   &(fin_params.auth_data_size));

	if (rc == 0) {
		/* Always copy back digest size + error_info */
		/* (that's the reason for keeping them together)  */
		rc = put_user(fin_params.auth_data_size,
			      &user_fin_params->auth_data_size);
		rc += put_user(fin_params.error_info,
				 &user_fin_params->error_info);

		/* We always need to copy back the digest size (even if 0)
		 * in order to indicate validity of digest buffer */
	}

	if ((rc == 0) && (op_ctx.error_info == 0)) {
		if (likely((fin_params.auth_data_size > 0) &&
			   (fin_params.auth_data_size <=
			    DXDI_DIGEST_SIZE_MAX))) {
			/* Copy back auth if valid */
			rc = copy_to_user(&(user_fin_params->auth_data),
					    fin_params.auth_data,
					    fin_params.auth_data_size);
		}
	}

	/* Even on SeP error the function above
	 * returns 0 (operation completed with no host side errors) */
	put_user(op_ctx.error_info, &(user_fin_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_combined_proc(struct sep_client_ctx *client_ctx,
				   unsigned long arg)
{
	struct dxdi_combined_proc_params __user *user_params =
	    (struct dxdi_combined_proc_params __user *)arg;
	struct dxdi_combined_proc_params params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
	    offsetof(struct dxdi_combined_proc_params, error_info);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&params, user_params, input_size)) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = process_combined_integrated(&op_ctx, &(params.props),
					 params.data_in, params.data_out,
					 params.data_in_size, params.auth_data,
					 &(params.auth_data_size));

	if (rc == 0) {
		/* Always copy back digest size + error_info */
		/* (that's the reason for keeping them together)  */
		rc = put_user(params.auth_data_size,
			      &user_params->auth_data_size);
		rc += put_user(params.error_info, &user_params->error_info);

		/* We always need to copy back the digest size (even if 0)
		 * in order to indicate validity of digest buffer */
	}

	if ((rc == 0) && (op_ctx.error_info == 0)) {
		if (likely((params.auth_data_size > 0) &&
			   (params.auth_data_size <= DXDI_DIGEST_SIZE_MAX))) {
			/* Copy back auth if valid */
			rc = copy_to_user(&(user_params->auth_data),
					  params.auth_data,
					  params.auth_data_size);
		}
	}

	/* Even on SeP error the function above
	 * returns 0 (operation completed with no host side errors) */
	put_user(op_ctx.error_info, &(user_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_sym_cipher_proc(struct sep_client_ctx *client_ctx,
				     unsigned long arg)
{
	struct dxdi_sym_cipher_proc_params __user *user_params =
	    (struct dxdi_sym_cipher_proc_params __user *)arg;
	struct dxdi_sym_cipher_proc_params params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
	    offsetof(struct dxdi_sym_cipher_proc_params, error_info);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&params, user_params, input_size)) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = process_integrated(&op_ctx, params.context_buf,
				ALG_CLASS_SYM_CIPHER, &(params.props),
				params.data_in, params.data_out,
				params.data_in_size, NULL, NULL);

	/* Even on SeP error the function above
	 * returns 0 (operation completed with no host side errors) */
	put_user(op_ctx.error_info, &(user_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_auth_enc_proc(struct sep_client_ctx *client_ctx,
				   unsigned long arg)
{
	struct dxdi_auth_enc_proc_params __user *user_params =
	    (struct dxdi_auth_enc_proc_params __user *)arg;
	struct dxdi_auth_enc_proc_params params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
	    offsetof(struct dxdi_auth_enc_proc_params, tag);
	int rc;
	u8 tag_size;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&params, user_params, input_size)) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);

	if (params.props.adata_size == 0) {
		/* without assoc data we can optimize for one descriptor
		 * sequence */
		rc = process_integrated(&op_ctx, params.context_buf,
					ALG_CLASS_AUTH_ENC, &(params.props),
					params.text_data, params.data_out,
					params.props.text_size, params.tag,
					&tag_size);
	} else {
		/* Integrated processing with auth. enc. algorithms with
		 * Additional-Data requires special two-descriptors flow */
		rc = process_integrated_auth_enc(&op_ctx, params.context_buf,
						 ALG_CLASS_AUTH_ENC,
						 &(params.props), params.adata,
						 params.text_data,
						 params.data_out,
						 params.props.adata_size,
						 params.props.text_size,
						 params.tag, &tag_size);

	}

	if ((rc == 0) && (tag_size != params.props.tag_size)) {
		pr_warn(
			"Tag result size different than requested (%u != %u)\n",
			tag_size, params.props.tag_size);
	}

	if ((rc == 0) && (op_ctx.error_info == 0) && (tag_size > 0)) {
		if (likely(tag_size <= DXDI_DIGEST_SIZE_MAX)) {
			/* Copy back digest/mac if valid */
			rc = __copy_to_user(&(user_params->tag), params.tag,
					    tag_size);
		} else {	/* Invalid digest/mac size! */
			pr_err("Got invalid tag size = %u", tag_size);
			op_ctx.error_info = DXDI_ERROR_INVAL_DATA_SIZE;
			rc = -EINVAL;
		}
	}

	/* Even on SeP error the function above
	 * returns 0 (operation completed with no host side errors) */
	put_user(op_ctx.error_info, &(user_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_mac_proc(struct sep_client_ctx *client_ctx,
			      unsigned long arg)
{
	struct dxdi_mac_proc_params __user *user_params =
	    (struct dxdi_mac_proc_params __user *)arg;
	struct dxdi_mac_proc_params params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
	    offsetof(struct dxdi_mac_proc_params, mac);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&params, user_params, input_size)) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = process_integrated(&op_ctx, params.context_buf,
				ALG_CLASS_MAC, &(params.props),
				params.data_in, NULL, params.data_in_size,
				params.mac, &(params.mac_size));

	if (rc == 0) {
		/* Always copy back mac size + error_info */
		/* (that's the reason for keeping them together)  */
		rc = put_user(params.mac_size, &user_params->mac_size);
		rc += put_user(params.error_info, &user_params->error_info);

		/* We always need to copy back the mac size (even if 0)
		 * in order to indicate validity of mac buffer */
	}

	if ((rc == 0) && (op_ctx.error_info == 0)) {
		if (likely((params.mac_size > 0) &&
			   (params.mac_size <= DXDI_DIGEST_SIZE_MAX))) {
			/* Copy back mac if valid */
			rc = copy_to_user(&(user_params->mac), params.mac,
					  params.mac_size);
		} else {	/* Invalid mac size! */
			pr_err("Got invalid MAC size = %u",
				    params.mac_size);
			op_ctx.error_info = DXDI_ERROR_INVAL_DATA_SIZE;
			rc = -EINVAL;
		}
	}

	/* Even on SeP error the function above
	 * returns 0 (operation completed with no host side errors) */
	put_user(op_ctx.error_info, &(user_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_hash_proc(struct sep_client_ctx *client_ctx,
			       unsigned long arg)
{
	struct dxdi_hash_proc_params __user *user_params =
	    (struct dxdi_hash_proc_params __user *)arg;
	struct dxdi_hash_proc_params params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
	    offsetof(struct dxdi_hash_proc_params, digest);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&params, user_params, input_size)) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = process_integrated(&op_ctx, params.context_buf,
				ALG_CLASS_HASH, &(params.hash_type),
				params.data_in, NULL, params.data_in_size,
				params.digest, &(params.digest_size));

	if (rc == 0) {
		/* Always copy back digest size + error_info */
		/* (that's the reason for keeping them together)  */
		rc = put_user(params.digest_size, &user_params->digest_size);
		rc += put_user(params.error_info, &user_params->error_info);

		/* We always need to copy back the digest size (even if 0)
		 * in order to indicate validity of digest buffer */
	}

	if ((rc == 0) && (op_ctx.error_info == 0)) {
		if (likely((params.digest_size > 0) &&
			   (params.digest_size <= DXDI_DIGEST_SIZE_MAX))) {
			/* Copy back mac if valid */
			rc = copy_to_user(&(user_params->digest),
					  params.digest, params.digest_size);
		} else {	/* Invalid digest size! */
			pr_err("Got invalid digest size = %u",
				    params.digest_size);
			op_ctx.error_info = DXDI_ERROR_INVAL_DATA_SIZE;
			rc = -EINVAL;
		}
	}

	/* Even on SeP error the function above
	 * returns 0 (operation completed with no host side errors) */
	put_user(op_ctx.error_info, &(user_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_sep_rpc(struct sep_client_ctx *client_ctx,
			     unsigned long arg)
{

	struct dxdi_sep_rpc_params __user *user_params =
	    (struct dxdi_sep_rpc_params __user *)arg;
	struct dxdi_sep_rpc_params params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
	    offsetof(struct dxdi_sep_rpc_params, error_info);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&params, user_params, input_size)) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = dispatch_sep_rpc(&op_ctx, params.agent_id, params.func_id,
			      params.mem_refs, params.rpc_params_size,
			      params.rpc_params);

	put_user(op_ctx.error_info, &(user_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

#if MAX_SEPAPP_SESSION_PER_CLIENT_CTX > 0
static int sep_ioctl_register_mem4dma(struct sep_client_ctx *client_ctx,
				      unsigned long arg)
{

	struct dxdi_register_mem4dma_params __user *user_params =
	    (struct dxdi_register_mem4dma_params __user *)arg;
	struct dxdi_register_mem4dma_params params;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
	    offsetof(struct dxdi_register_mem4dma_params, memref_id);
	enum dma_data_direction dma_dir;
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&params, user_params, input_size)) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}

	/* convert DMA direction to enum dma_data_direction */
	dma_dir = dxdi_data_dir_to_dma_data_dir(params.memref.dma_direction);
	if (unlikely(dma_dir == DMA_NONE)) {
		pr_err("Invalid DMA direction (%d)\n",
			    params.memref.dma_direction);
		rc = -EINVAL;
	} else {
		params.memref_id = register_client_memref(client_ctx,
							  (u8 __user *)
							  params.memref.
							  start_or_offset, NULL,
							  params.memref.size,
							  dma_dir);
		if (unlikely(!IS_VALID_MEMREF_IDX(params.memref_id))) {
			rc = -ENOMEM;
		} else {
			rc = put_user(params.memref_id,
				      &(user_params->memref_id));
			if (rc != 0)	/* revert if failed __put_user */
				(void)free_client_memref(client_ctx,
							 params.memref_id);
		}
	}

	return rc;
}

static int sep_ioctl_free_mem4dma(struct sep_client_ctx *client_ctx,
				  unsigned long arg)
{
	struct dxdi_free_mem4dma_params __user *user_params =
	    (struct dxdi_free_mem4dma_params __user *)arg;
	int memref_id;
	int err;

	/* access permissions to arg was already checked in sep_ioctl */
	err = __get_user(memref_id, &user_params->memref_id);
	if (err) {
		pr_err("Failed reading input parameter\n");
		return -EFAULT;
	}

	return free_client_memref(client_ctx, memref_id);
}
#endif

static int sep_ioctl_set_iv(struct sep_client_ctx *client_ctx,
			    unsigned long arg)
{
	struct dxdi_aes_iv_params *user_params =
	    (struct dxdi_aes_iv_params __user *)arg;
	struct dxdi_aes_iv_params params;
	struct host_crypto_ctx_sym_cipher *host_context =
	    (struct host_crypto_ctx_sym_cipher *)user_params->context_buf;
	struct crypto_ctx_uid uid;
	int err;

	/* Copy ctx uid from user context */
	if (copy_from_user(&uid, &host_context->uid,
			   sizeof(struct crypto_ctx_uid))) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}

	/* Copy IV from user context */
	if (__copy_from_user(&params, user_params,
			     sizeof(struct dxdi_aes_iv_params))) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}
	err =
	    ctxmgr_set_symcipher_iv_user(user_params->context_buf,
					 params.iv_ptr);
	if (err != 0)
		return err;

	ctxmgr_sep_cache_invalidate(client_ctx->drv_data->sep_cache,
				    uid, CRYPTO_CTX_ID_SINGLE_MASK);

	return 0;
}

static int sep_ioctl_get_iv(struct sep_client_ctx *client_ctx,
			    unsigned long arg)
{
	struct dxdi_aes_iv_params *user_params =
	    (struct dxdi_aes_iv_params __user *)arg;
	struct dxdi_aes_iv_params params;
	int err;

	/* copy context ptr from user */
	if (__copy_from_user(&params, user_params, sizeof(u32))) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}

	err =
	    ctxmgr_get_symcipher_iv_user(user_params->context_buf,
					 params.iv_ptr);
	if (err != 0)
		return err;

	if (copy_to_user(user_params, &params,
	    sizeof(struct dxdi_aes_iv_params))) {
		pr_err("Failed writing input parameters");
		return -EFAULT;
	}

	return 0;
}

/****** Driver entry points (open, release, ioctl, etc.) ******/

/**
 * init_client_ctx() - Initialize a client context object given
 * @drvdata:	Queue driver context
 * @client_ctx:
 *
 * Returns void
 */
void init_client_ctx(struct queue_drvdata *drvdata,
		     struct sep_client_ctx *client_ctx)
{
	int i;
	const unsigned int qid = drvdata - (drvdata->sep_data->queue);

	/* Initialize user data structure */
	client_ctx->qid = qid;
	client_ctx->drv_data = drvdata;
	atomic_set(&client_ctx->uid_cntr, 0);
#if MAX_SEPAPP_SESSION_PER_CLIENT_CTX > 0
	/* Initialize sessions */
	for (i = 0; i < MAX_SEPAPP_SESSION_PER_CLIENT_CTX; i++) {
		mutex_init(&client_ctx->sepapp_sessions[i].session_lock);
		client_ctx->sepapp_sessions[i].sep_session_id =
		    SEP_SESSION_ID_INVALID;
		/* The rest of the fields are 0/NULL from kzalloc */
	}
#endif
	/* Initialize memrefs */
	for (i = 0; i < MAX_REG_MEMREF_PER_CLIENT_CTX; i++) {
		mutex_init(&client_ctx->reg_memrefs[i].buf_lock);
		/* The rest of the fields are 0/NULL from kzalloc */
	}

	init_waitqueue_head(&client_ctx->memref_wq);
	client_ctx->memref_cnt = 0;
}

/**
 * sep_open() - "open" device file entry point.
 * @inode:
 * @file:
 *
 * Returns int
 */
static int sep_open(struct inode *inode, struct file *file)
{
	struct queue_drvdata *drvdata;
	struct sep_client_ctx *client_ctx;
	unsigned int qid;

	drvdata = container_of(inode->i_cdev, struct queue_drvdata, cdev);

	if (imajor(inode) != MAJOR(drvdata->sep_data->devt_base)) {
		pr_err("Invalid major device num=%d\n", imajor(inode));
		return -ENOENT;
	}
	qid = iminor(inode) - MINOR(drvdata->sep_data->devt_base);
	if (qid >= drvdata->sep_data->num_of_desc_queues) {
		pr_err("Invalid minor device num=%d\n", iminor(inode));
		return -ENOENT;
	}
#ifdef DEBUG
	/* The qid based on the minor device number must match the offset
	 * of given drvdata in the queues array of the sep_data context */
	if (qid != (drvdata - (drvdata->sep_data->queue))) {
		pr_err("qid=%d but drvdata index is %d\n",
			    qid, (drvdata - (drvdata->sep_data->queue)));
		return -EINVAL;
	}
#endif
	pr_debug("qid=%d\n", qid);

	client_ctx = kzalloc(sizeof(*client_ctx), GFP_KERNEL);
	if (client_ctx == NULL)
		return -ENOMEM;

	init_client_ctx(drvdata, client_ctx);

	file->private_data = client_ctx;

	return 0;
}

void cleanup_client_ctx(struct queue_drvdata *drvdata,
			struct sep_client_ctx *client_ctx)
{
	int memref_id;
#if MAX_SEPAPP_SESSION_PER_CLIENT_CTX > 0
	struct sep_op_ctx op_ctx;
	int session_id;
	struct crypto_ctx_uid uid;

	/* Free any Applet session left open */
	for (session_id = 0; session_id < MAX_SEPAPP_SESSION_PER_CLIENT_CTX;
	     session_id++) {
		if (IS_VALID_SESSION_CTX
		    (&client_ctx->sepapp_sessions[session_id])) {
			pr_debug("Closing session ID=%d\n", session_id);
			op_ctx_init(&op_ctx, client_ctx);
			sepapp_session_close(&op_ctx, session_id);
			/* Note: There is never a problem with the session's
			 * ref_cnt because when "release" is invoked there
			 * are no pending IOCTLs, so ref_cnt is at most 1 */
			op_ctx_fini(&op_ctx);
		}
		mutex_destroy(&client_ctx->sepapp_sessions[session_id].
			      session_lock);
	}
#endif				/*MAX_SEPAPP_SESSION_PER_CLIENT_CTX > 0 */

	/* Free registered user memory references */
	for (memref_id = 0; memref_id < MAX_REG_MEMREF_PER_CLIENT_CTX;
	     memref_id++) {
		if (client_ctx->reg_memrefs[memref_id].ref_cnt > 0) {
			pr_debug("Freeing user memref ID=%d\n", memref_id);
			(void)free_client_memref(client_ctx, memref_id);
		}
		/* There is no problem with memref ref_cnt because when
		 * "release" is invoked there are no pending IOCTLs,
		 * so ref_cnt is at most 1                             */
		mutex_destroy(&client_ctx->reg_memrefs[memref_id].buf_lock);
	}

	/* Invalidate any outstanding descriptors associated with this
	 * client_ctx */
	desc_q_mark_invalid_cookie(drvdata->desc_queue, (void *)client_ctx);

	uid.addr = ((u64) (unsigned long)client_ctx);
	uid.cntr = 0;

	/* Invalidate any crypto context cache entry associated with this client
	 * context before freeing context data object that may be reused.
	 * This assures retaining of UIDs uniqueness (and makes sense since all
	 * associated contexts does not exist anymore) */
	ctxmgr_sep_cache_invalidate(drvdata->sep_cache, uid,
				    CRYPTO_CTX_ID_CLIENT_MASK);
}

static int sep_release(struct inode *inode, struct file *file)
{
	struct sep_client_ctx *client_ctx = file->private_data;
	struct queue_drvdata *drvdata = client_ctx->drv_data;

	cleanup_client_ctx(drvdata, client_ctx);

	kfree(client_ctx);

	return 0;
}

static ssize_t
sep_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
	pr_debug("Invoked for %zu bytes", count);
	return -ENOSYS;		/* nothing to read... IOCTL only */
}

/*!
 * The SeP device does not support read/write
 * We use it for debug purposes. Currently loopback descriptor is sent given
 * number of times. Usage example: echo 10 > /dev/dx_sep_q0
 * TODO: Move this functionality to sysfs?
 *
 * \param filp
 * \param buf
 * \param count
 * \param ppos
 *
 * \return ssize_t
 */
static ssize_t
sep_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
#ifdef DEBUG
	struct sep_sw_desc desc;
	struct sep_client_ctx *client_ctx = filp->private_data;
	struct sep_op_ctx op_ctx;
	unsigned int loop_times = 1;
	unsigned int i;
	int rc = 0;
	char tmp_buf[80];

	if (count > 79)
		return -ENOMEM;	/* Avoid buffer overflow */
	memcpy(tmp_buf, buf, count);	/* Copy to NULL terminate */
	tmp_buf[count] = 0;	/* NULL terminate */

	/*pr_debug("Invoked for %u bytes", count); */
	sscanf(buf, "%u", &loop_times);
	pr_debug("Loopback X %u...\n", loop_times);

	op_ctx_init(&op_ctx, client_ctx);
	/* prepare loopback descriptor */
	desq_q_pack_debug_desc(&desc, &op_ctx);

	/* Perform loopback for given times */
	for (i = 0; i < loop_times; i++) {
		op_ctx.op_state = USER_OP_INPROC;
		rc = desc_q_enqueue(client_ctx->drv_data->desc_queue, &desc,
				    true);
		if (unlikely(IS_DESCQ_ENQUEUE_ERR(rc))) {
			pr_err("Failed sending desc. %u\n", i);
			break;
		}
		rc = wait_for_sep_op_result(&op_ctx);
		if (rc != 0) {
			pr_err("Failed completion of desc. %u\n", i);
			break;
		}
		op_ctx.op_state = USER_OP_NOP;
	}

	op_ctx_fini(&op_ctx);

	pr_debug("Completed loopback of %u desc.\n", i);

	return count;		/* Noting to write for this device... */
#else /* DEBUG */
	pr_debug("Invoked for %zu bytes", count);
	return -ENOSYS;		/* nothing to write... IOCTL only */
#endif /* DEBUG */
}

/*!
 * IOCTL entry point
 *
 * \param filp
 * \param cmd
 * \param arg
 *
 * \return int
 * \retval 0 Operation succeeded (but SeP return code may indicate an error)
 * \retval -ENOTTY  : Unknown IOCTL command
 * \retval -ENOSYS  : Unsupported/not-implemented (known) operation
 * \retval -EINVAL  : Invalid parameters
 * \retval -EFAULT  : Bad pointers for given user memory space
 * \retval -EPERM   : Not enough permissions for given command
 * \retval -ENOMEM,-EAGAIN: when not enough resources available for given op.
 * \retval -EIO     : SeP HW error or another internal error
 *                    (probably operation timed out or unexpected behavior)
 */
long sep_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct sep_client_ctx *client_ctx = filp->private_data;
	unsigned long long ioctl_start, ioctl_end;
	int err = 0;

	preempt_disable_notrace();
	ioctl_start = sched_clock();
	preempt_enable_notrace();

	/* Verify IOCTL command: magic + number */
	if (_IOC_TYPE(cmd) != DXDI_IOC_MAGIC) {
		pr_err("Invalid IOCTL type=%u", _IOC_TYPE(cmd));
		return -ENOTTY;
	}
	if (_IOC_NR(cmd) > DXDI_IOC_NR_MAX) {
		pr_err("IOCTL NR=%u out of range for ABI ver.=%u.%u",
			    _IOC_NR(cmd), DXDI_VER_MAJOR, DXDI_VER_MINOR);
		return -ENOTTY;
	}

	/* Verify permissions on parameters pointer (arg) */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(ACCESS_WRITE,
				 (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(ACCESS_READ,
				 (void __user *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

#ifdef SEP_RUNTIME_PM
	dx_sep_pm_runtime_get();
#endif

	switch (_IOC_NR(cmd)) {
		/* Version info. commands */
	case DXDI_IOC_NR_GET_VER_MAJOR:
		pr_debug("DXDI_IOC_NR_GET_VER_MAJOR\n");
		err = sep_ioctl_get_ver_major(arg);
		break;
	case DXDI_IOC_NR_GET_VER_MINOR:
		pr_debug("DXDI_IOC_NR_GET_VER_MINOR\n");
		err = sep_ioctl_get_ver_minor(arg);
		break;
		/* Context size queries */
	case DXDI_IOC_NR_GET_SYMCIPHER_CTX_SIZE:
		pr_debug("DXDI_IOC_NR_GET_SYMCIPHER_CTX_SIZE\n");
		err = sep_ioctl_get_sym_cipher_ctx_size(arg);
		break;
	case DXDI_IOC_NR_GET_AUTH_ENC_CTX_SIZE:
		pr_debug("DXDI_IOC_NR_GET_AUTH_ENC_CTX_SIZE\n");
		err = sep_ioctl_get_auth_enc_ctx_size(arg);
		break;
	case DXDI_IOC_NR_GET_MAC_CTX_SIZE:
		pr_debug("DXDI_IOC_NR_GET_MAC_CTX_SIZE\n");
		err = sep_ioctl_get_mac_ctx_size(arg);
		break;
	case DXDI_IOC_NR_GET_HASH_CTX_SIZE:
		pr_debug("DXDI_IOC_NR_GET_HASH_CTX_SIZE\n");
		err = sep_ioctl_get_hash_ctx_size(arg);
		break;
		/* Init context commands */
	case DXDI_IOC_NR_SYMCIPHER_INIT:
		pr_debug("DXDI_IOC_NR_SYMCIPHER_INIT\n");
		err = sep_ioctl_sym_cipher_init(client_ctx, arg);
		break;
	case DXDI_IOC_NR_AUTH_ENC_INIT:
		pr_debug("DXDI_IOC_NR_AUTH_ENC_INIT\n");
		err = sep_ioctl_auth_enc_init(client_ctx, arg);
		break;
	case DXDI_IOC_NR_MAC_INIT:
		pr_debug("DXDI_IOC_NR_MAC_INIT\n");
		err = sep_ioctl_mac_init(client_ctx, arg);
		break;
	case DXDI_IOC_NR_HASH_INIT:
		pr_debug("DXDI_IOC_NR_HASH_INIT\n");
		err = sep_ioctl_hash_init(client_ctx, arg);
		break;
	case DXDI_IOC_NR_COMBINED_INIT:
		pr_debug("DXDI_IOC_NR_COMBINED_INIT\n");
		err = sep_ioctl_combined_init(client_ctx, arg);
		break;
		/* Processing commands */
	case DXDI_IOC_NR_PROC_DBLK:
		pr_debug("DXDI_IOC_NR_PROC_DBLK\n");
		err = sep_ioctl_proc_dblk(client_ctx, arg);
		break;
	case DXDI_IOC_NR_COMBINED_PROC_DBLK:
		pr_debug("DXDI_IOC_NR_COMBINED_PROC_DBLK\n");
		err = sep_ioctl_combined_proc_dblk(client_ctx, arg);
		break;
	case DXDI_IOC_NR_FIN_PROC:
		pr_debug("DXDI_IOC_NR_FIN_PROC\n");
		err = sep_ioctl_fin_proc(client_ctx, arg);
		break;
	case DXDI_IOC_NR_COMBINED_PROC_FIN:
		pr_debug("DXDI_IOC_NR_COMBINED_PROC_FIN\n");
		err = sep_ioctl_combined_fin_proc(client_ctx, arg);
		break;
		/* "Integrated" processing operations */
	case DXDI_IOC_NR_SYMCIPHER_PROC:
		pr_debug("DXDI_IOC_NR_SYMCIPHER_PROC\n");
		err = sep_ioctl_sym_cipher_proc(client_ctx, arg);
		break;
	case DXDI_IOC_NR_AUTH_ENC_PROC:
		pr_debug("DXDI_IOC_NR_AUTH_ENC_PROC\n");
		err = sep_ioctl_auth_enc_proc(client_ctx, arg);
		break;
	case DXDI_IOC_NR_MAC_PROC:
		pr_debug("DXDI_IOC_NR_MAC_PROC\n");
		err = sep_ioctl_mac_proc(client_ctx, arg);
		break;
	case DXDI_IOC_NR_HASH_PROC:
		pr_debug("DXDI_IOC_NR_HASH_PROC\n");
		err = sep_ioctl_hash_proc(client_ctx, arg);
		break;
	case DXDI_IOC_NR_COMBINED_PROC:
		pr_debug("DXDI_IOC_NR_COMBINED_PROC\n");
		err = sep_ioctl_combined_proc(client_ctx, arg);
		break;
		/* SeP RPC */
	case DXDI_IOC_NR_SEP_RPC:
		err = sep_ioctl_sep_rpc(client_ctx, arg);
		break;
#if MAX_SEPAPP_SESSION_PER_CLIENT_CTX > 0
		/* Memory registation */
	case DXDI_IOC_NR_REGISTER_MEM4DMA:
		pr_debug("DXDI_IOC_NR_REGISTER_MEM4DMA\n");
		err = sep_ioctl_register_mem4dma(client_ctx, arg);
		break;
	case DXDI_IOC_NR_ALLOC_MEM4DMA:
		pr_err("DXDI_IOC_NR_ALLOC_MEM4DMA: Not supported, yet");
		err = -ENOTTY;
		break;
	case DXDI_IOC_NR_FREE_MEM4DMA:
		pr_debug("DXDI_IOC_NR_FREE_MEM4DMA\n");
		err = sep_ioctl_free_mem4dma(client_ctx, arg);
		break;
	case DXDI_IOC_NR_SEPAPP_SESSION_OPEN:
		pr_debug("DXDI_IOC_NR_SEPAPP_SESSION_OPEN\n");
		err = sep_ioctl_sepapp_session_open(client_ctx, arg);
		break;
	case DXDI_IOC_NR_SEPAPP_SESSION_CLOSE:
		pr_debug("DXDI_IOC_NR_SEPAPP_SESSION_CLOSE\n");
		err = sep_ioctl_sepapp_session_close(client_ctx, arg);
		break;
	case DXDI_IOC_NR_SEPAPP_COMMAND_INVOKE:
		pr_debug("DXDI_IOC_NR_SEPAPP_COMMAND_INVOKE\n");
		err = sep_ioctl_sepapp_command_invoke(client_ctx, arg);
		break;
#endif
	case DXDI_IOC_NR_SET_IV:
		pr_debug("DXDI_IOC_NR_SET_IV\n");
		err = sep_ioctl_set_iv(client_ctx, arg);
		break;
	case DXDI_IOC_NR_GET_IV:
		pr_debug("DXDI_IOC_NR_GET_IV\n");
		err = sep_ioctl_get_iv(client_ctx, arg);
		break;
	default:/* Not supposed to happen - we already tested for NR range */
		pr_err("bad IOCTL cmd 0x%08X\n", cmd);
		err = -ENOTTY;
	}

	/* Update stats per IOCTL command */
	if (err == 0) {
		preempt_disable_notrace();
		ioctl_end = sched_clock();
		preempt_enable_notrace();
		sysfs_update_drv_stats(client_ctx->qid, _IOC_NR(cmd),
				       ioctl_start, ioctl_end);
	}

#ifdef SEP_RUNTIME_PM
	dx_sep_pm_runtime_put();
#endif

	return err;
}

static const struct file_operations sep_fops = {
	.owner = THIS_MODULE,
	.open = sep_open,
	.release = sep_release,
	.read = sep_read,
	.write = sep_write,
#ifdef CONFIG_COMPAT
	.compat_ioctl = sep_compat_ioctl,
#else
	.unlocked_ioctl = sep_ioctl,
#endif
};

/**
 * get_q_cache_size() - Get the number of entries to allocate for the SeP/FW
 *			cache of given queue
 * @drvdata:	 Driver context
 * @qid:	 The queue to allocate for
 *
 * Get the number of entries to allocate for the SeP/FW cache of given queue
 * The function assumes that num_of_desc_queues and num_of_sep_cache_entries
 * are already initialized in drvdata.
 * Returns Number of cache entries to allocate
 */
static int get_q_cache_size(struct sep_drvdata *drvdata, int qid)
{
	/* Simple allocation - divide evenly among queues */
	/* consider prefering higher priority queues...   */
	return drvdata->num_of_sep_cache_entries / drvdata->num_of_desc_queues;
}

/**
 * enable_descq_interrupt() - Enable interrupt for given queue (GPR)
 * @drvdata:
 * @qid:
 *
 * Returns int
 */
static void enable_descq_interrupt(struct sep_drvdata *drvdata, int qid)
{

	/* clear pending interrupts in GPRs of SW-queues
	 * (leftovers from init writes to GPRs..) */
	WRITE_REGISTER(drvdata->cc_base + DX_CC_REG_OFFSET(HOST, ICR),
		       gpr_interrupt_mask[qid]);

	drvdata->irq_mask |= gpr_interrupt_mask[qid];
	/* set IMR register */
	WRITE_REGISTER(drvdata->cc_base + DX_CC_REG_OFFSET(HOST, IMR),
		       ~drvdata->irq_mask);
}

/**
 * alloc_host_mem_for_sep() - Allocate memory pages for sep icache/dcache
 *	or for SEP backup memory in case there is not SEP cache memory.
 *
 * @drvdata:
 *
 * Currently using alloc_pages to allocate the pages.
 * Consider using CMA feature for the memory allocation
 */
static int alloc_host_mem_for_sep(struct sep_drvdata *drvdata)
{
#ifdef CACHE_IMAGE_NAME
	int i;
	const int icache_sizes_enum2log[] = DX_CC_ICACHE_SIZE_ENUM2LOG;

	pr_debug("icache_size=%uKB dcache_size=%uKB\n",
		 1 << (icache_size_log2 - 10),
		 1 << (dcache_size_log2 - 10));

	/* Verify validity of chosen cache memory sizes */
	if ((dcache_size_log2 > DX_CC_INIT_D_CACHE_MAX_SIZE_LOG2) ||
	    (dcache_size_log2 < DX_CC_INIT_D_CACHE_MIN_SIZE_LOG2)) {
		pr_err("Requested Dcache size (%uKB) is invalid\n",
		       1 << (dcache_size_log2 - 10));
		return -EINVAL;
	}
	/* Icache size must be one of values defined for this device */
	for (i = 0; i < sizeof(icache_sizes_enum2log) / sizeof(int); i++)
		if ((icache_size_log2 == icache_sizes_enum2log[i]) &&
		    (icache_sizes_enum2log[i] >= 0))
			/* Found valid value */
			break;
	if (unlikely(i == sizeof(icache_sizes_enum2log))) {
		pr_err("Requested Icache size (%uKB) is invalid\n",
		       1 << (icache_size_log2 - 10));
	}
	drvdata->icache_size_log2 = icache_size_log2;
	/* Allocate pages suitable for 32bit DMA and out of cache (cold) */
	drvdata->icache_pages = alloc_pages(GFP_KERNEL | GFP_DMA32 | __GFP_COLD,
					    icache_size_log2 - PAGE_SHIFT);
	if (drvdata->icache_pages == NULL) {
		pr_err("Failed allocating %uKB for Icache\n",
			    1 << (icache_size_log2 - 10));
		return -ENOMEM;
	}
	drvdata->dcache_size_log2 = dcache_size_log2;
	/* same as for icache */
	drvdata->dcache_pages = alloc_pages(GFP_KERNEL | GFP_DMA32 | __GFP_COLD,
					    dcache_size_log2 - PAGE_SHIFT);
	if (drvdata->dcache_pages == NULL) {
		pr_err("Failed allocating %uKB for Dcache\n",
		       1 << (dcache_size_log2 - 10));
		__free_pages(drvdata->icache_pages,
			     drvdata->icache_size_log2 - PAGE_SHIFT);
		return -ENOMEM;
	}
#elif defined(SEP_BACKUP_BUF_SIZE)
	/* This size is not enforced by power of two, so we use
	 * alloc_pages_exact() */
	drvdata->sep_backup_buf = alloc_pages_exact(SEP_BACKUP_BUF_SIZE,
						    GFP_KERNEL | GFP_DMA32 |
						    __GFP_COLD);
	if (unlikely(drvdata->sep_backup_buf == NULL)) {
		pr_err("Failed allocating %d B for SEP backup buffer\n",
		       SEP_BACKUP_BUF_SIZE);
		return -ENOMEM;
	}
	drvdata->sep_backup_buf_size = SEP_BACKUP_BUF_SIZE;
#endif
	return 0;
}

/**
 * free_host_mem_for_sep() - Free the memory resources allocated by
 *	alloc_host_mem_for_sep()
 *
 * @drvdata:
 */
static void free_host_mem_for_sep(struct sep_drvdata *drvdata)
{
#ifdef CACHE_IMAGE_NAME
	if (drvdata->dcache_pages != NULL) {
		__free_pages(drvdata->dcache_pages,
			     drvdata->dcache_size_log2 - PAGE_SHIFT);
		drvdata->dcache_pages = NULL;
	}
	if (drvdata->icache_pages != NULL) {
		__free_pages(drvdata->icache_pages,
			     drvdata->icache_size_log2 - PAGE_SHIFT);
		drvdata->icache_pages = NULL;
	}
#elif defined(SEP_BACKUP_BUF_SIZE)
	if (drvdata->sep_backup_buf != NULL) {
		free_pages_exact(drvdata->sep_backup_buf,
				 drvdata->sep_backup_buf_size);
		drvdata->sep_backup_buf_size = 0;
		drvdata->sep_backup_buf = NULL;
	}
#endif
}

static int emmc_match(struct device *dev, const void *data)
{
	if (strcmp(dev_name(dev), data) == 0)
		return 1;
	return 0;
}

static int mmc_blk_rpmb_req_handle(struct mmc_ioc_rpmb_req *req)
{
#define EMMC_BLK_NAME   "mmcblk0rpmb"

	struct device *emmc = NULL;

	if (!req)
		return -EINVAL;

	emmc = class_find_device(&block_class, NULL, EMMC_BLK_NAME, emmc_match);
	if (!emmc) {
		pr_err("eMMC reg failed\n");
		return -ENODEV;
	}

	return mmc_rpmb_req_handle(emmc, req);
}

static int rpmb_agent(void *unused)
{
#define AGENT_TIMEOUT_MS (1000 * 60 * 5) /* 5 minutes */

#define AUTH_DAT_WR_REQ 0x0003
#define AUTH_DAT_RD_REQ 0x0004

#define RPMB_FRAME_LENGTH      512
#define RPMB_MAC_KEY_LENGTH     32
#define RPMB_NONCE_LENGTH       16
#define RPMB_DATA_LENGTH       256
#define RPMB_STUFFBYTES_LENGTH 196
#define RPMB_COUNTER_LENGTH      4
#define RPMB_ADDR_LENGTH         2
#define RPMB_BLKCNT_LENGTH       2
#define RPMB_RESULT_LENGTH       2
#define RPMB_RSPREQ_LENGTH       2

#define RPMB_STUFFBYTES_OFFSET 0
#define RPMB_MAC_KEY_OFFSET   (RPMB_STUFFBYTES_OFFSET + RPMB_STUFFBYTES_LENGTH)
#define RPMB_DATA_OFFSET      (RPMB_MAC_KEY_OFFSET + RPMB_MAC_KEY_LENGTH)
#define RPMB_NONCE_OFFSET     (RPMB_DATA_OFFSET + RPMB_DATA_LENGTH)
#define RPMB_COUNTER_OFFSET   (RPMB_NONCE_OFFSET + RPMB_NONCE_LENGTH)
#define RPMB_ADDR_OFFSET      (RPMB_COUNTER_OFFSET + RPMB_COUNTER_LENGTH)
#define RPMB_BLKCNT_OFFSET    (RPMB_ADDR_OFFSET + RPMB_ADDR_LENGTH)
#define RPMB_RESULT_OFFSET    (RPMB_BLKCNT_OFFSET + RPMB_BLKCNT_LENGTH)
#define RPMB_RSPREQ_OFFSET    (RPMB_RESULT_OFFSET + RPMB_RESULT_LENGTH)

	int ret = 0;
	u32 tmp = 0;
	u32 max_buf_size = 0;
	u8 in_buf[RPMB_FRAME_LENGTH];
	u8 *out_buf = NULL;
	u32 in_buf_size = RPMB_FRAME_LENGTH;
	u32 timeout = INT_MAX;
	/* structure to pass to the eMMC driver's RPMB API */
	struct mmc_ioc_rpmb_req req2emmc;

	ret = dx_sep_req_register_agent(RPMB_AGENT_ID, &max_buf_size);
	if (ret) {
		pr_err("REG FAIL %d\n", ret);
		return -EINVAL;
	}

	out_buf = kmalloc(RPMB_FRAME_LENGTH, GFP_KERNEL);
	if (!out_buf) {
		pr_err("MALLOC FAIL\n");
		return -ENOMEM;
	}

	while (1) {
		/* Block until called by SEP */
		do {
			pr_info("RPMB AGENT BLOCKED\n");
			ret = dx_sep_req_wait_for_request(RPMB_AGENT_ID,
					in_buf, &in_buf_size, timeout);
		} while (ret == -EAGAIN);

		if (ret) {
			pr_err("WAIT FAILED %d\n", ret);
			break;
		}

		pr_info("RPMB AGENT UNBLOCKED\n");

		/* Process request */
		memset(&req2emmc, 0x00, sizeof(struct mmc_ioc_rpmb_req));

		/* Copy from incoming buffer into variables and swap
		 * endianess if needed */
		req2emmc.addr = *((u16 *)(in_buf+RPMB_ADDR_OFFSET));
		req2emmc.addr = be16_to_cpu(req2emmc.addr);
		/* As we are supporting only one block transfers */
		req2emmc.blk_cnt = 1;
		req2emmc.data = in_buf+RPMB_DATA_OFFSET;
		req2emmc.mac = in_buf+RPMB_MAC_KEY_OFFSET;
		req2emmc.nonce = in_buf+RPMB_NONCE_OFFSET;
		req2emmc.result = (u16 *)(in_buf+RPMB_RESULT_OFFSET);
		req2emmc.type = *((u16 *)(in_buf+RPMB_RSPREQ_OFFSET));
		req2emmc.type = be16_to_cpu(req2emmc.type);
		req2emmc.wc = (u32 *)(in_buf+RPMB_COUNTER_OFFSET);
		*req2emmc.wc = be32_to_cpu(*req2emmc.wc);

		/* Send request to eMMC driver */
		ret = mmc_blk_rpmb_req_handle(&req2emmc);
		if (ret) {
			pr_err("mmc_blk_rpmb_req_handle fail %d", ret);
			/* If access to eMMC driver failed send back
			 * artificial error */
			req2emmc.type = 0x0008;
		}

		/* Rebuild RPMB from response */
		memset(out_buf, 0, RPMB_FRAME_LENGTH);

		if (req2emmc.type == AUTH_DAT_RD_REQ) {
			pr_info("READ OPERATION RETURN\n");
			memcpy(out_buf+RPMB_DATA_OFFSET,
					req2emmc.data,  RPMB_DATA_LENGTH);
			memcpy(out_buf+RPMB_NONCE_OFFSET,
					req2emmc.nonce, RPMB_NONCE_LENGTH);

			out_buf[RPMB_BLKCNT_OFFSET]   = req2emmc.blk_cnt >> 8;
			out_buf[RPMB_BLKCNT_OFFSET+1] = req2emmc.blk_cnt;
		} else {
			pr_info("WRITE OPERATION RETURN\n");
			memcpy(&tmp, req2emmc.wc, RPMB_COUNTER_LENGTH);
			tmp = cpu_to_be32(tmp);
			memcpy(out_buf+RPMB_COUNTER_OFFSET,
					&tmp, RPMB_COUNTER_LENGTH);
		}

		memcpy(out_buf+RPMB_MAC_KEY_OFFSET,
				req2emmc.mac,    RPMB_MAC_KEY_LENGTH);
		memcpy(out_buf+RPMB_RESULT_OFFSET,
				req2emmc.result, RPMB_RESULT_LENGTH);

		memcpy(out_buf+RPMB_RSPREQ_OFFSET,
				&req2emmc.type, RPMB_RSPREQ_LENGTH);
		out_buf[RPMB_ADDR_OFFSET]   = req2emmc.addr >> 8;
		out_buf[RPMB_ADDR_OFFSET+1] = req2emmc.addr;

		/* Send response */
		ret = dx_sep_req_send_response(RPMB_AGENT_ID,
				out_buf, RPMB_FRAME_LENGTH);
		if (ret) {
			pr_err("dx_sep_req_send_response fail %d", ret);
			break;
		}
	}

	kfree(out_buf);

	return ret;
}

static int sep_setup(struct device *dev,
		     const struct resource *regs_res,
		     struct resource *r_irq)
{
	dev_t devt;
	struct sep_drvdata *drvdata = NULL;
	enum dx_sep_state sep_state;
	int rc = 0;
	int i;
	/* Create kernel thread for RPMB agent */
	static struct task_struct *rpmb_thread;
	char thread_name[] = "rpmb_agent";
	int sess_id = 0;
	enum dxdi_sep_module ret_origin;
	struct sep_client_ctx *sctx = NULL;
	u8 uuid[16] = DEFAULT_APP_UUID;

	pr_info("Discretix %s Driver initializing...\n", DRIVER_NAME);

	drvdata = kzalloc(sizeof(struct sep_drvdata), GFP_KERNEL);
	if (unlikely(drvdata == NULL)) {
		pr_err("Unable to allocate device private record\n");
		rc = -ENOMEM;
		goto failed0;
	}
	dev_set_drvdata(dev, (void *)drvdata);

	if (!regs_res) {
		pr_err("Couldn't get registers resource\n");
		rc = -EFAULT;
		goto failed1;
	}

	if (q_num > SEP_MAX_NUM_OF_DESC_Q) {
		pr_err(
			    "Requested number of queues (%u) is out of range must be no more than %u\n",
			    q_num, SEP_MAX_NUM_OF_DESC_Q);
		rc = -EINVAL;
		goto failed1;
	}

	/* TODO: Verify number of queues also with SeP capabilities */
	/* Initialize objects arrays for proper cleanup in case of error */
	for (i = 0; i < SEP_MAX_NUM_OF_DESC_Q; i++) {
		drvdata->queue[i].desc_queue = DESC_Q_INVALID_HANDLE;
		drvdata->queue[i].sep_cache = SEP_CTX_CACHE_NULL_HANDLE;
	}

	drvdata->mem_start = regs_res->start;
	drvdata->mem_end = regs_res->end;
	drvdata->mem_size = regs_res->end - regs_res->start + 1;

	if (!request_mem_region(drvdata->mem_start,
				drvdata->mem_size, DRIVER_NAME)) {
		pr_err("Couldn't lock memory region at %Lx\n",
			    (unsigned long long)regs_res->start);
		rc = -EBUSY;
		goto failed1;
	}

	/* create a mask in the lower 4 GB of memory */
	if (!dma_set_mask(dev, DMA_BIT_MASK(32)))
		dma_set_coherent_mask(dev, DMA_BIT_MASK(32));
	else
		pr_warn("sep54: No suitble DMA available\n");

	drvdata->dev = dev;

	drvdata->cc_base = ioremap(drvdata->mem_start, drvdata->mem_size);
	if (drvdata->cc_base == NULL) {
		pr_err("ioremap() failed\n");
		goto failed2;
	}

	pr_info("regbase_phys=0x%p..0x%p\n", &drvdata->mem_start,
		&drvdata->mem_end);
	pr_info("regbase_virt=0x%p\n", drvdata->cc_base);

#ifdef DX_BASE_ENV_REGS
	pr_info("FPGA ver. = UNKNOWN\n");
	/* TODO: verify FPGA version against expected version */
#endif

#ifdef SEP_PRINTF
	/* Sync. host to SeP initialization counter */
	/* After seting the interrupt mask, the interrupt from the GPR would
	 * trigger host_printf_handler to ack this value */
	drvdata->last_ack_cntr = SEP_PRINTF_ACK_SYNC_VAL;
#endif

	dx_sep_power_init(drvdata);

	/* Interrupt handler setup */
#ifdef SEP_INTERRUPT_BY_TIMER
	init_timer(&drvdata->delegate);
	drvdata->delegate.function = sep_timer;
	drvdata->delegate.data = (unsigned long)drvdata;
	mod_timer(&drvdata->delegate, jiffies);

#else				/* IRQ handler setup */
	/* Initialize IMR (mask) before registering interrupt handler */
	/* Enable only state register interrupt */
	drvdata->irq_mask = SEP_HOST_GPR_IRQ_MASK(DX_SEP_STATE_GPR_IDX);
#ifdef SEP_PRINTF
	/* Enable interrupt from host_printf GPR */
	drvdata->irq_mask |= SEP_HOST_GPR_IRQ_MASK(DX_SEP_HOST_PRINTF_GPR_IDX);
#endif
	WRITE_REGISTER(drvdata->cc_base + DX_CC_REG_OFFSET(HOST, IMR),
		       ~drvdata->irq_mask);
	/* The GPRs interrupts are set only after sep_init is done to avoid
	 * "garbage" interrupts as a result of the FW init process */
	drvdata->irq = r_irq->start;
	rc = request_irq(drvdata->irq, sep_interrupt,
			 IRQF_SHARED, DRIVER_NAME, drvdata->dev);
	if (unlikely(rc != 0)) {
		pr_err("Could not allocate interrupt %d\n", drvdata->irq);
		goto failed3;
	}
	pr_info("%s at 0x%p mapped to interrupt %d\n",
		DRIVER_NAME, drvdata->cc_base, drvdata->irq);

#endif				/*SEP_INTERRUPT_BY_TIMER */

	/* SeP FW initialization sequence */
	/* Cold boot before creating descQ objects */
	sep_state = GET_SEP_STATE(drvdata);
	if (sep_state != DX_SEP_STATE_DONE_COLD_BOOT) {
		pr_debug("sep_state=0x%08X\n", sep_state);
		/* If INIT_CC was not done externally, take care of it here */
		rc = alloc_host_mem_for_sep(drvdata);
		if (unlikely(rc != 0))
			goto failed4;
		rc = sepinit_do_cc_init(drvdata);
		if (unlikely(rc != 0))
			goto failed5;
	}
	sepinit_get_fw_props(drvdata);
	if (drvdata->fw_ver != EXPECTED_FW_VER) {
		pr_warn("Expected FW version %u.%u.%u but got %u.%u.%u\n",
			     VER_MAJOR(EXPECTED_FW_VER),
			     VER_MINOR(EXPECTED_FW_VER),
			     VER_PATCH(EXPECTED_FW_VER),
			     VER_MAJOR(drvdata->fw_ver),
			     VER_MINOR(drvdata->fw_ver),
			     VER_PATCH(drvdata->fw_ver));
	}

	if (q_num > drvdata->num_of_desc_queues) {
		pr_err(
			    "Requested number of queues (%u) is greater than SEP could support (%u)\n",
			    q_num, drvdata->num_of_desc_queues);
		rc = -EINVAL;
		goto failed5;
	}

	if (q_num == 0) {
		if (drvdata->num_of_desc_queues > SEP_MAX_NUM_OF_DESC_Q) {
			pr_info(
				     "The SEP number of queues (%u) is greater than the driver could support (%u)\n",
				     drvdata->num_of_desc_queues,
				     SEP_MAX_NUM_OF_DESC_Q);
			q_num = SEP_MAX_NUM_OF_DESC_Q;
		} else {
			q_num = drvdata->num_of_desc_queues;
		}
	}
	drvdata->num_of_desc_queues = q_num;

	pr_info("q_num=%d\n", drvdata->num_of_desc_queues);

	rc = dx_sep_req_init(drvdata);
	if (unlikely(rc != 0))
		goto failed6;

	/* Create descriptor queues objects - must be before
	 *  sepinit_set_fw_init_params to assure GPRs from host are reset */
	for (i = 0; i < drvdata->num_of_desc_queues; i++) {
		drvdata->queue[i].sep_data = drvdata;
		mutex_init(&drvdata->queue[i].desc_queue_sequencer);
		drvdata->queue[i].desc_queue =
		    desc_q_create(i, &drvdata->queue[i]);
		if (drvdata->queue[i].desc_queue == DESC_Q_INVALID_HANDLE) {
			pr_err("Unable to allocate desc_q object (%d)\n", i);
			rc = -ENOMEM;
			goto failed7;
		}
	}

	/* Create context cache management objects */
	for (i = 0; i < drvdata->num_of_desc_queues; i++) {
		const int num_of_cache_entries = get_q_cache_size(drvdata, i);
		if (num_of_cache_entries < 1) {
			pr_err("No SeP cache entries were assigned for qid=%d",
			       i);
			rc = -ENOMEM;
			goto failed7;
		}
		drvdata->queue[i].sep_cache =
		    ctxmgr_sep_cache_create(num_of_cache_entries);
		if (drvdata->queue[i].sep_cache == SEP_CTX_CACHE_NULL_HANDLE) {
			pr_err("Unable to allocate SeP cache object (%d)\n", i);
			rc = -ENOMEM;
			goto failed7;
		}
	}

	rc = sepinit_do_fw_init(drvdata);
	if (unlikely(rc != 0))
		goto failed7;

	drvdata->llimgr = llimgr_create(drvdata->dev, drvdata->mlli_table_size);
	if (drvdata->llimgr == LLIMGR_NULL_HANDLE) {
		pr_err("Failed creating LLI-manager object\n");
		rc = -ENOMEM;
		goto failed7;
	}

	drvdata->spad_buf_pool = dma_pool_create("dx_sep_rpc_msg", drvdata->dev,
						 USER_SPAD_SIZE,
						 L1_CACHE_BYTES, 0);
	if (drvdata->spad_buf_pool == NULL) {
		pr_err("Failed allocating DMA pool for RPC messages\n");
		rc = -ENOMEM;
		goto failed8;
	}

	/* Add character device nodes */
	rc = alloc_chrdev_region(&drvdata->devt_base, 0, SEP_DEVICES,
				 DRIVER_NAME);
	if (unlikely(rc != 0))
		goto failed9;
	pr_debug("Allocated %u chrdevs at %u:%u\n", SEP_DEVICES,
		 MAJOR(drvdata->devt_base), MINOR(drvdata->devt_base));
	for (i = 0; i < drvdata->num_of_desc_queues; i++) {
		devt = MKDEV(MAJOR(drvdata->devt_base),
			     MINOR(drvdata->devt_base) + i);
		cdev_init(&drvdata->queue[i].cdev, &sep_fops);
		drvdata->queue[i].cdev.owner = THIS_MODULE;
		rc = cdev_add(&drvdata->queue[i].cdev, devt, 1);
		if (unlikely(rc != 0)) {
			pr_err("cdev_add() failed for q%d\n", i);
			goto failed9;
		}
		drvdata->queue[i].dev = device_create(sep_class, dev, devt,
						      &drvdata->queue[i],
						      "%s%d",
						      DEVICE_NAME_PREFIX, i);
		drvdata->queue[i].devt = devt;
	}

	rc = sep_setup_sysfs(&(dev->kobj), drvdata);
	if (unlikely(rc != 0))
		goto failed9;

#ifndef SEP_INTERRUPT_BY_TIMER
	/* Everything is ready - enable interrupts of desc-Qs */
	for (i = 0; i < drvdata->num_of_desc_queues; i++)
		enable_descq_interrupt(drvdata, i);
#endif

	/* Enable sep request interrupt handling */
	dx_sep_req_enable(drvdata);

	/* Init DX Linux crypto module */
	if (!disable_linux_crypto) {
		rc = dx_crypto_api_init(drvdata);
		if (unlikely(rc != 0))
			goto failed10;
		rc = hwk_init();
		if (unlikely(rc != 0))
			goto failed10;
	}
#if MAX_SEPAPP_SESSION_PER_CLIENT_CTX > 0
	dx_sepapp_init(drvdata);
#endif

	rpmb_thread = kthread_create(rpmb_agent, NULL, thread_name);
	if (!rpmb_thread) {
		pr_err("RPMB agent thread create fail");
		goto failed10;
	} else {
		wake_up_process(rpmb_thread);
	}

	/* Inform SEP RPMB driver that it can enable RPMB access again */
	sctx = dx_sepapp_context_alloc();
	if (unlikely(!sctx))
		goto failed10;

	rc = dx_sepapp_session_open(sctx, uuid, 0, NULL, NULL, &sess_id,
				    &ret_origin);
	if (unlikely(rc != 0))
		goto failed11;
	rc = dx_sepapp_command_invoke(sctx, sess_id, CMD_RPMB_ENABLE, NULL,
				      &ret_origin);
	if (unlikely(rc != 0))
		goto failed11;

	rc = dx_sepapp_session_close(sctx, sess_id);
	if (unlikely(rc != 0))
		goto failed11;

	dx_sepapp_context_free(sctx);

	return 0;

/* Error cases cleanup */
 failed11:
	dx_sepapp_context_free(sctx);
 failed10:
	/* Disable interrupts */
	WRITE_REGISTER(drvdata->cc_base + DX_CC_REG_OFFSET(HOST, IMR), ~0);
	sep_free_sysfs();
 failed9:
	dma_pool_destroy(drvdata->spad_buf_pool);
 failed8:
	llimgr_destroy(drvdata->llimgr);
 failed7:
	for (i = 0; i < drvdata->num_of_desc_queues; i++) {
		if (drvdata->queue[i].devt) {
			cdev_del(&drvdata->queue[i].cdev);
			device_destroy(sep_class, drvdata->queue[i].devt);
		}

		if (drvdata->queue[i].sep_cache != SEP_CTX_CACHE_NULL_HANDLE) {
			ctxmgr_sep_cache_destroy(drvdata->queue[i].sep_cache);
			drvdata->queue[i].sep_cache = SEP_CTX_CACHE_NULL_HANDLE;
		}

		if (drvdata->queue[i].desc_queue != DESC_Q_INVALID_HANDLE) {
			desc_q_destroy(drvdata->queue[i].desc_queue);
			drvdata->queue[i].desc_queue = DESC_Q_INVALID_HANDLE;
			mutex_destroy(&drvdata->queue[i].desc_queue_sequencer);
		}
	}

 failed6:
	dx_sep_req_fini(drvdata);
 failed5:
	free_host_mem_for_sep(drvdata);
 failed4:
#ifdef SEP_INTERRUPT_BY_TIMER
	del_timer_sync(&drvdata->delegate);
#else
	free_irq(drvdata->irq, dev);
#endif
 failed3:
	dx_sep_power_exit();
	iounmap(drvdata->cc_base);
 failed2:
	release_mem_region(regs_res->start, drvdata->mem_size);
 failed1:
	kfree(drvdata);
 failed0:

	return rc;
}

static void sep_pci_remove(struct pci_dev *pdev)
{
	struct sep_drvdata *drvdata =
	    (struct sep_drvdata *)dev_get_drvdata(&pdev->dev);
	int i;

	if (!drvdata)
		return;
	dx_sep_req_fini(drvdata);
	if (!disable_linux_crypto) {
		dx_crypto_api_fini();
		hwk_fini();
	}
	/* Disable interrupts */
	WRITE_REGISTER(drvdata->cc_base + DX_CC_REG_OFFSET(HOST, IMR), ~0);

#ifdef SEP_RUNTIME_PM
	pm_runtime_get_noresume(&pdev->dev);
	pm_runtime_forbid(&pdev->dev);
#endif

	for (i = 0; i < drvdata->num_of_desc_queues; i++) {
		if (drvdata->queue[i].desc_queue != DESC_Q_INVALID_HANDLE) {
			desc_q_destroy(drvdata->queue[i].desc_queue);
			mutex_destroy(&drvdata->queue[i].desc_queue_sequencer);
		}
		if (drvdata->queue[i].sep_cache != NULL)
			ctxmgr_sep_cache_destroy(drvdata->queue[i].sep_cache);
		cdev_del(&drvdata->queue[i].cdev);
		device_destroy(sep_class, drvdata->queue[i].devt);
	}

	dma_pool_destroy(drvdata->spad_buf_pool);
	drvdata->spad_buf_pool = NULL;
	llimgr_destroy(drvdata->llimgr);
	drvdata->llimgr = LLIMGR_NULL_HANDLE;
	free_host_mem_for_sep(drvdata);
#ifdef SEP_INTERRUPT_BY_TIMER
	del_timer_sync(&drvdata->delegate);
#else
	free_irq(drvdata->irq, &pdev->dev);
#endif
	dx_sep_power_exit();
	iounmap(drvdata->cc_base);
	release_mem_region(drvdata->mem_start, drvdata->mem_size);
	sep_free_sysfs();
	kfree(drvdata);
	dev_set_drvdata(&pdev->dev, NULL);
	pci_dev_put(pdev);
}

static DEFINE_PCI_DEVICE_TABLE(sep_pci_id_tbl) = {
	{
	PCI_DEVICE(PCI_VENDOR_ID_INTEL, MRLD_SEP_PCI_DEVICE_ID)}, {
	0}
};

MODULE_DEVICE_TABLE(pci, sep_pci_id_tbl);

/**
 *	sep_probe - probe a matching PCI device
 *	@pdev: pci_device
 *	@end: pci_device_id
 *
 *	Attempt to set up and configure a SEP device that has been
 *	discovered by the PCI layer.
 */
static int sep_pci_probe(struct pci_dev *pdev,
			 const struct pci_device_id *ent)
{
	int error;
	struct resource res;
	struct resource r_irq;

	security_cfg_reg = ioremap_nocache(SECURITY_CFG_ADDR, 4);
	if (security_cfg_reg == NULL) {
		dev_err(&pdev->dev, "ioremap of security_cfg_reg failed\n");
		return -ENOMEM;
	}

	/* Enable Chaabi */
	error = pci_enable_device(pdev);
	if (error) {
		dev_err(&pdev->dev, "error enabling SEP device\n");
		goto end;
	}

	/* Fill resource variables */
	res.start = pci_resource_start(pdev, 0);

#ifdef PCI_REGION_BUG		/* TODO for wrong sep address bug */
	res.start += 0x8000;
#endif

	if (!res.start) {
		dev_warn(&pdev->dev, "Error getting register start\n");
		error = -ENODEV;
		goto disable_pci;
	}

	res.end = pci_resource_end(pdev, 0);
	if (!res.end) {
		dev_warn(&pdev->dev, "Error getting register end\n");
		error = -ENODEV;
		goto disable_pci;
	}

	/* Fill irq resource variable */
	r_irq.start = pdev->irq;

	/* Use resource variables */
	error = sep_setup(&pdev->dev, &res, &r_irq);
	if (error)
		goto disable_pci;

	pdev = pci_dev_get(pdev);

#ifdef SEP_RUNTIME_PM
	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_allow(&pdev->dev);
	pm_runtime_set_autosuspend_delay(&pdev->dev, SEP_AUTOSUSPEND_DELAY);
	pm_runtime_mark_last_busy(&pdev->dev);
	pm_runtime_use_autosuspend(&pdev->dev);
#endif

	return 0;

 disable_pci:
	pci_disable_device(pdev);
 end:
	iounmap(security_cfg_reg);

	return error;
}

#if defined(CONFIG_PM_RUNTIME) && defined(SEP_RUNTIME_PM)
static int sep_runtime_suspend(struct device *dev)
{
	int ret;
	int count = 0;
	u32 val;
	struct pci_dev *pdev = to_pci_dev(dev);
	struct sep_drvdata *drvdata =
	    (struct sep_drvdata *)dev_get_drvdata(dev);

	ret = dx_sep_power_state_set(DX_SEP_POWER_HIBERNATED);
	if (ret) {
		pr_err("%s failed! ret = %d\n", __func__, ret);
		return ret;
	}

	/*poll for chaabi_powerdown_en bit in SECURITY_CFG*/
	while (count < SEP_TIMEOUT) {
		val = readl(security_cfg_reg);
		if (val & PWR_DWN_ENB_MASK)
			break;
		usleep_range(40, 60);
		count++;
	}
	if (count >= SEP_TIMEOUT) {
		dev_err(&pdev->dev,
			"SEP: timed out waiting for chaabi_powerdown_en\n");
		WARN_ON(1);
		/*Let's continue to suspend as chaabi is not stable*/
	}

	disable_irq(pdev->irq);
	drvdata->sep_suspended = 1;

	return ret;
}

static int sep_runtime_resume(struct device *dev)
{
	int ret;
	struct pci_dev *pdev = to_pci_dev(dev);
	struct sep_drvdata *drvdata =
	    (struct sep_drvdata *)dev_get_drvdata(dev);

	drvdata->sep_suspended = 0;
	enable_irq(pdev->irq);
	ret = dx_sep_power_state_set(DX_SEP_POWER_ACTIVE);
	WARN(ret, "%s failed! ret = %d\n", __func__, ret);

	/*
	 * sep device might return to ACTIVE in time.
	 * As sep device is not stable, we choose return 0
	 * in case it blocks s3.
	 */
	return 0;
}

static int sep_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct sep_drvdata *drvdata =
	    (struct sep_drvdata *)dev_get_drvdata(dev);
	int ret = 0;
	int count = 0;
	u32 val;

	ret = dx_sep_power_state_set(DX_SEP_POWER_HIBERNATED);
	if (ret) {
		pr_err("%s failed! ret = %d\n", __func__, ret);
		return ret;
	}

	/*poll for chaabi_powerdown_en bit in SECURITY_CFG*/
	while (count < SEP_TIMEOUT) {
		val = readl(security_cfg_reg);
		if (val & PWR_DWN_ENB_MASK)
			break;
		usleep_range(40, 60);
		count++;
	}
	if (count >= SEP_TIMEOUT) {
		dev_err(dev,
			"SEP: timed out waiting for chaabi_powerdown_en\n");
		WARN_ON(1);
		/*Let's continue to suspend as chaabi is not stable*/
	}

	disable_irq(pdev->irq);
	drvdata->sep_suspended = 1;

	pci_save_state(pdev);
	pci_disable_device(pdev);
	pci_set_power_state(pdev, PCI_D3hot);

	return ret;
}

static int sep_resume(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pdev = to_pci_dev(dev);
	struct sep_drvdata *drvdata =
	    (struct sep_drvdata *)dev_get_drvdata(dev);

	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);
	ret = pci_enable_device(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "SEP: pci_enable_device failed\n");
		return ret;
	}

	drvdata->sep_suspended = 0;
	enable_irq(pdev->irq);

	ret = dx_sep_power_state_set(DX_SEP_POWER_ACTIVE);
	WARN(ret, "%s failed! ret = %d\n", __func__, ret);

	/*
	 * sep device might return to ACTIVE in time.
	 * As sep device is not stable, we choose return 0
	 * in case it blocks s3.
	 */
	return 0;
}

static const struct dev_pm_ops sep_pm_ops = {
	.runtime_suspend = sep_runtime_suspend,
	.runtime_resume = sep_runtime_resume,
	.suspend = sep_suspend,
	.resume = sep_resume,
};
#endif /* CONFIG_PM_RUNTIME && SEP_RUNTIME_PM */

/* Field for registering driver to PCI device */
static struct pci_driver sep_pci_driver = {
#if defined(CONFIG_PM_RUNTIME) && defined(SEP_RUNTIME_PM)
	.driver = {
		.pm = &sep_pm_ops,
	},
#endif /* CONFIG_PM_RUNTIME && SEP_RUNTIME_PM */
	.name = DRIVER_NAME,
	.id_table = sep_pci_id_tbl,
	.probe = sep_pci_probe,
	.remove = sep_pci_remove
};

static int __init sep_module_init(void)
{
	int rc;

	sep_class = class_create(THIS_MODULE, "sep_ctl");

	/* Register PCI device */
	rc = pci_register_driver(&sep_pci_driver);
	if (rc) {
		class_destroy(sep_class);
		return rc;
	}

	return 0;		/*success */
}

static void __exit sep_module_cleanup(void)
{
	pci_unregister_driver(&sep_pci_driver);
	class_destroy(sep_class);
}

/* Entry points  */
module_init(sep_module_init);
module_exit(sep_module_cleanup);
/* Module description */
MODULE_DESCRIPTION("Discretix " DRIVER_NAME " Driver");
MODULE_VERSION("0.7");
MODULE_AUTHOR("Discretix");
MODULE_LICENSE("GPL v2");
