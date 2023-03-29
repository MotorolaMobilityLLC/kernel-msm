/*
 * Copyright (c) 2013-2021 The Linux Foundation. All rights reserved.
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
 *  DOC:  wma_mgmt.c
 *
 *  This file contains STA/SAP and protocol related functions.
 */

/* Header files */

#include "wma.h"
#include "wma_api.h"
#include "cds_api.h"
#include "wmi_unified_api.h"
#include "wlan_qct_sys.h"
#include "wni_api.h"
#include "ani_global.h"
#include "wmi_unified.h"
#include "wni_cfg.h"

#include "qdf_nbuf.h"
#include "qdf_types.h"
#include "qdf_mem.h"

#include "wma_types.h"
#include "lim_api.h"
#include "lim_session_utils.h"

#include "cds_utils.h"
#include "wlan_blm_api.h"
#if !defined(REMOVE_PKT_LOG)
#include "pktlog_ac.h"
#else
#include "pktlog_ac_fmt.h"
#endif /* REMOVE_PKT_LOG */

#include "dbglog_host.h"
#include "csr_api.h"
#include "ol_fw.h"
#include "wma_internal.h"
#include "wlan_policy_mgr_api.h"
#include "cdp_txrx_flow_ctrl_legacy.h"
#include <cdp_txrx_peer_ops.h>
#include <cdp_txrx_pmf.h>
#include <cdp_txrx_cfg.h>
#include <cdp_txrx_cmn.h>
#include <cdp_txrx_misc.h>
#include <cdp_txrx_misc.h>
#include "wlan_mgmt_txrx_tgt_api.h"
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_lmac_if_api.h"
#include <cdp_txrx_handle.h>
#include "wma_he.h"
#include <qdf_crypto.h>
#include "wma_twt.h"
#include "wlan_p2p_cfg_api.h"
#include "cfg_ucfg_api.h"
#include "cfg_mlme_sta.h"
#include "wlan_mlme_api.h"
#include "wmi_unified_bcn_api.h"
#include <wlan_crypto_global_api.h>
#include <wlan_mlme_main.h>
#include <../../core/src/vdev_mgr_ops.h>
#include "wlan_pkt_capture_ucfg_api.h"

#if !defined(REMOVE_PKT_LOG)
#include <wlan_logging_sock_svc.h>
#endif
#include "wlan_cm_roam_api.h"

/**
 * wma_send_bcn_buf_ll() - prepare and send beacon buffer to fw for LL
 * @wma: wma handle
 * @vdev_id: vdev id
 * @param_buf: SWBA parameters
 *
 * Return: none
 */
#ifdef WLAN_WMI_BCN
static void wma_send_bcn_buf_ll(tp_wma_handle wma,
				uint8_t vdev_id,
				WMI_HOST_SWBA_EVENTID_param_tlvs *param_buf)
{
	struct ieee80211_frame *wh;
	struct beacon_info *bcn;
	wmi_tim_info *tim_info = param_buf->tim_info;
	uint8_t *bcn_payload;
	QDF_STATUS ret;
	struct beacon_tim_ie *tim_ie;
	wmi_p2p_noa_info *p2p_noa_info = param_buf->p2p_noa_info;
	struct p2p_sub_element_noa noa_ie;
	struct wmi_bcn_send_from_host params;
	uint8_t i;

	bcn = wma->interfaces[vdev_id].beacon;
	if (!bcn || !bcn->buf) {
		wma_err("Invalid beacon buffer");
		return;
	}

	if (!param_buf->tim_info || !param_buf->p2p_noa_info) {
		wma_err("Invalid tim info or p2p noa info");
		return;
	}

	if (WMI_UNIFIED_NOA_ATTR_NUM_DESC_GET(p2p_noa_info) >
			WMI_P2P_MAX_NOA_DESCRIPTORS) {
		wma_err("Too many descriptors %d",
			WMI_UNIFIED_NOA_ATTR_NUM_DESC_GET(p2p_noa_info));
		return;
	}

	qdf_spin_lock_bh(&bcn->lock);

	bcn_payload = qdf_nbuf_data(bcn->buf);

	tim_ie = (struct beacon_tim_ie *)(&bcn_payload[bcn->tim_ie_offset]);

	if (tim_info->tim_changed) {
		if (tim_info->tim_num_ps_pending)
			qdf_mem_copy(&tim_ie->tim_bitmap, tim_info->tim_bitmap,
				     WMA_TIM_SUPPORTED_PVB_LENGTH);
		else
			qdf_mem_zero(&tim_ie->tim_bitmap,
				     WMA_TIM_SUPPORTED_PVB_LENGTH);
		/*
		 * Currently we support fixed number of
		 * peers as limited by HAL_NUM_STA.
		 * tim offset is always 0
		 */
		tim_ie->tim_bitctl = 0;
	}

	/* Update DTIM Count */
	if (tim_ie->dtim_count == 0)
		tim_ie->dtim_count = tim_ie->dtim_period - 1;
	else
		tim_ie->dtim_count--;

	/*
	 * DTIM count needs to be backedup so that
	 * when umac updates the beacon template
	 * current dtim count can be updated properly
	 */
	bcn->dtim_count = tim_ie->dtim_count;

	/* update state for buffered multicast frames on DTIM */
	if (tim_info->tim_mcast && (tim_ie->dtim_count == 0 ||
				    tim_ie->dtim_period == 1))
		tim_ie->tim_bitctl |= 1;
	else
		tim_ie->tim_bitctl &= ~1;

	/* To avoid sw generated frame sequence the same as H/W generated frame,
	 * the value lower than min_sw_seq is reserved for HW generated frame
	 */
	if ((bcn->seq_no & IEEE80211_SEQ_MASK) < MIN_SW_SEQ)
		bcn->seq_no = MIN_SW_SEQ;

	wh = (struct ieee80211_frame *)bcn_payload;
	*(uint16_t *) &wh->i_seq[0] = htole16(bcn->seq_no
					      << IEEE80211_SEQ_SEQ_SHIFT);
	bcn->seq_no++;

	if (WMI_UNIFIED_NOA_ATTR_IS_MODIFIED(p2p_noa_info)) {
		qdf_mem_zero(&noa_ie, sizeof(noa_ie));

		noa_ie.index =
			(uint8_t) WMI_UNIFIED_NOA_ATTR_INDEX_GET(p2p_noa_info);
		noa_ie.oppPS =
			(uint8_t) WMI_UNIFIED_NOA_ATTR_OPP_PS_GET(p2p_noa_info);
		noa_ie.ctwindow =
			(uint8_t) WMI_UNIFIED_NOA_ATTR_CTWIN_GET(p2p_noa_info);
		noa_ie.num_descriptors = (uint8_t)
				WMI_UNIFIED_NOA_ATTR_NUM_DESC_GET(p2p_noa_info);
		wma_debug("index %u, oppPs %u, ctwindow %u, num_descriptors = %u",
			 noa_ie.index,
			 noa_ie.oppPS, noa_ie.ctwindow, noa_ie.num_descriptors);
		for (i = 0; i < noa_ie.num_descriptors; i++) {
			noa_ie.noa_descriptors[i].type_count =
				(uint8_t) p2p_noa_info->noa_descriptors[i].
				type_count;
			noa_ie.noa_descriptors[i].duration =
				p2p_noa_info->noa_descriptors[i].duration;
			noa_ie.noa_descriptors[i].interval =
				p2p_noa_info->noa_descriptors[i].interval;
			noa_ie.noa_descriptors[i].start_time =
				p2p_noa_info->noa_descriptors[i].start_time;
			wma_debug("NoA descriptor[%d] type_count %u, duration %u, interval %u, start_time = %u",
				 i,
				 noa_ie.noa_descriptors[i].type_count,
				 noa_ie.noa_descriptors[i].duration,
				 noa_ie.noa_descriptors[i].interval,
				 noa_ie.noa_descriptors[i].start_time);
		}
		wma_update_noa(bcn, &noa_ie);

		/* Send a msg to LIM to update the NoA IE in probe response
		 * frames transmitted by the host
		 */
		wma_update_probe_resp_noa(wma, &noa_ie);
	}

	if (bcn->dma_mapped) {
		qdf_nbuf_unmap_single(wma->qdf_dev, bcn->buf, QDF_DMA_TO_DEVICE);
		bcn->dma_mapped = 0;
	}
	ret = qdf_nbuf_map_single(wma->qdf_dev, bcn->buf, QDF_DMA_TO_DEVICE);
	if (ret != QDF_STATUS_SUCCESS) {
		wma_err("failed map beacon buf to DMA region");
		qdf_spin_unlock_bh(&bcn->lock);
		return;
	}

	bcn->dma_mapped = 1;
	params.vdev_id = vdev_id;
	params.data_len = bcn->len;
	params.frame_ctrl = *((A_UINT16 *) wh->i_fc);
	params.frag_ptr = qdf_nbuf_get_frag_paddr(bcn->buf, 0);
	params.dtim_flag = 0;
	/* notify Firmware of DTM and mcast/bcast traffic */
	if (tim_ie->dtim_count == 0) {
		params.dtim_flag |= WMI_BCN_SEND_DTIM_ZERO;
		/* deliver mcast/bcast traffic in next DTIM beacon */
		if (tim_ie->tim_bitctl & 0x01)
			params.dtim_flag |= WMI_BCN_SEND_DTIM_BITCTL_SET;
	}

	wmi_unified_bcn_buf_ll_cmd(wma->wmi_handle,
					&params);

	qdf_spin_unlock_bh(&bcn->lock);
}
#else
static inline void
wma_send_bcn_buf_ll(tp_wma_handle wma,
		    uint8_t vdev_id,
		    WMI_HOST_SWBA_EVENTID_param_tlvs *param_buf)
{
}
#endif
/**
 * wma_beacon_swba_handler() - swba event handler
 * @handle: wma handle
 * @event: event data
 * @len: data length
 *
 * SWBA event is alert event to Host requesting host to Queue a beacon
 * for transmission use only in host beacon mode
 *
 * Return: 0 for success or error code
 */
#ifdef WLAN_WMI_BCN
int wma_beacon_swba_handler(void *handle, uint8_t *event, uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_HOST_SWBA_EVENTID_param_tlvs *param_buf;
	wmi_host_swba_event_fixed_param *swba_event;
	uint32_t vdev_map;
	uint8_t vdev_id = 0;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	param_buf = (WMI_HOST_SWBA_EVENTID_param_tlvs *) event;
	if (!param_buf) {
		wma_err("Invalid swba event buffer");
		return -EINVAL;
	}
	swba_event = param_buf->fixed_param;
	vdev_map = swba_event->vdev_map;

	wma_debug("vdev_map = %d", vdev_map);
	for (; vdev_map && vdev_id < wma->max_bssid;
			vdev_id++, vdev_map >>= 1) {
		if (!(vdev_map & 0x1))
			continue;
		if (!cdp_cfg_is_high_latency(soc,
			(struct cdp_cfg *)cds_get_context(QDF_MODULE_ID_CFG)))
			wma_send_bcn_buf_ll(wma, vdev_id, param_buf);
		break;
	}
	return 0;
}
#else
static inline int
wma_beacon_swba_handler(void *handle, uint8_t *event, uint32_t len)
{
	return 0;
}
#endif

#ifdef FEATURE_WLAN_DIAG_SUPPORT
void wma_sta_kickout_event(uint32_t kickout_reason, uint8_t vdev_id,
			   uint8_t *macaddr)
{
	WLAN_HOST_DIAG_EVENT_DEF(sta_kickout, struct host_event_wlan_kickout);
	qdf_mem_zero(&sta_kickout, sizeof(sta_kickout));
	sta_kickout.reasoncode = kickout_reason;
	sta_kickout.vdev_id = vdev_id;
	if (macaddr)
		qdf_mem_copy(sta_kickout.peer_mac, macaddr,
			     QDF_MAC_ADDR_SIZE);
	WLAN_HOST_DIAG_EVENT_REPORT(&sta_kickout, EVENT_WLAN_STA_KICKOUT);
}
#endif

int wma_peer_sta_kickout_event_handler(void *handle, uint8_t *event,
				       uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_PEER_STA_KICKOUT_EVENTID_param_tlvs *param_buf = NULL;
	wmi_peer_sta_kickout_event_fixed_param *kickout_event = NULL;
	uint8_t vdev_id, macaddr[QDF_MAC_ADDR_SIZE];
	tpDeleteStaContext del_sta_ctx;
	uint8_t *addr, *bssid;
	struct wlan_objmgr_vdev *vdev;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	param_buf = (WMI_PEER_STA_KICKOUT_EVENTID_param_tlvs *) event;
	kickout_event = param_buf->fixed_param;
	WMI_MAC_ADDR_TO_CHAR_ARRAY(&kickout_event->peer_macaddr, macaddr);
	if (cdp_peer_get_vdevid(soc, macaddr, &vdev_id) !=
			QDF_STATUS_SUCCESS) {
		wma_err("Not able to find BSSID for peer ["QDF_MAC_ADDR_FMT"]",
			 QDF_MAC_ADDR_REF(macaddr));
		return -EINVAL;
	}

	if (!wma_is_vdev_valid(vdev_id))
		return -EINVAL;

	vdev = wma->interfaces[vdev_id].vdev;
	if (!vdev) {
		wma_err("Not able to find vdev for VDEV_%d", vdev_id);
		return -EINVAL;
	}
	addr = wlan_vdev_mlme_get_macaddr(vdev);

	wma_nofl_info("STA kickout for "QDF_MAC_ADDR_FMT", on mac "QDF_MAC_ADDR_FMT", vdev %d, reason:%d",
		      QDF_MAC_ADDR_REF(macaddr), QDF_MAC_ADDR_REF(addr),
		      vdev_id, kickout_event->reason);

	if (wma->interfaces[vdev_id].roaming_in_progress) {
		wma_err("Ignore STA kick out since roaming is in progress");
		return -EINVAL;
	}
	bssid = wma_get_vdev_bssid(wma->interfaces[vdev_id].vdev);
	if (!bssid) {
		wma_err("Failed to get bssid for vdev_%d", vdev_id);
		return -ENOMEM;
	}

	switch (kickout_event->reason) {
	case WMI_PEER_STA_KICKOUT_REASON_IBSS_DISCONNECT:
		goto exit_handler;
#ifdef FEATURE_WLAN_TDLS
	case WMI_PEER_STA_KICKOUT_REASON_TDLS_DISCONNECT:
		del_sta_ctx = (tpDeleteStaContext)
			qdf_mem_malloc(sizeof(tDeleteStaContext));
		if (!del_sta_ctx) {
			wma_err("mem alloc failed for struct del_sta_context for TDLS peer: "QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(macaddr));
			return -ENOMEM;
		}

		del_sta_ctx->is_tdls = true;
		del_sta_ctx->vdev_id = vdev_id;
		qdf_mem_copy(del_sta_ctx->addr2, macaddr, QDF_MAC_ADDR_SIZE);
		qdf_mem_copy(del_sta_ctx->bssId, bssid,
			     QDF_MAC_ADDR_SIZE);
		del_sta_ctx->reasonCode = HAL_DEL_STA_REASON_CODE_KEEP_ALIVE;
		wma_send_msg(wma, SIR_LIM_DELETE_STA_CONTEXT_IND,
			     (void *)del_sta_ctx, 0);
		goto exit_handler;
#endif /* FEATURE_WLAN_TDLS */

	case WMI_PEER_STA_KICKOUT_REASON_UNSPECIFIED:
		/*
		 * Default legacy value used by original firmware implementation
		 */
		if (wma->interfaces[vdev_id].type == WMI_VDEV_TYPE_STA &&
		    (wma->interfaces[vdev_id].sub_type == 0 ||
		     wma->interfaces[vdev_id].sub_type ==
		     WMI_UNIFIED_VDEV_SUBTYPE_P2P_CLIENT) &&
		    !qdf_mem_cmp(bssid,
				    macaddr, QDF_MAC_ADDR_SIZE)) {
			wma_sta_kickout_event(
			HOST_STA_KICKOUT_REASON_UNSPECIFIED, vdev_id, macaddr);
			/*
			 * KICKOUT event is for current station-AP connection.
			 * Treat it like final beacon miss. Station may not have
			 * missed beacons but not able to transmit frames to AP
			 * for a long time. Must disconnect to get out of
			 * this sticky situation.
			 * In future implementation, roaming module will also
			 * handle this event and perform a scan.
			 */
			wma_warn("WMI_PEER_STA_KICKOUT_REASON_UNSPECIFIED event for STA");
			wma_beacon_miss_handler(wma, vdev_id,
						kickout_event->rssi);
			goto exit_handler;
		}
		break;

	case WMI_PEER_STA_KICKOUT_REASON_XRETRY:
	case WMI_PEER_STA_KICKOUT_REASON_INACTIVITY:
	/*
	 * Handle SA query kickout is same as inactivity kickout.
	 * This could be for STA or SAP role
	 */
	case WMI_PEER_STA_KICKOUT_REASON_SA_QUERY_TIMEOUT:
	default:
		break;
	}

	/*
	 * default action is to send delete station context indication to LIM
	 */
	del_sta_ctx =
		(tDeleteStaContext *) qdf_mem_malloc(sizeof(tDeleteStaContext));
	if (!del_sta_ctx) {
		wma_err("QDF MEM Alloc Failed for struct del_sta_context");
		return -ENOMEM;
	}

	del_sta_ctx->is_tdls = false;
	del_sta_ctx->vdev_id = vdev_id;
	qdf_mem_copy(del_sta_ctx->addr2, macaddr, QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(del_sta_ctx->bssId, addr, QDF_MAC_ADDR_SIZE);
	if (kickout_event->reason ==
		WMI_PEER_STA_KICKOUT_REASON_SA_QUERY_TIMEOUT)
		del_sta_ctx->reasonCode =
			HAL_DEL_STA_REASON_CODE_SA_QUERY_TIMEOUT;
	else if (kickout_event->reason == WMI_PEER_STA_KICKOUT_REASON_XRETRY)
		del_sta_ctx->reasonCode = HAL_DEL_STA_REASON_CODE_XRETRY;
	else
		del_sta_ctx->reasonCode = HAL_DEL_STA_REASON_CODE_KEEP_ALIVE;

	if (wmi_service_enabled(wma->wmi_handle,
				wmi_service_hw_db2dbm_support))
		del_sta_ctx->rssi = kickout_event->rssi;
	else
		del_sta_ctx->rssi = kickout_event->rssi +
					WMA_TGT_NOISE_FLOOR_DBM;
	wma_sta_kickout_event(del_sta_ctx->reasonCode, vdev_id, macaddr);
	wma_send_msg(wma, SIR_LIM_DELETE_STA_CONTEXT_IND, (void *)del_sta_ctx,
		     0);
	wma_lost_link_info_handler(wma, vdev_id, del_sta_ctx->rssi);

exit_handler:
	return 0;
}

int wma_unified_bcntx_status_event_handler(void *handle,
					   uint8_t *cmd_param_info,
					   uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_OFFLOAD_BCN_TX_STATUS_EVENTID_param_tlvs *param_buf;
	wmi_offload_bcn_tx_status_event_fixed_param *resp_event;
	tSirFirstBeaconTxCompleteInd *beacon_tx_complete_ind;

	param_buf =
		(WMI_OFFLOAD_BCN_TX_STATUS_EVENTID_param_tlvs *) cmd_param_info;
	if (!param_buf) {
		wma_err("Invalid bcn tx response event buffer");
		return -EINVAL;
	}

	resp_event = param_buf->fixed_param;

	if (resp_event->vdev_id >= wma->max_bssid) {
		wma_err("received invalid vdev_id %d", resp_event->vdev_id);
		return -EINVAL;
	}

	/* Check for valid handle to ensure session is not
	 * deleted in any race
	 */
	if (!wma->interfaces[resp_event->vdev_id].vdev) {
		wma_err("vdev is NULL for vdev_%d", resp_event->vdev_id);
		return -EINVAL;
	}

	/* Beacon Tx Indication supports only AP mode. Ignore in other modes */
	if (wma_is_vdev_in_ap_mode(wma, resp_event->vdev_id) == false) {
		wma_debug("Beacon Tx Indication does not support type %d and sub_type %d",
			 wma->interfaces[resp_event->vdev_id].type,
			 wma->interfaces[resp_event->vdev_id].sub_type);
		return 0;
	}

	beacon_tx_complete_ind = (tSirFirstBeaconTxCompleteInd *)
			qdf_mem_malloc(sizeof(tSirFirstBeaconTxCompleteInd));
	if (!beacon_tx_complete_ind) {
		wma_err("Failed to alloc beacon_tx_complete_ind");
		return -ENOMEM;
	}

	beacon_tx_complete_ind->messageType = WMA_DFS_BEACON_TX_SUCCESS_IND;
	beacon_tx_complete_ind->length = sizeof(tSirFirstBeaconTxCompleteInd);
	beacon_tx_complete_ind->bss_idx = resp_event->vdev_id;

	wma_send_msg(wma, WMA_DFS_BEACON_TX_SUCCESS_IND,
		     (void *)beacon_tx_complete_ind, 0);
	return 0;
}

/**
 * wma_get_go_probe_timeout() - get P2P GO probe timeout
 * @mac: UMAC handler
 * @max_inactive_time: return max inactive time
 * @max_unresponsive_time: return max unresponsive time
 *
 * Return: none
 */
#ifdef CONVERGED_P2P_ENABLE
static inline void
wma_get_go_probe_timeout(struct mac_context *mac,
			 uint32_t *max_inactive_time,
			 uint32_t *max_unresponsive_time)
{
	uint32_t keep_alive;
	QDF_STATUS status;

	status = cfg_p2p_get_go_link_monitor_period(mac->psoc,
						    max_inactive_time);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to go monitor period");
		*max_inactive_time = WMA_LINK_MONITOR_DEFAULT_TIME_SECS;
	}
	status = cfg_p2p_get_go_keepalive_period(mac->psoc,
						 &keep_alive);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to read go keep alive");
		keep_alive = WMA_KEEP_ALIVE_DEFAULT_TIME_SECS;
	}

	*max_unresponsive_time = *max_inactive_time + keep_alive;
}
#else
static inline void
wma_get_go_probe_timeout(struct mac_context *mac,
			 uint32_t *max_inactive_time,
			 uint32_t *max_unresponsive_time)
{
}
#endif

