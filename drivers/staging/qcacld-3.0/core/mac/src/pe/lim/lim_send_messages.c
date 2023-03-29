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
 *
 * lim_send_messages.c: Provides functions to send messages or Indications to HAL.
 * Author:    Sunit Bhatia
 * Date:       09/21/2006
 * History:-
 * Date        Modified by            Modification Information
 *
 * --------------------------------------------------------------------------
 *
 */
#include "lim_send_messages.h"
#include "lim_trace.h"
#include "wlan_reg_services_api.h"
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
#include "host_diag_core_log.h"
#endif /* FEATURE_WLAN_DIAG_SUPPORT */
#include "lim_utils.h"
#include "wma.h"
#include "../../core/src/vdev_mgr_ops.h"

/**
 * lim_send_beacon_params() - updates bcn params to WMA
 *
 * @mac                 : pointer to Global Mac structure.
 * @tpUpdateBeaconParams : pointer to the structure, which contains the beacon
 * parameters which are changed.
 *
 * This function is called to send beacon interval, short preamble or other
 * parameters to WMA, which are changed and indication is received in beacon.
 *
 * @return success if message send is ok, else false.
 */
QDF_STATUS lim_send_beacon_params(struct mac_context *mac,
				     tpUpdateBeaconParams pUpdatedBcnParams,
				     struct pe_session *pe_session)
{
	tpUpdateBeaconParams pBcnParams = NULL;
	QDF_STATUS retCode = QDF_STATUS_SUCCESS;
	struct scheduler_msg msgQ = {0};

	pBcnParams = qdf_mem_malloc(sizeof(*pBcnParams));
	if (!pBcnParams)
		return QDF_STATUS_E_NOMEM;
	qdf_mem_copy((uint8_t *) pBcnParams, pUpdatedBcnParams,
		     sizeof(*pBcnParams));
	msgQ.type = WMA_UPDATE_BEACON_IND;
	msgQ.reserved = 0;
	msgQ.bodyptr = pBcnParams;
	msgQ.bodyval = 0;
	pe_debug("Sending WMA_UPDATE_BEACON_IND, paramChangeBitmap in hex: %x",
	       pUpdatedBcnParams->paramChangeBitmap);
	if (!pe_session) {
		qdf_mem_free(pBcnParams);
		MTRACE(mac_trace_msg_tx(mac, NO_SESSION, msgQ.type));
		return QDF_STATUS_E_FAILURE;
	} else {
		MTRACE(mac_trace_msg_tx(mac,
					pe_session->peSessionId,
					msgQ.type));
	}
	pBcnParams->vdev_id = pe_session->vdev_id;
	retCode = wma_post_ctrl_msg(mac, &msgQ);
	if (QDF_STATUS_SUCCESS != retCode) {
		qdf_mem_free(pBcnParams);
		pe_err("Posting WMA_UPDATE_BEACON_IND, reason=%X",
			retCode);
	}
	lim_send_beacon_ind(mac, pe_session, REASON_DEFAULT);
	return retCode;
}

