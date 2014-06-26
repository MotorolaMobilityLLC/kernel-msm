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

#define SEP_LOG_CUR_COMPONENT SEP_LOG_MASK_SEP_REQUEST

#include <linux/sched.h>
/*#include <linux/export.h>*/
#include "dx_driver.h"
#include "dx_bitops.h"
#include "sep_log.h"
#include "dx_reg_base_host.h"
#include "dx_host.h"
#define DX_CC_HOST_VIRT	/* must be defined before including dx_cc_regs.h */
#include "dx_cc_regs.h"
#include "sep_request.h"
#include "sep_request_mgr.h"
#include "dx_sep_kapi.h"

/* The request/response coherent buffer size */
#define DX_SEP_REQUEST_BUF_SIZE (4*1024)
#if (DX_SEP_REQUEST_BUF_SIZE < DX_SEP_REQUEST_MIN_BUF_SIZE)
#error DX_SEP_REQUEST_BUF_SIZE too small
#endif
#if (DX_SEP_REQUEST_BUF_SIZE > DX_SEP_REQUEST_MAX_BUF_SIZE)
#error DX_SEP_REQUEST_BUF_SIZE too big
#endif
#if (DX_SEP_REQUEST_BUF_SIZE & DX_SEP_REQUEST_4KB_MASK)
#error DX_SEP_REQUEST_BUF_SIZE must be a 4KB multiple
#endif

/* The maximum number of sep request agents */
/* Valid IDs are 0 to (DX_SEP_REQUEST_MAX_AGENTS-1) */
#define DX_SEP_REQUEST_MAX_AGENTS 4

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* Sep Request state object */
static struct {
	u8 *sep_req_buf_p;
	dma_addr_t sep_req_buf_dma;
	u8 *host_resp_buf_p;
	dma_addr_t host_resp_buf_dma;
	u8 req_counter;
	wait_queue_head_t agent_event[DX_SEP_REQUEST_MAX_AGENTS];
	bool agent_valid[DX_SEP_REQUEST_MAX_AGENTS];
	bool agent_busy[DX_SEP_REQUEST_MAX_AGENTS];
	bool request_pending;
	u32 *sep_host_gpr_adr;
	u32 *host_sep_gpr_adr;
} sep_req_state;

/* TODO:
   1) request_pending should use agent id instead of a global flag
   2) agent id 0 should be changed to non-valid
   3) Change sep request params for sep init to [] array instead pointer
   4) Consider usage of a mutex for syncing all access to the state
*/

/**
 * dx_sep_req_handler() - SeP request interrupt handler
 * @drvdata: The driver private info
 */
