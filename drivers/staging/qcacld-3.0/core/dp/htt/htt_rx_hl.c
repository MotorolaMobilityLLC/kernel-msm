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

/*
 * This function is used both below within this file (which the compiler
 * will hopefully inline), and out-line from other files via the
 * htt_rx_msdu_first_msdu_flag function pointer.
 */
static inline bool
htt_rx_msdu_first_msdu_flag_hl(htt_pdev_handle pdev, void *msdu_desc)
{
	return ((u_int8_t *)msdu_desc - sizeof(struct hl_htt_rx_ind_base))
		[HTT_ENDIAN_BYTE_IDX_SWAP(HTT_RX_IND_HL_FLAG_OFFSET)] &
		HTT_RX_IND_HL_FLAG_FIRST_MSDU ? true : false;
}

u_int16_t
htt_rx_msdu_rx_desc_size_hl(
	htt_pdev_handle pdev,
	void *msdu_desc
		)
{
	return ((u_int8_t *)(msdu_desc) - HTT_RX_IND_HL_BYTES)
		[HTT_ENDIAN_BYTE_IDX_SWAP(HTT_RX_IND_HL_RX_DESC_LEN_OFFSET)];
}

#ifdef CHECKSUM_OFFLOAD
static void
htt_set_checksum_result_hl(qdf_nbuf_t msdu,
			   struct htt_host_rx_desc_base *rx_desc)
{
	u_int8_t flag = ((u_int8_t *)rx_desc -
				sizeof(struct hl_htt_rx_ind_base))[
					HTT_ENDIAN_BYTE_IDX_SWAP(
						HTT_RX_IND_HL_FLAG_OFFSET)];

	int is_ipv6 = flag & HTT_RX_IND_HL_FLAG_IPV6 ? 1 : 0;
	int is_tcp = flag & HTT_RX_IND_HL_FLAG_TCP ? 1 : 0;
	int is_udp = flag & HTT_RX_IND_HL_FLAG_UDP ? 1 : 0;

	qdf_nbuf_rx_cksum_t cksum = {
		QDF_NBUF_RX_CKSUM_NONE,
		QDF_NBUF_RX_CKSUM_NONE,
		0
	};

	switch ((is_udp << 2) | (is_tcp << 1) | (is_ipv6 << 0)) {
	case 0x4:
		cksum.l4_type = QDF_NBUF_RX_CKSUM_UDP;
		break;
	case 0x2:
		cksum.l4_type = QDF_NBUF_RX_CKSUM_TCP;
		break;
	case 0x5:
		cksum.l4_type = QDF_NBUF_RX_CKSUM_UDPIPV6;
		break;
	case 0x3:
		cksum.l4_type = QDF_NBUF_RX_CKSUM_TCPIPV6;
		break;
	default:
		cksum.l4_type = QDF_NBUF_RX_CKSUM_NONE;
		break;
	}
	if (cksum.l4_type != (qdf_nbuf_l4_rx_cksum_type_t)
				QDF_NBUF_RX_CKSUM_NONE) {
		cksum.l4_result = flag & HTT_RX_IND_HL_FLAG_C4_FAILED ?
			QDF_NBUF_RX_CKSUM_NONE :
				QDF_NBUF_RX_CKSUM_TCP_UDP_UNNECESSARY;
	}
	qdf_nbuf_set_rx_cksum(msdu, &cksum);
}
#else
static inline
void htt_set_checksum_result_hl(qdf_nbuf_t msdu,
				struct htt_host_rx_desc_base *rx_desc)
{
}
#endif

/**
 * htt_rx_fill_ring_count() - replenish rx msdu buffer
 * @pdev: Handle (pointer) to HTT pdev.
 *
 * This funciton will replenish the rx buffer to the max number
 * that can be kept in the ring
 *
 * Return: None
 */
void htt_rx_fill_ring_count(htt_pdev_handle pdev)
{
}

/**
 * htt_rx_mpdu_desc_list_next_hl() - provides an abstract way to obtain
 *				     the next MPDU descriptor
 * @pdev: the HTT instance the rx data was received on
 * @rx_ind_msg: the netbuf containing the rx indication message
 *
 * for HL, the returned value is not mpdu_desc,
 * it's translated hl_rx_desc just after the hl_ind_msg
 * for HL AMSDU, we can't point to payload now, because
 * hl rx desc is not fixed, we can't retrieve the desc
 * by minus rx_desc_size when release. keep point to hl rx desc
 * now
 *
 * Return: next abstract rx descriptor from the series of MPDUs
 *		   referenced by an rx ind msg
 */
