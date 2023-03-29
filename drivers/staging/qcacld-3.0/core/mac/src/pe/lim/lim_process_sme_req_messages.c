/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * This file lim_process_sme_req_messages.cc contains the code
 * for processing SME request messages.
 * Author:        Chandra Modumudi
 * Date:          02/11/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "cds_api.h"
#include "wni_api.h"
#include "wni_cfg.h"
#include "sir_api.h"
#include "sch_api.h"
#include "utils_api.h"
#include "lim_types.h"
#include "lim_utils.h"
#include "lim_assoc_utils.h"
#include "lim_security_utils.h"
#include "lim_ser_des_utils.h"
#include "lim_sme_req_utils.h"
#include "lim_admit_control.h"
#include "dph_hash_table.h"
#include "lim_send_messages.h"
#include "lim_api.h"
#include "wmm_apsd.h"
#include "sir_mac_prot_def.h"
#include "rrm_api.h"
#include "nan_datapath.h"
#include "sap_api.h"
#include <lim_ft.h>
#include "cds_regdomain.h"
#include <wlan_reg_services_api.h>
#include "lim_process_fils.h"
#include "wlan_utility.h"
#include <wlan_crypto_global_api.h>
#include "../../core/src/vdev_mgr_ops.h"
#include "wma.h"
#include <wlan_mlme_ucfg_api.h>
#include <wlan_reg_ucfg_api.h>
#include "wlan_lmac_if_def.h"
#include "wlan_reg_services_api.h"

/* SME REQ processing function templates */
static bool __lim_process_sme_sys_ready_ind(struct mac_context *, uint32_t *);
static bool __lim_process_sme_start_bss_req(struct mac_context *,
					    struct scheduler_msg *pMsg);
static void __lim_process_sme_disassoc_req(struct mac_context *, uint32_t *);
static void __lim_process_sme_disassoc_cnf(struct mac_context *, uint32_t *);
static void __lim_process_sme_deauth_req(struct mac_context *, uint32_t *);
static bool __lim_process_sme_stop_bss_req(struct mac_context *,
					   struct scheduler_msg *pMsg);
static void lim_process_sme_channel_change_request(struct mac_context *mac,
						   uint32_t *pMsg);
static void lim_process_sme_start_beacon_req(struct mac_context *mac, uint32_t *pMsg);
static void lim_process_sme_dfs_csa_ie_request(struct mac_context *mac, uint32_t *pMsg);
static void lim_process_nss_update_request(struct mac_context *mac, uint32_t *pMsg);
static void lim_process_set_ie_req(struct mac_context *mac, uint32_t *pMsg);

static void lim_start_bss_update_add_ie_buffer(struct mac_context *mac,
					       uint8_t **pDstData_buff,
					       uint16_t *pDstDataLen,
					       uint8_t *pSrcData_buff,
					       uint16_t srcDataLen);

static void lim_update_add_ie_buffer(struct mac_context *mac,
				     uint8_t **pDstData_buff,
				     uint16_t *pDstDataLen,
				     uint8_t *pSrcData_buff, uint16_t srcDataLen);
static void lim_process_modify_add_ies(struct mac_context *mac, uint32_t *pMsg);

static void lim_process_update_add_ies(struct mac_context *mac, uint32_t *pMsg);

static void lim_process_ext_change_channel(struct mac_context *mac_ctx,
						uint32_t *msg);

/**
 * enum get_next_lower_bw - Get next higher bandwidth for a given BW.
 * This enum is used in conjunction with
 * wlan_reg_set_channel_params_for_freq API to fetch center frequencies
 * for each BW starting from 20MHz upto Max BSS BW in case of non-PSD power.
 *
 */
static const enum phy_ch_width get_next_higher_bw[] = {
	[CH_WIDTH_20MHZ] = CH_WIDTH_40MHZ,
	[CH_WIDTH_40MHZ] = CH_WIDTH_80MHZ,
	[CH_WIDTH_80MHZ] = CH_WIDTH_160MHZ,
	[CH_WIDTH_160MHZ] = CH_WIDTH_INVALID
};

/**
 * lim_process_set_hw_mode() - Send set HW mode command to WMA
 * @mac: Globacl MAC pointer
 * @msg: Message containing the hw mode index
 *
 * Send the set HW mode command to WMA
 *
 * Return: QDF_STATUS_SUCCESS if message posting is successful
 */
static QDF_STATUS lim_process_set_hw_mode(struct mac_context *mac, uint32_t *msg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct scheduler_msg message = {0};
	struct policy_mgr_hw_mode *req_msg;
	uint32_t len;
	struct s_sir_set_hw_mode *buf;
	struct scheduler_msg resp_msg = {0};
	struct sir_set_hw_mode_resp *param;

	buf = (struct s_sir_set_hw_mode *) msg;
	if (!buf) {
		pe_err("Set HW mode param is NULL");
		status = QDF_STATUS_E_INVAL;
		/* To free the active command list */
		goto fail;
	}

	len = sizeof(*req_msg);

	req_msg = qdf_mem_malloc(len);
	if (!req_msg) {
		status = QDF_STATUS_E_NOMEM;
		goto fail;
	}

	req_msg->hw_mode_index = buf->set_hw.hw_mode_index;
	req_msg->reason = buf->set_hw.reason;
	/* Other parameters are not needed for WMA */

	message.bodyptr = req_msg;
	message.type    = SIR_HAL_PDEV_SET_HW_MODE;

	pe_debug("Posting SIR_HAL_SOC_SET_HW_MOD to WMA");
	status = scheduler_post_message(QDF_MODULE_ID_PE,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &message);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		pe_err("scheduler_post_msg failed!(err=%d)",
			status);
		qdf_mem_free(req_msg);
		goto fail;
	}
	return status;
fail:
	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return QDF_STATUS_E_FAILURE;
	param->status = SET_HW_MODE_STATUS_ECANCELED;
	param->cfgd_hw_mode_index = 0;
	param->num_vdev_mac_entries = 0;
	resp_msg.type = eWNI_SME_SET_HW_MODE_RESP;
	resp_msg.bodyptr = param;
	resp_msg.bodyval = 0;
	lim_sys_process_mmh_msg_api(mac, &resp_msg);
	return status;
}

/**
 * lim_process_set_dual_mac_cfg_req() - Set dual mac config command to WMA
 * @mac: Global MAC pointer
 * @msg: Message containing the dual mac config parameter
 *
 * Send the set dual mac config command to WMA
 *
 * Return: QDF_STATUS_SUCCESS if message posting is successful
 */
static QDF_STATUS lim_process_set_dual_mac_cfg_req(struct mac_context *mac,
		uint32_t *msg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct scheduler_msg message = {0};
	struct policy_mgr_dual_mac_config *req_msg;
	uint32_t len;
	struct sir_set_dual_mac_cfg *buf;
	struct scheduler_msg resp_msg = {0};
	struct sir_dual_mac_config_resp *param;

	buf = (struct sir_set_dual_mac_cfg *) msg;
	if (!buf) {
		pe_err("Set Dual mac config is NULL");
		status = QDF_STATUS_E_INVAL;
		/* To free the active command list */
		goto fail;
	}

	len = sizeof(*req_msg);

	req_msg = qdf_mem_malloc(len);
	if (!req_msg) {
		status = QDF_STATUS_E_NOMEM;
		goto fail;
	}

	req_msg->scan_config = buf->set_dual_mac.scan_config;
	req_msg->fw_mode_config = buf->set_dual_mac.fw_mode_config;
	/* Other parameters are not needed for WMA */

	message.bodyptr = req_msg;
	message.type    = SIR_HAL_PDEV_DUAL_MAC_CFG_REQ;

	pe_debug("Post SIR_HAL_PDEV_DUAL_MAC_CFG_REQ to WMA: %x %x",
		req_msg->scan_config, req_msg->fw_mode_config);
	status = scheduler_post_message(QDF_MODULE_ID_PE,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &message);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		pe_err("scheduler_post_msg failed!(err=%d)",
				status);
		qdf_mem_free(req_msg);
		goto fail;
	}
	return status;
fail:
	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return QDF_STATUS_E_FAILURE;
	param->status = SET_HW_MODE_STATUS_ECANCELED;
	resp_msg.type = eWNI_SME_SET_DUAL_MAC_CFG_RESP;
	resp_msg.bodyptr = param;
	resp_msg.bodyval = 0;
	lim_sys_process_mmh_msg_api(mac, &resp_msg);
	return status;
}

/**
 * lim_process_set_antenna_mode_req() - Set antenna mode command
 * to WMA
 * @mac: Global MAC pointer
 * @msg: Message containing the antenna mode parameter
 *
 * Send the set antenna mode command to WMA
 *
 * Return: QDF_STATUS_SUCCESS if message posting is successful
 */
static QDF_STATUS lim_process_set_antenna_mode_req(struct mac_context *mac,
		uint32_t *msg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct scheduler_msg message = {0};
	struct sir_antenna_mode_param *req_msg;
	struct sir_set_antenna_mode *buf;
	struct scheduler_msg resp_msg = {0};
	struct sir_antenna_mode_resp *param;

	buf = (struct sir_set_antenna_mode *) msg;
	if (!buf) {
		pe_err("Set antenna mode is NULL");
		status = QDF_STATUS_E_INVAL;
		/* To free the active command list */
		goto fail;
	}

	req_msg = qdf_mem_malloc(sizeof(*req_msg));
	if (!req_msg) {
		status = QDF_STATUS_E_NOMEM;
		goto fail;
	}

	req_msg->num_rx_chains = buf->set_antenna_mode.num_rx_chains;
	req_msg->num_tx_chains = buf->set_antenna_mode.num_tx_chains;

	message.bodyptr = req_msg;
	message.type    = SIR_HAL_SOC_ANTENNA_MODE_REQ;

	pe_debug("Post SIR_HAL_SOC_ANTENNA_MODE_REQ to WMA: %d %d",
		req_msg->num_rx_chains,
		req_msg->num_tx_chains);
	status = scheduler_post_message(QDF_MODULE_ID_PE,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &message);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		pe_err("scheduler_post_msg failed!(err=%d)",
				status);
		qdf_mem_free(req_msg);
		goto fail;
	}
	return status;
fail:
	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return QDF_STATUS_E_NOMEM;
	param->status = SET_ANTENNA_MODE_STATUS_ECANCELED;
	resp_msg.type = eWNI_SME_SET_ANTENNA_MODE_RESP;
	resp_msg.bodyptr = param;
	resp_msg.bodyval = 0;
	lim_sys_process_mmh_msg_api(mac, &resp_msg);
	return status;
}

/**
 * __lim_is_sme_assoc_cnf_valid() -- Validate ASSOC_CNF message
 * @assoc_cnf: Pointer to Received ASSOC_CNF message
 *
 * This function is called by __lim_process_sme_assoc_cnf_new() upon
 * receiving SME_ASSOC_CNF.
 *
 * Return: true when received SME_ASSOC_CNF is formatted correctly
 *         false otherwise
 */
static bool __lim_is_sme_assoc_cnf_valid(struct assoc_cnf *assoc_cnf)
{
	if (qdf_is_macaddr_group(&assoc_cnf->peer_macaddr))
		return false;

	return true;
}

/**
 * __lim_is_defered_msg_for_radar() - Defers the message if radar is detected
 * @mac_ctx: Pointer to Global MAC structure
 * @message: Pointer to message posted from SME to LIM.
 *
 * Has role only if 11h is enabled. Not used on STA side.
 * Defers the message if radar is detected.
 *
 * Return: true, if defered otherwise return false.
 */
static bool
__lim_is_defered_msg_for_radar(struct mac_context *mac_ctx,
			       struct scheduler_msg *message)
{
	/*
	 * fRadarDetCurOperChan will be set only if we
	 * detect radar in current operating channel and
	 * System Role == AP ROLE
	 *
	 * TODO: Need to take care radar detection.
	 *
	 * if (LIM_IS_RADAR_DETECTED(mac_ctx))
	 */
	if (0) {
		if (lim_defer_msg(mac_ctx, message) != TX_SUCCESS) {
			pe_err("Could not defer Msg: %d", message->type);
			return false;
		}
		pe_debug("Defer the message, in learn mode type: %d",
			message->type);
		return true;
	}
	return false;
}

/**
 * __lim_process_sme_sys_ready_ind () - Process ready indication from WMA
 * @mac: Global MAC context
 * @msg_buf: Message from WMA
 *
 * handles the notification from HDD. PE just forwards this message to HAL.
 *
 * Return: true-Posting to HAL failed, so PE will consume the buffer.
 *         false-Posting to HAL successful, so HAL will consume the buffer.
 */

static bool __lim_process_sme_sys_ready_ind(struct mac_context *mac,
					    uint32_t *msg_buf)
{
	struct scheduler_msg msg = {0};
	struct sme_ready_req *ready_req = (struct sme_ready_req *)msg_buf;

	msg.type = WMA_SYS_READY_IND;
	msg.reserved = 0;
	msg.bodyptr = msg_buf;
	msg.bodyval = 0;

	if (ANI_DRIVER_TYPE(mac) != QDF_DRIVER_TYPE_MFG) {
		ready_req->pe_roam_synch_cb = pe_roam_synch_callback;
		ready_req->pe_disconnect_cb = pe_disconnect_callback;
		pe_register_mgmt_rx_frm_callback(mac);
		pe_register_callbacks_with_wma(mac, ready_req);
		mac->lim.sme_msg_callback = ready_req->sme_msg_cb;
		mac->lim.stop_roaming_callback = ready_req->stop_roaming_cb;
	}

	pe_debug("sending WMA_SYS_READY_IND msg to HAL");
	MTRACE(mac_trace_msg_tx(mac, NO_SESSION, msg.type));

	if (QDF_STATUS_SUCCESS != wma_post_ctrl_msg(mac, &msg)) {
		pe_err("wma_post_ctrl_msg failed");
		return true;
	}
	return false;
}

/**
 *lim_configure_ap_start_bss_session() - Configure the AP Start BSS in session.
 *@mac_ctx: Pointer to Global MAC structure
 *@session: A pointer to session entry
 *@sme_start_bss_req: Start BSS Request from upper layers.
 *
 * This function is used to configure the start bss parameters
 * in to the session.
 *
 * Return: None.
 */
static void
lim_configure_ap_start_bss_session(struct mac_context *mac_ctx,
				   struct pe_session *session,
				   struct start_bss_req *sme_start_bss_req)
{
	session->limSystemRole = eLIM_AP_ROLE;
	session->privacy = sme_start_bss_req->privacy;
	session->fwdWPSPBCProbeReq = sme_start_bss_req->fwdWPSPBCProbeReq;
	session->authType = sme_start_bss_req->authType;
	/* Store the DTIM period */
	session->dtimPeriod = (uint8_t) sme_start_bss_req->dtimPeriod;
	/* Enable/disable UAPSD */
	session->apUapsdEnable = sme_start_bss_req->apUapsdEnable;
	if (session->opmode == QDF_P2P_GO_MODE) {
		session->proxyProbeRspEn = 0;
	} else {
		/*
		 * To detect PBC overlap in SAP WPS mode,
		 * Host handles Probe Requests.
		 */
		if (SAP_WPS_DISABLED == sme_start_bss_req->wps_state)
			session->proxyProbeRspEn = 1;
		else
			session->proxyProbeRspEn = 0;
	}
	session->ssidHidden = sme_start_bss_req->ssidHidden;
	session->wps_state = sme_start_bss_req->wps_state;
	session->sap_dot11mc = sme_start_bss_req->sap_dot11mc;
	session->vendor_vht_sap =
			sme_start_bss_req->vendor_vht_sap;
	lim_get_short_slot_from_phy_mode(mac_ctx, session, session->gLimPhyMode,
		&session->shortSlotTimeSupported);

	session->beacon_tx_rate = sme_start_bss_req->beacon_tx_rate;

}

/**
 * lim_send_start_vdev_req() - send vdev start request
 *@session: pe session
 *@mlm_start_req: vdev start req
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
lim_send_start_vdev_req(struct pe_session *session, tLimMlmStartReq *mlm_start_req)
{
	return wlan_vdev_mlme_sm_deliver_evt(session->vdev,
					     WLAN_VDEV_SM_EV_START,
					     sizeof(*mlm_start_req),
					     mlm_start_req);
}

#ifdef WLAN_FEATURE_11AX
void lim_strip_he_ies_from_add_ies(struct mac_context *mac_ctx,
				   struct pe_session *session)
{
	struct add_ie_params *add_ie = &session->add_ie_params;
	QDF_STATUS status;
	uint8_t he_cap_buff[DOT11F_IE_HE_CAP_MAX_LEN + 2];
	uint8_t he_op_buff[DOT11F_IE_HE_OP_MAX_LEN + 2];

	qdf_mem_zero(he_cap_buff, sizeof(he_cap_buff));
	qdf_mem_zero(he_op_buff, sizeof(he_op_buff));

	status = lim_strip_ie(mac_ctx, add_ie->probeRespBCNData_buff,
			      &add_ie->probeRespBCNDataLen,
			      DOT11F_EID_HE_CAP, ONE_BYTE,
			      HE_CAP_OUI_TYPE, (uint8_t)HE_CAP_OUI_SIZE,
			      he_cap_buff, DOT11F_IE_HE_CAP_MAX_LEN);
	if (status != QDF_STATUS_SUCCESS)
		pe_debug("Failed to strip HE cap IE status: %d", status);


	status = lim_strip_ie(mac_ctx, add_ie->probeRespBCNData_buff,
			      &add_ie->probeRespBCNDataLen,
			      DOT11F_EID_HE_OP, ONE_BYTE,
			      HE_OP_OUI_TYPE, (uint8_t)HE_OP_OUI_SIZE,
			      he_op_buff, DOT11F_IE_HE_OP_MAX_LEN);
	if (status != QDF_STATUS_SUCCESS)
		pe_debug("Failed to strip HE op IE status: %d", status);
}

static bool lim_is_6g_allowed_sec(struct mac_context *mac,
				  struct pe_session *session)
{
	struct wlan_objmgr_vdev *vdev;
	uint32_t keymgmt;
	uint16_t ie_len;
	bool status = false;

	if (!mac->mlme_cfg->he_caps.enable_6g_sec_check)
		return true;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc,
						    session->vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		pe_err("Invalid vdev");
		return false;
	}
	if (wlan_crypto_check_open_none(mac->psoc, session->vdev_id)) {
		pe_err("open mode sec not allowed for 6G conn");
		return false;
	}

	if (!session->limRmfEnabled) {
		pe_err("rmf enabled is false");
		return false;
	}

	keymgmt = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_KEY_MGMT);
	if (!keymgmt ||
	    (keymgmt & (1 << WLAN_CRYPTO_KEY_MGMT_NONE |
			1 << WLAN_CRYPTO_KEY_MGMT_SAE |
			1 << WLAN_CRYPTO_KEY_MGMT_FT_SAE |
			1 << WLAN_CRYPTO_KEY_MGMT_FILS_SHA256 |
			1 << WLAN_CRYPTO_KEY_MGMT_FILS_SHA384 |
			1 << WLAN_CRYPTO_KEY_MGMT_FT_FILS_SHA256 |
			1 << WLAN_CRYPTO_KEY_MGMT_FT_FILS_SHA384 |
			1 << WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SUITE_B |
			1 << WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SUITE_B_192 |
			1 << WLAN_CRYPTO_KEY_MGMT_OWE)))
		status = true;
	else
		pe_err("Invalid key_mgmt %0X for 6G connection, vdev %d",
		       keymgmt, session->vdev_id);

	if (!(keymgmt & (1 << WLAN_CRYPTO_KEY_MGMT_SAE |
			 1 << WLAN_CRYPTO_KEY_MGMT_FT_SAE)))
		return status;

	ie_len = lim_get_ielen_from_bss_description(
			&session->lim_join_req->bssDescription);
	if (!wlan_get_ie_ptr_from_eid(WLAN_ELEMID_RSNXE,
		(uint8_t *)session->lim_join_req->bssDescription.ieFields,
		ie_len)) {
		pe_err("RSNXE IE not present in beacon for 6G conn");
		return false;
	}

	if (!wlan_get_ie_ptr_from_eid(WLAN_ELEMID_RSNXE,
				session->lim_join_req->addIEAssoc.addIEdata,
				session->lim_join_req->addIEAssoc.length)) {
		pe_err("RSNXE IE not present in assoc add IE data for 6G conn");
		return false;
	}

	return status;
}

#else
void lim_strip_he_ies_from_add_ies(struct mac_context *mac_ctx,
				   struct pe_session *session)
{
}

static inline bool lim_is_6g_allowed_sec(struct mac_context *mac,
					 struct pe_session *session)
{
	return false;
}
#endif

/**
 * __lim_handle_sme_start_bss_request() - process SME_START_BSS_REQ message
 *@mac_ctx: Pointer to Global MAC structure
 *@msg_buf: A pointer to the SME message buffer
 *
 * This function is called to process SME_START_BSS_REQ message
 * from HDD or upper layer application.
 *
 * Return: None
 */
static void
__lim_handle_sme_start_bss_request(struct mac_context *mac_ctx, uint32_t *msg_buf)
{
	uint16_t size;
	uint32_t val = 0;
	tSirMacChanNum channel_number;
	tLimMlmStartReq *mlm_start_req = NULL;
	struct start_bss_req *sme_start_bss_req = NULL;
	tSirResultCodes ret_code = eSIR_SME_SUCCESS;
	uint8_t session_id;
	struct pe_session *session = NULL;
	uint8_t vdev_id = 0xFF;
	uint32_t chanwidth;
	struct vdev_type_nss *vdev_type_nss;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

/* FEATURE_WLAN_DIAG_SUPPORT */
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM
	/*
	 * Since the session is not created yet, sending NULL.
	 * The response should have the correct state.
	 */
	lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_START_BSS_REQ_EVENT,
			      NULL, 0, 0);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	size = sizeof(*sme_start_bss_req);
	sme_start_bss_req = qdf_mem_malloc(size);
	if (!sme_start_bss_req) {
		ret_code = eSIR_SME_RESOURCES_UNAVAILABLE;
		goto free;
	}
	qdf_mem_copy(sme_start_bss_req, msg_buf, size);
	vdev_id = sme_start_bss_req->vdev_id;

	if ((mac_ctx->lim.gLimSmeState == eLIM_SME_OFFLINE_STATE) ||
	    (mac_ctx->lim.gLimSmeState == eLIM_SME_IDLE_STATE)) {
		if (!lim_is_sme_start_bss_req_valid(mac_ctx,
					sme_start_bss_req)) {
			pe_warn("Received invalid eWNI_SME_START_BSS_REQ");
			ret_code = eSIR_SME_INVALID_PARAMETERS;
			goto free;
		}
		channel_number = wlan_reg_freq_to_chan(mac_ctx->pdev,
					sme_start_bss_req->oper_ch_freq);
		/*
		 * This is the place where PE is going to create a session.
		 * If session is not existed, then create a new session
		 */
		session = pe_find_session_by_bssid(mac_ctx,
				sme_start_bss_req->bssid.bytes, &session_id);
		if (session) {
			pe_warn("Session Already exists for given BSSID");
			ret_code = eSIR_SME_BSS_ALREADY_STARTED_OR_JOINED;
			session = NULL;
			goto free;
		} else {
			session = pe_create_session(mac_ctx,
					sme_start_bss_req->bssid.bytes,
					&session_id,
					mac_ctx->lim.max_sta_of_pe_session,
					sme_start_bss_req->bssType,
					sme_start_bss_req->vdev_id,
					sme_start_bss_req->bssPersona);
			if (!session) {
				pe_warn("Session Can not be created");
				ret_code = eSIR_SME_RESOURCES_UNAVAILABLE;
				goto free;
			}

			/* Update the beacon/probe filter in mac_ctx */
			lim_set_bcn_probe_filter(mac_ctx, session,
						 channel_number);
		}

		if (QDF_NDI_MODE != sme_start_bss_req->bssPersona) {
			/* Probe resp add ie */
			lim_start_bss_update_add_ie_buffer(mac_ctx,
				&session->add_ie_params.probeRespData_buff,
				&session->add_ie_params.probeRespDataLen,
				sme_start_bss_req->add_ie_params.
					probeRespData_buff,
				sme_start_bss_req->add_ie_params.
					probeRespDataLen);

			/* Probe Beacon add ie */
			lim_start_bss_update_add_ie_buffer(mac_ctx,
				&session->add_ie_params.probeRespBCNData_buff,
				&session->add_ie_params.probeRespBCNDataLen,
				sme_start_bss_req->add_ie_params.
					probeRespBCNData_buff,
				sme_start_bss_req->add_ie_params.
					probeRespBCNDataLen);

			/* Assoc resp IE */
			lim_start_bss_update_add_ie_buffer(mac_ctx,
				&session->add_ie_params.assocRespData_buff,
				&session->add_ie_params.assocRespDataLen,
				sme_start_bss_req->add_ie_params.
					assocRespData_buff,
				sme_start_bss_req->add_ie_params.
					assocRespDataLen);
		}
		/* Store the session related params in newly created session */
		session->pLimStartBssReq = sme_start_bss_req;
		session->ht_config = sme_start_bss_req->ht_config;
		session->vht_config = sme_start_bss_req->vht_config;

		sir_copy_mac_addr(session->self_mac_addr,
				  sme_start_bss_req->self_macaddr.bytes);

		/* Copy SSID to session table */
		qdf_mem_copy((uint8_t *) &session->ssId,
			     (uint8_t *) &sme_start_bss_req->ssId,
			     (sme_start_bss_req->ssId.length + 1));

		session->nwType = sme_start_bss_req->nwType;

		session->beaconParams.beaconInterval =
			sme_start_bss_req->beaconInterval;

		/* Store the oper freq in session Table */
		session->curr_op_freq = sme_start_bss_req->oper_ch_freq;

		/* Update the phymode */
		session->gLimPhyMode = sme_start_bss_req->nwType;

		session->maxTxPower = wlan_reg_get_channel_reg_power_for_freq(
			mac_ctx->pdev, session->curr_op_freq);
		/* Store the dot 11 mode in to the session Table */
		session->dot11mode = sme_start_bss_req->dot11mode;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
		session->cc_switch_mode =
			sme_start_bss_req->cc_switch_mode;
#endif
		session->htCapability =
			IS_DOT11_MODE_HT(session->dot11mode);
		session->vhtCapability =
			IS_DOT11_MODE_VHT(session->dot11mode);

		if (IS_DOT11_MODE_HE(session->dot11mode)) {
			lim_update_session_he_capable(mac_ctx, session);
			lim_copy_bss_he_cap(session);
		} else if (wlan_reg_is_6ghz_chan_freq(session->curr_op_freq)) {
			pe_err("Invalid oper_ch_freq %d for dot11mode %d",
			       session->curr_op_freq, session->dot11mode);
			ret_code = eSIR_SME_INVALID_PARAMETERS;
			goto free;
		} else {
			lim_strip_he_ies_from_add_ies(mac_ctx, session);
		}

		session->txLdpcIniFeatureEnabled =
			sme_start_bss_req->txLdpcIniFeatureEnabled;
#ifdef WLAN_FEATURE_11W
		session->limRmfEnabled = sme_start_bss_req->pmfCapable ? 1 : 0;
		pe_debug("RMF enabled: %d", session->limRmfEnabled);
#endif

		qdf_mem_copy((void *)&session->rateSet,
			     (void *)&sme_start_bss_req->operationalRateSet,
			     sizeof(tSirMacRateSet));
		qdf_mem_copy((void *)&session->extRateSet,
			     (void *)&sme_start_bss_req->extendedRateSet,
			     sizeof(tSirMacRateSet));

		if (!wlan_reg_is_24ghz_ch_freq(session->curr_op_freq))
			vdev_type_nss = &mac_ctx->vdev_type_nss_5g;
		else
			vdev_type_nss = &mac_ctx->vdev_type_nss_2g;

		switch (sme_start_bss_req->bssType) {
		case eSIR_INFRA_AP_MODE:
			lim_configure_ap_start_bss_session(mac_ctx, session,
				sme_start_bss_req);
			if (session->opmode == QDF_SAP_MODE)
				session->vdev_nss = vdev_type_nss->sap;
			else
				session->vdev_nss = vdev_type_nss->p2p_go;
			break;
		case eSIR_NDI_MODE:
			session->vdev_nss = vdev_type_nss->ndi;
			session->limSystemRole = eLIM_NDI_ROLE;
			break;


		/*
		 * There is one more mode called auto mode.
		 * which is used no where
		 */

		/* FORBUILD -TEMPFIX.. HOW TO use AUTO MODE????? */

		default:
			/* not used anywhere...used in scan function */
			break;
		}

		session->nss = session->vdev_nss;
		if (!mac_ctx->mlme_cfg->vht_caps.vht_cap_info.enable2x2)
			session->nss = 1;
		/*
		 * Allocate memory for the array of
		 * parsed (Re)Assoc request structure
		 */
		if (sme_start_bss_req->bssType == eSIR_INFRA_AP_MODE) {
			session->parsedAssocReq =
				qdf_mem_malloc(session->dph.dphHashTable.
						size * sizeof(tpSirAssocReq));
			if (!session->parsedAssocReq) {
				ret_code = eSIR_SME_RESOURCES_UNAVAILABLE;
				goto free;
			}
		}

		if (!sme_start_bss_req->oper_ch_freq &&
		    sme_start_bss_req->bssType != eSIR_NDI_MODE) {
			pe_err("Received invalid eWNI_SME_START_BSS_REQ");
			ret_code = eSIR_SME_INVALID_PARAMETERS;
			goto free;
		}
#ifdef QCA_HT_2040_COEX
		if (sme_start_bss_req->obssEnabled)
			session->htSupportedChannelWidthSet =
				session->htCapability;
		else
#endif
		session->htSupportedChannelWidthSet =
			(sme_start_bss_req->sec_ch_offset) ? 1 : 0;
		session->htSecondaryChannelOffset =
			sme_start_bss_req->sec_ch_offset;
		session->htRecommendedTxWidthSet =
			(session->htSecondaryChannelOffset) ? 1 : 0;
		if (lim_is_session_he_capable(session) ||
		    session->vhtCapability || session->htCapability) {
			chanwidth = sme_start_bss_req->vht_channel_width;
			session->ch_width = chanwidth;
			session->ch_center_freq_seg0 =
				sme_start_bss_req->center_freq_seg0;
			session->ch_center_freq_seg1 =
				sme_start_bss_req->center_freq_seg1;
			lim_update_he_bw_cap_mcs(session, NULL);
		}

		/* Delete pre-auth list if any */
		lim_delete_pre_auth_list(mac_ctx);

		/*
		 * keep the RSN/WPA IE information in PE Session Entry
		 * later will be using this to check when received (Re)Assoc req
		 */
		lim_set_rs_nie_wp_aiefrom_sme_start_bss_req_message(mac_ctx,
				&sme_start_bss_req->rsnIE, session);

		if (LIM_IS_AP_ROLE(session) || LIM_IS_NDI_ROLE(session)) {
			session->gLimProtectionControl =
				sme_start_bss_req->protEnabled;
			/*
			 * each byte will have the following info
			 * bit7       bit6    bit5   bit4 bit3   bit2  bit1 bit0
			 * reserved reserved   RIFS   Lsig n-GF   ht20  11g  11b
			 */
			qdf_mem_copy((void *)&session->cfgProtection,
				     (void *)&sme_start_bss_req->ht_capab,
				     sizeof(uint16_t));
			/* Initialize WPS PBC session link list */
			session->pAPWPSPBCSession = NULL;
		}
		/* Prepare and Issue LIM_MLM_START_REQ to MLM */
		mlm_start_req = qdf_mem_malloc(sizeof(tLimMlmStartReq));
		if (!mlm_start_req) {
			ret_code = eSIR_SME_RESOURCES_UNAVAILABLE;
			goto free;
		}

		/* Copy SSID to the MLM start structure */
		qdf_mem_copy((uint8_t *) &mlm_start_req->ssId,
			     (uint8_t *) &sme_start_bss_req->ssId,
			     sme_start_bss_req->ssId.length + 1);
		mlm_start_req->ssidHidden = sme_start_bss_req->ssidHidden;
		mlm_start_req->obssProtEnabled =
			sme_start_bss_req->obssProtEnabled;

		mlm_start_req->bssType = session->bssType;

		/* Fill PE session Id from the session Table */
		mlm_start_req->sessionId = session->peSessionId;

		sir_copy_mac_addr(mlm_start_req->bssId, session->bssId);
		/* store the channel num in mlmstart req structure */
		mlm_start_req->oper_ch_freq = session->curr_op_freq;
		mlm_start_req->cbMode = sme_start_bss_req->cbMode;
		mlm_start_req->beaconPeriod =
			session->beaconParams.beaconInterval;
		mlm_start_req->cac_duration_ms =
			sme_start_bss_req->cac_duration_ms;
		mlm_start_req->dfs_regdomain =
			sme_start_bss_req->dfs_regdomain;
		if (LIM_IS_AP_ROLE(session)) {
			mlm_start_req->dtimPeriod = session->dtimPeriod;
			mlm_start_req->wps_state = session->wps_state;
			session->cac_duration_ms =
				mlm_start_req->cac_duration_ms;
			session->dfs_regdomain = mlm_start_req->dfs_regdomain;
		} else {
			val = mac_ctx->mlme_cfg->sap_cfg.dtim_interval;
			mlm_start_req->dtimPeriod = (uint8_t) val;
		}

		mlm_start_req->cfParamSet.cfpPeriod =
			mac_ctx->mlme_cfg->rates.cfp_period;
		mlm_start_req->cfParamSet.cfpMaxDuration =
			mac_ctx->mlme_cfg->rates.cfp_max_duration;

		/*
		 * this may not be needed anymore now,
		 * as rateSet is now included in the
		 * session entry and MLM has session context.
		 */
		qdf_mem_copy((void *)&mlm_start_req->rateSet,
			     (void *)&session->rateSet,
			     sizeof(tSirMacRateSet));

		/* Now populate the 11n related parameters */
		mlm_start_req->nwType = session->nwType;
		mlm_start_req->htCapable = session->htCapability;

		mlm_start_req->htOperMode = mac_ctx->lim.gHTOperMode;
		/* Unused */
		mlm_start_req->dualCTSProtection =
			mac_ctx->lim.gHTDualCTSProtection;
		mlm_start_req->txChannelWidthSet =
			session->htRecommendedTxWidthSet;

		session->limRFBand = lim_get_rf_band(
			sme_start_bss_req->oper_ch_freq);

		/* Initialize 11h Enable Flag */
		session->lim11hEnable = 0;
		if ((CHAN_HOP_ALL_BANDS_ENABLE ||
		     REG_BAND_5G == session->limRFBand)) {
			session->lim11hEnable =
				mac_ctx->mlme_cfg->gen.enabled_11h;

			if (session->lim11hEnable &&
				(eSIR_INFRA_AP_MODE ==
					mlm_start_req->bssType)) {
				session->lim11hEnable =
					mac_ctx->mlme_cfg->
					dfs_cfg.dfs_master_capable;
			}
		}

		if (!session->lim11hEnable)
			mac_ctx->mlme_cfg->power.local_power_constraint = 0;

		mlm_start_req->beacon_tx_rate = session->beacon_tx_rate;

		session->limPrevSmeState = session->limSmeState;
		session->limSmeState = eLIM_SME_WT_START_BSS_STATE;

		pe_debug("Freq %d width %d freq0 %d freq1 %d, dot11mode %d nss %d vendor vht %d",
			 session->curr_op_freq, session->ch_width,
			 session->ch_center_freq_seg0,
			 session->ch_center_freq_seg1,
			 session->dot11mode, session->vdev_nss,
			 session->vendor_vht_sap);
		MTRACE(mac_trace
			(mac_ctx, TRACE_CODE_SME_STATE,
			session->peSessionId,
			session->limSmeState));

		qdf_status = lim_send_start_vdev_req(session, mlm_start_req);
		if (QDF_IS_STATUS_ERROR(qdf_status))
			goto free;
		qdf_mem_free(mlm_start_req);
		return;
	} else {

		pe_err("Received unexpected START_BSS_REQ, in state %X",
			mac_ctx->lim.gLimSmeState);
		ret_code = eSIR_SME_BSS_ALREADY_STARTED_OR_JOINED;
		goto free;
	} /* if (mac_ctx->lim.gLimSmeState == eLIM_SME_OFFLINE_STATE) */