void dx_sep_req_handler(struct sep_drvdata *drvdata)
{
	u8 agent_id;
	u32 gpr_val;
	u32 sep_req_error = DX_SEP_REQUEST_SUCCESS;
	u32 counter_val;
	u32 req_len;

	/* Read GPR3 value */
	gpr_val = READ_REGISTER(drvdata->cc_base +
				SEP_HOST_GPR_REG_OFFSET
				(DX_SEP_REQUEST_GPR_IDX));

	/* Parse the new gpr value */
	counter_val = SEP_REQUEST_GET_COUNTER(gpr_val);
	agent_id = SEP_REQUEST_GET_AGENT_ID(gpr_val);
	req_len = SEP_REQUEST_GET_REQ_LEN(gpr_val);

	/* Increase the req_counter value in the state structure */
	sep_req_state.req_counter++;

	if (unlikely(counter_val != sep_req_state.req_counter))
		/* Verify new req_counter value is equal to the req_counter
		 * value from the state. If not, proceed to critical error flow
		 * below with error code SEP_REQUEST_OUT_OF_SYNC_ERR. */
		sep_req_error = DX_SEP_REQUEST_OUT_OF_SYNC_ERR;
	else if (unlikely((agent_id >= DX_SEP_REQUEST_MAX_AGENTS) ||
			  (!sep_req_state.agent_valid[agent_id])))
		/* Verify that the SeP Req Agent ID is registered in the LUT, if
		 * not proceed to critical error flow below with error code
		 * SEP_REQUEST_INVALID_AGENT_ID_ERR. */
		sep_req_error = DX_SEP_REQUEST_INVALID_AGENT_ID_ERR;
	else if (unlikely(req_len > DX_SEP_REQUEST_BUF_SIZE))
		/* Verify the request length is not bigger than the maximum
		 * allocated request buffer size. In bigger, proceed to critical
		 * error flow below with the SEP_REQUEST_INVALID_REQ_SIZE_ERR
		 * error code. */
		sep_req_error = DX_SEP_REQUEST_INVALID_REQ_SIZE_ERR;

	if (likely(sep_req_error == DX_SEP_REQUEST_SUCCESS)) {
		/* Signal the wake up event according to the LUT */
		sep_req_state.agent_busy[agent_id] = true;
		wake_up_interruptible(&sep_req_state.agent_event[agent_id]);
	} else {
		/* Critical error flow */

		/* Build the new GPR3 value out of the req_counter from the
		 * state, the error condition and zero response length value */
		gpr_val = 0;
		SEP_REQUEST_SET_COUNTER(gpr_val, sep_req_state.req_counter);
		SEP_REQUEST_SET_RESP_LEN(gpr_val, 0);
		SEP_REQUEST_SET_RETURN_CODE(gpr_val, sep_req_error);
		WRITE_REGISTER(drvdata->cc_base +
			       HOST_SEP_GPR_REG_OFFSET(DX_SEP_REQUEST_GPR_IDX),
			       gpr_val);
	}
}

/**
 * dx_sep_req_register_agent() - Regsiter an agent
 * @agent_id: The agent ID
 * @max_buf_size: A pointer to the max buffer size
 *
 * Returns int 0 on success
 */
int dx_sep_req_register_agent(u8 agent_id, u32 *max_buf_size)
{
	pr_debug("Regsiter SeP Request agent (id=%d)\n", agent_id);

	/* Validate agent ID is in range */
	if (agent_id >= DX_SEP_REQUEST_MAX_AGENTS) {
		pr_err("Invalid agent ID\n");
		return -EINVAL;
	}

	/* Verify SeP Req Agent ID is not valid */
	if (sep_req_state.agent_valid[agent_id] == true) {
		pr_err("Agent already registered\n");
		return -EINVAL;
	}

	/* Verify max_buf_size pointer is not NULL */
	if (max_buf_size == NULL) {
		pr_err("max_buf_size is NULL\n");
		return -EINVAL;
	}

	/* Set "agent_valid" field to TRUE */
	sep_req_state.agent_valid[agent_id] = true;

	/* Return the request/response max buffer size */
	*max_buf_size = DX_SEP_REQUEST_BUF_SIZE;

	return 0;
}
EXPORT_SYMBOL(dx_sep_req_register_agent);

/**
 * dx_sep_req_unregister_agent() - Unregsiter an agent
 * @agent_id: The agent ID
 *
 * Returns int 0 on success
 */
int dx_sep_req_unregister_agent(u8 agent_id)
{
	pr_debug("Unregsiter SeP Request agent (id=%d)\n", agent_id);

	/* Validate agent ID is in range */
	if (agent_id >= DX_SEP_REQUEST_MAX_AGENTS) {
		pr_err("Invalid agent ID\n");
		return -EINVAL;
	}

	/* Verify SeP Req Agent ID is valid */
	if (sep_req_state.agent_valid[agent_id] == false) {
		pr_err("Agent not registered\n");
		return -EINVAL;
	}

	/* Verify SeP agent is not busy */
	if (sep_req_state.agent_busy[agent_id] == true) {
		pr_err("Agent is busy\n");
		return -EBUSY;
	}

	/* Set "agent_valid" field to FALSE */
	sep_req_state.agent_valid[agent_id] = false;

	return 0;
}
EXPORT_SYMBOL(dx_sep_req_unregister_agent);

