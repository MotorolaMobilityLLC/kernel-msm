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

#define SEP_LOG_CUR_COMPONENT SEP_LOG_MASK_SEP_APP

#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>
/*#include <linux/export.h>*/
#include "dx_driver.h"
#include "dx_sepapp_kapi.h"
#include "sep_applets.h"
#include "sep_power.h"
#include "crypto_api.h"

/* Global drvdata to be used by kernel clients via dx_sepapp_ API */
static struct sep_drvdata *kapps_drvdata;

/**
 * sepapp_params_cleanup() - Clean resources allocated for SeP Applet paramters
 * @client_ctx:	 The associated client context for this operation
 * @dxdi_params:	The client parameters as passed from the DriverInterface
 * @sw_desc_params:	The client parameters DMA information for SeP
 *			(required for value copy back)
 * @local_memref_idx:	Reference to array of SEPAPP_MAX_PARAMS
 *			memory reference indexes.
 *			Set for parameters of MEMREF type.
 * @mlli_table:	Reference to array of SEPAPP_MAX_PARAMS MLLI tables
 *		objects to be used for MEMREF parameters.
 *
 * Clean resources allocated for SeP Applet paramters
 * (primarily, MLLI tables and temporary registered user memory)
 * Returns void
 */
static void sepapp_params_cleanup(struct sep_client_ctx *client_ctx,
				  struct dxdi_sepapp_params *dxdi_params,
				  struct dxdi_sepapp_kparams *dxdi_kparams,
				  struct sepapp_client_params *sw_desc_params,
				  struct client_dma_buffer *local_dma_objs[],
				  struct mlli_tables_list mlli_tables[])
{
	int i;
	int memref_idx;
	struct queue_drvdata *drvdata = client_ctx->drv_data;
	enum dxdi_sepapp_param_type *params_types;
	struct dxdi_val_param *cur_val;

	if (dxdi_params != NULL)
		params_types = dxdi_params->params_types;
	else if (dxdi_kparams != NULL)	/* kernel parameters */
		params_types = dxdi_kparams->params_types;
	else			/* No parameters - nothing to clean */
		return;

	for (i = 0; (i < SEPAPP_MAX_PARAMS); i++) {
		if (params_types[i] == DXDI_SEPAPP_PARAM_MEMREF) {
			/* Can call for all (unitialized MLLI ignored) */
			llimgr_destroy_mlli(drvdata->sep_data->llimgr,
					    mlli_tables + i);
			memref_idx =
			    DMA_OBJ_TO_MEMREF_IDX(client_ctx,
						  local_dma_objs[i]);
			release_dma_obj(client_ctx, local_dma_objs[i]);
			if ((local_dma_objs[i] != NULL) &&
			    (((dxdi_params != NULL) && (memref_idx !=
							dxdi_params->params[i].
							memref.ref_id)) ||
			     (dxdi_kparams != NULL))) {
				/* There is DMA object to free
				 * (either user params of temp. reg. or
				 * kernel params - always temp.) */
				(void)free_client_memref(client_ctx,
							 memref_idx);
			}
			local_dma_objs[i] = NULL;
		} else if (params_types[i] == DXDI_SEPAPP_PARAM_VAL) {
			if (dxdi_params != NULL)
				cur_val = &dxdi_params->params[i].val;
			else	/* kernel parameters */
				cur_val = &dxdi_kparams->params[i].val;
			if (cur_val->copy_dir & DXDI_DATA_FROM_DEVICE) {
				/* Copy back output values */
				cur_val->data[0] =
				    sw_desc_params->params[i].val.data[0];
				cur_val->data[1] =
				    sw_desc_params->params[i].val.data[1];
			}
		}
	}
}

static int kernel_memref_to_sw_desc_memref(struct dxdi_kmemref *cur_memref,
					   struct sep_client_ctx *client_ctx,
					   u8 *sep_memref_type_p,
					   struct seprpc_memref *sep_memref_p,
					   struct client_dma_buffer
					   **local_dma_obj_pp,
					   struct mlli_tables_list
					   *mlli_table_p)
{
	void *llimgr = client_ctx->drv_data->sep_data->llimgr;
	enum dma_data_direction dma_dir;
	int memref_idx;
	int rc = 0;

	/* convert DMA direction to enum dma_data_direction */
	dma_dir = dxdi_data_dir_to_dma_data_dir(cur_memref->dma_direction);
	if (unlikely(dma_dir == DMA_NONE)) {
		pr_err("Invalid DMA direction (%d) for param.\n",
			    cur_memref->dma_direction);
		return -EINVAL;
	}

	/* For kernel parameters always temp. registration */
	memref_idx = register_client_memref(client_ctx,
					    NULL, cur_memref->sgl,
					    cur_memref->nbytes, dma_dir);
	if (unlikely(!IS_VALID_MEMREF_IDX(memref_idx))) {
		pr_err("Failed temp. memory registration (rc=%d)\n",
			    memref_idx);
		return -ENOMEM;
	}
	*local_dma_obj_pp = acquire_dma_obj(client_ctx, memref_idx);
	if (*local_dma_obj_pp == NULL)
		rc = -EINVAL;
	else
		/* MLLI table creation */
		rc = llimgr_create_mlli(llimgr,
				mlli_table_p, dma_dir, *local_dma_obj_pp, 0, 0);

	if (likely(rc == 0)) {
		llimgr_mlli_to_seprpc_memref(mlli_table_p, sep_memref_p);
		*sep_memref_type_p = SEPAPP_PARAM_TYPE_MEMREF;
	}

	return rc;		/* Cleanup on error in caller */
}

