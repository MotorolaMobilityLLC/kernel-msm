/*
 * Copyright (c) 2011-2021 The Linux Foundation. All rights reserved.
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

/*-
 * Copyright (c) 2002-2007 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <ol_htt_api.h>
#include <ol_txrx_api.h>
#include <ol_txrx_htt_api.h>
#include <ol_htt_rx_api.h>
#include <ol_rx_reorder.h>
#include <ol_rx_pn.h>
#include <ol_rx_fwd.h>
#include <ol_rx.h>
#include <ol_txrx_internal.h>
#include <ol_ctrl_txrx_api.h>
#include <ol_txrx_peer_find.h>
#include <qdf_nbuf.h>
#include <qdf_util.h>
#include <athdefs.h>
#include <qdf_mem.h>
#include <ol_rx_defrag.h>
#include <enet.h>
#include <qdf_time.h>           /* qdf_system_time */
#include <wlan_pkt_capture_ucfg_api.h>

#define DEFRAG_IEEE80211_ADDR_EQ(a1, a2) \
	(!qdf_mem_cmp(a1, a2, QDF_MAC_ADDR_SIZE))

#define DEFRAG_IEEE80211_ADDR_COPY(dst, src) \
	qdf_mem_copy(dst, src, QDF_MAC_ADDR_SIZE)

#define DEFRAG_IEEE80211_QOS_HAS_SEQ(wh) \
	(((wh)->i_fc[0] & \
	  (IEEE80211_FC0_TYPE_MASK | QDF_IEEE80211_FC0_SUBTYPE_QOS)) == \
	 (IEEE80211_FC0_TYPE_DATA | QDF_IEEE80211_FC0_SUBTYPE_QOS))

#define DEFRAG_IEEE80211_QOS_GET_TID(_x) \
	((_x)->i_qos[0] & IEEE80211_QOS_TID)

const struct ol_rx_defrag_cipher f_ccmp = {
	"AES-CCM",
	IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_EXTIVLEN,
	IEEE80211_WEP_MICLEN,
	0,
};

const struct ol_rx_defrag_cipher f_tkip = {
	"TKIP",
	IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_EXTIVLEN,
	IEEE80211_WEP_CRCLEN,
	IEEE80211_WEP_MICLEN,
};

const struct ol_rx_defrag_cipher f_wep = {
	"WEP",
	IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN,
	IEEE80211_WEP_CRCLEN,
	0,
};

#if defined(CONFIG_HL_SUPPORT)

/**
 * ol_rx_frag_get_mac_hdr() - retrieve mac header
 * @htt_pdev: pointer to htt pdev handle
 * @frag: rx fragment
 *
 * Return: pointer to ieee mac header of frag
 */
static struct ieee80211_frame *ol_rx_frag_get_mac_hdr(
	htt_pdev_handle htt_pdev, qdf_nbuf_t frag)
{
	void *rx_desc;
	int rx_desc_len;

	rx_desc = htt_rx_msdu_desc_retrieve(htt_pdev, frag);
	rx_desc_len = htt_rx_msdu_rx_desc_size_hl(htt_pdev, rx_desc);
	return (struct ieee80211_frame *)(qdf_nbuf_data(frag) + rx_desc_len);
}

/**
 * ol_rx_frag_pull_hdr() - point to payload of rx frag
 * @htt_pdev: pointer to htt pdev handle
 * @frag: rx fragment
 * @hdrsize: header size
 *
 * Return: None
 */
static void ol_rx_frag_pull_hdr(htt_pdev_handle htt_pdev,
	qdf_nbuf_t frag, int hdrsize)
{
	void *rx_desc;
	int rx_desc_len;

	rx_desc = htt_rx_msdu_desc_retrieve(htt_pdev, frag);
	rx_desc_len = htt_rx_msdu_rx_desc_size_hl(htt_pdev, rx_desc);
	qdf_nbuf_pull_head(frag, rx_desc_len + hdrsize);
}

/**
 * ol_rx_frag_desc_adjust() - adjust rx frag descriptor position
 * @pdev: pointer to txrx handle
 * @msdu: msdu
 * @rx_desc_old_position: rx descriptor old position
 * @ind_old_position:index of old position
 * @rx_desc_len: rx desciptor length
 *
 * Return: None
 */
static void
ol_rx_frag_desc_adjust(ol_txrx_pdev_handle pdev,
		       qdf_nbuf_t msdu,
			void **rx_desc_old_position,
			void **ind_old_position, int *rx_desc_len)
{
	*rx_desc_old_position = htt_rx_msdu_desc_retrieve(pdev->htt_pdev,
									msdu);
	*ind_old_position = *rx_desc_old_position - HTT_RX_IND_HL_BYTES;
	*rx_desc_len = htt_rx_msdu_rx_desc_size_hl(pdev->htt_pdev,
			*rx_desc_old_position);
}

/**
 * ol_rx_frag_restructure() - point to payload for HL
 * @pdev: physical device object
 * @msdu: the buffer containing the MSDU payload
 * @rx_desc_old_position: rx MSDU descriptor
 * @ind_old_position: rx msdu indication
 * @f_type: pointing to rx defrag cipher
 * @rx_desc_len: length by which rx descriptor to move
 *
 * Return: None
 */
static void
ol_rx_frag_restructure(
	ol_txrx_pdev_handle pdev,
	qdf_nbuf_t msdu,
	void *rx_desc_old_position,
	void *ind_old_position,
	const struct ol_rx_defrag_cipher *f_type,
	int rx_desc_len)
{
	if ((!ind_old_position) || (!rx_desc_old_position)) {
		ol_txrx_err("ind_old_position,rx_desc_old_position is NULL\n");
		ASSERT(0);
		return;
	}
	/* move rx description*/
	qdf_mem_move(rx_desc_old_position + f_type->ic_header,
		     rx_desc_old_position, rx_desc_len);
	/* move rx indication*/
	qdf_mem_move(ind_old_position + f_type->ic_header, ind_old_position,
		     HTT_RX_IND_HL_BYTES);
}

/**
 * ol_rx_get_desc_len() - point to payload for HL
 * @htt_pdev: the HTT instance the rx data was received on
 * @wbuf: buffer containing the MSDU payload
 * @rx_desc_old_position: rx MSDU descriptor
 *
 * Return: Return the HL rx desc size
 */
static
int ol_rx_get_desc_len(htt_pdev_handle htt_pdev,
			qdf_nbuf_t wbuf,
			void **rx_desc_old_position)
{
	int rx_desc_len = 0;
	*rx_desc_old_position = htt_rx_msdu_desc_retrieve(htt_pdev, wbuf);
	rx_desc_len = htt_rx_msdu_rx_desc_size_hl(htt_pdev,
			*rx_desc_old_position);

	return rx_desc_len;
}

