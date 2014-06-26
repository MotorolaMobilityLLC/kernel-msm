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

#ifndef _CRYPTO_CTX_MGR_H_
#define _CRYPTO_CTX_MGR_H_

#include "sep_ctx.h"
#include "dx_driver_abi.h"
#include "lli_mgr.h"

/* The largest hash block size is for SHA512 - 1024 bits */
#define HASH_BLK_SIZE_MAX (1024>>3)	/*octets */

/* Unique ID for a user context */
/* This value is made unique by concatenating session ptr (in kernel)
   with global counter incremented on each INIT phase                */
#define CTX_ID_INVALID ((u64)0)

enum host_ctx_state {
	CTX_STATE_UNINITIALIZED = 0,
	/* When a context is unitialized is can be any "garbage" since
	   the context buffer is given from the user... */
	CTX_STATE_INITIALIZED = 0x10000001,
	/* INITIALIZED = Initialized */
	CTX_STATE_PARTIAL_INIT = 0x10101011,
	/* PARTIAL_INIT = Init. was done only on host. INIT on SeP was postponed
	   - Requires INIT on next SeP operations. */
};

struct crypto_ctx_uid {
	u64 addr;
	u32 cntr;
};

/* Algorithm family/class enumeration */
enum crypto_alg_class {
	ALG_CLASS_NONE = 0,
	ALG_CLASS_SYM_CIPHER,
	ALG_CLASS_AUTH_ENC,
	ALG_CLASS_MAC,
	ALG_CLASS_HASH,
	ALG_CLASS_COMBINED,
	ALG_CLASS_MAX = ALG_CLASS_HASH
};

/* The common fields at start of a user context strcuture */
#define HOST_CTX_COMMON						\
		struct crypto_ctx_uid uid;					\
		/* To hold CTX_VALID_SIG when initialized */	\
		enum host_ctx_state state;			\
		/*determine the context specification*/		\
		enum crypto_alg_class alg_class;		\
		/* Cast the whole struct to matching user_ctx_* struct */ \
	/* When is_encrypted==true the props are not initialized and */\
	/* the contained sep_ctx is encrypted (when created in SeP)  */\
	/* Algorithm properties are encrypted */		\
		bool is_encrypted; \
		u32 sess_id; \
		struct sep_client_ctx *sctx

/* SeP context segment of a context */
#ifdef CONFIG_NOT_COHERENT_CACHE
/* Allocate cache line bytes before/after sep context to assure
   its cache line does not enter the cache during its mapping to SeP */
#define SEP_CTX_SEGMENT                                                        \
	u8 reserved_before[L1_CACHE_BYTES];			       \
	struct sep_ctx_cache_entry sep_ctx;                                    \
	u8 reserved_after[L1_CACHE_BYTES]
#else
/* Cache is coherent - no need for "protection" margins */
#define SEP_CTX_SEGMENT \
	struct sep_ctx_cache_entry sep_ctx
#endif

/* Generic host context*/
struct host_crypto_ctx {
	HOST_CTX_COMMON;
};

/* user_ctx specification for symettric ciphers */
struct host_crypto_ctx_sym_cipher {
	HOST_CTX_COMMON;
	struct dxdi_sym_cipher_props props;
	 SEP_CTX_SEGMENT;
};

/* user_ctx specification for authenticated encryption */
struct host_crypto_ctx_auth_enc {
	HOST_CTX_COMMON;
	bool is_adata_processed;/* flag indicates if adata was processed */
	struct dxdi_auth_enc_props props;
	 SEP_CTX_SEGMENT;
};

/* Host data for hash block remainders */
struct hash_block_remainder {
	u16 size;		/* Octets available in data_blk_tail */
	u8 data[HASH_BLK_SIZE_MAX] __aligned(8);
	/* data_blk_tail holds remainder of user data because the HW requires
	   integral hash blocks but for last data block.
	   We take it aligned to 8 in order to optimize HW access to it via
	   the 64bit AXI bus */
};

/* user_ctx specification for MAC algorithms */
struct host_crypto_ctx_mac {
	HOST_CTX_COMMON;
	u64 client_data_size;	/* Sum. up the data processed so far */
	struct dxdi_mac_props props;
	struct hash_block_remainder hmac_tail;
	 SEP_CTX_SEGMENT;
};

