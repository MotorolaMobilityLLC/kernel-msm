// SPDX-License-Identifier: GPL-2.0
/*
 * fs-verity: read-only file-based integrity/authenticity
 *
 * Copyright (C) 2018, Google, Inc.
 *
 * Written by Jaegeuk Kim, Michael Halcrow, and Eric Biggers.
 */

#ifndef _LINUX_FSVERITY_H
#define _LINUX_FSVERITY_H

#include <linux/fs.h>

/*
 * fs-verity operations for filesystems
 */
struct fsverity_operations {
	bool (*is_verity)(struct inode *inode);
	int (*set_verity)(struct inode *inode);
	struct page *(*read_metadata_page)(struct inode *inode, pgoff_t index);
};

#if __FS_HAS_VERITY
extern int fsverity_file_open(struct inode *inode, struct file *filp);
extern int fsverity_prepare_setattr(struct dentry *dentry, struct iattr *attr);
extern int fsverity_prepare_getattr(struct inode *inode);
extern void fsverity_cleanup_inode(struct inode *inode);
extern bool fsverity_verify_page(struct page *page);
extern void fsverity_verify_bio(struct bio *bio);
extern void fsverity_enqueue_verify_work(struct work_struct *work);
extern loff_t fsverity_full_isize(struct inode *inode);
extern int fsverity_ioctl_set_measurement(struct file *filp,
					  const void __user *arg);
extern int fsverity_ioctl_enable(struct file *filp, const void __user *arg);
#else
static inline int fsverity_file_open(struct inode *inode, struct file *filp)
{
	return -EOPNOTSUPP;
}

static inline int fsverity_prepare_setattr(struct dentry *dentry,
					   struct iattr *attr)
{
	return -EOPNOTSUPP;
}

static inline int fsverity_prepare_getattr(struct inode *inode)
{
	return -EOPNOTSUPP;
}

static inline void fsverity_cleanup_inode(struct inode *inode)
{
}

static inline bool fsverity_verify_page(struct page *page)
{
	WARN_ON(1);
	return false;
}

static inline void fsverity_verify_bio(struct bio *bio)
{
	WARN_ON(1);
}

static inline void fsverity_enqueue_verify_work(struct work_struct *work)
{
	WARN_ON(1);
}

static inline loff_t fsverity_full_isize(struct inode *inode)
{
	return i_size_read(inode);
}

static inline int fsverity_ioctl_set_measurement(struct file *filp,
						 const void __user *arg)
{
	return -EOPNOTSUPP;
}

static inline int fsverity_ioctl_enable(struct file *filp,
					const void __user *arg)
{
	return -EOPNOTSUPP;
}

#endif	/* !__FS_HAS_VERITY */

#endif	/* _LINUX_FSVERITY_H */
