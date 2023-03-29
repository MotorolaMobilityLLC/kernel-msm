/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
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
 * DOC: csr_roam_preauth.c
 *
 * Host based roaming preauthentication implementation
 */

#include "wma_types.h"
#include "csr_inside_api.h"
#include "sme_qos_internal.h"
#include "sme_inside.h"
#include "host_diag_core_event.h"
#include "host_diag_core_log.h"
#include "csr_api.h"
#include "sme_api.h"
#include "csr_neighbor_roam.h"
#include "mac_trace.h"
#include "wlan_policy_mgr_api.h"
#include "sir_api.h"

static QDF_STATUS csr_neighbor_roam_add_preauth_fail(struct mac_context *mac_ctx,
			uint8_t session_id, tSirMacAddr bssid);

/**
 * csr_neighbor_roam_state_preauth_done() - Check if state is preauth done
 * @mac_ctx: Global MAC context
 * @session_id: SME session ID
 *
 * Return: True if the state id preauth done, false otherwise
 */
bool csr_neighbor_roam_state_preauth_done(struct mac_context *mac_ctx,
		uint8_t session_id)
{
	return mac_ctx->roam.neighborRoamInfo[session_id].neighborRoamState ==
		eCSR_NEIGHBOR_ROAM_STATE_PREAUTH_DONE;
}

/**
 * csr_neighbor_roam_tranistion_preauth_done_to_disconnected() - Transition
 * the state from preauth done to disconnected
 * @mac_ctx: Global MAC Context
 * @session_id: SME Session ID
 *
 * In the event that we are associated with AP1 and we have
 * completed pre auth with AP2. Then we receive a deauth/disassoc from AP1.
 * At this point neighbor roam is in pre auth done state, pre auth timer
 * is running. We now handle this case by stopping timer and clearing
 * the pre-auth state. We basically clear up and just go to disconnected
 * state
 *
 * Return: None
 */
void csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
		struct mac_context *mac_ctx, uint8_t session_id)
{
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo =
		&mac_ctx->roam.neighborRoamInfo[session_id];
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);

	if (!session) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  FL("session is NULL"));
		return;
	}

	if (pNeighborRoamInfo->neighborRoamState !=
	    eCSR_NEIGHBOR_ROAM_STATE_PREAUTH_DONE)
		return;

	qdf_mc_timer_stop(&session->ftSmeContext.preAuthReassocIntvlTimer);
	csr_neighbor_roam_state_transition(mac_ctx,
		eCSR_NEIGHBOR_ROAM_STATE_INIT, session_id);
	pNeighborRoamInfo->roamChannelInfo.IAPPNeighborListReceived = false;
	pNeighborRoamInfo->uOsRequestedHandoff = 0;
}

/**
 * csr_roam_enqueue_preauth() - Put the preauth command in the queue
 * @mac_ctx: Global MAC Context
 * @session_id: SME Session ID
 * @bss_desc: BSS descriptor
 * @reason: roaming reason
 * @immediate: High priority in the queue or not
 *
 * Return: Success if queued properly, false otherwise.
 */
QDF_STATUS csr_roam_enqueue_preauth(struct mac_context *mac_ctx,
				    uint32_t session_id,
				    struct bss_description *bss_desc,
				    enum csr_roam_reason reason, bool immediate)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSmeCmd *command;

	command = csr_get_command_buffer(mac_ctx);
	if (!command) {
		sme_err("fail to get command buffer");
		status = QDF_STATUS_E_RESOURCES;
	} else {
		if (bss_desc) {
			command->command = eSmeCommandRoam;
			command->vdev_id = (uint8_t) session_id;
			command->u.roamCmd.roamReason = reason;
			command->u.roamCmd.pLastRoamBss = bss_desc;
			status = csr_queue_sme_command(mac_ctx, command,
					immediate);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				sme_err("fail to queue preauth,status: %d",
					status);
			}
		} else {
			status = QDF_STATUS_E_RESOURCES;
		}
	}
	return status;
}

