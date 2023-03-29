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

/**
 * @file htt_rx.c
 * @brief Implement receive aspects of HTT.
 * @details
 *  This file contains three categories of HTT rx code:
 *  1.  An abstraction of the rx descriptor, to hide the
 *      differences between the HL vs. LL rx descriptor.
 *  2.  Functions for providing access to the (series of)
 *      rx descriptor(s) and rx frame(s) associated with
 *      an rx indication message.
 *  3.  Functions for setting up and using the MAC DMA
 *      rx ring (applies to LL only).
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

/**
 * htt_rx_mpdu_wifi_hdr_retrieve() - retrieve 802.11 header
 * @pdev - pdev handle
 * @mpdu_desc - mpdu descriptor
 *
 * Return : pointer to 802.11 header
 */
char *htt_rx_mpdu_wifi_hdr_retrieve(htt_pdev_handle pdev, void *mpdu_desc)
{
	struct htt_host_rx_desc_base *rx_desc =
		(struct htt_host_rx_desc_base *)mpdu_desc;

	if (!rx_desc)
		return NULL;
	else
		return rx_desc->rx_hdr_status;
}

/**
 * htt_rx_mpdu_desc_tsf32() - Return the TSF timestamp indicating when
 *                            a MPDU was received.
 * @pdev - the HTT instance the rx data was received on
 * @mpdu_desc - the abstract descriptor for the MPDU in question
 *
 * return : 32 LSBs of TSF time at which the MPDU's PPDU was received
 */
uint32_t htt_rx_mpdu_desc_tsf32(htt_pdev_handle pdev, void *mpdu_desc)
{
	return 0;
}

static inline
uint8_t htt_rx_msdu_fw_desc_get(htt_pdev_handle pdev, void *msdu_desc)
{
	/*
	 * HL and LL use the same format for FW rx desc, but have the FW rx desc
	 * in different locations.
	 * In LL, the FW rx descriptor has been copied into the same
	 * htt_host_rx_desc_base struct that holds the HW rx desc.
	 * In HL, the FW rx descriptor, along with the MSDU payload,
	 * is in the same buffer as the rx indication message.
	 *
	 * Use the FW rx desc offset configured during startup to account for
	 * this difference between HL vs. LL.
	 *
	 * An optimization would be to define the LL and HL msdu_desc pointer
	 * in such a way that they both use the same offset to the FW rx desc.
	 * Then the following functions could be converted to macros, without
	 * needing to expose the htt_pdev_t definition outside HTT.
	 */
	return *(((uint8_t *)msdu_desc) + pdev->rx_fw_desc_offset);
}

int htt_rx_msdu_discard(htt_pdev_handle pdev, void *msdu_desc)
{
	return htt_rx_msdu_fw_desc_get(pdev, msdu_desc) & FW_RX_DESC_DISCARD_M;
}

int htt_rx_msdu_forward(htt_pdev_handle pdev, void *msdu_desc)
{
	return htt_rx_msdu_fw_desc_get(pdev, msdu_desc) & FW_RX_DESC_FORWARD_M;
}

int htt_rx_msdu_inspect(htt_pdev_handle pdev, void *msdu_desc)
{
	return htt_rx_msdu_fw_desc_get(pdev, msdu_desc) & FW_RX_DESC_INSPECT_M;
}

void
htt_rx_msdu_actions(htt_pdev_handle pdev,
		    void *msdu_desc, int *discard, int *forward, int *inspect)
{
	uint8_t rx_msdu_fw_desc = htt_rx_msdu_fw_desc_get(pdev, msdu_desc);
#ifdef HTT_DEBUG_DATA
	HTT_PRINT("act:0x%x ", rx_msdu_fw_desc);
#endif
	*discard = rx_msdu_fw_desc & FW_RX_DESC_DISCARD_M;
	*forward = rx_msdu_fw_desc & FW_RX_DESC_FORWARD_M;
	*inspect = rx_msdu_fw_desc & FW_RX_DESC_INSPECT_M;
}

