/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
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

#include <qdf_mem.h>         /* qdf_mem_malloc,free, etc. */
#include <qdf_types.h>          /* qdf_print, bool */
#include <qdf_nbuf.h>           /* qdf_nbuf_t, etc. */
#include <qdf_timer.h>		/* qdf_timer_free */

#include <htt.h>                /* HTT_HL_RX_DESC_SIZE */
#include <ol_cfg.h>
#include <ol_rx.h>
#include <ol_htt_rx_api.h>
#include <htt_internal.h>       /* HTT_ASSERT, htt_pdev_t, HTT_RX_BUF_SIZE */
#include "regtable.h"

#include <cds_ieee80211_common.h>   /* ieee80211_frame, ieee80211_qoscntl */
#include <cds_utils.h>
#include <wlan_policy_mgr_api.h>
#include "ol_txrx_types.h"
#ifdef DEBUG_DMA_DONE
#include <asm/barrier.h>
#include <wma_api.h>
#endif
#include <pktlog_ac_fmt.h>

#define HTT_FCS_LEN (4)

enum {
	HW_RX_DECAP_FORMAT_RAW = 0,
	HW_RX_DECAP_FORMAT_NWIFI,
	HW_RX_DECAP_FORMAT_8023,
	HW_RX_DECAP_FORMAT_ETH2,
};

struct mon_rx_status g_ppdu_rx_status;

/**
 * htt_rx_mon_note_capture_channel() - Make note of channel to update in
 * radiotap
 * @pdev: handle to htt_pdev
 * @mon_ch: capture channel number.
 *
 * Return: None
 */
void htt_rx_mon_note_capture_channel(htt_pdev_handle pdev, int mon_ch)
{
	struct mon_channel *ch_info = &pdev->mon_ch_info;

	ch_info->ch_num = mon_ch;
	ch_info->ch_freq = cds_chan_to_freq(mon_ch);
}

#ifndef CONFIG_HL_SUPPORT
/**
 * htt_mon_rx_handle_amsdu_packet() - Handle consecutive fragments of amsdu
 * @msdu: pointer to first msdu of amsdu
 * @pdev: Handle to htt_pdev_handle
 * @msg_word: Input and output variable, so pointer to HTT msg pointer
 * @amsdu_len: remaining length of all N-1 msdu msdu's
 * @frag_cnt: number of frags handled
 *
 * This function handles the (N-1) msdu's of amsdu, N'th msdu is already
 * handled by calling function. N-1 msdu's are tied using frags_list.
 * msdu_info field updated by FW indicates if this is last msdu. All the
 * msdu's before last msdu will be of MAX payload.
 *
 * Return: 1 on success and 0 on failure.
 */
static
int htt_mon_rx_handle_amsdu_packet(qdf_nbuf_t msdu, htt_pdev_handle pdev,
				   uint32_t **msg_word, uint32_t amsdu_len,
				   uint32_t *frag_cnt)
{
	qdf_nbuf_t frag_nbuf;
	qdf_nbuf_t prev_frag_nbuf;
	uint32_t len;
	uint32_t last_frag;
	qdf_dma_addr_t paddr;

	*msg_word += HTT_RX_IN_ORD_PADDR_IND_MSDU_DWORDS;
	paddr = htt_rx_in_ord_paddr_get(*msg_word);
	frag_nbuf = htt_rx_in_order_netbuf_pop(pdev, paddr);
	if (qdf_unlikely(!frag_nbuf)) {
		qdf_print("netbuf pop failed!");
		return 0;
	}
	*frag_cnt = *frag_cnt + 1;
	last_frag = ((struct htt_rx_in_ord_paddr_ind_msdu_t *)*msg_word)->
		msdu_info;
	qdf_nbuf_append_ext_list(msdu, frag_nbuf, amsdu_len);
	qdf_nbuf_set_pktlen(frag_nbuf, HTT_RX_BUF_SIZE);
	qdf_nbuf_unmap(pdev->osdev, frag_nbuf, QDF_DMA_FROM_DEVICE);
	/* For msdu's other than parent will not have htt_host_rx_desc_base */
	len = QDF_MIN(amsdu_len, HTT_RX_BUF_SIZE);
	amsdu_len -= len;
	qdf_nbuf_trim_tail(frag_nbuf, HTT_RX_BUF_SIZE - len);

	HTT_PKT_DUMP(qdf_trace_hex_dump(QDF_MODULE_ID_TXRX,
					QDF_TRACE_LEVEL_INFO_HIGH,
					qdf_nbuf_data(frag_nbuf),
					qdf_nbuf_len(frag_nbuf)));
	prev_frag_nbuf = frag_nbuf;
	while (!last_frag) {
		*msg_word += HTT_RX_IN_ORD_PADDR_IND_MSDU_DWORDS;
		paddr = htt_rx_in_ord_paddr_get(*msg_word);
		frag_nbuf = htt_rx_in_order_netbuf_pop(pdev, paddr);
		last_frag = ((struct htt_rx_in_ord_paddr_ind_msdu_t *)
			     *msg_word)->msdu_info;

		if (qdf_unlikely(!frag_nbuf)) {
			qdf_print("netbuf pop failed!");
			prev_frag_nbuf->next = NULL;
			return 0;
		}
		*frag_cnt = *frag_cnt + 1;
		qdf_nbuf_set_pktlen(frag_nbuf, HTT_RX_BUF_SIZE);
		qdf_nbuf_unmap(pdev->osdev, frag_nbuf, QDF_DMA_FROM_DEVICE);

		len = QDF_MIN(amsdu_len, HTT_RX_BUF_SIZE);
		amsdu_len -= len;
		qdf_nbuf_trim_tail(frag_nbuf, HTT_RX_BUF_SIZE - len);
		HTT_PKT_DUMP(qdf_trace_hex_dump(QDF_MODULE_ID_TXRX,
						QDF_TRACE_LEVEL_INFO_HIGH,
						qdf_nbuf_data(frag_nbuf),
						qdf_nbuf_len(frag_nbuf)));

		qdf_nbuf_set_next(prev_frag_nbuf, frag_nbuf);
		prev_frag_nbuf = frag_nbuf;
	}
	qdf_nbuf_set_next(prev_frag_nbuf, NULL);
	return 1;
}

