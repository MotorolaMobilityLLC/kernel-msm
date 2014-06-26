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

/***************************************************************************
 *  This file provides the CC-init ABI (SeP-Host binary interface)         *
 ***************************************************************************/

#ifndef __DX_INIT_CC_ABI_H__
#define __DX_INIT_CC_ABI_H__

#ifdef __KERNEL__
#include <linux/types.h>
#else
/* For SeP code environment */
#include <stdint.h>
#endif
#include "dx_bitops.h"

#ifndef INT16_MAX
#define INT16_MAX		(32767)
#endif
#ifndef INT32_MAX
#define INT32_MAX		(2147483647)
#endif

/***********************************/
/* SeP to host communication       */
/***********************************/
/* GPRs for CC-init state/status from SeP */
#define DX_SEP_STATE_GPR_IDX               7	/* SeP state */
#define DX_SEP_STATUS_GPR_IDX              6	/* SeP status */
/* GPRs used for passing driver init. parameters */
/* (Valid while in DX_SEP_STATE_DONE_COLD_BOOT) */
#define DX_SEP_INIT_SEP_PROPS_GPR_IDX      3	/* SEP properties passed to the
						 * driver (see fields below) */
#define DX_SEP_INIT_FW_PROPS_GPR_IDX      2	/* FW properties passed to the
						 * driver (see fields below) */
#define DX_SEP_INIT_FW_VER_GPR_IDX         1	/* SeP FW images version */
#define DX_SEP_INIT_ROM_VER_GPR_IDX        0	/* SeP ROM image version */

/* Debugging "stdout" tunnel via GPR5 - see sep_driver_cc54.c for details */
#define DX_SEP_HOST_PRINTF_GPR_IDX         5

/* Fields in DX_SEP_INIT_FW_PROPS_GPR_IDX */
/* MLLI table size in bytes */
#define DX_SEP_INIT_MLLI_TBL_SIZE_BIT_OFFSET	0
#define DX_SEP_INIT_MLLI_TBL_SIZE_BIT_SIZE	12
/* Maximum number of work queues supported */
#define DX_SEP_INIT_NUM_OF_QUEUES_BIT_OFFSET	12
#define DX_SEP_INIT_NUM_OF_QUEUES_BIT_SIZE	4
/* Number of availabel context cache entries */
#define DX_SEP_INIT_CACHE_CTX_SIZE_BIT_OFFSET	16
#define DX_SEP_INIT_CACHE_CTX_SIZE_BIT_SIZE     8

/* Fields in DX_SEP_INIT_SEP_PROPS_GPR_IDX */
/* SEP frequency */
#define DX_SEP_INIT_SEP_FREQUENCY_BIT_OFFSET	0
#define DX_SEP_INIT_SEP_FREQUENCY_BIT_SIZE	12

/***********************************/
/* Host to SeP communication       */
/***********************************/
/* GPRs for requests from host to SeP */
#define DX_HOST_REQ_GPR_IDX 7	/* Host-to-SeP requests */
#define DX_HOST_REQ_PARAM_GPR_IDX 6	/* Host request parameters */
/* The parameters in GPR6 must be ready before writing the request to GPR7*/

/* MAGIC value of TLV "FIRST" parameter */
#define DX_FW_INIT_PARAM_FIRST_MAGIC	0x3243F6A8

/* Type-Length word manipulation macros */
/* Note that all TLV communication assumes little-endian words */
/* (i.e., responsibility of host driver to convert to LE before passing) */
#define DX_TL_WORD(type, length)  ((length) << 16 | (type))
#define DX_TL_GET_TYPE(tl_word)   ((tl_word) & 0xFFFF)
#define DX_TL_GET_LENGTH(tl_word) ((tl_word) >> 16)

/* Macros for Assembly code */
#define ASM_DX_SEP_STATE_ILLEGAL_INST   0x100
#define ASM_DX_SEP_STATE_STACK_OVERFLOW 0x200
/* SeP states over (SeP-to-host) GPR7*/
enum dx_sep_state {
	DX_SEP_STATE_OFF = 0x0,
	DX_SEP_STATE_FATAL_ERROR = 0x1,
	DX_SEP_STATE_START_SECURE_BOOT = 0x2,
	DX_SEP_STATE_PROC_COLD_BOOT = 0x4,
	DX_SEP_STATE_PROC_WARM_BOOT = 0x8,
	DX_SEP_STATE_DONE_COLD_BOOT = 0x10,
	DX_SEP_STATE_DONE_WARM_BOOT = 0x20,
	/*DX_SEP_STATE_BM_ERR           = 0x40, */
	/*DX_SEP_STATE_SECOND_BM_ERR    = 0x80, */
	DX_SEP_STATE_ILLEGAL_INST = ASM_DX_SEP_STATE_ILLEGAL_INST,
	DX_SEP_STATE_STACK_OVERFLOW = ASM_DX_SEP_STATE_STACK_OVERFLOW,
	/* Response to DX_HOST_REQ_FW_INIT: */
	DX_SEP_STATE_DONE_FW_INIT = 0x400,
	DX_SEP_STATE_PROC_SLEEP_MODE = 0x800,
	DX_SEP_STATE_DONE_SLEEP_MODE = 0x1000,
	DX_SEP_STATE_FW_ABORT = 0xBAD0BAD0,
	DX_SEP_STATE_ROM_ABORT = 0xBAD1BAD1,
	DX_SEP_STATE_RESERVE32B = INT32_MAX
};

