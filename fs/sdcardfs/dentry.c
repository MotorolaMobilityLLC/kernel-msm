/*
 * fs/sdcardfs/dentry.c
 *
 * Copyright (c) 2013 Samsung Electronics Co. Ltd
 *   Authors: Daeho Jeong, Woojoong Lee, Seunghwan Hyun,
 *               Sunghwan Yun, Sungjong Seo
 *
 * This program has been developed as a stackable file system based on
 * the WrapFS which written by
 *
 * Copyright (c) 1998-2011 Erez Zadok
 * Copyright (c) 2009     Shrikar Archak
 * Copyright (c) 2003-2011 Stony Brook University
 * Copyright (c) 2003-2011 The Research Foundation of SUNY
 *
 * This file is dual licensed.  It may be redistributed and/or modified
 * under the terms of the Apache 2.0 License OR version 2 of the GNU
 * General Public License.
 */

#include "sdcardfs.h"
#include "linux/ctype.h"
#ifdef CONFIG_SDCARD_FS_DIR_WRITER
#include <linux/xattr.h>
#endif

/*
 * returns: -ERRNO if error (returned to user)
 *          0: tell VFS to invalidate dentry
 *          1: dentry is valid
 */
static int sdcardfs_d_revalidate(struct dentry *dentry, unsigned int flags)
{
	int err = 1;
	struct path parent_lower_path, lower_path;
	struct dentry *parent_dentry = NULL;
	struct dentry *parent_lower_dentry = NULL;
	struct dentry *lower_cur_parent_dentry = NULL;
	struct dentry *lower_dentry = NULL;
	struct inode *inode;
	struct sdcardfs_inode_data *data;

	if (flags & LOOKUP_RCU)
		return -ECHILD;

	spin_lock(&dentry->d_lock);
	if (IS_ROOT(dentry)) {
		spin_unlock(&dentry->d_lock);
		return 1;
	}
	spin_unlock(&dentry->d_lock);

	/* check uninitialized obb_dentry and
	 * whether the base obbpath has been changed or not
	 */
	if (is_obbpath_invalid(dentry)) {
		return 0;
	}

	parent_dentry = dget_parent(dentry);
	sdcardfs_get_lower_path(parent_dentry, &parent_lower_path);
	sdcardfs_get_real_lower(dentry, &lower_path);
	parent_lower_dentry = parent_lower_path.dentry;
	lower_dentry = lower_path.dentry;
	lower_cur_parent_dentry = dget_parent(lower_dentry);

	if ((lower_dentry->d_flags & DCACHE_OP_REVALIDATE)) {
		err = lower_dentry->d_op->d_revalidate(lower_dentry, flags);
		if (err == 0) {
			goto out;
		}
	}

	spin_lock(&lower_dentry->d_lock);
	if (d_unhashed(lower_dentry)) {
		spin_unlock(&lower_dentry->d_lock);
		err = 0;
		goto out;
	}
	spin_unlock(&lower_dentry->d_lock);

	if (parent_lower_dentry != lower_cur_parent_dentry) {
		err = 0;
		goto out;
	}

	if (dentry < lower_dentry) {
		spin_lock(&dentry->d_lock);
		spin_lock_nested(&lower_dentry->d_lock, DENTRY_D_LOCK_NESTED);
	} else {
		spin_lock(&lower_dentry->d_lock);
		spin_lock_nested(&dentry->d_lock, DENTRY_D_LOCK_NESTED);
	}

	if (!qstr_case_eq(&dentry->d_name, &lower_dentry->d_name)) {
		err = 0;
	}

	if (dentry < lower_dentry) {
		spin_unlock(&lower_dentry->d_lock);
		spin_unlock(&dentry->d_lock);
	} else {
		spin_unlock(&dentry->d_lock);
		spin_unlock(&lower_dentry->d_lock);
	}
	if (!err)
		goto out;

	/* If our top's inode is gone, we may be out of date */
	inode = igrab(d_inode(dentry));
	if (inode) {
		data = top_data_get(SDCARDFS_I(inode));
		if (!data || data->abandoned) {
			err = 0;
		}
		if (data)
			data_put(data);
		iput(inode);
	}

out:
	dput(parent_dentry);
	dput(lower_cur_parent_dentry);
	sdcardfs_put_lower_path(parent_dentry, &parent_lower_path);
	sdcardfs_put_real_lower(dentry, &lower_path);
	return err;
}

/* 1 = delete, 0 = cache */
static int sdcardfs_d_delete(const struct dentry *d)
{
	return SDCARDFS_SB(d->d_sb)->options.nocache ? 1 : 0;
}

