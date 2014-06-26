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

#ifndef __SEP_DRIVER_ABI_H__
#define __SEP_DRIVER_ABI_H__

#ifdef __KERNEL__
#include <linux/types.h>
#ifndef INT32_MAX
#define INT32_MAX 0x7FFFFFFFL
#endif
#else
/* For inclusion in user space library */
#include <stdint.h>
#endif

#include <linux/ioctl.h>
#include <linux/errno.h>
#include "sep_rpc.h"

/* Proprietary error code for unexpected internal errors */
#define EBUG 999

/****************************/
/**** IOCTL return codes ****/
/*****************************************************************************
 ENOTTY  : Unknown IOCTL command					     *
 ENOSYS  : Unsupported/not-implemented (known) operation		     *
 EINVAL  : Invalid parameters                                                *
 EFAULT  : Bad pointers for given user memory space                          *
 EPERM   : Not enough permissions for given command                          *
 ENOMEM,EAGAIN: when not enough resources available for given op.            *
 EIO     : SeP HW error or another internal error (probably operation timed  *
	   out or unexpected behavior)                                       *
 EBUG    : Driver bug found ("assertion") - see system log                   *
*****************************************************************************/

/****** IOCTL commands ********/
/* The magic number appears free in Documentation/ioctl/ioctl-number.txt */
#define DXDI_IOC_MAGIC 0xD1

/* IOCTL ordinal numbers */
/*(for backward compatibility, add new ones only at end of list!) */
enum dxdi_ioc_nr {
	/* Version info. commands */
	DXDI_IOC_NR_GET_VER_MAJOR = 0,
	DXDI_IOC_NR_GET_VER_MINOR = 1,
	/* Context size queries */
	DXDI_IOC_NR_GET_SYMCIPHER_CTX_SIZE = 2,
	DXDI_IOC_NR_GET_AUTH_ENC_CTX_SIZE = 3,
	DXDI_IOC_NR_GET_MAC_CTX_SIZE = 4,
	DXDI_IOC_NR_GET_HASH_CTX_SIZE = 5,
	/* Init context commands */
	DXDI_IOC_NR_SYMCIPHER_INIT = 7,
	DXDI_IOC_NR_AUTH_ENC_INIT = 8,
	DXDI_IOC_NR_MAC_INIT = 9,
	DXDI_IOC_NR_HASH_INIT = 10,
	/* Processing commands */
	DXDI_IOC_NR_PROC_DBLK = 12,
	DXDI_IOC_NR_FIN_PROC = 13,
	/* "Integrated" processing operations */
	DXDI_IOC_NR_SYMCIPHER_PROC = 14,
	DXDI_IOC_NR_AUTH_ENC_PROC = 15,
	DXDI_IOC_NR_MAC_PROC = 16,
	DXDI_IOC_NR_HASH_PROC = 17,
	/* SeP RPC */
	DXDI_IOC_NR_SEP_RPC = 19,
	/* Memory registration */
	DXDI_IOC_NR_REGISTER_MEM4DMA = 20,
	DXDI_IOC_NR_ALLOC_MEM4DMA = 21,
	DXDI_IOC_NR_FREE_MEM4DMA = 22,
	/* SeP Applets API */
	DXDI_IOC_NR_SEPAPP_SESSION_OPEN = 23,
	DXDI_IOC_NR_SEPAPP_SESSION_CLOSE = 24,
	DXDI_IOC_NR_SEPAPP_COMMAND_INVOKE = 25,
	/* Combined mode */
	DXDI_IOC_NR_COMBINED_INIT = 26,
	DXDI_IOC_NR_COMBINED_PROC_DBLK = 27,
	DXDI_IOC_NR_COMBINED_PROC_FIN = 28,
	DXDI_IOC_NR_COMBINED_PROC = 29,

	/* AES IV set/get API */
	DXDI_IOC_NR_SET_IV = 30,
	DXDI_IOC_NR_GET_IV = 31,
	DXDI_IOC_NR_MAX = DXDI_IOC_NR_GET_IV
};