/* Host requests over (host-to-SeP) GPR7 */
enum dx_host_req {
	DX_HOST_REQ_RELEASE_CRYPTO_ENGINES = 0x0,
	DX_HOST_REQ_ACQUIRE_CRYPTO_ENGINES = 0x2,
	DX_HOST_REQ_CC_INIT = 0x1,
	DX_HOST_REQ_FW_INIT = 0x4,
	DX_HOST_REQ_SEP_SLEEP = 0x8,
	DX_HOST_REQ_RESERVE32B = INT32_MAX
};

/* Init. TLV parameters from host to SeP */
/* Some parameters are used by CC-init flow with DX_HOST_REQ_CC_INIT          *
 * and the others by the host driver initialization with DX_HOST_REQ_FW_INIT  */
enum dx_fw_init_tlv_params {
	DX_FW_INIT_PARAM_NULL = 0,
	/* Common parameters */
	DX_FW_INIT_PARAM_FIRST = 1,	/* Param.=FIRST_MAGIC */
	DX_FW_INIT_PARAM_LAST = 2,	/* Param.=checksum 32b */

	/* CC-init. parameters */
	DX_FW_INIT_PARAM_DISABLE_MODULES = 3,
	DX_FW_INIT_PARAM_HOST_AXI_CONFIG = 4,
	DX_FW_INIT_PARAM_HOST_DEF_APPLET_CONFIG = 5,
	DX_FW_INIT_PARAM_SEP_FREQ = 6,

	/* Host driver (post-cold-boot) parameters */
	/* Number of descriptor queues: Length = 1 */
	DX_FW_INIT_PARAM_NUM_OF_DESC_QS = 0x101,
	/* The following parameters provide an array with value per queue: */
	/* Length = num. of queues */
	DX_FW_INIT_PARAM_DESC_QS_ADDR = 0x102,	/* Queue base addr. */
	DX_FW_INIT_PARAM_DESC_QS_SIZE = 0x103,	/* Queue size(byte) */
	/* FW context cache partition (num. of entries) per queue */
	DX_FW_INIT_PARAM_CTX_CACHE_PART = 0x104,
	/* sep request module parameters ( msg and response buffers and size */
	DX_FW_INIT_PARAM_SEP_REQUEST_PARAMS = 0x105,
	DX_FW_INIT_PARAM_RESERVE16B = INT16_MAX
};

/* FW-init. error code encoding - GPR6 contents in DX_SEP_STATE_DONE_FW_INIT  */
/* | 0xE | Param. Type | Error code |  */
/* |--4--|-----16------|----12------|  */
#define DX_FW_INIT_ERR_CODE_SIZE 12
#define DX_FW_INIT_ERR_PARAM_TYPE_SHIFT DX_FW_INIT_ERR_CODE_SIZE
#define DX_FW_INIT_ERR_PARAM_TYPE_SIZE 16
#define DX_FW_INIT_ERR_TYPE_SHIFT \
	(DX_FW_INIT_ERR_PARAM_TYPE_SHIFT + DX_FW_INIT_ERR_PARAM_TYPE_SIZE)
#define DX_FW_INIT_ERR_TYPE_SIZE 4
#define DX_FW_INIT_ERR_TYPE_VAL 0xE

#define DX_SEP_REQUEST_PARAM_MSG_LEN		3

/* Build error word to put in status GPR6 */
#define DX_FW_INIT_ERR_WORD(err_code, param_type)                   \
	((DX_FW_INIT_ERR_TYPE_VAL << DX_FW_INIT_ERR_TYPE_SHIFT) |   \
	 ((param_type & BITMASK(DX_FW_INIT_ERR_PARAM_TYPE_SIZE)) << \
	  DX_FW_INIT_ERR_PARAM_TYPE_SHIFT) |                        \
	 (err_code))
/* Parse status of DX_SEP_STATE_DONE_FW_INIT*/
#define DX_FW_INIT_IS_SUCCESS(status_word) ((err_word) == 0)
#define DX_FW_INIT_GET_ERR_CODE(status_word) \
	 ((status_word) & BITMASK(DX_FW_INIT_ERR_CODE_SIZE))

/* FW INIT Error codes */
/* extract from the status word - GPR6 - using DX_FW_INIT_GET_ERR_CODE() */
enum dx_fw_init_error_code {
	DX_FW_INIT_ERR_INVALID_TYPE = 0x001,
	DX_FW_INIT_ERR_INVALID_LENGTH = 0x002,
	DX_FW_INIT_ERR_INVALID_VALUE = 0x003,
	DX_FW_INIT_ERR_PARAM_MISSING = 0x004,
	DX_FW_INIT_ERR_NOT_SUPPORTED = 0x005,
	DX_FW_INIT_ERR_RNG_FAILURE = 0x00F,
	DX_FW_INIT_ERR_MALLOC_FAILURE = 0x0FC,
	DX_FW_INIT_ERR_INIT_FAILURE = 0x0FD,
	DX_FW_INIT_ERR_TIMEOUT = 0x0FE,
	DX_FW_INIT_ERR_GENERAL_FAILURE = 0x0FF
};

#endif /*__DX_INIT_CC_ABI_H__*/