/**
 * csr_neighbor_roam_purge_preauth_failed_list() - Purge the preauth fail list
 * @mac_ctx: Global MAC Context
 *
 * Return: None
 */
void csr_neighbor_roam_purge_preauth_failed_list(struct mac_context *mac_ctx)
{
	uint8_t i;
	uint8_t j;
	uint8_t num_mac_addr;
	tpCsrNeighborRoamControlInfo neigh_roam_info = NULL;
	tpCsrPreauthFailListInfo fail_list;

	for (j = 0; j < WLAN_MAX_VDEVS; j++) {
		neigh_roam_info = &mac_ctx->roam.neighborRoamInfo[j];
		fail_list = &neigh_roam_info->FTRoamInfo.preAuthFailList;
		num_mac_addr = fail_list->numMACAddress;
		for (i = 0; i < num_mac_addr; i++)
			qdf_mem_zero(fail_list->macAddress[i],
					sizeof(tSirMacAddr));
		fail_list->numMACAddress = 0;
	}
}

/**
 * @csr_neighbor_roam_reset_preauth_control_info - Reset preauth info
 * @mac_ctx: Global MAC Context
 * @session_id: SME Session ID
 *
 * Return: None
 */
void csr_neighbor_roam_reset_preauth_control_info(struct mac_context *mac_ctx,
		uint8_t session_id)
{
	tpCsrNeighborRoamControlInfo neigh_roam_info =
		&mac_ctx->roam.neighborRoamInfo[session_id];

	neigh_roam_info->is11rAssoc = false;
	csr_neighbor_roam_purge_preauth_failed_list(mac_ctx);

	neigh_roam_info->FTRoamInfo.preauthRspPending = false;
	neigh_roam_info->FTRoamInfo.numPreAuthRetries = 0;
	neigh_roam_info->FTRoamInfo.currentNeighborRptRetryNum = 0;
	neigh_roam_info->FTRoamInfo.neighborRptPending = false;
	neigh_roam_info->uOsRequestedHandoff = 0;
	qdf_mem_zero(&neigh_roam_info->handoffReqInfo,
		     sizeof(tCsrHandoffRequest));
}

/**
 * csr_neighbor_roam_preauth_rsp_handler() - handle preauth response
 * @mac_ctx: The handle returned by mac_open.
 * @session_id: SME session
 * @lim_status: QDF_STATUS_SUCCESS/QDF_STATUS_E_FAILURE/QDF_STATUS_E_NOSPC/
 *              eSIT_LIM_AUTH_RSP_TIMEOUT status from PE
 *
 * This function handle the Preauth response from PE
 * Every preauth is allowed max 3 tries if it fails. If a bssid failed
 * for more than MAX_TRIES, we will remove it from the list and try
 * with the next node in the roamable AP list and add the BSSID to
 * pre-auth failed list. If no more entries present in roamable AP list,
 * transition to REPORT_SCAN state.
 *
 * Return: QDF_STATUS_SUCCESS on success (i.e. pre-auth processed),
 *         QDF_STATUS_E_FAILURE otherwise
 */
