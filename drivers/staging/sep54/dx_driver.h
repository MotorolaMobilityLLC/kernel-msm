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

#ifndef _DX_DRIVER_H_
#define _DX_DRIVER_H_

#include <generated/autoconf.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/completion.h>
#include <linux/export.h>
#include <linux/semaphore.h>

#define DX_CC_HOST_VIRT	/* must be defined before including dx_cc_regs.h */
#include "dx_cc_regs.h"
#include "dx_reg_common.h"
#include "dx_host.h"
#include "sep_log.h"
#include "sep_rpc.h"
#include "crypto_ctx_mgr.h"
#include "desc_mgr.h"
#include "lli_mgr.h"
#include "dx_driver_abi.h"
#include "dx_dev_defs.h"

/* Control printf's from SeP via GPR.
 * Keep this macro defined as long as SeP code uses host_printf
 * (otherwise, SeP would stall waiting for host to ack characters)
 */
#define SEP_PRINTF
/* Note: If DEBUG macro is undefined, SeP prints would not be printed
 * but the host driver would still ack the characters.                */

#define MODULE_NAME "sep54"

/* PCI ID's */
#define MRLD_SEP_PCI_DEVICE_ID 0x1198

#define VER_MAJOR(ver)  ((ver) >> 24)
#define VER_MINOR(ver)  (((ver) >> 16) & 0xFF)
#define VER_PATCH(ver)  (((ver) >> 8) & 0xFF)
#define VER_INTERNAL(ver) ((ver) & 0xFF)

#define SECURITY_CFG_ADDR	0xFF03A01C
#define PWR_DWN_ENB_MASK	0x20
#define SEP_TIMEOUT		50000
#define SEP_POWERON_TIMEOUT     10000
#define SEP_SLEEP_ENABLE 5

#define SEP_AUTOSUSPEND_DELAY 5000

/* GPR that holds SeP state */
#define SEP_STATE_GPR_OFFSET SEP_HOST_GPR_REG_OFFSET(DX_SEP_STATE_GPR_IDX)
/* In case of a change in GPR7 (state) we dump also GPR6 */
#define SEP_STATUS_GPR_OFFSET SEP_HOST_GPR_REG_OFFSET(DX_SEP_STATUS_GPR_IDX)

/* User memref index access macros */
#define IS_VALID_MEMREF_IDX(idx) \
	(((idx) >= 0) && ((idx) < MAX_REG_MEMREF_PER_CLIENT_CTX))
#define INVALIDATE_MEMREF_IDX(idx) ((idx) = DXDI_MEMREF_ID_NULL)

/* Session context access macros - must be invoked with mutex acquired */
#define SEP_SESSION_ID_INVALID 0xFFFF
#define IS_VALID_SESSION_CTX(session_ctx_p) \
	 (((session_ctx_p)->ref_cnt > 0) && \
	 ((session_ctx_p)->sep_session_id != SEP_SESSION_ID_INVALID))
#define INVALIDATE_SESSION_CTX(session_ctx_p) do {              \
	session_ctx_p->sep_session_id = SEP_SESSION_ID_INVALID; \
	session_ctx_p->ref_cnt = 0;                             \
} while (0)
/* Session index access macros */
/* Index is considered valid even if the pointed session context is not.
   One should use IS_VALID_SESSION_CTX to verify the validity of the context. */
#define IS_VALID_SESSION_IDX(idx) \
	 (((idx) >= 0) && ((idx) < MAX_SEPAPP_SESSION_PER_CLIENT_CTX))
#define INVALIDATE_SESSION_IDX(idx) ((idx) = DXDI_SEPAPP_SESSION_INVALID)

/*
   Size of DMA-coherent scratchpad buffer allocated per client_ctx context
   Currently, this buffer is used for 3 purposes:
   1. SeP RPC messages.
   2. SeP Applets messages.
   3. AES-CCM A0 (prepend) data.
*/
#define USER_SPAD_SIZE SEP_RPC_MAX_MSG_SIZE
#if (SEP_RPC_MAX_MSG_SIZE >= (1 << SEP_SW_DESC_RPC_MSG_HMB_SIZE_BIT_SIZE))
#error SEP_RPC_MAX_MSG_SIZE too large for HMB_SIZE field
#endif

/* Get the memref ID/index for given dma_obj (struct user_dma_buffer) */
#define DMA_OBJ_TO_MEMREF_IDX(client_ctx, the_dma_obj)                 \
	(container_of(the_dma_obj, struct registered_memref, dma_obj) - \
		(client_ctx)->reg_memrefs)

