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

/*
 * This file lim ProcessMessageQueue.cc contains the code
 * for processing LIM message Queue.
 * Author:        Chandra Modumudi
 * Date:          02/11/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#include "cds_api.h"
#include "wni_api.h"
#include "wma_types.h"

#include "wni_cfg.h"
#include "sir_common.h"
#include "utils_api.h"
#include "lim_types.h"
#include "lim_utils.h"
#include "lim_assoc_utils.h"
#include "lim_prop_exts_utils.h"

#include "lim_admit_control.h"
#include "sch_api.h"
#include "lim_ft_defs.h"
#include "lim_session.h"
#include "lim_send_messages.h"

#include "rrm_api.h"

#include "lim_ft.h"

#include "qdf_types.h"
#include "cds_packet.h"
#include "qdf_mem.h"
#include "wlan_policy_mgr_api.h"
#include "nan_datapath.h"
#include "wlan_reg_services_api.h"
#include "lim_security_utils.h"
#include "cds_ieee80211_common.h"
#include <wlan_scan_ucfg_api.h>
#include "wlan_mlme_public_struct.h"
#include "wma.h"
#include "wma_internal.h"
#include "../../core/src/vdev_mgr_ops.h"
#include "wlan_p2p_cfg_api.h"

void lim_log_session_states(struct mac_context *mac);
static void lim_process_normal_hdd_msg(struct mac_context *mac_ctx,
	struct scheduler_msg *msg, uint8_t rsp_reqd);

#ifdef WLAN_FEATURE_SAE

/**
 * lim_process_sae_msg_sta() - Process SAE message for STA
 * @mac: Global MAC pointer
 * @session: Pointer to the PE session entry
 * @sae_msg: SAE message buffer pointer
 *
 * Return: None
 */
static void lim_process_sae_msg_sta(struct mac_context *mac,
				    struct pe_session *session,
				    struct sir_sae_msg *sae_msg)
{
	switch (session->limMlmState) {
	case eLIM_MLM_WT_SAE_AUTH_STATE:
		/* SAE authentication is completed.
		 * Restore from auth state
		 */
		if (tx_timer_running(&mac->lim.lim_timers.sae_auth_timer))
			lim_deactivate_and_change_timer(mac,
							eLIM_AUTH_SAE_TIMER);
		lim_sae_auth_cleanup_retry(mac, session->vdev_id);
		/* success */
		if (sae_msg->sae_status == IEEE80211_STATUS_SUCCESS)
			lim_restore_from_auth_state(mac,
						    eSIR_SME_SUCCESS,
						    STATUS_SUCCESS,
						    session);
		else
			lim_restore_from_auth_state(mac, eSIR_SME_AUTH_REFUSED,
				STATUS_UNSPECIFIED_FAILURE, session);
		break;
	default:
		/* SAE msg is received in unexpected state */
		pe_err("received SAE msg in state %X", session->limMlmState);
		lim_print_mlm_state(mac, LOGE, session->limMlmState);
		break;
	}
}

/**
 * lim_process_sae_msg_ap() - Process SAE message
 * @mac: Global MAC pointer
 * @session: Pointer to the PE session entry
 * @sae_msg: SAE message buffer pointer
 *
 * Return: None
 */
static void lim_process_sae_msg_ap(struct mac_context *mac,
				   struct pe_session *session,
				   struct sir_sae_msg *sae_msg)
{
	struct tLimPreAuthNode *sta_pre_auth_ctx;
	struct lim_assoc_data *assoc_req;
	bool assoc_ind_sent;

	/* Extract pre-auth context for the STA and move limMlmState
	 * of preauth node to eLIM_MLM_AUTHENTICATED_STATE
	 */
	sta_pre_auth_ctx = lim_search_pre_auth_list(mac,
						    sae_msg->peer_mac_addr);

	if (!sta_pre_auth_ctx) {
		pe_debug("No preauth node created for "
			 QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(sae_msg->peer_mac_addr));
		return;
	}

	assoc_req = &sta_pre_auth_ctx->assoc_req;

	if (sae_msg->sae_status != IEEE80211_STATUS_SUCCESS) {
		pe_debug("SAE authentication failed for "
			 QDF_MAC_ADDR_FMT " status: %u",
			 QDF_MAC_ADDR_REF(sae_msg->peer_mac_addr),
			 sae_msg->sae_status);
		if (assoc_req->present) {
			pe_debug("Assoc req cached; clean it up");
			lim_process_assoc_cleanup(mac, session,
						  assoc_req->assoc_req,
						  assoc_req->sta_ds,
						  assoc_req->assoc_req_copied);
			assoc_req->present = false;
		}
		lim_delete_pre_auth_node(mac, sae_msg->peer_mac_addr);
		return;
	}
	sta_pre_auth_ctx->mlmState = eLIM_MLM_AUTHENTICATED_STATE;
	/* Send assoc indication to SME if any assoc request is cached*/
	if (assoc_req->present) {
		/* Assoc request is present in preauth context. Get the assoc
		 * request and make it invalid in preauth context. It'll be
		 * freed later in the legacy path.
		 */
		bool assoc_req_copied;

		assoc_req->present = false;
		pe_debug("Assoc req cached; handle it");
		assoc_ind_sent =
			lim_send_assoc_ind_to_sme(mac, session,
						  assoc_req->sub_type,
						  &assoc_req->hdr,
						  assoc_req->assoc_req,
						  ANI_AKM_TYPE_SAE,
						  assoc_req->pmf_connection,
						  &assoc_req_copied,
						  assoc_req->dup_entry, false);
		if (!assoc_ind_sent)
			lim_process_assoc_cleanup(mac, session,
						  assoc_req->assoc_req,
						  assoc_req->sta_ds,
						  assoc_req_copied);
	}
}

/**
 * lim_process_sae_msg() - Process SAE message
 * @mac: Global MAC pointer
 * @body: Buffer pointer
 *
 * Return: None
 */
static void lim_process_sae_msg(struct mac_context *mac, struct sir_sae_msg *body)
{
	struct sir_sae_msg *sae_msg = body;
	struct pe_session *session;

	if (!sae_msg) {
		pe_err("SAE msg is NULL");
		return;
	}

	session = pe_find_session_by_vdev_id(mac, sae_msg->vdev_id);
	if (!session) {
		pe_err("SAE:Unable to find session");
		return;
	}

	if (session->opmode != QDF_STA_MODE &&
	    session->opmode != QDF_SAP_MODE &&
	    session->opmode != QDF_P2P_GO_MODE &&
	    session->opmode != QDF_P2P_CLIENT_MODE) {
		pe_err("SAE:Not supported in this mode %d",
				session->opmode);
		return;
	}

	pe_debug("SAE:status %d limMlmState %d opmode %d peer: "
		 QDF_MAC_ADDR_FMT, sae_msg->sae_status,
		 session->limMlmState, session->opmode,
		 QDF_MAC_ADDR_REF(sae_msg->peer_mac_addr));
	if (LIM_IS_STA_ROLE(session))
		lim_process_sae_msg_sta(mac, session, sae_msg);
	else if (LIM_IS_AP_ROLE(session))
		lim_process_sae_msg_ap(mac, session, sae_msg);
	else
		pe_debug("SAE message on unsupported interface");
}
#else
static inline void lim_process_sae_msg(struct mac_context *mac, void *body)
{}
#endif

/**
 * lim_process_dual_mac_cfg_resp() - Process set dual mac config response
 * @mac: Global MAC pointer
 * @body: Set dual mac config response in sir_dual_mac_config_resp format
 *
 * Process the set dual mac config response and post the message
 * to SME to process this further and release the active
 * command list
 *
 * Return: None
 */
static void lim_process_dual_mac_cfg_resp(struct mac_context *mac, void *body)
{
	struct sir_dual_mac_config_resp *resp, *param;
	uint32_t len, fail_resp = 0;
	struct scheduler_msg msg = {0};

	resp = (struct sir_dual_mac_config_resp *)body;
	if (!resp) {
		pe_err("Set dual mac cfg param is NULL");
		fail_resp = 1;
		/* Not returning here. If possible, let us proceed
		 * and send fail response to SME
		 */
	}

	len = sizeof(*param);

	param = qdf_mem_malloc(len);
	if (!param)
		return;

	if (fail_resp) {
		pe_err("Send fail status to SME");
		param->status = SET_HW_MODE_STATUS_ECANCELED;
	} else {
		param->status = resp->status;
		/*
		 * TODO: Update this HW mode info in any UMAC params, if needed
		 */
	}

	msg.type = eWNI_SME_SET_DUAL_MAC_CFG_RESP;
	msg.bodyptr = param;
	msg.bodyval = 0;
	pe_debug("Send eWNI_SME_SET_DUAL_MAC_CFG_RESP to SME");
	lim_sys_process_mmh_msg_api(mac, &msg);
	return;
}

/**
 * lim_process_set_hw_mode_resp() - Process set HW mode response
 * @mac: Global MAC pointer
 * @body: Set HW mode response in sir_set_hw_mode_resp format
 *
 * Process the set HW mode response and post the message
 * to SME to process this further and release the active
 * command list
 *
 * Return: None
 */
static void lim_process_set_hw_mode_resp(struct mac_context *mac, void *body)
{
	struct sir_set_hw_mode_resp *resp, *param;
	uint32_t len, i, fail_resp = 0;
	struct scheduler_msg msg = {0};

	resp = (struct sir_set_hw_mode_resp *)body;
	if (!resp) {
		pe_err("Set HW mode param is NULL");
		fail_resp = 1;
		/* Not returning here. If possible, let us proceed
		 * and send fail response to SME */
	}

	len = sizeof(*param);

	param = qdf_mem_malloc(len);
	if (!param)
		return;

	if (fail_resp) {
		pe_err("Send fail status to SME");
		param->status = SET_HW_MODE_STATUS_ECANCELED;
		param->cfgd_hw_mode_index = 0;
		param->num_vdev_mac_entries = 0;
	} else {
		param->status = resp->status;
		param->cfgd_hw_mode_index = resp->cfgd_hw_mode_index;
		param->num_vdev_mac_entries = resp->num_vdev_mac_entries;

		for (i = 0; i < resp->num_vdev_mac_entries; i++) {
			param->vdev_mac_map[i].vdev_id =
				resp->vdev_mac_map[i].vdev_id;
			param->vdev_mac_map[i].mac_id =
				resp->vdev_mac_map[i].mac_id;
		}
		/*
		 * TODO: Update this HW mode info in any UMAC params, if needed
		 */
	}

	msg.type = eWNI_SME_SET_HW_MODE_RESP;
	msg.bodyptr = param;
	msg.bodyval = 0;
	pe_debug("Send eWNI_SME_SET_HW_MODE_RESP to SME");
	lim_sys_process_mmh_msg_api(mac, &msg);
	return;
}

/**
 * lim_process_antenna_mode_resp() - Process set antenna mode
 * response
 * @mac: Global MAC pointer
 * @body: Set antenna mode response in sir_antenna_mode_resp
 * format
 *
 * Process the set antenna mode response and post the message
 * to SME to process this further and release the active
 * command list
 *
 * Return: None
 */
