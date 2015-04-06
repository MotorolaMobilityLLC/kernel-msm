/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
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

#ifndef _PKTLOG_AC_I_
#define _PKTLOG_AC_I_
#ifndef REMOVE_PKT_LOG

#include <ol_txrx_internal.h>
#include <pktlog_ac.h>

#define PKTLOG_DEFAULT_BUFSIZE		(1024 * 1024)
#define PKTLOG_DEFAULT_SACK_THR		3
#define PKTLOG_DEFAULT_TAIL_LENGTH	100
#define PKTLOG_DEFAULT_THRUPUT_THRESH	(64 * 1024)
#define PKTLOG_DEFAULT_PER_THRESH	30
#define PKTLOG_DEFAULT_PHYERR_THRESH	300
#define PKTLOG_DEFAULT_TRIGGER_INTERVAL	500
struct ath_pktlog_arg {
	struct ath_pktlog_info *pl_info;
	u_int32_t flags;
	u_int16_t missed_cnt;
	u_int16_t log_type;
	size_t log_size;
	u_int16_t timestamp;
	char *buf;
};

void pktlog_getbuf_intsafe(struct ath_pktlog_arg *plarg);
char *pktlog_getbuf(struct ol_pktlog_dev_t *pl_dev,
		    struct ath_pktlog_info *pl_info,
		    size_t log_size,
		    struct ath_pktlog_hdr *pl_hdr);

A_STATUS process_tx_info(struct ol_txrx_pdev_t *pdev, void *data);
A_STATUS process_rx_info(void *pdev, void *data);
A_STATUS process_rx_info_remote(void *pdev, adf_nbuf_t amsdu);
A_STATUS process_rate_find(void *pdev, void *data);
A_STATUS process_rate_update(void *pdev, void *data);

#endif  /* REMOVE_PKT_LOG */
#endif