static int user_memref_to_sw_desc_memref(struct dxdi_memref *cur_memref,
					 struct sep_client_ctx *client_ctx,
					 u8 *sep_memref_type_p,
					 struct seprpc_memref *sep_memref_p,
					 struct client_dma_buffer
					 **local_dma_obj_pp,
					 struct mlli_tables_list *mlli_table_p)
{
	void *llimgr = client_ctx->drv_data->sep_data->llimgr;
	enum dma_data_direction dma_dir;
	int memref_idx;
	int rc = 0;

	/* convert DMA direction to enum dma_data_direction */
	dma_dir = dxdi_data_dir_to_dma_data_dir(cur_memref->dma_direction);
	if (unlikely(dma_dir == DMA_NONE)) {
		pr_err("Invalid DMA direction (%d) for param.\n",
			    cur_memref->dma_direction);
		return -EINVAL;
	}

	if (IS_VALID_MEMREF_IDX(cur_memref->ref_id)) {
		/* Registered mem. */
		*local_dma_obj_pp =
		    acquire_dma_obj(client_ctx, cur_memref->ref_id);
		if (unlikely(*local_dma_obj_pp == NULL)) {
			pr_err("Failed to acquire DMA obj. at ref_id=%d\n",
				    cur_memref->ref_id);
			return -EINVAL;
		}
		if ((cur_memref->start_or_offset == 0) &&
		    (cur_memref->size == (*local_dma_obj_pp)->buf_size)) {
			/* Whole registered mem. */
			memref_idx = cur_memref->ref_id;
		} else {	/* Partial reference */
			/* Handle as unregistered memory at
			 * different address/len. */
			INVALIDATE_MEMREF_IDX(cur_memref->ref_id);
			cur_memref->start_or_offset += (unsigned long)
			    (*local_dma_obj_pp)->user_buf_ptr;
			/* Release base memref - not used */
			release_dma_obj(client_ctx, *local_dma_obj_pp);
			*local_dma_obj_pp = NULL;
		}
	}
	/* The following is not "else" of previous, because
	 * previous block may invalidate ref_id for partial
	 * reference to cause this temp. registartion. */
	if (!IS_VALID_MEMREF_IDX(cur_memref->ref_id)) {
		/* Temp. registration */
		memref_idx =
		    register_client_memref(client_ctx,
					   (u8 __user *)cur_memref->
					   start_or_offset, NULL,
					   cur_memref->size, dma_dir);
		if (unlikely(!IS_VALID_MEMREF_IDX(memref_idx))) {
			pr_err("Failed temp. memory " "registration\n");
			return -ENOMEM;
		}
		*local_dma_obj_pp = acquire_dma_obj(client_ctx, memref_idx);
	}

	if (*local_dma_obj_pp == NULL)
		rc = -EINVAL;
	else
		/* MLLI table creation */
		rc = llimgr_create_mlli(llimgr,
				mlli_table_p, dma_dir, *local_dma_obj_pp, 0, 0);

	if (likely(rc == 0)) {
		llimgr_mlli_to_seprpc_memref(mlli_table_p, sep_memref_p);
		*sep_memref_type_p = SEPAPP_PARAM_TYPE_MEMREF;
	}

	return rc;		/* Cleanup on error in caller */
}

/**
 * dxdi_sepapp_params_to_sw_desc_params() - Convert the client input parameters
 * @client_ctx:	 The associated client context for this operation
 * @dxdi_params:	The client parameters as passed from the DriverInterface
 * @sw_desc_params:	The returned client parameters DMA information for SeP
 * @local_memref_idx:	Reference to array of SEPAPP_MAX_PARAMS
 *			memory reference indexes.
 *			Set for parameters of MEMREF type.
 * @mlli_table:	Reference to array of SEPAPP_MAX_PARAMS MLLI tables
 *		objects to be used for MEMREF parameters.
 *
 * Convert the client input parameters array from dxdi format to the
 * SW descriptor format while creating the required MLLI tables
 * Returns int
 */
