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

/* mount options */
#define ESDFS_MOUNT_DERIVE_LEGACY	0x00000001
#define ESDFS_MOUNT_DERIVE_UNIFIED	0x00000002
#define ESDFS_MOUNT_DERIVE_MULTI	0x00000004
#define ESDFS_MOUNT_DERIVE_PUBLIC	0x00000008
#define ESDFS_MOUNT_DERIVE_CONFINE	0x00000010

#define clear_opt(sbi, option)	(sbi->options &= ~ESDFS_MOUNT_##option)
#define set_opt(sbi, option)	(sbi->options |= ESDFS_MOUNT_##option)
#define test_opt(sbi, option)	(sbi->options & ESDFS_MOUNT_##option)

#define ESDFS_DERIVE_PERMS(sbi)	(test_opt(sbi, DERIVE_UNIFIED) || \
				 test_opt(sbi, DERIVE_LEGACY))
#define ESDFS_RESTRICT_PERMS(sbi) (ESDFS_DERIVE_PERMS(sbi) && \
				   !test_opt(sbi, DERIVE_PUBLIC) && \
				   !test_opt(sbi, DERIVE_MULTI))

/* from android_filesystem_config.h */
#define AID_ROOT             0
#define AID_SDCARD_RW     1015
#define AID_MEDIA_RW      1023
#define AID_SDCARD_R      1028
#define AID_SDCARD_PICS   1033
#define AID_SDCARD_AV     1034
#define AID_SDCARD_ALL    1035

/* used in extra persmission check during file creation */
#define ESDFS_MAY_CREATE	0x00001000

/* derived permissions model based on tree location */
enum {
	ESDFS_TREE_NONE = 0,		/* permissions not derived */
	ESDFS_TREE_ROOT_LEGACY,		/* root for legacy emulated storage */
	ESDFS_TREE_ROOT,		/* root for a user */
	ESDFS_TREE_MEDIA,		/* per-user basic permissions */
	ESDFS_TREE_ANDROID,		/* .../Android */
	ESDFS_TREE_ANDROID_DATA,	/* .../Android/data */
	ESDFS_TREE_ANDROID_OBB,		/* .../Android/obb */
	ESDFS_TREE_ANDROID_MEDIA,	/* .../Android/media */
	ESDFS_TREE_ANDROID_APP,		/* .../Android/data|obb|media/... */
	ESDFS_TREE_ANDROID_USER,	/* .../Android/user */
};

/* for permissions table lookups */
enum {
	ESDFS_PERMS_LOWER_DEFAULT = 0,
	ESDFS_PERMS_UPPER_LEGACY,
	ESDFS_PERMS_UPPER_DERIVED,
	ESDFS_PERMS_TABLE_SIZE
};

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

extern void esdfs_msg(struct super_block *, const char *, const char *, ...);
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
extern int esdfs_init_package_list(void);
extern void esdfs_destroy_package_list(void);
extern void esdfs_derive_perms(struct dentry *dentry);
extern void esdfs_set_derived_perms(struct inode *inode);
extern int esdfs_derived_lookup(struct dentry *dentry, struct dentry **parent);
extern int esdfs_derived_revalidate(struct dentry *dentry,
				    struct dentry *parent);
extern int esdfs_check_derived_permission(struct inode *inode, int mask);
extern int esdfs_derive_mkdir_contents(struct dentry *dentry);

/* file private data */
struct esdfs_file_info {
	struct file *lower_file;
	const struct vm_operations_struct *lower_vm_ops;
};

struct esdfs_perms {
	uid_t uid;
	gid_t gid;
	unsigned short fmask;
	unsigned short dmask;
};

/* esdfs inode data in memory */
struct esdfs_inode_info {
	struct inode *lower_inode;
	struct inode vfs_inode;
	unsigned version;	/* package list version this was derived from */
	int tree;		/* storage tree location */
	uid_t userid;		/* Android User ID (not Linux UID) */
	uid_t appid;		/* Linux UID for this app/user combo */
};

