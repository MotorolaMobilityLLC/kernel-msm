// SPDX-License-Identifier: GPL-2.0
/*
 * fs-verity: read-only file-based integrity/authenticity
 *
 * Copyright (C) 2018, Google, Inc.
 *
 * Written by Jaegeuk Kim, Michael Halcrow, and Eric Biggers.
 */

#ifndef _FSVERITY_PRIVATE_H
#define _FSVERITY_PRIVATE_H

#ifdef CONFIG_FS_VERITY_DEBUG
#define DEBUG
#endif

#define pr_fmt(fmt) "fs-verity: " fmt

#include <crypto/sha.h>
#define __FS_HAS_VERITY 1
#include <linux/fsverity.h>

#define FS_VERITY_MAGIC		"TrueBrew"
#define FS_VERITY_MAJOR		1
#define FS_VERITY_MINOR		0
#define FS_VERITY_SALT_SIZE	8

struct fsverity_footer {
	u8 magic[8];		/* must be FS_VERITY_MAGIC */
	u8 major_version;	/* must be FS_VERITY_MAJOR */
	u8 minor_version;	/* must be FS_VERITY_MINOR */
	u8 log_blocksize;	/* log2(data-bytes-per-hash), e.g. 12 for 4KB */
	u8 log_arity;		/* log2(leaves-per-node), e.g. 7 for SHA-256 */
	__le16 meta_algorithm;	/* hash algorithm for tree blocks */
	__le16 data_algorithm;	/* hash algorithm for data blocks */
	__le32 flags;		/* flags */
	__le32 reserved1;	/* must be 0 */
	__le64 size;		/* size of the original, unpadded data */
	u8 reserved2[2];	/* must be 0 */
	u8 salt[FS_VERITY_SALT_SIZE]; /* used to salt the hashes */
	u8 reserved3[22];	/* must be 0 */
	/* This structure is 64 bytes long */
} __packed;

#define FS_VERITY_FLAG_INTEGRITY_ONLY	0x00000001

/*
 * Up to 64 levels are theoretically possible with a very small block size, but
 * we'd like to limit stack usage, and in practice this is plenty.  E.g., with
 * SHA-256 and 4K blocks, a file with size UINT64_MAX bytes needs just 8 levels.
 */
#define FS_VERITY_MAX_LEVELS		16

#define FS_VERITY_MAX_DIGEST_SIZE	SHA256_DIGEST_SIZE

struct fsverity_hash_alg;

/* Mode of an fs-verity file */
enum fsverity_mode {
	/* Waiting for root of trust to be provided, reads will fail */
	FS_VERITY_MODE_NEED_AUTHENTICATION,

	/* File contents don't match root of trust, reads will fail */
	FS_VERITY_MODE_AUTHENTICATION_FAILED,

	/* File contents match root of trust, reads will succeed */
	FS_VERITY_MODE_AUTHENTICATED,

	/* Integrity-only mode with no root of trust, reads will succeed */
	FS_VERITY_MODE_INTEGRITY_ONLY,
};


/**
 * fsverity_info - cached fs-verity metadata for an inode
 *
 * When a fs-verity file is first opened, an instance of this struct is
 * allocated and stored in ->i_verity_info.  It caches various values from the
 * fs-verity metadata, such as the tree topology and the root hash, which are
 * needed to efficiently verify data read from the file.  Once created, it
 * remains until the inode is evicted.
 */
struct fsverity_info {
	const struct fsverity_hash_alg *hash_alg; /* hash algorithm */
	u8 block_bits;			/* log2(block size) */
	u8 log_arity;			/* log2(hashes per hash block) */
	u8 depth;			/* depth of the Merkle tree */
	enum fsverity_mode mode;	/* current mode of the file */
	u8 salt[FS_VERITY_SALT_SIZE];	/* used to salt the hashes */
	u64 data_i_size;		/* original file size */
	u64 elided_i_size;		/* original file size after elisions */
	u64 full_i_size;		/* full file size including metadata */
	u8 root_hash[FS_VERITY_MAX_DIGEST_SIZE];   /* Merkle tree root hash */
	u8 measurement[FS_VERITY_MAX_DIGEST_SIZE]; /* file measurement */

	/* Starting blocks for each tree level. 'depth-1' is the root level. */
	u64 hash_lvl_region_idx[FS_VERITY_MAX_LEVELS];
};

#endif /* _FSVERITY_PRIVATE_H */