/**
 * wma_get_link_probe_timeout() - get link timeout based on sub type
 * @mac: UMAC handler
 * @sub_type: vdev syb type
 * @max_inactive_time: return max inactive time
 * @max_unresponsive_time: return max unresponsive time
 *
 * Return: none
 */
static inline void
wma_get_link_probe_timeout(struct mac_context *mac,
			   uint32_t sub_type,
			   uint32_t *max_inactive_time,
			   uint32_t *max_unresponsive_time)
{
	if (sub_type == WMI_UNIFIED_VDEV_SUBTYPE_P2P_GO) {
		wma_get_go_probe_timeout(mac, max_inactive_time,
					 max_unresponsive_time);
	} else {
		*max_inactive_time =
			mac->mlme_cfg->timeouts.ap_link_monitor_timeout;
		*max_unresponsive_time = *max_inactive_time +
			mac->mlme_cfg->timeouts.ap_keep_alive_timeout;
	}
}

/**
 * wma_verify_rate_code() - verify if rate code is valid.
 * @rate_code:     rate code
 * @band:     band information
 *
 * Return: verify result
 */
static bool wma_verify_rate_code(u_int32_t rate_code, enum cds_band_type band)
{
	uint8_t preamble, nss, rate;
	bool valid = true;

	preamble = (rate_code & 0xc0) >> 6;
	nss = (rate_code & 0x30) >> 4;
	rate = rate_code & 0xf;

	switch (preamble) {
	case WMI_RATE_PREAMBLE_CCK:
		if (nss != 0 || rate > 3 || band == CDS_BAND_5GHZ)
			valid = false;
		break;
	case WMI_RATE_PREAMBLE_OFDM:
		if (nss != 0 || rate > 7)
			valid = false;
		break;
	case WMI_RATE_PREAMBLE_HT:
		if (nss != 0 || rate > 7)
			valid = false;
		break;
	case WMI_RATE_PREAMBLE_VHT:
		if (nss != 0 || rate > 9)
			valid = false;
		break;
	default:
		break;
	}
	return valid;
}

#define TX_MGMT_RATE_2G_ENABLE_OFFSET 30
#define TX_MGMT_RATE_5G_ENABLE_OFFSET 31
#define TX_MGMT_RATE_2G_OFFSET 0
#define TX_MGMT_RATE_5G_OFFSET 12

/**
 * wma_set_mgmt_rate() - set vdev mgmt rate.
 * @wma:     wma handle
 * @vdev_id: vdev id
 *
 * Return: None
 */
void wma_set_vdev_mgmt_rate(tp_wma_handle wma, uint8_t vdev_id)
{
	uint32_t cfg_val;
	int ret;
	uint32_t per_band_mgmt_tx_rate = 0;
	enum cds_band_type band = 0;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac) {
		wma_err("Failed to get mac");
		return;
	}

	cfg_val = mac->mlme_cfg->sap_cfg.rate_tx_mgmt;
	band = CDS_BAND_ALL;
	if ((cfg_val == MLME_CFG_TX_MGMT_RATE_DEF) ||
	    !wma_verify_rate_code(cfg_val, band)) {
		wma_nofl_debug("default WNI_CFG_RATE_FOR_TX_MGMT, ignore");
	} else {
		ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
					 WMI_VDEV_PARAM_MGMT_TX_RATE,
					 cfg_val);
		if (ret)
			wma_err("Failed to set WMI_VDEV_PARAM_MGMT_TX_RATE");
	}

	cfg_val = mac->mlme_cfg->sap_cfg.rate_tx_mgmt_2g;
	band = CDS_BAND_2GHZ;
	if ((cfg_val == MLME_CFG_TX_MGMT_2G_RATE_DEF) ||
	    !wma_verify_rate_code(cfg_val, band)) {
		wma_nofl_debug("use default 2G MGMT rate.");
		per_band_mgmt_tx_rate &=
		    ~(1 << TX_MGMT_RATE_2G_ENABLE_OFFSET);
	} else {
		per_band_mgmt_tx_rate |=
		    (1 << TX_MGMT_RATE_2G_ENABLE_OFFSET);
		per_band_mgmt_tx_rate |=
		    ((cfg_val & 0x7FF) << TX_MGMT_RATE_2G_OFFSET);
	}

	cfg_val = mac->mlme_cfg->sap_cfg.rate_tx_mgmt;
	band = CDS_BAND_5GHZ;
	if ((cfg_val == MLME_CFG_TX_MGMT_5G_RATE_DEF) ||
	    !wma_verify_rate_code(cfg_val, band)) {
		wma_nofl_debug("use default 5G MGMT rate.");
		per_band_mgmt_tx_rate &=
		    ~(1 << TX_MGMT_RATE_5G_ENABLE_OFFSET);
	} else {
		per_band_mgmt_tx_rate |=
		    (1 << TX_MGMT_RATE_5G_ENABLE_OFFSET);
		per_band_mgmt_tx_rate |=
		    ((cfg_val & 0x7FF) << TX_MGMT_RATE_5G_OFFSET);
	}

	ret = wma_vdev_set_param(
		wma->wmi_handle,
		vdev_id,
		WMI_VDEV_PARAM_PER_BAND_MGMT_TX_RATE,
		per_band_mgmt_tx_rate);
	if (ret)
		wma_err("Failed to set WMI_VDEV_PARAM_PER_BAND_MGMT_TX_RATE");

}

/**
 * wma_set_sap_keepalive() - set SAP keep alive parameters to fw
 * @wma: wma handle
 * @vdev_id: vdev id
 *
 * Return: none
 */
void wma_set_sap_keepalive(tp_wma_handle wma, uint8_t vdev_id)
{
	uint32_t min_inactive_time, max_inactive_time, max_unresponsive_time;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	QDF_STATUS status;

	if (!mac) {
		wma_err("Failed to get mac");
		return;
	}

	wma_get_link_probe_timeout(mac, wma->interfaces[vdev_id].sub_type,
				   &max_inactive_time, &max_unresponsive_time);

	min_inactive_time = max_inactive_time / 2;

	status = wma_vdev_set_param(wma->wmi_handle, vdev_id,
			WMI_VDEV_PARAM_AP_KEEPALIVE_MIN_IDLE_INACTIVE_TIME_SECS,
			min_inactive_time);
	if (QDF_IS_STATUS_ERROR(status))
		wma_err("Failed to Set AP MIN IDLE INACTIVE TIME");

	status = wma_vdev_set_param(wma->wmi_handle, vdev_id,
			WMI_VDEV_PARAM_AP_KEEPALIVE_MAX_IDLE_INACTIVE_TIME_SECS,
			max_inactive_time);
	if (QDF_IS_STATUS_ERROR(status))
		wma_err("Failed to Set AP MAX IDLE INACTIVE TIME");

	status = wma_vdev_set_param(wma->wmi_handle, vdev_id,
			WMI_VDEV_PARAM_AP_KEEPALIVE_MAX_UNRESPONSIVE_TIME_SECS,
			max_unresponsive_time);
	if (QDF_IS_STATUS_ERROR(status))
		wma_err("Failed to Set MAX UNRESPONSIVE TIME");

	wma_debug("vdev_id:%d min_inactive_time: %u max_inactive_time: %u max_unresponsive_time: %u",
		  vdev_id, min_inactive_time, max_inactive_time,
		  max_unresponsive_time);
}

/**
 * wma_set_sta_sa_query_param() - set sta sa query parameters
 * @wma: wma handle
 * @vdev_id: vdev id

 * This function sets sta query related parameters in fw.
 *
 * Return: none
 */

void wma_set_sta_sa_query_param(tp_wma_handle wma,
				  uint8_t vdev_id)
{
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	uint8_t max_retries;
	uint16_t retry_interval;

	if (!mac) {
		wma_err("mac context is NULL");
		return;
	}

	max_retries = mac->mlme_cfg->gen.pmf_sa_query_max_retries;
	retry_interval = mac->mlme_cfg->gen.pmf_sa_query_retry_interval;

	wmi_unified_set_sta_sa_query_param_cmd(wma->wmi_handle,
						vdev_id,
						max_retries,
						retry_interval);
}

/**
 * wma_set_sta_keep_alive() - set sta keep alive parameters
 * @wma: wma handle
 * @vdev_id: vdev id
 * @method: method for keep alive
 * @timeperiod: time period
 * @hostv4addr: host ipv4 address
 * @destv4addr: dst ipv4 address
 * @destmac: destination mac
 *
 * This function sets keep alive related parameters in fw.
 *
 * Return: none
 */
void wma_set_sta_keep_alive(tp_wma_handle wma, uint8_t vdev_id,
			    uint32_t method, uint32_t timeperiod,
			    uint8_t *hostv4addr, uint8_t *destv4addr,
			    uint8_t *destmac)
{
	struct sta_keep_alive_params params = { 0 };

	if (!wma) {
		wma_err("wma handle is NULL");
		return;
	}

	if (timeperiod > cfg_max(CFG_INFRA_STA_KEEP_ALIVE_PERIOD)) {
		wmi_err("Invalid period %d Max limit %d", timeperiod,
			 cfg_max(CFG_INFRA_STA_KEEP_ALIVE_PERIOD));
		return;
	}

	params.vdev_id = vdev_id;
	params.method = method;
	params.timeperiod = timeperiod;
	if (hostv4addr)
		qdf_mem_copy(params.hostv4addr, hostv4addr, QDF_IPV4_ADDR_SIZE);
	if (destv4addr)
		qdf_mem_copy(params.destv4addr, destv4addr, QDF_IPV4_ADDR_SIZE);
	if (destmac)
		qdf_mem_copy(params.destmac, destmac, QDF_MAC_ADDR_SIZE);

	wmi_unified_set_sta_keep_alive_cmd(wma->wmi_handle, &params);
}

/**
 * wma_vdev_install_key_complete_event_handler() - install key complete handler
 * @handle: wma handle
 * @event: event data
 * @len: data length
 *
 * This event is sent by fw once WPA/WPA2 keys are installed in fw.
 *
 * Return: 0 for success or error code
 */
int wma_vdev_install_key_complete_event_handler(void *handle,
						uint8_t *event,
						uint32_t len)
{
	WMI_VDEV_INSTALL_KEY_COMPLETE_EVENTID_param_tlvs *param_buf = NULL;
	wmi_vdev_install_key_complete_event_fixed_param *key_fp = NULL;

	if (!event) {
		wma_err("event param null");
		return -EINVAL;
	}

	param_buf = (WMI_VDEV_INSTALL_KEY_COMPLETE_EVENTID_param_tlvs *) event;
	if (!param_buf) {
		wma_err("received null buf from target");
		return -EINVAL;
	}

	key_fp = param_buf->fixed_param;
	if (!key_fp) {
		wma_err("received null event data from target");
		return -EINVAL;
	}
	/*
	 * Do nothing for now. Completion of set key is already indicated to lim
	 */
	wma_debug("WMI_VDEV_INSTALL_KEY_COMPLETE_EVENTID");
	return 0;
}
/*
 * 802.11n D2.0 defined values for "Minimum MPDU Start Spacing":
 *   0 for no restriction
 *   1 for 1/4 us - Our lower layer calculations limit our precision to 1 msec
 *   2 for 1/2 us - Our lower layer calculations limit our precision to 1 msec
 *   3 for 1 us
 *   4 for 2 us
 *   5 for 4 us
 *   6 for 8 us
 *   7 for 16 us
 */
static const uint8_t wma_mpdu_spacing[] = { 0, 1, 1, 1, 2, 4, 8, 16 };

/**
 * wma_parse_mpdudensity() - give mpdu spacing from mpdu density
 * @mpdudensity: mpdu density
 *
 * Return: mpdu spacing or 0 for error
 */
static inline uint8_t wma_parse_mpdudensity(uint8_t mpdudensity)
{
	if (mpdudensity < sizeof(wma_mpdu_spacing))
		return wma_mpdu_spacing[mpdudensity];
	else
		return 0;
}

#define CFG_CTRL_MASK              0xFF00
#define CFG_DATA_MASK              0x00FF

/**
 * wma_mask_tx_ht_rate() - mask tx ht rate based on config
 * @wma:     wma handle
 * @mcs_set  mcs set buffer
 *
 * Return: None
 */
static void wma_mask_tx_ht_rate(tp_wma_handle wma, uint8_t *mcs_set)
{
	uint32_t i, j;
	uint16_t mcs_limit;
	uint8_t *rate_pos = mcs_set;
	struct mac_context *mac = wma->mac_context;

	/*
	 * Get MCS limit from ini configure, and map it to rate parameters
	 * This will limit HT rate upper bound. CFG_CTRL_MASK is used to
	 * check whether ini config is enabled and CFG_DATA_MASK to get the
	 * MCS value.
	 */
	mcs_limit = mac->mlme_cfg->rates.max_htmcs_txdata;

	if (mcs_limit & CFG_CTRL_MASK) {
		wma_debug("set mcs_limit %x", mcs_limit);

		mcs_limit &= CFG_DATA_MASK;
		for (i = 0, j = 0; i < MAX_SUPPORTED_RATES;) {
			if (j < mcs_limit / 8) {
				rate_pos[j] = 0xff;
				j++;
				i += 8;
			} else if (j < mcs_limit / 8 + 1) {
				if (i <= mcs_limit)
					rate_pos[i / 8] |= 1 << (i % 8);
				else
					rate_pos[i / 8] &= ~(1 << (i % 8));
				i++;

				if (i >= (j + 1) * 8)
					j++;
			} else {
				rate_pos[j++] = 0;
				i += 8;
			}
		}
	}
}

#if SUPPORT_11AX
/**
 * wma_fw_to_host_phymode_11ax() - convert fw to host phymode for 11ax phymodes
 * @phymode: phymode to convert
 *
 * Return: one of the 11ax values defined in enum wlan_phymode;
 *         or WLAN_PHYMODE_AUTO if the input is not an 11ax phymode
 */