static void lim_process_set_antenna_resp(struct mac_context *mac, void *body)
{
	struct sir_antenna_mode_resp *resp, *param;
	bool fail_resp = false;
	struct scheduler_msg msg = {0};

	resp = (struct sir_antenna_mode_resp *)body;
	if (!resp) {
		pe_err("Set antenna mode resp is NULL");
		fail_resp = true;
		/* Not returning here. If possible, let us proceed
		 * and send fail response to SME
		 */
	}

	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return;

	if (fail_resp) {
		pe_err("Send fail status to SME");
		param->status = SET_ANTENNA_MODE_STATUS_ECANCELED;
	} else {
		param->status = resp->status;
	}

	msg.type = eWNI_SME_SET_ANTENNA_MODE_RESP;
	msg.bodyptr = param;
	msg.bodyval = 0;
	pe_debug("Send eWNI_SME_SET_ANTENNA_MODE_RESP to SME");
	lim_sys_process_mmh_msg_api(mac, &msg);
	return;
}

/**
 * lim_process_set_default_scan_ie_request() - Process the Set default
 * Scan IE request from HDD.
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: Pointer to incoming data
 *
 * This function receives the default scan IEs and updates the ext cap IE
 * (if present) with FTM capabilities and pass the Scan IEs to WMA.
 *
 * Return: None
 */
static void lim_process_set_default_scan_ie_request(struct mac_context *mac_ctx,
							uint32_t *msg_buf)
{
	struct hdd_default_scan_ie *set_ie_params;
	struct vdev_ie_info *wma_ie_params;
	uint8_t *local_ie_buf;
	uint16_t local_ie_len;
	struct scheduler_msg msg_q = {0};
	QDF_STATUS ret_code;
	struct pe_session *pe_session;

	if (!msg_buf) {
		pe_err("msg_buf is NULL");
		return;
	}

	set_ie_params = (struct hdd_default_scan_ie *) msg_buf;
	local_ie_len = set_ie_params->ie_len;

	local_ie_buf = qdf_mem_malloc(MAX_DEFAULT_SCAN_IE_LEN);
	if (!local_ie_buf)
		return;

	pe_session = pe_find_session_by_vdev_id(mac_ctx,
						set_ie_params->vdev_id);
	if (lim_update_ext_cap_ie(mac_ctx,
			(uint8_t *)set_ie_params->ie_data,
			local_ie_buf, &local_ie_len, pe_session)) {
		pe_err("Update ext cap IEs fails");
		goto scan_ie_send_fail;
	}

	wma_ie_params = qdf_mem_malloc(sizeof(*wma_ie_params) + local_ie_len);
	if (!wma_ie_params)
		goto scan_ie_send_fail;

	wma_ie_params->vdev_id = set_ie_params->vdev_id;
	wma_ie_params->ie_id = DEFAULT_SCAN_IE_ID;
	wma_ie_params->length = local_ie_len;
	wma_ie_params->data = (uint8_t *)(wma_ie_params)
					+ sizeof(*wma_ie_params);
	qdf_mem_copy(wma_ie_params->data, local_ie_buf, local_ie_len);

	msg_q.type = WMA_SET_IE_INFO;
	msg_q.bodyptr = wma_ie_params;
	msg_q.bodyval = 0;
	ret_code = wma_post_ctrl_msg(mac_ctx, &msg_q);
	if (QDF_STATUS_SUCCESS != ret_code) {
		pe_err("fail to send WMA_SET_IE_INFO");
		qdf_mem_free(wma_ie_params);
	}
scan_ie_send_fail:
	qdf_mem_free(local_ie_buf);
}

/**
 * lim_process_hw_mode_trans_ind() - Process set HW mode transition indication
 * @mac: Global MAC pointer
 * @body: Set HW mode response in sir_hw_mode_trans_ind format
 *
 * Process the set HW mode transition indication and post the message
 * to SME to invoke the HDD callback
 * command list
 *
 * Return: None
 */
static void lim_process_hw_mode_trans_ind(struct mac_context *mac, void *body)
{
	struct sir_hw_mode_trans_ind *ind, *param;
	uint32_t len, i;
	struct scheduler_msg msg = {0};

	ind = (struct sir_hw_mode_trans_ind *)body;
	if (!ind) {
		pe_err("Set HW mode trans ind param is NULL");
		return;
	}

	len = sizeof(*param);

	param = qdf_mem_malloc(len);
	if (!param)
		return;

	param->old_hw_mode_index = ind->old_hw_mode_index;
	param->new_hw_mode_index = ind->new_hw_mode_index;
	param->num_vdev_mac_entries = ind->num_vdev_mac_entries;

	for (i = 0; i < ind->num_vdev_mac_entries; i++) {
		param->vdev_mac_map[i].vdev_id =
			ind->vdev_mac_map[i].vdev_id;
		param->vdev_mac_map[i].mac_id =
			ind->vdev_mac_map[i].mac_id;
	}

	/* TODO: Update this HW mode info in any UMAC params, if needed */

	msg.type = eWNI_SME_HW_MODE_TRANS_IND;
	msg.bodyptr = param;
	msg.bodyval = 0;
	pe_err("Send eWNI_SME_HW_MODE_TRANS_IND to SME");
	lim_sys_process_mmh_msg_api(mac, &msg);
	return;
}

/**
 * def_msg_decision() - Should a message be deferred?
 * @mac_ctx: The global MAC context
 * @lim_msg: The message to check for potential deferral
 *
 * This function decides whether to defer a message or not in
 * lim_message_processor() function
 *
 * Return: true if the message can be deferred, false otherwise
 */
static bool def_msg_decision(struct mac_context *mac_ctx,
			     struct scheduler_msg *lim_msg)
{
	uint8_t type, subtype;
	QDF_STATUS status;
	bool mgmt_pkt_defer = true;

	/* this function should not changed */
	if (mac_ctx->lim.gLimSmeState == eLIM_SME_OFFLINE_STATE) {
		/* Defer processing this message */
		if (lim_defer_msg(mac_ctx, lim_msg) != TX_SUCCESS) {
			QDF_TRACE(QDF_MODULE_ID_PE, LOGE,
					FL("Unable to Defer Msg"));
			lim_log_session_states(mac_ctx);
			lim_handle_defer_msg_error(mac_ctx, lim_msg);
		}

		return true;
	}

	/*
	 * When defer is requested then defer all the messages except
	 * HAL responses.
	 */
	if (!GET_LIM_PROCESS_DEFD_MESGS(mac_ctx)) {
		if (lim_msg->type == SIR_BB_XPORT_MGMT_MSG) {
			/*
			 * Dont defer beacon and probe response
			 * because it will fill the differ queue quickly
			 */
			status = lim_util_get_type_subtype(lim_msg->bodyptr,
							   &type, &subtype);
			if (QDF_IS_STATUS_SUCCESS(status) &&
				(type == SIR_MAC_MGMT_FRAME) &&
				((subtype == SIR_MAC_MGMT_BEACON) ||
				 (subtype == SIR_MAC_MGMT_PROBE_RSP)))
				mgmt_pkt_defer = false;
		}

		if ((lim_msg->type != WMA_DELETE_BSS_RSP) &&
		    (lim_msg->type != WMA_DELETE_BSS_HO_FAIL_RSP) &&
		    (lim_msg->type != WMA_ADD_STA_RSP) &&
		    (lim_msg->type != WMA_DELETE_STA_RSP) &&
		    (lim_msg->type != WMA_SET_BSSKEY_RSP) &&
		    (lim_msg->type != WMA_SET_STAKEY_RSP) &&
		    (lim_msg->type != WMA_SET_STA_BCASTKEY_RSP) &&
		    (lim_msg->type != WMA_AGGR_QOS_RSP) &&
		    (lim_msg->type != WMA_SET_MIMOPS_RSP) &&
		    (lim_msg->type != WMA_SWITCH_CHANNEL_RSP) &&
		    (lim_msg->type != WMA_P2P_NOA_ATTR_IND) &&
		    (lim_msg->type != WMA_ADD_TS_RSP) &&
		    /*
		     * LIM won't process any defer queue commands if gLimAddtsSent is
		     * set to TRUE. gLimAddtsSent will be set TRUE to while sending
		     * ADDTS REQ. Say, when deferring is enabled, if
		     * SIR_LIM_ADDTS_RSP_TIMEOUT is posted (because of not receiving ADDTS
		     * RSP) then this command will be added to defer queue and as
		     * gLimAddtsSent is set TRUE LIM will never process any commands from
		     * defer queue, including SIR_LIM_ADDTS_RSP_TIMEOUT. Hence allowing
		     * SIR_LIM_ADDTS_RSP_TIMEOUT command to be processed with deferring
		     * enabled, so that this will be processed immediately and sets
		     * gLimAddtsSent to FALSE.
		     */
		    (lim_msg->type != SIR_LIM_ADDTS_RSP_TIMEOUT) &&
		    /* Allow processing of RX frames while awaiting reception
		     * of ADD TS response over the air. This logic particularly
		     * handles the case when host sends ADD BA request to FW
		     * after ADD TS request is sent over the air and
		     * ADD TS response received over the air */
		    !(lim_msg->type == SIR_BB_XPORT_MGMT_MSG &&
		    mac_ctx->lim.gLimAddtsSent) &&
		    (mgmt_pkt_defer)) {
			pe_debug("Defer the current message %s , gLimProcessDefdMsgs is false and system is not in scan/learn mode",
				 lim_msg_str(lim_msg->type));
			/* Defer processing this message */
			if (lim_defer_msg(mac_ctx, lim_msg) != TX_SUCCESS) {
				QDF_TRACE(QDF_MODULE_ID_PE, LOGE,
					  FL("Unable to Defer Msg"));
				lim_log_session_states(mac_ctx);
				lim_handle_defer_msg_error(mac_ctx, lim_msg);
			}
			return true;
		}
	}
	return false;
}

#ifdef FEATURE_WLAN_EXTSCAN
static void
__lim_pno_match_fwd_bcn_probepsp(struct mac_context *pmac, uint8_t *rx_pkt_info,
				tSirProbeRespBeacon *frame, uint32_t ie_len,
				uint32_t msg_type)
{
	struct pno_match_found  *result;
	uint8_t                 *body;
	struct scheduler_msg    mmh_msg = {0};
	tpSirMacMgmtHdr         hdr;
	uint32_t num_results = 1, len, i;

	/* Upon receiving every matched beacon, bss info is forwarded to the
	 * the upper layer, hence num_results is set to 1 */
	len = sizeof(*result) + (num_results * sizeof(tSirWifiScanResult)) +
		ie_len;

	result = qdf_mem_malloc(len);
	if (!result)
		return;

	hdr = WMA_GET_RX_MAC_HEADER(rx_pkt_info);
	body = WMA_GET_RX_MPDU_DATA(rx_pkt_info);

	/* Received frame does not have request id, hence set 0 */
	result->request_id = 0;
	result->more_data = 0;
	result->num_results = num_results;

	for (i = 0; i < result->num_results; i++) {
		result->ap[i].ts = qdf_mc_timer_get_system_time();
		result->ap[i].beaconPeriod = frame->beaconInterval;
		result->ap[i].capability =
			lim_get_u16((uint8_t *) &frame->capabilityInfo);
		result->ap[i].channel = wlan_reg_freq_to_chan(
						pmac->pdev,
						WMA_GET_RX_FREQ(rx_pkt_info));
		result->ap[i].rssi = WMA_GET_RX_RSSI_NORMALIZED(rx_pkt_info);
		result->ap[i].rtt = 0;
		result->ap[i].rtt_sd = 0;
		result->ap[i].ieLength = ie_len;
		qdf_mem_copy((uint8_t *) &result->ap[i].ssid[0],
			(uint8_t *) frame->ssId.ssId, frame->ssId.length);
		result->ap[i].ssid[frame->ssId.length] = '\0';
		qdf_mem_copy((uint8_t *) &result->ap[i].bssid,
				(uint8_t *) hdr->bssId,
				sizeof(tSirMacAddr));
		/* Copy IE fields */
		qdf_mem_copy((uint8_t *) &result->ap[i].ieData,
				body + SIR_MAC_B_PR_SSID_OFFSET, ie_len);
	}

	mmh_msg.type = msg_type;
	mmh_msg.bodyptr = result;
	mmh_msg.bodyval = 0;
	lim_sys_process_mmh_msg_api(pmac, &mmh_msg);
}


