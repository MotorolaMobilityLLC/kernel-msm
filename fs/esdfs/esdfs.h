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

#ifndef _ESDFS_H_
#define _ESDFS_H_

#include <linux/dcache.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/aio.h>
#include <linux/mm.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/seq_file.h>
#include <linux/statfs.h>
#include <linux/fs_stack.h>
#include <linux/magic.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/fs_struct.h>

/* the file system name */
#define ESDFS_NAME "esdfs"

/* esdfs root inode number */
#define ESDFS_ROOT_INO     1

/* useful for tracking code reachability */
#define UDBG printk(KERN_DEFAULT "DBG:%s:%s:%d\n", __FILE__, __func__, __LINE__)

/* default permissions as specified by the Android sdcard service */
#define ESDFS_DEFAULT_LOWER_UID		1023	/* AID_MEDIA_RW */
#define ESDFS_DEFAULT_LOWER_GID		1023	/* AID_MEDIA_RW */
#define ESDFS_DEFAULT_LOWER_FMASK	0664
#define ESDFS_DEFAULT_LOWER_DMASK	0775
#define ESDFS_DEFAULT_UPPER_UID		0	/* AID_ROOT */
#define ESDFS_DEFAULT_UPPER_GID		1015	/* AID_SDCARD_RW */
#define ESDFS_DEFAULT_UPPER_FMASK	0664
#define ESDFS_DEFAULT_UPPER_DMASK	0775

/* operations vectors defined in specific files */
extern const struct file_operations esdfs_main_fops;
extern const struct file_operations esdfs_dir_fops;
extern const struct inode_operations esdfs_main_iops;
extern const struct inode_operations esdfs_dir_iops;
extern const struct inode_operations esdfs_symlink_iops;
extern const struct super_operations esdfs_sops;
extern const struct dentry_operations esdfs_dops;
extern const struct address_space_operations esdfs_aops, esdfs_dummy_aops;
extern const struct vm_operations_struct esdfs_vm_ops;

extern int esdfs_init_inode_cache(void);
extern void esdfs_destroy_inode_cache(void);
extern int esdfs_init_dentry_cache(void);
extern void esdfs_destroy_dentry_cache(void);
extern int new_dentry_private_data(struct dentry *dentry);
extern void free_dentry_private_data(struct dentry *dentry);
extern struct dentry *esdfs_lookup(struct inode *dir, struct dentry *dentry,
				    unsigned int flags);
extern struct inode *esdfs_iget(struct super_block *sb,
				 struct inode *lower_inode);
extern int esdfs_interpose(struct dentry *dentry, struct super_block *sb,
			    struct path *lower_path);

/* file private data */
struct esdfs_file_info {
	struct file *lower_file;
	const struct vm_operations_struct *lower_vm_ops;
};

/* esdfs inode data in memory */
struct esdfs_inode_info {
	struct inode *lower_inode;
	struct inode vfs_inode;
};

/* esdfs dentry data in memory */
struct esdfs_dentry_info {
	spinlock_t lock;	/* protects lower_path */
	struct path lower_path;
};

struct esdfs_perms {
	uid_t uid;
	gid_t gid;
	unsigned short fmask;
	unsigned short dmask;
};

/* esdfs super-block data in memory */
struct esdfs_sb_info {
	struct super_block *lower_sb;
	struct esdfs_perms lower_perms;
	struct esdfs_perms upper_perms;
	unsigned int options;
};

/*
 * inode to private data
 *
 * Since we use containers and the struct inode is _inside_ the
 * esdfs_inode_info structure, ESDFS_I will always (given a non-NULL
 * inode pointer), return a valid non-NULL pointer.
 */
static inline struct esdfs_inode_info *ESDFS_I(const struct inode *inode)
{
	return container_of(inode, struct esdfs_inode_info, vfs_inode);
}

/* dentry to private data */
#define ESDFS_D(dent) ((struct esdfs_dentry_info *)(dent)->d_fsdata)

/* superblock to private data */
#define ESDFS_SB(super) ((struct esdfs_sb_info *)(super)->s_fs_info)

/* file to private Data */
#define ESDFS_F(file) ((struct esdfs_file_info *)((file)->private_data))

/* file to lower file */
static inline struct file *esdfs_lower_file(const struct file *f)
{
	return ESDFS_F(f)->lower_file;
}

static inline void esdfs_set_lower_file(struct file *f, struct file *val)
{
	ESDFS_F(f)->lower_file = val;
}

/* inode to lower inode. */
static inline struct inode *esdfs_lower_inode(const struct inode *i)
{
	return ESDFS_I(i)->lower_inode;
}

static inline void esdfs_set_lower_inode(struct inode *i, struct inode *val)
{
	ESDFS_I(i)->lower_inode = val;
}

/* superblock to lower superblock */
static inline struct super_block *esdfs_lower_super(
	const struct super_block *sb)
{
	return ESDFS_SB(sb)->lower_sb;
}

static inline void esdfs_set_lower_super(struct super_block *sb,
					  struct super_block *val)
{
	ESDFS_SB(sb)->lower_sb = val;
}