#define SHORT_PREAMBLE 1
#define LONG_PREAMBLE  0
#ifdef HELIUMPLUS
/**
 * htt_rx_get_rate() - get rate info in terms of 500Kbps from htt_rx_desc
 * @l_sig_rate_select: OFDM or CCK rate
 * @l_sig_rate:
 *
 * If l_sig_rate_select is 0:
 * 0x8: OFDM 48 Mbps
 * 0x9: OFDM 24 Mbps
 * 0xA: OFDM 12 Mbps
 * 0xB: OFDM 6 Mbps
 * 0xC: OFDM 54 Mbps
 * 0xD: OFDM 36 Mbps
 * 0xE: OFDM 18 Mbps
 * 0xF: OFDM 9 Mbps
 * If l_sig_rate_select is 1:
 * 0x1:  DSSS 1 Mbps long preamble
 * 0x2:  DSSS 2 Mbps long preamble
 * 0x3:  CCK 5.5 Mbps long preamble
 * 0x4:  CCK 11 Mbps long preamble
 * 0x5:  DSSS 2 Mbps short preamble
 * 0x6:  CCK 5.5 Mbps
 * 0x7:  CCK 11 Mbps short  preamble
 *
 * Return: rate interms of 500Kbps.
 */
static unsigned char htt_rx_get_rate(uint32_t l_sig_rate_select,
				     uint32_t l_sig_rate, uint8_t *preamble)
{
	char ret = 0x0;
	*preamble = SHORT_PREAMBLE;
	if (l_sig_rate_select == 0) {
		switch (l_sig_rate) {
		case 0x8:
			ret = 0x60;
			break;
		case 0x9:
			ret = 0x30;
			break;
		case 0xA:
			ret = 0x18;
			break;
		case 0xB:
			ret = 0x0c;
			break;
		case 0xC:
			ret = 0x6c;
			break;
		case 0xD:
			ret = 0x48;
			break;
		case 0xE:
			ret = 0x24;
			break;
		case 0xF:
			ret = 0x12;
			break;
		default:
			break;
		}
	} else if (l_sig_rate_select == 1) {
		switch (l_sig_rate) {
		case 0x1:
			ret = 0x2;
			*preamble = LONG_PREAMBLE;
			break;
		case 0x2:
			ret = 0x4;
			*preamble = LONG_PREAMBLE;
			break;
		case 0x3:
			ret = 0xB;
			*preamble = LONG_PREAMBLE;
			break;
		case 0x4:
			ret = 0x16;
			*preamble = LONG_PREAMBLE;
			break;
		case 0x5:
			ret = 0x4;
			break;
		case 0x6:
			ret = 0xB;
			break;
		case 0x7:
			ret = 0x16;
			break;
		default:
			break;
		}
	} else {
		qdf_print("Invalid rate info\n");
	}
	return ret;
}
#else
/**
 * htt_rx_get_rate() - get rate info in terms of 500Kbps from htt_rx_desc
 * @l_sig_rate_select: OFDM or CCK rate
 * @l_sig_rate:
 *
 * If l_sig_rate_select is 0:
 * 0x8: OFDM 48 Mbps
 * 0x9: OFDM 24 Mbps
 * 0xA: OFDM 12 Mbps
 * 0xB: OFDM 6 Mbps
 * 0xC: OFDM 54 Mbps
 * 0xD: OFDM 36 Mbps
 * 0xE: OFDM 18 Mbps
 * 0xF: OFDM 9 Mbps
 * If l_sig_rate_select is 1:
 * 0x8: CCK 11 Mbps long preamble
 *  0x9: CCK 5.5 Mbps long preamble
 * 0xA: CCK 2 Mbps long preamble
 * 0xB: CCK 1 Mbps long preamble
 * 0xC: CCK 11 Mbps short preamble
 * 0xD: CCK 5.5 Mbps short preamble
 * 0xE: CCK 2 Mbps short preamble
 *
 * Return: rate interms of 500Kbps.
 */