static void
__lim_ext_scan_forward_bcn_probe_rsp(struct mac_context *pmac, uint8_t *rx_pkt_info,
					tSirProbeRespBeacon *frame,
					uint32_t ie_len,
					uint32_t msg_type)
{
	tpSirWifiFullScanResultEvent result;
	uint8_t                     *body;
	struct scheduler_msg         mmh_msg = {0};
	tpSirMacMgmtHdr              hdr;
	uint32_t frame_len;
	struct bss_description *bssdescr;

	result = qdf_mem_malloc(sizeof(*result) + ie_len);
	if (!result)
		return;

	hdr = WMA_GET_RX_MAC_HEADER(rx_pkt_info);
	body = WMA_GET_RX_MPDU_DATA(rx_pkt_info);

	/* Received frame does not have request id, hence set 0 */
	result->requestId = 0;

	result->moreData = 0;
	result->ap.ts = qdf_mc_timer_get_system_time();
	result->ap.beaconPeriod = frame->beaconInterval;
	result->ap.capability =
			lim_get_u16((uint8_t *) &frame->capabilityInfo);
	result->ap.channel = wlan_reg_freq_to_chan(
						pmac->pdev,
						WMA_GET_RX_FREQ(rx_pkt_info));
	result->ap.rssi = WMA_GET_RX_RSSI_NORMALIZED(rx_pkt_info);
	result->ap.rtt = 0;
	result->ap.rtt_sd = 0;
	result->ap.ieLength = ie_len;

	qdf_mem_copy((uint8_t *) &result->ap.ssid[0],
			(uint8_t *) frame->ssId.ssId, frame->ssId.length);
	result->ap.ssid[frame->ssId.length] = '\0';
	qdf_mem_copy((uint8_t *) &result->ap.bssid.bytes,
			(uint8_t *) hdr->bssId,
			QDF_MAC_ADDR_SIZE);
	/* Copy IE fields */
	qdf_mem_copy((uint8_t *) &result->ap.ieData,
			body + SIR_MAC_B_PR_SSID_OFFSET, ie_len);

	frame_len = sizeof(*bssdescr) + ie_len - sizeof(bssdescr->ieFields[1]);
	bssdescr = qdf_mem_malloc(frame_len);
	if (!bssdescr) {
		qdf_mem_free(result);
		return;
	}

	qdf_mem_zero(bssdescr, frame_len);

	lim_collect_bss_description(pmac, bssdescr, frame, rx_pkt_info, false);

	qdf_mem_free(bssdescr);

	mmh_msg.type = msg_type;
	mmh_msg.bodyptr = result;
	mmh_msg.bodyval = 0;
	lim_sys_process_mmh_msg_api(pmac, &mmh_msg);
}

static void
__lim_process_ext_scan_beacon_probe_rsp(struct mac_context *pmac,
					uint8_t *rx_pkt_info,
					uint8_t sub_type)
{
	tSirProbeRespBeacon  *frame;
	uint8_t              *body;
	uint32_t             frm_len;
	QDF_STATUS        status;

	frm_len = WMA_GET_RX_PAYLOAD_LEN(rx_pkt_info);
	if (frm_len <= SIR_MAC_B_PR_SSID_OFFSET) {
		pe_err("RX packet has invalid length %d", frm_len);
		return;
	}

	frame = qdf_mem_malloc(sizeof(*frame));
	if (!frame)
		return;

	if (sub_type == SIR_MAC_MGMT_BEACON) {
		pe_debug("Beacon due to ExtScan/epno");
		status = sir_convert_beacon_frame2_struct(pmac,
						(uint8_t *)rx_pkt_info,
						frame);
	} else if (sub_type == SIR_MAC_MGMT_PROBE_RSP) {
		pe_debug("Probe Rsp due to ExtScan/epno");
		body = WMA_GET_RX_MPDU_DATA(rx_pkt_info);
		status = sir_convert_probe_frame2_struct(pmac, body,
							frm_len, frame);
	} else {
		qdf_mem_free(frame);
		return;
	}

	if (status != QDF_STATUS_SUCCESS) {
		pe_err("Frame parsing failed");
		qdf_mem_free(frame);
		return;
	}

	if (WMA_IS_EXTSCAN_SCAN_SRC(rx_pkt_info))
		__lim_ext_scan_forward_bcn_probe_rsp(pmac, rx_pkt_info, frame,
					(frm_len - SIR_MAC_B_PR_SSID_OFFSET),
					eWNI_SME_EXTSCAN_FULL_SCAN_RESULT_IND);

	if (WMA_IS_EPNO_SCAN_SRC(rx_pkt_info))
		__lim_pno_match_fwd_bcn_probepsp(pmac, rx_pkt_info, frame,
					(frm_len - SIR_MAC_B_PR_SSID_OFFSET),
					eWNI_SME_EPNO_NETWORK_FOUND_IND);

	qdf_mem_free(frame);
}
#endif

/*
 * Beacon Handling Cases:
 * during scanning, when no session is active:
 *    handled by lim_handle_frames_in_scan_state before __lim_handle_beacon call is invoked.
 * during scanning, when any session is active, but beacon/Pr does not belong to that session, pe_session will be null.
 *    handled by lim_handle_frames_in_scan_state before __lim_handle_beacon call is invoked.
 * during scanning, when any session is active, and beacon/Pr belongs to one of the session, pe_session will not be null.
 *    handled by lim_handle_frames_in_scan_state before __lim_handle_beacon call is invoked.
 * Not scanning, no session:
 *    there should not be any beacon coming, if coming, should be dropped.
 * Not Scanning,
 */
static void
__lim_handle_beacon(struct mac_context *mac, struct scheduler_msg *pMsg,
		    struct pe_session *pe_session)
{
	uint8_t *pRxPacketInfo;

	lim_get_b_dfrom_rx_packet(mac, pMsg->bodyptr,
				  (uint32_t **) &pRxPacketInfo);

	/* This function should not be called if beacon is received in scan state. */
	/* So not doing any checks for the global state. */

	if (!pe_session) {
		sch_beacon_process(mac, pRxPacketInfo, NULL);
	} else if ((pe_session->limSmeState == eLIM_SME_LINK_EST_STATE) ||
		   (pe_session->limSmeState == eLIM_SME_NORMAL_STATE)) {
		sch_beacon_process(mac, pRxPacketInfo, pe_session);
	} else
		lim_process_beacon_frame(mac, pRxPacketInfo, pe_session);

	return;
}

/*
 * lim_fill_sap_bcn_pkt_meta(): Fills essential fields in Rx Pkt Meta
 * @scan_entry: pointer to the scan cache entry of the beacon
 * @rx_pkt: pointer to the cds pkt allocated
 *
 * This API fills only the essential parameters in the Rx Pkt Meta which are
 * required while converting the beacon frame to struct and while handling
 * the beacon for implementation of SAP protection mechanisms.
 *
 * Return: None
 */
static void lim_fill_sap_bcn_pkt_meta(struct scan_cache_entry *scan_entry,
					cds_pkt_t *rx_pkt)
{
	rx_pkt->pkt_meta.frequency = scan_entry->channel.chan_freq;
	rx_pkt->pkt_meta.mpdu_hdr_len = sizeof(struct ieee80211_frame);
	rx_pkt->pkt_meta.mpdu_len = scan_entry->raw_frame.len;
	rx_pkt->pkt_meta.mpdu_data_len = rx_pkt->pkt_meta.mpdu_len -
					rx_pkt->pkt_meta.mpdu_hdr_len;

	rx_pkt->pkt_meta.mpdu_hdr_ptr = scan_entry->raw_frame.ptr;
	rx_pkt->pkt_meta.mpdu_data_ptr = rx_pkt->pkt_meta.mpdu_hdr_ptr +
					rx_pkt->pkt_meta.mpdu_hdr_len;

	/*
	 * The scan_entry->raw_frame contains the qdf_nbuf->data from the SKB
	 * of the beacon. We set the rx_pkt->pkt_meta.mpdu_hdr_ptr to point
	 * to this memory directly. However we do not have the pointer to
	 * the SKB itself here which is usually is pointed by rx_pkt->pkt_buf.
	 * Also, we always get the pkt data using WMA_GET_RX_MPDU_DATA and
	 * dont actually use the pkt_buf. So setting this to NULL.
	 */
	rx_pkt->pkt_buf = NULL;
}

/*
 * lim_allocate_and_get_bcn() - Allocate and get the bcn frame pkt and structure
 * @mac_ctx: pointer to global mac_ctx
 * @pkt: pointer to the pkt to be allocated
 * @rx_pkt_info: pointer to the allocated pkt meta
 * @bcn: pointer to the beacon struct
 * @scan_entry: pointer to the scan cache entry from scan module
 *
 * Allocates a cds_pkt for beacon frame in scan cache entry,
 * fills the essential pkt_meta elements and converts the
 * pkt to beacon strcut.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS lim_allocate_and_get_bcn(
				struct mac_context *mac_ctx,
				cds_pkt_t **pkt,
				uint8_t **rx_pkt_info,
				tSchBeaconStruct **bcn,
				struct scan_cache_entry *scan_entry)
{
	QDF_STATUS status;
	uint8_t *rx_pkt_info_l = NULL;
	tSchBeaconStruct *bcn_l = NULL;
	cds_pkt_t *pkt_l = NULL;

	pkt_l = qdf_mem_malloc_atomic(sizeof(cds_pkt_t));
	if (!pkt_l)
		return QDF_STATUS_E_FAILURE;

	status = wma_ds_peek_rx_packet_info(
		pkt_l, (void *)&rx_pkt_info_l, false);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		pe_err("Failed to get Rx Pkt meta");
		goto free;
	}

	bcn_l = qdf_mem_malloc_atomic(sizeof(tSchBeaconStruct));
	if (!bcn_l)
		goto free;

	lim_fill_sap_bcn_pkt_meta(scan_entry, pkt_l);

	/* Convert the beacon frame into a structure */
	if (sir_convert_beacon_frame2_struct(mac_ctx,
	    (uint8_t *)rx_pkt_info_l,
	    bcn_l) != QDF_STATUS_SUCCESS) {
		pe_err("beacon parsing failed");
		goto free;
	}

	*pkt = pkt_l;
	*bcn = bcn_l;
	*rx_pkt_info = rx_pkt_info_l;

	return QDF_STATUS_SUCCESS;