free:
	if ((session) &&
	    (session->pLimStartBssReq == sme_start_bss_req)) {
		session->pLimStartBssReq = NULL;
	}
	if (sme_start_bss_req)
		qdf_mem_free(sme_start_bss_req);
	if (mlm_start_req)
		qdf_mem_free(mlm_start_req);
	if (session) {
		pe_delete_session(mac_ctx, session);
		session = NULL;
	}
	lim_send_sme_start_bss_rsp(mac_ctx, eWNI_SME_START_BSS_RSP, ret_code,
				   session, vdev_id);
}

/**
 * __lim_process_sme_start_bss_req() - Call handler to start BSS
 *
 * @mac: Global MAC context
 * @pMsg: Message pointer
 *
 * Wrapper for the function __lim_handle_sme_start_bss_request
 * This message will be defered until softmac come out of
 * scan mode or if we have detected radar on the current
 * operating channel.
 *
 * return true - If we consumed the buffer
 *        false - If have defered the message.
 */
static bool __lim_process_sme_start_bss_req(struct mac_context *mac,
					    struct scheduler_msg *pMsg)
{
	if (__lim_is_defered_msg_for_radar(mac, pMsg)) {
		/**
		 * If message defered, buffer is not consumed yet.
		 * So return false
		 */
		return false;
	}

	__lim_handle_sme_start_bss_request(mac, (uint32_t *) pMsg->bodyptr);
	return true;
}

/**
 *  lim_get_random_bssid()
 *
 *  FUNCTION:This function is called to process generate the random number for bssid
 *  This function is called to process SME_SCAN_REQ message
 *  from HDD or upper layer application.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 * 1. geneartes the unique random number for bssid in ibss
 *
 *  @param  mac      Pointer to Global MAC structure
 *  @param  *data      Pointer to  bssid  buffer
 *  @return None
 */
void lim_get_random_bssid(struct mac_context *mac, uint8_t *data)
{
	uint32_t random[2];

	random[0] = qdf_mc_timer_get_system_ticks();
	random[0] |= (random[0] << 15);
	random[1] = random[0] >> 1;
	qdf_mem_copy(data, random, sizeof(tSirMacAddr));
}

#ifdef WLAN_FEATURE_SAE

/**
 * lim_update_sae_config()- This API update SAE session info to csr config
 * from join request.
 * @session: PE session
 * @sme_join_req: pointer to join request
 *
 * Return: None
 */
static void lim_update_sae_config(struct pe_session *session,
				  struct join_req *sme_join_req)
{
	session->sae_pmk_cached = sme_join_req->sae_pmk_cached;

	pe_debug("pmk_cached %d for BSSID=" QDF_MAC_ADDR_FMT,
		session->sae_pmk_cached,
		QDF_MAC_ADDR_REF(sme_join_req->bssDescription.bssId));
}
#else
static inline void lim_update_sae_config(struct pe_session *session,
					 struct join_req *sme_join_req)
{}
#endif

/**
 * lim_send_join_req() - send vdev start request for assoc
 *@session: pe session
 *@mlm_join_req: join req
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS lim_send_join_req(struct pe_session *session,
				    tLimMlmJoinReq *mlm_join_req)
{
	QDF_STATUS status;

	/* Continue connect only if Vdev is in INIT state */
	status = wlan_vdev_mlme_is_init_state(session->vdev);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("Vdev %d not in int state cur state %d substate %d",
			session->vdev_id,
			wlan_vdev_mlme_get_state(session->vdev),
			wlan_vdev_mlme_get_substate(session->vdev));
		qdf_trigger_self_recovery(session->mac_ctx->psoc,
					  QDF_VDEV_SM_OUT_OF_SYNC);
		return status;
	}

	status = mlme_set_assoc_type(session->vdev, VDEV_ASSOC);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	return wlan_vdev_mlme_sm_deliver_evt(session->vdev,
					     WLAN_VDEV_SM_EV_START,
					     sizeof(*mlm_join_req),
					     mlm_join_req);
}

/**
 * lim_send_reassoc_req() - send vdev start request for reassoc
 *@session: pe session
 *@mlm_join_req: join req
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS lim_send_reassoc_req(struct pe_session *session,
				       tLimMlmReassocReq *reassoc_req)
{
	QDF_STATUS status;

	status = mlme_set_assoc_type(session->vdev, VDEV_REASSOC);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	if (wlan_vdev_mlme_get_state(session->vdev) != WLAN_VDEV_S_UP) {
		pe_err("Reassoc req in unexpected vdev SM state:%d",
		       wlan_vdev_mlme_get_state(session->vdev));
		return QDF_STATUS_E_FAILURE;
	}

	lim_process_mlm_reassoc_req(session->mac_ctx, reassoc_req);
	return QDF_STATUS_SUCCESS;
}

/**
 * lim_send_ft_reassoc_req() - send vdev start request for ft_reassoc
 *@session: pe session
 *@mlm_join_req: join req
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS lim_send_ft_reassoc_req(struct pe_session *session,
					  tLimMlmReassocReq *reassoc_req)
{
	QDF_STATUS status;

	status = mlme_set_assoc_type(session->vdev, VDEV_FT_REASSOC);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	if (wlan_vdev_mlme_get_state(session->vdev) == WLAN_VDEV_S_UP) {
		pe_err("ft_reassoc req in unexpected vdev SM state:%d",
		       wlan_vdev_mlme_get_state(session->vdev));
		return QDF_STATUS_E_FAILURE;
	}

	return wlan_vdev_mlme_sm_deliver_evt(session->vdev,
					     WLAN_VDEV_SM_EV_START,
					     sizeof(*reassoc_req),
					     reassoc_req);
}

#ifdef WLAN_FEATURE_11W
bool
lim_get_vdev_rmf_capable(struct mac_context *mac, struct pe_session *session)
{
	struct wlan_objmgr_vdev *vdev;
	int32_t rsn_caps;
	bool peer_rmf_capable = false;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc,
						    session->vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		pe_err("Invalid vdev");
		return false;
	}
	rsn_caps = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_RSN_CAP);
	if (rsn_caps < 0) {
		pe_err("Invalid mgmt cipher");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return false;
	}
	if (wlan_crypto_vdev_has_mgmtcipher(
				vdev,
				(1 << WLAN_CRYPTO_CIPHER_AES_GMAC) |
				(1 << WLAN_CRYPTO_CIPHER_AES_GMAC_256) |
				(1 << WLAN_CRYPTO_CIPHER_AES_CMAC)) &&
	    (rsn_caps & WLAN_CRYPTO_RSN_CAP_MFP_ENABLED))
		peer_rmf_capable = true;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	pe_debug("vdev %d peer_rmf_capable %d rsn_caps 0x%x",
		 session->vdev_id, peer_rmf_capable,
		 rsn_caps);

	return peer_rmf_capable;
}
#endif

/**
 * __lim_process_sme_join_req() - process SME_JOIN_REQ message
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: A pointer to the SME message buffer
 *
 * This function is called to process SME_JOIN_REQ message
 * from HDD or upper layer application.
 *
 * Return: None
 */
static void
__lim_process_sme_join_req(struct mac_context *mac_ctx, void *msg_buf)
{
	struct join_req *in_req = msg_buf;
	struct join_req *sme_join_req = NULL;
	tLimMlmJoinReq *mlm_join_req;
	tSirResultCodes ret_code = eSIR_SME_SUCCESS;
	uint32_t val = 0;
	uint8_t session_id;
	uint8_t bss_chan_id;
	struct pe_session *session = NULL;
	uint8_t vdev_id = 0;
	int8_t local_power_constraint = 0;
	struct vdev_mlme_obj *mlme_obj;
	bool is_pwr_constraint;
	uint16_t ie_len;
	const uint8_t *vendor_ie;
	struct bss_description *bss_desc;
	QDF_STATUS status;
	int8_t reg_max = 0;
	struct mlme_legacy_priv *mlme_priv;

	if (!mac_ctx || !msg_buf) {
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_ERROR,
			  FL("JOIN REQ with invalid data"));
		return;
	}

/* FEATURE_WLAN_DIAG_SUPPORT */
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM
	/*
	 * Not sending any session, since it is not created yet.
	 * The response whould have correct state.
	 */
	lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_JOIN_REQ_EVENT, NULL, 0, 0);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	/*
	 * Expect Join request in idle state.
	 * Reassociate request is expected in link established state.
	 */

	/* Global SME and LIM states are not defined yet for BT-AMP Support */
	if (mac_ctx->lim.gLimSmeState == eLIM_SME_IDLE_STATE) {
		sme_join_req = qdf_mem_malloc(in_req->length);
		if (!sme_join_req) {
			ret_code = eSIR_SME_RESOURCES_UNAVAILABLE;
			goto end;
		}
		qdf_mem_copy(sme_join_req, in_req, in_req->length);

		if (!lim_is_sme_join_req_valid(mac_ctx, sme_join_req)) {
			/* Received invalid eWNI_SME_JOIN_REQ */
			/* Log the event */
			pe_warn("vdev_id:%d JOIN REQ with invalid data",
				sme_join_req->vdev_id);
			ret_code = eSIR_SME_INVALID_PARAMETERS;
			goto end;
		}

		/*
		 * Update the capability here itself as this is used in
		 * lim_extract_ap_capability() below. If not updated issues
		 * like not honoring power constraint on 1st association after
		 * driver loading might occur.
		 */
		lim_update_rrm_capability(mac_ctx, sme_join_req);

		bss_desc = &sme_join_req->bssDescription;
		/* check for the existence of start BSS session  */
		session = pe_find_session_by_bssid(mac_ctx, bss_desc->bssId,
				&session_id);
		bss_chan_id = wlan_reg_freq_to_chan(mac_ctx->pdev,
						    bss_desc->chan_freq);

		if (session) {
			pe_err("Session(%d) Already exists for BSSID: "
				   QDF_MAC_ADDR_FMT " in limSmeState = %X",
				session_id,
				QDF_MAC_ADDR_REF(bss_desc->bssId),
				session->limSmeState);

			qdf_trigger_self_recovery(mac_ctx->psoc,
						  QDF_VDEV_SM_OUT_OF_SYNC);

			if (session->limSmeState == eLIM_SME_LINK_EST_STATE &&
			    session->smeSessionId == sme_join_req->vdev_id) {
				/*
				 * Received eWNI_SME_JOIN_REQ for same
				 * BSS as currently associated.
				 * Log the event and send success
				 */
				pe_warn("SessionId: %d", session_id);
				pe_warn("JOIN_REQ for current joined BSS");
				/* Send Join success response to host */
				ret_code = eSIR_SME_ALREADY_JOINED_A_BSS;
				session = NULL;
				goto end;
			} else {
				pe_err("JOIN_REQ not for current joined BSS");
				ret_code = eSIR_SME_REFUSED;
				session = NULL;
				goto end;
			}
		} else {
			/*
			 * Session Entry does not exist for given BSSId
			 * Try to Create a new session
			 */
			session = pe_create_session(mac_ctx, bss_desc->bssId,
					&session_id,
					mac_ctx->lim.max_sta_of_pe_session,
					eSIR_INFRASTRUCTURE_MODE,
					sme_join_req->vdev_id,
					sme_join_req->staPersona);
			if (!session) {
				pe_err("Session Can not be created");
				ret_code = eSIR_SME_RESOURCES_UNAVAILABLE;
				goto end;
			}
			/* Update the beacon/probe filter in mac_ctx */
			lim_set_bcn_probe_filter(mac_ctx, session,
						 bss_chan_id);
		}
		session->max_amsdu_num = sme_join_req->max_amsdu_num;
		session->enable_session_twt_support =
			sme_join_req->enable_session_twt_support;
		/*
		 * Store Session related parameters
		 */

		/* store the smejoin req handle in session table */
		session->lim_join_req = sme_join_req;

		/* Store beaconInterval */
		session->beaconParams.beaconInterval =
			bss_desc->beaconInterval;

		session->ht_config = sme_join_req->ht_config;
		session->vht_config = sme_join_req->vht_config;

		/* Copying of bssId is already done, while creating session */
		sir_copy_mac_addr(session->self_mac_addr,
				  sme_join_req->self_mac_addr);

		session->statypeForBss = STA_ENTRY_PEER;
		session->limWmeEnabled = sme_join_req->isWMEenabled;
		session->limQosEnabled = sme_join_req->isQosEnabled;
		session->wps_registration = sme_join_req->wps_registration;
		session->he_with_wep_tkip = sme_join_req->he_with_wep_tkip;

		session->enable_bcast_probe_rsp =
				sme_join_req->enable_bcast_probe_rsp;

		/* Store vendor specific IE for CISCO AP */
		ie_len = (bss_desc->length + sizeof(bss_desc->length) -
			 GET_FIELD_OFFSET(struct bss_description, ieFields));

		vendor_ie = wlan_get_vendor_ie_ptr_from_oui(
				SIR_MAC_CISCO_OUI, SIR_MAC_CISCO_OUI_SIZE,
				((uint8_t *)&bss_desc->ieFields), ie_len);

		if (vendor_ie)
			session->isCiscoVendorAP = true;
		else
			session->isCiscoVendorAP = false;

		/* Copy the dot 11 mode in to the session table */

		session->dot11mode = sme_join_req->dot11mode;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
		session->cc_switch_mode = sme_join_req->cc_switch_mode;
#endif
		session->nwType = bss_desc->nwType;
		session->enableAmpduPs = sme_join_req->enableAmpduPs;
		session->enableHtSmps = sme_join_req->enableHtSmps;
		session->htSmpsvalue = sme_join_req->htSmps;
		session->send_smps_action =
			sme_join_req->send_smps_action;
		/*
		 * By default supported NSS 1x1 is set to true
		 * and later on updated while determining session
		 * supported rates which is the intersection of
		 * self and peer rates
		 */
		session->supported_nss_1x1 = true;

		/* Copy oper freq to the session Table */
		session->curr_op_freq = bss_desc->chan_freq;
		session->vhtCapability =
			IS_DOT11_MODE_VHT(session->dot11mode);
		if (session->vhtCapability) {
			if (session->opmode == QDF_STA_MODE) {
				session->vht_config.su_beam_formee =
					sme_join_req->vht_config.su_beam_formee;
			} else {
				session->vht_config.su_beam_formee = 0;
			}
			session->enableVhtpAid =
				sme_join_req->enableVhtpAid;
			session->enableVhtGid =
				sme_join_req->enableVhtGid;
		}

		/*Phy mode */
		session->gLimPhyMode = bss_desc->nwType;
		handle_ht_capabilityand_ht_info(mac_ctx, session);
		session->force_24ghz_in_ht20 =
			sme_join_req->force_24ghz_in_ht20;
		/* cbMode is already merged value of peer and self -
		 * done by csr in csr_get_cb_mode_from_ies */
		session->htSupportedChannelWidthSet =
			(sme_join_req->cbMode) ? 1 : 0;
		session->htRecommendedTxWidthSet =
			session->htSupportedChannelWidthSet;
		session->htSecondaryChannelOffset = sme_join_req->cbMode;

		if (PHY_DOUBLE_CHANNEL_HIGH_PRIMARY == sme_join_req->cbMode) {
			session->ch_center_freq_seg0 =
				wlan_reg_freq_to_chan(
				mac_ctx->pdev, session->curr_op_freq) - 2;
			session->ch_width = CH_WIDTH_40MHZ;
		} else if (PHY_DOUBLE_CHANNEL_LOW_PRIMARY ==
				sme_join_req->cbMode) {
			session->ch_center_freq_seg0 =
				wlan_reg_freq_to_chan(
				mac_ctx->pdev, session->curr_op_freq) + 2;
			session->ch_width = CH_WIDTH_40MHZ;
		} else {
			session->ch_center_freq_seg0 = 0;
			session->ch_width = CH_WIDTH_20MHZ;
		}

		if (IS_DOT11_MODE_HE(session->dot11mode)) {
			lim_update_session_he_capable(mac_ctx, session);
			lim_copy_join_req_he_cap(session);
		}


		/* Record if management frames need to be protected */
		session->limRmfEnabled =
			lim_get_vdev_rmf_capable(mac_ctx, session);

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM
		session->rssi = bss_desc->rssi;
#endif

		/* Copy the SSID from smejoinreq to session entry  */
		session->ssId.length = sme_join_req->ssId.length;
		qdf_mem_copy(session->ssId.ssId, sme_join_req->ssId.ssId,
			session->ssId.length);

		/*
		 * Determin 11r or ESE connection based on input from SME
		 * which inturn is dependent on the profile the user wants
		 * to connect to, So input is coming from supplicant
		 */
		session->is11Rconnection = sme_join_req->is11Rconnection;
		session->connected_akm = sme_join_req->akm;
		session->is_adaptive_11r_connection =
				sme_join_req->is_adaptive_11r_connection;
#ifdef FEATURE_WLAN_ESE
		session->isESEconnection = sme_join_req->isESEconnection;
#endif
		session->isFastTransitionEnabled =
			sme_join_req->isFastTransitionEnabled;

		session->isFastRoamIniFeatureEnabled =
			sme_join_req->isFastRoamIniFeatureEnabled;
		session->txLdpcIniFeatureEnabled =
			sme_join_req->txLdpcIniFeatureEnabled;

		lim_update_fils_config(mac_ctx, session, sme_join_req);
		lim_update_sae_config(session, sme_join_req);
		if (session->bssType == eSIR_INFRASTRUCTURE_MODE) {
			session->limSystemRole = eLIM_STA_ROLE;
		} else {
			/*
			 * Throw an error and return and make
			 * sure to delete the session.
			 */
			pe_err("recvd JOIN_REQ with invalid bss type %d",
				session->bssType);
			ret_code = eSIR_SME_INVALID_PARAMETERS;
			goto end;
		}

		session->encryptType = sme_join_req->UCEncryptionType;
		if (wlan_reg_is_6ghz_chan_freq(session->curr_op_freq)) {
			if (!lim_is_session_he_capable(session) ||
			    !lim_is_6g_allowed_sec(mac_ctx, session)) {
				pe_err("JOIN_REQ with invalid 6G security");
				ret_code = eSIR_SME_INVALID_PARAMETERS;
				goto end;
			}

		}
		if (sme_join_req->addIEScan.length)
			qdf_mem_copy(&session->lim_join_req->addIEScan,
				     &sme_join_req->addIEScan,
				     sizeof(tSirAddie));

		if (sme_join_req->addIEAssoc.length)
			qdf_mem_copy(&session->lim_join_req->addIEAssoc,
				     &sme_join_req->addIEAssoc,
				     sizeof(tSirAddie));

		val = sizeof(tLimMlmJoinReq) +
			session->lim_join_req->bssDescription.length + 2;
		mlm_join_req = qdf_mem_malloc(val);
		if (!mlm_join_req) {
			ret_code = eSIR_SME_RESOURCES_UNAVAILABLE;
			goto end;
		}

		/* PE SessionId is stored as a part of JoinReq */
		mlm_join_req->sessionId = session->peSessionId;

		/* copy operational rate from session */
		qdf_mem_copy((void *)&session->rateSet,
			(void *)&sme_join_req->operationalRateSet,
			sizeof(tSirMacRateSet));
		qdf_mem_copy((void *)&session->extRateSet,
			(void *)&sme_join_req->extendedRateSet,
			sizeof(tSirMacRateSet));
		/*
		 * this may not be needed anymore now, as rateSet is now
		 * included in the session entry and MLM has session context.
		 */
		qdf_mem_copy((void *)&mlm_join_req->operationalRateSet,
			(void *)&session->rateSet,
			sizeof(tSirMacRateSet));


		session->supported_nss_1x1 = sme_join_req->supported_nss_1x1;
		session->vdev_nss = sme_join_req->vdev_nss;
		session->nss = sme_join_req->nss;
		session->nss_forced_1x1 = sme_join_req->nss_forced_1x1;

		mlm_join_req->bssDescription.length =
			session->lim_join_req->bssDescription.length;

		qdf_mem_copy((uint8_t *) &mlm_join_req->bssDescription.bssId,
			(uint8_t *)
			&session->lim_join_req->bssDescription.bssId,
			session->lim_join_req->bssDescription.length + 2);

		session->limCurrentBssCaps =
			session->lim_join_req->bssDescription.capabilityInfo;

		mlme_priv = wlan_vdev_mlme_get_ext_hdl(session->vdev);
		if (!mlme_priv) {
			pe_err("Invalid mlme priv object");
			ret_code = eSIR_SME_INVALID_PARAMETERS;
			goto end;
		}

		qdf_mem_zero(mlme_priv->connect_info.ext_cap_ie,
			     DOT11F_IE_EXTCAP_MAX_LEN + 2);

		if (session->lim_join_req->addIEAssoc.length) {
			uint8_t *add_ie = NULL;
			uint16_t add_ie_len;

			add_ie_len = session->lim_join_req->addIEAssoc.length;
			add_ie = qdf_mem_malloc(add_ie_len);
			if (!add_ie) {
				ret_code = eSIR_SME_INVALID_PARAMETERS;
				goto end;
			}
			qdf_mem_copy(add_ie,
				     session->lim_join_req->
				     addIEAssoc.addIEdata,
				     add_ie_len);

			status = lim_strip_ie(mac_ctx, add_ie, &add_ie_len,
					      DOT11F_EID_EXTCAP, ONE_BYTE,
					      NULL, 0,
					      mlme_priv->connect_info.
					      ext_cap_ie,
					      DOT11F_IE_EXTCAP_MAX_LEN);
			qdf_mem_free(add_ie);

			if (QDF_IS_STATUS_ERROR(status)) {
				pe_err("Parsing of ext cap failed with status : %d",
				       status);
				qdf_mem_zero(mlme_priv->connect_info.ext_cap_ie,
					     DOT11F_IE_EXTCAP_MAX_LEN + 2);
				ret_code = eSIR_SME_INVALID_PARAMETERS;
				goto end;
			}
		}

		mlme_obj = wlan_vdev_mlme_get_cmpt_obj(session->vdev);
		if (!mlme_obj)
			return;

		lim_extract_ap_capability(mac_ctx,
			(uint8_t *)
			session->lim_join_req->bssDescription.ieFields,
			lim_get_ielen_from_bss_description(
			&session->lim_join_req->bssDescription),
			&session->limCurrentBssQosCaps,
			&session->gLimCurrentBssUapsd,
			&local_power_constraint, session, &is_pwr_constraint);

		if (wlan_reg_is_ext_tpc_supported(mac_ctx->psoc)) {
			mlme_obj->reg_tpc_obj.ap_constraint_power =
							local_power_constraint;
		} else {
			reg_max = wlan_reg_get_channel_reg_power_for_freq(
					mac_ctx->pdev, session->curr_op_freq);
			if (is_pwr_constraint)
				local_power_constraint = reg_max -
							local_power_constraint;
			if (!local_power_constraint)
				local_power_constraint = reg_max;

			mlme_obj->reg_tpc_obj.reg_max[0] = reg_max;
			mlme_obj->reg_tpc_obj.ap_constraint_power =
							local_power_constraint;
			mlme_obj->reg_tpc_obj.frequency[0] =
						session->curr_op_freq;

			session->maxTxPower = lim_get_max_tx_power(mac_ctx,
								   mlme_obj);
			session->def_max_tx_pwr = session->maxTxPower;
		}

		pe_debug("Reg %d local %d session %d join req %d",
			 reg_max, local_power_constraint, session->maxTxPower,
			 sme_join_req->powerCap.maxTxPower);

		if (sme_join_req->powerCap.maxTxPower > session->maxTxPower)
			sme_join_req->powerCap.maxTxPower = session->maxTxPower;

		if (session->gLimCurrentBssUapsd) {
			session->gUapsdPerAcBitmask =
				session->lim_join_req->uapsdPerAcBitmask;
			pe_debug("UAPSD flag for all AC - 0x%2x",
				 session->gUapsdPerAcBitmask);

			/* resetting the dynamic uapsd mask  */
			session->gUapsdPerAcDeliveryEnableMask = 0;
			session->gUapsdPerAcTriggerEnableMask = 0;
		}

		session->limRFBand = lim_get_rf_band(session->curr_op_freq);

		/* Initialize 11h Enable Flag */
		if (session->limRFBand == REG_BAND_5G)
			session->lim11hEnable =
				mac_ctx->mlme_cfg->gen.enabled_11h;
		else
			session->lim11hEnable = 0;

		session->limPrevSmeState = session->limSmeState;
		session->limSmeState = eLIM_SME_WT_JOIN_STATE;
		MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
				session->peSessionId,
				session->limSmeState));

		/* Indicate whether spectrum management is enabled */
		session->spectrumMgtEnabled =
			sme_join_req->spectrumMgtIndicator;
		/* Enable MBSSID only for station */
		session->is_mbssid_enabled = wma_is_mbssid_enabled();

		/* Enable the spectrum management if this is a DFS channel */
		if (session->country_info_present &&
		    lim_isconnected_on_dfs_channel(
				mac_ctx,
				wlan_reg_freq_to_chan(
				mac_ctx->pdev, session->curr_op_freq)))
			session->spectrumMgtEnabled = true;

		session->isOSENConnection = sme_join_req->isOSENConnection;
		pe_debug("Freq %d width %d freq0 %d freq1 %d, Smps %d: mode %d action %d, nss 1x1 %d vdev_nss %d nss %d cbMode %d dot11mode %d subfer %d subfee %d csn %d is_cisco %d",
			 session->curr_op_freq, session->ch_width,
			 session->ch_center_freq_seg0,
			 session->ch_center_freq_seg1,
			 session->enableHtSmps, session->htSmpsvalue,
			 session->send_smps_action, session->supported_nss_1x1,
			 session->vdev_nss, session->nss,
			 sme_join_req->cbMode, session->dot11mode,
			 session->vht_config.su_beam_former,
			 session->vht_config.su_beam_formee,
			 session->vht_config.csnof_beamformer_antSup,
			 session->isCiscoVendorAP);

		/* Issue LIM_MLM_JOIN_REQ to MLM */
		status = lim_send_join_req(session, mlm_join_req);
		if (QDF_IS_STATUS_ERROR(status)) {
			qdf_mem_free(mlm_join_req);
			ret_code = eSIR_SME_REFUSED;
			goto end;
		}
		return;

	} else {
		/* Received eWNI_SME_JOIN_REQ un expected state */
		pe_err("received unexpected SME_JOIN_REQ in state %X",
			mac_ctx->lim.gLimSmeState);
		lim_print_sme_state(mac_ctx, LOGE, mac_ctx->lim.gLimSmeState);
		ret_code = eSIR_SME_UNEXPECTED_REQ_RESULT_CODE;
		session = NULL;
		goto end;
	}