/* esdfs dentry data in memory */
struct esdfs_dentry_info {
	spinlock_t lock;	/* protects lower_path and lower_stub_path */
	struct path lower_path;
	struct path lower_stub_path;
	struct dentry *real_parent;
};

/* esdfs super-block data in memory */
struct esdfs_sb_info {
	struct super_block *lower_sb;
	struct super_block *s_sb;
	struct list_head s_list;
	u32 lower_secid;
	struct esdfs_perms lower_perms;
	struct esdfs_perms upper_perms;	/* root in derived mode */
	struct dentry *obb_parent;	/* pinned dentry for obb link parent */
	unsigned int options;
};

extern struct esdfs_perms esdfs_perms_table[ESDFS_PERMS_TABLE_SIZE];
extern unsigned esdfs_package_list_version;

void esdfs_drop_shared_icache(struct super_block *, struct inode *);
void esdfs_drop_sb_icache(struct super_block *, unsigned long);
void esdfs_add_super(struct esdfs_sb_info *, struct super_block *);

#define ESDFS_INODE_IS_STALE(i) ((i)->version != esdfs_package_list_version)
#define ESDFS_INODE_CAN_LINK(i) (test_opt(ESDFS_SB((i)->i_sb), \
					  DERIVE_LEGACY) || \
				 (test_opt(ESDFS_SB((i)->i_sb), \
					   DERIVE_UNIFIED) && \
				  ESDFS_I(i)->userid > 0))
#define ESDFS_DENTRY_NEEDS_LINK(d) (!strncasecmp((d)->d_name.name, "obb", \
						 (d)->d_name.len))