/**
 * dx_sep_req_wait_for_request() - Wait from an incoming sep request
 * @agent_id: The agent ID
 * @sep_req_buf_p: Pointer to the incoming request buffer
 * @req_buf_size: Pointer to the incoming request size
 * @timeout: Time to wait for an incoming request in jiffies
 *
 * Returns int 0 on success
 */
int dx_sep_req_wait_for_request(u8 agent_id, u8 *sep_req_buf_p,
				u32 *req_buf_size, u32 timeout)
{
	u32 gpr_val;

	pr_debug("Wait for sep request\n");
	pr_debug("agent_id=%d sep_req_buf_p=0x%p timeout=%d\n",
		 agent_id, sep_req_buf_p, timeout);

	/* Validate agent ID is in range */
	if (agent_id >= DX_SEP_REQUEST_MAX_AGENTS) {
		pr_err("Invalid agent ID\n");
		return -EINVAL;
	}

	/* Verify SeP Req Agent ID is valid */
	if (sep_req_state.agent_valid[agent_id] == false) {
		pr_err("Agent not registered\n");
		return -EINVAL;
	}

	/* Verify SeP agent is not busy */
	if (sep_req_state.agent_busy[agent_id] == true) {
		pr_err("Agent is busy\n");
		return -EBUSY;
	}

	/* Verify that another sep request is not pending */
	if (sep_req_state.request_pending == true) {
		pr_err("Agent is busy\n");
		return -EBUSY;
	}

	/* Verify sep_req_buf_p pointer is not NULL */
	if (sep_req_buf_p == NULL) {
		pr_err("sep_req_buf_p is NULL\n");
		return -EINVAL;
	}

	/* Verify req_buf_size pointer is not NULL */
	if (req_buf_size == NULL) {
		pr_err("req_buf_size is NULL\n");
		return -EINVAL;
	}

	/* Verify *req_buf_size is not zero and not bigger than the
	 * allocated request buffer */
	if ((*req_buf_size == 0) || (*req_buf_size > DX_SEP_REQUEST_BUF_SIZE)) {
		pr_err("Invalid request buffer size\n");
		return -EINVAL;
	}

	/* Wait for incoming request */
	if (wait_event_interruptible_timeout(
			sep_req_state.agent_event[agent_id],
			sep_req_state.agent_busy[agent_id] == true,
			timeout) == 0) {
		/* operation timed-out */
		sep_req_state.agent_busy[agent_id] = false;

		pr_err("Request wait timed-out\n");
		return -EAGAIN;
	}

	/* Set "request_pending" field to TRUE */
	sep_req_state.request_pending = true;

	gpr_val = READ_REGISTER(sep_req_state.sep_host_gpr_adr);

	/* If the request length is bigger than the callers' specified
	 * buffer size, the request is partially copied to the callers'
	 * buffer (only the first relevant bytes). The caller will not be
	 * indicated for an error condition in this case. The remaining bytes
	 * in the callers' request buffer are left as is without clearing.
	 * If the request length is smaller than the callers' specified buffer
	 * size, the relevant bytes from the allocated kernel request buffer
	 * are copied to the callers' request buffer */
	memcpy(sep_req_buf_p, sep_req_state.sep_req_buf_p,
	       MIN(*req_buf_size, SEP_REQUEST_GET_REQ_LEN(gpr_val)));

	return 0;
}
EXPORT_SYMBOL(dx_sep_req_wait_for_request);

/**
 * dx_sep_req_send_response() - Send a response to the sep
 * @agent_id: The agent ID
 * @host_resp_buf_p: Pointer to the outgoing response buffer
 * @resp_buf_size: Pointer to the outgoing response size
 *
 * Returns int 0 on success
 */