free:
	if (pkt_l) {
		qdf_mem_free(pkt_l);
		pkt_l = NULL;
	}

	if (bcn_l) {
		qdf_mem_free(bcn_l);
		bcn_l = NULL;
	}

	return QDF_STATUS_E_FAILURE;
}

void lim_handle_sap_beacon(struct wlan_objmgr_pdev *pdev,
				struct scan_cache_entry *scan_entry)
{
	struct mac_context *mac_ctx;
	cds_pkt_t *pkt = NULL;
	tSchBeaconStruct *bcn = NULL;
	struct mgmt_beacon_probe_filter *filter;
	QDF_STATUS status;
	uint8_t *rx_pkt_info = NULL;
	int session_id;

	if (!scan_entry) {
		pe_err("scan_entry is NULL");
		return;
	}

	if (scan_entry->frm_subtype != MGMT_SUBTYPE_BEACON)
		return;

	mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac_ctx) {
		pe_err("Failed to get mac_ctx");
		return;
	}

	filter = &mac_ctx->bcn_filter;

	if (!filter->num_sap_sessions) {
		return;
	}
	for (session_id = 0; session_id < mac_ctx->lim.maxBssId; session_id++) {
		if (filter->sap_channel[session_id] &&
		    (filter->sap_channel[session_id] ==
		    wlan_reg_freq_to_chan(pdev,
					  scan_entry->channel.chan_freq))) {
			if (!pkt) {
				status = lim_allocate_and_get_bcn(
					mac_ctx, &pkt, &rx_pkt_info,
					&bcn, scan_entry);
				if (!QDF_IS_STATUS_SUCCESS(status)) {
					pe_debug("lim_allocate_and_get_bcn fail!");
					return;
				}
			}
			sch_beacon_process_for_ap(mac_ctx, session_id,
						  rx_pkt_info, bcn);
		}
	}

	/*
	 * Free only the pkt memory we allocated and not the pkt->pkt_buf.
	 * The actual SKB buffer is freed in the scan module from where
	 * this API is invoked via callback
	 */
	if (bcn)
		qdf_mem_free(bcn);
	if (pkt)
		qdf_mem_free(pkt);
}

/**
 * lim_defer_msg()
 *
 ***FUNCTION:
 * This function is called to defer the messages received
 * during Learn mode
 *
 ***LOGIC:
 * NA
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * NA
 *
 * @param  mac - Pointer to Global MAC structure
 * @param  pMsg of type struct scheduler_msg - Pointer to the message structure
 * @return None
 */

uint32_t lim_defer_msg(struct mac_context *mac, struct scheduler_msg *pMsg)
{
	uint32_t retCode = TX_SUCCESS;

	retCode = lim_write_deferred_msg_q(mac, pMsg);

	if (retCode == TX_SUCCESS) {
		MTRACE(mac_trace_msg_rx
			(mac, NO_SESSION,
			LIM_TRACE_MAKE_RXMSG(pMsg->type, LIM_MSG_DEFERRED)));
	} else {
		pe_err("Dropped lim message (0x%X) Message %s", pMsg->type, lim_msg_str(pMsg->type));
		MTRACE(mac_trace_msg_rx
			(mac, NO_SESSION,
			LIM_TRACE_MAKE_RXMSG(pMsg->type, LIM_MSG_DROPPED)));
	}

	return retCode;
} /*** end lim_defer_msg() ***/

/**
 * lim_handle_unknown_a2_index_frames() - This function handles Unknown Unicast
 *                                        (A2 Index) packets
 * @mac_ctx:          Pointer to the Global Mac Context.
 * @rx_pkt_buffer:    Pointer to the packet Buffer descriptor.
 * @session_entry:    Pointer to the PE Session Entry.
 *
 * This routine will handle public action frames.
 *
 * Return:      None.
 */
static void lim_handle_unknown_a2_index_frames(struct mac_context *mac_ctx,
	void *rx_pkt_buffer, struct pe_session *session_entry)
{
#ifdef FEATURE_WLAN_TDLS
	tpSirMacDataHdr3a mac_hdr;
#endif
	if (LIM_IS_P2P_DEVICE_ROLE(session_entry))
		lim_process_action_frame_no_session(mac_ctx,
			(uint8_t *) rx_pkt_buffer);
#ifdef FEATURE_WLAN_TDLS
	mac_hdr = WMA_GET_RX_MPDUHEADER3A(rx_pkt_buffer);

	if (IEEE80211_IS_MULTICAST(mac_hdr->addr2)) {
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			FL("Ignoring A2 Invalid Packet received for MC/BC:"));
		lim_print_mac_addr(mac_ctx, mac_hdr->addr2, LOGD);
		return;
	}
	QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			FL("type=0x%x, subtype=0x%x"),
		mac_hdr->fc.type, mac_hdr->fc.subType);
	/* Currently only following type and subtype are handled.
	 * If there are more combinations, then add switch-case
	 * statements.
	 */
	if (LIM_IS_STA_ROLE(session_entry) &&
		(mac_hdr->fc.type == SIR_MAC_MGMT_FRAME) &&
		(mac_hdr->fc.subType == SIR_MAC_MGMT_ACTION))
		lim_process_action_frame(mac_ctx, rx_pkt_buffer, session_entry);
#endif
	return;
}

/**
 * lim_check_mgmt_registered_frames() - This function handles registered
 *                                      management frames.
 *
 * @mac_ctx:          Pointer to the Global Mac Context.
 * @buff_desc:        Pointer to the packet Buffer descriptor.
 * @session_entry:    Pointer to the PE Session Entry.
 *
 * This function is called to process to check if received frame match with
 * any of the registered frame from HDD. If yes pass this frame to SME.
 *
 * Return:      True or False for Match or Mismatch respectively.
 */
static bool
lim_check_mgmt_registered_frames(struct mac_context *mac_ctx, uint8_t *buff_desc,
				 struct pe_session *session_entry)
{
	tSirMacFrameCtl fc;
	tpSirMacMgmtHdr hdr;
	uint8_t *body;
	struct mgmt_frm_reg_info *mgmt_frame = NULL;
	struct mgmt_frm_reg_info *next_frm = NULL;
	uint16_t frm_type;
	uint16_t frm_len;
	uint8_t type, sub_type;
	bool match = false;
	tpSirMacActionFrameHdr action_hdr;
	uint8_t actionID, category;
	QDF_STATUS qdf_status;

	hdr = WMA_GET_RX_MAC_HEADER(buff_desc);
	fc = hdr->fc;
	frm_type = (fc.type << 2) | (fc.subType << 4);
	body = WMA_GET_RX_MPDU_DATA(buff_desc);
	action_hdr = (tpSirMacActionFrameHdr)body;
	frm_len = WMA_GET_RX_PAYLOAD_LEN(buff_desc);

	qdf_mutex_acquire(&mac_ctx->lim.lim_frame_register_lock);
	qdf_list_peek_front(&mac_ctx->lim.gLimMgmtFrameRegistratinQueue,
			    (qdf_list_node_t **) &mgmt_frame);
	qdf_mutex_release(&mac_ctx->lim.lim_frame_register_lock);

	while (mgmt_frame) {
		type = (mgmt_frame->frameType >> 2) & 0x03;
		sub_type = (mgmt_frame->frameType >> 4) & 0x0f;
		if ((type == SIR_MAC_MGMT_FRAME)
		    && (fc.type == SIR_MAC_MGMT_FRAME)
		    && (sub_type == SIR_MAC_MGMT_RESERVED15)) {
			QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
				FL
				("rcvd frm match for SIR_MAC_MGMT_RESERVED15"));
			match = true;
			break;
		}
		if (mgmt_frame->frameType == frm_type) {
			if (mgmt_frame->matchLen <= 0) {
				match = true;
				break;
			}
			if (mgmt_frame->matchLen <= frm_len &&
				(!qdf_mem_cmp(mgmt_frame->matchData, body,
				mgmt_frame->matchLen))) {
				/* found match! */
				match = true;
				break;
			}
		}

		qdf_mutex_acquire(&mac_ctx->lim.lim_frame_register_lock);
		qdf_status =
			qdf_list_peek_next(
			&mac_ctx->lim.gLimMgmtFrameRegistratinQueue,
			(qdf_list_node_t *) mgmt_frame,
			(qdf_list_node_t **) &next_frm);
		qdf_mutex_release(&mac_ctx->lim.lim_frame_register_lock);
		mgmt_frame = next_frm;
		next_frm = NULL;
	}
	if (match) {
		pe_debug("rcvd frame match with registered frame params");

		/*
		 * Drop BTM frame received on STA interface if concurrent
		 * P2P connection is active and p2p_disable_roam ini is
		 * enabled. This will help to avoid scan triggered by
		 * userspace after processing the BTM frame from AP so the
		 * audio glitches are not seen in P2P connection.
		 */
		if (cfg_p2p_is_roam_config_disabled(mac_ctx->psoc) &&
		    session_entry && LIM_IS_STA_ROLE(session_entry) &&
		    (policy_mgr_mode_specific_connection_count(mac_ctx->psoc,
						PM_P2P_CLIENT_MODE, NULL) ||
		     policy_mgr_mode_specific_connection_count(mac_ctx->psoc,
						PM_P2P_GO_MODE, NULL))) {
			if (frm_len >= sizeof(*action_hdr) && action_hdr &&
			    fc.type == SIR_MAC_MGMT_FRAME &&
			    fc.subType == SIR_MAC_MGMT_ACTION) {
				actionID = action_hdr->actionID;
				category = action_hdr->category;
				if (category == ACTION_CATEGORY_WNM &&
				    (actionID == WNM_BSS_TM_QUERY ||
				     actionID == WNM_BSS_TM_REQUEST ||
				     actionID == WNM_BSS_TM_RESPONSE)) {
					pe_debug("p2p session active drop BTM frame");
					return match;
				}
			}
		}
		/* Indicate this to SME */
		lim_send_sme_mgmt_frame_ind(mac_ctx, hdr->fc.subType,
			(uint8_t *) hdr,
			WMA_GET_RX_PAYLOAD_LEN(buff_desc) +
			sizeof(tSirMacMgmtHdr), mgmt_frame->sessionId,
			WMA_GET_RX_FREQ(buff_desc), session_entry,
			WMA_GET_RX_RSSI_NORMALIZED(buff_desc),
			RXMGMT_FLAG_NONE);

		if ((type == SIR_MAC_MGMT_FRAME)
		    && (fc.type == SIR_MAC_MGMT_FRAME)
		    && (sub_type == SIR_MAC_MGMT_RESERVED15))
			/* These packets needs to be processed by PE/SME
			 * as well as HDD.If it returns true here,
			 * the packet is forwarded to HDD only.
			 */
			match = false;
	}

	return match;
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT
/**
 * lim_is_mgmt_frame_loggable() - to log non-excessive mgmt frames
 * @type: type of frames i.e. mgmt, control, data
 * @subtype: subtype of frames i.e. beacon, probe rsp, probe req and etc
 *
 * This API tells if given mgmt frame is expected to come excessive in
 * amount or not.
 *
 * Return: true if mgmt is expected to come not that often, so makes it
 *         loggable. false if mgmt is expected to come too often, so makes
 *         it not loggable
 */
