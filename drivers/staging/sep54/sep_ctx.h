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

#ifndef _SEP_CTX_H_
#define _SEP_CTX_H_
#ifdef __KERNEL__
#include <linux/types.h>
#define INT32_MAX 0x7FFFFFFFL
#else
#include <stdint.h>
#endif

#include "dx_cc_defs.h"

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

/* SeP context size */
#define SEP_CTX_SIZE_LOG2 7
#define SEP_CTX_SIZE (1<<SEP_CTX_SIZE_LOG2)
#define SEP_CTX_SIZE_WORDS (SEP_CTX_SIZE >> 2)

#define SEP_DES_IV_SIZE 8
#define SEP_DES_BLOCK_SIZE 8

#define SEP_DES_ONE_KEY_SIZE 8
#define SEP_DES_DOUBLE_KEY_SIZE 16
#define SEP_DES_TRIPLE_KEY_SIZE 24
#define SEP_DES_KEY_SIZE_MAX SEP_DES_TRIPLE_KEY_SIZE

#define SEP_AES_IV_SIZE 16
#define SEP_AES_IV_SIZE_WORDS (SEP_AES_IV_SIZE >> 2)

#define SEP_AES_BLOCK_SIZE 16
#define SEP_AES_BLOCK_SIZE_WORDS 4

#define SEP_AES_128_BIT_KEY_SIZE 16
#define SEP_AES_128_BIT_KEY_SIZE_WORDS	(SEP_AES_128_BIT_KEY_SIZE >> 2)
#define SEP_AES_192_BIT_KEY_SIZE 24
#define SEP_AES_192_BIT_KEY_SIZE_WORDS	(SEP_AES_192_BIT_KEY_SIZE >> 2)
#define SEP_AES_256_BIT_KEY_SIZE 32
#define SEP_AES_256_BIT_KEY_SIZE_WORDS	(SEP_AES_256_BIT_KEY_SIZE >> 2)
#define SEP_AES_KEY_SIZE_MAX			SEP_AES_256_BIT_KEY_SIZE
#define SEP_AES_KEY_SIZE_WORDS_MAX		(SEP_AES_KEY_SIZE_MAX >> 2)

#define SEP_SHA1_DIGEST_SIZE 20
#define SEP_SHA224_DIGEST_SIZE 28
#define SEP_SHA256_DIGEST_SIZE 32
#define SEP_SHA384_DIGEST_SIZE 48
#define SEP_SHA512_DIGEST_SIZE 64
#define SEP_SHA1024_DIGEST_SIZE 128

#define SEP_SHA1_BLOCK_SIZE 64
#define SEP_SHA224_BLOCK_SIZE 64
#define SEP_SHA256_BLOCK_SIZE 64
#define SEP_SHA1_224_256_BLOCK_SIZE 64
#define SEP_SHA384_BLOCK_SIZE 128
#define SEP_SHA512_BLOCK_SIZE 128
#define SEP_SHA1024_BLOCK_SIZE 128

#if (SEP_SUPPORT_SHA > 256)
#define SEP_DIGEST_SIZE_MAX SEP_SHA512_DIGEST_SIZE
#define SEP_HASH_BLOCK_SIZE_MAX SEP_SHA512_BLOCK_SIZE	/*1024b */
#else				/* Only up to SHA256 */
#define SEP_DIGEST_SIZE_MAX SEP_SHA256_DIGEST_SIZE
#define SEP_HASH_BLOCK_SIZE_MAX SEP_SHA256_BLOCK_SIZE	/*256 */
#endif

#define SEP_HMAC_BLOCK_SIZE_MAX SEP_HASH_BLOCK_SIZE_MAX

#define SEP_RC4_KEY_SIZE_MIN 1
#define SEP_RC4_KEY_SIZE_MAX 20
#define SEP_RC4_STATE_SIZE 264

#define SEP_C2_KEY_SIZE_MAX 16
#define SEP_C2_BLOCK_SIZE 8

#define SEP_ALG_MAX_BLOCK_SIZE SEP_HASH_BLOCK_SIZE_MAX

#define SEP_MAX_COMBINED_ENGINES 4

#define SEP_MAX_CTX_SIZE (max(sizeof(struct sep_ctx_rc4), \
				sizeof(struct sep_ctx_cache_entry)))