static int dxdi_sepapp_params_to_sw_desc_params(struct sep_client_ctx
						*client_ctx,
						struct dxdi_sepapp_params
						*dxdi_params,
						struct dxdi_sepapp_kparams
						*dxdi_kparams,
						struct sepapp_client_params
						*sw_desc_params,
						struct client_dma_buffer
						*local_dma_objs[],
						struct mlli_tables_list
						mlli_tables[])
{
	enum dxdi_sepapp_param_type *params_types;
	struct dxdi_val_param *cur_val;
	int i;
	int rc = 0;

	/* Init./clean arrays for proper cleanup in case of failure */
	for (i = 0; i < SEPAPP_MAX_PARAMS; i++) {
		MLLI_TABLES_LIST_INIT(mlli_tables + i);
		local_dma_objs[i] = NULL;
		sw_desc_params->params_types[i] = SEPAPP_PARAM_TYPE_NULL;
	}

	if (dxdi_params != NULL)
		params_types = dxdi_params->params_types;
	else if (dxdi_kparams != NULL)	/* kernel parameters */
		params_types = dxdi_kparams->params_types;
	else	/* No parameters - nothing to beyond initialization (above) */
		return 0;

	/* Convert each parameter based on its type */
	for (i = 0; (i < SEPAPP_MAX_PARAMS) && (rc == 0); i++) {
		switch (params_types[i]) {

		case DXDI_SEPAPP_PARAM_MEMREF:
			if (dxdi_params != NULL)
				rc = user_memref_to_sw_desc_memref
				    (&dxdi_params->params[i].memref, client_ctx,
				     &sw_desc_params->params_types[i],
				     &(sw_desc_params->params[i].memref),
				     local_dma_objs + i, mlli_tables + i);
			else
				rc = kernel_memref_to_sw_desc_memref
				    (&dxdi_kparams->params[i].kmemref,
				     client_ctx,
				     &sw_desc_params->params_types[i],
				     &(sw_desc_params->params[i].memref),
				     local_dma_objs + i, mlli_tables + i);
			break;	/* from switch */

		case DXDI_SEPAPP_PARAM_VAL:
			if (dxdi_params != NULL)
				cur_val = &dxdi_params->params[i].val;
			else	/* kernel parameters */
				cur_val = &dxdi_kparams->params[i].val;

			sw_desc_params->params[i].val.dir = SEPAPP_DIR_NULL;
			if (cur_val->copy_dir & DXDI_DATA_TO_DEVICE) {
				sw_desc_params->params[i].val.dir |=
				    SEPAPP_DIR_IN;
				sw_desc_params->params[i].val.data[0] =
				    cur_val->data[0];
				sw_desc_params->params[i].val.data[1] =
				    cur_val->data[1];
			}
			if (cur_val->copy_dir & DXDI_DATA_FROM_DEVICE) {
				sw_desc_params->params[i].val.dir |=
				    SEPAPP_DIR_OUT;
			}
			sw_desc_params->params_types[i] = SEPAPP_PARAM_TYPE_VAL;
			break;	/* from switch */

		case DXDI_SEPAPP_PARAM_NULL:
			sw_desc_params->params_types[i] =
			    SEPAPP_PARAM_TYPE_NULL;
			break;

		default:
			pr_err(
				"Invalid parameter type (%d) for #%d\n",
				params_types[i], i);
			rc = -EINVAL;
		}		/*switch */
	}			/*for parameters */

	/* Cleanup in case of error */
	if (rc != 0)
		sepapp_params_cleanup(client_ctx, dxdi_params, dxdi_kparams,
				      sw_desc_params, local_dma_objs,
				      mlli_tables);
	return rc;
}

/**
 * sepapp_session_open() - Open a session with given SeP Applet
 * @op_ctx:
 * @sepapp_uuid:	 Applet UUID
 * @auth_method:	 Client authentication method
 * @auth_data:	 Authentication data
 * @app_auth_data:	 Applet specific authentication data
 * @session_id:	 Returned allocated session ID
 * @sep_ret_origin:	 Origin in SeP of error code
 *
 * Returns int
 */
static int sepapp_session_open(struct sep_op_ctx *op_ctx,
			       u8 *sepapp_uuid,
			       u32 auth_method,
			       void *auth_data,
			       struct dxdi_sepapp_params *app_auth_data,
			       struct dxdi_sepapp_kparams *kapp_auth_data,
			       int *session_id,
			       enum dxdi_sep_module *sep_ret_origin)
{
	int rc;
	struct sep_app_session *new_session;
	struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;
	struct client_dma_buffer *local_dma_objs[SEPAPP_MAX_PARAMS];
	struct mlli_tables_list mlli_tables[SEPAPP_MAX_PARAMS];
	struct sep_sw_desc desc;
	struct sepapp_in_params_session_open *sepapp_msg_p;

	/* Verify that given spad_buf size can accomodate the in_params */
	BUILD_BUG_ON(sizeof(struct sepapp_in_params_session_open) >
		     USER_SPAD_SIZE);

	op_ctx->op_type = SEP_OP_APP;
	*sep_ret_origin = DXDI_SEP_MODULE_HOST_DRIVER;

	op_ctx->spad_buf_p = dma_pool_alloc(drvdata->sep_data->spad_buf_pool,
					    GFP_KERNEL,
					    &op_ctx->spad_buf_dma_addr);
	if (unlikely(op_ctx->spad_buf_p == NULL)) {
		pr_err(
			    "Failed allocating from spad_buf_pool for SeP Applet Request message\n");
		INVALIDATE_SESSION_IDX(*session_id);
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
		return -ENOMEM;
	}
	sepapp_msg_p =
	    (struct sepapp_in_params_session_open *)op_ctx->spad_buf_p;

	/* Find free session entry */
	for ((*session_id) = 0,
	     new_session = &op_ctx->client_ctx->sepapp_sessions[0];
	     (*session_id < MAX_SEPAPP_SESSION_PER_CLIENT_CTX);
	     new_session++, (*session_id)++) {
		mutex_lock(&new_session->session_lock);
		if (new_session->ref_cnt == 0)
			break;
		mutex_unlock(&new_session->session_lock);
	}
	if (*session_id == MAX_SEPAPP_SESSION_PER_CLIENT_CTX) {
		pr_err(
			    "Could not allocate session entry. all %u are in use.\n",
			    MAX_SEPAPP_SESSION_PER_CLIENT_CTX);
		INVALIDATE_SESSION_IDX(*session_id);
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
		return -ENOMEM;
	}