/* In case error is not DXDI_RET_ESEP these are the
*  errors embedded in the error info field "error_info" */
enum dxdi_error_info {
	DXDI_ERROR_NULL = 0,
	DXDI_ERROR_BAD_CTX = 1,
	DXDI_ERROR_UNSUP = 2,
	DXDI_ERROR_INVAL_MODE = 3,
	DXDI_ERROR_INVAL_DIRECTION = 4,
	DXDI_ERROR_INVAL_KEY_SIZE = 5,
	DXDI_ERROR_INVAL_NONCE_SIZE = 6,
	DXDI_ERROR_INVAL_TAG_SIZE = 7,
	DXDI_ERROR_INVAL_DIN_PTR = 8,
	DXDI_ERROR_INVAL_DOUT_PTR = 9,
	DXDI_ERROR_INVAL_DATA_SIZE = 10,
	DXDI_ERROR_DIN_DOUT_OVERLAP = 11,
	DXDI_ERROR_INTERNAL = 12,
	DXDI_ERROR_NO_RESOURCE = 13,
	DXDI_ERROR_FATAL = 14,
	DXDI_ERROR_INFO_RESERVE32B = INT32_MAX
};

/* ABI Version info. */
#define DXDI_VER_MAJOR 1
#define DXDI_VER_MINOR DXDI_IOC_NR_MAX

/******************************/
/* IOCTL commands definitions */
/******************************/
/* Version info. commands */
#define DXDI_IOC_GET_VER_MAJOR _IOR(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_GET_VER_MAJOR, u32)
#define DXDI_IOC_GET_VER_MINOR _IOR(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_GET_VER_MINOR, u32)
/* Context size queries */
#define DXDI_IOC_GET_SYMCIPHER_CTX_SIZE _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_GET_SYMCIPHER_CTX_SIZE,\
		struct dxdi_get_sym_cipher_ctx_size_params)
#define DXDI_IOC_GET_AUTH_ENC_CTX_SIZE _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_GET_AUTH_ENC_CTX_SIZE,\
		struct dxdi_get_auth_enc_ctx_size_params)
#define DXDI_IOC_GET_MAC_CTX_SIZE _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_GET_MAC_CTX_SIZE,\
		struct dxdi_get_mac_ctx_size_params)
#define DXDI_IOC_GET_HASH_CTX_SIZE _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_GET_HASH_CTX_SIZE,\
		struct dxdi_get_hash_ctx_size_params)
/* Init. Sym. Crypto. */
#define DXDI_IOC_SYMCIPHER_INIT _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_SYMCIPHER_INIT, struct dxdi_sym_cipher_init_params)
#define DXDI_IOC_AUTH_ENC_INIT _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_AUTH_ENC_INIT, struct dxdi_auth_enc_init_params)
#define DXDI_IOC_MAC_INIT _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_MAC_INIT, struct dxdi_mac_init_params)
#define DXDI_IOC_HASH_INIT _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_HASH_INIT, struct dxdi_hash_init_params)

/* Sym. Crypto. Processing commands */
#define DXDI_IOC_PROC_DBLK _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_PROC_DBLK, struct dxdi_process_dblk_params)
#define DXDI_IOC_FIN_PROC _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_FIN_PROC, struct dxdi_fin_process_params)

/* Integrated Sym. Crypto. */
#define DXDI_IOC_SYMCIPHER_PROC _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_SYMCIPHER_PROC, struct dxdi_sym_cipher_proc_params)
#define DXDI_IOC_AUTH_ENC_PROC _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_AUTH_ENC_PROC, struct dxdi_auth_enc_proc_params)
#define DXDI_IOC_MAC_PROC _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_MAC_PROC, struct dxdi_mac_proc_params)
#define DXDI_IOC_HASH_PROC _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_HASH_PROC, struct dxdi_hash_proc_params)

/* AES Initial Vector set/get */
#define DXDI_IOC_SET_IV _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_SET_IV, struct dxdi_aes_iv_params)
#define DXDI_IOC_GET_IV _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_GET_IV, struct dxdi_aes_iv_params)

