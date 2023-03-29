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
 * This file lim_utils.cc contains the utility functions
 * LIM uses.
 * Author:        Chandra Modumudi
 * Date:          02/13/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#include "sch_api.h"
#include "lim_utils.h"
#include "lim_types.h"
#include "lim_security_utils.h"
#include "lim_prop_exts_utils.h"
#include "lim_send_messages.h"
#include "lim_ser_des_utils.h"
#include "lim_admit_control.h"
#include "dot11f.h"
#include "dot11fdefs.h"
#include "wmm_apsd.h"
#include "lim_trace.h"

#ifdef FEATURE_WLAN_DIAG_SUPPORT
#include "host_diag_core_event.h"
#endif /* FEATURE_WLAN_DIAG_SUPPORT */
#include "lim_session_utils.h"
#include "lim_ft_defs.h"
#include "lim_session.h"
#include "cds_reg_service.h"
#include "nan_datapath.h"
#include "wma.h"
#include "wlan_reg_services_api.h"
#include "wlan_policy_mgr_api.h"
#include "wlan_mlme_public_struct.h"
#include "wlan_mlme_ucfg_api.h"
#ifdef WLAN_FEATURE_11AX_BSS_COLOR
#include "wma_he.h"
#endif
#include "wlan_utility.h"

#ifdef WLAN_FEATURE_11W
#include "wni_cfg.h"
#endif
#include "cfg_mlme_obss_ht40.h"
#include "cfg_ucfg_api.h"
#include "lim_ft.h"
#include "wlan_mlme_main.h"
#include "qdf_util.h"
#include "wlan_qct_sys.h"
#include <wlan_scan_ucfg_api.h>
#include <wlan_blm_api.h>
#include <lim_assoc_utils.h>
#include "wlan_mlme_ucfg_api.h"
#include "nan_ucfg_api.h"

/** -------------------------------------------------------------
   \fn lim_delete_dialogue_token_list
   \brief deletes the complete lim dialogue token linked list.
   \param     struct mac_context *   mac
   \return     None
   -------------------------------------------------------------*/
void lim_delete_dialogue_token_list(struct mac_context *mac)
{
	tpDialogueToken pCurrNode = mac->lim.pDialogueTokenHead;

	while (mac->lim.pDialogueTokenHead) {
		pCurrNode = mac->lim.pDialogueTokenHead;
		mac->lim.pDialogueTokenHead =
			mac->lim.pDialogueTokenHead->next;
		qdf_mem_free(pCurrNode);
		pCurrNode = NULL;
	}
	mac->lim.pDialogueTokenTail = NULL;
}

char *lim_mlm_state_str(tLimMlmStates state)
{
	switch (state) {
	case eLIM_MLM_OFFLINE_STATE:
		return "eLIM_MLM_OFFLINE_STATE";
	case eLIM_MLM_IDLE_STATE:
		return "eLIM_MLM_IDLE_STATE";
	case eLIM_MLM_WT_JOIN_BEACON_STATE:
		return "eLIM_MLM_WT_JOIN_BEACON_STATE";
	case eLIM_MLM_JOINED_STATE:
		return "eLIM_MLM_JOINED_STATE";
	case eLIM_MLM_BSS_STARTED_STATE:
		return "eLIM_MLM_BSS_STARTED_STATE";
	case eLIM_MLM_WT_AUTH_FRAME2_STATE:
		return "eLIM_MLM_WT_AUTH_FRAME2_STATE";
	case eLIM_MLM_WT_AUTH_FRAME3_STATE:
		return "eLIM_MLM_WT_AUTH_FRAME3_STATE";
	case eLIM_MLM_WT_AUTH_FRAME4_STATE:
		return "eLIM_MLM_WT_AUTH_FRAME4_STATE";
	case eLIM_MLM_AUTH_RSP_TIMEOUT_STATE:
		return "eLIM_MLM_AUTH_RSP_TIMEOUT_STATE";
	case eLIM_MLM_AUTHENTICATED_STATE:
		return "eLIM_MLM_AUTHENTICATED_STATE";
	case eLIM_MLM_WT_ASSOC_RSP_STATE:
		return "eLIM_MLM_WT_ASSOC_RSP_STATE";
	case eLIM_MLM_WT_REASSOC_RSP_STATE:
		return "eLIM_MLM_WT_REASSOC_RSP_STATE";
	case eLIM_MLM_WT_FT_REASSOC_RSP_STATE:
		return "eLIM_MLM_WT_FT_REASSOC_RSP_STATE";
	case eLIM_MLM_WT_DEL_STA_RSP_STATE:
		return "eLIM_MLM_WT_DEL_STA_RSP_STATE";
	case eLIM_MLM_WT_DEL_BSS_RSP_STATE:
		return "eLIM_MLM_WT_DEL_BSS_RSP_STATE";
	case eLIM_MLM_WT_ADD_STA_RSP_STATE:
		return "eLIM_MLM_WT_ADD_STA_RSP_STATE";
	case eLIM_MLM_WT_ADD_BSS_RSP_STATE:
		return "eLIM_MLM_WT_ADD_BSS_RSP_STATE";
	case eLIM_MLM_REASSOCIATED_STATE:
		return "eLIM_MLM_REASSOCIATED_STATE";
	case eLIM_MLM_LINK_ESTABLISHED_STATE:
		return "eLIM_MLM_LINK_ESTABLISHED_STATE";
	case eLIM_MLM_WT_ASSOC_CNF_STATE:
		return "eLIM_MLM_WT_ASSOC_CNF_STATE";
	case eLIM_MLM_WT_ADD_BSS_RSP_ASSOC_STATE:
		return "eLIM_MLM_WT_ADD_BSS_RSP_ASSOC_STATE";
	case eLIM_MLM_WT_ADD_BSS_RSP_REASSOC_STATE:
		return "eLIM_MLM_WT_ADD_BSS_RSP_REASSOC_STATE";
	case eLIM_MLM_WT_ADD_BSS_RSP_FT_REASSOC_STATE:
		return "eLIM_MLM_WT_ADD_BSS_RSP_FT_REASSOC_STATE";
	case eLIM_MLM_WT_ASSOC_DEL_STA_RSP_STATE:
		return "eLIM_MLM_WT_ASSOC_DEL_STA_RSP_STATE";
	case eLIM_MLM_WT_SET_BSS_KEY_STATE:
		return "eLIM_MLM_WT_SET_BSS_KEY_STATE";
	case eLIM_MLM_WT_SET_STA_KEY_STATE:
		return "eLIM_MLM_WT_SET_STA_KEY_STATE";
	default:
		return "INVALID MLM state";
	}
}

void
lim_print_mlm_state(struct mac_context *mac, uint16_t logLevel, tLimMlmStates state)
{
	pe_debug("Mlm state: %s", lim_mlm_state_str(state));
}

char *lim_sme_state_str(tLimSmeStates state)
{
	switch (state) {
	case eLIM_SME_OFFLINE_STATE:
		return "eLIM_SME_OFFLINE_STATE";
	case  eLIM_SME_IDLE_STATE:
		return "eLIM_SME_OFFLINE_STATE";
	case eLIM_SME_SUSPEND_STATE:
		return "eLIM_SME_SUSPEND_STATE";
	case eLIM_SME_WT_JOIN_STATE:
		return "eLIM_SME_WT_JOIN_STATE";
	case eLIM_SME_WT_AUTH_STATE:
		return "eLIM_SME_WT_AUTH_STATE";
	case eLIM_SME_WT_ASSOC_STATE:
		return "eLIM_SME_WT_ASSOC_STATE";
	case eLIM_SME_WT_REASSOC_STATE:
		return "eLIM_SME_WT_REASSOC_STATE";
	case eLIM_SME_JOIN_FAILURE_STATE:
		return "eLIM_SME_JOIN_FAILURE_STATE";
	case eLIM_SME_ASSOCIATED_STATE:
		return "eLIM_SME_ASSOCIATED_STATE";
	case eLIM_SME_REASSOCIATED_STATE:
		return "eLIM_SME_REASSOCIATED_STATE";
	case eLIM_SME_LINK_EST_STATE:
		return "eLIM_SME_LINK_EST_STATE";
	case eLIM_SME_WT_PRE_AUTH_STATE:
		return "eLIM_SME_WT_PRE_AUTH_STATE";
	case eLIM_SME_WT_DISASSOC_STATE:
		return "eLIM_SME_WT_DISASSOC_STATE";
	case eLIM_SME_WT_DEAUTH_STATE:
		return "eLIM_SME_WT_DEAUTH_STATE";
	case eLIM_SME_WT_START_BSS_STATE:
		return "eLIM_SME_WT_START_BSS_STATE";
	case eLIM_SME_WT_STOP_BSS_STATE:
		return "eLIM_SME_WT_STOP_BSS_STATE";
	case eLIM_SME_NORMAL_STATE:
		return "eLIM_SME_NORMAL_STATE";
	default:
		return "INVALID SME STATE";
	}
}

void
lim_print_sme_state(struct mac_context *mac, uint16_t logLevel, tLimSmeStates state)
{
	pe_debug("SME state: %s", lim_sme_state_str(state));
}

char *lim_msg_str(uint32_t msgType)
{
	switch (msgType) {
	case eWNI_SME_SYS_READY_IND:
		return "eWNI_SME_SYS_READY_IND";
	case eWNI_SME_JOIN_REQ:
		return "eWNI_SME_JOIN_REQ";
	case eWNI_SME_JOIN_RSP:
		return "eWNI_SME_JOIN_RSP";
	case eWNI_SME_SETCONTEXT_RSP:
		return "eWNI_SME_SETCONTEXT_RSP";
	case eWNI_SME_REASSOC_REQ:
		return "eWNI_SME_REASSOC_REQ";
	case eWNI_SME_REASSOC_RSP:
		return "eWNI_SME_REASSOC_RSP";
	case eWNI_SME_DISASSOC_REQ:
		return "eWNI_SME_DISASSOC_REQ";
	case eWNI_SME_DISASSOC_RSP:
		return "eWNI_SME_DISASSOC_RSP";
	case eWNI_SME_DISASSOC_IND:
		return "eWNI_SME_DISASSOC_IND";
	case eWNI_SME_DISASSOC_CNF:
		return "eWNI_SME_DISASSOC_CNF";
	case eWNI_SME_DEAUTH_REQ:
		return "eWNI_SME_DEAUTH_REQ";
	case eWNI_SME_DEAUTH_RSP:
		return "eWNI_SME_DEAUTH_RSP";
	case eWNI_SME_DEAUTH_IND:
		return "eWNI_SME_DEAUTH_IND";
	case eWNI_SME_WM_STATUS_CHANGE_NTF:
		return "eWNI_SME_WM_STATUS_CHANGE_NTF";
	case eWNI_SME_START_BSS_REQ:
		return "eWNI_SME_START_BSS_REQ";
	case eWNI_SME_START_BSS_RSP:
		return "eWNI_SME_START_BSS_RSP";
	case eWNI_SME_ASSOC_IND:
		return "eWNI_SME_ASSOC_IND";
	case eWNI_SME_ASSOC_IND_UPPER_LAYER:
		return "eWNI_SME_ASSOC_IND_UPPER_LAYER";
	case eWNI_SME_ASSOC_CNF:
		return "eWNI_SME_ASSOC_CNF";
	case eWNI_SME_SWITCH_CHL_IND:
		return "eWNI_SME_SWITCH_CHL_IND";
	case eWNI_SME_STOP_BSS_REQ:
		return "eWNI_SME_STOP_BSS_REQ";
	case eWNI_SME_STOP_BSS_RSP:
		return "eWNI_SME_STOP_BSS_RSP";
	case eWNI_SME_DEAUTH_CNF:
		return "eWNI_SME_DEAUTH_CNF";
	case eWNI_SME_ADDTS_REQ:
		return "eWNI_SME_ADDTS_REQ";
	case eWNI_SME_MSCS_REQ:
		return "eWNI_SME_MSCS_REQ";
	case eWNI_SME_ADDTS_RSP:
		return "eWNI_SME_ADDTS_RSP";
	case eWNI_SME_DELTS_REQ:
		return "eWNI_SME_DELTS_REQ";
	case eWNI_SME_DELTS_RSP:
		return "eWNI_SME_DELTS_RSP";
	case eWNI_SME_DELTS_IND:
		return "eWNI_SME_DELTS_IND";
	case SIR_BB_XPORT_MGMT_MSG:
		return "SIR_BB_XPORT_MGMT_MSG";
	case SIR_LIM_JOIN_FAIL_TIMEOUT:
		return "SIR_LIM_JOIN_FAIL_TIMEOUT";
	case SIR_LIM_AUTH_FAIL_TIMEOUT:
		return "SIR_LIM_AUTH_FAIL_TIMEOUT";
	case SIR_LIM_AUTH_RSP_TIMEOUT:
		return "SIR_LIM_AUTH_RSP_TIMEOUT";
	case SIR_LIM_ASSOC_FAIL_TIMEOUT:
		return "SIR_LIM_ASSOC_FAIL_TIMEOUT";
	case SIR_LIM_REASSOC_FAIL_TIMEOUT:
		return "SIR_LIM_REASSOC_FAIL_TIMEOUT";
	case SIR_LIM_HEART_BEAT_TIMEOUT:
		return "SIR_LIM_HEART_BEAT_TIMEOUT";
	case SIR_LIM_ADDTS_RSP_TIMEOUT:
		return "SIR_LIM_ADDTS_RSP_TIMEOUT";
	case SIR_LIM_LINK_TEST_DURATION_TIMEOUT:
		return "SIR_LIM_LINK_TEST_DURATION_TIMEOUT";
	case SIR_LIM_UPDATE_OLBC_CACHEL_TIMEOUT:
		return "SIR_LIM_UPDATE_OLBC_CACHEL_TIMEOUT";
	case SIR_LIM_CNF_WAIT_TIMEOUT:
		return "SIR_LIM_CNF_WAIT_TIMEOUT";
	case SIR_LIM_FT_PREAUTH_RSP_TIMEOUT:
		return "SIR_LIM_FT_PREAUTH_RSP_TIMEOUT";
#ifdef FEATURE_WLAN_ESE
	case eWNI_SME_GET_TSM_STATS_REQ:
		return "eWNI_SME_GET_TSM_STATS_REQ";
	case eWNI_SME_GET_TSM_STATS_RSP:
		return "eWNI_SME_GET_TSM_STATS_RSP";
#endif /* FEATURE_WLAN_ESE */
	case eWNI_SME_SET_HW_MODE_REQ:
		return "eWNI_SME_SET_HW_MODE_REQ";
	case eWNI_SME_SET_HW_MODE_RESP:
		return "eWNI_SME_SET_HW_MODE_RESP";
	case eWNI_SME_HW_MODE_TRANS_IND:
		return "eWNI_SME_HW_MODE_TRANS_IND";
	case SIR_LIM_PROCESS_DEFERRED_QUEUE:
		return "SIR_LIM_PROCESS_DEFERRED_QUEUE";
	default:
		return "Unknown";
	}
}

char *lim_result_code_str(tSirResultCodes resultCode)
{
	switch (resultCode) {
	case eSIR_SME_SUCCESS:
		return "eSIR_SME_SUCCESS";
	case eSIR_LOGE_EXCEPTION:
		return "eSIR_LOGE_EXCEPTION";
	case eSIR_SME_INVALID_PARAMETERS:
		return "eSIR_SME_INVALID_PARAMETERS";
	case eSIR_SME_UNEXPECTED_REQ_RESULT_CODE:
		return "eSIR_SME_UNEXPECTED_REQ_RESULT_CODE";
	case eSIR_SME_RESOURCES_UNAVAILABLE:
		return "eSIR_SME_RESOURCES_UNAVAILABLE";
	case eSIR_SME_SCAN_FAILED:
		return "eSIR_SME_SCAN_FAILED";
	case eSIR_SME_BSS_ALREADY_STARTED_OR_JOINED:
		return "eSIR_SME_BSS_ALREADY_STARTED_OR_JOINED";
	case eSIR_SME_LOST_LINK_WITH_PEER_RESULT_CODE:
		return "eSIR_SME_LOST_LINK_WITH_PEER_RESULT_CODE";
	case eSIR_SME_REFUSED:
		return "eSIR_SME_REFUSED";
	case eSIR_SME_JOIN_TIMEOUT_RESULT_CODE:
		return "eSIR_SME_JOIN_TIMEOUT_RESULT_CODE";
	case eSIR_SME_AUTH_TIMEOUT_RESULT_CODE:
		return "eSIR_SME_AUTH_TIMEOUT_RESULT_CODE";
	case eSIR_SME_ASSOC_TIMEOUT_RESULT_CODE:
		return "eSIR_SME_ASSOC_TIMEOUT_RESULT_CODE";
	case eSIR_SME_REASSOC_TIMEOUT_RESULT_CODE:
		return "eSIR_SME_REASSOC_TIMEOUT_RESULT_CODE";
	case eSIR_SME_MAX_NUM_OF_PRE_AUTH_REACHED:
		return "eSIR_SME_MAX_NUM_OF_PRE_AUTH_REACHED";
	case eSIR_SME_AUTH_REFUSED:
		return "eSIR_SME_AUTH_REFUSED";
	case eSIR_SME_INVALID_WEP_DEFAULT_KEY:
		return "eSIR_SME_INVALID_WEP_DEFAULT_KEY";
	case eSIR_SME_ASSOC_REFUSED:
		return "eSIR_SME_ASSOC_REFUSED";
	case eSIR_SME_REASSOC_REFUSED:
		return "eSIR_SME_REASSOC_REFUSED";
	case eSIR_SME_STA_NOT_AUTHENTICATED:
		return "eSIR_SME_STA_NOT_AUTHENTICATED";
	case eSIR_SME_STA_NOT_ASSOCIATED:
		return "eSIR_SME_STA_NOT_ASSOCIATED";
	case eSIR_SME_ALREADY_JOINED_A_BSS:
		return "eSIR_SME_ALREADY_JOINED_A_BSS";
	case eSIR_SME_MORE_SCAN_RESULTS_FOLLOW:
		return "eSIR_SME_MORE_SCAN_RESULTS_FOLLOW";
	case eSIR_SME_INVALID_ASSOC_RSP_RXED:
		return "eSIR_SME_INVALID_ASSOC_RSP_RXED";
	case eSIR_SME_MIC_COUNTER_MEASURES:
		return "eSIR_SME_MIC_COUNTER_MEASURES";
	case eSIR_SME_ADDTS_RSP_TIMEOUT:
		return "eSIR_SME_ADDTS_RSP_TIMEOUT";
	case eSIR_SME_CHANNEL_SWITCH_FAIL:
		return "eSIR_SME_CHANNEL_SWITCH_FAIL";
	case eSIR_SME_HAL_SCAN_INIT_FAILED:
		return "eSIR_SME_HAL_SCAN_INIT_FAILED";
	case eSIR_SME_HAL_SCAN_END_FAILED:
		return "eSIR_SME_HAL_SCAN_END_FAILED";
	case eSIR_SME_HAL_SCAN_FINISH_FAILED:
		return "eSIR_SME_HAL_SCAN_FINISH_FAILED";
	case eSIR_SME_HAL_SEND_MESSAGE_FAIL:
		return "eSIR_SME_HAL_SEND_MESSAGE_FAIL";

	default:
		return "Unknown resultCode";
	}
}

void lim_print_msg_name(struct mac_context *mac, uint16_t logLevel, uint32_t msgType)
{
	pe_debug("Msg: %s", lim_msg_str(msgType));
}

/**
 * lim_init_mlm() -  This function is called by limProcessSmeMessages() to
 * initialize MLM state machine on STA
 * @mac: Pointer to Global MAC structure
 *
 * @Return: Status of operation
 */
QDF_STATUS lim_init_mlm(struct mac_context *mac)
{
	uint32_t retVal;

	mac->lim.gLimTimersCreated = 0;

	MTRACE(mac_trace(mac, TRACE_CODE_MLM_STATE, NO_SESSION,
			  mac->lim.gLimMlmState));

	/* Initialize number of pre-auth contexts */
	mac->lim.gLimNumPreAuthContexts = 0;

	/* Initialize MAC based Authentication STA list */
	lim_init_pre_auth_list(mac);

	/* Create timers used by LIM */
	retVal = lim_create_timers(mac);
	if (retVal != TX_SUCCESS) {
		pe_err("lim_create_timers Failed");
		return QDF_STATUS_SUCCESS;
	}

	mac->lim.gLimTimersCreated = 1;
	return QDF_STATUS_SUCCESS;
} /*** end lim_init_mlm() ***/

void lim_deactivate_timers(struct mac_context *mac_ctx)
{
	uint32_t n;
	tLimTimers *lim_timer = &mac_ctx->lim.lim_timers;

	lim_deactivate_timers_host_roam(mac_ctx);

	/* Deactivate addts response timer. */
	tx_timer_deactivate(&lim_timer->gLimAddtsRspTimer);

	if (tx_timer_running(&lim_timer->gLimJoinFailureTimer)) {
		pe_err("Join failure timer running call the timeout API");
		/* Cleanup as if join timer expired */
		lim_timer_handler(mac_ctx, SIR_LIM_JOIN_FAIL_TIMEOUT);
	}
	/* Deactivate Join failure timer. */
	tx_timer_deactivate(&lim_timer->gLimJoinFailureTimer);

	/* Deactivate Periodic Join Probe Request timer. */
	tx_timer_deactivate(&lim_timer->gLimPeriodicJoinProbeReqTimer);

	/* Deactivate Auth Retry timer. */
	tx_timer_deactivate
			(&lim_timer->g_lim_periodic_auth_retry_timer);

	if (tx_timer_running(&lim_timer->gLimAssocFailureTimer)) {
		pe_err("Assoc failure timer running call the timeout API");
		/* Cleanup as if assoc timer expired */
		lim_assoc_failure_timer_handler(mac_ctx, LIM_ASSOC);
	}
	/* Deactivate Association failure timer. */
	tx_timer_deactivate(&lim_timer->gLimAssocFailureTimer);

	if (tx_timer_running(&mac_ctx->lim.lim_timers.gLimAuthFailureTimer)) {
		pe_err("Auth failure timer running call the timeout API");
		/* Cleanup as if auth timer expired */
		lim_timer_handler(mac_ctx, SIR_LIM_AUTH_FAIL_TIMEOUT);
	}
	/* Deactivate Authentication failure timer. */
	tx_timer_deactivate(&lim_timer->gLimAuthFailureTimer);

	/* Deactivate wait-for-probe-after-Heartbeat timer. */
	tx_timer_deactivate(&lim_timer->gLimProbeAfterHBTimer);

	/* Deactivate cnf wait timer */
	for (n = 0; n < (mac_ctx->lim.maxStation + 1); n++) {
		tx_timer_deactivate(&lim_timer->gpLimCnfWaitTimer[n]);
	}

	/* Deactivate any Authentication response timers */
	lim_delete_pre_auth_list(mac_ctx);

	tx_timer_deactivate(&lim_timer->gLimUpdateOlbcCacheTimer);
	tx_timer_deactivate(&lim_timer->gLimPreAuthClnupTimer);

	if (tx_timer_running(&lim_timer->gLimDisassocAckTimer)) {
		pe_err("Disassoc timer running call the timeout API");
		lim_timer_handler(mac_ctx, SIR_LIM_DISASSOC_ACK_TIMEOUT);
	}
	tx_timer_deactivate(&lim_timer->gLimDisassocAckTimer);

	if (tx_timer_running(&lim_timer->gLimDeauthAckTimer)) {
		pe_err("Deauth timer running call the timeout API");
		lim_timer_handler(mac_ctx, SIR_LIM_DEAUTH_ACK_TIMEOUT);
	}
	tx_timer_deactivate(&lim_timer->gLimDeauthAckTimer);

	if (tx_timer_running(&lim_timer->sae_auth_timer)) {
		pe_err("SAE Auth failure timer running call the timeout API");
		/* Cleanup as if SAE auth timer expired */
		lim_timer_handler(mac_ctx, SIR_LIM_AUTH_SAE_TIMEOUT);
	}

	tx_timer_deactivate(&lim_timer->sae_auth_timer);
}

void lim_deactivate_timers_for_vdev(struct mac_context *mac_ctx,
				    uint8_t vdev_id)
{
	tLimTimers *lim_timer = &mac_ctx->lim.lim_timers;
	struct pe_session *pe_session;

	pe_session = pe_find_session_by_vdev_id(mac_ctx, vdev_id);
	if (!pe_session) {
		pe_err("pe session invalid for vdev %d", vdev_id);
		return;
	}
	pe_debug("pe limMlmState %s vdev %d",
		 lim_mlm_state_str(pe_session->limMlmState),
		 vdev_id);
	switch (pe_session->limMlmState) {
	case eLIM_MLM_WT_JOIN_BEACON_STATE:
		if (tx_timer_running(
				&lim_timer->gLimJoinFailureTimer)) {
			pe_debug("Trigger Join failure timeout for vdev %d",
				 vdev_id);
			tx_timer_deactivate(
				&lim_timer->gLimJoinFailureTimer);
			lim_process_join_failure_timeout(mac_ctx);
		}
		break;
	case eLIM_MLM_WT_AUTH_FRAME2_STATE:
	case eLIM_MLM_WT_AUTH_FRAME4_STATE:
		if (tx_timer_running(
				&lim_timer->gLimAuthFailureTimer)) {
			pe_debug("Trigger Auth failure timeout for vdev %d",
				 vdev_id);
			tx_timer_deactivate(
				&lim_timer->gLimAuthFailureTimer);
			lim_process_auth_failure_timeout(mac_ctx);

		}
		break;
	case eLIM_MLM_WT_ASSOC_RSP_STATE:
		if (tx_timer_running(
				&lim_timer->gLimAssocFailureTimer)) {
			pe_debug("Trigger Assoc failure timeout for vdev %d",
				 vdev_id);
			tx_timer_deactivate(
				&lim_timer->gLimAssocFailureTimer);
			lim_process_assoc_failure_timeout(mac_ctx,
							  LIM_ASSOC);
		}
		break;
	case eLIM_MLM_WT_SAE_AUTH_STATE:
		if (tx_timer_running(&lim_timer->sae_auth_timer)) {
			pe_debug("Trigger SAE Auth failure timeout for vdev %d",
				 vdev_id);
			tx_timer_deactivate(
				&lim_timer->sae_auth_timer);
			lim_process_sae_auth_timeout(mac_ctx);
		}
		break;
	default:
		return;
	}
}


/**
 * lim_cleanup_mlm() - This function is called to cleanup
 * @mac_ctx: Pointer to Global MAC structure
 *
 * Function is called to cleanup any resources allocated by the  MLM
 * state machine.
 *
 * Return: none
 */
void lim_cleanup_mlm(struct mac_context *mac_ctx)
{
	uint32_t n;

	tLimPreAuthNode **pAuthNode;
	tLimTimers *lim_timer = NULL;

	if (mac_ctx->lim.gLimTimersCreated == 1) {
		lim_timer = &mac_ctx->lim.lim_timers;

		lim_deactivate_timers(mac_ctx);

		lim_delete_timers_host_roam(mac_ctx);

		/* Delete addts response timer. */
		tx_timer_delete(&lim_timer->gLimAddtsRspTimer);

		/* Delete Join failure timer. */
		tx_timer_delete(&lim_timer->gLimJoinFailureTimer);

		/* Delete Periodic Join Probe Request timer. */
		tx_timer_delete(&lim_timer->gLimPeriodicJoinProbeReqTimer);

		/* Delete Auth Retry timer. */
		tx_timer_delete(&lim_timer->g_lim_periodic_auth_retry_timer);

		/* Delete Association failure timer. */
		tx_timer_delete(&lim_timer->gLimAssocFailureTimer);

		/* Delete Authentication failure timer. */
		tx_timer_delete(&lim_timer->gLimAuthFailureTimer);

		/* Delete wait-for-probe-after-Heartbeat timer. */
		tx_timer_delete(&lim_timer->gLimProbeAfterHBTimer);

		/* Delete cnf wait timer */
		for (n = 0; n < (mac_ctx->lim.maxStation + 1); n++) {
			tx_timer_delete(&lim_timer->gpLimCnfWaitTimer[n]);
		}

		pAuthNode = mac_ctx->lim.gLimPreAuthTimerTable.pTable;

		/* Delete any Auth rsp timers, which might have been started */
		for (n = 0; n < mac_ctx->lim.gLimPreAuthTimerTable.numEntry;
				n++)
			tx_timer_delete(&pAuthNode[n]->timer);

		tx_timer_delete(&lim_timer->gLimUpdateOlbcCacheTimer);
		tx_timer_delete(&lim_timer->gLimPreAuthClnupTimer);

		tx_timer_delete(&lim_timer->gLimDisassocAckTimer);

		tx_timer_delete(&lim_timer->gLimDeauthAckTimer);

		tx_timer_delete(&lim_timer->sae_auth_timer);

		mac_ctx->lim.gLimTimersCreated = 0;
	}
} /*** end lim_cleanup_mlm() ***/

/**
 * lim_print_mac_addr()
 *
 ***FUNCTION:
 * This function is called to print passed MAC address
 * in : format.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * @param  macAddr  - MacAddr to be printed
 * @param  logLevel - Loglevel to be used
 *
 * @return None.
 */

void lim_print_mac_addr(struct mac_context *mac, tSirMacAddr macAddr, uint8_t logLevel)
{
	pe_debug(QDF_MAC_ADDR_FMT, QDF_MAC_ADDR_REF(macAddr));
} /****** end lim_print_mac_addr() ******/

/*
 * lim_reset_deferred_msg_q()
 *
 ***FUNCTION:
 * This function resets the deferred message queue parameters.
 *
 ***PARAMS:
 * @param mac     - Pointer to Global MAC structure
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * NA
 *
 ***RETURNS:
 * None
 */

void lim_reset_deferred_msg_q(struct mac_context *mac)
{
	struct scheduler_msg *read_msg = {0};

	if (mac->lim.gLimDeferredMsgQ.size > 0) {
		while ((read_msg = lim_read_deferred_msg_q(mac)) != NULL) {
			pe_free_msg(mac, read_msg);
		}
	}

	mac->lim.gLimDeferredMsgQ.size =
		mac->lim.gLimDeferredMsgQ.write =
			mac->lim.gLimDeferredMsgQ.read = 0;

}

#define LIM_DEFERRED_Q_CHECK_THRESHOLD  (MAX_DEFERRED_QUEUE_LEN/2)
#define LIM_MAX_NUM_MGMT_FRAME_DEFERRED (MAX_DEFERRED_QUEUE_LEN/2)

/**
 * lim_write_deferred_msg_q() - This function queues up a deferred message
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @lim_msg: a LIM message
 *
 * Function queues up a deferred message for later processing on the
 * STA side.
 *
 * Return: none
 */

uint8_t lim_write_deferred_msg_q(struct mac_context *mac_ctx,
				 struct scheduler_msg *lim_msg)
{
	uint8_t type = 0, subtype = 0;

	pe_debug("Queue a deferred message size: %d write: %d - type: 0x%x",
		mac_ctx->lim.gLimDeferredMsgQ.size,
		mac_ctx->lim.gLimDeferredMsgQ.write,
		lim_msg->type);

	/* check if the deferred message queue is full */
	if (mac_ctx->lim.gLimDeferredMsgQ.size >= MAX_DEFERRED_QUEUE_LEN) {
		if (!(mac_ctx->lim.deferredMsgCnt & 0xF)) {
			pe_err("queue->MsgQ full Msg: %d Msgs Failed: %d",
				lim_msg->type,
				++mac_ctx->lim.deferredMsgCnt);
			cds_flush_logs(WLAN_LOG_TYPE_NON_FATAL,
				WLAN_LOG_INDICATOR_HOST_DRIVER,
				WLAN_LOG_REASON_QUEUE_FULL,
				false, false);
		} else {
			mac_ctx->lim.deferredMsgCnt++;
		}
		return TX_QUEUE_FULL;
	}

	/*
	 * In the application, there should not be more than 1 message get
	 * queued up. If happens, flags a warning. In the future, this can
	 * happen.
	 */
	if (mac_ctx->lim.gLimDeferredMsgQ.size > 0)
		pe_debug("%d Deferred Msg type: 0x%x global sme: %d global mlme: %d addts: %d",
			mac_ctx->lim.gLimDeferredMsgQ.size,
			lim_msg->type,
			mac_ctx->lim.gLimSmeState,
			mac_ctx->lim.gLimMlmState,
			mac_ctx->lim.gLimAddtsSent);

	if (SIR_BB_XPORT_MGMT_MSG == lim_msg->type) {
		lim_util_get_type_subtype(lim_msg->bodyptr,
					&type, &subtype);
		pe_debug(" Deferred management type %d subtype %d ",
			type, subtype);
	}

	/*
	 * To prevent the deferred Q is full of management frames, only give
	 * them certain space
	 */
	if ((SIR_BB_XPORT_MGMT_MSG == lim_msg->type) &&
		(LIM_DEFERRED_Q_CHECK_THRESHOLD <
			mac_ctx->lim.gLimDeferredMsgQ.size)) {
		uint16_t idx, count = 0;

		for (idx = 0; idx < mac_ctx->lim.gLimDeferredMsgQ.size;
								idx++) {
			if (SIR_BB_XPORT_MGMT_MSG ==
					mac_ctx->lim.gLimDeferredMsgQ.
						deferredQueue[idx].type) {
				count++;
			}
		}
		if (LIM_MAX_NUM_MGMT_FRAME_DEFERRED < count) {
			/*
			 * We reach the quota for management frames,
			 * drop this one
			 */
			pe_warn("Too many queue->MsgQ Msg: %d count: %d",
				lim_msg->type, count);
			/* Return error, caller knows what to do */
			return TX_QUEUE_FULL;
		}
	}

	++mac_ctx->lim.gLimDeferredMsgQ.size;

	/* reset the count here since we are able to defer the message */
	if (mac_ctx->lim.deferredMsgCnt != 0)
		mac_ctx->lim.deferredMsgCnt = 0;

	/* if the write pointer hits the end of the queue, rewind it */
	if (mac_ctx->lim.gLimDeferredMsgQ.write >= MAX_DEFERRED_QUEUE_LEN)
		mac_ctx->lim.gLimDeferredMsgQ.write = 0;

	/* save the message to the queue and advanced the write pointer */
	qdf_mem_copy((uint8_t *) &mac_ctx->lim.gLimDeferredMsgQ.
			deferredQueue[mac_ctx->lim.gLimDeferredMsgQ.write++],
				(uint8_t *) lim_msg,
				sizeof(struct scheduler_msg));
	return TX_SUCCESS;

}

/*
 * lim_read_deferred_msg_q()
 *
 ***FUNCTION:
 * This function dequeues a deferred message for processing on the
 * STA side.
 *
 ***PARAMS:
 * @param mac     - Pointer to Global MAC structure
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 *
 *
 ***RETURNS:
 * Returns the message at the head of the deferred message queue
 */

struct scheduler_msg *lim_read_deferred_msg_q(struct mac_context *mac)
{
	struct scheduler_msg *msg = {0};

	/*
	** check any messages left. If no, return
	**/
	if (mac->lim.gLimDeferredMsgQ.size <= 0)
		return NULL;

	/*
	** decrement the queue size
	**/
	mac->lim.gLimDeferredMsgQ.size--;

	/*
	** retrieve the message from the head of the queue
	**/
	msg =
		&mac->lim.gLimDeferredMsgQ.deferredQueue[mac->lim.
							  gLimDeferredMsgQ.read];

	/*
	** advance the read pointer
	**/
	mac->lim.gLimDeferredMsgQ.read++;

	/*
	** if the read pointer hits the end of the queue, rewind it
	**/
	if (mac->lim.gLimDeferredMsgQ.read >= MAX_DEFERRED_QUEUE_LEN)
		mac->lim.gLimDeferredMsgQ.read = 0;

	pe_debug("DeQueue a deferred message size: %d read: %d - type: 0x%x",
			mac->lim.gLimDeferredMsgQ.size,
			mac->lim.gLimDeferredMsgQ.read, msg->type);

	pe_debug("DQ msg -- global sme: %d global mlme: %d addts: %d",
		 mac->lim.gLimSmeState, mac->lim.gLimMlmState,
		 mac->lim.gLimAddtsSent);

	return msg;
}

/*
 * lim_handle_update_olbc_cache() - This function update olbc cache
 *
 * @mac_ctx: Pointer to Global MAC structure
 *
 * Function updates olbc cache
 *
 * Return: none
 */
void lim_handle_update_olbc_cache(struct mac_context *mac_ctx)
{
	int i;
	static int enable;
	tUpdateBeaconParams beaconParams;

	struct pe_session *pe_session = lim_is_ap_session_active(mac_ctx);

	if (!pe_session) {
		pe_debug(" Session not found");
		return;
	}

	if (pe_session->is_session_obss_offload_enabled) {
		pe_debug("protection offloaded");
		return;
	}
	qdf_mem_zero((uint8_t *) &beaconParams, sizeof(tUpdateBeaconParams));
	beaconParams.bss_idx = pe_session->vdev_id;

	beaconParams.paramChangeBitmap = 0;
	/*
	 * This is doing a 2 pass check. The first pass is to invalidate
	 * all the cache entries. The second pass is to decide whether to
	 * disable protection.
	 */
	if (!enable) {
		pe_debug("Resetting OLBC cache");
		pe_session->gLimOlbcParams.numSta = 0;
		pe_session->gLimOverlap11gParams.numSta = 0;
		pe_session->gLimOverlapHt20Params.numSta = 0;
		pe_session->gLimNonGfParams.numSta = 0;
		pe_session->gLimLsigTxopParams.numSta = 0;

		for (i = 0; i < LIM_PROT_STA_OVERLAP_CACHE_SIZE; i++)
			mac_ctx->lim.protStaOverlapCache[i].active = false;

		enable = 1;
	} else {
		if ((!pe_session->gLimOlbcParams.numSta) &&
			(pe_session->gLimOlbcParams.protectionEnabled) &&
			(!pe_session->gLim11bParams.protectionEnabled)) {
			pe_debug("Overlap cache clear and no 11B STA set");
			lim_enable11g_protection(mac_ctx, false, true,
						&beaconParams,
						pe_session);
		}

		if ((!pe_session->gLimOverlap11gParams.numSta) &&
			(pe_session->gLimOverlap11gParams.protectionEnabled)
			&& (!pe_session->gLim11gParams.protectionEnabled)) {
			pe_debug("Overlap cache clear and no 11G STA set");
			lim_enable_ht_protection_from11g(mac_ctx, false, true,
							&beaconParams,
							pe_session);
		}

		if ((!pe_session->gLimOverlapHt20Params.numSta) &&
			(pe_session->gLimOverlapHt20Params.protectionEnabled)
			&& (!pe_session->gLimHt20Params.protectionEnabled)) {
			pe_debug("Overlap cache clear and no HT20 STA set");
			lim_enable11g_protection(mac_ctx, false, true,
						&beaconParams,
						pe_session);
		}

		enable = 0;
	}

	if ((false == mac_ctx->sap.SapDfsInfo.is_dfs_cac_timer_running)
					&& beaconParams.paramChangeBitmap) {
		sch_set_fixed_beacon_fields(mac_ctx, pe_session);
		lim_send_beacon_params(mac_ctx, &beaconParams, pe_session);
	}
	/* Start OLBC timer */
	if (tx_timer_activate(&mac_ctx->lim.lim_timers.gLimUpdateOlbcCacheTimer)
						!= TX_SUCCESS)
		pe_err("tx_timer_activate failed");
}

/**
 * lim_is_null_ssid() - This function checks if ssid supplied is Null SSID
 * @ssid: pointer to tSirMacSSid
 *
 * Function checks if ssid supplied is Null SSID
 *
 * Return: none
 */

uint8_t lim_is_null_ssid(tSirMacSSid *ssid)
{
	uint8_t fnull_ssid = false;
	uint32_t ssid_len;
	uint8_t *ssid_str;

	if (0 == ssid->length) {
		fnull_ssid = true;
		return fnull_ssid;
	}
	/* If the first charactes is space, then check if all
	 * characters in SSID are spaces to consider it as NULL SSID
	 */
	if ((ASCII_SPACE_CHARACTER == ssid->ssId[0]) &&
		(ssid->length == 1)) {
			fnull_ssid = true;
			return fnull_ssid;
	} else {
		/* check if all the charactes in SSID are NULL */
		ssid_len = ssid->length;
		ssid_str = ssid->ssId;

		while (ssid_len) {
			if (*ssid_str)
				return fnull_ssid;

			ssid_str++;
			ssid_len--;
		}

		if (0 == ssid_len) {
			fnull_ssid = true;
			return fnull_ssid;
		}
	}

	return fnull_ssid;
}

/** -------------------------------------------------------------
   \fn lim_update_prot_sta_params
   \brief updates protection related counters.
   \param      struct mac_context *   mac
   \param      tSirMacAddr peerMacAddr
   \param      tLimProtStaCacheType protStaCacheType
   \param      tHalBitVal gfSupported
   \param      tHalBitVal lsigTxopSupported
   \return      None
   -------------------------------------------------------------*/
static void
lim_update_prot_sta_params(struct mac_context *mac,
			   tSirMacAddr peerMacAddr,
			   tLimProtStaCacheType protStaCacheType,
			   tHalBitVal gfSupported, tHalBitVal lsigTxopSupported,
			   struct pe_session *pe_session)
{
	uint32_t i;

	pe_debug("Associated STA addr is:");
	lim_print_mac_addr(mac, peerMacAddr, LOGD);

	for (i = 0; i < LIM_PROT_STA_CACHE_SIZE; i++) {
		if (pe_session->protStaCache[i].active) {
			pe_debug("Addr:");
				lim_print_mac_addr
				(mac, pe_session->protStaCache[i].addr,
				LOGD);

			if (!qdf_mem_cmp
				    (pe_session->protStaCache[i].addr,
				    peerMacAddr, sizeof(tSirMacAddr))) {
				pe_debug("matching cache entry at: %d already active",
					i);
				return;
			}
		}
	}

	for (i = 0; i < LIM_PROT_STA_CACHE_SIZE; i++) {
		if (!pe_session->protStaCache[i].active)
			break;
	}

	if (i >= LIM_PROT_STA_CACHE_SIZE) {
		pe_err("No space in ProtStaCache");
		return;
	}

	qdf_mem_copy(pe_session->protStaCache[i].addr,
		     peerMacAddr, sizeof(tSirMacAddr));

	pe_session->protStaCache[i].protStaCacheType = protStaCacheType;
	pe_session->protStaCache[i].active = true;
	if (eLIM_PROT_STA_CACHE_TYPE_llB == protStaCacheType) {
		pe_session->gLim11bParams.numSta++;
		pe_debug("11B,");
	} else if (eLIM_PROT_STA_CACHE_TYPE_llG == protStaCacheType) {
		pe_session->gLim11gParams.numSta++;
		pe_debug("11G,");
	} else if (eLIM_PROT_STA_CACHE_TYPE_HT20 == protStaCacheType) {
		pe_session->gLimHt20Params.numSta++;
		pe_debug("HT20,");
	}

	if (!gfSupported) {
		pe_session->gLimNonGfParams.numSta++;
		pe_debug("NonGf,");
	}
	if (!lsigTxopSupported) {
		pe_session->gLimLsigTxopParams.numSta++;
		pe_debug("!lsigTxopSupported");
	}
} /* --------------------------------------------------------------------- */

/** -------------------------------------------------------------
   \fn lim_decide_ap_protection
   \brief Decides all the protection related staiton coexistence and also sets
 \        short preamble and short slot appropriately. This function will be called
 \        when AP is ready to send assocRsp tp the station joining right now.
   \param      struct mac_context *   mac
   \param      tSirMacAddr peerMacAddr
   \return      None
   -------------------------------------------------------------*/
void
lim_decide_ap_protection(struct mac_context *mac, tSirMacAddr peerMacAddr,
			 tpUpdateBeaconParams pBeaconParams,
			 struct pe_session *pe_session)
{
	uint16_t tmpAid;
	tpDphHashNode sta;
	enum reg_wifi_band rfBand = REG_BAND_UNKNOWN;
	uint32_t phyMode;
	tLimProtStaCacheType protStaCacheType =
		eLIM_PROT_STA_CACHE_TYPE_INVALID;
	tHalBitVal gfSupported = eHAL_SET, lsigTxopSupported = eHAL_SET;

	pBeaconParams->paramChangeBitmap = 0;
	/* check whether to enable protection or not */
	sta =
		dph_lookup_hash_entry(mac, peerMacAddr, &tmpAid,
				      &pe_session->dph.dphHashTable);
	if (!sta) {
		pe_err("sta is NULL");
		return;
	}
	lim_get_rf_band_new(mac, &rfBand, pe_session);
	/* if we are in 5 GHZ band */
	if (REG_BAND_5G == rfBand) {
		/* We are 11N. we need to protect from 11A and Ht20. we don't need any other protection in 5 GHZ. */
		/* HT20 case is common between both the bands and handled down as common code. */
		if (true == pe_session->htCapability) {
			/* we are 11N and 11A station is joining. */
			/* protection from 11A required. */
			if (false == sta->mlmStaContext.htCapability) {
				lim_update_11a_protection(mac, true, false,
							 pBeaconParams,
							 pe_session);
				return;
			}
		}
	} else if (REG_BAND_2G == rfBand) {
		lim_get_phy_mode(mac, &phyMode, pe_session);

		/* We are 11G. Check if we need protection from 11b Stations. */
		if ((phyMode == WNI_CFG_PHY_MODE_11G) &&
		    (false == pe_session->htCapability)) {

			if (sta->erpEnabled == eHAL_CLEAR) {
				protStaCacheType = eLIM_PROT_STA_CACHE_TYPE_llB;
				/* enable protection */
				pe_debug("Enabling protection from 11B");
				lim_enable11g_protection(mac, true, false,
							 pBeaconParams,
							 pe_session);
			}
		}
		/* HT station. */
		if (true == pe_session->htCapability) {
			/* check if we need protection from 11b station */
			if ((sta->erpEnabled == eHAL_CLEAR) &&
			    (!sta->mlmStaContext.htCapability)) {
				protStaCacheType = eLIM_PROT_STA_CACHE_TYPE_llB;
				/* enable protection */
				pe_debug("Enabling protection from 11B");
				lim_enable11g_protection(mac, true, false,
							 pBeaconParams,
							 pe_session);
			}
			/* station being joined is non-11b and non-ht ==> 11g device */
			else if (!sta->mlmStaContext.htCapability) {
				protStaCacheType = eLIM_PROT_STA_CACHE_TYPE_llG;
				/* enable protection */
				lim_enable_ht_protection_from11g(mac, true, false,
								 pBeaconParams,
								 pe_session);
			}
			/* ERP mode is enabled for the latest station joined */
			/* latest station joined is HT capable */
			/* This case is being handled in common code (commn between both the bands) below. */
		}
	}
	/* we are HT and HT station is joining. This code is common for both the bands. */
	if ((true == pe_session->htCapability) &&
	    (true == sta->mlmStaContext.htCapability)) {
		if (!sta->htGreenfield) {
			lim_enable_ht_non_gf_protection(mac, true, false,
							pBeaconParams,
							pe_session);
			gfSupported = eHAL_CLEAR;
		}
		/* Station joining is HT 20Mhz */
		if ((eHT_CHANNEL_WIDTH_20MHZ ==
		sta->htSupportedChannelWidthSet) &&
		(eHT_CHANNEL_WIDTH_20MHZ !=
		 pe_session->htSupportedChannelWidthSet)){
			protStaCacheType = eLIM_PROT_STA_CACHE_TYPE_HT20;
			lim_enable_ht20_protection(mac, true, false,
						   pBeaconParams, pe_session);
		}
		/* Station joining does not support LSIG TXOP Protection */
		if (!sta->htLsigTXOPProtection) {
			lim_enable_ht_lsig_txop_protection(mac, false, false,
							   pBeaconParams,
							   pe_session);
			lsigTxopSupported = eHAL_CLEAR;
		}
	}

	lim_update_prot_sta_params(mac, peerMacAddr, protStaCacheType,
				   gfSupported, lsigTxopSupported, pe_session);

	return;
}

/** -------------------------------------------------------------
   \fn lim_enable_overlap11g_protection
   \brief wrapper function for setting overlap 11g protection.
   \param      struct mac_context *   mac
   \param      tpUpdateBeaconParams pBeaconParams
   \param      tpSirMacMgmtHdr         pMh
   \return      None
   -------------------------------------------------------------*/
void
lim_enable_overlap11g_protection(struct mac_context *mac,
				 tpUpdateBeaconParams pBeaconParams,
				 tpSirMacMgmtHdr pMh, struct pe_session *pe_session)
{
	lim_update_overlap_sta_param(mac, pMh->bssId,
				     &(pe_session->gLimOlbcParams));

	if (pe_session->gLimOlbcParams.numSta &&
	    !pe_session->gLimOlbcParams.protectionEnabled) {
		/* enable protection */
		pe_debug("OLBC happens!!!");
		lim_enable11g_protection(mac, true, true, pBeaconParams,
					 pe_session);
	}
}

/**
 * lim_update_short_preamble() - This function Updates short preamble
 * @mac_ctx: pointer to Global MAC structure
 * @peer_mac_addr: pointer to tSirMacAddr
 * @pbeaconparams: pointer to tpUpdateBeaconParams
 * @psession_entry: pointer to struct pe_session *
 *
 * Function Updates short preamble if needed when a new station joins
 *
 * Return: none
 */
void
lim_update_short_preamble(struct mac_context *mac_ctx, tSirMacAddr peer_mac_addr,
				tpUpdateBeaconParams beaconparams,
				struct pe_session *psession_entry)
{
	uint16_t aid;
	tpDphHashNode sta_ds;
	uint32_t phy_mode;
	uint16_t i;

	/* check whether to enable protection or not */
	sta_ds =
		dph_lookup_hash_entry(mac_ctx, peer_mac_addr, &aid,
				      &psession_entry->dph.dphHashTable);

	lim_get_phy_mode(mac_ctx, &phy_mode, psession_entry);

	if (!sta_ds || phy_mode != WNI_CFG_PHY_MODE_11G)
		return;

	if (sta_ds->shortPreambleEnabled != eHAL_CLEAR)
		return;

	pe_debug("Short Preamble is not enabled in Assoc Req from");

	lim_print_mac_addr(mac_ctx, peer_mac_addr, LOGD);

	for (i = 0; i < LIM_PROT_STA_CACHE_SIZE; i++) {
		if (LIM_IS_AP_ROLE(psession_entry) &&
			(psession_entry->gLimNoShortParams.
				staNoShortCache[i].active) &&
			(!qdf_mem_cmp
				(psession_entry->gLimNoShortParams.
				staNoShortCache[i].addr,
				peer_mac_addr, sizeof(tSirMacAddr))))
			return;
		else if (!LIM_IS_AP_ROLE(psession_entry) &&
				(mac_ctx->lim.gLimNoShortParams.
					staNoShortCache[i].active) &&
			(!qdf_mem_cmp(mac_ctx->lim.gLimNoShortParams.
				staNoShortCache[i].addr,
				peer_mac_addr,
				sizeof(tSirMacAddr))))
			return;
	}

	for (i = 0; i < LIM_PROT_STA_CACHE_SIZE; i++) {
		if (LIM_IS_AP_ROLE(psession_entry) &&
			!psession_entry->gLimNoShortParams.
				staNoShortCache[i].active)
			break;
		else if (!mac_ctx->lim.gLimNoShortParams.
				staNoShortCache[i].active)
			break;
	}

	if (i >= LIM_PROT_STA_CACHE_SIZE) {
		tLimNoShortParams *lim_params =
				&psession_entry->gLimNoShortParams;
		if (LIM_IS_AP_ROLE(psession_entry)) {
			pe_err("No space in Short cache active: %d sta: %d for sta",
				i, lim_params->numNonShortPreambleSta);
			lim_print_mac_addr(mac_ctx, peer_mac_addr, LOGE);
			return;
		} else {
			pe_err("No space in Short cache active: %d sta: %d for sta",
				i, lim_params->numNonShortPreambleSta);
			lim_print_mac_addr(mac_ctx, peer_mac_addr, LOGE);
			return;
		}

	}

	if (LIM_IS_AP_ROLE(psession_entry)) {
		qdf_mem_copy(psession_entry->gLimNoShortParams.
				staNoShortCache[i].addr,
				peer_mac_addr, sizeof(tSirMacAddr));
		psession_entry->gLimNoShortParams.staNoShortCache[i].
							active = true;
		psession_entry->gLimNoShortParams.numNonShortPreambleSta++;
	} else {
		qdf_mem_copy(mac_ctx->lim.gLimNoShortParams.
					staNoShortCache[i].addr,
				peer_mac_addr, sizeof(tSirMacAddr));
		mac_ctx->lim.gLimNoShortParams.staNoShortCache[i].active = true;
		mac_ctx->lim.gLimNoShortParams.numNonShortPreambleSta++;
	}

	/* enable long preamble */
	pe_debug("Disabling short preamble");

	if (lim_enable_short_preamble(mac_ctx, false, beaconparams,
					psession_entry) != QDF_STATUS_SUCCESS)
		pe_err("Cannot enable long preamble");
}

/**
 * lim_update_short_slot_time() - This function Updates short slot time
 * @mac_ctx: pointer to Global MAC structure
 * @peer_mac_addr: pointer to tSirMacAddr
 * @beacon_params: pointer to tpUpdateBeaconParams
 * @psession_entry: pointer to struct pe_session *
 *
 * Function Updates short slot time if needed when a new station joins
 *
 * Return: None
 */
void
lim_update_short_slot_time(struct mac_context *mac_ctx, tSirMacAddr peer_mac_addr,
			   tpUpdateBeaconParams beacon_params,
			   struct pe_session *session_entry)
{
	uint16_t aid;
	tpDphHashNode sta_ds;
	uint32_t phy_mode;
	uint32_t val;
	uint16_t i;

	/* check whether to enable protection or not */
	sta_ds = dph_lookup_hash_entry(mac_ctx, peer_mac_addr, &aid,
				       &session_entry->dph.dphHashTable);
	lim_get_phy_mode(mac_ctx, &phy_mode, session_entry);

	if (!sta_ds || phy_mode != WNI_CFG_PHY_MODE_11G)
		return;

	/*
	 * Only in case of softap in 11g mode, slot time might change
	 * depending on the STA being added. In 11a case, it should
	 * be always 1 and in 11b case, it should be always 0.
	 * Only when the new STA has short slot time disabled, we need to
	 * change softap's overall slot time settings else the default for
	 * softap is always short slot enabled. When the last long slot STA
	 * leaves softAP, we take care of it in lim_decide_short_slot
	 */
	if (sta_ds->shortSlotTimeEnabled != eHAL_CLEAR)
		return;

	pe_debug("Short Slot Time is not enabled in Assoc Req from");
	lim_print_mac_addr(mac_ctx, peer_mac_addr, LOGD);
	for (i = 0; i < LIM_PROT_STA_CACHE_SIZE; i++) {
		if (LIM_IS_AP_ROLE(session_entry) &&
		    session_entry->gLimNoShortSlotParams.
		    staNoShortSlotCache[i].active) {
			if (!qdf_mem_cmp(session_entry->
			    gLimNoShortSlotParams.staNoShortSlotCache[i].addr,
			    peer_mac_addr, sizeof(tSirMacAddr)))
				return;
		} else if (!LIM_IS_AP_ROLE(session_entry)) {
			if (mac_ctx->lim.gLimNoShortSlotParams.
			    staNoShortSlotCache[i].active) {
				if (!qdf_mem_cmp(mac_ctx->
				    lim.gLimNoShortSlotParams.
				    staNoShortSlotCache[i].addr,
				    peer_mac_addr, sizeof(tSirMacAddr)))
						return;
			}
		}
	}
	for (i = 0; i < LIM_PROT_STA_CACHE_SIZE; i++) {
		if (LIM_IS_AP_ROLE(session_entry) &&
		    !session_entry->gLimNoShortSlotParams.
		    staNoShortSlotCache[i].active)
			break;
		else
			if (!mac_ctx->lim.gLimNoShortSlotParams.
			    staNoShortSlotCache[i].active)
				break;
	}

	if (i >= LIM_PROT_STA_CACHE_SIZE) {
		if (LIM_IS_AP_ROLE(session_entry)) {
			pe_err("No space in ShortSlot cache active: %d sta: %d for sta",
				i, session_entry->gLimNoShortSlotParams.
				numNonShortSlotSta);
			lim_print_mac_addr(mac_ctx, peer_mac_addr, LOGE);
			return;
		} else {
			pe_err("No space in ShortSlot cache active: %d sta: %d for sta",
				i, mac_ctx->lim.gLimNoShortSlotParams.
				numNonShortSlotSta);
			lim_print_mac_addr(mac_ctx, peer_mac_addr, LOGE);
			return;
		}
	}

	if (LIM_IS_AP_ROLE(session_entry)) {
		qdf_mem_copy(session_entry->gLimNoShortSlotParams.
			staNoShortSlotCache[i].addr,
			peer_mac_addr, sizeof(tSirMacAddr));
		session_entry->gLimNoShortSlotParams.
			staNoShortSlotCache[i].active = true;
		session_entry->gLimNoShortSlotParams.numNonShortSlotSta++;
	} else {
		qdf_mem_copy(mac_ctx->lim.gLimNoShortSlotParams.
			staNoShortSlotCache[i].addr,
			peer_mac_addr, sizeof(tSirMacAddr));
		mac_ctx->lim.gLimNoShortSlotParams.
			staNoShortSlotCache[i].active = true;
		mac_ctx->lim.gLimNoShortSlotParams.
			numNonShortSlotSta++;
	}
	val = mac_ctx->mlme_cfg->feature_flags.enable_short_slot_time_11g;
	/*
	 * Here we check if we are AP role and short slot enabled
	 * (both admin and oper modes) but we have atleast one STA
	 * connected with only long slot enabled, we need to change
	 * our beacon/pb rsp to broadcast short slot disabled
	 */
	if ((LIM_IS_AP_ROLE(session_entry)) && (val &&
	    session_entry->gLimNoShortSlotParams.numNonShortSlotSta
	    && session_entry->shortSlotTimeSupported)) {
		/* enable long slot time */
		beacon_params->fShortSlotTime = false;
		beacon_params->paramChangeBitmap |=
				PARAM_SHORT_SLOT_TIME_CHANGED;
		pe_debug("Disable short slot time. Enable long slot time");
		session_entry->shortSlotTimeSupported = false;
	} else if (!LIM_IS_AP_ROLE(session_entry) &&
		   (val && mac_ctx->lim.gLimNoShortSlotParams.
		    numNonShortSlotSta &&
		    session_entry->shortSlotTimeSupported)) {
		/* enable long slot time */
		beacon_params->fShortSlotTime = false;
		beacon_params->paramChangeBitmap |=
			PARAM_SHORT_SLOT_TIME_CHANGED;
		pe_debug("Disable short slot time. Enable long slot time");
		session_entry->shortSlotTimeSupported = false;
	}
}

/** -------------------------------------------------------------
   \fn lim_decide_sta_protection_on_assoc
   \brief Decide protection related settings on Sta while association.
   \param      struct mac_context *   mac
   \param      tpSchBeaconStruct pBeaconStruct
   \return      None
   -------------------------------------------------------------*/
void
lim_decide_sta_protection_on_assoc(struct mac_context *mac,
				   tpSchBeaconStruct pBeaconStruct,
				   struct pe_session *pe_session)
{
	enum reg_wifi_band rfBand = REG_BAND_UNKNOWN;
	uint32_t phyMode = WNI_CFG_PHY_MODE_NONE;

	lim_get_rf_band_new(mac, &rfBand, pe_session);
	lim_get_phy_mode(mac, &phyMode, pe_session);

	if (REG_BAND_5G == rfBand) {
		if ((eSIR_HT_OP_MODE_MIXED == pBeaconStruct->HTInfo.opMode) ||
		    (eSIR_HT_OP_MODE_OVERLAP_LEGACY ==
		     pBeaconStruct->HTInfo.opMode)) {
			if (mac->lim.cfgProtection.fromlla)
				pe_session->beaconParams.llaCoexist = true;
		} else if (eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT ==
			   pBeaconStruct->HTInfo.opMode) {
			if (mac->lim.cfgProtection.ht20)
				pe_session->beaconParams.ht20Coexist = true;
		}

	} else if (REG_BAND_2G == rfBand) {
		/* spec 7.3.2.13 */
		/* UseProtection will be set when nonERP STA is associated. */
		/* NonERPPresent bit will be set when: */
		/* --nonERP Sta is associated OR */
		/* --nonERP Sta exists in overlapping BSS */
		/* when useProtection is not set then protection from nonERP stations is optional. */

		/* CFG protection from 11b is enabled and */
		/* 11B device in the BSS */
		/* TODO, This is not sessionized */
		if (phyMode != WNI_CFG_PHY_MODE_11B) {
			if (mac->lim.cfgProtection.fromllb &&
			    pBeaconStruct->erpPresent &&
			    (pBeaconStruct->erpIEInfo.useProtection ||
			     pBeaconStruct->erpIEInfo.nonErpPresent)) {
				pe_session->beaconParams.llbCoexist = true;
			}
			/* AP has no 11b station associated. */
			else {
				pe_session->beaconParams.llbCoexist = false;
			}
		}
		/* following code block is only for HT station. */
		if ((pe_session->htCapability) &&
		    (pBeaconStruct->HTInfo.present)) {
			tDot11fIEHTInfo htInfo = pBeaconStruct->HTInfo;

			/* Obss Non HT STA present mode */
			pe_session->beaconParams.gHTObssMode =
				(uint8_t) htInfo.obssNonHTStaPresent;

			/* CFG protection from 11G is enabled and */
			/* our AP has at least one 11G station associated. */
			if (mac->lim.cfgProtection.fromllg &&
			    ((eSIR_HT_OP_MODE_MIXED == htInfo.opMode) ||
			     (eSIR_HT_OP_MODE_OVERLAP_LEGACY == htInfo.opMode))
			    && (!pe_session->beaconParams.llbCoexist)) {
				if (mac->lim.cfgProtection.fromllg)
					pe_session->beaconParams.llgCoexist =
						true;
			}
			/* AP has only HT stations associated and at least one station is HT 20 */
			/* disable protection from any non-HT devices. */
			/* decision for disabling protection from 11b has already been taken above. */
			if (eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT == htInfo.opMode) {
				/* Disable protection from 11G station. */
				pe_session->beaconParams.llgCoexist = false;
				/* CFG protection from HT 20 is enabled. */
				if (mac->lim.cfgProtection.ht20)
					pe_session->beaconParams.
					ht20Coexist = true;
			}
			/* Disable protection from non-HT and HT20 devices. */
			/* decision for disabling protection from 11b has already been taken above. */
			if (eSIR_HT_OP_MODE_PURE == htInfo.opMode) {
				pe_session->beaconParams.llgCoexist = false;
				pe_session->beaconParams.ht20Coexist = false;
			}

		}
	}
	/* protection related factors other than HT operating mode. Applies to 2.4 GHZ as well as 5 GHZ. */
	if ((pe_session->htCapability) && (pBeaconStruct->HTInfo.present)) {
		tDot11fIEHTInfo htInfo = pBeaconStruct->HTInfo;

		pe_session->beaconParams.fRIFSMode =
			(uint8_t) htInfo.rifsMode;
		pe_session->beaconParams.llnNonGFCoexist =
			(uint8_t) htInfo.nonGFDevicesPresent;
		pe_session->beaconParams.fLsigTXOPProtectionFullSupport =
			(uint8_t) htInfo.lsigTXOPProtectionFullSupport;
	}
}


/**
 * lim_decide_sta_11bg_protection() - decides protection related settings on sta
 * @mac_ctx: pointer to global mac structure
 * @beacon_struct: pointer to tpschbeaconstruct
 * @beaconparams: pointer to tpupdatebeaconparams
 * @psession_entry: pointer to tppesession
 * @phy_mode: phy mode index
 *
 * decides 11bg protection related settings on sta while processing beacon
 *
 * Return: none
 */
static void
lim_decide_sta_11bg_protection(struct mac_context *mac_ctx,
			tpSchBeaconStruct beacon_struct,
			tpUpdateBeaconParams beaconparams,
			struct pe_session *psession_entry,
			uint32_t phy_mode)
{

	tDot11fIEHTInfo htInfo;

	/*
	 * spec 7.3.2.13
	 * UseProtection will be set when nonERP STA is associated.
	 * NonERPPresent bit will be set when:
	 * --nonERP Sta is associated OR
	 * --nonERP Sta exists in overlapping BSS
	 * when useProtection is not set then protection from
	 * nonERP stations is optional.
	 */
	if (phy_mode != WNI_CFG_PHY_MODE_11B) {
		if (beacon_struct->erpPresent &&
			(beacon_struct->erpIEInfo.useProtection ||
			beacon_struct->erpIEInfo.nonErpPresent)) {
			lim_enable11g_protection(mac_ctx, true, false,
						beaconparams,
						psession_entry);
		}
		/* AP has no 11b station associated. */
		else {
			/* disable protection from 11b station */
			lim_enable11g_protection(mac_ctx, false, false,
						beaconparams,
						psession_entry);
		}
	}

	if (!(psession_entry->htCapability) ||
		!(beacon_struct->HTInfo.present))
		return;

	/* following code is only for HT station. */

	htInfo = beacon_struct->HTInfo;
	/* AP has at least one 11G station associated. */
	if (((eSIR_HT_OP_MODE_MIXED == htInfo.opMode) ||
		(eSIR_HT_OP_MODE_OVERLAP_LEGACY == htInfo.opMode)) &&
		(!psession_entry->beaconParams.llbCoexist)) {
		lim_enable_ht_protection_from11g(mac_ctx, true, false,
						beaconparams, psession_entry);

	}
	/*
	 * no HT operating mode change  ==> no change in
	 * protection settings except for MIXED_MODE/Legacy
	 * Mode.
	 */
	/*
	 * in Mixed mode/legacy Mode even if there is no
	 * change in HT operating mode, there might be
	 * change in 11bCoexist or 11gCoexist. Hence this
	 * check is being done after mixed/legacy mode
	 * check.
	 */
	if (mac_ctx->lim.gHTOperMode !=
		(tSirMacHTOperatingMode)htInfo.opMode) {
		mac_ctx->lim.gHTOperMode =
			(tSirMacHTOperatingMode) htInfo.opMode;
		/*
		 * AP has only HT stations associated and
		 * at least one station is HT 20
		 */

		/* disable protection from any non-HT devices. */

		/*
		 * decision for disabling protection from
		 * 11b has already been taken above.
		 */
		if (eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT ==
				htInfo.opMode) {
			/* Disable protection from 11G station. */
			lim_enable_ht_protection_from11g(mac_ctx, false,
						false, beaconparams,
						psession_entry);

			lim_enable_ht20_protection(mac_ctx, true, false,
						beaconparams,
						psession_entry);
		}
		/*
		 * Disable protection from non-HT and
		 * HT20 devices.
		 */
		/*
		 * decision for disabling protection from
		 * 11b has already been taken above.
		 */
		else if (eSIR_HT_OP_MODE_PURE == htInfo.opMode) {
			lim_enable_ht_protection_from11g(mac_ctx, false,
						false, beaconparams,
						psession_entry);

			lim_enable_ht20_protection(mac_ctx, false,
						false, beaconparams,
						psession_entry);

		}
	}

}

/**
 * lim_decide_sta_protection() -  decides protection related settings on sta
 * @mac_ctx: pointer to global mac structure
 * @beacon_struct: pointer to tpschbeaconstruct
 * @beaconparams: pointer to tpupdatebeaconparams
 * @psession_entry: pointer to tppesession
 *
 * decides protection related settings on sta while processing beacon
 *
 * Return: none
 */
void
lim_decide_sta_protection(struct mac_context *mac_ctx,
				tpSchBeaconStruct beacon_struct,
				tpUpdateBeaconParams beaconparams,
				struct pe_session *psession_entry)
{

	enum reg_wifi_band rfband = REG_BAND_UNKNOWN;
	uint32_t phy_mode = WNI_CFG_PHY_MODE_NONE;

	lim_get_rf_band_new(mac_ctx, &rfband, psession_entry);
	lim_get_phy_mode(mac_ctx, &phy_mode, psession_entry);

	if ((REG_BAND_5G == rfband) &&
		/* we are HT capable. */
		(true == psession_entry->htCapability) &&
		(beacon_struct->HTInfo.present)) {
		/*
		 * we are HT capable, AP's HT OPMode is
		 * mixed / overlap legacy ==> need protection
		 * from 11A.
		 */
		if ((eSIR_HT_OP_MODE_MIXED ==
				beacon_struct->HTInfo.opMode) ||
			(eSIR_HT_OP_MODE_OVERLAP_LEGACY ==
				beacon_struct->HTInfo.opMode)) {
			lim_update_11a_protection(mac_ctx, true, false,
						beaconparams, psession_entry);
		}
		/*
		 * we are HT capable, AP's HT OPMode is
		 * HT20 ==> disable protection from 11A if
		 * enabled.
		 */
		/* protection from HT20 if needed. */
		else if (eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT ==
					beacon_struct->HTInfo.opMode) {
			lim_update_11a_protection(mac_ctx, false, false,
						beaconparams, psession_entry);
			lim_enable_ht20_protection(mac_ctx, true, false,
						beaconparams, psession_entry);
		} else if (eSIR_HT_OP_MODE_PURE ==
				beacon_struct->HTInfo.opMode) {
			lim_update_11a_protection(mac_ctx, false, false,
						beaconparams, psession_entry);
			lim_enable_ht20_protection(mac_ctx, false,
						false, beaconparams,
						psession_entry);
		}
	} else if (REG_BAND_2G == rfband) {
		lim_decide_sta_11bg_protection(mac_ctx, beacon_struct,
					beaconparams, psession_entry, phy_mode);
	}
	/*
	 * following code block is only for HT station.
	 * (2.4 GHZ as well as 5 GHZ)
	 */
	if ((psession_entry->htCapability) && (beacon_struct->HTInfo.present)) {
		tDot11fIEHTInfo htInfo = beacon_struct->HTInfo;
		/*
		 * Check for changes in protection related factors other
		 * than HT operating mode.
		 */
		/*
		 * Check for changes in RIFS mode, nonGFDevicesPresent,
		 * lsigTXOPProtectionFullSupport.
		 */
		if (psession_entry->beaconParams.fRIFSMode !=
				(uint8_t) htInfo.rifsMode) {
			beaconparams->fRIFSMode =
				psession_entry->beaconParams.fRIFSMode =
						(uint8_t) htInfo.rifsMode;
			beaconparams->paramChangeBitmap |=
						PARAM_RIFS_MODE_CHANGED;
		}

		if (psession_entry->beaconParams.llnNonGFCoexist !=
					htInfo.nonGFDevicesPresent) {
			beaconparams->llnNonGFCoexist =
				psession_entry->beaconParams.llnNonGFCoexist =
					(uint8_t) htInfo.nonGFDevicesPresent;
			beaconparams->paramChangeBitmap |=
					PARAM_NON_GF_DEVICES_PRESENT_CHANGED;
		}

		if (psession_entry->beaconParams.
			fLsigTXOPProtectionFullSupport !=
			(uint8_t) htInfo.lsigTXOPProtectionFullSupport) {
			beaconparams->fLsigTXOPProtectionFullSupport =
				psession_entry->beaconParams.
					fLsigTXOPProtectionFullSupport =
						(uint8_t) htInfo.
						lsigTXOPProtectionFullSupport;
			beaconparams->paramChangeBitmap |=
					PARAM_LSIG_TXOP_FULL_SUPPORT_CHANGED;
		}
		/*
		 * For Station just update the global lim variable,
		 * no need to send message to HAL since Station already
		 * taking care of HT OPR Mode=01,
		 * meaning AP is seeing legacy
		 */
		/* stations in overlapping BSS. */
		if (psession_entry->beaconParams.gHTObssMode !=
				(uint8_t) htInfo.obssNonHTStaPresent)
			psession_entry->beaconParams.gHTObssMode =
				(uint8_t) htInfo.obssNonHTStaPresent;

	}
}

/**
 * __lim_process_channel_switch_timeout()
 *
 ***FUNCTION:
 * This function is invoked when Channel Switch Timer expires at
 * the STA.  Now, STA must stop traffic, and then change/disable
 * primary or secondary channel.
 *
 *
 ***NOTE:
 * @param  pe_session           - Pointer to pe session
 * @return None
 */
static void __lim_process_channel_switch_timeout(struct pe_session *pe_session)
{
	struct mac_context *mac;
	uint32_t channel_freq;

	if (!pe_session) {
		pe_err("Invalid pe session");
		return;
	}
	mac = pe_session->mac_ctx;
	if (!mac) {
		pe_err("Invalid mac context");
		return;
	}

	if (!LIM_IS_STA_ROLE(pe_session)) {
		pe_warn("Channel switch can be done only in STA role, Current Role: %d",
			       GET_LIM_SYSTEM_ROLE(pe_session));
		return;
	}

	if (pe_session->gLimSpecMgmt.dot11hChanSwState !=
	   eLIM_11H_CHANSW_RUNNING) {
		pe_warn("Channel switch timer should not have been running in state: %d",
			pe_session->gLimSpecMgmt.dot11hChanSwState);
		return;
	}

	channel_freq = pe_session->gLimChannelSwitch.sw_target_freq;
	/* Restore Channel Switch parameters to default */
	pe_session->gLimChannelSwitch.switchTimeoutValue = 0;

	/* Channel-switch timeout has occurred. reset the state */
	pe_session->gLimSpecMgmt.dot11hChanSwState = eLIM_11H_CHANSW_END;

	/* Check if the AP is switching to a channel that we support.
	 * Else, just don't bother to switch. Indicate HDD to look for a
	 * better AP to associate
	 */
	if (!lim_is_channel_valid_for_channel_switch(mac, channel_freq)) {
		/* We need to restore pre-channelSwitch state on the STA */
		if (lim_restore_pre_channel_switch_state(mac, pe_session) !=
		    QDF_STATUS_SUCCESS) {
			pe_err("Could not restore pre-channelSwitch (11h) state, resetting the system");
			return;
		}

		/*
		 * If the channel-list that AP is asking us to switch is invalid
		 * then we cannot switch the channel. Just disassociate from AP.
		 * We will find a better AP !!!
		 */
		if ((pe_session->limMlmState ==
		   eLIM_MLM_LINK_ESTABLISHED_STATE) &&
		   (pe_session->limSmeState != eLIM_SME_WT_DISASSOC_STATE) &&
		   (pe_session->limSmeState != eLIM_SME_WT_DEAUTH_STATE)) {
			pe_err("Invalid channel! Disconnect");
			lim_tear_down_link_with_ap(mac, pe_session->peSessionId,
					   REASON_UNSUPPORTED_CHANNEL_CSA,
					   eLIM_LINK_MONITORING_DISASSOC);
			return;
		}
	}
	switch (pe_session->gLimChannelSwitch.state) {
	case eLIM_CHANNEL_SWITCH_PRIMARY_ONLY:
		lim_switch_primary_channel(mac,
					   pe_session->gLimChannelSwitch.
					   sw_target_freq, pe_session);
		pe_session->gLimChannelSwitch.state =
			eLIM_CHANNEL_SWITCH_IDLE;
		break;
	case eLIM_CHANNEL_SWITCH_PRIMARY_AND_SECONDARY:
		lim_switch_primary_secondary_channel(mac, pe_session,
			pe_session->gLimChannelSwitch.sw_target_freq,
			pe_session->gLimChannelSwitch.ch_center_freq_seg0,
			pe_session->gLimChannelSwitch.ch_center_freq_seg1,
			pe_session->gLimChannelSwitch.ch_width);
		pe_session->gLimChannelSwitch.state =
			eLIM_CHANNEL_SWITCH_IDLE;
		break;

	case eLIM_CHANNEL_SWITCH_IDLE:
	default:
		pe_err("incorrect state");
		if (lim_restore_pre_channel_switch_state(mac, pe_session) !=
		    QDF_STATUS_SUCCESS) {
			pe_err("Could not restore pre-channelSwitch (11h) state, resetting the system");
		}
		return; /* Please note, this is 'return' and not 'break' */
	}
}

void lim_disconnect_complete(struct pe_session *session, bool del_bss)
{
	QDF_STATUS status;
	struct mac_context *mac = session->mac_ctx;

	if (wlan_vdev_mlme_get_substate(session->vdev) ==
	    WLAN_VDEV_SS_STOP_STOP_PROGRESS) {
		status = wlan_vdev_mlme_sm_deliver_evt(session->vdev,
						       WLAN_VDEV_SM_EV_STOP_REQ,
						       sizeof(*session),
						       session);
		return;
	}
	status =
	   wlan_vdev_mlme_sm_deliver_evt(session->vdev,
					 WLAN_VDEV_SM_EV_DISCONNECT_COMPLETE,
					 sizeof(*session), session);
	if (QDF_IS_STATUS_ERROR(status))
		lim_send_stop_bss_failure_resp(mac, session);
}

void lim_process_channel_switch(struct mac_context *mac_ctx, uint8_t vdev_id)
{
	struct pe_session *session_entry;
	QDF_STATUS status;

	session_entry = pe_find_session_by_vdev_id(mac_ctx, vdev_id);
	if (!session_entry) {
		pe_err("Session does not exist for given vdev_id %d", vdev_id);
		return;
	}

	session_entry->channelChangeReasonCode = LIM_SWITCH_CHANNEL_OPERATION;
	mlme_set_chan_switch_in_progress(session_entry->vdev, true);
	status = wlan_vdev_mlme_sm_deliver_evt(
					session_entry->vdev,
					WLAN_VDEV_SM_EV_FW_VDEV_RESTART,
					sizeof(*session_entry),
					session_entry);
	if (QDF_IS_STATUS_ERROR(status))
		mlme_set_chan_switch_in_progress(session_entry->vdev, false);
}

/** ------------------------------------------------------------------------ **/
/**
 * keep track of the number of ANI peers associated in the BSS
 * For the first and last ANI peer, we have to update EDCA params as needed
 *
 * When the first ANI peer joins the BSS, we notify SCH
 * When the last ANI peer leaves the BSS, we notfiy SCH
 */
void
lim_util_count_sta_add(struct mac_context *mac,
		       tpDphHashNode pSta, struct pe_session *pe_session)
{

	if ((!pSta) || (!pSta->valid) || (pSta->fAniCount))
		return;

	pSta->fAniCount = 1;

	if (mac->lim.gLimNumOfAniSTAs++ != 0)
		return;

	if (mac->mlme_cfg->wmm_params.edca_profile !=
	    WNI_CFG_EDCA_PROFILE_ANI)
		return;

	/* get here only if this is the first ANI peer in the BSS */
	sch_edca_profile_update(mac, pe_session);
}

void
lim_util_count_sta_del(struct mac_context *mac,
		       tpDphHashNode pSta, struct pe_session *pe_session)
{

	if ((!pSta) || (!pSta->fAniCount))
		return;

	/* Only if sta is invalid and the validInDummyState bit is set to 1,
	 * then go ahead and update the count and profiles. This ensures
	 * that the "number of ani station" count is properly incremented/decremented.
	 */
	if (pSta->valid == 1)
		return;

	pSta->fAniCount = 0;

	if (mac->lim.gLimNumOfAniSTAs <= 0) {
		pe_err("CountStaDel: ignoring Delete Req when AniPeer count: %d",
			mac->lim.gLimNumOfAniSTAs);
		return;
	}

	mac->lim.gLimNumOfAniSTAs--;

	if (mac->lim.gLimNumOfAniSTAs != 0)
		return;

	if (mac->mlme_cfg->wmm_params.edca_profile !=
	    WNI_CFG_EDCA_PROFILE_ANI)
		return;

	/* get here only if this is the last ANI peer in the BSS */
	sch_edca_profile_update(mac, pe_session);
}

/**
 * lim_switch_channel_vdev_started() - Send vdev started when switch channel
 *
 * @pe_session: PE session entry
 *
 * This function is called to deliver WLAN_VDEV_SM_EV_START_SUCCESS to VDEV SM
 *
 * Return: None
 */
static void lim_switch_channel_vdev_started(struct pe_session *pe_session)
{
	QDF_STATUS status;

	status = wlan_vdev_mlme_sm_deliver_evt(
				pe_session->vdev,
				WLAN_VDEV_SM_EV_START_SUCCESS,
				sizeof(*pe_session), pe_session);
}

/**
 * lim_switch_channel_cback()
 *
 ***FUNCTION:
 *  This is the callback function registered while requesting to switch channel
 *  after AP indicates a channel switch for spectrum management (11h).
 *
 ***NOTE:
 * @param  mac               Pointer to Global MAC structure
 * @param  status             Status of channel switch request
 * @param  data               User data
 * @param  pe_session      Session information
 * @return NONE
 */
void lim_switch_channel_cback(struct mac_context *mac, QDF_STATUS status,
			      uint32_t *data, struct pe_session *pe_session)
{
	struct scheduler_msg mmhMsg = { 0 };
	struct switch_channel_ind *pSirSmeSwitchChInd;
	struct wlan_channel *des_chan;
	struct vdev_mlme_obj *mlme_obj;

	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(pe_session->vdev);
	if (!mlme_obj) {
		pe_err("vdev component object is NULL");
		return;
	}

	des_chan = mlme_obj->vdev->vdev_mlme.des_chan;
	if (!des_chan) {
		pe_err("des_chan is NULL");
		return;
	}
	pe_session->curr_op_freq = pe_session->curr_req_chan_freq;
	/* We need to restore pre-channelSwitch state on the STA */
	if (lim_restore_pre_channel_switch_state(mac, pe_session) !=
	    QDF_STATUS_SUCCESS) {
		pe_err("Could not restore pre-channelSwitch (11h) state, resetting the system");
		return;
	}

	mmhMsg.type = eWNI_SME_SWITCH_CHL_IND;
	pSirSmeSwitchChInd = qdf_mem_malloc(sizeof(*pSirSmeSwitchChInd));
	if (!pSirSmeSwitchChInd)
		return;

	pSirSmeSwitchChInd->messageType = eWNI_SME_SWITCH_CHL_IND;
	pSirSmeSwitchChInd->length = sizeof(*pSirSmeSwitchChInd);
	pSirSmeSwitchChInd->freq = des_chan->ch_freq;
	pSirSmeSwitchChInd->sessionId = pe_session->smeSessionId;
	pSirSmeSwitchChInd->chan_params.ch_width = des_chan->ch_width;
	if (des_chan->ch_width > CH_WIDTH_20MHZ) {
		pSirSmeSwitchChInd->chan_params.sec_ch_offset =
			pe_session->gLimChannelSwitch.sec_ch_offset;
		pSirSmeSwitchChInd->chan_params.center_freq_seg0 =
							des_chan->ch_freq_seg1;
		pSirSmeSwitchChInd->chan_params.mhz_freq_seg0 =
							des_chan->ch_cfreq1;
		pSirSmeSwitchChInd->chan_params.center_freq_seg1 =
							des_chan->ch_freq_seg2;
		pSirSmeSwitchChInd->chan_params.mhz_freq_seg1 =
							des_chan->ch_cfreq2;
	}

	pSirSmeSwitchChInd->status = status;
	qdf_mem_copy(pSirSmeSwitchChInd->bssid.bytes, pe_session->bssId,
		     QDF_MAC_ADDR_SIZE);
	mmhMsg.bodyptr = pSirSmeSwitchChInd;
	mmhMsg.bodyval = 0;

	MTRACE(mac_trace(mac, TRACE_CODE_TX_SME_MSG,
			 pe_session->peSessionId, mmhMsg.type));

	sys_process_mmh_msg(mac, &mmhMsg);

	if (QDF_IS_STATUS_SUCCESS(status))
		lim_switch_channel_vdev_started(pe_session);
}

void lim_switch_primary_channel(struct mac_context *mac,
				uint32_t new_channel_freq,
				struct pe_session *pe_session)
{
	pe_debug("freq: %d --> freq: %d", pe_session->curr_op_freq,
		 new_channel_freq);

	pe_session->curr_req_chan_freq = new_channel_freq;
	pe_session->curr_op_freq = pe_session->curr_req_chan_freq;
	pe_session->ch_center_freq_seg0 = 0;
	pe_session->ch_center_freq_seg1 = 0;
	pe_session->ch_width = CH_WIDTH_20MHZ;
	pe_session->limRFBand = lim_get_rf_band(pe_session->curr_req_chan_freq);

	pe_session->channelChangeReasonCode = LIM_SWITCH_CHANNEL_OPERATION;

	mac->lim.gpchangeChannelCallback = lim_switch_channel_cback;
	mac->lim.gpchangeChannelData = NULL;

	lim_send_switch_chnl_params(mac, pe_session);
	return;
}

void lim_switch_primary_secondary_channel(struct mac_context *mac,
					  struct pe_session *pe_session,
					  uint32_t new_channel_freq,
					  uint8_t ch_center_freq_seg0,
					  uint8_t ch_center_freq_seg1,
					  enum phy_ch_width ch_width)
{

	/* Assign the callback to resume TX once channel is changed. */
	pe_session->curr_req_chan_freq = new_channel_freq;
	pe_session->limRFBand = lim_get_rf_band(pe_session->curr_req_chan_freq);
	pe_session->channelChangeReasonCode = LIM_SWITCH_CHANNEL_OPERATION;
	mac->lim.gpchangeChannelCallback = lim_switch_channel_cback;
	mac->lim.gpchangeChannelData = NULL;

	/* Store the new primary and secondary channel in session entries if different */
	if (pe_session->curr_op_freq != new_channel_freq) {
		pe_warn("freq: %d --> freq: %d", pe_session->curr_op_freq,
			new_channel_freq);
		pe_session->curr_op_freq = new_channel_freq;
	}
	if (pe_session->htSecondaryChannelOffset !=
			pe_session->gLimChannelSwitch.sec_ch_offset) {
		pe_warn("HT sec chnl: %d --> HT sec chnl: %d",
			pe_session->htSecondaryChannelOffset,
			pe_session->gLimChannelSwitch.sec_ch_offset);
		pe_session->htSecondaryChannelOffset =
			pe_session->gLimChannelSwitch.sec_ch_offset;
		if (pe_session->htSecondaryChannelOffset ==
		    PHY_SINGLE_CHANNEL_CENTERED) {
			pe_session->htSupportedChannelWidthSet =
				WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
		} else {
			pe_session->htSupportedChannelWidthSet =
				WNI_CFG_CHANNEL_BONDING_MODE_ENABLE;
		}
		pe_session->htRecommendedTxWidthSet =
			pe_session->htSupportedChannelWidthSet;
	}

	pe_session->ch_center_freq_seg0 = ch_center_freq_seg0;
	pe_session->ch_center_freq_seg1 = ch_center_freq_seg1;
	pe_session->ch_width = ch_width;

	lim_send_switch_chnl_params(mac, pe_session);

	return;
}

/**
 * lim_get_ht_capability()
 *
 ***FUNCTION:
 * A utility function that returns the "current HT capability state" for the HT
 * capability of interest (as requested in the API)
 *
 ***LOGIC:
 * This routine will return with the "current" setting of a requested HT
 * capability. This state info could be retrieved from -
 * a) CFG (for static entries)
 * b) Run time info
 *   - Dynamic state maintained by LIM
 *   - Configured at radio init time by SME
 *
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 *
 * @param  mac  Pointer to Global MAC structure
 * @param  htCap The HT capability being queried
 * @return uint8_t The current state of the requested HT capability is returned in a
 *            uint8_t variable
 */

uint8_t lim_get_ht_capability(struct mac_context *mac,
			      uint32_t htCap, struct pe_session *pe_session)
{
	uint8_t retVal = 0;
	uint8_t *ptr;
	tSirMacTxBFCapabilityInfo macTxBFCapabilityInfo = { 0 };
	tSirMacASCapabilityInfo macASCapabilityInfo = { 0 };
	struct mlme_vht_capabilities_info *vht_cap_info;

	vht_cap_info = &mac->mlme_cfg->vht_caps.vht_cap_info;

	/* */
	/* Determine which CFG to read from. Not ALL of the HT */
	/* related CFG's need to be read each time this API is */
	/* accessed */
	/* */
	if (htCap >= eHT_ANTENNA_SELECTION && htCap < eHT_SI_GRANULARITY) {
		/* Get Antenna Seletion HT Capabilities */
		ptr = (uint8_t *) &macASCapabilityInfo;
		*((uint8_t *)ptr) = (uint8_t)(vht_cap_info->as_cap & 0xff);
	} else if (htCap >= eHT_TX_BEAMFORMING &&
		   htCap < eHT_ANTENNA_SELECTION) {
		/* Get Transmit Beam Forming HT Capabilities */
		ptr = (uint8_t *)&macTxBFCapabilityInfo;
		*((uint32_t *)ptr) = (uint32_t)(vht_cap_info->tx_bf_cap);
	}

	switch (htCap) {
	case eHT_LSIG_TXOP_PROTECTION:
		retVal = mac->lim.gHTLsigTXOPProtection;
		break;

	case eHT_STBC_CONTROL_FRAME:
		retVal = (uint8_t)mac->mlme_cfg->ht_caps.ht_cap_info.
			stbc_control_frame;
		break;

	case eHT_PSMP:
		retVal = mac->lim.gHTPSMPSupport;
		break;

	case eHT_DSSS_CCK_MODE_40MHZ:
		retVal = mac->lim.gHTDsssCckRate40MHzSupport;
		break;

	case eHT_MAX_AMSDU_LENGTH:
		retVal = (uint8_t)mac->mlme_cfg->ht_caps.ht_cap_info.
			maximal_amsdu_size;
		break;

	case eHT_MAX_AMSDU_NUM:
		retVal = (uint8_t) pe_session->max_amsdu_num;
		break;

	case eHT_RX_STBC:
		retVal = (uint8_t) pe_session->ht_config.ht_rx_stbc;
		break;

	case eHT_TX_STBC:
		retVal = (uint8_t) pe_session->ht_config.ht_tx_stbc;
		break;

	case eHT_SHORT_GI_40MHZ:
		retVal = (uint8_t)(pe_session->ht_config.ht_sgi40) ?
			mac->mlme_cfg->ht_caps.ht_cap_info.short_gi_40_mhz : 0;
		break;

	case eHT_SHORT_GI_20MHZ:
		retVal = (uint8_t)(pe_session->ht_config.ht_sgi20) ?
			mac->mlme_cfg->ht_caps.ht_cap_info.short_gi_20_mhz : 0;
		break;

	case eHT_GREENFIELD:
		retVal = (uint8_t)mac->mlme_cfg->ht_caps.ht_cap_info.
			green_field;
		break;

	case eHT_MIMO_POWER_SAVE:
		retVal = (uint8_t) mac->lim.gHTMIMOPSState;
		break;

	case eHT_SUPPORTED_CHANNEL_WIDTH_SET:
		retVal = (uint8_t) pe_session->htSupportedChannelWidthSet;
		break;

	case eHT_ADVANCED_CODING:
		retVal = (uint8_t) pe_session->ht_config.ht_rx_ldpc;
		break;

	case eHT_MAX_RX_AMPDU_FACTOR:
		retVal = mac->lim.gHTMaxRxAMpduFactor;
		break;

	case eHT_MPDU_DENSITY:
		retVal = mac->lim.gHTAMpduDensity;
		break;

	case eHT_PCO:
		retVal = (uint8_t)mac->mlme_cfg->ht_caps.ext_cap_info.pco;
		break;

	case eHT_TRANSITION_TIME:
		retVal = (uint8_t)mac->mlme_cfg->ht_caps.ext_cap_info.
			transition_time;
		break;

	case eHT_MCS_FEEDBACK:
		retVal = (uint8_t)mac->mlme_cfg->ht_caps.ext_cap_info.
			mcs_feedback;
		break;

	case eHT_TX_BEAMFORMING:
		retVal = (uint8_t) macTxBFCapabilityInfo.txBF;
		break;

	case eHT_ANTENNA_SELECTION:
		retVal = (uint8_t) macASCapabilityInfo.antennaSelection;
		break;

	case eHT_SI_GRANULARITY:
		retVal = mac->lim.gHTServiceIntervalGranularity;
		break;

	case eHT_CONTROLLED_ACCESS:
		retVal = mac->lim.gHTControlledAccessOnly;
		break;

	case eHT_RIFS_MODE:
		retVal = pe_session->beaconParams.fRIFSMode;
		break;

	case eHT_RECOMMENDED_TX_WIDTH_SET:
		retVal = pe_session->htRecommendedTxWidthSet;
		break;

	case eHT_EXTENSION_CHANNEL_OFFSET:
		retVal = pe_session->htSecondaryChannelOffset;
		break;

	case eHT_OP_MODE:
		if (LIM_IS_AP_ROLE(pe_session))
			retVal = pe_session->htOperMode;
		else
			retVal = mac->lim.gHTOperMode;
		break;

	case eHT_BASIC_STBC_MCS:
		retVal = mac->lim.gHTSTBCBasicMCS;
		break;

	case eHT_DUAL_CTS_PROTECTION:
		retVal = mac->lim.gHTDualCTSProtection;
		break;

	case eHT_LSIG_TXOP_PROTECTION_FULL_SUPPORT:
		retVal =
			pe_session->beaconParams.fLsigTXOPProtectionFullSupport;
		break;

	case eHT_PCO_ACTIVE:
		retVal = mac->lim.gHTPCOActive;
		break;

	case eHT_PCO_PHASE:
		retVal = mac->lim.gHTPCOPhase;
		break;

	default:
		break;
	}

	return retVal;
}

/**
 * lim_enable_11a_protection() - updates protection params for enable 11a
 * protection request
 * @mac_ctx:    pointer to Global MAC structure
 * @overlap:    1=> called from overlap context, 0 => called from assoc context.
 * @bcn_prms:   beacon parameters
 * @pe_session: pe session entry
 *
 * This function updates protection params for enable 11a protection request
 *
 * @Return: void
 */
static void
lim_enable_11a_protection(struct mac_context *mac_ctx,
			 uint8_t overlap,
			 tpUpdateBeaconParams bcn_prms,
			 struct pe_session *pe_session)
{
	/*
	 * If we are AP and HT capable, we need to set the HT OP mode
	 * appropriately.
	 */
	if (LIM_IS_AP_ROLE(pe_session) && (true == pe_session->htCapability)) {
		if (overlap) {
			pe_session->gLimOverlap11aParams.protectionEnabled =
				true;
			if ((eSIR_HT_OP_MODE_OVERLAP_LEGACY !=
			    mac_ctx->lim.gHTOperMode)
				&& (eSIR_HT_OP_MODE_MIXED !=
				    mac_ctx->lim.gHTOperMode)) {
				mac_ctx->lim.gHTOperMode =
					eSIR_HT_OP_MODE_OVERLAP_LEGACY;
				pe_session->htOperMode =
					eSIR_HT_OP_MODE_OVERLAP_LEGACY;
				lim_enable_ht_rifs_protection(mac_ctx, true,
					overlap, bcn_prms, pe_session);
				lim_enable_ht_obss_protection(mac_ctx, true,
					overlap, bcn_prms, pe_session);
			}
		} else {
			pe_session->gLim11aParams.protectionEnabled = true;
			if (eSIR_HT_OP_MODE_MIXED != pe_session->htOperMode) {
				mac_ctx->lim.gHTOperMode =
					eSIR_HT_OP_MODE_MIXED;
				pe_session->htOperMode = eSIR_HT_OP_MODE_MIXED;
				lim_enable_ht_rifs_protection(mac_ctx, true,
					overlap, bcn_prms, pe_session);
				lim_enable_ht_obss_protection(mac_ctx, true,
					overlap, bcn_prms, pe_session);
			}
		}
	}
	/* This part is common for station as well. */
	if (false == pe_session->beaconParams.llaCoexist) {
		pe_debug(" => protection from 11A Enabled");
		bcn_prms->llaCoexist = true;
		pe_session->beaconParams.llaCoexist = true;
		bcn_prms->paramChangeBitmap |= PARAM_llACOEXIST_CHANGED;
	}
}

/**
 * lim_disable_11a_protection() - updates protection params for disable 11a
 * protection request
 * @mac_ctx:    pointer to Global MAC structure
 * @overlap:    1=> called from overlap context, 0 => called from assoc context.
 * @bcn_prms:   beacon parameters
 * @pe_session: pe session entry
 *
 * This function updates protection params for disable 11a protection request
 *
 * @Return: void
 */
static void
lim_disable_11a_protection(struct mac_context *mac_ctx,
			   uint8_t overlap,
			   tpUpdateBeaconParams bcn_prms,
			   struct pe_session *pe_session)
{
	if (false == pe_session->beaconParams.llaCoexist)
		return;

	/* for station role */
	if (!LIM_IS_AP_ROLE(pe_session)) {
		pe_debug("===> Protection from 11A Disabled");
		bcn_prms->llaCoexist = false;
		pe_session->beaconParams.llaCoexist = false;
		bcn_prms->paramChangeBitmap |= PARAM_llACOEXIST_CHANGED;
		return;
	}
	/*
	 * for AP role.
	 * we need to take care of HT OP mode change if needed.
	 * We need to take care of Overlap cases.
	 */
	if (overlap) {
		/* Overlap Legacy protection disabled. */
		pe_session->gLimOverlap11aParams.protectionEnabled = false;

		/*
		 * We need to take care of HT OP mode iff we are HT AP.
		 * OR no HT op-mode change is needed if any of the overlap
		 * protection enabled.
		 */
		if (!pe_session->htCapability ||
		     (pe_session->gLimOverlap11aParams.protectionEnabled
		     || pe_session->gLimOverlapHt20Params.protectionEnabled
		     || pe_session->gLimOverlapNonGfParams.protectionEnabled))
			goto disable_11a_end;

		/* Check if there is a need to change HT OP mode. */
		if (eSIR_HT_OP_MODE_OVERLAP_LEGACY ==
		    mac_ctx->lim.gHTOperMode) {
			lim_enable_ht_rifs_protection(mac_ctx, false, overlap,
						      bcn_prms, pe_session);
			lim_enable_ht_obss_protection(mac_ctx, false, overlap,
						      bcn_prms, pe_session);

			if (pe_session->gLimHt20Params.protectionEnabled)
				mac_ctx->lim.gHTOperMode =
					eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT;
			else
				mac_ctx->lim.gHTOperMode = eSIR_HT_OP_MODE_PURE;
		}
	} else {
		/* Disable protection from 11A stations. */
		pe_session->gLim11aParams.protectionEnabled = false;
		lim_enable_ht_obss_protection(mac_ctx, false, overlap,
					      bcn_prms, pe_session);

		/*
		 * Check if any other non-HT protection enabled. Right now we
		 * are in HT OP Mixed mode. Change HT op mode appropriately.
		 */

		/* Change HT OP mode to 01 if any overlap protection enabled */
		if (pe_session->gLimOverlap11aParams.protectionEnabled
		    || pe_session->gLimOverlapHt20Params.protectionEnabled
		    || pe_session->gLimOverlapNonGfParams.protectionEnabled) {
			mac_ctx->lim.gHTOperMode =
				eSIR_HT_OP_MODE_OVERLAP_LEGACY;
			pe_session->htOperMode = eSIR_HT_OP_MODE_OVERLAP_LEGACY;
			lim_enable_ht_rifs_protection(mac_ctx, true, overlap,
						      bcn_prms, pe_session);
		} else if (pe_session->gLimHt20Params.protectionEnabled) {
			mac_ctx->lim.gHTOperMode =
				eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT;
			pe_session->htOperMode =
				eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT;
			lim_enable_ht_rifs_protection(mac_ctx, false, overlap,
						      bcn_prms, pe_session);
		} else {
			mac_ctx->lim.gHTOperMode = eSIR_HT_OP_MODE_PURE;
			pe_session->htOperMode = eSIR_HT_OP_MODE_PURE;
			lim_enable_ht_rifs_protection(mac_ctx, false, overlap,
						      bcn_prms, pe_session);
		}
	}

disable_11a_end:
	if (!pe_session->gLimOverlap11aParams.protectionEnabled &&
	    !pe_session->gLim11aParams.protectionEnabled) {
		pe_warn("===> Protection from 11A Disabled");
		bcn_prms->llaCoexist = false;
		pe_session->beaconParams.llaCoexist = false;
		bcn_prms->paramChangeBitmap |= PARAM_llACOEXIST_CHANGED;
	}
}

/**
 * lim_update_11a_protection() - based on config setting enables\disables 11a
 * protection.
 * @mac_ctx:    pointer to Global MAC structure
 * @enable:     1=> enable protection, 0=> disable protection.
 * @overlap:    1=> called from overlap context, 0 => called from assoc context.
 * @bcn_prms:   beacon parameters
 * @session:    pe session entry
 *
 * This based on config setting enables\disables 11a protection.
 *
 * @Return: success of failure of operation
 */
QDF_STATUS
lim_update_11a_protection(struct mac_context *mac_ctx, uint8_t enable,
			 uint8_t overlap, tpUpdateBeaconParams bcn_prms,
			 struct pe_session *session)
{
	if (!session) {
		pe_err("session is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	/* overlapping protection configuration check. */
	if (!overlap) {
		/* normal protection config check */
		if ((LIM_IS_AP_ROLE(session)) &&
		    (!session->cfgProtection.fromlla)) {
			/* protection disabled. */
			pe_warn("protection from 11a is disabled");
			return QDF_STATUS_SUCCESS;
		}
	}

	if (enable)
		lim_enable_11a_protection(mac_ctx, overlap, bcn_prms, session);
	else
		lim_disable_11a_protection(mac_ctx, overlap, bcn_prms, session);

	return QDF_STATUS_SUCCESS;
}

/**
 * lim_handle_enable11g_protection_enabled() - handle 11g protection enabled
 * @mac_ctx: pointer to Globale Mac structure
 * @beaconparams: pointer to tpUpdateBeaconParams
 * @overlap: 1=> called from overlap context, 0 => called from assoc context.
 * @session_entry: pointer to struct pe_session *
 *
 * Function handles 11g protection enaled case
 *
 * Return: none
 */
static void
lim_handle_enable11g_protection_enabled(struct mac_context *mac_ctx,
			tpUpdateBeaconParams beaconparams,
			uint8_t overlap, struct pe_session *session_entry)
{
	/*
	 * If we are AP and HT capable, we need to set the HT OP mode
	 * appropriately.
	 */
	if (LIM_IS_AP_ROLE(session_entry) && overlap) {
		session_entry->gLimOlbcParams.protectionEnabled = true;

		pe_debug("protection from olbc is enabled");

		if (true == session_entry->htCapability) {
			if ((eSIR_HT_OP_MODE_OVERLAP_LEGACY !=
				session_entry->htOperMode) &&
				(eSIR_HT_OP_MODE_MIXED !=
				session_entry->htOperMode)) {
				session_entry->htOperMode =
					eSIR_HT_OP_MODE_OVERLAP_LEGACY;
			}
			/*
			 * CR-263021: OBSS bit is not switching back to 0 after
			 * disabling the overlapping legacy BSS
			 */
			/*
			 * This fixes issue of OBSS bit not set after 11b, 11g
			 * station leaves
			 */
			lim_enable_ht_rifs_protection(mac_ctx, true,
					overlap, beaconparams, session_entry);
			/*
			 * Not processing OBSS bit from other APs, as we are
			 * already taking care of Protection from overlapping
			 * BSS based on erp IE or useProtection bit
			 */
			lim_enable_ht_obss_protection(mac_ctx, true,
					overlap, beaconparams, session_entry);
		}
	} else if (LIM_IS_AP_ROLE(session_entry) && !overlap) {
		session_entry->gLim11bParams.protectionEnabled = true;
		pe_debug("protection from 11b is enabled");
		if (true == session_entry->htCapability) {
			if (eSIR_HT_OP_MODE_MIXED !=
				session_entry->htOperMode) {
				session_entry->htOperMode =
					eSIR_HT_OP_MODE_MIXED;
				lim_enable_ht_rifs_protection(mac_ctx,
						true, overlap, beaconparams,
						session_entry);
				lim_enable_ht_obss_protection(mac_ctx,
						true, overlap, beaconparams,
						session_entry);
			}
		}
	}

	/* This part is common for staiton as well. */
	if (false == session_entry->beaconParams.llbCoexist) {
		pe_debug("=> 11G Protection Enabled");
		beaconparams->llbCoexist =
			session_entry->beaconParams.llbCoexist = true;
		beaconparams->paramChangeBitmap |=
			PARAM_llBCOEXIST_CHANGED;
	}
}

/**
 * lim_handle_11g_protection_for_11bcoexist() - 11g protection for 11b co-ex
 * @mac_ctx: pointer to Globale Mac structure
 * @beaconparams: pointer to tpUpdateBeaconParams
 * @overlap: 1=> called from overlap context, 0 => called from assoc context.
 * @session_entry: pointer to struct pe_session *
 *
 * Function handles 11g protection for 11b co-exist
 *
 * Return: none
 */
static void
lim_handle_11g_protection_for_11bcoexist(struct mac_context *mac_ctx,
			tpUpdateBeaconParams beaconparams,
			uint8_t overlap, struct pe_session *session_entry)
{
	/*
	 * For AP role:
	 * we need to take care of HT OP mode change if needed.
	 * We need to take care of Overlap cases.
	 */
	if (LIM_IS_AP_ROLE(session_entry) && overlap) {
		/* Overlap Legacy protection disabled. */
		session_entry->gLimOlbcParams.protectionEnabled = false;

		/* We need to take care of HT OP mode if we are HT AP. */
		if (session_entry->htCapability) {
			/*
			 * no HT op mode change if any of the overlap
			 * protection enabled.
			 */
			if (!(session_entry->gLimOverlap11gParams.
					protectionEnabled ||
				session_entry->gLimOverlapHt20Params.
					protectionEnabled ||
				session_entry->gLimOverlapNonGfParams.
					protectionEnabled) &&
				/*
				 * Check if there is a need to change HT
				 * OP mode.
				 */
				(eSIR_HT_OP_MODE_OVERLAP_LEGACY ==
						session_entry->htOperMode)) {
				lim_enable_ht_rifs_protection(mac_ctx, false,
					overlap, beaconparams, session_entry);
				lim_enable_ht_obss_protection(mac_ctx, false,
					overlap, beaconparams, session_entry);
				if (session_entry->gLimHt20Params.
						protectionEnabled) {
				if (eHT_CHANNEL_WIDTH_20MHZ ==
					session_entry->htSupportedChannelWidthSet)
					session_entry->htOperMode =
						eSIR_HT_OP_MODE_PURE;
				else
					session_entry->htOperMode =
					eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT;
				} else
					session_entry->htOperMode =
						eSIR_HT_OP_MODE_PURE;
			}
		}
	} else if (LIM_IS_AP_ROLE(session_entry) && !overlap) {
		/* Disable protection from 11B stations. */
		session_entry->gLim11bParams.protectionEnabled = false;
		pe_debug("===> 11B Protection Disabled");
		/* Check if any other non-HT protection enabled. */
		if (!session_entry->gLim11gParams.protectionEnabled) {
			/* Right now we are in HT OP Mixed mode. */
			/* Change HT op mode appropriately. */
			lim_enable_ht_obss_protection(mac_ctx, false, overlap,
					beaconparams, session_entry);
			/*
			 * Change HT OP mode to 01 if any overlap protection
			 * enabled
			 */
			if (session_entry->gLimOlbcParams.protectionEnabled ||
				session_entry->gLimOverlap11gParams.
					protectionEnabled ||
				session_entry->gLimOverlapHt20Params.
					protectionEnabled ||
				session_entry->gLimOverlapNonGfParams.
					protectionEnabled) {
				session_entry->htOperMode =
					eSIR_HT_OP_MODE_OVERLAP_LEGACY;
				pe_debug("===> 11G Protection Disabled");
				lim_enable_ht_rifs_protection(mac_ctx, true,
						overlap, beaconparams,
						session_entry);
			} else if (session_entry->gLimHt20Params.
						protectionEnabled) {
				/* Commenting because of CR 258588 WFA cert */
				/* session_entry->htOperMode =
				eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT; */
				session_entry->htOperMode =
						eSIR_HT_OP_MODE_PURE;
				pe_debug("===> 11G Protection Disabled");
				lim_enable_ht_rifs_protection(mac_ctx, false,
						overlap, beaconparams,
						session_entry);
			} else {
				session_entry->htOperMode =
						eSIR_HT_OP_MODE_PURE;
				lim_enable_ht_rifs_protection(mac_ctx, false,
						overlap, beaconparams,
						session_entry);
			}
		}
	}
	if (LIM_IS_AP_ROLE(session_entry)) {
		if (!session_entry->gLimOlbcParams.protectionEnabled &&
			!session_entry->gLim11bParams.protectionEnabled) {
			pe_debug("===> 11G Protection Disabled");
			beaconparams->llbCoexist =
				session_entry->beaconParams.llbCoexist =
							false;
			beaconparams->paramChangeBitmap |=
				PARAM_llBCOEXIST_CHANGED;
		}
	}
	/* For station role */
	if (!LIM_IS_AP_ROLE(session_entry)) {
		pe_debug("===> 11G Protection Disabled");
		beaconparams->llbCoexist =
			session_entry->beaconParams.llbCoexist = false;
		beaconparams->paramChangeBitmap |=
			PARAM_llBCOEXIST_CHANGED;
	}
}

/**
 * lim_enable11g_protection() - Function to enable 11g protection
 * @mac_ctx: pointer to Global Mac structure
 * @enable: 1=> enable protection, 0=> disable protection.
 * @overlap: 1=> called from overlap context, 0 => called from assoc context.
 * @beaconparams: pointer to tpUpdateBeaconParams
 * @session_entry: pointer to struct pe_session *
 *
 * based on config setting enables\disables 11g protection.
 *
 * Return: Success - QDF_STATUS_SUCCESS - Success, Error number - Failure
 */
QDF_STATUS
lim_enable11g_protection(struct mac_context *mac_ctx, uint8_t enable,
			 uint8_t overlap, tpUpdateBeaconParams beaconparams,
			 struct pe_session *session_entry)
{

	/* overlapping protection configuration check. */
	if (!overlap) {
		/* normal protection config check */
		if ((LIM_IS_AP_ROLE(session_entry)) &&
			!session_entry->cfgProtection.fromllb) {
			/* protection disabled. */
			pe_debug("protection from 11b is disabled");
			return QDF_STATUS_SUCCESS;
		} else if (!LIM_IS_AP_ROLE(session_entry)) {
			if (!mac_ctx->lim.cfgProtection.fromllb) {
				/* protection disabled. */
				pe_debug("protection from 11b is disabled");
				return QDF_STATUS_SUCCESS;
			}
		}
	}

	if (enable) {
		lim_handle_enable11g_protection_enabled(mac_ctx, beaconparams,
					overlap, session_entry);
	} else if (true == session_entry->beaconParams.llbCoexist) {
		lim_handle_11g_protection_for_11bcoexist(mac_ctx, beaconparams,
					overlap, session_entry);
	}
	return QDF_STATUS_SUCCESS;
}

/** -------------------------------------------------------------
   \fn lim_enable_ht_protection_from11g
   \brief based on cofig enables\disables protection from 11g.
   \param      uint8_t enable : 1=> enable protection, 0=> disable protection.
   \param      uint8_t overlap: 1=> called from overlap context, 0 => called from assoc context.
   \param      tpUpdateBeaconParams pBeaconParams
   \return      None
   -------------------------------------------------------------*/
QDF_STATUS
lim_enable_ht_protection_from11g(struct mac_context *mac, uint8_t enable,
				 uint8_t overlap,
				 tpUpdateBeaconParams pBeaconParams,
				 struct pe_session *pe_session)
{
	if (!pe_session->htCapability)
		return QDF_STATUS_SUCCESS;  /* protection from 11g is only for HT stations. */

	/* overlapping protection configuration check. */
	if (overlap) {
		if ((LIM_IS_AP_ROLE(pe_session))
		    && (!pe_session->cfgProtection.overlapFromllg)) {
			/* protection disabled. */
			pe_debug("overlap protection from 11g is disabled");
			return QDF_STATUS_SUCCESS;
		}
	} else {
		/* normal protection config check */
		if (LIM_IS_AP_ROLE(pe_session) &&
		    !pe_session->cfgProtection.fromllg) {
			/* protection disabled. */
			pe_debug("protection from 11g is disabled");
			return QDF_STATUS_SUCCESS;
		} else if (!LIM_IS_AP_ROLE(pe_session)) {
			if (!mac->lim.cfgProtection.fromllg) {
				/* protection disabled. */
				pe_debug("protection from 11g is disabled");
				return QDF_STATUS_SUCCESS;
			}
		}
	}
	if (enable) {
		/* If we are AP and HT capable, we need to set the HT OP mode */
		/* appropriately. */

		if (LIM_IS_AP_ROLE(pe_session)) {
			if (overlap) {
				pe_session->gLimOverlap11gParams.
				protectionEnabled = true;
				/* 11g exists in overlap BSS. */
				/* need not to change the operating mode to overlap_legacy */
				/* if higher or same protection operating mode is enabled right now. */
				if ((eSIR_HT_OP_MODE_OVERLAP_LEGACY !=
				     pe_session->htOperMode)
				    && (eSIR_HT_OP_MODE_MIXED !=
					pe_session->htOperMode)) {
					pe_session->htOperMode =
						eSIR_HT_OP_MODE_OVERLAP_LEGACY;
				}
				lim_enable_ht_rifs_protection(mac, true, overlap,
							      pBeaconParams,
							      pe_session);
				lim_enable_ht_obss_protection(mac, true, overlap,
							      pBeaconParams,
							      pe_session);
			} else {
				/* 11g is associated to an AP operating in 11n mode. */
				/* Change the HT operating mode to 'mixed mode'. */
				pe_session->gLim11gParams.protectionEnabled =
					true;
				if (eSIR_HT_OP_MODE_MIXED !=
				    pe_session->htOperMode) {
					pe_session->htOperMode =
						eSIR_HT_OP_MODE_MIXED;
					lim_enable_ht_rifs_protection(mac, true,
								      overlap,
								      pBeaconParams,
								      pe_session);
					lim_enable_ht_obss_protection(mac, true,
								      overlap,
								      pBeaconParams,
								      pe_session);
				}
			}
		}
		/* This part is common for staiton as well. */
		if (false == pe_session->beaconParams.llgCoexist) {
			pBeaconParams->llgCoexist =
				pe_session->beaconParams.llgCoexist = true;
			pBeaconParams->paramChangeBitmap |=
				PARAM_llGCOEXIST_CHANGED;
		} else if (true ==
			   pe_session->gLimOverlap11gParams.
			   protectionEnabled) {
			/* As operating mode changed after G station assoc some way to update beacon */
			/* This addresses the issue of mode not changing to - 11 in beacon when OBSS overlap is enabled */
			/* mac->sch.beacon_changed = 1; */
			pBeaconParams->paramChangeBitmap |=
				PARAM_llGCOEXIST_CHANGED;
		}
	} else if (true == pe_session->beaconParams.llgCoexist) {
		/* for AP role. */
		/* we need to take care of HT OP mode change if needed. */
		/* We need to take care of Overlap cases. */

		if (LIM_IS_AP_ROLE(pe_session)) {
			if (overlap) {
				/* Overlap Legacy protection disabled. */
				if (pe_session->gLim11gParams.numSta == 0)
					pe_session->gLimOverlap11gParams.
					protectionEnabled = false;

				/* no HT op mode change if any of the overlap protection enabled. */
				if (!
				    (pe_session->gLimOlbcParams.
				     protectionEnabled
				     || pe_session->gLimOverlapHt20Params.
				     protectionEnabled
				     || pe_session->gLimOverlapNonGfParams.
				     protectionEnabled)) {
					/* Check if there is a need to change HT OP mode. */
					if (eSIR_HT_OP_MODE_OVERLAP_LEGACY ==
					    pe_session->htOperMode) {
						lim_enable_ht_rifs_protection(mac,
									      false,
									      overlap,
									      pBeaconParams,
									      pe_session);
						lim_enable_ht_obss_protection(mac,
									      false,
									      overlap,
									      pBeaconParams,
									      pe_session);

						if (pe_session->gLimHt20Params.protectionEnabled) {
						if (eHT_CHANNEL_WIDTH_20MHZ ==
							pe_session->htSupportedChannelWidthSet)
							pe_session->htOperMode =
								eSIR_HT_OP_MODE_PURE;
						else
							pe_session->htOperMode =
								eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT;
						} else
							pe_session->htOperMode =
								eSIR_HT_OP_MODE_PURE;
					}
				}
			} else {
				/* Disable protection from 11G stations. */
				pe_session->gLim11gParams.protectionEnabled =
					false;
				/* Check if any other non-HT protection enabled. */
				if (!pe_session->gLim11bParams.
				    protectionEnabled) {

					/* Right now we are in HT OP Mixed mode. */
					/* Change HT op mode appropriately. */
					lim_enable_ht_obss_protection(mac, false,
								      overlap,
								      pBeaconParams,
								      pe_session);

					/* Change HT OP mode to 01 if any overlap protection enabled */
					if (pe_session->gLimOlbcParams.
					    protectionEnabled
					    || pe_session->
					    gLimOverlap11gParams.
					    protectionEnabled
					    || pe_session->
					    gLimOverlapHt20Params.
					    protectionEnabled
					    || pe_session->
					    gLimOverlapNonGfParams.
					    protectionEnabled) {
						pe_session->htOperMode =
							eSIR_HT_OP_MODE_OVERLAP_LEGACY;
						lim_enable_ht_rifs_protection(mac,
									      true,
									      overlap,
									      pBeaconParams,
									      pe_session);
					} else if (pe_session->
						   gLimHt20Params.
						   protectionEnabled) {
						/* Commenting because of CR 258588 WFA cert */
						/* pe_session->htOperMode = eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT; */
						pe_session->htOperMode =
							eSIR_HT_OP_MODE_PURE;
						lim_enable_ht_rifs_protection(mac,
									      false,
									      overlap,
									      pBeaconParams,
									      pe_session);
					} else {
						pe_session->htOperMode =
							eSIR_HT_OP_MODE_PURE;
						lim_enable_ht_rifs_protection(mac,
									      false,
									      overlap,
									      pBeaconParams,
									      pe_session);
					}
				}
			}
			if (!pe_session->gLimOverlap11gParams.
			    protectionEnabled
			    && !pe_session->gLim11gParams.
			    protectionEnabled) {
				pe_debug("===> Protection from 11G Disabled");
				pBeaconParams->llgCoexist =
					pe_session->beaconParams.llgCoexist =
						false;
				pBeaconParams->paramChangeBitmap |=
					PARAM_llGCOEXIST_CHANGED;
			}
		}
		/* for station role */
		else {
			pe_debug("===> Protection from 11G Disabled");
			pBeaconParams->llgCoexist =
				pe_session->beaconParams.llgCoexist = false;
			pBeaconParams->paramChangeBitmap |=
				PARAM_llGCOEXIST_CHANGED;
		}
	}
	return QDF_STATUS_SUCCESS;
}

/* FIXME_PROTECTION : need to check for no APSD whenever we want to enable this protection. */
/* This check will be done at the caller. */

/** -------------------------------------------------------------
   \fn limEnableHtObssProtection
   \brief based on cofig enables\disables obss protection.
   \param      uint8_t enable : 1=> enable protection, 0=> disable protection.
   \param      uint8_t overlap: 1=> called from overlap context, 0 => called from assoc context.
   \param      tpUpdateBeaconParams pBeaconParams
   \return      None
   -------------------------------------------------------------*/
QDF_STATUS
lim_enable_ht_obss_protection(struct mac_context *mac, uint8_t enable,
			      uint8_t overlap, tpUpdateBeaconParams pBeaconParams,
			      struct pe_session *pe_session)
{

	if (!pe_session->htCapability)
		return QDF_STATUS_SUCCESS;  /* this protection  is only for HT stations. */

	/* overlapping protection configuration check. */
	if (overlap) {
		/* overlapping protection configuration check. */
	} else {
		/* normal protection config check */
		if ((LIM_IS_AP_ROLE(pe_session)) &&
		    !pe_session->cfgProtection.obss) { /* ToDo Update this field */
			/* protection disabled. */
			pe_debug("protection from Obss is disabled");
			return QDF_STATUS_SUCCESS;
		} else if (!LIM_IS_AP_ROLE(pe_session)) {
			if (!mac->lim.cfgProtection.obss) { /* ToDo Update this field */
				/* protection disabled. */
				pe_debug("protection from Obss is disabled");
				return QDF_STATUS_SUCCESS;
			}
		}
	}

	if (LIM_IS_AP_ROLE(pe_session)) {
		if ((enable)
		    && (false == pe_session->beaconParams.gHTObssMode)) {
			pe_debug("=>obss protection enabled");
			pe_session->beaconParams.gHTObssMode = true;
			pBeaconParams->paramChangeBitmap |= PARAM_OBSS_MODE_CHANGED; /* UPDATE AN ENUM FOR OBSS MODE <todo> */

		} else if (!enable
			   && (true ==
			       pe_session->beaconParams.gHTObssMode)) {
			pe_debug("===> obss Protection disabled");
			pe_session->beaconParams.gHTObssMode = false;
			pBeaconParams->paramChangeBitmap |=
				PARAM_OBSS_MODE_CHANGED;

		}
/* CR-263021: OBSS bit is not switching back to 0 after disabling the overlapping legacy BSS */
		if (!enable && !overlap) {
			pe_session->gLimOverlap11gParams.protectionEnabled =
				false;
		}
	} else {
		if ((enable)
		    && (false == pe_session->beaconParams.gHTObssMode)) {
			pe_debug("=>obss protection enabled");
			pe_session->beaconParams.gHTObssMode = true;
			pBeaconParams->paramChangeBitmap |= PARAM_OBSS_MODE_CHANGED; /* UPDATE AN ENUM FOR OBSS MODE <todo> */

		} else if (!enable
			   && (true ==
			       pe_session->beaconParams.gHTObssMode)) {
			pe_debug("===> obss Protection disabled");
			pe_session->beaconParams.gHTObssMode = false;
			pBeaconParams->paramChangeBitmap |=
				PARAM_OBSS_MODE_CHANGED;

		}
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * lim_handle_ht20protection_enabled() - Handle ht20 protection  enabled
 * @mac_ctx: pointer to Gloal Mac Structure
 * @overlap: variable for overlap detection
 * @beaconparams: pointer to tpUpdateBeaconParams
 * @session_entry: pointer to struct pe_session *
 *
 * Function handles ht20 protection enabled
 *
 * Return: none
 */
static void lim_handle_ht20protection_enabled(struct mac_context *mac_ctx,
			uint8_t overlap, tpUpdateBeaconParams beaconparams,
			struct pe_session *session_entry)
{
	/*
	 * If we are AP and HT capable, we need to set the HT OP mode
	 * appropriately.
	 */
	if (LIM_IS_AP_ROLE(session_entry) && overlap) {
		session_entry->gLimOverlapHt20Params.protectionEnabled = true;
		if ((eSIR_HT_OP_MODE_OVERLAP_LEGACY !=
				session_entry->htOperMode) &&
			(eSIR_HT_OP_MODE_MIXED !=
				session_entry->htOperMode)) {
			session_entry->htOperMode =
				eSIR_HT_OP_MODE_OVERLAP_LEGACY;
			lim_enable_ht_rifs_protection(mac_ctx, true,
				overlap, beaconparams, session_entry);
		}
	} else if (LIM_IS_AP_ROLE(session_entry) && !overlap) {
		session_entry->gLimHt20Params.protectionEnabled = true;
		if (eSIR_HT_OP_MODE_PURE == session_entry->htOperMode) {
			if (session_entry->htSupportedChannelWidthSet !=
					eHT_CHANNEL_WIDTH_20MHZ)
				 session_entry->htOperMode =
					eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT;
			lim_enable_ht_rifs_protection(mac_ctx, false,
				overlap, beaconparams, session_entry);
			lim_enable_ht_obss_protection(mac_ctx, false,
				overlap, beaconparams, session_entry);
		}
	}
	/* This part is common for staiton as well. */
	if (false == session_entry->beaconParams.ht20Coexist) {
		pe_debug("=> Protection from HT20 Enabled");
		beaconparams->ht20MhzCoexist =
			session_entry->beaconParams.ht20Coexist = true;
		beaconparams->paramChangeBitmap |=
			PARAM_HT20MHZCOEXIST_CHANGED;
	}
}

/**
 * lim_handle_ht20coexist_ht20protection() - ht20 protection for ht20 coexist
 * @mac_ctx: pointer to Gloal Mac Structure
 * @beaconparams: pointer to tpUpdateBeaconParams
 * @session_entry: pointer to struct pe_session *
 * @overlap: variable for overlap detection
 *
 * Function handles ht20 protection for ht20 coexist
 *
 * Return: none
 */
static void lim_handle_ht20coexist_ht20protection(struct mac_context *mac_ctx,
			tpUpdateBeaconParams beaconparams,
			struct pe_session *session_entry, uint8_t overlap)
{
	/*
	 * For AP role:
	 * we need to take care of HT OP mode change if needed.
	 * We need to take care of Overlap cases.
	 */
	if (LIM_IS_AP_ROLE(session_entry) && overlap) {
		/* Overlap Legacy protection disabled. */
		session_entry->gLimOverlapHt20Params.protectionEnabled =
			false;
		/*
		 * no HT op mode change if any of the overlap
		 * protection enabled.
		 */
		if (!(session_entry->gLimOlbcParams.protectionEnabled ||
			session_entry->gLimOverlap11gParams.protectionEnabled ||
			session_entry->gLimOverlapHt20Params.protectionEnabled
			|| session_entry->gLimOverlapNonGfParams.
				protectionEnabled) &&
			/*
			 * Check if there is a need to change HT
			 * OP mode.
			 */
			(eSIR_HT_OP_MODE_OVERLAP_LEGACY ==
				session_entry->htOperMode)) {
			if (session_entry->gLimHt20Params.
				protectionEnabled) {
				if (eHT_CHANNEL_WIDTH_20MHZ ==
					session_entry->
					htSupportedChannelWidthSet)
					session_entry->htOperMode =
						eSIR_HT_OP_MODE_PURE;
				else
					session_entry->htOperMode =
					eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT;

				lim_enable_ht_rifs_protection(mac_ctx,
						false, overlap, beaconparams,
						session_entry);
				lim_enable_ht_obss_protection(mac_ctx,
						false, overlap, beaconparams,
						session_entry);
			} else {
				session_entry->htOperMode =
					eSIR_HT_OP_MODE_PURE;
			}
		}
	} else if (LIM_IS_AP_ROLE(session_entry) && !overlap) {
		/* Disable protection from 11G stations. */
		session_entry->gLimHt20Params.protectionEnabled = false;
		/* Change HT op mode appropriately. */
		if (eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT ==
				session_entry->htOperMode) {
			session_entry->htOperMode =
				eSIR_HT_OP_MODE_PURE;
			lim_enable_ht_rifs_protection(mac_ctx, false,
					overlap, beaconparams, session_entry);
			lim_enable_ht_obss_protection(mac_ctx, false,
					overlap, beaconparams, session_entry);
		}
	}
	if (LIM_IS_AP_ROLE(session_entry)) {
		pe_debug("===> Protection from HT 20 Disabled");
		beaconparams->ht20MhzCoexist =
			session_entry->beaconParams.ht20Coexist = false;
		beaconparams->paramChangeBitmap |=
			PARAM_HT20MHZCOEXIST_CHANGED;
	}
	if (!LIM_IS_AP_ROLE(session_entry)) {
		/* For station role */
		pe_debug("===> Protection from HT20 Disabled");
		beaconparams->ht20MhzCoexist =
			session_entry->beaconParams.ht20Coexist = false;
		beaconparams->paramChangeBitmap |=
			PARAM_HT20MHZCOEXIST_CHANGED;
	}
}

/**
 * lim_enable_ht20_protection() -  Function to enable ht20 protection
 * @mac_ctx: pointer to Global Mac structure
 * @enable: 1=> enable protection, 0=> disable protection.
 * @overlap: 1=> called from overlap context, 0 => called from assoc context.
 * @beaconparams: pointer to tpUpdateBeaconParams
 * @session_entry: pointer to struct pe_session *
 *
 * based on cofig enables\disables protection from Ht20
 *
 * Return: 0 - success
 */
QDF_STATUS lim_enable_ht20_protection(struct mac_context *mac_ctx, uint8_t enable,
			   uint8_t overlap, tpUpdateBeaconParams beaconparams,
			   struct pe_session *session_entry)
{
	/* This protection  is only for HT stations. */
	if (!session_entry->htCapability)
		return QDF_STATUS_SUCCESS;

	/* overlapping protection configuration check. */
	if (!overlap) {
		/* normal protection config check */
		if ((LIM_IS_AP_ROLE(session_entry)) &&
		    !session_entry->cfgProtection.ht20) {
			/* protection disabled. */
			pe_debug("protection from HT20 is disabled");
			return QDF_STATUS_SUCCESS;
		} else if (!LIM_IS_AP_ROLE(session_entry)) {
			if (!mac_ctx->lim.cfgProtection.ht20) {
				/* protection disabled. */
				pe_debug("protection from HT20 is disabled");
				return QDF_STATUS_SUCCESS;
			}
		}
	}

	if (enable)
		lim_handle_ht20protection_enabled(mac_ctx, overlap,
				beaconparams, session_entry);
	else if (true == session_entry->beaconParams.ht20Coexist)
		lim_handle_ht20coexist_ht20protection(mac_ctx, beaconparams,
					session_entry, overlap);

	return QDF_STATUS_SUCCESS;
}

/** -------------------------------------------------------------
   \fn lim_enable_ht_non_gf_protection
   \brief based on cofig enables\disables protection from NonGf.
   \param      uint8_t enable : 1=> enable protection, 0=> disable protection.
   \param      uint8_t overlap: 1=> called from overlap context, 0 => called from assoc context.
   \param      tpUpdateBeaconParams pBeaconParams
   \return      None
   -------------------------------------------------------------*/
QDF_STATUS
lim_enable_ht_non_gf_protection(struct mac_context *mac, uint8_t enable,
				uint8_t overlap, tpUpdateBeaconParams pBeaconParams,
				struct pe_session *pe_session)
{
	if (!pe_session->htCapability)
		return QDF_STATUS_SUCCESS;  /* this protection  is only for HT stations. */

	/* overlapping protection configuration check. */
	if (overlap) {
	} else {
		/* normal protection config check */
		if (LIM_IS_AP_ROLE(pe_session) &&
		    !pe_session->cfgProtection.nonGf) {
			/* protection disabled. */
			pe_debug("protection from NonGf is disabled");
			return QDF_STATUS_SUCCESS;
		} else if (!LIM_IS_AP_ROLE(pe_session)) {
			/* normal protection config check */
			if (!mac->lim.cfgProtection.nonGf) {
				/* protection disabled. */
				pe_debug("protection from NonGf is disabled");
				return QDF_STATUS_SUCCESS;
			}
		}
	}
	if (LIM_IS_AP_ROLE(pe_session)) {
		if ((enable)
		    && (false == pe_session->beaconParams.llnNonGFCoexist)) {
			pe_debug(" => Protection from non GF Enabled");
			pBeaconParams->llnNonGFCoexist =
				pe_session->beaconParams.llnNonGFCoexist = true;
			pBeaconParams->paramChangeBitmap |=
				PARAM_NON_GF_DEVICES_PRESENT_CHANGED;
		} else if (!enable
			   && (true ==
			       pe_session->beaconParams.llnNonGFCoexist)) {
			pe_debug("===> Protection from Non GF Disabled");
			pBeaconParams->llnNonGFCoexist =
				pe_session->beaconParams.llnNonGFCoexist = false;
			pBeaconParams->paramChangeBitmap |=
				PARAM_NON_GF_DEVICES_PRESENT_CHANGED;
		}
	} else {
		if ((enable)
		    && (false == pe_session->beaconParams.llnNonGFCoexist)) {
			pe_debug(" => Protection from non GF Enabled");
			pBeaconParams->llnNonGFCoexist =
				pe_session->beaconParams.llnNonGFCoexist = true;
			pBeaconParams->paramChangeBitmap |=
				PARAM_NON_GF_DEVICES_PRESENT_CHANGED;
		} else if (!enable
			   && (true ==
			       pe_session->beaconParams.llnNonGFCoexist)) {
			pe_debug("===> Protection from Non GF Disabled");
			pBeaconParams->llnNonGFCoexist =
				pe_session->beaconParams.llnNonGFCoexist = false;
			pBeaconParams->paramChangeBitmap |=
				PARAM_NON_GF_DEVICES_PRESENT_CHANGED;
		}
	}

	return QDF_STATUS_SUCCESS;
}

/** -------------------------------------------------------------
   \fn lim_enable_ht_lsig_txop_protection
   \brief based on cofig enables\disables LsigTxop protection.
   \param      uint8_t enable : 1=> enable protection, 0=> disable protection.
   \param      uint8_t overlap: 1=> called from overlap context, 0 => called from assoc context.
   \param      tpUpdateBeaconParams pBeaconParams
   \return      None
   -------------------------------------------------------------*/
QDF_STATUS
lim_enable_ht_lsig_txop_protection(struct mac_context *mac, uint8_t enable,
				   uint8_t overlap,
				   tpUpdateBeaconParams pBeaconParams,
				   struct pe_session *pe_session)
{
	if (!pe_session->htCapability)
		return QDF_STATUS_SUCCESS;  /* this protection  is only for HT stations. */

	/* overlapping protection configuration check. */
	if (overlap) {
	} else {
		/* normal protection config check */
		if (LIM_IS_AP_ROLE(pe_session) &&
			!pe_session->cfgProtection.lsigTxop) {
			/* protection disabled. */
			pe_debug("protection from LsigTxop not supported is disabled");
			return QDF_STATUS_SUCCESS;
		} else if (!LIM_IS_AP_ROLE(pe_session)) {
			/* normal protection config check */
			if (!mac->lim.cfgProtection.lsigTxop) {
				/* protection disabled. */
				pe_debug("protection from LsigTxop not supported is disabled");
				return QDF_STATUS_SUCCESS;
			}
		}
	}

	if (LIM_IS_AP_ROLE(pe_session)) {
		if ((enable)
		    && (false ==
			pe_session->beaconParams.
			fLsigTXOPProtectionFullSupport)) {
			pe_debug(" => Protection from LsigTxop Enabled");
			pBeaconParams->fLsigTXOPProtectionFullSupport =
				pe_session->beaconParams.
				fLsigTXOPProtectionFullSupport = true;
			pBeaconParams->paramChangeBitmap |=
				PARAM_LSIG_TXOP_FULL_SUPPORT_CHANGED;
		} else if (!enable
			   && (true ==
			       pe_session->beaconParams.
			       fLsigTXOPProtectionFullSupport)) {
			pe_debug("===> Protection from LsigTxop Disabled");
			pBeaconParams->fLsigTXOPProtectionFullSupport =
				pe_session->beaconParams.
				fLsigTXOPProtectionFullSupport = false;
			pBeaconParams->paramChangeBitmap |=
				PARAM_LSIG_TXOP_FULL_SUPPORT_CHANGED;
		}
	} else {
		if ((enable)
		    && (false ==
			pe_session->beaconParams.
			fLsigTXOPProtectionFullSupport)) {
			pe_debug(" => Protection from LsigTxop Enabled");
			pBeaconParams->fLsigTXOPProtectionFullSupport =
				pe_session->beaconParams.
				fLsigTXOPProtectionFullSupport = true;
			pBeaconParams->paramChangeBitmap |=
				PARAM_LSIG_TXOP_FULL_SUPPORT_CHANGED;
		} else if (!enable
			   && (true ==
			       pe_session->beaconParams.
			       fLsigTXOPProtectionFullSupport)) {
			pe_debug("===> Protection from LsigTxop Disabled");
			pBeaconParams->fLsigTXOPProtectionFullSupport =
				pe_session->beaconParams.
				fLsigTXOPProtectionFullSupport = false;
			pBeaconParams->paramChangeBitmap |=
				PARAM_LSIG_TXOP_FULL_SUPPORT_CHANGED;
		}
	}
	return QDF_STATUS_SUCCESS;
}

/* FIXME_PROTECTION : need to check for no APSD whenever we want to enable this protection. */
/* This check will be done at the caller. */
/** -------------------------------------------------------------
   \fn lim_enable_ht_rifs_protection
   \brief based on cofig enables\disables Rifs protection.
   \param      uint8_t enable : 1=> enable protection, 0=> disable protection.
   \param      uint8_t overlap: 1=> called from overlap context, 0 => called from assoc context.
   \param      tpUpdateBeaconParams pBeaconParams
   \return      None
   -------------------------------------------------------------*/
QDF_STATUS
lim_enable_ht_rifs_protection(struct mac_context *mac, uint8_t enable,
			      uint8_t overlap, tpUpdateBeaconParams pBeaconParams,
			      struct pe_session *pe_session)
{
	if (!pe_session->htCapability)
		return QDF_STATUS_SUCCESS;  /* this protection  is only for HT stations. */

	/* overlapping protection configuration check. */
	if (overlap) {
	} else {
		/* normal protection config check */
		if (LIM_IS_AP_ROLE(pe_session) &&
		    !pe_session->cfgProtection.rifs) {
			/* protection disabled. */
			pe_debug("protection from Rifs is disabled");
			return QDF_STATUS_SUCCESS;
		} else if (!LIM_IS_AP_ROLE(pe_session)) {
			/* normal protection config check */
			if (!mac->lim.cfgProtection.rifs) {
				/* protection disabled. */
				pe_debug("protection from Rifs is disabled");
				return QDF_STATUS_SUCCESS;
			}
		}
	}

	if (LIM_IS_AP_ROLE(pe_session)) {
		/* Disabling the RIFS Protection means Enable the RIFS mode of operation in the BSS */
		if ((!enable)
		    && (false == pe_session->beaconParams.fRIFSMode)) {
			pe_debug(" => Rifs protection Disabled");
			pBeaconParams->fRIFSMode =
				pe_session->beaconParams.fRIFSMode = true;
			pBeaconParams->paramChangeBitmap |=
				PARAM_RIFS_MODE_CHANGED;
		}
		/* Enabling the RIFS Protection means Disable the RIFS mode of operation in the BSS */
		else if (enable
			 && (true == pe_session->beaconParams.fRIFSMode)) {
			pe_debug("===> Rifs Protection Enabled");
			pBeaconParams->fRIFSMode =
				pe_session->beaconParams.fRIFSMode = false;
			pBeaconParams->paramChangeBitmap |=
				PARAM_RIFS_MODE_CHANGED;
		}
	} else {
		/* Disabling the RIFS Protection means Enable the RIFS mode of operation in the BSS */
		if ((!enable)
		    && (false == pe_session->beaconParams.fRIFSMode)) {
			pe_debug(" => Rifs protection Disabled");
			pBeaconParams->fRIFSMode =
				pe_session->beaconParams.fRIFSMode = true;
			pBeaconParams->paramChangeBitmap |=
				PARAM_RIFS_MODE_CHANGED;
		}
		/* Enabling the RIFS Protection means Disable the RIFS mode of operation in the BSS */
		else if (enable
			 && (true == pe_session->beaconParams.fRIFSMode)) {
			pe_debug("===> Rifs Protection Enabled");
			pBeaconParams->fRIFSMode =
				pe_session->beaconParams.fRIFSMode = false;
			pBeaconParams->paramChangeBitmap |=
				PARAM_RIFS_MODE_CHANGED;
		}
	}
	return QDF_STATUS_SUCCESS;
}

/* --------------------------------------------------------------------- */
/**
 * lim_enable_short_preamble
 *
 * FUNCTION:
 * Enable/Disable short preamble
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param enable        Flag to enable/disable short preamble
 * @return None
 */

QDF_STATUS
lim_enable_short_preamble(struct mac_context *mac, uint8_t enable,
			  tpUpdateBeaconParams pBeaconParams,
			  struct pe_session *pe_session)
{
	if (!mac->mlme_cfg->ht_caps.short_preamble)
		return QDF_STATUS_SUCCESS;

	/* 11G short preamble switching is disabled. */
	if (!mac->mlme_cfg->feature_flags.enable_short_preamble_11g)
		return QDF_STATUS_SUCCESS;

	if (LIM_IS_AP_ROLE(pe_session)) {
		if (enable && (pe_session->beaconParams.fShortPreamble == 0)) {
			pe_debug("===> Short Preamble Enabled");
			pe_session->beaconParams.fShortPreamble = true;
			pBeaconParams->fShortPreamble =
				(uint8_t) pe_session->beaconParams.
				fShortPreamble;
			pBeaconParams->paramChangeBitmap |=
				PARAM_SHORT_PREAMBLE_CHANGED;
		} else if (!enable
			   && (pe_session->beaconParams.fShortPreamble ==
			       1)) {
			pe_debug("===> Short Preamble Disabled");
			pe_session->beaconParams.fShortPreamble = false;
			pBeaconParams->fShortPreamble =
				(uint8_t) pe_session->beaconParams.
				fShortPreamble;
			pBeaconParams->paramChangeBitmap |=
				PARAM_SHORT_PREAMBLE_CHANGED;
		}
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * lim_tx_complete
 *
 * Function:
 * This is LIM's very own "TX MGMT frame complete" completion routine.
 *
 * Logic:
 * LIM wants to send a MGMT frame (broadcast or unicast)
 * LIM allocates memory using cds_packet_alloc( ..., **pData, **pPacket )
 * LIM transmits the MGMT frame using the API:
 *  wma_tx_frame( ... pPacket, ..., (void *) lim_tx_complete, pData )
 * HDD, via wma_tx_frame/DXE, "transfers" the packet over to BMU
 * HDD, if it determines that a TX completion routine (in this case
 * lim_tx_complete) has been provided, will invoke this callback
 * LIM will try to free the TX MGMT packet that was earlier allocated, in order
 * to send this MGMT frame, using the PAL API cds_packet_free( ... pData, pPacket )
 *
 * Assumptions:
 * Presently, this is ONLY being used for MGMT frames/packets
 * TODO:
 * Would it do good for LIM to have some sort of "signature" validation to
 * ensure that the pData argument passed in was a buffer that was actually
 * allocated by LIM and/or is not corrupted?
 *
 * Note: FIXME and TODO
 * Looks like cds_packet_free() is interested in pPacket. But, when this completion
 * routine is called, only pData is made available to LIM!!
 *
 * @param void A pointer to pData. Shouldn't it be pPacket?!
 *
 * @return QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS lim_tx_complete(void *context, qdf_nbuf_t buf, bool free)
{
	if (free)
		cds_packet_free((void *)buf);

	return QDF_STATUS_SUCCESS;
}

static void lim_ht_switch_chnl_params(struct pe_session *pe_session)
{
	uint8_t center_freq = 0;
	enum phy_ch_width ch_width = CH_WIDTH_20MHZ;
	struct mac_context *mac;
	uint8_t primary_channel;

	mac = pe_session->mac_ctx;
	if (!mac) {
		pe_err("Invalid mac_ctx");
		return;
	}

	primary_channel = wlan_reg_freq_to_chan(mac->pdev,
						 pe_session->curr_op_freq);
	if (eHT_CHANNEL_WIDTH_40MHZ ==
	    pe_session->htRecommendedTxWidthSet) {
		ch_width = CH_WIDTH_40MHZ;
		if (PHY_DOUBLE_CHANNEL_LOW_PRIMARY ==
		    pe_session->htSecondaryChannelOffset)
			center_freq = primary_channel + 2;
		else if (PHY_DOUBLE_CHANNEL_HIGH_PRIMARY ==
			 pe_session->htSecondaryChannelOffset)
			center_freq = primary_channel - 2;
		else
			ch_width = CH_WIDTH_20MHZ;
	}
	pe_session->gLimChannelSwitch.primaryChannel = primary_channel;
	pe_session->curr_req_chan_freq = pe_session->curr_op_freq;
	pe_session->ch_center_freq_seg0 = center_freq;
	pe_session->gLimChannelSwitch.ch_center_freq_seg0 = center_freq;
	pe_session->gLimChannelSwitch.sw_target_freq =
						pe_session->curr_op_freq;
	pe_session->ch_width = ch_width;
	pe_session->gLimChannelSwitch.ch_width = ch_width;
	pe_session->gLimChannelSwitch.sec_ch_offset =
		pe_session->htSecondaryChannelOffset;
	pe_session->gLimChannelSwitch.ch_center_freq_seg1 = 0;

	pe_debug("HT IE changed: Primary Channel: %d center chan: %d Channel Width: %d cur op freq %d",
		 primary_channel, center_freq,
		 pe_session->htRecommendedTxWidthSet,
		 pe_session->gLimChannelSwitch.sw_target_freq);
	pe_session->channelChangeReasonCode =
			LIM_SWITCH_CHANNEL_HT_WIDTH;
	mac->lim.gpchangeChannelCallback = lim_switch_channel_cback;
	mac->lim.gpchangeChannelData = NULL;

	lim_send_switch_chnl_params(mac, pe_session);
}

static void lim_ht_switch_chnl_req(struct pe_session *session)
{
	struct mac_context *mac;
	QDF_STATUS status;

	mac = session->mac_ctx;
	if (!mac) {
		pe_err("Invalid mac context");
		return;
	}

	session->channelChangeReasonCode =
			LIM_SWITCH_CHANNEL_HT_WIDTH;
	mlme_set_chan_switch_in_progress(session->vdev, true);
	status = wlan_vdev_mlme_sm_deliver_evt(
					session->vdev,
					WLAN_VDEV_SM_EV_FW_VDEV_RESTART,
					sizeof(*session),
					session);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("Failed to post WLAN_VDEV_SM_EV_FW_VDEV_RESTART for vdevid %d",
		       session->smeSessionId);
		mlme_set_chan_switch_in_progress(session->vdev, false);
	}
}

void lim_update_sta_run_time_ht_switch_chnl_params(struct mac_context *mac,
						   tDot11fIEHTInfo *pHTInfo,
						   struct pe_session *pe_session)
{
	/* If self capability is set to '20Mhz only', then do not change the CB mode. */
	if (!lim_get_ht_capability
		    (mac, eHT_SUPPORTED_CHANNEL_WIDTH_SET, pe_session))
		return;

	if (wlan_reg_is_24ghz_ch_freq(pe_session->curr_op_freq) &&
	    pe_session->force_24ghz_in_ht20) {
		pe_debug("force_24ghz_in_ht20 is set and channel is 2.4 Ghz");
		return;
	}

	if (pe_session->ftPEContext.ftPreAuthSession) {
		pe_err("FT PREAUTH channel change is in progress");
		return;
	}

	if (pe_session->ch_switch_in_progress == true) {
		pe_debug("ch switch is in progress, ignore HT IE BW update");
		return;
	}

	if (wlan_reg_get_chan_enum(pHTInfo->primaryChannel) ==
	    INVALID_CHANNEL) {
		pe_debug("Ignore Invalid channel in HT info");
		return;
	}

	/* If channel mismatch the CSA will take care of this change */
	if (pHTInfo->primaryChannel != wlan_reg_freq_to_chan(
			mac->pdev, pe_session->curr_op_freq)) {
		pe_debug("Current channel doesnt match HT info ignore");
		return;
	}

	if (lim_is_roam_synch_in_progress(mac->psoc, pe_session)) {
		pe_debug("Roaming in progress, ignore HT IE BW update");
		return;
	}

	if (pe_session->htSecondaryChannelOffset !=
	    (uint8_t) pHTInfo->secondaryChannelOffset
	    || pe_session->htRecommendedTxWidthSet !=
	    (uint8_t) pHTInfo->recommendedTxWidthSet) {
		pe_session->htSecondaryChannelOffset =
			(ePhyChanBondState) pHTInfo->secondaryChannelOffset;
		pe_session->htRecommendedTxWidthSet =
			(uint8_t) pHTInfo->recommendedTxWidthSet;
		pe_session->htSupportedChannelWidthSet =
			pe_session->htRecommendedTxWidthSet;

		/* Before restarting vdev, delete the tdls peers */
		lim_update_tdls_set_state_for_fw(pe_session, false);
		lim_delete_tdls_peers(mac, pe_session);

		lim_ht_switch_chnl_req(pe_session);
	}

} /* End limUpdateStaRunTimeHTParams. */

/**
 * \brief This function updates the lim global structure, if any of the
 * HT Capabilities have changed.
 *
 *
 * \param mac Pointer to Global MAC structure
 *
 * \param pHTCapability Pointer to HT Capability Information Element
 * obtained from a Beacon or Probe Response
 *
 *
 *
 */

void lim_update_sta_run_time_ht_capability(struct mac_context *mac,
					   tDot11fIEHTCaps *pHTCaps)
{

	if (mac->lim.gHTLsigTXOPProtection !=
	    (uint8_t) pHTCaps->lsigTXOPProtection) {
		mac->lim.gHTLsigTXOPProtection =
			(uint8_t) pHTCaps->lsigTXOPProtection;
		/* Send change notification to HAL */
	}

	if (mac->lim.gHTAMpduDensity != (uint8_t) pHTCaps->mpduDensity) {
		mac->lim.gHTAMpduDensity = (uint8_t) pHTCaps->mpduDensity;
		/* Send change notification to HAL */
	}

	if (mac->lim.gHTMaxRxAMpduFactor !=
	    (uint8_t) pHTCaps->maxRxAMPDUFactor) {
		mac->lim.gHTMaxRxAMpduFactor =
			(uint8_t) pHTCaps->maxRxAMPDUFactor;
		/* Send change notification to HAL */
	}

} /* End lim_update_sta_run_time_ht_capability. */

/**
 * \brief This function updates lim global structure, if any of the HT
 * Info Parameters have changed.
 *
 *
 * \param mac Pointer to the global MAC structure
 *
 * \param pHTInfo Pointer to the HT Info IE obtained from a Beacon or
 * Probe Response
 *
 *
 */

void lim_update_sta_run_time_ht_info(struct mac_context *mac,
				     tDot11fIEHTInfo *pHTInfo,
				     struct pe_session *pe_session)
{
	if (pe_session->htRecommendedTxWidthSet !=
	    (uint8_t) pHTInfo->recommendedTxWidthSet) {
		pe_session->htRecommendedTxWidthSet =
			(uint8_t) pHTInfo->recommendedTxWidthSet;
		/* Send change notification to HAL */
	}

	if (pe_session->beaconParams.fRIFSMode !=
	    (uint8_t) pHTInfo->rifsMode) {
		pe_session->beaconParams.fRIFSMode =
			(uint8_t) pHTInfo->rifsMode;
		/* Send change notification to HAL */
	}

	if (mac->lim.gHTServiceIntervalGranularity !=
	    (uint8_t) pHTInfo->serviceIntervalGranularity) {
		mac->lim.gHTServiceIntervalGranularity =
			(uint8_t) pHTInfo->serviceIntervalGranularity;
		/* Send change notification to HAL */
	}

	if (mac->lim.gHTOperMode != (tSirMacHTOperatingMode) pHTInfo->opMode) {
		mac->lim.gHTOperMode =
			(tSirMacHTOperatingMode) pHTInfo->opMode;
		/* Send change notification to HAL */
	}

	if (pe_session->beaconParams.llnNonGFCoexist !=
	    pHTInfo->nonGFDevicesPresent) {
		pe_session->beaconParams.llnNonGFCoexist =
			(uint8_t) pHTInfo->nonGFDevicesPresent;
	}

	if (mac->lim.gHTSTBCBasicMCS != (uint8_t) pHTInfo->basicSTBCMCS) {
		mac->lim.gHTSTBCBasicMCS = (uint8_t) pHTInfo->basicSTBCMCS;
		/* Send change notification to HAL */
	}

	if (mac->lim.gHTDualCTSProtection !=
	    (uint8_t) pHTInfo->dualCTSProtection) {
		mac->lim.gHTDualCTSProtection =
			(uint8_t) pHTInfo->dualCTSProtection;
		/* Send change notification to HAL */
	}

	if (mac->lim.gHTSecondaryBeacon != (uint8_t) pHTInfo->secondaryBeacon) {
		mac->lim.gHTSecondaryBeacon =
			(uint8_t) pHTInfo->secondaryBeacon;
		/* Send change notification to HAL */
	}

	if (pe_session->beaconParams.fLsigTXOPProtectionFullSupport !=
	    (uint8_t) pHTInfo->lsigTXOPProtectionFullSupport) {
		pe_session->beaconParams.fLsigTXOPProtectionFullSupport =
			(uint8_t) pHTInfo->lsigTXOPProtectionFullSupport;
		/* Send change notification to HAL */
	}

	if (mac->lim.gHTPCOActive != (uint8_t) pHTInfo->pcoActive) {
		mac->lim.gHTPCOActive = (uint8_t) pHTInfo->pcoActive;
		/* Send change notification to HAL */
	}

	if (mac->lim.gHTPCOPhase != (uint8_t) pHTInfo->pcoPhase) {
		mac->lim.gHTPCOPhase = (uint8_t) pHTInfo->pcoPhase;
		/* Send change notification to HAL */
	}

} /* End lim_update_sta_run_time_ht_info. */

/**
 * lim_validate_delts_req() - This function validates DelTs req
 * @mac_ctx: pointer to Global Mac structure
 * @delts_req: pointer to delete traffic stream structure
 * @peer_mac_addr: variable for peer mac address
 * @session: pe session entry
 *
 * Function validates DelTs req originated by SME or by HAL and also
 * sends halMsg_DelTs to HAL
 *
 * Return: QDF_STATUS_SUCCESS - Success, QDF_STATUS_E_FAILURE - Failure
 */

QDF_STATUS
lim_validate_delts_req(struct mac_context *mac_ctx, tpSirDeltsReq delts_req,
		       tSirMacAddr peer_mac_addr,
		       struct pe_session *session)
{
	tpDphHashNode sta;
	uint8_t ts_status;
	struct mac_ts_info *tsinfo;
	uint32_t i;
	uint8_t tspec_idx;

	/*
	 * if sta
	 *  - verify assoc state
	 *  - del tspec locally
	 * if ap
	 *  - verify sta is in assoc state
	 *  - del sta tspec locally
	 */
	if (!delts_req) {
		pe_err("Delete TS request pointer is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (LIM_IS_STA_ROLE(session)) {
		uint32_t val;

		/* station always talks to the AP */
		sta = dph_get_hash_entry(mac_ctx, DPH_STA_HASH_INDEX_PEER,
					&session->dph.dphHashTable);

		val = sizeof(tSirMacAddr);
		sir_copy_mac_addr(peer_mac_addr, session->bssId);

	} else {
		uint16_t associd;
		uint8_t *macaddr = (uint8_t *) peer_mac_addr;

		associd = delts_req->aid;
		if (associd != 0)
			sta = dph_get_hash_entry(mac_ctx, associd,
					&session->dph.dphHashTable);
		else
			sta = dph_lookup_hash_entry(mac_ctx,
						delts_req->macaddr.bytes,
						&associd,
						&session->dph.
							dphHashTable);

		if (sta)
			/* TBD: check sta assoc state as well */
			for (i = 0; i < sizeof(tSirMacAddr); i++)
				macaddr[i] = sta->staAddr[i];
	}

	if (!sta) {
		pe_err("Cannot find station context for delts req");
		return QDF_STATUS_E_FAILURE;
	}

	if ((!sta->valid) ||
		(sta->mlmStaContext.mlmState !=
			eLIM_MLM_LINK_ESTABLISHED_STATE)) {
		pe_err("Invalid Sta (or state) for DelTsReq");
		return QDF_STATUS_E_FAILURE;
	}

	delts_req->req.wsmTspecPresent = 0;
	delts_req->req.wmeTspecPresent = 0;
	delts_req->req.lleTspecPresent = 0;

	if ((sta->wsmEnabled) &&
		(delts_req->req.tspec.tsinfo.traffic.accessPolicy !=
						SIR_MAC_ACCESSPOLICY_EDCA))
		delts_req->req.wsmTspecPresent = 1;
	else if (sta->wmeEnabled)
		delts_req->req.wmeTspecPresent = 1;
	else if (sta->lleEnabled)
		delts_req->req.lleTspecPresent = 1;
	else {
		pe_warn("DELTS_REQ ignore - qos is disabled");
		return QDF_STATUS_E_FAILURE;
	}

	tsinfo = delts_req->req.wmeTspecPresent ? &delts_req->req.tspec.tsinfo
						: &delts_req->req.tsinfo;
	pe_debug("received DELTS_REQ message wmeTspecPresent: %d lleTspecPresent: %d wsmTspecPresent: %d tsid: %d  up: %d direction: %d",
		delts_req->req.wmeTspecPresent,
		delts_req->req.lleTspecPresent,
		delts_req->req.wsmTspecPresent, tsinfo->traffic.tsid,
		tsinfo->traffic.userPrio, tsinfo->traffic.direction);

	/* if no Access Control, ignore the request */
	if (lim_admit_control_delete_ts(mac_ctx, sta->assocId, tsinfo,
				&ts_status, &tspec_idx) != QDF_STATUS_SUCCESS) {
		pe_err("DELTS request for sta assocId: %d tsid: %d up: %d",
			sta->assocId, tsinfo->traffic.tsid,
			tsinfo->traffic.userPrio);
		return QDF_STATUS_E_FAILURE;
	} else if ((tsinfo->traffic.accessPolicy == SIR_MAC_ACCESSPOLICY_HCCA)
				|| (tsinfo->traffic.accessPolicy ==
			SIR_MAC_ACCESSPOLICY_BOTH)) {
		/* edca only now. */
	} else if (tsinfo->traffic.accessPolicy == SIR_MAC_ACCESSPOLICY_EDCA) {
		/* send message to HAL to delete TS */
		if (QDF_STATUS_SUCCESS !=
			lim_send_hal_msg_del_ts(mac_ctx,
						tspec_idx, delts_req->req,
						session->peSessionId,
						session->bssId)) {
			pe_warn("DelTs with UP: %d failed in lim_send_hal_msg_del_ts - ignoring request",
				tsinfo->traffic.userPrio);
			return QDF_STATUS_E_FAILURE;
		}
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * @function :  lim_post_sm_state_update()
 *
 * @brief  :  This function Updates the HAL and Softmac about the change in the STA's SMPS state.
 *
 *      LOGIC:
 *
 *      ASSUMPTIONS:
 *          NA
 *
 *      NOTE:
 *          NA
 *
 * @param  mac - Pointer to Global MAC structure
 * @param  limMsg - Lim Message structure object with the MimoPSparam in body
 * @return None
 */
QDF_STATUS
lim_post_sm_state_update(struct mac_context *mac,
			 tSirMacHTMIMOPowerSaveState state,
			 uint8_t *pPeerStaMac, uint8_t sessionId)
{
	QDF_STATUS retCode = QDF_STATUS_SUCCESS;
	struct scheduler_msg msgQ = {0};
	tpSetMIMOPS pMIMO_PSParams;

	msgQ.reserved = 0;
	msgQ.type = WMA_SET_MIMOPS_REQ;

	/* Allocate for WMA_SET_MIMOPS_REQ */
	pMIMO_PSParams = qdf_mem_malloc(sizeof(tSetMIMOPS));
	if (!pMIMO_PSParams)
		return QDF_STATUS_E_NOMEM;

	pMIMO_PSParams->htMIMOPSState = state;
	pMIMO_PSParams->fsendRsp = true;
	pMIMO_PSParams->sessionId = sessionId;
	qdf_mem_copy(pMIMO_PSParams->peerMac, pPeerStaMac, sizeof(tSirMacAddr));

	msgQ.bodyptr = pMIMO_PSParams;
	msgQ.bodyval = 0;

	pe_debug("Sending WMA_SET_MIMOPS_REQ");

	MTRACE(mac_trace_msg_tx(mac, NO_SESSION, msgQ.type));
	retCode = wma_post_ctrl_msg(mac, &msgQ);
	if (QDF_STATUS_SUCCESS != retCode) {
		pe_err("Posting WMA_SET_MIMOPS_REQ to HAL failed! Reason: %d",
			retCode);
		qdf_mem_free(pMIMO_PSParams);
		return retCode;
	}

	return retCode;
}

void lim_pkt_free(struct mac_context *mac,
		  eFrameType frmType, uint8_t *pRxPacketInfo, void *pBody)
{
	(void)mac;
	(void)frmType;
	(void)pRxPacketInfo;
	(void)pBody;
}

/**
 * lim_get_b_dfrom_rx_packet()
 *
 ***FUNCTION:
 * This function is called to get pointer to Polaris
 * Buffer Descriptor containing MAC header & other control
 * info from the body of the message posted to LIM.
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
 * @param  body    - Received message body
 * @param  pRxPacketInfo     - Pointer to received BD
 * @return None
 */

void
lim_get_b_dfrom_rx_packet(struct mac_context *mac, void *body, uint32_t **pRxPacketInfo)
{
	*pRxPacketInfo = (uint32_t *) body;
} /*** end lim_get_b_dfrom_rx_packet() ***/

void lim_add_channel_status_info(struct mac_context *p_mac,
				 struct lim_channel_status *channel_stat,
				 uint8_t channel_id)
{
	uint8_t i;
	bool found = false;
	struct lim_scan_channel_status *channel_info =
		&p_mac->lim.scan_channel_status;
	struct lim_channel_status *channel_status_list =
		channel_info->channel_status_list;
	uint8_t total_channel = channel_info->total_channel;

	if (!p_mac->sap.acs_with_more_param)
		return;

	for (i = 0; i < total_channel; i++) {
		if (channel_status_list[i].channel_id == channel_id) {
			if (channel_stat->cmd_flags ==
			    WMI_CHAN_InFO_END_RESP &&
			    channel_status_list[i].cmd_flags ==
			    WMI_CHAN_InFO_START_RESP) {
				/* adjust to delta value for counts */
				channel_stat->rx_clear_count -=
				    channel_status_list[i].rx_clear_count;
				channel_stat->cycle_count -=
				    channel_status_list[i].cycle_count;
				channel_stat->rx_frame_count -=
				    channel_status_list[i].rx_frame_count;
				channel_stat->tx_frame_count -=
				    channel_status_list[i].tx_frame_count;
				channel_stat->bss_rx_cycle_count -=
				    channel_status_list[i].bss_rx_cycle_count;
			}
			qdf_mem_copy(&channel_status_list[i], channel_stat,
				     sizeof(*channel_status_list));
			found = true;
			break;
		}
	}

	if (!found) {
		if (total_channel < SIR_MAX_SUPPORTED_CHANNEL_LIST) {
			qdf_mem_copy(&channel_status_list[total_channel++],
				     channel_stat,
				     sizeof(*channel_status_list));
			channel_info->total_channel = total_channel;
		} else {
			pe_warn("Chan cnt exceed, channel_id=%d", channel_id);
		}
	}

	return;
}

bool lim_is_channel_valid_for_channel_switch(struct mac_context *mac,
					     uint32_t channel_freq)
{
	uint8_t index;
	bool ok = false;

	if (policy_mgr_is_chan_ok_for_dnbs(mac->psoc, channel_freq,
					   &ok)) {
		pe_err("policy_mgr_is_chan_ok_for_dnbs() returned error");
		return false;
	}

	if (!ok) {
		pe_debug("channel not ok for DNBS");
		return false;
	}

	for (index = 0; index < mac->mlme_cfg->reg.valid_channel_list_num; index++) {
		if (mac->mlme_cfg->reg.valid_channel_freq_list[index] !=
		    channel_freq)
			continue;

		ok = policy_mgr_is_valid_for_channel_switch(
				mac->psoc, channel_freq);
		return ok;
	}

	/* channel does not belong to list of valid channels */
	return false;
}

/**
 * @function :  lim_restore_pre_channel_switch_state()
 *
 * @brief  :  This API is called by the user to undo any
 *            specific changes done on the device during
 *            channel switch.
 *      LOGIC:
 *
 *      ASSUMPTIONS:
 *          NA
 *
 *      NOTE:
 *          NA
 *
 * @param  mac - Pointer to Global MAC structure
 * @return None
 */

QDF_STATUS
lim_restore_pre_channel_switch_state(struct mac_context *mac, struct pe_session *pe_session)
{

	QDF_STATUS retCode = QDF_STATUS_SUCCESS;

	if (!LIM_IS_STA_ROLE(pe_session))
		return retCode;

	/* Channel switch should be ready for the next time */
	pe_session->gLimSpecMgmt.dot11hChanSwState = eLIM_11H_CHANSW_INIT;

	return retCode;
}

/**
 * @function: lim_prepare_for11h_channel_switch()
 *
 * @brief  :  This API is called by the user to prepare for
 *            11h channel switch. As of now, the API does
 *            very minimal work. User can add more into the
 *            same API if needed.
 *      LOGIC:
 *
 *      ASSUMPTIONS:
 *          NA
 *
 *      NOTE:
 *          NA
 *
 * @param  mac - Pointer to Global MAC structure
 * @param  pe_session
 * @return None
 */
void
lim_prepare_for11h_channel_switch(struct mac_context *mac, struct pe_session *pe_session)
{
	if (!LIM_IS_STA_ROLE(pe_session))
		return;

	/* Flag to indicate 11h channel switch in progress */
	pe_session->gLimSpecMgmt.dot11hChanSwState = eLIM_11H_CHANSW_RUNNING;

	/** We are safe to switch channel at this point */
	lim_stop_tx_and_switch_channel(mac, pe_session->peSessionId);
}

tSirNwType lim_get_nw_type(struct mac_context *mac, uint32_t chan_freq, uint32_t type,
			   tpSchBeaconStruct pBeacon)
{
	tSirNwType nwType = eSIR_11B_NW_TYPE;

	/* Logic to be cleaned up for 11AC & 11AX */
	if (type == SIR_MAC_DATA_FRAME) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(chan_freq)) {
			nwType = eSIR_11G_NW_TYPE;
		} else {
			nwType = eSIR_11A_NW_TYPE;
		}
	} else {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(chan_freq)) {
			int i;
			/* 11b or 11g packet */
			/* 11g iff extended Rate IE is present or */
			/* if there is an A rate in suppRate IE */
			for (i = 0; i < pBeacon->supportedRates.numRates; i++) {
				if (sirIsArate
					    (pBeacon->supportedRates.rate[i] & 0x7f)) {
					nwType = eSIR_11G_NW_TYPE;
					break;
				}
			}
			if (pBeacon->extendedRatesPresent) {
				nwType = eSIR_11G_NW_TYPE;
			} else if (pBeacon->HTInfo.present ||
				   IS_BSS_VHT_CAPABLE(pBeacon->VHTCaps)) {
				nwType = eSIR_11G_NW_TYPE;
			}
		} else {
			/* 11a packet */
			nwType = eSIR_11A_NW_TYPE;
		}
	}
	return nwType;
}

uint32_t lim_get_channel_from_beacon(struct mac_context *mac, tpSchBeaconStruct pBeacon)
{
	uint8_t chan_freq = 0;

	if (pBeacon->he_op.oper_info_6g_present)
		chan_freq = wlan_reg_chan_band_to_freq(mac->pdev,
						   pBeacon->he_op.oper_info_6g.info.primary_ch,
						   BIT(REG_BAND_6G));
	else if (pBeacon->dsParamsPresent)
		chan_freq = pBeacon->chan_freq;
	else if (pBeacon->HTInfo.present)
		chan_freq = wlan_reg_legacy_chan_to_freq(mac->pdev,
							 pBeacon->HTInfo.primaryChannel);
	else
		chan_freq = pBeacon->chan_freq;

	return chan_freq;
}

void lim_set_tspec_uapsd_mask_per_session(struct mac_context *mac,
					  struct pe_session *pe_session,
					  struct mac_ts_info *pTsInfo,
					  uint32_t action)
{
	uint8_t userPrio = (uint8_t) pTsInfo->traffic.userPrio;
	uint16_t direction = pTsInfo->traffic.direction;
	uint8_t ac = upToAc(userPrio);

	pe_debug("Set UAPSD mask for AC: %d dir: %d action: %d"
			, ac, direction, action);

	/* Converting AC to appropriate Uapsd Bit Mask
	 * AC_BE(0) --> UAPSD_BITOFFSET_ACVO(3)
	 * AC_BK(1) --> UAPSD_BITOFFSET_ACVO(2)
	 * AC_VI(2) --> UAPSD_BITOFFSET_ACVO(1)
	 * AC_VO(3) --> UAPSD_BITOFFSET_ACVO(0)
	 */
	ac = ((~ac) & 0x3);

	if (action == CLEAR_UAPSD_MASK) {
		if (direction == SIR_MAC_DIRECTION_UPLINK)
			pe_session->gUapsdPerAcTriggerEnableMask &=
				~(1 << ac);
		else if (direction == SIR_MAC_DIRECTION_DNLINK)
			pe_session->gUapsdPerAcDeliveryEnableMask &=
				~(1 << ac);
		else if (direction == SIR_MAC_DIRECTION_BIDIR) {
			pe_session->gUapsdPerAcTriggerEnableMask &=
				~(1 << ac);
			pe_session->gUapsdPerAcDeliveryEnableMask &=
				~(1 << ac);
		}
	} else if (action == SET_UAPSD_MASK) {
		if (direction == SIR_MAC_DIRECTION_UPLINK)
			pe_session->gUapsdPerAcTriggerEnableMask |=
				(1 << ac);
		else if (direction == SIR_MAC_DIRECTION_DNLINK)
			pe_session->gUapsdPerAcDeliveryEnableMask |=
				(1 << ac);
		else if (direction == SIR_MAC_DIRECTION_BIDIR) {
			pe_session->gUapsdPerAcTriggerEnableMask |=
				(1 << ac);
			pe_session->gUapsdPerAcDeliveryEnableMask |=
				(1 << ac);
		}
	}

	pe_debug("New pe_session->gUapsdPerAcTriggerEnableMask 0x%x pe_session->gUapsdPerAcDeliveryEnableMask 0x%x",
		pe_session->gUapsdPerAcTriggerEnableMask,
		pe_session->gUapsdPerAcDeliveryEnableMask);

	return;
}

/**
 * lim_handle_heart_beat_timeout_for_session() - Handle heart beat time out
 * @mac_ctx: pointer to Global Mac Structure
 * @psession_entry: pointer to struct pe_session *
 *
 * Function handles heart beat time out for session
 *
 * Return: none
 */
void lim_handle_heart_beat_timeout_for_session(struct mac_context *mac_ctx,
					       struct pe_session *psession_entry)
{
	if (psession_entry->valid) {
		if ((psession_entry->bssType == eSIR_INFRASTRUCTURE_MODE) &&
					(LIM_IS_STA_ROLE(psession_entry)))
			lim_handle_heart_beat_failure(mac_ctx, psession_entry);
	}
	/*
	 * In the function lim_handle_heart_beat_failure things can change
	 * so check for the session entry  valid and the other things
	 * again
	 */
	if ((psession_entry->valid == true) &&
		(psession_entry->bssType == eSIR_INFRASTRUCTURE_MODE) &&
			(LIM_IS_STA_ROLE(psession_entry)) &&
				(psession_entry->LimHBFailureStatus == true)) {
		tLimTimers *lim_timer  = &mac_ctx->lim.lim_timers;
		/*
		 * Activate Probe After HeartBeat Timer incase HB
		 * Failure detected
		 */
		pe_debug("Sending Probe for Session: %d",
			psession_entry->vdev_id);
		lim_deactivate_and_change_timer(mac_ctx,
			eLIM_PROBE_AFTER_HB_TIMER);
		MTRACE(mac_trace(mac_ctx, TRACE_CODE_TIMER_ACTIVATE, 0,
			eLIM_PROBE_AFTER_HB_TIMER));
		if (tx_timer_activate(&lim_timer->gLimProbeAfterHBTimer)
					!= TX_SUCCESS)
			pe_err("Fail to re-activate Probe-after-hb timer");
	}
}

void lim_process_add_sta_rsp(struct mac_context *mac_ctx,
			     struct scheduler_msg *msg)
{
	struct pe_session *session;
	tpAddStaParams add_sta_params;

	add_sta_params = (tpAddStaParams) msg->bodyptr;

	session = pe_find_session_by_session_id(mac_ctx,
			add_sta_params->sessionId);
	if (!session) {
		pe_err("Session Does not exist for given sessionID");
		qdf_mem_free(add_sta_params);
		return;
	}
	session->csaOffloadEnable = add_sta_params->csaOffloadEnable;
	if (LIM_IS_NDI_ROLE(session))
		lim_ndp_add_sta_rsp(mac_ctx, session, msg->bodyptr);
#ifdef FEATURE_WLAN_TDLS
	else if (add_sta_params->staType == STA_ENTRY_TDLS_PEER)
		lim_process_tdls_add_sta_rsp(mac_ctx, msg->bodyptr, session);
#endif
	else
		lim_process_mlm_add_sta_rsp(mac_ctx, msg, session);

}

/**
 * lim_update_beacon() - This function updates beacon
 * @mac_ctx: pointer to Global Mac Structure
 *
 * This Function is invoked to update the beacon
 *
 * Return: none
 */
void lim_update_beacon(struct mac_context *mac_ctx)
{
	uint8_t i;

	for (i = 0; i < mac_ctx->lim.maxBssId; i++) {
		if (mac_ctx->lim.gpSession[i].valid != true)
			continue;
		if ((mac_ctx->lim.gpSession[i].limSystemRole == eLIM_AP_ROLE)
			&& (eLIM_SME_NORMAL_STATE ==
				mac_ctx->lim.gpSession[i].limSmeState)) {

			sch_set_fixed_beacon_fields(mac_ctx,
						&mac_ctx->lim.gpSession[i]);

			if (false == mac_ctx->sap.SapDfsInfo.
					is_dfs_cac_timer_running)
				lim_send_beacon_ind(mac_ctx,
						&mac_ctx->lim.gpSession[i],
						REASON_DEFAULT);
		}
	}
}

/**
 * lim_handle_heart_beat_failure_timeout - handle heart beat failure
 * @mac_ctx: pointer to Global Mac Structure
 *
 * Function handle heart beat failure timeout
 *
 * Return: none
 */
void lim_handle_heart_beat_failure_timeout(struct mac_context *mac_ctx)
{
	uint8_t i;
	struct pe_session *psession_entry;
	/*
	 * Probe response is not received  after HB failure.
	 * This is handled by LMM sub module.
	 */
	for (i = 0; i < mac_ctx->lim.maxBssId; i++) {
		if (mac_ctx->lim.gpSession[i].valid != true)
			continue;
		psession_entry = &mac_ctx->lim.gpSession[i];
		if (psession_entry->LimHBFailureStatus != true)
			continue;
		pe_debug("SME: %d MLME: %d HB-Count: %d",
				psession_entry->limSmeState,
				psession_entry->limMlmState,
				psession_entry->LimRxedBeaconCntDuringHB);

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM
		lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_HB_FAILURE_TIMEOUT,
					psession_entry, 0, 0);
#endif
		if ((psession_entry->limMlmState ==
					eLIM_MLM_LINK_ESTABLISHED_STATE) &&
			(psession_entry->limSmeState !=
					eLIM_SME_WT_DISASSOC_STATE) &&
			(psession_entry->limSmeState !=
					eLIM_SME_WT_DEAUTH_STATE) &&
			((!LIM_IS_CONNECTION_ACTIVE(psession_entry)) ||
			/*
			 * Disconnect even if we have not received a single
			 * beacon after connection.
			 */
			 (psession_entry->currentBssBeaconCnt == 0))) {
			pe_nofl_info("HB fail vdev %d",psession_entry->vdev_id);
			lim_send_deauth_mgmt_frame(mac_ctx,
				REASON_DISASSOC_DUE_TO_INACTIVITY,
				psession_entry->bssId, psession_entry, false);

			/*
			 * AP did not respond to Probe Request.
			 * Tear down link with it.
			 */
			lim_tear_down_link_with_ap(mac_ctx,
						psession_entry->peSessionId,
						REASON_BEACON_MISSED,
						eLIM_LINK_MONITORING_DISASSOC);
			mac_ctx->lim.gLimProbeFailureAfterHBfailedCnt++;
		} else {
			pe_err("Unexpected wt-probe-timeout in state");
			lim_print_mlm_state(mac_ctx, LOGE,
				psession_entry->limMlmState);
			if (mac_ctx->sme.tx_queue_cb)
				mac_ctx->sme.tx_queue_cb(mac_ctx->hdd_handle,
						psession_entry->smeSessionId,
						WLAN_WAKE_ALL_NETIF_QUEUE,
						WLAN_CONTROL_PATH);
		}
	}
	/*
	 * Deactivate Timer ProbeAfterHB Timer -> As its a oneshot timer,
	 * need not deactivate the timer
	 * tx_timer_deactivate(&mac->lim.lim_timers.gLimProbeAfterHBTimer);
	 */
}

struct pe_session *lim_is_ap_session_active(struct mac_context *mac)
{
	uint8_t i;

	for (i = 0; i < mac->lim.maxBssId; i++) {
		if (mac->lim.gpSession[i].valid &&
		    (mac->lim.gpSession[i].limSystemRole == eLIM_AP_ROLE))
			return &mac->lim.gpSession[i];
	}

	return NULL;
}

/**---------------------------------------------------------
   \fn        lim_handle_defer_msg_error
   \brief    handles error scenario, when the msg can not be deferred.
   \param mac
   \param pLimMsg LIM msg, which could not be deferred.
   \return void
   -----------------------------------------------------------*/

void lim_handle_defer_msg_error(struct mac_context *mac,
				struct scheduler_msg *pLimMsg)
{
	if (SIR_BB_XPORT_MGMT_MSG == pLimMsg->type) {
		lim_decrement_pending_mgmt_count(mac);
		cds_pkt_return_packet((cds_pkt_t *) pLimMsg->bodyptr);
		pLimMsg->bodyptr = NULL;
	} else if (pLimMsg->bodyptr) {
		qdf_mem_free(pLimMsg->bodyptr);
		pLimMsg->bodyptr = NULL;
	}

}

#ifdef FEATURE_WLAN_DIAG_SUPPORT
/**---------------------------------------------------------
   \fn    lim_diag_event_report
   \brief This function reports Diag event
   \param mac
   \param eventType
   \param bssid
   \param status
   \param reasonCode
   \return void
   -----------------------------------------------------------*/
void lim_diag_event_report(struct mac_context *mac, uint16_t eventType,
			   struct pe_session *pe_session, uint16_t status,
			   uint16_t reasonCode)
{
	tSirMacAddr nullBssid = { 0, 0, 0, 0, 0, 0 };

	WLAN_HOST_DIAG_EVENT_DEF(peEvent, host_event_wlan_pe_payload_type);

	qdf_mem_zero(&peEvent, sizeof(host_event_wlan_pe_payload_type));

	if (!pe_session) {
		qdf_mem_copy(peEvent.bssid, nullBssid, sizeof(tSirMacAddr));
		peEvent.sme_state = (uint16_t) mac->lim.gLimSmeState;
		peEvent.mlm_state = (uint16_t) mac->lim.gLimMlmState;

	} else {
		qdf_mem_copy(peEvent.bssid, pe_session->bssId,
			     sizeof(tSirMacAddr));
		peEvent.sme_state = (uint16_t) pe_session->limSmeState;
		peEvent.mlm_state = (uint16_t) pe_session->limMlmState;
	}
	peEvent.event_type = eventType;
	peEvent.status = status;
	peEvent.reason_code = reasonCode;

	WLAN_HOST_DIAG_EVENT_REPORT(&peEvent, EVENT_WLAN_PE);
	return;
}

static void lim_diag_fill_mgmt_event_report(struct mac_context *mac_ctx,
			tpSirMacMgmtHdr mac_hdr,
			struct pe_session *session, uint16_t result_code,
			uint16_t reason_code,
			struct host_event_wlan_mgmt_payload_type *mgmt_event)
{
	uint8_t length;

	qdf_mem_zero(mgmt_event, sizeof(*mgmt_event));
	mgmt_event->mgmt_type = mac_hdr->fc.type;
	mgmt_event->mgmt_subtype = mac_hdr->fc.subType;
	qdf_mem_copy(mgmt_event->self_mac_addr, session->self_mac_addr,
		     QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(mgmt_event->bssid, session->bssId,
		     QDF_MAC_ADDR_SIZE);
	length = session->ssId.length;
	if (length > WLAN_SSID_MAX_LEN)
		length = WLAN_SSID_MAX_LEN;
	qdf_mem_copy(mgmt_event->ssid, session->ssId.ssId, length);
	mgmt_event->ssid_len = length;
	mgmt_event->operating_channel = wlan_reg_freq_to_chan(
		mac_ctx->pdev, session->curr_op_freq);
	mgmt_event->result_code = result_code;
	mgmt_event->reason_code = reason_code;
}

void lim_diag_mgmt_tx_event_report(struct mac_context *mac_ctx, void *mgmt_hdr,
				   struct pe_session *session, uint16_t result_code,
				   uint16_t reason_code)
{
	tpSirMacMgmtHdr mac_hdr = mgmt_hdr;

	WLAN_HOST_DIAG_EVENT_DEF(mgmt_event,
				 struct host_event_wlan_mgmt_payload_type);
	if (!session || !mac_hdr) {
		pe_err("not valid input");
		return;
	}
	lim_diag_fill_mgmt_event_report(mac_ctx, mac_hdr, session,
					result_code, reason_code, &mgmt_event);

	pe_debug("TX frame: type:%d sub_type:%d seq_num:%d ssid:%.*s selfmacaddr:"QDF_MAC_ADDR_FMT" bssid:"QDF_MAC_ADDR_FMT" channel:%d",
		 mgmt_event.mgmt_type, mgmt_event.mgmt_subtype,
		 ((mac_hdr->seqControl.seqNumHi << 4) |
				mac_hdr->seqControl.seqNumLo),
		 mgmt_event.ssid_len, mgmt_event.ssid,
		 QDF_MAC_ADDR_REF(mgmt_event.self_mac_addr),
		 QDF_MAC_ADDR_REF(mgmt_event.bssid),
		 mgmt_event.operating_channel);
	WLAN_HOST_DIAG_EVENT_REPORT(&mgmt_event, EVENT_WLAN_HOST_MGMT_TX_V2);
}

void lim_diag_mgmt_rx_event_report(struct mac_context *mac_ctx, void *mgmt_hdr,
				   struct pe_session *session, uint16_t result_code,
				   uint16_t reason_code)
{
	tpSirMacMgmtHdr mac_hdr = mgmt_hdr;

	WLAN_HOST_DIAG_EVENT_DEF(mgmt_event,
				 struct host_event_wlan_mgmt_payload_type);
	if (!session || !mac_hdr) {
		pe_debug("not valid input");
		return;
	}
	lim_diag_fill_mgmt_event_report(mac_ctx, mac_hdr, session,
					result_code, reason_code, &mgmt_event);
	pe_debug("RX frame: type:%d sub_type:%d seq_num:%d ssid:%.*s selfmacaddr:"QDF_MAC_ADDR_FMT" bssid:"QDF_MAC_ADDR_FMT" channel:%d",
		 mgmt_event.mgmt_type, mgmt_event.mgmt_subtype,
		 ((mac_hdr->seqControl.seqNumHi << 4) |
				mac_hdr->seqControl.seqNumLo),
		 mgmt_event.ssid_len, mgmt_event.ssid,
		 QDF_MAC_ADDR_REF(mgmt_event.self_mac_addr),
		 QDF_MAC_ADDR_REF(mgmt_event.bssid),
		 mgmt_event.operating_channel);
	WLAN_HOST_DIAG_EVENT_REPORT(&mgmt_event, EVENT_WLAN_HOST_MGMT_RX_V2);
}
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

/* Returns length of P2P stream and Pointer ie passed to this function is filled with noa stream */

uint8_t lim_build_p2p_ie(struct mac_context *mac, uint8_t *ie, uint8_t *data,
			 uint8_t ie_len)
{
	int length = 0;
	uint8_t *ptr = ie;

	ptr[length++] = WLAN_ELEMID_VENDOR;
	ptr[length++] = ie_len + SIR_MAC_P2P_OUI_SIZE;
	qdf_mem_copy(&ptr[length], SIR_MAC_P2P_OUI, SIR_MAC_P2P_OUI_SIZE);
	qdf_mem_copy(&ptr[length + SIR_MAC_P2P_OUI_SIZE], data, ie_len);
	return ie_len + SIR_P2P_IE_HEADER_LEN;
}

/* Returns length of NoA stream and Pointer pNoaStream passed to this function is filled with noa stream */
uint8_t lim_get_noa_attr_stream(struct mac_context *mac, uint8_t *pNoaStream,
				struct pe_session *pe_session)
{
	uint8_t len = 0;

	uint8_t *pBody = pNoaStream;

	if ((pe_session) && (pe_session->valid) &&
	    (pe_session->opmode == QDF_P2P_GO_MODE)) {
		if ((!(pe_session->p2pGoPsUpdate.uNoa1Duration))
		    && (!(pe_session->p2pGoPsUpdate.uNoa2Duration))
		    && (!pe_session->p2pGoPsUpdate.oppPsFlag)
		    )
			return 0;  /* No NoA Descriptor then return 0 */

		pBody[0] = SIR_P2P_NOA_ATTR;

		pBody[3] = pe_session->p2pGoPsUpdate.index;
		pBody[4] =
			pe_session->p2pGoPsUpdate.ctWin | (pe_session->
							      p2pGoPsUpdate.
							      oppPsFlag << 7);
		len = 5;
		pBody += len;

		if (pe_session->p2pGoPsUpdate.uNoa1Duration) {
			*pBody = pe_session->p2pGoPsUpdate.uNoa1IntervalCnt;
			pBody += 1;
			len += 1;

			*((uint32_t *) (pBody)) =
				sir_swap_u32if_needed(pe_session->p2pGoPsUpdate.
						      uNoa1Duration);
			pBody += sizeof(uint32_t);
			len += 4;

			*((uint32_t *) (pBody)) =
				sir_swap_u32if_needed(pe_session->p2pGoPsUpdate.
						      uNoa1Interval);
			pBody += sizeof(uint32_t);
			len += 4;

			*((uint32_t *) (pBody)) =
				sir_swap_u32if_needed(pe_session->p2pGoPsUpdate.
						      uNoa1StartTime);
			pBody += sizeof(uint32_t);
			len += 4;

		}

		if (pe_session->p2pGoPsUpdate.uNoa2Duration) {
			*pBody = pe_session->p2pGoPsUpdate.uNoa2IntervalCnt;
			pBody += 1;
			len += 1;

			*((uint32_t *) (pBody)) =
				sir_swap_u32if_needed(pe_session->p2pGoPsUpdate.
						      uNoa2Duration);
			pBody += sizeof(uint32_t);
			len += 4;

			*((uint32_t *) (pBody)) =
				sir_swap_u32if_needed(pe_session->p2pGoPsUpdate.
						      uNoa2Interval);
			pBody += sizeof(uint32_t);
			len += 4;

			*((uint32_t *) (pBody)) =
				sir_swap_u32if_needed(pe_session->p2pGoPsUpdate.
						      uNoa2StartTime);
			pBody += sizeof(uint32_t);
			len += 4;

		}

		pBody = pNoaStream + 1;
		*((uint16_t *) (pBody)) = sir_swap_u16if_needed(len - 3); /*one byte for Attr and 2 bytes for length */

		return len;

	}
	return 0;

}

void pe_set_resume_channel(struct mac_context *mac, uint16_t channel,
			   ePhyChanBondState phyCbState)
{

	mac->lim.gResumeChannel = channel;
	mac->lim.gResumePhyCbState = phyCbState;
}

bool lim_isconnected_on_dfs_channel(struct mac_context *mac_ctx,
		uint8_t currentChannel)
{
	if (CHANNEL_STATE_DFS ==
	    wlan_reg_get_channel_state(mac_ctx->pdev, currentChannel)) {
		return true;
	} else {
		return false;
	}
}

#ifdef WLAN_FEATURE_11W
void lim_pmf_sa_query_timer_handler(void *pMacGlobal, uint32_t param)
{
	struct mac_context *mac = (struct mac_context *) pMacGlobal;
	tPmfSaQueryTimerId timerId;
	struct pe_session *pe_session;
	tpDphHashNode pSta;
	uint8_t maxretries;

	pe_debug("SA Query timer fires");
	timerId.value = param;

	/* Check that SA Query is in progress */
	pe_session = pe_find_session_by_session_id(mac,
			timerId.fields.sessionId);
	if (!pe_session) {
		pe_err("Session does not exist for given session ID: %d",
			timerId.fields.sessionId);
		return;
	}
	pSta = dph_get_hash_entry(mac, timerId.fields.peerIdx,
			       &pe_session->dph.dphHashTable);
	if (!pSta) {
		pe_err("Entry does not exist for given peer index: %d",
			timerId.fields.peerIdx);
		return;
	}
	if (DPH_SA_QUERY_IN_PROGRESS != pSta->pmfSaQueryState)
		return;

	/* Increment the retry count, check if reached maximum */
	maxretries = mac->mlme_cfg->gen.pmf_sa_query_max_retries;
	pSta->pmfSaQueryRetryCount++;
	if (pSta->pmfSaQueryRetryCount >= maxretries) {
		pe_err("SA Query timed out,Deleting STA");
		lim_print_mac_addr(mac, pSta->staAddr, LOGE);
		lim_send_disassoc_mgmt_frame(mac,
			REASON_DISASSOC_DUE_TO_INACTIVITY,
			pSta->staAddr, pe_session, false);
		lim_trigger_sta_deletion(mac, pSta, pe_session);
		pSta->pmfSaQueryState = DPH_SA_QUERY_TIMED_OUT;
		return;
	}
	/* Retry SA Query */
	lim_send_sa_query_request_frame(mac,
					(uint8_t *) &(pSta->
						      pmfSaQueryCurrentTransId),
					pSta->staAddr, pe_session);
	pSta->pmfSaQueryCurrentTransId++;
	pe_debug("Starting SA Query retry: %d", pSta->pmfSaQueryRetryCount);
	if (tx_timer_activate(&pSta->pmfSaQueryTimer) != TX_SUCCESS) {
		pe_err("PMF SA Query timer activation failed!");
		pSta->pmfSaQueryState = DPH_SA_QUERY_NOT_IN_PROGRESS;
	}
}
#endif

bool lim_check_vht_op_mode_change(struct mac_context *mac,
				  struct pe_session *pe_session,
				  uint8_t chanWidth, uint8_t *peerMac)
{
	tUpdateVHTOpMode tempParam;

	tempParam.opMode = chanWidth;
	tempParam.smesessionId = pe_session->smeSessionId;
	qdf_mem_copy(tempParam.peer_mac, peerMac, sizeof(tSirMacAddr));

	lim_send_mode_update(mac, &tempParam, pe_session);

	return true;
}

#ifdef WLAN_FEATURE_11AX_BSS_COLOR
bool lim_send_he_ie_update(struct mac_context *mac_ctx, struct pe_session *pe_session)
{
	QDF_STATUS status;

	status = wma_update_he_ops_ie(cds_get_context(QDF_MODULE_ID_WMA),
				      pe_session->smeSessionId,
				      &pe_session->he_op);
	if (QDF_IS_STATUS_ERROR(status))
		return false;

	return true;
}
#endif

bool lim_set_nss_change(struct mac_context *mac, struct pe_session *pe_session,
			uint8_t rxNss, uint8_t *peerMac)
{
	tUpdateRxNss tempParam;

	if (!rxNss) {
		pe_err("Invalid rxNss value: %u", rxNss);
		return false;
	}

	tempParam.rxNss = rxNss;
	tempParam.smesessionId = pe_session->smeSessionId;
	qdf_mem_copy(tempParam.peer_mac, peerMac, sizeof(tSirMacAddr));

	lim_send_rx_nss_update(mac, &tempParam, pe_session);

	return true;
}

bool lim_check_membership_user_position(struct mac_context *mac,
					struct pe_session *pe_session,
					uint32_t membership,
					uint32_t userPosition)
{
	tUpdateMembership tempParamMembership;
	tUpdateUserPos tempParamUserPosition;

	tempParamMembership.membership = membership;
	tempParamMembership.smesessionId = pe_session->smeSessionId;
	qdf_mem_copy(tempParamMembership.peer_mac, pe_session->bssId,
		     sizeof(tSirMacAddr));

	lim_set_membership(mac, &tempParamMembership, pe_session);

	tempParamUserPosition.userPos = userPosition;
	tempParamUserPosition.smesessionId = pe_session->smeSessionId;
	qdf_mem_copy(tempParamUserPosition.peer_mac, pe_session->bssId,
		     sizeof(tSirMacAddr));

	lim_set_user_pos(mac, &tempParamUserPosition, pe_session);

	return true;
}

void lim_get_short_slot_from_phy_mode(struct mac_context *mac, struct pe_session *pe_session,
				      uint32_t phyMode, uint8_t *pShortSlotEnabled)
{
	uint8_t val = 0;

	/* only 2.4G band should have short slot enable, rest it should be default */
	if (phyMode == WNI_CFG_PHY_MODE_11G) {
		/* short slot is default in all other modes */
		if ((pe_session->opmode == QDF_SAP_MODE) ||
		    (pe_session->opmode == QDF_IBSS_MODE) ||
		    (pe_session->opmode == QDF_P2P_GO_MODE)) {
			val = true;
		}
		/* Program Polaris based on AP capability */
		if (pe_session->limMlmState == eLIM_MLM_WT_JOIN_BEACON_STATE) {
			/* Joining BSS. */
			val =
				SIR_MAC_GET_SHORT_SLOT_TIME(pe_session->
							    limCurrentBssCaps);
		} else if (pe_session->limMlmState ==
			   eLIM_MLM_WT_REASSOC_RSP_STATE) {
			/* Reassociating with AP. */
			val =
				SIR_MAC_GET_SHORT_SLOT_TIME(pe_session->
							    limReassocBssCaps);
		}
	} else {
		/*
		 * 11B does not short slot and short slot is default
		 * for 11A mode. Hence, not need to set this bit
		 */
		val = false;
	}

	pe_debug("phyMode: %u shortslotsupported: %u", phyMode, val);
	*pShortSlotEnabled = val;
}

#ifdef WLAN_FEATURE_11W
/**
 *
 * \brief This function is called by various LIM modules to correctly set
 * the Protected bit in the Frame Control Field of the 802.11 frame MAC header
 *
 *
 * \param  mac Pointer to Global MAC structure
 *
 * \param pe_session Pointer to session corresponding to the connection
 *
 * \param peer Peer address of the STA to which the frame is to be sent
 *
 * \param pMacHdr Pointer to the frame MAC header
 *
 * \return nothing
 *
 *
 */
void
lim_set_protected_bit(struct mac_context *mac,
		      struct pe_session *pe_session,
		      tSirMacAddr peer, tpSirMacMgmtHdr pMacHdr)
{
	uint16_t aid;
	tpDphHashNode sta;

	sta = dph_lookup_hash_entry(mac, peer, &aid,
				    &pe_session->dph.dphHashTable);
	if (sta) {
		/* rmfenabled will be set at the time of addbss.
		 * but sometimes EAP auth fails and keys are not
		 * installed then if we send any management frame
		 * like deauth/disassoc with this bit set then
		 * firmware crashes. so check for keys are
		 * installed or not also before setting the bit
		 */
		if (sta->rmfEnabled && sta->is_key_installed)
			pMacHdr->fc.wep = 1;

		pe_debug("wep:%d rmf:%d is_key_set:%d", pMacHdr->fc.wep,
			 sta->rmfEnabled, sta->is_key_installed);
	}
} /*** end lim_set_protected_bit() ***/
#endif

void lim_set_ht_caps(struct mac_context *p_mac, struct pe_session *p_session_entry,
		uint8_t *p_ie_start, uint32_t num_bytes)
{
	const uint8_t *p_ie = NULL;
	tDot11fIEHTCaps dot11_ht_cap = {0,};

	populate_dot11f_ht_caps(p_mac, p_session_entry, &dot11_ht_cap);
	p_ie = wlan_get_ie_ptr_from_eid(DOT11F_EID_HTCAPS,
					p_ie_start, num_bytes);
	pe_debug("p_ie: %pK dot11_ht_cap.supportedMCSSet[0]: 0x%x",
		p_ie, dot11_ht_cap.supportedMCSSet[0]);
	if (p_ie) {
		/* convert from unpacked to packed structure */
		tHtCaps *p_ht_cap = (tHtCaps *) &p_ie[2];

		p_ht_cap->advCodingCap = dot11_ht_cap.advCodingCap;
		p_ht_cap->supportedChannelWidthSet =
			dot11_ht_cap.supportedChannelWidthSet;
		p_ht_cap->mimoPowerSave = dot11_ht_cap.mimoPowerSave;
		p_ht_cap->greenField = dot11_ht_cap.greenField;
		p_ht_cap->shortGI20MHz = dot11_ht_cap.shortGI20MHz;
		p_ht_cap->shortGI40MHz = dot11_ht_cap.shortGI40MHz;
		p_ht_cap->txSTBC = dot11_ht_cap.txSTBC;
		p_ht_cap->rxSTBC = dot11_ht_cap.rxSTBC;
		p_ht_cap->delayedBA = dot11_ht_cap.delayedBA;
		p_ht_cap->maximalAMSDUsize = dot11_ht_cap.maximalAMSDUsize;
		p_ht_cap->dsssCckMode40MHz = dot11_ht_cap.dsssCckMode40MHz;
		p_ht_cap->psmp = dot11_ht_cap.psmp;
		p_ht_cap->stbcControlFrame = dot11_ht_cap.stbcControlFrame;
		p_ht_cap->lsigTXOPProtection = dot11_ht_cap.lsigTXOPProtection;
		p_ht_cap->maxRxAMPDUFactor = dot11_ht_cap.maxRxAMPDUFactor;
		p_ht_cap->mpduDensity = dot11_ht_cap.mpduDensity;
		qdf_mem_copy((void *)p_ht_cap->supportedMCSSet,
			(void *)(dot11_ht_cap.supportedMCSSet),
			sizeof(p_ht_cap->supportedMCSSet));
		p_ht_cap->pco = dot11_ht_cap.pco;
		p_ht_cap->transitionTime = dot11_ht_cap.transitionTime;
		p_ht_cap->mcsFeedback = dot11_ht_cap.mcsFeedback;
		p_ht_cap->txBF = dot11_ht_cap.txBF;
		p_ht_cap->rxStaggeredSounding =
			dot11_ht_cap.rxStaggeredSounding;
		p_ht_cap->txStaggeredSounding =
			dot11_ht_cap.txStaggeredSounding;
		p_ht_cap->rxZLF = dot11_ht_cap.rxZLF;
		p_ht_cap->txZLF = dot11_ht_cap.txZLF;
		p_ht_cap->implicitTxBF = dot11_ht_cap.implicitTxBF;
		p_ht_cap->calibration = dot11_ht_cap.calibration;
		p_ht_cap->explicitCSITxBF = dot11_ht_cap.explicitCSITxBF;
		p_ht_cap->explicitUncompressedSteeringMatrix =
			dot11_ht_cap.explicitUncompressedSteeringMatrix;
		p_ht_cap->explicitBFCSIFeedback =
			dot11_ht_cap.explicitBFCSIFeedback;
		p_ht_cap->explicitUncompressedSteeringMatrixFeedback =
			dot11_ht_cap.explicitUncompressedSteeringMatrixFeedback;
		p_ht_cap->explicitCompressedSteeringMatrixFeedback =
			dot11_ht_cap.explicitCompressedSteeringMatrixFeedback;
		p_ht_cap->csiNumBFAntennae = dot11_ht_cap.csiNumBFAntennae;
		p_ht_cap->uncompressedSteeringMatrixBFAntennae =
			dot11_ht_cap.uncompressedSteeringMatrixBFAntennae;
		p_ht_cap->compressedSteeringMatrixBFAntennae =
			dot11_ht_cap.compressedSteeringMatrixBFAntennae;
		p_ht_cap->antennaSelection = dot11_ht_cap.antennaSelection;
		p_ht_cap->explicitCSIFeedbackTx =
			dot11_ht_cap.explicitCSIFeedbackTx;
		p_ht_cap->antennaIndicesFeedbackTx =
			dot11_ht_cap.antennaIndicesFeedbackTx;
		p_ht_cap->explicitCSIFeedback =
			dot11_ht_cap.explicitCSIFeedback;
		p_ht_cap->antennaIndicesFeedback =
			dot11_ht_cap.antennaIndicesFeedback;
		p_ht_cap->rxAS = dot11_ht_cap.rxAS;
		p_ht_cap->txSoundingPPDUs = dot11_ht_cap.txSoundingPPDUs;
	}
}

void lim_set_vht_caps(struct mac_context *p_mac, struct pe_session *p_session_entry,
		      uint8_t *p_ie_start, uint32_t num_bytes)
{
	const uint8_t       *p_ie = NULL;
	tDot11fIEVHTCaps     dot11_vht_cap;

	populate_dot11f_vht_caps(p_mac, p_session_entry, &dot11_vht_cap);
	p_ie = wlan_get_ie_ptr_from_eid(DOT11F_EID_VHTCAPS, p_ie_start,
					num_bytes);
	if (p_ie) {
		tSirMacVHTCapabilityInfo *vht_cap =
					(tSirMacVHTCapabilityInfo *) &p_ie[2];
		tSirVhtMcsInfo *vht_mcs = (tSirVhtMcsInfo *) &p_ie[2 +
					  sizeof(tSirMacVHTCapabilityInfo)];

		union {
			uint16_t                       u_value;
			tSirMacVHTRxSupDataRateInfo    vht_rx_supp_rate;
			tSirMacVHTTxSupDataRateInfo    vht_tx_supp_rate;
		} u_vht_data_rate_info;

		vht_cap->maxMPDULen = dot11_vht_cap.maxMPDULen;
		vht_cap->supportedChannelWidthSet =
					dot11_vht_cap.supportedChannelWidthSet;
		vht_cap->ldpcCodingCap = dot11_vht_cap.ldpcCodingCap;
		vht_cap->shortGI80MHz = dot11_vht_cap.shortGI80MHz;
		vht_cap->shortGI160and80plus80MHz =
					dot11_vht_cap.shortGI160and80plus80MHz;
		vht_cap->txSTBC = dot11_vht_cap.txSTBC;
		vht_cap->rxSTBC = dot11_vht_cap.rxSTBC;
		vht_cap->suBeamFormerCap = dot11_vht_cap.suBeamFormerCap;
		vht_cap->suBeamformeeCap = dot11_vht_cap.suBeamformeeCap;
		vht_cap->csnofBeamformerAntSup =
					dot11_vht_cap.csnofBeamformerAntSup;
		vht_cap->numSoundingDim = dot11_vht_cap.numSoundingDim;
		vht_cap->muBeamformerCap = dot11_vht_cap.muBeamformerCap;
		vht_cap->muBeamformeeCap = dot11_vht_cap.muBeamformeeCap;
		vht_cap->vhtTXOPPS = dot11_vht_cap.vhtTXOPPS;
		vht_cap->htcVHTCap = dot11_vht_cap.htcVHTCap;
		vht_cap->maxAMPDULenExp = dot11_vht_cap.maxAMPDULenExp;
		vht_cap->vhtLinkAdaptCap = dot11_vht_cap.vhtLinkAdaptCap;
		vht_cap->rxAntPattern = dot11_vht_cap.rxAntPattern;
		vht_cap->txAntPattern = dot11_vht_cap.txAntPattern;
		vht_cap->extended_nss_bw_supp =
			dot11_vht_cap.extended_nss_bw_supp;

		/* Populate VHT MCS Information */
		vht_mcs->rxMcsMap = dot11_vht_cap.rxMCSMap;
		u_vht_data_rate_info.vht_rx_supp_rate.rxSupDataRate =
					dot11_vht_cap.rxHighSupDataRate;
		u_vht_data_rate_info.vht_rx_supp_rate.max_nsts_total =
					dot11_vht_cap.max_nsts_total;
		vht_mcs->rxHighest = u_vht_data_rate_info.u_value;

		vht_mcs->txMcsMap = dot11_vht_cap.txMCSMap;
		u_vht_data_rate_info.vht_tx_supp_rate.txSupDataRate =
					dot11_vht_cap.txSupDataRate;
		u_vht_data_rate_info.vht_tx_supp_rate.vht_extended_nss_bw_cap =
					dot11_vht_cap.vht_extended_nss_bw_cap;
		vht_mcs->txHighest = u_vht_data_rate_info.u_value;
	}
}

/*
 * Firmware will send RTS for every frame and also will disable SIFS bursting
 * if value 0x11 is sent for RTS profile.
 */
#define A_EDCA_SCC_RTS_PROFILE_VALUE 0x11
#define MAX_NUMBER_OF_SINGLE_PORT_CONC_CONNECTIONS 2

static void lim_update_sta_edca_params(struct mac_context *mac,
				       struct pe_session *sta_session)
{
	uint8_t i;

	for (i = QCA_WLAN_AC_BE; i < QCA_WLAN_AC_ALL; i++) {
		sta_session->gLimEdcaParamsActive[i] =
						sta_session->gLimEdcaParams[i];
	}
	lim_send_edca_params(mac,
			     sta_session->gLimEdcaParamsActive,
			     sta_session->vdev_id, false);
}

/**
 * lim_check_conc_and_send_edca() - Function to check and update EDCA params
 *                                  and RTS profile based on STA/SAP
 *                                  concurrency. If updated, it will also send
 *                                  the updated parameters to FW. It will update
 * EDCA params and RTS profile such that:
 *       1) For STA and SAP concurrency, send STA's AP EDCA params to fw.
 *          Also, for SAP or P2P Go, update the value in Broadcast EDCA params
 *          as well so that it should be broadcasted to other stations connected
 *          to that BSS. Also, update the RTS profile value to 0x11 for which
 *          FW will send RTS for every frame and will also disable SIFS
 *          bursting.
 *
 *       2) For standalone STA (can even happen after SAP/P2P Go
 *          disconnects), if the parameters are updated, reset them to original
 *          parameters and send them to FW. Also, update the RTS profile
 *          value to which it was set before.
 *
 *       3) For standalone SAP (can even happen after STA disconnects),
 *          if the parameters are updated, reset them to original
 *          parameters and send them to FW and reset the  Broadcast EDCA params
 *          as well so that it should be broadcasted to other stations connected
 *          to that BSS Also, update the RTS profile value to which it was set
 *          before.
 *
 * This update is needed because throughput drop was seen because of
 * inconsistency in the EDCA params used in STA-SAP or STA-P2P_GO concurrency.
 *
 * Return: void
 */

static void lim_check_conc_and_send_edca(struct mac_context *mac,
					 struct pe_session *sta_session,
					 struct pe_session *sap_session)
{
	bool params_update_required = false;
	uint8_t i;
	tpDphHashNode sta_ds = NULL;
	QDF_STATUS status;
	uint16_t assoc_id;

	if (sta_session && sap_session &&
	    (sta_session->curr_op_freq ==
	     sap_session->curr_op_freq)) {
	/* RTS profile update to FW */
		wma_cli_set_command(sap_session->vdev_id,
				    WMI_VDEV_PARAM_ENABLE_RTSCTS,
				    A_EDCA_SCC_RTS_PROFILE_VALUE,
				    VDEV_CMD);
		wma_cli_set_command(sta_session->vdev_id,
				    WMI_VDEV_PARAM_ENABLE_RTSCTS,
				    A_EDCA_SCC_RTS_PROFILE_VALUE,
				    VDEV_CMD);

		sta_ds = dph_lookup_hash_entry(mac,
					       sta_session->bssId,
					       &assoc_id,
					       &sta_session->dph.dphHashTable);

		if (!sta_ds) {
			pe_debug("No STA DS entry found for " QDF_MAC_ADDR_FMT,
				 QDF_MAC_ADDR_REF(sta_session->bssId));
			return;
		}

		if (!sta_ds->qos.peer_edca_params.length) {
			pe_debug("No sta_ds edca_params present");
			return;
		}

	/*
	 * Here what we do is disable A-EDCA by sending the edca params of
	 * connected AP which we got as part of assoc resp So as these EDCA
	 * params are updated per mac , its fine to send for SAP which will
	 * be used for STA as well on the same channel. No need to send for
	 * both SAP and STA.
	 */

		sap_session->gLimEdcaParamsBC[QCA_WLAN_AC_BE] =
				sta_ds->qos.peer_edca_params.acbe;
		sap_session->gLimEdcaParamsBC[QCA_WLAN_AC_BK] =
				sta_ds->qos.peer_edca_params.acbk;
		sap_session->gLimEdcaParamsBC[QCA_WLAN_AC_VI] =
				sta_ds->qos.peer_edca_params.acvi;
		sap_session->gLimEdcaParamsBC[QCA_WLAN_AC_VO] =
				sta_ds->qos.peer_edca_params.acvo;

		sap_session->gLimEdcaParamsActive[QCA_WLAN_AC_BE] =
				sta_ds->qos.peer_edca_params.acbe;
		sap_session->gLimEdcaParamsActive[QCA_WLAN_AC_BK] =
				sta_ds->qos.peer_edca_params.acbk;
		sap_session->gLimEdcaParamsActive[QCA_WLAN_AC_VI] =
				sta_ds->qos.peer_edca_params.acvi;
		sap_session->gLimEdcaParamsActive[QCA_WLAN_AC_VO] =
				sta_ds->qos.peer_edca_params.acvo;

		for (i = QCA_WLAN_AC_BE; i < QCA_WLAN_AC_ALL; i++) {
			sta_session->gLimEdcaParamsActive[i] =
				sap_session->gLimEdcaParamsActive[i];
		}
		/* For AP, the bssID is stored in LIM Global context. */
		lim_send_edca_params(mac, sap_session->gLimEdcaParamsActive,
				     sap_session->vdev_id, false);

		sap_session->gLimEdcaParamSetCount++;
		status = sch_set_fixed_beacon_fields(mac, sap_session);
		if (QDF_IS_STATUS_ERROR(status))
			pe_debug("Unable to set beacon fields!");

	} else if (!sap_session && sta_session) {
	/*
	 * Enable A-EDCA for standalone STA. The original EDCA parameters are
	 * stored in gLimEdcaParams (computed by sch_beacon_edca_process()),
	 * if active parameters are not equal that means they have been updated
	 * because of conncurrency and are need to be restored now
	 */

		wma_cli_set_command(sta_session->vdev_id,
				    WMI_VDEV_PARAM_ENABLE_RTSCTS,
				    cfg_get(mac->psoc,
					    CFG_ENABLE_FW_RTS_PROFILE),
				    VDEV_CMD);
		for (i = QCA_WLAN_AC_BE; i < QCA_WLAN_AC_ALL; i++) {
			if (qdf_mem_cmp(&sta_session->gLimEdcaParamsActive[i],
					&sta_session->gLimEdcaParams[i],
					sizeof(tSirMacEdcaParamRecord))) {
				pe_debug("local sta EDCA params are not equal to Active EDCA params, hence update required");
				params_update_required = true;
				break;
			}
		}

		if (params_update_required) {
			lim_update_sta_edca_params(mac,
						   sta_session);
		}
	} else {
	/*
	 * For STA+SAP/GO DBS, STA+SAP/GO MCC or standalone SAP/GO
	 */

		wma_cli_set_command(sap_session->vdev_id,
				    WMI_VDEV_PARAM_ENABLE_RTSCTS,
				    cfg_get(mac->psoc,
					    CFG_ENABLE_FW_RTS_PROFILE),
				    VDEV_CMD);
		if (sta_session)
			wma_cli_set_command(sta_session->vdev_id,
					    WMI_VDEV_PARAM_ENABLE_RTSCTS,
					    cfg_get(mac->psoc,
						    CFG_ENABLE_FW_RTS_PROFILE),
					    VDEV_CMD);

		for (i = QCA_WLAN_AC_BE; i < QCA_WLAN_AC_ALL; i++) {
			if (qdf_mem_cmp(&sap_session->gLimEdcaParamsActive[i],
					&sap_session->gLimEdcaParams[i],
					sizeof(tSirMacEdcaParamRecord))) {
				pe_debug("local sap EDCA params are not equal to Active EDCA params, hence update required");
				params_update_required = true;
				break;
			}
		}

		if (params_update_required) {
			for (i = QCA_WLAN_AC_BE; i < QCA_WLAN_AC_ALL; i++) {
				sap_session->gLimEdcaParamsActive[i] =
					sap_session->gLimEdcaParams[i];
			}
			lim_send_edca_params(mac,
					     sap_session->gLimEdcaParamsActive,
					     sap_session->vdev_id, false);
			sch_qos_update_broadcast(mac, sap_session);

	/*
	 * In case of mcc, where cb can come from scc to mcc swtich where we
	 * need to restore the default parameters
	 */
			if (sta_session) {
				lim_update_sta_edca_params(mac,
							   sta_session);
				}
		}
	}
}

void lim_send_conc_params_update(void)
{
	struct pe_session *sta_session = NULL;
	struct pe_session *sap_session = NULL;
	uint8_t i;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac)
		return;

	if (!mac->mlme_cfg->edca_params.enable_edca_params ||
	    (policy_mgr_get_connection_count(mac->psoc) >
	     MAX_NUMBER_OF_SINGLE_PORT_CONC_CONNECTIONS)) {
		pe_debug("A-EDCA not enabled or max number of connections: %d",
			 policy_mgr_get_connection_count(mac->psoc));
		return;
	}

	for (i = 0; i < mac->lim.maxBssId; i++) {
		/*
		 * Finding whether STA or Go session exists
		 */
		if (sta_session && sap_session)
			break;

		if ((mac->lim.gpSession[i].valid) &&
		    (mac->lim.gpSession[i].limSystemRole ==
		     eLIM_STA_ROLE)) {
			sta_session = &mac->lim.gpSession[i];
			continue;
		}
		if ((mac->lim.gpSession[i].valid) &&
		    ((mac->lim.gpSession[i].limSystemRole == eLIM_AP_ROLE) ||
		    (mac->lim.gpSession[i].limSystemRole ==
		     eLIM_P2P_DEVICE_GO))) {
			sap_session = &mac->lim.gpSession[i];
			continue;
		}
	}

	if (!(sta_session || sap_session)) {
		pe_debug("No sta or sap or P2P go session");
		return;
	}

	pe_debug("Valid STA session: %d Valid SAP session: %d",
		 (sta_session ? sta_session->valid : 0),
		 (sap_session ? sap_session->valid : 0));
	lim_check_conc_and_send_edca(mac, sta_session, sap_session);
}

/**
 * lim_validate_received_frame_a1_addr() - To validate received frame's A1 addr
 * @mac_ctx: pointer to mac context
 * @a1: received frame's a1 address which is nothing but our self address
 * @session: PE session pointer
 *
 * This routine will validate, A1 address of the received frame
 *
 * Return: true or false
 */
bool lim_validate_received_frame_a1_addr(struct mac_context *mac_ctx,
		tSirMacAddr a1, struct pe_session *session)
{
	if (!mac_ctx || !session) {
		pe_err("mac or session context is null");
		/* let main routine handle it */
		return true;
	}
	if (IEEE80211_IS_MULTICAST(a1) || QDF_IS_ADDR_BROADCAST(a1)) {
		/* just for fail safe, don't handle MC/BC a1 in this routine */
		return true;
	}
	if (qdf_mem_cmp(a1, session->self_mac_addr, 6)) {
		pe_err("Invalid A1 address in received frame");
		return false;
	}
	return true;
}

/**
 * lim_check_and_reset_protection_params() - reset protection related parameters
 *
 * @mac_ctx: pointer to global mac structure
 *
 * resets protection related global parameters if the pe active session count
 * is zero.
 *
 * Return: None
 */
void lim_check_and_reset_protection_params(struct mac_context *mac_ctx)
{
	if (!pe_get_active_session_count(mac_ctx)) {
		mac_ctx->lim.gHTOperMode = eSIR_HT_OP_MODE_PURE;
	}
}

/**
 * lim_set_stads_rtt_cap() - update station node RTT capability
 * @sta_ds: Station hash node
 * @ext_cap: Pointer to extended capability
 * @mac_ctx: global MAC context
 *
 * This funciton update hash node's RTT capability based on received
 * Extended capability IE.
 *
 * Return: None
 */
void lim_set_stads_rtt_cap(tpDphHashNode sta_ds, struct s_ext_cap *ext_cap,
			   struct mac_context *mac_ctx)
{
	sta_ds->timingMeasCap = 0;
	sta_ds->timingMeasCap |= (ext_cap->timing_meas) ?
				  RTT_TIMING_MEAS_CAPABILITY :
				  RTT_INVALID;
	sta_ds->timingMeasCap |= (ext_cap->fine_time_meas_initiator) ?
				  RTT_FINE_TIME_MEAS_INITIATOR_CAPABILITY :
				  RTT_INVALID;
	sta_ds->timingMeasCap |= (ext_cap->fine_time_meas_responder) ?
				  RTT_FINE_TIME_MEAS_RESPONDER_CAPABILITY :
				  RTT_INVALID;

	pe_debug("ExtCap present, timingMeas: %d Initiator: %d Responder: %d",
	    ext_cap->timing_meas, ext_cap->fine_time_meas_initiator,
	    ext_cap->fine_time_meas_responder);
}

#ifdef WLAN_SUPPORT_TWT
void lim_set_peer_twt_cap(struct pe_session *session, struct s_ext_cap *ext_cap)
{
	if (session->enable_session_twt_support) {
		session->peer_twt_requestor = ext_cap->twt_requestor_support;
		session->peer_twt_responder = ext_cap->twt_responder_support;
	}

	pe_debug("Ext Cap peer TWT requestor: %d, responder: %d, enable_twt %d",
		 ext_cap->twt_requestor_support,
		 ext_cap->twt_responder_support,
		 session->enable_session_twt_support);
}
#endif

/**
 * lim_send_ie() - sends IE to wma
 * @mac_ctx: global MAC context
 * @vdev_id: vdev_id
 * @eid: IE id
 * @band: band for which IE is intended
 * @buf: buffer containing IE
 * @len: length of buffer
 *
 * This funciton sends the IE data to WMA.
 *
 * Return: status of operation
 */
static QDF_STATUS lim_send_ie(struct mac_context *mac_ctx, uint32_t vdev_id,
			      uint8_t eid, enum cds_band_type band,
			      uint8_t *buf, uint32_t len)
{
	struct vdev_ie_info *ie_msg;
	struct scheduler_msg msg = {0};
	QDF_STATUS status;

	/* Allocate memory for the WMI request */
	ie_msg = qdf_mem_malloc(sizeof(*ie_msg) + len);
	if (!ie_msg)
		return QDF_STATUS_E_NOMEM;

	ie_msg->vdev_id = vdev_id;
	ie_msg->ie_id = eid;
	ie_msg->length = len;
	ie_msg->band = band;
	/* IE data buffer starts at end of the struct */
	ie_msg->data = (uint8_t *)&ie_msg[1];

	qdf_mem_copy(ie_msg->data, buf, len);
	msg.type = WMA_SET_IE_INFO;
	msg.bodyptr = ie_msg;
	msg.reserved = 0;

	status = scheduler_post_message(QDF_MODULE_ID_PE,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &msg);
	if (QDF_STATUS_SUCCESS != status) {
		pe_err("Not able to post WMA_SET_IE_INFO to WMA");
		qdf_mem_free(ie_msg);
		return status;
	}

	return status;
}

/**
 * lim_get_rx_ldpc() - gets ldpc setting for given channel(band)
 * @mac_ctx: global mac context
 * @ch: channel enum for which ldpc setting is required
 *      Note: ch param is not absolute channel number rather it is
 *            channel number enum.
 *
 * Return: true if enabled and false otherwise
 */
static inline bool lim_get_rx_ldpc(struct mac_context *mac_ctx,
				   enum channel_enum ch)
{
	if (mac_ctx->mlme_cfg->ht_caps.ht_cap_info.adv_coding_cap &&
	    wma_is_rx_ldpc_supported_for_channel(wlan_reg_ch_to_freq(ch)))
		return true;
	else
		return false;
}

/**
 * lim_populate_mcs_set_ht_per_vdev() - update the MCS set according to vdev nss
 * @mac_ctx: global mac context
 * @ht_cap: pointer to ht caps
 * @vdev_id: vdev for which IE is targeted
 * @band: band for which the MCS set has to be updated
 *
 * This function updates the MCS set according to vdev nss
 *
 * Return: None
 */
static void lim_populate_mcs_set_ht_per_vdev(struct mac_context *mac_ctx,
						      struct sHtCaps *ht_cap,
						      uint8_t vdev_id,
						      uint8_t band)
{
	struct wlan_mlme_nss_chains *nss_chains_ini_cfg;
	struct wlan_objmgr_vdev *vdev =
			wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
							     vdev_id,
							     WLAN_MLME_SB_ID);
	if (!vdev) {
		pe_err("Got NULL vdev obj, returning");
		return;
	}
	if (!ht_cap->supportedMCSSet[1])
		goto end;
	nss_chains_ini_cfg = mlme_get_ini_vdev_config(vdev);
	if (!nss_chains_ini_cfg) {
		pe_err("nss chain dynamic config NULL");
		goto end;
	}

	/* convert from unpacked to packed structure */
	if (nss_chains_ini_cfg->rx_nss[band] == 1)
		ht_cap->supportedMCSSet[1] = 0;

end:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
}

/**
 * lim_populate_mcs_set_vht_per_vdev() - update MCS set according to vdev nss
 * @mac_ctx: global mac context
 * @vht_caps: pointer to vht_caps
 * @vdev_id: vdev for which IE is targeted
 * @band: band for which the MCS set has to be updated
 *
 * This function updates the MCS set according to vdev nss
 *
 * Return: None
 */
static void lim_populate_mcs_set_vht_per_vdev(struct mac_context *mac_ctx,
					      uint8_t *vht_caps,
					      uint8_t vdev_id,
					      uint8_t band)
{
	struct wlan_mlme_nss_chains *nss_chains_ini_cfg;
	tSirVhtMcsInfo *vht_mcs;
	struct wlan_objmgr_vdev *vdev =
			wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
							     vdev_id,
							     WLAN_MLME_SB_ID);
	if (!vdev) {
		pe_err("Got NULL vdev obj, returning");
		return;
	}

	nss_chains_ini_cfg = mlme_get_ini_vdev_config(vdev);
	if (!nss_chains_ini_cfg) {
		pe_err("nss chain dynamic config NULL");
		goto end;
	}

	vht_mcs = (tSirVhtMcsInfo *)&vht_caps[2 +
					sizeof(tSirMacVHTCapabilityInfo)];
	if (nss_chains_ini_cfg->tx_nss[band] == 1) {
	/* Populate VHT MCS Information */
		vht_mcs->txMcsMap |= DISABLE_NSS2_MCS;
		vht_mcs->txHighest =
				VHT_TX_HIGHEST_SUPPORTED_DATA_RATE_1_1;
	}

	if (nss_chains_ini_cfg->rx_nss[band] == 1) {
	/* Populate VHT MCS Information */
		vht_mcs->rxMcsMap |= DISABLE_NSS2_MCS;
		vht_mcs->rxHighest =
				VHT_RX_HIGHEST_SUPPORTED_DATA_RATE_1_1;
	}

end:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
}

/**
 * is_dot11mode_support_ht_cap() - Check dot11mode supports HT capability
 * @dot11mode: dot11mode
 *
 * This function checks whether dot11mode support HT capability or not
 *
 * Return: True, if supports. False otherwise
 */
static bool is_dot11mode_support_ht_cap(enum csr_cfgdot11mode dot11mode)
{
	if ((dot11mode == eCSR_CFG_DOT11_MODE_AUTO) ||
	    (dot11mode == eCSR_CFG_DOT11_MODE_11N) ||
	    (dot11mode == eCSR_CFG_DOT11_MODE_11AC) ||
	    (dot11mode == eCSR_CFG_DOT11_MODE_11N_ONLY) ||
	    (dot11mode == eCSR_CFG_DOT11_MODE_11AC_ONLY) ||
	    (dot11mode == eCSR_CFG_DOT11_MODE_11AX) ||
	    (dot11mode == eCSR_CFG_DOT11_MODE_11AX_ONLY)) {
		return true;
	}

	return false;
}

/**
 * is_dot11mode_support_vht_cap() - Check dot11mode supports VHT capability
 * @dot11mode: dot11mode
 *
 * This function checks whether dot11mode support VHT capability or not
 *
 * Return: True, if supports. False otherwise
 */
static bool is_dot11mode_support_vht_cap(enum csr_cfgdot11mode dot11mode)
{
	if ((dot11mode == eCSR_CFG_DOT11_MODE_AUTO) ||
	    (dot11mode == eCSR_CFG_DOT11_MODE_11AC) ||
	    (dot11mode == eCSR_CFG_DOT11_MODE_11AC_ONLY) ||
	    (dot11mode == eCSR_CFG_DOT11_MODE_11AX) ||
	    (dot11mode == eCSR_CFG_DOT11_MODE_11AX_ONLY)) {
		return true;
	}

	return false;
}

/**
 * is_dot11mode_support_he_cap() - Check dot11mode supports HE capability
 * @dot11mode: dot11mode
 *
 * This function checks whether dot11mode support HE capability or not
 *
 * Return: True, if supports. False otherwise
 */
static bool is_dot11mode_support_he_cap(enum csr_cfgdot11mode dot11mode)
{
	if ((dot11mode == eCSR_CFG_DOT11_MODE_AUTO) ||
	    (dot11mode == eCSR_CFG_DOT11_MODE_11AX) ||
	    (dot11mode == eCSR_CFG_DOT11_MODE_11AX_ONLY)) {
		return true;
	}

	return false;
}

/**
 * lim_send_ht_caps_ie() - gets HT capability and send to firmware via wma
 * @mac_ctx: global mac context
 * @session: pe session. This can be NULL. In that case self cap will be sent
 * @device_mode: VDEV op mode
 * @vdev_id: vdev for which IE is targeted
 *
 * This function gets HT capability and send to firmware via wma
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS lim_send_ht_caps_ie(struct mac_context *mac_ctx,
				      struct pe_session *session,
				      enum QDF_OPMODE device_mode,
				      uint8_t vdev_id)
{
	uint8_t ht_caps[DOT11F_IE_HTCAPS_MIN_LEN + 2] = {0};
	tHtCaps *p_ht_cap = (tHtCaps *)(&ht_caps[2]);
	QDF_STATUS status_5g, status_2g;
	bool nan_beamforming_supported;

	ht_caps[0] = DOT11F_EID_HTCAPS;
	ht_caps[1] = DOT11F_IE_HTCAPS_MIN_LEN;
	lim_set_ht_caps(mac_ctx, session, ht_caps,
			DOT11F_IE_HTCAPS_MIN_LEN + 2);
	/* Get LDPC and over write for 2G */
	p_ht_cap->advCodingCap = lim_get_rx_ldpc(mac_ctx,
						 CHAN_ENUM_2437);
	/* Get self cap for HT40 support in 2G */
	if (mac_ctx->roam.configParam.channelBondingMode24GHz) {
		p_ht_cap->supportedChannelWidthSet = 1;
		p_ht_cap->shortGI40MHz = 1;
	} else {
		p_ht_cap->supportedChannelWidthSet = 0;
		p_ht_cap->shortGI40MHz = 0;
	}

	lim_populate_mcs_set_ht_per_vdev(mac_ctx, p_ht_cap, vdev_id,
					 NSS_CHAINS_BAND_2GHZ);

	nan_beamforming_supported =
		ucfg_nan_is_beamforming_supported(mac_ctx->psoc);
	if (device_mode == QDF_NDI_MODE && !nan_beamforming_supported) {
		p_ht_cap->txBF = 0;
		p_ht_cap->implicitTxBF = 0;
		p_ht_cap->explicitCSITxBF = 0;
	}

	status_2g = lim_send_ie(mac_ctx, vdev_id, DOT11F_EID_HTCAPS,
				CDS_BAND_2GHZ, &ht_caps[2],
				DOT11F_IE_HTCAPS_MIN_LEN);
	/*
	 * Get LDPC and over write for 5G - using channel 64 because it
	 * is available in all reg domains.
	 */
	p_ht_cap->advCodingCap = lim_get_rx_ldpc(mac_ctx, CHAN_ENUM_5320);
	/* Get self cap for HT40 support in 5G */
	if (mac_ctx->roam.configParam.channelBondingMode5GHz) {
		p_ht_cap->supportedChannelWidthSet = 1;
		p_ht_cap->shortGI40MHz = 1;
	} else {
		p_ht_cap->supportedChannelWidthSet = 0;
		p_ht_cap->shortGI40MHz = 0;
	}
	lim_populate_mcs_set_ht_per_vdev(mac_ctx, p_ht_cap, vdev_id,
					 NSS_CHAINS_BAND_5GHZ);
	status_5g = lim_send_ie(mac_ctx, vdev_id, DOT11F_EID_HTCAPS,
				CDS_BAND_5GHZ, &ht_caps[2],
				DOT11F_IE_HTCAPS_MIN_LEN);

	if (QDF_IS_STATUS_SUCCESS(status_2g) &&
	    QDF_IS_STATUS_SUCCESS(status_5g))
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}

/**
 * lim_send_vht_caps_ie() - gets VHT capability and send to firmware via wma
 * @mac_ctx: global mac context
 * @session: pe session. This can be NULL. In that case self cap will be sent
 * @device_mode: VDEV op mode
 * @vdev_id: vdev for which IE is targeted
 *
 * This function gets VHT capability and send to firmware via wma
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS lim_send_vht_caps_ie(struct mac_context *mac_ctx,
				       struct pe_session *session,
				       enum QDF_OPMODE device_mode,
				       uint8_t vdev_id)
{
	uint8_t vht_caps[DOT11F_IE_VHTCAPS_MAX_LEN + 2] = {0};
	bool vht_for_2g_enabled = false, nan_beamforming_supported;
	tSirMacVHTCapabilityInfo *p_vht_cap =
			(tSirMacVHTCapabilityInfo *)(&vht_caps[2]);
	QDF_STATUS status_5g, status_2g;

	vht_caps[0] = DOT11F_EID_VHTCAPS;
	vht_caps[1] = DOT11F_IE_VHTCAPS_MAX_LEN;
	lim_set_vht_caps(mac_ctx, session, vht_caps,
			DOT11F_IE_VHTCAPS_MIN_LEN + 2);
	/*
	 * Get LDPC and over write for 5G - using channel 64 because it
	 * is available in all reg domains.
	 */
	p_vht_cap->ldpcCodingCap = lim_get_rx_ldpc(mac_ctx, CHAN_ENUM_5320);
	lim_populate_mcs_set_vht_per_vdev(mac_ctx, vht_caps, vdev_id,
					  NSS_CHAINS_BAND_5GHZ);

	nan_beamforming_supported =
		ucfg_nan_is_beamforming_supported(mac_ctx->psoc);
	if (device_mode == QDF_NDI_MODE && !nan_beamforming_supported) {
		p_vht_cap->muBeamformeeCap = 0;
		p_vht_cap->muBeamformerCap = 0;
		p_vht_cap->suBeamformeeCap = 0;
		p_vht_cap->suBeamFormerCap = 0;
	}
	/*
	 * Self VHT channel width for 5G is already negotiated
	 * with FW
	 */
	status_5g = lim_send_ie(mac_ctx, vdev_id, DOT11F_EID_VHTCAPS,
				CDS_BAND_5GHZ, &vht_caps[2],
				DOT11F_IE_VHTCAPS_MIN_LEN);
	/* Send VHT CAP for 2.4G band based on CFG_ENABLE_VHT_FOR_24GHZ ini */
	ucfg_mlme_get_vht_for_24ghz(mac_ctx->psoc, &vht_for_2g_enabled);

	if (!vht_for_2g_enabled)
		return status_5g;


	/* Get LDPC and over write for 2G */
	p_vht_cap->ldpcCodingCap = lim_get_rx_ldpc(mac_ctx, CHAN_ENUM_2437);
	/* Self VHT 80/160/80+80 channel width for 2G is 0 */
	p_vht_cap->supportedChannelWidthSet = 0;
	p_vht_cap->shortGI80MHz = 0;
	p_vht_cap->shortGI160and80plus80MHz = 0;
	lim_populate_mcs_set_vht_per_vdev(mac_ctx, vht_caps, vdev_id,
					  NSS_CHAINS_BAND_2GHZ);
	status_2g = lim_send_ie(mac_ctx, vdev_id, DOT11F_EID_VHTCAPS,
				CDS_BAND_2GHZ, &vht_caps[2],
				DOT11F_IE_VHTCAPS_MIN_LEN);

	if (QDF_IS_STATUS_SUCCESS(status_2g) &&
	    QDF_IS_STATUS_SUCCESS(status_5g))
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS lim_send_ies_per_band(struct mac_context *mac_ctx,
				 struct pe_session *session, uint8_t vdev_id,
				 enum csr_cfgdot11mode dot11_mode,
				 enum QDF_OPMODE device_mode)
{
	QDF_STATUS status_ht = QDF_STATUS_SUCCESS;
	QDF_STATUS status_vht = QDF_STATUS_SUCCESS;
	QDF_STATUS status_he = QDF_STATUS_SUCCESS;

	/*
	 * Note: Do not use Dot11f VHT structure, since 1 byte present flag in
	 * it is causing weird padding errors. Instead use Sir Mac VHT struct
	 * to send IE to wma.
	 */
	if (is_dot11mode_support_ht_cap(dot11_mode))
		status_ht = lim_send_ht_caps_ie(mac_ctx, session,
						device_mode, vdev_id);

	if (is_dot11mode_support_vht_cap(dot11_mode))
		status_vht = lim_send_vht_caps_ie(mac_ctx, session,
						  device_mode, vdev_id);

	if (is_dot11mode_support_he_cap(dot11_mode)) {
		status_he = lim_send_he_caps_ie(mac_ctx, session,
						device_mode, vdev_id);

		if (QDF_IS_STATUS_SUCCESS(status_he))
			status_he = lim_send_he_6g_band_caps_ie(mac_ctx,
								session,
								vdev_id);
	}

	if (QDF_IS_STATUS_SUCCESS(status_ht) &&
	    QDF_IS_STATUS_SUCCESS(status_vht) &&
	    QDF_IS_STATUS_SUCCESS(status_he))
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}

#ifdef WLAN_FEATURE_11AX
static
void lim_update_ext_cap_he_params(struct mac_context *mac_ctx,
				  tDot11fIEExtCap *ext_cap_data,
				  uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;
	tDot11fIEhe_cap *he_cap;
	struct s_ext_cap *p_ext_cap;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, vdev_id,
						    WLAN_LEGACY_MAC_ID);
	if (!vdev)
		return;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);
		return;
	}

	he_cap = &mlme_priv->he_config;

	p_ext_cap = (struct s_ext_cap *)ext_cap_data->bytes;
	p_ext_cap->twt_requestor_support = he_cap->twt_request;
	p_ext_cap->twt_responder_support = he_cap->twt_responder;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);

	ext_cap_data->num_bytes = lim_compute_ext_cap_ie_length(ext_cap_data);
}
#else
static inline void
lim_update_ext_cap_he_params(struct mac_context *mac_ctx,
			     tDot11fIEExtCap *ext_cap_data,
			     uint8_t vdev_id)
{}
#endif

/**
 * lim_send_ext_cap_ie() - send ext cap IE to FW
 * @mac_ctx: global MAC context
 * @session_entry: PE session
 * @extra_extcap: extracted ext cap
 * @merge: merge extra ext cap
 *
 * This function is invoked after VDEV is created to update firmware
 * about the extended capabilities that the corresponding VDEV is capable
 * of. Since STA/SAP can have different Extended capabilities set, this function
 * is called per vdev creation.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS lim_send_ext_cap_ie(struct mac_context *mac_ctx,
			       uint32_t vdev_id,
			       tDot11fIEExtCap *extra_extcap, bool merge)
{
	tDot11fIEExtCap ext_cap_data = {0};
	uint32_t dot11mode, num_bytes;
	bool vht_enabled = false;
	struct vdev_ie_info *vdev_ie;
	struct scheduler_msg msg = {0};
	QDF_STATUS status;

	dot11mode = mac_ctx->mlme_cfg->dot11_mode.dot11_mode;
	if (IS_DOT11_MODE_VHT(dot11mode))
		vht_enabled = true;

	status = populate_dot11f_ext_cap(mac_ctx, vht_enabled, &ext_cap_data,
					 NULL);
	if (QDF_STATUS_SUCCESS != status) {
		pe_err("Failed to populate ext cap IE");
		return QDF_STATUS_E_FAILURE;
	}

	lim_update_ext_cap_he_params(mac_ctx, &ext_cap_data, vdev_id);
	num_bytes = ext_cap_data.num_bytes;

	if (merge && extra_extcap && extra_extcap->num_bytes > 0) {
		if (extra_extcap->num_bytes > ext_cap_data.num_bytes)
			num_bytes = extra_extcap->num_bytes;
		lim_merge_extcap_struct(&ext_cap_data, extra_extcap, true);
	}

	/* Allocate memory for the WMI request, and copy the parameter */
	vdev_ie = qdf_mem_malloc(sizeof(*vdev_ie) + num_bytes);
	if (!vdev_ie)
		return QDF_STATUS_E_NOMEM;

	vdev_ie->vdev_id = vdev_id;
	vdev_ie->ie_id = DOT11F_EID_EXTCAP;
	vdev_ie->length = num_bytes;
	vdev_ie->band = 0;

	vdev_ie->data = (uint8_t *)vdev_ie + sizeof(*vdev_ie);
	qdf_mem_copy(vdev_ie->data, ext_cap_data.bytes, num_bytes);

	msg.type = WMA_SET_IE_INFO;
	msg.bodyptr = vdev_ie;
	msg.reserved = 0;

	if (QDF_STATUS_SUCCESS !=
		scheduler_post_message(QDF_MODULE_ID_PE,
				       QDF_MODULE_ID_WMA,
				       QDF_MODULE_ID_WMA, &msg)) {
		pe_err("Not able to post WMA_SET_IE_INFO to WDA");
		qdf_mem_free(vdev_ie);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * lim_strip_ie() - strip requested IE from IE buffer
 * @mac_ctx: global MAC context
 * @addn_ie: Additional IE buffer
 * @addn_ielen: Length of additional IE
 * @eid: EID of IE to strip
 * @size_of_len_field: length of IE length field
 * @oui: if present matches OUI also
 * @oui_length: if previous present, this is length of oui
 * @extracted_ie: if not NULL, copy the stripped IE to this buffer
 *
 * This utility function is used to strip of the requested IE if present
 * in IE buffer.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS lim_strip_ie(struct mac_context *mac_ctx,
		uint8_t *addn_ie, uint16_t *addn_ielen,
		uint8_t eid, eSizeOfLenField size_of_len_field,
		uint8_t *oui, uint8_t oui_length, uint8_t *extracted_ie,
		uint32_t eid_max_len)
{
	uint8_t *tempbuf = NULL;
	uint16_t templen = 0;
	int left = *addn_ielen;
	uint8_t *ptr = addn_ie;
	uint8_t elem_id;
	uint16_t elem_len, ie_len, extracted_ie_len = 0;

	if (!addn_ie) {
		pe_debug("NULL addn_ie pointer");
		return QDF_STATUS_E_INVAL;
	}

	tempbuf = qdf_mem_malloc(left);
	if (!tempbuf)
		return QDF_STATUS_E_NOMEM;

	if (extracted_ie)
		qdf_mem_zero(extracted_ie, eid_max_len + size_of_len_field + 1);

	while (left >= 2) {
		elem_id  = ptr[0];
		left -= 1;
		if (size_of_len_field == TWO_BYTE) {
			elem_len = *((uint16_t *)&ptr[1]);
			left -= 2;
		} else {
			elem_len = ptr[1];
			left -= 1;
		}
		if (elem_len > left) {
			pe_err("Invalid IEs eid: %d elem_len: %d left: %d",
				elem_id, elem_len, left);
			qdf_mem_free(tempbuf);
			return QDF_STATUS_E_FAILURE;
		}

		if (eid != elem_id ||
				(oui && qdf_mem_cmp(oui,
						&ptr[size_of_len_field + 1],
						oui_length))) {
			qdf_mem_copy(tempbuf + templen, &ptr[0],
				     elem_len + size_of_len_field + 1);
			templen += (elem_len + size_of_len_field + 1);
		} else {
			/*
			 * eid matched and if provided OUI also matched
			 * take oui IE and store in provided buffer.
			 */
			if (extracted_ie) {
				ie_len = elem_len + size_of_len_field + 1;
				if (ie_len <= eid_max_len - extracted_ie_len) {
					qdf_mem_copy(
					extracted_ie + extracted_ie_len,
					&ptr[0], ie_len);
					extracted_ie_len += ie_len;
				}
			}
		}
		left -= elem_len;
		ptr += (elem_len + size_of_len_field + 1);
	}
	qdf_mem_copy(addn_ie, tempbuf, templen);

	*addn_ielen = templen;
	qdf_mem_free(tempbuf);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11W
void lim_del_pmf_sa_query_timer(struct mac_context *mac_ctx, struct pe_session *pe_session)
{
	uint32_t associated_sta;
	tpDphHashNode sta_ds = NULL;

	for (associated_sta = 1;
			associated_sta <=
			mac_ctx->lim.max_sta_of_pe_session;
			associated_sta++) {
		sta_ds = dph_get_hash_entry(mac_ctx, associated_sta,
				&pe_session->dph.dphHashTable);
		if (!sta_ds)
			continue;
		if (!sta_ds->rmfEnabled) {
			pe_debug("no PMF timer for assoc-id:%d sta mac"
				 QDF_MAC_ADDR_FMT, sta_ds->assocId,
				 QDF_MAC_ADDR_REF(sta_ds->staAddr));
			continue;
		}

		pe_debug("Deleting pmfSaQueryTimer for assoc-id:%d sta mac"
			 QDF_MAC_ADDR_FMT, sta_ds->assocId,
			 QDF_MAC_ADDR_REF(sta_ds->staAddr));
		tx_timer_deactivate(&sta_ds->pmfSaQueryTimer);
		tx_timer_delete(&sta_ds->pmfSaQueryTimer);
	}
}
#endif

QDF_STATUS lim_strip_supp_op_class_update_struct(struct mac_context *mac_ctx,
		uint8_t *addn_ie, uint16_t *addn_ielen,
		tDot11fIESuppOperatingClasses *dst)
{
	uint8_t extracted_buff[DOT11F_IE_SUPPOPERATINGCLASSES_MAX_LEN + 2];
	QDF_STATUS status;

	qdf_mem_zero((uint8_t *)&extracted_buff[0],
		    DOT11F_IE_SUPPOPERATINGCLASSES_MAX_LEN + 2);
	status = lim_strip_ie(mac_ctx, addn_ie, addn_ielen,
			      DOT11F_EID_SUPPOPERATINGCLASSES, ONE_BYTE,
			      NULL, 0, extracted_buff,
			      DOT11F_IE_SUPPOPERATINGCLASSES_MAX_LEN);
	if (QDF_STATUS_SUCCESS != status) {
		pe_warn("Failed to strip supp_op_mode IE status: %d",
		       status);
		return status;
	}

	if (DOT11F_EID_SUPPOPERATINGCLASSES != extracted_buff[0] ||
	    extracted_buff[1] > DOT11F_IE_SUPPOPERATINGCLASSES_MAX_LEN) {
		pe_warn("Invalid IEs eid: %d elem_len: %d",
			extracted_buff[0], extracted_buff[1]);
		return QDF_STATUS_E_FAILURE;
	}

	/* update the extracted supp op class to struct*/
	if (DOT11F_PARSE_SUCCESS != dot11f_unpack_ie_supp_operating_classes(
	    mac_ctx, &extracted_buff[2], extracted_buff[1], dst, false)) {
		pe_err("dot11f_unpack Parse Error");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

uint8_t lim_op_class_from_bandwidth(struct mac_context *mac_ctx,
				    uint16_t channel_freq,
				    enum phy_ch_width ch_bandwidth,
				    enum offset_t offset)
{
	uint8_t op_class = 0;
	uint16_t ch_behav_limit = BEHAV_NONE;
	uint8_t channel;

	if (ch_bandwidth == CH_WIDTH_40MHZ &&
	    wlan_reg_is_24ghz_ch_freq(channel_freq)) {
		if (offset == BW40_LOW_PRIMARY)
			ch_behav_limit = BEHAV_BW40_LOW_PRIMARY;
		else
			ch_behav_limit = BEHAV_BW40_HIGH_PRIMARY;
	} else if (ch_bandwidth == CH_WIDTH_80P80MHZ) {
		ch_behav_limit = BEHAV_BW80_PLUS;
	}
	wlan_reg_freq_width_to_chan_op_class(mac_ctx->pdev, channel_freq,
					     ch_width_in_mhz(ch_bandwidth),
					     true, BIT(ch_behav_limit),
					     &op_class, &channel);

	return op_class;
}

/**
 * lim_update_extcap_struct() - poputlate the dot11f structure
 * @mac_ctx: global MAC context
 * @buf: extracted IE buffer
 * @dst: extended capability IE structure to be updated
 *
 * This function is used to update the extended capability structure
 * with @buf.
 *
 * Return: None
 */
void lim_update_extcap_struct(struct mac_context *mac_ctx,
	uint8_t *buf, tDot11fIEExtCap *dst)
{
	uint8_t out[DOT11F_IE_EXTCAP_MAX_LEN];
	uint32_t status;

	if (!buf) {
		pe_err("Invalid Buffer Address");
		return;
	}

	if (!dst) {
		pe_err("NULL dst pointer");
		return;
	}

	if (DOT11F_EID_EXTCAP != buf[0] || buf[1] > DOT11F_IE_EXTCAP_MAX_LEN) {
		pe_debug_rl("Invalid IEs eid: %d elem_len: %d", buf[0], buf[1]);
		return;
	}

	qdf_mem_zero((uint8_t *)&out[0], DOT11F_IE_EXTCAP_MAX_LEN);
	qdf_mem_copy(&out[0], &buf[2], buf[1]);

	status = dot11f_unpack_ie_ext_cap(mac_ctx, &out[0],
					buf[1], dst, false);
	if (DOT11F_PARSE_SUCCESS != status)
		pe_err("dot11f_unpack Parse Error %d", status);
}

/**
 * lim_strip_extcap_update_struct - strip extended capability IE and populate
 *				  the dot11f structure
 * @mac_ctx: global MAC context
 * @addn_ie: Additional IE buffer
 * @addn_ielen: Length of additional IE
 * @dst: extended capability IE structure to be updated
 *
 * This function is used to strip extended capability IE from IE buffer and
 * update the passed structure.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS lim_strip_extcap_update_struct(struct mac_context *mac_ctx,
		uint8_t *addn_ie, uint16_t *addn_ielen, tDot11fIEExtCap *dst)
{
	uint8_t extracted_buff[DOT11F_IE_EXTCAP_MAX_LEN + 2];
	QDF_STATUS status;

	qdf_mem_zero((uint8_t *)&extracted_buff[0],
			DOT11F_IE_EXTCAP_MAX_LEN + 2);
	status = lim_strip_ie(mac_ctx, addn_ie, addn_ielen,
			      DOT11F_EID_EXTCAP, ONE_BYTE,
			      NULL, 0, extracted_buff,
			      DOT11F_IE_EXTCAP_MAX_LEN);
	if (QDF_STATUS_SUCCESS != status) {
		pe_debug("Failed to strip extcap IE status: %d", status);
		return status;
	}

	/* update the extracted ExtCap to struct*/
	lim_update_extcap_struct(mac_ctx, extracted_buff, dst);
	return status;
}

/**
 * lim_merge_extcap_struct() - merge extended capabilities info
 * @dst: destination extended capabilities
 * @src: source extended capabilities
 * @add: true if add the capabilities, false if strip the capabilities.
 *
 * This function is used to take @src info and add/strip it to/from
 * @dst extended capabilities info.
 *
 * Return: None
 */
void lim_merge_extcap_struct(tDot11fIEExtCap *dst,
			     tDot11fIEExtCap *src,
			     bool add)
{
	uint8_t *tempdst = (uint8_t *)dst->bytes;
	uint8_t *tempsrc = (uint8_t *)src->bytes;
	uint8_t structlen = member_size(tDot11fIEExtCap, bytes);

	/* Return if @src not present */
	if (!src->present)
		return;

	/* Return if strip the capabilities from @dst which not present */
	if (!dst->present && !add)
		return;

	/* Merge the capabilities info in other cases */
	while (tempdst && tempsrc && structlen--) {
		if (add)
			*tempdst |= *tempsrc;
		else
			*tempdst &= *tempsrc;
		tempdst++;
		tempsrc++;
	}
	dst->num_bytes = lim_compute_ext_cap_ie_length(dst);
	if (dst->num_bytes == 0)
		dst->present = 0;
	else
		dst->present = 1;
}

/**
 * lim_send_action_frm_tb_ppdu_cfg_flush_cb() - flush TB PPDU cfg msg
 * @msg: Message pointer
 *
 * Flushes the send action frame in HE TB PPDU configuration message.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
lim_send_action_frm_tb_ppdu_cfg_flush_cb(struct scheduler_msg *msg)
{
	if (msg->bodyptr) {
		qdf_mem_free(msg->bodyptr);
		msg->bodyptr = NULL;
	}
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS lim_send_action_frm_tb_ppdu_cfg(struct mac_context *mac_ctx,
					   uint32_t vdev_id, uint8_t cfg)
{
	tDot11fvendor_action_frame *frm;
	uint8_t frm_len = sizeof(*frm);
	struct pe_session *session;
	struct cfg_action_frm_tb_ppdu *cfg_msg;
	struct scheduler_msg msg = {0};
	uint8_t *data_buf;

	session = pe_find_session_by_vdev_id(mac_ctx, vdev_id);
	if (!session) {
		pe_err("pe session does not exist for vdev_id %d",
		       vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	data_buf = qdf_mem_malloc(frm_len + sizeof(*cfg_msg));
	if (!data_buf)
		return QDF_STATUS_E_FAILURE;

	cfg_msg = (struct cfg_action_frm_tb_ppdu *)data_buf;

	frm = (tDot11fvendor_action_frame *)(data_buf + sizeof(*cfg_msg));

	frm->Category.category = SIR_MAC_ACTION_VENDOR_SPECIFIC_CATEGORY;

	frm->vendor_oui.oui_data[0] = 0x00;
	frm->vendor_oui.oui_data[1] = 0xA0;
	frm->vendor_oui.oui_data[2] = 0xC6;

	frm->vendor_action_subtype.subtype = 0xFF;

	cfg_msg->vdev_id = vdev_id;
	cfg_msg->cfg = cfg;
	cfg_msg->frm_len = frm_len;
	cfg_msg->data = (uint8_t *)frm;

	msg.type = WMA_CFG_VENDOR_ACTION_TB_PPDU;
	msg.bodyptr = cfg_msg;
	msg.reserved = 0;
	msg.flush_callback = lim_send_action_frm_tb_ppdu_cfg_flush_cb;

	if (QDF_STATUS_SUCCESS !=
		scheduler_post_message(QDF_MODULE_ID_PE,
				       QDF_MODULE_ID_WMA,
				       QDF_MODULE_ID_WMA, &msg)) {
		pe_err("Not able to post WMA_SET_IE_INFO to WDA");
		qdf_mem_free(data_buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * lim_get_80Mhz_center_channel - finds 80 Mhz center channel
 *
 * @primary_channel:   Primary channel for given 80 MHz band
 *
 * There are fixed 80MHz band and for each fixed band there is only one center
 * valid channel. Also location of primary channel decides what 80 MHz band will
 * it use, hence it decides what center channel will be used. This function
 * does thus calculation and returns the center channel.
 *
 * Return: center channel
 */
uint8_t
lim_get_80Mhz_center_channel(uint8_t primary_channel)
{
	if (primary_channel >= 36 && primary_channel <= 48)
		return (36+48)/2;
	if (primary_channel >= 52 && primary_channel <= 64)
		return (52+64)/2;
	if (primary_channel >= 100 && primary_channel <= 112)
		return (100+112)/2;
	if (primary_channel >= 116 && primary_channel <= 128)
		return (116+128)/2;
	if (primary_channel >= 132 && primary_channel <= 144)
		return (132+144)/2;
	if (primary_channel >= 149 && primary_channel <= 161)
		return (149+161)/2;

	return INVALID_CHANNEL_ID;
}

/**
 * lim_bss_type_to_string(): converts bss type enum to string.
 * @bss_type: enum value of bss_type.
 *
 * Return: Printable string for bss_type
 */
const char *lim_bss_type_to_string(const uint16_t bss_type)
{
	switch (bss_type) {
	CASE_RETURN_STRING(eSIR_INFRASTRUCTURE_MODE);
	CASE_RETURN_STRING(eSIR_INFRA_AP_MODE);
	CASE_RETURN_STRING(eSIR_AUTO_MODE);
	CASE_RETURN_STRING(eSIR_NDI_MODE);
	default:
		return "Unknown bss_type";
	}
}

/**
 * lim_init_obss_params(): Initializes the OBSS Scan Parameters
 * @sesssion: LIM session
 * @mac_ctx: Mac context
 *
 * Return: None
 */
void lim_init_obss_params(struct mac_context *mac_ctx, struct pe_session *session)
{
	struct wlan_mlme_obss_ht40 *obss_ht40;

	if (!(mac_ctx->mlme_cfg)) {
		pe_err("invalid mlme cfg");
		return;
	}

	obss_ht40  = &mac_ctx->mlme_cfg->obss_ht40;

	session->obss_ht40_scanparam.obss_active_dwelltime =
		obss_ht40->active_dwelltime;

	session->obss_ht40_scanparam.obss_passive_dwelltime =
		obss_ht40->passive_dwelltime;

	session->obss_ht40_scanparam.obss_width_trigger_interval =
		obss_ht40->width_trigger_interval;

	session->obss_ht40_scanparam.obss_active_total_per_channel =
		obss_ht40->active_per_channel;

	session->obss_ht40_scanparam.obss_passive_total_per_channel =
		obss_ht40->passive_per_channel;

	session->obss_ht40_scanparam.bsswidth_ch_trans_delay =
		obss_ht40->width_trans_delay;

	session->obss_ht40_scanparam.obss_activity_threshold =
		obss_ht40->scan_activity_threshold;
}

/**
 * lim_update_obss_scanparams(): Updates OBSS SCAN IE parameters to session
 * @sesssion: LIM session
 * @scan_params: Scan parameters
 *
 * Return: None
 */
void lim_update_obss_scanparams(struct pe_session *session,
			tDot11fIEOBSSScanParameters *scan_params)
{
	/*
	 * If the received value is not in the range specified
	 * by the Specification then it will be the default value
	 * configured through cfg
	 */
	if ((scan_params->obssScanActiveDwell >
		cfg_min(CFG_OBSS_HT40_SCAN_ACTIVE_DWELL_TIME)) &&
		(scan_params->obssScanActiveDwell <
		cfg_max(CFG_OBSS_HT40_SCAN_ACTIVE_DWELL_TIME)))
		session->obss_ht40_scanparam.obss_active_dwelltime =
			scan_params->obssScanActiveDwell;

	if ((scan_params->obssScanPassiveDwell >
		cfg_min(CFG_OBSS_HT40_SCAN_PASSIVE_DWELL_TIME)) &&
		(scan_params->obssScanPassiveDwell <
		cfg_max(CFG_OBSS_HT40_SCAN_PASSIVE_DWELL_TIME)))
		session->obss_ht40_scanparam.obss_passive_dwelltime =
			scan_params->obssScanPassiveDwell;

	if ((scan_params->bssWidthChannelTransitionDelayFactor >
		cfg_min(CFG_OBSS_HT40_WIDTH_CH_TRANSITION_DELAY)) &&
		(scan_params->bssWidthChannelTransitionDelayFactor <
		cfg_max(CFG_OBSS_HT40_WIDTH_CH_TRANSITION_DELAY)))
		session->obss_ht40_scanparam.bsswidth_ch_trans_delay =
			scan_params->bssWidthChannelTransitionDelayFactor;

	if ((scan_params->obssScanActiveTotalPerChannel >
		cfg_min(CFG_OBSS_HT40_SCAN_ACTIVE_TOTAL_PER_CHANNEL)) &&
		(scan_params->obssScanActiveTotalPerChannel <
		cfg_max(CFG_OBSS_HT40_SCAN_ACTIVE_TOTAL_PER_CHANNEL)))
		session->obss_ht40_scanparam.obss_active_total_per_channel =
			scan_params->obssScanActiveTotalPerChannel;

	if ((scan_params->obssScanPassiveTotalPerChannel >
		cfg_min(CFG_OBSS_HT40_SCAN_PASSIVE_TOTAL_PER_CHANNEL)) &&
		(scan_params->obssScanPassiveTotalPerChannel <
		cfg_max(CFG_OBSS_HT40_SCAN_PASSIVE_TOTAL_PER_CHANNEL)))
		session->obss_ht40_scanparam.obss_passive_total_per_channel =
			scan_params->obssScanPassiveTotalPerChannel;

	if ((scan_params->bssChannelWidthTriggerScanInterval >
		cfg_min(CFG_OBSS_HT40_SCAN_WIDTH_TRIGGER_INTERVAL)) &&
		(scan_params->bssChannelWidthTriggerScanInterval <
		cfg_max(CFG_OBSS_HT40_SCAN_WIDTH_TRIGGER_INTERVAL)))
		session->obss_ht40_scanparam.obss_width_trigger_interval =
			scan_params->bssChannelWidthTriggerScanInterval;

	if ((scan_params->obssScanActivityThreshold >
		cfg_min(CFG_OBSS_HT40_SCAN_ACTIVITY_THRESHOLD)) &&
		(scan_params->obssScanActivityThreshold <
		cfg_max(CFG_OBSS_HT40_SCAN_ACTIVITY_THRESHOLD)))
		session->obss_ht40_scanparam.obss_activity_threshold =
			scan_params->obssScanActivityThreshold;
	return;
}

/**
 * lim_is_robust_mgmt_action_frame() - Check if action category is
 * robust action frame
 * @action_category: Action frame category.
 *
 * This function is used to check if given action category is robust
 * action frame.
 *
 * Return: bool
 */
bool lim_is_robust_mgmt_action_frame(uint8_t action_category)
{
	switch (action_category) {
	/*
	 * NOTE: This function doesn't take care of the DMG
	 * (Directional Multi-Gigatbit) BSS case as 8011ad
	 * support is not yet added. In future, if the support
	 * is required then this function need few more arguments
	 * and little change in logic.
	 */
	case ACTION_CATEGORY_SPECTRUM_MGMT:
	case ACTION_CATEGORY_QOS:
	case ACTION_CATEGORY_DLS:
	case ACTION_CATEGORY_BACK:
	case ACTION_CATEGORY_RRM:
	case ACTION_FAST_BSS_TRNST:
	case ACTION_CATEGORY_SA_QUERY:
	case ACTION_CATEGORY_PROTECTED_DUAL_OF_PUBLIC_ACTION:
	case ACTION_CATEGORY_WNM:
	case ACTION_CATEGORY_MESH_ACTION:
	case ACTION_CATEGORY_MULTIHOP_ACTION:
	case ACTION_CATEGORY_FST:
		return true;
	default:
		pe_debug("non-PMF action category: %d", action_category);
		break;
	}
	return false;
}

/**
 * lim_compute_ext_cap_ie_length - compute the length of ext cap ie
 * based on the bits set
 * @ext_cap: extended IEs structure
 *
 * Return: length of the ext cap ie, 0 means should not present
 */
uint8_t lim_compute_ext_cap_ie_length(tDot11fIEExtCap *ext_cap)
{
	uint8_t i = DOT11F_IE_EXTCAP_MAX_LEN;

	while (i) {
		if (ext_cap->bytes[i-1])
			break;
		i--;
	}

	return i;
}

/**
 * lim_update_caps_info_for_bss - Update capability info for this BSS
 *
 * @mac_ctx: mac context
 * @caps: Pointer to capability info to be updated
 * @bss_caps: Capability info of the BSS
 *
 * Update the capability info in Assoc/Reassoc request frames and reset
 * the spectrum management, short preamble, immediate block ack bits
 * if the BSS doesnot support it
 *
 * Return: None
 */
void lim_update_caps_info_for_bss(struct mac_context *mac_ctx,
		uint16_t *caps, uint16_t bss_caps)
{
	if (!(bss_caps & LIM_SPECTRUM_MANAGEMENT_BIT_MASK)) {
		*caps &= (~LIM_SPECTRUM_MANAGEMENT_BIT_MASK);
		pe_debug("Clearing spectrum management:no AP support");
	}

	if (!(bss_caps & LIM_SHORT_PREAMBLE_BIT_MASK)) {
		*caps &= (~LIM_SHORT_PREAMBLE_BIT_MASK);
		pe_debug("Clearing short preamble:no AP support");
	}

	if (!(bss_caps & LIM_IMMEDIATE_BLOCK_ACK_MASK)) {
		*caps &= (~LIM_IMMEDIATE_BLOCK_ACK_MASK);
		pe_debug("Clearing Immed Blk Ack:no AP support");
	}
}
/**
 * lim_send_set_dtim_period(): Send SIR_HAL_SET_DTIM_PERIOD message
 * to set dtim period.
 *
 * @sesssion: LIM session
 * @dtim_period: dtim value
 * @mac_ctx: Mac context
 * @return None
 */
void lim_send_set_dtim_period(struct mac_context *mac_ctx, uint8_t dtim_period,
			      struct pe_session *session)
{
	struct set_dtim_params *dtim_params = NULL;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;
	struct scheduler_msg msg = {0};

	if (!session) {
		pe_err("Inavalid parameters");
		return;
	}
	dtim_params = qdf_mem_malloc(sizeof(*dtim_params));
	if (!dtim_params)
		return;
	dtim_params->dtim_period = dtim_period;
	dtim_params->session_id = session->smeSessionId;
	msg.type = WMA_SET_DTIM_PERIOD;
	msg.bodyptr = dtim_params;
	msg.bodyval = 0;
	pe_debug("Post WMA_SET_DTIM_PERIOD to WMA");
	ret = wma_post_ctrl_msg(mac_ctx, &msg);
	if (QDF_STATUS_SUCCESS != ret) {
		pe_err("wma_post_ctrl_msg() failed");
		qdf_mem_free(dtim_params);
	}
}

/**
 * lim_is_valid_frame(): validate RX frame using last processed frame details
 * to find if it is duplicate frame.
 *
 * @last_processed_frm: last processed frame pointer.
 * @pRxPacketInfo: RX packet.
 *
 * Frame treat as duplicate:
 * if retry bit is set and
 *	 if source address and seq number matches with the last processed frame
 *
 * Return: false if duplicate frame, else true.
 */
bool lim_is_valid_frame(last_processed_msg *last_processed_frm,
		uint8_t *pRxPacketInfo)
{
	uint16_t seq_num;
	tpSirMacMgmtHdr pHdr;

	if (!pRxPacketInfo) {
		pe_err("Invalid RX frame");
		return false;
	}

	pHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);

	if (pHdr->fc.retry == 0)
		return true;

	seq_num = (((pHdr->seqControl.seqNumHi <<
			HIGH_SEQ_NUM_OFFSET) |
			pHdr->seqControl.seqNumLo));

	if (last_processed_frm->seq_num == seq_num &&
		qdf_mem_cmp(last_processed_frm->sa, pHdr->sa, ETH_ALEN) == 0) {
		pe_err("Duplicate frame from "QDF_MAC_ADDR_FMT " Seq Number %d",
		QDF_MAC_ADDR_REF(pHdr->sa), seq_num);
		return false;
	}
	return true;
}

/**
 * lim_update_last_processed_frame(): update new processed frame info to cache.
 *
 * @last_processed_frm: last processed frame pointer.
 * @pRxPacketInfo: Successfully processed RX packet.
 *
 * Return: None.
 */
void lim_update_last_processed_frame(last_processed_msg *last_processed_frm,
		uint8_t *pRxPacketInfo)
{
	uint16_t seq_num;
	tpSirMacMgmtHdr pHdr;

	if (!pRxPacketInfo) {
		pe_err("Invalid RX frame");
		return;
	}

	pHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);
	seq_num = (((pHdr->seqControl.seqNumHi <<
			HIGH_SEQ_NUM_OFFSET) |
			pHdr->seqControl.seqNumLo));

	qdf_mem_copy(last_processed_frm->sa, pHdr->sa, ETH_ALEN);
	last_processed_frm->seq_num = seq_num;
}

#if defined(CONFIG_BAND_6GHZ) && defined(WLAN_FEATURE_11AX)
static bool lim_support_6ghz_band_op_class(struct mac_context *mac_ctx,
					   tDot11fIESuppOperatingClasses *
					   op_class_ie)
{
	uint16_t i;

	if (!op_class_ie->present)
		return false;
	for (i = 0; i < op_class_ie->num_classes; i++) {
		if (wlan_reg_is_6ghz_op_class(mac_ctx->pdev,
					      op_class_ie->classes[i]))
			break;
	}
	if (i < op_class_ie->num_classes)
		return true;

	return false;
}

void lim_ap_check_6g_compatible_peer(struct mac_context *mac_ctx,
				     struct pe_session *session)
{
	uint16_t i;
	tpDphHashNode sta_ds;
	bool legacy_client_present = false;

	if (!LIM_IS_AP_ROLE(session))
		return;

	for (i = 1; i < session->dph.dphHashTable.size; i++) {
		sta_ds = dph_get_hash_entry(mac_ctx, i,
					    &session->dph.dphHashTable);
		if (!sta_ds)
			continue;
		if (sta_ds->staType != STA_ENTRY_PEER)
			continue;
		if (!lim_support_6ghz_band_op_class(
			mac_ctx, &sta_ds->supp_operating_classes)) {
			legacy_client_present = true;
			pe_debug("peer "QDF_MAC_ADDR_FMT" 6ghz not supported",
				 QDF_MAC_ADDR_REF(sta_ds->staAddr));
			break;
		}
		pe_debug("peer "QDF_MAC_ADDR_FMT" 6ghz supported",
			 QDF_MAC_ADDR_REF(sta_ds->staAddr));
	}
	if (legacy_client_present)
		policy_mgr_set_ap_6ghz_capable(
			mac_ctx->psoc, session->vdev_id, false,
			CONN_6GHZ_FLAG_NO_LEGACY_CLIENT);
	else
		policy_mgr_set_ap_6ghz_capable(
			mac_ctx->psoc, session->vdev_id, true,
			CONN_6GHZ_FLAG_NO_LEGACY_CLIENT);
}
#endif

#ifdef WLAN_FEATURE_11AX
void lim_update_he_6ghz_band_caps(struct mac_context *mac,
				  tDot11fIEhe_6ghz_band_cap *he_6ghz_band_cap,
				  tpAddStaParams params)
{
	qdf_mem_copy(&params->he_6ghz_band_caps, he_6ghz_band_cap,
		     sizeof(tDot11fIEhe_6ghz_band_cap));

	lim_log_he_6g_cap(mac, &params->he_6ghz_band_caps);
}

void lim_add_he_cap(struct mac_context *mac_ctx, struct pe_session *pe_session,
		    tpAddStaParams add_sta_params, tpSirAssocReq assoc_req)
{
	if (!add_sta_params->he_capable || !assoc_req)
		return;

	qdf_mem_copy(&add_sta_params->he_config, &assoc_req->he_cap,
		     sizeof(add_sta_params->he_config));

	add_sta_params->he_mcs_12_13_map =
		assoc_req->qcn_ie.he_mcs13_attr.he_mcs_12_13_supp_80 |
		assoc_req->qcn_ie.he_mcs13_attr.he_mcs_12_13_supp_160 << 8;

	if (lim_is_he_6ghz_band(pe_session))
		lim_update_he_6ghz_band_caps(mac_ctx,
					     &assoc_req->he_6ghz_band_cap,
					     add_sta_params);

}

void lim_add_self_he_cap(tpAddStaParams add_sta_params, struct pe_session *session)
{
	if (!session)
		return;

	add_sta_params->he_capable = true;

	qdf_mem_copy(&add_sta_params->he_config, &session->he_config,
		     sizeof(add_sta_params->he_config));
	qdf_mem_copy(&add_sta_params->he_op, &session->he_op,
		     sizeof(add_sta_params->he_op));
}

static bool lim_check_is_bss_greater_than_4_nss_supp(struct pe_session *session,
						     tDot11fIEhe_cap *he_cap)
{
	uint8_t i;
	uint16_t mcs_map;
#define NSS_4 4
#define NSS_8 8

	if (!session->he_capable || !he_cap->present)
		return false;
	mcs_map = he_cap->rx_he_mcs_map_lt_80;
	for (i = NSS_4; i < NSS_8; i++) {
		if (((mcs_map >> (i * 2)) & 0x3) != 0x3)
			return true;
	}

	return false;
}

bool lim_check_he_80_mcs11_supp(struct pe_session *session,
				tDot11fIEhe_cap *he_cap)
{
	uint16_t rx_mcs_map;
	uint16_t tx_mcs_map;

	rx_mcs_map = he_cap->rx_he_mcs_map_lt_80;
	tx_mcs_map = he_cap->tx_he_mcs_map_lt_80;
	if ((session->nss == NSS_1x1_MODE) &&
	    ((HE_GET_MCS_4_NSS(rx_mcs_map, 1) == HE_MCS_0_11) ||
	     (HE_GET_MCS_4_NSS(tx_mcs_map, 1) == HE_MCS_0_11)))
		return true;

	if ((session->nss == NSS_2x2_MODE) &&
	    ((HE_GET_MCS_4_NSS(rx_mcs_map, 2) == HE_MCS_0_11) ||
	     (HE_GET_MCS_4_NSS(tx_mcs_map, 2) == HE_MCS_0_11)))
		return true;

	return false;
}

/**
 * lim_check_he_ldpc_cap() - set he ldpc coding to one if
 * channel width is > 20 or mcs 10/11 bit are supported or
 * nss is greater than 4.
 * @beacon_struct: beacon structure
 * @session: A pointer to session entry.
 *
 * Return: None
 */

void lim_check_and_force_he_ldpc_cap(struct pe_session *session,
				     tDot11fIEhe_cap *he_cap)
{
	if (!he_cap->ldpc_coding &&
	    (session->ch_width > CH_WIDTH_20MHZ ||
	    lim_check_he_80_mcs11_supp(session, he_cap) ||
	    lim_check_is_bss_greater_than_4_nss_supp(session, he_cap)))
		he_cap->ldpc_coding = 1;
}

/**
 * lim_intersect_he_caps() - Intersect peer capability and self capability
 * @rcvd_he: pointer to received peer capability
 * @peer_he: pointer to Intersected capability
 * @session: A pointer to session entry.
 *
 * Return: None
 */
static void lim_intersect_he_caps(tDot11fIEhe_cap *rcvd_he,
				  tDot11fIEhe_cap *peer_he,
				  struct pe_session *session)
{
	uint8_t val;
	tDot11fIEhe_cap *session_he = &session->he_config;

	qdf_mem_copy(peer_he, rcvd_he, sizeof(*peer_he));

	peer_he->fragmentation = QDF_MIN(session_he->fragmentation,
					 peer_he->fragmentation);

	peer_he->ldpc_coding &= session_he->ldpc_coding;
	lim_check_and_force_he_ldpc_cap(session, peer_he);

	if (session_he->tb_ppdu_tx_stbc_lt_80mhz && peer_he->rx_stbc_lt_80mhz)
		peer_he->rx_stbc_lt_80mhz = 1;
	else
		peer_he->rx_stbc_lt_80mhz = 0;

	if (session_he->rx_stbc_lt_80mhz && peer_he->tb_ppdu_tx_stbc_lt_80mhz)
		peer_he->tb_ppdu_tx_stbc_lt_80mhz = 1;
	else
		peer_he->tb_ppdu_tx_stbc_lt_80mhz = 0;

	if (session_he->tb_ppdu_tx_stbc_gt_80mhz && peer_he->rx_stbc_gt_80mhz)
		peer_he->rx_stbc_gt_80mhz = 1;
	else
		peer_he->rx_stbc_gt_80mhz = 0;

	if (session_he->rx_stbc_gt_80mhz && peer_he->tb_ppdu_tx_stbc_gt_80mhz)
		peer_he->tb_ppdu_tx_stbc_gt_80mhz = 1;
	else
		peer_he->tb_ppdu_tx_stbc_gt_80mhz = 0;

	/* Tx Doppler is first bit and Rx Doppler is second bit */
	if (session_he->doppler) {
		val = 0;
		if ((session_he->doppler & 0x1) && (peer_he->doppler & 0x10))
			val |= (1 << 1);
		if ((session_he->doppler & 0x10) && (peer_he->doppler & 0x1))
			val |= (1 << 0);
		peer_he->doppler = val;
	}

	peer_he->su_beamformer = session_he->su_beamformee ?
					peer_he->su_beamformer : 0;
	peer_he->su_beamformee = (session_he->su_beamformer ||
				  session_he->mu_beamformer) ?
					peer_he->su_beamformee : 0;
	peer_he->mu_beamformer = session_he->su_beamformee ?
					peer_he->mu_beamformer : 0;

	peer_he->twt_request = session_he->twt_responder ?
					peer_he->twt_request : 0;
	peer_he->twt_responder = session_he->twt_request ?
					peer_he->twt_responder : 0;
}

void lim_intersect_sta_he_caps(struct mac_context *mac_ctx,
			       tpSirAssocReq assoc_req,
			       struct pe_session *session,
			       tpDphHashNode sta_ds)
{
	tDot11fIEhe_cap *rcvd_he = &assoc_req->he_cap;
	tDot11fIEhe_cap *peer_he = &sta_ds->he_config;

	if (!sta_ds->mlmStaContext.he_capable)
		return;

	/* If HE is not supported, do not fill sta_ds and return */
	if (!IS_DOT11_MODE_HE(session->dot11mode))
		return;

	lim_intersect_he_caps(rcvd_he, peer_he, session);

	/* If MCS 12/13 is supported from assoc QCN IE */
	if (assoc_req->qcn_ie.present &&
	    assoc_req->qcn_ie.he_mcs13_attr.present) {
		sta_ds->he_mcs_12_13_map =
		assoc_req->qcn_ie.he_mcs13_attr.he_mcs_12_13_supp_80 |
		assoc_req->qcn_ie.he_mcs13_attr.he_mcs_12_13_supp_160 << 8;
	} else {
		return;
	}

	/* Take intersection of FW capability for HE MCS 12/13 */
	if (wlan_reg_is_24ghz_ch_freq(session->curr_op_freq))
		sta_ds->he_mcs_12_13_map &=
			mac_ctx->mlme_cfg->he_caps.he_mcs_12_13_supp_2g;
	else
		sta_ds->he_mcs_12_13_map &=
			mac_ctx->mlme_cfg->he_caps.he_mcs_12_13_supp_5g;
}

void lim_intersect_ap_he_caps(struct pe_session *session, struct bss_params *add_bss,
		tSchBeaconStruct *beacon, tpSirAssocRsp assoc_rsp)
{
	tDot11fIEhe_cap *rcvd_he;
	tDot11fIEhe_cap *peer_he = &add_bss->staContext.he_config;

	if (assoc_rsp && assoc_rsp->he_cap.present)
		rcvd_he = &assoc_rsp->he_cap;
	else
		rcvd_he = &beacon->he_cap;

	lim_intersect_he_caps(rcvd_he, peer_he, session);
	add_bss->staContext.he_capable = true;

	if (wlan_mlme_is_bad_htc_he_iot_ap(session->vdev)) {
		peer_he->htc_he = 0;
		pe_debug("disable ht control in he cap for iot ap");
	}
}

void lim_add_bss_he_cap(struct bss_params *add_bss, tpSirAssocRsp assoc_rsp)
{
	tDot11fIEhe_cap *he_cap;
	tDot11fIEhe_op *he_op;

	he_cap = &assoc_rsp->he_cap;
	he_op = &assoc_rsp->he_op;
	add_bss->he_capable = he_cap->present;
	if (he_cap)
		qdf_mem_copy(&add_bss->staContext.he_config,
			     he_cap, sizeof(*he_cap));
	if (he_op)
		qdf_mem_copy(&add_bss->staContext.he_op,
			     he_op, sizeof(*he_op));
}

void lim_add_bss_he_cfg(struct bss_params *add_bss, struct pe_session *session)
{
	add_bss->he_sta_obsspd = session->he_sta_obsspd;
}

void lim_update_he_6gop_assoc_resp(struct bss_params *pAddBssParams,
				   tDot11fIEhe_op *he_op,
				   struct pe_session *pe_session)
{
	if (!pe_session->he_6ghz_band)
		return;

	if (!he_op->oper_info_6g_present) {
		pe_debug("6G operation info not present in beacon");
		return;
	}
	if (!pe_session->ch_width)
		return;

	pAddBssParams->ch_width = QDF_MIN(he_op->oper_info_6g.info.ch_width,
					  pe_session->ch_width);

	if (pAddBssParams->ch_width == CH_WIDTH_160MHZ)
		pAddBssParams->ch_width = pe_session->ch_width;
	pAddBssParams->staContext.ch_width = pAddBssParams->ch_width;
}

void lim_update_stads_he_caps(struct mac_context *mac_ctx,
			      tpDphHashNode sta_ds, tpSirAssocRsp assoc_rsp,
			      struct pe_session *session_entry,
			      tSchBeaconStruct *beacon)
{
	/* If HE is not supported, do not fill sta_ds and return */
	if (!IS_DOT11_MODE_HE(session_entry->dot11mode))
		goto out;

	if (!assoc_rsp->he_cap.present && beacon && beacon->he_cap.present) {
		/* Use beacon HE caps if assoc resp doesn't have he caps */
		pe_debug("he_caps missing in assoc rsp");
		qdf_mem_copy(&assoc_rsp->he_cap, &beacon->he_cap,
			     sizeof(tDot11fIEhe_cap));
	}

	/* assoc resp and beacon doesn't have he caps */
	if (!assoc_rsp->he_cap.present)
		goto out;

	sta_ds->mlmStaContext.he_capable = assoc_rsp->he_cap.present;

	/* setting ldpc_coding if any of assoc_rsp or beacon has ldpc_coding
	 * enabled
	 */
	if (beacon)
		assoc_rsp->he_cap.ldpc_coding |= beacon->he_cap.ldpc_coding;
	lim_check_and_force_he_ldpc_cap(session_entry, &assoc_rsp->he_cap);
	if (beacon)
		beacon->he_cap.ldpc_coding = assoc_rsp->he_cap.ldpc_coding;

	qdf_mem_copy(&sta_ds->he_config, &assoc_rsp->he_cap,
		     sizeof(tDot11fIEhe_cap));

	/* If MCS 12/13 is supported by the assoc resp QCN IE */
	if (assoc_rsp->qcn_ie.present &&
	    assoc_rsp->qcn_ie.he_mcs13_attr.present) {
		sta_ds->he_mcs_12_13_map =
		assoc_rsp->qcn_ie.he_mcs13_attr.he_mcs_12_13_supp_80 |
		assoc_rsp->qcn_ie.he_mcs13_attr.he_mcs_12_13_supp_160 << 8;
	}

	/* Take intersection of the FW capability for HE MCS 12/13 */
	if (wlan_reg_is_24ghz_ch_freq(session_entry->curr_op_freq))
		sta_ds->he_mcs_12_13_map &=
			mac_ctx->mlme_cfg->he_caps.he_mcs_12_13_supp_2g;
	else
		sta_ds->he_mcs_12_13_map &=
			mac_ctx->mlme_cfg->he_caps.he_mcs_12_13_supp_5g;
out:
	pe_debug("he_mcs_12_13_map: sta_ds 0x%x, 2g_fw 0x%x, 5g_fw 0x%x",
		 sta_ds->he_mcs_12_13_map,
		 mac_ctx->mlme_cfg->he_caps.he_mcs_12_13_supp_2g,
		 mac_ctx->mlme_cfg->he_caps.he_mcs_12_13_supp_5g);
	lim_update_he_mcs_12_13_map(mac_ctx->psoc,
				    session_entry->smeSessionId,
				    sta_ds->he_mcs_12_13_map);

}

void lim_update_stads_he_6ghz_op(struct pe_session *session,
				 tpDphHashNode sta_ds)
{
	tDot11fIEhe_cap *peer_he = &sta_ds->he_config;
	enum phy_ch_width ch_width;

	if (!session->he_6ghz_band)
		return;

	if (!peer_he->present) {
		pe_debug("HE cap not present in peer");
		return;
	}

	if (peer_he->chan_width_3)
		ch_width = CH_WIDTH_80P80MHZ;
	else if (peer_he->chan_width_2)
		ch_width = CH_WIDTH_160MHZ;
	else if (peer_he->chan_width_1)
		ch_width = CH_WIDTH_80MHZ;
	else
		ch_width = CH_WIDTH_20MHZ;
	if (ch_width > session->ch_width)
		ch_width = session->ch_width;
	sta_ds->ch_width = ch_width;
}

void lim_update_usr_he_cap(struct mac_context *mac_ctx, struct pe_session *session)
{
	struct add_ie_params *add_ie = &session->add_ie_params;
	tDot11fIEhe_cap *he_cap = &session->he_config;
	struct he_cap_network_endian *he_cap_from_ie;
	uint8_t extracted_buff[DOT11F_IE_HE_CAP_MAX_LEN + 2];
	QDF_STATUS status;
	struct sir_vht_config *vht_cfg = &session->vht_config;
	struct mlme_legacy_priv *mlme_priv;

	qdf_mem_zero(extracted_buff, sizeof(extracted_buff));
	status = lim_strip_ie(mac_ctx, add_ie->probeRespBCNData_buff,
			&add_ie->probeRespBCNDataLen,
			DOT11F_EID_HE_CAP, ONE_BYTE,
			HE_CAP_OUI_TYPE, (uint8_t)HE_CAP_OUI_SIZE,
			extracted_buff, DOT11F_IE_HE_CAP_MAX_LEN);
	if (QDF_STATUS_SUCCESS != status) {
		pe_debug("Failed to strip HE cap IE status: %d", status);
		return;
	}

	pe_debug("Before update: su_beamformer: %d, su_beamformee: %d, mu_beamformer: %d",
		he_cap->su_beamformer, he_cap->su_beamformee, he_cap->mu_beamformer);

	he_cap_from_ie = (struct he_cap_network_endian *)
					&extracted_buff[HE_CAP_OUI_SIZE + 2];

	he_cap->su_beamformer =
		he_cap->su_beamformer & he_cap_from_ie->su_beamformer;
	he_cap->su_beamformee =
		he_cap->su_beamformee & he_cap_from_ie->su_beamformee;
	he_cap->mu_beamformer =
		he_cap->mu_beamformer & he_cap_from_ie->mu_beamformer;

	pe_debug("After update: su_beamformer: %d, su_beamformee: %d, mu_beamformer: %d",
		he_cap->su_beamformer, he_cap->su_beamformee, he_cap->mu_beamformer);
	if (!he_cap->su_beamformer) {
		he_cap->mu_beamformer = 0;
		he_cap->num_sounding_lt_80 = 0;
		he_cap->num_sounding_gt_80 = 0;
		vht_cfg->su_beam_former = 0;
		vht_cfg->mu_beam_former = 0;
		vht_cfg->num_soundingdim = 0;
	}
	if (!he_cap->su_beamformee) {
		he_cap->bfee_sts_lt_80 = 0;
		he_cap->bfee_sts_gt_80 = 0;
		vht_cfg->su_beam_formee = 0;
		vht_cfg->mu_beam_formee = 0;
		vht_cfg->csnof_beamformer_antSup = 0;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(session->vdev);
	if (mlme_priv) {
		mlme_priv->he_config.mu_beamformer = he_cap->mu_beamformer;
		mlme_priv->he_config.su_beamformer = he_cap->su_beamformer;
		mlme_priv->he_config.su_beamformee = he_cap->su_beamformee;
		mlme_priv->he_config.bfee_sts_lt_80 = he_cap->bfee_sts_lt_80;
		mlme_priv->he_config.bfee_sts_gt_80 = he_cap->bfee_sts_gt_80;
		mlme_priv->he_config.num_sounding_lt_80 =
						he_cap->num_sounding_lt_80;
		mlme_priv->he_config.num_sounding_gt_80 =
						he_cap->num_sounding_gt_80;
	}
	wma_set_he_txbf_params(session->vdev_id, he_cap->su_beamformer,
			       he_cap->su_beamformee, he_cap->mu_beamformer);
}

void lim_decide_he_op(struct mac_context *mac_ctx, uint32_t *mlme_he_ops,
		      struct pe_session *session)
{
	uint32_t val;
	uint8_t color;
	struct he_ops_network_endian *he_ops_from_ie;
	tDot11fIEhe_op he_ops = {0};
	struct add_ie_params *add_ie = &session->add_ie_params;
	uint8_t extracted_buff[DOT11F_IE_HE_OP_MAX_LEN + 2];
	QDF_STATUS status;

	qdf_mem_zero(extracted_buff, sizeof(extracted_buff));
	status = lim_strip_ie(mac_ctx, add_ie->probeRespBCNData_buff,
			&add_ie->probeRespBCNDataLen,
			DOT11F_EID_HE_OP, ONE_BYTE,
			HE_OP_OUI_TYPE, (uint8_t)HE_OP_OUI_SIZE,
			extracted_buff, DOT11F_IE_HE_OP_MAX_LEN);
	if (QDF_STATUS_SUCCESS != status) {
		pe_debug("Failed to strip HE OP IE status: %d", status);
		return;
	}
	he_ops_from_ie = (struct he_ops_network_endian *)
					&extracted_buff[HE_OP_OUI_SIZE + 2];

	if (he_ops_from_ie->bss_color) {
		he_ops.bss_color = he_ops_from_ie->bss_color;
	} else {
		qdf_get_random_bytes(&color, sizeof(color));
		/* make sure color is within 1-63*/
		he_ops.bss_color = (color % WNI_CFG_HE_OPS_BSS_COLOR_MAX) + 1;
	}
	he_ops.default_pe = he_ops_from_ie->default_pe;
	he_ops.twt_required = he_ops_from_ie->twt_required;
	he_ops.txop_rts_threshold = he_ops_from_ie->txop_rts_threshold;
	he_ops.partial_bss_col = he_ops_from_ie->partial_bss_col;
	he_ops.bss_col_disabled = he_ops_from_ie->bss_col_disabled;

	val = mac_ctx->mlme_cfg->he_caps.he_ops_basic_mcs_nss;

	*((uint16_t *)he_ops.basic_mcs_nss) = (uint16_t)val;

	qdf_mem_copy(&session->he_op, &he_ops, sizeof(tDot11fIEhe_op));

	pe_debug("HE Op: bss_color: 0x%0x, default_pe_duration: 0x%0x",
		he_ops.bss_color, he_ops.default_pe);
	pe_debug("He Op: twt_required: 0x%0x, txop_rts_threshold: 0x%0x",
		 he_ops.twt_required, he_ops.txop_rts_threshold);
	pe_debug("HE Op: partial_bss_color: 0x%0x",
		 he_ops.partial_bss_col);
	pe_debug("HE Op: BSS color disabled: 0x%0x",
		he_ops.bss_col_disabled);
	pe_debug("HE Op: Basic MCS NSS: 0x%04x",
		*((uint16_t *)he_ops.basic_mcs_nss));

	wma_update_vdev_he_ops(mlme_he_ops, &he_ops);
}

static void
lim_revise_req_he_cap_per_band(struct mlme_legacy_priv *mlme_priv,
			       struct pe_session *session)
{
	struct mac_context *mac = session->mac_ctx;
	tDot11fIEhe_cap *he_config;

	he_config = &mlme_priv->he_config;
	if (wlan_reg_is_24ghz_ch_freq(session->curr_op_freq)) {
		he_config->bfee_sts_lt_80 =
			mac->he_cap_2g.bfee_sts_lt_80;
	} else {
		he_config->bfee_sts_lt_80 =
			mac->he_cap_5g.bfee_sts_lt_80;

		he_config->num_sounding_lt_80 =
			mac->he_cap_5g.num_sounding_lt_80;
		if (he_config->chan_width_2 ||
		    he_config->chan_width_3) {
			he_config->bfee_sts_gt_80 =
				mac->he_cap_5g.bfee_sts_gt_80;
			he_config->num_sounding_gt_80 =
				mac->he_cap_5g.num_sounding_gt_80;
			he_config->he_ppdu_20_in_160_80p80Mhz =
				mac->he_cap_5g.he_ppdu_20_in_160_80p80Mhz;
			he_config->he_ppdu_80_in_160_80p80Mhz =
				mac->he_cap_5g.he_ppdu_80_in_160_80p80Mhz;
		}
	}
}

void lim_copy_bss_he_cap(struct pe_session *session)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(session->vdev);
	if (!mlme_priv)
		return;
	lim_revise_req_he_cap_per_band(mlme_priv, session);
	qdf_mem_copy(&(session->he_config), &(mlme_priv->he_config),
		     sizeof(session->he_config));
}

void lim_copy_join_req_he_cap(struct pe_session *session)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(session->vdev);
	if (!mlme_priv)
		return;
	if (!session->mac_ctx->usr_cfg_tx_bfee_nsts)
		lim_revise_req_he_cap_per_band(mlme_priv, session);
	qdf_mem_copy(&(session->he_config), &(mlme_priv->he_config),
		     sizeof(session->he_config));
}

void lim_log_he_cap(struct mac_context *mac, tDot11fIEhe_cap *he_cap)
{
	uint8_t chan_width;
	struct ppet_hdr *hdr;

	if (!he_cap->present)
		return;

	pe_nofl_debug("HE Capabilities: htc_he 0x%x twt_req 0x%x twt_res 0x%x fragmentation 0x%x max frag msdu amsdu 0x%x min frag 0x%x",
		      he_cap->htc_he, he_cap->twt_request,
		      he_cap->twt_responder, he_cap->fragmentation,
		      he_cap->max_num_frag_msdu_amsdu_exp,
		      he_cap->min_frag_size);
	pe_nofl_debug("\ttrig frm mac pad 0x%x multi tid aggr supp 0x%x link adaptaion 0x%x all ack 0x%x trigd_rsp_sched 0x%x a_bsr 0x%x",
		      he_cap->trigger_frm_mac_pad,
		      he_cap->multi_tid_aggr_rx_supp,
		      he_cap->he_link_adaptation, he_cap->all_ack,
		      he_cap->trigd_rsp_sched, he_cap->a_bsr);
	pe_nofl_debug("\tBC twt 0x%x ba_32bit_bitmap supp 0x%x mu_cascade 0x%x ack_enabled_multitid 0x%x omi_a_ctrl 0x%x ofdma_ra 0x%x",
		      he_cap->broadcast_twt, he_cap->ba_32bit_bitmap,
		      he_cap->mu_cascade, he_cap->ack_enabled_multitid,
		      he_cap->omi_a_ctrl, he_cap->ofdma_ra);
	pe_nofl_debug("\tmax_ampdu_len exp ext 0x%x amsdu_frag 0x%x flex_twt_sched 0x%x rx_ctrl frm 0x%x bsrp_ampdu_aggr 0x%x qtp 0x%x a_bqr 0x%x",
		      he_cap->max_ampdu_len_exp_ext, he_cap->amsdu_frag,
		      he_cap->flex_twt_sched, he_cap->rx_ctrl_frame,
		      he_cap->bsrp_ampdu_aggr, he_cap->qtp, he_cap->a_bqr);
	pe_nofl_debug("\tSR Reponder 0x%x ndp_feedback 0x%x ops_supp 0x%x amsdu_in_ampdu 0x%x multi_tid_aggr_tx 0x%x he_sub_ch_sel_tx 0x%x",
		      he_cap->spatial_reuse_param_rspder,
		      he_cap->ndp_feedback_supp,
		      he_cap->ops_supp, he_cap->amsdu_in_ampdu,
		      he_cap->multi_tid_aggr_tx_supp,
		      he_cap->he_sub_ch_sel_tx_supp);

	pe_nofl_debug("\tul_2x996_tone_ru 0x%x om_ctrl_ul_mu_data_dis_rx 0x%x dynamic_smps 0x%x punctured_sounding 0x%x ht_vht_trg_frm_rx 0x%x",
		      he_cap->ul_2x996_tone_ru_supp,
		      he_cap->om_ctrl_ul_mu_data_dis_rx,
		      he_cap->he_dynamic_smps, he_cap->punctured_sounding_supp,
		      he_cap->ht_vht_trg_frm_rx_supp);

	chan_width = HE_CH_WIDTH_COMBINE(he_cap->chan_width_0,
			he_cap->chan_width_1, he_cap->chan_width_2,
			he_cap->chan_width_3, he_cap->chan_width_4,
			he_cap->chan_width_5, he_cap->chan_width_6);

	pe_nofl_debug("\tchan width %d rx_pream_puncturing 0x%x device_class 0x%x ldpc_coding 0x%x 1x_ltf_800_gi_ppdu 0x%x midamble_tx_rx_max_nsts 0x%x",
		      chan_width, he_cap->rx_pream_puncturing,
		      he_cap->device_class,
		      he_cap->ldpc_coding, he_cap->he_1x_ltf_800_gi_ppdu,
		      he_cap->midamble_tx_rx_max_nsts);

	pe_nofl_debug("\t4x_ltf_3200_gi_ndp 0x%x tb_ppdu_tx_stbc_lt_80mhz 0x%x rx_stbc_lt_80mhz 0x%x doppler 0x%x ul_mu 0x%x dcm_enc_tx 0x%x dcm_enc_rx 0x%x",
		      he_cap->he_4x_ltf_3200_gi_ndp,
		      he_cap->tb_ppdu_tx_stbc_lt_80mhz,
		      he_cap->rx_stbc_lt_80mhz, he_cap->doppler, he_cap->ul_mu,
		      he_cap->dcm_enc_tx, he_cap->dcm_enc_rx);

	pe_nofl_debug("\tul_he_mu 0x%x su_bfer 0x%x su_fee 0x%x mu_bfer 0x%x bfee_sts_lt_80 0x%x bfee_sts_gt_80 0x%x num_sd_lt_80 0x%x num_sd_gt_80 0x%x",
		      he_cap->ul_he_mu, he_cap->su_beamformer,
		      he_cap->su_beamformee,
		      he_cap->mu_beamformer, he_cap->bfee_sts_lt_80,
		      he_cap->bfee_sts_gt_80, he_cap->num_sounding_lt_80,
		      he_cap->num_sounding_gt_80);

	pe_nofl_debug("\tsu_fb_tone16 0x%x mu_fb_tone16 0x%x codebook_su 0x%x codebook_mu 0x%x bforming_feedback 0x%x he_er_su_ppdu 0x%x dl_mu_mimo_part_bw 0x%x",
		      he_cap->su_feedback_tone16, he_cap->mu_feedback_tone16,
		      he_cap->codebook_su, he_cap->codebook_mu,
		      he_cap->beamforming_feedback, he_cap->he_er_su_ppdu,
		      he_cap->dl_mu_mimo_part_bw);

	pe_nofl_debug("\tppet_present 0x%x srp 0x%x power_boost 0x%x ltf_800_gi_4x 0x%x tb_ppdu_tx_stbc_gt_80mhz 0x%x rx_stbc_gt_80mhz 0x%x max_nc 0x%x",
		      he_cap->ppet_present, he_cap->srp,
		      he_cap->power_boost, he_cap->he_ltf_800_gi_4x,
		      he_cap->tb_ppdu_tx_stbc_gt_80mhz,
		      he_cap->rx_stbc_gt_80mhz, he_cap->max_nc);

	pe_nofl_debug("\ter_ltf_800_gi_4x 0x%x ppdu_20_in_40Mhz_2G 0x%x ppdu_20_in_160_80p80Mhz 0x%x ppdu_80_in_160_80p80Mhz 0x%x er_1x_ltf_gi 0x%x",
		      he_cap->er_he_ltf_800_gi_4x,
		      he_cap->he_ppdu_20_in_40Mhz_2G,
		      he_cap->he_ppdu_20_in_160_80p80Mhz,
		      he_cap->he_ppdu_80_in_160_80p80Mhz,
		      he_cap->er_1x_he_ltf_gi);

	pe_nofl_debug("\tmidamble_tx_rx_1x_ltf 0x%x dcm_max_bw 0x%x longer_than_16_he_sigb_ofdm_sym 0x%x non_trig_cqi_feedback 0x%x tx_1024_qam_lt_242_tone_ru 0x%x",
		      he_cap->midamble_tx_rx_1x_he_ltf, he_cap->dcm_max_bw,
		      he_cap->longer_than_16_he_sigb_ofdm_sym,
		      he_cap->non_trig_cqi_feedback,
		      he_cap->tx_1024_qam_lt_242_tone_ru);

	pe_nofl_debug("\trx_1024_qam_lt_242_tone_ru 0x%x rx_full_bw_su_he_mu_compress_sigb 0x%x rx_he_mcs_map_lt_80 0x%x tx_he_mcs_map_lt_80 0x%x",
		      he_cap->rx_1024_qam_lt_242_tone_ru,
		      he_cap->rx_full_bw_su_he_mu_compress_sigb,
		      he_cap->rx_he_mcs_map_lt_80,
		      he_cap->tx_he_mcs_map_lt_80);

	hdr = (struct ppet_hdr *)&he_cap->ppet;
	pe_nofl_debug("\tRx MCS map 160 Mhz: 0x%x Tx MCS map 160 Mhz: 0x%x Rx MCS map 80+80 Mhz: 0x%x Tx MCS map 80+80 Mhz: 0x%x ppe_th:: nss_count: %d, ru_idx_msk: %d",
		      *((uint16_t *)he_cap->rx_he_mcs_map_160),
		      *((uint16_t *)he_cap->tx_he_mcs_map_160),
		      *((uint16_t *)he_cap->rx_he_mcs_map_80_80),
		      *((uint16_t *)he_cap->tx_he_mcs_map_80_80),
		      hdr->nss, hdr->ru_idx_mask);

	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
		&he_cap->ppet, HE_MAX_PPET_SIZE);
}

void lim_log_he_op(struct mac_context *mac, tDot11fIEhe_op *he_ops,
		   struct pe_session *session)
{
	pe_debug("bss_color 0x%x, default_pe_dur 0x%x, twt_req  0x%x, txop_rts_thres  0x%x, vht_op 0x%x",
		 he_ops->bss_color, he_ops->default_pe,
		 he_ops->twt_required, he_ops->txop_rts_threshold,
		 he_ops->vht_oper_present);
	pe_debug("\tpart_bss_color 0x%x, Co-located BSS 0x%x, BSS color dis 0x%x basic mcs nss: 0x%x",
		 he_ops->partial_bss_col, he_ops->co_located_bss,
		 he_ops->bss_col_disabled,
		 *((uint16_t *)he_ops->basic_mcs_nss));

	if (!session->he_6ghz_band) {
		if (!he_ops->vht_oper_present)
			pe_debug("VHT Info not present in HE Operation");
		else
			pe_debug("VHT Info: ch_bw %d cntr_freq0 %d cntr_freq1 %d",
				 he_ops->vht_oper.info.chan_width,
				 he_ops->vht_oper.info.center_freq_seg0,
				 he_ops->vht_oper.info.center_freq_seg1);
	} else {
		if (!he_ops->oper_info_6g_present)
			pe_debug("6G op_info not present in HE Operation");
		else
			pe_debug("6G_op_info:ch_bw %d cntr_freq0 %d cntr_freq1 %d dup_bcon %d, min_rate %d",
				 he_ops->oper_info_6g.info.ch_width,
				 he_ops->oper_info_6g.info.center_freq_seg0,
				 he_ops->oper_info_6g.info.center_freq_seg1,
				 he_ops->oper_info_6g.info.dup_bcon,
				 he_ops->oper_info_6g.info.min_rate);
	}
}

void lim_log_he_6g_cap(struct mac_context *mac,
		       tDot11fIEhe_6ghz_band_cap *he_6g_cap)
{
	pe_debug("min_mpdu_space: %0d, max_mpdu_len_exp: %0x, max_mpdu_len %0x, smps %0x, rd %0x rx_ant_ptn %d tx_ant_ptn %d",
		 he_6g_cap->min_mpdu_start_spacing,
		 he_6g_cap->max_ampdu_len_exp, he_6g_cap->max_mpdu_len,
		 he_6g_cap->sm_pow_save, he_6g_cap->rd_responder,
		 he_6g_cap->rx_ant_pattern_consistency,
		 he_6g_cap->tx_ant_pattern_consistency);
}

#ifdef WLAN_FEATURE_11AX_BSS_COLOR
void lim_log_he_bss_color(struct mac_context *mac,
				tDot11fIEbss_color_change *he_bss_color)
{
	pe_debug("countdown: %d, new_color: %d",
			he_bss_color->countdown, he_bss_color->new_color);
}
#endif

void lim_update_sta_he_capable(struct mac_context *mac,
	tpAddStaParams add_sta_params, tpDphHashNode sta_ds,
	struct pe_session *session_entry)
{
	if (LIM_IS_AP_ROLE(session_entry))
		add_sta_params->he_capable = sta_ds->mlmStaContext.he_capable &&
						session_entry->he_capable;
#ifdef FEATURE_WLAN_TDLS
	else if (STA_ENTRY_TDLS_PEER == sta_ds->staType)
		add_sta_params->he_capable = sta_ds->mlmStaContext.he_capable;
#endif
	else
		add_sta_params->he_capable = session_entry->he_capable;

	add_sta_params->he_mcs_12_13_map = sta_ds->he_mcs_12_13_map;
	pe_debug("he_capable: %d", add_sta_params->he_capable);
}

void lim_update_bss_he_capable(struct mac_context *mac,
			       struct bss_params *add_bss)
{
	add_bss->he_capable = true;
	pe_debug("he_capable: %d", add_bss->he_capable);
}

void lim_update_stads_he_capable(tpDphHashNode sta_ds, tpSirAssocReq assoc_req)
{
	sta_ds->mlmStaContext.he_capable = assoc_req->he_cap.present;
}

void lim_update_session_he_capable(struct mac_context *mac, struct pe_session *session)
{
	session->he_capable = true;
	if (wlan_reg_is_6ghz_chan_freq(session->curr_op_freq)) {
		session->htCapability = 0;
		session->vhtCapability = 0;
		session->he_6ghz_band = 1;
	}
	if (wlan_reg_is_24ghz_ch_freq(session->curr_op_freq) &&
	    !mac->mlme_cfg->vht_caps.vht_cap_info.b24ghz_band)
		session->vhtCapability = 0;

	pe_debug("he_capable: %d", session->he_capable);
}

void lim_update_session_he_capable_chan_switch(struct mac_context *mac,
					       struct pe_session *session,
					       uint32_t new_chan_freq)
{
	session->he_capable = true;
	if (wlan_reg_is_6ghz_chan_freq(session->curr_op_freq) &&
	    !wlan_reg_is_6ghz_chan_freq(new_chan_freq)) {
		session->htCapability = 1;
		session->vhtCapability = 1;
		session->he_6ghz_band = 0;
	} else if (!wlan_reg_is_6ghz_chan_freq(session->curr_op_freq) &&
		   wlan_reg_is_6ghz_chan_freq(new_chan_freq)) {
		session->htCapability = 0;
		session->vhtCapability = 0;
		session->he_6ghz_band = 1;
	}

	/*
	 * If new channel is 2.4gh set VHT as per the b24ghz_band INI
	 * if new channel is 5Ghz set the vht, this will happen if we move from
	 * 2.4Ghz to 5Ghz.
	 */
	if (wlan_reg_is_24ghz_ch_freq(new_chan_freq) &&
	    !mac->mlme_cfg->vht_caps.vht_cap_info.b24ghz_band)
		session->vhtCapability = 0;
	else if (wlan_reg_is_5ghz_ch_freq(new_chan_freq))
		session->vhtCapability = 1;
	/*
	 * Re-initialize color bss parameters during channel change
	 */

	session->he_op.bss_col_disabled = 1;
	session->bss_color_changing = 1;
	session->he_bss_color_change.new_color = session->he_op.bss_color;
	session->he_bss_color_change.countdown = BSS_COLOR_SWITCH_COUNTDOWN;
	pe_debug("he_capable: %d ht %d vht %d 6ghz_band %d new freq %d vht in 2.4gh %d",
		 session->he_capable, session->htCapability,
		 session->vhtCapability, session->he_6ghz_band, new_chan_freq,
		 mac->mlme_cfg->vht_caps.vht_cap_info.b24ghz_band);
}

void lim_set_he_caps(struct mac_context *mac, struct pe_session *session,
		     uint8_t *ie_start, uint32_t num_bytes)
{
	const uint8_t *ie = NULL;
	tDot11fIEhe_cap dot11_cap;
	struct he_capability_info *he_cap;

	populate_dot11f_he_caps(mac, session, &dot11_cap);
	lim_log_he_cap(mac, &dot11_cap);
	ie = wlan_get_ext_ie_ptr_from_ext_id(HE_CAP_OUI_TYPE,
			HE_CAP_OUI_SIZE, ie_start, num_bytes);
	if (ie) {
		/* convert from unpacked to packed structure */
		he_cap = (struct he_capability_info *) &ie[2 + HE_CAP_OUI_SIZE];

		he_cap->htc_he = dot11_cap.htc_he;
		he_cap->twt_request = dot11_cap.twt_request;
		he_cap->twt_responder = dot11_cap.twt_responder;
		he_cap->fragmentation = dot11_cap.fragmentation;
		he_cap->max_num_frag_msdu_amsdu_exp =
			dot11_cap.max_num_frag_msdu_amsdu_exp;
		he_cap->min_frag_size = dot11_cap.min_frag_size;
		he_cap->trigger_frm_mac_pad = dot11_cap.trigger_frm_mac_pad;
		he_cap->multi_tid_aggr_rx_supp =
			dot11_cap.multi_tid_aggr_rx_supp;
		he_cap->he_link_adaptation = dot11_cap.he_link_adaptation;
		he_cap->all_ack = dot11_cap.all_ack;
		he_cap->trigd_rsp_sched = dot11_cap.trigd_rsp_sched;
		he_cap->a_bsr = dot11_cap.a_bsr;
		he_cap->broadcast_twt = dot11_cap.broadcast_twt;
		he_cap->ba_32bit_bitmap = dot11_cap.ba_32bit_bitmap;
		he_cap->mu_cascade = dot11_cap.mu_cascade;
		he_cap->ack_enabled_multitid = dot11_cap.ack_enabled_multitid;
		he_cap->omi_a_ctrl = dot11_cap.omi_a_ctrl;
		he_cap->ofdma_ra = dot11_cap.ofdma_ra;
		he_cap->max_ampdu_len_exp_ext = dot11_cap.max_ampdu_len_exp_ext;
		he_cap->amsdu_frag = dot11_cap.amsdu_frag;
		he_cap->flex_twt_sched = dot11_cap.flex_twt_sched;
		he_cap->rx_ctrl_frame = dot11_cap.rx_ctrl_frame;

		he_cap->bsrp_ampdu_aggr = dot11_cap.bsrp_ampdu_aggr;
		he_cap->qtp = dot11_cap.qtp;
		he_cap->a_bqr = dot11_cap.a_bqr;
		he_cap->spatial_reuse_param_rspder =
			dot11_cap.spatial_reuse_param_rspder;
		he_cap->ops_supp = dot11_cap.ops_supp;
		he_cap->ndp_feedback_supp = dot11_cap.ndp_feedback_supp;
		he_cap->amsdu_in_ampdu = dot11_cap.amsdu_in_ampdu;

		he_cap->chan_width = HE_CH_WIDTH_COMBINE(dot11_cap.chan_width_0,
				dot11_cap.chan_width_1, dot11_cap.chan_width_2,
				dot11_cap.chan_width_3, dot11_cap.chan_width_4,
				dot11_cap.chan_width_5, dot11_cap.chan_width_6);

		he_cap->rx_pream_puncturing = dot11_cap.rx_pream_puncturing;
		he_cap->device_class = dot11_cap.device_class;
		he_cap->ldpc_coding = dot11_cap.ldpc_coding;
		he_cap->he_1x_ltf_800_gi_ppdu = dot11_cap.he_1x_ltf_800_gi_ppdu;
		he_cap->midamble_tx_rx_max_nsts =
			dot11_cap.midamble_tx_rx_max_nsts;
		he_cap->he_4x_ltf_3200_gi_ndp = dot11_cap.he_4x_ltf_3200_gi_ndp;
		he_cap->tb_ppdu_tx_stbc_lt_80mhz =
			dot11_cap.tb_ppdu_tx_stbc_lt_80mhz;
		he_cap->rx_stbc_lt_80mhz = dot11_cap.rx_stbc_lt_80mhz;
		he_cap->tb_ppdu_tx_stbc_gt_80mhz =
			dot11_cap.tb_ppdu_tx_stbc_gt_80mhz;
		he_cap->rx_stbc_gt_80mhz = dot11_cap.rx_stbc_gt_80mhz;
		he_cap->doppler = dot11_cap.doppler;
		he_cap->ul_mu = dot11_cap.ul_mu;
		he_cap->dcm_enc_tx = dot11_cap.dcm_enc_tx;
		he_cap->dcm_enc_rx = dot11_cap.dcm_enc_rx;
		he_cap->ul_he_mu = dot11_cap.ul_he_mu;
		he_cap->su_beamformer = dot11_cap.su_beamformer;

		he_cap->su_beamformee = dot11_cap.su_beamformee;
		he_cap->mu_beamformer = dot11_cap.mu_beamformer;
		he_cap->bfee_sts_lt_80 = dot11_cap.bfee_sts_lt_80;
		he_cap->bfee_sts_gt_80 = dot11_cap.bfee_sts_gt_80;
		he_cap->num_sounding_lt_80 = dot11_cap.num_sounding_lt_80;
		he_cap->num_sounding_gt_80 = dot11_cap.num_sounding_gt_80;
		he_cap->su_feedback_tone16 = dot11_cap.su_feedback_tone16;
		he_cap->mu_feedback_tone16 = dot11_cap.mu_feedback_tone16;
		he_cap->codebook_su = dot11_cap.codebook_su;
		he_cap->codebook_mu = dot11_cap.codebook_mu;
		he_cap->beamforming_feedback = dot11_cap.beamforming_feedback;
		he_cap->he_er_su_ppdu = dot11_cap.he_er_su_ppdu;
		he_cap->dl_mu_mimo_part_bw = dot11_cap.dl_mu_mimo_part_bw;
		he_cap->ppet_present = dot11_cap.ppet_present;
		he_cap->srp = dot11_cap.srp;
		he_cap->power_boost = dot11_cap.power_boost;

		he_cap->tx_1024_qam_lt_242_tone_ru =
			dot11_cap.tx_1024_qam_lt_242_tone_ru;
		he_cap->rx_1024_qam_lt_242_tone_ru =
			dot11_cap.rx_1024_qam_lt_242_tone_ru;

		he_cap->he_ltf_800_gi_4x = dot11_cap.he_ltf_800_gi_4x;
		he_cap->max_nc = dot11_cap.max_nc;
		he_cap->er_he_ltf_800_gi_4x = dot11_cap.er_he_ltf_800_gi_4x;
		he_cap->he_ppdu_20_in_40Mhz_2G =
					dot11_cap.he_ppdu_20_in_40Mhz_2G;
		he_cap->he_ppdu_20_in_160_80p80Mhz =
					dot11_cap.he_ppdu_20_in_160_80p80Mhz;
		he_cap->he_ppdu_80_in_160_80p80Mhz =
					dot11_cap.he_ppdu_80_in_160_80p80Mhz;
		he_cap->er_1x_he_ltf_gi =
					dot11_cap.er_1x_he_ltf_gi;
		he_cap->midamble_tx_rx_1x_he_ltf =
					dot11_cap.midamble_tx_rx_1x_he_ltf;
		he_cap->reserved2 = dot11_cap.reserved2;

		he_cap->rx_he_mcs_map_lt_80 = dot11_cap.rx_he_mcs_map_lt_80;
		he_cap->tx_he_mcs_map_lt_80 = dot11_cap.tx_he_mcs_map_lt_80;
		if (dot11_cap.chan_width_2) {
			he_cap->rx_he_mcs_map_160 =
				*((uint16_t *)dot11_cap.rx_he_mcs_map_160);
			he_cap->tx_he_mcs_map_160 =
				*((uint16_t *)dot11_cap.tx_he_mcs_map_160);
			ie_start[1] += HE_CAP_160M_MCS_MAP_LEN;
		}
		if (dot11_cap.chan_width_3) {
			he_cap->rx_he_mcs_map_80_80 =
				*((uint16_t *)dot11_cap.rx_he_mcs_map_80_80);
			he_cap->tx_he_mcs_map_80_80 =
				*((uint16_t *)dot11_cap.tx_he_mcs_map_80_80);
			ie_start[1] += HE_CAP_80P80_MCS_MAP_LEN;
		}
	}
}

static void lim_intersect_he_ch_width_2g(struct mac_context *mac,
					 struct he_capability_info *he_cap)
{
	struct wlan_objmgr_psoc *psoc;
	uint32_t cbm_24ghz;
	QDF_STATUS ret;

	psoc = mac->psoc;
	if (!psoc)
		return;

	ret = ucfg_mlme_get_channel_bonding_24ghz(psoc, &cbm_24ghz);
	if (QDF_IS_STATUS_ERROR(ret))
		return;

	pe_debug("channel bonding mode 2.4GHz %d", cbm_24ghz);

	if (!cbm_24ghz) {
		/* B0: 40Mhz channel width in the 2.4GHz band */
		he_cap->chan_width = HE_CH_WIDTH_CLR_BIT(he_cap->chan_width, 0);
		he_cap->he_ppdu_20_in_40Mhz_2G = 0;
	}

	pe_debug("HE cap: chan_width: 0x%07x he_ppdu_20_in_40Mhz_2G %d",
		 he_cap->chan_width, he_cap->he_ppdu_20_in_40Mhz_2G);
}

static uint8_t lim_set_he_caps_ppet(struct mac_context *mac, uint8_t *ie,
				    enum cds_band_type band)
{
	uint8_t ppe_th[WNI_CFG_HE_PPET_LEN] = {0};
	/* Append at the end after ID + LEN + OUI + IE_Data */
	uint8_t offset = ie[1] + 1 + 1 + 1;
	uint8_t num_ppe_th;

	if (band == CDS_BAND_2GHZ)
		qdf_mem_copy(ppe_th, mac->mlme_cfg->he_caps.he_ppet_2g,
			     WNI_CFG_HE_PPET_LEN);
	else if (band == CDS_BAND_5GHZ)
		qdf_mem_copy(ppe_th, mac->mlme_cfg->he_caps.he_ppet_5g,
			     WNI_CFG_HE_PPET_LEN);
	else
		return 0;

	num_ppe_th = lim_truncate_ppet(ppe_th, WNI_CFG_HE_PPET_LEN);

	qdf_mem_copy(ie + offset, ppe_th, num_ppe_th);

	return num_ppe_th;
}

QDF_STATUS lim_send_he_caps_ie(struct mac_context *mac_ctx,
			       struct pe_session *session,
			       enum QDF_OPMODE device_mode,
			       uint8_t vdev_id)
{
	uint8_t he_caps[SIR_MAC_HE_CAP_MIN_LEN + HE_CAP_OUI_LEN +
			HE_CAP_160M_MCS_MAP_LEN + HE_CAP_80P80_MCS_MAP_LEN +
			WNI_CFG_HE_PPET_LEN];
	struct he_capability_info *he_cap;
	QDF_STATUS status_5g, status_2g;
	uint8_t he_cap_total_len = SIR_MAC_HE_CAP_MIN_LEN + HE_CAP_OUI_LEN +
				   HE_CAP_160M_MCS_MAP_LEN +
				   HE_CAP_80P80_MCS_MAP_LEN;
	uint8_t num_ppe_th = 0;
	bool nan_beamforming_supported;
	bool disable_nan_tx_bf = false;

	/* Sending only minimal info(no PPET) to FW now, update if required */
	qdf_mem_zero(he_caps, he_cap_total_len);
	he_caps[0] = DOT11F_EID_HE_CAP;
	he_caps[1] = SIR_MAC_HE_CAP_MIN_LEN;
	qdf_mem_copy(&he_caps[2], HE_CAP_OUI_TYPE, HE_CAP_OUI_SIZE);
	lim_set_he_caps(mac_ctx, session, he_caps, he_cap_total_len);
	he_cap = (struct he_capability_info *) (&he_caps[2 + HE_CAP_OUI_SIZE]);

	nan_beamforming_supported =
		ucfg_nan_is_beamforming_supported(mac_ctx->psoc);
	if (device_mode == QDF_NDI_MODE && !nan_beamforming_supported) {
		he_cap->su_beamformee = 0;
		he_cap->su_beamformer = 0;
		he_cap->mu_beamformer = 0;
		he_cap->bfee_sts_gt_80 = 0;
		he_cap->bfee_sts_lt_80 = 0;
		he_cap->num_sounding_gt_80 = 0;
		he_cap->num_sounding_lt_80 = 0;
		he_cap->su_feedback_tone16 = 0;
		he_cap->mu_feedback_tone16 = 0;
		disable_nan_tx_bf = true;
	}

	/*
	 * For 5G band HE cap, set the beamformee STS <= 80Mhz to
	 * mac->he_cap_5g.bfee_sts_lt_80 to keep the values same
	 * as initial connection
	 */
	if (!disable_nan_tx_bf) {
		he_cap->bfee_sts_lt_80 = mac_ctx->he_cap_5g.bfee_sts_lt_80;
		he_cap->bfee_sts_gt_80 = mac_ctx->he_cap_5g.bfee_sts_gt_80;
		he_cap->num_sounding_gt_80 =
					mac_ctx->he_cap_5g.num_sounding_gt_80;
		pe_debug("he_cap_5g: bfee_sts_gt_80 %d num_sounding_gt_80 %d",
			 he_cap->bfee_sts_gt_80, he_cap->num_sounding_gt_80);
	}

	if (he_cap->ppet_present)
		num_ppe_th = lim_set_he_caps_ppet(mac_ctx, he_caps,
						  CDS_BAND_5GHZ);

	status_5g = lim_send_ie(mac_ctx, vdev_id, DOT11F_EID_HE_CAP,
			CDS_BAND_5GHZ, &he_caps[2],
			he_caps[1] + 1 + num_ppe_th);
	if (QDF_IS_STATUS_ERROR(status_5g))
		pe_err("Unable send HE Cap IE for 5GHZ band, status: %d",
			status_5g);

	/*
	 * For 5G band HE cap, set the beamformee STS <= 80Mhz to
	 * mac->he_cap_5g.bfee_sts_lt_80 to keep the values same
	 * as initial connection
	 */
	if (!disable_nan_tx_bf) {
		he_cap->bfee_sts_lt_80 = mac_ctx->he_cap_2g.bfee_sts_lt_80;
		he_cap->bfee_sts_gt_80 = mac_ctx->he_cap_2g.bfee_sts_gt_80;
		he_cap->num_sounding_gt_80 =
					mac_ctx->he_cap_2g.num_sounding_gt_80;
		pe_debug("he_cap_2g: bfee_sts_gt_80 %d num_sounding_gt_80 %d",
			 he_cap->bfee_sts_gt_80, he_cap->num_sounding_gt_80);
	}

	lim_intersect_he_ch_width_2g(mac_ctx, he_cap);

	if (he_cap->ppet_present)
		num_ppe_th = lim_set_he_caps_ppet(mac_ctx, he_caps,
						  CDS_BAND_2GHZ);

	status_2g = lim_send_ie(mac_ctx, vdev_id, DOT11F_EID_HE_CAP,
			CDS_BAND_2GHZ, &he_caps[2],
			he_caps[1] + 1 + num_ppe_th);
	if (QDF_IS_STATUS_ERROR(status_2g))
		pe_err("Unable send HE Cap IE for 2GHZ band, status: %d",
			status_2g);

	if (QDF_IS_STATUS_SUCCESS(status_2g) &&
		QDF_IS_STATUS_SUCCESS(status_5g))
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}

/**
 * lim_populate_he_mcs_per_bw() - pouldate HE mcs set per BW (le 80, 160, 80+80)
 * @mac_ctx: Global MAC context
 * @self_rx: self rx mcs set
 * @self_tx: self tx mcs set
 * @peer_rx: peer rx mcs set
 * @peer_tx: peer tx mcs set
 * @nss: nss
 * @cfg_rx_param: rx wni param to read
 * @cfg_tx_param: tx wni param to read
 *
 * MCS values are interpreted as in IEEE 11ax-D1.4 spec onwards
 * +-----------------------------------------------------+
 * |  SS8  |  SS7  |  SS6  | SS5 | SS4 | SS3 | SS2 | SS1 |
 * +-----------------------------------------------------+
 * | 15-14 | 13-12 | 11-10 | 9-8 | 7-6 | 5-4 | 3-2 | 1-0 |
 * +-----------------------------------------------------+
 *
 * Return: status of operation
 */
static QDF_STATUS lim_populate_he_mcs_per_bw(struct mac_context *mac_ctx,
				uint16_t *supp_rx_mcs, uint16_t *supp_tx_mcs,
				uint16_t peer_rx, uint16_t peer_tx, uint8_t nss,
				uint16_t rx_mcs, uint16_t tx_mcs)
{

	pe_debug("peer rates: rx_mcs - 0x%04x tx_mcs - 0x%04x",
		 peer_rx, peer_tx);

	*supp_rx_mcs = rx_mcs;
	*supp_tx_mcs = tx_mcs;

	*supp_tx_mcs = HE_INTERSECT_MCS(*supp_rx_mcs, peer_tx);
	*supp_rx_mcs = HE_INTERSECT_MCS(*supp_tx_mcs, peer_rx);

	if (nss == NSS_1x1_MODE) {
		*supp_rx_mcs |= HE_MCS_INV_MSK_4_NSS(1);
		*supp_tx_mcs |= HE_MCS_INV_MSK_4_NSS(1);
	}
	/* if nss is 2, disable higher NSS */
	if (nss == NSS_2x2_MODE) {
		*supp_rx_mcs |= (HE_MCS_INV_MSK_4_NSS(1) &
				 HE_MCS_INV_MSK_4_NSS(2));
		*supp_tx_mcs |= (HE_MCS_INV_MSK_4_NSS(1) &
				 HE_MCS_INV_MSK_4_NSS(2));
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS lim_populate_he_mcs_set(struct mac_context *mac_ctx,
				   struct supported_rates *rates,
				   tDot11fIEhe_cap *peer_he_caps,
				   struct pe_session *session_entry, uint8_t nss)
{
	bool support_2x2 = false;
	uint32_t self_sta_dot11mode = mac_ctx->mlme_cfg->dot11_mode.dot11_mode;

	if (!IS_DOT11_MODE_HE(self_sta_dot11mode))
		return QDF_STATUS_SUCCESS;

	if ((!peer_he_caps) || (!peer_he_caps->present)) {
		pe_debug("peer not he capable or he_caps NULL");
		return QDF_STATUS_SUCCESS;
	}

	if (!session_entry) {
		pe_err("session is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	pe_debug("PEER: lt 80: rx 0x%04x tx 0x%04x, 160: rx 0x%04x tx 0x%04x, 80+80: rx 0x%04x tx 0x%04x",
		peer_he_caps->rx_he_mcs_map_lt_80,
		peer_he_caps->tx_he_mcs_map_lt_80,
		(*(uint16_t *)peer_he_caps->rx_he_mcs_map_160),
		(*(uint16_t *)peer_he_caps->tx_he_mcs_map_160),
		(*(uint16_t *)peer_he_caps->rx_he_mcs_map_80_80),
		(*(uint16_t *)peer_he_caps->tx_he_mcs_map_80_80));

	if (nss == NSS_2x2_MODE) {
		if (mac_ctx->mlme_cfg->gen.as_enabled &&
		    wlan_reg_is_24ghz_ch_freq(session_entry->curr_op_freq)) {
			if (IS_2X2_CHAIN(session_entry->chainMask))
				support_2x2 = true;
			else
				pe_err("2x2 not enabled %d",
					session_entry->chainMask);
		} else {
			support_2x2 = true;
		}
	}

	lim_populate_he_mcs_per_bw(mac_ctx,
		&rates->rx_he_mcs_map_lt_80, &rates->tx_he_mcs_map_lt_80,
		peer_he_caps->rx_he_mcs_map_lt_80,
		peer_he_caps->tx_he_mcs_map_lt_80, nss,
		mac_ctx->mlme_cfg->he_caps.dot11_he_cap.rx_he_mcs_map_lt_80,
		mac_ctx->mlme_cfg->he_caps.dot11_he_cap.tx_he_mcs_map_lt_80);

	if (session_entry->ch_width == CH_WIDTH_160MHZ) {
		lim_populate_he_mcs_per_bw(
			mac_ctx, &rates->rx_he_mcs_map_160,
			&rates->tx_he_mcs_map_160,
			*((uint16_t *)peer_he_caps->rx_he_mcs_map_160),
			*((uint16_t *)peer_he_caps->tx_he_mcs_map_160),
			nss,
			*((uint16_t *)mac_ctx->mlme_cfg->he_caps.dot11_he_cap.
				rx_he_mcs_map_160),
			*((uint16_t *)mac_ctx->mlme_cfg->he_caps.dot11_he_cap.
					tx_he_mcs_map_160));
	} else {
		rates->tx_he_mcs_map_160 = HE_MCS_ALL_DISABLED;
		rates->rx_he_mcs_map_160 = HE_MCS_ALL_DISABLED;
	}
	if (session_entry->ch_width == CH_WIDTH_80P80MHZ) {
		lim_populate_he_mcs_per_bw(
			mac_ctx, &rates->rx_he_mcs_map_80_80,
			&rates->tx_he_mcs_map_80_80,
			*((uint16_t *)peer_he_caps->rx_he_mcs_map_80_80),
			*((uint16_t *)peer_he_caps->tx_he_mcs_map_80_80), nss,
			*((uint16_t *)mac_ctx->mlme_cfg->he_caps.dot11_he_cap.
					rx_he_mcs_map_80_80),
			*((uint16_t *)mac_ctx->mlme_cfg->he_caps.dot11_he_cap.
					tx_he_mcs_map_80_80));
	} else {
		rates->tx_he_mcs_map_80_80 = HE_MCS_ALL_DISABLED;
		rates->rx_he_mcs_map_80_80 = HE_MCS_ALL_DISABLED;
	}
	if (!support_2x2) {
		/* disable 2 and higher NSS MCS sets */
		rates->rx_he_mcs_map_lt_80 |= HE_MCS_INV_MSK_4_NSS(1);
		rates->tx_he_mcs_map_lt_80 |= HE_MCS_INV_MSK_4_NSS(1);
		rates->rx_he_mcs_map_160 |= HE_MCS_INV_MSK_4_NSS(1);
		rates->tx_he_mcs_map_160 |= HE_MCS_INV_MSK_4_NSS(1);
		rates->rx_he_mcs_map_80_80 |= HE_MCS_INV_MSK_4_NSS(1);
		rates->tx_he_mcs_map_80_80 |= HE_MCS_INV_MSK_4_NSS(1);
	}

	pe_debug("lt 80: rx 0x%x tx 0x%x, 160: rx 0x%x tx 0x%x, 80_80: rx 0x%x tx 0x%x",
		 rates->rx_he_mcs_map_lt_80, rates->tx_he_mcs_map_lt_80,
		 rates->rx_he_mcs_map_160, rates->tx_he_mcs_map_160,
		 rates->rx_he_mcs_map_80_80, rates->tx_he_mcs_map_80_80);

	return QDF_STATUS_SUCCESS;
}
#endif

#if defined(CONFIG_BAND_6GHZ) && defined(WLAN_FEATURE_11AX)
QDF_STATUS lim_send_he_6g_band_caps_ie(struct mac_context *mac_ctx,
				       struct pe_session *session,
				       uint8_t vdev_id)
{
	uint8_t he_6g_band_caps_ie[DOT11F_IE_HE_6GHZ_BAND_CAP_MIN_LEN + 3];
	tDot11fIEhe_6ghz_band_cap he_6g_band_cap;
	QDF_STATUS status;
	uint32_t size = 0;
	uint32_t result;

	qdf_mem_zero(&he_6g_band_cap, sizeof(he_6g_band_cap));
	populate_dot11f_he_6ghz_cap(mac_ctx, session, &he_6g_band_cap);
	if (!he_6g_band_cap.present) {
		pe_debug("no HE 6g band cap for vdev %d", vdev_id);
		return QDF_STATUS_SUCCESS;
	}

	qdf_mem_zero(he_6g_band_caps_ie, sizeof(he_6g_band_caps_ie));
	result = dot11f_pack_ie_he_6ghz_band_cap(mac_ctx, &he_6g_band_cap,
						 he_6g_band_caps_ie,
						 sizeof(he_6g_band_caps_ie),
						 &size);
	if (result != DOT11F_PARSE_SUCCESS) {
		pe_err("pack erro for HE 6g band cap for vdev %d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}
	pe_debug("send HE 6ghz band cap: 0x%01x 0x%01x for vdev %d",
		 he_6g_band_caps_ie[3], he_6g_band_caps_ie[4],
		 vdev_id);
	status = lim_send_ie(mac_ctx, vdev_id, DOT11F_EID_HE_6GHZ_BAND_CAP,
			     CDS_BAND_5GHZ, &he_6g_band_caps_ie[2],
			     DOT11F_IE_HE_6GHZ_BAND_CAP_MIN_LEN + 1);
	if (QDF_IS_STATUS_ERROR(status))
		pe_err("Unable send HE 6g band Cap IE for 5GHZ band, status: %d",
		       status);

	return status;
}
#endif

int
lim_assoc_rej_get_remaining_delta(struct sir_rssi_disallow_lst *node)
{
	qdf_time_t cur_time;
	uint32_t time_diff;

	cur_time = qdf_do_div(qdf_get_monotonic_boottime(),
				QDF_MC_TIMER_TO_MS_UNIT);
	time_diff = cur_time - node->time_during_rejection;

	return node->retry_delay - time_diff;
}

QDF_STATUS
lim_rem_blacklist_entry_with_lowest_delta(qdf_list_t *list)
{
	struct sir_rssi_disallow_lst *oldest_node = NULL;
	struct sir_rssi_disallow_lst *cur_node;
	qdf_list_node_t *cur_list = NULL;
	qdf_list_node_t *next_list = NULL;

	qdf_list_peek_front(list, &cur_list);
	while (cur_list) {
		cur_node = qdf_container_of(cur_list,
			struct sir_rssi_disallow_lst, node);
		if (!oldest_node ||
		   (lim_assoc_rej_get_remaining_delta(oldest_node) >
		    lim_assoc_rej_get_remaining_delta(cur_node)))
			oldest_node = cur_node;

		qdf_list_peek_next(list, cur_list, &next_list);
		cur_list = next_list;
		next_list = NULL;
	}

	if (oldest_node) {
		pe_debug("remove node "QDF_MAC_ADDR_FMT" with lowest delta %d",
			QDF_MAC_ADDR_REF(oldest_node->bssid.bytes),
			lim_assoc_rej_get_remaining_delta(oldest_node));
		qdf_list_remove_node(list, &oldest_node->node);
		qdf_mem_free(oldest_node);
		return QDF_STATUS_SUCCESS;
	}

	return QDF_STATUS_E_INVAL;
}

void
lim_add_bssid_to_reject_list(struct wlan_objmgr_pdev *pdev,
			     struct sir_rssi_disallow_lst *entry)
{
	struct reject_ap_info ap_info;

	ap_info.bssid = entry->bssid;
	ap_info.reject_ap_type = DRIVER_RSSI_REJECT_TYPE;
	ap_info.rssi_reject_params.expected_rssi = entry->expected_rssi;
	ap_info.rssi_reject_params.retry_delay = entry->retry_delay;
	ap_info.reject_reason = entry->reject_reason;
	ap_info.source = entry->source;
	ap_info.rssi_reject_params.received_time = entry->received_time;
	ap_info.rssi_reject_params.original_timeout = entry->original_timeout;
	/* Add this ap info to the rssi reject ap type in blacklist manager */
	wlan_blm_add_bssid_to_reject_list(pdev, &ap_info);
}

void lim_decrement_pending_mgmt_count(struct mac_context *mac_ctx)
{
	qdf_spin_lock(&mac_ctx->sys.bbt_mgmt_lock);
	if (!mac_ctx->sys.sys_bbt_pending_mgmt_count) {
		qdf_spin_unlock(&mac_ctx->sys.bbt_mgmt_lock);
		return;
	}
	mac_ctx->sys.sys_bbt_pending_mgmt_count--;
	qdf_spin_unlock(&mac_ctx->sys.bbt_mgmt_lock);
}

bool lim_check_if_vendor_oui_match(struct mac_context *mac_ctx,
					uint8_t *oui, uint8_t oui_len,
			       uint8_t *ie, uint8_t ie_len)
{
	uint8_t *ptr = ie;
	uint8_t elem_id;

	if (!ie || 0 == ie_len) {
		pe_err("IE Null or ie len zero %d", ie_len);
		return false;
	}

	elem_id = *ie;

	if (elem_id == WLAN_ELEMID_VENDOR &&
		!qdf_mem_cmp(&ptr[2], oui, oui_len))
		return true;
	else
		return false;
}

QDF_STATUS lim_util_get_type_subtype(void *pkt, uint8_t *type,
						uint8_t *subtype)
{
	cds_pkt_t *cds_pkt;
	QDF_STATUS status;
	tpSirMacMgmtHdr hdr;
	uint8_t *rxpktinfor;

	cds_pkt = (cds_pkt_t *) pkt;
	if (!cds_pkt) {
		pe_err("NULL packet received");
		return QDF_STATUS_E_FAILURE;
	}
	status =
		wma_ds_peek_rx_packet_info(cds_pkt, (void *)&rxpktinfor, false);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		pe_err("Failed extract cds packet. status %d", status);
		return QDF_STATUS_E_FAILURE;
	}

	hdr = WMA_GET_RX_MAC_HEADER(rxpktinfor);
	if (hdr->fc.type == SIR_MAC_MGMT_FRAME) {
		pe_debug("RxBd: %pK mHdr: %pK Type: %d Subtype: %d SizeFC: %zu",
				rxpktinfor, hdr, hdr->fc.type, hdr->fc.subType,
				sizeof(tSirMacFrameCtl));
		*type = hdr->fc.type;
		*subtype = hdr->fc.subType;
	} else {
		pe_err("Not a management packet type %d", hdr->fc.type);
		return QDF_STATUS_E_INVAL;
	}
	return QDF_STATUS_SUCCESS;
}

enum rateid lim_get_min_session_txrate(struct pe_session *session)
{
	enum rateid rid = RATEID_DEFAULT;
	uint8_t min_rate = SIR_MAC_RATE_54, curr_rate, i;
	tSirMacRateSet *rateset = &session->rateSet;

	if (!session)
		return rid;

	for (i = 0; i < rateset->numRates; i++) {
		/* Ignore MSB - set to indicate basic rate */
		curr_rate = rateset->rate[i] & 0x7F;
		min_rate =  (curr_rate < min_rate) ? curr_rate : min_rate;
	}
	pe_debug("supported min_rate: %0x(%d)", min_rate, min_rate);

	switch (min_rate) {
	case SIR_MAC_RATE_1:
		rid = RATEID_1MBPS;
		break;
	case SIR_MAC_RATE_2:
		rid = RATEID_2MBPS;
		break;
	case SIR_MAC_RATE_5_5:
		rid = RATEID_5_5MBPS;
		break;
	case SIR_MAC_RATE_11:
		rid = RATEID_11MBPS;
		break;
	case SIR_MAC_RATE_6:
		rid = RATEID_6MBPS;
		break;
	case SIR_MAC_RATE_9:
		rid = RATEID_9MBPS;
		break;
	case SIR_MAC_RATE_12:
		rid = RATEID_12MBPS;
		break;
	case SIR_MAC_RATE_18:
		rid = RATEID_18MBPS;
		break;
	case SIR_MAC_RATE_24:
		rid = RATEID_24MBPS;
		break;
	case SIR_MAC_RATE_36:
		rid = RATEID_36MBPS;
		break;
	case SIR_MAC_RATE_48:
		rid = RATEID_48MBPS;
		break;
	case SIR_MAC_RATE_54:
		rid = RATEID_54MBPS;
		break;
	default:
		rid = RATEID_DEFAULT;
		break;
	}

	return rid;
}

void lim_send_sme_mgmt_frame_ind(struct mac_context *mac_ctx, uint8_t frame_type,
				 uint8_t *frame, uint32_t frame_len,
				 uint16_t session_id, uint32_t rx_freq,
				 struct pe_session *psession_entry,
				 int8_t rx_rssi, enum rxmgmt_flags rx_flags)
{
	tpSirSmeMgmtFrameInd sme_mgmt_frame = NULL;
	uint16_t length;

	length = sizeof(tSirSmeMgmtFrameInd) + frame_len;

	sme_mgmt_frame = qdf_mem_malloc(length);
	if (!sme_mgmt_frame)
		return;

	if (qdf_is_macaddr_broadcast(
		(struct qdf_mac_addr *)(frame + 4)) &&
		!session_id) {
		pe_debug("Broadcast action frame");
		session_id = SME_SESSION_ID_BROADCAST;
	}

	sme_mgmt_frame->frame_len = frame_len;
	sme_mgmt_frame->sessionId = session_id;
	sme_mgmt_frame->frameType = frame_type;
	sme_mgmt_frame->rxRssi = rx_rssi;
	sme_mgmt_frame->rx_freq = rx_freq;
	sme_mgmt_frame->rx_flags = rx_flags;

	qdf_mem_zero(sme_mgmt_frame->frameBuf, frame_len);
	qdf_mem_copy(sme_mgmt_frame->frameBuf, frame, frame_len);

	if (mac_ctx->mgmt_frame_ind_cb)
		mac_ctx->mgmt_frame_ind_cb(sme_mgmt_frame);
	else
		pe_debug_rl("Management indication callback not registered!!");
	qdf_mem_free(sme_mgmt_frame);
	return;
}

void
lim_send_dfs_chan_sw_ie_update(struct mac_context *mac_ctx, struct pe_session *session)
{
	/* Update the beacon template and send to FW */
	if (sch_set_fixed_beacon_fields(mac_ctx, session) !=
					QDF_STATUS_SUCCESS) {
		pe_err("Unable to set CSA IE in beacon");
		return;
	}

	/* Send update beacon template message */
	lim_send_beacon_ind(mac_ctx, session, REASON_CHANNEL_SWITCH);
	pe_debug("Updated CSA IE, IE COUNT: %d",
		 session->gLimChannelSwitch.switchCount);
}

bool lim_is_csa_tx_pending(uint8_t vdev_id)
{
	struct pe_session *session;
	struct mac_context *mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	bool csa_tx_offload;

	if (!mac_ctx) {
		mlme_err("Invalid mac context");
		return false;
	}

	session = pe_find_session_by_vdev_id(mac_ctx, vdev_id);
	if (!session) {
		pe_err("Session does not exist for given vdev_id %d", vdev_id);
		return false;
	}

	csa_tx_offload = wlan_psoc_nif_fw_ext_cap_get(mac_ctx->psoc,
						  WLAN_SOC_CEXT_CSA_TX_OFFLOAD);
	if (session->dfsIncludeChanSwIe &&
	    (session->gLimChannelSwitch.switchCount ==
	     mac_ctx->sap.SapDfsInfo.sap_ch_switch_beacon_cnt) &&
	     csa_tx_offload)
		return true;

	return false;
}

void lim_send_csa_tx_complete(uint8_t vdev_id)
{
	QDF_STATUS status;
	tSirSmeCSAIeTxCompleteRsp *chan_switch_tx_rsp;
	struct pe_session *session;
	struct scheduler_msg msg = {0};
	bool csa_tx_offload;
	uint8_t length = sizeof(*chan_switch_tx_rsp);
	struct mac_context *mac_ctx = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac_ctx) {
		mlme_err("Invalid mac context");
		return;
	}

	session = pe_find_session_by_vdev_id(mac_ctx, vdev_id);
	if (!session) {
		pe_err("Session does not exist for given vdev_id %d", vdev_id);
		return;
	}

	/* Stop the timer if already running in case of csa*/
	csa_tx_offload = wlan_psoc_nif_fw_ext_cap_get(mac_ctx->psoc,
						WLAN_SOC_CEXT_CSA_TX_OFFLOAD);
	if (csa_tx_offload)
		qdf_mc_timer_stop(&session->ap_ecsa_timer);

	/* Done with CSA IE update, send response back to SME */
	session->gLimChannelSwitch.switchCount = 0;
	if (mac_ctx->sap.SapDfsInfo.disable_dfs_ch_switch == false)
		session->gLimChannelSwitch.switchMode = 0;

	session->dfsIncludeChanSwIe = false;
	session->dfsIncludeChanWrapperIe = false;

	chan_switch_tx_rsp = qdf_mem_malloc(length);
	if (!chan_switch_tx_rsp)
		return;

	chan_switch_tx_rsp->sessionId = session->smeSessionId;
	chan_switch_tx_rsp->chanSwIeTxStatus = QDF_STATUS_SUCCESS;

	msg.type = eWNI_SME_DFS_CSAIE_TX_COMPLETE_IND;
	msg.bodyptr = chan_switch_tx_rsp;

	status = scheduler_post_message(QDF_MODULE_ID_PE, QDF_MODULE_ID_SME,
					QDF_MODULE_ID_SME, &msg);
	if (QDF_IS_STATUS_ERROR(status))
		qdf_mem_free(chan_switch_tx_rsp);
}

void lim_process_ap_ecsa_timeout(void *data)
{
	struct pe_session *session = (struct pe_session *)data;
	struct mac_context *mac_ctx;
	uint8_t bcn_int, ch_width;
	uint32_t ch_freq;
	bool csa_tx_offload;
	QDF_STATUS status;

	if (!session || !session->valid) {
		pe_err("Session is not valid");
		return;
	}

	mac_ctx = session->mac_ctx;

	if (!session->dfsIncludeChanSwIe) {
		pe_debug("session->dfsIncludeChanSwIe not set");
		return;
	}

	if (lim_is_csa_tx_pending(session->vdev_id))
		return lim_send_csa_tx_complete(session->vdev_id);

	csa_tx_offload = wlan_psoc_nif_fw_ext_cap_get(mac_ctx->psoc,
						  WLAN_SOC_CEXT_CSA_TX_OFFLOAD);

	if (csa_tx_offload)
		return;

	/* Stop the timer if already running */
	qdf_mc_timer_stop(&session->ap_ecsa_timer);

	if (session->gLimChannelSwitch.switchCount)
		/* Decrement the beacon switch count */
		session->gLimChannelSwitch.switchCount--;

	/*
	 * Send only g_sap_chanswitch_beacon_cnt beacons with CSA IE Set in
	 * when a radar is detected
	 */
	if (session->gLimChannelSwitch.switchCount > 0) {
		/* Send the next beacon with updated CSA IE count */
		lim_send_dfs_chan_sw_ie_update(mac_ctx, session);

		ch_freq = session->gLimChannelSwitch.sw_target_freq;
		ch_width = session->gLimChannelSwitch.ch_width;
		if (mac_ctx->mlme_cfg->dfs_cfg.dfs_beacon_tx_enhanced) {
			if (WLAN_REG_IS_6GHZ_CHAN_FREQ(ch_freq)) {
				send_extended_chan_switch_action_frame
					(mac_ctx, ch_freq, ch_width,
					 session);
			} else {
				/* Send Action frame after updating beacon */
				lim_send_chan_switch_action_frame
					(mac_ctx, ch_freq, ch_width,
					 session);
			}
		}

		/* Restart the timer */
		if (session->beaconParams.beaconInterval)
			bcn_int = session->beaconParams.beaconInterval;
		else
			bcn_int = MLME_CFG_BEACON_INTERVAL_DEF;

		status = qdf_mc_timer_start(&session->ap_ecsa_timer,
					    bcn_int);
		if (QDF_IS_STATUS_ERROR(status)) {
			pe_err("cannot start ap_ecsa_timer");
			lim_process_ap_ecsa_timeout(session);
		}
	} else {
		lim_send_csa_tx_complete(session->vdev_id);
	}
}

QDF_STATUS lim_sta_mlme_vdev_start_send(struct vdev_mlme_obj *vdev_mlme,
					uint16_t data_len, void *data)
{
	struct mac_context *mac_ctx;
	enum vdev_assoc_type assoc_type;

	mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac_ctx) {
		pe_err("mac_ctx is NULL");
		if (data)
			qdf_mem_free(data);
		return QDF_STATUS_E_INVAL;
	}

	assoc_type = mlme_get_assoc_type(vdev_mlme->vdev);
	switch (assoc_type) {
	case VDEV_ASSOC:
		lim_process_mlm_join_req(mac_ctx, (tLimMlmJoinReq *)data);
		break;
	case VDEV_REASSOC:
		lim_process_mlm_reassoc_req(mac_ctx, (tLimMlmReassocReq *)data);
		break;
	case VDEV_FT_REASSOC:
		lim_process_mlm_ft_reassoc_req(mac_ctx,
					       (tLimMlmReassocReq *)data);
		break;
	default:
		pe_err("assoc_type %d is invalid", assoc_type);
	}
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS lim_sta_mlme_vdev_restart_send(struct vdev_mlme_obj *vdev_mlme,
					  uint16_t data_len, void *data)
{
	struct pe_session *session;

	session = (struct pe_session *)data;
	if (!session) {
		pe_err("Invalid session");
		return QDF_STATUS_E_INVAL;
	}
	if (!vdev_mlme) {
		pe_err("vdev_mlme is NULL");
		return QDF_STATUS_E_INVAL;
	}
	if (!data) {
		pe_err("event_data is NULL");
		return QDF_STATUS_E_INVAL;
	}
	if (mlme_is_chan_switch_in_progress(vdev_mlme->vdev)) {
		switch (session->channelChangeReasonCode) {
		case LIM_SWITCH_CHANNEL_OPERATION:
			__lim_process_channel_switch_timeout(session);
			break;
		case LIM_SWITCH_CHANNEL_HT_WIDTH:
			lim_ht_switch_chnl_params(session);
			break;
		case LIM_SWITCH_CHANNEL_REASSOC:
			lim_send_switch_chnl_params(session->mac_ctx, session);
			break;
		default:
			break;
		}
	}
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS lim_sta_mlme_vdev_stop_send(struct vdev_mlme_obj *vdev_mlme,
				       uint16_t data_len, void *data)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	bool connection_fail = false;
	enum vdev_assoc_type assoc_type;

	if (!vdev_mlme) {
		pe_err("vdev_mlme is NULL");
		return QDF_STATUS_E_INVAL;
	}
	if (!data) {
		pe_err("event_data is NULL");
		return QDF_STATUS_E_INVAL;
	}

	connection_fail = mlme_is_connection_fail(vdev_mlme->vdev);
	pe_debug("Send vdev stop, connection_fail %d", connection_fail);
	if (connection_fail) {
		assoc_type = mlme_get_assoc_type(vdev_mlme->vdev);
		switch (assoc_type) {
		case VDEV_ASSOC:
			status =
			    lim_sta_handle_connect_fail((join_params *)data);
			break;
		case VDEV_REASSOC:
		case VDEV_FT_REASSOC:
			status = lim_sta_reassoc_error_handler(
					(struct reassoc_params *)data);
			break;
		default:
			pe_info("Invalid assoc_type %d", assoc_type);
			status = QDF_STATUS_E_INVAL;
			break;
		}
		mlme_set_connection_fail(vdev_mlme->vdev, false);
	} else {
		status = lim_sta_send_del_bss((struct pe_session *)data);
	}

	return status;
}

QDF_STATUS lim_sta_mlme_vdev_req_fail(struct vdev_mlme_obj *vdev_mlme,
				      uint16_t data_len, void *data)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	enum vdev_assoc_type assoc_type;

	if (!vdev_mlme) {
		pe_err("vdev_mlme is NULL");
		return QDF_STATUS_E_INVAL;
	}
	if (!data) {
		pe_err("event_data is NULL");
		return QDF_STATUS_E_INVAL;
	}

	assoc_type = mlme_get_assoc_type(vdev_mlme->vdev);
	switch (assoc_type) {
	case VDEV_ASSOC:
		status = lim_sta_handle_connect_fail((join_params *)data);
		break;
	case VDEV_REASSOC:
	case VDEV_FT_REASSOC:
		status = lim_sta_reassoc_error_handler(
				(struct reassoc_params *)data);
		break;
	default:
		pe_info("Invalid assoc_type %d", assoc_type);
		status = QDF_STATUS_E_INVAL;
		break;
	}

	return status;
}

void lim_send_beacon(struct mac_context *mac_ctx, struct pe_session *session)
{
	if (wlan_vdev_mlme_get_state(session->vdev) ==
	    WLAN_VDEV_S_DFS_CAC_WAIT)
		wlan_vdev_mlme_sm_deliver_evt(session->vdev,
					      WLAN_VDEV_SM_EV_DFS_CAC_COMPLETED,
					      sizeof(*session), session);
	else if (mac_ctx->mlme_cfg->dfs_cfg.dfs_disable_channel_switch &&
		 (wlan_vdev_mlme_get_substate(session->vdev) ==
		  WLAN_VDEV_SS_SUSPEND_CSA_RESTART))
		wlan_vdev_mlme_sm_deliver_evt(
					session->vdev,
					WLAN_VDEV_SM_EV_CHAN_SWITCH_DISABLED,
					sizeof(*session), session);
	else
		wlan_vdev_mlme_sm_deliver_evt(session->vdev,
					      WLAN_VDEV_SM_EV_START_SUCCESS,
					      sizeof(*session), session);
}

void lim_ndi_mlme_vdev_up_transition(struct pe_session *session)
{
	if (!LIM_IS_NDI_ROLE(session))
		return;

	wlan_vdev_mlme_sm_deliver_evt(session->vdev,
				      WLAN_VDEV_SM_EV_START_SUCCESS,
				      sizeof(*session), session);
}

QDF_STATUS lim_sap_move_to_cac_wait_state(struct pe_session *session)
{
	QDF_STATUS status;

	status =
		wlan_vdev_mlme_sm_deliver_evt(session->vdev,
					      WLAN_VDEV_SM_EV_DFS_CAC_WAIT,
					      sizeof(*session), session);
	return status;
}

QDF_STATUS lim_ap_mlme_vdev_start_send(struct vdev_mlme_obj *vdev_mlme,
				       uint16_t data_len, void *data)
{
	struct pe_session *session;
	tpLimMlmStartReq start_req = (tLimMlmStartReq *)data;
	struct mac_context *mac_ctx;

	if (!data) {
		pe_err("data is NULL");
		return QDF_STATUS_E_INVAL;
	}

	mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac_ctx) {
		pe_err("mac_ctx is NULL");
		return QDF_STATUS_E_INVAL;
	}

	session = pe_find_session_by_session_id(mac_ctx,
						start_req->sessionId);
	if (!session) {
		pe_err("session is NULL");
		return QDF_STATUS_E_INVAL;
	}

	lim_process_mlm_start_req(session->mac_ctx, start_req);

	return QDF_STATUS_SUCCESS;
}

static inline void lim_send_csa_restart_resp(struct mac_context *mac_ctx,
					     struct pe_session *session)
{
	struct scheduler_msg msg = {0};
	QDF_STATUS status;

	msg.type = eWNI_SME_CSA_RESTART_RSP;
	msg.bodyptr = NULL;
	msg.bodyval = session->smeSessionId;

	status = scheduler_post_message(QDF_MODULE_ID_PE, QDF_MODULE_ID_SME,
					QDF_MODULE_ID_SME, &msg);
}

QDF_STATUS lim_ap_mlme_vdev_update_beacon(struct vdev_mlme_obj *vdev_mlme,
					  enum beacon_update_op op,
					  uint16_t data_len, void *data)
{
	struct pe_session *session;

	if (!data) {
		pe_err("event_data is NULL");
		return QDF_STATUS_E_INVAL;
	}
	session = (struct pe_session *)data;
	if (LIM_IS_NDI_ROLE(session))
		return QDF_STATUS_SUCCESS;

	if (op == BEACON_INIT)
		lim_send_beacon_ind(session->mac_ctx, session, REASON_DEFAULT);
	else if (op == BEACON_UPDATE)
		lim_send_beacon_ind(session->mac_ctx,
				    session,
				    REASON_CONFIG_UPDATE);
	else if (op == BEACON_CSA)
		lim_send_csa_restart_resp(session->mac_ctx, session);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS lim_ap_mlme_vdev_up_send(struct vdev_mlme_obj *vdev_mlme,
				    uint16_t data_len, void *data)
{
	struct scheduler_msg msg = {0};
	QDF_STATUS status;
	struct pe_session *session = (struct pe_session *)data;

	if (!session) {
		pe_err("session is NULL");
		return QDF_STATUS_E_INVAL;
	}
	if (LIM_IS_NDI_ROLE(session))
		return QDF_STATUS_SUCCESS;


	msg.type = SIR_HAL_SEND_AP_VDEV_UP;
	msg.bodyval = session->smeSessionId;

	status = scheduler_post_message(QDF_MODULE_ID_PE, QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &msg);

	return status;
}

QDF_STATUS lim_ap_mlme_vdev_disconnect_peers(struct vdev_mlme_obj *vdev_mlme,
					     uint16_t data_len, void *data)
{
	struct pe_session *session = (struct pe_session *)data;

	if (!data) {
		pe_err("data is NULL");
		return QDF_STATUS_E_INVAL;
	}

	lim_delete_all_peers(session);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS lim_ap_mlme_vdev_stop_send(struct vdev_mlme_obj *vdev_mlme,
				      uint16_t data_len, void *data)
{
	struct pe_session *session = (struct pe_session *)data;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!data) {
		pe_err("data is NULL");
		return QDF_STATUS_E_INVAL;
	}

	status =  lim_send_vdev_stop(session);

	return status;
}

QDF_STATUS lim_ap_mlme_vdev_restart_send(struct vdev_mlme_obj *vdev_mlme,
					 uint16_t data_len, void *data)
{
	struct pe_session *session = (struct pe_session *)data;

	if (!data) {
		pe_err("data is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (ap_mlme_is_hidden_ssid_restart_in_progress(vdev_mlme->vdev))
		lim_send_vdev_restart(session->mac_ctx, session,
				      session->smeSessionId);
	else
		lim_send_switch_chnl_params(session->mac_ctx, session);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS lim_ap_mlme_vdev_start_req_failed(struct vdev_mlme_obj *vdev_mlme,
					     uint16_t data_len, void *data)
{
	struct mac_context *mac_ctx;

	mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac_ctx) {
		pe_err("mac_ctx is NULL");
		if (data)
			qdf_mem_free(data);
		return QDF_STATUS_E_INVAL;
	}

	lim_process_mlm_start_cnf(mac_ctx, data);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS lim_mon_mlme_vdev_start_send(struct vdev_mlme_obj *vdev_mlme,
					uint16_t data_len, void *data)
{
	struct mac_context *mac_ctx;
	struct pe_session *session = (struct pe_session *)data;

	if (!data) {
		pe_err("data is NULL");
		return QDF_STATUS_E_INVAL;
	}

	mac_ctx = session->mac_ctx;
	if (!mac_ctx) {
		pe_err("mac_ctx is NULL");
		return QDF_STATUS_E_INVAL;
	}

	lim_send_switch_chnl_params(mac_ctx, session);

	return QDF_STATUS_SUCCESS;
}

void lim_send_start_bss_confirm(struct mac_context *mac_ctx,
				tLimMlmStartCnf *start_cnf)
{
	if (start_cnf->resultCode == eSIR_SME_SUCCESS) {
		lim_post_sme_message(mac_ctx, LIM_MLM_START_CNF,
				     (uint32_t *)start_cnf);
	} else {
		struct pe_session *session;

		session = pe_find_session_by_session_id(mac_ctx,
							start_cnf->sessionId);
		if (!session) {
			pe_err("session is NULL");
			return;
		}
		mlme_set_vdev_start_failed(session->vdev, true);
		wlan_vdev_mlme_sm_deliver_evt(session->vdev,
					      WLAN_VDEV_SM_EV_START_REQ_FAIL,
					      sizeof(*start_cnf), start_cnf);
	}
}

QDF_STATUS lim_get_capability_info(struct mac_context *mac, uint16_t *pcap,
				   struct pe_session *pe_session)
{
	uint32_t val = 0;
	tpSirMacCapabilityInfo pcap_info;

	*pcap = 0;
	pcap_info = (tpSirMacCapabilityInfo)pcap;

	if (LIM_IS_AP_ROLE(pe_session) ||
		LIM_IS_STA_ROLE(pe_session))
		pcap_info->ess = 1;      /* ESS bit */
	else if (LIM_IS_P2P_DEVICE_ROLE(pe_session) ||
		LIM_IS_NDI_ROLE(pe_session)) {
		pcap_info->ess = 0;
		pcap_info->ibss = 0;
	} else
		pe_warn("can't get capability, role is UNKNOWN!!");

	if (LIM_IS_AP_ROLE(pe_session)) {
		val = pe_session->privacy;
	} else {
		/* PRIVACY bit */
		val = mac->mlme_cfg->wep_params.is_privacy_enabled;
	}
	if (val)
		pcap_info->privacy = 1;

	/* Short preamble bit */
	if (mac->mlme_cfg->ht_caps.short_preamble)
		pcap_info->shortPreamble =
			mac->mlme_cfg->ht_caps.short_preamble;

	/* PBCC bit */
	pcap_info->pbcc = 0;

	/* Channel agility bit */
	pcap_info->channelAgility = 0;
	/* If STA/AP operating in 11B mode, don't set rest of the
	 * capability info bits.
	 */
	if (pe_session->dot11mode == MLME_DOT11_MODE_11B)
		return QDF_STATUS_SUCCESS;

	/* Short slot time bit */
	if (LIM_IS_AP_ROLE(pe_session)) {
		pcap_info->shortSlotTime = pe_session->shortSlotTimeSupported;
	} else {
		/* When in STA mode, we need to check if short slot is
		 * enabled as well as check if the current operating
		 * mode is short slot time and then decide whether to
		 * enable short slot or not. It is safe to check both
		 * cfg values to determine short slot value in this
		 * funcn since this funcn is always used after assoc
		 * when these cfg values are already set based on
		 * peer's capability. Even in case of IBSS, its value
		 * is set to correct value either in delBSS as part of
		 * deleting the previous IBSS or in start BSS as part
		 * of coalescing
		 */
		if (mac->mlme_cfg->feature_flags.enable_short_slot_time_11g) {
			pcap_info->shortSlotTime =
				pe_session->shortSlotTimeSupported;
		}
	}

	/* Spectrum Management bit */
	if (pe_session->lim11hEnable) {
		if (mac->mlme_cfg->gen.enabled_11h)
			pcap_info->spectrumMgt = 1;
	}
	/* QoS bit */
	if (mac->mlme_cfg->wmm_params.qos_enabled)
		pcap_info->qos = 1;

	/* APSD bit */
	if (mac->mlme_cfg->roam_scoring.apsd_enabled)
		pcap_info->apsd = 1;

	pcap_info->rrm = mac->rrm.rrmConfig.rrm_enabled;
	pe_debug("RRM: %d", pcap_info->rrm);
	/* DSSS-OFDM */
	/* FIXME : no config defined yet. */

	/* Block ack bit */
	val = mac->mlme_cfg->feature_flags.enable_block_ack;
	pcap_info->delayedBA =
		(uint16_t) ((val >> WNI_CFG_BLOCK_ACK_ENABLED_DELAYED) & 1);
	pcap_info->immediateBA =
		(uint16_t) ((val >> WNI_CFG_BLOCK_ACK_ENABLED_IMMEDIATE) & 1);

	return QDF_STATUS_SUCCESS;
}

void lim_flush_bssid(struct mac_context *mac_ctx, uint8_t *bssid)
{
	struct scan_filter *filter;
	struct wlan_objmgr_pdev *pdev = NULL;
	QDF_STATUS status;

	if (!bssid)
		return;

	filter = qdf_mem_malloc(sizeof(*filter));
	if (!filter)
		return;

	filter->num_of_bssid = 1;
	qdf_mem_copy(filter->bssid_list[0].bytes, bssid, QDF_MAC_ADDR_SIZE);

	pdev = wlan_objmgr_get_pdev_by_id(mac_ctx->psoc, 0, WLAN_LEGACY_MAC_ID);
	if (!pdev) {
		pe_err("pdev is NULL");
		qdf_mem_free(filter);
		return;
	}
	status = ucfg_scan_flush_results(pdev, filter);

	wlan_objmgr_pdev_release_ref(pdev, WLAN_LEGACY_MAC_ID);

	if (QDF_IS_STATUS_SUCCESS(status))
		pe_debug("Removed BSS entry:"QDF_MAC_ADDR_FMT" from scan cache",
			 QDF_MAC_ADDR_REF(bssid));

	if (filter)
		qdf_mem_free(filter);
}

bool lim_is_sha384_akm(enum ani_akm_type akm)
{
	switch (akm) {
	case ANI_AKM_TYPE_FILS_SHA384:
	case ANI_AKM_TYPE_FT_FILS_SHA384:
	case ANI_AKM_TYPE_SUITEB_EAP_SHA384:
	case ANI_AKM_TYPE_FT_SUITEB_EAP_SHA384:
		return true;
	default:
		return false;
	}
}

QDF_STATUS lim_set_ch_phy_mode(struct wlan_objmgr_vdev *vdev, uint8_t dot11mode)
{
	struct vdev_mlme_obj *mlme_obj;
	struct wlan_channel *des_chan;
	uint32_t chan_mode;
	enum phy_ch_width ch_width;
	struct mac_context *mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	uint16_t bw_val;
	enum reg_wifi_band band;
	uint8_t band_mask;
	enum channel_state ch_state;
	uint32_t start_ch_freq;

	if (!mac_ctx) {
		pe_err("Invalid mac context");
		return QDF_STATUS_E_FAILURE;
	}
	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!mlme_obj) {
		pe_err("vdev component object is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	des_chan = vdev->vdev_mlme.des_chan;
	/*
	 * Set ch_cfreq1 to ch_freq for 20Mhz. If BW is greater than 20 it
	 * will be updated from ch_freq_seg1.
	 */
	des_chan->ch_cfreq1 = des_chan->ch_freq;
	band = wlan_reg_freq_to_band(des_chan->ch_freq);
	band_mask = 1 << band;
	ch_width = des_chan->ch_width;
	bw_val = wlan_reg_get_bw_value(ch_width);
	if (bw_val > 20) {
		if (des_chan->ch_freq_seg1) {
			des_chan->ch_cfreq1 =
			wlan_reg_chan_band_to_freq(mac_ctx->pdev,
						   des_chan->ch_freq_seg1,
						   band_mask);
		} else {
			pe_err("Invalid cntr_freq for bw %d, drop to 20",
			       bw_val);
			ch_width = CH_WIDTH_20MHZ;
			bw_val = 20;
			if (des_chan->ch_cfreq1)
				des_chan->ch_freq_seg1 =
					wlan_reg_freq_to_chan(
						mac_ctx->pdev,
						des_chan->ch_cfreq1);
		}
	} else if (des_chan->ch_cfreq1) {
		des_chan->ch_freq_seg1 =
			wlan_reg_freq_to_chan(mac_ctx->pdev,
					      des_chan->ch_cfreq1);
	}
	if (bw_val > 80) {
		if (des_chan->ch_freq_seg2) {
			des_chan->ch_cfreq2 =
			wlan_reg_chan_band_to_freq(mac_ctx->pdev,
						   des_chan->ch_freq_seg2,
						   band_mask);
		} else {
			pe_err("Invalid cntr_freq for bw %d, drop to 80",
			       bw_val);
			des_chan->ch_cfreq2 = 0;
			des_chan->ch_freq_seg2 = 0;
			ch_width = CH_WIDTH_80MHZ;
		}
	} else {
		des_chan->ch_cfreq2 = 0;
		des_chan->ch_freq_seg2 = 0;
	}

	des_chan->ch_width = ch_width;

	des_chan->ch_flags = 0;
	switch (ch_width) {
	case CH_WIDTH_20MHZ:
		des_chan->ch_flags |= IEEE80211_CHAN_VHT20;
		break;
	case CH_WIDTH_40MHZ:
		des_chan->ch_flags |= IEEE80211_CHAN_VHT40PLUS;
		break;
	case CH_WIDTH_80MHZ:
		des_chan->ch_flags |= IEEE80211_CHAN_VHT80;
		break;
	case CH_WIDTH_80P80MHZ:
		des_chan->ch_flags |= IEEE80211_CHAN_VHT80_80;
		break;
	case CH_WIDTH_160MHZ:
		des_chan->ch_flags |= IEEE80211_CHAN_VHT160;
		break;
	default:
		break;
	}

	if (WLAN_REG_IS_24GHZ_CH_FREQ(des_chan->ch_freq))
		des_chan->ch_flags |= IEEE80211_CHAN_2GHZ;
	else
		des_chan->ch_flags |= IEEE80211_CHAN_5GHZ;

	des_chan->ch_flagext = 0;
	if (wlan_reg_is_dfs_for_freq(mac_ctx->pdev, des_chan->ch_freq))
		des_chan->ch_flagext |= IEEE80211_CHAN_DFS;
	if (des_chan->ch_cfreq2) {
		if (CH_WIDTH_80P80MHZ == des_chan->ch_width)
			start_ch_freq = des_chan->ch_cfreq2 - 30;
		else
			start_ch_freq = des_chan->ch_freq;

		ch_state = wlan_reg_get_5g_bonded_channel_state_for_freq(
					mac_ctx->pdev, start_ch_freq,
					des_chan->ch_width);
		if (CHANNEL_STATE_DFS == ch_state)
			des_chan->ch_flagext |= IEEE80211_CHAN_DFS_CFREQ2;
	}

	chan_mode = wma_chan_phy_mode(des_chan->ch_freq, ch_width,
				      dot11mode);

	if (chan_mode == WLAN_PHYMODE_AUTO || chan_mode == WLAN_PHYMODE_MAX) {
		pe_err("Invalid phy mode!");
		return QDF_STATUS_E_FAILURE;
	}
	if (!des_chan->ch_cfreq1) {
		pe_err("Invalid center freq1");
		return QDF_STATUS_E_FAILURE;
	}

	if ((ch_width == CH_WIDTH_160MHZ ||
	     ch_width == CH_WIDTH_80P80MHZ) && !des_chan->ch_cfreq2) {
		pe_err("Invalid center freq2 for 160MHz");
		return QDF_STATUS_E_FAILURE;
	}
	/* Till conversion is not done in WMI we need to fill fw phy mode */
	mlme_obj->mgmt.generic.phy_mode = wma_host_to_fw_phymode(chan_mode);
	des_chan->ch_phymode = chan_mode;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS lim_pre_vdev_start(struct mac_context *mac,
			      struct vdev_mlme_obj *mlme_obj,
			      struct pe_session *session)
{
	struct wlan_channel *des_chan;
	enum reg_wifi_band band;
	uint8_t band_mask;
	struct ch_params ch_params = {0};
	qdf_freq_t sec_chan_freq = 0;

	band = wlan_reg_freq_to_band(session->curr_op_freq);
	band_mask = 1 << band;

	ch_params.ch_width = session->ch_width;
	ch_params.mhz_freq_seg0 =
		wlan_reg_chan_band_to_freq(mac->pdev,
					   session->ch_center_freq_seg0,
					   band_mask);

	if (session->ch_center_freq_seg1)
		ch_params.mhz_freq_seg1 =
			wlan_reg_chan_band_to_freq(mac->pdev,
						   session->ch_center_freq_seg1,
						   band_mask);

	if (band == (REG_BAND_2G) && (ch_params.ch_width == CH_WIDTH_40MHZ)) {
		if (ch_params.mhz_freq_seg0 ==  session->curr_op_freq + 10)
			sec_chan_freq = session->curr_op_freq + 20;
		if (ch_params.mhz_freq_seg0 ==  session->curr_op_freq - 10)
			sec_chan_freq = session->curr_op_freq - 20;
	}

	wlan_reg_set_channel_params_for_freq(mac->pdev, session->curr_op_freq,
					     sec_chan_freq, &ch_params);

	pe_debug("vdev id %d freq %d seg0 %d seg1 %d ch_width %d cac_duration_ms %d beacon_interval %d hidden_ssid: %d dtimPeriod %d slot_time %d bcn tx rate %d mhz seg0 %d mhz seg1 %d",
		 session->vdev_id, session->curr_op_freq,
		 ch_params.center_freq_seg0,
		 ch_params.center_freq_seg1, ch_params.ch_width,
		 session->cac_duration_ms,
		 session->beaconParams.beaconInterval,
		 session->ssidHidden, session->dtimPeriod,
		 session->shortSlotTimeSupported,
		 session->beacon_tx_rate,
		 ch_params.mhz_freq_seg0,
		 ch_params.mhz_freq_seg1);
	/* Invalid channel width means no suitable channel bonding in current
	 * regdomain for requested channel frequency. Abort vdev start.
	 */
	if (ch_params.ch_width == CH_WIDTH_INVALID) {
		pe_debug("abort vdev start invalid chan parameters");
		return QDF_STATUS_E_INVAL;
	}

	des_chan = mlme_obj->vdev->vdev_mlme.des_chan;
	des_chan->ch_freq = session->curr_op_freq;
	des_chan->ch_width = ch_params.ch_width;
	des_chan->ch_freq_seg1 = ch_params.center_freq_seg0;
	des_chan->ch_freq_seg2 = ch_params.center_freq_seg1;
	des_chan->ch_ieee = wlan_reg_freq_to_chan(mac->pdev, des_chan->ch_freq);
	session->ch_width = ch_params.ch_width;
	session->ch_center_freq_seg0 = ch_params.center_freq_seg0;
	session->ch_center_freq_seg1 = ch_params.center_freq_seg1;

	mlme_obj->mgmt.generic.maxregpower = session->maxTxPower;
	mlme_obj->proto.generic.beacon_interval =
				session->beaconParams.beaconInterval;
	if (mlme_obj->mgmt.generic.type == WLAN_VDEV_MLME_TYPE_AP) {
		mlme_obj->mgmt.ap.hidden_ssid = session->ssidHidden;
		mlme_obj->mgmt.ap.cac_duration_ms = session->cac_duration_ms;
	}
	mlme_obj->proto.generic.dtim_period = session->dtimPeriod;
	mlme_obj->proto.generic.slot_time = session->shortSlotTimeSupported;
	mlme_obj->mgmt.rate_info.bcn_tx_rate = session->beacon_tx_rate;

	mlme_obj->proto.ht_info.allow_ht = !!session->htCapability;
	mlme_obj->proto.vht_info.allow_vht = !!session->vhtCapability;

	if (cds_is_5_mhz_enabled())
		mlme_obj->mgmt.rate_info.quarter_rate = 1;
	else if (cds_is_10_mhz_enabled())
		mlme_obj->mgmt.rate_info.half_rate = 1;

	if (session->nss == 2) {
		mlme_obj->mgmt.chainmask_info.num_rx_chain = 2;
		mlme_obj->mgmt.chainmask_info.num_tx_chain = 2;
	} else {
		mlme_obj->mgmt.chainmask_info.num_rx_chain = 1;
		mlme_obj->mgmt.chainmask_info.num_tx_chain = 1;
	}
	wlan_vdev_mlme_set_ssid(mlme_obj->vdev, session->ssId.ssId,
				session->ssId.length);

	return lim_set_ch_phy_mode(mlme_obj->vdev, session->dot11mode);
}