int dx_sep_req_send_response(u8 agent_id, u8 *host_resp_buf_p,
			     u32 resp_buf_size)
{
	u32 gpr_val;

	pr_debug("Send host response\n");
	pr_debug("agent_id=%d host_resp_buf_p=0x%p resp_buf_size=%d\n",
		 agent_id, host_resp_buf_p, resp_buf_size);

	/* Validate agent ID is in range */
	if (agent_id >= DX_SEP_REQUEST_MAX_AGENTS) {
		pr_err("Invalid agent ID\n");
		return -EINVAL;
	}

	/* Verify SeP Req Agent ID is valid */
	if (sep_req_state.agent_valid[agent_id] == false) {
		pr_err("Agent not registered\n");
		return -EINVAL;
	}

	/* Verify SeP agent is busy */
	if (sep_req_state.agent_busy[agent_id] == false) {
		pr_err("Agent is not busy\n");
		return -EBUSY;
	}

	/* Verify that a sep request is pending */
	if (sep_req_state.request_pending == false) {
		pr_err("No requests are pending\n");
		return -EBUSY;
	}

	/* Verify host_resp_buf_p pointer is not NULL */
	if (host_resp_buf_p == NULL) {
		pr_err("host_resp_buf_p is NULL\n");
		return -EINVAL;
	}

	/* Verify resp_buf_size is not zero and not bigger than the allocated
	 * request buffer */
	if ((resp_buf_size == 0) || (resp_buf_size > DX_SEP_REQUEST_BUF_SIZE)) {
		pr_err("Invalid response buffer size\n");
		return -EINVAL;
	}

	/* The request is copied from the callers' buffer to the global
	 * response buffer, only up to the callers' response length */
	memcpy(sep_req_state.host_resp_buf_p, host_resp_buf_p, resp_buf_size);

	/* Clear the request message buffer */
	memset(sep_req_state.sep_req_buf_p, 0, DX_SEP_REQUEST_BUF_SIZE);

	/* Clear the response message buffer for all remaining bytes
	 * beyond the response buffer actual size */
	memset(sep_req_state.host_resp_buf_p + resp_buf_size, 0,
	       DX_SEP_REQUEST_BUF_SIZE - resp_buf_size);

	/* Build the new GPR3 value out of the req_counter from the state,
	 * the response length and the DX_SEP_REQUEST_SUCCESS return code.
	 * Place the value in GPR3. */
	gpr_val = 0;
	SEP_REQUEST_SET_COUNTER(gpr_val, sep_req_state.req_counter);
	SEP_REQUEST_SET_RESP_LEN(gpr_val, resp_buf_size);
	SEP_REQUEST_SET_RETURN_CODE(gpr_val, DX_SEP_REQUEST_SUCCESS);
	WRITE_REGISTER(sep_req_state.host_sep_gpr_adr, gpr_val);

	/* Set "agent_busy" field to TRUE */
	sep_req_state.agent_busy[agent_id] = false;

	/* Set "request_pending" field to TRUE */
	sep_req_state.request_pending = false;

	return 0;
}
EXPORT_SYMBOL(dx_sep_req_send_response);

/**
 * dx_sep_req_get_sep_init_params() - Setup sep init params
 * @sep_request_params: The sep init parameters array
 */
void dx_sep_req_get_sep_init_params(u32 *sep_request_params)
{
	sep_request_params[0] = (u32) sep_req_state.sep_req_buf_dma;
	sep_request_params[1] = (u32) sep_req_state.host_resp_buf_dma;
	sep_request_params[2] = DX_SEP_REQUEST_BUF_SIZE;
}

/**
 * dx_sep_req_enable() - Enable the sep request interrupt handling
 * @drvdata: Driver private data
 */
void dx_sep_req_enable(struct sep_drvdata *drvdata)
{
	/* clear pending interrupts in GPRs of SEP request
	 * (leftovers from init writes to GPRs..) */
	WRITE_REGISTER(drvdata->cc_base + DX_CC_REG_OFFSET(HOST, ICR),
		       SEP_HOST_GPR_IRQ_MASK(DX_SEP_REQUEST_GPR_IDX));

	/* set IMR register */
	drvdata->irq_mask |= SEP_HOST_GPR_IRQ_MASK(DX_SEP_REQUEST_GPR_IDX);
	WRITE_REGISTER(drvdata->cc_base + DX_CC_REG_OFFSET(HOST, IMR),
		       ~drvdata->irq_mask);
}