static unsigned char htt_rx_get_rate(uint32_t l_sig_rate_select,
				     uint32_t l_sig_rate, uint8_t *preamble)
{
	char ret = 0x0;
	*preamble = SHORT_PREAMBLE;
	if (l_sig_rate_select == 0) {
		switch (l_sig_rate) {
		case 0x8:
			ret = 0x60;
			break;
		case 0x9:
			ret = 0x30;
			break;
		case 0xA:
			ret = 0x18;
			break;
		case 0xB:
			ret = 0x0c;
			break;
		case 0xC:
			ret = 0x6c;
			break;
		case 0xD:
			ret = 0x48;
			break;
		case 0xE:
			ret = 0x24;
			break;
		case 0xF:
			ret = 0x12;
			break;
		default:
			break;
		}
	} else if (l_sig_rate_select == 1) {
		switch (l_sig_rate) {
		case 0x8:
			ret = 0x16;
			*preamble = LONG_PREAMBLE;
			break;
		case 0x9:
			ret = 0x0B;
			*preamble = LONG_PREAMBLE;
			break;
		case 0xA:
			ret = 0x4;
			*preamble = LONG_PREAMBLE;
			break;
		case 0xB:
			ret = 0x02;
			*preamble = LONG_PREAMBLE;
			break;
		case 0xC:
			ret = 0x16;
			break;
		case 0xD:
			ret = 0x0B;
			break;
		case 0xE:
			ret = 0x04;
			break;
		default:
			break;
		}
	} else {
		qdf_print("Invalid rate info\n");
	}
	return ret;
}
#endif /* HELIUMPLUS */

/**
 * htt_mon_rx_get_phy_info() - Get phy info
 * @rx_desc: Pointer to struct htt_host_rx_desc_base
 * @rx_status: Return variable updated with phy_info in rx_status
 *
 * Return: None
 */
static void htt_mon_rx_get_phy_info(struct htt_host_rx_desc_base *rx_desc,
				    struct mon_rx_status *rx_status)
{
	uint8_t preamble = 0;
	uint8_t preamble_type = rx_desc->ppdu_start.preamble_type;
	uint8_t mcs = 0, nss = 0, sgi = 0, bw = 0, beamformed = 0;
	uint16_t vht_flags = 0, ht_flags = 0;
	uint32_t l_sig_rate_select = rx_desc->ppdu_start.l_sig_rate_select;
	uint32_t l_sig_rate = rx_desc->ppdu_start.l_sig_rate;
	bool is_stbc = 0, ldpc = 0;

	switch (preamble_type) {
	case 4:
	/* legacy */
		rx_status->rate = htt_rx_get_rate(l_sig_rate_select, l_sig_rate,
						&preamble);
		break;
	case 8:
		is_stbc = ((VHT_SIG_A_2(rx_desc) >> 4) & 3);
		/* fallthrough */
	case 9:
		ht_flags = 1;
		sgi = (VHT_SIG_A_2(rx_desc) >> 7) & 0x01;
		bw = (VHT_SIG_A_1(rx_desc) >> 7) & 0x01;
		mcs = (VHT_SIG_A_1(rx_desc) & 0x7f);
		nss = mcs >> 3;
		beamformed =
			(VHT_SIG_A_2(rx_desc) >> 8) & 0x1;
		break;
	case 0x0c:
		is_stbc = (VHT_SIG_A_2(rx_desc) >> 3) & 1;
		ldpc = (VHT_SIG_A_2(rx_desc) >> 2) & 1;
		/* fallthrough */
	case 0x0d:
	{
		uint8_t gid_in_sig = ((VHT_SIG_A_1(rx_desc) >> 4) & 0x3f);

		vht_flags = 1;
		sgi = VHT_SIG_A_2(rx_desc) & 0x01;
		bw = (VHT_SIG_A_1(rx_desc) & 0x03);
		if (gid_in_sig == 0 || gid_in_sig == 63) {
			/* SU case */
			mcs = (VHT_SIG_A_2(rx_desc) >> 4) &
				0xf;
			nss = (VHT_SIG_A_1(rx_desc) >> 10) &
				0x7;
		} else {
			/* MU case */
			uint8_t sta_user_pos =
				(uint8_t)((rx_desc->ppdu_start.reserved_4a >> 8)
					  & 0x3);
			mcs = (rx_desc->ppdu_start.vht_sig_b >> 16);
			if (bw >= 2)
				mcs >>= 3;
			else if (bw > 0)
				mcs >>= 1;
			mcs &= 0xf;
			nss = (((VHT_SIG_A_1(rx_desc) >> 10) +
				sta_user_pos * 3) & 0x7);
		}
		beamformed = (VHT_SIG_A_2(rx_desc) >> 8) & 0x1;
	}
		/* fallthrough */
	default:
		break;
	}