static enum wlan_phymode
wma_fw_to_host_phymode_11ax(WMI_HOST_WLAN_PHY_MODE phymode)
{
	switch (phymode) {
	default:
		return WLAN_PHYMODE_AUTO;
	case WMI_HOST_MODE_11AX_HE20:
		return WLAN_PHYMODE_11AXA_HE20;
	case WMI_HOST_MODE_11AX_HE40:
		return WLAN_PHYMODE_11AXA_HE40;
	case WMI_HOST_MODE_11AX_HE80:
		return WLAN_PHYMODE_11AXA_HE80;
	case WMI_HOST_MODE_11AX_HE80_80:
		return WLAN_PHYMODE_11AXA_HE80_80;
	case WMI_HOST_MODE_11AX_HE160:
		return WLAN_PHYMODE_11AXA_HE160;
	case WMI_HOST_MODE_11AX_HE20_2G:
		return WLAN_PHYMODE_11AXG_HE20;
	case WMI_HOST_MODE_11AX_HE40_2G:
		return WLAN_PHYMODE_11AXG_HE40;
	case WMI_HOST_MODE_11AX_HE80_2G:
		return WLAN_PHYMODE_11AXG_HE80;
	}
	return WLAN_PHYMODE_AUTO;
}
#else
static enum wlan_phymode
wma_fw_to_host_phymode_11ax(WMI_HOST_WLAN_PHY_MODE phymode)
{
	return WLAN_PHYMODE_AUTO;
}
#endif

#ifdef CONFIG_160MHZ_SUPPORT
/**
 * wma_fw_to_host_phymode_160() - convert fw to host phymode for 160 mhz
 * phymodes
 * @phymode: phymode to convert
 *
 * Return: one of the 160 mhz values defined in enum wlan_phymode;
 *         or WLAN_PHYMODE_AUTO if the input is not a 160 mhz phymode
 */
static enum wlan_phymode
wma_fw_to_host_phymode_160(WMI_HOST_WLAN_PHY_MODE phymode)
{
	switch (phymode) {
	default:
		return WLAN_PHYMODE_AUTO;
	case WMI_HOST_MODE_11AC_VHT80_80:
		return WLAN_PHYMODE_11AC_VHT80_80;
	case WMI_HOST_MODE_11AC_VHT160:
		return WLAN_PHYMODE_11AC_VHT160;
	}
}
#else
static enum wlan_phymode
wma_fw_to_host_phymode_160(WMI_HOST_WLAN_PHY_MODE phymode)
{
	return WLAN_PHYMODE_AUTO;
}
#endif

enum wlan_phymode wma_fw_to_host_phymode(WMI_HOST_WLAN_PHY_MODE phymode)
{
	enum wlan_phymode host_phymode;
	switch (phymode) {
	default:
		host_phymode = wma_fw_to_host_phymode_160(phymode);
		if (host_phymode != WLAN_PHYMODE_AUTO)
			return host_phymode;
		return wma_fw_to_host_phymode_11ax(phymode);
	case WMI_HOST_MODE_11A:
		return WLAN_PHYMODE_11A;
	case WMI_HOST_MODE_11G:
		return WLAN_PHYMODE_11G;
	case WMI_HOST_MODE_11B:
		return WLAN_PHYMODE_11B;
	case WMI_HOST_MODE_11GONLY:
		return WLAN_PHYMODE_11G_ONLY;
	case WMI_HOST_MODE_11NA_HT20:
		return WLAN_PHYMODE_11NA_HT20;
	case WMI_HOST_MODE_11NG_HT20:
		return WLAN_PHYMODE_11NG_HT20;
	case WMI_HOST_MODE_11NA_HT40:
		return WLAN_PHYMODE_11NA_HT40;
	case WMI_HOST_MODE_11NG_HT40:
		return WLAN_PHYMODE_11NG_HT40;
	case WMI_HOST_MODE_11AC_VHT20:
		return WLAN_PHYMODE_11AC_VHT20;
	case WMI_HOST_MODE_11AC_VHT40:
		return WLAN_PHYMODE_11AC_VHT40;
	case WMI_HOST_MODE_11AC_VHT80:
		return WLAN_PHYMODE_11AC_VHT80;
	case WMI_HOST_MODE_11AC_VHT20_2G:
		return WLAN_PHYMODE_11AC_VHT20_2G;
	case WMI_HOST_MODE_11AC_VHT40_2G:
		return WLAN_PHYMODE_11AC_VHT40_2G;
	case WMI_HOST_MODE_11AC_VHT80_2G:
		return WLAN_PHYMODE_11AC_VHT80_2G;
	}
}

#ifdef CONFIG_160MHZ_SUPPORT
/**
 * wma_host_to_fw_phymode_160() - convert host to fw phymode for 160 mhz
 * @host_phymode: phymode to convert
 *
 * Return: one of the 160 mhz values defined in enum WMI_HOST_WLAN_PHY_MODE;
 *         or WMI_HOST_MODE_UNKNOWN if the input is not a 160 mhz phymode
 */
static WMI_HOST_WLAN_PHY_MODE
wma_host_to_fw_phymode_160(enum wlan_phymode host_phymode)
{
	switch (host_phymode) {
	case WLAN_PHYMODE_11AC_VHT80_80:
		return WMI_HOST_MODE_11AC_VHT80_80;
	case WLAN_PHYMODE_11AC_VHT160:
		return WMI_HOST_MODE_11AC_VHT160;
	default:
		return WMI_HOST_MODE_UNKNOWN;
	}
}
#else
static WMI_HOST_WLAN_PHY_MODE
wma_host_to_fw_phymode_160(enum wlan_phymode host_phymode)
{
	return WMI_HOST_MODE_UNKNOWN;
}
#endif

#if SUPPORT_11AX
/**
 * wma_host_to_fw_phymode_11ax() - convert host to fw phymode for 11ax phymode
 * @host_phymode: phymode to convert
 *
 * Return: one of the 11ax values defined in enum WMI_HOST_WLAN_PHY_MODE;
 *         or WMI_HOST_MODE_UNKNOWN if the input is not an 11ax phymode
 */
static WMI_HOST_WLAN_PHY_MODE
wma_host_to_fw_phymode_11ax(enum wlan_phymode host_phymode)
{
	switch (host_phymode) {
	case WLAN_PHYMODE_11AXA_HE20:
		return WMI_HOST_MODE_11AX_HE20;
	case WLAN_PHYMODE_11AXA_HE40:
		return WMI_HOST_MODE_11AX_HE40;
	case WLAN_PHYMODE_11AXA_HE80:
		return WMI_HOST_MODE_11AX_HE80;
	case WLAN_PHYMODE_11AXA_HE80_80:
		return WMI_HOST_MODE_11AX_HE80_80;
	case WLAN_PHYMODE_11AXA_HE160:
		return WMI_HOST_MODE_11AX_HE160;
	case WLAN_PHYMODE_11AXG_HE20:
		return WMI_HOST_MODE_11AX_HE20_2G;
	case WLAN_PHYMODE_11AXG_HE40:
	case WLAN_PHYMODE_11AXG_HE40PLUS:
	case WLAN_PHYMODE_11AXG_HE40MINUS:
		return WMI_HOST_MODE_11AX_HE40_2G;
	case WLAN_PHYMODE_11AXG_HE80:
		return WMI_HOST_MODE_11AX_HE80_2G;
	default:
		return WMI_HOST_MODE_UNKNOWN;
	}
}
#else
static WMI_HOST_WLAN_PHY_MODE
wma_host_to_fw_phymode_11ax(enum wlan_phymode host_phymode)
{
	return WMI_HOST_MODE_UNKNOWN;
}
#endif

WMI_HOST_WLAN_PHY_MODE wma_host_to_fw_phymode(enum wlan_phymode host_phymode)
{
	WMI_HOST_WLAN_PHY_MODE fw_phymode;

	switch (host_phymode) {
	case WLAN_PHYMODE_11A:
		return WMI_HOST_MODE_11A;
	case WLAN_PHYMODE_11G:
		return WMI_HOST_MODE_11G;
	case WLAN_PHYMODE_11B:
		return WMI_HOST_MODE_11B;
	case WLAN_PHYMODE_11G_ONLY:
		return WMI_HOST_MODE_11GONLY;
	case WLAN_PHYMODE_11NA_HT20:
		return WMI_HOST_MODE_11NA_HT20;
	case WLAN_PHYMODE_11NG_HT20:
		return WMI_HOST_MODE_11NG_HT20;
	case WLAN_PHYMODE_11NA_HT40:
		return WMI_HOST_MODE_11NA_HT40;
	case WLAN_PHYMODE_11NG_HT40:
	case WLAN_PHYMODE_11NG_HT40PLUS:
	case WLAN_PHYMODE_11NG_HT40MINUS:
		return WMI_HOST_MODE_11NG_HT40;
	case WLAN_PHYMODE_11AC_VHT20:
		return WMI_HOST_MODE_11AC_VHT20;
	case WLAN_PHYMODE_11AC_VHT40:
		return WMI_HOST_MODE_11AC_VHT40;
	case WLAN_PHYMODE_11AC_VHT80:
		return WMI_HOST_MODE_11AC_VHT80;
	case WLAN_PHYMODE_11AC_VHT20_2G:
		return WMI_HOST_MODE_11AC_VHT20_2G;
	case WLAN_PHYMODE_11AC_VHT40PLUS_2G:
	case WLAN_PHYMODE_11AC_VHT40MINUS_2G:
	case WLAN_PHYMODE_11AC_VHT40_2G:
		return WMI_HOST_MODE_11AC_VHT40_2G;
	case WLAN_PHYMODE_11AC_VHT80_2G:
		return WMI_HOST_MODE_11AC_VHT80_2G;
	default:
		fw_phymode = wma_host_to_fw_phymode_160(host_phymode);
		if (fw_phymode != WMI_HOST_MODE_UNKNOWN)
			return fw_phymode;
		return wma_host_to_fw_phymode_11ax(host_phymode);
	}
}

void wma_objmgr_set_peer_mlme_phymode(tp_wma_handle wma, uint8_t *mac_addr,
				      enum wlan_phymode phymode)
{
	uint8_t pdev_id;
	struct wlan_objmgr_peer *peer;
	struct wlan_objmgr_psoc *psoc = wma->psoc;

	pdev_id = wlan_objmgr_pdev_get_pdev_id(wma->pdev);
	peer = wlan_objmgr_get_peer(psoc, pdev_id, mac_addr,
				    WLAN_LEGACY_WMA_ID);
	if (!peer)
		return;

	wlan_peer_obj_lock(peer);
	wlan_peer_set_phymode(peer, phymode);
	wlan_peer_obj_unlock(peer);
	wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_WMA_ID);
}

/**
 * wma_objmgr_set_peer_mlme_type() - set peer type to peer object
 * @wma:      wma handle
 * @mac_addr: mac addr of peer
 * @peer_type:  peer type value to set
 *
 * Return: None
 */
static void wma_objmgr_set_peer_mlme_type(tp_wma_handle wma,
					  uint8_t *mac_addr,
					  enum wlan_peer_type peer_type)
{
	uint8_t pdev_id;
	struct wlan_objmgr_peer *peer;
	struct wlan_objmgr_psoc *psoc = wma->psoc;

	pdev_id = wlan_objmgr_pdev_get_pdev_id(wma->pdev);
	peer = wlan_objmgr_get_peer(psoc, pdev_id, mac_addr,
				    WLAN_LEGACY_WMA_ID);
	if (!peer)
		return;

	wlan_peer_obj_lock(peer);
	wlan_peer_set_peer_type(peer, peer_type);
	wlan_peer_obj_unlock(peer);
	wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_WMA_ID);
}

/**
 * wmi_unified_send_peer_assoc() - send peer assoc command to fw
 * @wma: wma handle
 * @nw_type: nw type
 * @params: add sta params
 *
 * This function send peer assoc command to firmware with
 * different parameters.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wma_send_peer_assoc(tp_wma_handle wma,
				    tSirNwType nw_type,
				    tpAddStaParams params)
{
	struct peer_assoc_params *cmd;
	int32_t ret, max_rates, i;
	uint8_t *rate_pos;
	wmi_rate_set peer_legacy_rates, peer_ht_rates;
	uint32_t num_peer_11b_rates = 0;
	uint32_t num_peer_11a_rates = 0;
	enum wlan_phymode phymode, vdev_phymode;
	uint32_t peer_nss = 1;
	struct wma_txrx_node *intr = NULL;
	bool is_he;
	QDF_STATUS status;
	struct mac_context *mac = wma->mac_context;
	struct wlan_channel *des_chan;
	int32_t keymgmt, uccipher, authmode;

	cmd = qdf_mem_malloc(sizeof(struct peer_assoc_params));
	if (!cmd) {
		wma_err("Failed to allocate peer_assoc_params param");
		return QDF_STATUS_E_NOMEM;
	}

	intr = &wma->interfaces[params->smesessionId];

	wma_mask_tx_ht_rate(wma, params->supportedRates.supportedMCSSet);

	qdf_mem_zero(&peer_legacy_rates, sizeof(wmi_rate_set));
	qdf_mem_zero(&peer_ht_rates, sizeof(wmi_rate_set));
	qdf_mem_zero(cmd, sizeof(struct peer_assoc_params));

	is_he = wma_is_peer_he_capable(params);
	if ((params->ch_width > CH_WIDTH_40MHZ) &&
	    ((nw_type == eSIR_11G_NW_TYPE) ||
	     (nw_type == eSIR_11B_NW_TYPE))) {
		wma_err("ch_width %d sent in 11G, configure to 40MHz",
			params->ch_width);
		params->ch_width = CH_WIDTH_40MHZ;
	}
	phymode = wma_peer_phymode(nw_type, params->staType,
				   params->htCapable, params->ch_width,
				   params->vhtCapable, is_he);

	des_chan = wlan_vdev_mlme_get_des_chan(intr->vdev);
	vdev_phymode = des_chan->ch_phymode;
	if ((intr->type == WMI_VDEV_TYPE_AP) && (phymode > vdev_phymode)) {
		wma_nofl_debug("Peer phymode %d is not allowed. Set it equal to sap/go phymode %d",
			       phymode, vdev_phymode);
		phymode = vdev_phymode;
	}

	if (!mac->mlme_cfg->rates.disable_abg_rate_txdata) {
		/* Legacy Rateset */
		rate_pos = (uint8_t *) peer_legacy_rates.rates;
		for (i = 0; i < SIR_NUM_11B_RATES; i++) {
			if (!params->supportedRates.llbRates[i])
				continue;
			rate_pos[peer_legacy_rates.num_rates++] =
				params->supportedRates.llbRates[i];
			num_peer_11b_rates++;
		}
		for (i = 0; i < SIR_NUM_11A_RATES; i++) {
			if (!params->supportedRates.llaRates[i])
				continue;
			rate_pos[peer_legacy_rates.num_rates++] =
				params->supportedRates.llaRates[i];
			num_peer_11a_rates++;
		}
	}

	if ((phymode == WLAN_PHYMODE_11A && num_peer_11a_rates == 0) ||
	    (phymode == WLAN_PHYMODE_11B && num_peer_11b_rates == 0)) {
		wma_warn("Invalid phy rates. phymode 0x%x, 11b_rates %d, 11a_rates %d",
			phymode, num_peer_11b_rates,
			num_peer_11a_rates);
		qdf_mem_free(cmd);
		return QDF_STATUS_E_INVAL;
	}

	/* HT Rateset */
	max_rates = sizeof(peer_ht_rates.rates) /
		    sizeof(peer_ht_rates.rates[0]);
	rate_pos = (uint8_t *) peer_ht_rates.rates;
	for (i = 0; i < MAX_SUPPORTED_RATES; i++) {
		if (params->supportedRates.supportedMCSSet[i / 8] &
		    (1 << (i % 8))) {
			rate_pos[peer_ht_rates.num_rates++] = i;
			if (i >= 8) {
				/* MCS8 or higher rate is present, must be 2x2 */
				peer_nss = 2;
			}
		}
		if (peer_ht_rates.num_rates == max_rates)
			break;
	}

	if (params->htCapable && !peer_ht_rates.num_rates) {
		uint8_t temp_ni_rates[8] = { 0x0, 0x1, 0x2, 0x3,
					     0x4, 0x5, 0x6, 0x7};
		/*
		 * Workaround for EV 116382: The peer is marked HT but with
		 * supported rx mcs set is set to 0. 11n spec mandates MCS0-7
		 * for a HT STA. So forcing the supported rx mcs rate to
		 * MCS 0-7. This workaround will be removed once we get
		 * clarification from WFA regarding this STA behavior.
		 */

		/* TODO: Do we really need this? */
		wma_warn("Peer is marked as HT capable but supported mcs rate is 0");
		peer_ht_rates.num_rates = sizeof(temp_ni_rates);
		qdf_mem_copy((uint8_t *) peer_ht_rates.rates, temp_ni_rates,
			     peer_ht_rates.num_rates);
	}

	/* in ap mode and for tdls peer, use mac address of the peer in
	 * the other end as the new peer address; in sta mode, use bss id to
	 * be the new peer address
	 */
	if ((wma_is_vdev_in_ap_mode(wma, params->smesessionId))
#ifdef FEATURE_WLAN_TDLS
	    || (STA_ENTRY_TDLS_PEER == params->staType)
#endif /* FEATURE_WLAN_TDLS */
	    ) {
		qdf_mem_copy(cmd->peer_mac, params->staMac,
						sizeof(cmd->peer_mac));
	} else {
		qdf_mem_copy(cmd->peer_mac, params->bssId,
						sizeof(cmd->peer_mac));
	}
	wma_objmgr_set_peer_mlme_phymode(wma, cmd->peer_mac, phymode);

	cmd->vdev_id = params->smesessionId;
	cmd->peer_new_assoc = 1;
	cmd->peer_associd = params->assocId;

	cmd->is_wme_set = 1;

	if (params->wmmEnabled)
		cmd->qos_flag = 1;

	if (params->uAPSD) {
		cmd->apsd_flag = 1;
		wma_nofl_debug("Set WMI_PEER_APSD: uapsd Mask %d",
			       params->uAPSD);
	}

	if (params->htCapable) {
		cmd->ht_flag = 1;
		cmd->qos_flag = 1;
		cmd->peer_rate_caps |= WMI_RC_HT_FLAG;
	}

	if (params->vhtCapable) {
		cmd->ht_flag = 1;
		cmd->qos_flag = 1;
		cmd->vht_flag = 1;
		cmd->peer_rate_caps |= WMI_RC_HT_FLAG;
	}

	if (params->ch_width) {
		cmd->bw_40 = 1;
		cmd->peer_rate_caps |= WMI_RC_CW40_FLAG;
		if (params->fShortGI40Mhz)
			cmd->peer_rate_caps |= WMI_RC_SGI_FLAG;
	} else if (params->fShortGI20Mhz) {
		cmd->peer_rate_caps |= WMI_RC_SGI_FLAG;
	}

	if (params->ch_width == CH_WIDTH_80MHZ)
		cmd->bw_80 = 1;
	else if (params->ch_width == CH_WIDTH_160MHZ)
		cmd->bw_160 = 1;
	else if (params->ch_width == CH_WIDTH_80P80MHZ)
		cmd->bw_160 = 1;

	cmd->peer_vht_caps = params->vht_caps;
	if (params->p2pCapableSta) {
		cmd->p2p_capable_sta = 1;
		wma_objmgr_set_peer_mlme_type(wma, params->staMac,
					      WLAN_PEER_P2P_CLI);
	}

	if (params->rmfEnabled)
		cmd->is_pmf_enabled = 1;

	if (params->stbc_capable)
		cmd->stbc_flag = 1;

	if (params->htLdpcCapable || params->vhtLdpcCapable)
		cmd->ldpc_flag = 1;

	switch (params->mimoPS) {
	case eSIR_HT_MIMO_PS_STATIC:
		cmd->static_mimops_flag = 1;
		break;
	case eSIR_HT_MIMO_PS_DYNAMIC:
		cmd->dynamic_mimops_flag = 1;
		break;
	case eSIR_HT_MIMO_PS_NO_LIMIT:
		cmd->spatial_mux_flag = 1;
		break;
	default:
		break;
	}

	wma_set_twt_peer_caps(params, cmd);