enum sep_engine_type {
	SEP_ENGINE_NULL = 0,
	SEP_ENGINE_AES = 1,
	SEP_ENGINE_DES = 2,
	SEP_ENGINE_HASH = 3,
	SEP_ENGINE_RC4 = 4,
	SEP_ENGINE_DOUT = 5,
	SEP_ENGINE_RESERVE32B = INT32_MAX,
};

enum sep_crypto_alg {
	SEP_CRYPTO_ALG_NULL = -1,
	SEP_CRYPTO_ALG_AES = 0,
	SEP_CRYPTO_ALG_DES = 1,
	SEP_CRYPTO_ALG_HASH = 2,
	SEP_CRYPTO_ALG_RC4 = 3,
	SEP_CRYPTO_ALG_C2 = 4,
	SEP_CRYPTO_ALG_HMAC = 5,
	SEP_CRYPTO_ALG_AEAD = 6,
	SEP_CRYPTO_ALG_BYPASS = 7,
	SEP_CRYPTO_ALG_COMBINED = 8,
	SEP_CRYPTO_ALG_NUM = 9,
	SEP_CRYPTO_ALG_RESERVE32B = INT32_MAX
};

enum sep_crypto_direction {
	SEP_CRYPTO_DIRECTION_NULL = -1,
	SEP_CRYPTO_DIRECTION_ENCRYPT = 0,
	SEP_CRYPTO_DIRECTION_DECRYPT = 1,
	SEP_CRYPTO_DIRECTION_DECRYPT_ENCRYPT = 3,
	SEP_CRYPTO_DIRECTION_RESERVE32B = INT32_MAX
};

enum sep_cipher_mode {
	SEP_CIPHER_NULL_MODE = -1,
	SEP_CIPHER_ECB = 0,
	SEP_CIPHER_CBC = 1,
	SEP_CIPHER_CTR = 2,
	SEP_CIPHER_CBC_MAC = 3,
	SEP_CIPHER_XTS = 4,
	SEP_CIPHER_XCBC_MAC = 5,
	SEP_CIPHER_CMAC = 7,
	SEP_CIPHER_CCM = 8,
	SEP_CIPHER_RESERVE32B = INT32_MAX
};

enum sep_hash_mode {
	SEP_HASH_NULL = -1,
	SEP_HASH_SHA1 = 0,
	SEP_HASH_SHA256 = 1,
	SEP_HASH_SHA224 = 2,
	SEP_HASH_MODE_NUM = 3,

	/* Unsupported */
	SEP_HASH_SHA512 = 3,
	SEP_HASH_SHA384 = 4,
	SEP_HASH_RESERVE32B = INT32_MAX
};

enum sep_hash_hw_mode {
	SEP_HASH_HW_SHA1 = 1,
	SEP_HASH_HW_SHA256 = 2,
	SEP_HASH_HW_SHA224 = 10,
	SEP_HASH_HW_SHA512 = 4,
	SEP_HASH_HW_SHA384 = 12,
	SEP_HASH_HW_RESERVE32B = INT32_MAX
};

enum sep_c2_mode {
	SEP_C2_NULL = -1,
	SEP_C2_ECB = 0,
	SEP_C2_CBC = 1,
	SEP_C2_RESERVE32B = INT32_MAX
};

/*******************************************************************/
/***************** DESCRIPTOR BASED CONTEXTS ***********************/
/*******************************************************************/

 /* Generic context ("super-class") */
struct sep_ctx_generic {
	enum sep_crypto_alg alg;
} __attribute__ ((__may_alias__));

/* Cache context entry ("sub-class") */
struct sep_ctx_cache_entry {
	enum sep_crypto_alg alg;
	u32 reserved[SEP_CTX_SIZE_WORDS - 1];
};

struct sep_ctx_c2 {
	enum sep_crypto_alg alg;	/* SEP_CRYPTO_ALG_C2 */
	enum sep_c2_mode mode;
	enum sep_crypto_direction direction;
	/* reserve to end of allocated context size */
	u32 key_size;	/* numeric value in bytes */
	u8 key[SEP_C2_KEY_SIZE_MAX];
	u8 reserved[SEP_CTX_SIZE - 4 * sizeof(u32) -
			 SEP_C2_KEY_SIZE_MAX];
};