static bool
lim_is_mgmt_frame_loggable(uint8_t type, uint8_t subtype)
{
	if (type != SIR_MAC_MGMT_FRAME)
		return false;

	switch (subtype) {
	case SIR_MAC_MGMT_BEACON:
	case SIR_MAC_MGMT_PROBE_REQ:
	case SIR_MAC_MGMT_PROBE_RSP:
		return false;
	default:
		return true;
	}
}
#else
static bool
lim_is_mgmt_frame_loggable(uint8_t type, uint8_t subtype)
{
	return false;
}
#endif

/**
 * lim_handle80211_frames()
 *
 ***FUNCTION:
 * This function is called to process 802.11 frames
 * received by LIM.
 *
 ***LOGIC:
 * NA
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * NA
 *
 * @param  mac - Pointer to Global MAC structure
 * @param  pMsg of type struct scheduler_msg - Pointer to the message structure
 * @return None
 */

static void
lim_handle80211_frames(struct mac_context *mac, struct scheduler_msg *limMsg,
		       uint8_t *pDeferMsg)
{
	uint8_t *pRxPacketInfo = NULL;
	tSirMacFrameCtl fc;
	tpSirMacMgmtHdr pHdr = NULL;
	struct pe_session *pe_session = NULL;
	uint8_t sessionId;
	bool isFrmFt = false;
	uint32_t frequency;
	bool is_hw_sbs_capable = false;

	*pDeferMsg = false;
	lim_get_b_dfrom_rx_packet(mac, limMsg->bodyptr,
				  (uint32_t **) &pRxPacketInfo);

	pHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);
	isFrmFt = WMA_GET_RX_FT_DONE(pRxPacketInfo);
	frequency = WMA_GET_RX_FREQ(pRxPacketInfo);
	fc = pHdr->fc;

	is_hw_sbs_capable =
		policy_mgr_is_hw_sbs_capable(mac->psoc);
	if (WLAN_REG_IS_5GHZ_CH_FREQ(frequency) &&
	    (!is_hw_sbs_capable ||
	    (is_hw_sbs_capable &&
	    wlan_reg_is_dfs_for_freq(mac->pdev, frequency))) &&
	    mac->sap.SapDfsInfo.is_dfs_cac_timer_running) {
		pe_session = pe_find_session_by_bssid(mac,
					pHdr->bssId, &sessionId);
		if (pe_session &&
		    (QDF_SAP_MODE == pe_session->opmode)) {
			pe_debug("CAC timer running - drop the frame");
			goto end;
		}
	}

#ifdef WLAN_DUMP_MGMTFRAMES
	pe_debug("ProtVersion %d, Type %d, Subtype %d rateIndex=%d",
		fc.protVer, fc.type, fc.subType,
		WMA_GET_RX_MAC_RATE_IDX(pRxPacketInfo));
	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_ERROR, pHdr,
			   WMA_GET_RX_MPDU_HEADER_LEN(pRxPacketInfo));
#endif
	if (mac->mlme_cfg->gen.debug_packet_log & 0x1) {
		if ((fc.type == SIR_MAC_MGMT_FRAME) &&
		    (fc.subType != SIR_MAC_MGMT_PROBE_REQ) &&
		    (fc.subType != SIR_MAC_MGMT_PROBE_RSP) &&
		    (fc.subType != SIR_MAC_MGMT_BEACON)) {
			pe_debug("RX MGMT - Type %hu, SubType %hu, seq num[%d]",
				   fc.type,
				   fc.subType,
				   ((pHdr->seqControl.seqNumHi <<
				   HIGH_SEQ_NUM_OFFSET) |
				   pHdr->seqControl.seqNumLo));
		}
	}
#ifdef FEATURE_WLAN_EXTSCAN
	if (WMA_IS_EXTSCAN_SCAN_SRC(pRxPacketInfo) ||
		WMA_IS_EPNO_SCAN_SRC(pRxPacketInfo)) {
		if (fc.subType == SIR_MAC_MGMT_BEACON ||
		    fc.subType == SIR_MAC_MGMT_PROBE_RSP) {
			__lim_process_ext_scan_beacon_probe_rsp(mac,
								pRxPacketInfo,
								fc.subType);
		} else {
			pe_err("Wrong frameType %d, Subtype %d for %d",
				fc.type, fc.subType,
				WMA_GET_SCAN_SRC(pRxPacketInfo));
		}
		goto end;
	}
#endif
	/* Added For BT-AMP Support */
	pe_session = pe_find_session_by_bssid(mac, pHdr->bssId,
						 &sessionId);
	if (!pe_session) {
		if (fc.subType == SIR_MAC_MGMT_AUTH) {
			pe_debug("ProtVersion %d, Type %d, Subtype %d rateIndex=%d",
				fc.protVer, fc.type, fc.subType,
				WMA_GET_RX_MAC_RATE_IDX(pRxPacketInfo));
			lim_print_mac_addr(mac, pHdr->bssId, LOGD);
			if (lim_process_auth_frame_no_session
				    (mac, pRxPacketInfo,
				    limMsg->bodyptr) == QDF_STATUS_SUCCESS) {
				goto end;
			}
		}
		/* Public action frame can be received from non-assoc stations*/
		if ((fc.subType != SIR_MAC_MGMT_PROBE_RSP) &&
		    (fc.subType != SIR_MAC_MGMT_BEACON) &&
		    (fc.subType != SIR_MAC_MGMT_PROBE_REQ)
		    && (fc.subType != SIR_MAC_MGMT_ACTION)) {

			pe_session = pe_find_session_by_peer_sta(mac,
						pHdr->sa, &sessionId);
			if (!pe_session) {
				pe_debug("session does not exist for bssId");
				lim_print_mac_addr(mac, pHdr->sa, LOGD);
				goto end;
			} else {
				pe_debug("SessionId:%d exists for given Bssid",
					pe_session->peSessionId);
			}
		}
		/*  For p2p resp frames search for valid session with DA as */
		/*  BSSID will be SA and session will be present with DA only */
		if (fc.subType == SIR_MAC_MGMT_ACTION) {
			pe_session =
				pe_find_session_by_bssid(mac, pHdr->da, &sessionId);
		}
	}

	/* Check if frame is registered by HDD */
	if (lim_check_mgmt_registered_frames(mac, pRxPacketInfo, pe_session)) {
		pe_debug("Received frame is passed to SME");
		goto end;
	}

	if (fc.protVer != SIR_MAC_PROTOCOL_VERSION) {   /* Received Frame with non-zero Protocol Version */
		pe_err("Unexpected frame with protVersion %d received",
			fc.protVer);
		lim_pkt_free(mac, TXRX_FRM_802_11_MGMT, pRxPacketInfo,
			     (void *)limMsg->bodyptr);
#ifdef WLAN_DEBUG
		mac->lim.numProtErr++;
#endif
		goto end;
	}

/* Chance of crashing : to be done BT-AMP ........happens when broadcast probe req is received */

#ifdef WLAN_DEBUG
	mac->lim.numMAC[fc.type][fc.subType]++;
#endif

	switch (fc.type) {
	case SIR_MAC_MGMT_FRAME:
	{
		/* Received Management frame */
		switch (fc.subType) {
		case SIR_MAC_MGMT_ASSOC_REQ:
			/* Make sure the role supports Association */
			if (LIM_IS_AP_ROLE(pe_session))
				lim_process_assoc_req_frame(mac,
							    pRxPacketInfo,
							    LIM_ASSOC,
							    pe_session);
			else {
				pe_err("unexpected message received %X",
					limMsg->type);
				lim_print_msg_name(mac, LOGE,
						   limMsg->type);
			}
			break;

		case SIR_MAC_MGMT_ASSOC_RSP:
			lim_process_assoc_rsp_frame(mac, pRxPacketInfo,
						    ASSOC_FRAME_LEN,
						    LIM_ASSOC,
						    pe_session);
			break;

		case SIR_MAC_MGMT_REASSOC_REQ:
			/* Make sure the role supports Reassociation */
			if (LIM_IS_AP_ROLE(pe_session)) {
				lim_process_assoc_req_frame(mac,
							    pRxPacketInfo,
							    LIM_REASSOC,
							    pe_session);
			} else {
				pe_err("unexpected message received %X",
					limMsg->type);
				lim_print_msg_name(mac, LOGE,
					limMsg->type);
			}
			break;

		case SIR_MAC_MGMT_REASSOC_RSP:
			lim_process_assoc_rsp_frame(mac, pRxPacketInfo,
						    ASSOC_FRAME_LEN,
						    LIM_REASSOC,
						    pe_session);
			break;

		case SIR_MAC_MGMT_PROBE_REQ:
			lim_process_probe_req_frame_multiple_bss(mac,
								 pRxPacketInfo,
								 pe_session);
			break;

		case SIR_MAC_MGMT_PROBE_RSP:
			if (pe_session)
				lim_process_probe_rsp_frame(mac,
							    pRxPacketInfo,
							    pe_session);
			break;

		case SIR_MAC_MGMT_BEACON:
			__lim_handle_beacon(mac, limMsg, pe_session);
			break;

		case SIR_MAC_MGMT_DISASSOC:
			lim_process_disassoc_frame(mac, pRxPacketInfo,
						   pe_session);
			break;

		case SIR_MAC_MGMT_AUTH:
			lim_process_auth_frame(mac, pRxPacketInfo,
					       pe_session);
			break;

		case SIR_MAC_MGMT_DEAUTH:
			lim_process_deauth_frame(mac, pRxPacketInfo,
						 pe_session);
			break;

		case SIR_MAC_MGMT_ACTION:
			if (!pe_session)
				lim_process_action_frame_no_session(mac,
								    pRxPacketInfo);
			else {
				if (WMA_GET_RX_UNKNOWN_UCAST
					    (pRxPacketInfo))
					lim_handle_unknown_a2_index_frames
						(mac, pRxPacketInfo,
						pe_session);
				else
					lim_process_action_frame(mac,
								 pRxPacketInfo,
								 pe_session);
			}
			break;
		default:
			/* Received Management frame of 'reserved' subtype */
			break;
		} /* switch (fc.subType) */

	}
	break;
	case SIR_MAC_DATA_FRAME:
	{
	}
	break;
	default:
		/* Received frame of type 'reserved' */
		break;

	} /* switch (fc.type) */
	if (lim_is_mgmt_frame_loggable(fc.type, fc.subType))
		lim_diag_mgmt_rx_event_report(mac, pHdr,
					      pe_session,
					      QDF_STATUS_SUCCESS, QDF_STATUS_SUCCESS);
end:
	lim_pkt_free(mac, TXRX_FRM_802_11_MGMT, pRxPacketInfo,
		     (void *)limMsg->bodyptr);
	return;
} /*** end lim_handle80211_frames() ***/