#ifdef FEATURE_WLAN_TDLS
	if (STA_ENTRY_TDLS_PEER == params->staType)
		cmd->auth_flag = 1;
#endif /* FEATURE_WLAN_TDLS */

	if (params->wpa_rsn
#ifdef FEATURE_WLAN_WAPI
	    || params->encryptType == eSIR_ED_WPI
#endif /* FEATURE_WLAN_WAPI */
	    ) {
		if (!params->no_ptk_4_way)
			cmd->need_ptk_4_way = 1;
		wma_nofl_debug("Acquire set key wake lock for %d ms",
			       WMA_VDEV_SET_KEY_WAKELOCK_TIMEOUT);
		wma_acquire_wakelock(&intr->vdev_set_key_wakelock,
			WMA_VDEV_SET_KEY_WAKELOCK_TIMEOUT);
		qdf_runtime_pm_prevent_suspend(
			&intr->vdev_set_key_runtime_wakelock);
	}
	if (params->wpa_rsn >> 1)
		cmd->need_gtk_2_way = 1;

#ifdef FEATURE_WLAN_WAPI
	if (params->encryptType == eSIR_ED_WPI) {
		ret = wma_vdev_set_param(wma->wmi_handle, params->smesessionId,
				      WMI_VDEV_PARAM_DROP_UNENCRY, false);
		if (ret) {
			wma_err("Set WMI_VDEV_PARAM_DROP_UNENCRY Param status:%d",
				ret);
			qdf_mem_free(cmd);
			return ret;
		}
	}
#endif /* FEATURE_WLAN_WAPI */

	cmd->peer_caps = params->capab_info;
	cmd->peer_listen_intval = params->listenInterval;
	cmd->peer_ht_caps = params->ht_caps;
	cmd->peer_max_mpdu = (1 << (IEEE80211_HTCAP_MAXRXAMPDU_FACTOR +
				    params->maxAmpduSize)) - 1;
	cmd->peer_mpdu_density = wma_parse_mpdudensity(params->maxAmpduDensity);

	if (params->supportedRates.supportedMCSSet[1] &&
	    params->supportedRates.supportedMCSSet[2])
		cmd->peer_rate_caps |= WMI_RC_TS_FLAG;
	else if (params->supportedRates.supportedMCSSet[1])
		cmd->peer_rate_caps |= WMI_RC_DS_FLAG;

	/* Update peer legacy rate information */
	cmd->peer_legacy_rates.num_rates = peer_legacy_rates.num_rates;
	qdf_mem_copy(cmd->peer_legacy_rates.rates, peer_legacy_rates.rates,
		     peer_legacy_rates.num_rates);

	/* Update peer HT rate information */
	cmd->peer_ht_rates.num_rates = peer_ht_rates.num_rates;
	qdf_mem_copy(cmd->peer_ht_rates.rates, peer_ht_rates.rates,
				 peer_ht_rates.num_rates);

	/* VHT Rates */

	cmd->peer_nss = peer_nss;
	/*
	 * Because of DBS a vdev may come up in any of the two MACs with
	 * different capabilities. STBC capab should be fetched for given
	 * hard_mode->MAC_id combo. It is planned that firmware should provide
	 * these dev capabilities. But for now number of tx streams can be used
	 * to identify if Tx STBC needs to be disabled.
	 */
	if (intr->tx_streams < 2) {
		cmd->peer_vht_caps &= ~(1 << SIR_MAC_VHT_CAP_TXSTBC);
		wma_nofl_debug("Num tx_streams: %d, Disabled txSTBC",
			       intr->tx_streams);
	}

	cmd->vht_capable = params->vhtCapable;
	if (params->vhtCapable) {
#define VHT2x2MCSMASK 0xc
		cmd->rx_max_rate = params->supportedRates.vhtRxHighestDataRate;
		cmd->rx_mcs_set = params->supportedRates.vhtRxMCSMap;
		cmd->tx_max_rate = params->supportedRates.vhtTxHighestDataRate;
		cmd->tx_mcs_set = params->supportedRates.vhtTxMCSMap;
		/*
		 *  tx_mcs_set is intersection of self tx NSS and peer rx mcs map
		 */
		if (params->vhtSupportedRxNss)
			cmd->peer_nss = params->vhtSupportedRxNss;
		else
			cmd->peer_nss = ((cmd->tx_mcs_set & VHT2x2MCSMASK)
					== VHT2x2MCSMASK) ? 1 : 2;

		if (params->vht_mcs_10_11_supp) {
			WMI_SET_BITS(cmd->tx_mcs_set, 16, cmd->peer_nss,
				     ((1 << cmd->peer_nss) - 1));
			WMI_VHT_MCS_NOTIFY_EXT_SS_SET(cmd->tx_mcs_set, 1);
		}
		if (params->vht_extended_nss_bw_cap &&
		    (params->vht_160mhz_nss || params->vht_80p80mhz_nss)) {
			/*
			 * bit[2:0] : Represents value of Rx NSS for 160 MHz
			 * bit[5:3] : Represents value of Rx NSS for 80_80 MHz
			 *             Extended NSS support
			 * bit[30:6]: Reserved
			 * bit[31]  : MSB(0/1): 1 in case of valid data
			 */
			cmd->peer_bw_rxnss_override |= (1 << 31);
			if (params->vht_160mhz_nss)
				cmd->peer_bw_rxnss_override |=
					(params->vht_160mhz_nss - 1);
			if (params->vht_80p80mhz_nss)
				cmd->peer_bw_rxnss_override |=
					((params->vht_80p80mhz_nss - 1) << 3);
			wma_debug("peer_bw_rxnss_override %0X",
				  cmd->peer_bw_rxnss_override);
		}
	}

	wma_debug("rx_max_rate %d, rx_mcs %x, tx_max_rate %d, tx_mcs: %x num rates %d need 4 way %d",
		  cmd->rx_max_rate, cmd->rx_mcs_set, cmd->tx_max_rate,
		  cmd->tx_mcs_set, peer_ht_rates.num_rates,
		  cmd->need_ptk_4_way);

	/*
	 * Limit nss to max number of rf chain supported by target
	 * Otherwise Fw will crash
	 */
	if (cmd->peer_nss > WMA_MAX_NSS) {
		wma_err("peer Nss %d is more than supported", cmd->peer_nss);
		cmd->peer_nss = WMA_MAX_NSS;
	}

	wma_populate_peer_he_cap(cmd, params);
	if (!wma_is_vdev_in_ap_mode(wma, params->smesessionId))
		intr->nss = cmd->peer_nss;

	/* Till conversion is not done in WMI we need to fill fw phy mode */
	cmd->peer_phymode = wma_host_to_fw_phymode(phymode);

	keymgmt = wlan_crypto_get_param(intr->vdev, WLAN_CRYPTO_PARAM_KEY_MGMT);
	authmode = wlan_crypto_get_param(intr->vdev,
					 WLAN_CRYPTO_PARAM_AUTH_MODE);
	uccipher = wlan_crypto_get_param(intr->vdev,
					 WLAN_CRYPTO_PARAM_UCAST_CIPHER);

	cmd->akm = cm_crypto_authmode_to_wmi_authmode(authmode,
						      keymgmt,
						      uccipher);

	status = wmi_unified_peer_assoc_send(wma->wmi_handle,
					 cmd);
	if (QDF_IS_STATUS_ERROR(status))
		wma_alert("Failed to send peer assoc command status = %d",
			 status);
	qdf_mem_free(cmd);

	return status;
}

/**
 * wmi_unified_vdev_set_gtx_cfg_send() - set GTX params
 * @wmi_handle: wmi handle
 * @if_id: vdev id
 * @gtx_info: GTX config params
 *
 * This function set GTX related params in firmware.
 *
 * Return: 0 for success or error code
 */
QDF_STATUS wmi_unified_vdev_set_gtx_cfg_send(wmi_unified_t wmi_handle,
				  uint32_t if_id,
				  gtx_config_t *gtx_info)
{
	struct wmi_gtx_config params;

	params.gtx_rt_mask[0] = gtx_info->gtxRTMask[0];
	params.gtx_rt_mask[1] = gtx_info->gtxRTMask[1];
	params.gtx_usrcfg = gtx_info->gtxUsrcfg;
	params.gtx_threshold = gtx_info->gtxPERThreshold;
	params.gtx_margin = gtx_info->gtxPERMargin;
	params.gtx_tpcstep = gtx_info->gtxTPCstep;
	params.gtx_tpcmin = gtx_info->gtxTPCMin;
	params.gtx_bwmask = gtx_info->gtxBWMask;

	return wmi_unified_vdev_set_gtx_cfg_cmd(wmi_handle,
						if_id, &params);

}

/**
 * wma_update_protection_mode() - update protection mode
 * @wma: wma handle
 * @vdev_id: vdev id
 * @llbcoexist: protection mode info
 *
 * This function set protection mode(RTS/CTS) to fw for passed vdev id.
 *
 * Return: none
 */
void wma_update_protection_mode(tp_wma_handle wma, uint8_t vdev_id,
			   uint8_t llbcoexist)
{
	QDF_STATUS ret;
	enum ieee80211_protmode prot_mode;

	prot_mode = llbcoexist ? IEEE80211_PROT_CTSONLY : IEEE80211_PROT_NONE;

	ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
					      WMI_VDEV_PARAM_PROTECTION_MODE,
					      prot_mode);

	if (QDF_IS_STATUS_ERROR(ret))
		wma_err("Failed to send wmi protection mode cmd");
	else
		wma_nofl_debug("Updated protection mode %d to target",
			       prot_mode);
}

void
wma_update_beacon_interval(tp_wma_handle wma, uint8_t vdev_id,
			   uint16_t beaconInterval)
{
	QDF_STATUS ret;

	ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
					      WMI_VDEV_PARAM_BEACON_INTERVAL,
					      beaconInterval);

	if (QDF_IS_STATUS_ERROR(ret))
		wma_err("Failed to update beacon interval");
	else
		wma_info("Updated beacon interval %d for vdev %d",
			 beaconInterval, vdev_id);
}

#ifdef WLAN_FEATURE_11AX_BSS_COLOR
/**
 * wma_update_bss_color() - update beacon bss color in fw
 * @wma: wma handle
 * @vdev_id: vdev id
 * @he_ops: HE operation, only the bss_color and bss_color_disabled fields
 * are updated.
 *
 * Return: none
 */
static void
wma_update_bss_color(tp_wma_handle wma, uint8_t vdev_id,
		     tUpdateBeaconParams *bcn_params)
{
	QDF_STATUS ret;
	uint32_t dword_he_ops = 0;

	WMI_HEOPS_COLOR_SET(dword_he_ops, bcn_params->bss_color);
	WMI_HEOPS_BSSCOLORDISABLE_SET(dword_he_ops,
				bcn_params->bss_color_disabled);
	wma_nofl_debug("vdev: %d, update bss color, HE_OPS: 0x%x",
		       vdev_id, dword_he_ops);
	ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
			      WMI_VDEV_PARAM_BSS_COLOR, dword_he_ops);
	if (QDF_IS_STATUS_ERROR(ret))
		wma_err("Failed to update HE operations");
}
#else
static void wma_update_bss_color(tp_wma_handle wma, uint8_t vdev_id,
			   tUpdateBeaconParams *bcn_params)
{
}
#endif

/**
 * wma_process_update_beacon_params() - update beacon parameters to target
 * @wma: wma handle
 * @bcn_params: beacon parameters
 *
 * Return: none
 */
void
wma_process_update_beacon_params(tp_wma_handle wma,
				 tUpdateBeaconParams *bcn_params)
{
	if (!bcn_params) {
		wma_err("bcn_params NULL");
		return;
	}

	if (bcn_params->vdev_id >= wma->max_bssid) {
		wma_err("Invalid vdev id %d", bcn_params->vdev_id);
		return;
	}

	if (bcn_params->paramChangeBitmap & PARAM_BCN_INTERVAL_CHANGED) {
		wma_update_beacon_interval(wma, bcn_params->vdev_id,
					   bcn_params->beaconInterval);
	}

	if (bcn_params->paramChangeBitmap & PARAM_llBCOEXIST_CHANGED)
		wma_update_protection_mode(wma, bcn_params->vdev_id,
					   bcn_params->llbCoexist);

	if (bcn_params->paramChangeBitmap & PARAM_BSS_COLOR_CHANGED)
		wma_update_bss_color(wma, bcn_params->vdev_id,
				     bcn_params);
}

void wma_update_rts_params(tp_wma_handle wma, uint32_t value)
{
	uint8_t vdev_id;
	QDF_STATUS ret;
	struct wlan_objmgr_vdev *vdev;

	for (vdev_id = 0; vdev_id < wma->max_bssid; vdev_id++) {
		vdev = wma->interfaces[vdev_id].vdev;
		if (!vdev)
			continue;
		ret = wma_vdev_set_param(wma->wmi_handle,
					 vdev_id,
					 WMI_VDEV_PARAM_RTS_THRESHOLD,
					 value);
		if (QDF_IS_STATUS_ERROR(ret))
			wma_err("Update cfg param fail for vdevId %d", vdev_id);
	}
}

void wma_update_frag_params(tp_wma_handle wma, uint32_t value)
{
	uint8_t vdev_id;
	QDF_STATUS ret;
	struct wlan_objmgr_vdev *vdev;

	for (vdev_id = 0; vdev_id < wma->max_bssid; vdev_id++) {
		vdev = wma->interfaces[vdev_id].vdev;
		if (!vdev)
			continue;
		ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
					 WMI_VDEV_PARAM_FRAGMENTATION_THRESHOLD,
					 value);
		if (QDF_IS_STATUS_ERROR(ret))
			wma_err("Update cfg params failed for vdevId %d",
				 vdev_id);
	}
}

#ifdef FEATURE_WLAN_WAPI
#define WPI_IV_LEN 16
#if defined(QCA_WIFI_QCA6290) || defined(QCA_WIFI_QCA6390) || \
    defined(QCA_WIFI_QCA6490) || defined(QCA_WIFI_QCA6750)
/**
 * wma_fill_in_wapi_key_params() - update key parameters about wapi
 * @key_params: wma key parameters
 * @params: parameters pointer to be set
 * @mode: operation mode
 *
 * Return: None
 */
static inline void wma_fill_in_wapi_key_params(
		struct wma_set_key_params *key_params,
		struct set_key_params *params, uint8_t mode)
{
	/*
	 * Since MCL shares same FW with WIN for Napier/Hasting, FW WAPI logic
	 * is fit for WIN, change it to align with WIN.
	 */
	unsigned char iv_init_ap[16] = { 0x5c, 0x36, 0x5c, 0x36, 0x5c, 0x36,
					 0x5c, 0x36, 0x5c, 0x36, 0x5c, 0x36,
					 0x5c, 0x36, 0x5c, 0x37};
	unsigned char iv_init_sta[16] = { 0x5c, 0x36, 0x5c, 0x36, 0x5c, 0x36,
					  0x5c, 0x36, 0x5c, 0x36, 0x5c, 0x36,
					  0x5c, 0x36, 0x5c, 0x36};

	if (mode == wlan_op_mode_ap) {
		qdf_mem_copy(params->rx_iv, iv_init_sta,
			     WPI_IV_LEN);
		qdf_mem_copy(params->tx_iv, iv_init_ap,
			     WPI_IV_LEN);
	} else {
		qdf_mem_copy(params->rx_iv, iv_init_ap,
			     WPI_IV_LEN);
		qdf_mem_copy(params->tx_iv, iv_init_sta,
			     WPI_IV_LEN);
	}

	params->key_txmic_len = WMA_TXMIC_LEN;
	params->key_rxmic_len = WMA_RXMIC_LEN;

	params->key_cipher = WMI_CIPHER_WAPI;
}
#else
static inline void wma_fill_in_wapi_key_params(
		struct wma_set_key_params *key_params,
		struct set_key_params *params, uint8_t mode)
{
	/*initialize receive and transmit IV with default values */
	/* **Note: tx_iv must be sent in reverse** */
	unsigned char tx_iv[16] = { 0x36, 0x5c, 0x36, 0x5c, 0x36, 0x5c,
				    0x36, 0x5c, 0x36, 0x5c, 0x36, 0x5c,
				    0x36, 0x5c, 0x36, 0x5c};
	unsigned char rx_iv[16] = { 0x5c, 0x36, 0x5c, 0x36, 0x5c, 0x36,
				    0x5c, 0x36, 0x5c, 0x36, 0x5c, 0x36,
				    0x5c, 0x36, 0x5c, 0x37};
	if (mode == wlan_op_mode_ap) {
		/* Authenticator initializes the value of PN as
		 * 0x5C365C365C365C365C365C365C365C36 for MCastkeyUpdate
		 */
		if (key_params->unicast)
			tx_iv[0] = 0x37;

		rx_iv[WPI_IV_LEN - 1] = 0x36;
	} else {
		if (!key_params->unicast)
			rx_iv[WPI_IV_LEN - 1] = 0x36;
	}

	params->key_txmic_len = WMA_TXMIC_LEN;
	params->key_rxmic_len = WMA_RXMIC_LEN;

	qdf_mem_copy(params->rx_iv, &rx_iv,
		     WPI_IV_LEN);
	qdf_mem_copy(params->tx_iv, &tx_iv,
		     WPI_IV_LEN);
	params->key_cipher = WMI_CIPHER_WAPI;
}
#endif
#endif