/**
 * ol_rx_defrag_push_rx_desc() - point to payload for HL
 * @nbuf: buffer containing the MSDU payload
 * @rx_desc_old_position: rx MSDU descriptor
 * @ind_old_position: rx msdu indication
 * @rx_desc_len: HL rx desc size
 *
 * Return: Return the HL rx desc size
 */
static
void ol_rx_defrag_push_rx_desc(qdf_nbuf_t nbuf,
				void *rx_desc_old_position,
				void *ind_old_position,
				int rx_desc_len)
{
	qdf_nbuf_push_head(nbuf, rx_desc_len);
	qdf_mem_move(
		qdf_nbuf_data(nbuf), rx_desc_old_position, rx_desc_len);
	qdf_mem_move(
		qdf_nbuf_data(nbuf) - HTT_RX_IND_HL_BYTES, ind_old_position,
		HTT_RX_IND_HL_BYTES);
}
#else

static inline struct ieee80211_frame *ol_rx_frag_get_mac_hdr(
	htt_pdev_handle htt_pdev,
	qdf_nbuf_t frag)
{
	return
		(struct ieee80211_frame *) qdf_nbuf_data(frag);
}

static inline void ol_rx_frag_pull_hdr(htt_pdev_handle htt_pdev,
	qdf_nbuf_t frag, int hdrsize)
{
	qdf_nbuf_pull_head(frag, hdrsize);
}

static inline void
ol_rx_frag_desc_adjust(ol_txrx_pdev_handle pdev,
		       qdf_nbuf_t msdu,
		       void **rx_desc_old_position,
		       void **ind_old_position, int *rx_desc_len)
{
	*rx_desc_old_position = NULL;
	*ind_old_position = NULL;
	*rx_desc_len = 0;
}

static inline void
ol_rx_frag_restructure(
		ol_txrx_pdev_handle pdev,
		qdf_nbuf_t msdu,
		void *rx_desc_old_position,
		void *ind_old_position,
		const struct ol_rx_defrag_cipher *f_type,
		int rx_desc_len)
{
	/* no op */
}

static inline
int ol_rx_get_desc_len(htt_pdev_handle htt_pdev,
			qdf_nbuf_t wbuf,
			void **rx_desc_old_position)
{
	return 0;
}

static inline
void ol_rx_defrag_push_rx_desc(qdf_nbuf_t nbuf,
			void *rx_desc_old_position,
			void *ind_old_position,
			int rx_desc_len)
{
	return;
}
#endif /* CONFIG_HL_SUPPORT */

/*
 * Process incoming fragments
 */
void
ol_rx_frag_indication_handler(ol_txrx_pdev_handle pdev,
			      qdf_nbuf_t rx_frag_ind_msg,
			      uint16_t peer_id, uint8_t tid)
{
	uint16_t seq_num;
	uint16_t seq_num_start, seq_num_end;
	struct ol_txrx_peer_t *peer;
	htt_pdev_handle htt_pdev;
	qdf_nbuf_t head_msdu, tail_msdu;
	void *rx_mpdu_desc;
	uint8_t pktlog_bit;
	uint32_t msdu_count = 0;
	int ret;
	void *rx_desc;

	if (tid >= OL_TXRX_NUM_EXT_TIDS) {
		ol_txrx_err("%s:  invalid tid, %u\n", __FUNCTION__, tid);
		return;
	}

	htt_pdev = pdev->htt_pdev;
	peer = ol_txrx_peer_find_by_id(pdev, peer_id);

	if (!ol_cfg_is_full_reorder_offload(pdev->ctrl_pdev) &&
	    htt_rx_ind_flush(pdev->htt_pdev, rx_frag_ind_msg) && peer) {
		htt_rx_frag_ind_flush_seq_num_range(pdev->htt_pdev,
						    rx_frag_ind_msg,
						    &seq_num_start,
						    &seq_num_end);
		/*
		 * Assuming flush indication for frags sent from target is
		 * separate from normal frames
		 */
		ol_rx_reorder_flush_frag(htt_pdev, peer, tid, seq_num_start);
	} else {
		uint32_t *msg_word;
		uint8_t *rx_ind_data;

		rx_ind_data = qdf_nbuf_data(rx_frag_ind_msg);
		msg_word = (uint32_t *)rx_ind_data;
		msdu_count = HTT_RX_IN_ORD_PADDR_IND_MSDU_CNT_GET(*(msg_word +
								    1));
	}

	pktlog_bit =
		(htt_rx_amsdu_rx_in_order_get_pktlog(rx_frag_ind_msg) == 0x01);
	ret = htt_rx_frag_pop(htt_pdev, rx_frag_ind_msg, &head_msdu,
			      &tail_msdu, &msdu_count);
	/* Return if msdu pop fails from rx hash table, as recovery
	 * is triggered and we exit gracefully.
	 */
	if (!ret)
		return;
	if (peer) {
		qdf_assert(head_msdu == tail_msdu);
		if (ol_cfg_is_full_reorder_offload(pdev->ctrl_pdev)) {
			rx_mpdu_desc =
				htt_rx_mpdu_desc_list_next(htt_pdev, head_msdu);
		} else {
			rx_mpdu_desc =
				htt_rx_mpdu_desc_list_next(htt_pdev,
							   rx_frag_ind_msg);
		}
		seq_num = htt_rx_mpdu_desc_seq_num(htt_pdev,
						   rx_mpdu_desc, true);
		OL_RX_ERR_STATISTICS_1(pdev, peer->vdev, peer, rx_mpdu_desc,
				       OL_RX_ERR_NONE_FRAG);
		ol_rx_send_pktlog_event(pdev, peer, head_msdu, pktlog_bit);
		rx_desc = htt_rx_msdu_desc_retrieve(pdev->htt_pdev, head_msdu);
		ol_rx_timestamp(pdev->ctrl_pdev, rx_desc, head_msdu);
		ol_rx_reorder_store_frag(pdev, peer, tid, seq_num, head_msdu);
	} else {
		/* invalid frame - discard it */
		if (ol_cfg_is_full_reorder_offload(pdev->ctrl_pdev))
			htt_rx_msdu_desc_retrieve(htt_pdev, head_msdu);
		else
			htt_rx_mpdu_desc_list_next(htt_pdev, rx_frag_ind_msg);

		ol_rx_send_pktlog_event(pdev, peer, head_msdu, pktlog_bit);
		htt_rx_desc_frame_free(htt_pdev, head_msdu);
	}
	/* request HTT to provide new rx MSDU buffers for the target to fill. */
	if (ol_cfg_is_full_reorder_offload(pdev->ctrl_pdev) &&
	    !pdev->cfg.is_high_latency)
		htt_rx_msdu_buff_in_order_replenish(htt_pdev, msdu_count);
	else
		htt_rx_msdu_buff_replenish(htt_pdev);
}

