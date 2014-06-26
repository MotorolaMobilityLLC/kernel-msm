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

#ifndef __SEP_RPC_H__
#define __SEP_RPC_H__

/* SeP RPC infrastructure API */
#ifdef __KERNEL__
#include <linux/types.h>

#else
#include "dx_pal_types.h"

#endif /*__KERNEL__*/

/* Maximum size of SeP RPC message in bytes */
#define SEP_RPC_MAX_MSG_SIZE 8191
#define SEP_RPC_MAX_WORKSPACE_SIZE 8191

/* The maximum allowed user memory references per function
   (CRYS requires only 2, but GPAPI/TEE needs up to 4) */
#define SEP_RPC_MAX_MEMREF_PER_FUNC 4

/* If this macro is not provided by the includer of this file,
   the log message would be dropped */
#ifndef SEP_RPC_LOG
#define SEP_RPC_LOG(format, ...) do {} while (0)
#endif

#define SEP_RPC_ASSERT(cond, inval_param_retcode) {\
	if (!(cond)) {\
		SEP_RPC_LOG("SEP_RPC_ASSERT: %s\n", #cond);\
		return inval_param_retcode;\
	} \
}

/* NOTE:
   All data must be in little (SeP) endian */

enum seprpc_retcode {
	SEPRPC_RET_OK = 0,
	SEPRPC_RET_ERROR,	/*Generic error code (not one of the others) */
	SEPRPC_RET_EINVAL_AGENT,	/* Unknown agent ID */
	SEPRPC_RET_EINVAL_FUNC,	/* Unknown function ID for given agent */
	SEPRPC_RET_EINVAL,	/* Invalid parameter */
	SEPRPC_RET_ENORSC,	/* Not enough resources to complete request */
	SEPRPC_RET_RESERVE32 = 0x7FFFFFFF	/* assure this enum is 32b */
};

enum seprpc_memref_type {
	SEPRPC_MEMREF_NULL = 0,	/* Invalid memory reference */
	SEPRPC_MEMREF_EMBED = 1,/* Data embedded in parameters message */
	SEPRPC_MEMREF_DLLI = 2,
	SEPRPC_MEMREF_MLLI = 3,
	SEPRPC_MEMREF_MAX = SEPRPC_MEMREF_MLLI,
	SEPRPC_MEMREF_RESERVE32 = 0x7FFFFFFF	/* assure this enum is 32b */
};

#pragma pack(push)
#pragma pack(4)
/* A strcuture to pass host memory reference */
struct seprpc_memref {
	enum seprpc_memref_type ref_type;
	u32 location;
	u32 size;
	u32 count;
	/* SEPRPC_MEMREF_EMBED: location= offset in SepRpc_Params .
	 * size= data size in bytes. count= N/A */
	/* SEPRPC_MEMREF_DLLI: location= DMA address of data in host memory.
	 * size= data size in bytes. count= N/A. */
	/* SEPRPC_MEMREF_MLLI: location= DMA address of first MLLI table.
	 * size= size in bytes of first table.
	 * count= Num. of MLLI tables. */
};

struct seprpc_params {
	u32 num_of_memrefs;/* Number of elements in the memRef array */
	struct seprpc_memref memref[1];
	/* This array is actually in the size of numOfMemRefs
	 * (i.e., it is just a placeholder that may be void) */
	/*   Following this array comes the function-specific parameters */
} __attribute__ ((__may_alias__));

#pragma pack(pop)

#endif /*__SEP_RPC_H__*/