	new_session->ref_cnt = 1;	/* To be decremented by close_session */
	/* Invalidate session ID so it cannot ne used until opening the session
	 * is actually opened */
	new_session->sep_session_id = SEP_SESSION_ID_INVALID;
	mutex_unlock(&new_session->session_lock);

	/* Convert parameters to SeP Applet format */
	rc = dxdi_sepapp_params_to_sw_desc_params(op_ctx->client_ctx,
						  app_auth_data, kapp_auth_data,
						  &sepapp_msg_p->client_params,
						  local_dma_objs, mlli_tables);

	if (likely(rc == 0)) {
		memcpy(&sepapp_msg_p->app_uuid, sepapp_uuid, SEPAPP_UUID_SIZE);
		sepapp_msg_p->auth_method = cpu_to_le32(auth_method);
		/* TODO: Fill msg.auth_data as required for supported methods,
		 * e.g. client application ID */

		/* Pack SW descriptor */
		/* Set invalid session ID so in case of error the ID set
		 * in the session context remains invalid. */
		desc_q_pack_app_req_desc(&desc, op_ctx,
					 SEPAPP_REQ_TYPE_SESSION_OPEN,
					 SEP_SESSION_ID_INVALID,
					 op_ctx->spad_buf_dma_addr);
		/* Associate operation with the session */
		op_ctx->session_ctx = new_session;
		op_ctx->internal_error = false;
		rc = desc_q_enqueue(drvdata->desc_queue, &desc, true);
		if (likely(!IS_DESCQ_ENQUEUE_ERR(rc)))
			rc = 0;
	}

	if (likely(rc == 0)) {
		rc = wait_for_sep_op_result(op_ctx);
		/* Process descriptor completion */
		if (likely(rc == 0)) {
			if ((op_ctx->error_info != 0) &&
			    (op_ctx->internal_error)) {
				*sep_ret_origin = DXDI_SEP_MODULE_APP_MGR;
			} else {	/* Success or error from applet */
				*sep_ret_origin = DXDI_SEP_MODULE_APP;
			}
		} else {	/* Descriptor processing failed */
			*sep_ret_origin = DXDI_SEP_MODULE_SW_QUEUE;
			op_ctx->error_info = DXDI_ERROR_INTERNAL;
		}
	}

	if (unlikely((rc != 0) || (op_ctx->error_info != 0))) {
		mutex_lock(&new_session->session_lock);
		new_session->ref_cnt = 0;
		mutex_unlock(&new_session->session_lock);
		INVALIDATE_SESSION_IDX(*session_id);
	}
	op_ctx->op_state = USER_OP_NOP;
	sepapp_params_cleanup(op_ctx->client_ctx, app_auth_data, kapp_auth_data,
			      &sepapp_msg_p->client_params, local_dma_objs,
			      mlli_tables);

	return rc;
}

/**
 * sepapp_session_close() - Close given SeP Applet context
 * @op_ctx:
 * @session_id:
 *
 * Returns int
 */
int sepapp_session_close(struct sep_op_ctx *op_ctx, int session_id)
{
	struct sep_client_ctx *client_ctx = op_ctx->client_ctx;
	struct sep_app_session *session_ctx =
	    &client_ctx->sepapp_sessions[session_id];
	struct queue_drvdata *drvdata = client_ctx->drv_data;
	struct sep_sw_desc desc;
	int rc;
	u16 sep_session_id;

#ifdef SEP_RUNTIME_PM
	dx_sep_pm_runtime_get();
#endif
	if (!IS_VALID_SESSION_IDX(session_id)) {
		pr_err("Invalid session_id=%d\n", session_id);
		rc = -EINVAL;
		goto end;
	}
	op_ctx->op_type = SEP_OP_APP;

	mutex_lock(&session_ctx->session_lock);

	if (!IS_VALID_SESSION_CTX(session_ctx)) {
		mutex_unlock(&session_ctx->session_lock);
		pr_err("Invalid session ID %d for user %p\n",
			    session_id, client_ctx);
		op_ctx->error_info = DXDI_ERROR_BAD_CTX;
		rc = -EINVAL;
		goto end;
	}

	if (session_ctx->ref_cnt > 1) {
		mutex_unlock(&session_ctx->session_lock);
		pr_err("Invoked while still has pending commands!\n");
		op_ctx->error_info = DXDI_ERROR_FATAL;
		rc = -EBUSY;
		goto end;
	}

	sep_session_id = session_ctx->sep_session_id;/* save before release */
	/* Release host resources anyway... */
	INVALIDATE_SESSION_CTX(session_ctx);
	mutex_unlock(&session_ctx->session_lock);

	/* Now release session resources on SeP */
	/* Pack SW descriptor */
	desc_q_pack_app_req_desc(&desc, op_ctx,
				 SEPAPP_REQ_TYPE_SESSION_CLOSE, sep_session_id,
				 0);
	/* Associate operation with the session */
	op_ctx->session_ctx = session_ctx;
	op_ctx->internal_error = false;
	rc = desc_q_enqueue(drvdata->desc_queue, &desc, true);
	if (likely(!IS_DESCQ_ENQUEUE_ERR(rc)))
		rc = wait_for_sep_op_result(op_ctx);

	if (unlikely(rc != 0)) {	/* Not supposed to happen */
		pr_err(
			    "Failure is SESSION_CLOSE operation for sep_session_id=%u\n",
			    session_ctx->sep_session_id);
		op_ctx->error_info = DXDI_ERROR_FATAL;
	}

end:
#ifdef SEP_RUNTIME_PM
	dx_sep_pm_runtime_put();
#endif
	return rc;
}