uint32_t htt_rx_amsdu_rx_in_order_get_pktlog(qdf_nbuf_t rx_ind_msg)
{
	uint32_t *msg_word;

	msg_word = (uint32_t *)qdf_nbuf_data(rx_ind_msg);
	return HTT_RX_IN_ORD_PADDR_IND_PKTLOG_GET(*msg_word);
}

int16_t htt_rx_mpdu_desc_rssi_dbm(htt_pdev_handle pdev, void *mpdu_desc)
{
	/*
	 * Currently the RSSI is provided only as a field in the
	 * HTT_T2H_RX_IND message, rather than in each rx descriptor.
	 */
	return HTT_RSSI_INVALID;
}

/*
 * htt_rx_amsdu_pop -
 * global function pointer that is programmed during attach to point
 * to either htt_rx_amsdu_pop_ll or htt_rx_amsdu_rx_in_order_pop_ll.
 */
int (*htt_rx_amsdu_pop)(htt_pdev_handle pdev,
			qdf_nbuf_t rx_ind_msg,
			qdf_nbuf_t *head_msdu, qdf_nbuf_t *tail_msdu,
			uint32_t *msdu_count);

/*
 * htt_rx_frag_pop -
 * global function pointer that is programmed during attach to point
 * to either htt_rx_amsdu_pop_ll
 */
int (*htt_rx_frag_pop)(htt_pdev_handle pdev,
		       qdf_nbuf_t rx_ind_msg,
		       qdf_nbuf_t *head_msdu, qdf_nbuf_t *tail_msdu,
		       uint32_t *msdu_count);

int (*htt_rx_offload_msdu_cnt)(htt_pdev_handle pdev);

int
(*htt_rx_offload_msdu_pop)(htt_pdev_handle pdev,
			   qdf_nbuf_t offload_deliver_msg,
			   int *vdev_id,
			   int *peer_id,
			   int *tid,
			   uint8_t *fw_desc,
			   qdf_nbuf_t *head_buf, qdf_nbuf_t *tail_buf);

void * (*htt_rx_mpdu_desc_list_next)(htt_pdev_handle pdev,
				     qdf_nbuf_t rx_ind_msg);

bool (*htt_rx_mpdu_desc_retry)(htt_pdev_handle pdev, void *mpdu_desc);

uint16_t (*htt_rx_mpdu_desc_seq_num)(htt_pdev_handle pdev, void *mpdu_desc,
				     bool update_seq_num);

void (*htt_rx_mpdu_desc_pn)(htt_pdev_handle pdev,
			    void *mpdu_desc,
			    union htt_rx_pn_t *pn, int pn_len_bits);

uint8_t (*htt_rx_mpdu_desc_tid)(htt_pdev_handle pdev, void *mpdu_desc);

bool (*htt_rx_msdu_desc_completes_mpdu)(htt_pdev_handle pdev, void *msdu_desc);

bool (*htt_rx_msdu_first_msdu_flag)(htt_pdev_handle pdev, void *msdu_desc);

int (*htt_rx_msdu_has_wlan_mcast_flag)(htt_pdev_handle pdev, void *msdu_desc);

bool (*htt_rx_msdu_is_wlan_mcast)(htt_pdev_handle pdev, void *msdu_desc);

int (*htt_rx_msdu_is_frag)(htt_pdev_handle pdev, void *msdu_desc);

void * (*htt_rx_msdu_desc_retrieve)(htt_pdev_handle pdev, qdf_nbuf_t msdu);

bool (*htt_rx_mpdu_is_encrypted)(htt_pdev_handle pdev, void *mpdu_desc);

bool (*htt_rx_msdu_desc_key_id)(htt_pdev_handle pdev,
				void *mpdu_desc, uint8_t *key_id);

bool (*htt_rx_msdu_chan_info_present)(
	htt_pdev_handle pdev,
	void *mpdu_desc);

