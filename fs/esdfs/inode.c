/*
 * Copyright (c) 1998-2014 Erez Zadok
 * Copyright (c) 2009	   Shrikar Archak
 * Copyright (c) 2003-2014 Stony Brook University
 * Copyright (c) 2003-2014 The Research Foundation of SUNY
 * Copyright (C) 2013-2014 Motorola Mobility, LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "esdfs.h"

static int esdfs_create(struct inode *dir, struct dentry *dentry,
			 umode_t mode, bool want_excl)
{
	int err;
	struct dentry *lower_dentry;
	struct dentry *lower_parent_dentry = NULL;
	struct path lower_path;
	int mask;
	const struct cred *creds =
			esdfs_override_creds(ESDFS_SB(dir->i_sb), &mask);
	if (!creds)
		return -ENOMEM;

	esdfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_parent_dentry = lock_parent(lower_dentry);

	esdfs_set_lower_mode(ESDFS_SB(dir->i_sb), &mode);

	err = vfs_create(lower_parent_dentry->d_inode, lower_dentry, mode,
			 want_excl);
	if (err)
		goto out;

	err = esdfs_interpose(dentry, dir->i_sb, &lower_path);
	if (err)
		goto out;
	fsstack_copy_attr_times(dir, esdfs_lower_inode(dir));
	fsstack_copy_inode_size(dir, lower_parent_dentry->d_inode);

out:
	unlock_dir(lower_parent_dentry);
	esdfs_put_lower_path(dentry, &lower_path);
	esdfs_revert_creds(creds, &mask);
	return err;
}

static int esdfs_unlink(struct inode *dir, struct dentry *dentry)
{
	int err;
	struct dentry *lower_dentry;
	struct inode *lower_dir_inode = esdfs_lower_inode(dir);
	struct dentry *lower_dir_dentry;
	struct path lower_path;
	const struct cred *creds =
			esdfs_override_creds(ESDFS_SB(dir->i_sb), NULL);
	if (!creds)
		return -ENOMEM;

	esdfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	dget(lower_dentry);
	lower_dir_dentry = lock_parent(lower_dentry);

	err = vfs_unlink(lower_dir_inode, lower_dentry, NULL);

	/*
	 * Note: unlinking on top of NFS can cause silly-renamed files.
	 * Trying to delete such files results in EBUSY from NFS
	 * below.  Silly-renamed files will get deleted by NFS later on, so
	 * we just need to detect them here and treat such EBUSY errors as
	 * if the upper file was successfully deleted.
	 */
	if (err == -EBUSY && lower_dentry->d_flags & DCACHE_NFSFS_RENAMED)
		err = 0;
	if (err)
		goto out;
	fsstack_copy_attr_times(dir, lower_dir_inode);
	fsstack_copy_inode_size(dir, lower_dir_inode);
	set_nlink(dentry->d_inode,
		  esdfs_lower_inode(dentry->d_inode)->i_nlink);
	dentry->d_inode->i_ctime = dir->i_ctime;
	d_drop(dentry); /* this is needed, else LTP fails (VFS won't do it) */
out:
	unlock_dir(lower_dir_dentry);
	dput(lower_dentry);
	esdfs_put_lower_path(dentry, &lower_path);
	esdfs_revert_creds(creds, NULL);
	return err;
}

static int esdfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	int err;
	struct dentry *lower_dentry;
	struct dentry *lower_parent_dentry = NULL;
	struct path lower_path;
	int mask;
	const struct cred *creds =
			esdfs_override_creds(ESDFS_SB(dir->i_sb), &mask);
	if (!creds)
		return -ENOMEM;

	esdfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_parent_dentry = lock_parent(lower_dentry);

	mode |= S_IFDIR;
	esdfs_set_lower_mode(ESDFS_SB(dir->i_sb), &mode);

	err = vfs_mkdir(lower_parent_dentry->d_inode, lower_dentry, mode);
	if (err)
		goto out;

	err = esdfs_interpose(dentry, dir->i_sb, &lower_path);
	if (err)
		goto out;

	fsstack_copy_attr_times(dir, esdfs_lower_inode(dir));
	fsstack_copy_inode_size(dir, lower_parent_dentry->d_inode);
	/* update number of links on parent directory */
	set_nlink(dir, esdfs_lower_inode(dir)->i_nlink);

