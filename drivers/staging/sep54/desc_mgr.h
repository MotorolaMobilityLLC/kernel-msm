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

/*! \file desc_mgr.h
    Descriptor manager API and associated data structures
*/

#ifndef _DESC_MGR_H_
#define _DESC_MGR_H_

#include "sep_sw_desc.h"

/* Max. from Combined mode operation */
#define MAX_PENDING_DESCS 2

/* Evaluate desc_q_enqueue() return code (EINPROGRESS and EBUSY are ok) */
#define IS_DESCQ_ENQUEUE_ERR(_rc) ((_rc != -EINPROGRESS) && (_rc != -EBUSY))

#define DESC_Q_INVALID_HANDLE NULL

/* Opaque structure - accessed using SEP_SW_DESC_* macros */
struct sep_sw_desc {
	u32 data[SEP_SW_DESC_WORD_SIZE];
};

enum desc_q_state {
	DESC_Q_UNINITIALIZED,	/* Before initializing the queue */
	DESC_Q_ACTIVE,		/* Queue has pending requests    */
	DESC_Q_ASLEEP		/* Queue is in "sleep" state (cannot accept
				   additional requests - descs should be
				   enqueued in descs backlog queue) */
};

/* Declare like this to avoid cyclic inclusion with dx_cc54_driver.h */
struct sep_op_ctx;
struct queue_drvdata;
struct sep_app_session;

/**
 * desc_q_create() - Create descriptors queue object
 * @qid:	 The queue ID (index)
 * @drvdata:	 The associated queue driver data
 *
 * Returns Allocated queue object handle (DESC_Q_INVALID_HANDLE for failure)
 */
void *desc_q_create(int qid, struct queue_drvdata *drvdata);

/**
 * desc_q_destroy() - Destroy descriptors queue object (free resources)
 * @q_h: The queue object handle
 */
void desc_q_destroy(void *q_h);

/**
 * desc_q_set_state() - Set queue state (SLEEP or ACTIVE)
 * @q_h:	The queue object handle
 * @state:	The requested state
 */
int desc_q_set_state(void *q_h, enum desc_q_state state);

/**
 * desc_q_get_state() - Get queue state
 * @q_h:	The queue object handle
 */
enum desc_q_state desc_q_get_state(void *q_h);

/**
 * desc_q_is_idle() - Report if given queue is active but empty/idle.
 * @q_h:		The queue object handle
 * @idle_jiffies_p:	Return jiffies at which the queue became idle
 */
bool desc_q_is_idle(void *q_h, unsigned long *idle_jiffies_p);

/**
 * desc_q_reset() - Reset sent/completed counters of queue
 * @q_h:	The queue object handle
 *
 * This function should be invoked only when the queue is in ASLEEP state
 * after the transition of SeP to sleep state completed.
 * Returns -EBUSY if the queue is not in the correct state for reset.
 */
int desc_q_reset(void *q_h);

/**
 * desc_q_enqueue_sleep_req() - Enqueue SLEEP_REQ descriptor
 * @q_h:	The queue object handle
 * @op_ctx:	The operation context for this descriptor
 */
int desc_q_enqueue_sleep_req(void *q_h, struct sep_op_ctx *op_ctx);

/**
 * desc_q_get_info4sep()- Get queue address and size to be used in FW init phase
 * @q_h:		The queue object handle
 * @base_addr_p:	Base address return parameter
 * @size_p:		Queue size (in bytes) return parameter
 */
void desc_q_get_info4sep(void *q_h,
			 dma_addr_t *base_addr_p, unsigned long *size_p);

/**
 * desc_q_enqueue() - Enqueue given descriptor in given queue
 * @q_h:		The queue object handle
 * @desc_p:		Pointer to descriptor
 * @may_backlog:	When "true" and descQ is full or ASLEEP, may enqueue
 *			the given desc. in the backlog queue.
 *			When "false", any of the above cases would cause
 *			returning -ENOMEM.
 *
 * The function updates the op_ctx->op_state accoring to its results.
 * Returns -EINPROGRESS on success to dispatch into the SW desc. q.
 * Returns -EBUSY if may_backlog==true and the descriptor was enqueued in the
 * the backlog queue.
 * Returns -ENOMEM if queue is full and cannot enqueue in the backlog queue
 */
int desc_q_enqueue(void *q_h, struct sep_sw_desc *desc_p, bool may_backlog);

/*!
 * Mark given cookie as invalid in case marked as completed after a timeout
 * Invoke this before releasing the op_ctx object.
 * There is no race with the interrupt because the client_ctx (cookie) is still
 * valid when invoking this function.
 *
 * \param q_h Descriptor queue handle
 * \param cookie Invalidate descriptors with this cookie
 */
void desc_q_mark_invalid_cookie(void *q_h, void *cookie);

