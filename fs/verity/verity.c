// SPDX-License-Identifier: GPL-2.0
/*
 * fs-verity: read-only file-based integrity/authenticity
 *
 * Copyright (C) 2018, Google, Inc.
 *
 * Written by Jaegeuk Kim, Michael Halcrow, and Eric Biggers.
 */

#include "fsverity_private.h"

#include <asm/unaligned.h>
#include <crypto/hash.h>
#include <linux/bio.h>
#include <linux/list.h>
#include <linux/list_sort.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/printk.h>
#include <linux/ratelimit.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

/* Allocated at module initialization time */
static struct workqueue_struct *fsverity_read_workqueue;
static struct kmem_cache *fsverity_info_cachep;

/* List of supported hash algorithms */
static struct fsverity_hash_alg {
	struct crypto_shash *tfm; /* allocated on demand */
	const char *name;
	unsigned int digest_size;
} fsverity_hash_algs[] = {
	[FS_VERITY_ALG_SHA256] = {
		.name = "sha256",
		.digest_size = 32,
	},
	[FS_VERITY_ALG_CRC32] = {
		.name = "crc32",
		.digest_size = 4,
	},
};

/*
 * Translate the given fs-verity hash algorithm number into a struct describing
 * the algorithm, and ensure it has a hash transform ready to go.  The hash
 * transforms are allocated on-demand firstly to not waste resources when they
 * aren't needed, and secondly because the fs-verity module may be loaded
 * earlier than the needed crypto modules.
 */
static const struct fsverity_hash_alg *get_hash_algorithm(unsigned int num)
{
	struct fsverity_hash_alg *alg;
	struct crypto_shash *tfm;
	int err;

	if (num >= ARRAY_SIZE(fsverity_hash_algs) ||
	    !fsverity_hash_algs[num].name) {
		pr_warn("Unknown hash algorithm: %u\n", num);
		return ERR_PTR(-EINVAL);
	}
	alg = &fsverity_hash_algs[num];
retry:
	/* pairs with cmpxchg_release() below */
	tfm = smp_load_acquire(&alg->tfm);
	if (tfm)
		return alg;

	tfm = crypto_alloc_shash(alg->name, 0, 0);
	if (IS_ERR(tfm)) {
		if (PTR_ERR(tfm) == -ENOENT)
			pr_warn("Algorithm %u (%s) is unavailable\n",
				num, alg->name);
		else
			pr_warn("Error allocating algorithm %u (%s): %ld\n",
				num, alg->name, PTR_ERR(tfm));
		return ERR_CAST(tfm);
	}

	err = -EINVAL;
	if (WARN_ON(alg->digest_size != crypto_shash_digestsize(tfm)))
		goto err_free_tfm;

	pr_debug("Allocated '%s' transform; implementation '%s'\n",
		 alg->name, crypto_shash_alg(tfm)->base.cra_driver_name);

	/* Set the initial value if required by the algorithm */
	if (num == FS_VERITY_ALG_CRC32) {
		__le32 initial_value = cpu_to_le32(0xFFFFFFFF);

		err = crypto_shash_setkey(tfm, (u8 *)&initial_value, 4);
		if (err) {
			pr_warn("Error setting CRC-32 initial value: %d\n",
				err);
			goto err_free_tfm;
		}
	}

	/* pairs with smp_load_acquire() above */
	if (cmpxchg_release(&alg->tfm, NULL, tfm) != NULL) {
		crypto_free_shash(tfm);
		goto retry;
	}

	return alg;

err_free_tfm:
	crypto_free_shash(tfm);
	return ERR_PTR(err);
}

static int finalize_hash(struct fsverity_info *vi, struct shash_desc *desc,
			 u8 *out)
{
	int err;

	err = crypto_shash_final(desc, out);
	if (err)
		return err;

	/* For CRC-32, invert at the end and convert to big-endian format */
	if (vi->hash_alg == &fsverity_hash_algs[FS_VERITY_ALG_CRC32])
		put_unaligned_be32(~get_unaligned_le32(out), out);
	return 0;
}

static struct fsverity_info *get_fsverity_info(const struct inode *inode)
{
	/* pairs with cmpxchg_release() in set_fsverity_info() */
	return smp_load_acquire(&inode->i_verity_info);
}

static bool set_fsverity_info(struct inode *inode, struct fsverity_info *vi)
{
	/* pairs with smp_load_acquire() in get_fsverity_info() */
	if (cmpxchg_release(&inode->i_verity_info, NULL, vi) != NULL)
		return false;

	/*
	 * Set the in-memory i_size to the data i_size.  But it should *not* be
	 * written to disk, since then it would no longer be possible to find
	 * the fs-verity footer.
	 */
	i_size_write(inode, vi->data_i_size);
	return true;
}

static void dump_fsverity_footer(const struct fsverity_footer *ftr)
{
	pr_debug("magic = %.*s\n", (int)sizeof(ftr->magic), ftr->magic);
	pr_debug("major_version = %u\n", ftr->major_version);
	pr_debug("minor_version = %u\n", ftr->minor_version);
	pr_debug("log_blocksize = %u\n", ftr->log_blocksize);
	pr_debug("log_arity = %u\n", ftr->log_arity);
	pr_debug("meta_algorithm = %u\n", le16_to_cpu(ftr->meta_algorithm));
	pr_debug("data_algorithm = %u\n", le16_to_cpu(ftr->data_algorithm));
	pr_debug("flags = %#x\n", le32_to_cpu(ftr->flags));
	pr_debug("size = %llu\n", le64_to_cpu(ftr->size));
	pr_debug("salt = %*phN\n", (int)sizeof(ftr->salt), ftr->salt);
}