	rx_status->mcs = mcs;
	rx_status->bw = bw;
	rx_status->nr_ant = nss;
	rx_status->is_stbc = is_stbc;
	rx_status->sgi = sgi;
	rx_status->ldpc = ldpc;
	rx_status->beamformed = beamformed;
	rx_status->vht_flag_values3[0] = mcs << 0x4 | (nss + 1);
	if (ht_flags)
		rx_status->ht_mcs = mcs;
	rx_status->ht_flags = ht_flags;
	rx_status->vht_flags = vht_flags;
	rx_status->rtap_flags |= ((preamble == SHORT_PREAMBLE) ? BIT(1) : 0);
	rx_status->vht_flag_values2 = bw;
}

/**
 * htt_mon_rx_get_rtap_flags() - Get radiotap flags
 * @rx_desc: Pointer to struct htt_host_rx_desc_base
 *
 * Return: Bitmapped radiotap flags.
 */
static uint8_t htt_mon_rx_get_rtap_flags(struct htt_host_rx_desc_base *rx_desc)
{
	uint8_t rtap_flags = 0;

	/* WEP40 || WEP104 || WEP128 */
	if (rx_desc->mpdu_start.encrypt_type == 0 ||
	    rx_desc->mpdu_start.encrypt_type == 1 ||
	    rx_desc->mpdu_start.encrypt_type == 3)
		rtap_flags |= BIT(2);

	/* IEEE80211_RADIOTAP_F_FRAG */
	if (rx_desc->attention.fragment)
		rtap_flags |= BIT(3);

	/* IEEE80211_RADIOTAP_F_FCS */
	rtap_flags |= BIT(4);

	/* IEEE80211_RADIOTAP_F_BADFCS */
	if (rx_desc->mpdu_end.fcs_err)
		rtap_flags |= BIT(6);

	return rtap_flags;
}

void htt_rx_mon_get_rx_status(htt_pdev_handle pdev,
			      struct htt_host_rx_desc_base *rx_desc,
			      struct mon_rx_status *rx_status)
{
	struct mon_channel *ch_info = &pdev->mon_ch_info;

	rx_status->tsft = (u_int64_t)TSF_TIMESTAMP(rx_desc);
	rx_status->chan_freq = ch_info->ch_freq;
	rx_status->chan_num = ch_info->ch_num;
	htt_mon_rx_get_phy_info(rx_desc, rx_status);
	rx_status->rtap_flags |= htt_mon_rx_get_rtap_flags(rx_desc);

	if (rx_desc->ppdu_start.l_sig_rate_select)
		rx_status->cck_flag = 1;
	else
		rx_status->ofdm_flag = 1;

	rx_status->ant_signal_db = rx_desc->ppdu_start.rssi_comb;
	rx_status->rssi_comb = rx_desc->ppdu_start.rssi_comb;
	rx_status->chan_noise_floor = pdev->txrx_pdev->chan_noise_floor;
}

/**
 * htt_rx_mon_amsdu_rx_in_order_pop_ll() - Monitor mode HTT Rx in order pop
 * function
 * @pdev: Handle to htt_pdev_handle
 * @rx_ind_msg: In order indication message.
 * @head_msdu: Return variable pointing to head msdu.
 * @tail_msdu: Return variable pointing to tail msdu.
 *
 * This function pops the msdu based on paddr:length of inorder indication
 * message.
 *
 * Return: 1 for success, 0 on failure.
 */