/*
 * Flushing fragments
 */
void
ol_rx_reorder_flush_frag(htt_pdev_handle htt_pdev,
			 struct ol_txrx_peer_t *peer,
			 unsigned int tid, uint16_t seq_num)
{
	struct ol_rx_reorder_array_elem_t *rx_reorder_array_elem;
	int seq;

	seq = seq_num & peer->tids_rx_reorder[tid].win_sz_mask;
	rx_reorder_array_elem = &peer->tids_rx_reorder[tid].array[seq];
	if (rx_reorder_array_elem->head) {
		ol_rx_frames_free(htt_pdev, rx_reorder_array_elem->head);
		rx_reorder_array_elem->head = NULL;
		rx_reorder_array_elem->tail = NULL;
	}
}

/*
 * Reorder and store fragments
 */
void
ol_rx_reorder_store_frag(ol_txrx_pdev_handle pdev,
			 struct ol_txrx_peer_t *peer,
			 unsigned int tid, uint16_t seq_num, qdf_nbuf_t frag)
{
	struct ieee80211_frame *fmac_hdr, *mac_hdr;
	uint8_t fragno, more_frag, all_frag_present = 0;
	struct ol_rx_reorder_array_elem_t *rx_reorder_array_elem;
	uint16_t frxseq, rxseq, seq;
	htt_pdev_handle htt_pdev = pdev->htt_pdev;
	void *rx_desc;
	uint8_t index;

	seq = seq_num & peer->tids_rx_reorder[tid].win_sz_mask;
	qdf_assert(seq == 0);
	rx_reorder_array_elem = &peer->tids_rx_reorder[tid].array[seq];

	mac_hdr = (struct ieee80211_frame *)
		ol_rx_frag_get_mac_hdr(htt_pdev, frag);
	rxseq = qdf_le16_to_cpu(*(uint16_t *) mac_hdr->i_seq) >>
		IEEE80211_SEQ_SEQ_SHIFT;
	fragno = qdf_le16_to_cpu(*(uint16_t *) mac_hdr->i_seq) &
		IEEE80211_SEQ_FRAG_MASK;
	more_frag = mac_hdr->i_fc[1] & IEEE80211_FC1_MORE_FRAG;

	rx_desc = htt_rx_msdu_desc_retrieve(htt_pdev, frag);
	qdf_assert(htt_rx_msdu_has_wlan_mcast_flag(htt_pdev, rx_desc));
	index = htt_rx_msdu_is_wlan_mcast(htt_pdev, rx_desc) ?
		txrx_sec_mcast : txrx_sec_ucast;

	/*
	 * Multicast/Broadcast frames should not be fragmented so drop
	 * such frames.
	 */
	if (index != txrx_sec_ucast) {
		ol_rx_frames_free(htt_pdev, frag);
		return;
	}

	if (peer->security[index].sec_type != htt_sec_type_none &&
	    !htt_rx_mpdu_is_encrypted(htt_pdev, rx_desc)) {
		ol_txrx_err("Unencrypted fragment received in security mode %d",
			    peer->security[index].sec_type);
		ol_rx_frames_free(htt_pdev, frag);
		return;
	}

	if ((!more_frag) && (!fragno) && (!rx_reorder_array_elem->head)) {
		rx_reorder_array_elem->head = frag;
		rx_reorder_array_elem->tail = frag;
		qdf_nbuf_set_next(frag, NULL);
		ol_rx_defrag(pdev, peer, tid, rx_reorder_array_elem->head);
		rx_reorder_array_elem->head = NULL;
		rx_reorder_array_elem->tail = NULL;
		return;
	}
	if (rx_reorder_array_elem->head) {
		fmac_hdr = (struct ieee80211_frame *)
			ol_rx_frag_get_mac_hdr(htt_pdev,
					       rx_reorder_array_elem->head);
		frxseq = qdf_le16_to_cpu(*(uint16_t *) fmac_hdr->i_seq) >>
			IEEE80211_SEQ_SEQ_SHIFT;
		if (rxseq != frxseq
		    || !DEFRAG_IEEE80211_ADDR_EQ(mac_hdr->i_addr1,
						 fmac_hdr->i_addr1)
		    || !DEFRAG_IEEE80211_ADDR_EQ(mac_hdr->i_addr2,
						 fmac_hdr->i_addr2)) {
			ol_rx_frames_free(htt_pdev,
					  rx_reorder_array_elem->head);
			rx_reorder_array_elem->head = NULL;
			rx_reorder_array_elem->tail = NULL;
			ol_txrx_err("\n ol_rx_reorder_store:%s mismatch\n",
				   (rxseq == frxseq)
				   ? "address"
				   : "seq number");
		}
	}

	ol_rx_fraglist_insert(htt_pdev, &rx_reorder_array_elem->head,
			      &rx_reorder_array_elem->tail, frag,
			      &all_frag_present);

	if (pdev->rx.flags.defrag_timeout_check)
		ol_rx_defrag_waitlist_remove(peer, tid);

	if (all_frag_present) {
		ol_rx_defrag(pdev, peer, tid, rx_reorder_array_elem->head);
		rx_reorder_array_elem->head = NULL;
		rx_reorder_array_elem->tail = NULL;
		peer->tids_rx_reorder[tid].defrag_timeout_ms = 0;
		peer->tids_last_seq[tid] = seq_num;
	} else if (pdev->rx.flags.defrag_timeout_check) {
		uint32_t now_ms = qdf_system_ticks_to_msecs(qdf_system_ticks());

		peer->tids_rx_reorder[tid].defrag_timeout_ms =
			now_ms + pdev->rx.defrag.timeout_ms;
		ol_rx_defrag_waitlist_add(peer, tid);
	}
}

/*
 * Insert and store fragments
 */