/*
 * Parse an fs-verity footer, loading information into the fsverity_info.
 *
 * Return: 0 on success, -errno on failure
 */
static int parse_footer(struct fsverity_info *vi,
			const struct fsverity_footer *ftr)
{
	unsigned int alg_num;
	u32 flags;

	/* magic */
	if (memcmp(ftr->magic, FS_VERITY_MAGIC, sizeof(ftr->magic))) {
		pr_warn("fs-verity footer not found\n");
		return -EINVAL;
	}

	/* major_version */
	if (ftr->major_version != FS_VERITY_MAJOR) {
		pr_warn("Unsupported major version (%u)\n", ftr->major_version);
		return -EINVAL;
	}

	/* minor_version */
	if (ftr->minor_version != FS_VERITY_MINOR) {
		pr_warn("Unsupported minor version (%u)\n", ftr->minor_version);
		return -EINVAL;
	}

	/* data_algorithm and meta_algorithm */
	alg_num = le16_to_cpu(ftr->data_algorithm);
	if (alg_num != le16_to_cpu(ftr->meta_algorithm)) {
		pr_warn("Unimplemented case: data (%u) and metadata (%u) hash algorithms differ\n",
			alg_num, le16_to_cpu(ftr->meta_algorithm));
		return -EINVAL;
	}
	vi->hash_alg = get_hash_algorithm(alg_num);
	if (IS_ERR(vi->hash_alg))
		return PTR_ERR(vi->hash_alg);

	/* log_blocksize */
	if (ftr->log_blocksize != PAGE_SHIFT) {
		pr_warn("Unsupported log_blocksize (%u).  Need block_size == PAGE_SIZE.\n",
			ftr->log_blocksize);
		return -EINVAL;
	}
	vi->block_bits = ftr->log_blocksize;

	/* log_arity */
	vi->log_arity = ftr->log_arity;
	if (vi->log_arity !=
	    vi->block_bits - ilog2(vi->hash_alg->digest_size)) {
		pr_warn("Unsupported log_arity (%u)\n", vi->log_arity);
		return -EINVAL;
	}

	/* flags */
	flags = le32_to_cpu(ftr->flags);
	if (flags & ~FS_VERITY_FLAG_INTEGRITY_ONLY) {
		pr_warn("Unsupported flags (%#x)\n", flags);
		return -EINVAL;
	}

	if (flags & FS_VERITY_FLAG_INTEGRITY_ONLY) {
#ifdef CONFIG_FS_VERITY_INTEGRITY_ONLY
		pr_warn("Using experimental integrity-only mode!\n");
		vi->mode = FS_VERITY_MODE_INTEGRITY_ONLY;
#else
		pr_warn("Integrity-only mode not supported\n");
		return -EINVAL;
#endif
	}

	/* reserved fields */
	if (ftr->reserved1 ||
	    memchr_inv(ftr->reserved2, 0, sizeof(ftr->reserved2)) ||
	    memchr_inv(ftr->reserved3, 0, sizeof(ftr->reserved3))) {
		pr_warn("Reserved bits set in footer\n");
		return -EINVAL;
	}

	/* size */
	vi->data_i_size = le64_to_cpu(ftr->size);
	if (vi->data_i_size == 0) {
		pr_warn("Original file size is 0; this is not supported\n");
		return -EINVAL;
	}
	if (vi->data_i_size > vi->full_i_size) {
		pr_warn("Original file size is too large (%llu)\n",
			vi->data_i_size);
		return -EINVAL;
	}
	vi->elided_i_size = vi->data_i_size;

	/* salt */
	memcpy(vi->salt, ftr->salt, FS_VERITY_SALT_SIZE);

	return 0;
}

/*
 * Calculate the depth of the Merkle tree, then create a map from level to the
 * block offset at which that level's hash blocks start.  Level 'depth - 1' is
 * the root and is stored first in the file, in the first block following the
 * original data.  Level 0 is the level directly "above" the data blocks and is
 * stored last in the file, just before the fs-verity footer.
 */
static int compute_tree_depth_and_offsets(struct fsverity_info *vi)
{
	unsigned int hashes_per_block = 1 << vi->log_arity;
	u64 blocks = (vi->elided_i_size + (1 << vi->block_bits) - 1) >>
			vi->block_bits;
	u64 offset = (vi->data_i_size + (1 << vi->block_bits) - 1) >>
			vi->block_bits;
	int depth = 0;
	int i;

	while (blocks > 1) {
		if (depth >= FS_VERITY_MAX_LEVELS) {
			pr_warn("Too many tree levels (max is %d)\n",
				FS_VERITY_MAX_LEVELS);
			return -EINVAL;
		}
		blocks = (blocks + hashes_per_block - 1) >> vi->log_arity;
		vi->hash_lvl_region_idx[depth++] = blocks;
	}
	vi->depth = depth;
	pr_debug("depth = %d\n", depth);

	for (i = depth - 1; i >= 0; i--) {
		u64 next_count = vi->hash_lvl_region_idx[i];

		vi->hash_lvl_region_idx[i] = offset;
		pr_debug("Level %d is [%llu..%llu] (%llu blocks)\n",
			 i, offset, offset + next_count - 1, next_count);
		offset += next_count;
	}
	return 0;
}