QDF_STATUS lim_send_switch_chnl_params(struct mac_context *mac,
				       struct pe_session *session)
{
	struct vdev_mlme_obj *mlme_obj;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct vdev_start_response rsp = {0};
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma) {
		pe_err("Invalid wma handle");
		return QDF_STATUS_E_FAILURE;
	}
	if (!session) {
		pe_err("session is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	if (!session->vdev) {
		pe_err("vdev is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(session->vdev);
	if (!mlme_obj) {
		pe_err("vdev component object is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	status = lim_pre_vdev_start(mac, mlme_obj, session);
	if (QDF_IS_STATUS_ERROR(status))
		goto send_resp;

	session->ch_switch_in_progress = true;

	/* we need to defer the message until we
	 * get the response back from WMA
	 */
	SET_LIM_PROCESS_DEFD_MESGS(mac, false);

	status = wma_pre_chan_switch_setup(session->vdev_id);
	if (status != QDF_STATUS_SUCCESS) {
		pe_err("failed status = %d", status);
		goto send_resp;
	}
	status = vdev_mgr_start_send(mlme_obj,
				mlme_is_chan_switch_in_progress(session->vdev));
	if (status != QDF_STATUS_SUCCESS) {
		pe_err("failed status = %d", status);
		goto send_resp;
	}
	wma_post_chan_switch_setup(session->vdev_id);

	return QDF_STATUS_SUCCESS;
send_resp:
	rsp.status = status;
	rsp.vdev_id = session->vdev_id;

	wma_handle_channel_switch_resp(wma, &rsp);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS lim_send_edca_params(struct mac_context *mac,
				tSirMacEdcaParamRecord *pUpdatedEdcaParams,
				uint16_t vdev_id, bool mu_edca)
{
	tEdcaParams *pEdcaParams = NULL;
	QDF_STATUS retCode = QDF_STATUS_SUCCESS;
	struct scheduler_msg msgQ = {0};

	pEdcaParams = qdf_mem_malloc(sizeof(tEdcaParams));
	if (!pEdcaParams)
		return QDF_STATUS_E_NOMEM;
	pEdcaParams->vdev_id = vdev_id;
	pEdcaParams->acbe = pUpdatedEdcaParams[QCA_WLAN_AC_BE];
	pEdcaParams->acbk = pUpdatedEdcaParams[QCA_WLAN_AC_BK];
	pEdcaParams->acvi = pUpdatedEdcaParams[QCA_WLAN_AC_VI];
	pEdcaParams->acvo = pUpdatedEdcaParams[QCA_WLAN_AC_VO];
	pEdcaParams->mu_edca_params = mu_edca;
	msgQ.type = WMA_UPDATE_EDCA_PROFILE_IND;
	msgQ.reserved = 0;
	msgQ.bodyptr = pEdcaParams;
	msgQ.bodyval = 0;
	pe_debug("Sending WMA_UPDATE_EDCA_PROFILE_IND");
	MTRACE(mac_trace_msg_tx(mac, NO_SESSION, msgQ.type));
	retCode = wma_post_ctrl_msg(mac, &msgQ);
	if (QDF_STATUS_SUCCESS != retCode) {
		qdf_mem_free(pEdcaParams);
		pe_err("Posting WMA_UPDATE_EDCA_PROFILE_IND failed, reason=%X",
			retCode);
	}
	return retCode;
}

void lim_set_active_edca_params(struct mac_context *mac_ctx,
				tSirMacEdcaParamRecord *edca_params,
				struct pe_session *pe_session)
{
	uint8_t ac, new_ac, i;
	uint8_t ac_admitted;
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	host_log_qos_edca_pkt_type *log_ptr = NULL;
#endif /* FEATURE_WLAN_DIAG_SUPPORT */
	/* Initialize gLimEdcaParamsActive[] to be same as localEdcaParams */
	pe_session->gLimEdcaParamsActive[QCA_WLAN_AC_BE] = edca_params[QCA_WLAN_AC_BE];
	pe_session->gLimEdcaParamsActive[QCA_WLAN_AC_BK] = edca_params[QCA_WLAN_AC_BK];
	pe_session->gLimEdcaParamsActive[QCA_WLAN_AC_VI] = edca_params[QCA_WLAN_AC_VI];
	pe_session->gLimEdcaParamsActive[QCA_WLAN_AC_VO] = edca_params[QCA_WLAN_AC_VO];

	pe_session->gLimEdcaParamsActive[QCA_WLAN_AC_BE].no_ack =
					mac_ctx->no_ack_policy_cfg[QCA_WLAN_AC_BE];
	pe_session->gLimEdcaParamsActive[QCA_WLAN_AC_BK].no_ack =
					mac_ctx->no_ack_policy_cfg[QCA_WLAN_AC_BK];
	pe_session->gLimEdcaParamsActive[QCA_WLAN_AC_VI].no_ack =
					mac_ctx->no_ack_policy_cfg[QCA_WLAN_AC_VI];
	pe_session->gLimEdcaParamsActive[QCA_WLAN_AC_VO].no_ack =
					mac_ctx->no_ack_policy_cfg[QCA_WLAN_AC_VO];

	/* An AC requires downgrade if the ACM bit is set, and the AC has not
	 * yet been admitted in uplink or bi-directions.
	 * If an AC requires downgrade, it will downgrade to the next beset AC
	 * for which ACM is not enabled.
	 *
	 * - There's no need to downgrade AC_BE since it IS the lowest AC. Hence
	 *   start the for loop with AC_BK.
	 * - If ACM bit is set for an AC, initially downgrade it to AC_BE. Then
	 *   traverse thru the AC list. If we do find the next best AC which is
	 *   better than AC_BE, then use that one. For example, if ACM bits are set
	 *   such that: BE_ACM=1, BK_ACM=1, VI_ACM=1, VO_ACM=0
	 *   then all AC will be downgraded to AC_BE.
	 */
	pe_debug("adAdmitMask[UPLINK] = 0x%x ",
		pe_session->gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK]);
	pe_debug("adAdmitMask[DOWNLINK] = 0x%x ",
		pe_session->gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK]);
	for (ac = QCA_WLAN_AC_BK; ac <= QCA_WLAN_AC_VO; ac++) {
		ac_admitted =
			((pe_session->gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] &
			 (1 << ac)) >> ac);

		pe_debug("For AC[%d]: acm=%d,  ac_admitted=%d ",
			ac, edca_params[ac].aci.acm, ac_admitted);
		if ((edca_params[ac].aci.acm == 1) && (ac_admitted == 0)) {
			pe_debug("We need to downgrade AC %d!!", ac);
			/* Loop backwards through AC values until it finds
			 * acm == 0 or reaches QCA_WLAN_AC_BE.
			 * Note that for block has no executable statements.
			 */
			for (i = ac - 1;
			    (i > QCA_WLAN_AC_BE &&
				(edca_params[i].aci.acm != 0));
			     i--)
				;
			new_ac = i;
			pe_debug("Downgrading AC %d ---> AC %d ", ac, new_ac);
			pe_session->gLimEdcaParamsActive[ac] =
				edca_params[new_ac];
		}
	}
/* log: LOG_WLAN_QOS_EDCA_C */
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	WLAN_HOST_DIAG_LOG_ALLOC(log_ptr, host_log_qos_edca_pkt_type,
				 LOG_WLAN_QOS_EDCA_C);
	if (log_ptr) {
		tSirMacEdcaParamRecord *rec;

		rec = &pe_session->gLimEdcaParamsActive[QCA_WLAN_AC_BE];
		log_ptr->aci_be = rec->aci.aci;
		log_ptr->cw_be = rec->cw.max << 4 | rec->cw.min;
		log_ptr->txoplimit_be = rec->txoplimit;

		rec = &pe_session->gLimEdcaParamsActive[QCA_WLAN_AC_BK];
		log_ptr->aci_bk = rec->aci.aci;
		log_ptr->cw_bk = rec->cw.max << 4 | rec->cw.min;
		log_ptr->txoplimit_bk = rec->txoplimit;

		rec = &pe_session->gLimEdcaParamsActive[QCA_WLAN_AC_VI];
		log_ptr->aci_vi = rec->aci.aci;
		log_ptr->cw_vi = rec->cw.max << 4 | rec->cw.min;
		log_ptr->txoplimit_vi = rec->txoplimit;

		rec = &pe_session->gLimEdcaParamsActive[QCA_WLAN_AC_VO];
		log_ptr->aci_vo = rec->aci.aci;
		log_ptr->cw_vo = rec->cw.max << 4 | rec->cw.min;
		log_ptr->txoplimit_vo = rec->txoplimit;
	}
	WLAN_HOST_DIAG_LOG_REPORT(log_ptr);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	return;
}

QDF_STATUS lim_send_mode_update(struct mac_context *mac,
				   tUpdateVHTOpMode *pTempParam,
				   struct pe_session *pe_session)
{
	tUpdateVHTOpMode *pVhtOpMode = NULL;
	QDF_STATUS retCode = QDF_STATUS_SUCCESS;
	struct scheduler_msg msgQ = {0};

	pVhtOpMode = qdf_mem_malloc(sizeof(tUpdateVHTOpMode));
	if (!pVhtOpMode)
		return QDF_STATUS_E_NOMEM;
	qdf_mem_copy((uint8_t *) pVhtOpMode, pTempParam,
		     sizeof(tUpdateVHTOpMode));
	msgQ.type = WMA_UPDATE_OP_MODE;
	msgQ.reserved = 0;
	msgQ.bodyptr = pVhtOpMode;
	msgQ.bodyval = 0;
	pe_debug("Sending WMA_UPDATE_OP_MODE, op_mode %d",
			pVhtOpMode->opMode);
	if (!pe_session)
		MTRACE(mac_trace_msg_tx(mac, NO_SESSION, msgQ.type));
	else
		MTRACE(mac_trace_msg_tx(mac,
					pe_session->peSessionId,
					msgQ.type));
	retCode = wma_post_ctrl_msg(mac, &msgQ);
	if (QDF_STATUS_SUCCESS != retCode) {
		qdf_mem_free(pVhtOpMode);
		pe_err("Posting WMA_UPDATE_OP_MODE failed, reason=%X",
			retCode);
	}

	return retCode;
}

QDF_STATUS lim_send_rx_nss_update(struct mac_context *mac,
				     tUpdateRxNss *pTempParam,
				     struct pe_session *pe_session)
{
	tUpdateRxNss *pRxNss = NULL;
	QDF_STATUS retCode = QDF_STATUS_SUCCESS;
	struct scheduler_msg msgQ = {0};

	pRxNss = qdf_mem_malloc(sizeof(tUpdateRxNss));
	if (!pRxNss)
		return QDF_STATUS_E_NOMEM;
	qdf_mem_copy((uint8_t *) pRxNss, pTempParam, sizeof(tUpdateRxNss));
	msgQ.type = WMA_UPDATE_RX_NSS;
	msgQ.reserved = 0;
	msgQ.bodyptr = pRxNss;
	msgQ.bodyval = 0;
	pe_debug("Sending WMA_UPDATE_RX_NSS");
	if (!pe_session)
		MTRACE(mac_trace_msg_tx(mac, NO_SESSION, msgQ.type));
	else
		MTRACE(mac_trace_msg_tx(mac,
					pe_session->peSessionId,
					msgQ.type));
	retCode = wma_post_ctrl_msg(mac, &msgQ);
	if (QDF_STATUS_SUCCESS != retCode) {
		qdf_mem_free(pRxNss);
		pe_err("Posting WMA_UPDATE_RX_NSS failed, reason=%X",
			retCode);
	}

	return retCode;
}

QDF_STATUS lim_set_membership(struct mac_context *mac,
				 tUpdateMembership *pTempParam,
				 struct pe_session *pe_session)
{
	tUpdateMembership *pMembership = NULL;
	QDF_STATUS retCode = QDF_STATUS_SUCCESS;
	struct scheduler_msg msgQ = {0};

	pMembership = qdf_mem_malloc(sizeof(tUpdateMembership));
	if (!pMembership)
		return QDF_STATUS_E_NOMEM;
	qdf_mem_copy((uint8_t *) pMembership, pTempParam,
		     sizeof(tUpdateMembership));

	msgQ.type = WMA_UPDATE_MEMBERSHIP;
	msgQ.reserved = 0;
	msgQ.bodyptr = pMembership;
	msgQ.bodyval = 0;
	pe_debug("Sending WMA_UPDATE_MEMBERSHIP");
	if (!pe_session)
		MTRACE(mac_trace_msg_tx(mac, NO_SESSION, msgQ.type));
	else
		MTRACE(mac_trace_msg_tx(mac,
					pe_session->peSessionId,
					msgQ.type));
	retCode = wma_post_ctrl_msg(mac, &msgQ);
	if (QDF_STATUS_SUCCESS != retCode) {
		qdf_mem_free(pMembership);
		pe_err("Posting WMA_UPDATE_MEMBERSHIP failed, reason=%X",
			retCode);
	}

	return retCode;
}

QDF_STATUS lim_set_user_pos(struct mac_context *mac,
			       tUpdateUserPos *pTempParam,
			       struct pe_session *pe_session)
{
	tUpdateUserPos *pUserPos = NULL;
	QDF_STATUS retCode = QDF_STATUS_SUCCESS;
	struct scheduler_msg msgQ = {0};

	pUserPos = qdf_mem_malloc(sizeof(tUpdateUserPos));
	if (!pUserPos)
		return QDF_STATUS_E_NOMEM;
	qdf_mem_copy((uint8_t *) pUserPos, pTempParam, sizeof(tUpdateUserPos));

	msgQ.type = WMA_UPDATE_USERPOS;
	msgQ.reserved = 0;
	msgQ.bodyptr = pUserPos;
	msgQ.bodyval = 0;
	pe_debug("Sending WMA_UPDATE_USERPOS");
	if (!pe_session)
		MTRACE(mac_trace_msg_tx(mac, NO_SESSION, msgQ.type));
	else
		MTRACE(mac_trace_msg_tx(mac,
					pe_session->peSessionId,
					msgQ.type));
	retCode = wma_post_ctrl_msg(mac, &msgQ);
	if (QDF_STATUS_SUCCESS != retCode) {
		qdf_mem_free(pUserPos);
		pe_err("Posting WMA_UPDATE_USERPOS failed, reason=%X",
			retCode);
	}

	return retCode;
}

#ifdef WLAN_FEATURE_11W
/**
 * lim_send_exclude_unencrypt_ind() - sends WMA_EXCLUDE_UNENCRYPTED_IND to HAL
 * @mac:          mac global context
 * @excludeUnenc:  true: ignore, false: indicate
 * @pe_session: session context
 *
 * LIM sends a message to HAL to indicate whether to ignore or indicate the
 * unprotected packet error.
 *
 * Return: status of operation
 */
QDF_STATUS lim_send_exclude_unencrypt_ind(struct mac_context *mac,
					     bool excludeUnenc,
					     struct pe_session *pe_session)
{
	QDF_STATUS retCode = QDF_STATUS_SUCCESS;
	struct scheduler_msg msgQ = {0};
	tSirWlanExcludeUnencryptParam *pExcludeUnencryptParam;

	pExcludeUnencryptParam =
		qdf_mem_malloc(sizeof(tSirWlanExcludeUnencryptParam));
	if (!pExcludeUnencryptParam)
		return QDF_STATUS_E_NOMEM;

	pExcludeUnencryptParam->excludeUnencrypt = excludeUnenc;
	qdf_mem_copy(pExcludeUnencryptParam->bssid.bytes, pe_session->bssId,
			QDF_MAC_ADDR_SIZE);

	msgQ.type = WMA_EXCLUDE_UNENCRYPTED_IND;
	msgQ.reserved = 0;
	msgQ.bodyptr = pExcludeUnencryptParam;
	msgQ.bodyval = 0;
	pe_debug("Sending WMA_EXCLUDE_UNENCRYPTED_IND");
	MTRACE(mac_trace_msg_tx(mac, pe_session->peSessionId, msgQ.type));
	retCode = wma_post_ctrl_msg(mac, &msgQ);
	if (QDF_STATUS_SUCCESS != retCode) {
		qdf_mem_free(pExcludeUnencryptParam);
		pe_err("Posting WMA_EXCLUDE_UNENCRYPTED_IND failed, reason=%X",
			retCode);
	}

	return retCode;
}
#endif

/**
 * lim_send_ht40_obss_scanind() - send ht40 obss start scan request
 * mac: mac context
 * session  PE session handle
 *
 * LIM sends a HT40 start scan message to WMA
 *
 * Return: status of operation
 */
QDF_STATUS lim_send_ht40_obss_scanind(struct mac_context *mac_ctx,
						struct pe_session *session)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;
	struct obss_ht40_scanind *ht40_obss_scanind;
	uint32_t channelnum, chan_freq;
	struct scheduler_msg msg = {0};
	uint8_t channel24gnum, count;

	ht40_obss_scanind = qdf_mem_malloc(sizeof(struct obss_ht40_scanind));
	if (!ht40_obss_scanind)
		return QDF_STATUS_E_FAILURE;
	QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_ERROR,
		  "OBSS Scan Indication bssid " QDF_MAC_ADDR_FMT,
		  QDF_MAC_ADDR_REF(session->bssId));

	ht40_obss_scanind->cmd = HT40_OBSS_SCAN_PARAM_START;
	ht40_obss_scanind->scan_type = eSIR_ACTIVE_SCAN;
	ht40_obss_scanind->obss_passive_dwelltime =
		session->obss_ht40_scanparam.obss_passive_dwelltime;
	ht40_obss_scanind->obss_active_dwelltime =
		session->obss_ht40_scanparam.obss_active_dwelltime;
	ht40_obss_scanind->obss_width_trigger_interval =
		session->obss_ht40_scanparam.obss_width_trigger_interval;
	ht40_obss_scanind->obss_passive_total_per_channel =
		session->obss_ht40_scanparam.obss_passive_total_per_channel;
	ht40_obss_scanind->obss_active_total_per_channel =
		session->obss_ht40_scanparam.obss_active_total_per_channel;
	ht40_obss_scanind->bsswidth_ch_trans_delay =
		session->obss_ht40_scanparam.bsswidth_ch_trans_delay;
	ht40_obss_scanind->obss_activity_threshold =
		session->obss_ht40_scanparam.obss_activity_threshold;
	ht40_obss_scanind->current_operatingclass =
		wlan_reg_dmn_get_opclass_from_channel(
			mac_ctx->scan.countryCodeCurrent,
			wlan_reg_freq_to_chan(
			mac_ctx->pdev, session->curr_op_freq),
			session->ch_width);
	channelnum = mac_ctx->mlme_cfg->reg.valid_channel_list_num;

	/* Extract 24G channel list */
	channel24gnum = 0;
	for (count = 0; count < channelnum &&
		(channel24gnum < SIR_ROAM_MAX_CHANNELS); count++) {
		chan_freq =
			mac_ctx->mlme_cfg->reg.valid_channel_freq_list[count];
		if (wlan_reg_is_24ghz_ch_freq(chan_freq)) {
			ht40_obss_scanind->chan_freq_list[channel24gnum] =
				chan_freq;
			channel24gnum++;
		}
	}
	ht40_obss_scanind->channel_count = channel24gnum;
	/* FW API requests BSS IDX */
	ht40_obss_scanind->bss_id = session->vdev_id;
	ht40_obss_scanind->fortymhz_intolerent = 0;
	ht40_obss_scanind->iefield_len = 0;
	msg.type = WMA_HT40_OBSS_SCAN_IND;
	msg.reserved = 0;
	msg.bodyptr = (void *)ht40_obss_scanind;
	msg.bodyval = 0;
	pe_debug("Sending WDA_HT40_OBSS_SCAN_IND to WDA"
		"Obss Scan trigger width: %d, delay factor: %d",
		ht40_obss_scanind->obss_width_trigger_interval,
		ht40_obss_scanind->bsswidth_ch_trans_delay);
	ret = wma_post_ctrl_msg(mac_ctx, &msg);
	if (QDF_STATUS_SUCCESS != ret) {
		pe_err("WDA_HT40_OBSS_SCAN_IND msg failed, reason=%X",
			ret);
		qdf_mem_free(ht40_obss_scanind);
	}
	return ret;
}