int htt_rx_mon_amsdu_rx_in_order_pop_ll(htt_pdev_handle pdev,
					qdf_nbuf_t rx_ind_msg,
					qdf_nbuf_t *head_msdu,
					qdf_nbuf_t *tail_msdu,
					uint32_t *replenish_cnt)
{
	qdf_nbuf_t msdu, next, prev = NULL;
	uint8_t *rx_ind_data;
	uint32_t *msg_word;
	uint32_t msdu_count;
	struct htt_host_rx_desc_base *rx_desc;
	uint32_t amsdu_len;
	uint32_t len;
	uint32_t last_frag;
	qdf_dma_addr_t paddr;
	static uint8_t preamble_type;
	static uint32_t vht_sig_a_1;
	static uint32_t vht_sig_a_2;

	HTT_ASSERT1(htt_rx_in_order_ring_elems(pdev) != 0);

	rx_ind_data = qdf_nbuf_data(rx_ind_msg);
	msg_word = (uint32_t *)rx_ind_data;

	*replenish_cnt = 0;
	HTT_PKT_DUMP(qdf_trace_hex_dump(QDF_MODULE_ID_TXRX,
					QDF_TRACE_LEVEL_INFO_HIGH,
					(void *)rx_ind_data,
					(int)qdf_nbuf_len(rx_ind_msg)));

	/* Get the total number of MSDUs */
	msdu_count = HTT_RX_IN_ORD_PADDR_IND_MSDU_CNT_GET(*(msg_word + 1));
	HTT_RX_CHECK_MSDU_COUNT(msdu_count);

	msg_word = (uint32_t *)(rx_ind_data +
				 HTT_RX_IN_ORD_PADDR_IND_HDR_BYTES);
	paddr = htt_rx_in_ord_paddr_get(msg_word);
	msdu = htt_rx_in_order_netbuf_pop(pdev, paddr);

	if (qdf_unlikely(!msdu)) {
		qdf_print("netbuf pop failed!");
		*tail_msdu = NULL;
		return 0;
	}
	*replenish_cnt = *replenish_cnt + 1;

	while (msdu_count > 0) {
		msdu_count--;
		/*
		 * Set the netbuf length to be the entire buffer length
		 * initially, so the unmap will unmap the entire buffer.
		 */
		qdf_nbuf_set_pktlen(msdu, HTT_RX_BUF_SIZE);
		qdf_nbuf_unmap(pdev->osdev, msdu, QDF_DMA_FROM_DEVICE);

		/*
		 * cache consistency has been taken care of by the
		 * qdf_nbuf_unmap
		 */
		rx_desc = htt_rx_desc(msdu);
		if ((unsigned int)(*(uint32_t *)&rx_desc->attention) &
				RX_DESC_ATTN_MPDU_LEN_ERR_BIT) {
			qdf_nbuf_free(msdu);
			last_frag = ((struct htt_rx_in_ord_paddr_ind_msdu_t *)
			     msg_word)->msdu_info;
			while (!last_frag) {
				msg_word += HTT_RX_IN_ORD_PADDR_IND_MSDU_DWORDS;
				paddr = htt_rx_in_ord_paddr_get(msg_word);
				msdu = htt_rx_in_order_netbuf_pop(pdev, paddr);
				last_frag = ((struct
					htt_rx_in_ord_paddr_ind_msdu_t *)
					msg_word)->msdu_info;
				if (qdf_unlikely(!msdu)) {
					qdf_print("netbuf pop failed!");
					return 0;
				}
				*replenish_cnt = *replenish_cnt + 1;
				qdf_nbuf_unmap(pdev->osdev, msdu,
					       QDF_DMA_FROM_DEVICE);
				qdf_nbuf_free(msdu);
			}
			msdu = prev;
			goto next_pop;
		}

		if (!prev)
			(*head_msdu) = msdu;
		prev = msdu;

		HTT_PKT_DUMP(htt_print_rx_desc(rx_desc));

		/*
		 * Only the first mpdu has valid preamble type, so use it
		 * till the last mpdu is reached
		 */
		if (rx_desc->attention.first_mpdu) {
			preamble_type = rx_desc->ppdu_start.preamble_type;
			if (preamble_type == 8 || preamble_type == 9 ||
			    preamble_type == 0x0c || preamble_type == 0x0d) {
				vht_sig_a_1 = VHT_SIG_A_1(rx_desc);
				vht_sig_a_2 = VHT_SIG_A_2(rx_desc);
			}
		} else {
			rx_desc->ppdu_start.preamble_type = preamble_type;
			if (preamble_type == 8 || preamble_type == 9 ||
			    preamble_type == 0x0c || preamble_type == 0x0d) {
				VHT_SIG_A_1(rx_desc) = vht_sig_a_1;
				VHT_SIG_A_2(rx_desc) = vht_sig_a_2;
			}
		}

		if (rx_desc->attention.last_mpdu) {
			preamble_type = 0;
			vht_sig_a_1 = 0;
			vht_sig_a_2 = 0;
		}

		/*
		 * Make the netbuf's data pointer point to the payload rather
		 * than the descriptor.
		 */
		if (rx_desc->attention.first_mpdu) {
			memset(&g_ppdu_rx_status, 0,
			       sizeof(struct mon_rx_status));
			htt_rx_mon_get_rx_status(pdev, rx_desc,
						 &g_ppdu_rx_status);
		}
		/*
		 * For certain platform, 350 bytes of headroom is already
		 * appended to accommodate radiotap header but
		 * qdf_nbuf_update_radiotap() API again will try to create
		 * a room for radiotap header. To make our design simple
		 * let qdf_nbuf_update_radiotap() API create a room for radiotap
		 * header and update it, do qdf_nbuf_pull_head() operation and
		 * pull 350 bytes of headroom.
		 *
		 *
		 *
		 *               (SKB buffer)
		 * skb->head --> +-----------+ <-- skb->data
		 *               |           |     (Before pulling headroom)
		 *               |           |
		 *               |   HEAD    |  350 bytes of headroom
		 *               |           |
		 *               |           |
		 *               +-----------+ <-- skb->data
		 *               |           |     (After pulling headroom)
		 *               |           |
		 *               |   DATA    |
		 *               |           |
		 *               |           |
		 *               +-----------+
		 *               |           |
		 *               |           |
		 *               |   TAIL    |
		 *               |           |
		 *               |           |
		 *               +-----------+
		 *
		 */
		if (qdf_nbuf_head(msdu) == qdf_nbuf_data(msdu))
			qdf_nbuf_pull_head(msdu, HTT_RX_STD_DESC_RESERVATION);
		qdf_nbuf_update_radiotap(&g_ppdu_rx_status, msdu,
					 HTT_RX_STD_DESC_RESERVATION);
		amsdu_len = HTT_RX_IN_ORD_PADDR_IND_MSDU_LEN_GET(*(msg_word +
						NEXT_FIELD_OFFSET_IN32));

		/*
		 * MAX_RX_PAYLOAD_SZ when we have AMSDU packet. amsdu_len in
		 * which case is the total length of sum of all AMSDU's
		 */
		len = QDF_MIN(amsdu_len, MAX_RX_PAYLOAD_SZ);
		amsdu_len -= len;
		qdf_nbuf_trim_tail(msdu, HTT_RX_BUF_SIZE -
				   (RX_STD_DESC_SIZE + len));

		HTT_PKT_DUMP(qdf_trace_hex_dump(QDF_MODULE_ID_TXRX,
						QDF_TRACE_LEVEL_INFO_HIGH,
						qdf_nbuf_data(msdu),
						qdf_nbuf_len(msdu)));
		last_frag = ((struct htt_rx_in_ord_paddr_ind_msdu_t *)
			     msg_word)->msdu_info;

		/* Handle amsdu packet */
		if (!last_frag) {
			/*
			 * For AMSDU packet msdu->len is sum of all the msdu's
			 * length, msdu->data_len is sum of length's of
			 * remaining msdu's other than parent.
			 */
			if (!htt_mon_rx_handle_amsdu_packet(msdu, pdev,
							    &msg_word,
							    amsdu_len,
							    replenish_cnt)) {
				qdf_print("failed to handle amsdu packet");
				return 0;
			}
		}

next_pop:
		/* check if this is the last msdu */
		if (msdu_count) {
			msg_word += HTT_RX_IN_ORD_PADDR_IND_MSDU_DWORDS;
			paddr = htt_rx_in_ord_paddr_get(msg_word);
			next = htt_rx_in_order_netbuf_pop(pdev, paddr);
			if (qdf_unlikely(!next)) {
				qdf_print("netbuf pop failed!");
				*tail_msdu = NULL;
				return 0;
			}
			*replenish_cnt = *replenish_cnt + 1;
			if (msdu)
				qdf_nbuf_set_next(msdu, next);
			msdu = next;
		} else {
			*tail_msdu = msdu;
			if (msdu)
				qdf_nbuf_set_next(msdu, NULL);
		}
	}

	return 1;
}
#endif /* CONFIG_HL_SUPPORT */