/* user_ctx specification for Hash algorithms */
struct host_crypto_ctx_hash {
	HOST_CTX_COMMON;
	enum dxdi_hash_type hash_type;
	struct hash_block_remainder hash_tail;
	 SEP_CTX_SEGMENT;
};

/**
 * struct client_crypto_ctx_info - Meta data on the client application crypto
 *					context buffer and its mapping
 * @dev:	Device context of the context (DMA) mapping.
 * @user_ptr:	address of current context in user space (if no kernel op.)
 * @ctx_page:	Mapped user page where user_ptr is located
 * @ctx_kptr:	Mapping context to kernel VA
 * @sep_ctx_kptr:	Kernel VA of SeP context portion of the host context
 *			(for async. operations, this may be outside of host
 *			 context)
 * @sep_ctx_dma_addr:	DMA address of SeP context
 * @hash_tail_dma_addr:	DMA of host_ctx_hash:data_blk_tail
 * @sep_cache_idx:	if >=0, saves the allocated sep cache entry index
 */
struct client_crypto_ctx_info {
	struct device *dev;
	u32 __user *user_ptr;	/* */
	struct page *ctx_page;
	struct host_crypto_ctx *ctx_kptr;
	struct sep_ctx_cache_entry *sep_ctx_kptr;
	dma_addr_t sep_ctx_dma_addr;
	dma_addr_t hash_tail_dma_addr;
	int sep_cache_idx;

	int sess_id;
	struct sep_client_ctx *sctx;
};
/* Macro to initialize the context info structure */
#define USER_CTX_INFO_INIT(ctx_info_p)	\
do {					\
	memset((ctx_info_p), 0, sizeof(struct client_crypto_ctx_info)); \
	(ctx_info_p)->sep_cache_idx = -1; /* 0 is a valid entry idx */ \
} while (0)

#define SEP_CTX_CACHE_NULL_HANDLE NULL

/**
 * ctxmgr_get_ctx_size() - Get host context size for given algorithm class
 * @alg_class:	 Queries algorithm class
 *
 * Returns size_t Size in bytes of host context
 */
size_t ctxmgr_get_ctx_size(enum crypto_alg_class alg_class);

/**
 * ctxmgr_map_user_ctx() - Map given user context to kernel space + DMA
 * @ctx_info:	 User context info structure
 * @mydev:	 Associated device context
 * @alg_class:	If !ALG_CLASS_NONE, consider context of given class for
 *		size validation (used in uninitialized context mapping)
 *		When set to ALG_CLASS_NONE, the alg_class field of the
 *		host_ctx is used to verify mapped buffer size.
 * @user_ctx_ptr: Pointer to user space context
 *
 * Returns int 0 for success
 */
int ctxmgr_map_user_ctx(struct client_crypto_ctx_info *ctx_info,
			struct device *mydev,
			enum crypto_alg_class alg_class,
			u32 __user *user_ctx_ptr);

/**
 * ctxmgr_unmap_user_ctx() - Unmap given currently mapped user context
 * @ctx_info:	 User context info structure
 */
void ctxmgr_unmap_user_ctx(struct client_crypto_ctx_info *ctx_info);

/**
 * ctxmgr_map_kernel_ctx() - Map given kernel context + clone SeP context into
 *				Privately allocated DMA buffer
 *				(required for async. ops. on the same context)
 * @ctx_info:	Client crypto context info structure
 * @mydev:	Associated device context
 * @alg_class:	If !ALG_CLASS_NONE, consider context of given class for
 *		size validation (used in uninitialized context mapping)
 *		When set to ALG_CLASS_NONE, the alg_class field of the
 *		host_crypto_ctx is used to verify mapped buffer size.
 * @kernel_ctx_p:	Pointer to kernel space crypto context
 * @sep_ctx_p:	Pointer to (private) SeP context. If !NULL the embedded sep
 *		context is copied into this buffer.
 *		Set to NULL to use the one embedded in host_crypto_ctx.
 * @sep_ctx_dma_addr:	DMA address of private SeP context (if sep_ctx_p!=NULL)
 *
 * Returns int 0 for success
 */
int ctxmgr_map_kernel_ctx(struct client_crypto_ctx_info *ctx_info,
			  struct device *mydev,
			  enum crypto_alg_class alg_class,
			  struct host_crypto_ctx *kernel_ctx_p,
			  struct sep_ctx_cache_entry *sep_ctx_p,
			  dma_addr_t sep_ctx_dma_addr);