#define ESDFS_DENTRY_IS_LINKED(d) (ESDFS_D(d)->real_parent)
#define ESDFS_DENTRY_HAS_STUB(d) (ESDFS_D(d)->lower_stub_path.dentry)

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
static inline void esdfs_get_lower_stub_path(const struct dentry *dent,
					     struct path *lower_stub_path)
{
	spin_lock(&ESDFS_D(dent)->lock);
	pathcpy(lower_stub_path, &ESDFS_D(dent)->lower_stub_path);
	path_get(lower_stub_path);
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
static inline void esdfs_set_lower_stub_path(const struct dentry *dent,
					     struct path *lower_stub_path)
{
	spin_lock(&ESDFS_D(dent)->lock);
	pathcpy(&ESDFS_D(dent)->lower_stub_path, lower_stub_path);
	spin_unlock(&ESDFS_D(dent)->lock);
	return;
}
static inline void esdfs_put_reset_lower_paths(const struct dentry *dent)
{
	struct path lower_path;
	struct path lower_stub_path = { NULL, NULL };

	spin_lock(&ESDFS_D(dent)->lock);
	pathcpy(&lower_path, &ESDFS_D(dent)->lower_path);
	ESDFS_D(dent)->lower_path.dentry = NULL;
	ESDFS_D(dent)->lower_path.mnt = NULL;
	if (ESDFS_DENTRY_HAS_STUB(dent)) {
		pathcpy(&lower_stub_path, &ESDFS_D(dent)->lower_stub_path);
		ESDFS_D(dent)->lower_stub_path.dentry = NULL;
		ESDFS_D(dent)->lower_stub_path.mnt = NULL;
	}
	spin_unlock(&ESDFS_D(dent)->lock);

	path_put(&lower_path);
	if (lower_stub_path.dentry)
		path_put(&lower_stub_path);
	return;
}
static inline void esdfs_get_lower_parent(const struct dentry *dent,
					  struct dentry *lower_dentry,
					  struct dentry **lower_parent)
{
	*lower_parent = NULL;
	spin_lock(&ESDFS_D(dent)->lock);
	if (ESDFS_DENTRY_IS_LINKED(dent)) {
		*lower_parent = ESDFS_D(dent)->real_parent;
		dget(*lower_parent);
	}
	spin_unlock(&ESDFS_D(dent)->lock);
	if (!*lower_parent)
		*lower_parent = dget_parent(lower_dentry);
	return;
}
static inline void esdfs_put_lower_parent(const struct dentry *dent,
					  struct dentry **lower_parent)
{
	dput(*lower_parent);
	return;
}
static inline void esdfs_set_lower_parent(const struct dentry *dent,
					  struct dentry *parent)
{
	struct dentry *old_parent = NULL;
	spin_lock(&ESDFS_D(dent)->lock);
	if (ESDFS_DENTRY_IS_LINKED(dent))
		old_parent = ESDFS_D(dent)->real_parent;
	ESDFS_D(dent)->real_parent = parent;
	dget(parent);	/* pin the lower parent */
	spin_unlock(&ESDFS_D(dent)->lock);
	if (old_parent)
		dput(old_parent);
	return;
}
static inline void esdfs_release_lower_parent(const struct dentry *dent)
{
	struct dentry *real_parent = NULL;
	spin_lock(&ESDFS_D(dent)->lock);
	if (ESDFS_DENTRY_IS_LINKED(dent)) {
		real_parent = ESDFS_D(dent)->real_parent;
		ESDFS_D(dent)->real_parent = NULL;
	}
	spin_unlock(&ESDFS_D(dent)->lock);
	if (real_parent)
		dput(real_parent);
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

static inline void esdfs_set_perms(struct inode *inode)
{
	struct esdfs_sb_info *sbi = ESDFS_SB(inode->i_sb);

	if (ESDFS_DERIVE_PERMS(sbi)) {
		esdfs_set_derived_perms(inode);
		return;
	}
	i_uid_write(inode, sbi->upper_perms.uid);
	i_gid_write(inode, sbi->upper_perms.gid);
	if (S_ISDIR(inode->i_mode))
		inode->i_mode = (inode->i_mode & S_IFMT) |
				sbi->upper_perms.dmask;
	else
		inode->i_mode = (inode->i_mode & S_IFMT) |
				sbi->upper_perms.fmask;
	return;
}

static inline void esdfs_revalidate_perms(struct dentry *dentry)
{
	if (ESDFS_DERIVE_PERMS(ESDFS_SB(dentry->d_sb)) &&
	    dentry->d_inode &&
	    ESDFS_INODE_IS_STALE(ESDFS_I(dentry->d_inode))) {
		esdfs_derive_perms(dentry);
		esdfs_set_perms(dentry->d_inode);
	}
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
	esdfs_copy_lower_attr(dest, src);
	esdfs_set_perms(dest);
}

#ifdef CONFIG_SECURITY_SELINUX
/*
 * Hard-code the lower source context to prevent anyone with mount permissions
 * from doing something nasty.
 */
#define ESDFS_LOWER_SECCTX "u:r:sdcardd:s0"

/*
 * Hack to be able to poke at the SID.  The Linux Security API does not provide
 * a way to change just the SID in the creds (probably on purpose).
 */
struct task_security_struct {
	u32 osid;		/* SID prior to last execve */
	u32 sid;		/* current SID */
	u32 exec_sid;		/* exec SID */
	u32 create_sid;		/* fscreate SID */
	u32 keycreate_sid;	/* keycreate SID */
	u32 sockcreate_sid;	/* fscreate SID */
};
static inline void esdfs_override_secid(struct esdfs_sb_info *sbi,
					struct cred *creds)
{
	struct task_security_struct *tsec = creds->security;

	if (sbi->lower_secid)
		tsec->sid = sbi->lower_secid;
}
#else
static inline void esdfs_override_secid(struct esdfs_sb_info *sbi,
					struct cred *creds) {}
#endif
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
	esdfs_override_secid(sbi, creds);

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