/* path based (dentry/mnt) macros */
static inline void pathcpy(struct path *dst, const struct path *src)
{
	dst->dentry = src->dentry;
	dst->mnt = src->mnt;
}
/* Returns struct path.  Caller must path_put it. */
static inline void esdfs_get_lower_path(const struct dentry *dent,
					 struct path *lower_path)
{
	spin_lock(&ESDFS_D(dent)->lock);
	pathcpy(lower_path, &ESDFS_D(dent)->lower_path);
	path_get(lower_path);
	spin_unlock(&ESDFS_D(dent)->lock);
	return;
}
static inline void esdfs_put_lower_path(const struct dentry *dent,
					 struct path *lower_path)
{
	path_put(lower_path);
	return;
}
static inline void esdfs_set_lower_path(const struct dentry *dent,
					 struct path *lower_path)
{
	spin_lock(&ESDFS_D(dent)->lock);
	pathcpy(&ESDFS_D(dent)->lower_path, lower_path);
	spin_unlock(&ESDFS_D(dent)->lock);
	return;
}
static inline void esdfs_reset_lower_path(const struct dentry *dent)
{
	spin_lock(&ESDFS_D(dent)->lock);
	ESDFS_D(dent)->lower_path.dentry = NULL;
	ESDFS_D(dent)->lower_path.mnt = NULL;
	spin_unlock(&ESDFS_D(dent)->lock);
	return;
}
static inline void esdfs_put_reset_lower_path(const struct dentry *dent)
{
	struct path lower_path;
	spin_lock(&ESDFS_D(dent)->lock);
	pathcpy(&lower_path, &ESDFS_D(dent)->lower_path);
	ESDFS_D(dent)->lower_path.dentry = NULL;
	ESDFS_D(dent)->lower_path.mnt = NULL;
	spin_unlock(&ESDFS_D(dent)->lock);
	path_put(&lower_path);
	return;
}

/* locking helpers */
static inline struct dentry *lock_parent(struct dentry *dentry)
{
	struct dentry *dir = dget_parent(dentry);
	mutex_lock_nested(&dir->d_inode->i_mutex, I_MUTEX_PARENT);
	return dir;
}

static inline void unlock_dir(struct dentry *dir)
{
	mutex_unlock(&dir->d_inode->i_mutex);
	dput(dir);
}

static inline void esdfs_set_lower_mode(struct esdfs_sb_info *sbi,
		umode_t *mode)
{
	if (S_ISDIR(*mode))
		*mode = (*mode & S_IFMT) | sbi->lower_perms.dmask;
	else
		*mode = (*mode & S_IFMT) | sbi->lower_perms.fmask;
}

static inline void esdfs_lower_i_perms(struct inode *inode)
{
	struct esdfs_sb_info *sbi = ESDFS_SB(inode->i_sb);
	
	i_uid_write(inode, sbi->lower_perms.uid);
	i_gid_write(inode, sbi->lower_perms.gid);
	esdfs_set_lower_mode(sbi, &inode->i_mode);
}

/* file attribute helpers */
static inline void esdfs_copy_lower_attr(struct inode *dest,
					 const struct inode *src)
{
	dest->i_mode = src->i_mode & S_IFMT;
	dest->i_rdev = src->i_rdev;
	dest->i_atime = src->i_atime;
	dest->i_mtime = src->i_mtime;
	dest->i_ctime = src->i_ctime;
	dest->i_blkbits = src->i_blkbits;
	dest->i_flags = src->i_flags;
	set_nlink(dest, src->i_nlink);
}

static inline void esdfs_copy_attr(struct inode *dest, const struct inode *src)
{
	struct esdfs_sb_info *sbi = ESDFS_SB(dest->i_sb);

	esdfs_copy_lower_attr(dest, src);
	i_uid_write(dest, sbi->upper_perms.uid);
	i_gid_write(dest, sbi->upper_perms.gid);
	if (S_ISDIR(dest->i_mode))
		dest->i_mode |= sbi->upper_perms.dmask;
	else
		dest->i_mode |= sbi->upper_perms.fmask;
}

/*
 * Based on nfs4_save_creds() and nfs4_reset_creds() in nfsd/nfs4recover.c.
 * Returns NULL if prepare_creds() could not allocate heap, otherwise
 */
static inline const struct cred *esdfs_override_creds(
		struct esdfs_sb_info *sbi, int *mask)
{
	struct cred *creds = prepare_creds();

	if (!creds)
		return NULL;

	/* clear the umask so that the lower mode works for create cases */
	if (mask) {
		*mask = 0;
		*mask = xchg(&current->fs->umask, *mask & S_IRWXUGO);
	}

	creds->fsuid = make_kuid(&init_user_ns, sbi->lower_perms.uid);
	creds->fsgid = make_kgid(&init_user_ns, sbi->lower_perms.gid);

	/* this installs the new creds into current, which we must destroy */
	return override_creds(creds);
}

static inline void esdfs_revert_creds(const struct cred *creds, int *mask)
{
	const struct cred *current_creds = current->cred;

	/* restore the old umask */
	if (mask)
		*mask = xchg(&current->fs->umask, *mask & S_IRWXUGO);

	/* restore the old creds into current */
	revert_creds(creds);
	put_cred(current_creds);	/* destroy the old temporary creds */
}

#endif	/* not _ESDFS_H_ */
