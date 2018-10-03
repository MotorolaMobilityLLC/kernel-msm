/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MOTUTIL_H__
#define __MOTUTIL_H__

#include <linux/debugfs.h>
#include <linux/types.h>

enum sde_motUtil_type {
};

struct motUtil {
	struct mutex lock;
	enum sde_motUtil_type motUtil_type;
	bool read_cmd;
	bool last_cmd_tx_status;
	u16 read_len;
	u8 *rd_buf;
	int val;
	bool te_enable;
};

int sde_debugfs_mot_util_init(struct sde_kms *sde_kms,
			struct dentry *parent);

#endif /* __MOTUTIL_H__ */
