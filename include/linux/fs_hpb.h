/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_FS_HPB_H
#define _LINUX_FS_HPB_H

#include <linux/fs.h>

bool hpb_ext_in_list(const char* filename, const char token);

static inline bool __is_hpb_extension(const char *name)
{
	return hpb_ext_in_list(name, '.');
}

static inline bool __has_hpb_permission(struct inode *inode)
{
	return !!(inode->i_mode & (S_IXUSR | S_IXGRP | S_IXOTH));
}

static inline bool __is_hpb_file(const char *name, struct inode *inode)
{
	bool ret = false;

	if (inode && !S_ISREG(inode->i_mode))
		return false;

	if (name)
		ret |= __is_hpb_extension(name);
	if (inode)
		ret |= __has_hpb_permission(inode);

	return ret;
}
#endif