end:
	vdev_id = in_req->vdev_id;

	if (sme_join_req) {
		qdf_mem_free(sme_join_req);
		sme_join_req = NULL;
		if (session)
			session->lim_join_req = NULL;
	}
	if (ret_code != eSIR_SME_SUCCESS) {
		if (session) {
			pe_delete_session(mac_ctx, session);
			session = NULL;
		}
	}
	pe_debug("Send failure status on vdev_id: %d with ret_code: %d",
		vdev_id, ret_code);
	lim_send_sme_join_reassoc_rsp(mac_ctx, eWNI_SME_JOIN_RSP, ret_code,
		STATUS_UNSPECIFIED_FAILURE, session, vdev_id);
}

static uint8_t lim_get_num_tpe_octets(uint8_t max_transmit_power_count)
{
	if (!max_transmit_power_count)
		return max_transmit_power_count;

	return 1 << (max_transmit_power_count - 1);
}

void lim_parse_tpe_ie(struct mac_context *mac, struct pe_session *session,
		      tDot11fIEtransmit_power_env *tpe_ies, uint8_t num_tpe_ies,
		      tDot11fIEhe_op *he_op, bool *has_tpe_updated)
{
	struct vdev_mlme_obj *vdev_mlme;
	uint8_t i, local_tpe_count = 0, reg_tpe_count = 0, num_octets;
	uint8_t psd_index = 0, non_psd_index = 0;
	uint16_t bw_val, ch_width;
	qdf_freq_t curr_op_freq, curr_freq;
	enum reg_6g_client_type client_mobility_type;
	struct ch_params ch_params = {0};
	tDot11fIEtransmit_power_env single_tpe;
	/*
	 * PSD is power spectral density, incoming TPE could contain
	 * non PSD info, or PSD info, or both, so need to keep track of them
	 */
	bool use_local_tpe, non_psd_set = false, psd_set = false;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(session->vdev);
	if (!vdev_mlme)
		return;

	vdev_mlme->reg_tpc_obj.num_pwr_levels = 0;
	*has_tpe_updated = false;

	wlan_reg_get_cur_6g_client_type(mac->pdev, &client_mobility_type);

	for (i = 0; i < num_tpe_ies; i++) {
		single_tpe = tpe_ies[i];
		if (single_tpe.present &&
		    (single_tpe.max_tx_pwr_category == client_mobility_type)) {
			if (single_tpe.max_tx_pwr_interpret == LOCAL_EIRP ||
			    single_tpe.max_tx_pwr_interpret == LOCAL_EIRP_PSD)
				local_tpe_count++;
			else if (single_tpe.max_tx_pwr_interpret ==
				 REGULATORY_CLIENT_EIRP ||
				 single_tpe.max_tx_pwr_interpret ==
				 REGULATORY_CLIENT_EIRP_PSD)
				reg_tpe_count++;
		}
	}

	if (!reg_tpe_count && !local_tpe_count) {
		pe_debug("No TPEs found in beacon IE");
		return;
	} else if (!reg_tpe_count) {
		use_local_tpe = true;
	} else if (!local_tpe_count) {
		use_local_tpe = false;
	} else {
		use_local_tpe = wlan_mlme_is_local_tpe_pref(mac->psoc);
	}

	for (i = 0; i < num_tpe_ies; i++) {
		single_tpe = tpe_ies[i];
		if (single_tpe.present &&
		    (single_tpe.max_tx_pwr_category == client_mobility_type)) {
			if (use_local_tpe) {
				if (single_tpe.max_tx_pwr_interpret ==
				    LOCAL_EIRP) {
					non_psd_index = i;
					non_psd_set = true;
				}
				if (single_tpe.max_tx_pwr_interpret ==
				    LOCAL_EIRP_PSD) {
					psd_index = i;
					psd_set = true;
				}
			} else {
				if (single_tpe.max_tx_pwr_interpret ==
				    REGULATORY_CLIENT_EIRP) {
					non_psd_index = i;
					non_psd_set = true;
				}
				if (single_tpe.max_tx_pwr_interpret ==
				    REGULATORY_CLIENT_EIRP_PSD) {
					psd_index = i;
					psd_set = true;
				}
			}
		}
	}

	curr_op_freq = session->curr_op_freq;
	bw_val = wlan_reg_get_bw_value(session->ch_width);

	if (non_psd_set && !psd_set) {
		single_tpe = tpe_ies[non_psd_index];
		vdev_mlme->reg_tpc_obj.is_psd_power = false;
		vdev_mlme->reg_tpc_obj.eirp_power = 0;
		vdev_mlme->reg_tpc_obj.num_pwr_levels =
					single_tpe.max_tx_pwr_count + 1;

		ch_params.ch_width = CH_WIDTH_20MHZ;

		for (i = 0; i < single_tpe.max_tx_pwr_count + 1; i++) {
			wlan_reg_set_channel_params_for_freq(mac->pdev,
							     curr_op_freq, 0,
							     &ch_params);
			if (vdev_mlme->reg_tpc_obj.tpe[i] !=
			    single_tpe.tx_power[i] ||
			    vdev_mlme->reg_tpc_obj.frequency[i] !=
			    ch_params.mhz_freq_seg0)
				*has_tpe_updated = true;
			vdev_mlme->reg_tpc_obj.frequency[i] =
							ch_params.mhz_freq_seg0;
			vdev_mlme->reg_tpc_obj.tpe[i] = single_tpe.tx_power[i];
			ch_params.ch_width =
				get_next_higher_bw[ch_params.ch_width];
		}
	}

	if (psd_set) {
		single_tpe = tpe_ies[psd_index];
		vdev_mlme->reg_tpc_obj.is_psd_power = true;
		num_octets =
			lim_get_num_tpe_octets(single_tpe.max_tx_pwr_count);
		vdev_mlme->reg_tpc_obj.num_pwr_levels = num_octets;

		ch_params.ch_width = session->ch_width;
		wlan_reg_set_channel_params_for_freq(mac->pdev, curr_op_freq, 0,
						     &ch_params);

		if (ch_params.mhz_freq_seg1)
			curr_freq = ch_params.mhz_freq_seg1 - bw_val / 2 + 10;
		else
			curr_freq = ch_params.mhz_freq_seg0 - bw_val / 2 + 10;

		if (!num_octets) {
			if (!he_op->oper_info_6g_present)
				ch_width = session->ch_width;
			else
				ch_width = he_op->oper_info_6g.info.ch_width;
			num_octets = lim_get_num_pwr_levels(true,
							    session->ch_width);
			vdev_mlme->reg_tpc_obj.num_pwr_levels = num_octets;
			for (i = 0; i < num_octets; i++) {
				if (vdev_mlme->reg_tpc_obj.tpe[i] !=
				    single_tpe.tx_power[0] ||
				    vdev_mlme->reg_tpc_obj.frequency[i] !=
				    curr_freq)
					*has_tpe_updated = true;
				vdev_mlme->reg_tpc_obj.frequency[i] = curr_freq;
				curr_freq += 20;
				vdev_mlme->reg_tpc_obj.tpe[i] =
							single_tpe.tx_power[0];
			}
		} else {
			for (i = 0; i < num_octets; i++) {
				if (vdev_mlme->reg_tpc_obj.tpe[i] !=
				    single_tpe.tx_power[i] ||
				    vdev_mlme->reg_tpc_obj.frequency[i] !=
				    curr_freq)
					*has_tpe_updated = true;
				vdev_mlme->reg_tpc_obj.frequency[i] = curr_freq;
				curr_freq += 20;
				vdev_mlme->reg_tpc_obj.tpe[i] =
							single_tpe.tx_power[i];
			}
		}
	}

	if (non_psd_set) {
		single_tpe = tpe_ies[non_psd_index];
		vdev_mlme->reg_tpc_obj.eirp_power =
			single_tpe.tx_power[single_tpe.max_tx_pwr_count];
	}
}

void lim_process_tpe_ie_from_beacon(struct mac_context *mac,
				    struct pe_session *session,
				    struct bss_description *bss_desc,
				    bool *has_tpe_updated)
{
	tDot11fBeaconIEs *bcn_ie;
	uint32_t buf_len;
	uint8_t *buf;
	int status;

	bcn_ie = qdf_mem_malloc(sizeof(*bcn_ie));
	if (!bcn_ie)
		return;

	buf_len = lim_get_ielen_from_bss_description(bss_desc);
	buf = (uint8_t *)bss_desc->ieFields;
	status = dot11f_unpack_beacon_i_es(mac, buf, buf_len, bcn_ie, false);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to parse Beacon IEs (0x%08x, %d bytes):",
		       status, buf_len);
		qdf_mem_free(bcn_ie);
		return;
	} else if (DOT11F_WARNED(status)) {
		pe_debug("warnings (0x%08x, %d bytes):", status, buf_len);
	}

	lim_parse_tpe_ie(mac, session, bcn_ie->transmit_power_env,
			 bcn_ie->num_transmit_power_env, &bcn_ie->he_op,
			 has_tpe_updated);
	qdf_mem_free(bcn_ie);
}

uint32_t lim_get_num_pwr_levels(bool is_psd,
				enum phy_ch_width ch_width)
{
	uint32_t num_pwr_levels = 0;

	if (is_psd) {
		switch (ch_width) {
		case CH_WIDTH_20MHZ:
			num_pwr_levels = 1;
			break;
		case CH_WIDTH_40MHZ:
			num_pwr_levels = 2;
			break;
		case CH_WIDTH_80MHZ:
			num_pwr_levels = 4;
			break;
		case CH_WIDTH_160MHZ:
			num_pwr_levels = 8;
			break;
		default:
			pe_err("Invalid channel width");
			return 0;
		}
	} else {
		switch (ch_width) {
		case CH_WIDTH_20MHZ:
			num_pwr_levels = 1;
			break;
		case CH_WIDTH_40MHZ:
			num_pwr_levels = 2;
			break;
		case CH_WIDTH_80MHZ:
			num_pwr_levels = 3;
			break;
		case CH_WIDTH_160MHZ:
			num_pwr_levels = 4;
			break;
		default:
			pe_err("Invalid channel width");
			return 0;
		}
	}
	return num_pwr_levels;
}

uint8_t lim_get_max_tx_power(struct mac_context *mac,
			     struct vdev_mlme_obj *mlme_obj)
{
	uint8_t max_tx_power = 0;
	uint8_t tx_power;

	if (wlan_reg_get_fcc_constraint(mac->pdev,
					mlme_obj->reg_tpc_obj.frequency[0]))
		return mlme_obj->reg_tpc_obj.reg_max[0];

	tx_power = QDF_MIN(mlme_obj->reg_tpc_obj.reg_max[0],
			   mlme_obj->reg_tpc_obj.ap_constraint_power);

	if (tx_power >= MIN_TX_PWR_CAP && tx_power <= MAX_TX_PWR_CAP)
		max_tx_power = tx_power;
	else if (tx_power < MIN_TX_PWR_CAP)
		max_tx_power = MIN_TX_PWR_CAP;
	else
		max_tx_power = MAX_TX_PWR_CAP;

	return max_tx_power;
}

void lim_calculate_tpc(struct mac_context *mac,
		       struct pe_session *session,
		       bool is_pwr_constraint_absolute,
		       uint8_t ap_pwr_type,
		       bool ctry_code_match)
{
	bool is_psd_power = false;
	bool is_tpe_present = false, is_6ghz_freq = false;
	uint8_t i = 0;
	int8_t max_tx_power;
	uint16_t reg_max = 0, psd_power = 0;
	uint16_t local_constraint, bw_val = 0;
	uint32_t num_pwr_levels, ap_power_type_6g = 0;
	qdf_freq_t oper_freq, start_freq = 0;
	struct ch_params ch_params;
	struct vdev_mlme_obj *mlme_obj;
	uint8_t tpe_power;
	bool skip_tpe = false;

	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(session->vdev);
	if (!mlme_obj) {
		pe_err("vdev component object is NULL");
		return;
	}

	oper_freq = session->curr_op_freq;
	bw_val = wlan_reg_get_bw_value(session->ch_width);

	ch_params.ch_width = session->ch_width;
	/* start frequency calculation */
	wlan_reg_set_channel_params_for_freq(mac->pdev, oper_freq, 0,
					     &ch_params);
	if (ch_params.mhz_freq_seg1)
		start_freq = ch_params.mhz_freq_seg1 - bw_val / 2 + 10;
	else
		start_freq = ch_params.mhz_freq_seg0 - bw_val / 2 + 10;

	if (!wlan_reg_is_6ghz_chan_freq(oper_freq)) {
		reg_max = wlan_reg_get_channel_reg_power_for_freq(mac->pdev,
								  oper_freq);
		skip_tpe = wlan_mlme_skip_tpe(mac->psoc);
	} else {
		is_6ghz_freq = true;
		is_psd_power = wlan_reg_is_6g_psd_power(mac->pdev);
		/* Power mode calculation for 6G*/
		ap_power_type_6g = session->ap_power_type;
		if (LIM_IS_STA_ROLE(session)) {
			if (!session->lim_join_req) {
				if (!ctry_code_match)
					ap_power_type_6g = ap_pwr_type;
			} else {
				if (!session->lim_join_req->same_ctry_code)
					ap_power_type_6g =
					session->lim_join_req->ap_power_type_6g;
			}
		}
	}

	if (mlme_obj->reg_tpc_obj.num_pwr_levels) {
		is_tpe_present = true;
		num_pwr_levels = mlme_obj->reg_tpc_obj.num_pwr_levels;
	} else {
		num_pwr_levels = lim_get_num_pwr_levels(is_psd_power,
							session->ch_width);
	}

	ch_params.ch_width = CH_WIDTH_20MHZ;

	for (i = 0; i < num_pwr_levels; i++) {
		if (is_tpe_present) {
			if (is_6ghz_freq) {
				wlan_reg_get_client_power_for_connecting_ap(
				mac->pdev, ap_power_type_6g,
				mlme_obj->reg_tpc_obj.frequency[i],
				&is_psd_power, &reg_max, &psd_power);
			}
		} else {
			/* center frequency calculation */
			if (is_psd_power) {
				mlme_obj->reg_tpc_obj.frequency[i] =
						start_freq + (20 * i);
			} else {
				wlan_reg_set_channel_params_for_freq(
					mac->pdev, oper_freq, 0, &ch_params);
				mlme_obj->reg_tpc_obj.frequency[i] =
					ch_params.mhz_freq_seg0;
				ch_params.ch_width =
					get_next_higher_bw[ch_params.ch_width];
			}
			if (is_6ghz_freq) {
				if (LIM_IS_STA_ROLE(session)) {
					wlan_reg_get_client_power_for_connecting_ap
					(mac->pdev, ap_power_type_6g,
					 mlme_obj->reg_tpc_obj.frequency[i],
					 &is_psd_power, &reg_max, &psd_power);
				} else {
					ap_power_type_6g =
						wlan_reg_get_cur_6g_ap_pwr_type(
							mac->pdev,
							&ap_power_type_6g);
					wlan_reg_get_6g_chan_ap_power(
					mac->pdev,
					mlme_obj->reg_tpc_obj.frequency[i],
					&is_psd_power, &reg_max, &psd_power);
				}
			}
		}
		mlme_obj->reg_tpc_obj.reg_max[i] = reg_max;
		mlme_obj->reg_tpc_obj.chan_power_info[i].chan_cfreq =
					mlme_obj->reg_tpc_obj.frequency[i];

		/* max tx power calculation */
		max_tx_power = mlme_obj->reg_tpc_obj.reg_max[i];
		/* If AP local power constraint is present */
		if (mlme_obj->reg_tpc_obj.ap_constraint_power) {
			local_constraint =
				mlme_obj->reg_tpc_obj.ap_constraint_power;
			reg_max = mlme_obj->reg_tpc_obj.reg_max[i];
			if (is_pwr_constraint_absolute)
				max_tx_power = QDF_MIN(reg_max,
						       local_constraint);
			else
				max_tx_power = reg_max - local_constraint;
		}
		/* If TPE is present */
		if (is_tpe_present && !skip_tpe) {
			if (!is_psd_power && mlme_obj->reg_tpc_obj.eirp_power)
				tpe_power =  mlme_obj->reg_tpc_obj.eirp_power;
			else
				tpe_power = mlme_obj->reg_tpc_obj.tpe[i];
			max_tx_power = QDF_MIN(max_tx_power, (int8_t)tpe_power);
			pe_debug("TPE: %d", tpe_power);
		}

		/** If firmware updated max tx power is non zero,
		 * then allocate the min of firmware updated ap tx
		 * power and max power derived from above mentioned
		 * parameters.
		 */
		if (mlme_obj->mgmt.generic.tx_pwrlimit)
			max_tx_power =
				QDF_MIN(max_tx_power, (int8_t)
					mlme_obj->mgmt.generic.tx_pwrlimit);
		else
			pe_err("HW power limit from FW is zero");
		mlme_obj->reg_tpc_obj.chan_power_info[i].tx_power =
						(uint8_t)max_tx_power;

		pe_debug("freq: %d max_tx_power: %d",
			 mlme_obj->reg_tpc_obj.frequency[i], max_tx_power);
	}

	mlme_obj->reg_tpc_obj.num_pwr_levels = num_pwr_levels;
	mlme_obj->reg_tpc_obj.is_psd_power = is_psd_power;
	mlme_obj->reg_tpc_obj.eirp_power = reg_max;
	mlme_obj->reg_tpc_obj.power_type_6g = ap_power_type_6g;

	pe_debug("num_pwr_levels: %d, is_psd_power: %d, total eirp_power: %d, ap_pwr_type: %d",
		 num_pwr_levels, is_psd_power, reg_max, ap_power_type_6g);
}

/**
 * __lim_process_sme_reassoc_req() - process reassoc req
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: pointer to the SME message buffer
 *
 * This function is called to process SME_REASSOC_REQ message
 * from HDD or upper layer application.
 *
 * Return: None
 */

static void __lim_process_sme_reassoc_req(struct mac_context *mac_ctx,
					  void *msg_buf)
{
	uint16_t caps;
	uint32_t val;
	struct join_req *in_req = msg_buf;
	struct join_req *reassoc_req;
	tLimMlmReassocReq *mlm_reassoc_req;
	tSirResultCodes ret_code = eSIR_SME_SUCCESS;
	struct pe_session *session_entry = NULL;
	uint8_t session_id;
	uint8_t vdev_id;
	int8_t local_pwr_constraint = 0, reg_max = 0;
	uint32_t tele_bcn_en = 0;
	QDF_STATUS status;
	bool is_pwr_constraint;

	vdev_id = in_req->vdev_id;

	reassoc_req = qdf_mem_malloc(in_req->length);
	if (!reassoc_req) {
		ret_code = eSIR_SME_RESOURCES_UNAVAILABLE;
		goto end;
	}
	qdf_mem_copy(reassoc_req, in_req, in_req->length);

	if (!lim_is_sme_join_req_valid(mac_ctx, reassoc_req)) {
		/*
		 * Received invalid eWNI_SME_REASSOC_REQ
		 */
		pe_warn("received SME_REASSOC_REQ with invalid data");

		ret_code = eSIR_SME_INVALID_PARAMETERS;
		goto end;
	}

	session_entry = pe_find_session_by_bssid(mac_ctx,
			reassoc_req->bssDescription.bssId,
			&session_id);
	if (!session_entry) {
		pe_err("Session does not exist for given bssId");
		lim_print_mac_addr(mac_ctx, reassoc_req->bssDescription.bssId,
				LOGE);
		ret_code = eSIR_SME_INVALID_PARAMETERS;
		session_entry =
			pe_find_session_by_vdev_id(mac_ctx, vdev_id);
		if (session_entry)
			lim_handle_sme_join_result(mac_ctx,
					eSIR_SME_INVALID_PARAMETERS,
					STATUS_UNSPECIFIED_FAILURE,
					session_entry);
		goto end;
	}
#ifdef FEATURE_WLAN_DIAG_SUPPORT /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_REASSOC_REQ_EVENT,
			session_entry, QDF_STATUS_SUCCESS, QDF_STATUS_SUCCESS);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */
	/* mac_ctx->lim.gpLimReassocReq = reassoc_req;//TO SUPPORT BT-AMP */

	/* Store the reassoc handle in the session Table */
	session_entry->pLimReAssocReq = reassoc_req;

	session_entry->dot11mode = reassoc_req->dot11mode;
	session_entry->vhtCapability =
		IS_DOT11_MODE_VHT(reassoc_req->dot11mode);

	if (session_entry->vhtCapability) {
		if (session_entry->opmode == QDF_STA_MODE) {
			session_entry->vht_config.su_beam_formee =
				reassoc_req->vht_config.su_beam_formee;
		} else {
			reassoc_req->vht_config.su_beam_formee = 0;
		}
		session_entry->enableVhtpAid =
			reassoc_req->enableVhtpAid;
		session_entry->enableVhtGid =
			reassoc_req->enableVhtGid;
		pe_debug("vht su bformer [%d]", session_entry->vht_config.su_beam_former);
	}

	session_entry->supported_nss_1x1 = reassoc_req->supported_nss_1x1;
	session_entry->vdev_nss = reassoc_req->vdev_nss;
	session_entry->nss = reassoc_req->nss;
	session_entry->nss_forced_1x1 = reassoc_req->nss_forced_1x1;

	pe_debug("vhtCapability: %d su_beam_formee: %d su_tx_bformer %d",
		session_entry->vhtCapability,
		session_entry->vht_config.su_beam_formee,
		session_entry->vht_config.su_beam_former);

	session_entry->enableHtSmps = reassoc_req->enableHtSmps;
	session_entry->htSmpsvalue = reassoc_req->htSmps;
	session_entry->send_smps_action =
		reassoc_req->send_smps_action;
	pe_debug("enableHtSmps: %d htSmps: %d send action: %d supported nss 1x1: %d",
		session_entry->enableHtSmps,
		session_entry->htSmpsvalue,
		session_entry->send_smps_action,
		session_entry->supported_nss_1x1);
	/*
	 * Reassociate request is expected
	 * in link established state only.
	 */

	if (session_entry->limSmeState != eLIM_SME_LINK_EST_STATE) {
		if (session_entry->limSmeState == eLIM_SME_WT_REASSOC_STATE) {
			/*
			 * May be from 11r FT pre-auth. So lets check it
			 * before we bail out
			 */
			pe_debug("Session in reassoc state is %d",
					session_entry->peSessionId);

			/* Make sure its our preauth bssid */
			if (qdf_mem_cmp(reassoc_req->bssDescription.bssId,
					     session_entry->limReAssocbssId,
					     6)) {
				lim_print_mac_addr(mac_ctx,
						   reassoc_req->bssDescription.
						   bssId, LOGE);
				pe_err("Unknown bssId in reassoc state");
				ret_code = eSIR_SME_INVALID_PARAMETERS;
				goto end;
			}

			session_entry->vdev_id = vdev_id;
			mlm_reassoc_req =
				qdf_mem_malloc(sizeof(*mlm_reassoc_req));
			if (!mlm_reassoc_req) {
				ret_code = eSIR_SME_RESOURCES_UNAVAILABLE;
				goto end;
			}

			/* Update PE sessionId */
			mlm_reassoc_req->sessionId = session_entry->peSessionId;
			status = lim_send_ft_reassoc_req(session_entry,
							 mlm_reassoc_req);
			if (QDF_IS_STATUS_ERROR(status)) {
				qdf_mem_free(mlm_reassoc_req);
				ret_code = eSIR_SME_REFUSED;
				goto end;
			}
			return;
		}
		/*
		 * Should not have received eWNI_SME_REASSOC_REQ
		 */
		pe_err("received unexpected SME_REASSOC_REQ in state %X",
			session_entry->limSmeState);
		lim_print_sme_state(mac_ctx, LOGE, session_entry->limSmeState);

		ret_code = eSIR_SME_UNEXPECTED_REQ_RESULT_CODE;
		goto end;
	}

	qdf_mem_copy(session_entry->limReAssocbssId,
		     session_entry->pLimReAssocReq->bssDescription.bssId,
		     sizeof(tSirMacAddr));

	session_entry->lim_reassoc_chan_freq =
		session_entry->pLimReAssocReq->bssDescription.chan_freq;
	session_entry->reAssocHtSupportedChannelWidthSet =
		(session_entry->pLimReAssocReq->cbMode) ? 1 : 0;
	session_entry->reAssocHtRecommendedTxWidthSet =
		session_entry->reAssocHtSupportedChannelWidthSet;
	session_entry->reAssocHtSecondaryChannelOffset =
		session_entry->pLimReAssocReq->cbMode;

	session_entry->limReassocBssCaps =
		session_entry->pLimReAssocReq->bssDescription.capabilityInfo;
	reg_max = wlan_reg_get_channel_reg_power_for_freq(
		mac_ctx->pdev, session_entry->curr_op_freq);
	local_pwr_constraint = reg_max;

	lim_extract_ap_capability(mac_ctx,
		(uint8_t *)session_entry->pLimReAssocReq->bssDescription.ieFields,
		lim_get_ielen_from_bss_description(
			&session_entry->pLimReAssocReq->bssDescription),
		&session_entry->limReassocBssQosCaps,
		&session_entry->gLimCurrentBssUapsd,
		&local_pwr_constraint, session_entry,
		&is_pwr_constraint);
	if (is_pwr_constraint)
		local_pwr_constraint = reg_max - local_pwr_constraint;

	session_entry->maxTxPower = QDF_MIN(reg_max, (local_pwr_constraint));
	pe_err("Reg max = %d, local pwr constraint = %d, max tx = %d",
		reg_max, local_pwr_constraint, session_entry->maxTxPower);
	/* Copy the SSID from session entry to local variable */
	session_entry->limReassocSSID.length = reassoc_req->ssId.length;
	qdf_mem_copy(session_entry->limReassocSSID.ssId,
		     reassoc_req->ssId.ssId,
		     session_entry->limReassocSSID.length);
	if (session_entry->gLimCurrentBssUapsd) {
		session_entry->gUapsdPerAcBitmask =
			session_entry->pLimReAssocReq->uapsdPerAcBitmask;
		pe_debug("UAPSD flag for all AC - 0x%2x",
			session_entry->gUapsdPerAcBitmask);
	}

	mlm_reassoc_req = qdf_mem_malloc(sizeof(tLimMlmReassocReq));
	if (!mlm_reassoc_req) {
		ret_code = eSIR_SME_RESOURCES_UNAVAILABLE;
		goto end;
	}

	qdf_mem_copy(mlm_reassoc_req->peerMacAddr,
		     session_entry->limReAssocbssId, sizeof(tSirMacAddr));

	if (lim_get_capability_info(mac_ctx, &caps, session_entry) !=
	    QDF_STATUS_SUCCESS)
		pe_err("could not retrieve Capabilities value");

	lim_update_caps_info_for_bss(mac_ctx, &caps,
				reassoc_req->bssDescription.capabilityInfo);
	pe_debug("Capabilities info Reassoc: 0x%X", caps);

	mlm_reassoc_req->capabilityInfo = caps;

	/* Update PE session_id */
	mlm_reassoc_req->sessionId = session_id;

	/*
	 * If telescopic beaconing is enabled, set listen interval to
	 * CFG_TELE_BCN_MAX_LI
	 */

	tele_bcn_en = mac_ctx->mlme_cfg->sap_cfg.tele_bcn_wakeup_en;

	if (tele_bcn_en)
		val = mac_ctx->mlme_cfg->sap_cfg.tele_bcn_max_li;
	else
		val = mac_ctx->mlme_cfg->sap_cfg.listen_interval;

	mlm_reassoc_req->listenInterval = (uint16_t) val;

	/* Indicate whether spectrum management is enabled */
	session_entry->spectrumMgtEnabled = reassoc_req->spectrumMgtIndicator;

	/* Enable the spectrum management if this is a DFS channel */
	if (session_entry->country_info_present &&
	    lim_isconnected_on_dfs_channel(
		mac_ctx, wlan_reg_freq_to_chan(
		mac_ctx->pdev, session_entry->curr_op_freq)))
		session_entry->spectrumMgtEnabled = true;

	session_entry->limPrevSmeState = session_entry->limSmeState;
	session_entry->limSmeState = eLIM_SME_WT_REASSOC_STATE;

	MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
				session_entry->peSessionId,
				session_entry->limSmeState));

	status = lim_send_reassoc_req(session_entry, mlm_reassoc_req);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(mlm_reassoc_req);
		ret_code = eSIR_SME_REFUSED;
		goto end;
	}

	return;
end:
	if (reassoc_req) {
		qdf_mem_free(reassoc_req);
		if (session_entry)
			session_entry->pLimReAssocReq = NULL;
	}

	if (session_entry)
		/*
		 * error occurred after we determined the session so
		 * extract session id from there, otherwise we'll use
		 * the value already extracted from the message
		 */
		vdev_id = session_entry->vdev_id;

	/*
	 * Send Reassoc failure response to host
	 * (note session_entry may be NULL, but that's OK)
	 */
	lim_send_sme_join_reassoc_rsp(mac_ctx, eWNI_SME_REASSOC_RSP,
				      ret_code, STATUS_UNSPECIFIED_FAILURE,
				      session_entry, vdev_id);
}

bool send_disassoc_frame = 1;
/**
 * __lim_process_sme_disassoc_req()
 *
 ***FUNCTION:
 * This function is called to process SME_DISASSOC_REQ message
 * from HDD or upper layer application.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param  mac      Pointer to Global MAC structure
 * @param  *msg_buf  A pointer to the SME message buffer
 * @return None
 */