/**
 * ctxmgr_unmap_kernel_ctx() - Unmap given currently mapped kernel context
 *				(was mapped with map_kernel_ctx)
 * @ctx_info:	 User context info structure
 */
void ctxmgr_unmap_kernel_ctx(struct client_crypto_ctx_info *ctx_info);

/**
 * ctxmgr_map2dev_hash_tail() - Map hash data tail buffer in the host context
 *				for DMA to device
 * @ctx_info:
 * @mydev:
 *
 * Returns int 0 on success
 */
int ctxmgr_map2dev_hash_tail(struct client_crypto_ctx_info *ctx_info,
			     struct device *mydev);
/**
 * ctxmgr_unmap2dev_hash_tail() - Unmap hash data tail buffer from DMA tp device
 * @ctx_info:
 * @mydev:
 *
 */
void ctxmgr_unmap2dev_hash_tail(struct client_crypto_ctx_info *ctx_info,
				struct device *mydev);

/**
 * ctxmgr_set_ctx_state() - Set context state
 * @ctx_info:	 User context info structure
 * @state:	    State to set in context
 *
 * Returns void
 */
void ctxmgr_set_ctx_state(struct client_crypto_ctx_info *ctx_info,
			  const enum host_ctx_state state);

/**
 * ctxmgr_get_ctx_state() - Set context state
 * @ctx_info:	 User context info structure
 *
 * Returns Current context state
 */
enum host_ctx_state ctxmgr_get_ctx_state(const struct client_crypto_ctx_info
					 *ctx_info);

/**
 * ctxmgr_set_ctx_id() - Allocate unique ID for (initialized) user context
 * @ctx_info:	 Client crypto context info structure
 * @ctx_id:	 The unique ID allocated for given context
 *
 * Allocate unique ID for (initialized) user context
 * (Assumes invoked within session mutex so no need for counter protection)
 */
void ctxmgr_set_ctx_id(struct client_crypto_ctx_info *ctx_info,
		       const struct crypto_ctx_uid ctx_id);

/**
 * ctxmgr_get_ctx_id() - Return the unique ID for current user context
 * @ctx_info:	 User context info structure
 *
 * Returns Allocated ID (or CTX_INVALID_ID if none)
 */
struct crypto_ctx_uid ctxmgr_get_ctx_id(struct client_crypto_ctx_info
					*ctx_info);

/**
 * ctxmgr_get_session_id() - Return the session ID of given context ID
 * @ctx_info:
 *
 * Return the session ID of given context ID
 * This may be used to validate ID and verify that it was not tampered
 * in a manner that can allow access to a session of another process
 * Returns u32
 */
u64 ctxmgr_get_session_id(struct client_crypto_ctx_info *ctx_info);

/**
 * ctxmgr_get_alg_class() - Get algorithm class of context
 *				(set during _init_ of a context)
 * @ctx_info:	 User context info structure
 *
 * Returns Current algorithm class of context
 */
enum crypto_alg_class ctxmgr_get_alg_class(const struct client_crypto_ctx_info
					   *ctx_info);

/**
 * ctxmgr_get_crypto_blk_size() - Get the crypto-block length of given context
 *					in octets
 * @ctx_info:	 User context info structure
 *
 * Returns u32 Cypto-block size in bytes, 0 if invalid/unsupported alg.
 */
u32 ctxmgr_get_crypto_blk_size(struct client_crypto_ctx_info *ctx_info);

/**
 * ctxmgr_is_valid_adata_size() - Validate additional/associated data for
 * @ctx_info:
 * @adata_size:
 *
 * Validate additional/associated data for
 * auth/enc algs.
 * Returns bool
 */
bool ctxmgr_is_valid_adata_size(struct client_crypto_ctx_info *ctx_info,
				unsigned long adata_size);

/**
 * ctxmgr_is_valid_size() - Validate given data unit for given alg./mode
 * @ctx_info:
 * @data_unit_size:
 * @is_finalize:
 *
 * Returns bool true is valid.
 */
bool ctxmgr_is_valid_size(struct client_crypto_ctx_info *ctx_info,
				    unsigned long data_unit_size,
				    bool is_finalize);