/* Crypto context IDs masks (to be used with ctxmgr_sep_cache_invalidate) */
#define CRYPTO_CTX_ID_SINGLE_MASK 0xFFFFFFFFFFFFFFFFULL
#define CRYPTO_CTX_ID_CLIENT_SHIFT 32
#define CRYPTO_CTX_ID_CLIENT_MASK (0xFFFFFFFFULL << CRYPTO_CTX_ID_CLIENT_SHIFT)

/* Return 'true' if val is a multiple of given blk_size */
/* blk_size must be a power of 2 */
#define IS_MULT_OF(val, blk_size)  ((val & (blk_size - 1)) == 0)

struct sep_drvdata;

/**
 * struct queue_drvdata - Data for a specific SeP queue
 * @desc_queue:	The associated descriptor queue object
 * @desc_queue_sequencer: Mutex to assure sequence of operations associated
 *			  with desc. queue enqueue
 * @sep_cache:	SeP context cache management object
 * @cdev:	Assocated character device
 * @devt:	Associated device
 * @sep_data:	Associated SeP driver context
 */
struct queue_drvdata {
	void *desc_queue;
	struct mutex desc_queue_sequencer;
	void *sep_cache;
	struct device *dev;
	struct cdev cdev;
	dev_t devt;
	struct sep_drvdata *sep_data;
};

/**
 * struct sep_drvdata - SeP driver private data context
 * @mem_start:	phys. address of the control registers
 * @mem_end:	phys. address of the control registers
 * @mem_size:	Control registers memory range size (mem_end - mem_start)
 * @cc_base:	virt address of the CC registers
 * @irq:	device IRQ number
 * @irq_mask:	Interrupt mask
 * @rom_ver:	SeP ROM version
 * @fw_ver:	SeP loaded firmware version
 * @icache_size_log2:	Icache memory size in power of 2 bytes
 * @dcache_size_log2:	Dcache memory size in power of 2 bytes
 * @icache_pages:	Pages allocated for Icache
 * @dcache_pages:	Pages allocated for Dcache
 * @sep_backup_buf_size:	Size of host memory allocated for sep context
 * @sep_backup_buf:		Buffer allocated for sep context backup
 * @sepinit_params_buf_dma:	DMA address of sepinit_params_buf_p
 * @spad_buf_pool:	DMA pool for RPC messages or scratch pad use
 * @mlli_table_size:	bytes as set by sepinit_get_fw_props
 * @llimgr:	LLI-manager object handle
 * @num_of_desc_queues:	Actual number of (active) queues
 * @num_of_sep_cache_entries:	Available SeP/FW cache entries
 * @devt_base:	Allocated char.dev. major/minor (with alloc_chrdev_region)
 * @dev:	Device context
 * @queue:	Array of objects for each SeP SW-queue
 * @last_ack_cntr:	The last counter value ACK'ed over the SEP_PRINTF GPR
 * @cur_line_buf_offset:	Offset in line_buf
 * @line_buf:	A buffer to accumulate SEP_PRINTF characters up to EOL
 * @delegate:	Timer to initiate GPRs polling for interrupt-less system
 */
struct sep_drvdata {
	resource_size_t mem_start;
	resource_size_t mem_end;
	resource_size_t mem_size;
	void __iomem *cc_base;
	unsigned int irq;
	u32 irq_mask;
	u32 rom_ver;
	u32 fw_ver;
#ifdef CACHE_IMAGE_NAME
	u8 icache_size_log2;
	u8 dcache_size_log2;
	struct page *icache_pages;
	struct page *dcache_pages;
#elif defined(SEP_BACKUP_BUF_SIZE)
	unsigned long sep_backup_buf_size;
	void *sep_backup_buf;
#endif
	int sep_suspended;
	struct dma_pool *spad_buf_pool;
	unsigned long mlli_table_size;
	void *llimgr;
	unsigned int num_of_desc_queues;
	int num_of_sep_cache_entries;
	dev_t devt_base;
	struct device *dev;
	struct queue_drvdata queue[SEP_MAX_NUM_OF_DESC_Q];

#ifdef SEP_PRINTF
	int last_ack_cntr;
	int cur_line_buf_offset;
#define SEP_PRINTF_LINE_SIZE 100
	char line_buf[SEP_PRINTF_LINE_SIZE + 1];
#endif

#ifdef SEP_INTERRUPT_BY_TIMER
	struct timer_list delegate;
#endif
};