static void __lim_process_sme_disassoc_req(struct mac_context *mac,
					   uint32_t *msg_buf)
{
	uint16_t disassocTrigger, reasonCode;
	tLimMlmDisassocReq *pMlmDisassocReq;
	tSirResultCodes retCode = eSIR_SME_SUCCESS;
	struct disassoc_req smeDisassocReq;
	struct pe_session *pe_session = NULL;
	uint8_t sessionId;
	uint8_t smesessionId;

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	qdf_mem_copy(&smeDisassocReq, msg_buf, sizeof(struct disassoc_req));
	smesessionId = smeDisassocReq.sessionId;
	if (!lim_is_sme_disassoc_req_valid(mac,
					   &smeDisassocReq,
					   pe_session)) {
		pe_err("received invalid SME_DISASSOC_REQ message");
		if (mac->lim.gLimRspReqd) {
			mac->lim.gLimRspReqd = false;

			retCode = eSIR_SME_INVALID_PARAMETERS;
			disassocTrigger = eLIM_HOST_DISASSOC;
			goto sendDisassoc;
		}

		return;
	}

	pe_session = pe_find_session_by_bssid(mac,
				smeDisassocReq.bssid.bytes,
				&sessionId);
	if (!pe_session) {
		pe_err("session does not exist for given bssId "
			   QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(smeDisassocReq.bssid.bytes));
		retCode = eSIR_SME_INVALID_PARAMETERS;
		disassocTrigger = eLIM_HOST_DISASSOC;
		goto sendDisassoc;
	}
	pe_debug("vdev %d (session %d) Systemrole %d Reason: %u SmeState: %d limMlmState %d ho fail %d  send OTA %d from: " QDF_MAC_ADDR_FMT " bssid :" QDF_MAC_ADDR_FMT,
		 pe_session->vdev_id, pe_session->peSessionId,
		 GET_LIM_SYSTEM_ROLE(pe_session), smeDisassocReq.reasonCode,
		 pe_session->limSmeState, pe_session->limMlmState,
		 smeDisassocReq.process_ho_fail,
		 smeDisassocReq.doNotSendOverTheAir,
		 QDF_MAC_ADDR_REF(smeDisassocReq.peer_macaddr.bytes),
		 QDF_MAC_ADDR_REF(smeDisassocReq.bssid.bytes));

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_diag_event_report(mac, WLAN_PE_DIAG_DISASSOC_REQ_EVENT, pe_session,
			      0, smeDisassocReq.reasonCode);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	pe_session->smeSessionId = smesessionId;
	pe_session->process_ho_fail = smeDisassocReq.process_ho_fail;

	switch (GET_LIM_SYSTEM_ROLE(pe_session)) {
	case eLIM_STA_ROLE:
		switch (pe_session->limSmeState) {
		case eLIM_SME_ASSOCIATED_STATE:
		case eLIM_SME_LINK_EST_STATE:
			pe_session->limPrevSmeState =
				pe_session->limSmeState;
			pe_session->limSmeState = eLIM_SME_WT_DISASSOC_STATE;
			/* Delete all TDLS peers connected before leaving BSS */
			lim_delete_tdls_peers(mac, pe_session);
			MTRACE(mac_trace(mac, TRACE_CODE_SME_STATE,
				pe_session->peSessionId,
				pe_session->limSmeState));
			break;

		case eLIM_SME_WT_DEAUTH_STATE:
			/* PE shall still process the DISASSOC_REQ and proceed with
			 * link tear down even if it had already sent a DEAUTH_IND to
			 * to SME. mac->lim.gLimPrevSmeState shall remain the same as
			 * its been set when PE entered WT_DEAUTH_STATE.
			 */
			pe_session->limSmeState = eLIM_SME_WT_DISASSOC_STATE;
			MTRACE(mac_trace
				       (mac, TRACE_CODE_SME_STATE,
				       pe_session->peSessionId,
				       pe_session->limSmeState));
			break;

		case eLIM_SME_WT_DISASSOC_STATE:
			/* PE Received a Disassoc frame. Normally it gets DISASSOC_CNF but it
			 * received DISASSOC_REQ. Which means host is also trying to disconnect.
			 * PE can continue processing DISASSOC_REQ and send the response instead
			 * of failing the request. SME will anyway ignore DEAUTH_IND that was sent
			 * for disassoc frame.
			 *
			 * It will send a disassoc, which is ok. However, we can use the global flag
			 * sendDisassoc to not send disassoc frame.
			 */
			break;

		case eLIM_SME_JOIN_FAILURE_STATE: {
			/* Already in Disconnected State, return success */
			if (mac->lim.gLimRspReqd) {
				retCode = eSIR_SME_SUCCESS;
				disassocTrigger = eLIM_HOST_DISASSOC;
				goto sendDisassoc;
			}
		}
		break;
		default:
			/**
			 * STA is not currently associated.
			 * Log error and send response to host
			 */
			pe_err("received unexpected SME_DISASSOC_REQ in state %X",
				pe_session->limSmeState);
			lim_print_sme_state(mac, LOGE,
				pe_session->limSmeState);

			if (mac->lim.gLimRspReqd) {
				if (pe_session->limSmeState !=
				    eLIM_SME_WT_ASSOC_STATE)
					mac->lim.gLimRspReqd = false;

				retCode = eSIR_SME_UNEXPECTED_REQ_RESULT_CODE;
				disassocTrigger = eLIM_HOST_DISASSOC;
				goto sendDisassoc;
			}

			return;
		}

		break;

	case eLIM_AP_ROLE:
		/* Fall through */
		break;

	default:
		/* eLIM_UNKNOWN_ROLE */
		pe_err("received unexpected SME_DISASSOC_REQ for role %d",
			GET_LIM_SYSTEM_ROLE(pe_session));

		retCode = eSIR_SME_UNEXPECTED_REQ_RESULT_CODE;
		disassocTrigger = eLIM_HOST_DISASSOC;
		goto sendDisassoc;
	} /* end switch (mac->lim.gLimSystemRole) */

	disassocTrigger = eLIM_HOST_DISASSOC;
	reasonCode = smeDisassocReq.reasonCode;

	if (smeDisassocReq.doNotSendOverTheAir)
		send_disassoc_frame = 0;

	pMlmDisassocReq = qdf_mem_malloc(sizeof(tLimMlmDisassocReq));
	if (!pMlmDisassocReq)
		return;

	qdf_copy_macaddr(&pMlmDisassocReq->peer_macaddr,
			 &smeDisassocReq.peer_macaddr);

	pMlmDisassocReq->reasonCode = reasonCode;
	pMlmDisassocReq->disassocTrigger = disassocTrigger;

	/* Update PE session ID */
	pMlmDisassocReq->sessionId = sessionId;

	lim_post_mlm_message(mac,
			     LIM_MLM_DISASSOC_REQ, (uint32_t *) pMlmDisassocReq);
	return;

sendDisassoc:
	if (pe_session)
		lim_send_sme_disassoc_ntf(mac,
					  smeDisassocReq.peer_macaddr.bytes,
					  retCode,
					  disassocTrigger,
					  1, smesessionId,
					  pe_session);
	else
		lim_send_sme_disassoc_ntf(mac,
					  smeDisassocReq.peer_macaddr.bytes,
					  retCode, disassocTrigger, 1,
					  smesessionId, NULL);

} /*** end __lim_process_sme_disassoc_req() ***/

/** -----------------------------------------------------------------
   \brief __lim_process_sme_disassoc_cnf() - Process SME_DISASSOC_CNF

   This function is called to process SME_DISASSOC_CNF message
   from HDD or upper layer application.

   \param mac - global mac structure
   \param sta - station dph hash node
   \return none
   \sa
   ----------------------------------------------------------------- */
void __lim_process_sme_disassoc_cnf(struct mac_context *mac, uint32_t *msg_buf)
{
	struct disassoc_cnf smeDisassocCnf;
	uint16_t aid;
	tpDphHashNode sta;
	struct pe_session *pe_session;
	uint8_t sessionId;
	uint32_t *msg = NULL;
	QDF_STATUS status;

	qdf_mem_copy(&smeDisassocCnf, msg_buf, sizeof(smeDisassocCnf));

	pe_session = pe_find_session_by_bssid(mac,
				smeDisassocCnf.bssid.bytes,
				&sessionId);
	if (!pe_session) {
		pe_err("session does not exist for given bssId");
		status = lim_prepare_disconnect_done_ind(mac, &msg,
						smeDisassocCnf.vdev_id,
						eSIR_SME_INVALID_SESSION,
						NULL);
		if (QDF_IS_STATUS_SUCCESS(status))
			lim_send_sme_disassoc_deauth_ntf(mac,
							 QDF_STATUS_SUCCESS,
							 (uint32_t *)msg);
		return;
	}

	if (!lim_is_sme_disassoc_cnf_valid(mac, &smeDisassocCnf, pe_session)) {
		pe_err("received invalid SME_DISASSOC_CNF message");
		status = lim_prepare_disconnect_done_ind(mac, &msg,
						pe_session->smeSessionId,
						eSIR_SME_INVALID_PARAMETERS,
						&smeDisassocCnf.bssid.bytes[0]);
		if (QDF_IS_STATUS_SUCCESS(status))
			lim_send_sme_disassoc_deauth_ntf(mac,
							 QDF_STATUS_SUCCESS,
							 (uint32_t *)msg);
		return;
	}
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	if (smeDisassocCnf.messageType == eWNI_SME_DISASSOC_CNF)
		lim_diag_event_report(mac, WLAN_PE_DIAG_DISASSOC_CNF_EVENT,
				      pe_session,
				      (uint16_t)smeDisassocCnf.status_code, 0);
	else if (smeDisassocCnf.messageType == eWNI_SME_DEAUTH_CNF)
		lim_diag_event_report(mac, WLAN_PE_DIAG_DEAUTH_CNF_EVENT,
				      pe_session,
				      (uint16_t)smeDisassocCnf.status_code, 0);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	switch (GET_LIM_SYSTEM_ROLE(pe_session)) {
	case eLIM_STA_ROLE:
		if ((pe_session->limSmeState != eLIM_SME_IDLE_STATE) &&
		    (pe_session->limSmeState != eLIM_SME_WT_DISASSOC_STATE)
		    && (pe_session->limSmeState !=
			eLIM_SME_WT_DEAUTH_STATE)) {
			pe_err("received unexp SME_DISASSOC_CNF in state %X",
				pe_session->limSmeState);
			lim_print_sme_state(mac, LOGE,
					    pe_session->limSmeState);
			status = lim_prepare_disconnect_done_ind(mac, &msg,
						pe_session->smeSessionId,
						eSIR_SME_INVALID_STATE,
						&smeDisassocCnf.bssid.bytes[0]);
			if (QDF_IS_STATUS_SUCCESS(status))
				lim_send_sme_disassoc_deauth_ntf(mac,
							QDF_STATUS_SUCCESS,
							(uint32_t *)msg);
			return;
		}
		break;

	case eLIM_AP_ROLE:
		/* Fall through */
		break;
	default:                /* eLIM_UNKNOWN_ROLE */
		pe_err("received unexpected SME_DISASSOC_CNF role %d",
			GET_LIM_SYSTEM_ROLE(pe_session));
		status = lim_prepare_disconnect_done_ind(mac, &msg,
						pe_session->smeSessionId,
						eSIR_SME_INVALID_STATE,
						&smeDisassocCnf.bssid.bytes[0]);
		if (QDF_IS_STATUS_SUCCESS(status))
			lim_send_sme_disassoc_deauth_ntf(mac,
							 QDF_STATUS_SUCCESS,
							 (uint32_t *)msg);
		return;
	}

	if ((pe_session->limSmeState == eLIM_SME_WT_DISASSOC_STATE) ||
	    (pe_session->limSmeState == eLIM_SME_WT_DEAUTH_STATE) ||
	    LIM_IS_AP_ROLE(pe_session)) {
		sta = dph_lookup_hash_entry(mac,
				smeDisassocCnf.peer_macaddr.bytes, &aid,
				&pe_session->dph.dphHashTable);
		if (!sta) {
			pe_err("DISASSOC_CNF for a STA with no context, addr= "
				QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(smeDisassocCnf.peer_macaddr.bytes));
			status = lim_prepare_disconnect_done_ind(mac, &msg,
						pe_session->smeSessionId,
						eSIR_SME_INVALID_PARAMETERS,
						&smeDisassocCnf.bssid.bytes[0]);
			if (QDF_IS_STATUS_SUCCESS(status))
				lim_send_sme_disassoc_deauth_ntf(mac,
							QDF_STATUS_SUCCESS,
							(uint32_t *)msg);
			return;
		}

		if ((sta->mlmStaContext.mlmState ==
				eLIM_MLM_WT_DEL_STA_RSP_STATE) ||
			(sta->mlmStaContext.mlmState ==
				eLIM_MLM_WT_DEL_BSS_RSP_STATE)) {
			pe_err("No need of cleanup for addr:" QDF_MAC_ADDR_FMT "as MLM state is %d",
				QDF_MAC_ADDR_REF(smeDisassocCnf.peer_macaddr.bytes),
				sta->mlmStaContext.mlmState);
			status = lim_prepare_disconnect_done_ind(mac, &msg,
						pe_session->smeSessionId,
						eSIR_SME_SUCCESS,
						NULL);
			if (QDF_IS_STATUS_SUCCESS(status))
				lim_send_sme_disassoc_deauth_ntf(mac,
							QDF_STATUS_SUCCESS,
							(uint32_t *)msg);
			return;
		}

		/* Delete FT session if there exists one */
		lim_ft_cleanup_pre_auth_info(mac, pe_session);
		lim_cleanup_rx_path(mac, sta, pe_session, true);

		lim_clean_up_disassoc_deauth_req(mac,
				 (char *)&smeDisassocCnf.peer_macaddr, 0);
	}

	return;
}

/**
 * __lim_process_sme_deauth_req() - process sme deauth req
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: pointer to the SME message buffer
 *
 * This function is called to process SME_DEAUTH_REQ message
 * from HDD or upper layer application.
 *
 * Return: None
 */

static void __lim_process_sme_deauth_req(struct mac_context *mac_ctx,
		uint32_t *msg_buf)
{
	uint16_t deauth_trigger, reason_code;
	tLimMlmDeauthReq *mlm_deauth_req;
	struct deauth_req sme_deauth_req;
	tSirResultCodes ret_code = eSIR_SME_SUCCESS;
	struct pe_session *session_entry;
	uint8_t session_id;      /* PE sessionId */
	uint8_t vdev_id;

	qdf_mem_copy(&sme_deauth_req, msg_buf, sizeof(sme_deauth_req));
	vdev_id = sme_deauth_req.vdev_id;

	/*
	 * We need to get a session first but we don't even know
	 * if the message is correct.
	 */
	session_entry = pe_find_session_by_bssid(mac_ctx,
					sme_deauth_req.bssid.bytes,
					&session_id);
	if (!session_entry) {
		pe_err("session does not exist for given bssId");
		ret_code = eSIR_SME_INVALID_PARAMETERS;
		deauth_trigger = eLIM_HOST_DEAUTH;
		goto send_deauth;
	}

	if (!lim_is_sme_deauth_req_valid(mac_ctx, &sme_deauth_req,
				session_entry)) {
		pe_err("received invalid SME_DEAUTH_REQ message");
		mac_ctx->lim.gLimRspReqd = false;

		ret_code = eSIR_SME_INVALID_PARAMETERS;
		deauth_trigger = eLIM_HOST_DEAUTH;
		goto send_deauth;
	}
	pe_debug("vdev %d (session %d) Systemrole %d reasoncode %u limSmestate %d limMlmState %d from " QDF_MAC_ADDR_FMT " bssid : " QDF_MAC_ADDR_FMT,
		 vdev_id, session_entry->peSessionId,
		 GET_LIM_SYSTEM_ROLE(session_entry), sme_deauth_req.reasonCode,
		 session_entry->limSmeState, session_entry->limMlmState,
		 QDF_MAC_ADDR_REF(sme_deauth_req.peer_macaddr.bytes),
		 QDF_MAC_ADDR_REF(sme_deauth_req.bssid.bytes));
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_DEAUTH_REQ_EVENT,
			session_entry, 0, sme_deauth_req.reasonCode);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	session_entry->vdev_id = vdev_id;

	switch (GET_LIM_SYSTEM_ROLE(session_entry)) {
	case eLIM_STA_ROLE:
		switch (session_entry->limSmeState) {
		case eLIM_SME_ASSOCIATED_STATE:
		case eLIM_SME_LINK_EST_STATE:
			/* Delete all TDLS peers connected before leaving BSS */
			lim_delete_tdls_peers(mac_ctx, session_entry);
		/* fallthrough */
		case eLIM_SME_WT_ASSOC_STATE:
		case eLIM_SME_JOIN_FAILURE_STATE:
		case eLIM_SME_IDLE_STATE:
			session_entry->limPrevSmeState =
				session_entry->limSmeState;
			session_entry->limSmeState = eLIM_SME_WT_DEAUTH_STATE;
			MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
				       session_entry->peSessionId,
				       session_entry->limSmeState));
			/* Send Deauthentication request to MLM below */
			break;
		case eLIM_SME_WT_DEAUTH_STATE:
		case eLIM_SME_WT_DISASSOC_STATE:
			/*
			 * PE Received a Deauth/Disassoc frame. Normally it get
			 * DEAUTH_CNF/DISASSOC_CNF but it received DEAUTH_REQ.
			 * Which means host is also trying to disconnect.
			 * PE can continue processing DEAUTH_REQ and send
			 * the response instead of failing the request.
			 * SME will anyway ignore DEAUTH_IND/DISASSOC_IND that
			 * was sent for deauth/disassoc frame.
			 */
			session_entry->limSmeState = eLIM_SME_WT_DEAUTH_STATE;
			break;
		default:
			/*
			 * STA is not in a state to deauthenticate with
			 * peer. Log error and send response to host.
			 */
			pe_err("received unexp SME_DEAUTH_REQ in state %X",
				session_entry->limSmeState);
			lim_print_sme_state(mac_ctx, LOGE,
					    session_entry->limSmeState);

			if (mac_ctx->lim.gLimRspReqd) {
				mac_ctx->lim.gLimRspReqd = false;

				ret_code = eSIR_SME_STA_NOT_AUTHENTICATED;
				deauth_trigger = eLIM_HOST_DEAUTH;

				/*
				 * here we received deauth request from AP so
				 * sme state is eLIM_SME_WT_DEAUTH_STATE.if we
				 * have ISSUED delSta then mlm state should be
				 * eLIM_MLM_WT_DEL_STA_RSP_STATE and ifwe got
				 * delBSS rsp then mlm state should be
				 * eLIM_MLM_IDLE_STATE so the below condition
				 * captures the state where delSta not done
				 * and firmware still in connected state.
				 */
				if (session_entry->limSmeState ==
					eLIM_SME_WT_DEAUTH_STATE &&
					session_entry->limMlmState !=
					eLIM_MLM_IDLE_STATE &&
					session_entry->limMlmState !=
					eLIM_MLM_WT_DEL_STA_RSP_STATE)
					ret_code = eSIR_SME_DEAUTH_STATUS;
				goto send_deauth;
			}
			return;
		}
		break;
	case eLIM_AP_ROLE:
		break;
	default:
		pe_err("received unexpected SME_DEAUTH_REQ for role %X",
			GET_LIM_SYSTEM_ROLE(session_entry));
		if (mac_ctx->lim.gLimRspReqd) {
			mac_ctx->lim.gLimRspReqd = false;
			ret_code = eSIR_SME_INVALID_PARAMETERS;
			deauth_trigger = eLIM_HOST_DEAUTH;
			goto send_deauth;
		}
		return;
	} /* end switch (mac_ctx->lim.gLimSystemRole) */

	if (sme_deauth_req.reasonCode == eLIM_LINK_MONITORING_DEAUTH &&
	    session_entry->limSystemRole == eLIM_STA_ROLE) {
		/* Deauthentication is triggered by Link Monitoring */
		pe_debug("** Lost link with AP **");
		deauth_trigger = eLIM_LINK_MONITORING_DEAUTH;
		reason_code = REASON_UNSPEC_FAILURE;
	} else {
		deauth_trigger = eLIM_HOST_DEAUTH;
		reason_code = sme_deauth_req.reasonCode;
	}

	/* Trigger Deauthentication frame to peer MAC entity */
	mlm_deauth_req = qdf_mem_malloc(sizeof(tLimMlmDeauthReq));
	if (!mlm_deauth_req) {
		if (mac_ctx->lim.gLimRspReqd) {
			mac_ctx->lim.gLimRspReqd = false;
			ret_code = eSIR_SME_RESOURCES_UNAVAILABLE;
			deauth_trigger = eLIM_HOST_DEAUTH;
			goto send_deauth;
		}
		return;
	}

	qdf_copy_macaddr(&mlm_deauth_req->peer_macaddr,
			 &sme_deauth_req.peer_macaddr);

	mlm_deauth_req->reasonCode = reason_code;
	mlm_deauth_req->deauthTrigger = deauth_trigger;

	/* Update PE session Id */
	mlm_deauth_req->sessionId = session_id;
	lim_process_mlm_deauth_req(mac_ctx, (uint32_t *)mlm_deauth_req);

	return;

send_deauth:
	lim_send_sme_deauth_ntf(mac_ctx, sme_deauth_req.peer_macaddr.bytes,
				ret_code, deauth_trigger, 1, vdev_id);
}

/**
 * __lim_counter_measures()
 *
 * FUNCTION:
 * This function is called to "implement" MIC counter measure
 * and is *temporary* only
 *
 * LOGIC: on AP, disassoc all STA associated thru TKIP,
 * we don't do the proper STA disassoc sequence since the
 * BSS will be stopped anyway
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param  mac      Pointer to Global MAC structure
 * @return None
 */

static void __lim_counter_measures(struct mac_context *mac, struct pe_session *pe_session)
{
	tSirMacAddr mac_addr = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

	if (LIM_IS_AP_ROLE(pe_session))
		lim_send_disassoc_mgmt_frame(mac, REASON_MIC_FAILURE,
					     mac_addr, pe_session, false);
};

void lim_send_stop_bss_failure_resp(struct mac_context *mac_ctx,
				    struct pe_session *session)
{
	session->limSmeState = session->limPrevSmeState;

	MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE, session->peSessionId,
			  session->limSmeState));

	lim_send_sme_rsp(mac_ctx, eWNI_SME_STOP_BSS_RSP,
			 eSIR_SME_STOP_BSS_FAILURE, session->smeSessionId);
}

void lim_delete_all_peers(struct pe_session *session)
{
	uint8_t i = 0;
	struct mac_context *mac_ctx = session->mac_ctx;
	tpDphHashNode sta_ds = NULL;
	QDF_STATUS status;
	tSirMacAddr bc_addr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

	/* IBSS and NDI doesn't send Disassoc frame */
	if (!LIM_IS_NDI_ROLE(session)) {
		pe_debug("stop_bss_reason: %d", session->stop_bss_reason);
		if (session->stop_bss_reason == eSIR_SME_MIC_COUNTER_MEASURES)
			__lim_counter_measures(mac_ctx, session);
		else
			lim_send_disassoc_mgmt_frame(mac_ctx,
				REASON_DEAUTH_NETWORK_LEAVING,
				bc_addr, session, false);
	}

	for (i = 1; i < session->dph.dphHashTable.size; i++) {
		sta_ds = dph_get_hash_entry(mac_ctx, i,
					    &session->dph.dphHashTable);
		if (!sta_ds)
			continue;
		status = lim_del_sta(mac_ctx, sta_ds, true, session);
		if (QDF_STATUS_SUCCESS == status) {
			lim_delete_dph_hash_entry(mac_ctx, sta_ds->staAddr,
						  sta_ds->assocId, session);
			lim_release_peer_idx(mac_ctx, sta_ds->assocId, session);
		} else {
			pe_err("lim_del_sta failed with Status: %d", status);
			QDF_ASSERT(0);
		}
	}
	lim_disconnect_complete(session, false);
	if (mac_ctx->del_peers_ind_cb)
		mac_ctx->del_peers_ind_cb(mac_ctx->psoc, session->vdev_id);
}

QDF_STATUS lim_sta_send_del_bss(struct pe_session *session)
{
	struct mac_context *mac_ctx = session->mac_ctx;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	tpDphHashNode sta_ds = NULL;

	sta_ds = dph_get_hash_entry(mac_ctx, DPH_STA_HASH_INDEX_PEER,
				    &session->dph.dphHashTable);
	if (!sta_ds) {
		pe_err("DPH Entry for STA is missing, failed to send delbss");
		goto end;
	}

	status = lim_del_bss(mac_ctx, sta_ds, 0, session);
	if (QDF_IS_STATUS_ERROR(status))
		pe_err("delBss failed for bss %d", session->vdev_id);

end:
	return status;
}

QDF_STATUS lim_send_vdev_stop(struct pe_session *session)
{
	struct mac_context *mac_ctx = session->mac_ctx;
	QDF_STATUS status;

	status = lim_del_bss(mac_ctx, NULL, session->vdev_id, session);

	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("delBss failed for bss %d", session->vdev_id);
		lim_send_stop_bss_failure_resp(mac_ctx, session);
	}

	return status;
}

/**
 * lim_delete_peers_and_send_vdev_stop() -delete peers and send vdev stop
 * @session: session pointer
 *
 * Return None
 */
static void lim_delete_peers_and_send_vdev_stop(struct pe_session *session)
{
	struct mac_context *mac_ctx = session->mac_ctx;
	QDF_STATUS status;

	if (QDF_IS_STATUS_SUCCESS(
	    wlan_vdev_is_restart_progress(session->vdev)))
		status =
		 wlan_vdev_mlme_sm_deliver_evt(session->vdev,
					       WLAN_VDEV_SM_EV_RESTART_REQ_FAIL,
					       sizeof(*session), session);
	else
		status = wlan_vdev_mlme_sm_deliver_evt(session->vdev,
						       WLAN_VDEV_SM_EV_DOWN,
						       sizeof(*session),
						       session);
	if (QDF_IS_STATUS_ERROR(status))
		lim_send_stop_bss_failure_resp(mac_ctx, session);
}

static void
__lim_handle_sme_stop_bss_request(struct mac_context *mac, uint32_t *msg_buf)
{
	struct stop_bss_req stop_bss_req;
	tLimSmeStates prevState;
	struct pe_session *pe_session;
	uint8_t smesessionId;
	uint8_t sessionId;

	qdf_mem_copy(&stop_bss_req, msg_buf, sizeof(stop_bss_req));
	smesessionId = stop_bss_req.sessionId;

	if (!lim_is_sme_stop_bss_req_valid(msg_buf)) {
		pe_warn("received invalid SME_STOP_BSS_REQ message");
		/* Send Stop BSS response to host */
		lim_send_sme_rsp(mac, eWNI_SME_STOP_BSS_RSP,
				 eSIR_SME_INVALID_PARAMETERS, smesessionId);
		return;
	}

	pe_session = pe_find_session_by_bssid(mac,
				stop_bss_req.bssid.bytes,
				&sessionId);
	if (!pe_session) {
		pe_err("session does not exist for given BSSID");
		lim_send_sme_rsp(mac, eWNI_SME_STOP_BSS_RSP,
				 eSIR_SME_INVALID_PARAMETERS, smesessionId);
		return;
	}
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_diag_event_report(mac, WLAN_PE_DIAG_STOP_BSS_REQ_EVENT, pe_session,
			      0, 0);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	if (pe_session->limSmeState != eLIM_SME_NORMAL_STATE ||    /* Added For BT -AMP Support */
	    LIM_IS_STA_ROLE(pe_session)) {
		/**
		 * Should not have received STOP_BSS_REQ in states
		 * other than 'normal' state or on STA in Infrastructure
		 * mode. Log error and return response to host.
		 */
		pe_err("received unexpected SME_STOP_BSS_REQ in state %X, for role %d",
			pe_session->limSmeState,
			GET_LIM_SYSTEM_ROLE(pe_session));
		lim_print_sme_state(mac, LOGE, pe_session->limSmeState);
		/* / Send Stop BSS response to host */
		lim_send_sme_rsp(mac, eWNI_SME_STOP_BSS_RSP,
				 eSIR_SME_UNEXPECTED_REQ_RESULT_CODE,
				 smesessionId);
		return;
	}

	if (LIM_IS_AP_ROLE(pe_session))
		lim_wpspbc_close(mac, pe_session);

	pe_debug("RECEIVED STOP_BSS_REQ with reason code=%d",
		stop_bss_req.reasonCode);

	prevState = pe_session->limSmeState;
	pe_session->limPrevSmeState = prevState;

	pe_session->limSmeState = eLIM_SME_IDLE_STATE;
	MTRACE(mac_trace
		       (mac, TRACE_CODE_SME_STATE, pe_session->peSessionId,
		       pe_session->limSmeState));

	pe_session->smeSessionId = smesessionId;
	pe_session->stop_bss_reason = stop_bss_req.reasonCode;

	if (!LIM_IS_NDI_ROLE(pe_session)) {
		/* Free the buffer allocated in START_BSS_REQ */
		qdf_mem_free(pe_session->add_ie_params.probeRespData_buff);
		pe_session->add_ie_params.probeRespDataLen = 0;
		pe_session->add_ie_params.probeRespData_buff = NULL;

		qdf_mem_free(pe_session->add_ie_params.assocRespData_buff);
		pe_session->add_ie_params.assocRespDataLen = 0;
		pe_session->add_ie_params.assocRespData_buff = NULL;

		qdf_mem_free(pe_session->add_ie_params.probeRespBCNData_buff);
		pe_session->add_ie_params.probeRespBCNDataLen = 0;
		pe_session->add_ie_params.probeRespBCNData_buff = NULL;
	}

	lim_delete_peers_and_send_vdev_stop(pe_session);

}

/**
 * __lim_process_sme_stop_bss_req() - Process STOP_BSS from SME
 * @mac: Global MAC context
 * @pMsg: Message from SME
 *
 * Wrapper for the function __lim_handle_sme_stop_bss_request
 * This message will be defered until softmac come out of
 * scan mode. Message should be handled even if we have
 * detected radar in the current operating channel.
 *
 * Return: true - If we consumed the buffer
 *         false - If have defered the message.
 */

static bool __lim_process_sme_stop_bss_req(struct mac_context *mac,
					   struct scheduler_msg *pMsg)
{
	__lim_handle_sme_stop_bss_request(mac, (uint32_t *) pMsg->bodyptr);
	return true;
} /*** end __lim_process_sme_stop_bss_req() ***/

void lim_process_sme_del_bss_rsp(struct mac_context *mac,
				 struct pe_session *pe_session)
{
	SET_LIM_PROCESS_DEFD_MESGS(mac, true);
	dph_hash_table_init(mac, &pe_session->dph.dphHashTable);
	lim_delete_pre_auth_list(mac);
	lim_send_sme_rsp(mac, eWNI_SME_STOP_BSS_RSP, eSIR_SME_SUCCESS,
			 pe_session->smeSessionId);
	return;
}

/**
 * __lim_process_sme_assoc_cnf_new() - process sme assoc/reassoc cnf
 *
 * @mac_ctx: pointer to mac context
 * @msg_type: message type
 * @msg_buf: pointer to the SME message buffer
 *
 * This function handles SME_ASSOC_CNF/SME_REASSOC_CNF
 * in BTAMP AP.
 *
 * Return: None
 */

