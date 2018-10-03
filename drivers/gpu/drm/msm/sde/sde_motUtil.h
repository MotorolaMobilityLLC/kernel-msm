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
        MOTUTIL_DISP_UTIL = 0,
};

enum sde_motUtil_disp_cmd {
	DISPUTIL_TYPE = 0,
	DISPUTIL_PANEL_TYPE,
	DISPUTIL_CMD_TYPE,
	DISPUTIL_CMD_XFER_MODE,
	DISPUTIL_MIPI_CMD_TYPE,
	DISPUTIL_NUM_BYTE_RD,
	DISPUTIL_PAYLOAD_LEN_U,
	DISPUTIL_PAYLOAD_LEN_L,
	DISPUTIL_PAYLOAD,
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

#define DISPUTIL_DSI_WRITE      0
#define DISPUTIL_DSI_READ       1

#define DISPUTIL_DSI_DCS        0
#define DISPUTIL_DSI_GENERIC    1

#define DISPUTIL_DSI_HS_MODE    0
#define DISPUTIL_DSI_LP_MODE    1

#define MOTUTIL_MAIN_DISP	0
#define MOTUTIL_CLI_DISP	1

int sde_debugfs_mot_util_init(struct sde_kms *sde_kms,
			struct dentry *parent);

#endif /* __MOTUTIL_H__ */