static int sepapp_command_invoke(struct sep_op_ctx *op_ctx,
				 int session_id,
				 u32 command_id,
				 struct dxdi_sepapp_params *command_params,
				 struct dxdi_sepapp_kparams *command_kparams,
				 enum dxdi_sep_module *sep_ret_origin,
				 int async)
{
	int rc;
	struct sep_app_session *session_ctx =
	    &op_ctx->client_ctx->sepapp_sessions[session_id];
	struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;
	struct sep_sw_desc desc;
	struct sepapp_in_params_command_invoke *sepapp_msg_p;

	op_ctx->op_type = SEP_OP_APP;
	/* Verify that given spad_buf size can accomodate the in_params */
	BUILD_BUG_ON(sizeof(struct sepapp_in_params_command_invoke) >
		     USER_SPAD_SIZE);

	if (!IS_VALID_SESSION_IDX(session_id)) {
		pr_err("Invalid session_id=%d\n", session_id);
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
		return -EINVAL;
	}
	mutex_lock(&session_ctx->session_lock);
	if (!IS_VALID_SESSION_CTX(session_ctx)) {
		mutex_unlock(&session_ctx->session_lock);
		pr_err("Invalid session ID %d for user %p\n",
			    session_id, op_ctx->client_ctx);
		op_ctx->error_info = DXDI_ERROR_BAD_CTX;
		return -EINVAL;
	}
	session_ctx->ref_cnt++;	/* Prevent deletion while in use */
	/* Unlock to allow concurrent session use from different threads */
	mutex_unlock(&session_ctx->session_lock);

	op_ctx->spad_buf_p = dma_pool_alloc(drvdata->sep_data->spad_buf_pool,
					    GFP_KERNEL,
					    &op_ctx->spad_buf_dma_addr);
	if (unlikely(op_ctx->spad_buf_p == NULL)) {
		pr_err(
			    "Failed allocating from spad_buf_pool for SeP Applet Request message\n");
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
		rc = -ENOMEM;
		goto sepapp_command_exit;
	}
	sepapp_msg_p =
	    (struct sepapp_in_params_command_invoke *)op_ctx->spad_buf_p;

	op_ctx->async_info.dxdi_params = command_params;
	op_ctx->async_info.dxdi_kparams = command_kparams;
	op_ctx->async_info.sw_desc_params = &sepapp_msg_p->client_params;
	op_ctx->async_info.session_id = session_id;

	if (async) {
		wait_event_interruptible(op_ctx->client_ctx->memref_wq,
			op_ctx->client_ctx->memref_cnt < 1);
		mutex_lock(&session_ctx->session_lock);
		op_ctx->client_ctx->memref_cnt++;
		mutex_unlock(&session_ctx->session_lock);
	}

	mutex_lock(&drvdata->desc_queue_sequencer);
	/* Convert parameters to SeP Applet format */
	rc = dxdi_sepapp_params_to_sw_desc_params(op_ctx->client_ctx,
					  command_params,
					  command_kparams,
					  &sepapp_msg_p->client_params,
					  op_ctx->async_info.local_dma_objs,
					  op_ctx->async_info.mlli_tables);

	if (likely(rc == 0)) {
		sepapp_msg_p->command_id = cpu_to_le32(command_id);
		desc_q_pack_app_req_desc(&desc, op_ctx,
					 SEPAPP_REQ_TYPE_COMMAND_INVOKE,
					 session_ctx->sep_session_id,
					 op_ctx->spad_buf_dma_addr);
		/* Associate operation with the session */
		op_ctx->session_ctx = session_ctx;
		op_ctx->internal_error = false;
		op_ctx->op_state = USER_OP_INPROC;
		rc = desc_q_enqueue(drvdata->desc_queue, &desc, true);
		if (likely(!IS_DESCQ_ENQUEUE_ERR(rc)))
			rc = 0;

		if (async && rc != 0) {
			mutex_lock(&session_ctx->session_lock);
			op_ctx->client_ctx->memref_cnt--;
			mutex_unlock(&session_ctx->session_lock);
		}
	}
	mutex_unlock(&drvdata->desc_queue_sequencer);

	if (likely(rc == 0) && !async)
		rc = wait_for_sep_op_result(op_ctx);

	/* Process descriptor completion */
	if (likely(rc == 0)) {
		if ((op_ctx->error_info != 0) && (op_ctx->internal_error)) {
			*sep_ret_origin = DXDI_SEP_MODULE_APP_MGR;
		} else {	/* Success or error from applet */
			*sep_ret_origin = DXDI_SEP_MODULE_APP;
		}
	} else {		/* Descriptor processing failed */
		*sep_ret_origin = DXDI_SEP_MODULE_SW_QUEUE;
		op_ctx->error_info = DXDI_ERROR_INTERNAL;
	}
	if (!async) {
		op_ctx->op_state = USER_OP_NOP;
		sepapp_params_cleanup(op_ctx->client_ctx,
				command_params, command_kparams,
				&sepapp_msg_p->client_params,
				op_ctx->async_info.local_dma_objs,
				op_ctx->async_info.mlli_tables);
	}

sepapp_command_exit:
	if (!async) {
		/* Release session */
		mutex_lock(&session_ctx->session_lock);
		session_ctx->ref_cnt--;
		mutex_unlock(&session_ctx->session_lock);
	}

	return rc;

}