static inline void *
htt_rx_mpdu_desc_list_next_hl(htt_pdev_handle pdev, qdf_nbuf_t rx_ind_msg)
{
	void *mpdu_desc = (void *)qdf_nbuf_data(rx_ind_msg);
	return mpdu_desc;
}

/**
 * htt_rx_msdu_desc_retrieve_hl() - Retrieve a previously-stored rx descriptor
 *				    from a MSDU buffer
 * @pdev: the HTT instance the rx data was received on
 * @msdu - the buffer containing the MSDU payload
 *
 * currently for HL AMSDU, we don't point to payload.
 * we shift to payload in ol_rx_deliver later
 *
 * Return: the corresponding abstract rx MSDU descriptor
 */
static inline void *
htt_rx_msdu_desc_retrieve_hl(htt_pdev_handle pdev, qdf_nbuf_t msdu)
{
	return qdf_nbuf_data(msdu);
}

static
bool htt_rx_mpdu_is_encrypted_hl(htt_pdev_handle pdev, void *mpdu_desc)
{
	if (htt_rx_msdu_first_msdu_flag_hl(pdev, mpdu_desc) == true) {
		/* Fix Me: only for little endian */
		struct hl_htt_rx_desc_base *rx_desc =
			(struct hl_htt_rx_desc_base *)mpdu_desc;

		return HTT_WORD_GET(*(u_int32_t *)rx_desc,
					HTT_HL_RX_DESC_MPDU_ENC);
	} else {
		/* not first msdu, no encrypt info for hl */
		qdf_print(
			"Error: get encrypted from a not-first msdu.\n");
		qdf_assert(0);
		return false;
	}
}

static inline bool
htt_rx_msdu_chan_info_present_hl(htt_pdev_handle pdev, void *mpdu_desc)
{
	if (htt_rx_msdu_first_msdu_flag_hl(pdev, mpdu_desc) == true &&
	    HTT_WORD_GET(*(u_int32_t *)mpdu_desc,
			 HTT_HL_RX_DESC_CHAN_INFO_PRESENT))
		return true;

	return false;
}

static bool
htt_rx_msdu_center_freq_hl(htt_pdev_handle pdev,
			   struct ol_txrx_peer_t *peer,
			   void *mpdu_desc,
			   uint16_t *primary_chan_center_freq_mhz,
			   uint16_t *contig_chan1_center_freq_mhz,
			   uint16_t *contig_chan2_center_freq_mhz,
			   uint8_t *phy_mode)
{
	int pn_len, index;
	uint32_t *chan_info;

	index = htt_rx_msdu_is_wlan_mcast(pdev, mpdu_desc) ?
		txrx_sec_mcast : txrx_sec_ucast;

	pn_len = (peer ?
			pdev->txrx_pdev->rx_pn[peer->security[index].sec_type].
								len : 0);
	chan_info = (uint32_t *)((uint8_t *)mpdu_desc +
			HTT_HL_RX_DESC_PN_OFFSET + pn_len);

	if (htt_rx_msdu_chan_info_present_hl(pdev, mpdu_desc)) {
		if (primary_chan_center_freq_mhz)
			*primary_chan_center_freq_mhz =
				HTT_WORD_GET(
					*chan_info,
					HTT_CHAN_INFO_PRIMARY_CHAN_CENTER_FREQ);
		if (contig_chan1_center_freq_mhz)
			*contig_chan1_center_freq_mhz =
				HTT_WORD_GET(
					*chan_info,
					HTT_CHAN_INFO_CONTIG_CHAN1_CENTER_FREQ);
		chan_info++;
		if (contig_chan2_center_freq_mhz)
			*contig_chan2_center_freq_mhz =
				HTT_WORD_GET(
					*chan_info,
					HTT_CHAN_INFO_CONTIG_CHAN2_CENTER_FREQ);
		if (phy_mode)
			*phy_mode =
				HTT_WORD_GET(*chan_info,
					     HTT_CHAN_INFO_PHY_MODE);
		return true;
	}

	if (primary_chan_center_freq_mhz)
		*primary_chan_center_freq_mhz = 0;
	if (contig_chan1_center_freq_mhz)
		*contig_chan1_center_freq_mhz = 0;
	if (contig_chan2_center_freq_mhz)
		*contig_chan2_center_freq_mhz = 0;
	if (phy_mode)
		*phy_mode = 0;
	return false;
}