out:
	unlock_dir(lower_parent_dentry);
	esdfs_put_lower_path(dentry, &lower_path);
	esdfs_revert_creds(creds, &mask);
	return err;
}

static int esdfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct dentry *lower_dentry;
	struct dentry *lower_dir_dentry;
	int err;
	struct path lower_path;
	const struct cred *creds =
			esdfs_override_creds(ESDFS_SB(dir->i_sb), NULL);
	if (!creds)
		return -ENOMEM;

	esdfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_dir_dentry = lock_parent(lower_dentry);

	err = vfs_rmdir(lower_dir_dentry->d_inode, lower_dentry);
	if (err)
		goto out;

	d_drop(dentry);	/* drop our dentry on success (why not VFS's job?) */
	if (dentry->d_inode)
		clear_nlink(dentry->d_inode);
	fsstack_copy_attr_times(dir, lower_dir_dentry->d_inode);
	fsstack_copy_inode_size(dir, lower_dir_dentry->d_inode);
	set_nlink(dir, lower_dir_dentry->d_inode->i_nlink);

out:
	unlock_dir(lower_dir_dentry);
	esdfs_put_lower_path(dentry, &lower_path);
	esdfs_revert_creds(creds, NULL);
	return err;
}

/*
 * The locking rules in esdfs_rename are complex.  We could use a simpler
 * superblock-level name-space lock for renames and copy-ups.
 */
static int esdfs_rename(struct inode *old_dir, struct dentry *old_dentry,
			struct inode *new_dir, struct dentry *new_dentry)
{
	int err = 0;
	struct dentry *lower_old_dentry = NULL;
	struct dentry *lower_new_dentry = NULL;
	struct dentry *lower_old_dir_dentry = NULL;
	struct dentry *lower_new_dir_dentry = NULL;
	struct dentry *trap = NULL;
	struct path lower_old_path, lower_new_path;
	int mask;
	const struct cred *creds =
			esdfs_override_creds(ESDFS_SB(old_dir->i_sb), &mask);
	if (!creds)
		return -ENOMEM;

	esdfs_get_lower_path(old_dentry, &lower_old_path);
	esdfs_get_lower_path(new_dentry, &lower_new_path);
	lower_old_dentry = lower_old_path.dentry;
	lower_new_dentry = lower_new_path.dentry;
	lower_old_dir_dentry = dget_parent(lower_old_dentry);
	lower_new_dir_dentry = dget_parent(lower_new_dentry);

	trap = lock_rename(lower_old_dir_dentry, lower_new_dir_dentry);
	/* source should not be ancestor of target */
	if (trap == lower_old_dentry) {
		err = -EINVAL;
		goto out;
	}
	/* target should not be ancestor of source */
	if (trap == lower_new_dentry) {
		err = -ENOTEMPTY;
		goto out;
	}

	err = vfs_rename(lower_old_dir_dentry->d_inode, lower_old_dentry,
			 lower_new_dir_dentry->d_inode, lower_new_dentry,
			 NULL, 0);
	if (err)
		goto out;

	esdfs_copy_attr(new_dir, lower_new_dir_dentry->d_inode);
	fsstack_copy_inode_size(new_dir, lower_new_dir_dentry->d_inode);
	if (new_dir != old_dir) {
		esdfs_copy_attr(old_dir,
				lower_old_dir_dentry->d_inode);
		fsstack_copy_inode_size(old_dir,
					lower_old_dir_dentry->d_inode);
	}

out:
	unlock_rename(lower_old_dir_dentry, lower_new_dir_dentry);
	dput(lower_old_dir_dentry);
	dput(lower_new_dir_dentry);
	esdfs_put_lower_path(old_dentry, &lower_old_path);
	esdfs_put_lower_path(new_dentry, &lower_new_path);
	esdfs_revert_creds(creds, &mask);
	return err;
}

static int esdfs_permission(struct inode *inode, int mask)
{
	struct inode *lower_inode;
	int err;
	int oldmask;
	const struct cred *creds =
			esdfs_override_creds(ESDFS_SB(inode->i_sb), &oldmask);
	if (!creds)
		return -ENOMEM;

	lower_inode = esdfs_lower_inode(inode);
	err = inode_permission(lower_inode, mask);

	esdfs_revert_creds(creds, &oldmask);
	return err;
}