/**
 * ctxmgr_get_sym_cipher_type() - Returns the sym cipher specific type.
 * @ctx_info:	 The context info object of the sym cipher alg.
 *
 * Returns enum dxdi_sym_cipher_type The sym cipher type.
 */
enum dxdi_sym_cipher_type ctxmgr_get_sym_cipher_type(const struct
						     client_crypto_ctx_info
						     *ctx_info);

/**
 * ctxmgr_get_mac_type() - Returns the mac specific type.
 * @ctx_info:	 The context info object of the mac alg.
 *
 * Returns enum host_crypto_ctx_mac The mac type.
 */
enum dxdi_mac_type ctxmgr_get_mac_type(const struct client_crypto_ctx_info
				       *ctx_info);

/**
 * ctxmgr_get_hash_type() - Returns the hash specific type.
 * @ctx_info:	 The context info object of the hash alg.
 *
 * Returns enum dxdi_hash_type The hash type.
 */
enum dxdi_hash_type ctxmgr_get_hash_type(const struct client_crypto_ctx_info
					 *ctx_info);

/**
 * ctxmgr_get_mac_type() - Returns the hash specific type.
 * @ctx_info:	 The context info object of the hash alg.
 *
 * Returns enum dxdi_mac_type The hash type.
 */
enum dxdi_mac_type ctxmgr_get_mac_type(const struct client_crypto_ctx_info
				       *ctx_info);

/**
 * ctxmgr_save_hash_blk_remainder() - Save hash block tail data in given
 *	context. The data is taken from the save4next chunk of given client
 *	buffer.
 * @ctx_info:	 Client context info structure (HASH's or HMAC's)
 * @client_dma_buf_p:	A client DMA buffer object. Data is taken from the
 *			save4next chunk of this buffer.
 * @append_data:	When true, given data is appended to existing
 *
 * Returns 0 on success
 */
int ctxmgr_save_hash_blk_remainder(struct client_crypto_ctx_info *ctx_info,
				   struct client_dma_buffer *client_dma_buf_p,
				   bool append_data);

/**
 * ctxmgr_get_hash_blk_remainder_buf() - Get DMA info for hash block remainder
 *	buffer from given context
 * @ctx_info:			User context info structure
 * @hash_blk_tail_dma_p:	Returned tail buffer DMA address
 *
 * Note: This function must be invoked only when tail_buf is mapped2dev
 *	(using ctxmgr_map2dev_hash_tail)
 * Returns u16 Number of valid bytes/octets in tail buffer
 */
u16 ctxmgr_get_hash_blk_remainder_buf(struct client_crypto_ctx_info
					   *ctx_info,
					   dma_addr_t *
					   hash_blk_remainder_dma_p);

/**
 * ctxmgr_get_digest_or_mac() - Get the digest/MAC result when applicable
 * @ctx_info:		User context info structure
 * @digest_or_mac:	Pointer to digest/MAC buffer
 *
 * Returns Returned digest/MAC size
 */
u32 ctxmgr_get_digest_or_mac(struct client_crypto_ctx_info *ctx_info,
				  u8 *digest_or_mac);

/**
 * ctxmgr_get_digest_or_mac_ptr() - Get the digest/MAC pointer in SeP context
 * @ctx_info:		User context info structure
 * @digest_or_mac_pp:	Returned pointer to digest/MAC buffer
 *
 * Returns Returned digest/MAC size
 * This function may be used for the caller to reference the result instead
 * of always copying
 */
u32 ctxmgr_get_digest_or_mac_ptr(struct client_crypto_ctx_info *ctx_info,
				      u8 **digest_or_mac_pp);

/**
 * ctxmgr_set_symcipher_iv_user() - Set IV of symcipher context given in
 *					user space pointer
 * @user_ctx_ptr:	 A user space pointer to the host context
 * @iv_ptr:	 The IV to set
 *
 * Returns int
 */
int ctxmgr_set_symcipher_iv_user(u32 __user *user_ctx_ptr,
				 u8 *iv_ptr);

/**
 * ctxmgr_get_symcipher_iv_user() - Read current "IV"
 * @user_ctx_ptr:	A user space pointer to the host context
 * @iv_ptr:		Where to return the read IV
 *
 * Read current "IV" (block state - not the actual set IV during initialization)
 * This function works directly over a user space context
 * Returns int
 */