void __lim_process_sme_assoc_cnf_new(struct mac_context *mac_ctx, uint32_t msg_type,
				uint32_t *msg_buf)
{
	struct assoc_cnf assoc_cnf;
	tpDphHashNode sta_ds = NULL;
	struct pe_session *session_entry = NULL;
	uint8_t session_id;
	tpSirAssocReq assoc_req;

	if (!msg_buf) {
		pe_err("msg_buf is NULL");
		return;
	}

	qdf_mem_copy(&assoc_cnf, msg_buf, sizeof(assoc_cnf));
	if (!__lim_is_sme_assoc_cnf_valid(&assoc_cnf)) {
		pe_err("Received invalid SME_RE(ASSOC)_CNF message");
		goto end;
	}

	session_entry = pe_find_session_by_bssid(mac_ctx, assoc_cnf.bssid.bytes,
			&session_id);
	if (!session_entry) {
		pe_err("session does not exist for given bssId");
		goto end;
	}

	if ((!LIM_IS_AP_ROLE(session_entry)) ||
	    (session_entry->limSmeState != eLIM_SME_NORMAL_STATE)) {
		pe_err("Rcvd unexpected msg %X in state %X, in role %X",
			msg_type, session_entry->limSmeState,
			GET_LIM_SYSTEM_ROLE(session_entry));
		goto end;
	}
	sta_ds = dph_get_hash_entry(mac_ctx, assoc_cnf.aid,
			&session_entry->dph.dphHashTable);
	if (!sta_ds) {
		pe_err("Rcvd invalid msg %X due to no STA ctx, aid %d, peer",
				msg_type, assoc_cnf.aid);
		lim_print_mac_addr(mac_ctx, assoc_cnf.peer_macaddr.bytes, LOGE);

		/*
		 * send a DISASSOC_IND message to WSM to make sure
		 * the state in WSM and LIM is the same
		 */
		lim_send_sme_disassoc_ntf(mac_ctx, assoc_cnf.peer_macaddr.bytes,
				eSIR_SME_STA_NOT_ASSOCIATED,
				eLIM_PEER_ENTITY_DISASSOC, assoc_cnf.aid,
				session_entry->smeSessionId,
				session_entry);
		goto end;
	}
	if (qdf_mem_cmp((uint8_t *)sta_ds->staAddr,
				(uint8_t *) assoc_cnf.peer_macaddr.bytes,
				QDF_MAC_ADDR_SIZE)) {
		pe_debug("peerMacAddr mismatched for aid %d, peer ",
				assoc_cnf.aid);
		lim_print_mac_addr(mac_ctx, assoc_cnf.peer_macaddr.bytes, LOGD);
		goto end;
	}

	if ((sta_ds->mlmStaContext.mlmState != eLIM_MLM_WT_ASSOC_CNF_STATE) ||
		((sta_ds->mlmStaContext.subType == LIM_ASSOC) &&
		 (msg_type != eWNI_SME_ASSOC_CNF)) ||
		((sta_ds->mlmStaContext.subType == LIM_REASSOC) &&
		 (msg_type != eWNI_SME_ASSOC_CNF))) {
		pe_debug("not in MLM_WT_ASSOC_CNF_STATE, for aid %d, peer"
			"StaD mlmState: %d",
			assoc_cnf.aid, sta_ds->mlmStaContext.mlmState);
		lim_print_mac_addr(mac_ctx, assoc_cnf.peer_macaddr.bytes, LOGD);
		goto end;
	}
	/*
	 * Deactivate/delet CNF_WAIT timer since ASSOC_CNF
	 * has been received
	 */
	pe_debug("Received SME_ASSOC_CNF. Delete Timer");
	lim_deactivate_and_change_per_sta_id_timer(mac_ctx,
			eLIM_CNF_WAIT_TIMER, sta_ds->assocId);

	if (assoc_cnf.status_code == eSIR_SME_SUCCESS) {
		/*
		 * In BTAMP-AP, PE already finished the WMA_ADD_STA sequence
		 * when it had received Assoc Request frame. Now, PE just needs
		 * to send association rsp frame to the requesting BTAMP-STA.
		 */
		sta_ds->mlmStaContext.mlmState =
			eLIM_MLM_LINK_ESTABLISHED_STATE;
		sta_ds->mlmStaContext.owe_ie = assoc_cnf.owe_ie;
		sta_ds->mlmStaContext.owe_ie_len = assoc_cnf.owe_ie_len;
		pe_debug("sending Assoc Rsp frame to STA assoc id=%d, tx cb %d",
			 sta_ds->assocId, assoc_cnf.need_assoc_rsp_tx_cb);
		lim_send_assoc_rsp_mgmt_frame(
					mac_ctx, QDF_STATUS_SUCCESS,
					sta_ds->assocId, sta_ds->staAddr,
					sta_ds->mlmStaContext.subType, sta_ds,
					session_entry,
					assoc_cnf.need_assoc_rsp_tx_cb);
		sta_ds->mlmStaContext.owe_ie = NULL;
		sta_ds->mlmStaContext.owe_ie_len = 0;
		goto end;
	} else {
		uint8_t add_pre_auth_context = true;
		/*
		 * SME_ASSOC_CNF status is non-success, so STA is not allowed
		 * to be associated since the HAL sta entry is created for
		 * denied STA we need to remove this HAL entry.
		 * So to do that set updateContext to 1
		 */
		enum wlan_status_code mac_status_code =
					STATUS_UNSPECIFIED_FAILURE;

		if (!sta_ds->mlmStaContext.updateContext)
			sta_ds->mlmStaContext.updateContext = 1;
		pe_debug("Recv Assoc Cnf, status Code : %d(assoc id=%d) Reason code: %d",
			 assoc_cnf.status_code, sta_ds->assocId,
			 assoc_cnf.mac_status_code);
		if (assoc_cnf.mac_status_code)
			mac_status_code = assoc_cnf.mac_status_code;
		if (assoc_cnf.mac_status_code == STATUS_INVALID_PMKID ||
		    assoc_cnf.mac_status_code ==
			STATUS_NOT_SUPPORTED_AUTH_ALG)
			add_pre_auth_context = false;

		lim_reject_association(mac_ctx, sta_ds->staAddr,
				       sta_ds->mlmStaContext.subType,
				       add_pre_auth_context,
				       sta_ds->mlmStaContext.authType,
				       sta_ds->assocId, true,
				       mac_status_code,
				       session_entry);
	}
end:
	if (((session_entry) && (sta_ds)) &&
		(session_entry->parsedAssocReq[sta_ds->assocId]) &&
		!assoc_cnf.need_assoc_rsp_tx_cb) {
		assoc_req = (tpSirAssocReq)
			session_entry->parsedAssocReq[sta_ds->assocId];
		if (assoc_req->assocReqFrame) {
			qdf_mem_free(assoc_req->assocReqFrame);
			assoc_req->assocReqFrame = NULL;
		}
		qdf_mem_free(session_entry->parsedAssocReq[sta_ds->assocId]);
		session_entry->parsedAssocReq[sta_ds->assocId] = NULL;
	}
	qdf_mem_free(assoc_cnf.owe_ie);
}

static void
__lim_process_sme_addts_req(struct mac_context *mac, uint32_t *msg_buf)
{
	tpDphHashNode sta;
	tSirMacAddr peerMac;
	tpSirAddtsReq pSirAddts;
	uint32_t timeout;
	struct pe_session *pe_session;
	uint8_t sessionId;      /* PE sessionId */
	uint8_t smesessionId;

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	pSirAddts = (tpSirAddtsReq) msg_buf;
	smesessionId = pSirAddts->sessionId;
	pe_session = pe_find_session_by_bssid(mac, pSirAddts->bssid.bytes,
						 &sessionId);
	if (!pe_session) {
		pe_err("Session Does not exist for given bssId");
		lim_send_sme_addts_rsp(mac, pSirAddts->rspReqd,
				       QDF_STATUS_E_FAILURE,
				       NULL, pSirAddts->req.tspec,
				       smesessionId);
		return;
	}
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_diag_event_report(mac, WLAN_PE_DIAG_ADDTS_REQ_EVENT, pe_session, 0,
			      0);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	/* if sta
	 *  - verify assoc state
	 *  - send addts request to ap
	 *  - wait for addts response from ap
	 * if ap, just ignore with error log
	 */
	pe_debug("Received SME_ADDTS_REQ (TSid %d, UP %d)",
		pSirAddts->req.tspec.tsinfo.traffic.tsid,
		pSirAddts->req.tspec.tsinfo.traffic.userPrio);

	if (!LIM_IS_STA_ROLE(pe_session)) {
		pe_err("AddTs received on AP - ignoring");
		goto send_failure_addts_rsp;
	}

	sta =
		dph_get_hash_entry(mac, DPH_STA_HASH_INDEX_PEER,
				   &pe_session->dph.dphHashTable);

	if (!sta) {
		pe_err("Cannot find AP context for addts req");
		goto send_failure_addts_rsp;
	}

	if ((!sta->valid) || (sta->mlmStaContext.mlmState !=
	    eLIM_MLM_LINK_ESTABLISHED_STATE)) {
		pe_err("AddTs received in invalid MLM state");
		goto send_failure_addts_rsp;
	}

	pSirAddts->req.wsmTspecPresent = 0;
	pSirAddts->req.wmeTspecPresent = 0;
	pSirAddts->req.lleTspecPresent = 0;

	if ((sta->wsmEnabled) &&
	    (pSirAddts->req.tspec.tsinfo.traffic.accessPolicy !=
	     SIR_MAC_ACCESSPOLICY_EDCA))
		pSirAddts->req.wsmTspecPresent = 1;
	else if (sta->wmeEnabled)
		pSirAddts->req.wmeTspecPresent = 1;
	else if (sta->lleEnabled)
		pSirAddts->req.lleTspecPresent = 1;
	else {
		pe_warn("ADDTS_REQ ignore - qos is disabled");
		goto send_failure_addts_rsp;
	}

	if ((pe_session->limSmeState != eLIM_SME_ASSOCIATED_STATE) &&
	    (pe_session->limSmeState != eLIM_SME_LINK_EST_STATE)) {
		pe_err("AddTs received in invalid LIMsme state (%d)",
			pe_session->limSmeState);
		goto send_failure_addts_rsp;
	}

	if (mac->lim.gLimAddtsSent) {
		pe_err("Addts (token %d, tsid %d, up %d) is still pending",
			mac->lim.gLimAddtsReq.req.dialogToken,
			mac->lim.gLimAddtsReq.req.tspec.tsinfo.traffic.tsid,
			mac->lim.gLimAddtsReq.req.tspec.tsinfo.traffic.
			userPrio);
		goto send_failure_addts_rsp;
	}

	sir_copy_mac_addr(peerMac, pe_session->bssId);

	/* save the addts request */
	mac->lim.gLimAddtsSent = true;
	qdf_mem_copy((uint8_t *) &mac->lim.gLimAddtsReq,
		     (uint8_t *) pSirAddts, sizeof(tSirAddtsReq));

	/* ship out the message now */
	lim_send_addts_req_action_frame(mac, peerMac, &pSirAddts->req,
					pe_session);
	pe_err("Sent ADDTS request");
	/* start a timer to wait for the response */
	if (pSirAddts->timeout)
		timeout = pSirAddts->timeout;
	else
		timeout = mac->mlme_cfg->timeouts.addts_rsp_timeout;

	timeout = SYS_MS_TO_TICKS(timeout);
	if (tx_timer_change(&mac->lim.lim_timers.gLimAddtsRspTimer, timeout, 0)
	    != TX_SUCCESS) {
		pe_err("AddtsRsp timer change failed!");
		goto send_failure_addts_rsp;
	}
	mac->lim.gLimAddtsRspTimerCount++;
	if (tx_timer_change_context(&mac->lim.lim_timers.gLimAddtsRspTimer,
				    mac->lim.gLimAddtsRspTimerCount) !=
	    TX_SUCCESS) {
		pe_err("AddtsRsp timer change failed!");
		goto send_failure_addts_rsp;
	}
	MTRACE(mac_trace
		       (mac, TRACE_CODE_TIMER_ACTIVATE, pe_session->peSessionId,
		       eLIM_ADDTS_RSP_TIMER));

	/* add the sessionId to the timer object */
	mac->lim.lim_timers.gLimAddtsRspTimer.sessionId = sessionId;
	if (tx_timer_activate(&mac->lim.lim_timers.gLimAddtsRspTimer) !=
	    TX_SUCCESS) {
		pe_err("AddtsRsp timer activation failed!");
		goto send_failure_addts_rsp;
	}
	return;

send_failure_addts_rsp:
	lim_send_sme_addts_rsp(mac, pSirAddts->rspReqd, QDF_STATUS_E_FAILURE,
			       pe_session, pSirAddts->req.tspec,
			       smesessionId);
}

#ifdef WLAN_FEATURE_MSCS
static void
__lim_process_sme_mscs_req(struct mac_context *mac, uint32_t *msg_buf)
{
	struct qdf_mac_addr peer_mac;
	struct mscs_req_info *mscs_req;
	struct pe_session *pe_session;
	uint8_t pe_session_id;

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	mscs_req = (struct mscs_req_info *) msg_buf;
	pe_session = pe_find_session_by_bssid(mac, mscs_req->bssid.bytes,
					      &pe_session_id);
	if (!pe_session) {
		pe_err("Session Does not exist for bssid: " QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(mscs_req->bssid.bytes));
		return;
	}

	if (!LIM_IS_STA_ROLE(pe_session)) {
		pe_err("MSCS req received on AP - ignoring");
		return;
	}

	if (QDF_IS_STATUS_ERROR(wlan_vdev_mlme_is_active(pe_session->vdev))) {
		pe_err("mscs req in unexpected vdev SM state:%d",
		       wlan_vdev_mlme_get_state(pe_session->vdev));
		return;
	}

	if (mscs_req->is_mscs_req_sent) {
		pe_err("MSCS req already sent");
		return;
	}

	qdf_mem_copy(peer_mac.bytes, pe_session->bssId, QDF_MAC_ADDR_SIZE);

	/* save the mscs request */
	mscs_req->is_mscs_req_sent = true;

	/* ship out the message now */
	lim_send_mscs_req_action_frame(mac, peer_mac, mscs_req,
				       pe_session);
}
#else
static inline void
__lim_process_sme_mscs_req(struct mac_context *mac, uint32_t *msg_buf)
{
	return;
}

#endif

static void
__lim_process_sme_delts_req(struct mac_context *mac, uint32_t *msg_buf)
{
	tSirMacAddr peerMacAddr;
	uint8_t ac;
	struct mac_ts_info *pTsinfo;
	tpSirDeltsReq pDeltsReq = (tpSirDeltsReq) msg_buf;
	tpDphHashNode sta = NULL;
	struct pe_session *pe_session;
	uint8_t sessionId;
	uint32_t status = QDF_STATUS_SUCCESS;
	uint8_t smesessionId;

	smesessionId = pDeltsReq->sessionId;

	pe_session = pe_find_session_by_bssid(mac,
				pDeltsReq->bssid.bytes,
				&sessionId);
	if (!pe_session) {
		pe_err("Session Does not exist for given bssId");
		status = QDF_STATUS_E_FAILURE;
		goto end;
	}
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_diag_event_report(mac, WLAN_PE_DIAG_DELTS_REQ_EVENT, pe_session, 0,
			      0);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	if (QDF_STATUS_SUCCESS !=
	    lim_validate_delts_req(mac, pDeltsReq, peerMacAddr, pe_session)) {
		pe_err("lim_validate_delts_req failed");
		status = QDF_STATUS_E_FAILURE;
		lim_send_sme_delts_rsp(mac, pDeltsReq, QDF_STATUS_E_FAILURE,
				       pe_session, smesessionId);
		return;
	}

	pe_debug("Sent DELTS request to station with assocId = %d MacAddr = "
		QDF_MAC_ADDR_FMT,
		pDeltsReq->aid, QDF_MAC_ADDR_REF(peerMacAddr));

	lim_send_delts_req_action_frame(mac, peerMacAddr,
					pDeltsReq->req.wmeTspecPresent,
					&pDeltsReq->req.tsinfo,
					&pDeltsReq->req.tspec, pe_session);

	pTsinfo =
		pDeltsReq->req.wmeTspecPresent ? &pDeltsReq->req.tspec.
		tsinfo : &pDeltsReq->req.tsinfo;

	/* We've successfully send DELTS frame to AP. Update the
	 * dynamic UAPSD mask. The AC for this TSPEC to be deleted
	 * is no longer trigger enabled or delivery enabled
	 */
	lim_set_tspec_uapsd_mask_per_session(mac, pe_session,
					     pTsinfo, CLEAR_UAPSD_MASK);

	/* We're deleting the TSPEC, so this particular AC is no longer
	 * admitted.  PE needs to downgrade the EDCA
	 * parameters(for the AC for which TS is being deleted) to the
	 * next best AC for which ACM is not enabled, and send the
	 * updated values to HAL.
	 */
	ac = upToAc(pTsinfo->traffic.userPrio);

	if (pTsinfo->traffic.direction == SIR_MAC_DIRECTION_UPLINK) {
		pe_session->gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] &=
			~(1 << ac);
	} else if (pTsinfo->traffic.direction ==
		   SIR_MAC_DIRECTION_DNLINK) {
		pe_session->gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] &=
			~(1 << ac);
	} else if (pTsinfo->traffic.direction ==
		   SIR_MAC_DIRECTION_BIDIR) {
		pe_session->gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] &=
			~(1 << ac);
		pe_session->gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] &=
			~(1 << ac);
	}

	lim_set_active_edca_params(mac, pe_session->gLimEdcaParams,
				   pe_session);

	sta =
		dph_get_hash_entry(mac, DPH_STA_HASH_INDEX_PEER,
				   &pe_session->dph.dphHashTable);
	if (sta) {
		lim_send_edca_params(mac, pe_session->gLimEdcaParamsActive,
				     pe_session->vdev_id, false);
		status = QDF_STATUS_SUCCESS;
	} else {
		pe_err("Self entry missing in Hash Table");
		status = QDF_STATUS_E_FAILURE;
	}
#ifdef FEATURE_WLAN_ESE
	lim_send_sme_tsm_ie_ind(mac, pe_session, 0, 0, 0);
#endif

	/* send an sme response back */
end:
	lim_send_sme_delts_rsp(mac, pDeltsReq, QDF_STATUS_SUCCESS, pe_session,
			       smesessionId);
}

void lim_process_sme_addts_rsp_timeout(struct mac_context *mac, uint32_t param)
{
	/* fetch the pe_session based on the sessionId */
	struct pe_session *pe_session;

	pe_session = pe_find_session_by_session_id(mac,
				mac->lim.lim_timers.gLimAddtsRspTimer.
				sessionId);
	if (!pe_session) {
		pe_err("Session Does not exist for given sessionID");
		return;
	}

	if (!LIM_IS_STA_ROLE(pe_session)) {
		pe_warn("AddtsRspTimeout in non-Sta role (%d)",
			GET_LIM_SYSTEM_ROLE(pe_session));
		mac->lim.gLimAddtsSent = false;
		return;
	}

	if (!mac->lim.gLimAddtsSent) {
		pe_warn("AddtsRspTimeout but no AddtsSent");
		return;
	}

	if (param != mac->lim.gLimAddtsRspTimerCount) {
		pe_err("Invalid AddtsRsp Timer count %d (exp %d)", param,
			mac->lim.gLimAddtsRspTimerCount);
		return;
	}
	/* this a real response timeout */
	mac->lim.gLimAddtsSent = false;
	mac->lim.gLimAddtsRspTimerCount++;

	lim_send_sme_addts_rsp(mac, true, eSIR_SME_ADDTS_RSP_TIMEOUT,
			       pe_session, mac->lim.gLimAddtsReq.req.tspec,
			       pe_session->smeSessionId);
}

#ifdef FEATURE_WLAN_ESE
/**
 * __lim_process_sme_get_tsm_stats_request() - get tsm stats request
 *
 * @mac: Pointer to Global MAC structure
 * @msg_buf: A pointer to the SME message buffer
 *
 * Return: None
 */
static void __lim_process_sme_get_tsm_stats_request(struct mac_context *mac,
						    uint32_t *msg_buf)
{
	struct scheduler_msg msgQ = {0};

	msgQ.type = WMA_TSM_STATS_REQ;
	msgQ.reserved = 0;
	msgQ.bodyptr = msg_buf;
	msgQ.bodyval = 0;
	MTRACE(mac_trace_msg_tx(mac, NO_SESSION, msgQ.type));

	if (QDF_STATUS_SUCCESS != (wma_post_ctrl_msg(mac, &msgQ))) {
		qdf_mem_free(msg_buf);
		msg_buf = NULL;
		pe_err("Unable to forward request");
		return;
	}
}
#endif /* FEATURE_WLAN_ESE */

static void lim_process_sme_set_addba_accept(struct mac_context *mac_ctx,
		struct sme_addba_accept *msg)
{
	if (!msg) {
		pe_err("Msg Buffer is NULL");
		return;
	}
	if (!msg->addba_accept)
		mac_ctx->reject_addba_req = 1;
	else
		mac_ctx->reject_addba_req = 0;
}

static void lim_process_sme_update_edca_params(struct mac_context *mac_ctx,
					       uint32_t vdev_id)
{
	struct pe_session *pe_session;
	tpDphHashNode sta_ds_ptr;

	pe_session = pe_find_session_by_vdev_id(mac_ctx, vdev_id);
	if (!pe_session) {
		pe_err("Session does not exist: vdev_id %d", vdev_id);
		return;
	}
	pe_session->gLimEdcaParamsActive[QCA_WLAN_AC_BE].no_ack =
		mac_ctx->no_ack_policy_cfg[QCA_WLAN_AC_BE];
	pe_session->gLimEdcaParamsActive[QCA_WLAN_AC_BK].no_ack =
		mac_ctx->no_ack_policy_cfg[QCA_WLAN_AC_BK];
	pe_session->gLimEdcaParamsActive[QCA_WLAN_AC_VI].no_ack =
		mac_ctx->no_ack_policy_cfg[QCA_WLAN_AC_VI];
	pe_session->gLimEdcaParamsActive[QCA_WLAN_AC_VO].no_ack =
		mac_ctx->no_ack_policy_cfg[QCA_WLAN_AC_VO];
	sta_ds_ptr = dph_get_hash_entry(mac_ctx, DPH_STA_HASH_INDEX_PEER,
					&pe_session->dph.dphHashTable);
	if (sta_ds_ptr)
		lim_send_edca_params(mac_ctx,
				     pe_session->gLimEdcaParamsActive,
				     pe_session->vdev_id, false);
	else
		pe_err("Self entry missing in Hash Table");
}

/**
 * lim_process_sme_update_session_edca_txq_params()
 * Update the edca tx queue parameters for the vdev
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: Pointer to SME message buffer
 *
 * Return: None
 */
static void
lim_process_sme_update_session_edca_txq_params(struct mac_context *mac_ctx,
					       uint32_t *msg_buf)
{
	struct sir_update_session_txq_edca_param *msg;
	struct pe_session *pe_session;
	uint8_t ac;

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	msg = (struct sir_update_session_txq_edca_param *)msg_buf;

	pe_session = pe_find_session_by_vdev_id(mac_ctx, msg->vdev_id);
	if (!pe_session) {
		pe_warn("Session does not exist for given vdev_id %d",
			msg->vdev_id);
		return;
	}

	ac = msg->txq_edca_params.aci.aci;
	pe_debug("received SME Session tx queue update for vdev %d queue %d",
		 msg->vdev_id, ac);

	if ((!LIM_IS_AP_ROLE(pe_session)) ||
	    (pe_session->limSmeState != eLIM_SME_NORMAL_STATE)) {
		pe_err("Rcvd edca update req in state %X, in role %X",
		       pe_session->limSmeState,
		       GET_LIM_SYSTEM_ROLE(pe_session));
		return;
	}

	pe_session->gLimEdcaParams[ac].cw.min =
			msg->txq_edca_params.cw.min;
	pe_session->gLimEdcaParams[ac].cw.max =
			msg->txq_edca_params.cw.max;
	pe_session->gLimEdcaParams[ac].aci.aci =
			msg->txq_edca_params.aci.aci;
	pe_session->gLimEdcaParams[ac].aci.aifsn =
			msg->txq_edca_params.aci.aifsn;
	pe_session->gLimEdcaParams[ac].txoplimit =
			msg->txq_edca_params.txoplimit;

	lim_send_edca_params(mac_ctx,
			     pe_session->gLimEdcaParams,
			     pe_session->vdev_id, false);
}

static void lim_process_sme_update_mu_edca_params(struct mac_context *mac_ctx,
						  uint32_t vdev_id)
{
	struct pe_session *pe_session;
	tpDphHashNode sta_ds_ptr;

	pe_session = pe_find_session_by_vdev_id(mac_ctx, vdev_id);
	if (!pe_session) {
		pe_err("Session does not exist: vdev_id %d", vdev_id);
		return;
	}
	sta_ds_ptr = dph_get_hash_entry(mac_ctx, DPH_STA_HASH_INDEX_PEER,
					&pe_session->dph.dphHashTable);
	if (sta_ds_ptr)
		lim_send_edca_params(mac_ctx, mac_ctx->usr_mu_edca_params,
				     pe_session->vdev_id, true);
	else
		pe_err("Self entry missing in Hash Table");
}

static void
lim_process_sme_cfg_action_frm_in_tb_ppdu(struct mac_context *mac_ctx,
					  struct  sir_cfg_action_frm_tb_ppdu
					  *msg)
{
	if (!msg) {
		pe_err("Buffer is NULL");
		return;
	}

	lim_send_action_frm_tb_ppdu_cfg(mac_ctx, msg->vdev_id, msg->cfg);
}

static void lim_process_sme_update_config(struct mac_context *mac_ctx,
					  struct update_config *msg)
{
	struct pe_session *pe_session;

	pe_debug("received eWNI_SME_UPDATE_HT_CONFIG message");
	if (!msg) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	pe_session = pe_find_session_by_vdev_id(mac_ctx, msg->vdev_id);
	if (!pe_session) {
		pe_warn("Session does not exist for given BSSID");
		return;
	}

	switch (msg->capab) {
	case WNI_CFG_HT_CAP_INFO_ADVANCE_CODING:
		pe_session->ht_config.ht_rx_ldpc = msg->value;
		break;
	case WNI_CFG_HT_CAP_INFO_TX_STBC:
		pe_session->ht_config.ht_tx_stbc = msg->value;
		break;
	case WNI_CFG_HT_CAP_INFO_RX_STBC:
		pe_session->ht_config.ht_rx_stbc = msg->value;
		break;
	case WNI_CFG_HT_CAP_INFO_SHORT_GI_20MHZ:
		pe_session->ht_config.ht_sgi20 = msg->value;
		break;
	case WNI_CFG_HT_CAP_INFO_SHORT_GI_40MHZ:
		pe_session->ht_config.ht_sgi40 = msg->value;
		break;
	}

	if (LIM_IS_AP_ROLE(pe_session)) {
		sch_set_fixed_beacon_fields(mac_ctx, pe_session);
		lim_send_beacon_ind(mac_ctx, pe_session, REASON_CONFIG_UPDATE);
	}
}

void
lim_send_vdev_restart(struct mac_context *mac,
		      struct pe_session *pe_session, uint8_t sessionId)
{
	struct vdev_mlme_obj *mlme_obj;

	if (!pe_session) {
		pe_err("Invalid parameters");
		return;
	}

	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(pe_session->vdev);
	if (!mlme_obj) {
		pe_err("vdev component object is NULL");
		return;
	}

	mlme_obj->mgmt.ap.hidden_ssid = pe_session->ssidHidden ? true : false;

	vdev_mgr_start_send(mlme_obj,  true);
}

#if defined(WLAN_FEATURE_HOST_ROAM) || defined(WLAN_FEATURE_ROAM_OFFLOAD)
/**
 * lim_send_roam_set_pcl() - Process Roam offload flag from csr
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: Pointer to SME message buffer
 *
 * Return: None
 */
static void lim_send_roam_set_pcl(struct mac_context *mac_ctx,
				  struct set_pcl_req *msg_buf)
{
	struct scheduler_msg wma_msg = {0};
	QDF_STATUS status;

	wma_msg.type = SIR_HAL_SET_PCL_TO_FW;
	wma_msg.bodyptr = msg_buf;

	status = wma_post_ctrl_msg(mac_ctx, &wma_msg);
	if (QDF_STATUS_SUCCESS != status) {
		pe_err("Posting WMA_ROAM_SET_PCL failed");
		qdf_mem_free(msg_buf);
	}
}
#else
static inline void lim_send_roam_set_pcl(struct mac_context *mac_ctx,
					 struct set_pcl_req *msg_buf)
{
	qdf_mem_free(msg_buf);
}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * lim_process_roam_invoke() - process the Roam Invoke req
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: Pointer to the SME message buffer
 *
 * This function is called by limProcessMessageQueue(). This function sends the
 * ROAM_INVOKE command to WMA.
 *
 * Return: None
 */
static void lim_process_roam_invoke(struct mac_context *mac_ctx,
				    uint32_t *msg_buf)
{
	struct scheduler_msg msg = {0};
	QDF_STATUS status;

	msg.type = SIR_HAL_ROAM_INVOKE;
	msg.bodyptr = msg_buf;
	msg.reserved = 0;

	status = wma_post_ctrl_msg(mac_ctx, &msg);
	if (QDF_STATUS_SUCCESS != status)
		pe_err("Not able to post SIR_HAL_ROAM_INVOKE to WMA");
}
#else
static void lim_process_roam_invoke(struct mac_context *mac_ctx,
				    uint32_t *msg_buf)
{
	qdf_mem_free(msg_buf);
}
#endif

static void lim_handle_update_ssid_hidden(struct mac_context *mac_ctx,
				struct pe_session *session, uint8_t ssid_hidden)
{
	pe_debug("rcvd HIDE_SSID message old HIDE_SSID: %d new HIDE_SSID: %d",
			session->ssidHidden, ssid_hidden);

	if (ssid_hidden != session->ssidHidden) {
		session->ssidHidden = ssid_hidden;
	} else {
		pe_debug("Dont process HIDE_SSID msg with existing setting");
		return;
	}

	ap_mlme_set_hidden_ssid_restart_in_progress(session->vdev, true);
	wlan_vdev_mlme_sm_deliver_evt(session->vdev,
				      WLAN_VDEV_SM_EV_FW_VDEV_RESTART,
				      sizeof(*session), session);
}

/**
 * __lim_process_sme_session_update - process SME session update msg
 *
 * @mac_ctx: Pointer to global mac context
 * @msg_buf: Pointer to the received message buffer
 *
 * Return: None
 */
static void __lim_process_sme_session_update(struct mac_context *mac_ctx,
						uint32_t *msg_buf)
{
	struct sir_update_session_param *msg;
	struct pe_session *session;

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	msg = (struct sir_update_session_param *) msg_buf;

	session = pe_find_session_by_vdev_id(mac_ctx, msg->vdev_id);
	if (!session) {
		pe_warn("Session does not exist for given vdev_id %d",
			msg->vdev_id);
		return;
	}

	pe_debug("received SME Session update for %d val %d",
			msg->param_type, msg->param_val);
	switch (msg->param_type) {
	case SIR_PARAM_SSID_HIDDEN:
		lim_handle_update_ssid_hidden(mac_ctx, session, msg->param_val);
		break;
	default:
		pe_err("Unknown session param");
		break;
	}
}

/*
   Update the beacon Interval dynamically if beaconInterval is different in MCC
 */
static void __lim_process_sme_change_bi(struct mac_context *mac,
					uint32_t *msg_buf)
{
	struct wlan_change_bi *pChangeBIParams;
	struct pe_session *pe_session;
	uint8_t sessionId = 0;
	tUpdateBeaconParams beaconParams;

	pe_debug("received Update Beacon Interval message");

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	qdf_mem_zero(&beaconParams, sizeof(tUpdateBeaconParams));
	pChangeBIParams = (struct wlan_change_bi *)msg_buf;

	pe_session = pe_find_session_by_bssid(mac,
				pChangeBIParams->bssid.bytes,
				&sessionId);
	if (!pe_session) {
		pe_err("Session does not exist for given BSSID");
		return;
	}

	/*Update pe_session Beacon Interval */
	if (pe_session->beaconParams.beaconInterval !=
	    pChangeBIParams->beacon_interval) {
		pe_session->beaconParams.beaconInterval =
			pChangeBIParams->beacon_interval;
	}

	/*Update sch beaconInterval */
	if (mac->sch.beacon_interval !=
	    pChangeBIParams->beacon_interval) {
		mac->sch.beacon_interval =
			pChangeBIParams->beacon_interval;

		pe_debug("LIM send update BeaconInterval Indication: %d",
			pChangeBIParams->beacon_interval);

		if (false == mac->sap.SapDfsInfo.is_dfs_cac_timer_running) {
			/* Update beacon */
			sch_set_fixed_beacon_fields(mac, pe_session);

			beaconParams.bss_idx = pe_session->vdev_id;
			/* Set change in beacon Interval */
			beaconParams.beaconInterval =
				pChangeBIParams->beacon_interval;
			beaconParams.paramChangeBitmap =
				PARAM_BCN_INTERVAL_CHANGED;
			lim_send_beacon_params(mac, &beaconParams, pe_session);
		}
	}

	return;
} /*** end __lim_process_sme_change_bi() ***/