QDF_STATUS csr_neighbor_roam_preauth_rsp_handler(struct mac_context *mac_ctx,
						 uint8_t session_id,
						 QDF_STATUS lim_status)
{
	tpCsrNeighborRoamControlInfo neighbor_roam_info =
		&mac_ctx->roam.neighborRoamInfo[session_id];
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	QDF_STATUS preauth_processed = QDF_STATUS_SUCCESS;
	tpCsrNeighborRoamBSSInfo preauth_rsp_node = NULL;
	uint8_t reason;

	if (false == neighbor_roam_info->FTRoamInfo.preauthRspPending) {
		/*
		 * This can happen when we disconnect immediately
		 * after sending a pre-auth request. During processing
		 * of the disconnect command, we would have reset
		 * preauthRspPending and transitioned to INIT state.
		 */
		sme_warn("Unexpected pre-auth response in state %d",
			neighbor_roam_info->neighborRoamState);
		preauth_processed = QDF_STATUS_E_FAILURE;
		goto DEQ_PREAUTH;
	}
	/* We can receive it in these 2 states. */
	if ((neighbor_roam_info->neighborRoamState !=
	     eCSR_NEIGHBOR_ROAM_STATE_PREAUTHENTICATING)) {
		sme_debug("Preauth response received in state %s",
			mac_trace_get_neighbour_roam_state
				(neighbor_roam_info->neighborRoamState));
		preauth_processed = QDF_STATUS_E_FAILURE;
		goto DEQ_PREAUTH;
	}

	neighbor_roam_info->FTRoamInfo.preauthRspPending = false;

	if (QDF_STATUS_SUCCESS == lim_status)
		preauth_rsp_node =
			csr_neighbor_roam_next_roamable_ap(
				mac_ctx, &neighbor_roam_info->roamableAPList,
				NULL);
	if ((QDF_STATUS_SUCCESS == lim_status) && (preauth_rsp_node)) {
		sme_debug("Preauth completed successfully after %d tries",
			  neighbor_roam_info->FTRoamInfo.numPreAuthRetries);
		sme_debug("After Pre-Auth: BSSID " QDF_MAC_ADDR_FMT ", ChFq:%d",
			  QDF_MAC_ADDR_REF(
				preauth_rsp_node->pBssDescription->bssId),
			  preauth_rsp_node->pBssDescription->chan_freq);

		csr_neighbor_roam_send_lfr_metric_event(mac_ctx, session_id,
			preauth_rsp_node->pBssDescription->bssId,
			eCSR_ROAM_PREAUTH_STATUS_SUCCESS);
		/*
		 * Preauth completed successfully. Insert the preauthenticated
		 * node to tail of preAuthDoneList.
		 */
		csr_neighbor_roam_remove_roamable_ap_list_entry(mac_ctx,
			&neighbor_roam_info->roamableAPList,
			preauth_rsp_node);
		csr_ll_insert_tail(
			&neighbor_roam_info->FTRoamInfo.preAuthDoneList,
			&preauth_rsp_node->List, LL_ACCESS_LOCK);

		csr_neighbor_roam_state_transition(mac_ctx,
			eCSR_NEIGHBOR_ROAM_STATE_PREAUTH_DONE, session_id);
		neighbor_roam_info->FTRoamInfo.numPreAuthRetries = 0;

		/*
		 * The caller of this function would start a timer and by
		 * the time it expires, supplicant should have provided
		 * the updated FTIEs to SME. So, when it expires, handoff
		 * will be triggered then.
		 */
	} else {
		tpCsrNeighborRoamBSSInfo neighbor_bss_node = NULL;
		tListElem *entry;
		bool is_dis_pending = false;

		sme_err("Preauth failed retry number %d, status: 0x%x",
			neighbor_roam_info->FTRoamInfo.numPreAuthRetries,
			lim_status);

		/*
		 * Preauth failed. Add the bssId to the preAuth failed list
		 * of MAC Address. Also remove the AP from roamable AP list.
		 */
		if ((neighbor_roam_info->FTRoamInfo.numPreAuthRetries >=
		     CSR_NEIGHBOR_ROAM_MAX_NUM_PREAUTH_RETRIES) ||
		    (QDF_STATUS_E_NOSPC == lim_status)) {
			/*
			 * We are going to remove the node as it fails for
			 * more than MAX tries. Reset this count to 0
			 */
			neighbor_roam_info->FTRoamInfo.numPreAuthRetries = 0;

			/*
			 * The one in the head of the list should be one with
			 * which we issued pre-auth and failed
			 */
			entry = csr_ll_remove_head(
					&neighbor_roam_info->roamableAPList,
					LL_ACCESS_LOCK);
			if (!entry) {
				sme_err("Preauth list is empty");
				goto NEXT_PREAUTH;
			}
			neighbor_bss_node = GET_BASE_ADDR(entry,
					tCsrNeighborRoamBSSInfo,
					List);
			/*
			 * Add the BSSID to pre-auth fail list if
			 * it is not requested by HDD
			 */
			if (!neighbor_roam_info->uOsRequestedHandoff)
				status =
					csr_neighbor_roam_add_preauth_fail(
						mac_ctx, session_id,
						neighbor_bss_node->
							pBssDescription->bssId);
			csr_neighbor_roam_send_lfr_metric_event(mac_ctx,
				session_id,
				neighbor_bss_node->pBssDescription->bssId,
				eCSR_ROAM_PREAUTH_STATUS_FAILURE);
			/* Now we can free this node */
			csr_neighbor_roam_free_neighbor_roam_bss_node(
				mac_ctx, neighbor_bss_node);
		}
NEXT_PREAUTH:
		is_dis_pending = is_disconnect_pending(mac_ctx, session_id);
		if (is_dis_pending) {
			sme_err("Disconnect in progress, Abort preauth");
			goto ABORT_PREAUTH;
		}
		/* Issue preauth request for the same/next entry */
		if (QDF_STATUS_SUCCESS == csr_neighbor_roam_issue_preauth_req(
						mac_ctx, session_id))
			goto DEQ_PREAUTH;
ABORT_PREAUTH:
		if (csr_roam_is_roam_offload_scan_enabled(mac_ctx)) {
			reason = REASON_PREAUTH_FAILED_FOR_ALL;
			if (neighbor_roam_info->uOsRequestedHandoff) {
				neighbor_roam_info->uOsRequestedHandoff = 0;
				csr_post_roam_state_change(
						   mac_ctx, session_id,
						   WLAN_ROAM_RSO_ENABLED,
						   reason);
			} else {
				/* ROAM_SCAN_OFFLOAD_RESTART is a
				 * special command to trigger bmiss
				 * handler internally for LFR2 all candidate
				 * preauth failure.
				 * This should be decoupled from RSO.
				 */
				csr_roam_offload_scan(mac_ctx, session_id,
						      ROAM_SCAN_OFFLOAD_RESTART,
						      reason);
			}
			csr_neighbor_roam_state_transition(mac_ctx,
				eCSR_NEIGHBOR_ROAM_STATE_CONNECTED, session_id);
		}
	}

DEQ_PREAUTH:
	csr_dequeue_roam_command(mac_ctx, eCsrPerformPreauth, session_id);
	return preauth_processed;
}