/*!
 * Dequeue and process any completed descriptors in the queue
 * (This function assumes non-reentrancy since it is invoked from
 *  either interrupt handler or in workqueue context)
 *
 * \param q_h The queue object handle
 *
 */
void desc_q_process_completed(void *q_h);

/*!
 * Create a debug descriptor in given buffer
 *
 * \param desc_p The descriptor buffer
 * \param op_ctx The operation context
 * TODO: Get additional debug descriptors (in addition to loopback)
 */
void desq_q_pack_debug_desc(struct sep_sw_desc *desc_p,
			    struct sep_op_ctx *op_ctx);

/*!
 * Pack a CRYPTO_OP descriptor in given descriptor buffer
 *
 * \param desc_p The descriptor buffer
 * \param op_ctx The operation context
 * \param sep_ctx_load_req Context load request flag
 * \param sep_ctx_init_req Context initialize request flag
 * \param proc_mode Descriptor processing mode
 */
void desc_q_pack_crypto_op_desc(struct sep_sw_desc *desc_p,
				struct sep_op_ctx *op_ctx,
				int sep_ctx_load_req, int sep_ctx_init_req,
				enum sep_proc_mode proc_mode);

/*!
 * Pack a COMBINED_OP descriptor in given descriptor buffer
 *
 * \param desc_p The descriptor buffer
 * \param op_ctx The operation context
 * \param sep_ctx_load_req Context load request flag
 * \param sep_ctx_init_req Context initialize request flag
 * \param proc_mode Descriptor processing mode
 * \param cfg_scheme The SEP format configuration scheme claimed by the user
 */
void desc_q_pack_combined_op_desc(struct sep_sw_desc *desc_p,
				  struct sep_op_ctx *op_ctx,
				  int sep_ctx_load_req, int sep_ctx_init_req,
				  enum sep_proc_mode proc_mode,
				  u32 cfg_scheme);
/*!
 * Pack a LOAD_OP descriptor in given descriptor buffer
 *
 * \param desc_p The descriptor buffer
 * \param op_ctx The operation context
 * \param sep_ctx_load_req Context load request flag
 */
void desc_q_pack_load_op_desc(struct sep_sw_desc *desc_p,
			      struct sep_op_ctx *op_ctx, int *sep_ctx_load_req);

/*!
 * Pack the RPC (message) descriptor type
 *
 *
 * \param desc_p The descriptor buffer
 * \param op_ctx The operation context
 * \param agent_id RPC agent (API) ID
 * \param func_id Function ID (index)
 * \param rpc_msg_size Size of RPC parameters message buffer
 * \param rpc_msg_dma_addr DMA address of RPC parameters message buffer
 */
void desc_q_pack_rpc_desc(struct sep_sw_desc *desc_p,
			  struct sep_op_ctx *op_ctx,
			  u16 agent_id,
			  u16 func_id,
			  unsigned long rpc_msg_size,
			  dma_addr_t rpc_msg_dma_addr);

/*!
 * Pack the Applet Request descriptor
 *
 * \param desc_p The descriptor buffer
 * \param op_ctx The operation context
 * \param req_type The Applet request type
 * \param session_id Session ID - Required only for SESSION_CLOSE and
 *                   COMMAND_INVOKE requests
 * \param inparams_addr DMA address of the "In Params." structure for the
 *                      request.
 */
void desc_q_pack_app_req_desc(struct sep_sw_desc *desc_p,
			      struct sep_op_ctx *op_ctx,
			      enum sepapp_req_type req_type,
			      u16 session_id, dma_addr_t inparams_addr);

/*!
 * Convert from crypto_proc_mode to string
 *
 * \param proc_mode The proc_mode enumeration value
 *
 * \return A string description of the processing mode
 */
const char *crypto_proc_mode_to_str(enum sep_proc_mode proc_mode);

inline u32 add_cookie(uintptr_t op_ctx);

inline void delete_cookie(u32 index);

inline void delete_context(uintptr_t op_ctx);

inline uintptr_t get_cookie(u32 index);

#define SEP_SW_DESC_GET_COOKIE(desc_p)\
	((struct sep_op_ctx *)get_cookie(((u32 *)desc_p)[SEP_SW_DESC_COOKIE_WORD_OFFSET]))

#define SEP_SW_DESC_SET_COOKIE(desc_p, op_ctx) \
do {\
	u32 __ctx_ptr__ = 0;\
	if (op_ctx == NULL) {\
		delete_cookie(((u32 *)desc_p)[SEP_SW_DESC_COOKIE_WORD_OFFSET]);\
	} else {\
		__ctx_ptr__ = add_cookie((uintptr_t)op_ctx);\
	} \
	memcpy(((u32 *)desc_p) + SEP_SW_DESC_COOKIE_WORD_OFFSET,\
	&__ctx_ptr__, sizeof(u32));\
} while (0)

#endif /*_DESC_MGR_H_*/
