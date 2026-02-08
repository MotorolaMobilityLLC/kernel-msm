/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Manual GC support
 *
 * Copyright 2020 Google LLC
 *
 * Authors: Jaegeuk Kim <jaegeuk@google.com>
 */

#ifndef _UFS_HID_JEDEC_H_
#define _UFS_HID_JEDEC_H_

#include <asm/unaligned.h>
//#include <linux/unaligned.h>
#include <ufs/ufshcd.h>

/* bDefragOperation attribute values */
enum moto_hid_defrag_operation {
	MOTO_HID_ANALYSIS_AND_DEFRAG_DISABLE	= 0,
	MOTO_HID_ANALYSIS_ENABLE		= 1,
	MOTO_HID_ANALYSIS_AND_DEFRAG_ENABLE	= 2,
};

/* bHIDState attribute values */
enum moto_hid_state {
	MOTO_HID_IDLE		= 0,
	MOTO_ANALYSIS_IN_PROGRESS	= 1,
	MOTO_DEFRAG_REQUIRED		= 2,
	MOTO_DEFRAG_IN_PROGRESS	= 3,
	MOTO_DEFRAG_COMPLETED	= 4,
	MOTO_DEFRAG_NOT_REQUIRED	= 5,
	MOTO_NUM_UFS_HID_STATES	= 6,
};

static const char * const moto_hid_states[] = {
	[MOTO_HID_IDLE]		= "idle",
	[MOTO_ANALYSIS_IN_PROGRESS]	= "analysis_in_progress",
	[MOTO_DEFRAG_REQUIRED]	= "defrag_required",
	[MOTO_DEFRAG_IN_PROGRESS]	= "defrag_in_progress",
	[MOTO_DEFRAG_COMPLETED]	= "defrag_completed",
	[MOTO_DEFRAG_NOT_REQUIRED]	= "defrag_not_required",
};

#define MOTO_QUERY_ATTR_IDN_HID_DEFRAG_OPERATION	 0x35
#define MOTO_QUERY_ATTR_IDN_HID_AVAILABLE_SIZE	 0x36
#define MOTO_QUERY_ATTR_IDN_HID_SIZE			 0x37
#define MOTO_QUERY_ATTR_IDN_HID_PROGRESS_RATIO	 0x38
#define MOTO_QUERY_ATTR_IDN_HID_STATE		 0x39

#define MOTO_UFS_DEV_HID_SUPPORT		BIT(13)

struct moto_hid_jedec_feature {
	struct ufs_hba *hba;
	bool hid_sup;
};

extern void moto_sysfs_update_hid(struct ufs_hba *hba);
extern int moto_hid_jedec_init(struct ufs_hba *hba);
#endif