/* Combined mode  */
#define DXDI_IOC_COMBINED_INIT _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_COMBINED_INIT,\
		struct dxdi_combined_init_params)
#define DXDI_IOC_COMBINED_PROC_DBLK _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_COMBINED_PROC_DBLK,\
		struct dxdi_combined_proc_dblk_params)
#define DXDI_IOC_COMBINED_PROC_FIN _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_COMBINED_PROC_FIN,\
		struct dxdi_combined_proc_params)
#define DXDI_IOC_COMBINED_PROC _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_COMBINED_PROC,\
		struct dxdi_combined_proc_params)

/* SeP RPC */
#define DXDI_IOC_SEP_RPC _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_SEP_RPC, struct dxdi_sep_rpc_params)
/* Memory registration */
#define DXDI_IOC_REGISTER_MEM4DMA _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_REGISTER_MEM4DMA, \
		struct dxdi_register_mem4dma_params)
#define DXDI_IOC_ALLOC_MEM4DMA _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_ALLOC_MEM4DMA, \
		struct dxdi_alloc_mem4dma_params)
#define DXDI_IOC_FREE_MEM4DMA _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_FREE_MEM4DMA, \
		struct dxdi_free_mem4dma_params)
/* SeP Applets API */
#define DXDI_IOC_SEPAPP_SESSION_OPEN _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_SEPAPP_SESSION_OPEN, \
		struct dxdi_sepapp_session_open_params)
#define DXDI_IOC_SEPAPP_SESSION_CLOSE _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_SEPAPP_SESSION_CLOSE, \
		struct dxdi_sepapp_session_close_params)
#define DXDI_IOC_SEPAPP_COMMAND_INVOKE _IORW(DXDI_IOC_MAGIC,\
		DXDI_IOC_NR_SEPAPP_COMMAND_INVOKE, \
		struct dxdi_sepapp_command_invoke_params)

/*** ABI constants ***/
/* Max. symmetric crypto key size (512b) */
#define DXDI_SYM_KEY_SIZE_MAX 64	/*octets */
/* Max. MAC key size (applicable to HMAC-SHA512) */
#define DXDI_MAC_KEY_SIZE_MAX 128	/*octets */
/* AES IV/Counter size (128b) */
#define DXDI_AES_BLOCK_SIZE 16	/*octets */
/* DES IV size (64b) */
#define DXDI_DES_BLOCK_SIZE 8	/*octets */
/* Max. Nonce size */
#define DXDI_NONCE_SIZE_MAX 16	/*octets */
/* Max. digest size */
#define DXDI_DIGEST_SIZE_MAX 64	/*octets */
/* Max. nodes */
#define DXDI_COMBINED_NODES_MAX 4
#define DXDI_AES_IV_SIZE DXDI_AES_BLOCK_SIZE

/*** ABI data types ***/

enum dxdi_cipher_direction {
	DXDI_CDIR_ENC = 0,
	DXDI_CDIR_DEC = 1,
	DXDI_CDIR_MAX = DXDI_CDIR_DEC,
	DXDI_CDIR_RESERVE32B = INT32_MAX
};

