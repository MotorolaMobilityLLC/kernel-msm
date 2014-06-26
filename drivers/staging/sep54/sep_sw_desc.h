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

#ifndef _SEP_SW_DESC_H_
#define _SEP_SW_DESC_H_
#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif
#include "dx_bitops.h"
#include "sep_rpc.h"

/* Common descriptor fields access (type independent) */
/* To be used with fields: TYPE, RET_CODE, COOKIE     */
#define SEP_SW_DESC_GET(desc_p, desc_field) BITFIELD_GET(                     \
	((u32 *)(desc_p))[SEP_SW_DESC_ ## desc_field ## _WORD_OFFSET],   \
	SEP_SW_DESC_ ## desc_field ## _BIT_OFFSET,			      \
	SEP_SW_DESC_ ## desc_field ## _BIT_SIZE)
#define SEP_SW_DESC_SET(desc_p, desc_field, new_val) BITFIELD_SET(            \
	((u32 *)(desc_p))[SEP_SW_DESC_ ## desc_field ## _WORD_OFFSET],   \
	SEP_SW_DESC_ ## desc_field ## _BIT_OFFSET,			      \
	SEP_SW_DESC_ ## desc_field ## _BIT_SIZE,			      \
	new_val)

/* Type specific descriptor fields access */
#define SEP_SW_DESC_GET4TYPE(desc_p, desc_type, desc_field) BITFIELD_GET(     \
	((u32 *)(desc_p))                                                \
	[SEP_SW_DESC_ ## desc_type ## _ ## desc_field ## _WORD_OFFSET],	      \
	SEP_SW_DESC_ ## desc_type ## _ ## desc_field ##  _BIT_OFFSET,	      \
	SEP_SW_DESC_ ## desc_type ## _ ## desc_field ## _BIT_SIZE)
#define SEP_SW_DESC_SET4TYPE(desc_p, desc_type, desc_field, new_val)          \
	BITFIELD_SET(							      \
	((u32 *)(desc_p))                                                \
	[SEP_SW_DESC_ ## desc_type ## _ ## desc_field ## _WORD_OFFSET],       \
	SEP_SW_DESC_ ## desc_type ## _ ## desc_field ##  _BIT_OFFSET,	      \
	SEP_SW_DESC_ ## desc_type ## _ ## desc_field ## _BIT_SIZE, new_val)

#define SEP_SW_DESC_INIT(desc_p) \
	memset(desc_p, 0, SEP_SW_DESC_WORD_SIZE * sizeof(u32))

/* Total descriptor size in 32b words */
#define SEP_SW_DESC_WORD_SIZE 8
#define SEP_SW_DESC_WORD_SIZE_LOG 3

/***********************************/
/* Common bit fields definitions   */
/***********************************/
 /* Descriptor type: TYPE */
#define SEP_SW_DESC_TYPE_WORD_OFFSET 0
#define SEP_SW_DESC_TYPE_BIT_OFFSET 0
#define SEP_SW_DESC_TYPE_BIT_SIZE 4
/* Descriptor type encoding */
enum sep_sw_desc_type {
	SEP_SW_DESC_TYPE_NULL = 0,
	SEP_SW_DESC_TYPE_CRYPTO_OP = 0x1,
	SEP_SW_DESC_TYPE_RPC_MSG = 0x2,
	SEP_SW_DESC_TYPE_APP_REQ = 0x3,
	SEP_SW_DESC_TYPE_LOAD_OP = 0x4,
	SEP_SW_DESC_TYPE_COMBINED_OP = 0x5,
	SEP_SW_DESC_TYPE_SLEEP_REQ = 0x6,
	SEP_SW_DESC_TYPE_DEBUG = 0xF
	    /* Only 4 bits - do not extend to 32b */
};

enum sep_sw_desc_retcode {
	SEP_SW_DESC_RET_OK = 0,
	SEP_SW_DESC_RET_EINVAL_DESC_TYPE	/* Invalid descriptor type */
};

/* Return code: RET_CODE */
#define SEP_SW_DESC_RET_CODE_WORD_OFFSET 6
#define SEP_SW_DESC_RET_CODE_BIT_OFFSET 0
#define SEP_SW_DESC_RET_CODE_BIT_SIZE 32

/* Descriptor cookie: COOKIE */
#define SEP_SW_DESC_COOKIE_WORD_OFFSET 7
#define SEP_SW_DESC_COOKIE_BIT_OFFSET 0
#define SEP_SW_DESC_COOKIE_BIT_SIZE 32

/****************************************/
/* Crypto-Op descriptor type: CRYPTO_OP */
/****************************************/

/* L bit: Load cache: L */
#define SEP_SW_DESC_CRYPTO_OP_L_WORD_OFFSET 0
#define SEP_SW_DESC_CRYPTO_OP_L_BIT_OFFSET 31
#define SEP_SW_DESC_CRYPTO_OP_L_BIT_SIZE 1

/* I bit: Initialize context: I */
#define SEP_SW_DESC_CRYPTO_OP_I_WORD_OFFSET 0
#define SEP_SW_DESC_CRYPTO_OP_I_BIT_OFFSET 30
#define SEP_SW_DESC_CRYPTO_OP_I_BIT_SIZE 1

/* Process mode: PROC_MODE */
#define SEP_SW_DESC_CRYPTO_OP_PROC_MODE_WORD_OFFSET 0
#define SEP_SW_DESC_CRYPTO_OP_PROC_MODE_BIT_OFFSET 28
#define SEP_SW_DESC_CRYPTO_OP_PROC_MODE_BIT_SIZE 2
/* Process mode field options */
enum sep_proc_mode {
	SEP_PROC_MODE_NOP = 0,
	SEP_PROC_MODE_PROC_T = 1,	/* Process (Text data) */
	SEP_PROC_MODE_FIN = 2,	/* Finalize (optional: with text data) */
	SEP_PROC_MODE_PROC_A = 3	/* Process (Additional/Auth. data) */
	    /* Only 2b - do not extend to 32b */
};

/* SeP/FW Cache Index: FW_CACHE_IDX */
#define SEP_SW_DESC_CRYPTO_OP_FW_CACHE_IDX_WORD_OFFSET 0
#define SEP_SW_DESC_CRYPTO_OP_FW_CACHE_IDX_BIT_OFFSET 4
#define SEP_SW_DESC_CRYPTO_OP_FW_CACHE_IDX_BIT_SIZE 8

/* HCB address: HCB_ADDR */
#define SEP_SW_DESC_CRYPTO_OP_HCB_ADDR_WORD_OFFSET 3
#define SEP_SW_DESC_CRYPTO_OP_HCB_ADDR_BIT_OFFSET 0
#define SEP_SW_DESC_CRYPTO_OP_HCB_ADDR_BIT_SIZE 32

/* IFT: IFT_ADDR, IFT_SIZE, IFT_NUM */
#define SEP_SW_DESC_CRYPTO_OP_IFT_ADDR_WORD_OFFSET 1
#define SEP_SW_DESC_CRYPTO_OP_IFT_ADDR_BIT_OFFSET 0
#define SEP_SW_DESC_CRYPTO_OP_IFT_ADDR_BIT_SIZE 32
#define SEP_SW_DESC_CRYPTO_OP_IFT_SIZE_WORD_OFFSET 4
#define SEP_SW_DESC_CRYPTO_OP_IFT_SIZE_BIT_OFFSET 0
#define SEP_SW_DESC_CRYPTO_OP_IFT_SIZE_BIT_SIZE 16
#define SEP_SW_DESC_CRYPTO_OP_IFT_NUM_WORD_OFFSET 5
#define SEP_SW_DESC_CRYPTO_OP_IFT_NUM_BIT_OFFSET 0
#define SEP_SW_DESC_CRYPTO_OP_IFT_NUM_BIT_SIZE 16

/* OFT: OFT_ADDR, OFT_SIZE, OFT_NUM */
#define SEP_SW_DESC_CRYPTO_OP_OFT_ADDR_WORD_OFFSET 2
#define SEP_SW_DESC_CRYPTO_OP_OFT_ADDR_BIT_OFFSET 0
#define SEP_SW_DESC_CRYPTO_OP_OFT_ADDR_BIT_SIZE 32
#define SEP_SW_DESC_CRYPTO_OP_OFT_SIZE_WORD_OFFSET 4
#define SEP_SW_DESC_CRYPTO_OP_OFT_SIZE_BIT_OFFSET 16
#define SEP_SW_DESC_CRYPTO_OP_OFT_SIZE_BIT_SIZE 16
#define SEP_SW_DESC_CRYPTO_OP_OFT_NUM_WORD_OFFSET 5
#define SEP_SW_DESC_CRYPTO_OP_OFT_NUM_BIT_OFFSET 16
#define SEP_SW_DESC_CRYPTO_OP_OFT_NUM_BIT_SIZE 16

/********************************************/
/* Combined-Op descriptor type: COMBINED_OP */
/********************************************/

/* L bit: Load cache: L */
#define SEP_SW_DESC_COMBINED_OP_L_WORD_OFFSET \
	SEP_SW_DESC_CRYPTO_OP_L_WORD_OFFSET
#define SEP_SW_DESC_COMBINED_OP_L_BIT_OFFSET SEP_SW_DESC_CRYPTO_OP_L_BIT_OFFSET
#define SEP_SW_DESC_COMBINED_OP_L_BIT_SIZE SEP_SW_DESC_CRYPTO_OP_L_BIT_SIZE

/* I bit: Initialize context: I */
#define SEP_SW_DESC_COMBINED_OP_I_WORD_OFFSET \
	SEP_SW_DESC_CRYPTO_OP_I_WORD_OFFSET
#define SEP_SW_DESC_COMBINED_OP_I_BIT_OFFSET SEP_SW_DESC_CRYPTO_OP_I_BIT_OFFSET
#define SEP_SW_DESC_COMBINED_OP_I_BIT_SIZE SEP_SW_DESC_CRYPTO_OP_I_BIT_SIZE

/* Process mode: PROC_MODE */
#define SEP_SW_DESC_COMBINED_OP_PROC_MODE_WORD_OFFSET \
			SEP_SW_DESC_CRYPTO_OP_PROC_MODE_WORD_OFFSET
#define SEP_SW_DESC_COMBINED_OP_PROC_MODE_BIT_OFFSET \
			SEP_SW_DESC_CRYPTO_OP_PROC_MODE_BIT_OFFSET
#define SEP_SW_DESC_COMBINED_OP_PROC_MODE_BIT_SIZE \
			SEP_SW_DESC_CRYPTO_OP_PROC_MODE_BIT_SIZE

/* Configuration scheme: CONFIG_SCHEME */
#define SEP_SW_DESC_COMBINED_OP_CONFIG_SCHEME_WORD_OFFSET \
			SEP_SW_DESC_CRYPTO_OP_HCB_ADDR_WORD_OFFSET
#define SEP_SW_DESC_COMBINED_OP_CONFIG_SCHEME_BIT_OFFSET \
			SEP_SW_DESC_CRYPTO_OP_HCB_ADDR_BIT_OFFSET
#define SEP_SW_DESC_COMBINED_OP_CONFIG_SCHEME_BIT_SIZE \
			SEP_SW_DESC_CRYPTO_OP_HCB_ADDR_BIT_SIZE

/* IFT: IFT_ADDR, IFT_SIZE, IFT_NUM */
#define SEP_SW_DESC_COMBINED_OP_IFT_ADDR_WORD_OFFSET \
			SEP_SW_DESC_CRYPTO_OP_IFT_ADDR_WORD_OFFSET
#define SEP_SW_DESC_COMBINED_OP_IFT_ADDR_BIT_OFFSET \
			SEP_SW_DESC_CRYPTO_OP_IFT_ADDR_BIT_OFFSET
#define SEP_SW_DESC_COMBINED_OP_IFT_ADDR_BIT_SIZE \
			SEP_SW_DESC_CRYPTO_OP_IFT_ADDR_BIT_SIZE
#define SEP_SW_DESC_COMBINED_OP_IFT_SIZE_WORD_OFFSET \
			SEP_SW_DESC_CRYPTO_OP_IFT_SIZE_WORD_OFFSET
#define SEP_SW_DESC_COMBINED_OP_IFT_SIZE_BIT_OFFSET \
			SEP_SW_DESC_CRYPTO_OP_IFT_SIZE_BIT_OFFSET
#define SEP_SW_DESC_COMBINED_OP_IFT_SIZE_BIT_SIZE \
			SEP_SW_DESC_CRYPTO_OP_IFT_SIZE_BIT_SIZE
#define SEP_SW_DESC_COMBINED_OP_IFT_NUM_WORD_OFFSET \
			SEP_SW_DESC_CRYPTO_OP_IFT_NUM_WORD_OFFSET
#define SEP_SW_DESC_COMBINED_OP_IFT_NUM_BIT_OFFSET \
			SEP_SW_DESC_CRYPTO_OP_IFT_NUM_BIT_OFFSET
#define SEP_SW_DESC_COMBINED_OP_IFT_NUM_BIT_SIZE \
			SEP_SW_DESC_CRYPTO_OP_IFT_NUM_BIT_SIZE

/* OFT: OFT_ADDR, OFT_SIZE, OFT_NUM */
#define SEP_SW_DESC_COMBINED_OP_OFT_ADDR_WORD_OFFSET \
			SEP_SW_DESC_CRYPTO_OP_OFT_ADDR_WORD_OFFSET
#define SEP_SW_DESC_COMBINED_OP_OFT_ADDR_BIT_OFFSET \
			SEP_SW_DESC_CRYPTO_OP_OFT_ADDR_BIT_OFFSET
#define SEP_SW_DESC_COMBINED_OP_OFT_ADDR_BIT_SIZE \
			SEP_SW_DESC_CRYPTO_OP_OFT_ADDR_BIT_SIZE
#define SEP_SW_DESC_COMBINED_OP_OFT_SIZE_WORD_OFFSET \
			SEP_SW_DESC_CRYPTO_OP_OFT_SIZE_WORD_OFFSET
#define SEP_SW_DESC_COMBINED_OP_OFT_SIZE_BIT_OFFSET \
			SEP_SW_DESC_CRYPTO_OP_OFT_SIZE_BIT_OFFSET
#define SEP_SW_DESC_COMBINED_OP_OFT_SIZE_BIT_SIZE \
			SEP_SW_DESC_CRYPTO_OP_OFT_SIZE_BIT_SIZE
#define SEP_SW_DESC_COMBINED_OP_OFT_NUM_WORD_OFFSET \
			SEP_SW_DESC_CRYPTO_OP_OFT_NUM_WORD_OFFSET
#define SEP_SW_DESC_COMBINED_OP_OFT_NUM_BIT_OFFSET \
			SEP_SW_DESC_CRYPTO_OP_OFT_NUM_BIT_OFFSET
#define SEP_SW_DESC_COMBINED_OP_OFT_NUM_BIT_SIZE \
			SEP_SW_DESC_CRYPTO_OP_OFT_NUM_BIT_SIZE

/* combined scheme macros:
   these set of macros meant for configuration scheme encoding
   from the user level interface to the SEP combined driver.
*/
#define SEP_ENGINE_TYPE_BIT_SHIFT 0
#define SEP_ENGINE_TYPE_BIT_SIZE 4
#define SEP_ENGINE_SRC_BIT_SHIFT 4
#define SEP_ENGINE_SRC_BIT_SIZE 4
#define SEP_ENGINE_SLOT_BIT_SIZE \
		(SEP_ENGINE_SRC_BIT_SIZE + SEP_ENGINE_TYPE_BIT_SIZE)

/******************************* MACROS ***********************************/
#define _sep_comb_eng_pack_item(eng_src, eng_type) \
		(((eng_src) << SEP_ENGINE_SRC_BIT_SIZE) | \
		 ((eng_type) << SEP_ENGINE_TYPE_BIT_SHIFT))

#define _sep_comb_eng_pack_n_shift(src, type, slot) \
		(_sep_comb_eng_pack_item(src, type) << \
		(slot * SEP_ENGINE_SLOT_BIT_SIZE))

#define sep_comb_eng_props_set(cfg_p, eng_idx, eng_src, eng_type) do { \
	BITFIELD_SET(*cfg_p, \
		(eng_idx * SEP_ENGINE_SLOT_BIT_SIZE), \
		SEP_ENGINE_SLOT_BIT_SIZE, 0); \
	BITFIELD_SET(*cfg_p, \
		(eng_idx * SEP_ENGINE_SLOT_BIT_SIZE), \
		SEP_ENGINE_SLOT_BIT_SIZE, \
		_sep_comb_eng_pack_item(eng_src, eng_type)); \
} while (0)

#define sep_comb_eng_props_get(cfg_p, eng_idx, eng_src, eng_type) do { \
	*(eng_type) = BITFIELD_GET(*cfg_p, \
				(eng_idx * SEP_ENGINE_SLOT_BIT_SIZE),\
				SEP_ENGINE_TYPE_BIT_SIZE); \
	*(eng_src) = BITFIELD_GET(*cfg_p, \
				(eng_idx * SEP_ENGINE_SLOT_BIT_SIZE) \
				+ SEP_ENGINE_TYPE_BIT_SIZE, \
				SEP_ENGINE_SRC_BIT_SIZE); \
} while (0)

/******************************************/
/* Message-Op descriptor type: RPC_MSG    */
/******************************************/

/* Agent ID: AGENT_ID */
#define SEP_SW_DESC_RPC_MSG_AGENT_ID_WORD_OFFSET 1
#define SEP_SW_DESC_RPC_MSG_AGENT_ID_BIT_OFFSET 0
#define SEP_SW_DESC_RPC_MSG_AGENT_ID_BIT_SIZE 8

/* Function ID: FUNC_ID */
#define SEP_SW_DESC_RPC_MSG_FUNC_ID_WORD_OFFSET 1
#define SEP_SW_DESC_RPC_MSG_FUNC_ID_BIT_OFFSET 16
#define SEP_SW_DESC_RPC_MSG_FUNC_ID_BIT_SIZE 16

/* HMB: HMB_ADDR , HMB_SIZE */
#define SEP_SW_DESC_RPC_MSG_HMB_ADDR_WORD_OFFSET 2
#define SEP_SW_DESC_RPC_MSG_HMB_ADDR_BIT_OFFSET 0
#define SEP_SW_DESC_RPC_MSG_HMB_ADDR_BIT_SIZE 32
#define SEP_SW_DESC_RPC_MSG_HMB_SIZE_WORD_OFFSET 3
#define SEP_SW_DESC_RPC_MSG_HMB_SIZE_BIT_OFFSET 0
#define SEP_SW_DESC_RPC_MSG_HMB_SIZE_BIT_SIZE 13

/************************************************/
/* SeP Applet Request descriptor type: APP_REQ  */
/************************************************/

/* Request Type: REQ_TYPE */
#define SEP_SW_DESC_APP_REQ_REQ_TYPE_WORD_OFFSET 0
#define SEP_SW_DESC_APP_REQ_REQ_TYPE_BIT_OFFSET 4
#define SEP_SW_DESC_APP_REQ_REQ_TYPE_BIT_SIZE 2

/* Session ID: SESSION_ID */
#define SEP_SW_DESC_APP_REQ_SESSION_ID_WORD_OFFSET 0
#define SEP_SW_DESC_APP_REQ_SESSION_ID_BIT_OFFSET 16
#define SEP_SW_DESC_APP_REQ_SESSION_ID_BIT_SIZE 12

/* Internal error: INTERNAL_ERR */
#define SEP_SW_DESC_APP_REQ_INTERNAL_ERR_WORD_OFFSET 0
#define SEP_SW_DESC_APP_REQ_INTERNAL_ERR_BIT_OFFSET 31
#define SEP_SW_DESC_APP_REQ_INTERNAL_ERR_BIT_SIZE 1

/* In-Params. Buffer Address: IN_PARAMS_ADDR */
#define SEP_SW_DESC_APP_REQ_IN_PARAMS_ADDR_WORD_OFFSET 1
#define SEP_SW_DESC_APP_REQ_IN_PARAMS_ADDR_BIT_OFFSET 0
#define SEP_SW_DESC_APP_REQ_IN_PARAMS_ADDR_BIT_SIZE 32

/* Return codes for APP_REQ descriptor */
enum sepapp_retcode {
	SEPAPP_RET_OK = 0,
	SEPAPP_RET_EINVAL_QUEUE,	/* Request sent on the wrong SW queue */
	SEPAPP_RET_EINVAL,	/* Invalid parameters in descriptor */
};

/* REQ_TYPE field encoding */
enum sepapp_req_type {
	SEPAPP_REQ_TYPE_SESSION_OPEN = 1,
	SEPAPP_REQ_TYPE_SESSION_CLOSE = 2,
	SEPAPP_REQ_TYPE_COMMAND_INVOKE = 3
};

/** in-params. data types **/

#define SEPAPP_UUID_SIZE 16
#define SEPAPP_MAX_PARAMS 4
#define SEPAPP_MAX_AUTH_DATA_SIZE 16	/* For Application UUID case */

enum sepapp_param_type {
	SEPAPP_PARAM_TYPE_NULL = 0,
	SEPAPP_PARAM_TYPE_VAL = 1,
	SEPAPP_PARAM_TYPE_MEMREF = 2
};

enum sepapp_data_direction {
	SEPAPP_DIR_NULL = 0,
	SEPAPP_DIR_IN = 1,
	SEPAPP_DIR_OUT = (1 << 1),
	SEPAPP_DIR_INOUT = SEPAPP_DIR_IN | SEPAPP_DIR_OUT
};

/* Descriptor for "by value" parameter */
struct sepapp_val_param {
	/*enum sepapp_data_direction */ u8 dir;
	u32 data[2];
};

/* Depends on seprpc data type defined in sep_rpc.h */
union sepapp_client_param {
	struct sepapp_val_param val;
	struct seprpc_memref memref;
};

struct sepapp_client_params {
	u8 params_types[SEPAPP_MAX_PARAMS];
	union sepapp_client_param params[SEPAPP_MAX_PARAMS];
};

/* In-params. for SESSION_OPEN request type */
struct sepapp_in_params_session_open {
	u8 app_uuid[SEPAPP_UUID_SIZE];
	u32 auth_method;
	u8 auth_data[SEPAPP_MAX_AUTH_DATA_SIZE];
	struct sepapp_client_params client_params;
};

struct sepapp_in_params_command_invoke {
	u32 command_id;
	struct sepapp_client_params client_params;
};

/* Return codes for SLEEP_REQ descriptor */
enum sepslp_mode_req_retcode {
	SEPSLP_MODE_REQ_RET_OK = 0,
	SEPSLP_MODE_REQ_EGEN,	/* general error */
	SEPSLP_MODE_REQ_EINVAL_QUEUE,	/* Request sent on the wrong SW queue */
	SEPSLP_MODE_REQ_EBUSY,/* Request sent while desc. queue is not empty */
	SEPSLP_MODE_REQ_EABORT	/* Applet requested aborting this request */
};

/****************************************/
/* Load-Op descriptor type: LOAD_OP */
/****************************************/

/* SeP/FW Cache Index: FW_CACHE_IDX */
#define SEP_SW_DESC_LOAD_OP_FW_CACHE_IDX_WORD_OFFSET 1
#define SEP_SW_DESC_LOAD_OP_FW_CACHE_IDX_BIT_OFFSET(slot) ((slot) * 8)
#define SEP_SW_DESC_LOAD_OP_FW_CACHE_IDX_BIT_SIZE 8

/* HCB address: HCB_ADDR */
#define SEP_SW_DESC_LOAD_OP_HCB_ADDR_WORD_OFFSET(slot) ((slot) + 2)
#define SEP_SW_DESC_LOAD_OP_HCB_ADDR_BIT_OFFSET 1
#define SEP_SW_DESC_LOAD_OP_HCB_ADDR_BIT_SIZE 31

/* L bit: Load cache: L */
#define SEP_SW_DESC_LOAD_OP_L_WORD_OFFSET(slot) ((slot) + 2)
#define SEP_SW_DESC_LOAD_OP_L_BIT_OFFSET 0
#define SEP_SW_DESC_LOAD_OP_L_BIT_SIZE 1

/*****************************/
/*** Descriptor copy flows ***/
/*****************************/
/* Copy host descriptor scratchpad to descriptor queue buffer */
#ifdef __BIG_ENDIAN

/* Verify descriptor copy flows assumptions at compile time:
   assumes "retcode" and "cookie" are the last words */
#if (SEP_SW_DESC_RET_CODE_WORD_OFFSET != 6)
#error SW_DESC_RET_CODE location assumption is broken!
#endif
#if (SEP_SW_DESC_COOKIE_WORD_OFFSET != 7)
#error SW_DESC_COOKIE location assumption is broken!
#endif

#define SEP_SW_DESC_COPY_TO_SEP(queue_desc_p, spad_desc_p) do {	               \
	u32 *cur_q_desc_word_p = (u32 *)queue_desc_p;                \
	u32 *cur_spad_desc_word_p = (u32 *)spad_desc_p;              \
	int i;	                                                               \
	/* First 6 words are input data to SeP-FW. Must be in SeP endianess:*/ \
	/* Copy 7th word too in order to init./clear retcode field	    */ \
	for (i = 0; i <= SEP_SW_DESC_RET_CODE_WORD_OFFSET; i++) {              \
		*cur_q_desc_word_p = cpu_to_le32(*cur_spad_desc_word_p);       \
		cur_spad_desc_word_p++;                                        \
		cur_q_desc_word_p++;                                           \
	}                                                                      \
	/* Word 8 is the cookie which is referenced only by the host */        \
	/* No need to swap endianess */                                        \
	*cur_q_desc_word_p = *cur_spad_desc_word_p;                            \
} while (0)

/* and vice-versa */
#define SEP_SW_DESC_COPY_FROM_SEP(spad_desc_p, queue_desc_p) do {              \
	u32 *cur_q_desc_word_p = (u32 *)queue_desc_p;                \
	u32 *cur_spad_desc_word_p = (u32 *)spad_desc_p;              \
	int i;	                                                               \
	/* First 6 words are input data to SeP-FW in SeP endianess:*/          \
	/* Copy 7th word too in order to get retcode field	   */          \
	for (i = 0; i <= SEP_SW_DESC_RET_CODE_WORD_OFFSET; i++) {              \
		*cur_spad_desc_word_p = le32_to_cpu(*cur_q_desc_word_p);       \
		cur_spad_desc_word_p++;                                        \
		cur_q_desc_word_p++;                                           \
	}                                                                      \
	/* Word 8 is the cookie which is referenced only by the host */        \
	/* No need to swap endianess */                                        \
	*cur_spad_desc_word_p = *cur_q_desc_word_p;                            \
} while (0)

#else				/* __LITTLE_ENDIAN - simple memcpy */
#define SEP_SW_DESC_COPY_TO_SEP(queue_desc_p, spad_desc_p)                     \
	memcpy(queue_desc_p, spad_desc_p, SEP_SW_DESC_WORD_SIZE<<2)

#define SEP_SW_DESC_COPY_FROM_SEP(spad_desc_p, queue_desc_p)                 \
	memcpy(spad_desc_p, queue_desc_p, SEP_SW_DESC_WORD_SIZE<<2)
#endif

#endif /*_SEP_SW_DESC_H_*/