void
ol_rx_fraglist_insert(htt_pdev_handle htt_pdev,
		      qdf_nbuf_t *head_addr,
		      qdf_nbuf_t *tail_addr,
		      qdf_nbuf_t frag, uint8_t *all_frag_present)
{
	qdf_nbuf_t next, prev = NULL, cur = *head_addr;
	struct ieee80211_frame *mac_hdr, *cmac_hdr, *next_hdr, *lmac_hdr;
	uint8_t fragno, cur_fragno, lfragno, next_fragno;
	uint8_t last_morefrag = 1, count = 0;

	qdf_assert(frag);

	mac_hdr = (struct ieee80211_frame *)
		ol_rx_frag_get_mac_hdr(htt_pdev, frag);
	fragno = qdf_le16_to_cpu(*(uint16_t *) mac_hdr->i_seq) &
		IEEE80211_SEQ_FRAG_MASK;

	if (!(*head_addr)) {
		*head_addr = frag;
		*tail_addr = frag;
		qdf_nbuf_set_next(*tail_addr, NULL);
		return;
	}
	/* For efficiency, compare with tail first */
	lmac_hdr = (struct ieee80211_frame *)
		ol_rx_frag_get_mac_hdr(htt_pdev, *tail_addr);
	lfragno = qdf_le16_to_cpu(*(uint16_t *) lmac_hdr->i_seq) &
		  IEEE80211_SEQ_FRAG_MASK;
	if (fragno > lfragno) {
		qdf_nbuf_set_next(*tail_addr, frag);
		*tail_addr = frag;
		qdf_nbuf_set_next(*tail_addr, NULL);
	} else {
		do {
			cmac_hdr = (struct ieee80211_frame *)
				ol_rx_frag_get_mac_hdr(htt_pdev, cur);
			cur_fragno =
				qdf_le16_to_cpu(*(uint16_t *) cmac_hdr->i_seq) &
				IEEE80211_SEQ_FRAG_MASK;
			prev = cur;
			cur = qdf_nbuf_next(cur);
		} while (fragno > cur_fragno);

		if (fragno == cur_fragno) {
			htt_rx_desc_frame_free(htt_pdev, frag);
			*all_frag_present = 0;
			return;
		}

		qdf_nbuf_set_next(prev, frag);
		qdf_nbuf_set_next(frag, cur);
	}
	next = qdf_nbuf_next(*head_addr);
	lmac_hdr = (struct ieee80211_frame *)ol_rx_frag_get_mac_hdr(htt_pdev,
								    *tail_addr);
	last_morefrag = lmac_hdr->i_fc[1] & IEEE80211_FC1_MORE_FRAG;
	if (!last_morefrag) {
		do {
			next_hdr =
				(struct ieee80211_frame *)
				ol_rx_frag_get_mac_hdr(htt_pdev, next);
			next_fragno =
				qdf_le16_to_cpu(*(uint16_t *) next_hdr->i_seq) &
				IEEE80211_SEQ_FRAG_MASK;
			count++;
			if (next_fragno != count)
				break;

			next = qdf_nbuf_next(next);
		} while (next);

		if (!next) {
			*all_frag_present = 1;
			return;
		}
	}
	*all_frag_present = 0;
}

/*
 * add tid to pending fragment wait list
 */
void ol_rx_defrag_waitlist_add(struct ol_txrx_peer_t *peer, unsigned int tid)
{
	struct ol_txrx_pdev_t *pdev = peer->vdev->pdev;
	struct ol_rx_reorder_t *rx_reorder = &peer->tids_rx_reorder[tid];

	TAILQ_INSERT_TAIL(&pdev->rx.defrag.waitlist, rx_reorder,
			  defrag_waitlist_elem);
}

/*
 * remove tid from pending fragment wait list
 */
void ol_rx_defrag_waitlist_remove(struct ol_txrx_peer_t *peer, unsigned int tid)
{
	struct ol_txrx_pdev_t *pdev = peer->vdev->pdev;
	struct ol_rx_reorder_t *rx_reorder = &peer->tids_rx_reorder[tid];

	if (rx_reorder->defrag_waitlist_elem.tqe_next) {

		TAILQ_REMOVE(&pdev->rx.defrag.waitlist, rx_reorder,
			     defrag_waitlist_elem);

		rx_reorder->defrag_waitlist_elem.tqe_next = NULL;
		rx_reorder->defrag_waitlist_elem.tqe_prev = NULL;
	} else if (rx_reorder->defrag_waitlist_elem.tqe_next) {
		ol_txrx_alert("waitlist->tqe_prv = NULL\n");
		QDF_ASSERT(0);
		rx_reorder->defrag_waitlist_elem.tqe_next = NULL;
	}
}

#ifndef container_of
#define container_of(ptr, type, member) \
	((type *)((char *)(ptr) - (char *)(&((type *)0)->member)))
#endif

/*
 * flush stale fragments from the waitlist
 */
void ol_rx_defrag_waitlist_flush(struct ol_txrx_pdev_t *pdev)
{
	struct ol_rx_reorder_t *rx_reorder, *tmp;
	uint32_t now_ms = qdf_system_ticks_to_msecs(qdf_system_ticks());

	TAILQ_FOREACH_SAFE(rx_reorder, &pdev->rx.defrag.waitlist,
			   defrag_waitlist_elem, tmp) {
		struct ol_txrx_peer_t *peer;
		struct ol_rx_reorder_t *rx_reorder_base;
		unsigned int tid;

		if (rx_reorder->defrag_timeout_ms > now_ms)
			break;

		tid = rx_reorder->tid;
		if (tid >= OL_TXRX_NUM_EXT_TIDS) {
			ol_txrx_err("%s:  invalid tid, %u\n", __FUNCTION__, tid);
			WARN_ON(1);
			continue;
		}
		/* get index 0 of the rx_reorder array */
		rx_reorder_base = rx_reorder - tid;
		peer =
			container_of(rx_reorder_base, struct ol_txrx_peer_t,
				     tids_rx_reorder[0]);

		ol_rx_defrag_waitlist_remove(peer, tid);
		ol_rx_reorder_flush_frag(pdev->htt_pdev, peer, tid,
					 0 /* frags always stored at seq 0 */);
	}
}

/*
 * Handling security checking and processing fragments
 */