#if defined(FEATURE_MONITOR_MODE_SUPPORT)
#if !defined(QCA6290_HEADERS_DEF) && !defined(QCA6390_HEADERS_DEF) && \
    !defined(QCA6490_HEADERS_DEF) && !defined(QCA6750_HEADERS_DEF)
static void
htt_rx_parse_ppdu_start_status(struct htt_host_rx_desc_base *rx_desc,
			       struct ieee80211_rx_status *rs)
{
	struct rx_ppdu_start *ppdu_start = &rx_desc->ppdu_start;

	/* RSSI */
	rs->rs_rssi = ppdu_start->rssi_comb;

	/* PHY rate */
	/*
	 * rs_ratephy coding
	 * [b3 - b0]
	 * 0 -> OFDM
	 * 1 -> CCK
	 * 2 -> HT
	 * 3 -> VHT
	 * OFDM / CCK
	 * [b7  - b4 ] => LSIG rate
	 * [b23 - b8 ] => service field
	 * (b'12 static/dynamic,
	 * b'14..b'13 BW for VHT)
	 * [b31 - b24 ] => Reserved
	 * HT / VHT
	 * [b15 - b4 ] => SIG A_2 12 LSBs
	 * [b31 - b16] => SIG A_1 16 LSBs
	 */
	if (ppdu_start->preamble_type == 0x4) {
		rs->rs_ratephy = ppdu_start->l_sig_rate_select;
		rs->rs_ratephy |= ppdu_start->l_sig_rate << 4;
		rs->rs_ratephy |= ppdu_start->service << 8;
	} else {
		rs->rs_ratephy = (ppdu_start->preamble_type & 0x4) ? 3 : 2;
#ifdef HELIUMPLUS
		rs->rs_ratephy |=
			(ppdu_start->ht_sig_vht_sig_ah_sig_a_2 & 0xFFF) << 4;
		rs->rs_ratephy |=
			(ppdu_start->ht_sig_vht_sig_ah_sig_a_1 & 0xFFFF) << 16;
#else
		rs->rs_ratephy |= (ppdu_start->ht_sig_vht_sig_a_2 & 0xFFF) << 4;
		rs->rs_ratephy |=
			(ppdu_start->ht_sig_vht_sig_a_1 & 0xFFFF) << 16;
#endif
	}
}

/* Util fake function that has same prototype as qdf_nbuf_clone that just
 * returns the same nbuf
 */
static qdf_nbuf_t htt_rx_qdf_noclone_buf(qdf_nbuf_t buf)
{
	return buf;
}

/* This function is used by montior mode code to restitch an MSDU list
 * corresponding to an MPDU back into an MPDU by linking up the skbs.
 */
