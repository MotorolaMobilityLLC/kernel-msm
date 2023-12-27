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
#ifndef _UFSWHINT_H_
#define _UFSWHINT_H_

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm_types.h>
#include <linux/blk_types.h>
#include <linux/string.h>
#include <linux/pagemap.h>
#include <linux/mm.h>
#include <linux/bio.h>
#include "f2fs-mimic.h"

#define LOG_SECTORS_PER_BLOCK		3	/* log number for sector/blk */
#define BLKSIZE_BITS			12	/* bits for BLKSIZE */
#define F2FS_RESERVED_INODE		3	/* 0, 1, 2 is reserved node number in F2FS */
enum ufsf_rw_hint {
	UFSF_HINT_NOT_SET		= 0,
	UFSF_HINT_NONE			= 1,
	UFSF_HINT_SHORT			= 2,
	UFSF_HINT_MEDIUM		= 3,
	UFSF_HINT_LONG			= 4,
	UFSF_HINT_EXTREME		= 5,
};

struct inode *ufswhint_get_inode(struct bio *bio);
bool ufswhint_check_f2fs_io(struct super_block *sb);
enum ufsf_rw_hint ufswhint_get_type(struct f2fs_sb_info *sbi, struct inode *inode, struct bio *bio);
enum ufsf_rw_hint ufswhint_get_whint(struct bio *bio);
#endif /* End of Header */