/**
 * wma_process_update_edca_param_req() - update EDCA params
 * @handle: wma handle
 * @edca_params: edca parameters
 *
 * This function updates EDCA parameters to the target
 *
 * Return: QDF Status
 */
QDF_STATUS wma_process_update_edca_param_req(WMA_HANDLE handle,
					     tEdcaParams *edca_params)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	struct wmi_host_wme_vparams wmm_param[QCA_WLAN_AC_ALL];
	tSirMacEdcaParamRecord *edca_record;
	int ac;
	struct ol_tx_wmm_param_t ol_tx_wmm_param;
	uint8_t vdev_id;
	QDF_STATUS status;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	vdev_id = edca_params->vdev_id;
	if (!wma_is_vdev_valid(vdev_id)) {
		wma_err("vdev id:%d is not active ", vdev_id);
		goto fail;
	}

	for (ac = 0; ac < QCA_WLAN_AC_ALL; ac++) {
		switch (ac) {
		case QCA_WLAN_AC_BE:
			edca_record = &edca_params->acbe;
			break;
		case QCA_WLAN_AC_BK:
			edca_record = &edca_params->acbk;
			break;
		case QCA_WLAN_AC_VI:
			edca_record = &edca_params->acvi;
			break;
		case QCA_WLAN_AC_VO:
			edca_record = &edca_params->acvo;
			break;
		default:
			goto fail;
		}

		wma_update_edca_params_for_ac(edca_record, &wmm_param[ac], ac,
				edca_params->mu_edca_params);

		ol_tx_wmm_param.ac[ac].aifs = wmm_param[ac].aifs;
		ol_tx_wmm_param.ac[ac].cwmin = wmm_param[ac].cwmin;
		ol_tx_wmm_param.ac[ac].cwmax = wmm_param[ac].cwmax;
	}

	status = wmi_unified_process_update_edca_param(wma_handle->wmi_handle,
						vdev_id,
						edca_params->mu_edca_params,
						wmm_param);
	if (status == QDF_STATUS_E_NOMEM)
		return status;
	else if (status == QDF_STATUS_E_FAILURE)
		goto fail;

	cdp_set_wmm_param(soc, WMI_PDEV_ID_SOC, ol_tx_wmm_param);

	return QDF_STATUS_SUCCESS;

fail:
	wma_err("Failed to set WMM Paremeters");
	return QDF_STATUS_E_FAILURE;
}

/**
 * wmi_unified_probe_rsp_tmpl_send() - send probe response template to fw
 * @wma: wma handle
 * @vdev_id: vdev id
 * @probe_rsp_info: probe response info
 *
 * Return: 0 for success or error code
 */
static int wmi_unified_probe_rsp_tmpl_send(tp_wma_handle wma,
				   uint8_t vdev_id,
				   tpSendProbeRespParams probe_rsp_info)
{
	uint64_t adjusted_tsf_le;
	struct ieee80211_frame *wh;
	struct wmi_probe_resp_params params;

	wma_debug("Send probe response template for vdev %d", vdev_id);

	/*
	 * Make the TSF offset negative so probe response in the same
	 * staggered batch have the same TSF.
	 */
	adjusted_tsf_le = cpu_to_le64(0ULL -
				      wma->interfaces[vdev_id].tsfadjust);
	/* Update the timstamp in the probe response buffer with adjusted TSF */
	wh = (struct ieee80211_frame *)probe_rsp_info->probeRespTemplate;
	A_MEMCPY(&wh[1], &adjusted_tsf_le, sizeof(adjusted_tsf_le));

	params.prb_rsp_template_len = probe_rsp_info->probeRespTemplateLen;
	params.prb_rsp_template_frm = probe_rsp_info->probeRespTemplate;

	return wmi_unified_probe_rsp_tmpl_send_cmd(wma->wmi_handle, vdev_id,
						   &params);
}

/**
 * wma_unified_bcn_tmpl_send() - send beacon template to fw
 * @wma:wma handle
 * @vdev_id: vdev id
 * @bcn_info: beacon info
 * @bytes_to_strip: bytes to strip
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS wma_unified_bcn_tmpl_send(tp_wma_handle wma,
				     uint8_t vdev_id,
				     const tpSendbeaconParams bcn_info,
				     uint8_t bytes_to_strip)
{
	struct beacon_tmpl_params params = {0};
	uint32_t tmpl_len, tmpl_len_aligned;
	uint8_t *frm;
	QDF_STATUS ret;
	uint8_t *p2p_ie;
	uint16_t p2p_ie_len = 0;
	uint64_t adjusted_tsf_le;
	struct ieee80211_frame *wh;

	if (!wma_is_vdev_valid(vdev_id)) {
		wma_err("vdev id:%d is not active ", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	wma_nofl_debug("Send beacon template for vdev %d", vdev_id);

	if (bcn_info->p2pIeOffset) {
		p2p_ie = bcn_info->beacon + bcn_info->p2pIeOffset;
		p2p_ie_len = (uint16_t) p2p_ie[1] + 2;
	}

	/*
	 * XXX: The first byte of beacon buffer contains beacon length
	 * only when UMAC in sending the beacon template. In othercases
	 * (ex: from tbtt update) beacon length is read from beacon
	 * information.
	 */
	if (bytes_to_strip)
		tmpl_len = *(uint32_t *) &bcn_info->beacon[0];
	else
		tmpl_len = bcn_info->beaconLength;

	if (tmpl_len > WMI_BEACON_TX_BUFFER_SIZE) {
		wma_err("tmpl_len: %d > %d. Invalid tmpl len", tmpl_len,
			WMI_BEACON_TX_BUFFER_SIZE);
		return -EINVAL;
	}

	if (p2p_ie_len) {
		if (tmpl_len <= p2p_ie_len) {
			wma_err("tmpl_len %d <= p2p_ie_len %d, Invalid",
				tmpl_len, p2p_ie_len);
			return -EINVAL;
		}
		tmpl_len -= (uint32_t) p2p_ie_len;
	}

	frm = bcn_info->beacon + bytes_to_strip;
	tmpl_len_aligned = roundup(tmpl_len, sizeof(A_UINT32));
	/*
	 * Make the TSF offset negative so beacons in the same
	 * staggered batch have the same TSF.
	 */
	adjusted_tsf_le = cpu_to_le64(0ULL -
				      wma->interfaces[vdev_id].tsfadjust);
	/* Update the timstamp in the beacon buffer with adjusted TSF */
	wh = (struct ieee80211_frame *)frm;
	A_MEMCPY(&wh[1], &adjusted_tsf_le, sizeof(adjusted_tsf_le));



	params.vdev_id = vdev_id;
	params.tim_ie_offset = bcn_info->timIeOffset - bytes_to_strip;
	params.tmpl_len = tmpl_len;
	params.frm = frm;
	params.tmpl_len_aligned = tmpl_len_aligned;
	params.enable_bigtk =
		mlme_get_bigtk_support(wma->interfaces[vdev_id].vdev);
	if (bcn_info->csa_count_offset &&
	    (bcn_info->csa_count_offset > bytes_to_strip))
		params.csa_switch_count_offset =
			bcn_info->csa_count_offset - bytes_to_strip;
	if (bcn_info->ecsa_count_offset &&
	    (bcn_info->ecsa_count_offset > bytes_to_strip))
		params.ext_csa_switch_count_offset =
			bcn_info->ecsa_count_offset - bytes_to_strip;

	ret = wmi_unified_beacon_tmpl_send_cmd(wma->wmi_handle,
				 &params);
	if (QDF_IS_STATUS_ERROR(ret))
		wma_err("Failed to send bcn tmpl: %d", ret);

	return ret;
}

/**
 * wma_store_bcn_tmpl() - store beacon template
 * @wma: wma handle
 * @vdev_id: vdev id
 * @bcn_info: beacon params
 *
 * This function stores beacon template locally.
 * This will send to target on the reception of
 * SWBA event.
 *
 * Return: QDF status
 */
static QDF_STATUS wma_store_bcn_tmpl(tp_wma_handle wma, uint8_t vdev_id,
				     tpSendbeaconParams bcn_info)
{
	struct beacon_info *bcn;
	uint32_t len;
	uint8_t *bcn_payload;
	struct beacon_tim_ie *tim_ie;

	bcn = wma->interfaces[vdev_id].beacon;
	if (!bcn || !bcn->buf) {
		wma_err("Memory is not allocated to hold bcn template");
		return QDF_STATUS_E_INVAL;
	}

	len = *(uint32_t *) &bcn_info->beacon[0];
	if (len > SIR_MAX_BEACON_SIZE - sizeof(uint32_t)) {
		wma_err("Received beacon len %u exceeding max limit %lu",
			len, (unsigned long)(
			 SIR_MAX_BEACON_SIZE - sizeof(uint32_t)));
		return QDF_STATUS_E_INVAL;
	}
	qdf_spin_lock_bh(&bcn->lock);

	/*
	 * Copy received beacon template content in local buffer.
	 * this will be send to target on the reception of SWBA
	 * event from target.
	 */
	qdf_nbuf_trim_tail(bcn->buf, qdf_nbuf_len(bcn->buf));
	memcpy(qdf_nbuf_data(bcn->buf),
	       bcn_info->beacon + 4 /* Exclude beacon length field */,
	       len);
	if (bcn_info->timIeOffset > 3)
		bcn->tim_ie_offset = bcn_info->timIeOffset - 4;
	else
		bcn->tim_ie_offset = bcn_info->timIeOffset;

	if (bcn_info->p2pIeOffset > 3)
		bcn->p2p_ie_offset = bcn_info->p2pIeOffset - 4;
	else
		bcn->p2p_ie_offset = bcn_info->p2pIeOffset;

	bcn_payload = qdf_nbuf_data(bcn->buf);
	if (bcn->tim_ie_offset) {
		tim_ie = (struct beacon_tim_ie *)
				(&bcn_payload[bcn->tim_ie_offset]);
		/*
		 * Initial Value of bcn->dtim_count will be 0.
		 * But if the beacon gets updated then current dtim
		 * count will be restored
		 */
		tim_ie->dtim_count = bcn->dtim_count;
		tim_ie->tim_bitctl = 0;
	}

	qdf_nbuf_put_tail(bcn->buf, len);
	bcn->len = len;

	qdf_spin_unlock_bh(&bcn->lock);

	return QDF_STATUS_SUCCESS;
}

int wma_tbttoffset_update_event_handler(void *handle, uint8_t *event,
					       uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_TBTTOFFSET_UPDATE_EVENTID_param_tlvs *param_buf;
	wmi_tbtt_offset_event_fixed_param *tbtt_offset_event;
	struct wma_txrx_node *intf;
	struct beacon_info *bcn;
	tSendbeaconParams bcn_info;
	uint32_t *adjusted_tsf = NULL;
	uint32_t if_id = 0, vdev_map;

	if (!wma) {
		wma_err("Invalid wma handle");
		return -EINVAL;
	}

	param_buf = (WMI_TBTTOFFSET_UPDATE_EVENTID_param_tlvs *) event;
	if (!param_buf) {
		wma_err("Invalid tbtt update event buffer");
		return -EINVAL;
	}

	tbtt_offset_event = param_buf->fixed_param;
	intf = wma->interfaces;
	vdev_map = tbtt_offset_event->vdev_map;
	adjusted_tsf = param_buf->tbttoffset_list;
	if (!adjusted_tsf) {
		wma_err("Invalid adjusted_tsf");
		return -EINVAL;
	}

	for (; (if_id < wma->max_bssid && vdev_map); vdev_map >>= 1, if_id++) {
		if (!intf[if_id].vdev)
			continue;

		if (!(vdev_map & 0x1))
			continue;

		bcn = intf[if_id].beacon;
		if (!bcn) {
			wma_err("Invalid beacon");
			return -EINVAL;
		}
		if (!bcn->buf) {
			wma_err("Invalid beacon buffer");
			return -EINVAL;
		}
		/* Save the adjusted TSF */
		intf[if_id].tsfadjust = adjusted_tsf[if_id];

		qdf_spin_lock_bh(&bcn->lock);
		qdf_mem_zero(&bcn_info, sizeof(bcn_info));
		qdf_mem_copy(bcn_info.beacon,
			     qdf_nbuf_data(bcn->buf), bcn->len);
		bcn_info.p2pIeOffset = bcn->p2p_ie_offset;
		bcn_info.beaconLength = bcn->len;
		bcn_info.timIeOffset = bcn->tim_ie_offset;
		qdf_spin_unlock_bh(&bcn->lock);

		/* Update beacon template in firmware */
		wma_unified_bcn_tmpl_send(wma, if_id, &bcn_info, 0);
	}
	return 0;
}

/**
 * wma_p2p_go_set_beacon_ie() - set beacon IE for p2p go
 * @wma_handle: wma handle
 * @vdev_id: vdev id
 * @p2pIe: p2p IE
 *
 * Return: 0 for success or error code
 */
static int wma_p2p_go_set_beacon_ie(t_wma_handle *wma_handle,
				    A_UINT32 vdev_id, uint8_t *p2pIe)
{
	if (!wma_handle) {
		wma_err("wma handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wmi_unified_p2p_go_set_beacon_ie_cmd(wma_handle->wmi_handle,
							vdev_id, p2pIe);
}

/**
 * wma_send_probe_rsp_tmpl() - send probe resp template
 * @wma: wma handle
 * @probe_rsp_info: probe response info
 *
 * This funciton sends probe response template to fw which
 * firmware will use in case of probe response offload.
 *
 * Return: none
 */
void wma_send_probe_rsp_tmpl(tp_wma_handle wma,
				    tpSendProbeRespParams probe_rsp_info)
{
	uint8_t vdev_id;
	struct sAniProbeRspStruct *probe_rsp;

	if (!probe_rsp_info) {
		wma_err("probe_rsp_info is NULL");
		return;
	}

	probe_rsp = (struct sAniProbeRspStruct *)
			(probe_rsp_info->probeRespTemplate);
	if (!probe_rsp) {
		wma_err("probe_rsp is NULL");
		return;
	}

	if (wma_find_vdev_id_by_addr(wma, probe_rsp->macHdr.sa, &vdev_id)) {
		wma_err("failed to get vdev id");
		return;
	}

	if (wmi_service_enabled(wma->wmi_handle,
				   wmi_service_beacon_offload)) {
		wma_nofl_debug("Beacon Offload Enabled Sending Unified command");
		if (wmi_unified_probe_rsp_tmpl_send(wma, vdev_id,
						    probe_rsp_info) < 0) {
			wma_err("wmi_unified_probe_rsp_tmpl_send Failed");
			return;
		}
	}
}

QDF_STATUS wma_set_ap_vdev_up(tp_wma_handle wma, uint8_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct vdev_mlme_obj *mlme_obj;
	struct wlan_objmgr_vdev *vdev;
	struct wma_txrx_node *iface;

	iface = &wma->interfaces[vdev_id];
	vdev = iface->vdev;
	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!mlme_obj) {
		wma_err("failed to get mlme_obj");
		return QDF_STATUS_E_INVAL;
	}
	mlme_obj->proto.sta.assoc_id = 0;

	status = vdev_mgr_up_send(mlme_obj);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("failed to send vdev up");
		return status;
	}
	wma_set_sap_keepalive(wma, vdev_id);
	wma_set_vdev_mgmt_rate(wma, vdev_id);

	return status;
}

/**
 * wma_send_beacon() - send beacon template
 * @wma: wma handle
 * @bcn_info: beacon info
 *
 * This funciton store beacon template locally and
 * update keep alive parameters
 *
 * Return: none
 */
void wma_send_beacon(tp_wma_handle wma, tpSendbeaconParams bcn_info)
{
	uint8_t vdev_id;
	QDF_STATUS status;
	uint8_t *p2p_ie;
	struct sAniBeaconStruct *beacon;

	wma_nofl_debug("Beacon update reason %d", bcn_info->reason);
	beacon = (struct sAniBeaconStruct *) (bcn_info->beacon);
	if (wma_find_vdev_id_by_addr(wma, beacon->macHdr.sa, &vdev_id)) {
		wma_err("failed to get vdev id");
		status = QDF_STATUS_E_INVAL;
		goto send_rsp;
	}

	if (wmi_service_enabled(wma->wmi_handle,
				   wmi_service_beacon_offload)) {
		status = wma_unified_bcn_tmpl_send(wma, vdev_id, bcn_info, 4);
		if (QDF_IS_STATUS_ERROR(status)) {
			wma_err("wmi_unified_bcn_tmpl_send Failed");
			goto send_rsp;
		}

		if (bcn_info->p2pIeOffset) {
			p2p_ie = bcn_info->beacon + bcn_info->p2pIeOffset;
			wma_debug("p2pIe is present - vdev_id %hu, p2p_ie = %pK, p2p ie len = %hu",
				  vdev_id, p2p_ie, p2p_ie[1]);
			if (wma_p2p_go_set_beacon_ie(wma, vdev_id,
							 p2p_ie) < 0) {
				wma_err("wmi_unified_bcn_tmpl_send Failed");
				status = QDF_STATUS_E_INVAL;
				goto send_rsp;
			}
		}
	}
	status = wma_store_bcn_tmpl(wma, vdev_id, bcn_info);
	if (status != QDF_STATUS_SUCCESS) {
		wma_err("wma_store_bcn_tmpl Failed");
		goto send_rsp;
	}

send_rsp:
	bcn_info->status = status;
	wma_send_msg(wma, WMA_SEND_BCN_RSP, (void *)bcn_info, 0);
}

/**
 * wma_set_keepalive_req() - send keep alive request to fw
 * @wma: wma handle
 * @keepalive: keep alive parameters
 *
 * Return: none
 */
void wma_set_keepalive_req(tp_wma_handle wma,
			   struct keep_alive_req *keepalive)
{
	wma_nofl_debug("KEEPALIVE:PacketType:%d", keepalive->packetType);
	wma_set_sta_keep_alive(wma, keepalive->sessionId,
			       keepalive->packetType,
			       keepalive->timePeriod,
			       keepalive->hostIpv4Addr,
			       keepalive->destIpv4Addr,
			       keepalive->dest_macaddr.bytes);

	qdf_mem_free(keepalive);
}

/**
 * wma_beacon_miss_handler() - beacon miss event handler
 * @wma: wma handle
 * @vdev_id: vdev id
 * @riis: rssi value
 *
 * This function send beacon miss indication to upper layers.
 *
 * Return: none
 */
