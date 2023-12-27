/*
 * Universal Flash Storage Write Hint (UFS whint)
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * Author:
 *      Soon Hwang <soon95.hwang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * See the COPYING file in the top-level directory or visit
 * <http://www.gnu.org/licenses/gpl-2.0.html>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This program is provided "AS IS" and "WITH ALL FAULTS" and
 * without warranty of any kind. You are solely responsible for
 * determining the appropriateness of using and distributing
 * the program and assume all risks associated with your exercise
 * of rights with respect to the program, including but not limited
 * to infringement of third party rights, the risks and costs of
 * program errors, damage to or loss of data, programs or equipment,
 * and unavailability or interruption of operations. Under no
 * circumstances will the contributor of this Program be liable for
 * any damages of any kind arising from your use or distribution of
 * this program.
 *
 * The Linux Foundation chooses to take subject only to the GPLv2
 * license terms, and distributes only under these terms.
 */

#include "ufswhint.h"

struct inode *ufswhint_get_inode(struct bio *bio)
{
	struct address_space *f_mapping;
	struct page *page;

	if (!bio || !bio->bi_io_vec || !bio->bi_io_vec->bv_page)
		return NULL;
	page = bio->bi_io_vec->bv_page;

	if (PageAnon(page))
		return NULL;
	f_mapping = page_file_mapping(page);
	if (!f_mapping || !f_mapping->host)
		return NULL;

	return f_mapping->host;
}

bool ufswhint_check_f2fs_io(struct super_block *sb)
{
	const char *check_str = "f2fs";

	if (!sb || !sb->s_type || !sb->s_type->name)
		return false;

	if (strcmp(sb->s_type->name, check_str) != 0)
		return false;

	if (!sb->s_fs_info)
		return false;

	return true;
}

enum ufsf_rw_hint ufswhint_get_type(struct f2fs_sb_info *sbi, struct inode *inode, struct bio *bio)
{
	struct page *page = bio->bi_io_vec->bv_page;

	if (!sbi)
		return UFSF_HINT_NOT_SET;
	/*
	 *  Write Hint is detected by Driver with following sequence.
	 *
	 * 1. WARM & HOT Node
	 * 2. COLD Node
	 * 3. COLD Data
	 * 4. HOT Data
	 * 5. META
	 * 6. WARM Data
	 */

	if (NODE_MAPPING(sbi) == page->mapping) {
		if (IS_DNODE(page))
			return UFSF_HINT_NOT_SET;
		else
			return UFSF_HINT_NONE;
	} else if (is_inode_flag_set(inode, FI_ALIGNED_WRITE) || page_private_gcing(page) ||
			file_is_cold(inode) || f2fs_need_compress_data(inode))
		return UFSF_HINT_EXTREME;
	else if (file_is_hot(inode) || is_inode_flag_set(inode, FI_HOT_DATA) ||
			f2fs_is_atomic_file(inode) || f2fs_is_volatile_file(inode))
		return UFSF_HINT_SHORT;
	else if ((bio->bi_opf & REQ_META) && (bio->bi_opf & REQ_PRIO))
		return UFSF_HINT_MEDIUM;
	else {
		if (inode->i_write_hint)
			return inode->i_write_hint;
		else
			return UFSF_HINT_LONG;
	}
	return UFSF_HINT_NOT_SET;
}

enum ufsf_rw_hint ufswhint_get_whint(struct bio *bio)
{
	bool check_f2fs_io;
	struct inode *inode;
	struct super_block *sb;
	struct f2fs_sb_info *sbi;

	if (!op_is_write(bio->bi_opf))
		return UFSF_HINT_NOT_SET;

	inode = ufswhint_get_inode(bio);
	if (!inode)
		return UFSF_HINT_NOT_SET;

	if (!inode->i_sb)
		return UFSF_HINT_NOT_SET;

	sb = inode->i_sb;
	check_f2fs_io = ufswhint_check_f2fs_io(sb);

	if (!check_f2fs_io)
		return UFSF_HINT_NOT_SET;

	sbi = (struct f2fs_sb_info *)sb->s_fs_info;
	return ufswhint_get_type(sbi, inode, bio);

}
