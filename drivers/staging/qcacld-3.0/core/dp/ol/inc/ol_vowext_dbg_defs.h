/*
 * Copyright (c) 2012, 2014-2016, 2018 The Linux Foundation. All rights reserved.
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

#ifndef _VOW_DEFINES__H_
#define _VOW_DEFINES__H_

#define UDP_CKSUM_OFFSET   40   /* UDP check sum offset in network buffer */
#define RTP_HDR_OFFSET  42      /* RTP header offset in network buffer */
#define EXT_HDR_OFFSET 54       /* Extension header offset in network buffer */
#define UDP_PDU_RTP_EXT  0x90   /* ((2 << 6) | (1 << 4)) RTP V2 + X bit */
#define IP_VER4_N_NO_EXTRA_HEADERS 0x45
#define IPERF3_DATA_OFFSET 12   /* iperf3 data offset from EXT_HDR_OFFSET */
#define HAL_RX_40  0x08         /* 40 Mhz */
#define HAL_RX_GI  0x04         /* full gi */

struct vow_extstats {
	uint8_t rx_rssi_ctl0;   /* control channel chain0 rssi */
	uint8_t rx_rssi_ctl1;   /* control channel chain1 rssi */
	uint8_t rx_rssi_ctl2;   /* control channel chain2 rssi */
	uint8_t rx_rssi_ext0;   /* extension channel chain0 rssi */
	uint8_t rx_rssi_ext1;   /* extension channel chain1 rssi */
	uint8_t rx_rssi_ext2;   /* extension channel chain2 rssi */
	uint8_t rx_rssi_comb;   /* combined RSSI value */
	uint8_t rx_bw;          /* Band width 0-20, 1-40, 2-80 */
	uint8_t rx_sgi;         /* Guard interval, 0-Long GI, 1-Short GI */
	uint8_t rx_nss;         /* Number of spatial streams */
	uint8_t rx_mcs;         /* Rate MCS value */
	uint8_t rx_ratecode;    /* Hardware rate code */
	uint8_t rx_rs_flags;    /* Receive misc flags */
	uint8_t rx_moreaggr;    /* 0 - non aggr frame */
	uint32_t rx_macTs;      /* Time stamp */
	uint16_t rx_seqno;      /* rx sequence number */
};

/**
 * @brief populates vow ext stats in given network buffer.
 * @param msdu - network buffer handle
 * @param pdev - handle to htt dev.
 */
void ol_ath_add_vow_extstats(htt_pdev_handle pdev, qdf_nbuf_t msdu);

#endif /* _VOW_DEFINES__H_ */
