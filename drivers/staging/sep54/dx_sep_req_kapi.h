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
 * Kernel services API of SeP Request agent API.
 */
#ifndef __DX_SEP_REQ_KAPI_H__
#define __DX_SEP_REQ_KAPI_H__

#include <linux/types.h>

/*******************************/
/* SeP-to-Host request agents  */
/*******************************/

/**
 * dx_sep_req_register_agent() - Register an agent
 * @agent_id: The agent ID
 * @max_buf_size: A pointer to the max buffer size
 *
 * Returns int 0 on success
 */
int dx_sep_req_register_agent(u8 agent_id, u32 *max_buf_size);

/**
 * dx_sep_req_unregister_agent() - Unregister an agent
 * @agent_id: The agent ID
 *
 * Returns int 0 on success
 */
int dx_sep_req_unregister_agent(u8 agent_id);

/**
 * dx_sep_req_wait_for_request() - Wait from an incoming sep request
 * @agent_id: The agent ID
 * @sep_req_buf_p: Pointer to the incoming request buffer
 * @req_buf_size: Pointer to the incoming request size
 *
 * Returns int 0 on success
 */
int dx_sep_req_wait_for_request(u8 agent_id, u8 *sep_req_buf_p,
				u32 *req_buf_size);

/**
 * dx_sep_req_send_response() - Send a response to the sep
 * @agent_id: The agent ID
 * @host_resp_buf_p: Pointer to the outgoing response buffer
 * @resp_buf_size: Pointer to the outgoing response size
 *
 * Returns int 0 on success
 */
int dx_sep_req_send_response(u8 agent_id, u8 *host_resp_buf_p,
			     u32 resp_buf_size);

#endif /*__DX_SEP_REQ_KAPI_H__*/
