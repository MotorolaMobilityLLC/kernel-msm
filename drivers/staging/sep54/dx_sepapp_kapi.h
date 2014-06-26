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
/**
 * Kernel API for Host-to-SeP Applet request API.
 */
#ifndef __DX_SEPAPP_KAPI_H__
#define __DX_SEPAPP_KAPI_H__

#include <linux/types.h>
#include "dx_driver_abi.h"
#include "crypto_api.h"

/**
 * struct dxdi_kmemref - Kernel memory reference
 * @dma_direction:	Planned DMA direction
 * @sgl:	Scatter/Gather list of given buffer (NULL if using buf_p)
 * @nbytes:	Size in bytes of data referenced by "sgl"
 */
struct dxdi_kmemref {
	enum dxdi_data_direction dma_direction;
	struct scatterlist *sgl;
	unsigned long nbytes;	/* data size */
};

/**
 * dxdi_sepapp_kparams - Kernel parameters description for dx_sepapp_* func.
 * @params_types:	The type of each paramter in params[] array
 * @params:		The given parameters description
 */
struct dxdi_sepapp_kparams {
	enum dxdi_sepapp_param_type params_types[SEP_APP_PARAMS_MAX];
	union {
		struct dxdi_val_param val;	/* DXDI_SEPAPP_PARAM_VAL */
		struct dxdi_kmemref kmemref;	/* DXDI_SEPAPP_PARAM_MEMREF */
	} params[SEP_APP_PARAMS_MAX];
};

#define DX_SEPAPP_CLIENT_CTX_NULL NULL

/*******************************/
/* Host-to-SeP Applet requests */
/*******************************/

/**
 * dx_sepapp_context_alloc() - Allocate client context for SeP applets ops.
 * Returns DX_SEPAPP_CLIENT_CTX_NULL on failure.
 */
void *dx_sepapp_context_alloc(void);

/**
 * dx_sepapp_context_free() - Free client context.
 *
 * @ctx: Client context to free.
 */
void dx_sepapp_context_free(void *ctx);

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
			   int *session_id, enum dxdi_sep_module *ret_origin);

/**
 * dx_sepapp_session_close() - Close a session with an applet
 *
 * @ctx:	SeP client context
 * @session_id: Session ID as returned from dx_sepapp_open_session()
 *
 * Return code would be 0 on success
 */
int dx_sepapp_session_close(void *ctx, int session_id);

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
			     enum dxdi_sep_module *ret_origin);

int async_sepapp_command_invoke(void *ctx,
			     int session_id,
			     u32 command_id,
			     struct dxdi_sepapp_kparams *command_params,
			     enum dxdi_sep_module *ret_origin,
			     struct async_req_ctx *areq_ctx);

#endif /*__DX_SEPAPP_KAPI_H__*/
