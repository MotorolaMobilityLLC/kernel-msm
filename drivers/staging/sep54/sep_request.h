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

#ifndef _SEP_REQUEST_H_
#define _SEP_REQUEST_H_
#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif
#include "dx_bitops.h"

#define DX_SEP_REQUEST_GPR_IDX 3

#define DX_SEP_REQUEST_4KB_MASK 0xFFF
#define DX_SEP_REQUEST_MIN_BUF_SIZE (4*1024)
#define DX_SEP_REQUEST_MAX_BUF_SIZE (32*1024)

/* Protocol error codes */
#define DX_SEP_REQUEST_SUCCESS 0x00
#define DX_SEP_REQUEST_OUT_OF_SYNC_ERR 0x01
#define DX_SEP_REQUEST_INVALID_REQ_SIZE_ERR 0x02
#define DX_SEP_REQUEST_INVALID_AGENT_ID_ERR 0x03

/* Sep Request GPR3 format (Sep to Host) */
#define DX_SEP_REQUEST_AGENT_ID_BIT_OFFSET 0
#define DX_SEP_REQUEST_AGENT_ID_BIT_SIZE 8
#define DX_SEP_REQUEST_COUNTER_BIT_OFFSET 8
#define DX_SEP_REQUEST_COUNTER_BIT_SIZE 8
#define DX_SEP_REQUEST_REQ_LEN_BIT_OFFSET 16
#define DX_SEP_REQUEST_REQ_LEN_BIT_SIZE 16

/* Sep Request GPR3 format (Host to Sep) */
#define DX_SEP_REQUEST_RETURN_CODE_BIT_OFFSET 0
#define DX_SEP_REQUEST_RETURN_CODE_BIT_SIZE 8
/* #define DX_SEP_REQUEST_COUNTER_BIT_OFFSET 8 */
/* #define DX_SEP_REQUEST_COUNTER_BIT_SIZE 8 */
#define DX_SEP_REQUEST_RESP_LEN_BIT_OFFSET 16
#define DX_SEP_REQUEST_RESP_LEN_BIT_SIZE 16

/* Get/Set macros */
#define SEP_REQUEST_GET_AGENT_ID(gpr) BITFIELD_GET(                           \
	(gpr), DX_SEP_REQUEST_AGENT_ID_BIT_OFFSET,                            \
	DX_SEP_REQUEST_AGENT_ID_BIT_SIZE)
#define SEP_REQUEST_SET_AGENT_ID(gpr, val) BITFIELD_SET(                      \
	(gpr), DX_SEP_REQUEST_AGENT_ID_BIT_OFFSET,                            \
	DX_SEP_REQUEST_AGENT_ID_BIT_SIZE, (val))
#define SEP_REQUEST_GET_RETURN_CODE(gpr) BITFIELD_GET(                        \
	(gpr), DX_SEP_REQUEST_RETURN_CODE_BIT_OFFSET,                         \
	DX_SEP_REQUEST_RETURN_CODE_BIT_SIZE)
#define SEP_REQUEST_SET_RETURN_CODE(gpr, val) BITFIELD_SET(                   \
	(gpr), DX_SEP_REQUEST_RETURN_CODE_BIT_OFFSET,                         \
	DX_SEP_REQUEST_RETURN_CODE_BIT_SIZE, (val))
#define SEP_REQUEST_GET_COUNTER(gpr) BITFIELD_GET(                            \
	(gpr), DX_SEP_REQUEST_COUNTER_BIT_OFFSET,                             \
	DX_SEP_REQUEST_COUNTER_BIT_SIZE)
#define SEP_REQUEST_SET_COUNTER(gpr, val) BITFIELD_SET(                       \
	(gpr), DX_SEP_REQUEST_COUNTER_BIT_OFFSET,                             \
	DX_SEP_REQUEST_COUNTER_BIT_SIZE, (val))
#define SEP_REQUEST_GET_REQ_LEN(gpr) BITFIELD_GET(                            \
	(gpr), DX_SEP_REQUEST_REQ_LEN_BIT_OFFSET,                             \
	DX_SEP_REQUEST_REQ_LEN_BIT_SIZE)
#define SEP_REQUEST_SET_REQ_LEN(gpr, val) BITFIELD_SET(                       \
	(gpr), DX_SEP_REQUEST_REQ_LEN_BIT_OFFSET,                             \
	DX_SEP_REQUEST_REQ_LEN_BIT_SIZE, (val))
#define SEP_REQUEST_GET_RESP_LEN(gpr) BITFIELD_GET(                           \
	(gpr), DX_SEP_REQUEST_RESP_LEN_BIT_OFFSET,                            \
	DX_SEP_REQUEST_RESP_LEN_BIT_SIZE)
#define SEP_REQUEST_SET_RESP_LEN(gpr, val) BITFIELD_SET(                      \
	(gpr), DX_SEP_REQUEST_RESP_LEN_BIT_OFFSET,                            \
	DX_SEP_REQUEST_RESP_LEN_BIT_SIZE, (val))

#endif /*_SEP_REQUEST_H_*/
