/*
 * Copyright (c) 2013-2021, The Linux Foundation. All rights reserved.
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

#include "osdep.h"
#include "wmi.h"
#include "wmi_unified_priv.h"
#include "wmi_unified_param.h"

/**
 * extract_per_chain_rssi_stats_tlv() - api to extract rssi stats from event
 * buffer
 * @wmi_handle: wmi handle
 * @evt_buf: pointer to event buffer
 * @index: Index into vdev stats
 * @rssi_stats: Pointer to hold rssi stats
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS
extract_per_chain_rssi_stats_tlv(wmi_unified_t wmi_handle, void *evt_buf,
			       uint32_t index,
			       struct wmi_host_per_chain_rssi_stats *rssi_stats)
{
	uint8_t *data;
	wmi_rssi_stats *fw_rssi_stats;
	wmi_per_chain_rssi_stats *rssi_event;
	WMI_UPDATE_STATS_EVENTID_param_tlvs *param_buf;

	if (!evt_buf) {
		wmi_err("evt_buf is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	param_buf = (WMI_UPDATE_STATS_EVENTID_param_tlvs *) evt_buf;
	rssi_event = param_buf->chain_stats;

	if (index >= rssi_event->num_per_chain_rssi_stats) {
		wmi_err("Invalid index: %u", index);
		return QDF_STATUS_E_INVAL;
	}

	data = ((uint8_t *)(&rssi_event[1])) + WMI_TLV_HDR_SIZE;
	fw_rssi_stats = &((wmi_rssi_stats *)data)[index];
	if (fw_rssi_stats->vdev_id >= WLAN_UMAC_PDEV_MAX_VDEVS)
		return QDF_STATUS_E_INVAL;

	rssi_stats->vdev_id = fw_rssi_stats->vdev_id;
	qdf_mem_copy(rssi_stats->rssi_avg_beacon,
		     fw_rssi_stats->rssi_avg_beacon,
		     sizeof(fw_rssi_stats->rssi_avg_beacon));
	qdf_mem_copy(rssi_stats->rssi_avg_data,
		     fw_rssi_stats->rssi_avg_data,
		     sizeof(fw_rssi_stats->rssi_avg_data));
	qdf_mem_copy(&rssi_stats->peer_macaddr,
		     &fw_rssi_stats->peer_macaddr,
		     sizeof(fw_rssi_stats->peer_macaddr));

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_peer_adv_stats_tlv() - extract adv peer stats from event
 * @param wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param index: Index into extended peer stats
 * @param peer_adv_stats: Pointer to hold adv peer stats
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS extract_peer_adv_stats_tlv(wmi_unified_t wmi_handle,
					     void *evt_buf,
					     struct wmi_host_peer_adv_stats
					     *peer_adv_stats)
{
	WMI_UPDATE_STATS_EVENTID_param_tlvs *param_buf;
	wmi_peer_extd2_stats *adv_stats;
	int i;

	param_buf = (WMI_UPDATE_STATS_EVENTID_param_tlvs *)evt_buf;

	adv_stats = param_buf->peer_extd2_stats;
	if (!adv_stats) {
		wmi_debug("no peer_adv stats in event buffer");
		return QDF_STATUS_E_INVAL;
	}

	for (i = 0; i < param_buf->num_peer_extd2_stats; i++) {
		WMI_MAC_ADDR_TO_CHAR_ARRAY(&adv_stats[i].peer_macaddr,
					   peer_adv_stats[i].peer_macaddr);
		peer_adv_stats[i].fcs_count = adv_stats[i].rx_fcs_err;
		peer_adv_stats[i].rx_bytes =
				(uint64_t)adv_stats[i].rx_bytes_u32 <<
				WMI_LOWER_BITS_SHIFT_32 |
				adv_stats[i].rx_bytes_l32;
		peer_adv_stats[i].rx_count = adv_stats[i].rx_mpdus;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_MIB_STATS
/**
 * extract_mib_stats_tlv() - extract mib stats from event
 * @wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param mib_stats: pointer to hold mib stats
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS extract_mib_stats_tlv(wmi_unified_t wmi_handle,
					void *evt_buf,
					struct mib_stats_metrics
					*mib_stats)
{
	WMI_UPDATE_STATS_EVENTID_param_tlvs *param_buf;
	wmi_stats_event_fixed_param *ev_param;
	uint8_t *data;
	wmi_mib_stats *ev;
	wmi_mib_extd_stats *ev_extd;

	param_buf = (WMI_UPDATE_STATS_EVENTID_param_tlvs *)evt_buf;
	ev_param = (wmi_stats_event_fixed_param *)param_buf->fixed_param;
	data = (uint8_t *)param_buf->data;

	ev = (wmi_mib_stats *)(data +
	      ev_param->num_pdev_stats * sizeof(wmi_pdev_stats) +
	      ev_param->num_vdev_stats * sizeof(wmi_vdev_stats) +
	      ev_param->num_peer_stats * sizeof(wmi_peer_stats) +
	      ev_param->num_bcnflt_stats *
	      sizeof(wmi_bcnfilter_stats_t) +
	      ev_param->num_chan_stats * sizeof(wmi_chan_stats));

	qdf_mem_zero(mib_stats, sizeof(*mib_stats));

	mib_stats->mib_counters.tx_frags =
		ev->tx_mpdu_grp_frag_cnt;
	mib_stats->mib_counters.group_tx_frames =
		ev->tx_msdu_grp_frm_cnt;
	mib_stats->mib_counters.failed_cnt = ev->tx_msdu_fail_cnt;
	mib_stats->mib_counters.rx_frags = ev->rx_mpdu_frag_cnt;
	mib_stats->mib_counters.group_rx_frames =
		ev->rx_msdu_grp_frm_cnt;
	mib_stats->mib_counters.fcs_error_cnt =
		ev->rx_mpdu_fcs_err;
	mib_stats->mib_counters.tx_frames =
		ev->tx_msdu_frm_cnt;
	mib_stats->mib_mac_statistics.retry_cnt =
		ev->tx_msdu_retry_cnt;
	mib_stats->mib_mac_statistics.frame_dup_cnt =
		ev->rx_frm_dup_cnt;
	mib_stats->mib_mac_statistics.rts_success_cnt =
		ev->tx_rts_success_cnt;
	mib_stats->mib_mac_statistics.rts_fail_cnt =
		ev->tx_rts_fail_cnt;

	mib_stats->mib_qos_counters.qos_tx_frag_cnt =
		 ev->tx_Qos_mpdu_grp_frag_cnt;
	mib_stats->mib_qos_counters.qos_retry_cnt =
		ev->tx_Qos_msdu_retry_UP;
	mib_stats->mib_qos_counters.qos_failed_cnt =
		ev->tx_Qos_msdu_fail_UP;
	mib_stats->mib_qos_counters.qos_frame_dup_cnt =
		ev->rx_Qos_frm_dup_cnt_UP;
	mib_stats->mib_qos_counters.qos_rts_success_cnt =
		ev->tx_Qos_rts_success_cnt_UP;
	mib_stats->mib_qos_counters.qos_rts_fail_cnt =
		ev->tx_Qos_rts_fail_cnt_UP;
	mib_stats->mib_qos_counters.qos_rx_frag_cnt =
		ev->rx_Qos_mpdu_frag_cnt_UP;
	mib_stats->mib_qos_counters.qos_tx_frame_cnt =
		ev->tx_Qos_msdu_frm_cnt_UP;
	mib_stats->mib_qos_counters.qos_discarded_frame_cnt =
		ev->rx_Qos_msdu_discard_cnt_UP;
	mib_stats->mib_qos_counters.qos_mpdu_rx_cnt =
		ev->rx_Qos_mpdu_cnt;
	mib_stats->mib_qos_counters.qos_retries_rx_cnt =
		ev->rx_Qos_mpdu_retryBit_cnt;

	mib_stats->mib_rsna_stats.tkip_icv_err =
		ev->rsna_TKIP_icv_err_cnt;
	mib_stats->mib_rsna_stats.tkip_replays =
		ev->rsna_TKIP_replay_err_cnt;
	mib_stats->mib_rsna_stats.ccmp_decrypt_err =
		ev->rsna_CCMP_decrypt_err_cnt;
	mib_stats->mib_rsna_stats.ccmp_replays =
		ev->rsna_CCMP_replay_err_cnt;

	mib_stats->mib_counters_group3.tx_ampdu_cnt =
		ev->tx_ampdu_cnt;
	mib_stats->mib_counters_group3.tx_mpdus_in_ampdu_cnt =
		ev->tx_mpdu_cnt_in_ampdu;
	mib_stats->mib_counters_group3.tx_octets_in_ampdu_cnt =
		ev->tx_octets_in_ampdu.upload.high;
	mib_stats->mib_counters_group3.tx_octets_in_ampdu_cnt =
		mib_stats->mib_counters_group3.tx_octets_in_ampdu_cnt << 32;
	mib_stats->mib_counters_group3.tx_octets_in_ampdu_cnt +=
		ev->tx_octets_in_ampdu.upload.low;

	mib_stats->mib_counters_group3.ampdu_rx_cnt =
		ev->rx_ampdu_cnt;
	mib_stats->mib_counters_group3.mpdu_in_rx_ampdu_cnt =
		ev->rx_mpdu_cnt_in_ampdu;
	mib_stats->mib_counters_group3.rx_octets_in_ampdu_cnt =
		ev->rx_octets_in_ampdu.upload.rx_octets_in_ampdu_high;
	mib_stats->mib_counters_group3.rx_octets_in_ampdu_cnt =
		mib_stats->mib_counters_group3.rx_octets_in_ampdu_cnt << 32;
	mib_stats->mib_counters_group3.rx_octets_in_ampdu_cnt +=
		ev->rx_octets_in_ampdu.upload.rx_octets_in_ampdu_low;

	if (ev_param->num_mib_extd_stats) {
		ev_extd = (wmi_mib_extd_stats *)((uint8_t *)ev +
			   ev_param->num_mib_stats * sizeof(wmi_mib_stats) +
			   ev_param->num_bcn_stats * sizeof(wmi_bcn_stats) +
			   ev_param->num_peer_extd_stats *
			   sizeof(wmi_peer_extd_stats));
		mib_stats->mib_mac_statistics.multi_retry_cnt =
			ev_extd->tx_msdu_multi_retry_cnt;
		mib_stats->mib_mac_statistics.tx_ack_fail_cnt =
			ev_extd->tx_ack_fail_cnt;

		mib_stats->mib_qos_counters.qos_multi_retry_cnt =
			ev_extd->tx_qos_msdu_multi_retry_up;
		mib_stats->mib_qos_counters.tx_qos_ack_fail_cnt_up =
			ev_extd->tx_qos_ack_fail_cnt_up;

		mib_stats->mib_rsna_stats.cmac_icv_err =
			ev_extd->rsna_cmac_icv_err_cnt;
		mib_stats->mib_rsna_stats.cmac_replays =
			ev_extd->rsna_cmac_replay_err_cnt;

		mib_stats->mib_counters_group3.rx_ampdu_deli_crc_err_cnt =
			ev_extd->rx_ampdu_deli_crc_err_cnt;
	}

	return QDF_STATUS_SUCCESS;
}

static void wmi_cp_stats_attach_mib_stats_tlv(struct wmi_ops *ops)
{
	ops->extract_mib_stats = extract_mib_stats_tlv;
}
#else
static void wmi_cp_stats_attach_mib_stats_tlv(struct wmi_ops *ops)
{
}
#endif /* WLAN_FEATURE_MIB_STATS */