#ifdef QCA_HT_2040_COEX
static void __lim_process_sme_set_ht2040_mode(struct mac_context *mac,
					      uint32_t *msg_buf)
{
	struct set_ht2040_mode *pSetHT2040Mode;
	struct pe_session *pe_session;
	uint8_t sessionId = 0;
	struct scheduler_msg msg = {0};
	tUpdateVHTOpMode *pHtOpMode = NULL;
	uint16_t staId = 0;
	tpDphHashNode sta = NULL;

	pe_debug("received Set HT 20/40 mode message");
	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	pSetHT2040Mode = (struct set_ht2040_mode *)msg_buf;

	pe_session = pe_find_session_by_bssid(mac,
				pSetHT2040Mode->bssid.bytes,
				&sessionId);
	if (!pe_session) {
		pe_debug("Session does not exist for given BSSID");
		lim_print_mac_addr(mac, pSetHT2040Mode->bssid.bytes, LOGD);
		return;
	}

	pe_debug("Update session entry for cbMod=%d",
		pSetHT2040Mode->cbMode);
	/*Update pe_session HT related fields */
	switch (pSetHT2040Mode->cbMode) {
	case PHY_SINGLE_CHANNEL_CENTERED:
		pe_session->htSecondaryChannelOffset =
			PHY_SINGLE_CHANNEL_CENTERED;
		pe_session->htRecommendedTxWidthSet = 0;
		if (pSetHT2040Mode->obssEnabled)
			pe_session->htSupportedChannelWidthSet
					= eHT_CHANNEL_WIDTH_40MHZ;
		else
			pe_session->htSupportedChannelWidthSet
					= eHT_CHANNEL_WIDTH_20MHZ;
		break;
	case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
		pe_session->htSecondaryChannelOffset =
			PHY_DOUBLE_CHANNEL_LOW_PRIMARY;
		pe_session->htRecommendedTxWidthSet = 1;
		break;
	case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
		pe_session->htSecondaryChannelOffset =
			PHY_DOUBLE_CHANNEL_HIGH_PRIMARY;
		pe_session->htRecommendedTxWidthSet = 1;
		break;
	default:
		pe_err("Invalid cbMode");
		return;
	}

	/* Update beacon */
	sch_set_fixed_beacon_fields(mac, pe_session);
	lim_send_beacon_ind(mac, pe_session, REASON_SET_HT2040);

	/* update OP Mode for each associated peer */
	for (staId = 0; staId < pe_session->dph.dphHashTable.size; staId++) {
		sta = dph_get_hash_entry(mac, staId,
				&pe_session->dph.dphHashTable);
		if (!sta)
			continue;

		if (sta->valid && sta->htSupportedChannelWidthSet) {
			pHtOpMode = qdf_mem_malloc(sizeof(tUpdateVHTOpMode));
			if (!pHtOpMode)
				return;
			pHtOpMode->opMode =
				(pe_session->htSecondaryChannelOffset ==
				 PHY_SINGLE_CHANNEL_CENTERED) ?
				eHT_CHANNEL_WIDTH_20MHZ : eHT_CHANNEL_WIDTH_40MHZ;
			qdf_mem_copy(pHtOpMode->peer_mac, &sta->staAddr,
				     sizeof(tSirMacAddr));
			pHtOpMode->smesessionId = sessionId;

			msg.type = WMA_UPDATE_OP_MODE;
			msg.reserved = 0;
			msg.bodyptr = pHtOpMode;
			if (!QDF_IS_STATUS_SUCCESS
				    (scheduler_post_message(QDF_MODULE_ID_PE,
							    QDF_MODULE_ID_WMA,
							    QDF_MODULE_ID_WMA,
							    &msg))) {
				pe_err("Not able to post WMA_UPDATE_OP_MODE message to WMA");
				qdf_mem_free(pHtOpMode);
				return;
			}
			pe_debug("Notified FW about OP mode: %d",
				pHtOpMode->opMode);

		} else
			pe_debug("station does not support HT40");
	}

	return;
}
#endif

/* -------------------------------------------------------------------- */
/**
 * __lim_process_report_message
 *
 * FUNCTION:  Processes the next received Radio Resource Management message
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param None
 * @return None
 */

static void __lim_process_report_message(struct mac_context *mac,
					 struct scheduler_msg *pMsg)
{
	switch (pMsg->type) {
	case eWNI_SME_NEIGHBOR_REPORT_REQ_IND:
		rrm_process_neighbor_report_req(mac, pMsg->bodyptr);
		break;
	case eWNI_SME_BEACON_REPORT_RESP_XMIT_IND:
		rrm_process_beacon_report_xmit(mac, pMsg->bodyptr);
		break;
	default:
		pe_err("Invalid msg type: %d", pMsg->type);
	}
}

/* -------------------------------------------------------------------- */
/**
 * lim_send_set_max_tx_power_req
 *
 * FUNCTION:  Send SIR_HAL_SET_MAX_TX_POWER_REQ message to change the max tx power.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param txPower txPower to be set.
 * @param pe_session session entry.
 * @return None
 */
QDF_STATUS
lim_send_set_max_tx_power_req(struct mac_context *mac, int8_t txPower,
			      struct pe_session *pe_session)
{
	tpMaxTxPowerParams pMaxTxParams = NULL;
	QDF_STATUS retCode = QDF_STATUS_SUCCESS;
	struct scheduler_msg msgQ = {0};

	if (!pe_session) {
		pe_err("Invalid parameters");
		return QDF_STATUS_E_FAILURE;
	}

	pMaxTxParams = qdf_mem_malloc(sizeof(tMaxTxPowerParams));
	if (!pMaxTxParams)
		return QDF_STATUS_E_NOMEM;
	pMaxTxParams->power = txPower;
	qdf_mem_copy(pMaxTxParams->bssId.bytes, pe_session->bssId,
		     QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(pMaxTxParams->selfStaMacAddr.bytes,
			pe_session->self_mac_addr,
			QDF_MAC_ADDR_SIZE);

	msgQ.type = WMA_SET_MAX_TX_POWER_REQ;
	msgQ.bodyptr = pMaxTxParams;
	msgQ.bodyval = 0;
	pe_debug("Post WMA_SET_MAX_TX_POWER_REQ to WMA");
	MTRACE(mac_trace_msg_tx(mac, pe_session->peSessionId, msgQ.type));
	retCode = wma_post_ctrl_msg(mac, &msgQ);
	if (QDF_STATUS_SUCCESS != retCode) {
		pe_err("wma_post_ctrl_msg() failed");
		qdf_mem_free(pMaxTxParams);
	}
	return retCode;
}

/**
 * __lim_process_sme_register_mgmt_frame_req() - process sme reg mgmt frame req
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: pointer to the SME message buffer
 *
 * This function is called to process eWNI_SME_REGISTER_MGMT_FRAME_REQ message
 * from SME. It Register this information within PE.
 *
 * Return: None
 */
static void __lim_process_sme_register_mgmt_frame_req(struct mac_context *mac_ctx,
		uint32_t *msg_buf)
{
	QDF_STATUS qdf_status;
	struct register_mgmt_frame *sme_req =
					(struct register_mgmt_frame *)msg_buf;
	struct mgmt_frm_reg_info *lim_mgmt_regn = NULL;
	struct mgmt_frm_reg_info *next = NULL;
	bool match = false;

	pe_nofl_debug("Register Frame: register %d, type %d, match length %d",
		      sme_req->registerFrame, sme_req->frameType,
		      sme_req->matchLen);
	/* First check whether entry exists already */
	qdf_mutex_acquire(&mac_ctx->lim.lim_frame_register_lock);
	qdf_list_peek_front(&mac_ctx->lim.gLimMgmtFrameRegistratinQueue,
			    (qdf_list_node_t **) &lim_mgmt_regn);
	qdf_mutex_release(&mac_ctx->lim.lim_frame_register_lock);

	while (lim_mgmt_regn) {
		if (lim_mgmt_regn->frameType != sme_req->frameType)
			goto skip_match;
		if (sme_req->matchLen) {
			if ((lim_mgmt_regn->matchLen == sme_req->matchLen) &&
				(!qdf_mem_cmp(lim_mgmt_regn->matchData,
					sme_req->matchData,
					lim_mgmt_regn->matchLen))) {
					/* found match! */
					match = true;
					break;
			}
		} else {
			/* found match! */
			match = true;
			break;
		}
skip_match:
		qdf_mutex_acquire(&mac_ctx->lim.lim_frame_register_lock);
		qdf_status = qdf_list_peek_next(
				&mac_ctx->lim.gLimMgmtFrameRegistratinQueue,
				(qdf_list_node_t *)lim_mgmt_regn,
				(qdf_list_node_t **)&next);
		qdf_mutex_release(&mac_ctx->lim.lim_frame_register_lock);
		lim_mgmt_regn = next;
		next = NULL;
	}
	if (match) {
		qdf_mutex_acquire(&mac_ctx->lim.lim_frame_register_lock);
		if (QDF_STATUS_SUCCESS ==
				qdf_list_remove_node(
				&mac_ctx->lim.gLimMgmtFrameRegistratinQueue,
				(qdf_list_node_t *)lim_mgmt_regn))
			qdf_mem_free(lim_mgmt_regn);
		qdf_mutex_release(&mac_ctx->lim.lim_frame_register_lock);
	}

	if (sme_req->registerFrame) {
		lim_mgmt_regn =
			qdf_mem_malloc(sizeof(struct mgmt_frm_reg_info) +
					sme_req->matchLen);
		if (lim_mgmt_regn) {
			lim_mgmt_regn->frameType = sme_req->frameType;
			lim_mgmt_regn->matchLen = sme_req->matchLen;
			lim_mgmt_regn->sessionId = sme_req->sessionId;
			if (sme_req->matchLen) {
				qdf_mem_copy(lim_mgmt_regn->matchData,
					     sme_req->matchData,
					     sme_req->matchLen);
			}
			qdf_mutex_acquire(
					&mac_ctx->lim.lim_frame_register_lock);
			qdf_list_insert_front(&mac_ctx->lim.
					      gLimMgmtFrameRegistratinQueue,
					      &lim_mgmt_regn->node);
			qdf_mutex_release(
					&mac_ctx->lim.lim_frame_register_lock);
		}
	}
	return;
}

static void __lim_process_sme_reset_ap_caps_change(struct mac_context *mac,
						   uint32_t *msg_buf)
{
	tpSirResetAPCapsChange pResetCapsChange;
	struct pe_session *pe_session;
	uint8_t sessionId = 0;

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	pResetCapsChange = (tpSirResetAPCapsChange)msg_buf;
	pe_session =
		pe_find_session_by_bssid(mac, pResetCapsChange->bssId.bytes,
					 &sessionId);
	if (!pe_session) {
		pe_err("Session does not exist for given BSSID");
		return;
	}

	pe_session->limSentCapsChangeNtf = false;
	return;
}

/**
 * lim_register_mgmt_frame_ind_cb() - Save the Management frame
 * indication callback in PE.
 * @mac_ptr: Mac pointer
 * @msg_buf: Msg pointer containing the callback
 *
 * This function is used save the Management frame
 * indication callback in PE.
 *
 * Return: None
 */
static void lim_register_mgmt_frame_ind_cb(struct mac_context *mac_ctx,
							uint32_t *msg_buf)
{
	struct sir_sme_mgmt_frame_cb_req *sme_req =
		(struct sir_sme_mgmt_frame_cb_req *)msg_buf;

	if (!msg_buf) {
		pe_err("msg_buf is null");
		return;
	}
	if (sme_req->callback)
		mac_ctx->mgmt_frame_ind_cb =
			(sir_mgmt_frame_ind_callback)sme_req->callback;
	else
		pe_err("sme_req->callback is null");
}

/**
 *__lim_process_send_disassoc_frame() - processes send disassoc frame request
 * @mac_ctx: pointer to mac context
 * @msg_buf: request message buffer
 *
 * Process a request from SME to send a disassoc frame
 *
 * Return: none
 */
static void __lim_process_send_disassoc_frame(struct mac_context *mac_ctx,
					      void *msg_buf)
{
	struct sme_send_disassoc_frm_req *req = msg_buf;
	struct pe_session *session_entry;

	if (!req) {
		pe_err("NULL req");
		return;
	}

	if ((IEEE80211_IS_MULTICAST(req->peer_mac) &&
	     !QDF_IS_ADDR_BROADCAST(req->peer_mac))) {
		pe_err("received invalid SME_DISASSOC_REQ message");
		return;
	}

	session_entry = pe_find_session_by_vdev_id(mac_ctx, req->vdev_id);
	if (!session_entry) {
		pe_err("session does not exist for given bssId "
		       QDF_MAC_ADDR_FMT, QDF_MAC_ADDR_REF(req->peer_mac));
		return;
	}

	pe_debug("msg_type %d len %d vdev_id %d mac: " QDF_MAC_ADDR_FMT " reason %d wait_for_ack %d",
		 req->msg_type, req->length,  req->vdev_id,
		 QDF_MAC_ADDR_REF(req->peer_mac), req->reason, req->wait_for_ack);

	lim_send_disassoc_mgmt_frame(mac_ctx, req->reason, req->peer_mac,
				     session_entry, req->wait_for_ack);
}

/**
 * lim_set_pdev_ht_ie() - sends the set HT IE req to FW
 * @mac_ctx: Pointer to Global MAC structure
 * @pdev_id: pdev id to set the IE.
 * @nss: Nss values to prepare the HT IE.
 *
 * Prepares the HT IE with self capabilities for different
 * Nss values and sends the set HT IE req to FW.
 *
 * Return: None
 */
static void lim_set_pdev_ht_ie(struct mac_context *mac_ctx, uint8_t pdev_id,
		uint8_t nss)
{
	struct set_ie_param *ie_params;
	struct scheduler_msg msg = {0};
	QDF_STATUS rc = QDF_STATUS_SUCCESS;
	const uint8_t *p_ie = NULL;
	tHtCaps *p_ht_cap;
	int i;

	for (i = 1; i <= nss; i++) {
		ie_params = qdf_mem_malloc(sizeof(*ie_params));
		if (!ie_params)
			return;
		ie_params->nss = i;
		ie_params->pdev_id = pdev_id;
		ie_params->ie_type = DOT11_HT_IE;
		/* 2 for IE len and EID */
		ie_params->ie_len = 2 + sizeof(tHtCaps);
		ie_params->ie_ptr = qdf_mem_malloc(ie_params->ie_len);
		if (!ie_params->ie_ptr) {
			qdf_mem_free(ie_params);
			return;
		}
		*ie_params->ie_ptr = WLAN_ELEMID_HTCAP_ANA;
		*(ie_params->ie_ptr + 1) = ie_params->ie_len - 2;
		lim_set_ht_caps(mac_ctx, NULL, ie_params->ie_ptr,
				ie_params->ie_len);

		if (NSS_1x1_MODE == i) {
			p_ie = wlan_get_ie_ptr_from_eid(DOT11F_EID_HTCAPS,
					ie_params->ie_ptr, ie_params->ie_len);
			if (!p_ie) {
				qdf_mem_free(ie_params->ie_ptr);
				qdf_mem_free(ie_params);
				pe_err("failed to get IE ptr");
				return;
			}
			p_ht_cap = (tHtCaps *)&p_ie[2];
			p_ht_cap->supportedMCSSet[1] = 0;
			p_ht_cap->txSTBC = 0;
		}

		msg.type = WMA_SET_PDEV_IE_REQ;
		msg.bodyptr = ie_params;
		msg.bodyval = 0;

		rc = wma_post_ctrl_msg(mac_ctx, &msg);
		if (rc != QDF_STATUS_SUCCESS) {
			pe_err("wma_post_ctrl_msg() return failure");
			qdf_mem_free(ie_params->ie_ptr);
			qdf_mem_free(ie_params);
			return;
		}
	}
}

/**
 * lim_set_pdev_vht_ie() - sends the set VHT IE to req FW
 * @mac_ctx: Pointer to Global MAC structure
 * @pdev_id: pdev id to set the IE.
 * @nss: Nss values to prepare the VHT IE.
 *
 * Prepares the VHT IE with self capabilities for different
 * Nss values and sends the set VHT IE req to FW.
 *
 * Return: None
 */
static void lim_set_pdev_vht_ie(struct mac_context *mac_ctx, uint8_t pdev_id,
		uint8_t nss)
{
	struct set_ie_param *ie_params;
	struct scheduler_msg msg = {0};
	QDF_STATUS rc = QDF_STATUS_SUCCESS;
	const uint8_t *p_ie = NULL;
	tSirMacVHTCapabilityInfo *vht_cap;
	int i;
	tSirVhtMcsInfo *vht_mcs;

	for (i = 1; i <= nss; i++) {
		ie_params = qdf_mem_malloc(sizeof(*ie_params));
		if (!ie_params)
			return;
		ie_params->nss = i;
		ie_params->pdev_id = pdev_id;
		ie_params->ie_type = DOT11_VHT_IE;
		/* 2 for IE len and EID */
		ie_params->ie_len = 2 + sizeof(tSirMacVHTCapabilityInfo) +
			sizeof(tSirVhtMcsInfo);
		ie_params->ie_ptr = qdf_mem_malloc(ie_params->ie_len);
		if (!ie_params->ie_ptr) {
			qdf_mem_free(ie_params);
			return;
		}
		*ie_params->ie_ptr = WLAN_ELEMID_VHTCAP;
		*(ie_params->ie_ptr + 1) = ie_params->ie_len - 2;
		lim_set_vht_caps(mac_ctx, NULL, ie_params->ie_ptr,
				ie_params->ie_len);

		if (NSS_1x1_MODE == i) {
			p_ie = wlan_get_ie_ptr_from_eid(DOT11F_EID_VHTCAPS,
					ie_params->ie_ptr, ie_params->ie_len);
			if (!p_ie) {
				qdf_mem_free(ie_params->ie_ptr);
				qdf_mem_free(ie_params);
				pe_err("failed to get IE ptr");
				return;
			}
			vht_cap = (tSirMacVHTCapabilityInfo *)&p_ie[2];
			vht_cap->txSTBC = 0;
			vht_mcs =
				(tSirVhtMcsInfo *)&p_ie[2 +
				sizeof(tSirMacVHTCapabilityInfo)];
			vht_mcs->rxMcsMap |= DISABLE_NSS2_MCS;
			vht_mcs->rxHighest =
				VHT_RX_HIGHEST_SUPPORTED_DATA_RATE_1_1;
			vht_mcs->txMcsMap |= DISABLE_NSS2_MCS;
			vht_mcs->txHighest =
				VHT_TX_HIGHEST_SUPPORTED_DATA_RATE_1_1;
		}
		msg.type = WMA_SET_PDEV_IE_REQ;
		msg.bodyptr = ie_params;
		msg.bodyval = 0;

		rc = wma_post_ctrl_msg(mac_ctx, &msg);
		if (rc != QDF_STATUS_SUCCESS) {
			pe_err("wma_post_ctrl_msg failure");
			qdf_mem_free(ie_params->ie_ptr);
			qdf_mem_free(ie_params);
			return;
		}
	}
}

/**
 * lim_process_set_vdev_ies_per_band() - process the set vdev IE req
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: Pointer to the SME message buffer
 *
 * This function is called by limProcessMessageQueue(). This function sets the
 * VDEV IEs to the FW.
 *
 * Return: None
 */
static void lim_process_set_vdev_ies_per_band(struct mac_context *mac_ctx,
					      uint32_t *msg_buf)
{
	struct sir_set_vdev_ies_per_band *p_msg =
				(struct sir_set_vdev_ies_per_band *)msg_buf;

	if (!p_msg) {
		pe_err("NULL p_msg");
		return;
	}

	pe_debug("rcvd set vdev ie per band req vdev_id = %d",
		p_msg->vdev_id);
	/* intentionally using NULL here so that self capabilty are sent */
	if (lim_send_ies_per_band(mac_ctx, NULL, p_msg->vdev_id,
				  p_msg->dot11_mode, p_msg->device_mode) !=
	    QDF_STATUS_SUCCESS)
		pe_err("Unable to send HT/VHT Cap to FW");
}

/**
 * lim_process_set_pdev_IEs() - process the set pdev IE req
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: Pointer to the SME message buffer
 *
 * This function is called by limProcessMessageQueue(). This
 * function sets the PDEV IEs to the FW.
 *
 * Return: None
 */
static void lim_process_set_pdev_IEs(struct mac_context *mac_ctx, uint32_t *msg_buf)
{
	struct sir_set_ht_vht_cfg *ht_vht_cfg;

	ht_vht_cfg = (struct sir_set_ht_vht_cfg *)msg_buf;

	if (!ht_vht_cfg) {
		pe_err("NULL ht_vht_cfg");
		return;
	}

	pe_debug("rcvd set pdev ht vht ie req with nss = %d",
			ht_vht_cfg->nss);
	lim_set_pdev_ht_ie(mac_ctx, ht_vht_cfg->pdev_id, ht_vht_cfg->nss);

	if (IS_DOT11_MODE_VHT(ht_vht_cfg->dot11mode))
		lim_set_pdev_vht_ie(mac_ctx, ht_vht_cfg->pdev_id,
				ht_vht_cfg->nss);
}

/**
 * lim_process_sme_update_access_policy_vendor_ie: function updates vendor IE
 *
 * access policy
 * @mac_ctx: pointer to mac context
 * @msg: message buffer
 *
 * function processes vendor IE and access policy from SME and updates PE
 *
 * session entry
 *
 * return: none
*/
static void lim_process_sme_update_access_policy_vendor_ie(
						struct mac_context *mac_ctx,
						uint32_t *msg)
{
	struct sme_update_access_policy_vendor_ie *update_vendor_ie;
	struct pe_session *pe_session_entry;
	uint16_t num_bytes;

	if (!msg) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}
	update_vendor_ie = (struct sme_update_access_policy_vendor_ie *) msg;
	pe_session_entry = pe_find_session_by_vdev_id(mac_ctx,
					update_vendor_ie->vdev_id);

	if (!pe_session_entry) {
		pe_err("Session does not exist for given vdev_id %d",
			update_vendor_ie->vdev_id);
		return;
	}
	if (pe_session_entry->access_policy_vendor_ie)
		qdf_mem_free(pe_session_entry->access_policy_vendor_ie);

	num_bytes = update_vendor_ie->ie[1] + 2;
	pe_session_entry->access_policy_vendor_ie = qdf_mem_malloc(num_bytes);
	if (!pe_session_entry->access_policy_vendor_ie)
		return;
	qdf_mem_copy(pe_session_entry->access_policy_vendor_ie,
		&update_vendor_ie->ie[0], num_bytes);

	pe_session_entry->access_policy = update_vendor_ie->access_policy;
}

QDF_STATUS lim_sta_mlme_vdev_disconnect_bss(struct vdev_mlme_obj *vdev_mlme,
					    uint16_t data_len, void *data)
{
	struct mac_context *mac_ctx;
	struct scheduler_msg *msg = (struct scheduler_msg *)data;

	mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac_ctx) {
		pe_err("mac_ctx is NULL");
		if (data)
			qdf_mem_free(data);
		return QDF_STATUS_E_INVAL;
	}
	pe_debug("VDEV Manager disconnect bss callback type:(%d)", msg->type);

	switch (msg->type) {
	case eWNI_SME_DEAUTH_REQ:
		__lim_process_sme_deauth_req(mac_ctx,
					     (uint32_t *)msg->bodyptr);
		break;
	case eWNI_SME_DISASSOC_CNF:
	case eWNI_SME_DEAUTH_CNF:
		__lim_process_sme_disassoc_cnf(mac_ctx,
					       (uint32_t *)msg->bodyptr);
		break;
	case eWNI_SME_DISASSOC_REQ:
		__lim_process_sme_disassoc_req(mac_ctx,
					       (uint32_t *)msg->bodyptr);
		break;
	default:
		pe_debug("Wrong message type received %d", msg->type);
	}
	return QDF_STATUS_SUCCESS;
}

static void lim_process_disconnect_sta(struct pe_session *session,
				       struct scheduler_msg *msg)
{
	if (QDF_IS_STATUS_SUCCESS(
	    wlan_vdev_is_restart_progress(session->vdev)))
		wlan_vdev_mlme_sm_deliver_evt(session->vdev,
					      WLAN_VDEV_SM_EV_RESTART_REQ_FAIL,
					      sizeof(*msg), msg);
	else
		wlan_vdev_mlme_sm_deliver_evt(session->vdev,
					      WLAN_VDEV_SM_EV_DOWN,
					      sizeof(*msg), msg);
}

static
struct pe_session *lim_get_disconnect_session(struct mac_context *mac_ctx,
					      uint8_t *bssid,
					      uint8_t vdev_id)
{
	struct pe_session *session;
	uint8_t session_id;

	/* Try to find pe session with bssid */
	session = pe_find_session_by_bssid(mac_ctx, bssid,
					   &session_id);

	/*
	 * If bssid search fail try to find by vdev id, this can happen if
	 * Roaming change the BSSID during disconnect was getting processed.
	 */
	if (!session) {
		session = pe_find_session_by_vdev_id(mac_ctx, vdev_id);
		if (session)
			pe_info("Session found for vdev_id : %d session id : %d Requested bssid : " QDF_MAC_ADDR_FMT " Actual bssid : " QDF_MAC_ADDR_FMT " sme state %d mlm state %d",
				vdev_id, session->peSessionId,
				QDF_MAC_ADDR_REF(bssid),
				QDF_MAC_ADDR_REF(session->bssId),
				session->limSmeState,
				session->limMlmState);
	}

	return session;
}

static void lim_process_sme_disassoc_cnf(struct mac_context *mac_ctx,
					 struct scheduler_msg *msg)
{
	struct disassoc_cnf sme_disassoc_cnf;
	struct pe_session *session;
	uint32_t *err_msg = NULL;
	QDF_STATUS status;

	qdf_mem_copy(&sme_disassoc_cnf, msg->bodyptr, sizeof(sme_disassoc_cnf));

	session = lim_get_disconnect_session(mac_ctx,
					     sme_disassoc_cnf.bssid.bytes,
					     sme_disassoc_cnf.vdev_id);
	if (!session) {
		pe_err("session not found for bssid:"QDF_MAC_ADDR_FMT "vdev id: %d",
		       QDF_MAC_ADDR_REF(sme_disassoc_cnf.bssid.bytes),
		       sme_disassoc_cnf.vdev_id);
		status = lim_prepare_disconnect_done_ind
						(mac_ctx, &err_msg,
						sme_disassoc_cnf.vdev_id,
						eSIR_SME_INVALID_SESSION,
						NULL);

		if (QDF_IS_STATUS_SUCCESS(status))
			lim_send_sme_disassoc_deauth_ntf(mac_ctx,
							 QDF_STATUS_SUCCESS,
							 err_msg);
		return;
	}

	/* In LFR3, if Roam synch indication processing might fail
	 * in CSR, then CSR and LIM peer will be out-of-sync.
	 * To recover from this, during HO failure delete the LIM
	 * session corresponding to the vdev id, irrespective of the
	 * BSSID. Copy the BSSID present in LIM session into the
	 * disassoc cnf msg sent to LIM and SME.
	 */
	if (LIM_IS_STA_ROLE(session)) {
		struct disassoc_cnf *sta_disassoc_cnf = msg->bodyptr;
		qdf_mem_copy(sta_disassoc_cnf->bssid.bytes,
			     session->bssId, QDF_MAC_ADDR_SIZE);
		qdf_mem_copy(sta_disassoc_cnf->peer_macaddr.bytes,
			     session->bssId, QDF_MAC_ADDR_SIZE);
		lim_process_disconnect_sta(session, msg);
	} else {
		__lim_process_sme_disassoc_cnf(mac_ctx,
					       (uint32_t *)msg->bodyptr);
	}
}

static void lim_process_sme_disassoc_req(struct mac_context *mac_ctx,
					 struct scheduler_msg *msg)
{
	struct disassoc_req disassoc_req;
	struct pe_session *session;

	qdf_mem_copy(&disassoc_req, msg->bodyptr, sizeof(struct disassoc_req));

	session = lim_get_disconnect_session(mac_ctx,
					     disassoc_req.bssid.bytes,
					     disassoc_req.sessionId);
	if (!session) {
		pe_err("session not found for bssid:"QDF_MAC_ADDR_FMT "vdev id: %d",
		       QDF_MAC_ADDR_REF(disassoc_req.bssid.bytes),
		       disassoc_req.sessionId);
		lim_send_sme_disassoc_ntf(mac_ctx,
					  disassoc_req.peer_macaddr.bytes,
					  eSIR_SME_INVALID_PARAMETERS,
					  eLIM_HOST_DISASSOC, 1,
					  disassoc_req.sessionId, NULL);

		return;
	}

	/* In LFR2.0 roaming scenario, if association is not completed with
	 * new AP, there is possibality of trying to send disassoc in
	 * failure handling. So, if vdev is in INIT state send
	 * disassoc failure and cleanup session.
	 */
	if (QDF_IS_STATUS_SUCCESS(
		wlan_vdev_mlme_is_init_state(session->vdev))) {
		pe_err("vdev is in INIT state. Send failure.");
		lim_send_sme_disassoc_ntf(mac_ctx,
					  disassoc_req.peer_macaddr.bytes,
					  eSIR_SME_INVALID_PARAMETERS,
					  eLIM_HOST_DISASSOC, 1,
					  disassoc_req.sessionId, session);

		return;
	}

	/* In LFR3, if Roam synch indication processing might fail
	 * in CSR, then CSR and LIM peer will be out-of-sync.
	 * To recover from this, during HO failure delete the LIM
	 * session corresponding to the vdev id, irrespective of the
	 * BSSID. Copy the BSSID present in LIM session into the
	 * disassoc cnf msg sent to LIM and SME.
	 */
	if (LIM_IS_STA_ROLE(session)) {
		struct disassoc_req *sta_disassoc_req = msg->bodyptr;
		qdf_mem_copy(sta_disassoc_req->bssid.bytes,
			     session->bssId, QDF_MAC_ADDR_SIZE);
		qdf_mem_copy(sta_disassoc_req->peer_macaddr.bytes,
			     session->bssId, QDF_MAC_ADDR_SIZE);
		lim_process_disconnect_sta(session, msg);
	} else {
		__lim_process_sme_disassoc_req(mac_ctx,
					       (uint32_t *)msg->bodyptr);
	}
}

static void lim_process_sme_deauth_req(struct mac_context *mac_ctx,
				       struct scheduler_msg *msg)
{
	struct deauth_req sme_deauth_req;
	struct pe_session *session;

	qdf_mem_copy(&sme_deauth_req, msg->bodyptr, sizeof(sme_deauth_req));

	session = lim_get_disconnect_session(mac_ctx,
					     sme_deauth_req.bssid.bytes,
					     sme_deauth_req.vdev_id);

	if (!session) {
		pe_err("session not found for bssid:"QDF_MAC_ADDR_FMT "vdev id: %d",
		       QDF_MAC_ADDR_REF(sme_deauth_req.bssid.bytes),
		       sme_deauth_req.vdev_id);
		lim_send_sme_deauth_ntf(mac_ctx,
					sme_deauth_req.peer_macaddr.bytes,
					eSIR_SME_INVALID_PARAMETERS,
					eLIM_HOST_DEAUTH, 1,
					sme_deauth_req.vdev_id);

		return;
	}

	if (QDF_IS_STATUS_SUCCESS(
		wlan_vdev_mlme_is_init_state(session->vdev))) {
		pe_err("vdev is in INIT state. Send failure.");
		lim_send_sme_deauth_ntf(mac_ctx,
					sme_deauth_req.peer_macaddr.bytes,
					eSIR_SME_INVALID_PARAMETERS,
					eLIM_HOST_DEAUTH, 1,
					sme_deauth_req.vdev_id);
		return;
	}