struct sep_ctx_hash {
	enum sep_crypto_alg alg;	/* SEP_CRYPTO_ALG_HASH */
	enum sep_hash_mode mode;
	u8 digest[SEP_DIGEST_SIZE_MAX];
	/* reserve to end of allocated context size */
	u8 reserved[SEP_CTX_SIZE - 2 * sizeof(u32) -
			 SEP_DIGEST_SIZE_MAX];
};

/* !!!! sep_ctx_hmac should have the same structure as sep_ctx_hash except
   k0, k0_size fields */
struct sep_ctx_hmac {
	enum sep_crypto_alg alg;	/* SEP_CRYPTO_ALG_HMAC */
	enum sep_hash_mode mode;
	u8 digest[SEP_DIGEST_SIZE_MAX];
	u8 k0[SEP_HMAC_BLOCK_SIZE_MAX];
	u32 k0_size;
	/* reserve to end of allocated context size */
	u8 reserved[SEP_CTX_SIZE - 3 * sizeof(u32) -
			 SEP_DIGEST_SIZE_MAX - SEP_HMAC_BLOCK_SIZE_MAX];
};

struct sep_ctx_cipher {
	enum sep_crypto_alg alg;	/* SEP_CRYPTO_ALG_AES */
	enum sep_cipher_mode mode;
	enum sep_crypto_direction direction;
	enum dx_crypto_key_type crypto_key_type;
	u32 key_size;	/* numeric value in bytes   */
	u32 data_unit_size;	/* required for XTS */
	/* block_state is the AES engine block state.
	 *  It is used by the host to pass IV or counter at initialization.
	 *  It is used by SeP for intermediate block chaining state and for
	 *  returning MAC algorithms results.           */
	u8 block_state[SEP_AES_BLOCK_SIZE];
	u8 key[SEP_AES_KEY_SIZE_MAX];
	u8 xex_key[SEP_AES_KEY_SIZE_MAX];
	/* reserve to end of allocated context size */
	u32 reserved[SEP_CTX_SIZE_WORDS - 6 -
			  SEP_AES_BLOCK_SIZE / sizeof(u32) - 2 *
			  (SEP_AES_KEY_SIZE_MAX / sizeof(u32))];
};

/* authentication and encryption with associated data class */
struct sep_ctx_aead {
	enum sep_crypto_alg alg;	/* SEP_CRYPTO_ALG_AES */
	enum sep_cipher_mode mode;
	enum sep_crypto_direction direction;
	u32 key_size;	/* numeric value in bytes   */
	u32 nonce_size;	/* nonce size (octets) */
	u32 header_size;	/* finit additional data size (octets) */
	u32 text_size;	/* finit text data size (octets) */
	/* mac size, element of {4, 6, 8, 10, 12, 14, 16} */
	u32 tag_size;
	/* block_state1/2 is the AES engine block state */
	u8 block_state[SEP_AES_BLOCK_SIZE];
	u8 mac_state[SEP_AES_BLOCK_SIZE];	/* MAC result */
	u8 nonce[SEP_AES_BLOCK_SIZE];	/* nonce buffer */
	u8 key[SEP_AES_KEY_SIZE_MAX];
	/* reserve to end of allocated context size */
	u32 reserved[SEP_CTX_SIZE_WORDS - 8 -
			  3 * (SEP_AES_BLOCK_SIZE / sizeof(u32)) -
			  SEP_AES_KEY_SIZE_MAX / sizeof(u32)];
};

/* crys combined context */
struct sep_ctx_combined {
	enum sep_crypto_alg alg;
	u32 mode;
	/* array of sub contexts used for the combined operation      *
	 *  according to the given mode                               */
	struct sep_ctx_cache_entry *sub_ctx[SEP_MAX_COMBINED_ENGINES];
	/* store the host contexts addresses (optimization) */
	u32 host_addr[SEP_MAX_COMBINED_ENGINES];
};

/*******************************************************************/
/***************** MESSAGE BASED CONTEXTS **************************/
/*******************************************************************/

struct sep_ctx_rc4 {
	enum sep_crypto_alg alg;	/* SEP_CRYPTO_ALG_RC4 */
	u32 key_size;	/* numeric value in bytes */
	u8 key[SEP_RC4_KEY_SIZE_MAX];
	u8 state[SEP_RC4_STATE_SIZE];
};

#endif				/* _SEP_CTX_H_ */
