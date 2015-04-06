/*
 * Copyright (c) 2013 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

#ifndef _TESTMODE_H_
#define _TESTMODE_H_

#define AR6K_TM_DATA_MAX_LEN	5000

enum ar6k_testmode_attr {
	__AR6K_TM_ATTR_INVALID	= 0,
	AR6K_TM_ATTR_CMD	= 1,
	AR6K_TM_ATTR_DATA	= 2,
	AR6K_TM_ATTR_STREAM_ID	= 3,

	/* keep last */
	__AR6K_TM_ATTR_AFTER_LAST,
	AR6K_TM_ATTR_MAX	= __AR6K_TM_ATTR_AFTER_LAST - 1
};

enum ar6k_testmode_cmd {
	AR6K_TM_CMD_TCMD	= 0,
	AR6K_TM_CMD_WMI_CMD	= 0xF000,
};

enum ar6k_tcmd_ep {
	TCMD_EP_TCMD,
	TCMD_EP_WMI,
};

struct ar6k_testmode_cmd_data {
	void *data;
	int len;
};
#endif	/* #ifndef _TESTMODE_H_ */