	/* In LFR3, if Roam synch indication processing might fail
	 * in CSR, then CSR and LIM peer will be out-of-sync.
	 * To recover from this, during HO failure delete the LIM
	 * session corresponding to the vdev id, irrespective of the
	 * BSSID. Copy the BSSID present in LIM session into the
	 * disassoc cnf msg sent to LIM and SME.
	 */
	if (LIM_IS_STA_ROLE(session)) {
		struct deauth_req *sta_deauth_req = msg->bodyptr;
		qdf_mem_copy(sta_deauth_req->bssid.bytes,
			     session->bssId, QDF_MAC_ADDR_SIZE);
		qdf_mem_copy(sta_deauth_req->peer_macaddr.bytes,
			     session->bssId, QDF_MAC_ADDR_SIZE);
		lim_process_disconnect_sta(session, msg);
	} else {
		__lim_process_sme_deauth_req(mac_ctx,
					     (uint32_t *)msg->bodyptr);
	}
}

/**
 * lim_process_sme_req_messages()
 *
 ***FUNCTION:
 * This function is called by limProcessMessageQueue(). This
 * function processes SME request messages from HDD or upper layer
 * application.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param  mac      Pointer to Global MAC structure
 * @param  msgType   Indicates the SME message type
 * @param  *msg_buf  A pointer to the SME message buffer
 * @return Boolean - true - if msg_buf is consumed and can be freed.
 *                   false - if msg_buf is not to be freed.
 */

bool lim_process_sme_req_messages(struct mac_context *mac,
				  struct scheduler_msg *pMsg)
{
	/*
	 * Set this flag to false within case block of any following message,
	 * that doesn't want msg_buf to be freed.
	 */
	bool bufConsumed = true;
	uint32_t *msg_buf = pMsg->bodyptr;

	pe_nofl_debug("LIM handle SME Msg %s(%d)",
		      lim_msg_str(pMsg->type), pMsg->type);

	/* If no insert NOA required then execute the code below */

	switch (pMsg->type) {
	case eWNI_SME_SYS_READY_IND:
		bufConsumed = __lim_process_sme_sys_ready_ind(mac, msg_buf);
		break;

	case eWNI_SME_START_BSS_REQ:
		bufConsumed = __lim_process_sme_start_bss_req(mac, pMsg);
		break;

	case eWNI_SME_JOIN_REQ:
		__lim_process_sme_join_req(mac, msg_buf);
		break;

	case eWNI_SME_REASSOC_REQ:
		__lim_process_sme_reassoc_req(mac, msg_buf);
		break;

	case eWNI_SME_DISASSOC_REQ:
		lim_process_sme_disassoc_req(mac, pMsg);
		break;

	case eWNI_SME_DISASSOC_CNF:
	case eWNI_SME_DEAUTH_CNF:
		lim_process_sme_disassoc_cnf(mac, pMsg);
		break;

	case eWNI_SME_DEAUTH_REQ:
		lim_process_sme_deauth_req(mac, pMsg);
		break;

	case eWNI_SME_SEND_DISASSOC_FRAME:
		__lim_process_send_disassoc_frame(mac, msg_buf);
		break;

	case eWNI_SME_STOP_BSS_REQ:
		bufConsumed = __lim_process_sme_stop_bss_req(mac, pMsg);
		break;

	case eWNI_SME_ASSOC_CNF:
		if (pMsg->type == eWNI_SME_ASSOC_CNF)
			pe_debug("Received ASSOC_CNF message");
			__lim_process_sme_assoc_cnf_new(mac, pMsg->type,
							msg_buf);
		break;

	case eWNI_SME_ADDTS_REQ:
		pe_debug("Received ADDTS_REQ message");
		__lim_process_sme_addts_req(mac, msg_buf);
		break;

	case eWNI_SME_MSCS_REQ:
		pe_debug("Received MSCS_REQ message");
		__lim_process_sme_mscs_req(mac, msg_buf);
		break;

	case eWNI_SME_DELTS_REQ:
		pe_debug("Received DELTS_REQ message");
		__lim_process_sme_delts_req(mac, msg_buf);
		break;

	case SIR_LIM_ADDTS_RSP_TIMEOUT:
		pe_debug("Received SIR_LIM_ADDTS_RSP_TIMEOUT message");
		lim_process_sme_addts_rsp_timeout(mac, pMsg->bodyval);
		break;

#ifdef FEATURE_WLAN_ESE
	case eWNI_SME_GET_TSM_STATS_REQ:
		__lim_process_sme_get_tsm_stats_request(mac, msg_buf);
		bufConsumed = false;
		break;
#endif /* FEATURE_WLAN_ESE */
	case eWNI_SME_SESSION_UPDATE_PARAM:
		__lim_process_sme_session_update(mac, msg_buf);
		break;
	case eWNI_SME_ROAM_SEND_SET_PCL_REQ:
		lim_send_roam_set_pcl(mac, (struct set_pcl_req *)msg_buf);
		bufConsumed = false;
		break;
	case eWNI_SME_ROAM_INVOKE:
		lim_process_roam_invoke(mac, msg_buf);
		bufConsumed = false;
		break;
	case eWNI_SME_CHNG_MCC_BEACON_INTERVAL:
		/* Update the beaconInterval */
		__lim_process_sme_change_bi(mac, msg_buf);
		break;

#ifdef QCA_HT_2040_COEX
	case eWNI_SME_SET_HT_2040_MODE:
		__lim_process_sme_set_ht2040_mode(mac, msg_buf);
		break;
#endif

	case eWNI_SME_NEIGHBOR_REPORT_REQ_IND:
	case eWNI_SME_BEACON_REPORT_RESP_XMIT_IND:
		__lim_process_report_message(mac, pMsg);
		break;

	case eWNI_SME_FT_PRE_AUTH_REQ:
		bufConsumed = (bool) lim_process_ft_pre_auth_req(mac, pMsg);
		break;

	case eWNI_SME_FT_AGGR_QOS_REQ:
		lim_process_ft_aggr_qos_req(mac, msg_buf);
		break;

	case eWNI_SME_REGISTER_MGMT_FRAME_REQ:
		__lim_process_sme_register_mgmt_frame_req(mac, msg_buf);
		break;
#ifdef FEATURE_WLAN_TDLS
	case eWNI_SME_TDLS_SEND_MGMT_REQ:
		lim_process_sme_tdls_mgmt_send_req(mac, msg_buf);
		break;
	case eWNI_SME_TDLS_ADD_STA_REQ:
		lim_process_sme_tdls_add_sta_req(mac, msg_buf);
		break;
	case eWNI_SME_TDLS_DEL_STA_REQ:
		lim_process_sme_tdls_del_sta_req(mac, msg_buf);
		break;
#endif
	case eWNI_SME_RESET_AP_CAPS_CHANGED:
		__lim_process_sme_reset_ap_caps_change(mac, msg_buf);
		break;

	case eWNI_SME_CHANNEL_CHANGE_REQ:
		lim_process_sme_channel_change_request(mac, msg_buf);
		break;

	case eWNI_SME_START_BEACON_REQ:
		lim_process_sme_start_beacon_req(mac, msg_buf);
		break;

	case eWNI_SME_DFS_BEACON_CHAN_SW_IE_REQ:
		lim_process_sme_dfs_csa_ie_request(mac, msg_buf);
		break;

	case eWNI_SME_UPDATE_ADDITIONAL_IES:
		lim_process_update_add_ies(mac, msg_buf);
		break;

	case eWNI_SME_MODIFY_ADDITIONAL_IES:
		lim_process_modify_add_ies(mac, msg_buf);
		break;
	case eWNI_SME_SET_HW_MODE_REQ:
		lim_process_set_hw_mode(mac, msg_buf);
		break;
	case eWNI_SME_NSS_UPDATE_REQ:
		lim_process_nss_update_request(mac, msg_buf);
		break;
	case eWNI_SME_SET_DUAL_MAC_CFG_REQ:
		lim_process_set_dual_mac_cfg_req(mac, msg_buf);
		break;
	case eWNI_SME_SET_IE_REQ:
		lim_process_set_ie_req(mac, msg_buf);
		break;
	case eWNI_SME_REGISTER_MGMT_FRAME_CB:
		lim_register_mgmt_frame_ind_cb(mac, msg_buf);
		break;
	case eWNI_SME_EXT_CHANGE_CHANNEL:
		lim_process_ext_change_channel(mac, msg_buf);
		break;
	case eWNI_SME_SET_ANTENNA_MODE_REQ:
		lim_process_set_antenna_mode_req(mac, msg_buf);
		break;
	case eWNI_SME_PDEV_SET_HT_VHT_IE:
		lim_process_set_pdev_IEs(mac, msg_buf);
		break;
	case eWNI_SME_SET_VDEV_IES_PER_BAND:
		lim_process_set_vdev_ies_per_band(mac, msg_buf);
		break;
	case eWNI_SME_UPDATE_ACCESS_POLICY_VENDOR_IE:
		lim_process_sme_update_access_policy_vendor_ie(mac, msg_buf);
		break;
	case eWNI_SME_UPDATE_CONFIG:
		lim_process_sme_update_config(mac,
					(struct update_config *)msg_buf);
		break;
	case eWNI_SME_SET_ADDBA_ACCEPT:
		lim_process_sme_set_addba_accept(mac,
					(struct sme_addba_accept *)msg_buf);
		break;
	case eWNI_SME_UPDATE_EDCA_PROFILE:
		lim_process_sme_update_edca_params(mac, pMsg->bodyval);
		break;
	case WNI_SME_UPDATE_MU_EDCA_PARAMS:
		lim_process_sme_update_mu_edca_params(mac, pMsg->bodyval);
		break;
	case eWNI_SME_UPDATE_SESSION_EDCA_TXQ_PARAMS:
		lim_process_sme_update_session_edca_txq_params(mac, msg_buf);
		break;
	case WNI_SME_CFG_ACTION_FRM_HE_TB_PPDU:
		lim_process_sme_cfg_action_frm_in_tb_ppdu(mac,
				(struct  sir_cfg_action_frm_tb_ppdu *)msg_buf);
		break;
	default:
		qdf_mem_free((void *)pMsg->bodyptr);
		pMsg->bodyptr = NULL;
		break;
	} /* switch (msgType) */

	return bufConsumed;
} /*** end lim_process_sme_req_messages() ***/

/**
 * lim_process_sme_start_beacon_req()
 *
 ***FUNCTION:
 * This function is called by limProcessMessageQueue(). This
 * function processes SME request messages from HDD or upper layer
 * application.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param  mac      Pointer to Global MAC structure
 * @param  msgType   Indicates the SME message type
 * @param  *msg_buf  A pointer to the SME message buffer
 * @return Boolean - true - if msg_buf is consumed and can be freed.
 *                   false - if msg_buf is not to be freed.
 */
static void lim_process_sme_start_beacon_req(struct mac_context *mac, uint32_t *pMsg)
{
	tpSirStartBeaconIndication pBeaconStartInd;
	struct pe_session *pe_session;
	uint8_t sessionId;      /* PE sessionID */

	if (!pMsg) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	pBeaconStartInd = (tpSirStartBeaconIndication) pMsg;
	pe_session = pe_find_session_by_bssid(mac,
				pBeaconStartInd->bssid,
				&sessionId);
	if (!pe_session) {
		lim_print_mac_addr(mac, pBeaconStartInd->bssid, LOGE);
		pe_err("Session does not exist for given bssId");
		return;
	}

	if (pBeaconStartInd->beaconStartStatus == true) {
		/*
		 * Currently this Indication comes from SAP
		 * to start Beacon Tx on a DFS channel
		 * since beaconing has to be done on DFS
		 * channel only after CAC WAIT is completed.
		 * On a DFS Channel LIM does not start beacon
		 * Tx right after the WMA_ADD_BSS_RSP.
		 */
		lim_apply_configuration(mac, pe_session);
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			  FL("Start Beacon with ssid %s Ch freq %d"),
			  pe_session->ssId.ssId,
			  pe_session->curr_op_freq);
		lim_send_beacon(mac, pe_session);
		lim_enable_obss_detection_config(mac, pe_session);
		lim_send_obss_color_collision_cfg(mac, pe_session,
					OBSS_COLOR_COLLISION_DETECTION);
	} else {
		pe_err("Invalid Beacon Start Indication");
		return;
	}
}

static void lim_mon_change_channel(
	struct mac_context *mac_ctx,
	struct pe_session *session_entry)
{
	if (wlan_vdev_mlme_get_state(session_entry->vdev) == WLAN_VDEV_S_INIT)
		wlan_vdev_mlme_sm_deliver_evt(session_entry->vdev,
					      WLAN_VDEV_SM_EV_START,
					      sizeof(*session_entry),
					      session_entry);
	else if (wlan_vdev_mlme_get_state(session_entry->vdev) ==
		 WLAN_VDEV_S_UP) {
		mlme_set_chan_switch_in_progress(session_entry->vdev, true);
		wlan_vdev_mlme_sm_deliver_evt(session_entry->vdev,
					      WLAN_VDEV_SM_EV_FW_VDEV_RESTART,
					      sizeof(*session_entry),
					      session_entry);
	} else {
		pe_err("Invalid vdev state to change channel");
	}
}

static void lim_change_channel(
	struct mac_context *mac_ctx,
	struct pe_session *session_entry)
{
	if (session_entry->bssType == eSIR_MONITOR_MODE)
		return lim_mon_change_channel(mac_ctx, session_entry);

	mlme_set_chan_switch_in_progress(session_entry->vdev, true);

	if (wlan_vdev_mlme_get_state(session_entry->vdev) ==
	    WLAN_VDEV_S_DFS_CAC_WAIT)
		wlan_vdev_mlme_sm_deliver_evt(session_entry->vdev,
					      WLAN_VDEV_SM_EV_RADAR_DETECTED,
					      sizeof(*session_entry),
					      session_entry);
	else
		wlan_vdev_mlme_sm_deliver_evt(session_entry->vdev,
					      WLAN_VDEV_SM_EV_CSA_COMPLETE,
					      sizeof(*session_entry),
					      session_entry);
}

/**
 * lim_process_sme_channel_change_request() - process sme ch change req
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: pointer to the SME message buffer
 *
 * This function is called to process SME_CHANNEL_CHANGE_REQ message
 *
 * Return: None
 */
static void lim_process_sme_channel_change_request(struct mac_context *mac_ctx,
		uint32_t *msg_buf)
{
	tpSirChanChangeRequest ch_change_req;
	struct pe_session *session_entry;
	uint8_t session_id;      /* PE session_id */
	int8_t max_tx_pwr;
	uint32_t target_freq;
	bool is_curr_ch_2g, is_new_ch_2g, update_he_cap;

	if (!msg_buf) {
		pe_err("msg_buf is NULL");
		return;
	}
	ch_change_req = (tpSirChanChangeRequest)msg_buf;

	target_freq = ch_change_req->target_chan_freq;

	max_tx_pwr = wlan_reg_get_channel_reg_power_for_freq(
				mac_ctx->pdev, target_freq);

	if ((ch_change_req->messageType != eWNI_SME_CHANNEL_CHANGE_REQ) ||
			(max_tx_pwr == WMA_MAX_TXPOWER_INVALID)) {
		pe_err("Invalid Request/max_tx_pwr");
		return;
	}

	session_entry = pe_find_session_by_bssid(mac_ctx,
			ch_change_req->bssid, &session_id);
	if (!session_entry) {
		lim_print_mac_addr(mac_ctx, ch_change_req->bssid, LOGE);
		pe_err("Session does not exist for given bssId");
		return;
	}

	if (session_entry->curr_op_freq == target_freq &&
	    session_entry->ch_width == ch_change_req->ch_width) {
		pe_err("Target channel and mode is same as current channel and mode channel freq %d and mode %d",
		       session_entry->curr_op_freq, session_entry->ch_width);
		return;
	}

	if (LIM_IS_AP_ROLE(session_entry))
		session_entry->channelChangeReasonCode =
			LIM_SWITCH_CHANNEL_SAP_DFS;
	else
		session_entry->channelChangeReasonCode =
			LIM_SWITCH_CHANNEL_MONITOR;

	pe_nofl_debug("SAP CSA: %d ---> %d, ch_bw %d, nw_type %d, dot11mode %d",
		      session_entry->curr_op_freq, target_freq,
		      ch_change_req->ch_width, ch_change_req->nw_type,
		      ch_change_req->dot11mode);

	if (IS_DOT11_MODE_HE(ch_change_req->dot11mode) &&
		((QDF_MONITOR_MODE == session_entry->opmode) ||
		lim_is_session_he_capable(session_entry))) {
		lim_update_session_he_capable_chan_switch
			(mac_ctx, session_entry, target_freq);
		is_new_ch_2g = wlan_reg_is_24ghz_ch_freq(target_freq);
		is_curr_ch_2g = wlan_reg_is_24ghz_ch_freq(
					session_entry->curr_op_freq);
		if ((is_new_ch_2g && !is_curr_ch_2g) ||
		    (!is_new_ch_2g && is_curr_ch_2g))
			update_he_cap = true;
		else
			update_he_cap = false;
		if (!update_he_cap) {
			if ((session_entry->ch_width !=
			     ch_change_req->ch_width) &&
			    (session_entry->ch_width > CH_WIDTH_80MHZ ||
			     ch_change_req->ch_width > CH_WIDTH_80MHZ))
				update_he_cap = true;
		}
		if (update_he_cap) {
			session_entry->curr_op_freq = target_freq;
			session_entry->ch_width = ch_change_req->ch_width;
			lim_copy_bss_he_cap(session_entry);
			lim_update_he_bw_cap_mcs(session_entry, NULL);
		}
	} else if (wlan_reg_is_6ghz_chan_freq(target_freq)) {
		pe_debug("Invalid target_freq %d for dot11mode %d cur HE %d",
			 target_freq, ch_change_req->dot11mode,
			 lim_is_session_he_capable(session_entry));
		return;
	}

	/* Store the New Channel Params in session_entry */
	session_entry->ch_width = ch_change_req->ch_width;
	session_entry->ch_center_freq_seg0 =
		ch_change_req->center_freq_seg_0;
	session_entry->ch_center_freq_seg1 =
		ch_change_req->center_freq_seg_1;
	session_entry->htSecondaryChannelOffset = ch_change_req->sec_ch_offset;
	session_entry->htSupportedChannelWidthSet =
		(ch_change_req->ch_width ? 1 : 0);
	session_entry->htRecommendedTxWidthSet =
		session_entry->htSupportedChannelWidthSet;
	session_entry->curr_op_freq = target_freq;
	session_entry->limRFBand = lim_get_rf_band(
		session_entry->curr_op_freq);
	if (mlme_get_cac_required(session_entry->vdev))
		session_entry->cac_duration_ms = ch_change_req->cac_duration_ms;
	else
		session_entry->cac_duration_ms = 0;
	session_entry->dfs_regdomain = ch_change_req->dfs_regdomain;
	session_entry->maxTxPower = max_tx_pwr;

	/* Update the global beacon filter */
	lim_update_bcn_probe_filter(mac_ctx, session_entry);

	/* Initialize 11h Enable Flag */
	if (CHAN_HOP_ALL_BANDS_ENABLE ||
	    session_entry->limRFBand == REG_BAND_5G)
		session_entry->lim11hEnable =
			mac_ctx->mlme_cfg->gen.enabled_11h;
	else
		session_entry->lim11hEnable = 0;

	session_entry->dot11mode = ch_change_req->dot11mode;
	session_entry->nwType = ch_change_req->nw_type;
	qdf_mem_copy(&session_entry->rateSet,
			&ch_change_req->operational_rateset,
			sizeof(session_entry->rateSet));
	qdf_mem_copy(&session_entry->extRateSet,
			&ch_change_req->extended_rateset,
			sizeof(session_entry->extRateSet));

	lim_change_channel(mac_ctx, session_entry);
}

/******************************************************************************
* lim_start_bss_update_add_ie_buffer()
*
***FUNCTION:
* This function checks the src buffer and its length and then malloc for
* dst buffer update the same
*
***LOGIC:
*
***ASSUMPTIONS:
*
***NOTE:
*
* @param  mac      Pointer to Global MAC structure
* @param  **pDstData_buff  A pointer to pointer of  uint8_t dst buffer
* @param  *pDstDataLen  A pointer to pointer of  uint16_t dst buffer length
* @param  *pSrcData_buff  A pointer of  uint8_t  src buffer
* @param  srcDataLen  src buffer length
******************************************************************************/

static void
lim_start_bss_update_add_ie_buffer(struct mac_context *mac,
				   uint8_t **pDstData_buff,
				   uint16_t *pDstDataLen,
				   uint8_t *pSrcData_buff, uint16_t srcDataLen)
{

	if (srcDataLen > 0 && pSrcData_buff) {
		*pDstDataLen = srcDataLen;

		*pDstData_buff = qdf_mem_malloc(*pDstDataLen);
		if (!*pDstData_buff)
			return;
		qdf_mem_copy(*pDstData_buff, pSrcData_buff, *pDstDataLen);
	} else {
		*pDstData_buff = NULL;
		*pDstDataLen = 0;
	}
}

/******************************************************************************
* lim_update_add_ie_buffer()
*
***FUNCTION:
* This function checks the src buffer and length if src buffer length more
* than dst buffer length then free the dst buffer and malloc for the new src
* length, and update the dst buffer and length. But if dst buffer is bigger
* than src buffer length then it just update the dst buffer and length
*
***LOGIC:
*
***ASSUMPTIONS:
*
***NOTE:
*
* @param  mac      Pointer to Global MAC structure
* @param  **pDstData_buff  A pointer to pointer of  uint8_t dst buffer
* @param  *pDstDataLen  A pointer to pointer of  uint16_t dst buffer length
* @param  *pSrcData_buff  A pointer of  uint8_t  src buffer
* @param  srcDataLen  src buffer length
******************************************************************************/

static void
lim_update_add_ie_buffer(struct mac_context *mac,
			 uint8_t **pDstData_buff,
			 uint16_t *pDstDataLen,
			 uint8_t *pSrcData_buff, uint16_t srcDataLen)
{

	if (!pSrcData_buff) {
		pe_err("src buffer is null");
		return;
	}

	if (srcDataLen > *pDstDataLen) {
		*pDstDataLen = srcDataLen;
		/* free old buffer */
		qdf_mem_free(*pDstData_buff);
		/* allocate a new */
		*pDstData_buff = qdf_mem_malloc(*pDstDataLen);
		if (!*pDstData_buff) {
			*pDstDataLen = 0;
			return;
		}
	}

	/* copy the content of buffer into dst buffer
	 */
	*pDstDataLen = srcDataLen;
	qdf_mem_copy(*pDstData_buff, pSrcData_buff, *pDstDataLen);

}

/*
* lim_process_modify_add_ies() - process modify additional IE req.
*
* @mac_ctx: Pointer to Global MAC structure
* @msg_buf: pointer to the SME message buffer
*
* This function update the PE buffers for additional IEs.
*
* Return: None
*/
static void lim_process_modify_add_ies(struct mac_context *mac_ctx,
		uint32_t *msg_buf)
{
	tpSirModifyIEsInd modify_add_ies;
	struct pe_session *session_entry;
	uint8_t session_id;
	bool ret = false;
	struct add_ie_params *add_ie_params;

	if (!msg_buf) {
		pe_err("msg_buf is NULL");
		return;
	}

	modify_add_ies = (tpSirModifyIEsInd)msg_buf;
	/* Incoming message has smeSession, use BSSID to find PE session */
	session_entry = pe_find_session_by_bssid(mac_ctx,
			modify_add_ies->modifyIE.bssid.bytes, &session_id);

	if (!session_entry) {
		pe_err("Session not found for given bssid"
					QDF_MAC_ADDR_FMT,
		QDF_MAC_ADDR_REF(modify_add_ies->modifyIE.bssid.bytes));
		goto end;
	}
	if ((0 == modify_add_ies->modifyIE.ieBufferlength) ||
		(0 == modify_add_ies->modifyIE.ieIDLen) ||
		(!modify_add_ies->modifyIE.pIEBuffer)) {
		pe_err("Invalid request pIEBuffer %pK ieBufferlength %d ieIDLen %d ieID %d. update Type %d",
				modify_add_ies->modifyIE.pIEBuffer,
				modify_add_ies->modifyIE.ieBufferlength,
				modify_add_ies->modifyIE.ieID,
				modify_add_ies->modifyIE.ieIDLen,
				modify_add_ies->updateType);
		goto end;
	}
	add_ie_params = &session_entry->add_ie_params;
	switch (modify_add_ies->updateType) {
	case eUPDATE_IE_PROBE_RESP:
		/* Probe resp */
		break;
	case eUPDATE_IE_ASSOC_RESP:
		/* assoc resp IE */
		if (add_ie_params->assocRespDataLen == 0) {
			QDF_TRACE(QDF_MODULE_ID_PE,
					QDF_TRACE_LEVEL_ERROR, FL(
				"assoc resp add ie not present %d"),
				add_ie_params->assocRespDataLen);
		}
		/* search through the buffer and modify the IE */
		break;
	case eUPDATE_IE_PROBE_BCN:
		/*probe beacon IE */
		if (ret == true && modify_add_ies->modifyIE.notify) {
			lim_handle_param_update(mac_ctx,
					modify_add_ies->updateType);
		}
		break;
	default:
		pe_err("unhandled buffer type %d",
				modify_add_ies->updateType);
		break;
	}
end:
	qdf_mem_free(modify_add_ies->modifyIE.pIEBuffer);
	modify_add_ies->modifyIE.pIEBuffer = NULL;
}

/*
* lim_process_update_add_ies() - process additional IE update req
*
* @mac_ctx: Pointer to Global MAC structure
* @msg_buf: pointer to the SME message buffer
*
* This function update the PE buffers for additional IEs.
*
* Return: None
*/
static void lim_process_update_add_ies(struct mac_context *mac_ctx,
		uint32_t *msg_buf)
{
	tpSirUpdateIEsInd update_add_ies = (tpSirUpdateIEsInd)msg_buf;
	uint8_t session_id;
	struct pe_session *session_entry;
	struct add_ie_params *addn_ie;
	uint16_t new_length = 0;
	uint8_t *new_ptr = NULL;
	tSirUpdateIE *update_ie;

	if (!msg_buf) {
		pe_err("msg_buf is NULL");
		return;
	}
	update_ie = &update_add_ies->updateIE;
	/* incoming message has smeSession, use BSSID to find PE session */
	session_entry = pe_find_session_by_bssid(mac_ctx,
			update_ie->bssid.bytes, &session_id);

	if (!session_entry) {
		pe_debug("Session not found for given bssid"
			 QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(update_ie->bssid.bytes));
		goto end;
	}
	addn_ie = &session_entry->add_ie_params;
	/* if len is 0, upper layer requested freeing of buffer */
	if (0 == update_ie->ieBufferlength) {
		switch (update_add_ies->updateType) {
		case eUPDATE_IE_PROBE_RESP:
			qdf_mem_free(addn_ie->probeRespData_buff);
			addn_ie->probeRespData_buff = NULL;
			addn_ie->probeRespDataLen = 0;
			break;
		case eUPDATE_IE_ASSOC_RESP:
			qdf_mem_free(addn_ie->assocRespData_buff);
			addn_ie->assocRespData_buff = NULL;
			addn_ie->assocRespDataLen = 0;
			break;
		case eUPDATE_IE_PROBE_BCN:
			qdf_mem_free(addn_ie->probeRespBCNData_buff);
			addn_ie->probeRespBCNData_buff = NULL;
			addn_ie->probeRespBCNDataLen = 0;

			if (update_ie->notify)
				lim_handle_param_update(mac_ctx,
						update_add_ies->updateType);
			break;
		default:
			break;
		}
		return;
	}
	switch (update_add_ies->updateType) {
	case eUPDATE_IE_PROBE_RESP:
		if (update_ie->append) {
			/*
			 * In case of append, allocate new memory
			 * with combined length.
			 * Multiple back to back append commands
			 * can lead to a huge length.So, check
			 * for the validity of the length.
			 */
			if (addn_ie->probeRespDataLen >
				(USHRT_MAX - update_ie->ieBufferlength)) {
				pe_err("IE Length overflow, curr:%d, new:%d",
					addn_ie->probeRespDataLen,
					update_ie->ieBufferlength);
				goto end;
			}
			new_length = update_ie->ieBufferlength +
				addn_ie->probeRespDataLen;
			new_ptr = qdf_mem_malloc(new_length);
			if (!new_ptr)
				goto end;
			/* append buffer to end of local buffers */
			qdf_mem_copy(new_ptr, addn_ie->probeRespData_buff,
					addn_ie->probeRespDataLen);
			qdf_mem_copy(&new_ptr[addn_ie->probeRespDataLen],
				     update_ie->pAdditionIEBuffer,
				     update_ie->ieBufferlength);
			/* free old memory */
			qdf_mem_free(addn_ie->probeRespData_buff);
			/* adjust length accordingly */
			addn_ie->probeRespDataLen = new_length;
			/* save refernece of local buffer in PE session */
			addn_ie->probeRespData_buff = new_ptr;
			goto end;
		}
		lim_update_add_ie_buffer(mac_ctx, &addn_ie->probeRespData_buff,
				&addn_ie->probeRespDataLen,
				update_ie->pAdditionIEBuffer,
				update_ie->ieBufferlength);
		break;
	case eUPDATE_IE_ASSOC_RESP:
		/* assoc resp IE */
		lim_update_add_ie_buffer(mac_ctx, &addn_ie->assocRespData_buff,
				&addn_ie->assocRespDataLen,
				update_ie->pAdditionIEBuffer,
				update_ie->ieBufferlength);
		break;
	case eUPDATE_IE_PROBE_BCN:
		/* probe resp Bcn IE */
		lim_update_add_ie_buffer(mac_ctx,
				&addn_ie->probeRespBCNData_buff,
				&addn_ie->probeRespBCNDataLen,
				update_ie->pAdditionIEBuffer,
				update_ie->ieBufferlength);
		if (update_ie->notify)
			lim_handle_param_update(mac_ctx,
					update_add_ies->updateType);
		break;
	default:
		pe_err("unhandled buffer type %d", update_add_ies->updateType);
		break;
	}
end:
	qdf_mem_free(update_ie->pAdditionIEBuffer);
	update_ie->pAdditionIEBuffer = NULL;
}

void send_extended_chan_switch_action_frame(struct mac_context *mac_ctx,
					    uint16_t new_channel_freq,
					    enum phy_ch_width ch_bandwidth,
					    struct pe_session *session_entry)
{
	uint8_t op_class = 0;
	uint8_t switch_mode = 0, i;
	tpDphHashNode psta;
	uint8_t switch_count;
	uint8_t new_channel = 0;

	op_class =
		lim_op_class_from_bandwidth(mac_ctx, new_channel_freq,
					    ch_bandwidth,
					    session_entry->gLimChannelSwitch.sec_ch_offset);
	new_channel = wlan_reg_freq_to_chan(mac_ctx->pdev, new_channel_freq);
	if (LIM_IS_AP_ROLE(session_entry) &&
		(mac_ctx->sap.SapDfsInfo.disable_dfs_ch_switch == false))
		switch_mode = session_entry->gLimChannelSwitch.switchMode;

	switch_count = session_entry->gLimChannelSwitch.switchCount;

