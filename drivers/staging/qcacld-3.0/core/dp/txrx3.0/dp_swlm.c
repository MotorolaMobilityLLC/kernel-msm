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

#ifdef WLAN_DP_FEATURE_SW_LATENCY_MGR

#include <dp_types.h>
#include <dp_internal.h>
#include <wlan_cfg.h>
#include "dp_swlm.h"

/**
 * dp_swlm_is_tput_thresh_reached() - Calculate the current tx and rx TPUT
 *				      and check if it passes the pre-set
 *				      threshold.
 * @soc: Datapath global soc handle
 *
 * This function calculates the current TX and RX throughput and checks
 * if it is above the pre-set thresholds by SWLM.
 *
 * Returns: true, if the TX/RX throughput is passing the threshold
 *	    false, otherwise
 */
static bool dp_swlm_is_tput_thresh_reached(struct dp_soc *soc)
{
	struct dp_swlm *swlm = &soc->swlm;
	static int prev_rx_bytes, prev_tx_bytes;
	int rx_delta, tx_delta, tx_packet_delta;
	static int prev_tx_packets;
	bool result = false;

	tx_delta = soc->stats.tx.egress.bytes - prev_tx_bytes;
	prev_tx_bytes = soc->stats.tx.egress.bytes;
	if (tx_delta > swlm->params.tcl.tx_traffic_thresh) {
		swlm->params.tcl.sampling_session_tx_bytes = tx_delta;
		result = true;
	}

	rx_delta = soc->stats.rx.ingress.bytes - prev_rx_bytes;
	prev_rx_bytes = soc->stats.rx.ingress.bytes;
	if (!result && rx_delta > swlm->params.tcl.rx_traffic_thresh) {
		swlm->params.tcl.sampling_session_tx_bytes = tx_delta;
		result = true;
	}

	tx_packet_delta = soc->stats.tx.egress.num - prev_tx_packets;
	prev_tx_packets = soc->stats.tx.egress.num;
	if (tx_packet_delta < swlm->params.tcl.tx_pkt_thresh)
		result = false;

	return result;
}

/**
 * dp_swlm_can_tcl_wr_coalesce() - To check if current TCL reg write can be
 *				   coalesced or not.
 * @soc: Datapath global soc handle
 * @tcl_data: priv data for tcl coalescing
 *
 * This function takes into account the current tx and rx throughput and
 * decides whether the TCL register write corresponding to the current packet,
 * to be transmitted, is to be processed or coalesced.
 * It maintains a session for which the TCL register writes are coalesced and
 * then flushed if a certain time/bytes threshold is reached.
 *
 * Returns: 1 if the current TCL write is to be coalesced
 *	    0, if the current TCL write is to be processed.
 */
static int
dp_swlm_can_tcl_wr_coalesce(struct dp_soc *soc,
			    struct dp_swlm_tcl_data *tcl_data)
{
	u64 curr_time = qdf_get_log_timestamp_usecs();
	int tput_level_pass, coalesce = 0;
	struct dp_swlm *swlm = &soc->swlm;
	static int tput_pass_cnt;
	static u64 expire_time;

	if (curr_time >= expire_time) {
		expire_time = qdf_get_log_timestamp_usecs() +
			      swlm->params.tcl.sampling_time;
		tput_level_pass = dp_swlm_is_tput_thresh_reached(soc);
		if (tput_level_pass) {
			tput_pass_cnt++;
		} else {
			tput_pass_cnt = 0;
			DP_STATS_INC(swlm, tcl.tput_criteria_fail, 1);
			goto coalescing_fail;
		}
	}

	swlm->params.tcl.bytes_coalesced += qdf_nbuf_len(tcl_data->nbuf);

	if (tput_pass_cnt > DP_SWLM_TCL_TPUT_PASS_THRESH) {
		coalesce = 1;
		if (swlm->params.tcl.bytes_coalesced >
		    swlm->params.tcl.bytes_flush_thresh) {
			coalesce = 0;
			DP_STATS_INC(swlm, tcl.bytes_thresh_reached, 1);
		} else if (curr_time > swlm->params.tcl.coalesce_end_time) {
			coalesce = 0;
			DP_STATS_INC(swlm, tcl.time_thresh_reached, 1);
		}
	}

coalescing_fail:
	if (!coalesce) {
		dp_swlm_tcl_reset_session_data(soc);
		return 0;
	}

	qdf_timer_mod(&swlm->params.tcl.flush_timer, 1);

	return 1;
}

QDF_STATUS dp_print_swlm_stats(struct dp_soc *soc)
{
	struct dp_swlm *swlm = &soc->swlm;

	dp_info("TCL Coalescing stats:");
	dp_info("Num coalesce success: %d", swlm->stats.tcl.coalesce_success);
	dp_info("Num coalesce fail: %d", swlm->stats.tcl.coalesce_fail);
	dp_info("Timer flush success: %d", swlm->stats.tcl.timer_flush_success);
	dp_info("Timer flush fail: %d", swlm->stats.tcl.timer_flush_fail);
	dp_info("Coalesce fail (TID): %d", swlm->stats.tcl.tid_fail);
	dp_info("Coalesce fail (special frame): %d", swlm->stats.tcl.sp_frames);
	dp_info("Coalesce fail (Low latency connection): %d",
		swlm->stats.tcl.ll_connection);
	dp_info("Coalesce fail (bytes thresh crossed): %d",
		swlm->stats.tcl.bytes_thresh_reached);
	dp_info("Coalesce fail (time thresh crossed): %d",
		swlm->stats.tcl.time_thresh_reached);
	dp_info("Coalesce fail (TPUT sampling fail): %d",
		swlm->stats.tcl.tput_criteria_fail);

	return QDF_STATUS_SUCCESS;
}