bool (*htt_rx_msdu_center_freq)(
	htt_pdev_handle pdev,
	struct ol_txrx_peer_t *peer,
	void *mpdu_desc,
	uint16_t *primary_chan_center_freq_mhz,
	uint16_t *contig_chan1_center_freq_mhz,
	uint16_t *contig_chan2_center_freq_mhz,
	uint8_t *phy_mode);

void htt_rx_desc_frame_free(htt_pdev_handle htt_pdev, qdf_nbuf_t msdu)
{
	qdf_nbuf_free(msdu);
}

void htt_rx_msdu_desc_free(htt_pdev_handle htt_pdev, qdf_nbuf_t msdu)
{
	/*
	 * The rx descriptor is in the same buffer as the rx MSDU payload,
	 * and does not need to be freed separately.
	 */
}

void htt_rx_msdu_buff_replenish(htt_pdev_handle pdev)
{
	if (qdf_atomic_dec_and_test(&pdev->rx_ring.refill_ref_cnt))
		htt_rx_fill_ring_count(pdev);

	qdf_atomic_inc(&pdev->rx_ring.refill_ref_cnt);
}

#ifdef IPA_OFFLOAD
#ifdef QCA_WIFI_3_0
/**
 * htt_rx_ipa_uc_alloc_wdi2_rsc() - Allocate WDI2.0 resources
 * @pdev: htt context
 * @rx_ind_ring_elements: rx ring elements
 *
 * Return: 0 success
 */
static int htt_rx_ipa_uc_alloc_wdi2_rsc(struct htt_pdev_t *pdev,
			 unsigned int rx_ind_ring_elements)
{
	/*
	 * Allocate RX2 indication ring
	 * RX2 IND ring element
	 *   4bytes: pointer
	 *   2bytes: VDEV ID
	 *   2bytes: length
	 *
	 * RX indication ring size, by bytes
	 */
	pdev->ipa_uc_rx_rsc.rx2_ind_ring =
		qdf_mem_shared_mem_alloc(pdev->osdev,
					 rx_ind_ring_elements *
					 sizeof(target_paddr_t));
	if (!pdev->ipa_uc_rx_rsc.rx2_ind_ring) {
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_ERROR,
			  "%s: Unable to allocate memory for IPA rx2 ind ring",
			  __func__);
		return 1;
	}

	pdev->ipa_uc_rx_rsc.rx2_ipa_prc_done_idx =
		qdf_mem_shared_mem_alloc(pdev->osdev, 4);
	if (!pdev->ipa_uc_rx_rsc.rx2_ipa_prc_done_idx) {
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_ERROR,
			  "%s: Unable to allocate memory for IPA rx proc done index",
			  __func__);
		qdf_mem_shared_mem_free(pdev->osdev,
					pdev->ipa_uc_rx_rsc.rx2_ind_ring);
		return 1;
	}

	return 0;
}

/**
 * htt_rx_ipa_uc_free_wdi2_rsc() - Free WDI2.0 resources
 * @pdev: htt context
 *
 * Return: None
 */
static void htt_rx_ipa_uc_free_wdi2_rsc(struct htt_pdev_t *pdev)
{
	qdf_mem_shared_mem_free(pdev->osdev, pdev->ipa_uc_rx_rsc.rx2_ind_ring);
	qdf_mem_shared_mem_free(pdev->osdev,
				pdev->ipa_uc_rx_rsc.rx2_ipa_prc_done_idx);
}
#else
static int htt_rx_ipa_uc_alloc_wdi2_rsc(struct htt_pdev_t *pdev,
			 unsigned int rx_ind_ring_elements)
{
	return 0;
}

static void htt_rx_ipa_uc_free_wdi2_rsc(struct htt_pdev_t *pdev)
{
}
#endif

/**
 * htt_rx_ipa_uc_attach() - attach htt ipa uc rx resource
 * @pdev: htt context
 * @rx_ind_ring_size: rx ring size
 *
 * Return: 0 success
 */