void wma_beacon_miss_handler(tp_wma_handle wma, uint32_t vdev_id, int32_t rssi)
{
	struct missed_beacon_ind *beacon_miss_ind;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);

	beacon_miss_ind = qdf_mem_malloc(sizeof(*beacon_miss_ind));
	if (!beacon_miss_ind)
		return;

	if (mac && mac->sme.tx_queue_cb)
		mac->sme.tx_queue_cb(mac->hdd_handle, vdev_id,
				     WLAN_STOP_ALL_NETIF_QUEUE,
				     WLAN_CONTROL_PATH);
	beacon_miss_ind->messageType = WMA_MISSED_BEACON_IND;
	beacon_miss_ind->length = sizeof(*beacon_miss_ind);
	beacon_miss_ind->bss_idx = vdev_id;

	wma_send_msg(wma, WMA_MISSED_BEACON_IND, beacon_miss_ind, 0);
	if (!wmi_service_enabled(wma->wmi_handle,
				 wmi_service_hw_db2dbm_support))
		rssi += WMA_TGT_NOISE_FLOOR_DBM;
	wma_lost_link_info_handler(wma, vdev_id, rssi);
}

void wlan_cm_send_beacon_miss(uint8_t vdev_id, int32_t rssi)
{
	tp_wma_handle wma;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		wma_err("Invalid wma");
		return;
	}

	wma_beacon_miss_handler(wma, vdev_id, rssi);
}

/**
 * wma_get_status_str() - get string of tx status from firmware
 * @status: tx status
 *
 * Return: converted string of tx status
 */
static const char *wma_get_status_str(uint32_t status)
{
	switch (status) {
	default:
		return "unknown";
	CASE_RETURN_STRING(WMI_MGMT_TX_COMP_TYPE_COMPLETE_OK);
	CASE_RETURN_STRING(WMI_MGMT_TX_COMP_TYPE_DISCARD);
	CASE_RETURN_STRING(WMI_MGMT_TX_COMP_TYPE_INSPECT);
	CASE_RETURN_STRING(WMI_MGMT_TX_COMP_TYPE_COMPLETE_NO_ACK);
	CASE_RETURN_STRING(WMI_MGMT_TX_COMP_TYPE_MAX);
	}
}

#ifdef CONFIG_HL_SUPPORT
static inline void wma_mgmt_unmap_buf(tp_wma_handle wma_handle, qdf_nbuf_t buf)
{
}
#else
static inline void wma_mgmt_unmap_buf(tp_wma_handle wma_handle, qdf_nbuf_t buf)
{
	qdf_nbuf_unmap_single(wma_handle->qdf_dev, buf, QDF_DMA_TO_DEVICE);
}
#endif

#if !defined(REMOVE_PKT_LOG)
/**
 * wma_mgmt_pktdump_status_map() - map MGMT Tx completion status with
 * packet dump Tx status
 * @status: MGMT Tx completion status
 *
 * Return: packet dump tx_status enum
 */
static inline enum tx_status
wma_mgmt_pktdump_status_map(WMI_MGMT_TX_COMP_STATUS_TYPE status)
{
	enum tx_status pktdump_status;

	switch (status) {
	case WMI_MGMT_TX_COMP_TYPE_COMPLETE_OK:
		pktdump_status = tx_status_ok;
		break;
	case WMI_MGMT_TX_COMP_TYPE_DISCARD:
		pktdump_status = tx_status_discard;
		break;
	case WMI_MGMT_TX_COMP_TYPE_COMPLETE_NO_ACK:
		pktdump_status = tx_status_no_ack;
		break;
	default:
		pktdump_status = tx_status_discard;
		break;
	}
	return pktdump_status;
}
#endif

/**
 * wma_process_mgmt_tx_completion() - process mgmt completion
 * @wma_handle: wma handle
 * @desc_id: descriptor id
 * @status: status
 *
 * Return: 0 for success or error code
 */
static int wma_process_mgmt_tx_completion(tp_wma_handle wma_handle,
					  uint32_t desc_id, uint32_t status)
{
	struct wlan_objmgr_pdev *pdev;
	qdf_nbuf_t buf = NULL;
	QDF_STATUS ret;
#if !defined(REMOVE_PKT_LOG)
	uint8_t vdev_id = 0;
	ol_txrx_pktdump_cb packetdump_cb;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	enum tx_status pktdump_status;
#endif
	struct wmi_mgmt_params mgmt_params = {};

	if (!wma_handle) {
		wma_err("wma handle is NULL");
		return -EINVAL;
	}

	wma_debug("status: %s wmi_desc_id: %d",
		  wma_get_status_str(status), desc_id);

	pdev = wma_handle->pdev;
	if (!pdev) {
		wma_err("psoc ptr is NULL");
		return -EINVAL;
	}

	buf = mgmt_txrx_get_nbuf(pdev, desc_id);


	if (buf)
		wma_mgmt_unmap_buf(wma_handle, buf);

#if !defined(REMOVE_PKT_LOG)
	vdev_id = mgmt_txrx_get_vdev_id(pdev, desc_id);
	mgmt_params.vdev_id = vdev_id;
	packetdump_cb = wma_handle->wma_mgmt_tx_packetdump_cb;
	pktdump_status = wma_mgmt_pktdump_status_map(status);
	if (packetdump_cb)
		packetdump_cb(soc, WMI_PDEV_ID_SOC, vdev_id,
			      buf, pktdump_status, TX_MGMT_PKT);
#endif

	ret = mgmt_txrx_tx_completion_handler(pdev, desc_id, status,
					      &mgmt_params);

	if (ret != QDF_STATUS_SUCCESS) {
		wma_err("Failed to process mgmt tx completion");
		return -EINVAL;
	}

	return 0;
}

/**
 * wma_extract_mgmt_offload_event_params() - Extract mgmt event params
 * @params: Management offload event params
 * @hdr: Management header to extract
 *
 * Return: None
 */
static void wma_extract_mgmt_offload_event_params(
				struct mgmt_offload_event_params *params,
				wmi_mgmt_hdr *hdr)
{
	params->tsf_l32 = hdr->tsf_l32;
	params->chan_freq = hdr->chan_freq;
	params->rate_kbps = hdr->rate_kbps;
	params->rssi = hdr->rssi;
	params->buf_len = hdr->buf_len;
	params->tx_status = hdr->tx_status;
	params->tx_retry_cnt = hdr->tx_retry_cnt;
}

/**
 * wma_mgmt_tx_completion_handler() - wma mgmt Tx completion event handler
 * @handle: wma handle
 * @cmpl_event_params: completion event handler data
 * @len: length of @cmpl_event_params
 *
 * Return: 0 on success; error number otherwise
 */

int wma_mgmt_tx_completion_handler(void *handle, uint8_t *cmpl_event_params,
				   uint32_t len)
{
	tp_wma_handle wma_handle = (tp_wma_handle)handle;
	WMI_MGMT_TX_COMPLETION_EVENTID_param_tlvs *param_buf;
	wmi_mgmt_tx_compl_event_fixed_param *cmpl_params;

	param_buf = (WMI_MGMT_TX_COMPLETION_EVENTID_param_tlvs *)
		cmpl_event_params;
	if (!param_buf || !wma_handle) {
		wma_err("Invalid mgmt Tx completion event");
		return -EINVAL;
	}
	cmpl_params = param_buf->fixed_param;

	if ((ucfg_pkt_capture_get_pktcap_mode(wma_handle->psoc) &
	    PKT_CAPTURE_MODE_MGMT_ONLY) && param_buf->mgmt_hdr) {
		struct mgmt_offload_event_params params = {0};

		wma_extract_mgmt_offload_event_params(
					&params,
					(wmi_mgmt_hdr *)param_buf->mgmt_hdr);
		ucfg_pkt_capture_mgmt_tx_completion(wma_handle->pdev,
						    cmpl_params->desc_id,
						    cmpl_params->status,
						    &params);
	}

	wma_process_mgmt_tx_completion(wma_handle, cmpl_params->desc_id,
				       cmpl_params->status);

	return 0;
}

/**
 * wma_mgmt_tx_bundle_completion_handler() - mgmt bundle comp handler
 * @handle: wma handle
 * @buf: buffer
 * @len: length
 *
 * Return: 0 for success or error code
 */
int wma_mgmt_tx_bundle_completion_handler(void *handle, uint8_t *buf,
				   uint32_t len)
{
	tp_wma_handle wma_handle = (tp_wma_handle)handle;
	WMI_MGMT_TX_BUNDLE_COMPLETION_EVENTID_param_tlvs *param_buf;
	wmi_mgmt_tx_compl_bundle_event_fixed_param	*cmpl_params;
	uint32_t num_reports;
	uint32_t *desc_ids;
	uint32_t *status;
	uint32_t i, buf_len;
	bool excess_data = false;

	param_buf = (WMI_MGMT_TX_BUNDLE_COMPLETION_EVENTID_param_tlvs *)buf;
	if (!param_buf || !wma_handle) {
		wma_err("Invalid mgmt Tx completion event");
		return -EINVAL;
	}
	cmpl_params = param_buf->fixed_param;
	num_reports = cmpl_params->num_reports;
	desc_ids = (uint32_t *)(param_buf->desc_ids);
	status = (uint32_t *)(param_buf->status);

	/* buf contains num_reports * sizeof(uint32) len of desc_ids and
	 * num_reports * sizeof(uint32) status,
	 * so (2 x (num_reports * sizeof(uint32)) should not exceed MAX
	 */
	if (cmpl_params->num_reports > (WMI_SVC_MSG_MAX_SIZE /
	    (2 * sizeof(uint32_t))))
		excess_data = true;
	else
		buf_len = cmpl_params->num_reports * (2 * sizeof(uint32_t));

	if (excess_data || (sizeof(*cmpl_params) > (WMI_SVC_MSG_MAX_SIZE -
	    buf_len))) {
		wma_err("excess wmi buffer: num_reports %d",
			cmpl_params->num_reports);
		return -EINVAL;
	}

	if ((cmpl_params->num_reports > param_buf->num_desc_ids) ||
	    (cmpl_params->num_reports > param_buf->num_status)) {
		wma_err("Invalid num_reports %d, num_desc_ids %d, num_status %d",
			 cmpl_params->num_reports, param_buf->num_desc_ids,
			 param_buf->num_status);
		return -EINVAL;
	}

	for (i = 0; i < num_reports; i++) {
		if ((ucfg_pkt_capture_get_pktcap_mode(wma_handle->psoc) &
		    PKT_CAPTURE_MODE_MGMT_ONLY) && param_buf->mgmt_hdr) {
			struct mgmt_offload_event_params params = {0};

			wma_extract_mgmt_offload_event_params(
				&params,
				&((wmi_mgmt_hdr *)param_buf->mgmt_hdr)[i]);
			ucfg_pkt_capture_mgmt_tx_completion(
				wma_handle->pdev, desc_ids[i],
				status[i], &params);
		}

		wma_process_mgmt_tx_completion(wma_handle,
					       desc_ids[i], status[i]);
	}
	return 0;
}

/**
 * wma_process_update_opmode() - process update VHT opmode cmd from UMAC
 * @wma_handle: wma handle
 * @update_vht_opmode: vht opmode
 *
 * Return: none
 */
void wma_process_update_opmode(tp_wma_handle wma_handle,
			       tUpdateVHTOpMode *update_vht_opmode)
{
	wmi_host_channel_width ch_width;
	uint8_t pdev_id;
	struct wlan_objmgr_peer *peer;
	struct wlan_objmgr_psoc *psoc = wma_handle->psoc;
	enum wlan_phymode peer_phymode;
	uint32_t fw_phymode;
	enum wlan_peer_type peer_type;

	pdev_id = wlan_objmgr_pdev_get_pdev_id(wma_handle->pdev);
	peer = wlan_objmgr_get_peer(psoc, pdev_id,
				    update_vht_opmode->peer_mac,
				    WLAN_LEGACY_WMA_ID);
	if (!peer) {
		wma_err("peer object invalid");
		return;
	}

	peer_type = wlan_peer_get_peer_type(peer);
	if (peer_type == WLAN_PEER_SELF) {
		wma_err("self peer wrongly used");
		wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_WMA_ID);
		return;
	}

	wlan_peer_obj_lock(peer);
	peer_phymode = wlan_peer_get_phymode(peer);
	wlan_peer_obj_unlock(peer);
	wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_WMA_ID);

	fw_phymode = wma_host_to_fw_phymode(peer_phymode);

	ch_width = wmi_get_ch_width_from_phy_mode(wma_handle->wmi_handle,
						  fw_phymode);
	wma_debug("ch_width: %d, fw phymode: %d peer_phymode %d",
		  ch_width, fw_phymode, peer_phymode);
	if (ch_width < update_vht_opmode->opMode) {
		wma_err("Invalid peer bw update %d, self bw %d",
			update_vht_opmode->opMode, ch_width);
		return;
	}

	wma_debug("opMode = %d", update_vht_opmode->opMode);
	wma_set_peer_param(wma_handle, update_vht_opmode->peer_mac,
			   WMI_PEER_CHWIDTH, update_vht_opmode->opMode,
			   update_vht_opmode->smesessionId);
}

/**
 * wma_process_update_rx_nss() - process update RX NSS cmd from UMAC
 * @wma_handle: wma handle
 * @update_rx_nss: rx nss value
 *
 * Return: none
 */
void wma_process_update_rx_nss(tp_wma_handle wma_handle,
			       tUpdateRxNss *update_rx_nss)
{
	struct target_psoc_info *tgt_hdl;
	struct wma_txrx_node *intr =
		&wma_handle->interfaces[update_rx_nss->smesessionId];
	int rx_nss = update_rx_nss->rxNss;
	int num_rf_chains;

	tgt_hdl = wlan_psoc_get_tgt_if_handle(wma_handle->psoc);
	if (!tgt_hdl) {
		wma_err("target psoc info is NULL");
		return;
	}

	num_rf_chains = target_if_get_num_rf_chains(tgt_hdl);
	if (rx_nss > num_rf_chains || rx_nss > WMA_MAX_NSS)
		rx_nss = QDF_MIN(num_rf_chains, WMA_MAX_NSS);

	intr->nss = (uint8_t)rx_nss;
	update_rx_nss->rxNss = (uint32_t)rx_nss;

	wma_debug("Rx Nss = %d", update_rx_nss->rxNss);

	wma_set_peer_param(wma_handle, update_rx_nss->peer_mac,
			   WMI_PEER_NSS, update_rx_nss->rxNss,
			   update_rx_nss->smesessionId);
}

/**
 * wma_process_update_membership() - process update group membership cmd
 * @wma_handle: wma handle
 * @membership: group membership info
 *
 * Return: none
 */
void wma_process_update_membership(tp_wma_handle wma_handle,
				   tUpdateMembership *membership)
{
	wma_debug("membership = %x ", membership->membership);

	wma_set_peer_param(wma_handle, membership->peer_mac,
			   WMI_PEER_MEMBERSHIP, membership->membership,
			   membership->smesessionId);
}

/**
 * wma_process_update_userpos() - process update user pos cmd from UMAC
 * @wma_handle: wma handle
 * @userpos: user pos value
 *
 * Return: none
 */
void wma_process_update_userpos(tp_wma_handle wma_handle,
				tUpdateUserPos *userpos)
{
	wma_debug("userPos = %x ", userpos->userPos);

	wma_set_peer_param(wma_handle, userpos->peer_mac,
			   WMI_PEER_USERPOS, userpos->userPos,
			   userpos->smesessionId);

	/* Now that membership/userpos is updated in fw,
	 * enable GID PPS.
	 */
	wma_set_ppsconfig(userpos->smesessionId, WMA_VHT_PPS_GID_MATCH, 1);

}

QDF_STATUS wma_set_cts2self_for_p2p_go(void *wma_handle,
				    uint32_t cts2self_for_p2p_go)
{
	int32_t ret;
	tp_wma_handle wma = (tp_wma_handle)wma_handle;
	struct pdev_params pdevparam;

	pdevparam.param_id = WMI_PDEV_PARAM_CTS2SELF_FOR_P2P_GO_CONFIG;
	pdevparam.param_value = cts2self_for_p2p_go;

	ret = wmi_unified_pdev_param_send(wma->wmi_handle,
			&pdevparam,
			WMA_WILDCARD_PDEV_ID);
	if (ret) {
		wma_err("Fail to Set CTS2SELF for p2p GO %d",
			cts2self_for_p2p_go);
		return QDF_STATUS_E_FAILURE;
	}

	wma_nofl_debug("Successfully Set CTS2SELF for p2p GO %d",
		       cts2self_for_p2p_go);

	return QDF_STATUS_SUCCESS;
}


/**
 * wma_set_htconfig() - set ht config parameters to target
 * @vdev_id: vdev id
 * @ht_capab: ht capablity
 * @value: value of ht param
 *
 * Return: QDF status
 */
QDF_STATUS wma_set_htconfig(uint8_t vdev_id, uint16_t ht_capab, int value)
{
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);
	QDF_STATUS ret = QDF_STATUS_E_FAILURE;

	if (!wma) {
		wma_err("Failed to get wma");
		return QDF_STATUS_E_INVAL;
	}

	switch (ht_capab) {
	case WNI_CFG_HT_CAP_INFO_ADVANCE_CODING:
		ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
						      WMI_VDEV_PARAM_LDPC,
						      value);
		break;
	case WNI_CFG_HT_CAP_INFO_TX_STBC:
		ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
						      WMI_VDEV_PARAM_TX_STBC,
						      value);
		break;
	case WNI_CFG_HT_CAP_INFO_RX_STBC:
		ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
						      WMI_VDEV_PARAM_RX_STBC,
						      value);
		break;
	case WNI_CFG_HT_CAP_INFO_SHORT_GI_20MHZ:
	case WNI_CFG_HT_CAP_INFO_SHORT_GI_40MHZ:
		wma_err("ht_capab = %d, value = %d", ht_capab,
			 value);
		ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
						WMI_VDEV_PARAM_SGI, value);
		if (ret == QDF_STATUS_SUCCESS)
			wma->interfaces[vdev_id].config.shortgi = value;
		break;
	default:
		wma_err("INVALID HT CONFIG");
	}

	return ret;
}

#ifdef WLAN_FEATURE_11W

/**
 * wma_extract_ccmp_pn() - extract 6 byte PN from the CCMP header
 * @ccmp_ptr: CCMP header
 *
 * Return: PN extracted from header.
 */
