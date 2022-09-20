/*
 * Universal Flash Storage Stream ID (UFS SID)
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * Author:
 *	Keoseong Park <keosung.park@samsung.com>
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

#ifndef _UFSSID_H_
#define _UFSSID_H_

#define UFSSID_VER					0x0201
#define UFSSID_DD_VER					0x010400
#define UFSSID_DD_VER_POST				""

#define UFS_FEATURE_SUPPORT_SID_BIT			(1 << 4)

struct ufssid_dev {
	struct ufsf_feature *ufsf;

	bool stream_id_enabled;

	/* For sysfs */
	struct kobject kobj;
	struct mutex sysfs_lock;
	struct ufssid_sysfs_entry *sysfs_entries;
};

struct ufssid_sysfs_entry {
	struct attribute attr;
	ssize_t (*show)(struct ufssid_dev *sid, char *buf);
	ssize_t (*store)(struct ufssid_dev *sid, const char *buf, size_t count);
};

struct ufshcd_lrb;

void ufssid_get_dev_info(struct ufsf_feature *ufsf, u8 *desc_buf);
void ufssid_prep_fn(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp);
void ufssid_init(struct ufsf_feature *ufsf);
void ufssid_remove(struct ufsf_feature *ufsf);
#endif /* End of Header */
