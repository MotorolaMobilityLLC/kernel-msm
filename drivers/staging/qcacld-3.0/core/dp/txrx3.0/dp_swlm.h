/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _DP_SWLM_H_
#define _DP_SWLM_H_

#ifdef WLAN_DP_FEATURE_SW_LATENCY_MGR

#define DP_SWLM_TCL_TPUT_PASS_THRESH 3

#define DP_SWLM_TCL_RX_TRAFFIC_THRESH	50
#define DP_SWLM_TCL_TX_TRAFFIC_THRESH	50
#define DP_SWLM_TCL_TX_PKT_THRESH	2

/* Traffic test time is in us */
#define DP_SWLM_TCL_TRAFFIC_SAMPLING_TIME 250
#define DP_SWLM_TCL_TIME_FLUSH_THRESH 1000
#define DP_SWLM_TCL_TX_THRESH_MULTIPLIER 2

/* Inline Functions */

/**
 * dp_tx_is_special_frame() - check if this TX frame is a special frame.
 * @nbuf: TX skb pointer
 * @frame_mask: the mask for required special frames
 *
 * Check if TX frame is a required special frame.
 *
 * Returns: true, if this frame is a needed special frame,
 *	    false, otherwise
 */
static inline
bool dp_tx_is_special_frame(qdf_nbuf_t nbuf, uint32_t frame_mask)
{
	if (((frame_mask & FRAME_MASK_IPV4_ARP) &&
	     qdf_nbuf_is_ipv4_arp_pkt(nbuf)) ||
	    ((frame_mask & FRAME_MASK_IPV4_DHCP) &&
	     qdf_nbuf_is_ipv4_dhcp_pkt(nbuf)) ||
	    ((frame_mask & FRAME_MASK_IPV4_EAPOL) &&
	     qdf_nbuf_is_ipv4_eapol_pkt(nbuf)) ||
	    ((frame_mask & FRAME_MASK_IPV6_DHCP) &&
	     qdf_nbuf_is_ipv6_dhcp_pkt(nbuf)))
		return true;

	return false;
}

/**
 * dp_swlm_tcl_reset_session_data() -  Reset the TCL coalescing session data
 * @soc: DP soc handle
 *
 * Returns QDF_STATUS
 */
static inline QDF_STATUS
dp_swlm_tcl_reset_session_data(struct dp_soc *soc)
{
	struct dp_swlm *swlm = &soc->swlm;

	swlm->params.tcl.coalesce_end_time = qdf_get_log_timestamp_usecs() +
		     swlm->params.tcl.time_flush_thresh;
	swlm->params.tcl.bytes_coalesced = 0;
	swlm->params.tcl.bytes_flush_thresh =
				swlm->params.tcl.sampling_session_tx_bytes *
				swlm->params.tcl.tx_thresh_multiplier;
	qdf_timer_sync_cancel(&swlm->params.tcl.flush_timer);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_swlm_tcl_pre_check() - Pre checks for current packet to be transmitted
 * @soc: Datapath soc handle
 * @tcl_data: tcl swlm data
 *
 * Returns: QDF_STATUS_SUCCESS, if all pre-check conditions pass
 *	    QDF_STATUS_E_FAILURE, otherwise
 */
static inline QDF_STATUS
dp_swlm_tcl_pre_check(struct dp_soc *soc,
		      struct dp_swlm_tcl_data *tcl_data)
{
	struct dp_swlm *swlm = &soc->swlm;
	uint32_t frame_mask = FRAME_MASK_IPV4_ARP | FRAME_MASK_IPV4_DHCP |
				FRAME_MASK_IPV4_EAPOL | FRAME_MASK_IPV6_DHCP;

	if (tcl_data->tid > DP_VO_TID) {
		DP_STATS_INC(swlm, tcl.tid_fail, 1);
		goto fail;
	}

	if (dp_tx_is_special_frame(tcl_data->nbuf, frame_mask)) {
		DP_STATS_INC(swlm, tcl.sp_frames, 1);
		goto fail;
	}

	if (tcl_data->num_ll_connections) {
		DP_STATS_INC(swlm, tcl.ll_connection, 1);
		goto fail;
	}

	return QDF_STATUS_SUCCESS;

fail:
	return QDF_STATUS_E_FAILURE;
}

/**
 * dp_swlm_query_policy() - apply software latency policy based on ring type.
 * @soc: Datapath global soc handle
 * @ring_type: SRNG type
 * @query_data: private data for the query corresponding to the ring type
 *
 * Returns: 1, if policy is to be applied
 *	    0, if policy is not to be applied
 */
static inline int dp_swlm_query_policy(struct dp_soc *soc, int ring_type,
				       union swlm_data query_data)
{
	struct dp_swlm *swlm = &soc->swlm;

	switch (ring_type) {
	case TCL_DATA:
		return swlm->ops->tcl_wr_coalesce_check(soc,
							query_data.tcl_data);
	default:
		dp_err("Ring type %d not supported by SW latency manager",
		       ring_type);
		break;
	}

	return 0;
}

/* Function Declarations */

/**
 * dp_soc_swlm_attach() - attach the software latency manager resources
 * @soc: Datapath global soc handle
 *
 * Returns: QDF_STATUS
 */
QDF_STATUS dp_soc_swlm_attach(struct dp_soc *soc);

/**
 * dp_soc_swlm_detach() - detach the software latency manager resources
 * @soc: Datapath global soc handle
 *
 * Returns: QDF_STATUS
 */
QDF_STATUS dp_soc_swlm_detach(struct dp_soc *soc);

/**
 * dp_print_swlm_stats() - Print the SWLM stats
 * @soc: Datapath soc handle
 *
 * Returns: QDF_STATUS
 */
QDF_STATUS dp_print_swlm_stats(struct dp_soc *soc);

#endif /* WLAN_DP_FEATURE_SW_LATENCY_MGR */

#endif