/* Enumerate the session operational state */
enum user_op_state {
	USER_OP_NOP = 0,	/* No operation is in processing */
	USER_OP_PENDING,	/* Operation waiting to enter the desc. queue */
	USER_OP_INPROC,		/* Operation is in process       */
	USER_OP_COMPLETED	/* Operation completed, waiting for "read" */
};

/* Enumerate the data operation types */
enum crypto_data_intent {
	CRYPTO_DATA_NULL = 0,
	CRYPTO_DATA_TEXT,	/* plain/cipher text */
	CRYPTO_DATA_TEXT_FINALIZE,
	CRYPTO_DATA_ADATA,	/* Additional/Associated data for AEAD */
	CRYPTO_DATA_MAX = CRYPTO_DATA_ADATA,
};

/* SeP Applet session data */
struct sep_app_session {
	struct mutex session_lock;	/* Protect updates in entry */
	/* Reference count on session (initialized to 1 on opening) */
	u16 ref_cnt;
	u16 sep_session_id;
};

/**
 * struct registered_memref - Management information for registered memory
 * @buf_lock:	Mutex on buffer state changes (ref. count, etc.)
 * @ref_cnt:	Reference count for protecting freeing while in use.
 * @dma_obj:	The client DMA object container for the registered mem.
 */
struct registered_memref {
	struct mutex buf_lock;
	unsigned int ref_cnt;
	struct client_dma_buffer dma_obj;
};

struct async_ctx_info {
	struct dxdi_sepapp_params *dxdi_params;
	struct dxdi_sepapp_kparams *dxdi_kparams;
	struct sepapp_client_params *sw_desc_params;
	struct client_dma_buffer *local_dma_objs[SEPAPP_MAX_PARAMS];
	struct mlli_tables_list mlli_tables[SEPAPP_MAX_PARAMS];
	int session_id;
};

/*
 * struct sep_client_ctx - SeP client application context allocated per each
 *                         open()
 * @drv_data:	Associated queue driver context
 * @qid:	Priority queue ID
 * @uid_cntr:	Persistent unique ID counter to be used for crypto context UIDs
 *		allocation
 * @user_memrefs:	Registered user DMA memory buffers
 * @sepapp_sessions:	SeP Applet client sessions
 */
struct sep_client_ctx {
	struct queue_drvdata *drv_data;
	unsigned int qid;
	atomic_t uid_cntr;
	struct registered_memref reg_memrefs[MAX_REG_MEMREF_PER_CLIENT_CTX];
	struct sep_app_session
	    sepapp_sessions[MAX_SEPAPP_SESSION_PER_CLIENT_CTX];

	wait_queue_head_t memref_wq;
	int memref_cnt;
	struct mutex memref_lock;
};

/**
 * sep_op_type - Flags to describe dispatched type of sep_op_ctx
 * The flags may be combined when applicable (primarily for CRYPTO_OP desc.).
 * Because operations maybe asynchronous, we cannot set operation type in the
 * crypto context.
 */
enum sep_op_type {
	SEP_OP_NULL = 0,
	SEP_OP_CRYPTO_INIT = 1,	/* CRYPTO_OP::Init. */
	SEP_OP_CRYPTO_PROC = (1 << 1),	/* CRYPTO_OP::Process */
	SEP_OP_CRYPTO_FINI = (1 << 2),	/* CRYPTO_OP::Finalize (integrated) */
	SEP_OP_RPC = (1 << 3),	/* RPC_MSG */
	SEP_OP_APP = (1 << 4),	/* APP_REQ */
	SEP_OP_SLEEP = (1 << 7)	/* SLEEP_REQ */
};