int sep_ioctl_sepapp_session_open(struct sep_client_ctx *client_ctx,
				  unsigned long arg)
{
	struct dxdi_sepapp_session_open_params __user *user_params =
	    (struct dxdi_sepapp_session_open_params __user *)arg;
	struct dxdi_sepapp_session_open_params params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
	    offsetof(struct dxdi_sepapp_session_open_params, session_id);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&params, user_params, input_size)) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = sepapp_session_open(&op_ctx,
				 params.app_uuid, params.auth_method,
				 &params.auth_data, &params.app_auth_data, NULL,
				 &params.session_id, &params.sep_ret_origin);

	/* Copying back from app_auth_data in case of output of "by value"... */
	if (copy_to_user(&user_params->app_auth_data, &params.app_auth_data,
			   sizeof(struct dxdi_sepapp_params))
	    || put_user(params.session_id, &user_params->session_id)
	    || put_user(params.sep_ret_origin,
			  &user_params->sep_ret_origin)) {
		pr_err("Failed writing input parameters");
		return -EFAULT;
	}

	put_user(op_ctx.error_info, &(user_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

/**
 * dx_sepapp_session_open() - Open a session with a SeP applet
 *
 * @ctx:		SeP client context
 * @sepapp_uuid:	Target applet UUID
 * @auth_method:	Session connection authentication method
 *			(Currently only 0/Public is supported)
 * @auth_data:		Pointer to authentication data - Should be NULL
 * @open_params:	Parameters for session opening
 * @session_id:		Returned session ID (on success)
 * @ret_origin:		Return code origin
 *
 * If ret_origin is not DXDI_SEP_MODULE_APP (i.e., above the applet), it must
 * be 0 on success. For DXDI_SEP_MODULE_APP it is an applet-specific return code
 */
int dx_sepapp_session_open(void *ctx,
			   u8 *sepapp_uuid,
			   u32 auth_method,
			   void *auth_data,
			   struct dxdi_sepapp_kparams *open_params,
			   int *session_id, enum dxdi_sep_module *ret_origin)
{
	struct sep_client_ctx *client_ctx = (struct sep_client_ctx *)ctx;
	struct sep_op_ctx op_ctx;
	int rc;

	op_ctx_init(&op_ctx, client_ctx);
	rc = sepapp_session_open(&op_ctx,
				 sepapp_uuid, auth_method, auth_data,
				 NULL, open_params, session_id, ret_origin);
	/* If request operation suceeded return the return code from SeP */
	if (likely(rc == 0))
		rc = op_ctx.error_info;
	op_ctx_fini(&op_ctx);
	return rc;
}
EXPORT_SYMBOL(dx_sepapp_session_open);

int sep_ioctl_sepapp_session_close(struct sep_client_ctx *client_ctx,
				   unsigned long arg)
{
	struct dxdi_sepapp_session_close_params __user *user_params =
	    (struct dxdi_sepapp_session_close_params __user *)arg;
	int session_id;
	struct sep_op_ctx op_ctx;
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	rc = __get_user(session_id, &user_params->session_id);
	if (rc) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = sepapp_session_close(&op_ctx, session_id);
	op_ctx_fini(&op_ctx);

	return rc;
}

/**
 * dx_sepapp_session_close() - Close a session with an applet
 *
 * @ctx:	SeP client context
 * @session_id: Session ID as returned from dx_sepapp_open_session()
 *
 * Return code would be 0 on success
 */
int dx_sepapp_session_close(void *ctx, int session_id)
{
	struct sep_client_ctx *client_ctx = (struct sep_client_ctx *)ctx;
	struct sep_op_ctx op_ctx;
	int rc;

	op_ctx_init(&op_ctx, client_ctx);
	rc = sepapp_session_close(&op_ctx, session_id);
	op_ctx_fini(&op_ctx);
	return rc;
}
EXPORT_SYMBOL(dx_sepapp_session_close);

int sep_ioctl_sepapp_command_invoke(struct sep_client_ctx *client_ctx,
				    unsigned long arg)
{
	struct dxdi_sepapp_command_invoke_params __user *user_params =
	    (struct dxdi_sepapp_command_invoke_params __user *)arg;
	struct dxdi_sepapp_command_invoke_params params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
	    offsetof(struct dxdi_sepapp_command_invoke_params,
		     sep_ret_origin);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&params, user_params, input_size)) {
		pr_err("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = sepapp_command_invoke(&op_ctx,
				   params.session_id, params.command_id,
				   &params.command_params, NULL,
				   &params.sep_ret_origin, 0);

	/* Copying back from command_params in case of output of "by value" */
	if (copy_to_user(&user_params->command_params,
			   &params.command_params,
			   sizeof(struct dxdi_sepapp_params))
	    || put_user(params.sep_ret_origin,
			  &user_params->sep_ret_origin)) {
		pr_err("Failed writing input parameters");
		return -EFAULT;
	}

	put_user(op_ctx.error_info, &(user_params->error_info));

	op_ctx_fini(&op_ctx);
	return rc;
}

/**
 * dx_sepapp_command_invoke() - Initiate command in the applet associated with
 *				given session ID
 *
 * @ctx:	SeP client context
 * @session_id:	The target session ID
 * @command_id:	The ID of the command to initiate (applet-specific)
 * @command_params:	The command parameters
 * @ret_origin:	The origin of the return code
 */
int dx_sepapp_command_invoke(void *ctx,
			     int session_id,
			     u32 command_id,
			     struct dxdi_sepapp_kparams *command_params,
			     enum dxdi_sep_module *ret_origin)
{
	struct sep_client_ctx *client_ctx = (struct sep_client_ctx *)ctx;
	struct sep_op_ctx op_ctx;
	int rc;

	op_ctx_init(&op_ctx, client_ctx);
	rc = sepapp_command_invoke(&op_ctx, session_id, command_id,
				   NULL, command_params, ret_origin, 0);
	/* If request operation suceeded return the return code from SeP */
	if (likely(rc == 0))
		rc = op_ctx.error_info;
	op_ctx_fini(&op_ctx);
	return rc;
}
EXPORT_SYMBOL(dx_sepapp_command_invoke);

static void async_app_handle_op_completion(struct work_struct *work)
{
	struct async_req_ctx *areq_ctx =
	    container_of(work, struct async_req_ctx, comp_work);
	struct sep_op_ctx *op_ctx = &areq_ctx->op_ctx;
	struct crypto_async_request *initiating_req = areq_ctx->initiating_req;
	int err = 0;
	struct sep_app_session *session_ctx =
	  &op_ctx->client_ctx->sepapp_sessions[op_ctx->async_info.session_id];

	SEP_LOG_DEBUG("req=%p op_ctx=%p\n", initiating_req, op_ctx);
	if (op_ctx == NULL) {
		SEP_LOG_ERR("Invalid work context (%p)\n", work);
		return;
	}

	if (op_ctx->op_state == USER_OP_COMPLETED) {

		if (unlikely(op_ctx->error_info != 0)) {
			SEP_LOG_ERR("SeP crypto-op failed (sep_rc=0x%08X)\n",
				    op_ctx->error_info);
		}
		/* Save ret_code info before cleaning op_ctx */
		err = -(op_ctx->error_info);
		if (unlikely(err == -EINPROGRESS)) {
			/* SeP error code collides with EINPROGRESS */
			SEP_LOG_ERR("Invalid SeP error code 0x%08X\n",
				    op_ctx->error_info);
			err = -EINVAL;	/* fallback */
		}
		sepapp_params_cleanup(op_ctx->client_ctx,
					op_ctx->async_info.dxdi_params,
					op_ctx->async_info.dxdi_kparams,
					op_ctx->async_info.sw_desc_params,
					op_ctx->async_info.local_dma_objs,
					op_ctx->async_info.mlli_tables);

		mutex_lock(&session_ctx->session_lock);
		session_ctx->ref_cnt--;
		mutex_unlock(&session_ctx->session_lock);

		op_ctx->client_ctx->memref_cnt--;
		wake_up_interruptible(&op_ctx->client_ctx->memref_wq);

		if (op_ctx->async_info.dxdi_kparams != NULL)
			kfree(op_ctx->async_info.dxdi_kparams);
		op_ctx_fini(op_ctx);
	} else if (op_ctx->op_state == USER_OP_INPROC) {
		/* Report with the callback the dispatch from backlog to
		   the actual processing in the SW descriptors queue
		   (Returned -EBUSY when the request was dispatched) */
		err = -EINPROGRESS;
	} else {
		SEP_LOG_ERR("Invalid state (%d) for op_ctx %p\n",
			    op_ctx->op_state, op_ctx);
		BUG();
	}

	if (likely(initiating_req->complete != NULL))
		initiating_req->complete(initiating_req, err);
	else
		SEP_LOG_ERR("Async. operation has no completion callback.\n");
}

int async_sepapp_command_invoke(void *ctx,
			     int session_id,
			     u32 command_id,
			     struct dxdi_sepapp_kparams *command_params,
			     enum dxdi_sep_module *ret_origin,
			     struct async_req_ctx *areq_ctx)
{
	struct sep_client_ctx *client_ctx = (struct sep_client_ctx *)ctx;
	struct sep_op_ctx *op_ctx = &areq_ctx->op_ctx;
	int rc;

	INIT_WORK(&areq_ctx->comp_work, async_app_handle_op_completion);
	op_ctx_init(op_ctx, client_ctx);
	op_ctx->comp_work = &areq_ctx->comp_work;
	rc = sepapp_command_invoke(op_ctx, session_id, command_id,
				   NULL, command_params, ret_origin, 1);

	if (rc == 0)
		return -EINPROGRESS;
	else
		return rc;
}

/**
 * dx_sepapp_context_alloc() - Allocate client context for SeP applets ops.
 * Returns DX_SEPAPP_CLIENT_CTX_NULL on failure.
 */
void *dx_sepapp_context_alloc(void)
{
	struct sep_client_ctx *client_ctx;

	client_ctx = kzalloc(sizeof(struct sep_client_ctx), GFP_KERNEL);
	if (client_ctx == NULL)
		return DX_SEPAPP_CLIENT_CTX_NULL;

	/* Always use queue 0 */
	init_client_ctx(&kapps_drvdata->queue[0], client_ctx);

	return (void *)client_ctx;
}
EXPORT_SYMBOL(dx_sepapp_context_alloc);

/**
 * dx_sepapp_context_free() - Free client context.
 *
 * @ctx: Client context to free.
 *
 * Returns 0 on success, -EBUSY if resources (sessions) are still allocated
 */
void dx_sepapp_context_free(void *ctx)
{
	struct sep_client_ctx *client_ctx = (struct sep_client_ctx *)ctx;
	struct queue_drvdata *drvdata = client_ctx->drv_data;

	cleanup_client_ctx(drvdata, client_ctx);
	kfree(client_ctx);
}
EXPORT_SYMBOL(dx_sepapp_context_free);

void dx_sepapp_init(struct sep_drvdata *drvdata)
{
	kapps_drvdata = drvdata;	/* Save for dx_sepapp_ API */
}

int sepapp_image_verify(u8 *addr, ssize_t size, u32 key_index, u32 magic_num)
{
	int sess_id = 0;
	enum dxdi_sep_module ret_origin;
	struct sep_client_ctx *sctx = NULL;
	u8 uuid[16] = DEFAULT_APP_UUID;
	struct dxdi_sepapp_kparams cmd_params;
	int rc = 0;

	pr_info("image verify: addr 0x%p size: %zd key_index: 0x%08X magic_num: 0x%08X\n",
		addr, size, key_index, magic_num);

	cmd_params.params_types[0] = DXDI_SEPAPP_PARAM_VAL;
	/* addr is already a physical address, so this works on
	 * a system with <= 4GB RAM.
	 * TODO revisit this if the physical address of IMR can be higher
	 */
	cmd_params.params[0].val.data[0] = (unsigned long)addr & (DMA_BIT_MASK(32));
	cmd_params.params[0].val.data[1] = 0;
	cmd_params.params[0].val.copy_dir = DXDI_DATA_TO_DEVICE;

	cmd_params.params_types[1] = DXDI_SEPAPP_PARAM_VAL;
	cmd_params.params[1].val.data[0] = (u32)size;
	cmd_params.params[1].val.data[1] = 0;
	cmd_params.params[1].val.copy_dir = DXDI_DATA_TO_DEVICE;

	cmd_params.params_types[2] = DXDI_SEPAPP_PARAM_VAL;
	cmd_params.params[2].val.data[0] = key_index;
	cmd_params.params[2].val.data[1] = 0;
	cmd_params.params[2].val.copy_dir = DXDI_DATA_TO_DEVICE;

	cmd_params.params_types[3] = DXDI_SEPAPP_PARAM_VAL;
	cmd_params.params[3].val.data[0] = magic_num;
	cmd_params.params[3].val.data[1] = 0;
	cmd_params.params[3].val.copy_dir = DXDI_DATA_TO_DEVICE;

	sctx = dx_sepapp_context_alloc();
	if (unlikely(!sctx))
		return -ENOMEM;

#ifdef SEP_RUNTIME_PM
	dx_sep_pm_runtime_get();
#endif

	rc = dx_sepapp_session_open(sctx, uuid, 0, NULL, NULL, &sess_id,
					&ret_origin);
	if (unlikely(rc != 0))
		goto failed;

	rc = dx_sepapp_command_invoke(sctx, sess_id, CMD_IMAGE_VERIFY,
					&cmd_params, &ret_origin);

	dx_sepapp_session_close(sctx, sess_id);

failed:
#ifdef SEP_RUNTIME_PM
	dx_sep_pm_runtime_put();
#endif

	dx_sepapp_context_free(sctx);
	return rc;
}
EXPORT_SYMBOL(sepapp_image_verify);

int sepapp_hdmi_status(u8 status, u8 bksv[5])
{
	int sess_id = 0;
	enum dxdi_sep_module ret_origin;
	struct sep_client_ctx *sctx = NULL;
	u8 uuid[16] = HDCP_APP_UUID;
	struct dxdi_sepapp_kparams cmd_params;
	int rc = 0;

	pr_info("Hdmi status: status 0x%02x\n", status);

	memset(&cmd_params, 0, sizeof(struct dxdi_sepapp_kparams));

	cmd_params.params_types[0] = DXDI_SEPAPP_PARAM_VAL;
	cmd_params.params[0].val.data[0] = status;
	cmd_params.params[0].val.data[1] = 0;
	cmd_params.params[0].val.copy_dir = DXDI_DATA_TO_DEVICE;

	cmd_params.params_types[1] = DXDI_SEPAPP_PARAM_VAL;
	memcpy(&cmd_params.params[1].val.data[0], bksv, sizeof(u32));
	memcpy((uint8_t *)&cmd_params.params[1].val.data[1],
		&bksv[4], sizeof(u8));
	cmd_params.params[1].val.copy_dir = DXDI_DATA_TO_DEVICE;

	sctx = dx_sepapp_context_alloc();
	if (unlikely(!sctx))
		return -ENOMEM;

#ifdef SEP_RUNTIME_PM
	dx_sep_pm_runtime_get();
#endif

	rc = dx_sepapp_session_open(sctx, uuid, 0, NULL, NULL, &sess_id,
				    &ret_origin);
	if (unlikely(rc != 0))
		goto failed;

	rc = dx_sepapp_command_invoke(sctx, sess_id, HDCP_RX_HDMI_STATUS,
				      &cmd_params, &ret_origin);

	dx_sepapp_session_close(sctx, sess_id);

failed:
#ifdef SEP_RUNTIME_PM
	dx_sep_pm_runtime_put();
#endif

	dx_sepapp_context_free(sctx);
	return rc;
}
EXPORT_SYMBOL(sepapp_hdmi_status);