	if (LIM_IS_AP_ROLE(session_entry)) {
		for (i = 0; i <= mac_ctx->lim.max_sta_of_pe_session; i++) {
			psta =
			  session_entry->dph.dphHashTable.pDphNodeArray + i;
			if (psta && psta->added)
				lim_send_extended_chan_switch_action_frame(
					mac_ctx,
					psta->staAddr,
					switch_mode, op_class, new_channel,
					switch_count, session_entry);
		}
	} else if (LIM_IS_STA_ROLE(session_entry)) {
		lim_send_extended_chan_switch_action_frame(mac_ctx,
					session_entry->bssId,
					switch_mode, op_class, new_channel,
					switch_count, session_entry);
	}

}

void lim_send_chan_switch_action_frame(struct mac_context *mac_ctx,
				       uint16_t new_channel_freq,
				       enum phy_ch_width ch_bandwidth,
				       struct pe_session *session_entry)
{
	uint8_t op_class = 0, new_channel;
	uint8_t switch_mode = 0, i;
	uint8_t switch_count;
	tpDphHashNode psta;
	tpDphHashNode dph_node_array_ptr;

	dph_node_array_ptr = session_entry->dph.dphHashTable.pDphNodeArray;
	op_class =
		lim_op_class_from_bandwidth(mac_ctx, new_channel_freq,
					    ch_bandwidth,
					    session_entry->gLimChannelSwitch.sec_ch_offset);
	new_channel = wlan_reg_freq_to_chan(mac_ctx->pdev, new_channel_freq);

	if (LIM_IS_AP_ROLE(session_entry) &&
	    (false == mac_ctx->sap.SapDfsInfo.disable_dfs_ch_switch))
		switch_mode = session_entry->gLimChannelSwitch.switchMode;

	switch_count = session_entry->gLimChannelSwitch.switchCount;

	if (LIM_IS_AP_ROLE(session_entry)) {
		for (i = 0; i <= mac_ctx->lim.max_sta_of_pe_session; i++) {
			psta = dph_node_array_ptr + i;
			if (!(psta && psta->added))
				continue;
			if (session_entry->lim_non_ecsa_cap_num == 0)
				lim_send_extended_chan_switch_action_frame
					(mac_ctx, psta->staAddr, switch_mode,
					 op_class, new_channel, switch_count,
					 session_entry);
			else
				lim_send_channel_switch_mgmt_frame
					(mac_ctx, psta->staAddr, switch_mode,
					 new_channel, switch_count,
					 session_entry);
		}
	} else if (LIM_IS_STA_ROLE(session_entry)) {
		lim_send_extended_chan_switch_action_frame
			(mac_ctx, session_entry->bssId, switch_mode, op_class,
			 new_channel, switch_count, session_entry);
	}
}

/**
 * lim_process_sme_dfs_csa_ie_request() - process sme dfs csa ie req
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: pointer to the SME message buffer
 *
 * This function processes SME request messages from HDD or upper layer
 * application.
 *
 * Return: None
 */
static void lim_process_sme_dfs_csa_ie_request(struct mac_context *mac_ctx,
		uint32_t *msg_buf)
{
	tpSirDfsCsaIeRequest dfs_csa_ie_req;
	struct pe_session *session_entry = NULL;
	uint8_t session_id;
	tLimWiderBWChannelSwitchInfo *wider_bw_ch_switch;
	QDF_STATUS status;
	enum phy_ch_width ch_width;
	uint32_t target_ch_freq;

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	dfs_csa_ie_req = (tSirDfsCsaIeRequest *)msg_buf;
	session_entry = pe_find_session_by_bssid(mac_ctx,
			dfs_csa_ie_req->bssid, &session_id);
	if (!session_entry) {
		pe_err("Session not found for given BSSID" QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(dfs_csa_ie_req->bssid));
		return;
	}

	if (session_entry->valid && !LIM_IS_AP_ROLE(session_entry)) {
		pe_err("Invalid SystemRole %d",
			GET_LIM_SYSTEM_ROLE(session_entry));
		return;
	}

	/* target channel */
	session_entry->gLimChannelSwitch.primaryChannel =
		wlan_reg_freq_to_chan(mac_ctx->pdev,
				      dfs_csa_ie_req->target_chan_freq);
	session_entry->gLimChannelSwitch.sw_target_freq =
		dfs_csa_ie_req->target_chan_freq;
	target_ch_freq = dfs_csa_ie_req->target_chan_freq;
	/* Channel switch announcement needs to be included in beacon */
	session_entry->dfsIncludeChanSwIe = true;
	session_entry->gLimChannelSwitch.switchCount =
		 dfs_csa_ie_req->ch_switch_beacon_cnt;
	ch_width = dfs_csa_ie_req->ch_params.ch_width;
	if (ch_width >= CH_WIDTH_160MHZ &&
	    wma_get_vht_ch_width() < WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ) {
		ch_width = CH_WIDTH_80MHZ;
	}
	session_entry->gLimChannelSwitch.ch_width = ch_width;
	session_entry->gLimChannelSwitch.sec_ch_offset =
				 dfs_csa_ie_req->ch_params.sec_ch_offset;
	if (mac_ctx->sap.SapDfsInfo.disable_dfs_ch_switch == false)
		session_entry->gLimChannelSwitch.switchMode =
			 dfs_csa_ie_req->ch_switch_mode;

	/*
	 * Validate if SAP is operating HT or VHT/HE mode and set the Channel
	 * Switch Wrapper element with the Wide Band Switch subelement.
	 */
	if (!(session_entry->vhtCapability ||
	      lim_is_session_he_capable(session_entry)))
		goto skip_vht;

	/* Now encode the Wider Ch BW element depending on the ch width */
	wider_bw_ch_switch = &session_entry->gLimWiderBWChannelSwitch;
	switch (ch_width) {
	case CH_WIDTH_20MHZ:
		/*
		 * Wide channel BW sublement in channel wrapper element is not
		 * required in case of 20 Mhz operation. Currently It is set
		 * only set in case of 40/80 Mhz Operation.
		 */
		session_entry->dfsIncludeChanWrapperIe = false;
		wider_bw_ch_switch->newChanWidth =
			WNI_CFG_VHT_CHANNEL_WIDTH_20_40MHZ;
		break;
	case CH_WIDTH_40MHZ:
		session_entry->dfsIncludeChanWrapperIe = false;
		wider_bw_ch_switch->newChanWidth =
			WNI_CFG_VHT_CHANNEL_WIDTH_20_40MHZ;
		break;
	case CH_WIDTH_80MHZ:
		session_entry->dfsIncludeChanWrapperIe = true;
		wider_bw_ch_switch->newChanWidth =
			WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ;
		break;
	case CH_WIDTH_160MHZ:
		session_entry->dfsIncludeChanWrapperIe = true;
		wider_bw_ch_switch->newChanWidth =
			WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ;
		break;
	case CH_WIDTH_80P80MHZ:
		session_entry->dfsIncludeChanWrapperIe = true;
		wider_bw_ch_switch->newChanWidth =
			WNI_CFG_VHT_CHANNEL_WIDTH_80_PLUS_80MHZ;
		/*
		 * This is not applicable for 20/40/80 Mhz.
		 * Only used when we support 80+80 Mhz operation.
		 * In case of 80+80 Mhz, this parameter indicates
		 * center channel frequency index of 80 Mhz channel of
		 * frequency segment 1.
		 */
		wider_bw_ch_switch->newCenterChanFreq1 =
			dfs_csa_ie_req->ch_params.center_freq_seg1;
		break;
	default:
		session_entry->dfsIncludeChanWrapperIe = false;
		/*
		 * Need to handle 80+80 Mhz Scenario. When 80+80 is supported
		 * set the gLimWiderBWChannelSwitch.newChanWidth to 3
		 */
		pe_err("Invalid Channel Width");
		break;
	}
	/* Fetch the center channel based on the channel width */
	wider_bw_ch_switch->newCenterChanFreq0 =
		dfs_csa_ie_req->ch_params.center_freq_seg0;
skip_vht:

	/* Take a wakelock for CSA for 5 seconds and release in vdev start */

	qdf_wake_lock_timeout_acquire(&session_entry->ap_ecsa_wakelock,
				      MAX_WAKELOCK_FOR_CSA);
	qdf_runtime_pm_prevent_suspend(&session_entry->ap_ecsa_runtime_lock);

	/* Send CSA IE request from here */
	lim_send_dfs_chan_sw_ie_update(mac_ctx, session_entry);

	/*
	 * Wait for MAX_WAIT_FOR_BCN_TX_COMPLETE ms for tx complete for beacon.
	 * If tx complete for beacon is received before this timer expire,
	 * stop this timer and then this will be restarted for every beacon
	 * interval until switchCount become 0 and bcn template with new
	 * switchCount will be sent to firmware.
	 * OR
	 * If no tx complete for beacon is recived till this timer expire
	 * this will be restarted for every beacon interval until switchCount
	 * become 0 and bcn template with new switchCount will be sent to
	 * firmware.
	 */
	status = qdf_mc_timer_start(&session_entry->ap_ecsa_timer,
				    MAX_WAIT_FOR_BCN_TX_COMPLETE);
	if (QDF_IS_STATUS_ERROR(status))
		pe_err("cannot start ap_ecsa_timer");

	pe_debug("IE count:%d chan:%d freq %d width:%d wrapper:%d ch_offset:%d",
		 session_entry->gLimChannelSwitch.switchCount,
		 session_entry->gLimChannelSwitch.primaryChannel,
		 session_entry->gLimChannelSwitch.sw_target_freq,
		 session_entry->gLimChannelSwitch.ch_width,
		 session_entry->dfsIncludeChanWrapperIe,
		 session_entry->gLimChannelSwitch.sec_ch_offset);

	/* Send ECSA/CSA Action frame after updating the beacon */
	if (CHAN_HOP_ALL_BANDS_ENABLE &&
	    !WLAN_REG_IS_6GHZ_CHAN_FREQ(target_ch_freq))
		lim_send_chan_switch_action_frame
			(mac_ctx,
			 session_entry->gLimChannelSwitch.primaryChannel,
			 ch_width, session_entry);
	else
		send_extended_chan_switch_action_frame
			(mac_ctx, target_ch_freq, ch_width,
			 session_entry);
}

/**
 * lim_process_ext_change_channel()- function to send ECSA
 * action frame for STA/CLI .
 * @mac_ctx: pointer to global mac structure
 * @msg: params from sme for new channel.
 *
 * This function is called to send ECSA frame for STA/CLI.
 *
 * Return: void
 */

static void lim_process_ext_change_channel(struct mac_context *mac_ctx,
							uint32_t *msg)
{
	struct sir_sme_ext_cng_chan_req *ext_chng_channel =
				(struct sir_sme_ext_cng_chan_req *) msg;
	struct pe_session *session_entry = NULL;
	uint32_t new_ext_chan_freq;

	if (!msg) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}
	session_entry =
		pe_find_session_by_vdev_id(mac_ctx, ext_chng_channel->vdev_id);
	if (!session_entry) {
		pe_err("Session not found for given vdev_id %d",
			ext_chng_channel->vdev_id);
		return;
	}
	if (LIM_IS_AP_ROLE(session_entry)) {
		pe_err("not an STA/CLI session");
		return;
	}
	new_ext_chan_freq =
		wlan_reg_legacy_chan_to_freq(mac_ctx->pdev,
					     ext_chng_channel->new_channel);
	session_entry->gLimChannelSwitch.sec_ch_offset = 0;
	send_extended_chan_switch_action_frame(mac_ctx, new_ext_chan_freq, 0,
					       session_entry);
}

/**
 * lim_nss_update_rsp() - send NSS update response to SME
 * @mac_ctx Pointer to Global MAC structure
 * @vdev_id: vdev id
 * @status: nss update status
 *
 * Return: None
 */
static void lim_nss_update_rsp(struct mac_context *mac_ctx,
			       uint8_t vdev_id, QDF_STATUS status)
{
	struct scheduler_msg msg = {0};
	struct sir_bcn_update_rsp *nss_rsp;
	QDF_STATUS qdf_status;

	nss_rsp = qdf_mem_malloc(sizeof(*nss_rsp));
	if (!nss_rsp)
		return;

	nss_rsp->vdev_id = vdev_id;
	nss_rsp->status = status;
	nss_rsp->reason = REASON_NSS_UPDATE;

	msg.type = eWNI_SME_NSS_UPDATE_RSP;
	msg.bodyptr = nss_rsp;
	msg.bodyval = 0;
	qdf_status = scheduler_post_message(QDF_MODULE_ID_PE, QDF_MODULE_ID_SME,
					    QDF_MODULE_ID_SME, &msg);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		qdf_mem_free(nss_rsp);
}

void lim_send_bcn_rsp(struct mac_context *mac_ctx, tpSendbeaconParams rsp)
{
	if (!rsp) {
		pe_err("rsp is NULL");
		return;
	}

	pe_debug("Send beacon resp status %d for reason %d",
		 rsp->status, rsp->reason);

	if (rsp->reason == REASON_NSS_UPDATE)
		lim_nss_update_rsp(mac_ctx, rsp->vdev_id, rsp->status);
}

/**
 * lim_process_nss_update_request() - process sme nss update req
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: pointer to the SME message buffer
 *
 * This function processes SME request messages from HDD or upper layer
 * application.
 *
 * Return: None
 */
static void lim_process_nss_update_request(struct mac_context *mac_ctx,
		uint32_t *msg_buf)
{
	struct sir_nss_update_request *nss_update_req_ptr;
	struct pe_session *session_entry = NULL;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint8_t vdev_id;

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	nss_update_req_ptr = (struct sir_nss_update_request *)msg_buf;
	vdev_id = nss_update_req_ptr->vdev_id;
	session_entry = pe_find_session_by_vdev_id(mac_ctx,
						   nss_update_req_ptr->vdev_id);
	if (!session_entry) {
		pe_err("Session not found for given session_id %d",
			nss_update_req_ptr->vdev_id);
		goto end;
	}

	if (session_entry->valid && !LIM_IS_AP_ROLE(session_entry)) {
		pe_err("Invalid SystemRole %d",
			GET_LIM_SYSTEM_ROLE(session_entry));
		goto end;
	}

	/* populate nss field in the beacon */
	session_entry->gLimOperatingMode.present = 1;
	session_entry->gLimOperatingMode.rxNSS = nss_update_req_ptr->new_nss;
	session_entry->gLimOperatingMode.chanWidth = session_entry->ch_width;

	if ((nss_update_req_ptr->new_nss == NSS_1x1_MODE) &&
			(session_entry->ch_width > CH_WIDTH_80MHZ))
		session_entry->gLimOperatingMode.chanWidth = CH_WIDTH_80MHZ;
	if (session_entry->gLimOperatingMode.chanWidth <= CH_WIDTH_160MHZ &&
	    nss_update_req_ptr->ch_width <
			session_entry->gLimOperatingMode.chanWidth)
		session_entry->gLimOperatingMode.chanWidth =
			nss_update_req_ptr->ch_width;

	pe_debug("ch width %d Rx NSS %d",
		 session_entry->gLimOperatingMode.chanWidth,
		 session_entry->gLimOperatingMode.rxNSS);

	/* Send nss update request from here */
	status = sch_set_fixed_beacon_fields(mac_ctx, session_entry);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("Unable to set op mode IE in beacon");
		goto end;
	}

	status = lim_send_beacon_ind(mac_ctx, session_entry, REASON_NSS_UPDATE);
	if (QDF_IS_STATUS_SUCCESS(status))
		return;

	pe_err("Unable to send beacon");
end:
	/*
	 * send resp only in case of failure,
	 * success case response will be from wma.
	 */
	lim_nss_update_rsp(mac_ctx, vdev_id, status);
}

/**
 * lim_process_set_ie_req() - process sme set IE request
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: pointer to the SME message buffer
 *
 * This function processes SME request messages from HDD or upper layer
 * application.
 *
 * Return: None
 */
static void lim_process_set_ie_req(struct mac_context *mac_ctx, uint32_t *msg_buf)
{
	struct send_extcap_ie *msg;
	QDF_STATUS status;
	tDot11fIEExtCap extra_ext_cap = {0};
	uint16_t vdev_id;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct mlme_legacy_priv *mlme_priv;
	struct s_ext_cap *p_ext_cap;

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	msg = (struct send_extcap_ie *)msg_buf;
	vdev_id = msg->session_id;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev)
		return;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}

	if (!mlme_priv->connect_info.ext_cap_ie[0])
		goto send_ie;

	lim_update_extcap_struct(mac_ctx, mlme_priv->connect_info.ext_cap_ie,
				 &extra_ext_cap);

	p_ext_cap = (struct s_ext_cap *)extra_ext_cap.bytes;
	if (p_ext_cap->interworking_service)
		p_ext_cap->qos_map = 1;

	extra_ext_cap.num_bytes = lim_compute_ext_cap_ie_length(&extra_ext_cap);

send_ie:
	status = lim_send_ext_cap_ie(mac_ctx, msg->session_id, &extra_ext_cap,
				     true);
	if (QDF_IS_STATUS_ERROR(status))
		pe_err("Unable to send ExtCap to FW");
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}

#ifdef WLAN_FEATURE_11AX_BSS_COLOR

/**
 * obss_color_collision_process_color_disable() - Disable bss color
 * @mac_ctx: Pointer to Global MAC structure
 * @session: pointer to session
 *
 * This function will disbale bss color.
 *
 * Return: None
 */
static void obss_color_collision_process_color_disable(struct mac_context *mac_ctx,
						       struct pe_session *session)
{
	tUpdateBeaconParams beacon_params;

	if (!session) {
		pe_err("Invalid session");
		return;
	}

	if (session->valid && !LIM_IS_AP_ROLE(session)) {
		pe_err("Invalid SystemRole %d",
		       GET_LIM_SYSTEM_ROLE(session));
		return;
	}

	if (session->bss_color_changing == 1) {
		pe_warn("%d: color change in progress", session->smeSessionId);
		/* Continue color collision detection */
		lim_send_obss_color_collision_cfg(mac_ctx, session,
				OBSS_COLOR_COLLISION_DETECTION);
		return;
	}

	if (session->he_op.bss_col_disabled == 1) {
		pe_warn("%d: bss color already disabled",
			session->smeSessionId);
		/* Continue free color detection */
		lim_send_obss_color_collision_cfg(mac_ctx, session,
				OBSS_COLOR_FREE_SLOT_AVAILABLE);
		return;
	}

	qdf_mem_zero(&beacon_params, sizeof(beacon_params));
	beacon_params.paramChangeBitmap |= PARAM_BSS_COLOR_CHANGED;
	session->he_op.bss_col_disabled = 1;
	beacon_params.bss_color_disabled = 1;
	beacon_params.bss_color = session->he_op.bss_color;

	if (sch_set_fixed_beacon_fields(mac_ctx, session) !=
	    QDF_STATUS_SUCCESS) {
		pe_err("Unable to set op mode IE in beacon");
		return;
	}

	lim_send_beacon_params(mac_ctx, &beacon_params, session);
	lim_send_obss_color_collision_cfg(mac_ctx, session,
					  OBSS_COLOR_FREE_SLOT_AVAILABLE);
}

/**
 * obss_color_collision_process_color_change() - Process bss color change
 * @mac_ctx: Pointer to Global MAC structure
 * @session: pointer to session
 * @obss_color_info: obss color collision/free slot indication info
 *
 * This function selects new color ib case of bss color collision.
 *
 * Return: None
 */
static void obss_color_collision_process_color_change(struct mac_context *mac_ctx,
		struct pe_session *session,
		struct wmi_obss_color_collision_info *obss_color_info)
{
	int i, num_bss_color = 0;
	uint32_t bss_color_bitmap;
	uint8_t bss_color_index_array[MAX_BSS_COLOR_VALUE];
	uint32_t rand_byte = 0;
	struct sir_set_he_bss_color he_bss_color;
	bool is_color_collision = false;


	if (session->bss_color_changing == 1) {
		pe_err("%d: color change in progress", session->smeSessionId);
		return;
	}

	if (!session->he_op.bss_col_disabled) {
		if (session->he_op.bss_color < 32)
			is_color_collision = (obss_color_info->
					     obss_color_bitmap_bit0to31 >>
					     session->he_op.bss_color) & 0x01;
		else
			is_color_collision = (obss_color_info->
					     obss_color_bitmap_bit32to63 >>
					     (session->he_op.bss_color -
					      32)) & 0x01;
		if (!is_color_collision) {
			pe_err("%d: color collision not found, curr_color: %d",
			       session->smeSessionId,
			       session->he_op.bss_color);
			return;
		}
	}

	bss_color_bitmap = obss_color_info->obss_color_bitmap_bit0to31;

	/* Skip color zero */
	bss_color_bitmap = bss_color_bitmap >> 1;
	for (i = 0; (i < 31) && (num_bss_color < MAX_BSS_COLOR_VALUE); i++) {
		if (!(bss_color_bitmap & 0x01)) {
			bss_color_index_array[num_bss_color] = i + 1;
			num_bss_color++;
		}
		bss_color_bitmap = bss_color_bitmap >> 1;
	}

	bss_color_bitmap = obss_color_info->obss_color_bitmap_bit32to63;
	for (i = 0; (i < 32) && (num_bss_color < MAX_BSS_COLOR_VALUE); i++) {
		if (!(bss_color_bitmap & 0x01)) {
			bss_color_index_array[num_bss_color] = i + 32;
			num_bss_color++;
		}
		bss_color_bitmap = bss_color_bitmap >> 1;
	}

	if (num_bss_color) {
		qdf_get_random_bytes((void *) &rand_byte, 1);
		i = (rand_byte + qdf_mc_timer_get_system_ticks()) %
		    num_bss_color;
		pe_debug("New bss color = %d", bss_color_index_array[i]);
		he_bss_color.vdev_id = obss_color_info->vdev_id;
		he_bss_color.bss_color = bss_color_index_array[i];
		lim_process_set_he_bss_color(mac_ctx,
					     (uint32_t *)&he_bss_color);
	} else {
		pe_err("Unable to find bss color from bitmasp");
		if (obss_color_info->evt_type ==
		    OBSS_COLOR_FREE_SLOT_TIMER_EXPIRY &&
		    session->obss_color_collision_dec_evt ==
		    OBSS_COLOR_FREE_SLOT_TIMER_EXPIRY)
			/* In dot11BSSColorCollisionAPPeriod and
			 * timer expired, time to disable bss color.
			 */
			obss_color_collision_process_color_disable(mac_ctx,
								   session);
		else
			/*
			 * Enter dot11BSSColorCollisionAPPeriod period.
			 */
			lim_send_obss_color_collision_cfg(mac_ctx, session,
					OBSS_COLOR_FREE_SLOT_TIMER_EXPIRY);
	}
}

void lim_process_set_he_bss_color(struct mac_context *mac_ctx, uint32_t *msg_buf)
{
	struct sir_set_he_bss_color *bss_color;
	struct pe_session *session_entry = NULL;
	tUpdateBeaconParams beacon_params;

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	bss_color = (struct sir_set_he_bss_color *)msg_buf;
	session_entry = pe_find_session_by_vdev_id(mac_ctx, bss_color->vdev_id);
	if (!session_entry) {
		pe_err("Session not found for given vdev_id %d",
			bss_color->vdev_id);
		return;
	}

	if (session_entry->valid && !LIM_IS_AP_ROLE(session_entry)) {
		pe_err("Invalid SystemRole %d",
			GET_LIM_SYSTEM_ROLE(session_entry));
		return;
	}

	if (bss_color->bss_color == session_entry->he_op.bss_color) {
		pe_err("No change in  BSS color, current BSS color %d",
			bss_color->bss_color);
		return;
	}
	qdf_mem_zero(&beacon_params, sizeof(beacon_params));
	beacon_params.paramChangeBitmap |= PARAM_BSS_COLOR_CHANGED;
	session_entry->he_op.bss_col_disabled = 1;
	session_entry->he_bss_color_change.countdown =
		BSS_COLOR_SWITCH_COUNTDOWN;
	session_entry->he_bss_color_change.new_color = bss_color->bss_color;
	beacon_params.bss_color_disabled = 1;
	beacon_params.bss_color = session_entry->he_op.bss_color;
	session_entry->bss_color_changing = 1;

	if (sch_set_fixed_beacon_fields(mac_ctx, session_entry) !=
			QDF_STATUS_SUCCESS) {
		pe_err("Unable to set op mode IE in beacon");
		return;
	}

	lim_send_beacon_params(mac_ctx, &beacon_params, session_entry);
	lim_send_obss_color_collision_cfg(mac_ctx, session_entry,
			OBSS_COLOR_COLLISION_DETECTION_DISABLE);
}

void lim_send_obss_color_collision_cfg(struct mac_context *mac_ctx,
				       struct pe_session *session,
				       enum wmi_obss_color_collision_evt_type
				       event_type)
{
	struct wmi_obss_color_collision_cfg_param *cfg_param;
	struct scheduler_msg msg = {0};

	if (!session) {
		pe_err("Invalid session");
		return;
	}

	if (!session->he_capable ||
	    !session->is_session_obss_color_collision_det_enabled) {
		pe_debug("%d: obss color det not enabled, he_cap:%d, sup:%d:%d",
			 session->smeSessionId, session->he_capable,
			 session->is_session_obss_color_collision_det_enabled,
			 mac_ctx->mlme_cfg->obss_ht40.
			 obss_color_collision_offload_enabled);
		return;
	}

	cfg_param = qdf_mem_malloc(sizeof(*cfg_param));
	if (!cfg_param)
		return;

	pe_debug("%d: sending event:%d", session->smeSessionId, event_type);
	qdf_mem_zero(cfg_param, sizeof(*cfg_param));
	cfg_param->vdev_id = session->smeSessionId;
	cfg_param->evt_type = event_type;
	cfg_param->current_bss_color = session->he_op.bss_color;
	if (LIM_IS_AP_ROLE(session))
		cfg_param->detection_period_ms =
			OBSS_COLOR_COLLISION_DETECTION_AP_PERIOD_MS;
	else
		cfg_param->detection_period_ms =
			OBSS_COLOR_COLLISION_DETECTION_STA_PERIOD_MS;

	cfg_param->scan_period_ms = OBSS_COLOR_COLLISION_SCAN_PERIOD_MS;
	if (event_type == OBSS_COLOR_FREE_SLOT_TIMER_EXPIRY)
		cfg_param->free_slot_expiry_time_ms =
			OBSS_COLOR_COLLISION_FREE_SLOT_EXPIRY_MS;

	msg.type = WMA_OBSS_COLOR_COLLISION_REQ;
	msg.bodyptr = cfg_param;
	msg.reserved = 0;

	if (QDF_IS_STATUS_ERROR(scheduler_post_message(QDF_MODULE_ID_PE,
						       QDF_MODULE_ID_WMA,
						       QDF_MODULE_ID_WMA,
						       &msg))) {
		qdf_mem_free(cfg_param);
	} else {
		session->obss_color_collision_dec_evt = event_type;
	}
}

void lim_process_obss_color_collision_info(struct mac_context *mac_ctx,
					   uint32_t *msg_buf)
{
	struct wmi_obss_color_collision_info *obss_color_info;
	struct pe_session *session;

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	obss_color_info = (struct wmi_obss_color_collision_info *)msg_buf;
	session = pe_find_session_by_vdev_id(mac_ctx, obss_color_info->vdev_id);
	if (!session) {
		pe_err("Session not found for given session_id %d",
			obss_color_info->vdev_id);
		return;
	}

	pe_debug("vdev_id:%d, evt:%d:%d, 0to31:0x%x, 32to63:0x%x, cap:%d:%d:%d",
		 obss_color_info->vdev_id,
		 obss_color_info->evt_type,
		 session->obss_color_collision_dec_evt,
		 obss_color_info->obss_color_bitmap_bit0to31,
		 obss_color_info->obss_color_bitmap_bit32to63,
		 session->he_capable,
		 session->is_session_obss_color_collision_det_enabled,
		 mac_ctx->mlme_cfg->obss_ht40.
		 obss_color_collision_offload_enabled);

	if (!session->he_capable ||
	    !session->is_session_obss_color_collision_det_enabled) {
		return;
	}

	switch (obss_color_info->evt_type) {
	case OBSS_COLOR_COLLISION_DETECTION_DISABLE:
		pe_err("%d: FW disabled obss color det. he_cap:%d, sup:%d:%d",
		       session->smeSessionId, session->he_capable,
		       session->is_session_obss_color_collision_det_enabled,
		       mac_ctx->mlme_cfg->obss_ht40.
		       obss_color_collision_offload_enabled);
		session->is_session_obss_color_collision_det_enabled = false;
		return;
	case OBSS_COLOR_FREE_SLOT_AVAILABLE:
	case OBSS_COLOR_COLLISION_DETECTION:
	case OBSS_COLOR_FREE_SLOT_TIMER_EXPIRY:
		if (session->valid && !LIM_IS_AP_ROLE(session)) {
			pe_debug("Invalid System Role %d",
				 GET_LIM_SYSTEM_ROLE(session));
			return;
		}

		if (session->obss_color_collision_dec_evt !=
		    obss_color_info->evt_type) {
			pe_debug("%d: Wrong event: %d, skiping",
				 obss_color_info->vdev_id,
				 obss_color_info->evt_type);
			return;
		}
		obss_color_collision_process_color_change(mac_ctx, session,
							  obss_color_info);
		break;
	default:
		pe_err("%d: Invalid event type %d",
		       obss_color_info->vdev_id, obss_color_info->evt_type);
		return;
	}
}
#endif

void lim_send_csa_restart_req(struct mac_context *mac_ctx, uint8_t vdev_id)
{
	struct pe_session *session;

	session = pe_find_session_by_vdev_id(mac_ctx, vdev_id);
	if (!session) {
		pe_err("session not found for vdev id %d", vdev_id);
		return;
	}

	wlan_vdev_mlme_sm_deliver_evt(session->vdev,
				      WLAN_VDEV_SM_EV_CSA_RESTART,
				      sizeof(*session), session);
}

void lim_continue_sta_csa_req(struct mac_context *mac_ctx, uint8_t vdev_id)
{
	pe_info("Continue CSA for STA vdev id %d", vdev_id);
	lim_process_channel_switch(mac_ctx, vdev_id);
}

void lim_add_roam_blacklist_ap(struct mac_context *mac_ctx,
			       struct roam_blacklist_event *src_lst)
{
	uint32_t i;
	struct sir_rssi_disallow_lst entry;
	struct roam_blacklist_timeout *blacklist;

	pe_debug("Received Blacklist event from FW num entries %d",
		 src_lst->num_entries);
	blacklist = &src_lst->roam_blacklist[0];
	for (i = 0; i < src_lst->num_entries; i++) {

		entry.bssid = blacklist->bssid;
		entry.time_during_rejection = blacklist->received_time;
		entry.reject_reason = blacklist->reject_reason;
		entry.source = blacklist->source ? blacklist->source :
						   ADDED_BY_TARGET;
		entry.original_timeout = blacklist->original_timeout;
		entry.received_time = blacklist->received_time;
		/* If timeout = 0 and rssi = 0 ignore the entry */
		if (!blacklist->timeout && !blacklist->rssi) {
			continue;
		} else if (blacklist->timeout) {
			entry.retry_delay = blacklist->timeout;
			/* set 0dbm as expected rssi */
			entry.expected_rssi = LIM_MIN_RSSI;
		} else {
			/* blacklist timeout as 0 */
			entry.retry_delay = blacklist->timeout;
			entry.expected_rssi = blacklist->rssi;
		}

		/* Add this bssid to the rssi reject ap type in blacklist mgr */
		lim_add_bssid_to_reject_list(mac_ctx->pdev, &entry);
		blacklist++;
	}
}