int ctxmgr_get_symcipher_iv_user(u32 __user *user_ctx_ptr,
				 u8 *iv_ptr);

/**
 * ctxmgr_set_symcipher_iv() - Set IV for given block symcipher algorithm
 * @ctx_info:	Context to update
 * @iv:		New IV
 *
 * Returns int 0 if changed IV, -EINVAL for error
 * (given cipher type does not have IV)
 */
int ctxmgr_set_symcipher_iv(struct client_crypto_ctx_info *ctx_info,
			    u8 *iv);

/**
 * ctxmgr_get_symcipher_iv() - Return given cipher IV
 * @ctx_info:	Context to query
 * @iv_user:	The IV given by the user on last ctxmgr_set_symcipher_iv
 * @iv_current:	The current IV state block
 * @iv_size_p:	[I/O] The given buffers size and returns actual IV size
 *
 * Return given cipher IV - Original IV given by user and current state "IV"
 * The given IV buffers must be large enough to accomodate the IVs
 * Returns int 0 on success, -ENOMEM if given iv_size is too small
 */
int ctxmgr_get_symcipher_iv(struct client_crypto_ctx_info *ctx_info,
			    u8 *iv_user, u8 *iv_current,
			    u8 *iv_size_p);

/**
 * ctxmgr_set_symcipher_direction() - Set the operation direction for given
 *					symcipher context
 * @ctx_info:		Context to update
 * @dxdi_direction:	Requested cipher direction
 *
 * Returns int 0 on success
 */
int ctxmgr_set_symcipher_direction(struct client_crypto_ctx_info *ctx_info,
				   enum dxdi_cipher_direction dxdi_direction);

/**
 * ctxmgr_get_symcipher_direction() - Return the operation direction of given
 *					symcipher context
 * @ctx_info:	Context to query
 *
 * Returns enum dxdi_cipher_direction (<0 on error)
 */
enum dxdi_cipher_direction ctxmgr_get_symcipher_direction(struct
							  client_crypto_ctx_info
							  *ctx_info);

/**
 * ctxmgr_set_symcipher_key() - Set a symcipher context key
 * @ctx_info:	Context to update
 * @key_size:	Size of key in bytes
 * @key:	New key pointer
 *
 * Set a symcipher context key
 * After invoking this function the context should be reinitialized by SeP
 * (set its state to "partial init" if not done in this sequence)
 * Returns int 0 on success, -EINVAL Invalid key len, -EPERM Forbidden/weak key
 */
int ctxmgr_set_symcipher_key(struct client_crypto_ctx_info *ctx_info,
			     u8 key_size, const u8 *key);

/**
 * ctxmgr_init_symcipher_ctx_no_props() - Initialize symcipher context when full
 *					props are not available, yet.
 * @ctx_info:		Context to init.
 * @cipher_type:	Cipher type for context
 *
 * Initialize symcipher context when full props are not available, yet.
 * Later set_key and set_iv may update the context.
 * Returns int 0 on success
 */
int ctxmgr_init_symcipher_ctx_no_props(struct client_crypto_ctx_info *ctx_info,
				       enum dxdi_sym_cipher_type cipher_type);

/**
 * ctxmgr_init_symcipher_ctx() - Initialize symCipher context based on given
 *				properties.
 * @ctx_info:	 User context mapping info.
 * @props:	 The initialization properties
 * @postpone_init:	Return "true" if INIT on SeP should be postponed
 *			to first processing (e.g, in AES-XTS)
 * @error_info:	Error info
 *
 * Returns 0 on success, otherwise on error
 */
int ctxmgr_init_symcipher_ctx(struct client_crypto_ctx_info *ctx_info,
			      struct dxdi_sym_cipher_props *props,
			      bool *postpone_init, u32 *error_info);

/**
 * ctxmgr_init_auth_enc_ctx() - Initialize Authenticated Encryption class
 *				context
 * @ctx_info:
 * @props:
 * @error_info:	Error info
 *
 * Returns int
 */
int ctxmgr_init_auth_enc_ctx(struct client_crypto_ctx_info *ctx_info,
			     struct dxdi_auth_enc_props *props,
			     u32 *error_info);

/**
 * ctxmgr_init_mac_ctx() - Initialize context for MAC algorithm
 * @ctx_info:
 * @props:
 * @error_info:	Error info
 *
 * Returns int
 */