/*
 * Compute the hash of the root of the Merkle tree (or of the lone data block
 * for files <= blocksize in length) and store it in vi->root_hash.
 */
static int compute_root_hash(struct inode *inode, struct fsverity_info *vi)
{
	SHASH_DESC_ON_STACK(desc, vi->hash_alg->tfm);
	struct page *root_page;
	pgoff_t root_idx;
	int err;

	desc->tfm = vi->hash_alg->tfm;
	desc->flags = 0;

	if (vi->depth)
		root_idx = vi->hash_lvl_region_idx[vi->depth - 1];
	else
		root_idx = 0;

	root_page = inode->i_sb->s_vop->read_metadata_page(inode, root_idx);
	if (IS_ERR(root_page)) {
		pr_warn("Error reading root block: %ld\n", PTR_ERR(root_page));
		return PTR_ERR(root_page);
	}

	err = crypto_shash_init(desc);
	if (!err)
		err = crypto_shash_update(desc, vi->salt, FS_VERITY_SALT_SIZE);
	if (!err) {
		/* atomic, since we aren't using CRYPTO_TFM_REQ_MAY_SLEEP */
		void *root = kmap_atomic(root_page);

		err = crypto_shash_update(desc, root, PAGE_SIZE);
		kunmap_atomic(root);
	}
	if (!err)
		err = finalize_hash(vi, desc, vi->root_hash);

	if (err)
		pr_warn("Error computing Merkle tree root hash: %d\n", err);
	else
		pr_debug("Computed Merkle tree root hash: %*phN\n",
			 vi->hash_alg->digest_size, vi->root_hash);
	put_page(root_page);

	if (vi->depth == 0) {
		/*
		 * The data page must not be left in the page cache, otherwise
		 * it would appear to have been verified.
		 */
		truncate_inode_pages_range(inode->i_mapping,
				   (loff_t)root_idx << PAGE_SHIFT,
				   (((loff_t)root_idx + 1) << PAGE_SHIFT) - 1);
	}

	return err;
}

/* Compute the file's measurement and store it in vi->measurement */
static int compute_measurement(struct fsverity_info *vi,
			       const struct fsverity_footer *ftr)
{
	SHASH_DESC_ON_STACK(desc, vi->hash_alg->tfm);
	int err;

	desc->tfm = vi->hash_alg->tfm;
	desc->flags = 0;

	err = crypto_shash_init(desc);
	if (!err)
		err = crypto_shash_update(desc, (const u8 *)ftr, sizeof(*ftr));
	if (!err)
		err = crypto_shash_update(desc, vi->root_hash,
					  vi->hash_alg->digest_size);
	if (!err)
		err = finalize_hash(vi, desc, vi->measurement);

	if (err)
		pr_warn("Error computing fs-verity measurement: %d\n", err);
	else
		pr_debug("Computed measurement: %*phN\n",
			 vi->hash_alg->digest_size, vi->measurement);
	return err;
}

static struct fsverity_info *alloc_fsverity_info(void)
{
	struct fsverity_info *vi;

	vi = kmem_cache_zalloc(fsverity_info_cachep, GFP_NOFS);
	if (!vi)
		return NULL;
	vi->mode = FS_VERITY_MODE_NEED_AUTHENTICATION;
	return vi;
}

static void free_fsverity_info(struct fsverity_info *vi)
{
	if (!vi)
		return;
	kmem_cache_free(fsverity_info_cachep, vi);
}

/*
 * Read the file's fs-verity footer and create an fsverity_info for it.
 *
 * TODO(mhalcrow): This logic only works with full-size Merkle
 * trees that include all padding and/or when footer/extent
 * content fits in one page.
 */
static struct fsverity_info *create_fsverity_info(struct inode *inode)
{
	struct fsverity_info *vi;
	loff_t full_isize = i_size_read(inode);
	unsigned int last_validsize = ((full_isize - 1) & ~PAGE_MASK) + 1;
	pgoff_t ftr_pgoff = (full_isize - 1) >> PAGE_SHIFT;
	struct page *ftr_page = NULL;
	const void *ftr_virt;
	unsigned int ftr_reverse_offset_loc;
	u32 ftr_reverse_offset;
	unsigned int ftr_loc;
	const struct fsverity_footer *ftr;
	int err;

	pr_debug("full_isize = %lld\n", full_isize);
	pr_debug("last_validsize=%u\n", last_validsize);
	pr_debug("ftr_pgoff=%lu\n", ftr_pgoff);

	if (full_isize <= 0) {
		pr_warn("File is empty!\n");
		return ERR_PTR(-EINVAL);
	}

	if (last_validsize < sizeof(*ftr) + sizeof(__le32)) {
		pr_warn("Unsupported alignment for fs-verity metadata -- not enough data in last page (only %u bytes)\n",
			last_validsize);
		return ERR_PTR(-EINVAL);
	}

	vi = alloc_fsverity_info();
	if (!vi)
		return ERR_PTR(-ENOMEM);

	vi->full_i_size = full_isize;

	ftr_page = inode->i_sb->s_vop->read_metadata_page(inode, ftr_pgoff);
	if (IS_ERR(ftr_page)) {
		err = PTR_ERR(ftr_page);
		pr_warn("Error reading footer page: %d\n", err);
		ftr_page = NULL;
		goto out;
	}
	ftr_virt = kmap(ftr_page);