void lim_process_abort_scan_ind(struct mac_context *mac_ctx,
	uint8_t vdev_id, uint32_t scan_id, uint32_t scan_requestor_id)
{
	QDF_STATUS status;
	struct scan_cancel_request *req;
	struct wlan_objmgr_vdev *vdev;

	pe_debug("scan_id %d, scan_requestor_id 0x%x",
			scan_id, scan_requestor_id);

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
			mac_ctx->psoc, vdev_id,
			WLAN_LEGACY_MAC_ID);
	if (!vdev) {
		pe_debug("vdev is NULL");
		return;
	}

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		goto fail;

	req->vdev = vdev;
	req->cancel_req.requester = scan_requestor_id;
	req->cancel_req.scan_id = scan_id;
	req->cancel_req.vdev_id = vdev_id;
	req->cancel_req.req_type = WLAN_SCAN_CANCEL_SINGLE;

	status = ucfg_scan_cancel(req);
	if (QDF_IS_STATUS_ERROR(status))
		pe_err("Cancel scan request failed");

fail:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);
}

static void lim_process_sme_obss_scan_ind(struct mac_context *mac_ctx,
					  struct scheduler_msg *msg)
{
	struct pe_session *session;
	uint8_t session_id;
	struct sme_obss_ht40_scanind_msg *ht40_scanind;

	ht40_scanind = (struct sme_obss_ht40_scanind_msg *)msg->bodyptr;
	session = pe_find_session_by_bssid(mac_ctx,
			ht40_scanind->mac_addr.bytes, &session_id);
	if (!session) {
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			"OBSS Scan not started: session id is NULL");
		return;
	}
	if (session->htSupportedChannelWidthSet ==
			WNI_CFG_CHANNEL_BONDING_MODE_ENABLE) {
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			"OBSS Scan Start Req: session id %d"
			"htSupportedChannelWidthSet %d",
			session->peSessionId,
			session->htSupportedChannelWidthSet);
		lim_send_ht40_obss_scanind(mac_ctx, session);
	} else {
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			"OBSS Scan not started: channel width - %d session %d",
			session->htSupportedChannelWidthSet,
			session->peSessionId);
	}
	return;
}

/**
 * lim_process_messages() - Process messages from upper layers.
 *
 * @mac_ctx: Pointer to the Global Mac Context.
 * @msg: Received message.
 *
 * Return:  None.
 */
static void lim_process_messages(struct mac_context *mac_ctx,
				 struct scheduler_msg *msg)
{
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
	uint8_t vdev_id = 0;
	tUpdateBeaconParams beacon_params;
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
	uint8_t i;
	struct pe_session *session_entry = NULL;
	uint8_t defer_msg = false;
	uint16_t pkt_len = 0;
	cds_pkt_t *body_ptr = NULL;
	QDF_STATUS qdf_status;
	struct scheduler_msg new_msg = {0};

	if (!msg) {
		pe_err("Message pointer is Null");
		QDF_ASSERT(0);
		return;
	}

	if (ANI_DRIVER_TYPE(mac_ctx) == QDF_DRIVER_TYPE_MFG) {
		qdf_mem_free(msg->bodyptr);
		msg->bodyptr = NULL;
		return;
	}

#ifdef WLAN_DEBUG
	mac_ctx->lim.numTot++;
#endif
	/*
	 * MTRACE logs not captured for events received from SME
	 * SME enums (eWNI_SME_START_REQ) starts with 0x16xx.
	 * Compare received SME events with SIR_SME_MODULE_ID
	 */
	if ((SIR_SME_MODULE_ID ==
	    (uint8_t)MAC_TRACE_GET_MODULE_ID(msg->type)) &&
	    (msg->type != eWNI_SME_REGISTER_MGMT_FRAME_REQ)) {
		MTRACE(mac_trace(mac_ctx, TRACE_CODE_RX_SME_MSG,
				 NO_SESSION, msg->type));
	} else {
		/*
		 * Omitting below message types as these are too frequent
		 * and when crash happens we loose critical trace logs
		 * if these are also logged
		 */
		if (msg->type != SIR_BB_XPORT_MGMT_MSG &&
		    msg->type != WMA_RX_SCAN_EVENT)
			MTRACE(mac_trace_msg_rx(mac_ctx, NO_SESSION,
				LIM_TRACE_MAKE_RXMSG(msg->type,
				LIM_MSG_PROCESSED)));
	}

	switch (msg->type) {

	case SIR_LIM_UPDATE_BEACON:
		lim_update_beacon(mac_ctx);
		break;
#ifdef ANI_SIR_IBSS_PEER_CACHING
	case WMA_IBSS_STA_ADD:
		lim_ibss_sta_add(mac_ctx, msg->bodyptr);
		break;
#endif
	case SIR_BB_XPORT_MGMT_MSG:
		/* These messages are from Peer MAC entity. */
#ifdef WLAN_DEBUG
		mac_ctx->lim.numBbt++;
#endif
		/* The original msg which we were deferring have the
		 * bodyPointer point to 'BD' instead of 'cds pkt'. If we
		 * don't make a copy of msg, then overwrite the
		 * msg->bodyPointer and next time when we try to
		 * process the msg, we will try to use 'BD' as
		 * 'cds Pkt' which will cause a crash
		 */
		if (!msg->bodyptr) {
			pe_err("Message bodyptr is Null");
			QDF_ASSERT(0);
			break;
		}
		qdf_mem_copy((uint8_t *) &new_msg,
			(uint8_t *) msg, sizeof(struct scheduler_msg));
		body_ptr = (cds_pkt_t *) new_msg.bodyptr;
		cds_pkt_get_packet_length(body_ptr, &pkt_len);

		qdf_status = wma_ds_peek_rx_packet_info(body_ptr,
			(void **) &new_msg.bodyptr, false);

		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			lim_decrement_pending_mgmt_count(mac_ctx);
			cds_pkt_return_packet(body_ptr);
			break;
		}
		if (WMA_GET_ROAMCANDIDATEIND(new_msg.bodyptr))
			pe_debug("roamCandidateInd: %d",
				 WMA_GET_ROAMCANDIDATEIND(new_msg.bodyptr));
		if (WMA_GET_OFFLOADSCANLEARN(new_msg.bodyptr))
			pe_debug("offloadScanLearn: %d",
				 WMA_GET_OFFLOADSCANLEARN(new_msg.bodyptr));

		lim_handle80211_frames(mac_ctx, &new_msg, &defer_msg);