int htt_rx_ipa_uc_attach(struct htt_pdev_t *pdev,
			 unsigned int rx_ind_ring_elements)
{
	int ret = 0;

	/*
	 * Allocate RX indication ring
	 * RX IND ring element
	 *   4bytes: pointer
	 *   2bytes: VDEV ID
	 *   2bytes: length
	 */
	pdev->ipa_uc_rx_rsc.rx_ind_ring =
		qdf_mem_shared_mem_alloc(pdev->osdev,
					 rx_ind_ring_elements *
					 sizeof(struct ipa_uc_rx_ring_elem_t));
	if (!pdev->ipa_uc_rx_rsc.rx_ind_ring) {
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_ERROR,
			  "%s: Unable to allocate memory for IPA rx ind ring",
			  __func__);
		return 1;
	}

	pdev->ipa_uc_rx_rsc.rx_ipa_prc_done_idx =
		qdf_mem_shared_mem_alloc(pdev->osdev, 4);
	if (!pdev->ipa_uc_rx_rsc.rx_ipa_prc_done_idx) {
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_ERROR,
			  "%s: Unable to allocate memory for IPA rx proc done index",
			  __func__);
		qdf_mem_shared_mem_free(pdev->osdev,
					pdev->ipa_uc_rx_rsc.rx_ind_ring);
		return 1;
	}

	ret = htt_rx_ipa_uc_alloc_wdi2_rsc(pdev, rx_ind_ring_elements);
	if (ret) {
		qdf_mem_shared_mem_free(pdev->osdev, pdev->ipa_uc_rx_rsc.rx_ind_ring);
		qdf_mem_shared_mem_free(pdev->osdev,
					pdev->ipa_uc_rx_rsc.rx_ipa_prc_done_idx);
	}
	return ret;
}

int htt_rx_ipa_uc_detach(struct htt_pdev_t *pdev)
{
	qdf_mem_shared_mem_free(pdev->osdev, pdev->ipa_uc_rx_rsc.rx_ind_ring);
	qdf_mem_shared_mem_free(pdev->osdev,
				pdev->ipa_uc_rx_rsc.rx_ipa_prc_done_idx);

	htt_rx_ipa_uc_free_wdi2_rsc(pdev);
	return 0;
}
#endif /* IPA_OFFLOAD */

#ifndef REMOVE_PKT_LOG
/**
 * htt_register_rx_pkt_dump_callback() - registers callback to
 *   get rx pkt status and call callback to do rx packet dump
 *
 * @pdev: htt pdev handle
 * @callback: callback to get rx pkt status and
 *     call callback to do rx packet dump
 *
 * This function is used to register the callback to get
 * rx pkt status and call callback to do rx packet dump
 *
 * Return: None
 *
 */
void htt_register_rx_pkt_dump_callback(struct htt_pdev_t *pdev,
				tp_rx_pkt_dump_cb callback)
{
	if (!pdev) {
		qdf_print("pdev is NULL");
		return;
	}
	pdev->rx_pkt_dump_cb = callback;
}

/**
 * htt_deregister_rx_pkt_dump_callback() - deregisters callback to
 *   get rx pkt status and call callback to do rx packet dump
 *
 * @pdev: htt pdev handle
 *
 * This function is used to deregister the callback to get
 * rx pkt status and call callback to do rx packet dump
 *
 * Return: None
 *
 */
void htt_deregister_rx_pkt_dump_callback(struct htt_pdev_t *pdev)
{
	if (!pdev) {
		qdf_print("pdev is NULL");
		return;
	}
	pdev->rx_pkt_dump_cb = NULL;
}
#endif

#ifdef WLAN_FEATURE_TSF_PLUS
void htt_rx_enable_ppdu_end(int *enable_ppdu_end)
{
	*enable_ppdu_end = 1;
}
#endif