enum dxdi_sym_cipher_type {
	DXDI_SYMCIPHER_NONE = 0,
	_DXDI_SYMCIPHER_AES_FIRST = 1,
	DXDI_SYMCIPHER_AES_XXX = _DXDI_SYMCIPHER_AES_FIRST,
	DXDI_SYMCIPHER_AES_ECB = _DXDI_SYMCIPHER_AES_FIRST + 1,
	DXDI_SYMCIPHER_AES_CBC = _DXDI_SYMCIPHER_AES_FIRST + 2,
	DXDI_SYMCIPHER_AES_CTR = _DXDI_SYMCIPHER_AES_FIRST + 3,
	DXDI_SYMCIPHER_AES_XTS = _DXDI_SYMCIPHER_AES_FIRST + 4,
	_DXDI_SYMCIPHER_AES_LAST = DXDI_SYMCIPHER_AES_XTS,
	_DXDI_SYMCIPHER_DES_FIRST = 0x11,
	DXDI_SYMCIPHER_DES_ECB = _DXDI_SYMCIPHER_DES_FIRST,
	DXDI_SYMCIPHER_DES_CBC = _DXDI_SYMCIPHER_DES_FIRST + 1,
	_DXDI_SYMCIPHER_DES_LAST = DXDI_SYMCIPHER_DES_CBC,
	_DXDI_SYMCIPHER_C2_FIRST = 0x21,
	DXDI_SYMCIPHER_C2_ECB = _DXDI_SYMCIPHER_C2_FIRST,
	DXDI_SYMCIPHER_C2_CBC = _DXDI_SYMCIPHER_C2_FIRST + 1,
	_DXDI_SYMCIPHER_C2_LAST = DXDI_SYMCIPHER_C2_CBC,
	DXDI_SYMCIPHER_RC4 = 0x31,	/* Supported in message API only */
	DXDI_SYMCIPHER_RESERVE32B = INT32_MAX
};

enum dxdi_auth_enc_type {
	DXDI_AUTHENC_NONE = 0,
	DXDI_AUTHENC_AES_CCM = 1,
	DXDI_AUTHENC_AES_GCM = 2,
	DXDI_AUTHENC_MAX = DXDI_AUTHENC_AES_GCM,
	DXDI_AUTHENC_RESERVE32B = INT32_MAX
};

enum dxdi_mac_type {
	DXDI_MAC_NONE = 0,
	DXDI_MAC_HMAC = 1,
	DXDI_MAC_AES_MAC = 2,
	DXDI_MAC_AES_CMAC = 3,
	DXDI_MAC_AES_XCBC_MAC = 4,
	DXDI_MAC_MAX = DXDI_MAC_AES_XCBC_MAC,
	DXDI_MAC_RESERVE32B = INT32_MAX
};

enum dxdi_hash_type {
	DXDI_HASH_NONE = 0,
	DXDI_HASH_MD5 = 1,
	DXDI_HASH_SHA1 = 2,
	DXDI_HASH_SHA224 = 3,
	DXDI_HASH_SHA256 = 4,
	DXDI_HASH_SHA384 = 5,
	DXDI_HASH_SHA512 = 6,
	DXDI_HASH_MAX = DXDI_HASH_SHA512,
	DXDI_HASH_RESERVE32B = INT32_MAX
};

enum dxdi_data_block_type {
	DXDI_DATA_TYPE_NULL = 0,
	DXDI_DATA_TYPE_TEXT = 1,/* Plain/cipher text */
	DXDI_DATA_TYPE_ADATA = 2,/* Additional/Associated data for AEAD */
	DXDI_DATA_TYPE_MAX = DXDI_DATA_TYPE_ADATA,
	DXDI_DATA_TYPE_RESERVE32B = INT32_MAX
};

enum dxdi_input_engine_type {
	DXDI_INPUT_NULL = 0,	/* no input */
	DXDI_INPUT_ENGINE_1 = 1,
	DXID_INPUT_ENGINE_2 = 2,
	DXDI_INPUT_DIN = 15,	/* input from DIN */
	DXDI_INPUT_ENGINE_RESERVE32B = INT32_MAX,
};

#pragma pack(push)
#pragma pack(4) /* Force to 32 bit alignment */
/* Properties of specific ciphers */
/* (for use in alg_specific union of dxdi_cipher_props) */
struct dxdi_des_cbc_props {
	u8 iv[DXDI_DES_BLOCK_SIZE];
};
struct dxdi_aes_cbc_props {
	u8 iv[DXDI_AES_BLOCK_SIZE];
};
struct dxdi_aes_ctr_props {
	u8 cntr[DXDI_AES_BLOCK_SIZE];
};
struct dxdi_aes_xts_props {
	u8 init_tweak[DXDI_AES_BLOCK_SIZE];
	u32 data_unit_size;
};
struct dxdi_c2_cbc_props {
	u32 reset_interval;
};