static uint64_t wma_extract_ccmp_pn(uint8_t *ccmp_ptr)
{
	uint8_t rsvd, key, pn[6];
	uint64_t new_pn;

	/*
	 *   +-----+-----+------+----------+-----+-----+-----+-----+
	 *   | PN0 | PN1 | rsvd | rsvd/key | PN2 | PN3 | PN4 | PN5 |
	 *   +-----+-----+------+----------+-----+-----+-----+-----+
	 *                   CCMP Header Format
	 */

	/* Extract individual bytes */
	pn[0] = (uint8_t) *ccmp_ptr;
	pn[1] = (uint8_t) *(ccmp_ptr + 1);
	rsvd = (uint8_t) *(ccmp_ptr + 2);
	key = (uint8_t) *(ccmp_ptr + 3);
	pn[2] = (uint8_t) *(ccmp_ptr + 4);
	pn[3] = (uint8_t) *(ccmp_ptr + 5);
	pn[4] = (uint8_t) *(ccmp_ptr + 6);
	pn[5] = (uint8_t) *(ccmp_ptr + 7);

	/* Form 6 byte PN with 6 individual bytes of PN */
	new_pn = ((uint64_t) pn[5] << 40) |
		 ((uint64_t) pn[4] << 32) |
		 ((uint64_t) pn[3] << 24) |
		 ((uint64_t) pn[2] << 16) |
		 ((uint64_t) pn[1] << 8) | ((uint64_t) pn[0] << 0);

	return new_pn;
}

/**
 * wma_is_ccmp_pn_replay_attack() - detect replay attacking using PN in CCMP
 * @wma: wma context
 * @wh: 802.11 frame header
 * @ccmp_ptr: CCMP frame header
 *
 * Return: true/false
 */
static bool
wma_is_ccmp_pn_replay_attack(tp_wma_handle wma, struct ieee80211_frame *wh,
			     uint8_t *ccmp_ptr)
{
	uint64_t new_pn;
	bool ret = false;
	struct peer_mlme_priv_obj *peer_priv;
	struct wlan_objmgr_peer *peer;

	new_pn = wma_extract_ccmp_pn(ccmp_ptr);

	peer = wlan_objmgr_get_peer_by_mac(wma->psoc, wh->i_addr2,
					   WLAN_LEGACY_WMA_ID);
	if (!peer)
		return ret;

	peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							  WLAN_UMAC_COMP_MLME);
	if (!peer_priv) {
		wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_WMA_ID);
		return ret;
	}

	if (peer_priv->last_pn_valid) {
		if (new_pn > peer_priv->last_pn) {
			peer_priv->last_pn = new_pn;
		} else {
			wma_err_rl("PN Replay attack detected");
			/* per 11W amendment, keeping track of replay attacks */
			peer_priv->rmf_pn_replays += 1;
			ret = true;
		}
	} else {
		peer_priv->last_pn_valid = 1;
		peer_priv->last_pn = new_pn;
	}

	wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_WMA_ID);

	return ret;
}

#ifdef WLAN_FEATURE_11W
/**
 * wma_process_bip() - process mmie in rmf frame
 * @wma_handle: wma handle
 * @iface: txrx node
 * @wh: 80211 frame
 * @wbuf: Buffer
 *
 * Return: 0 for success or error code
 */
static
int wma_process_bip(tp_wma_handle wma_handle, struct wma_txrx_node *iface,
		    struct ieee80211_frame *wh, qdf_nbuf_t wbuf)
{
	uint16_t mmie_size;
	uint8_t *efrm;
	int32_t mgmtcipherset;
	enum wlan_crypto_cipher_type key_cipher;

	efrm = qdf_nbuf_data(wbuf) + qdf_nbuf_len(wbuf);

	mgmtcipherset = wlan_crypto_get_param(iface->vdev,
					      WLAN_CRYPTO_PARAM_MGMT_CIPHER);
	if (mgmtcipherset <= 0) {
		wma_err("Invalid key cipher %d", mgmtcipherset);
		return -EINVAL;
	}

	if (mgmtcipherset & (1 << WLAN_CRYPTO_CIPHER_AES_CMAC)) {
		key_cipher = WLAN_CRYPTO_CIPHER_AES_CMAC;
		mmie_size = cds_get_mmie_size();
	} else if (mgmtcipherset & (1 << WLAN_CRYPTO_CIPHER_AES_GMAC)) {
		key_cipher = WLAN_CRYPTO_CIPHER_AES_GMAC;
		mmie_size = cds_get_gmac_mmie_size();
	} else if (mgmtcipherset & (1 << WLAN_CRYPTO_CIPHER_AES_GMAC_256)) {
		key_cipher = WLAN_CRYPTO_CIPHER_AES_GMAC_256;
		mmie_size = cds_get_gmac_mmie_size();
	} else {
		wma_err("Invalid key cipher %d", mgmtcipherset);
		return -EINVAL;
	}

	/* Check if frame is invalid length */
	if (efrm - (uint8_t *)wh < sizeof(*wh) + mmie_size) {
		wma_err("Invalid frame length");
		return -EINVAL;
	}

	switch (key_cipher) {
	case WLAN_CRYPTO_CIPHER_AES_CMAC:
		if (!wmi_service_enabled(wma_handle->wmi_handle,
					 wmi_service_sta_pmf_offload)) {
			if (!wlan_crypto_is_mmie_valid(iface->vdev,
						       (uint8_t *)wh, efrm)) {
				wma_debug("BC/MC MIC error or MMIE not present, dropping the frame");
				return -EINVAL;
			}
		}
		break;
	case WLAN_CRYPTO_CIPHER_AES_GMAC:
	case WLAN_CRYPTO_CIPHER_AES_GMAC_256:
		if (!wmi_service_enabled(wma_handle->wmi_handle,
					 wmi_service_gmac_offload_support)) {
			if (!wlan_crypto_is_mmie_valid(iface->vdev,
						       (uint8_t *)wh, efrm)) {
				wma_debug("BC/MC GMAC MIC error or MMIE not present, dropping the frame");
				return -EINVAL;
			}
		}
		break;
	default:
		wma_err("Invalid key_type %d", key_cipher);
		return -EINVAL;
	}

	qdf_nbuf_trim_tail(wbuf, mmie_size);

	return 0;
}
#endif

/**
 * wma_process_rmf_frame() - process rmf frame
 * @wma_handle: wma handle
 * @iface: txrx node
 * @wh: 80211 frame
 * @rx_pkt: rx packet
 * @wbuf: Buffer
 *
 * Return: 0 for success or error code
 */
static
int wma_process_rmf_frame(tp_wma_handle wma_handle,
	struct wma_txrx_node *iface,
	struct ieee80211_frame *wh,
	cds_pkt_t *rx_pkt,
	qdf_nbuf_t wbuf)
{
	uint8_t *orig_hdr;
	uint8_t *ccmp;
	uint8_t mic_len, hdr_len, pdev_id;
	QDF_STATUS status;

	if ((wh)->i_fc[1] & IEEE80211_FC1_WEP) {
		if (QDF_IS_ADDR_BROADCAST(wh->i_addr1) ||
		    IEEE80211_IS_MULTICAST(wh->i_addr1)) {
			wma_err("Encrypted BC/MC frame dropping the frame");
			cds_pkt_return_packet(rx_pkt);
			return -EINVAL;
		}

		if (iface->type == WMI_VDEV_TYPE_NDI) {
			hdr_len = IEEE80211_CCMP_HEADERLEN;
			mic_len = IEEE80211_CCMP_MICLEN;
		} else {
			pdev_id =
				wlan_objmgr_pdev_get_pdev_id(wma_handle->pdev);
			status = mlme_get_peer_mic_len(wma_handle->psoc,
						       pdev_id, wh->i_addr2,
						       &mic_len, &hdr_len);
			if (QDF_IS_STATUS_ERROR(status)) {
				wma_err("Failed to get mic hdr and length");
				cds_pkt_return_packet(rx_pkt);
				return -EINVAL;
			}
		}

		if (qdf_nbuf_len(wbuf) < (sizeof(*wh) + hdr_len + mic_len)) {
			wma_err("Buffer length less than expected %d",
				 (int)qdf_nbuf_len(wbuf));
			cds_pkt_return_packet(rx_pkt);
			return -EINVAL;
		}

		orig_hdr = (uint8_t *) qdf_nbuf_data(wbuf);
		/* Pointer to head of CCMP header */
		ccmp = orig_hdr + sizeof(*wh);
		if (wma_is_ccmp_pn_replay_attack(wma_handle, wh, ccmp)) {
			wma_err_rl("Dropping the frame");
			cds_pkt_return_packet(rx_pkt);
			return -EINVAL;
		}

		/* Strip privacy headers (and trailer)
		 * for a received frame
		 */
		qdf_mem_move(orig_hdr +
			hdr_len, wh,
			sizeof(*wh));
		qdf_nbuf_pull_head(wbuf,
			hdr_len);
		qdf_nbuf_trim_tail(wbuf, mic_len);
		/*
		 * CCMP header has been pulled off
		 * reinitialize the start pointer of mac header
		 * to avoid accessing incorrect address
		 */
		wh = (struct ieee80211_frame *) qdf_nbuf_data(wbuf);
		rx_pkt->pkt_meta.mpdu_hdr_ptr =
				qdf_nbuf_data(wbuf);
		rx_pkt->pkt_meta.mpdu_len = qdf_nbuf_len(wbuf);
		rx_pkt->pkt_buf = wbuf;
		if (rx_pkt->pkt_meta.mpdu_len >=
			rx_pkt->pkt_meta.mpdu_hdr_len) {
			rx_pkt->pkt_meta.mpdu_data_len =
				rx_pkt->pkt_meta.mpdu_len -
				rx_pkt->pkt_meta.mpdu_hdr_len;
		} else {
			wma_err("mpdu len %d less than hdr %d, dropping frame",
				rx_pkt->pkt_meta.mpdu_len,
				rx_pkt->pkt_meta.mpdu_hdr_len);
			cds_pkt_return_packet(rx_pkt);
			return -EINVAL;
		}

		if (rx_pkt->pkt_meta.mpdu_data_len > MAX_MGMT_MPDU_LEN) {
			wma_err("Data Len %d greater than max, dropping frame",
				rx_pkt->pkt_meta.mpdu_data_len);
			cds_pkt_return_packet(rx_pkt);
			return -EINVAL;
		}
		rx_pkt->pkt_meta.mpdu_data_ptr =
		rx_pkt->pkt_meta.mpdu_hdr_ptr +
		rx_pkt->pkt_meta.mpdu_hdr_len;
		wma_debug("BSSID: "QDF_MAC_ADDR_FMT" tsf_delta: %u",
			  QDF_MAC_ADDR_REF(wh->i_addr3),
			  rx_pkt->pkt_meta.tsf_delta);
	} else {
		if (QDF_IS_ADDR_BROADCAST(wh->i_addr1) ||
		    IEEE80211_IS_MULTICAST(wh->i_addr1)) {
			if (0 != wma_process_bip(wma_handle, iface, wh, wbuf)) {
				cds_pkt_return_packet(rx_pkt);
				return -EINVAL;
			}
		} else {
			wma_err("Rx unprotected unicast mgmt frame");
			rx_pkt->pkt_meta.dpuFeedback =
				DPU_FEEDBACK_UNPROTECTED_ERROR;
		}
	}
	return 0;
}

/**
 * wma_get_peer_pmf_status() - Get the PMF capability of peer
 * @wma: wma handle
 * @peer_mac: peer mac addr
 *
 * Return: True if PMF is enabled, false otherwise.
 */
static bool
wma_get_peer_pmf_status(tp_wma_handle wma, uint8_t *peer_mac)
{
	struct wlan_objmgr_peer *peer;
	bool is_pmf_enabled;

	if (!peer_mac) {
		wma_err("peer_mac is NULL");
		return false;
	}

	peer = wlan_objmgr_get_peer(wma->psoc,
				    wlan_objmgr_pdev_get_pdev_id(wma->pdev),
				    peer_mac, WLAN_LEGACY_WMA_ID);
	if (!peer) {
		wma_err("Peer of peer_mac "QDF_MAC_ADDR_FMT" not found",
			 QDF_MAC_ADDR_REF(peer_mac));
		return false;
	}
	is_pmf_enabled = mlme_get_peer_pmf_status(peer);
	wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_WMA_ID);
	wma_nofl_debug("get is_pmf_enabled %d for "QDF_MAC_ADDR_FMT,
		       is_pmf_enabled, QDF_MAC_ADDR_REF(peer_mac));

	return is_pmf_enabled;
}

/**
 * wma_check_and_process_rmf_frame() - Process the frame if it is of rmf type
 * @wma_handle: wma handle
 * @vdev_id: vdev id
 * @wh: double pointer to 802.11 frame header which will be updated if the
 *	frame is of rmf type.
 * @rx_pkt: rx packet
 * @buf: Buffer
 *
 * Process the frame as rmf frame only if both DUT and peer are of PMF capable
 *
 * Return: 0 for success or error code
 */
static int
wma_check_and_process_rmf_frame(tp_wma_handle wma_handle,
				uint8_t vdev_id,
				struct ieee80211_frame **wh,
				cds_pkt_t *rx_pkt,
				qdf_nbuf_t buf)
{
	int status;
	struct wma_txrx_node *iface;
	struct ieee80211_frame *hdr = *wh;

	iface = &(wma_handle->interfaces[vdev_id]);
	if (iface->type != WMI_VDEV_TYPE_NDI && !iface->rmfEnabled)
		return 0;

	if (qdf_is_macaddr_group((struct qdf_mac_addr *)(hdr->i_addr1)) ||
	    qdf_is_macaddr_broadcast((struct qdf_mac_addr *)(hdr->i_addr1)) ||
	    wma_get_peer_pmf_status(wma_handle, hdr->i_addr2) ||
	    (iface->type == WMI_VDEV_TYPE_NDI &&
	    (hdr->i_fc[1] & IEEE80211_FC1_WEP))) {
		status = wma_process_rmf_frame(wma_handle, iface, hdr,
					       rx_pkt, buf);
		if (status)
			return status;
		/*
		 * CCMP header might have been pulled off reinitialize the
		 * start pointer of mac header
		 */
		*wh = (struct ieee80211_frame *)qdf_nbuf_data(buf);
	}

	return 0;
}
#else
static inline int
wma_check_and_process_rmf_frame(tp_wma_handle wma_handle,
				uint8_t vdev_id,
				struct ieee80211_frame **wh,
				cds_pkt_t *rx_pkt,
				qdf_nbuf_t buf)
{
	return 0;
}
#endif

/**
 * wma_is_pkt_drop_candidate() - check if the mgmt frame should be droppped
 * @wma_handle: wma handle
 * @peer_addr: peer MAC address
 * @bssid: BSSID Address
 * @subtype: Management frame subtype
 *
 * This function is used to decide if a particular management frame should be
 * dropped to prevent DOS attack. Timestamp is used to decide the DOS attack.
 *
 * Return: true if the packet should be dropped and false oterwise
 */
static bool wma_is_pkt_drop_candidate(tp_wma_handle wma_handle,
				      uint8_t *peer_addr, uint8_t *bssid,
				      uint8_t subtype)
{
	bool should_drop = false;
	uint8_t nan_addr[] = {0x50, 0x6F, 0x9A, 0x01, 0x00, 0x00};

	/* Drop the beacons from NAN device */
	if ((subtype == MGMT_SUBTYPE_BEACON) &&
		(!qdf_mem_cmp(nan_addr, bssid, NAN_CLUSTER_ID_BYTES))) {
			should_drop = true;
			goto end;
	}
end:
	return should_drop;
}

#define RATE_LIMIT 16