/**
 * csr_neighbor_roam_add_preauth_fail() - add bssid to preauth failed list
 * @mac_ctx: The handle returned by mac_open.
 * @bssid: BSSID to be added to the preauth fail list
 *
 * This function adds the given BSSID to the Preauth fail list
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_FAILURE otherwise
 */
static QDF_STATUS csr_neighbor_roam_add_preauth_fail(struct mac_context *mac_ctx,
			uint8_t session_id, tSirMacAddr bssid)
{
	uint8_t i = 0;
	tpCsrNeighborRoamControlInfo neighbor_roam_info =
		&mac_ctx->roam.neighborRoamInfo[session_id];
	uint8_t num_mac_addr = neighbor_roam_info->FTRoamInfo.preAuthFailList.
				numMACAddress;

	sme_warn("Added BSSID " QDF_MAC_ADDR_FMT " to Preauth failed list",
		QDF_MAC_ADDR_REF(bssid));

	for (i = 0;
	     i < neighbor_roam_info->FTRoamInfo.preAuthFailList.numMACAddress;
	     i++) {
		if (!qdf_mem_cmp(
		   neighbor_roam_info->FTRoamInfo.preAuthFailList.macAddress[i],
		   bssid, sizeof(tSirMacAddr))) {
			sme_warn("BSSID "QDF_MAC_ADDR_FMT" already fail list",
			QDF_MAC_ADDR_REF(bssid));
			return QDF_STATUS_SUCCESS;
		}
	}

	if ((num_mac_addr + 1) > MAX_NUM_PREAUTH_FAIL_LIST_ADDRESS) {
		sme_err("Cannot add, preauth fail list is full");
		return QDF_STATUS_E_FAILURE;
	}
	qdf_mem_copy(neighbor_roam_info->FTRoamInfo.preAuthFailList.
		     macAddress[num_mac_addr], bssid, sizeof(tSirMacAddr));
	neighbor_roam_info->FTRoamInfo.preAuthFailList.numMACAddress++;

	return QDF_STATUS_SUCCESS;
}