void
ol_rx_defrag(ol_txrx_pdev_handle pdev,
	     struct ol_txrx_peer_t *peer, unsigned int tid,
	     qdf_nbuf_t frag_list)
{
	struct ol_txrx_vdev_t *vdev = NULL;
	qdf_nbuf_t tmp_next, msdu, prev = NULL, cur = frag_list;
	uint8_t index, tkip_demic = 0;
	uint16_t hdr_space;
	void *rx_desc;
	struct ieee80211_frame *wh;
	uint8_t key[DEFRAG_IEEE80211_KEY_LEN];
	htt_pdev_handle htt_pdev = pdev->htt_pdev;
	struct ol_txrx_peer_t *peer_head = NULL;
	uint8_t bssid[QDF_MAC_ADDR_SIZE];
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);

	if (qdf_unlikely(!soc)) {
		ol_txrx_err("soc is NULL");
		return;
	}
	vdev = peer->vdev;

	/* bypass defrag for safe mode */
	if (vdev->safemode) {
		if (ol_cfg_is_full_reorder_offload(pdev->ctrl_pdev))
			ol_rx_in_order_deliver(vdev, peer, tid, frag_list);
		else
			ol_rx_deliver(vdev, peer, tid, frag_list);
		return;
	}

	while (cur) {
		tmp_next = qdf_nbuf_next(cur);
		qdf_nbuf_set_next(cur, NULL);
		/*
		 * Strict PN check between the first fragment of the current
		 * frame and the last fragment of the previous frame is not
		 * necessary.
		 */
		if (!ol_rx_pn_check_base(vdev, peer, tid, cur,
					 (cur == frag_list) ? false : true)) {
			/* PN check failed,discard frags */
			if (prev) {
				qdf_nbuf_set_next(prev, NULL);
				ol_rx_frames_free(htt_pdev, frag_list);
			}
			ol_rx_frames_free(htt_pdev, tmp_next);
			ol_txrx_err("PN Check failed");
			return;
		}
		/* remove FCS from each fragment */
		qdf_nbuf_trim_tail(cur, DEFRAG_IEEE80211_FCS_LEN);
		prev = cur;
		qdf_nbuf_set_next(cur, tmp_next);
		cur = tmp_next;
	}
	cur = frag_list;
	wh = (struct ieee80211_frame *)ol_rx_frag_get_mac_hdr(htt_pdev, cur);
	hdr_space = ol_rx_frag_hdrsize(wh);
	rx_desc = htt_rx_msdu_desc_retrieve(htt_pdev, frag_list);
	qdf_assert(htt_rx_msdu_has_wlan_mcast_flag(htt_pdev, rx_desc));
	index = htt_rx_msdu_is_wlan_mcast(htt_pdev, rx_desc) ?
		txrx_sec_mcast : txrx_sec_ucast;

	switch (peer->security[index].sec_type) {
	case htt_sec_type_tkip:
		tkip_demic = 1;
	/* fall-through to rest of tkip ops */
	case htt_sec_type_tkip_nomic:
		while (cur) {
			tmp_next = qdf_nbuf_next(cur);
			if (!ol_rx_frag_tkip_decap(pdev, cur, hdr_space)) {
				/* TKIP decap failed, discard frags */
				ol_rx_frames_free(htt_pdev, frag_list);
				ol_txrx_err("TKIP decap failed");
				return;
			}
			cur = tmp_next;
		}
		break;

	case htt_sec_type_aes_ccmp:
		while (cur) {
			tmp_next = qdf_nbuf_next(cur);
			if (!ol_rx_frag_ccmp_demic(pdev, cur, hdr_space)) {
				/* CCMP demic failed, discard frags */
				ol_rx_frames_free(htt_pdev, frag_list);
				ol_txrx_err("CCMP demic failed");
				return;
			}
			if (!ol_rx_frag_ccmp_decap(pdev, cur, hdr_space)) {
				/* CCMP decap failed, discard frags */
				ol_rx_frames_free(htt_pdev, frag_list);
				ol_txrx_err("CCMP decap failed");
				return;
			}
			cur = tmp_next;
		}
		break;

	case htt_sec_type_wep40:
	case htt_sec_type_wep104:
	case htt_sec_type_wep128:
		while (cur) {
			tmp_next = qdf_nbuf_next(cur);
			if (!ol_rx_frag_wep_decap(pdev, cur, hdr_space)) {
				/* wep decap failed, discard frags */
				ol_rx_frames_free(htt_pdev, frag_list);
				ol_txrx_err("wep decap failed");
				return;
			}
			cur = tmp_next;
		}
		break;

	default:
		break;
	}

	msdu = ol_rx_defrag_decap_recombine(htt_pdev, frag_list, hdr_space);
	if (!msdu)
		return;

	if (tkip_demic) {
		qdf_mem_copy(key,
			     peer->security[index].michael_key,
			     sizeof(peer->security[index].michael_key));
		if (!ol_rx_frag_tkip_demic(pdev, key, msdu, hdr_space)) {
			htt_rx_desc_frame_free(htt_pdev, msdu);
			ol_rx_err(pdev->ctrl_pdev,
				  vdev->vdev_id, peer->mac_addr.raw, tid, 0,
				  OL_RX_DEFRAG_ERR, msdu, NULL, 0);
			ol_txrx_err("TKIP demic failed");
			return;
		}
	}
	wh = (struct ieee80211_frame *)ol_rx_frag_get_mac_hdr(htt_pdev, msdu);
	if (DEFRAG_IEEE80211_QOS_HAS_SEQ(wh))
		ol_rx_defrag_qos_decap(pdev, msdu, hdr_space);
	if (ol_cfg_frame_type(pdev->ctrl_pdev) == wlan_frm_fmt_802_3)
		ol_rx_defrag_nwifi_to_8023(pdev, msdu);

	/* Packet Capture Mode */

	if ((ucfg_pkt_capture_get_pktcap_mode((void *)soc->psoc) &
	      PKT_CAPTURE_MODE_DATA_ONLY)) {
		if (peer) {
			if (peer->vdev) {
				qdf_spin_lock_bh(&pdev->peer_ref_mutex);
				peer_head = TAILQ_FIRST(&vdev->peer_list);
				qdf_spin_unlock_bh(&pdev->peer_ref_mutex);
				if (peer_head) {
					qdf_spin_lock_bh(
						&peer_head->peer_info_lock);
					qdf_mem_copy(bssid,
						     &peer_head->mac_addr.raw,
						     QDF_MAC_ADDR_SIZE);
					qdf_spin_unlock_bh(
						&peer_head->peer_info_lock);

					ucfg_pkt_capture_rx_msdu_process(
								bssid, msdu,
								vdev->vdev_id,
								htt_pdev);
				}
			}
		}
	}

	ol_rx_fwd_check(vdev, peer, tid, msdu);
}

/*
 * Handling TKIP processing for defragmentation
 */