	ftr_reverse_offset_loc = last_validsize - sizeof(__le32);
	pr_debug("ftr_reverse_offset_loc=%u\n", ftr_reverse_offset_loc);
	ftr_reverse_offset = get_unaligned_le32(ftr_virt +
						ftr_reverse_offset_loc);
	pr_debug("ftr_reverse_offset=%u\n", ftr_reverse_offset);
	if (ftr_reverse_offset < sizeof(*ftr) + sizeof(__le32)) {
		pr_warn("Invalid fs-verity metadata (unexpected ftr_reverse_offset: %u)\n",
			ftr_reverse_offset);
		err = -EINVAL;
		goto out;
	}

	if (ftr_reverse_offset > last_validsize) {
		pr_warn("Unimplemented case - fs-verity footer crosses pages (last_validsize=%u, ftr_reverse_offset=%u)",
			last_validsize, ftr_reverse_offset);
		err = -EINVAL;
		goto out;
	}
	ftr_loc = last_validsize - ftr_reverse_offset;
	if (ftr_loc & 7) {
		pr_warn("fs-verity footer is misaligned. last_validsize=%u, ftr_reverse_offset=%u\n",
			last_validsize, ftr_reverse_offset);
		err = -EINVAL;
		goto out;
	}
	ftr = ftr_virt + ftr_loc;
	dump_fsverity_footer(ftr);
	err = parse_footer(vi, ftr);
	if (err)
		goto out;

	err = compute_tree_depth_and_offsets(vi);
	if (err)
		goto out;

	err = compute_root_hash(inode, vi);
	if (err)
		goto out;
	err = compute_measurement(vi, ftr);
out:
	if (ftr_page) {
		kunmap(ftr_page);
		put_page(ftr_page);
	}
	if (err) {
		free_fsverity_info(vi);
		vi = ERR_PTR(err);
	}
	return vi;
}

/* Ensure the inode has an ->i_verity_info */
static int setup_fsverity_info(struct inode *inode)
{
	struct fsverity_info *vi = get_fsverity_info(inode);

	if (vi)
		return 0;

	vi = create_fsverity_info(inode);
	if (IS_ERR(vi))
		return PTR_ERR(vi);

	if (!set_fsverity_info(inode, vi))
		free_fsverity_info(vi);
	return 0;
}

/**
 * fsverity_file_open - prepare to open an fs-verity file
 * @inode: the inode being opened
 * @filp: the struct file being set up
 *
 * When opening an fs-verity file, deny the open if it is for writing.
 * Otherwise, set up the inode's ->i_verity_info (if not already done) by
 * parsing the fs-verity metadata at the end of the file.
 *
 * When combined with fscrypt, this must be called after fscrypt_file_open().
 * Otherwise, we won't have the key set up to decrypt the fs-verity metadata.
 *
 * TODO: currently userspace has to provide the expected measurement using
 * FS_IOC_SET_VERITY_MEASUREMENT, but in the future here we will validate the
 * computed measurement against a PKCS#7 message embedded in the fs-verity
 * metadata which contains the signed expected measurement.
 *
 * Return: 0 on success, -errno on failure
 */
int fsverity_file_open(struct inode *inode, struct file *filp)
{
	if (filp->f_mode & FMODE_WRITE) {
		pr_debug("Denying opening fs-verity file (ino %lu) for write\n",
			 inode->i_ino);
		return -EPERM;
	}

	pr_debug("Opening fs-verity file (ino %lu)\n", inode->i_ino);

	return setup_fsverity_info(inode);
}
EXPORT_SYMBOL_GPL(fsverity_file_open);

/**
 * fsverity_prepare_setattr - prepare to change an fs-verity inode's attributes
 * @dentry: dentry through which the inode is being changed
 * @attr: attributes to change
 *
 * fs-verity files are immutable, so deny truncates.  This isn't covered by the
 * open-time check because sys_truncate() takes a path, not a file descriptor.
 *
 * Return: 0 on success, -errno on failure
 */