static void sdcardfs_d_release(struct dentry *dentry)
{
	if (!dentry || !dentry->d_fsdata)
		return;
	/* release and reset the lower paths */
	if (has_graft_path(dentry))
		sdcardfs_put_reset_orig_path(dentry);
	sdcardfs_put_reset_lower_path(dentry);
	free_dentry_private_data(dentry);
}

static int sdcardfs_hash_ci(const struct dentry *dentry,
				struct qstr *qstr)
{
	/*
	 * This function is copy of vfat_hashi.
	 * FIXME Should we support national language?
	 *       Refer to vfat_hashi()
	 * struct nls_table *t = MSDOS_SB(dentry->d_sb)->nls_io;
	 */
	const unsigned char *name;
	unsigned int len;
	unsigned long hash;

	name = qstr->name;
	len = qstr->len;

	hash = init_name_hash(dentry);
	while (len--)
		hash = partial_name_hash(tolower(*name++), hash);
	qstr->hash = end_name_hash(hash);

	return 0;
}

/*
 * Case insensitive compare of two vfat names.
 */
static int sdcardfs_cmp_ci(const struct dentry *dentry,
		unsigned int len, const char *str, const struct qstr *name)
{
	/* FIXME Should we support national language? */

	if (name->len == len) {
		if (str_n_case_eq(name->name, str, len))
			return 0;
	}
	return 1;
}

static void sdcardfs_canonical_path(const struct path *path,
				struct path *actual_path)
{
	sdcardfs_get_real_lower(path->dentry, actual_path);
}

#ifdef CONFIG_SDCARD_FS_DIR_WRITER
void sdcardfs_update_xattr_dirwriter(struct dentry *lower_dentry,
	uid_t writer_uid)
{
	static char xattr_val[256];
	struct dentry *dentry, *parent;
	const char *dir_name[2];
	int xlen, depth;
	const char *xattr_feat_name = "user.dwriter";
	const char *xattr_name = "user.dwriter.name";
	struct dentry *xdentry = NULL, *child = NULL;
	appid_t app_id = uid_is_app(writer_uid) ?
		writer_uid % AID_USER_OFFSET : 0;

	dentry = lower_dentry;
	if (IS_ERR_OR_NULL(dentry) || !app_id)
		return;

	while (1) {
		parent = dget_parent(dentry);
		xlen = vfs_getxattr(parent, xattr_feat_name, (void *)xattr_val,
			sizeof(xattr_val));
		if (xlen > 0 && xattr_val[0] != '0') {
			dput(parent);
			xattr_val[xlen] = 0;
			dir_name[0] = dentry->d_name.name;
			if (child)
				dir_name[1] = child->d_name.name;
			depth = wildcard_path_match(xattr_val, dir_name,
				child ? 2 : 1);
			if (depth == 1)
				xdentry = dentry;
			else if (depth == 2)
				xdentry = child;
			break;
		} else if (IS_ROOT(parent)) {
			dput(parent);
			break;
		}
		child = dentry;
		dentry = parent;
		dput(parent);
	}

	if (IS_ERR_OR_NULL(xdentry) ||
		!S_ISDIR(d_inode(xdentry)->i_mode))
		return;

	dget(xdentry);
	memset(xattr_val, 0, sizeof(xattr_val));
	vfs_getxattr(xdentry, xattr_name,
		(void *)xattr_val, sizeof(xattr_val));
	xattr_val[sizeof(xattr_val) - 1] = 0;
	if (!strncmp(xattr_val, "overrun;", sizeof(xattr_val)))
		goto out_unlock;
	xlen = add_app_name_to_list(app_id, xattr_val,
		sizeof(xattr_val));
	if (xlen == 0)
		goto out_unlock;
	else if (xlen < 0)
		snprintf(xattr_val, sizeof(xattr_val), "%s", "overrun;");

	if (vfs_setxattr(xdentry, xattr_name, xattr_val,
		strlen(xattr_val), 0)) {
		pr_err("sdcardfs: failed to set %lu %s=%s\n",
			d_inode(xdentry)->i_ino, xattr_name, xattr_val);
		goto out_unlock;
	}

	pr_info("sdcardfs: set %lu %s=%s\n",
		d_inode(xdentry)->i_ino, xattr_name, xattr_val);
out_unlock:
	dput(xdentry);
}
#endif

const struct dentry_operations sdcardfs_ci_dops = {
	.d_revalidate	= sdcardfs_d_revalidate,
	.d_delete	= sdcardfs_d_delete,
	.d_release	= sdcardfs_d_release,
	.d_hash	= sdcardfs_hash_ci,
	.d_compare	= sdcardfs_cmp_ci,
	.d_canonical_path = sdcardfs_canonical_path,
};