qdf_nbuf_t
htt_rx_restitch_mpdu_from_msdus(htt_pdev_handle pdev,
				qdf_nbuf_t head_msdu,
				struct ieee80211_rx_status *rx_status,
				unsigned int clone_not_reqd)
{
	qdf_nbuf_t msdu, mpdu_buf, prev_buf, msdu_orig, head_frag_list_cloned;
	unsigned int decap_format, wifi_hdr_len, sec_hdr_len, msdu_llc_len,
		 mpdu_buf_len, decap_hdr_pull_bytes, frag_list_sum_len, dir,
		 is_amsdu, is_first_frag, amsdu_pad, msdu_len;
	struct htt_host_rx_desc_base *rx_desc;
	char *hdr_desc;
	unsigned char *dest;
	struct ieee80211_frame *wh;
	struct ieee80211_qoscntl *qos;

	/* The nbuf has been pulled just beyond the status and points to the
	 * payload
	 */
	msdu_orig = head_msdu;
	rx_desc = htt_rx_desc(msdu_orig);

	/* Fill out the rx_status from the PPDU start and end fields */
	if (rx_desc->attention.first_mpdu) {
		htt_rx_parse_ppdu_start_status(rx_desc, rx_status);

		/* The timestamp is no longer valid - It will be valid only for
		 * the last MPDU
		 */
		rx_status->rs_tstamp.tsf = ~0;
	}

	decap_format =
		GET_FIELD(&rx_desc->msdu_start, RX_MSDU_START_2_DECAP_FORMAT);

	head_frag_list_cloned = NULL;

	/* Easy case - The MSDU status indicates that this is a non-decapped
	 * packet in RAW mode.
	 * return
	 */
	if (decap_format == HW_RX_DECAP_FORMAT_RAW) {
		/* Note that this path might suffer from headroom unavailabilty,
		 * but the RX status is usually enough
		 */
		if (clone_not_reqd)
			mpdu_buf = htt_rx_qdf_noclone_buf(head_msdu);
		else
			mpdu_buf = qdf_nbuf_clone(head_msdu);

		if (!mpdu_buf)
			goto mpdu_stitch_fail;

		prev_buf = mpdu_buf;

		frag_list_sum_len = 0;
		is_first_frag = 1;
		msdu_len = qdf_nbuf_len(mpdu_buf);

		/* Drop the zero-length msdu */
		if (!msdu_len)
			goto mpdu_stitch_fail;

		msdu_orig = qdf_nbuf_next(head_msdu);

		while (msdu_orig) {
			/* TODO: intra AMSDU padding - do we need it ??? */
			if (clone_not_reqd)
				msdu = htt_rx_qdf_noclone_buf(msdu_orig);
			else
				msdu = qdf_nbuf_clone(msdu_orig);

			if (!msdu)
				goto mpdu_stitch_fail;

			if (is_first_frag) {
				is_first_frag = 0;
				head_frag_list_cloned = msdu;
			}

			msdu_len = qdf_nbuf_len(msdu);
			/* Drop the zero-length msdu */
			if (!msdu_len)
				goto mpdu_stitch_fail;

			frag_list_sum_len += msdu_len;

			/* Maintain the linking of the cloned MSDUS */
			qdf_nbuf_set_next_ext(prev_buf, msdu);

			/* Move to the next */
			prev_buf = msdu;
			msdu_orig = qdf_nbuf_next(msdu_orig);
		}

		/* The last msdu length need be larger than HTT_FCS_LEN */
		if (msdu_len < HTT_FCS_LEN)
			goto mpdu_stitch_fail;

		qdf_nbuf_trim_tail(prev_buf, HTT_FCS_LEN);

		/* If there were more fragments to this RAW frame */
		if (head_frag_list_cloned) {
			qdf_nbuf_append_ext_list(mpdu_buf,
						 head_frag_list_cloned,
						 frag_list_sum_len);
		}

		goto mpdu_stitch_done;
	}

	/* Decap mode:
	 * Calculate the amount of header in decapped packet to knock off based
	 * on the decap type and the corresponding number of raw bytes to copy
	 * status header
	 */

	hdr_desc = &rx_desc->rx_hdr_status[0];

	/* Base size */
	wifi_hdr_len = sizeof(struct ieee80211_frame);
	wh = (struct ieee80211_frame *)hdr_desc;

	dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;
	if (dir == IEEE80211_FC1_DIR_DSTODS)
		wifi_hdr_len += 6;

	is_amsdu = 0;
	if (wh->i_fc[0] & QDF_IEEE80211_FC0_SUBTYPE_QOS) {
		qos = (struct ieee80211_qoscntl *)
		      (hdr_desc + wifi_hdr_len);
		wifi_hdr_len += 2;

		is_amsdu = (qos->i_qos[0] & IEEE80211_QOS_AMSDU);
	}

	/* TODO: Any security headers associated with MPDU */
	sec_hdr_len = 0;

	/* MSDU related stuff LLC - AMSDU subframe header etc */
	msdu_llc_len = is_amsdu ? (14 + 8) : 8;

	mpdu_buf_len = wifi_hdr_len + sec_hdr_len + msdu_llc_len;

	/* "Decap" header to remove from MSDU buffer */
	decap_hdr_pull_bytes = 14;

	/* Allocate a new nbuf for holding the 802.11 header retrieved from the
	 * status of the now decapped first msdu. Leave enough headroom for
	 * accomodating any radio-tap /prism like PHY header
	 */
#define HTT_MAX_MONITOR_HEADER (512)
	mpdu_buf = qdf_nbuf_alloc(pdev->osdev,
				  HTT_MAX_MONITOR_HEADER + mpdu_buf_len,
				  HTT_MAX_MONITOR_HEADER, 4, false);

	if (!mpdu_buf)
		goto mpdu_stitch_fail;

	/* Copy the MPDU related header and enc headers into the first buffer
	 * - Note that there can be a 2 byte pad between heaader and enc header
	 */

	prev_buf = mpdu_buf;
	dest = qdf_nbuf_put_tail(prev_buf, wifi_hdr_len);
	if (!dest)
		goto mpdu_stitch_fail;
	qdf_mem_copy(dest, hdr_desc, wifi_hdr_len);
	hdr_desc += wifi_hdr_len;

	/* NOTE - This padding is present only in the RAW header status - not
	 * when the MSDU data payload is in RAW format.
	 */
	/* Skip the "IV pad" */
	if (wifi_hdr_len & 0x3)
		hdr_desc += 2;

	/* The first LLC len is copied into the MPDU buffer */
	frag_list_sum_len = 0;
	frag_list_sum_len -= msdu_llc_len;

	msdu_orig = head_msdu;
	is_first_frag = 1;
	amsdu_pad = 0;

	while (msdu_orig) {
		/* TODO: intra AMSDU padding - do we need it ??? */
		if (clone_not_reqd)
			msdu = htt_rx_qdf_noclone_buf(msdu_orig);
		else
			msdu = qdf_nbuf_clone(msdu_orig);

		if (!msdu)
			goto mpdu_stitch_fail;

		if (is_first_frag) {
			is_first_frag = 0;
			head_frag_list_cloned = msdu;
		} else {
			/* Maintain the linking of the cloned MSDUS */
			qdf_nbuf_set_next_ext(prev_buf, msdu);

			/* Reload the hdr ptr only on non-first MSDUs */
			rx_desc = htt_rx_desc(msdu_orig);
			hdr_desc = &rx_desc->rx_hdr_status[0];
		}

		/* Copy this buffers MSDU related status into the prev buffer */
		dest = qdf_nbuf_put_tail(prev_buf, msdu_llc_len + amsdu_pad);
		dest += amsdu_pad;
		qdf_mem_copy(dest, hdr_desc, msdu_llc_len);

		/* Push the MSDU buffer beyond the decap header */
		qdf_nbuf_pull_head(msdu, decap_hdr_pull_bytes);
		frag_list_sum_len +=
			msdu_llc_len + qdf_nbuf_len(msdu) + amsdu_pad;

		/*
		 * Set up intra-AMSDU pad to be added to start of next buffer -
		 * AMSDU pad is 4 byte pad on AMSDU subframe
		 */
		amsdu_pad = (msdu_llc_len + qdf_nbuf_len(msdu)) & 0x3;
		amsdu_pad = amsdu_pad ? (4 - amsdu_pad) : 0;

		/*
		 * TODO FIXME How do we handle MSDUs that have fraglist - Should
		 * probably iterate all the frags cloning them along the way and
		 * and also updating the prev_buf pointer
		 */

		/* Move to the next */
		prev_buf = msdu;
		msdu_orig = qdf_nbuf_next(msdu_orig);
	}

	/* TODO: Convert this to suitable qdf routines */
	qdf_nbuf_append_ext_list(mpdu_buf, head_frag_list_cloned,
				 frag_list_sum_len);

mpdu_stitch_done:
	/* Check if this buffer contains the PPDU end status for TSF */
	if (rx_desc->attention.last_mpdu)
#ifdef HELIUMPLUS
		rx_status->rs_tstamp.tsf =
			rx_desc->ppdu_end.rx_pkt_end.phy_timestamp_1_lower_32;
#else
		rx_status->rs_tstamp.tsf = rx_desc->ppdu_end.tsf_timestamp;
#endif
	/* All the nbufs have been linked into the ext list
	 * and then unlink the nbuf list
	 */
	if (clone_not_reqd) {
		msdu = head_msdu;
		while (msdu) {
			msdu_orig = msdu;
			msdu = qdf_nbuf_next(msdu);
			qdf_nbuf_set_next(msdu_orig, NULL);
		}
	}

	return mpdu_buf;

mpdu_stitch_fail:
	/* Free these alloced buffers and the orig buffers in non-clone case */
	if (!clone_not_reqd) {
		/* Free the head buffer */
		if (mpdu_buf)
			qdf_nbuf_free(mpdu_buf);

		/* Free the partial list */
		while (head_frag_list_cloned) {
			msdu = head_frag_list_cloned;
			head_frag_list_cloned =
				qdf_nbuf_next_ext(head_frag_list_cloned);
			qdf_nbuf_free(msdu);
		}
	} else {
		/* Free the alloced head buffer */
		if (decap_format != HW_RX_DECAP_FORMAT_RAW)
			if (mpdu_buf)
				qdf_nbuf_free(mpdu_buf);

		/* Free the orig buffers */
		msdu = head_msdu;
		while (msdu) {
			msdu_orig = msdu;
			msdu = qdf_nbuf_next(msdu);
			qdf_nbuf_free(msdu_orig);
		}
	}

	return NULL;
}
#endif
#endif