int
ol_rx_frag_tkip_decap(ol_txrx_pdev_handle pdev,
		      qdf_nbuf_t msdu, uint16_t hdrlen)
{
	uint8_t *ivp, *origHdr;

	void *rx_desc_old_position = NULL;
	void *ind_old_position = NULL;
	int rx_desc_len = 0;

	ol_rx_frag_desc_adjust(pdev,
			       msdu,
			       &rx_desc_old_position,
			       &ind_old_position, &rx_desc_len);
	/* Header should have extended IV */
	origHdr = (uint8_t *) (qdf_nbuf_data(msdu) + rx_desc_len);

	ivp = origHdr + hdrlen;
	if (!(ivp[IEEE80211_WEP_IVLEN] & IEEE80211_WEP_EXTIV))
		return OL_RX_DEFRAG_ERR;

	qdf_mem_move(origHdr + f_tkip.ic_header, origHdr, hdrlen);
	ol_rx_frag_restructure(
			pdev,
			msdu,
			rx_desc_old_position,
			ind_old_position,
			&f_tkip,
			rx_desc_len);
	qdf_nbuf_pull_head(msdu, f_tkip.ic_header);
	qdf_nbuf_trim_tail(msdu, f_tkip.ic_trailer);
	return OL_RX_DEFRAG_OK;
}

/*
 * Handling WEP processing for defragmentation
 */
int
ol_rx_frag_wep_decap(ol_txrx_pdev_handle pdev, qdf_nbuf_t msdu, uint16_t hdrlen)
{
	uint8_t *origHdr;
	void *rx_desc_old_position = NULL;
	void *ind_old_position = NULL;
	int rx_desc_len = 0;

	ol_rx_frag_desc_adjust(pdev,
			       msdu,
			       &rx_desc_old_position,
			       &ind_old_position, &rx_desc_len);
	origHdr = (uint8_t *) (qdf_nbuf_data(msdu) + rx_desc_len);
	qdf_mem_move(origHdr + f_wep.ic_header, origHdr, hdrlen);
	ol_rx_frag_restructure(
			pdev,
			msdu,
			rx_desc_old_position,
			ind_old_position,
			&f_wep,
			rx_desc_len);
	qdf_nbuf_pull_head(msdu, f_wep.ic_header);
	qdf_nbuf_trim_tail(msdu, f_wep.ic_trailer);
	return OL_RX_DEFRAG_OK;
}

/*
 * Verify and strip MIC from the frame.
 */
int
ol_rx_frag_tkip_demic(ol_txrx_pdev_handle pdev, const uint8_t *key,
		      qdf_nbuf_t msdu, uint16_t hdrlen)
{
	int status;
	uint32_t pktlen;
	uint8_t mic[IEEE80211_WEP_MICLEN];
	uint8_t mic0[IEEE80211_WEP_MICLEN];
	void *rx_desc_old_position = NULL;
	void *ind_old_position = NULL;
	int rx_desc_len = 0;

	ol_rx_frag_desc_adjust(pdev,
			       msdu,
			       &rx_desc_old_position,
			       &ind_old_position, &rx_desc_len);

	pktlen = ol_rx_defrag_len(msdu) - rx_desc_len;

	status = ol_rx_defrag_mic(pdev, key, msdu, hdrlen,
				  pktlen - (hdrlen + f_tkip.ic_miclen), mic);
	if (status != OL_RX_DEFRAG_OK)
		return OL_RX_DEFRAG_ERR;

	ol_rx_defrag_copydata(msdu, pktlen - f_tkip.ic_miclen + rx_desc_len,
			      f_tkip.ic_miclen, (caddr_t) mic0);
	if (qdf_mem_cmp(mic, mic0, f_tkip.ic_miclen))
		return OL_RX_DEFRAG_ERR;

	qdf_nbuf_trim_tail(msdu, f_tkip.ic_miclen);
	return OL_RX_DEFRAG_OK;
}

/*
 * Handling CCMP processing for defragmentation
 */
int
ol_rx_frag_ccmp_decap(ol_txrx_pdev_handle pdev,
		      qdf_nbuf_t nbuf, uint16_t hdrlen)
{
	uint8_t *ivp, *origHdr;
	void *rx_desc_old_position = NULL;
	void *ind_old_position = NULL;
	int rx_desc_len = 0;

	ol_rx_frag_desc_adjust(pdev,
			       nbuf,
			       &rx_desc_old_position,
			       &ind_old_position, &rx_desc_len);

	origHdr = (uint8_t *) (qdf_nbuf_data(nbuf) + rx_desc_len);
	ivp = origHdr + hdrlen;
	if (!(ivp[IEEE80211_WEP_IVLEN] & IEEE80211_WEP_EXTIV))
		return OL_RX_DEFRAG_ERR;

	qdf_mem_move(origHdr + f_ccmp.ic_header, origHdr, hdrlen);
	ol_rx_frag_restructure(
			pdev,
			nbuf,
			rx_desc_old_position,
			ind_old_position,
			&f_ccmp,
			rx_desc_len);
	qdf_nbuf_pull_head(nbuf, f_ccmp.ic_header);

	return OL_RX_DEFRAG_OK;
}

/*
 * Verify and strip MIC from the frame.
 */
int
ol_rx_frag_ccmp_demic(ol_txrx_pdev_handle pdev,
		      qdf_nbuf_t wbuf, uint16_t hdrlen)
{
	uint8_t *ivp, *origHdr;
	void *rx_desc_old_position = NULL;
	void *ind_old_position = NULL;
	int rx_desc_len = 0;

	ol_rx_frag_desc_adjust(pdev,
			       wbuf,
			       &rx_desc_old_position,
			       &ind_old_position, &rx_desc_len);

	origHdr = (uint8_t *) (qdf_nbuf_data(wbuf) + rx_desc_len);

	ivp = origHdr + hdrlen;
	if (!(ivp[IEEE80211_WEP_IVLEN] & IEEE80211_WEP_EXTIV))
		return OL_RX_DEFRAG_ERR;

	qdf_nbuf_trim_tail(wbuf, f_ccmp.ic_trailer);

	return OL_RX_DEFRAG_OK;
}

/*
 * Craft pseudo header used to calculate the MIC.
 */