struct dxdi_sym_cipher_props {
	enum dxdi_sym_cipher_type cipher_type;
	enum dxdi_cipher_direction direction;
	u8 key_size;	/* In octets */
	u8 key[DXDI_SYM_KEY_SIZE_MAX];
	union {			/* cipher specific properties */
		struct dxdi_des_cbc_props des_cbc;
		struct dxdi_aes_cbc_props aes_cbc;
		struct dxdi_aes_ctr_props aes_ctr;
		struct dxdi_aes_xts_props aes_xts;
		struct dxdi_c2_cbc_props c2_cbc;
		u32 __assure_32b_union_alignment;
		/* Reserve space for future extension? */
	} alg_specific;
};

struct dxdi_auth_enc_props {
	enum dxdi_auth_enc_type ae_type;
	enum dxdi_cipher_direction direction;
	u32 adata_size;	/* In octets */
	u32 text_size;	/* In octets */
	u8 key_size;	/* In octets */
	u8 nonce_size;	/* In octets */
	u8 tag_size;	/* In octets */
	u8 key[DXDI_SYM_KEY_SIZE_MAX];
	u8 nonce[DXDI_NONCE_SIZE_MAX];
};

/* Properties specific for HMAC */
/* (for use in properties union of dxdi_mac_props) */
struct dxdi_hmac_props {
	enum dxdi_hash_type hash_type;
};

struct dxdi_aes_mac_props {
	u8 iv[DXDI_AES_BLOCK_SIZE];
};

struct dxdi_mac_props {
	enum dxdi_mac_type mac_type;
	u32 key_size;	/* In octets */
	u8 key[DXDI_MAC_KEY_SIZE_MAX];
	union {			/* Union of algorithm specific properties */
		struct dxdi_hmac_props hmac;
		struct dxdi_aes_mac_props aes_mac;
		u32 __assure_32b_union_alignment;
		/* Reserve space for future extension? */
	} alg_specific;
};

/* Combined mode props */
struct dxdi_combined_node_props {
	u32 *context;
	enum dxdi_input_engine_type eng_input;
};

struct dxdi_combined_props {
	struct dxdi_combined_node_props node_props[DXDI_COMBINED_NODES_MAX];
};

/*** IOCTL commands parameters structures ***/

struct dxdi_get_sym_cipher_ctx_size_params {
	enum dxdi_sym_cipher_type sym_cipher_type;	/*[in] */
	u32 ctx_size;	/*[out] */
};

struct dxdi_get_auth_enc_ctx_size_params {
	enum dxdi_auth_enc_type ae_type;	/*[in] */
	u32 ctx_size;	/*[out] */
};

struct dxdi_get_mac_ctx_size_params {
	enum dxdi_mac_type mac_type;	/*[in] */
	u32 ctx_size;	/*[out] */
};

struct dxdi_get_hash_ctx_size_params {
	enum dxdi_hash_type hash_type;	/*[in] */
	u32 ctx_size;	/*[out] */
};

/* Init params */
struct dxdi_sym_cipher_init_params {
	u32 *context_buf;	/*[in] */
	struct dxdi_sym_cipher_props props;	/*[in] */
	u32 error_info;	/*[out] */
};

struct dxdi_auth_enc_init_params {
	u32 *context_buf;	/*[in] */
	struct dxdi_auth_enc_props props;	/*[in] */
	u32 error_info;	/*[out] */
};

struct dxdi_mac_init_params {
	u32 *context_buf;	/*[in] */
	struct dxdi_mac_props props;	/*[in] */
	u32 error_info;	/*[out] */
};

struct dxdi_hash_init_params {
	u32 *context_buf;	/*[in] */
	enum dxdi_hash_type hash_type;	/*[in] */
	u32 error_info;	/*[out] */
};