/**
 * dx_sep_req_init() - Initialize the sep request state
 * @drvdata: Driver private data
 */
int dx_sep_req_init(struct sep_drvdata *drvdata)
{
	int i;

	pr_debug("Initialize SeP Request state\n");

	sep_req_state.request_pending = false;
	sep_req_state.req_counter = 0;

	for (i = 0; i < DX_SEP_REQUEST_MAX_AGENTS; i++) {
		sep_req_state.agent_valid[i] = false;
		sep_req_state.agent_busy[i] = false;
		init_waitqueue_head(&sep_req_state.agent_event[i]);
	}

	/* allocate coherent request buffer */
	sep_req_state.sep_req_buf_p = dma_alloc_coherent(drvdata->dev,
						 DX_SEP_REQUEST_BUF_SIZE,
						 &sep_req_state.
						 sep_req_buf_dma,
						 GFP_KERNEL);
	pr_debug("sep_req_buf_dma=0x%08X sep_req_buf_p=0x%p size=0x%08X\n",
		      (u32)sep_req_state.sep_req_buf_dma,
		      sep_req_state.sep_req_buf_p, DX_SEP_REQUEST_BUF_SIZE);

	if (sep_req_state.sep_req_buf_p == NULL) {
		pr_err("Unable to allocate coherent request buffer\n");
		return -ENOMEM;
	}

	/* Clear the request buffer */
	memset(sep_req_state.sep_req_buf_p, 0, DX_SEP_REQUEST_BUF_SIZE);

	/* allocate coherent response buffer */
	sep_req_state.host_resp_buf_p = dma_alloc_coherent(drvdata->dev,
						   DX_SEP_REQUEST_BUF_SIZE,
						   &sep_req_state.
						   host_resp_buf_dma,
						   GFP_KERNEL);
	pr_debug(
		      "host_resp_buf_dma=0x%08X host_resp_buf_p=0x%p size=0x%08X\n",
		      (u32)sep_req_state.host_resp_buf_dma,
		      sep_req_state.host_resp_buf_p, DX_SEP_REQUEST_BUF_SIZE);

	if (sep_req_state.host_resp_buf_p == NULL) {
		pr_err("Unable to allocate coherent response buffer\n");
		return -ENOMEM;
	}

	/* Clear the response buffer */
	memset(sep_req_state.host_resp_buf_p, 0, DX_SEP_REQUEST_BUF_SIZE);

	/* Setup the GPR address */
	sep_req_state.sep_host_gpr_adr = drvdata->cc_base +
	    SEP_HOST_GPR_REG_OFFSET(DX_SEP_REQUEST_GPR_IDX);

	sep_req_state.host_sep_gpr_adr = drvdata->cc_base +
	    HOST_SEP_GPR_REG_OFFSET(DX_SEP_REQUEST_GPR_IDX);

	return 0;
}

/**
 * dx_sep_req_fini() - Finalize the sep request state
 * @drvdata: Driver private data
 */
void dx_sep_req_fini(struct sep_drvdata *drvdata)
{
	int i;

	pr_debug("Finalize SeP Request state\n");

	sep_req_state.request_pending = false;
	sep_req_state.req_counter = 0;
	for (i = 0; i < DX_SEP_REQUEST_MAX_AGENTS; i++) {
		sep_req_state.agent_valid[i] = false;
		sep_req_state.agent_busy[i] = false;
	}

	dma_free_coherent(drvdata->dev, DX_SEP_REQUEST_BUF_SIZE,
			  sep_req_state.sep_req_buf_p,
			  sep_req_state.sep_req_buf_dma);

	dma_free_coherent(drvdata->dev, DX_SEP_REQUEST_BUF_SIZE,
			  sep_req_state.host_resp_buf_p,
			  sep_req_state.host_resp_buf_dma);
}
