/*
 * Copyright (c) 1998-2014 Erez Zadok
 * Copyright (c) 2009	   Shrikar Archak
 * Copyright (c) 2003-2014 Stony Brook University
 * Copyright (c) 2003-2014 The Research Foundation of SUNY
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

/* the file system name */
#define ESDFS_NAME "esdfs"

/* esdfs root inode number */
#define ESDFS_ROOT_INO     1

/* useful for tracking code reachability */
#define UDBG printk(KERN_DEFAULT "DBG:%s:%s:%d\n", __FILE__, __func__, __LINE__)

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

/* esdfs super-block data in memory */
struct esdfs_sb_info {
	struct super_block *lower_sb;
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
#endif	/* not _ESDFS_H_ */