int ctxmgr_init_mac_ctx(struct client_crypto_ctx_info *ctx_info,
			struct dxdi_mac_props *props, u32 *error_info);

/**
 * ctxmgr_init_hash_ctx() - Initialize hash context
 * @ctx_info:	 User context mapping info.
 * @hash_type:	 Assigned hash type
 * @error_info:	Error info
 *
 * Returns int 0 on success, -EINVAL, -ENOSYS
 */
int ctxmgr_init_hash_ctx(struct client_crypto_ctx_info *ctx_info,
			 enum dxdi_hash_type hash_type, u32 *error_info);

/**
 * ctxmgr_set_sep_cache_idx() - Set the index of this context in the sep_cache
 * @ctx_info:	 User context info structure
 * @sep_cache_idx:	 The allocated index in SeP cache for this context
 *
 * Returns void
 */
void ctxmgr_set_sep_cache_idx(struct client_crypto_ctx_info *ctx_info,
			      int sep_cache_idx);

/**
 * ctxmgr_get_sep_cache_idx() - Get the index of this context in the sep_cache
 * @ctx_info:	 User context info structure
 *
 * Returns The allocated index in SeP cache for this context
 */
int ctxmgr_get_sep_cache_idx(struct client_crypto_ctx_info *ctx_info);

#ifdef DEBUG
/**
 * ctxmgr_dump_sep_ctx() - Dump SeP context data
 * @ctx_info:
 *
 */
void ctxmgr_dump_sep_ctx(const struct client_crypto_ctx_info *ctx_info);
#else
#define ctxmgr_dump_sep_ctx(ctx_info) do {} while (0)
#endif /*DEBUG*/
/**
 * ctxmgr_sync_sep_ctx() - Sync. SeP context to device (flush from cache...)
 * @ctx_info:	 User context info structure
 * @mydev:	 Associated device context
 *
 * Returns void
 */
void ctxmgr_sync_sep_ctx(const struct client_crypto_ctx_info *ctx_info,
			 struct device *mydev);

/**
 * ctxmgr_get_sep_ctx_dma_addr() - Return DMA address of SeP (FW) area of the context
 * @ctx_info:	 User context info structure
 *
 * Returns DMA address of SeP (FW) area of the context
 */
dma_addr_t ctxmgr_get_sep_ctx_dma_addr(const struct client_crypto_ctx_info
				       *ctx_info);

/******************************
 * SeP context cache functions
 ******************************/

/**
 * ctxmgr_sep_cache_create() - Create a SeP (FW) cache manager of given num. of
 *				entries
 * @num_of_entries:	 = Number of entries available in cache
 *
 * Returns void * handle (NULL on failure)
 */
void *ctxmgr_sep_cache_create(int num_of_entries);

/**
 * ctxmgr_sep_cache_destroy() - Destory SeP (FW) cache manager object
 * @sep_cache:	 = The cache object
 *
 * Returns void
 */
void ctxmgr_sep_cache_destroy(void *sep_cache);

/**
 * ctxmgr_sep_cache_get_size() - Get cache size (entries count)
 * @sep_cache:
 *
 * Returns int Number of cache entries available
 */
int ctxmgr_sep_cache_get_size(void *sep_cache);

/**
 * ctxmgr_sep_cache_alloc() - Allocate a cache entry of given SeP context cache
 * @sep_cache:	 The cache object
 * @ctx_id:	 The user context ID
 * @load_required_p:	Pointed int is set to !0 if a cache load is required
 *			(i.e., if item already loaded in cache it would be 0)
 *
 * Returns cache index
 */
int ctxmgr_sep_cache_alloc(void *sep_cache,
			   struct crypto_ctx_uid ctx_id, int *load_required_p);

/**
 * ctxmgr_sep_cache_invalidate() - Invalidate cache entry for given context ID
 * @sep_cache:	The cache object
 * @ctx_id:	The host crypto. context ID
 * @id_mask:	A bit mask to be used when comparing the ID
 *		(to be used for a set of entries from the same client)
 *
 * Returns void
 */
void ctxmgr_sep_cache_invalidate(void *sep_cache,
				 struct crypto_ctx_uid ctx_id,
				 u64 id_mask);

#endif /*_CRYPTO_CTX_MGR_H_*/