static bool
htt_rx_msdu_desc_key_id_hl(htt_pdev_handle htt_pdev,
			   void *mpdu_desc, u_int8_t *key_id)
{
	if (htt_rx_msdu_first_msdu_flag_hl(htt_pdev, mpdu_desc) == true) {
		/* Fix Me: only for little endian */
		struct hl_htt_rx_desc_base *rx_desc =
			(struct hl_htt_rx_desc_base *)mpdu_desc;

		*key_id = rx_desc->key_id_oct;
		return true;
	}

	return false;
}

/**
 * htt_rx_mpdu_desc_retry_hl() - Returns the retry bit from the Rx descriptor
 *                               for the High Latency driver
 * @pdev: Handle (pointer) to HTT pdev.
 * @mpdu_desc: Void pointer to the Rx descriptor for MPDU
 *             before the beginning of the payload.
 *
 *  This function returns the retry bit of the 802.11 header for the
 *  provided rx MPDU descriptor. For the high latency driver, this function
 *  pretends as if the retry bit is never set so that the mcast duplicate
 *  detection never fails.
 *
 * Return:        boolean -- false always for HL
 */
static inline bool
htt_rx_mpdu_desc_retry_hl(htt_pdev_handle pdev, void *mpdu_desc)
{
	return false;
}

static int
htt_rx_amsdu_pop_hl(
	htt_pdev_handle pdev,
	qdf_nbuf_t rx_ind_msg,
	qdf_nbuf_t *head_msdu,
	qdf_nbuf_t *tail_msdu,
	uint32_t *msdu_count)
{
	pdev->rx_desc_size_hl =
		(qdf_nbuf_data(rx_ind_msg))
		[HTT_ENDIAN_BYTE_IDX_SWAP(
				HTT_RX_IND_HL_RX_DESC_LEN_OFFSET)];

	/* point to the rx desc */
	qdf_nbuf_pull_head(rx_ind_msg,
			   sizeof(struct hl_htt_rx_ind_base));
	*head_msdu = *tail_msdu = rx_ind_msg;

	htt_set_checksum_result_hl(rx_ind_msg,
				   (struct htt_host_rx_desc_base *)
				   (qdf_nbuf_data(rx_ind_msg)));

	qdf_nbuf_set_next(*tail_msdu, NULL);
	return 0;
}

static int
htt_rx_frag_pop_hl(
	htt_pdev_handle pdev,
	qdf_nbuf_t frag_msg,
	qdf_nbuf_t *head_msdu,
	qdf_nbuf_t *tail_msdu,
	uint32_t *msdu_count)
{
	qdf_nbuf_pull_head(frag_msg, HTT_RX_FRAG_IND_BYTES);
	pdev->rx_desc_size_hl =
		(qdf_nbuf_data(frag_msg))
		[HTT_ENDIAN_BYTE_IDX_SWAP(
				HTT_RX_IND_HL_RX_DESC_LEN_OFFSET)];

	/* point to the rx desc */
	qdf_nbuf_pull_head(frag_msg,
			   sizeof(struct hl_htt_rx_ind_base));
	*head_msdu = *tail_msdu = frag_msg;

	qdf_nbuf_set_next(*tail_msdu, NULL);
	return 1;
}

static inline int
htt_rx_offload_msdu_cnt_hl(htt_pdev_handle pdev)
{
	return 1;
}