/* Processing params */
struct dxdi_process_dblk_params {
	u32 *context_buf;	/*[in] */
	u8 *data_in;	/*[in] */
	u8 *data_out;	/*[in] */
	enum dxdi_data_block_type data_block_type;	/*[in] */
	u32 data_in_size;	/*[in] */
	u32 error_info;	/*[out] */
};

struct dxdi_fin_process_params {
	u32 *context_buf;	/*[in] */
	u8 *data_in;	/*[in] */
	u8 *data_out;	/*[in] */
	u32 data_in_size;	/*[in] (octets) */
	u8 digest_or_mac[DXDI_DIGEST_SIZE_MAX];	/*[out] */
	u8 digest_or_mac_size;	/*[out] (octets) */
	u32 error_info;	/*[out] */
};

struct dxdi_sym_cipher_proc_params {
	u32 *context_buf;	/*[in] */
	struct dxdi_sym_cipher_props props;	/*[in] */
	u8 *data_in;	/*[in] */
	u8 *data_out;	/*[in] */
	u32 data_in_size;	/*[in] (octets) */
	u32 error_info;	/*[out] */
};

struct dxdi_auth_enc_proc_params {
	u32 *context_buf;	/*[in] */
	struct dxdi_auth_enc_props props;	/*[in] */
	u8 *adata;		/*[in] */
	u8 *text_data;	/*[in] */
	u8 *data_out;	/*[in] */
	u8 tag[DXDI_DIGEST_SIZE_MAX];	/*[out] */
	u32 error_info;	/*[out] */
};

struct dxdi_mac_proc_params {
	u32 *context_buf;	/*[in] */
	struct dxdi_mac_props props;	/*[in] */
	u8 *data_in;	/*[in] */
	u32 data_in_size;	/*[in] (octets) */
	u8 mac[DXDI_DIGEST_SIZE_MAX];	/*[out] */
	u8 mac_size;	/*[out] (octets) */
	u32 error_info;	/*[out] */
};

struct dxdi_hash_proc_params {
	u32 *context_buf;	/*[in] */
	enum dxdi_hash_type hash_type;	/*[in] */
	u8 *data_in;	/*[in] */
	u32 data_in_size;	/*[in] (octets) */
	u8 digest[DXDI_DIGEST_SIZE_MAX];	/*[out] */
	u8 digest_size;	/*[out] (octets) */
	u32 error_info;	/*[out] */
};

struct dxdi_aes_iv_params {
	u32 *context_buf;	/*[in] */
	u8 iv_ptr[DXDI_AES_IV_SIZE];	/*[in]/[out] */
};

/* Combined params */
struct dxdi_combined_init_params {
	struct dxdi_combined_props props;	/*[in] */
	u32 error_info;	/*[out] */
};

struct dxdi_combined_proc_dblk_params {
	struct dxdi_combined_props props;	/*[in] */
	u8 *data_in;	/*[in] */
	u8 *data_out;	/*[out] */
	u32 data_in_size;	/*[in] (octets) */
	u32 error_info;	/*[out] */
};

/* the structure used in finalize and integrated processing */
struct dxdi_combined_proc_params {
	struct dxdi_combined_props props;	/*[in] */
	u8 *data_in;	/*[in] */
	u8 *data_out;	/*[out] */
	u32 data_in_size;	/*[in] (octets) */
	u8 auth_data[DXDI_DIGEST_SIZE_MAX];	/*[out] */
	u8 auth_data_size;	/*[out] (octets) */
	u32 error_info;	/*[out] */
};

/**************************************/
/* Memory references and registration */
/**************************************/

enum dxdi_data_direction {
	DXDI_DATA_NULL = 0,
	DXDI_DATA_TO_DEVICE = 1,
	DXDI_DATA_FROM_DEVICE = (1 << 1),
	DXDI_DATA_BIDIR = (DXDI_DATA_TO_DEVICE | DXDI_DATA_FROM_DEVICE)
};

/* Reference to pre-registered memory */
#define DXDI_MEMREF_ID_NULL -1