int wma_form_rx_packet(qdf_nbuf_t buf,
			struct mgmt_rx_event_params *mgmt_rx_params,
			cds_pkt_t *rx_pkt)
{
	uint8_t vdev_id = WMA_INVALID_VDEV_ID;
	struct ieee80211_frame *wh;
	uint8_t mgt_type, mgt_subtype;
	int status;
	tp_wma_handle wma_handle = (tp_wma_handle)
				cds_get_context(QDF_MODULE_ID_WMA);
#if !defined(REMOVE_PKT_LOG)
	ol_txrx_pktdump_cb packetdump_cb;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
#endif
	static uint8_t limit_prints_invalid_len = RATE_LIMIT - 1;
	static uint8_t limit_prints_load_unload = RATE_LIMIT - 1;
	static uint8_t limit_prints_recovery = RATE_LIMIT - 1;

	if (!wma_handle) {
		wma_err("wma handle is NULL");
		qdf_nbuf_free(buf);
		qdf_mem_free(rx_pkt);
		return -EINVAL;
	}

	if (!mgmt_rx_params) {
		limit_prints_invalid_len++;
		if (limit_prints_invalid_len == RATE_LIMIT) {
			wma_debug("mgmt rx params is NULL");
			limit_prints_invalid_len = 0;
		}
		qdf_nbuf_free(buf);
		qdf_mem_free(rx_pkt);
		return -EINVAL;
	}

	if (cds_is_load_or_unload_in_progress()) {
		limit_prints_load_unload++;
		if (limit_prints_load_unload == RATE_LIMIT) {
			wma_debug("Load/Unload in progress");
			limit_prints_load_unload = 0;
		}
		qdf_nbuf_free(buf);
		qdf_mem_free(rx_pkt);
		return -EINVAL;
	}

	if (cds_is_driver_recovering()) {
		limit_prints_recovery++;
		if (limit_prints_recovery == RATE_LIMIT) {
			wma_debug("Recovery in progress");
			limit_prints_recovery = 0;
		}
		qdf_nbuf_free(buf);
		qdf_mem_free(rx_pkt);
		return -EINVAL;
	}

	if (cds_is_driver_in_bad_state()) {
		limit_prints_recovery++;
		if (limit_prints_recovery == RATE_LIMIT) {
			wma_debug("Driver in bad state");
			limit_prints_recovery = 0;
		}
		qdf_nbuf_free(buf);
		qdf_mem_free(rx_pkt);
		return -EINVAL;
	}

	/*
	 * Fill in meta information needed by pe/lim
	 * TODO: Try to maintain rx metainfo as part of skb->data.
	 */
	rx_pkt->pkt_meta.frequency = mgmt_rx_params->chan_freq;
	rx_pkt->pkt_meta.scan_src = mgmt_rx_params->flags;

	/*
	 * Get the rssi value from the current snr value
	 * using standard noise floor of -96.
	 */
	rx_pkt->pkt_meta.rssi = mgmt_rx_params->snr +
				WMA_NOISE_FLOOR_DBM_DEFAULT;
	rx_pkt->pkt_meta.snr = mgmt_rx_params->snr;

	/* If absolute rssi is available from firmware, use it */
	if (mgmt_rx_params->rssi != 0)
		rx_pkt->pkt_meta.rssi_raw = mgmt_rx_params->rssi;
	else
		rx_pkt->pkt_meta.rssi_raw = rx_pkt->pkt_meta.rssi;


	/*
	 * FIXME: Assigning the local timestamp as hw timestamp is not
	 * available. Need to see if pe/lim really uses this data.
	 */
	rx_pkt->pkt_meta.timestamp = (uint32_t) jiffies;
	rx_pkt->pkt_meta.mpdu_hdr_len = sizeof(struct ieee80211_frame);
	rx_pkt->pkt_meta.mpdu_len = mgmt_rx_params->buf_len;

	/*
	 * The buf_len should be at least 802.11 header len
	 */
	if (mgmt_rx_params->buf_len < rx_pkt->pkt_meta.mpdu_hdr_len) {
		wma_err("MPDU Len %d lesser than header len %d",
			 mgmt_rx_params->buf_len,
			 rx_pkt->pkt_meta.mpdu_hdr_len);
		qdf_nbuf_free(buf);
		qdf_mem_free(rx_pkt);
		return -EINVAL;
	}

	rx_pkt->pkt_meta.mpdu_data_len = mgmt_rx_params->buf_len -
					 rx_pkt->pkt_meta.mpdu_hdr_len;

	rx_pkt->pkt_meta.roamCandidateInd = 0;

	wh = (struct ieee80211_frame *)qdf_nbuf_data(buf);

	/*
	 * If the mpdu_data_len is greater than Max (2k), drop the frame
	 */
	if (rx_pkt->pkt_meta.mpdu_data_len > MAX_MGMT_MPDU_LEN) {
		wma_err("Data Len %d greater than max, dropping frame from "QDF_MAC_ADDR_FMT,
			 rx_pkt->pkt_meta.mpdu_data_len,
			 QDF_MAC_ADDR_REF(wh->i_addr3));
		qdf_nbuf_free(buf);
		qdf_mem_free(rx_pkt);
		return -EINVAL;
	}

	rx_pkt->pkt_meta.mpdu_hdr_ptr = qdf_nbuf_data(buf);
	rx_pkt->pkt_meta.mpdu_data_ptr = rx_pkt->pkt_meta.mpdu_hdr_ptr +
					 rx_pkt->pkt_meta.mpdu_hdr_len;
	rx_pkt->pkt_meta.tsf_delta = mgmt_rx_params->tsf_delta;
	rx_pkt->pkt_buf = buf;

	/* If it is a beacon/probe response, save it for future use */
	mgt_type = (wh)->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	mgt_subtype = (wh)->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

	if (mgt_type == IEEE80211_FC0_TYPE_MGT &&
	    (mgt_subtype == MGMT_SUBTYPE_DISASSOC ||
	     mgt_subtype == MGMT_SUBTYPE_DEAUTH ||
	     mgt_subtype == MGMT_SUBTYPE_ACTION)) {
		if (wma_find_vdev_id_by_bssid(wma_handle, wh->i_addr3,
					      &vdev_id) == QDF_STATUS_SUCCESS) {
			status = wma_check_and_process_rmf_frame(wma_handle,
								 vdev_id,
								 &wh,
								 rx_pkt,
								 buf);
			if (status)
				return status;
		} else if (wma_find_vdev_id_by_addr(wma_handle, wh->i_addr1,
					      &vdev_id) == QDF_STATUS_SUCCESS) {
			status = wma_check_and_process_rmf_frame(wma_handle,
								 vdev_id,
								 &wh,
								 rx_pkt,
								 buf);
			if (status)
				return status;
		}
	}

	rx_pkt->pkt_meta.session_id =
		(vdev_id == WMA_INVALID_VDEV_ID ? 0 : vdev_id);

	if (mgt_type == IEEE80211_FC0_TYPE_MGT &&
	    (mgt_subtype == MGMT_SUBTYPE_BEACON ||
	     mgt_subtype == MGMT_SUBTYPE_PROBE_RESP)) {
		if (mgmt_rx_params->buf_len <=
			(sizeof(struct ieee80211_frame) +
			offsetof(struct wlan_bcn_frame, ie))) {
			wma_debug("Dropping frame from "QDF_MAC_ADDR_FMT,
				 QDF_MAC_ADDR_REF(wh->i_addr3));
			cds_pkt_return_packet(rx_pkt);
			return -EINVAL;
		}
	}

	if (wma_is_pkt_drop_candidate(wma_handle, wh->i_addr2, wh->i_addr3,
					mgt_subtype)) {
		cds_pkt_return_packet(rx_pkt);
		return -EINVAL;
	}

#if !defined(REMOVE_PKT_LOG)
	packetdump_cb = wma_handle->wma_mgmt_rx_packetdump_cb;
	if ((mgt_type == IEEE80211_FC0_TYPE_MGT &&
		mgt_subtype != MGMT_SUBTYPE_BEACON) &&
		packetdump_cb)
		packetdump_cb(soc, mgmt_rx_params->pdev_id,
			      rx_pkt->pkt_meta.session_id, rx_pkt->pkt_buf,
			      QDF_STATUS_SUCCESS, RX_MGMT_PKT);
#endif
	return 0;
}

/**
 * wma_mem_endianness_based_copy() - does memory copy from src to dst
 * @dst: destination address
 * @src: source address
 * @size: size to be copied
 *
 * This function copies the memory of size passed from source
 * address to destination address.
 *
 * Return: Nothing
 */
#ifdef BIG_ENDIAN_HOST
static void wma_mem_endianness_based_copy(
			uint8_t *dst, uint8_t *src, uint32_t size)
{
	/*
	 * For big endian host, copy engine byte_swap is enabled
	 * But the rx mgmt frame buffer content is in network byte order
	 * Need to byte swap the mgmt frame buffer content - so when
	 * copy engine does byte_swap - host gets buffer content in the
	 * correct byte order.
	 */

	uint32_t i;
	uint32_t *destp, *srcp;

	destp = (uint32_t *) dst;
	srcp = (uint32_t *) src;
	for (i = 0; i < (roundup(size, sizeof(uint32_t)) / 4); i++) {
		*destp = cpu_to_le32(*srcp);
		destp++;
		srcp++;
	}
}
#else
static void wma_mem_endianness_based_copy(
			uint8_t *dst, uint8_t *src, uint32_t size)
{
	qdf_mem_copy(dst, src, size);
}
#endif

#define RESERVE_BYTES                   100
/**
 * wma_mgmt_rx_process() - process management rx frame.
 * @handle: wma handle
 * @data: rx data
 * @data_len: data length
 *
 * Return: 0 for success or error code
 */
static int wma_mgmt_rx_process(void *handle, uint8_t *data,
				  uint32_t data_len)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	struct mgmt_rx_event_params *mgmt_rx_params;
	struct wlan_objmgr_psoc *psoc;
	uint8_t *bufp;
	qdf_nbuf_t wbuf;
	QDF_STATUS status;

	if (!wma_handle) {
		wma_err("Failed to get WMA  context");
		return -EINVAL;
	}

	mgmt_rx_params = qdf_mem_malloc(sizeof(*mgmt_rx_params));
	if (!mgmt_rx_params) {
		wma_err("memory allocation failed");
		return -ENOMEM;
	}

	if (wmi_extract_mgmt_rx_params(wma_handle->wmi_handle,
			data, mgmt_rx_params, &bufp) != QDF_STATUS_SUCCESS) {
		wma_err("Extraction of mgmt rx params failed");
		qdf_mem_free(mgmt_rx_params);
		return -EINVAL;
	}

	if (mgmt_rx_params->buf_len > data_len ||
	    !mgmt_rx_params->buf_len ||
	    !bufp) {
		wma_err("Invalid data_len %u, buf_len %u bufp %pK",
			data_len, mgmt_rx_params->buf_len, bufp);
		qdf_mem_free(mgmt_rx_params);
		return -EINVAL;
	}

	if (!mgmt_rx_params->chan_freq) {
		/*
		 * It indicates that FW is legacy and is operating on
		 * channel numbers and it also indicates that BAND_6G support
		 * is not there as BAND_6G works only on frequencies and channel
		 * numbers can be treated as unique.
		 */
		mgmt_rx_params->chan_freq = wlan_reg_legacy_chan_to_freq(
					    wma_handle->pdev,
					    mgmt_rx_params->channel);
	}

	mgmt_rx_params->pdev_id = 0;
	mgmt_rx_params->rx_params = NULL;

	/*
	 * Allocate the memory for this rx packet, add extra 100 bytes for:-
	 *
	 * 1.  Filling the missing RSN capabilites by some APs, which fill the
	 *     RSN IE length as extra 2 bytes but dont fill the IE data with
	 *     capabilities, resulting in failure in unpack core due to length
	 *     mismatch. Check sir_validate_and_rectify_ies for more info.
	 *
	 * 2.  In the API wma_process_rmf_frame(), the driver trims the CCMP
	 *     header by overwriting the IEEE header to memory occupied by CCMP
	 *     header, but an overflow is possible if the memory allocated to
	 *     frame is less than the sizeof(struct ieee80211_frame) +CCMP
	 *     HEADER len, so allocating 100 bytes would solve this issue too.
	 *
	 * 3.  CCMP header is pointing to orig_hdr +
	 *     sizeof(struct ieee80211_frame) which could also result in OOB
	 *     access, if the data len is less than
	 *     sizeof(struct ieee80211_frame), allocating extra bytes would
	 *     result in solving this issue too.
	 */
	wbuf = qdf_nbuf_alloc(NULL, roundup(mgmt_rx_params->buf_len +
							RESERVE_BYTES,
							4), 0, 4, false);
	if (!wbuf) {
		qdf_mem_free(mgmt_rx_params);
		return -ENOMEM;
	}

	qdf_nbuf_put_tail(wbuf, mgmt_rx_params->buf_len);
	qdf_nbuf_set_protocol(wbuf, ETH_P_CONTROL);

	qdf_mem_zero(((uint8_t *)qdf_nbuf_data(wbuf) + mgmt_rx_params->buf_len),
		     (roundup(mgmt_rx_params->buf_len + RESERVE_BYTES, 4) -
		     mgmt_rx_params->buf_len));

	wma_mem_endianness_based_copy(qdf_nbuf_data(wbuf),
			bufp, mgmt_rx_params->buf_len);

	psoc = (struct wlan_objmgr_psoc *)
				wma_handle->psoc;
	if (!psoc) {
		wma_err("psoc ctx is NULL");
		qdf_nbuf_free(wbuf);
		qdf_mem_free(mgmt_rx_params);
		return -EINVAL;
	}

	status = mgmt_txrx_rx_handler(psoc, wbuf, mgmt_rx_params);
	if (status != QDF_STATUS_SUCCESS) {
		qdf_mem_free(mgmt_rx_params);
		return -EINVAL;
	}

	qdf_mem_free(mgmt_rx_params);
	return 0;
}

/**
 * wma_de_register_mgmt_frm_client() - deregister management frame
 *
 * This function deregisters the event handler registered for
 * WMI_MGMT_RX_EVENTID.
 *
 * Return: QDF status
 */
QDF_STATUS wma_de_register_mgmt_frm_client(void)
{
	tp_wma_handle wma_handle = (tp_wma_handle)
				cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma_handle) {
		wma_err("Failed to get WMA context");
		return QDF_STATUS_E_NULL_VALUE;
	}

#ifdef QCA_WIFI_FTM
	if (cds_get_conparam() == QDF_GLOBAL_FTM_MODE)
		return QDF_STATUS_SUCCESS;
#endif

	if (wmi_unified_unregister_event_handler(wma_handle->wmi_handle,
						 wmi_mgmt_rx_event_id) != 0) {
		wma_err("Failed to Unregister rx mgmt handler with wmi");
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * wma_register_roaming_callbacks() - Register roaming callbacks
 * @csr_roam_synch_cb: CSR roam synch callback routine pointer
 * @pe_roam_synch_cb: PE roam synch callback routine pointer
 * @csr_roam_auth_event_handle_cb: CSR callback routine pointer
 * @csr_roam_pmkid_req_cb: CSR roam pmkid callback routine pointer
 *
 * Register the SME and PE callback routines with WMA for
 * handling roaming
 *
 * Return: Success or Failure Status
 */
QDF_STATUS wma_register_roaming_callbacks(
	csr_roam_synch_fn_t csr_roam_synch_cb,
	QDF_STATUS (*csr_roam_auth_event_handle_cb)(struct mac_context *mac,
						    uint8_t vdev_id,
						    struct qdf_mac_addr bssid),
	pe_roam_synch_fn_t pe_roam_synch_cb,
	QDF_STATUS (*pe_disconnect_cb) (struct mac_context *mac,
					uint8_t vdev_id,
					uint8_t *deauth_disassoc_frame,
					uint16_t deauth_disassoc_frame_len,
					uint16_t reason_code),
	csr_roam_pmkid_req_fn_t csr_roam_pmkid_req_cb)
{

	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma) {
		wma_err("Failed to get WMA context");
		return QDF_STATUS_E_FAILURE;
	}
	wma->csr_roam_synch_cb = csr_roam_synch_cb;
	wma->csr_roam_auth_event_handle_cb = csr_roam_auth_event_handle_cb;
	wma->pe_roam_synch_cb = pe_roam_synch_cb;
	wma->pe_disconnect_cb = pe_disconnect_cb;
	wma_debug("Registered roam synch callbacks with WMA successfully");

	wma->csr_roam_pmkid_req_cb = csr_roam_pmkid_req_cb;
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * wma_register_mgmt_frm_client() - register management frame callback
 *
 * This function registers event handler for WMI_MGMT_RX_EVENTID.
 *
 * Return: QDF status
 */
QDF_STATUS wma_register_mgmt_frm_client(void)
{
	tp_wma_handle wma_handle = (tp_wma_handle)
				cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma_handle) {
		wma_err("Failed to get WMA context");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (wmi_unified_register_event_handler(wma_handle->wmi_handle,
					       wmi_mgmt_rx_event_id,
					       wma_mgmt_rx_process,
					       WMA_RX_WORK_CTX) != 0) {
		wma_err("Failed to register rx mgmt handler with wmi");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_register_packetdump_callback() - stores tx and rx mgmt packet dump
 *   callback handler
 * @tx_cb: tx mgmt packetdump cb
 * @rx_cb: rx mgmt packetdump cb
 *
 * This function is used to store tx and rx mgmt. packet dump callback
 *
 * Return: None
 *
 */
void wma_register_packetdump_callback(
	ol_txrx_pktdump_cb tx_cb,
	ol_txrx_pktdump_cb rx_cb)
{
	tp_wma_handle wma_handle = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma_handle) {
		wma_err("wma handle is NULL");
		return;
	}

	wma_handle->wma_mgmt_tx_packetdump_cb = tx_cb;
	wma_handle->wma_mgmt_rx_packetdump_cb = rx_cb;
}

/**
 * wma_deregister_packetdump_callback() - removes tx and rx mgmt packet dump
 *   callback handler
 *
 * This function is used to remove tx and rx mgmt. packet dump callback
 *
 * Return: None
 *
 */
void wma_deregister_packetdump_callback(void)
{
	tp_wma_handle wma_handle = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma_handle) {
		wma_err("wma handle is NULL");
		return;
	}

	wma_handle->wma_mgmt_tx_packetdump_cb = NULL;
	wma_handle->wma_mgmt_rx_packetdump_cb = NULL;
}

QDF_STATUS wma_mgmt_unified_cmd_send(struct wlan_objmgr_vdev *vdev,
				qdf_nbuf_t buf, uint32_t desc_id,
				void *mgmt_tx_params)
{
	tp_wma_handle wma_handle;
	int ret;
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	struct wmi_mgmt_params *mgmt_params =
			(struct wmi_mgmt_params *)mgmt_tx_params;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	if (!mgmt_params) {
		wma_err("mgmt_params ptr passed is NULL");
		return QDF_STATUS_E_INVAL;
	}
	mgmt_params->desc_id = desc_id;

	if (!vdev) {
		wma_err("vdev ptr passed is NULL");
		return QDF_STATUS_E_INVAL;
	}

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		wma_err("wma handle is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (wmi_service_enabled(wma_handle->wmi_handle,
				   wmi_service_mgmt_tx_wmi)) {
		status = wmi_mgmt_unified_cmd_send(wma_handle->wmi_handle,
						   mgmt_params);
	} else {
		QDF_NBUF_CB_MGMT_TXRX_DESC_ID(buf)
						= mgmt_params->desc_id;

		ret = cdp_mgmt_send_ext(soc, mgmt_params->vdev_id, buf,
					mgmt_params->tx_type,
					mgmt_params->use_6mbps,
					mgmt_params->chanfreq);
		status = qdf_status_from_os_return(ret);
	}

	if (status != QDF_STATUS_SUCCESS) {
		wma_err("mgmt tx failed");
		return status;
	}

	return QDF_STATUS_SUCCESS;
}

#ifndef CONFIG_HL_SUPPORT
void wma_mgmt_nbuf_unmap_cb(struct wlan_objmgr_pdev *pdev,
			    qdf_nbuf_t buf)
{
	struct wlan_objmgr_psoc *psoc;
	qdf_device_t dev;

	if (!buf)
		return;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		wma_err("Psoc handle NULL");
		return;
	}

	dev = wlan_psoc_get_qdf_dev(psoc);
	qdf_nbuf_unmap_single(dev, buf, QDF_DMA_TO_DEVICE);
}

QDF_STATUS wma_mgmt_frame_fill_peer_cb(struct wlan_objmgr_peer *peer,
				       qdf_nbuf_t buf)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;

	psoc = wlan_peer_get_psoc(peer);
	if (!psoc) {
		wma_err("Psoc handle NULL");
		return QDF_STATUS_E_INVAL;
	}

	pdev = wlan_objmgr_get_pdev_by_id((struct wlan_objmgr_psoc *)psoc,
					  wlan_peer_get_pdev_id(peer),
					  WLAN_LEGACY_WMA_ID);
	if (!pdev) {
		wma_err("Pdev handle NULL");
		return QDF_STATUS_E_INVAL;
	}
	wma_mgmt_nbuf_unmap_cb(pdev, buf);
	wlan_objmgr_pdev_release_ref(pdev, WLAN_LEGACY_WMA_ID);

	return QDF_STATUS_SUCCESS;
}
#endif