		if (defer_msg == true) {
			QDF_TRACE(QDF_MODULE_ID_PE, LOGD,
					FL("Defer Msg type=%x"), msg->type);
			if (lim_defer_msg(mac_ctx, msg) != TX_SUCCESS) {
				QDF_TRACE(QDF_MODULE_ID_PE, LOGE,
						FL("Unable to Defer Msg"));
				lim_log_session_states(mac_ctx);
				lim_decrement_pending_mgmt_count(mac_ctx);
				cds_pkt_return_packet(body_ptr);
			}
		} else {
			/* PE is not deferring this 802.11 frame so we need to
			 * call cds_pkt_return. Asumption here is when Rx mgmt
			 * frame processing is done, cds packet could be
			 * freed here.
			 */
			lim_decrement_pending_mgmt_count(mac_ctx);
			cds_pkt_return_packet(body_ptr);
		}
		break;
	case eWNI_SME_DISASSOC_REQ:
	case eWNI_SME_DEAUTH_REQ:
#ifdef FEATURE_WLAN_TDLS
	case eWNI_SME_TDLS_SEND_MGMT_REQ:
	case eWNI_SME_TDLS_ADD_STA_REQ:
	case eWNI_SME_TDLS_DEL_STA_REQ:
	case eWNI_SME_TDLS_LINK_ESTABLISH_REQ:
#endif
	case eWNI_SME_RESET_AP_CAPS_CHANGED:
	case eWNI_SME_SET_HW_MODE_REQ:
	case eWNI_SME_SET_DUAL_MAC_CFG_REQ:
	case eWNI_SME_SET_ANTENNA_MODE_REQ:
	case eWNI_SME_UPDATE_ACCESS_POLICY_VENDOR_IE:
	case eWNI_SME_UPDATE_CONFIG:
		/* These messages are from HDD. Need to respond to HDD */
		lim_process_normal_hdd_msg(mac_ctx, msg, true);
		break;
	case eWNI_SME_SEND_DISASSOC_FRAME:
		/* Need to response to hdd */
		lim_process_normal_hdd_msg(mac_ctx, msg, true);
		break;
	case eWNI_SME_PDEV_SET_HT_VHT_IE:
	case eWNI_SME_SET_VDEV_IES_PER_BAND:
	case eWNI_SME_SYS_READY_IND:
	case eWNI_SME_JOIN_REQ:
	case eWNI_SME_REASSOC_REQ:
	case eWNI_SME_START_BSS_REQ:
	case eWNI_SME_STOP_BSS_REQ:
	case eWNI_SME_SWITCH_CHL_IND:
	case eWNI_SME_DISASSOC_CNF:
	case eWNI_SME_DEAUTH_CNF:
	case eWNI_SME_ASSOC_CNF:
	case eWNI_SME_ADDTS_REQ:
	case eWNI_SME_MSCS_REQ:
	case eWNI_SME_DELTS_REQ:
	case eWNI_SME_SESSION_UPDATE_PARAM:
	case eWNI_SME_CHNG_MCC_BEACON_INTERVAL:
	case eWNI_SME_NEIGHBOR_REPORT_REQ_IND:
	case eWNI_SME_BEACON_REPORT_RESP_XMIT_IND:
#if defined FEATURE_WLAN_ESE
	case eWNI_SME_ESE_ADJACENT_AP_REPORT:
#endif
	case eWNI_SME_FT_PRE_AUTH_REQ:
	case eWNI_SME_FT_AGGR_QOS_REQ:
	case eWNI_SME_REGISTER_MGMT_FRAME_REQ:
#ifdef FEATURE_WLAN_ESE
	case eWNI_SME_GET_TSM_STATS_REQ:
#endif  /* FEATURE_WLAN_ESE */
	case eWNI_SME_REGISTER_MGMT_FRAME_CB:
	case eWNI_SME_EXT_CHANGE_CHANNEL:
	case eWNI_SME_ROAM_INVOKE:
		/* fall through */
	case eWNI_SME_ROAM_SEND_SET_PCL_REQ:
	case eWNI_SME_SET_ADDBA_ACCEPT:
	case eWNI_SME_UPDATE_EDCA_PROFILE:
	case WNI_SME_UPDATE_MU_EDCA_PARAMS:
	case eWNI_SME_UPDATE_SESSION_EDCA_TXQ_PARAMS:
	case WNI_SME_CFG_ACTION_FRM_HE_TB_PPDU:
		/* These messages are from HDD.No need to respond to HDD */
		lim_process_normal_hdd_msg(mac_ctx, msg, false);
		break;
	case eWNI_SME_SEND_MGMT_FRAME_TX:
		lim_send_mgmt_frame_tx(mac_ctx, msg);
		qdf_mem_free(msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case eWNI_SME_MON_INIT_SESSION:
		lim_mon_init_session(mac_ctx, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case eWNI_SME_MON_DEINIT_SESSION:
		lim_mon_deinit_session(mac_ctx, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case SIR_HAL_P2P_NOA_ATTR_IND:
		session_entry = &mac_ctx->lim.gpSession[0];
		pe_debug("Received message Noa_ATTR %x",
			msg->type);
		for (i = 0; i < mac_ctx->lim.maxBssId; i++) {
			session_entry = &mac_ctx->lim.gpSession[i];
			if ((session_entry) && (session_entry->valid) &&
			    (session_entry->opmode == QDF_P2P_GO_MODE)) {
				/* Save P2P attr for Go */
				qdf_mem_copy(&session_entry->p2pGoPsUpdate,
					     msg->bodyptr,
					     sizeof(tSirP2PNoaAttr));
				pe_debug("bssId"
					 QDF_MAC_ADDR_FMT
					 " ctWin=%d oppPsFlag=%d",
					 QDF_MAC_ADDR_REF(session_entry->bssId),
					 session_entry->p2pGoPsUpdate.ctWin,
					 session_entry->p2pGoPsUpdate.oppPsFlag);
				pe_debug("uNoa1IntervalCnt=%d uNoa1Duration=%d uNoa1Interval=%d uNoa1StartTime=%d",
					 session_entry->p2pGoPsUpdate.uNoa1IntervalCnt,
					 session_entry->p2pGoPsUpdate.uNoa1Duration,
					 session_entry->p2pGoPsUpdate.uNoa1Interval,
					 session_entry->p2pGoPsUpdate.uNoa1StartTime);
				break;
			}
		}
		qdf_mem_free(msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case WMA_MISSED_BEACON_IND:
		lim_ps_offload_handle_missed_beacon_ind(mac_ctx, msg);
		qdf_mem_free(msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case SIR_LIM_ADDTS_RSP_TIMEOUT:
		lim_process_sme_req_messages(mac_ctx, msg);
		break;
#ifdef FEATURE_WLAN_ESE
	case WMA_TSM_STATS_RSP:
		lim_send_sme_pe_ese_tsm_rsp(mac_ctx,
			(tAniGetTsmStatsRsp *) msg->bodyptr);
		break;
#endif
	case WMA_ADD_TS_RSP:
		lim_process_hal_add_ts_rsp(mac_ctx, msg);
		break;
	case SIR_LIM_BEACON_GEN_IND:
		if (mac_ctx->lim.gLimSystemRole != eLIM_AP_ROLE)
			sch_process_pre_beacon_ind(mac_ctx,
						   msg, REASON_DEFAULT);
		break;
	case SIR_LIM_DELETE_STA_CONTEXT_IND:
		lim_delete_sta_context(mac_ctx, msg);
		break;
	case SIR_LIM_RX_INVALID_PEER:
		lim_rx_invalid_peer_process(mac_ctx, msg);
		break;
	case SIR_HAL_REQ_SEND_DELBA_REQ_IND:
		lim_req_send_delba_ind_process(mac_ctx, msg);
		break;
	case SIR_LIM_JOIN_FAIL_TIMEOUT:
	case SIR_LIM_PERIODIC_JOIN_PROBE_REQ_TIMEOUT:
	case SIR_LIM_AUTH_FAIL_TIMEOUT:
	case SIR_LIM_AUTH_RSP_TIMEOUT:
	case SIR_LIM_ASSOC_FAIL_TIMEOUT:
	case SIR_LIM_REASSOC_FAIL_TIMEOUT:
	case SIR_LIM_FT_PREAUTH_RSP_TIMEOUT:
	case SIR_LIM_DISASSOC_ACK_TIMEOUT:
	case SIR_LIM_DEAUTH_ACK_TIMEOUT:
	case SIR_LIM_AUTH_RETRY_TIMEOUT:
	case SIR_LIM_AUTH_SAE_TIMEOUT:
		/* These timeout messages are handled by MLM sub module */
		lim_process_mlm_req_messages(mac_ctx, msg);
		break;
	case SIR_LIM_HEART_BEAT_TIMEOUT:
		/** check if heart beat failed, even if one Beacon
		 * is rcvd within the Heart Beat interval continue
		 * normal processing
		 */
		if (!msg->bodyptr)
			pe_err("Can't Process HB TO - bodyptr is Null");
		else {
			session_entry = (struct pe_session *) msg->bodyptr;
			pe_err("SIR_LIM_HEART_BEAT_TIMEOUT, Session %d",
				((struct pe_session *) msg->bodyptr)->peSessionId);
			limResetHBPktCount(session_entry);
			lim_handle_heart_beat_timeout_for_session(mac_ctx,
							session_entry);
		}
		break;
	case SIR_LIM_PROBE_HB_FAILURE_TIMEOUT:
		lim_handle_heart_beat_failure_timeout(mac_ctx);
		break;
	case SIR_LIM_CNF_WAIT_TIMEOUT:
		/* Does not receive CNF or dummy packet */
		lim_handle_cnf_wait_timeout(mac_ctx, (uint16_t) msg->bodyval);
		break;
	case SIR_LIM_UPDATE_OLBC_CACHEL_TIMEOUT:
		lim_handle_update_olbc_cache(mac_ctx);
		break;
	case WMA_ADD_STA_RSP:
		lim_process_add_sta_rsp(mac_ctx, msg);
		break;
	case WMA_DELETE_STA_RSP:
		lim_process_mlm_del_sta_rsp(mac_ctx, msg);
		break;
	case WMA_DELETE_BSS_RSP:
	case WMA_DELETE_BSS_HO_FAIL_RSP:
		lim_handle_delete_bss_rsp(mac_ctx, msg->bodyptr);
		break;
	case WMA_CSA_OFFLOAD_EVENT:
		lim_handle_csa_offload_msg(mac_ctx, msg);
		break;
	case WMA_SET_BSSKEY_RSP:
	case WMA_SET_STA_BCASTKEY_RSP:
		lim_process_mlm_set_bss_key_rsp(mac_ctx, msg);
		break;
	case WMA_SET_STAKEY_RSP:
		lim_process_mlm_set_sta_key_rsp(mac_ctx, msg);
		break;
	case WMA_SET_MIMOPS_RSP:
	case WMA_SET_TX_POWER_RSP:
		qdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case WMA_SET_MAX_TX_POWER_RSP:
		rrm_set_max_tx_power_rsp(mac_ctx, msg);
		if (msg->bodyptr) {
			qdf_mem_free((void *)msg->bodyptr);
			msg->bodyptr = NULL;
		}
		break;
	case SIR_LIM_ADDR2_MISS_IND:
		pe_err(
			FL("Addr2 mismatch interrupt received %X"), msg->type);
		/* message from HAL indicating addr2 mismatch interrupt occurred
		 * msg->bodyptr contains only pointer to 48-bit addr2 field
		 */
		qdf_mem_free((void *)(msg->bodyptr));
		msg->bodyptr = NULL;
		break;
	case WMA_AGGR_QOS_RSP:
		lim_process_ft_aggr_qos_rsp(mac_ctx, msg);
		break;
	case WMA_RX_CHN_STATUS_EVENT:
		lim_process_rx_channel_status_event(mac_ctx, msg->bodyptr);
		break;
	case WMA_DFS_BEACON_TX_SUCCESS_IND:
		lim_process_beacon_tx_success_ind(mac_ctx, msg->type,
				(void *)msg->bodyptr);
		qdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case WMA_DISASSOC_TX_COMP:
		lim_disassoc_tx_complete_cnf(mac_ctx, msg->bodyval,
					     msg->bodyptr);
		break;
	case WMA_DEAUTH_TX_COMP:
		lim_deauth_tx_complete_cnf(mac_ctx, msg->bodyval,
					   msg->bodyptr);
		break;
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
	case WMA_UPDATE_Q2Q_IE_IND:
		qdf_mem_zero(&beacon_params, sizeof(tUpdateBeaconParams));
		beacon_params.paramChangeBitmap = 0;
		for (i = 0; i < mac_ctx->lim.maxBssId; i++) {
			vdev_id = ((uint8_t *)msg->bodyptr)[i];
			session_entry = pe_find_session_by_vdev_id(mac_ctx,
								   vdev_id);
			if (!session_entry)
				continue;
			session_entry->sap_advertise_avoid_ch_ie =
				(uint8_t)msg->bodyval;
			/*
			 * if message comes for DFS channel, no need to update:
			 * 1) We wont have MCC with DFS channels. so no need to
			 *    add Q2Q IE
			 * 2) We cannot end up in DFS channel SCC by channel
			 *    switch from non DFS MCC scenario, so no need to
			 *    remove Q2Q IE
			 * 3) There is however a case where device start MCC and
			 *    then user modifies hostapd.conf and does SAP
			 *    restart, in such a case, beacon params will be
			 *    reset and thus will not contain Q2Q IE, by default
			 */
			if (wlan_reg_get_channel_state(
				mac_ctx->pdev,
				wlan_reg_freq_to_chan(
				mac_ctx->pdev, session_entry->curr_op_freq))
					!= CHANNEL_STATE_DFS) {
				beacon_params.bss_idx = session_entry->vdev_id;
				beacon_params.beaconInterval =
					session_entry->beaconParams.beaconInterval;
				beacon_params.paramChangeBitmap |=
					PARAM_BCN_INTERVAL_CHANGED;
				sch_set_fixed_beacon_fields(mac_ctx,
					session_entry);
				lim_send_beacon_params(mac_ctx, &beacon_params,
					session_entry);
			}
		}
		qdf_mem_free(msg->bodyptr);
		msg->bodyptr = NULL;
		break;
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
	case eWNI_SME_NSS_UPDATE_REQ:
	case eWNI_SME_DFS_BEACON_CHAN_SW_IE_REQ:
		lim_process_sme_req_messages(mac_ctx, msg);
		qdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case eWNI_SME_CHANNEL_CHANGE_REQ:
		lim_process_sme_req_messages(mac_ctx, msg);
		qdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case eWNI_SME_START_BEACON_REQ:
		lim_process_sme_req_messages(mac_ctx, msg);
		qdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case eWNI_SME_UPDATE_ADDITIONAL_IES:
		lim_process_sme_req_messages(mac_ctx, msg);
		qdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case eWNI_SME_MODIFY_ADDITIONAL_IES:
		lim_process_sme_req_messages(mac_ctx, msg);
		qdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
#ifdef QCA_HT_2040_COEX
	case eWNI_SME_SET_HT_2040_MODE:
		lim_process_sme_req_messages(mac_ctx, msg);
		qdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
#endif
	case SIR_HAL_PDEV_SET_HW_MODE_RESP:
		lim_process_set_hw_mode_resp(mac_ctx, msg->bodyptr);
		qdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case SIR_HAL_PDEV_HW_MODE_TRANS_IND:
		lim_process_hw_mode_trans_ind(mac_ctx, msg->bodyptr);
		qdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case SIR_HAL_PDEV_MAC_CFG_RESP:
		lim_process_dual_mac_cfg_resp(mac_ctx, msg->bodyptr);
		qdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case eWNI_SME_SET_IE_REQ:
		lim_process_sme_req_messages(mac_ctx, msg);
		qdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case eWNI_SME_HT40_OBSS_SCAN_IND:
		lim_process_sme_obss_scan_ind(mac_ctx, msg);
		qdf_mem_free(msg->bodyptr);
		break;
	case SIR_HAL_SOC_ANTENNA_MODE_RESP:
		lim_process_set_antenna_resp(mac_ctx, msg->bodyptr);
		qdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case eWNI_SME_DEFAULT_SCAN_IE:
		lim_process_set_default_scan_ie_request(mac_ctx, msg->bodyptr);
		qdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case eWNI_SME_SET_HE_BSS_COLOR:
		lim_process_set_he_bss_color(mac_ctx, msg->bodyptr);
		qdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case eWNI_SME_DEL_ALL_TDLS_PEERS:
		lim_process_sme_del_all_tdls_peers(mac_ctx, msg->bodyptr);
		qdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case WMA_OBSS_DETECTION_INFO:
		lim_process_obss_detection_ind(mac_ctx, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case eWNI_SME_SEND_SAE_MSG:
		lim_process_sae_msg(mac_ctx, msg->bodyptr);
		qdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case WMA_OBSS_COLOR_COLLISION_INFO:
		lim_process_obss_color_collision_info(mac_ctx, msg->bodyptr);
		qdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case eWNI_SME_CSA_RESTART_REQ:
		lim_send_csa_restart_req(mac_ctx, msg->bodyval);
		break;
	case eWNI_SME_STA_CSA_CONTINUE_REQ:
		lim_continue_sta_csa_req(mac_ctx, msg->bodyval);
		break;
	case WMA_SEND_BCN_RSP:
		lim_send_bcn_rsp(mac_ctx, (tpSendbeaconParams)msg->bodyptr);
		qdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case WMA_ROAM_BLACKLIST_MSG:
		lim_add_roam_blacklist_ap(mac_ctx,
					  (struct roam_blacklist_event *)
					  msg->bodyptr);
		qdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case SIR_LIM_PROCESS_DEFERRED_QUEUE:
		break;
	case eWNI_SME_ABORT_CONN_TIMER:
		lim_deactivate_timers_for_vdev(mac_ctx, msg->bodyval);
		break;
	default:
		qdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		pe_debug("Discarding unexpected message received %X",
			msg->type);
		lim_print_msg_name(mac_ctx, LOGE, msg->type);
		break;

	} /* switch (msg->type) */
} /*** end lim_process_messages() ***/

/**
 * lim_process_deferred_message_queue()
 *
 * This function is called by LIM while exiting from Learn
 * mode. This function fetches messages posted to the LIM
 * deferred message queue limDeferredMsgQ.
 *
 * @mac: Pointer to Global MAC structure
 * @return None
 */

static void lim_process_deferred_message_queue(struct mac_context *mac)
{
	struct scheduler_msg limMsg = {0};
	struct scheduler_msg *readMsg;
	uint16_t size;

	/*
	** check any deferred messages need to be processed
	**/
	size = mac->lim.gLimDeferredMsgQ.size;
	if (size > 0) {
		while ((readMsg = lim_read_deferred_msg_q(mac)) != NULL) {
			qdf_mem_copy((uint8_t *) &limMsg,
				     (uint8_t *) readMsg,
				     sizeof(struct scheduler_msg));
			size--;
			lim_process_messages(mac, &limMsg);

			if (!GET_LIM_PROCESS_DEFD_MESGS(mac) ||
			    mac->lim.gLimAddtsSent)
				break;
		}
	}
} /*** end lim_process_deferred_message_queue() ***/

/**
 * lim_message_processor() - Process messages from LIM.
 * @mac_ctx: Pointer to the Global Mac Context.
 * @msg: Received LIM message.
 *
 * Wrapper function for lim_process_messages when handling messages received by
 * LIM. Could either defer messages or process them.
 *
 * Return:  None.
 */
void lim_message_processor(struct mac_context *mac_ctx, struct scheduler_msg *msg)
{
	if (eLIM_MLM_OFFLINE_STATE == mac_ctx->lim.gLimMlmState) {
		pe_free_msg(mac_ctx, msg);
		return;
	}

	if (!def_msg_decision(mac_ctx, msg)) {
		lim_process_messages(mac_ctx, msg);
		/* process deferred message queue if allowed */
		if ((!(mac_ctx->lim.gLimAddtsSent)) &&
		    GET_LIM_PROCESS_DEFD_MESGS(mac_ctx))
			lim_process_deferred_message_queue(mac_ctx);
	}
}

/**
 * lim_process_normal_hdd_msg() - Process the message and defer if needed
 * @mac_ctx :     Pointer to Global MAC structure
 * @msg     :     The message need to be processed
 * @rsp_reqd:     whether return result to hdd
 *
 * This function checks the current lim state and decide whether the message
 * passed will be deferred or not.
 *
 * Return: None
 */
static void lim_process_normal_hdd_msg(struct mac_context *mac_ctx,
				       struct scheduler_msg *msg,
				       uint8_t rsp_reqd)
{
	bool defer_msg = true;

	/* Added For BT-AMP Support */
	if ((mac_ctx->lim.gLimSystemRole == eLIM_AP_ROLE)
		|| (mac_ctx->lim.gLimSystemRole == eLIM_UNKNOWN_ROLE)) {
		/*
		 * This check is required only for the AP and in 2 cases.
		 * 1. If we are in learn mode and we receive any of these
		 * messages, you have to come out of scan and process the
		 * message, hence dont defer the message here. In handler,
		 * these message could be defered till we actually come out of
		 * scan mode.
		 * 2. If radar is detected, you might have to defer all of
		 * these messages except Stop BSS request/ Switch channel
		 * request. This decision is also made inside its handler.
		 *
		 * Please be careful while using the flag defer_msg. Possibly
		 * you might end up in an infinite loop.
		 */
		if ((msg->type == eWNI_SME_START_BSS_REQ) ||
			(msg->type == eWNI_SME_STOP_BSS_REQ) ||
			(msg->type == eWNI_SME_SWITCH_CHL_IND))
			defer_msg = false;
	}

	if (mac_ctx->lim.gLimAddtsSent && defer_msg) {
		/*
		 * System is in DFS (Learn) mode or awaiting addts response or
		 * if radar is detected, Defer processing this message
		 */
		if (lim_defer_msg(mac_ctx, msg) != TX_SUCCESS) {
#ifdef WLAN_DEBUG
			mac_ctx->lim.numSme++;
#endif
			lim_log_session_states(mac_ctx);
			/* Release body */
			qdf_mem_free(msg->bodyptr);
			msg->bodyptr = NULL;
		}
	} else {
		/*
		 * These messages are from HDD.Since these requests may also be
		 * generated internally within LIM module, need to distinquish
		 * and send response to host
		 */
		if (rsp_reqd)
			mac_ctx->lim.gLimRspReqd = true;
#ifdef WLAN_DEBUG
		mac_ctx->lim.numSme++;
#endif
		if (lim_process_sme_req_messages(mac_ctx, msg)) {
			/*
			 * Release body. limProcessSmeReqMessage consumed the
			 * buffer. We can free it.
			 */
			qdf_mem_free(msg->bodyptr);
			msg->bodyptr = NULL;
		}
	}
}

void
handle_ht_capabilityand_ht_info(struct mac_context *mac,
				struct pe_session *pe_session)
{
	struct mlme_ht_capabilities_info *ht_cap_info;

	ht_cap_info = &mac->mlme_cfg->ht_caps.ht_cap_info;
	mac->lim.gHTLsigTXOPProtection =
		(uint8_t)ht_cap_info->l_sig_tx_op_protection;
	mac->lim.gHTMIMOPSState =
		(tSirMacHTMIMOPowerSaveState)ht_cap_info->mimo_power_save;
	mac->lim.gHTGreenfield = (uint8_t)ht_cap_info->green_field;
	mac->lim.gHTMaxAmsduLength =
		(uint8_t)ht_cap_info->maximal_amsdu_size;
	mac->lim.gHTShortGI20Mhz = (uint8_t)ht_cap_info->short_gi_20_mhz;
	mac->lim.gHTShortGI40Mhz = (uint8_t)ht_cap_info->short_gi_40_mhz;
	mac->lim.gHTPSMPSupport = (uint8_t)ht_cap_info->psmp;
	mac->lim.gHTDsssCckRate40MHzSupport =
		(uint8_t)ht_cap_info->dsss_cck_mode_40_mhz;

	mac->lim.gHTAMpduDensity =
		(uint8_t)mac->mlme_cfg->ht_caps.ampdu_params.mpdu_density;
	mac->lim.gHTMaxRxAMpduFactor =
		(uint8_t)mac->mlme_cfg->ht_caps.ampdu_params.
		max_rx_ampdu_factor;

	/* Get HT IE Info */
	mac->lim.gHTServiceIntervalGranularity =
		(uint8_t)mac->mlme_cfg->ht_caps.info_field_1.
		service_interval_granularity;
	mac->lim.gHTControlledAccessOnly =
		(uint8_t)mac->mlme_cfg->ht_caps.info_field_1.
		controlled_access_only;

	mac->lim.gHTOperMode = (tSirMacHTOperatingMode)mac->mlme_cfg->ht_caps.
		info_field_2.op_mode;

	mac->lim.gHTPCOActive = (uint8_t)mac->mlme_cfg->ht_caps.info_field_3.
								pco_active;
	mac->lim.gHTPCOPhase = (uint8_t)mac->mlme_cfg->ht_caps.info_field_3.
								pco_phase;
	mac->lim.gHTSecondaryBeacon =
		(uint8_t)mac->mlme_cfg->ht_caps.info_field_3.secondary_beacon;
	mac->lim.gHTDualCTSProtection = (uint8_t)mac->mlme_cfg->ht_caps.
					info_field_3.dual_cts_protection;
	mac->lim.gHTSTBCBasicMCS = (uint8_t)mac->mlme_cfg->ht_caps.
					info_field_3.basic_stbc_mcs;

	/* The lim globals for channelwidth and secondary chnl have been removed and should not be used during no session;
	 * instead direct cfg is read and used when no session for transmission of mgmt frames (same as old);
	 * For now, we might come here during init and join with pe_session = NULL; in that case just fill the globals which exist
	 * Sessionized entries values will be filled in join or add bss req. The ones which are missed in join are filled below
	 */
	if (pe_session) {
		pe_session->htCapability =
			IS_DOT11_MODE_HT(pe_session->dot11mode);
		pe_session->beaconParams.fLsigTXOPProtectionFullSupport =
			(uint8_t)mac->mlme_cfg->ht_caps.info_field_3.
			lsig_txop_protection_full_support;
		lim_init_obss_params(mac, pe_session);
	}
}

void lim_log_session_states(struct mac_context *mac_ctx)
{
#ifdef WLAN_DEBUG
	int i;

	for (i = 0; i < mac_ctx->lim.maxBssId; i++) {
		if (mac_ctx->lim.gpSession[i].valid) {
			QDF_TRACE(QDF_MODULE_ID_PE, LOGD,
				FL("sysRole(%d) Session (%d)"),
				mac_ctx->lim.gLimSystemRole, i);
			QDF_TRACE(QDF_MODULE_ID_PE, LOGD,
				FL("SME: Curr %s,Prev %s,MLM: Curr %s,Prev %s"),
				lim_sme_state_str(
				mac_ctx->lim.gpSession[i].limSmeState),
				lim_sme_state_str(
				mac_ctx->lim.gpSession[i].limPrevSmeState),
				lim_mlm_state_str(
				mac_ctx->lim.gpSession[i].limMlmState),
				lim_mlm_state_str(
				mac_ctx->lim.gpSession[i].limPrevMlmState));
		}
	}
#endif
}