/*
 * struct sep_op_ctx - A SeP operation context.
 * @client_ctx:	The client context associated with this operation
 * @session_ctx:	For SEP_SW_DESC_TYPE_APP_REQ we need the session context
 *			(otherwise NULL)
 * @op_state:	Operation progress state indicator
 * @ioctl_op_compl:	Operation completion signaling object for IOCTLs
 *			(updated by desc_mgr on completion)
 * @comp_work:	Async. completion work. NULL for IOCTLs.
 * @op_ret_code: The operation return code from SeP (valid on desc. completion)
 * @internal_error:	Mark that return code (error) is from the SeP FW
 *			infrastructure and not from the requested operation.
 *			Currently, this is used only for Applet Manager errors.
 * @ctx_info:	Current Context. If there are more than one context (such as
 *		in combined alg.) use (&ctx_info)[] into _ctx_info[].
 * @_ctx_info:	Extension of ctx_info for additional contexts associated with
 *		current operation (combined op.)
 * @ctx_info_num:	number of active ctx_info in (&ctx_info)[]
 * @pending_descs_cntr:	Pending SW descriptor associated with this operation.
 *			(Number of descriptor completions required to complete
 *			this operation)
 * @backlog_descs_cntr:	Descriptors of this operation enqueued in the backlog q.
 * @ift:	Input data MLLI table object
 * @oft:	Output data MLLI table object
 * @ift_dma_obj:	Temporary user memory registrations for IFT
 * @oft_dma_obj:	Temporary user memory registrations for OFT
 * @spad_buf_p:	Scratchpad DMA buffer for different temp. buffers required
 *		during a specific operation: (allocated from rpc_msg_pool)
 *		- SeP RPC message buffer         or
 *		- AES-CCM A0 scratchpad buffers  or
 *		- Next IV for AES-CBC, AES-CTR, DES-CBC
 * @spad_buf_dma_addr:	DMA address of spad_buf_p
 * @next_hash_blk_tail_size:	The tail of data_in which is a remainder of a
 *				block size. We use this info to copy the
 *				remainder data to the context after block
 *				processing completion (saved from
 *				prepare_data_for_sep).
 *
 * Retains the operation status and associated resources while operation in
 * progress. This object provides threads concurrency support since each thread
 * may work on different instance of this object, within the scope of the same
 * client (process) context.
*/
struct sep_op_ctx {
	struct sep_client_ctx *client_ctx;
	struct sep_app_session *session_ctx;
	enum sep_op_type op_type;
	enum user_op_state op_state;
	struct completion ioctl_op_compl;
	struct work_struct *comp_work;
	u32 error_info;
	bool internal_error;
	struct client_crypto_ctx_info ctx_info;
	struct client_crypto_ctx_info _ctx_info[DXDI_COMBINED_NODES_MAX - 1];
	u8 ctx_info_num;
	u8 pending_descs_cntr;
	u8 backlog_descs_cntr;
	/* Client memory resources for (sym.) crypto-ops */
	struct mlli_tables_list ift;
	struct mlli_tables_list oft;
	struct client_dma_buffer din_dma_obj;
	struct client_dma_buffer dout_dma_obj;
	void *spad_buf_p;
	dma_addr_t spad_buf_dma_addr;

	struct async_ctx_info async_info;
};

/***************************/
/* SeP registers access    */
/***************************/
/* "Raw" read version to be used in IS_CC_BUS_OPEN and in READ_REGISTER */
#define _READ_REGISTER(_addr) __raw_readl(        \
	(const volatile void __iomem *)(_addr))
/* The device register space is considered accessible when expected
   device signature value is read from HOST_CC_SIGNATURE register   */
#define IS_CC_BUS_OPEN(drvdata)                                           \
	(_READ_REGISTER(DX_CC_REG_ADDR(drvdata->cc_base,                  \
		CRY_KERNEL, HOST_CC_SIGNATURE)) == DX_DEV_SIGNATURE)
/*FIXME: Temporary w/a to HW problem with registers reading - double-read */
#define READ_REGISTER(_addr) \
	({(void)_READ_REGISTER(_addr); _READ_REGISTER(_addr); })
#define WRITE_REGISTER(_addr, _data)  __raw_writel(_data, \
	(volatile void __iomem *)(_addr))
#define GET_SEP_STATE(drvdata)                                           \
	(IS_CC_BUS_OPEN(drvdata) ?                                       \
		READ_REGISTER(drvdata->cc_base + SEP_STATE_GPR_OFFSET) : \
		DX_SEP_STATE_OFF)

#ifdef DEBUG
void dump_byte_array(const char *name, const u8 *the_array,
		     unsigned long size);
void dump_word_array(const char *name, const u32 *the_array,
		     unsigned long size_in_words);
#else
#define dump_byte_array(name, the_array, size) do {} while (0)
#define dump_word_array(name, the_array, size_in_words) do {} while (0)
#endif


/**
 * alloc_crypto_ctx_id() - Allocate unique ID for crypto context
 * @client_ctx:	 The client context object
 *
 */
static inline struct crypto_ctx_uid alloc_crypto_ctx_id(
	struct sep_client_ctx *client_ctx)
{
	struct crypto_ctx_uid uid;

	/* Assuming 32 bit atomic counter is large enough to never wrap
	 * during a lifetime of a process...
	 * Someone would laugh (or cry) on this one day */
#ifdef DEBUG
	if (atomic_read(&client_ctx->uid_cntr) == 0xFFFFFFFF) {
		pr_err("uid_cntr overflow for client_ctx=%p\n",
			    client_ctx);
		BUG();
	}
#endif