void ol_rx_defrag_michdr(const struct ieee80211_frame *wh0, uint8_t hdr[])
{
	const struct ieee80211_frame_addr4 *wh =
		(const struct ieee80211_frame_addr4 *)wh0;

	switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
	case IEEE80211_FC1_DIR_NODS:
		DEFRAG_IEEE80211_ADDR_COPY(hdr, wh->i_addr1);   /* DA */
		DEFRAG_IEEE80211_ADDR_COPY(hdr + QDF_MAC_ADDR_SIZE,
					   wh->i_addr2);
		break;
	case IEEE80211_FC1_DIR_TODS:
		DEFRAG_IEEE80211_ADDR_COPY(hdr, wh->i_addr3);   /* DA */
		DEFRAG_IEEE80211_ADDR_COPY(hdr + QDF_MAC_ADDR_SIZE,
					   wh->i_addr2);
		break;
	case IEEE80211_FC1_DIR_FROMDS:
		DEFRAG_IEEE80211_ADDR_COPY(hdr, wh->i_addr1);   /* DA */
		DEFRAG_IEEE80211_ADDR_COPY(hdr + QDF_MAC_ADDR_SIZE,
					   wh->i_addr3);
		break;
	case IEEE80211_FC1_DIR_DSTODS:
		DEFRAG_IEEE80211_ADDR_COPY(hdr, wh->i_addr3);   /* DA */
		DEFRAG_IEEE80211_ADDR_COPY(hdr + QDF_MAC_ADDR_SIZE,
					   wh->i_addr4);
		break;
	}
	/*
	 * Bit 7 is QDF_IEEE80211_FC0_SUBTYPE_QOS for data frame, but
	 * it could also be set for deauth, disassoc, action, etc. for
	 * a mgt type frame. It comes into picture for MFP.
	 */
	if (wh->i_fc[0] & QDF_IEEE80211_FC0_SUBTYPE_QOS) {
		const struct ieee80211_qosframe *qwh =
			(const struct ieee80211_qosframe *)wh;
		hdr[12] = qwh->i_qos[0] & IEEE80211_QOS_TID;
	} else {
		hdr[12] = 0;
	}
	hdr[13] = hdr[14] = hdr[15] = 0;        /* reserved */
}

/*
 * Michael_mic for defragmentation
 */
int
ol_rx_defrag_mic(ol_txrx_pdev_handle pdev,
		 const uint8_t *key,
		 qdf_nbuf_t wbuf,
		 uint16_t off, uint16_t data_len, uint8_t mic[])
{
	uint8_t hdr[16] = { 0, };
	uint32_t l, r;
	const uint8_t *data;
	uint32_t space;
	void *rx_desc_old_position = NULL;
	void *ind_old_position = NULL;
	int rx_desc_len = 0;
	htt_pdev_handle htt_pdev = pdev->htt_pdev;

	ol_rx_frag_desc_adjust(pdev,
			       wbuf,
			       &rx_desc_old_position,
			       &ind_old_position, &rx_desc_len);

	ol_rx_defrag_michdr((struct ieee80211_frame *)(qdf_nbuf_data(wbuf) +
						       rx_desc_len), hdr);
	l = get_le32(key);
	r = get_le32(key + 4);

	/* Michael MIC pseudo header: DA, SA, 3 x 0, Priority */
	l ^= get_le32(hdr);
	michael_block(l, r);
	l ^= get_le32(&hdr[4]);
	michael_block(l, r);
	l ^= get_le32(&hdr[8]);
	michael_block(l, r);
	l ^= get_le32(&hdr[12]);
	michael_block(l, r);

	/* first buffer has special handling */
	data = (uint8_t *) qdf_nbuf_data(wbuf) + rx_desc_len + off;
	space = ol_rx_defrag_len(wbuf) - rx_desc_len - off;
	for (;; ) {
		if (space > data_len)
			space = data_len;

		/* collect 32-bit blocks from current buffer */
		while (space >= sizeof(uint32_t)) {
			l ^= get_le32(data);
			michael_block(l, r);
			data += sizeof(uint32_t);
			space -= sizeof(uint32_t);
			data_len -= sizeof(uint32_t);
		}
		if (data_len < sizeof(uint32_t))
			break;

		wbuf = qdf_nbuf_next(wbuf);
		if (!wbuf)
			return OL_RX_DEFRAG_ERR;

		rx_desc_len = ol_rx_get_desc_len(htt_pdev, wbuf,
						 &rx_desc_old_position);

		if (space != 0) {
			const uint8_t *data_next;
			/*
			 * Block straddles buffers, split references.
			 */
			data_next =
				(uint8_t *) qdf_nbuf_data(wbuf) + rx_desc_len;
			if ((ol_rx_defrag_len(wbuf) - rx_desc_len) <
			    sizeof(uint32_t) - space) {
				return OL_RX_DEFRAG_ERR;
			}
			switch (space) {
			case 1:
				l ^= get_le32_split(data[0], data_next[0],
						    data_next[1], data_next[2]);
				data = data_next + 3;
				space = (ol_rx_defrag_len(wbuf) - rx_desc_len)
					- 3;
				break;
			case 2:
				l ^= get_le32_split(data[0], data[1],
						    data_next[0], data_next[1]);
				data = data_next + 2;
				space = (ol_rx_defrag_len(wbuf) - rx_desc_len)
					- 2;
				break;
			case 3:
				l ^= get_le32_split(data[0], data[1], data[2],
						    data_next[0]);
				data = data_next + 1;
				space = (ol_rx_defrag_len(wbuf) - rx_desc_len)
					- 1;
				break;
			}
			michael_block(l, r);
			data_len -= sizeof(uint32_t);
		} else {
			/*
			 * Setup for next buffer.
			 */
			data = (uint8_t *) qdf_nbuf_data(wbuf) + rx_desc_len;
			space = ol_rx_defrag_len(wbuf) - rx_desc_len;
		}
	}
	/* Last block and padding (0x5a, 4..7 x 0) */
	switch (data_len) {
	case 0:
		l ^= get_le32_split(0x5a, 0, 0, 0);
		break;
	case 1:
		l ^= get_le32_split(data[0], 0x5a, 0, 0);
		break;
	case 2:
		l ^= get_le32_split(data[0], data[1], 0x5a, 0);
		break;
	case 3:
		l ^= get_le32_split(data[0], data[1], data[2], 0x5a);
		break;
	}
	michael_block(l, r);
	michael_block(l, r);
	put_le32(mic, l);
	put_le32(mic + 4, r);

	return OL_RX_DEFRAG_OK;
}

/*
 * Calculate headersize
 */
uint16_t ol_rx_frag_hdrsize(const void *data)
{
	const struct ieee80211_frame *wh = (const struct ieee80211_frame *)data;
	uint16_t size = sizeof(struct ieee80211_frame);

	if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS)
		size += QDF_MAC_ADDR_SIZE;

	if (DEFRAG_IEEE80211_QOS_HAS_SEQ(wh)) {
		size += sizeof(uint16_t);
		if (wh->i_fc[1] & IEEE80211_FC1_ORDER)
			size += sizeof(struct ieee80211_htc);
	}
	return size;
}

/*
 * Recombine and decap fragments
 */