static struct dp_swlm_ops dp_latency_mgr_ops = {
	.tcl_wr_coalesce_check = dp_swlm_can_tcl_wr_coalesce,
};

/**
 * dp_swlm_tcl_flush_timer() - Timer handler for tcl register write coalescing
 * @arg: private data of the timer
 *
 * Returns: none
 */
static void dp_swlm_tcl_flush_timer(void *arg)
{
	struct dp_soc *soc = arg;
	struct dp_swlm *swlm = &soc->swlm;
	hal_ring_handle_t hal_ring_hdl =
				soc->tcl_data_ring[0].hal_srng;

	if (hal_srng_try_access_start(soc->hal_soc, hal_ring_hdl) < 0)
		goto fail;

	if (hif_pm_runtime_get(soc->hif_handle, RTPM_ID_DW_TX_HW_ENQUEUE,
			       true)) {
		hal_srng_access_end_reap(soc->hal_soc, hal_ring_hdl);
		hal_srng_set_event(hal_ring_hdl, HAL_SRNG_FLUSH_EVENT);
		hal_srng_inc_flush_cnt(hal_ring_hdl);
		goto fail;
	}

	DP_STATS_INC(swlm, tcl.timer_flush_success, 1);
	hal_srng_access_end(soc->hal_soc, hal_ring_hdl);
	hif_pm_runtime_put(soc->hif_handle, RTPM_ID_DW_TX_HW_ENQUEUE);

	return;

fail:
	DP_STATS_INC(swlm, tcl.timer_flush_fail, 1);

	return;
}

/**
 * dp_soc_swlm_tcl_attach() - attach the TCL resources for the software
 *			      latency manager.
 * @soc: Datapath global soc handle
 *
 * Returns: QDF_STATUS
 */
static inline QDF_STATUS dp_soc_swlm_tcl_attach(struct dp_soc *soc)
{
	struct dp_swlm *swlm = &soc->swlm;

	swlm->params.tcl.rx_traffic_thresh = DP_SWLM_TCL_RX_TRAFFIC_THRESH;
	swlm->params.tcl.tx_traffic_thresh = DP_SWLM_TCL_TX_TRAFFIC_THRESH;
	swlm->params.tcl.sampling_time = DP_SWLM_TCL_TRAFFIC_SAMPLING_TIME;
	swlm->params.tcl.bytes_flush_thresh = 0;
	swlm->params.tcl.time_flush_thresh = DP_SWLM_TCL_TIME_FLUSH_THRESH;
	swlm->params.tcl.tx_thresh_multiplier =
					DP_SWLM_TCL_TX_THRESH_MULTIPLIER;
	swlm->params.tcl.tx_pkt_thresh = DP_SWLM_TCL_TX_PKT_THRESH;

	qdf_timer_init(soc->osdev, &swlm->params.tcl.flush_timer,
		       dp_swlm_tcl_flush_timer, (void *)soc,
		       QDF_TIMER_TYPE_WAKE_APPS);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_soc_swlm_tcl_detach() - detach the TCL resources for the software
 *			      latency manager.
 * @soc: Datapath global soc handle
 *
 * Returns: QDF_STATUS
 */
static inline QDF_STATUS dp_soc_swlm_tcl_detach(struct dp_soc *soc)
{
	struct dp_swlm *swlm = &soc->swlm;

	qdf_timer_stop(&swlm->params.tcl.flush_timer);
	qdf_timer_free(&swlm->params.tcl.flush_timer);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_soc_swlm_attach(struct dp_soc *soc)
{
	struct wlan_cfg_dp_soc_ctxt *cfg = soc->wlan_cfg_ctx;
	struct dp_swlm *swlm = &soc->swlm;
	QDF_STATUS ret;

	/* Check if it is enabled in the INI */
	if (!wlan_cfg_is_swlm_enabled(cfg)) {
		dp_err("SWLM feature is disabled");
		swlm->is_init = false;
		swlm->is_enabled = false;
		return QDF_STATUS_E_NOSUPPORT;
	}

	swlm->ops = &dp_latency_mgr_ops;

	ret = dp_soc_swlm_tcl_attach(soc);
	if (QDF_IS_STATUS_ERROR(ret))
		goto swlm_tcl_setup_fail;

	swlm->is_init = true;
	swlm->is_enabled = true;

	return QDF_STATUS_SUCCESS;

swlm_tcl_setup_fail:
	swlm->is_enabled = false;
	return ret;
}

QDF_STATUS dp_soc_swlm_detach(struct dp_soc *soc)
{
	struct dp_swlm *swlm = &soc->swlm;
	QDF_STATUS ret;

	swlm->is_enabled = false;

	ret = dp_soc_swlm_tcl_detach(soc);
	if (QDF_IS_STATUS_ERROR(ret))
		return ret;

	swlm->ops = NULL;

	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_DP_FEATURE_SW_LATENCY_MGR */