static inline int
htt_rx_offload_msdu_pop_hl(htt_pdev_handle pdev,
			   qdf_nbuf_t offload_deliver_msg,
			   int *vdev_id,
			   int *peer_id,
			   int *tid,
			   u_int8_t *fw_desc,
			   qdf_nbuf_t *head_buf,
			   qdf_nbuf_t *tail_buf)
{
	qdf_nbuf_t buf;
	u_int32_t *msdu_hdr, msdu_len;
	int ret = 0;

	*head_buf = *tail_buf = buf = offload_deliver_msg;
	msdu_hdr = (u_int32_t *)qdf_nbuf_data(buf);
	/* First dword */

	/* Second dword */
	msdu_hdr++;
	msdu_len = HTT_RX_OFFLOAD_DELIVER_IND_MSDU_LEN_GET(*msdu_hdr);
	*peer_id = HTT_RX_OFFLOAD_DELIVER_IND_MSDU_PEER_ID_GET(*msdu_hdr);

	/* Third dword */
	msdu_hdr++;
	*vdev_id = HTT_RX_OFFLOAD_DELIVER_IND_MSDU_VDEV_ID_GET(*msdu_hdr);
	*tid = HTT_RX_OFFLOAD_DELIVER_IND_MSDU_TID_GET(*msdu_hdr);
	*fw_desc = HTT_RX_OFFLOAD_DELIVER_IND_MSDU_DESC_GET(*msdu_hdr);

	qdf_nbuf_pull_head(buf, HTT_RX_OFFLOAD_DELIVER_IND_MSDU_HDR_BYTES
			+ HTT_RX_OFFLOAD_DELIVER_IND_HDR_BYTES);

	if (msdu_len <= qdf_nbuf_len(buf)) {
		qdf_nbuf_set_pktlen(buf, msdu_len);
	} else {
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_ERROR,
			  "%s: drop frame with invalid msdu len %d %d",
			  __func__, msdu_len, (int)qdf_nbuf_len(buf));
		qdf_nbuf_free(offload_deliver_msg);
		ret = -1;
	}

	return ret;
}

static uint16_t
htt_rx_mpdu_desc_seq_num_hl(htt_pdev_handle pdev, void *mpdu_desc,
			    bool update_seq_num)
{
	if (pdev->rx_desc_size_hl) {
		if (update_seq_num)
			return pdev->cur_seq_num_hl =
			       (u_int16_t)(HTT_WORD_GET(*(u_int32_t *)mpdu_desc,
					   HTT_HL_RX_DESC_MPDU_SEQ_NUM));
		else
			return (u_int16_t)(HTT_WORD_GET(*(u_int32_t *)mpdu_desc,
					   HTT_HL_RX_DESC_MPDU_SEQ_NUM));
	} else {
		return (u_int16_t)(pdev->cur_seq_num_hl);
	}
}

static void
htt_rx_mpdu_desc_pn_hl(
	htt_pdev_handle pdev,
	void *mpdu_desc,
	union htt_rx_pn_t *pn,
	int pn_len_bits)
{
	if (htt_rx_msdu_first_msdu_flag_hl(pdev, mpdu_desc) == true) {
		/* Fix Me: only for little endian */
		struct hl_htt_rx_desc_base *rx_desc =
			(struct hl_htt_rx_desc_base *)mpdu_desc;
		u_int32_t *word_ptr = (u_int32_t *)pn->pn128;

		/* TODO: for Host of big endian */
		switch (pn_len_bits) {
		case 128:
			/* bits 128:64 */
			*(word_ptr + 3) = rx_desc->pn_127_96;
			/* bits 63:0 */
			*(word_ptr + 2) = rx_desc->pn_95_64;
		case 48:
			/* bits 48:0
			 * copy 64 bits
			 */
			*(word_ptr + 1) = rx_desc->u0.pn_63_32;
		case 24:
			/* bits 23:0
			 * copy 32 bits
			 */
			*(word_ptr + 0) = rx_desc->pn_31_0;
			break;
		default:
			QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_ERROR,
				  "Error: invalid length spec (%d bits) for PN",
				  pn_len_bits);
			qdf_assert(0);
			break;
		};
	} else {
		/* not first msdu, no pn info */
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_ERROR,
			  "Error: get pn from a not-first msdu.");
		qdf_assert(0);
	}
}

/**
 * htt_rx_mpdu_desc_tid_hl() - Returns the TID value from the Rx descriptor
 *                             for High Latency driver
 * @pdev:                        Handle (pointer) to HTT pdev.
 * @mpdu_desc:                   Void pointer to the Rx descriptor for the MPDU
 *                               before the beginning of the payload.
 *
 * This function returns the TID set in the 802.11 QoS Control for the MPDU
 * in the packet header, by looking at the mpdu_start of the Rx descriptor.
 * Rx descriptor gets a copy of the TID from the MAC.
 * For the HL driver, this is currently uimplemented and always returns
 * an invalid tid. It is the responsibility of the caller to make
 * sure that return value is checked for valid range.
 *
 * Return:        Invalid TID value (0xff) for HL driver.
 */
static inline uint8_t
htt_rx_mpdu_desc_tid_hl(htt_pdev_handle pdev, void *mpdu_desc)
{
	return 0xff;  /* Invalid TID */
}