struct dxdi_memref {
	enum dxdi_data_direction dma_direction;
	/* Memory reference ID - DXDI_MEMREF_ID_NULL if not registered */
	int ref_id;
	/* Start address of a non-registered memory or offset within a
	 * registered memory */
	u32 start_or_offset;
	/* Size in bytes of non-registered buffer or size of chunk within a
	 * registered buffer */
	u32 size;
};

struct dxdi_register_mem4dma_params {
	struct dxdi_memref memref;	/*[in] */
	int memref_id;	/*[out] */
};

struct dxdi_alloc_mem4dma_params {
	u32 size;	/*[in] */
	int memref_id;	/*[out] */
};

struct dxdi_free_mem4dma_params {
	int memref_id;	/*[in] */
};

/***********/
/* SeP-RPC */
/***********/
struct dxdi_sep_rpc_params {
	u16 agent_id;	/*[in] */
	u16 func_id;	/*[in] */
	struct dxdi_memref mem_refs[SEP_RPC_MAX_MEMREF_PER_FUNC]; /*[in] */
	u32 rpc_params_size;	/*[in] */
	struct seprpc_params *rpc_params;	/*[in] */
	/* rpc_params to be copied into kernel DMA buffer */
	enum seprpc_retcode error_info;	/*[out] */
};

/***************/
/* SeP Applets */
/***************/

enum dxdi_sepapp_param_type {
	DXDI_SEPAPP_PARAM_NULL = 0,
	DXDI_SEPAPP_PARAM_VAL = 1,
	DXDI_SEPAPP_PARAM_MEMREF = 2,
	DXDI_SEPAPP_PARAM_RESERVE32B = 0x7FFFFFFF
};

struct dxdi_val_param {
	enum dxdi_data_direction copy_dir;	/* Copy direction */
	u32 data[2];
};

#define SEP_APP_PARAMS_MAX 4

struct dxdi_sepapp_params {
	enum dxdi_sepapp_param_type params_types[SEP_APP_PARAMS_MAX];
	union {
		struct dxdi_val_param val;
		struct dxdi_memref memref;
	} params[SEP_APP_PARAMS_MAX];
};

/* SeP modules ID for returnOrigin */
enum dxdi_sep_module {
	DXDI_SEP_MODULE_NULL = 0,
	DXDI_SEP_MODULE_HOST_DRIVER = 1,
	DXDI_SEP_MODULE_SW_QUEUE = 2,	/* SW-queue task: Inc. desc. parsers */
	DXDI_SEP_MODULE_APP_MGR = 3,	/* Applet Manager */
	DXDI_SEP_MODULE_APP = 4,	/* Applet */
	DXDI_SEP_MODULE_RPC_AGENT = 5,	/* Down to RPC parsers */
	DXDI_SEP_MODULE_SYM_CRYPTO = 6,	/* Symmetric crypto driver */
	DXDI_SEP_MODULE_RESERVE32B = 0x7FFFFFFF
};

#define DXDI_SEPAPP_UUID_SIZE 16

#define DXDI_SEPAPP_SESSION_INVALID (-1)

struct dxdi_sepapp_session_open_params {
	u8 app_uuid[DXDI_SEPAPP_UUID_SIZE];	/*[in] */
	u32 auth_method;	/*[in] */
	u32 auth_data[3];	/*[in] */
	struct dxdi_sepapp_params app_auth_data;	/*[in/out] */
	int session_id;	/*[out] */
	enum dxdi_sep_module sep_ret_origin;	/*[out] */
	u32 error_info;	/*[out] */
};

struct dxdi_sepapp_session_close_params {
	int session_id;	/*[in] */
};

struct dxdi_sepapp_command_invoke_params {
	int session_id;	/*[in] */
	u32 command_id;	/*[in] */
	struct dxdi_sepapp_params command_params;	/*[in/out] */
	enum dxdi_sep_module sep_ret_origin;	/*[out] */
	u32 error_info;	/*[out] */
};

#pragma pack(pop)

#endif /*__SEP_DRIVER_ABI_H__*/