/**
 * send_request_peer_stats_info_cmd_tlv() - WMI request peer stats function
 * @wmi_handle: handle to WMI.
 * @param: pointer to hold peer stats request parameter
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
send_request_peer_stats_info_cmd_tlv(wmi_unified_t wmi_handle,
				     struct peer_stats_request_params *param)
{
	int32_t ret;
	wmi_request_peer_stats_info_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	uint16_t len = sizeof(wmi_request_peer_stats_info_cmd_fixed_param);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_request_peer_stats_info_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		   WMITLV_TAG_STRUC_wmi_request_peer_stats_info_cmd_fixed_param,
		   WMITLV_GET_STRUCT_TLVLEN(
				  wmi_request_peer_stats_info_cmd_fixed_param));
	cmd->request_type = param->request_type;
	cmd->vdev_id = param->vdev_id;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(param->peer_mac_addr, &cmd->peer_macaddr);
	cmd->reset_after_request = param->reset_after_request;

	wmi_debug("PEER STATS REQ VDEV_ID:%d PEER:"QDF_MAC_ADDR_FMT" TYPE:%d RESET:%d",
		 cmd->vdev_id, QDF_MAC_ADDR_REF(param->peer_mac_addr),
		 cmd->request_type,
		 cmd->reset_after_request);

	wmi_mtrace(WMI_REQUEST_PEER_STATS_INFO_CMDID, cmd->vdev_id, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
				   WMI_REQUEST_PEER_STATS_INFO_CMDID);
	if (ret) {
		wmi_err("Failed to send peer stats request to fw =%d", ret);
		wmi_buf_free(buf);
	}

	return qdf_status_from_os_return(ret);
}

/**
 * extract_peer_stats_count_tlv() - extract peer stats count from event
 * @wmi_handle: wmi handle
 * @evt_buf: pointer to event buffer
 * @stats_param: Pointer to hold stats count
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS
extract_peer_stats_count_tlv(wmi_unified_t wmi_handle, void *evt_buf,
			     wmi_host_stats_event *stats_param)
{
	WMI_PEER_STATS_INFO_EVENTID_param_tlvs *param_buf;
	wmi_peer_stats_info_event_fixed_param *ev_param;

	param_buf = (WMI_PEER_STATS_INFO_EVENTID_param_tlvs *)evt_buf;
	if (!param_buf)
		return QDF_STATUS_E_FAILURE;

	ev_param = param_buf->fixed_param;
	if (!ev_param)
		return QDF_STATUS_E_FAILURE;

	if (!param_buf->num_peer_stats_info ||
	    param_buf->num_peer_stats_info < ev_param->num_peers) {
		wmi_err_rl("actual num of peers stats info: %d is less than provided peers: %d",
			   param_buf->num_peer_stats_info, ev_param->num_peers);
		return QDF_STATUS_E_FAULT;
	}

	if (!stats_param)
		return QDF_STATUS_E_FAILURE;

	stats_param->num_peer_stats_info_ext = ev_param->num_peers;

	return QDF_STATUS_SUCCESS;
}

static void dump_peer_stats_info(wmi_peer_stats_info *stats)
{
	u_int8_t mac[6];
	int i;

	WMI_MAC_ADDR_TO_CHAR_ARRAY(&stats->peer_macaddr, mac);
	wmi_debug("mac "QDF_MAC_ADDR_FMT, QDF_MAC_ADDR_REF(mac));
	wmi_debug("tx_bytes %d %d tx_packets %d %d tx_retries %d tx_failed %d",
		 stats->tx_bytes.low_32,
		 stats->tx_bytes.high_32,
		 stats->tx_packets.low_32,
		 stats->tx_packets.high_32,
		 stats->tx_retries, stats->tx_failed);
	wmi_debug("rx_bytes %d %d rx_packets %d %d",
		 stats->rx_bytes.low_32,
		 stats->rx_bytes.high_32,
		 stats->rx_packets.low_32,
		 stats->rx_packets.high_32);
	wmi_debug("tx_rate_code %x rx_rate_code %x tx_rate %x rx_rate %x peer_rssi %d tx_succeed %d",
		 stats->last_tx_rate_code,
		 stats->last_rx_rate_code,
		 stats->last_tx_bitrate_kbps,
		 stats->last_rx_bitrate_kbps,
		 stats->peer_rssi, stats->tx_succeed);
	for (i = 0; i < WMI_MAX_CHAINS; i++)
		wmi_debug("chain%d_rssi %d", i, stats->peer_rssi_per_chain[i]);
}

/**
 * extract_peer_stats_info_tlv() - extract peer stats info from event
 * @wmi_handle: wmi handle
 * @evt_buf: pointer to event buffer
 * @index: Index into vdev stats
 * @peer_stats_info: Pointer to hold peer stats info
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS
extract_peer_stats_info_tlv(wmi_unified_t wmi_handle, void *evt_buf,
			    uint32_t index,
			    wmi_host_peer_stats_info *peer_stats_info)
{
	WMI_PEER_STATS_INFO_EVENTID_param_tlvs *param_buf;
	wmi_peer_stats_info_event_fixed_param *ev_param;

	param_buf = (WMI_PEER_STATS_INFO_EVENTID_param_tlvs *)evt_buf;
	ev_param = param_buf->fixed_param;

	if (index < ev_param->num_peers) {
		wmi_peer_stats_info *ev = &param_buf->peer_stats_info[index];
		int i;

		dump_peer_stats_info(ev);

		WMI_MAC_ADDR_TO_CHAR_ARRAY(&ev->peer_macaddr,
					   peer_stats_info->peer_macaddr.bytes);
		peer_stats_info->tx_packets = ev->tx_packets.low_32;
		peer_stats_info->tx_bytes = ev->tx_bytes.high_32;
		peer_stats_info->tx_bytes <<= 32;
		peer_stats_info->tx_bytes += ev->tx_bytes.low_32;
		peer_stats_info->rx_packets = ev->rx_packets.low_32;
		peer_stats_info->rx_bytes = ev->rx_bytes.high_32;
		peer_stats_info->rx_bytes <<= 32;
		peer_stats_info->rx_bytes += ev->rx_bytes.low_32;
		peer_stats_info->tx_retries = ev->tx_retries;
		peer_stats_info->tx_failed = ev->tx_failed;
		peer_stats_info->tx_succeed = ev->tx_succeed;
		peer_stats_info->peer_rssi = ev->peer_rssi;
		peer_stats_info->last_tx_bitrate_kbps =
						ev->last_tx_bitrate_kbps;
		peer_stats_info->last_tx_rate_code = ev->last_tx_rate_code;
		peer_stats_info->last_rx_bitrate_kbps =
						ev->last_rx_bitrate_kbps;
		peer_stats_info->last_rx_rate_code = ev->last_rx_rate_code;
		for (i = 0; i < WMI_MAX_CHAINS; i++)
			peer_stats_info->peer_rssi_per_chain[i] =
						     ev->peer_rssi_per_chain[i];
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_BIG_DATA_STATS
/**
 * extract_big_data_stats_tlv() - extract big data from event
 * @param wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param stats_param: Pointer to hold big data stats
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS
extract_big_data_stats_tlv(wmi_unified_t wmi_handle, void *evt_buf,
			   struct big_data_stats_event *stats)
{
	WMI_VDEV_SEND_BIG_DATA_P2_EVENTID_param_tlvs *param_buf;
	wmi_vdev_send_big_data_p2_event_fixed_param *event;
	wmi_big_data_dp_stats_tlv_param *dp_stats_param_buf;

	param_buf = (WMI_VDEV_SEND_BIG_DATA_P2_EVENTID_param_tlvs *)evt_buf;
	if (!param_buf) {
		wmi_err("invalid buffer");
		return QDF_STATUS_E_FAILURE;
	}

	event = param_buf->fixed_param;
	if (!event) {
		wmi_err("invalid fixed param");
		return QDF_STATUS_E_FAILURE;
	}

	dp_stats_param_buf = param_buf->big_data_dp_stats;
	if (!dp_stats_param_buf) {
		wmi_err("invalid dp stats param");
		return QDF_STATUS_E_FAILURE;
	}

	stats->vdev_id = event->vdev_id;
	stats->ani_level = event->ani_level;
	stats->tsf_out_of_sync = event->tsf_out_of_sync;

	stats->last_data_tx_pwr = dp_stats_param_buf->last_data_tx_pwr;
	stats->target_power_dsss = dp_stats_param_buf->target_power_dsss;
	stats->target_power_ofdm = dp_stats_param_buf->target_power_ofdm;
	stats->last_tx_data_rix = dp_stats_param_buf->last_tx_data_rix;
	stats->last_tx_data_rate_kbps =
		dp_stats_param_buf->last_tx_data_rate_kbps;

	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_FEATURE_BIG_DATA_STATS
static void
wmi_attach_big_data_stats_handler(struct wmi_ops *ops)
{
	ops->extract_big_data_stats = extract_big_data_stats_tlv;
}
#else
static void
wmi_attach_big_data_stats_handler(struct wmi_ops *ops)
{}
#endif

void wmi_mc_cp_stats_attach_tlv(wmi_unified_t wmi_handle)
{
	struct wmi_ops *ops = wmi_handle->ops;

	ops->extract_per_chain_rssi_stats = extract_per_chain_rssi_stats_tlv;
	ops->extract_peer_adv_stats = extract_peer_adv_stats_tlv;
	wmi_cp_stats_attach_mib_stats_tlv(ops);
	ops->send_request_peer_stats_info_cmd =
		send_request_peer_stats_info_cmd_tlv;
	ops->extract_peer_stats_count = extract_peer_stats_count_tlv;
	ops->extract_peer_stats_info = extract_peer_stats_info_tlv;
	wmi_attach_big_data_stats_handler(ops);
}