	uid.addr = (uintptr_t)client_ctx;
	uid.cntr = (u32)atomic_inc_return(&client_ctx->uid_cntr);

	return uid;
}

/**
 * op_ctx_init() - Initialize an operation context
 * @op_ctx:	 The allocated struct sep_op_ctx (may be on caller's stack)
 * @client_ctx:	 The "parent" client context
 *
 */
static inline void op_ctx_init(struct sep_op_ctx *op_ctx,
			       struct sep_client_ctx *client_ctx)
{
	int i;
	struct client_crypto_ctx_info *ctx_info_p = &(op_ctx->ctx_info);

	pr_debug("op_ctx=%p\n", op_ctx);
	memset(op_ctx, 0, sizeof(struct sep_op_ctx));
	op_ctx->client_ctx = client_ctx;
	op_ctx->ctx_info_num = 1;	/*assume a signle context operation */
	op_ctx->pending_descs_cntr = 1;	/* assume a single desc. transcation */
	init_completion(&(op_ctx->ioctl_op_compl));
	MLLI_TABLES_LIST_INIT(&(op_ctx->ift));
	MLLI_TABLES_LIST_INIT(&(op_ctx->oft));
	for (i = 0; i < DXDI_COMBINED_NODES_MAX; i++, ctx_info_p++)
		USER_CTX_INFO_INIT(ctx_info_p);
}

/**
 * op_ctx_fini() - Finalize op_ctx (free associated resources before freeing
 *		memory)
 * @op_ctx:	The op_ctx initialized with op_ctx_init
 *
 * Returns void
 */
static inline void op_ctx_fini(struct sep_op_ctx *op_ctx)
{
	pr_debug("op_ctx=%p\n", op_ctx);
	if (op_ctx->spad_buf_p != NULL)
		dma_pool_free(op_ctx->client_ctx->drv_data->sep_data->
			      spad_buf_pool, op_ctx->spad_buf_p,
			      op_ctx->spad_buf_dma_addr);

	delete_context((uintptr_t)op_ctx);
	memset(op_ctx, 0, sizeof(struct sep_op_ctx));
}

/**
 * init_client_ctx() - Initialize a client context object given
 * @drvdata:	Queue driver context
 * @client_ctx:
 *
 * Returns void
 */
void init_client_ctx(struct queue_drvdata *drvdata,
		     struct sep_client_ctx *client_ctx);

void cleanup_client_ctx(struct queue_drvdata *drvdata,
			struct sep_client_ctx *client_ctx);

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
			   const enum dma_data_direction dma_direction);

/**
 * free_client_memref() - Free resources associated with a client mem. reference
 * @client_ctx:	 User context data
 * @memref_idx:	 Index of the user memory reference
 *
 * Free resources associated with a user memory reference
 * (The referenced memory may be locked user pages or allocated DMA-coherent
 *  memory mmap'ed to the user space)
 * Returns int !0 on failure (memref still in use or unknown)
 */
int free_client_memref(struct sep_client_ctx *client_ctx,
		       int memref_idx);

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
					  int memref_idx);

/**
 * release_dma_obj() - Release memref object taken with get_memref_obj
 *			(Does not free!)
 * @client_ctx:	Associated client context
 * @dma_obj:	The DMA object returned from acquire_dma_obj()
 *
 * Returns void
 */
void release_dma_obj(struct sep_client_ctx *client_ctx,
		     struct client_dma_buffer *dma_obj);

/**
 * dxdi_data_dir_to_dma_data_dir() - Convert from DxDI DMA direction type to
 *					Linux kernel DMA direction type
 * @dxdi_dir:	 DMA direction in DxDI encoding
 *
 * Returns enum dma_data_direction
 */
enum dma_data_direction dxdi_data_dir_to_dma_data_dir(enum dxdi_data_direction
						      dxdi_dir);

/**
 * wait_for_sep_op_result() - Wait for outstanding SeP operation to complete and
 *				fetch SeP ret-code
 * @op_ctx:
 *
 * Wait for outstanding SeP operation to complete and fetch SeP ret-code
 * into op_ctx->sep_ret_code
 * Returns int
 */
int wait_for_sep_op_result(struct sep_op_ctx *op_ctx);

/**
 * crypto_op_completion_cleanup() - Cleanup CRYPTO_OP descriptor operation
 *					resources after completion
 * @op_ctx:
 *
 * Returns int
 */
int crypto_op_completion_cleanup(struct sep_op_ctx *op_ctx);


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
long sep_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

#endif				/* _DX_DRIVER_H_ */
