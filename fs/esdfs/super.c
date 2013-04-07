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

#include "esdfs.h"

/*
 * The inode cache is used with alloc_inode for both our inode info and the
 * vfs inode.
 */
static struct kmem_cache *esdfs_inode_cachep;

/* final actions when unmounting a file system */
static void esdfs_put_super(struct super_block *sb)
{
	struct esdfs_sb_info *spd;
	struct super_block *s;

	spd = ESDFS_SB(sb);
	if (!spd)
		return;

	/* decrement lower super references */
	s = esdfs_lower_super(sb);
	esdfs_set_lower_super(sb, NULL);
	atomic_dec(&s->s_active);

	kfree(spd);
	sb->s_fs_info = NULL;
}

static int esdfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	int err;
	struct path lower_path;

	esdfs_get_lower_path(dentry, &lower_path);
	err = vfs_statfs(&lower_path, buf);
	esdfs_put_lower_path(dentry, &lower_path);

	/* set return buf to our f/s to avoid confusing user-level utils */
	buf->f_type = ESDFS_SUPER_MAGIC;

	return err;
}

/*
 * @flags: numeric mount options
 * @options: mount options string
 */
static int esdfs_remount_fs(struct super_block *sb, int *flags, char *options)
{
	int err = 0;

	/*
	 * The VFS will take care of "ro" and "rw" flags among others.  We
	 * can safely accept a few flags (RDONLY, MANDLOCK), and honor
	 * SILENT, but anything else left over is an error.
	 */
	if ((*flags & ~(MS_RDONLY | MS_MANDLOCK | MS_SILENT)) != 0) {
		printk(KERN_ERR
		       "esdfs: remount flags 0x%x unsupported\n", *flags);
		err = -EINVAL;
	}

	return err;
}

/*
 * Called by iput() when the inode reference count reached zero
 * and the inode is not hashed anywhere.  Used to clear anything
 * that needs to be, before the inode is completely destroyed and put
 * on the inode free list.
 */
static void esdfs_evict_inode(struct inode *inode)
{
	struct inode *lower_inode;

	truncate_inode_pages(&inode->i_data, 0);
	clear_inode(inode);
	/*
	 * Decrement a reference to a lower_inode, which was incremented
	 * by our read_inode when it was created initially.
	 */
	lower_inode = esdfs_lower_inode(inode);
	esdfs_set_lower_inode(inode, NULL);
	iput(lower_inode);
}

static struct inode *esdfs_alloc_inode(struct super_block *sb)
{
	struct esdfs_inode_info *i;

	i = kmem_cache_alloc(esdfs_inode_cachep, GFP_KERNEL);
	if (!i)
		return NULL;

	/* memset everything up to the inode to 0 */
	memset(i, 0, offsetof(struct esdfs_inode_info, vfs_inode));

	i->vfs_inode.i_version = 1;
	return &i->vfs_inode;
}

static void esdfs_destroy_inode(struct inode *inode)
{
	kmem_cache_free(esdfs_inode_cachep, ESDFS_I(inode));
}

/* esdfs inode cache constructor */
static void init_once(void *obj)
{
	struct esdfs_inode_info *i = obj;

	inode_init_once(&i->vfs_inode);
}

int esdfs_init_inode_cache(void)
{
	int err = 0;

	esdfs_inode_cachep =
		kmem_cache_create("esdfs_inode_cache",
				  sizeof(struct esdfs_inode_info), 0,
				  SLAB_RECLAIM_ACCOUNT, init_once);
	if (!esdfs_inode_cachep)
		err = -ENOMEM;
	return err;
}

/* esdfs inode cache destructor */
void esdfs_destroy_inode_cache(void)
{
	if (esdfs_inode_cachep)
		kmem_cache_destroy(esdfs_inode_cachep);
}

/*
 * Used only in nfs, to kill any pending RPC tasks, so that subsequent
 * code can actually succeed and won't leave tasks that need handling.
 */
static void esdfs_umount_begin(struct super_block *sb)
{
	struct super_block *lower_sb;

	lower_sb = esdfs_lower_super(sb);
	if (lower_sb && lower_sb->s_op && lower_sb->s_op->umount_begin)
		lower_sb->s_op->umount_begin(lower_sb);
}

const struct super_operations esdfs_sops = {
	.put_super	= esdfs_put_super,
	.statfs		= esdfs_statfs,
	.remount_fs	= esdfs_remount_fs,
	.evict_inode	= esdfs_evict_inode,
	.umount_begin	= esdfs_umount_begin,
	.show_options	= generic_show_options,
	.alloc_inode	= esdfs_alloc_inode,
	.destroy_inode	= esdfs_destroy_inode,
	.drop_inode	= generic_delete_inode,
};