int fsverity_prepare_setattr(struct dentry *dentry, struct iattr *attr)
{
	if (attr->ia_valid & ATTR_SIZE) {
		pr_debug("Denying truncate of fs-verity file (ino %lu)\n",
			 d_inode(dentry)->i_ino);
		return -EPERM;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(fsverity_prepare_setattr);

/**
 * fsverity_prepare_getattr - prepare to get an fsverity inode's attributes
 * @inode: the inode for which the attributes are being retrieved
 *
 * To make st_size exclude the fs-verity metadata even before the file has been
 * opened for the first time, we need to grab the original data size from the
 * fs-verity footer.  Currently, to implement this we just set up the
 * ->i_verity_info, like in the ->open() hook.
 *
 * However, when combined with fscrypt, on an encrypted file this must only be
 * called if the encryption key has been set up!
 *
 * Return: 0 on success, -errno on failure
 */
int fsverity_prepare_getattr(struct inode *inode)
{
	return setup_fsverity_info(inode);
}
EXPORT_SYMBOL_GPL(fsverity_prepare_getattr);

/**
 * fsverity_cleanup_inode - free the inode's verity info, if present
 *
 * Filesystems must call this on inode eviction to free ->i_verity_info.
 */
void fsverity_cleanup_inode(struct inode *inode)
{
	free_fsverity_info(inode->i_verity_info);
}
EXPORT_SYMBOL_GPL(fsverity_cleanup_inode);

/**
 * hash_at_level() - compute the location of the block's hash at the given level
 *
 * @vi:		(in) the file's verity info
 * @dindex:	(in) the index of the data block being verified
 * @level:	(in) the level of hash we want
 * @hindex:	(out) the index of the hash block containing the wanted hash
 * @hoffset:	(out) the byte offset to the wanted hash within the hash block
 */
static void hash_at_level(const struct fsverity_info *vi, pgoff_t dindex,
			  unsigned int level, pgoff_t *hindex,
			  unsigned int *hoffset)
{
	pgoff_t hoffset_in_lvl;

	/*
	 * Compute the offset of the hash within the level's region, in hashes.
	 * For example, with 4096-byte blocks and 32-byte hashes, there are
	 * 4096/32 = 128 = 2^7 hashes per hash block, i.e. log_arity = 7.  Then,
	 * if the data block index is 65668 and we want the level 1 hash, it is
	 * located at 65668 >> 7 = 513 hashes into the level 1 region.
	 */
	hoffset_in_lvl = dindex >> (level * vi->log_arity);

	/*
	 * Compute the index of the hash block containing the wanted hash.
	 * Continuing the above example, the block would be at index 513 >> 7 =
	 * 4 within the level 1 region.  To this we'd add the index at which the
	 * level 1 region starts.
	 */
	*hindex = vi->hash_lvl_region_idx[level] +
		  (hoffset_in_lvl >> vi->log_arity);

	/*
	 * Finally, compute the index of the hash within the block rather than
	 * the region, and multiply by the hash size to turn it into a byte
	 * offset.  Continuing the above example, the hash would be at byte
	 * offset (513 & ((1 << 7) - 1)) * 32 = 32 within the block.
	 */
	*hoffset = (hoffset_in_lvl & ((1 << vi->log_arity) - 1)) *
		   vi->hash_alg->digest_size;
}

/* Extract a hash from a hash page */
static void extract_hash(struct page *hpage, unsigned int hoffset,
			 unsigned int hsize, u8 *out)
{
	void *virt = kmap_atomic(hpage);

	memcpy(out, virt + hoffset, hsize);
	kunmap_atomic(virt);
}

static int hash_page(struct fsverity_info *vi, struct page *page, u8 *out)
{
	SHASH_DESC_ON_STACK(desc, vi->hash_alg->tfm);
	void *virt;
	int err;

	desc->tfm = vi->hash_alg->tfm;
	desc->flags = 0;

	err = crypto_shash_init(desc);
	if (err)
		return err;

	err = crypto_shash_update(desc, vi->salt, FS_VERITY_SALT_SIZE);
	if (err)
		return err;

	virt = kmap_atomic(page);
	err = crypto_shash_update(desc, virt, PAGE_SIZE);
	kunmap_atomic(virt);
	if (err)
		return err;

	return finalize_hash(vi, desc, out);
}

static int compare_hashes(const u8 *want_hash, const u8 *real_hash,
			  int digest_size, struct inode *inode, pgoff_t index,
			  int level)
{
	if (memcmp(want_hash, real_hash, digest_size) == 0)
		return 0;

	pr_warn_ratelimited("VERIFICATION FAILURE!  ino=%lu, index=%lu, level=%d, want_hash=%*phN, real_hash=%*phN\n",
			    inode->i_ino, index, level,
			    digest_size, want_hash, digest_size, real_hash);
	return -EBADMSG;
}

/**
 * fsverity_verify_page - verify a data page
 *
 * Verify the integrity and/or authenticity of a page that has just been read
 * from the file.  The page is assumed to be a pagecache page.
 *
 * Return: true if the page is valid, else false.
 */
bool fsverity_verify_page(struct page *data_page)
{
	struct address_space *mapping = data_page->mapping;
	struct inode *inode = mapping->host;
	struct fsverity_info *vi = get_fsverity_info(inode);
	pgoff_t index = data_page->index;
	int level = 0;
	u8 _want_hash[FS_VERITY_MAX_DIGEST_SIZE];
	u8 *want_hash = NULL;
	u8 real_hash[FS_VERITY_MAX_DIGEST_SIZE];
	struct page *hpages[FS_VERITY_MAX_LEVELS];
	unsigned int hoffsets[FS_VERITY_MAX_LEVELS];
	int err;

	/*
	 * It shouldn't be possible to get here without ->i_verity_info set,
	 * since it's set on ->open().
	 */
	if (WARN_ON_ONCE(!vi))
		return false;

	/* The page must not be unlocked until verification has completed. */
	if (WARN_ON_ONCE(!PageLocked(data_page)))
		return false;

	/* Fail the read if the data needs to be authenticated but isn't */
	switch (vi->mode) {
	case FS_VERITY_MODE_AUTHENTICATION_FAILED:
	case FS_VERITY_MODE_NEED_AUTHENTICATION:
		pr_warn("%s (ino %lu), failing read; index=%lu\n",
			(vi->mode == FS_VERITY_MODE_NEED_AUTHENTICATION ?
			 "No root of trust provided yet" :
			 "Root authentication failed"), inode->i_ino, index);
		return false;
	case FS_VERITY_MODE_AUTHENTICATED:
	case FS_VERITY_MODE_INTEGRITY_ONLY:
		break;
	default:
		WARN_ON_ONCE(1);
		return false;
	}

	/*
	 * Since ->i_size is overridden with ->data_i_size, and fs-verity avoids
	 * recursing into itself when reading hash pages, we shouldn't normally
	 * get here with a page beyond ->data_i_size.  But, it can happen if a
	 * read is issued at or beyond EOF since the VFS doesn't check i_size
	 * before calling ->readpage().  Thus, just skip verification if the
	 * page is beyond ->data_i_size.
	 */
	if (index >= (vi->data_i_size + PAGE_SIZE - 1) >> PAGE_SHIFT) {
		pr_debug("Page %lu is in metadata region\n", index);
		return true;
	}

	pr_debug_ratelimited("Verifying data page %lu...\n", index);

	/*
	 * Starting at the leaves, ascend the tree saving hash pages along the
	 * way until we find a verified hash page, indicated by PageChecked; or
	 * until we reach the root.
	 */
	for (level = 0; level < vi->depth; level++) {
		pgoff_t hindex;
		unsigned int hoffset;
		struct page *hpage;

		hash_at_level(vi, index, level, &hindex, &hoffset);

		pr_debug_ratelimited("Level %d: hindex=%lu, hoffset=%u\n",
				     level, hindex, hoffset);

		hpage = inode->i_sb->s_vop->read_metadata_page(inode, hindex);
		if (IS_ERR(hpage)) {
			err = PTR_ERR(hpage);
			goto out;
		}

		if (PageChecked(hpage)) {
			want_hash = _want_hash;
			extract_hash(hpage, hoffset, vi->hash_alg->digest_size,
				     want_hash);
			put_page(hpage);
			pr_debug_ratelimited("Hash page already checked, want hash %*phN\n",
					     vi->hash_alg->digest_size,
					     want_hash);
			break;
		}
		pr_debug_ratelimited("Hash page not yet checked\n");
		hpages[level] = hpage;
		hoffsets[level] = hoffset;
	}

	if (!want_hash) {
		want_hash = vi->root_hash;
		pr_debug("Want root hash: %*phN\n", vi->hash_alg->digest_size,
			 want_hash);
	}

	/* Descend the tree verifying hash pages */
	for (; level > 0; level--) {
		struct page *hpage = hpages[level - 1];
		unsigned int hoffset = hoffsets[level - 1];

		err = hash_page(vi, hpage, real_hash);
		if (err)
			goto out;
		err = compare_hashes(want_hash, real_hash,
				     vi->hash_alg->digest_size,
				     inode, index, level - 1);
		if (err)
			goto out;
		SetPageChecked(hpage);
		want_hash = _want_hash;
		extract_hash(hpage, hoffset, vi->hash_alg->digest_size,
			     want_hash);
		put_page(hpage);
		pr_debug("Verified hash page at level %d, now want hash %*phN\n",
			 level - 1, vi->hash_alg->digest_size, want_hash);
	}

	/* Finally, verify the data page */
	err = hash_page(vi, data_page, real_hash);
	if (err)
		goto out;
	err = compare_hashes(want_hash, real_hash, vi->hash_alg->digest_size,
			     inode, index, -1);
out:
	for (; level > 0; level--)
		put_page(hpages[level - 1]);
	if (err) {
		pr_warn_ratelimited("Error verifying page; ino=%lu, index=%lu (err=%d)\n",
				    inode->i_ino, data_page->index, err);
		return false;
	}
	return true;
}
EXPORT_SYMBOL_GPL(fsverity_verify_page);

/**
 * fsverity_verify_bio - verify a 'read' bio that has just completed
 *
 * Verify the integrity and/or authenticity of a set of pages that have just
 * been read from the file.  The pages are assumed to be pagecache pages.  Pages
 * that fail verification are set to the Error state.  Verification is skipped
 * for pages already in the Error state, e.g. due to fscrypt decryption failure.
 */
void fsverity_verify_bio(struct bio *bio)
{
	struct bio_vec *bv;
	int i;

	bio_for_each_segment_all(bv, bio, i) {
		struct page *page = bv->bv_page;

		if (!PageError(page) && !fsverity_verify_page(page))
			SetPageError(page);
	}
}
EXPORT_SYMBOL_GPL(fsverity_verify_bio);

/**
 * fsverity_enqueue_verify_work - enqueue work on the fs-verity workqueue
 *
 * Enqueue verification work for asynchronous processing.
 */
void fsverity_enqueue_verify_work(struct work_struct *work)
{
	queue_work(fsverity_read_workqueue, work);
}
EXPORT_SYMBOL_GPL(fsverity_enqueue_verify_work);

/**
 * fsverity_full_isize - get the full (on-disk) file size
 *
 * If the inode has had its in-memory ->i_size overridden for fs-verity (to
 * exclude the metadata at the end of the file), then return the full i_size
 * which is stored on-disk.  Otherwise, just return the in-memory ->i_size.
 *
 * Return: the full (on-disk) file size
 */
loff_t fsverity_full_isize(struct inode *inode)
{
	struct fsverity_info *vi = get_fsverity_info(inode);

	if (vi)
		return vi->full_i_size;

	return i_size_read(inode);
}
EXPORT_SYMBOL_GPL(fsverity_full_isize);

/**
 * fsverity_ioctl_set_measurement - set a verity file's expected measurement
 *
 * The FS_IOC_SET_VERITY_MEASUREMENT ioctl informs the kernel which
 * "measurement" (hash of the Merkle tree root hash and the fsverity footer) is
 * expected for an fs-verity file.  If the actual measurement matches the
 * expected one, then reads from the file are allowed and the ioctl succeeds;
 * additionally, the inode is pinned in memory so that the "authenticated"
 * status doesn't get lost on eviction.  Otherwise, reads from the file are
 * forbidden and the ioctl fails.
 *
 * TODO(mhalcrow): In the future, we're going to want to have a signature
 * attached to the files that we validate whenever we load the authenticated
 * data structure root into memory.
 *
 * Return: 0 on success, -errno on failure
 */
int fsverity_ioctl_set_measurement(struct file *filp, const void __user *_uarg)
{
	const struct fsverity_measurement __user *uarg = _uarg;
	struct inode *inode = file_inode(filp);
	struct super_block *sb = inode->i_sb;
	struct fsverity_measurement arg;
	u8 digest[FS_VERITY_MAX_DIGEST_SIZE];
	const struct fsverity_hash_alg *hash_alg;
	struct fsverity_info *vi;
	int err;

	if (!capable(CAP_SYS_ADMIN)) {
		pr_debug("Process '%s' is not authorized to provide fs-verity measurements\n",
			 current->comm);
		err = -EACCES;
		goto out;
	}

	if (copy_from_user(&arg, uarg, sizeof(arg))) {
		err = -EFAULT;
		goto out;
	}

	if (arg.reserved1 ||
	    memchr_inv(arg.reserved2, 0, sizeof(arg.reserved2))) {
		pr_debug("Reserved fields are set in fsverity_measurement\n");
		err = -EINVAL;
		goto out;
	}

	hash_alg = get_hash_algorithm(arg.digest_algorithm);
	if (IS_ERR(hash_alg)) {
		err = PTR_ERR(hash_alg);
		goto out;
	}

	if (arg.digest_size != hash_alg->digest_size) {
		pr_debug("Wrong digest_size in fsverity_measurement: wanted %u for algorithm '%s', but got %u\n",
			 hash_alg->digest_size, hash_alg->name,
			 arg.digest_size);
		err = -EINVAL;
		goto out;
	}

	/* Redundant, but just in case */
	if (WARN_ON(hash_alg->digest_size > sizeof(digest))) {
		err = -EINVAL;
		goto out;
	}

	if (copy_from_user(digest, uarg->digest, hash_alg->digest_size)) {
		err = -EFAULT;
		goto out;
	}

	vi = get_fsverity_info(inode);
	if (!vi) {
		pr_debug("File does not have fs-verity enabled\n");
		err = -EINVAL;
		goto out;
	}

	inode_lock(inode);

	if (hash_alg != vi->hash_alg) {
		pr_debug("Hash algorithm mismatch: provided '%s', but file uses '%s'\n",
			 hash_alg->name, vi->hash_alg->name);
		err = -EBADMSG;
		vi->mode = FS_VERITY_MODE_AUTHENTICATION_FAILED;
		truncate_inode_pages(inode->i_mapping, 0);
		goto out_unlock;
	}

	if (memcmp(digest, vi->measurement, hash_alg->digest_size)) {
		if (vi->mode == FS_VERITY_MODE_AUTHENTICATED)
			pr_warn("Inconsistent measurements were provided! %*phN and %*phN\n",
				hash_alg->digest_size, digest,
				hash_alg->digest_size, vi->measurement);
		else
			pr_warn("File corrupted!  Wanted measurement: %*phN, real measurement: %*phN\n",
				hash_alg->digest_size, digest,
				hash_alg->digest_size, vi->measurement);
		vi->mode = FS_VERITY_MODE_AUTHENTICATION_FAILED;
		truncate_inode_pages(inode->i_mapping, 0);
		err = -EBADMSG;
		goto out_unlock;
	}

	if (vi->mode == FS_VERITY_MODE_AUTHENTICATED) {
		pr_debug("Measurement already being enforced: %*phN\n",
			 hash_alg->digest_size, vi->measurement);
	} else {
		pr_debug("Measurement validated: %*phN\n",
			 hash_alg->digest_size, vi->measurement);
		vi->mode = FS_VERITY_MODE_AUTHENTICATED;

		/* Pin the inode so that the measurement doesn't go away */
		ihold(inode);
		spin_lock(&sb->s_inode_fsveritylist_lock);
		list_add(&inode->i_fsverity_list, &sb->s_inodes_fsverity);
		spin_unlock(&sb->s_inode_fsveritylist_lock);
	}
	err = 0;
out_unlock:
	inode_unlock(inode);
out:
	if (err)
		pr_debug("FS_IOC_SET_VERITY_MEASUREMENT failed on inode %lu (err %d)\n",
			 inode->i_ino, err);
	else
		pr_debug("FS_IOC_SET_VERITY_MEASUREMENT succeeded on inode %lu\n",
			 inode->i_ino);
	return err;
}
EXPORT_SYMBOL_GPL(fsverity_ioctl_set_measurement);

/**
 * fsverity_ioctl_enable - enable fs-verity on a file
 *
 * Enable the fs-verity bit on a file.  Userspace must have already appended the
 * fs-verity metadata to the file.
 *
 * Enabling fs-verity makes the file contents immutable, and the filesystem
 * doesn't allow disabling it (other than by replacing the file).
 *
 * To avoid races with the file contents being modified, no processes must have
 * the file open for writing.  This includes the caller!
 *
 * Return: 0 on success, -errno on failure
 */
int fsverity_ioctl_enable(struct file *filp, const void __user *arg)
{
	struct inode *inode = file_inode(filp);
	struct fsverity_info *vi;
	int err;

	if (!capable(CAP_SYS_ADMIN)) {
		pr_debug("Process '%s' is not authorized to enable fs-verity\n",
			 current->comm);
		err = -EACCES;
		goto out;
	}

	if (arg) {
		pr_debug("FS_IOC_ENABLE_VERITY doesn't take an argument\n");
		err = -EINVAL;
		goto out;
	}

	if (S_ISDIR(inode->i_mode))  {
		pr_debug("Inode is a directory\n");
		err = -EISDIR;
		goto out;
	}

	if (!S_ISREG(inode->i_mode)) {
		pr_debug("Inode is not a regular file\n");
		err = -EINVAL;
		goto out;
	}

	err = mnt_want_write_file(filp);
	if (err)
		goto out;

	/*
	 * Temporarily lock out writers via writable file descriptors or
	 * truncate().  This should stabilize the contents of the file as well
	 * as its size.  Note that at the end of this ioctl we will unlock
	 * writers, but at that point the fs-verity bit will be set (if the
	 * ioctl succeeded), preventing future writers.
	 */
	err = deny_write_access(filp);
	if (err) {
		pr_debug("File is open for writing!\n");
		goto out_drop_write;
	}

	/*
	 * fsync so that the fs-verity bit can't be persisted to disk prior to
	 * the data, causing verification errors after a crash.
	 */
	err = vfs_fsync(filp, 1);
	if (err) {
		pr_debug("I/O error occurred during fsync\n");
		goto out_allow_write;
	}

	/* Serialize concurrent use of this ioctl on the same inode */
	inode_lock(inode);

	if (inode->i_sb->s_vop->is_verity(inode)) {
		pr_debug("Fs-verity is already enabled on this file\n");
		err = -EEXIST;
		goto out_unlock;
	}

	/* Validate the fs-verity footer */
	vi = create_fsverity_info(inode);
	if (IS_ERR(vi)) {
		pr_debug("create_fsverity_info() failed\n");
		err = PTR_ERR(vi);
		goto out_unlock;
	}

	/* Set the fs-verity bit */
	err = inode->i_sb->s_vop->set_verity(inode);
	if (err) {
		pr_debug("Filesystem ->set_verity() method failed\n");
		goto out_free_vi;
	}

	/* Invalidate all cached pages, forcing re-verification */
	truncate_inode_pages(inode->i_mapping, 0);

	/* Set ->i_verity_info */
	if (set_fsverity_info(inode, vi))
		vi = NULL;
	err = 0;
out_free_vi:
	free_fsverity_info(vi);
out_unlock:
	inode_unlock(inode);
out_allow_write:
	allow_write_access(filp);
out_drop_write:
	mnt_drop_write_file(filp);
out:
	if (err)
		pr_debug("Failed to enable fs-verity on inode %lu (err=%d)\n",
			 inode->i_ino, err);
	else
		pr_debug("Successfully enabled fs-verity on inode %lu\n",
			 inode->i_ino);
	return err;
}
EXPORT_SYMBOL_GPL(fsverity_ioctl_enable);

static int __init fsverity_module_init(void)
{
	int err;
	int i;

	/*
	 * Use an unbound workqueue to better distribute verification work among
	 * multiple CPUs.
	 *
	 * Use a high-priority workqueue to prioritize verification work, which
	 * blocks reads from completing, over regular application tasks.
	 */
	err = -ENOMEM;
	fsverity_read_workqueue = alloc_workqueue("fsverity_read_queue",
						  WQ_CPU_INTENSIVE |
							WQ_HIGHPRI | WQ_UNBOUND,
						  num_online_cpus());
	if (!fsverity_read_workqueue)
		goto error;

	err = -ENOMEM;
	fsverity_info_cachep = KMEM_CACHE(fsverity_info, SLAB_RECLAIM_ACCOUNT);
	if (!fsverity_info_cachep)
		goto error_free_workqueue;

	/*
	 * Sanity check the digest sizes (could be a build-time check, but
	 * they're in an array)
	 */
	for (i = 0; i < ARRAY_SIZE(fsverity_hash_algs); i++) {
		struct fsverity_hash_alg *alg = &fsverity_hash_algs[i];

		if (!alg->name)
			continue;
		BUG_ON(alg->digest_size > FS_VERITY_MAX_DIGEST_SIZE);
		BUG_ON(!is_power_of_2(alg->digest_size));
	}

	pr_debug("Initialized fs-verity\n");
	return 0;

error_free_workqueue:
	destroy_workqueue(fsverity_read_workqueue);
error:
	return err;
}

static void __exit fsverity_module_exit(void)
{
	int i;

	destroy_workqueue(fsverity_read_workqueue);
	kmem_cache_destroy(fsverity_info_cachep);
	for (i = 0; i < ARRAY_SIZE(fsverity_hash_algs); i++)
		crypto_free_shash(fsverity_hash_algs[i].tfm);
}

module_init(fsverity_module_init)
module_exit(fsverity_module_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("fs-verity: read-only file-based integrity/authenticity");