/**
 * csr_neighbor_roam_is_preauth_candidate()
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @bssId : BSSID of the candidate
 *
 * This function checks whether the given MAC address is already present
 * in the preauth fail list and returns true/false accordingly
 *
 * Return: true if preauth candidate, false otherwise
 */
bool csr_neighbor_roam_is_preauth_candidate(struct mac_context *mac,
		    uint8_t sessionId, tSirMacAddr bssId)
{
	uint8_t i = 0;
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo =
		&mac->roam.neighborRoamInfo[sessionId];

	if (csr_roam_is_roam_offload_scan_enabled(mac))
		return true;
	if (0 == pNeighborRoamInfo->FTRoamInfo.preAuthFailList.numMACAddress)
		return true;

	for (i = 0;
	     i < pNeighborRoamInfo->FTRoamInfo.preAuthFailList.numMACAddress;
	     i++) {
		if (!qdf_mem_cmp(pNeighborRoamInfo->FTRoamInfo.
				    preAuthFailList.macAddress[i], bssId,
				    sizeof(tSirMacAddr))) {
			sme_err("BSSID exists in fail list" QDF_MAC_ADDR_FMT,
					QDF_MAC_ADDR_REF(bssId));
			return false;
		}
	}

	return true;
}

/**
 * csr_get_dot11_mode() - Derives dot11mode
 * @mac_ctx: Global MAC context
 * @session_id: SME Session ID
 * @bss_desc: BSS descriptor
 *
 * Return: dot11mode
 */
static uint32_t csr_get_dot11_mode(struct mac_context *mac_ctx,
				   uint32_t session_id,
				   struct bss_description *bss_desc)
{
	struct csr_roam_session *csr_session = CSR_GET_SESSION(mac_ctx,
				session_id);
	enum csr_cfgdot11mode ucfg_dot11_mode, cfg_dot11_mode;
	QDF_STATUS status;
	tDot11fBeaconIEs *ies_local = NULL;
	uint32_t dot11mode = 0;

	if (!csr_session) {
		sme_err("Invalid session id %d", session_id);
		return 0;
	}

	sme_debug("phyMode %d", csr_session->pCurRoamProfile->phyMode);

	/* Get IE's */
	status = csr_get_parsed_bss_description_ies(mac_ctx, bss_desc,
							&ies_local);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("csr_get_parsed_bss_description_ies failed");
		return 0;
	}
	if (!ies_local) {
		sme_err("ies_local is NULL");
		return 0;
	}

	if (csr_is_phy_mode_match(mac_ctx,
			csr_session->pCurRoamProfile->phyMode,
			bss_desc, csr_session->pCurRoamProfile,
			&cfg_dot11_mode, ies_local))
		ucfg_dot11_mode = cfg_dot11_mode;
	else {
		sme_err("Can not find match phy mode");
		if (WLAN_REG_IS_5GHZ_CH_FREQ(bss_desc->chan_freq))
			ucfg_dot11_mode = eCSR_CFG_DOT11_MODE_11A;
		else
			ucfg_dot11_mode = eCSR_CFG_DOT11_MODE_11G;
	}

	/* dot11mode */
	dot11mode = csr_translate_to_wni_cfg_dot11_mode(mac_ctx,
							ucfg_dot11_mode);
	sme_debug("dot11mode %d ucfg_dot11_mode %d",
			dot11mode, ucfg_dot11_mode);

	if (bss_desc->chan_freq <= CDS_CHAN_14_FREQ &&
	    !mac_ctx->mlme_cfg->vht_caps.vht_cap_info.b24ghz_band &&
	    MLME_DOT11_MODE_11AC == dot11mode) {
		/* Need to disable VHT operation in 2.4 GHz band */
		dot11mode = MLME_DOT11_MODE_11N;
	}
	qdf_mem_free(ies_local);
	return dot11mode;
}