qdf_nbuf_t
ol_rx_defrag_decap_recombine(htt_pdev_handle htt_pdev,
			     qdf_nbuf_t frag_list, uint16_t hdrsize)
{
	qdf_nbuf_t tmp;
	qdf_nbuf_t msdu = frag_list;
	qdf_nbuf_t rx_nbuf = frag_list;
	struct ieee80211_frame *wh;

	msdu = qdf_nbuf_next(msdu);
	qdf_nbuf_set_next(rx_nbuf, NULL);
	while (msdu) {
		htt_rx_msdu_desc_free(htt_pdev, msdu);
		tmp = qdf_nbuf_next(msdu);
		qdf_nbuf_set_next(msdu, NULL);
		ol_rx_frag_pull_hdr(htt_pdev, msdu, hdrsize);
		if (!ol_rx_defrag_concat(rx_nbuf, msdu)) {
			ol_rx_frames_free(htt_pdev, tmp);
			htt_rx_desc_frame_free(htt_pdev, rx_nbuf);
			qdf_nbuf_free(msdu);
			/* msdu rx desc already freed above */
			return NULL;
		}
		msdu = tmp;
	}
	wh = (struct ieee80211_frame *)ol_rx_frag_get_mac_hdr(htt_pdev,
							      rx_nbuf);
	wh->i_fc[1] &= ~IEEE80211_FC1_MORE_FRAG;
	*(uint16_t *) wh->i_seq &= ~IEEE80211_SEQ_FRAG_MASK;

	return rx_nbuf;
}

void ol_rx_defrag_nwifi_to_8023(ol_txrx_pdev_handle pdev, qdf_nbuf_t msdu)
{
	struct ieee80211_frame wh;
	uint32_t hdrsize;
	struct llc_snap_hdr_t llchdr;
	struct ethernet_hdr_t *eth_hdr;
	void *rx_desc_old_position = NULL;
	void *ind_old_position = NULL;
	int rx_desc_len = 0;
	struct ieee80211_frame *wh_ptr;

	ol_rx_frag_desc_adjust(pdev,
			       msdu,
			       &rx_desc_old_position,
			       &ind_old_position, &rx_desc_len);

	wh_ptr = (struct ieee80211_frame *)(qdf_nbuf_data(msdu) + rx_desc_len);
	qdf_mem_copy(&wh, wh_ptr, sizeof(wh));
	hdrsize = sizeof(struct ieee80211_frame);
	qdf_mem_copy(&llchdr, ((uint8_t *) (qdf_nbuf_data(msdu) +
					    rx_desc_len)) + hdrsize,
		     sizeof(struct llc_snap_hdr_t));

	/*
	 * Now move the data pointer to the beginning of the mac header :
	 * new-header = old-hdr + (wifhdrsize + llchdrsize - ethhdrsize)
	 */
	qdf_nbuf_pull_head(msdu, (rx_desc_len + hdrsize +
				  sizeof(struct llc_snap_hdr_t) -
				  sizeof(struct ethernet_hdr_t)));
	eth_hdr = (struct ethernet_hdr_t *)(qdf_nbuf_data(msdu));
	switch (wh.i_fc[1] & IEEE80211_FC1_DIR_MASK) {
	case IEEE80211_FC1_DIR_NODS:
		qdf_mem_copy(eth_hdr->dest_addr, wh.i_addr1,
			     QDF_MAC_ADDR_SIZE);
		qdf_mem_copy(eth_hdr->src_addr, wh.i_addr2, QDF_MAC_ADDR_SIZE);
		break;
	case IEEE80211_FC1_DIR_TODS:
		qdf_mem_copy(eth_hdr->dest_addr, wh.i_addr3,
			     QDF_MAC_ADDR_SIZE);
		qdf_mem_copy(eth_hdr->src_addr, wh.i_addr2, QDF_MAC_ADDR_SIZE);
		break;
	case IEEE80211_FC1_DIR_FROMDS:
		qdf_mem_copy(eth_hdr->dest_addr, wh.i_addr1,
			     QDF_MAC_ADDR_SIZE);
		qdf_mem_copy(eth_hdr->src_addr, wh.i_addr3, QDF_MAC_ADDR_SIZE);
		break;
	case IEEE80211_FC1_DIR_DSTODS:
		break;
	}

	qdf_mem_copy(eth_hdr->ethertype, llchdr.ethertype,
		     sizeof(llchdr.ethertype));

	ol_rx_defrag_push_rx_desc(msdu, rx_desc_old_position,
					  ind_old_position, rx_desc_len);
}

/*
 * Handling QOS for defragmentation
 */
void
ol_rx_defrag_qos_decap(ol_txrx_pdev_handle pdev,
		       qdf_nbuf_t nbuf, uint16_t hdrlen)
{
	struct ieee80211_frame *wh;
	uint16_t qoslen;
	void *rx_desc_old_position = NULL;
	void *ind_old_position = NULL;
	int rx_desc_len = 0;

	ol_rx_frag_desc_adjust(pdev,
			       nbuf,
			       &rx_desc_old_position,
			       &ind_old_position, &rx_desc_len);

	wh = (struct ieee80211_frame *)(qdf_nbuf_data(nbuf) + rx_desc_len);
	if (DEFRAG_IEEE80211_QOS_HAS_SEQ(wh)) {
		qoslen = sizeof(struct ieee80211_qoscntl);
		/* Qos frame with Order bit set indicates a HTC frame */
		if (wh->i_fc[1] & IEEE80211_FC1_ORDER)
			qoslen += sizeof(struct ieee80211_htc);

		/* remove QoS filed from header */
		hdrlen -= qoslen;
		qdf_mem_move((uint8_t *) wh + qoslen, wh, hdrlen);
		wh = (struct ieee80211_frame *)qdf_nbuf_pull_head(nbuf,
								  rx_desc_len +
								  qoslen);
		/* clear QoS bit */
		/*
		 * KW# 6154 'qdf_nbuf_pull_head' in turn calls
		 * __qdf_nbuf_pull_head,
		 * which returns NULL if there is not sufficient data to pull.
		 * It's guaranteed that qdf_nbuf_pull_head will succeed rather
		 * than returning NULL, since the entire rx frame is already
		 * present in the rx buffer.
		 * However, to make it obvious to static analyzers that this
		 * code is safe, add an explicit check that qdf_nbuf_pull_head
		 * returns a non-NULL value.
		 * Since this part of the code is not performance-critical,
		 * adding this explicit check is okay.
		 */
		if (wh)
			wh->i_fc[0] &= ~QDF_IEEE80211_FC0_SUBTYPE_QOS;

		ol_rx_defrag_push_rx_desc(nbuf, rx_desc_old_position,
					  ind_old_position, rx_desc_len);

	}
}