static inline bool
htt_rx_msdu_desc_completes_mpdu_hl(htt_pdev_handle pdev, void *msdu_desc)
{
	return (
		((u_int8_t *)(msdu_desc) - sizeof(struct hl_htt_rx_ind_base))
		[HTT_ENDIAN_BYTE_IDX_SWAP(HTT_RX_IND_HL_FLAG_OFFSET)]
		& HTT_RX_IND_HL_FLAG_LAST_MSDU)
		? true : false;
}

static inline int
htt_rx_msdu_has_wlan_mcast_flag_hl(htt_pdev_handle pdev, void *msdu_desc)
{
	/* currently, only first msdu has hl rx_desc */
	return htt_rx_msdu_first_msdu_flag_hl(pdev, msdu_desc) == true;
}

static inline bool
htt_rx_msdu_is_wlan_mcast_hl(htt_pdev_handle pdev, void *msdu_desc)
{
	struct hl_htt_rx_desc_base *rx_desc =
		(struct hl_htt_rx_desc_base *)msdu_desc;

	return
		HTT_WORD_GET(*(u_int32_t *)rx_desc, HTT_HL_RX_DESC_MCAST_BCAST);
}

static inline int
htt_rx_msdu_is_frag_hl(htt_pdev_handle pdev, void *msdu_desc)
{
	struct hl_htt_rx_desc_base *rx_desc =
		(struct hl_htt_rx_desc_base *)msdu_desc;

	return
		HTT_WORD_GET(*(u_int32_t *)rx_desc, HTT_HL_RX_DESC_MCAST_BCAST);
}

int htt_rx_attach(struct htt_pdev_t *pdev)
{
	pdev->rx_ring.size = HTT_RX_RING_SIZE_MIN;
	HTT_ASSERT2(IS_PWR2(pdev->rx_ring.size));
	pdev->rx_ring.size_mask = pdev->rx_ring.size - 1;
	/* host can force ring base address if it wish to do so */
	pdev->rx_ring.base_paddr = 0;
	htt_rx_amsdu_pop = htt_rx_amsdu_pop_hl;
	htt_rx_frag_pop = htt_rx_frag_pop_hl;
	htt_rx_offload_msdu_cnt = htt_rx_offload_msdu_cnt_hl;
	htt_rx_offload_msdu_pop = htt_rx_offload_msdu_pop_hl;
	htt_rx_mpdu_desc_list_next = htt_rx_mpdu_desc_list_next_hl;
	htt_rx_mpdu_desc_retry = htt_rx_mpdu_desc_retry_hl;
	htt_rx_mpdu_desc_seq_num = htt_rx_mpdu_desc_seq_num_hl;
	htt_rx_mpdu_desc_pn = htt_rx_mpdu_desc_pn_hl;
	htt_rx_mpdu_desc_tid = htt_rx_mpdu_desc_tid_hl;
	htt_rx_msdu_desc_completes_mpdu = htt_rx_msdu_desc_completes_mpdu_hl;
	htt_rx_msdu_first_msdu_flag = htt_rx_msdu_first_msdu_flag_hl;
	htt_rx_msdu_has_wlan_mcast_flag = htt_rx_msdu_has_wlan_mcast_flag_hl;
	htt_rx_msdu_is_wlan_mcast = htt_rx_msdu_is_wlan_mcast_hl;
	htt_rx_msdu_is_frag = htt_rx_msdu_is_frag_hl;
	htt_rx_msdu_desc_retrieve = htt_rx_msdu_desc_retrieve_hl;
	htt_rx_mpdu_is_encrypted = htt_rx_mpdu_is_encrypted_hl;
	htt_rx_msdu_desc_key_id = htt_rx_msdu_desc_key_id_hl;
	htt_rx_msdu_chan_info_present = htt_rx_msdu_chan_info_present_hl;
	htt_rx_msdu_center_freq = htt_rx_msdu_center_freq_hl;

	/*
	 * HL case, the rx descriptor can be different sizes for
	 * different sub-types of RX_IND messages, e.g. for the
	 * initial vs. interior vs. final MSDUs within a PPDU.
	 * The size of each RX_IND message's rx desc is read from
	 * a field within the RX_IND message itself.
	 * In the meantime, until the rx_desc_size_hl variable is
	 * set to its real value based on the RX_IND message,
	 * initialize it to a reasonable value (zero).
	 */
	pdev->rx_desc_size_hl = 0;
	return 0;	/* success */
}