QDF_STATUS csr_roam_issue_ft_preauth_req(struct mac_context *mac_ctx,
					 uint32_t vdev_id,
					 struct bss_description *bss_desc)
{
	tpSirFTPreAuthReq preauth_req;
	uint16_t auth_req_len;
	struct bss_description *buf;
	uint32_t dot11mode, buf_len;
	QDF_STATUS status;
	struct csr_roam_session *csr_session = CSR_GET_SESSION(mac_ctx,
				vdev_id);

	if (!csr_session) {
		sme_err("Session does not exist for vdev_id: %d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	dot11mode = csr_get_dot11_mode(mac_ctx, vdev_id, bss_desc);
	if (!dot11mode) {
		sme_err("dot11mode is zero");
		return QDF_STATUS_E_FAILURE;
	}

	auth_req_len = sizeof(tSirFTPreAuthReq);
	preauth_req = qdf_mem_malloc(auth_req_len);
	if (!preauth_req)
		return QDF_STATUS_E_NOMEM;

	buf_len = sizeof(bss_desc->length) + bss_desc->length;
	buf = qdf_mem_malloc(buf_len);
	if (!buf) {
		qdf_mem_free(preauth_req);
		return QDF_STATUS_E_NOMEM;
	}

	/* Save the SME Session ID. We need it while processing preauth resp */
	csr_session->ftSmeContext.vdev_id = vdev_id;
	preauth_req->messageType = eWNI_SME_FT_PRE_AUTH_REQ;
	preauth_req->pre_auth_channel_freq = bss_desc->chan_freq;
	preauth_req->dot11mode = dot11mode;

	qdf_mem_copy((void *)&preauth_req->currbssId,
			(void *)csr_session->connectedProfile.bssid.bytes,
			sizeof(tSirMacAddr));
	qdf_mem_copy((void *)&preauth_req->preAuthbssId,
			(void *)bss_desc->bssId, sizeof(tSirMacAddr));
	qdf_mem_copy((void *)&preauth_req->self_mac_addr,
		(void *)&csr_session->self_mac_addr.bytes, sizeof(tSirMacAddr));

	if (csr_roam_is11r_assoc(mac_ctx, vdev_id) &&
	     (mac_ctx->roam.roamSession[vdev_id].connectedProfile.AuthType !=
	      eCSR_AUTH_TYPE_OPEN_SYSTEM)) {
		preauth_req->ft_ies_length =
			(uint16_t) csr_session->ftSmeContext.auth_ft_ies_length;
		qdf_mem_copy(preauth_req->ft_ies,
				csr_session->ftSmeContext.auth_ft_ies,
				csr_session->ftSmeContext.auth_ft_ies_length);
	} else {
		preauth_req->ft_ies_length = 0;
	}
	qdf_mem_copy(buf, bss_desc, buf_len);
	preauth_req->pbssDescription = buf;
	preauth_req->length = auth_req_len;

	status = umac_send_mb_message_to_mac(preauth_req);
	if (QDF_IS_STATUS_ERROR(status))
		qdf_mem_free(buf);

	return status;
}

void csr_roam_ft_pre_auth_rsp_processor(struct mac_context *mac_ctx,
					tpSirFTPreAuthRsp preauth_rsp)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_info *roam_info;
	enum csr_akm_type conn_Auth_type;
	uint32_t vdev_id = preauth_rsp->vdev_id;
	struct csr_roam_session *csr_session = CSR_GET_SESSION(mac_ctx,
				vdev_id);
	tDot11fAuthentication *p_auth = NULL;

	if (!csr_session) {
		sme_err("CSR session is NULL");
		return;
	}
	status = csr_neighbor_roam_preauth_rsp_handler(mac_ctx,
			preauth_rsp->vdev_id, preauth_rsp->status);
	if (status != QDF_STATUS_SUCCESS) {
		sme_err("Preauth was not processed: %d SessionID: %d",
			status, vdev_id);
		return;
	}

	if (QDF_STATUS_SUCCESS != (QDF_STATUS) preauth_rsp->status)
		return;
	csr_session->ftSmeContext.FTState = eFT_AUTH_COMPLETE;
	csr_session->ftSmeContext.psavedFTPreAuthRsp = preauth_rsp;
	/* No need to notify qos module if this is a non 11r & ESE roam */
	if (csr_roam_is11r_assoc(mac_ctx, preauth_rsp->vdev_id)
#ifdef FEATURE_WLAN_ESE
		|| csr_roam_is_ese_assoc(mac_ctx, preauth_rsp->vdev_id)
#endif
	   ) {
		sme_qos_csr_event_ind(mac_ctx,
			csr_session->ftSmeContext.vdev_id,
			SME_QOS_CSR_PREAUTH_SUCCESS_IND, NULL);
	}
	status =
		qdf_mc_timer_start(
			&csr_session->ftSmeContext.preAuthReassocIntvlTimer,
			60);
	if (QDF_STATUS_SUCCESS != status) {
		sme_err("PreauthReassocInterval timer failed status %d",
			status);
		return;
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	qdf_mem_copy((void *)&csr_session->ftSmeContext.preAuthbssId,
		(void *)preauth_rsp->preAuthbssId,
		sizeof(struct qdf_mac_addr));
	if (csr_roam_is11r_assoc(mac_ctx, preauth_rsp->vdev_id))
		csr_roam_call_callback(mac_ctx, preauth_rsp->vdev_id,
			NULL, 0, eCSR_ROAM_FT_RESPONSE, eCSR_ROAM_RESULT_NONE);

#ifdef FEATURE_WLAN_ESE
	if (csr_roam_is_ese_assoc(mac_ctx, preauth_rsp->vdev_id)) {
		csr_roam_read_tsf(mac_ctx, (uint8_t *)&roam_info->timestamp,
				  preauth_rsp->vdev_id);
		qdf_mem_copy((void *)&roam_info->bssid,
			     (void *)preauth_rsp->preAuthbssId,
			     sizeof(struct qdf_mac_addr));
		csr_roam_call_callback(mac_ctx, preauth_rsp->vdev_id,
				       roam_info, 0,
				       eCSR_ROAM_CCKM_PREAUTH_NOTIFY, 0);
	}
#endif

	if (csr_roam_is_fast_roam_enabled(mac_ctx, preauth_rsp->vdev_id)) {
		/* Save the bssid from the received response */
		qdf_mem_copy((void *)&roam_info->bssid,
			     (void *)preauth_rsp->preAuthbssId,
			     sizeof(struct qdf_mac_addr));
		csr_roam_call_callback(mac_ctx, preauth_rsp->vdev_id,
				       roam_info, 0, eCSR_ROAM_PMK_NOTIFY, 0);
	}

	qdf_mem_free(roam_info);

	/* If its an Open Auth, FT IEs are not provided by supplicant */
	/* Hence populate them here */
	conn_Auth_type =
		mac_ctx->roam.roamSession[vdev_id].connectedProfile.AuthType;

	csr_session->ftSmeContext.addMDIE = false;

	/* Done with it, init it. */
	csr_session->ftSmeContext.psavedFTPreAuthRsp = NULL;

	if (csr_roam_is11r_assoc(mac_ctx, preauth_rsp->vdev_id) &&
			(conn_Auth_type == eCSR_AUTH_TYPE_OPEN_SYSTEM)) {
		uint16_t ft_ies_length;

		ft_ies_length = preauth_rsp->ric_ies_length;

		if ((csr_session->ftSmeContext.reassoc_ft_ies) &&
			(csr_session->ftSmeContext.reassoc_ft_ies_length)) {
			qdf_mem_free(csr_session->ftSmeContext.reassoc_ft_ies);
			csr_session->ftSmeContext.reassoc_ft_ies_length = 0;
			csr_session->ftSmeContext.reassoc_ft_ies = NULL;
		}
		p_auth = (tDot11fAuthentication *) qdf_mem_malloc(
						sizeof(tDot11fAuthentication));

		if (!p_auth)
			return;

		status = dot11f_unpack_authentication(mac_ctx,
				preauth_rsp->ft_ies,
				preauth_rsp->ft_ies_length, p_auth, false);
		if (DOT11F_FAILED(status))
			sme_err("Failed to parse an Authentication frame");
		else if (p_auth->MobilityDomain.present)
			csr_session->ftSmeContext.addMDIE = true;

		qdf_mem_free(p_auth);

		if (!ft_ies_length)
			return;

		csr_session->ftSmeContext.reassoc_ft_ies =
			qdf_mem_malloc(ft_ies_length);
		if (!csr_session->ftSmeContext.reassoc_ft_ies)
			return;

		/* Copy the RIC IEs to reassoc IEs */
		qdf_mem_copy(((uint8_t *) csr_session->ftSmeContext.
					reassoc_ft_ies),
					(uint8_t *) preauth_rsp->ric_ies,
					preauth_rsp->ric_ies_length);
		csr_session->ftSmeContext.reassoc_ft_ies_length = ft_ies_length;
		csr_session->ftSmeContext.addMDIE = true;
	}
}

/**
 * csr_neighbor_roam_issue_preauth_req() - Send preauth request to first AP
 * @mac_ctx: The handle returned by mac_open.
 * @session_id: Session information
 *
 * This function issues preauth request to PE with the 1st AP entry in the
 * roamable AP list
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_FAILURE otherwise
 */
QDF_STATUS csr_neighbor_roam_issue_preauth_req(struct mac_context *mac_ctx,
						      uint8_t session_id)
{
	tpCsrNeighborRoamControlInfo neighbor_roam_info =
		&mac_ctx->roam.neighborRoamInfo[session_id];
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tpCsrNeighborRoamBSSInfo neighbor_bss_node;


	if (false != neighbor_roam_info->FTRoamInfo.preauthRspPending) {
		/* This must not be true here */
		QDF_ASSERT(neighbor_roam_info->FTRoamInfo.preauthRspPending ==
			   false);
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * Issue Preauth request to PE here.
	 * Need to issue the preauth request with the BSSID that is in the
	 * head of the roamable AP list. Parameters that should be passed are
	 * BSSID, Channel number and the neighborScanPeriod(probably). If
	 * roamableAPList gets empty, should transition to REPORT_SCAN state
	 */
	neighbor_bss_node = csr_neighbor_roam_next_roamable_ap(mac_ctx,
				&neighbor_roam_info->roamableAPList, NULL);

	if (!neighbor_bss_node) {
		sme_err("Roamable AP list is empty");
		return QDF_STATUS_E_FAILURE;
	}
	csr_neighbor_roam_send_lfr_metric_event(mac_ctx, session_id,
			neighbor_bss_node->pBssDescription->bssId,
			eCSR_ROAM_PREAUTH_INIT_NOTIFY);
	status = csr_roam_enqueue_preauth(mac_ctx, session_id,
				neighbor_bss_node->pBssDescription,
				eCsrPerformPreauth, true);

	sme_debug("Before Pre-Auth: BSSID " QDF_MAC_ADDR_FMT ", Ch:%d",
		  QDF_MAC_ADDR_REF(neighbor_bss_node->pBssDescription->bssId),
		  neighbor_bss_node->pBssDescription->chan_freq);

	if (QDF_STATUS_SUCCESS != status) {
		sme_err("Return failed preauth request status %d",
			status);
		return status;
	}

	neighbor_roam_info->FTRoamInfo.preauthRspPending = true;
	neighbor_roam_info->FTRoamInfo.numPreAuthRetries++;
	csr_neighbor_roam_state_transition(mac_ctx,
		eCSR_NEIGHBOR_ROAM_STATE_PREAUTHENTICATING, session_id);

	return status;
}