static int esdfs_setattr(struct dentry *dentry, struct iattr *ia)
{
	int err;
	struct dentry *lower_dentry;
	struct inode *inode;
	struct inode *lower_inode;
	struct path lower_path;
	struct iattr lower_ia;

	/* We don't allow chmod or chown, so skip those */
	ia->ia_valid &= ~(ATTR_UID | ATTR_GID | ATTR_MODE);
	if (!ia->ia_valid)
		return 0;

	inode = dentry->d_inode;

	/*
	 * Check if user has permission to change inode.  We don't check if
	 * this user can change the lower inode: that should happen when
	 * calling notify_change on the lower inode.
	 */
	err = inode_change_ok(inode, ia);
	if (err)
		goto out_err;

	esdfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_inode = esdfs_lower_inode(inode);

	/* prepare our own lower struct iattr (with the lower file) */
	memcpy(&lower_ia, ia, sizeof(lower_ia));
	if (ia->ia_valid & ATTR_FILE)
		lower_ia.ia_file = esdfs_lower_file(ia->ia_file);

	/*
	 * If shrinking, first truncate upper level to cancel writing dirty
	 * pages beyond the new eof; and also if its' maxbytes is more
	 * limiting (fail with -EFBIG before making any change to the lower
	 * level).  There is no need to vmtruncate the upper level
	 * afterwards in the other cases: we fsstack_copy_inode_size from
	 * the lower level.
	 */
	if (ia->ia_valid & ATTR_SIZE) {
		err = inode_newsize_ok(inode, ia->ia_size);
		if (err)
			goto out;
		truncate_setsize(inode, ia->ia_size);
	}

	/*
	 * mode change is for clearing setuid/setgid bits. Allow lower fs
	 * to interpret this in its own way.
	 */
	if (lower_ia.ia_valid & (ATTR_KILL_SUID | ATTR_KILL_SGID))
		lower_ia.ia_valid &= ~ATTR_MODE;

	/* notify the (possibly copied-up) lower inode */
	/*
	 * Note: we use lower_dentry->d_inode, because lower_inode may be
	 * unlinked (no inode->i_sb and i_ino==0.  This happens if someone
	 * tries to open(), unlink(), then ftruncate() a file.
	 */
	mutex_lock(&lower_dentry->d_inode->i_mutex);
	err = notify_change(lower_dentry, &lower_ia, /* note: lower_ia */
			    NULL);
	mutex_unlock(&lower_dentry->d_inode->i_mutex);
	if (err)
		goto out;

	/* get attributes from the lower inode */
	esdfs_copy_attr(inode, lower_inode);
	/*
	 * Not running fsstack_copy_inode_size(inode, lower_inode), because
	 * VFS should update our inode size, and notify_change on
	 * lower_inode should update its size.
	 */

out:
	esdfs_put_lower_path(dentry, &lower_path);
out_err:
	return err;
}

static int esdfs_getattr(struct vfsmount *mnt, struct dentry *dentry,
			 struct kstat *stat)
{
	struct dentry *lower_dentry;
	struct inode *inode;
	struct inode *lower_inode;
	struct path lower_path;

	inode = dentry->d_inode;

	esdfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_inode = esdfs_lower_inode(inode);

	esdfs_copy_attr(inode, lower_inode);

	generic_fillattr(inode, stat);
	esdfs_put_lower_path(dentry, &lower_path);
	return 0;
}

const struct inode_operations esdfs_symlink_iops = {
	.permission     = esdfs_permission,
	.setattr	= esdfs_setattr,
	.getattr	= esdfs_getattr,
};

const struct inode_operations esdfs_dir_iops = {
	.create		= esdfs_create,
	.lookup		= esdfs_lookup,
	.unlink		= esdfs_unlink,
	.mkdir		= esdfs_mkdir,
	.rmdir		= esdfs_rmdir,
	.rename		= esdfs_rename,
	.permission     = esdfs_permission,
	.setattr	= esdfs_setattr,
	.getattr	= esdfs_getattr,
};

const struct inode_operations esdfs_main_iops = {
	.permission     = esdfs_permission,
	.setattr	= esdfs_setattr,
	.getattr	= esdfs_getattr,
};
