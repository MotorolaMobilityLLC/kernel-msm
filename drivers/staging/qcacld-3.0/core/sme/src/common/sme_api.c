/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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
 * DOC: sme_api.c
 *
 * Definitions for SME APIs
 */

/* Include Files */
#include <sir_common.h>
#include <ani_global.h>
#include "sme_api.h"
#include "csr_inside_api.h"
#include "sme_inside.h"
#include "csr_internal.h"
#include "wma_types.h"
#include "wma_if.h"
#include "wma.h"
#include "wma_fips_api.h"
#include "wma_fw_state.h"
#include "qdf_trace.h"
#include "sme_trace.h"
#include "qdf_types.h"
#include "qdf_util.h"
#include "qdf_trace.h"
#include "cds_utils.h"
#include "sap_api.h"
#include "mac_trace.h"
#include "cds_regdomain.h"
#include "sme_power_save_api.h"
#include "wma.h"
#include "wma_twt.h"
#include "sch_api.h"
#include "sme_nan_datapath.h"
#include "csr_api.h"
#include "wlan_reg_services_api.h"
#include <wlan_scan_ucfg_api.h>
#include "wlan_reg_ucfg_api.h"
#include "ol_txrx.h"
#include "wifi_pos_api.h"
#include "net/cfg80211.h"
#include <wlan_spectral_utils_api.h>
#include "wlan_mlme_public_struct.h"
#include "wlan_mlme_main.h"
#include "cfg_ucfg_api.h"
#include "wlan_fwol_ucfg_api.h"
#include <wlan_coex_ucfg_api.h>
#include "wlan_crypto_global_api.h"
#include "wlan_mlme_ucfg_api.h"
#include "wlan_psoc_mlme_api.h"
#include "mac_init_api.h"
#include "wlan_cm_roam_api.h"
#include "wlan_cm_tgt_if_tx_api.h"
#include "wlan_mlme_twt_public_struct.h"
#include "wlan_mlme_twt_api.h"
#include "wlan_mlme_twt_ucfg_api.h"
#include "parser_api.h"
#include <wlan_mlme_twt_api.h>

static QDF_STATUS init_sme_cmd_list(struct mac_context *mac);

static void sme_disconnect_connected_sessions(struct mac_context *mac,
					      enum wlan_reason_code reason);

static QDF_STATUS sme_handle_generic_change_country_code(struct mac_context *mac,
						  void *msg_buf);

static QDF_STATUS sme_process_nss_update_resp(struct mac_context *mac, uint8_t *msg);

/* Channel Change Response Indication Handler */
static QDF_STATUS sme_process_channel_change_resp(struct mac_context *mac,
					   uint16_t msg_type, void *msg_buf);

static QDF_STATUS sme_stats_ext_event(struct mac_context *mac,
				      struct stats_ext_event *msg);

static QDF_STATUS sme_fw_state_resp(struct mac_context *mac);

/* Internal SME APIs */
QDF_STATUS sme_acquire_global_lock(struct sme_context *sme)
{
	if (!sme)
		return QDF_STATUS_E_INVAL;

	return qdf_mutex_acquire(&sme->sme_global_lock);
}

QDF_STATUS sme_release_global_lock(struct sme_context *sme)
{
	if (!sme)
		return QDF_STATUS_E_INVAL;

	return qdf_mutex_release(&sme->sme_global_lock);
}

QDF_STATUS cm_roam_acquire_lock(void)
{
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_SME);

	if (!mac)
		return QDF_STATUS_E_FAILURE;

	return sme_acquire_global_lock(&mac->sme);
}

QDF_STATUS cm_roam_release_lock(void)
{
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_SME);

	if (!mac)
		return QDF_STATUS_E_FAILURE;

	return sme_release_global_lock(&mac->sme);
}

struct mac_context *sme_get_mac_context(void)
{
	struct mac_context *mac_ctx;
	mac_handle_t mac_handle;

	mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	if (!mac_handle) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_FATAL,
		FL("invalid mac_handle"));
		return NULL;
	}

	mac_ctx = MAC_CONTEXT(mac_handle);

	return mac_ctx;
}

/**
 * sme_process_set_hw_mode_resp() - Process set HW mode response
 * @mac: Global MAC pointer
 * @msg: HW mode response
 *
 * Processes the HW mode response and invokes the HDD callback
 * to process further
 */
static QDF_STATUS sme_process_set_hw_mode_resp(struct mac_context *mac, uint8_t *msg)
{
	tListElem *entry;
	tSmeCmd *command = NULL;
	bool found;
	policy_mgr_pdev_set_hw_mode_cback callback = NULL;
	struct sir_set_hw_mode_resp *param;
	enum policy_mgr_conn_update_reason reason;
	struct csr_roam_session *session;
	uint32_t session_id;
	uint32_t request_id;

	param = (struct sir_set_hw_mode_resp *)msg;
	if (!param) {
		sme_err("HW mode resp param is NULL");
		/* Not returning. Need to check if active command list
		 * needs to be freed
		 */
	}

	entry = csr_nonscan_active_ll_peek_head(mac, LL_ACCESS_LOCK);
	if (!entry) {
		sme_err("No cmd found in active list");
		return QDF_STATUS_E_FAILURE;
	}

	command = GET_BASE_ADDR(entry, tSmeCmd, Link);
	if (!command) {
		sme_err("Base address is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (e_sme_command_set_hw_mode != command->command) {
		sme_err("Command mismatch!");
		return QDF_STATUS_E_FAILURE;
	}

	callback = command->u.set_hw_mode_cmd.set_hw_mode_cb;
	reason = command->u.set_hw_mode_cmd.reason;
	session_id = command->u.set_hw_mode_cmd.session_id;
	request_id = command->u.set_hw_mode_cmd.request_id;

	sme_debug("reason: %d session: %d",
		command->u.set_hw_mode_cmd.reason,
		command->u.set_hw_mode_cmd.session_id);

	if (!callback) {
		sme_err("Callback does not exist");
		goto end;
	}

	if (!param) {
		sme_err("Callback failed since HW mode params is NULL");
		goto end;
	}

	/* Irrespective of the reason for which the hw mode change request
	 * was issued, the policy manager connection table needs to be updated
	 * with the new vdev-mac id mapping, tx/rx spatial streams etc., if the
	 * set hw mode was successful.
	 */
	callback(param->status,
			param->cfgd_hw_mode_index,
			param->num_vdev_mac_entries,
			param->vdev_mac_map,
			command->u.set_hw_mode_cmd.next_action,
			command->u.set_hw_mode_cmd.reason,
			command->u.set_hw_mode_cmd.session_id,
			command->u.set_hw_mode_cmd.context,
			command->u.set_hw_mode_cmd.request_id);
	if (!CSR_IS_SESSION_VALID(mac, session_id)) {
		sme_err("session %d is invalid", session_id);
		goto end;
	}
	session = CSR_GET_SESSION(mac, session_id);
	if (reason == POLICY_MGR_UPDATE_REASON_HIDDEN_STA) {
		/* In the case of hidden SSID, connection update
		 * (set hw mode) is done after the scan with reason
		 * code eCsrScanForSsid completes. The connect/failure
		 * needs to be handled after the response of set hw
		 * mode
		 */
		if (param->status == SET_HW_MODE_STATUS_OK ||
		    param->status == SET_HW_MODE_STATUS_ALREADY) {
			sme_debug("search for ssid success");
			csr_scan_handle_search_for_ssid(mac,
					session_id);
		} else {
			sme_debug("search for ssid failure");
			csr_scan_handle_search_for_ssid_failure(mac,
					session_id);
		}
		csr_saved_scan_cmd_free_fields(mac, session);
	}
	if (reason == POLICY_MGR_UPDATE_REASON_CHANNEL_SWITCH_STA) {
		sme_debug("Continue channel switch for STA on vdev %d",
			  session_id);
		csr_sta_continue_csa(mac, session_id);
	}

	if (reason == POLICY_MGR_UPDATE_REASON_CHANNEL_SWITCH_SAP) {
		sme_debug("Continue channel switch for SAP on vdev %d",
			  session_id);
		csr_csa_restart(mac, session_id);
	}
	if (reason == POLICY_MGR_UPDATE_REASON_LFR2_ROAM)
		csr_continue_lfr2_connect(mac, session_id);

end:
	found = csr_nonscan_active_ll_remove_entry(mac, entry,
			LL_ACCESS_LOCK);
	if (found)
		/* Now put this command back on the avilable command list */
		csr_release_command(mac, command);

	return QDF_STATUS_SUCCESS;
}

/**
 * sme_process_hw_mode_trans_ind() - Process HW mode transition indication
 * @mac: Global MAC pointer
 * @msg: HW mode transition response
 *
 * Processes the HW mode transition indication and invoke the HDD callback
 * to process further
 */
static QDF_STATUS sme_process_hw_mode_trans_ind(struct mac_context *mac,
						uint8_t *msg)
{
	struct sir_hw_mode_trans_ind *param;

	param = (struct sir_hw_mode_trans_ind *)msg;
	if (!param) {
		sme_err("HW mode trans ind param is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	policy_mgr_hw_mode_transition_cb(param->old_hw_mode_index,
		param->new_hw_mode_index,
		param->num_vdev_mac_entries,
		param->vdev_mac_map, mac->psoc);

	return QDF_STATUS_SUCCESS;
}

void sme_purge_pdev_all_ser_cmd_list(mac_handle_t mac_handle)
{
	QDF_STATUS status;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_IS_STATUS_ERROR(status))
		return;

	csr_purge_pdev_all_ser_cmd_list(mac_ctx);
	sme_release_global_lock(&mac_ctx->sme);
}

/**
 * free_sme_cmds() - This function frees memory allocated for SME commands
 * @mac_ctx:      Pointer to Global MAC structure
 *
 * This function frees memory allocated for SME commands
 *
 * @Return: void
 */
static void free_sme_cmds(struct mac_context *mac_ctx)
{
	uint32_t idx;

	if (!mac_ctx->sme.sme_cmd_buf_addr)
		return;

	for (idx = 0; idx < mac_ctx->sme.sme_cmd_count; idx++)
		qdf_mem_free(mac_ctx->sme.sme_cmd_buf_addr[idx]);

	qdf_mem_free(mac_ctx->sme.sme_cmd_buf_addr);
	mac_ctx->sme.sme_cmd_buf_addr = NULL;
}

static QDF_STATUS init_sme_cmd_list(struct mac_context *mac)
{
	QDF_STATUS status;
	tSmeCmd *cmd;
	uint32_t cmd_idx;
	uint32_t sme_cmd_ptr_ary_sz;

	mac->sme.sme_cmd_count = SME_TOTAL_COMMAND;

	status = csr_ll_open(&mac->sme.sme_cmd_freelist);
	if (!QDF_IS_STATUS_SUCCESS(status))
		goto end;

	/* following pointer contains array of pointers for tSmeCmd* */
	sme_cmd_ptr_ary_sz = sizeof(void *) * mac->sme.sme_cmd_count;
	mac->sme.sme_cmd_buf_addr = qdf_mem_malloc(sme_cmd_ptr_ary_sz);
	if (!mac->sme.sme_cmd_buf_addr) {
		status = QDF_STATUS_E_NOMEM;
		goto end;
	}

	status = QDF_STATUS_SUCCESS;
	for (cmd_idx = 0; cmd_idx < mac->sme.sme_cmd_count; cmd_idx++) {
		/*
		 * Since total size of all commands together can be huge chunk
		 * of memory, allocate SME cmd individually. These SME CMDs are
		 * moved between pending and active queues. And these freeing of
		 * these queues just manipulates the list but does not actually
		 * frees SME CMD pointers. Hence store each SME CMD address in
		 * the array, sme.sme_cmd_buf_addr. This will later facilitate
		 * freeing up of all SME CMDs with just a for loop.
		 */
		cmd = qdf_mem_malloc(sizeof(*cmd));
		if (!cmd) {
			status = QDF_STATUS_E_NOMEM;
			free_sme_cmds(mac);
			goto end;
		}
		mac->sme.sme_cmd_buf_addr[cmd_idx] = cmd;
		csr_ll_insert_tail(&mac->sme.sme_cmd_freelist,
				   &cmd->Link, LL_ACCESS_LOCK);
	}

end:
	if (!QDF_IS_STATUS_SUCCESS(status))
		sme_err("Failed to initialize sme command list: %d", status);

	return status;
}

void sme_release_command(struct mac_context *mac_ctx, tSmeCmd *sme_cmd)
{
	sme_cmd->command = eSmeNoCommand;
	csr_ll_insert_tail(&mac_ctx->sme.sme_cmd_freelist, &sme_cmd->Link,
			   LL_ACCESS_LOCK);
}

static QDF_STATUS free_sme_cmd_list(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	csr_ll_close(&mac->sme.sme_cmd_freelist);

	status = sme_acquire_global_lock(&mac->sme);
	if (status != QDF_STATUS_SUCCESS)
		goto done;

	free_sme_cmds(mac);

	status = sme_release_global_lock(&mac->sme);
	if (status != QDF_STATUS_SUCCESS)
		sme_err("Failed to release the lock status: %d", status);
done:
	return status;
}

static void dump_csr_command_info(struct mac_context *mac, tSmeCmd *pCmd)
{
	switch (pCmd->command) {
	case eSmeCommandRoam:
		sme_debug("roam command reason is %d",
			pCmd->u.roamCmd.roamReason);
		break;

	case eSmeCommandWmStatusChange:
		sme_debug("WMStatusChange command type is %d",
			pCmd->u.wmStatusChangeCmd.Type);
		break;

	default:
		sme_debug("default: Unhandled command %d",
			pCmd->command);
		break;
	}
}

tSmeCmd *sme_get_command_buffer(struct mac_context *mac)
{
	tSmeCmd *pRetCmd = NULL, *pTempCmd = NULL;
	tListElem *pEntry;
	static int sme_command_queue_full;

	pEntry = csr_ll_remove_head(&mac->sme.sme_cmd_freelist, LL_ACCESS_LOCK);

	/* If we can get another MS Msg buffer, then we are ok.  Just
	 * link the entry onto the linked list.  (We are using the
	 * linked list to keep track of the message buffers).
	 */
	if (pEntry) {
		pRetCmd = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
		/* reset when free list is available */
		sme_command_queue_full = 0;
	} else {
		int idx = 1;

		/* Cannot change pRetCmd here since it needs to return later. */
		pEntry = csr_nonscan_active_ll_peek_head(mac, LL_ACCESS_LOCK);
		if (pEntry)
			pTempCmd = GET_BASE_ADDR(pEntry, tSmeCmd, Link);

		sme_err("Out of command buffer.... command (0x%X) stuck",
			(pTempCmd) ? pTempCmd->command : eSmeNoCommand);
		if (pTempCmd) {
			if (eSmeCsrCommandMask & pTempCmd->command)
				/* CSR command is stuck. See what the reason
				 * code is for that command
				 */
				dump_csr_command_info(mac, pTempCmd);
		} /* if(pTempCmd) */

		/* dump what is in the pending queue */
		pEntry =
			csr_nonscan_pending_ll_peek_head(mac,
					 LL_ACCESS_NOLOCK);
		while (pEntry && !sme_command_queue_full) {
			pTempCmd = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
			/* Print only 1st five commands from pending queue. */
			if (idx <= 5)
				sme_err("Out of command buffer.... SME pending command #%d (0x%X)",
					idx, pTempCmd->command);
			idx++;
			if (eSmeCsrCommandMask & pTempCmd->command)
				/* CSR command is stuck. See what the reason
				 * code is for that command
				 */
				dump_csr_command_info(mac, pTempCmd);
			pEntry = csr_nonscan_pending_ll_next(mac, pEntry,
					    LL_ACCESS_NOLOCK);
		}

		if (mac->mlme_cfg->gen.fatal_event_trigger)
			cds_flush_logs(WLAN_LOG_TYPE_FATAL,
				WLAN_LOG_INDICATOR_HOST_DRIVER,
				WLAN_LOG_REASON_SME_OUT_OF_CMD_BUF,
				false,
				mac->mlme_cfg->gen.self_recovery);
		else
			cds_trigger_recovery(QDF_GET_MSG_BUFF_FAILURE);
	}

	/* memset to zero */
	if (pRetCmd) {
		qdf_mem_zero((uint8_t *)&pRetCmd->command,
			    sizeof(pRetCmd->command));
		qdf_mem_zero((uint8_t *)&pRetCmd->vdev_id,
			    sizeof(pRetCmd->vdev_id));
		qdf_mem_zero((uint8_t *)&pRetCmd->u, sizeof(pRetCmd->u));
	}

	return pRetCmd;
}

/**
 * sme_ser_handle_active_cmd() - handle command activation callback from
 *					new serialization module
 * @cmd: pointer to new serialization command
 *
 * This API is to handle command activation callback from new serialization
 * callback
 *
 * Return: QDF_STATUS_SUCCESS
 */
static
QDF_STATUS sme_ser_handle_active_cmd(struct wlan_serialization_command *cmd)
{
	tSmeCmd *sme_cmd;
	mac_handle_t mac_handle;
	struct mac_context *mac_ctx;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	bool do_continue;

	if (!cmd) {
		sme_err("No serialization command found");
		return QDF_STATUS_E_FAILURE;
	}

	mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	if (mac_handle) {
		mac_ctx = MAC_CONTEXT(mac_handle);
	} else {
		sme_err("No mac_handle found");
		return QDF_STATUS_E_FAILURE;
	}
	sme_cmd = cmd->umac_cmd;
	if (!sme_cmd) {
		sme_err("No SME command found");
		return QDF_STATUS_E_FAILURE;
	}

	switch (sme_cmd->command) {
	case eSmeCommandRoam:
		status = csr_roam_process_command(mac_ctx, sme_cmd);
		break;
	case eSmeCommandWmStatusChange:
		csr_roam_process_wm_status_change_command(mac_ctx,
					sme_cmd);
		break;
	case eSmeCommandGetdisconnectStats:
		csr_roam_process_get_disconnect_stats_command(mac_ctx,
							      sme_cmd);
		break;

	case eSmeCommandAddTs:
	case eSmeCommandDelTs:
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		do_continue = qos_process_command(mac_ctx, sme_cmd);
		if (do_continue)
			status = QDF_STATUS_E_FAILURE;
#endif
		break;
	case e_sme_command_set_hw_mode:
		csr_process_set_hw_mode(mac_ctx, sme_cmd);
		break;
	case e_sme_command_nss_update:
		csr_process_nss_update_req(mac_ctx, sme_cmd);
		break;
	case e_sme_command_set_dual_mac_config:
		csr_process_set_dual_mac_config(mac_ctx, sme_cmd);
		break;
	case e_sme_command_set_antenna_mode:
		csr_process_set_antenna_mode(mac_ctx, sme_cmd);
		break;
	default:
		/* something is wrong */
		sme_err("unknown command %d", sme_cmd->command);
		status = QDF_STATUS_E_FAILURE;
		break;
	}
	return status;
}

QDF_STATUS sme_ser_cmd_callback(struct wlan_serialization_command *cmd,
				enum wlan_serialization_cb_reason reason)
{
	mac_handle_t mac_handle;
	struct mac_context *mac_ctx;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSmeCmd *sme_cmd;

	mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	if (mac_handle) {
		mac_ctx = MAC_CONTEXT(mac_handle);
	} else {
		sme_err("mac_handle is null");
		return QDF_STATUS_E_FAILURE;
	}
	/*
	 * Do not acquire lock here as sme global lock is already acquired in
	 * caller or MC thread context
	 */
	if (!cmd) {
		sme_err("serialization command is null");
		return QDF_STATUS_E_FAILURE;
	}

	switch (reason) {
	case WLAN_SER_CB_ACTIVATE_CMD:
		status = sme_ser_handle_active_cmd(cmd);
		break;
	case WLAN_SER_CB_CANCEL_CMD:
		break;
	case WLAN_SER_CB_RELEASE_MEM_CMD:
		if (cmd->vdev)
			wlan_objmgr_vdev_release_ref(cmd->vdev,
						     WLAN_LEGACY_SME_ID);
		sme_cmd = cmd->umac_cmd;
		csr_release_command_buffer(mac_ctx, sme_cmd);
		break;
	case WLAN_SER_CB_ACTIVE_CMD_TIMEOUT:
		sme_cmd = cmd->umac_cmd;
		if (sme_cmd && (sme_cmd->command == eSmeCommandRoam ||
		    sme_cmd->command == eSmeCommandWmStatusChange))
			qdf_trigger_self_recovery(mac_ctx->psoc,
						  QDF_ACTIVE_LIST_TIMEOUT);
		break;
	default:
		sme_debug("unknown reason code");
		return QDF_STATUS_E_FAILURE;
	}
	return status;
}

#ifdef WLAN_FEATURE_MEMDUMP_ENABLE
/**
 * sme_get_sessionid_from_activelist() - gets vdev_id
 * @mac: mac context
 *
 * This function is used to get session id from sme command
 * active list
 *
 * Return: returns vdev_id
 */
static uint32_t sme_get_sessionid_from_activelist(struct mac_context *mac)
{
	tListElem *entry;
	tSmeCmd *command;
	uint32_t vdev_id = WLAN_UMAC_VDEV_ID_MAX;

	entry = csr_nonscan_active_ll_peek_head(mac, LL_ACCESS_LOCK);
	if (entry) {
		command = GET_BASE_ADDR(entry, tSmeCmd, Link);
		vdev_id = command->vdev_id;
	}

	return vdev_id;
}

/**
 * sme_state_info_dump() - prints state information of sme layer
 * @buf: buffer pointer
 * @size: size of buffer to be filled
 *
 * This function is used to dump state information of sme layer
 *
 * Return: None
 */
static void sme_state_info_dump(char **buf_ptr, uint16_t *size)
{
	uint32_t session_id, active_session_id;
	mac_handle_t mac_handle;
	struct mac_context *mac;
	uint16_t len = 0;
	char *buf = *buf_ptr;
	eCsrConnectState connect_state;

	mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	if (!mac_handle) {
		QDF_ASSERT(0);
		return;
	}

	mac = MAC_CONTEXT(mac_handle);

	active_session_id = sme_get_sessionid_from_activelist(mac);
	if (active_session_id != WLAN_UMAC_VDEV_ID_MAX) {
		len += qdf_scnprintf(buf + len, *size - len,
			"\n active command sessionid %d", active_session_id);
	}

	for (session_id = 0; session_id < WLAN_MAX_VDEVS; session_id++) {
		if (CSR_IS_SESSION_VALID(mac, session_id)) {
			connect_state =
				mac->roam.roamSession[session_id].connectState;
			if ((eCSR_ASSOC_STATE_TYPE_INFRA_ASSOCIATED ==
			     connect_state)
			    || (eCSR_ASSOC_STATE_TYPE_INFRA_CONNECTED ==
				connect_state)) {
				len += qdf_scnprintf(buf + len, *size - len,
					"\n NeighborRoamState: %d",
					mac->roam.neighborRoamInfo[session_id].
						neighborRoamState);
				len += qdf_scnprintf(buf + len, *size - len,
					"\n RoamState: %d", mac->roam.
						curState[session_id]);
				len += qdf_scnprintf(buf + len, *size - len,
					"\n RoamSubState: %d", mac->roam.
						curSubState[session_id]);
				len += qdf_scnprintf(buf + len, *size - len,
					"\n ConnectState: %d",
					connect_state);
			}
		}
	}

	*size -= len;
	*buf_ptr += len;
}

/**
 * sme_register_debug_callback() - registration function sme layer
 * to print sme state information
 *
 * Return: None
 */
static void sme_register_debug_callback(void)
{
	qdf_register_debug_callback(QDF_MODULE_ID_SME, &sme_state_info_dump);
}
#else /* WLAN_FEATURE_MEMDUMP_ENABLE */
static void sme_register_debug_callback(void)
{
}
#endif /* WLAN_FEATURE_MEMDUMP_ENABLE */

#ifdef WLAN_POWER_DEBUG
static void sme_power_debug_stats_cb(struct mac_context *mac,
				     struct power_stats_response *response)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (mac->sme.power_stats_resp_callback)
			mac->sme.power_stats_resp_callback(
					response,
					mac->sme.power_debug_stats_context);
		else
			sme_err("Null hdd cb");
		mac->sme.power_stats_resp_callback = NULL;
		mac->sme.power_debug_stats_context = NULL;
		sme_release_global_lock(&mac->sme);
	}
}

static void sme_register_power_debug_stats_cb(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = sme_acquire_global_lock(&mac->sme);

	if (QDF_IS_STATUS_SUCCESS(status)) {
		mac->sme.sme_power_debug_stats_callback =
						sme_power_debug_stats_cb;
		sme_release_global_lock(&mac->sme);
	}
}

static void sme_unregister_power_debug_stats_cb(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		mac->sme.sme_power_debug_stats_callback = NULL;
		sme_release_global_lock(&mac->sme);
	}
}
#else
static inline void sme_register_power_debug_stats_cb(struct mac_context *mac)
{
}

static inline void sme_unregister_power_debug_stats_cb(struct mac_context *mac)
{
}
#endif

/* Global APIs */

/**
 * sme_open() - Initialze all SME modules and put them at idle state
 * @mac_handle:       The handle returned by mac_open
 *
 * The function initializes each module inside SME, PMC, CSR, etc. Upon
 * successfully return, all modules are at idle state ready to start.
 * smeOpen must be called before any other SME APIs can be involved.
 * smeOpen must be called after mac_open.
 *
 * Return: QDF_STATUS_SUCCESS - SME is successfully initialized.
 *         Other status means SME is failed to be initialized
 */
QDF_STATUS sme_open(mac_handle_t mac_handle)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	mac->sme.state = SME_STATE_STOP;
	mac->sme.curr_device_mode = QDF_STA_MODE;
	if (!QDF_IS_STATUS_SUCCESS(qdf_mutex_create(
					&mac->sme.sme_global_lock))) {
		sme_err("Init lock failed");
		return  QDF_STATUS_E_FAILURE;
	}
	status = csr_open(mac);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("csr_open failed, status: %d", status);
		return status;
	}

	status = sme_ps_open(mac_handle);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("sme_ps_open failed with status: %d", status);
		return status;
	}

#ifndef WLAN_MDM_CODE_REDUCTION_OPT
	status = sme_qos_open(mac);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Qos open, status: %d", status);
		return status;
	}
#endif
	status = init_sme_cmd_list(mac);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;

	status = rrm_open(mac);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("rrm_open failed, status: %d", status);
		return status;
	}
	sme_trace_init(mac);
	sme_register_debug_callback();
	sme_register_power_debug_stats_cb(mac);

	return status;
}

/*
 * sme_init_chan_list, triggers channel setup based on country code.
 */
QDF_STATUS sme_init_chan_list(mac_handle_t mac_handle, uint8_t *alpha2,
			      enum country_src cc_src)
{
	struct mac_context *pmac = MAC_CONTEXT(mac_handle);

	if ((cc_src == SOURCE_USERSPACE) &&
	    (pmac->mlme_cfg->sap_cfg.country_code_priority)) {
		pmac->mlme_cfg->gen.enabled_11d = false;
	}

	return csr_init_chan_list(pmac, alpha2);
}

/*
 * sme_set11dinfo() - Set the 11d information about valid channels
 *  and there power using information from nvRAM
 *  This function is called only for AP.
 *
 *  This is a synchronous call
 *
 * mac_handle - The handle returned by mac_open.
 * pSmeConfigParams - a pointer to a caller allocated object of
 *  struct sme_config_params.
 *
 * Return QDF_STATUS_SUCCESS - SME update the config parameters successfully.
 *
 *  Other status means SME is failed to update the config parameters.
 */

QDF_STATUS sme_set11dinfo(mac_handle_t mac_handle,
			  struct sme_config_params *pSmeConfigParams)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_MSG_SET_11DINFO, NO_SESSION, 0));
	if (!pSmeConfigParams) {
		sme_err("SME config params empty");
		return status;
	}

	status = csr_set_channels(mac_ctx, &pSmeConfigParams->csr_config);
	if (!QDF_IS_STATUS_SUCCESS(status))
		sme_err("csr_set_channels failed with status: %d", status);

	return status;
}

/**
 * sme_update_fine_time_measurement_capab() - Update the FTM capabitlies from
 * incoming val
 * @mac_handle: Opaque handle to the global MAC context
 * @val: New FTM capability value
 *
 * Return: None
 */
void sme_update_fine_time_measurement_capab(mac_handle_t mac_handle,
					    uint8_t session_id,
					    uint32_t val)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	QDF_STATUS status;

	ucfg_wifi_pos_set_ftm_cap(mac_ctx->psoc, val);

	if (!val) {
		mac_ctx->rrm.rrmPEContext.rrmEnabledCaps.fine_time_meas_rpt = 0;
		((tpRRMCaps)mac_ctx->rrm.rrmConfig.
			rm_capability)->fine_time_meas_rpt = 0;
	} else {
		mac_ctx->rrm.rrmPEContext.rrmEnabledCaps.fine_time_meas_rpt = 1;
		((tpRRMCaps)mac_ctx->rrm.rrmConfig.
			rm_capability)->fine_time_meas_rpt = 1;
	}

	/* Inform this RRM IE change to FW */
	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		csr_roam_update_cfg(mac_ctx, session_id,
				    REASON_CONNECT_IES_CHANGED);
		sme_release_global_lock(&mac_ctx->sme);
	}
}

void sme_update_nud_config(mac_handle_t mac_handle, uint8_t nud_fail_behavior)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	mac->nud_fail_behaviour = nud_fail_behavior;
}

/**
 * sme_update_neighbor_report_config() - Update CSR config for 11k params
 * @mac_handle: Pointer to MAC context
 * @csr_config: Pointer to CSR config data structure
 *
 * Return: None
 */
static void
sme_update_neighbor_report_config(struct mac_context *mac,
				  struct csr_config_params *csr_config)
{
	struct wlan_fwol_neighbor_report_cfg fwol_neighbor_report_cfg = {0};
	QDF_STATUS status;

	status = ucfg_fwol_get_neighbor_report_cfg(mac->psoc,
						   &fwol_neighbor_report_cfg);
	if (!QDF_IS_STATUS_SUCCESS(status))
		sme_err("Using defaults for 11K offload params: Error: %d",
			status);

	csr_config->offload_11k_enable_bitmask =
				fwol_neighbor_report_cfg.enable_bitmask;
	csr_config->neighbor_report_offload.params_bitmask =
				fwol_neighbor_report_cfg.params_bitmask;
	csr_config->neighbor_report_offload.time_offset =
				fwol_neighbor_report_cfg.time_offset;
	csr_config->neighbor_report_offload.low_rssi_offset =
				fwol_neighbor_report_cfg.low_rssi_offset;
	csr_config->neighbor_report_offload.bmiss_count_trigger =
				fwol_neighbor_report_cfg.bmiss_count_trigger;
	csr_config->neighbor_report_offload.per_threshold_offset =
				fwol_neighbor_report_cfg.per_threshold_offset;
	csr_config->neighbor_report_offload.neighbor_report_cache_timeout =
				fwol_neighbor_report_cfg.cache_timeout;
	csr_config->neighbor_report_offload.max_neighbor_report_req_cap =
				fwol_neighbor_report_cfg.max_req_cap;
}

/*
 * sme_update_config() - Change configurations for all SME moduels
 * The function updates some configuration for modules in SME, CSR, etc
 *  during SMEs close open sequence.
 * Modules inside SME apply the new configuration at the next transaction.
 * This is a synchronous call
 *
 * mac_handle - The handle returned by mac_open.
 * pSmeConfigParams - a pointer to a caller allocated object of
 *  struct sme_config_params.
 * Return QDF_STATUS_SUCCESS - SME update the config parameters successfully.
 *  Other status means SME is failed to update the config parameters.
 */
QDF_STATUS sme_update_config(mac_handle_t mac_handle,
			     struct sme_config_params *pSmeConfigParams)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_MSG_UPDATE_CONFIG, NO_SESSION,
			 0));
	if (!pSmeConfigParams) {
		sme_err("SME config params empty");
		return status;
	}
	sme_update_neighbor_report_config(mac, &pSmeConfigParams->csr_config);
	status = csr_change_default_config_param(mac, &pSmeConfigParams->
						csr_config);

	if (!QDF_IS_STATUS_SUCCESS(status))
		sme_err("csr_change_default_config_param failed status: %d",
			status);

	/* For SOC, CFG is set before start We don't want to apply global CFG
	 * in connect state because that may cause some side affect
	 */
	if (csr_is_all_session_disconnected(mac))
		csr_set_global_cfgs(mac);

	return status;
}

/**
 * sme_update_roam_params() - Store/Update the roaming params
 * @mac_handle: Opaque handle to the global MAC context
 * @session_id:               SME Session ID
 * @roam_params_src:          The source buffer to copy
 * @update_param:             Type of parameter to be updated
 *
 * Return: Return the status of the updation.
 */
QDF_STATUS sme_update_roam_params(mac_handle_t mac_handle,
				  uint8_t session_id,
				  struct roam_ext_params *roam_params_src,
				  int update_param)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct roam_ext_params *roam_params_dst;
	QDF_STATUS status;
	uint8_t i;

	roam_params_dst = &mac_ctx->roam.configParam.roam_params;
	switch (update_param) {
	case REASON_ROAM_EXT_SCAN_PARAMS_CHANGED:
		mac_ctx->mlme_cfg->lfr.rssi_boost_threshold_5g =
			roam_params_src->raise_rssi_thresh_5g;
		mac_ctx->mlme_cfg->lfr.rssi_penalize_threshold_5g =
			roam_params_src->drop_rssi_thresh_5g;
		mac_ctx->mlme_cfg->lfr.rssi_boost_factor_5g =
			roam_params_src->raise_factor_5g;
		mac_ctx->mlme_cfg->lfr.rssi_penalize_factor_5g =
			roam_params_src->drop_factor_5g;
		mac_ctx->mlme_cfg->lfr.max_rssi_boost_5g =
			roam_params_src->max_raise_rssi_5g;
		mac_ctx->mlme_cfg->lfr.max_rssi_penalize_5g =
			roam_params_src->max_drop_rssi_5g;
		roam_params_dst->alert_rssi_threshold =
			roam_params_src->alert_rssi_threshold;
		mac_ctx->mlme_cfg->lfr.enable_5g_band_pref = true;
		break;
	case REASON_ROAM_SET_SSID_ALLOWED:
		qdf_mem_zero(&roam_params_dst->ssid_allowed_list,
				sizeof(tSirMacSSid) * MAX_SSID_ALLOWED_LIST);
		roam_params_dst->num_ssid_allowed_list =
			roam_params_src->num_ssid_allowed_list;
		for (i = 0; i < roam_params_dst->num_ssid_allowed_list; i++) {
			roam_params_dst->ssid_allowed_list[i].length =
				roam_params_src->ssid_allowed_list[i].length;
			qdf_mem_copy(roam_params_dst->ssid_allowed_list[i].ssId,
				roam_params_src->ssid_allowed_list[i].ssId,
				roam_params_dst->ssid_allowed_list[i].length);
		}
		break;
	case REASON_ROAM_SET_FAVORED_BSSID:
		qdf_mem_zero(&roam_params_dst->bssid_favored,
			sizeof(tSirMacAddr) * MAX_BSSID_FAVORED);
		roam_params_dst->num_bssid_favored =
			roam_params_src->num_bssid_favored;
		for (i = 0; i < roam_params_dst->num_bssid_favored; i++) {
			qdf_mem_copy(&roam_params_dst->bssid_favored[i],
				&roam_params_src->bssid_favored[i],
				sizeof(tSirMacAddr));
			roam_params_dst->bssid_favored_factor[i] =
				roam_params_src->bssid_favored_factor[i];
		}
		break;
	case REASON_ROAM_SET_BLACKLIST_BSSID:
		qdf_mem_zero(&roam_params_dst->bssid_avoid_list,
			QDF_MAC_ADDR_SIZE * MAX_BSSID_AVOID_LIST);
		roam_params_dst->num_bssid_avoid_list =
			roam_params_src->num_bssid_avoid_list;
		for (i = 0; i < roam_params_dst->num_bssid_avoid_list; i++) {
			qdf_copy_macaddr(&roam_params_dst->bssid_avoid_list[i],
					&roam_params_src->bssid_avoid_list[i]);
		}
		break;
	case REASON_ROAM_GOOD_RSSI_CHANGED:
		roam_params_dst->good_rssi_roam =
			roam_params_src->good_rssi_roam;
		break;
	default:
		break;
	}

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		csr_roam_update_cfg(mac_ctx, session_id, update_param);
		sme_release_global_lock(&mac_ctx->sme);
	}

	return 0;
}

#ifdef WLAN_FEATURE_EXTWOW_SUPPORT

/**
 * sme_process_ready_to_ext_wow() - inform ready to ExtWoW indication.
 * @mac: Global MAC context
 * @indication: ready to Ext WoW indication from lower layer
 *
 * On getting ready to Ext WoW indication, this function calls callback
 * registered (HDD callback) with SME to inform ready to ExtWoW indication.
 *
 * Return: None
 */
static void sme_process_ready_to_ext_wow(struct mac_context *mac,
					 tpSirReadyToExtWoWInd indication)
{
	if (!mac) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_FATAL,
			  "%s: mac is null", __func__);
		return;
	}

	if (mac->readyToExtWoWCallback) {
		mac->readyToExtWoWCallback(mac->readyToExtWoWContext,
					   indication->status);
		mac->readyToExtWoWCallback = NULL;
		mac->readyToExtWoWContext = NULL;
	}

}
#endif

/*
 * sme_hdd_ready_ind() - SME sends eWNI_SME_SYS_READY_IND to PE to inform
 *  that the NIC is ready tio run.
 * The function is called by HDD at the end of initialization stage so PE/HAL
 * can enable the NIC to running state.
 * This is a synchronous call
 *
 * @mac_handle - The handle returned by mac_open.
 * Return QDF_STATUS_SUCCESS - eWNI_SME_SYS_READY_IND is sent to PE
 *				successfully.
 * Other status means SME failed to send the message to PE.
 */
QDF_STATUS sme_hdd_ready_ind(mac_handle_t mac_handle)
{
	struct sme_ready_req *msg;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_MSG_HDDREADYIND, NO_SESSION, 0));
	do {

		msg = qdf_mem_malloc(sizeof(*msg));
		if (!msg)
			return QDF_STATUS_E_NOMEM;

		msg->messageType = eWNI_SME_SYS_READY_IND;
		msg->length = sizeof(*msg);
		msg->csr_roam_synch_cb = csr_roam_synch_callback;
		msg->sme_msg_cb = sme_process_msg_callback;
		msg->stop_roaming_cb = sme_stop_roaming;
		msg->csr_roam_auth_event_handle_cb =
				csr_roam_auth_offload_callback;
		msg->csr_roam_pmkid_req_cb = csr_roam_pmkid_req_callback;

		status = u_mac_post_ctrl_msg(mac_handle, (tSirMbMsg *)msg);
		if (QDF_IS_STATUS_ERROR(status)) {
			sme_err("u_mac_post_ctrl_msg failed to send eWNI_SME_SYS_READY_IND");
			break;
		}

		status = csr_ready(mac);
		if (QDF_IS_STATUS_ERROR(status)) {
			sme_err("csr_ready failed with status: %d", status);
			break;
		}

		mac->sme.state = SME_STATE_READY;
	} while (0);

	return status;
}

#ifdef WLAN_BCN_RECV_FEATURE
QDF_STATUS
sme_register_bcn_report_pe_cb(mac_handle_t mac_handle, beacon_report_cb cb)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	if (!mac) {
		sme_err("Invalid mac context");
		return QDF_STATUS_E_INVAL;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		mac_register_bcn_report_send_cb(mac, cb);
		sme_release_global_lock(&mac->sme);
	}

	return status;
}
#endif

QDF_STATUS sme_get_valid_channels(uint32_t *ch_freq_list, uint32_t *list_len)
{
	struct mac_context *mac_ctx = sme_get_mac_context();
	uint32_t num_valid_chan;
	uint8_t i;

	if (!mac_ctx) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			FL("Invalid MAC context"));
		*list_len = 0;
		return QDF_STATUS_E_FAILURE;
	}

	num_valid_chan = mac_ctx->mlme_cfg->reg.valid_channel_list_num;

	if (num_valid_chan > *list_len) {
		sme_err("list len size %d less than expected %d", *list_len,
			num_valid_chan);
		num_valid_chan = *list_len;
	}
	*list_len = num_valid_chan;
	for (i = 0; i < *list_len; i++) {
		ch_freq_list[i] =
			mac_ctx->mlme_cfg->reg.valid_channel_freq_list[i];
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_CONV_SPECTRAL_ENABLE
static QDF_STATUS sme_register_spectral_cb(struct mac_context *mac_ctx)
{
	struct spectral_legacy_cbacks spectral_cb = {0};
	QDF_STATUS status;

	spectral_cb.vdev_get_chan_freq = sme_get_oper_chan_freq;
	spectral_cb.vdev_get_ch_width = sme_get_oper_ch_width;
	spectral_cb.vdev_get_sec20chan_freq_mhz = sme_get_sec20chan_freq_mhz;
	status = spectral_register_legacy_cb(mac_ctx->psoc, &spectral_cb);

	return status;
}
#else
static QDF_STATUS sme_register_spectral_cb(struct mac_context *mac_ctx)
{
	return QDF_STATUS_SUCCESS;
}
#endif
/*
 * sme_start() - Put all SME modules at ready state.
 *  The function starts each module in SME, PMC, CSR, etc. . Upon
 *  successfully return, all modules are ready to run.
 *  This is a synchronous call
 *
 * mac_handle - The handle returned by mac_open.
 * Return QDF_STATUS_SUCCESS - SME is ready.
 * Other status means SME is failed to start
 */
QDF_STATUS sme_start(mac_handle_t mac_handle)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct policy_mgr_sme_cbacks sme_cbacks;

	do {
		status = csr_start(mac);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("csr_start failed status: %d", status);
			break;
		}
		sme_cbacks.sme_get_nss_for_vdev = sme_get_vdev_type_nss;
		sme_cbacks.sme_nss_update_request = sme_nss_update_request;
		sme_cbacks.sme_pdev_set_hw_mode = sme_pdev_set_hw_mode;
		sme_cbacks.sme_soc_set_dual_mac_config =
			sme_soc_set_dual_mac_config;
		sme_cbacks.sme_change_mcc_beacon_interval =
			sme_change_mcc_beacon_interval;
		sme_cbacks.sme_get_ap_channel_from_scan =
			sme_get_ap_channel_from_scan;
		sme_cbacks.sme_scan_result_purge = sme_scan_result_purge;
		sme_cbacks.sme_rso_start_cb = sme_start_roaming;
		sme_cbacks.sme_rso_stop_cb = sme_stop_roaming;
		status = policy_mgr_register_sme_cb(mac->psoc, &sme_cbacks);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("Failed to register sme cb with Policy Manager: %d",
				status);
			break;
		}
		sme_register_spectral_cb(mac);
		mac->sme.state = SME_STATE_START;

		/* START RRM */
		status = rrm_start(mac);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("Failed to start RRM");
			break;
		}
	} while (0);
	return status;
}

static QDF_STATUS dfs_msg_processor(struct mac_context *mac,
		struct scheduler_msg *msg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_info *roam_info;
	tSirSmeCSAIeTxCompleteRsp *csa_ie_tx_complete_rsp;
	uint32_t session_id = 0;
	eRoamCmdStatus roam_status;
	eCsrRoamResult roam_result;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_NOMEM;

	switch (msg->type) {
	case eWNI_SME_DFS_RADAR_FOUND:
	{
		session_id = policy_mgr_get_dfs_beaconing_session_id(mac->psoc);
		if (!CSR_IS_SESSION_VALID(mac, session_id)) {
			sme_err("CSR session not valid: %d", session_id);
			qdf_mem_free(roam_info);
			return QDF_STATUS_E_FAILURE;
		}
		roam_status = eCSR_ROAM_DFS_RADAR_IND;
		roam_result = eCSR_ROAM_RESULT_DFS_RADAR_FOUND_IND;
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "sapdfs: Radar indication event occurred");
		break;
	}
	case eWNI_SME_DFS_CSAIE_TX_COMPLETE_IND:
	{
		csa_ie_tx_complete_rsp =
			(tSirSmeCSAIeTxCompleteRsp *) msg->bodyptr;
		if (!csa_ie_tx_complete_rsp) {
			sme_err("eWNI_SME_DFS_CSAIE_TX_COMPLETE_IND null msg");
			qdf_mem_free(roam_info);
			return QDF_STATUS_E_FAILURE;
		}
		session_id = csa_ie_tx_complete_rsp->sessionId;
		roam_status = eCSR_ROAM_DFS_CHAN_SW_NOTIFY;
		roam_result = eCSR_ROAM_RESULT_DFS_CHANSW_UPDATE_SUCCESS;
		break;
	}
	case eWNI_SME_DFS_CAC_COMPLETE:
	{
		session_id = msg->bodyval;
		roam_status = eCSR_ROAM_CAC_COMPLETE_IND;
		roam_result = eCSR_ROAM_RESULT_CAC_END_IND;
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "sapdfs: Received eWNI_SME_DFS_CAC_COMPLETE vdevid%d",
			  session_id);
		break;
	}
	case eWNI_SME_CSA_RESTART_RSP:
	{
		session_id = msg->bodyval;
		roam_status = 0;
		roam_result = eCSR_ROAM_RESULT_CSA_RESTART_RSP;
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "sapdfs: Received eCSR_ROAM_RESULT_DFS_CHANSW_UPDATE_REQ vdevid%d",
			  session_id);
		break;
	}
	default:
	{
		sme_err("Invalid DFS message: 0x%x", msg->type);
		qdf_mem_free(roam_info);
		status = QDF_STATUS_E_FAILURE;
		return status;
	}
	}

	/* Indicate Radar Event to SAP */
	csr_roam_call_callback(mac, session_id, roam_info, 0,
			       roam_status, roam_result);
	qdf_mem_free(roam_info);
	return status;
}


#ifdef WLAN_FEATURE_11W
/*
 * Handle the unprotected management frame indication from LIM and
 * forward it to HDD.
 */
static QDF_STATUS
sme_unprotected_mgmt_frm_ind(struct mac_context *mac,
			     tpSirSmeUnprotMgmtFrameInd pSmeMgmtFrm)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_info *roam_info;
	uint32_t SessionId = pSmeMgmtFrm->sessionId;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_NOMEM;

	roam_info->nFrameLength = pSmeMgmtFrm->frameLen;
	roam_info->pbFrames = pSmeMgmtFrm->frameBuf;
	roam_info->frameType = pSmeMgmtFrm->frameType;

	/* forward the mgmt frame to HDD */
	csr_roam_call_callback(mac, SessionId, roam_info, 0,
			       eCSR_ROAM_UNPROT_MGMT_FRAME_IND, 0);

	qdf_mem_free(roam_info);

	return status;
}
#endif

QDF_STATUS sme_update_new_channel_event(mac_handle_t mac_handle,
					uint8_t session_id)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct csr_roam_info *roamInfo;
	eRoamCmdStatus roamStatus;
	eCsrRoamResult roamResult;

	roamInfo = qdf_mem_malloc(sizeof(*roamInfo));
	if (!roamInfo)
		return QDF_STATUS_E_FAILURE;

	roamInfo->dfs_event.sessionId = session_id;
	roamStatus = eCSR_ROAM_CHANNEL_COMPLETE_IND;
	roamResult = eCSR_ROAM_RESULT_DFS_RADAR_FOUND_IND;
	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		  "sapdfs: Updated new channel event");

	/* Indicate channel Event to SAP */
	csr_roam_call_callback(mac, session_id, roamInfo, 0,
			       roamStatus, roamResult);

	qdf_mem_free(roamInfo);
	return status;
}


/**
 * sme_extended_change_channel_ind()- function to indicate ECSA
 * action frame is received in lim to SAP
 * @mac_ctx:  pointer to global mac structure
 * @msg_buf: contain new channel and session id.
 *
 * This function is called to post ECSA action frame
 * receive event to SAP.
 *
 * Return: success if msg indicated to SAP else return failure
 */
static QDF_STATUS sme_extended_change_channel_ind(struct mac_context *mac_ctx,
						void *msg_buf)
{
	struct sir_sme_ext_cng_chan_ind *ext_chan_ind;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t session_id = 0;
	struct csr_roam_info *roam_info;
	eRoamCmdStatus roam_status;
	eCsrRoamResult roam_result;

	ext_chan_ind = msg_buf;
	if (!ext_chan_ind) {
		sme_err("ext_chan_ind is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_NOMEM;

	session_id = ext_chan_ind->session_id;
	roam_info->target_chan_freq = ext_chan_ind->new_chan_freq;
	roam_status = eCSR_ROAM_EXT_CHG_CHNL_IND;
	roam_result = eCSR_ROAM_EXT_CHG_CHNL_UPDATE_IND;
	sme_debug("sapdfs: Received eWNI_SME_EXT_CHANGE_CHANNEL_IND for session id [%d]",
		 session_id);

	/* Indicate Ext Channel Change event to SAP */
	csr_roam_call_callback(mac_ctx, session_id, roam_info, 0,
			       roam_status, roam_result);
	qdf_mem_free(roam_info);
	return status;
}

#ifdef FEATURE_WLAN_ESE
/**
 * sme_update_is_ese_feature_enabled() - enable/disable ESE support at runtime
 * @mac_handle: Opaque handle to the global MAC context
 * @sessionId: session id
 * @isEseIniFeatureEnabled: ese ini enabled
 *
 * It is used at in the REG_DYNAMIC_VARIABLE macro definition of
 * isEseIniFeatureEnabled. This is a synchronous call
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS sme_update_is_ese_feature_enabled(mac_handle_t mac_handle,
			uint8_t sessionId, const bool isEseIniFeatureEnabled)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status;

	if (mac->mlme_cfg->lfr.ese_enabled ==
	    isEseIniFeatureEnabled) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "%s: ESE Mode is already enabled or disabled, nothing to do (returning) old(%d) new(%d)",
			  __func__,
			  mac->mlme_cfg->lfr.ese_enabled,
			  isEseIniFeatureEnabled);
		return QDF_STATUS_SUCCESS;
	}

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		  "%s: EseEnabled is changed from %d to %d", __func__,
		  mac->mlme_cfg->lfr.ese_enabled,
		  isEseIniFeatureEnabled);
	mac->mlme_cfg->lfr.ese_enabled = isEseIniFeatureEnabled;
	csr_neighbor_roam_update_fast_roaming_enabled(
			mac, sessionId, isEseIniFeatureEnabled);

	if (true == isEseIniFeatureEnabled)
		mac->mlme_cfg->lfr.fast_transition_enabled = true;

	if (mac->mlme_cfg->lfr.roam_scan_offload_enabled) {
		status = sme_acquire_global_lock(&mac->sme);
		if (QDF_IS_STATUS_SUCCESS(status)) {
			csr_roam_update_cfg(mac, sessionId,
					    REASON_ESE_INI_CFG_CHANGED);
			sme_release_global_lock(&mac->sme);
		} else {
			return status;
		}
	}
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sme_set_plm_request(mac_handle_t mac_handle,
			       struct plm_req_params *req)
{
	QDF_STATUS status;
	bool ret = false;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	uint32_t ch_freq_list[CFG_VALID_CHANNEL_LIST_LEN] = { 0 };
	uint8_t count, valid_count = 0;
	struct scheduler_msg msg = {0};
	struct csr_roam_session *session;
	struct plm_req_params *body;
	uint32_t ch_freq;

	if (!req)
		return QDF_STATUS_E_FAILURE;

	status = sme_acquire_global_lock(&mac->sme);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;

	session = CSR_GET_SESSION(mac, req->vdev_id);
	if (!session) {
		sme_err("session %d not found", req->vdev_id);
		sme_release_global_lock(&mac->sme);
		return QDF_STATUS_E_FAILURE;
	}

	if (!session->sessionActive) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Invalid Sessionid"));
		sme_release_global_lock(&mac->sme);
		return QDF_STATUS_E_FAILURE;
	}

	/* per contract must make a copy of the params when messaging */
	body = qdf_mem_malloc(sizeof(*body));

	if (!body) {
		sme_release_global_lock(&mac->sme);
		return QDF_STATUS_E_NOMEM;
	}

	*body = *req;

	if (!body->enable)
		goto send_plm_start;
	/* validating channel numbers */
	for (count = 0; count < body->plm_num_ch; count++) {
		ch_freq = body->plm_ch_freq_list[count];
		ret = csr_is_supported_channel(mac, ch_freq);
		if (!ret) {
			/* Not supported, ignore the channel */
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
				  FL("Unsupported freq %d ignored for PLM"),
				  ch_freq);
			continue;
		}

		if (ch_freq > 2477) {
			enum channel_state state =
				wlan_reg_get_channel_state_for_freq(
					mac->pdev, ch_freq);

			if (state == CHANNEL_STATE_DFS) {
				/* DFS channel is provided, no PLM bursts can be
				 * transmitted. Ignoring these channels.
				 */
				QDF_TRACE(QDF_MODULE_ID_SME,
					  QDF_TRACE_LEVEL_DEBUG,
					  FL("DFS channel %d ignored for PLM"),
					  ch_freq);
				continue;
			}
		}
		ch_freq_list[valid_count++] = ch_freq;
	} /* End of for () */

	/* Copying back the valid channel list to plm struct */
	qdf_mem_zero(body->plm_ch_freq_list, body->plm_num_ch);
	if (valid_count)
		qdf_mem_copy(body->plm_ch_freq_list, ch_freq_list, valid_count);
	/* All are invalid channels, FW need to send the PLM
	 *  report with "incapable" bit set.
	 */
	body->plm_num_ch = valid_count;

send_plm_start:
	/* PLM START */
	msg.type = WMA_SET_PLM_REQ;
	msg.reserved = 0;
	msg.bodyptr = body;

	if (!QDF_IS_STATUS_SUCCESS(scheduler_post_message(QDF_MODULE_ID_SME,
							  QDF_MODULE_ID_WMA,
							  QDF_MODULE_ID_WMA,
							  &msg))) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Not able to post WMA_SET_PLM_REQ to WMA"));
		sme_release_global_lock(&mac->sme);
		qdf_mem_free(body);
		return QDF_STATUS_E_FAILURE;
	}

	sme_release_global_lock(&mac->sme);
	return QDF_STATUS_SUCCESS;
}

/**
 * sme_tsm_ie_ind() - sme tsm ie indication
 * @mac: Global mac context
 * @pSmeTsmIeInd: Pointer to tsm ie indication
 *
 * Handle the tsm ie indication from  LIM and forward it to HDD.
 *
 * Return: QDF_STATUS enumeration
 */
static QDF_STATUS sme_tsm_ie_ind(struct mac_context *mac,
				 struct tsm_ie_ind *pSmeTsmIeInd)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_info *roam_info;
	uint32_t SessionId = pSmeTsmIeInd->sessionId;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_NOMEM;

	roam_info->tsm_ie.tsid = pSmeTsmIeInd->tsm_ie.tsid;
	roam_info->tsm_ie.state = pSmeTsmIeInd->tsm_ie.state;
	roam_info->tsm_ie.msmt_interval = pSmeTsmIeInd->tsm_ie.msmt_interval;
	/* forward the tsm ie information to HDD */
	csr_roam_call_callback(mac, SessionId, roam_info, 0,
			       eCSR_ROAM_TSM_IE_IND, 0);
	qdf_mem_free(roam_info);
	return status;
}

/**
 * sme_set_cckm_ie() - set cckm ie
 * @mac_handle: Opaque handle to the global MAC context
 * @sessionId: session id
 * @pCckmIe: Pointer to CCKM Ie
 * @cckmIeLen: Length of @pCckmIe
 *
 * Function to store the CCKM IE passed from supplicant and use
 * it while packing reassociation request.
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS sme_set_cckm_ie(mac_handle_t mac_handle, uint8_t sessionId,
			   uint8_t *pCckmIe, uint8_t cckmIeLen)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		csr_set_cckm_ie(mac, sessionId, pCckmIe, cckmIeLen);
		sme_release_global_lock(&mac->sme);
	}
	return status;
}

/**
 * sme_set_ese_beacon_request() - set ese beacon request
 * @mac_handle: Opaque handle to the global MAC context
 * @sessionId: session id
 * @in_req: Ese beacon report request
 *
 * function to set ESE beacon request parameters
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS sme_set_ese_beacon_request(mac_handle_t mac_handle,
				      const uint8_t sessionId,
				      const tCsrEseBeaconReq *in_req)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	tpSirBeaconReportReqInd sme_bcn_rpt_req = NULL;
	const tCsrEseBeaconReqParams *bcn_req = NULL;
	uint8_t counter = 0;
	struct csr_roam_session *session = CSR_GET_SESSION(mac, sessionId);
	tpRrmSMEContext sme_rrm_ctx = &mac->rrm.rrmSmeContext[0];

	if (sme_rrm_ctx->eseBcnReqInProgress == true) {
		sme_err("A Beacon Report Req is already in progress");
		return QDF_STATUS_E_RESOURCES;
	}

	/* Store the info in RRM context */
	qdf_mem_copy(&sme_rrm_ctx->eseBcnReqInfo, in_req,
		     sizeof(tCsrEseBeaconReq));

	/* Prepare the request to send to SME. */
	sme_bcn_rpt_req = qdf_mem_malloc(sizeof(tSirBeaconReportReqInd));
	if (!sme_bcn_rpt_req)
		return QDF_STATUS_E_NOMEM;

	sme_rrm_ctx->eseBcnReqInProgress = true;

	sme_debug("Sending Beacon Report Req to SME");

	sme_bcn_rpt_req->messageType = eWNI_SME_BEACON_REPORT_REQ_IND;
	sme_bcn_rpt_req->length = sizeof(tSirBeaconReportReqInd);
	qdf_mem_copy(sme_bcn_rpt_req->bssId,
		     session->connectedProfile.bssid.bytes,
		     sizeof(tSirMacAddr));
	sme_bcn_rpt_req->channel_info.chan_num = 255;
	sme_bcn_rpt_req->channel_list.num_channels = in_req->numBcnReqIe;
	sme_bcn_rpt_req->msgSource = eRRM_MSG_SOURCE_ESE_UPLOAD;
	sme_bcn_rpt_req->measurement_idx = 0;

	for (counter = 0; counter < in_req->numBcnReqIe; counter++) {
		bcn_req = &in_req->bcnReq[counter];
		sme_bcn_rpt_req->fMeasurementtype[counter] =
			bcn_req->scanMode;
		sme_bcn_rpt_req->measurementDuration[counter] =
			SYS_TU_TO_MS(bcn_req->measurementDuration);
		sme_bcn_rpt_req->channel_list.chan_freq_lst[counter] =
			bcn_req->ch_freq;
	}

	status = sme_rrm_process_beacon_report_req_ind(mac, sme_bcn_rpt_req);

	if (status != QDF_STATUS_SUCCESS)
		sme_rrm_ctx->eseBcnReqInProgress = false;

	qdf_mem_free(sme_bcn_rpt_req);

	return status;
}

/**
 * sme_get_tsm_stats() - SME get tsm stats
 * @mac_handle: Opaque handle to the global MAC context
 * @callback: SME sends back the requested stats using the callback
 * @staId: The station ID for which the stats is requested for
 * @bssId: bssid
 * @pContext: user context to be passed back along with the callback
 * @tid: Traffic id
 *
 * API register a callback to get TSM Stats.
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS sme_get_tsm_stats(mac_handle_t mac_handle,
			     tCsrTsmStatsCallback callback,
			     struct qdf_mac_addr bssId,
			     void *pContext, uint8_t tid)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = csr_get_tsm_stats(mac, callback,
					   bssId, pContext,
					   tid);
		sme_release_global_lock(&mac->sme);
	}
	return status;
}

/**
 * sme_set_ese_roam_scan_channel_list() - To set ese roam scan channel list
 * @mac_handle: Opaque handle to the global MAC context
 * @sessionId: sme session id
 * @chan_freq_list: Output channel list
 * @numChannels: Output number of channels
 *
 * This routine is called to set ese roam scan channel list.
 * This is a synchronous call
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_set_ese_roam_scan_channel_list(mac_handle_t mac_handle,
					      uint8_t sessionId,
					      uint32_t *chan_freq_list,
					      uint8_t numChannels)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo = NULL;
	tpCsrChannelInfo curchnl_list_info = NULL;
	uint8_t oldChannelList[CFG_VALID_CHANNEL_LIST_LEN * 5] = { 0 };
	uint8_t newChannelList[CFG_VALID_CHANNEL_LIST_LEN * 5] = { 0 };
	uint8_t i = 0, j = 0;
	enum band_info band = -1;
	uint32_t band_bitmap;

	if (sessionId >= WLAN_MAX_VDEVS) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Invalid sme session id: %d"), sessionId);
		return QDF_STATUS_E_INVAL;
	}

	pNeighborRoamInfo = &mac->roam.neighborRoamInfo[sessionId];
	curchnl_list_info =
		&pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo;

	status = sme_acquire_global_lock(&mac->sme);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;

	if (curchnl_list_info->freq_list) {
		for (i = 0; i < curchnl_list_info->numOfChannels; i++) {
			j += snprintf(oldChannelList + j,
				sizeof(oldChannelList) - j, "%d",
				curchnl_list_info->freq_list[i]);
		}
	}
	ucfg_reg_get_band(mac->pdev, &band_bitmap);
	band = wlan_reg_band_bitmap_to_band_info(band_bitmap);
	status = csr_create_roam_scan_channel_list(mac, sessionId,
				chan_freq_list, numChannels,
				band);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (curchnl_list_info->freq_list) {
			j = 0;
			for (i = 0; i < curchnl_list_info->numOfChannels; i++) {
				j += snprintf(newChannelList + j,
					sizeof(newChannelList) - j, "%d",
					curchnl_list_info->freq_list[i]);
			}
		}
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			"ESE roam scan chnl list successfully set to %s-old value is %s-roam state is %d",
			newChannelList, oldChannelList,
			pNeighborRoamInfo->neighborRoamState);
	}

	if (mac->mlme_cfg->lfr.roam_scan_offload_enabled)
		csr_roam_update_cfg(mac, sessionId,
				    REASON_CHANNEL_LIST_CHANGED);

	sme_release_global_lock(&mac->sme);
	return status;
}

#endif /* FEATURE_WLAN_ESE */

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
QDF_STATUS sme_get_roam_scan_ch(mac_handle_t mac_handle,
				uint8_t vdev_id, void *pcontext)
{
	struct scheduler_msg msg = {0};
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_E_FAILURE;

	msg.type = WMA_ROAM_SCAN_CH_REQ;
	msg.bodyval = vdev_id;
	mac->sme.roam_scan_ch_get_context = pcontext;

	if (scheduler_post_message(QDF_MODULE_ID_SME,
				   QDF_MODULE_ID_WMA,
				   QDF_MODULE_ID_WMA,
				   &msg)) {
		sme_err("Posting message %d failed",
			WMA_ROAM_SCAN_CH_REQ);
		mac->sme.roam_scan_ch_get_context = NULL;
		sme_release_global_lock(&mac->sme);
		return QDF_STATUS_E_FAILURE;
	}

	sme_release_global_lock(&mac->sme);
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * sme_process_dual_mac_config_resp() - Process set Dual mac config response
 * @mac: Global MAC pointer
 * @msg: Dual mac config response
 *
 * Processes the dual mac configuration response and invokes the HDD callback
 * to process further
 */
static QDF_STATUS sme_process_dual_mac_config_resp(struct mac_context *mac,
		uint8_t *msg)
{
	tListElem *entry = NULL;
	tSmeCmd *command = NULL;
	bool found;
	dual_mac_cb callback = NULL;
	struct sir_dual_mac_config_resp *param;

	param = (struct sir_dual_mac_config_resp *)msg;
	if (!param) {
		sme_err("Dual mac config resp param is NULL");
		/* Not returning. Need to check if active command list
		 * needs to be freed
		 */
	}

	entry = csr_nonscan_active_ll_peek_head(mac, LL_ACCESS_LOCK);
	if (!entry) {
		sme_err("No cmd found in active list");
		return QDF_STATUS_E_FAILURE;
	}

	command = GET_BASE_ADDR(entry, tSmeCmd, Link);
	if (!command) {
		sme_err("Base address is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (e_sme_command_set_dual_mac_config != command->command) {
		sme_err("Command mismatch!");
		return QDF_STATUS_E_FAILURE;
	}

	callback = command->u.set_dual_mac_cmd.set_dual_mac_cb;
	if (callback) {
		if (!param) {
			sme_err("Callback failed-Dual mac config is NULL");
		} else {
			sme_debug("Calling HDD callback for Dual mac config");
			callback(param->status,
				command->u.set_dual_mac_cmd.scan_config,
				command->u.set_dual_mac_cmd.fw_mode_config);
		}
	} else {
		sme_err("Callback does not exist");
	}

	found = csr_nonscan_active_ll_remove_entry(mac, entry, LL_ACCESS_LOCK);
	if (found)
		/* Now put this command back on the available command list */
		csr_release_command(mac, command);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
QDF_STATUS sme_set_roam_scan_ch_event_cb(mac_handle_t mac_handle,
					 sme_get_raom_scan_ch_callback cb)
{
	QDF_STATUS qdf_status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	qdf_status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		return qdf_status;

	mac->sme.roam_scan_ch_callback = cb;
	sme_release_global_lock(&mac->sme);

	return qdf_status;
}

/**
 * sme_process_roam_scan_ch_list_resp() - Process get roam scan ch list
 * response
 * @mac: Global MAC pointer
 * @msgbuf: pointer to roam scan ch list response
 *
 * This function checks the roam scan chan list message is for command
 * response or a async event and accordingly data is given to user space.
 * callback to process further
 */
static void
sme_process_roam_scan_ch_list_resp(struct mac_context *mac,
				   struct roam_scan_ch_resp *roam_ch)
{
	sme_get_raom_scan_ch_callback callback =
				mac->sme.roam_scan_ch_callback;

	if (!roam_ch)
		return;

	if (callback)
		callback(mac->hdd_handle, roam_ch,
			 mac->sme.roam_scan_ch_get_context);
}
#else
static void
sme_process_roam_scan_ch_list_resp(tpAniSirGlobal mac,
				   struct roam_scan_ch_resp *roam_ch)
{
}
#endif

/**
 * sme_process_antenna_mode_resp() - Process set antenna mode
 * response
 * @mac: Global MAC pointer
 * @msg: antenna mode response
 *
 * Processes the antenna mode response and invokes the HDD
 * callback to process further
 */
static QDF_STATUS sme_process_antenna_mode_resp(struct mac_context *mac,
		uint8_t *msg)
{
	tListElem *entry;
	tSmeCmd *command;
	bool found;
	void *context = NULL;
	antenna_mode_cb callback;
	struct sir_antenna_mode_resp *param;

	param = (struct sir_antenna_mode_resp *)msg;
	if (!param)
		sme_err("set antenna mode resp is NULL");
		/* Not returning. Need to check if active command list
		 * needs to be freed
		 */

	entry = csr_nonscan_active_ll_peek_head(mac, LL_ACCESS_LOCK);
	if (!entry) {
		sme_err("No cmd found in active list");
		return QDF_STATUS_E_FAILURE;
	}

	command = GET_BASE_ADDR(entry, tSmeCmd, Link);
	if (!command) {
		sme_err("Base address is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (e_sme_command_set_antenna_mode != command->command) {
		sme_err("Command mismatch!");
		return QDF_STATUS_E_FAILURE;
	}

	context = command->u.set_antenna_mode_cmd.set_antenna_mode_ctx;
	callback = command->u.set_antenna_mode_cmd.set_antenna_mode_resp;
	if (callback) {
		if (!param)
			sme_err("Set antenna mode call back is NULL");
		else
			callback(param->status, context);
	} else {
		sme_err("Callback does not exist");
	}

	found = csr_nonscan_active_ll_remove_entry(mac, entry, LL_ACCESS_LOCK);
	if (found)
		/* Now put this command back on the available command list */
		csr_release_command(mac, command);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_SUPPORT_TWT
/**
 * sme_sap_twt_is_command_in_progress() - Based on the input peer mac address
 * invoke the appropriate function to check if the given command is in progress
 * @psoc: Pointer to psoc object
 * @vdev_id: Vdev id
 * @peer_mac: Peer MAC address
 * @dialog_id: Dialog id
 * @cmd: command
 *
 * If the input @peer_mac is a broadcast MAC address then the expectation is
 * to iterate through the list of all peers and check for any given @dialog_id
 * if the command @cmd is in progress.
 * Note: If @peer_mac is broadcast MAC address then @dialog_id shall always
 * be WLAN_ALL_SESSIONS_DIALOG_ID.
 * For ex: If TWT teardown command is issued on a particular @dialog_id and
 * non-broadcast peer mac and FW response is not yet received then for that
 * particular @dialog_id and @peer_mac, TWT teardown is the active command,
 * then if the driver receives another TWT teardown request with broadcast
 * peer mac, then API mlme_twt_any_peer_cmd_in_progress() shall iterate
 * through the list of all peers and returns command in progress as true.
 *
 * If the input @peer_mac is a non-broadcast MAC address then
 * mlme_sap_twt_peer_is_cmd_in_progress() shall check only for that
 * particular @peer_mac and @dialog_id.
 *
 * Return: true if command is in progress, false otherwise
 */
static bool
sme_sap_twt_is_command_in_progress(struct wlan_objmgr_psoc *psoc,
				   uint8_t vdev_id,
				   struct qdf_mac_addr *peer_mac,
				   uint8_t dialog_id,
				   enum wlan_twt_commands cmd)
{
	if (qdf_is_macaddr_broadcast(peer_mac)) {
		return mlme_twt_any_peer_cmd_in_progress(psoc, vdev_id,
							 dialog_id, cmd);
	} else {
		return mlme_sap_twt_peer_is_cmd_in_progress(psoc, peer_mac,
							    dialog_id, cmd);
	}
}

/**
 * sme_sap_add_twt_session() - Based on the input peer mac address
 * invoke the appropriate function to add dialog_id to the TWT session context
 * @psoc: Pointer to psoc object
 * @vdev_id: Vdev id
 * @peer_mac: Peer MAC address
 * @dialog_id: Dialog id
 *
 * If the input @peer_mac is a broadcast MAC address then there is nothing
 * to do, because the initialized structure is already in the expected format
 * Note: If @peer_mac is broadcast MAC address then @dialog_id shall always
 * be WLAN_ALL_SESSIONS_DIALOG_ID.
 *
 * If the input @peer_mac is a non-broadcast MAC address then
 * mlme_add_twt_session() shall add the @dialog_id to the @peer_mac
 * TWT session context.
 *
 * Return: None
 */
static void
sme_sap_add_twt_session(struct wlan_objmgr_psoc *psoc,
			uint8_t vdev_id,
			struct qdf_mac_addr *peer_mac,
			uint8_t dialog_id)
{
	if (!qdf_is_macaddr_broadcast(peer_mac))
		mlme_add_twt_session(psoc, peer_mac, dialog_id);
}

/**
 * sme_sap_set_twt_command_in_progress() - Based on the input peer mac address
 * invoke the appropriate function to set the command is in progress
 * @psoc: Pointer to psoc object
 * @vdev_id: Vdev id
 * @peer_mac: Peer MAC address
 * @dialog_id: Dialog id
 * @cmd: command
 *
 * If the input @peer_mac is a broadcast MAC address then the expectation is
 * to iterate through the list of all peers and set the active command to @cmd
 * for the given @dialog_id
 * Note: If @peer_mac is broadcast MAC address then @dialog_id shall always
 * be WLAN_ALL_SESSIONS_DIALOG_ID.
 * For ex: If TWT teardown command is issued on broadcast @peer_mac, then
 * it is same as issuing TWT teardown for all the peers (all TWT sessions).
 * Invoking mlme_sap_set_twt_all_peers_cmd_in_progress() shall iterate through
 * all the peers and set the active command to @cmd.
 *
 * If the input @peer_mac is a non-broadcast MAC address then
 * mlme_set_twt_command_in_progress() shall set the active command to @cmd
 * only for that particular @peer_mac and @dialog_id.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
sme_sap_set_twt_command_in_progress(struct wlan_objmgr_psoc *psoc,
				    uint8_t vdev_id,
				    struct qdf_mac_addr *peer_mac,
				    uint8_t dialog_id,
				    enum wlan_twt_commands cmd)
{
	if (qdf_is_macaddr_broadcast(peer_mac)) {
		return mlme_sap_set_twt_all_peers_cmd_in_progress(psoc,
								  vdev_id,
								  dialog_id,
								  cmd);
	} else {
		return mlme_set_twt_command_in_progress(psoc, peer_mac,
							dialog_id, cmd);
	}
}

/**
 * sme_sap_init_twt_context() - Based on the input peer mac address
 * invoke the appropriate function to initialize the TWT session context
 * @psoc: Pointer to psoc object
 * @vdev_id: Vdev id
 * @peer_mac: Peer MAC address
 * @dialog_id: Dialog id
 *
 * If the input @peer_mac is a broadcast MAC address then the expectation is
 * to iterate through the list of all peers and initialize the TWT session
 * context
 * Note: If @peer_mac is broadcast MAC address then @dialog_id shall always
 * be WLAN_ALL_SESSIONS_DIALOG_ID.
 * For ex: If TWT teardown command is issued on broadcast @peer_mac, then
 * it is same as issuing TWT teardown for all the peers (all TWT sessions).
 * Then active command for all the peers is set to @WLAN_TWT_TERMINATE.
 * Upon receiving the TWT teardown WMI event, mlme_init_all_peers_twt_context()
 * shall iterate through the list of all peers and initializes the TWT session
 * context back to its initial state.
 *
 * If the input @peer_mac is a non-broadcast MAC address then
 * mlme_init_twt_context() shall initialize the TWT session context
 * only for that particular @peer_mac and @dialog_id.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
sme_sap_init_twt_context(struct wlan_objmgr_psoc *psoc,
			 uint8_t vdev_id,
			 struct qdf_mac_addr *peer_mac,
			 uint8_t dialog_id)
{
	if (qdf_is_macaddr_broadcast(peer_mac)) {
		return mlme_init_all_peers_twt_context(psoc, vdev_id,
						       dialog_id);
	} else {
		return mlme_init_twt_context(psoc, peer_mac, dialog_id);
	}
}

/**
 * sme_process_twt_add_renego_failure() - Process TWT re-negotiation failure
 *
 * @mac: Global MAC pointer
 * @add_dialog_event: pointer to event buf containing twt response parameters
 *
 * Return: None
 */
static void
sme_process_twt_add_renego_failure(struct mac_context *mac,
				 struct twt_add_dialog_complete_event *add_dialog_event)
{
	twt_add_dialog_cb callback;

	/* Reset the active TWT command to none */
	mlme_set_twt_command_in_progress(
		mac->psoc,
		(struct qdf_mac_addr *)add_dialog_event->params.peer_macaddr,
		add_dialog_event->params.dialog_id, WLAN_TWT_NONE);

	callback = mac->sme.twt_add_dialog_cb;
	if (callback)
		callback(mac->psoc, add_dialog_event, true);
}

/**
 * sme_process_twt_add_initial_nego() - Process initial TWT setup or
 * re-negotiation successful setup
 * @mac: Global MAC pointer
 * @add_dialog_event: pointer to event buf containing twt response parameters
 *
 * Return: None
 */
static void
sme_process_twt_add_initial_nego(struct mac_context *mac,
				 struct twt_add_dialog_complete_event *add_dialog_event)
{
	twt_add_dialog_cb callback;

	callback = mac->sme.twt_add_dialog_cb;
	if (callback)
		callback(mac->psoc, add_dialog_event, false);

	/* Reset the active TWT command to none */
	mlme_set_twt_command_in_progress(
		mac->psoc,
		(struct qdf_mac_addr *)add_dialog_event->params.peer_macaddr,
		add_dialog_event->params.dialog_id, WLAN_TWT_NONE);

	if (add_dialog_event->params.status) {
		/* Clear the stored TWT dialog ID as TWT setup failed */
		ucfg_mlme_init_twt_context(mac->psoc, (struct qdf_mac_addr *)
					   add_dialog_event->params.peer_macaddr,
					   add_dialog_event->params.dialog_id);
		return;
	}

	ucfg_mlme_set_twt_setup_done(mac->psoc, (struct qdf_mac_addr *)
				     add_dialog_event->params.peer_macaddr,
				     add_dialog_event->params.dialog_id, true);

	ucfg_mlme_set_twt_session_state(
		mac->psoc,
		(struct qdf_mac_addr *)add_dialog_event->params.peer_macaddr,
		add_dialog_event->params.dialog_id,
		WLAN_TWT_SETUP_STATE_ACTIVE);
}

/**
 * sme_process_twt_add_dialog_event() - Process twt add dialog event
 * response from firmware
 * @mac: Global MAC pointer
 * @add_dialog_event: pointer to event buf containing twt response parameters
 *
 * Return: None
 */
static void
sme_process_twt_add_dialog_event(struct mac_context *mac,
				 struct twt_add_dialog_complete_event
				 *add_dialog_event)
{
	bool is_evt_allowed;
	bool setup_done;
	enum WMI_HOST_ADD_TWT_STATUS status = add_dialog_event->params.status;
	enum wlan_twt_commands active_cmd = WLAN_TWT_NONE;
	enum QDF_OPMODE opmode;
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	twt_add_dialog_cb callback;

	opmode = wlan_get_opmode_from_vdev_id(mac->pdev,
					      add_dialog_event->params.vdev_id);

	qdf_status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		return;

	switch (opmode) {
	case QDF_SAP_MODE:
		callback = mac->sme.twt_add_dialog_cb;
		if (callback)
			callback(mac->psoc, add_dialog_event, false);
		break;
	case QDF_STA_MODE:
		is_evt_allowed = mlme_twt_is_command_in_progress(
					mac->psoc, (struct qdf_mac_addr *)
					add_dialog_event->params.peer_macaddr,
					add_dialog_event->params.dialog_id,
					WLAN_TWT_SETUP,	&active_cmd);

		if (!is_evt_allowed) {
			sme_debug("Drop TWT add dialog event for dialog_id:%d status:%d active_cmd:%d",
				  add_dialog_event->params.dialog_id, status,
				  active_cmd);
			sme_release_global_lock(&mac->sme);
			return;
		}

		setup_done = ucfg_mlme_is_twt_setup_done(
					mac->psoc, (struct qdf_mac_addr *)
					add_dialog_event->params.peer_macaddr,
					add_dialog_event->params.dialog_id);
		sme_debug("setup_done:%d status:%d", setup_done, status);

		if (setup_done && status) {
			/*This is re-negotiation failure case */
			sme_process_twt_add_renego_failure(mac,
							   add_dialog_event);
		} else {
			sme_process_twt_add_initial_nego(mac,
							 add_dialog_event);
		}
		break;
	default:
		sme_debug("TWT Setup is not supported on %s",
			  qdf_opmode_str(opmode));
	}

	sme_release_global_lock(&mac->sme);
	return;
}

static bool
sme_is_twt_teardown_failed(enum WMI_HOST_DEL_TWT_STATUS teardown_status)
{
	switch (teardown_status) {
	case WMI_HOST_DEL_TWT_STATUS_DIALOG_ID_NOT_EXIST:
	case WMI_HOST_DEL_TWT_STATUS_INVALID_PARAM:
	case WMI_HOST_DEL_TWT_STATUS_DIALOG_ID_BUSY:
	case WMI_HOST_DEL_TWT_STATUS_NO_RESOURCE:
	case WMI_HOST_DEL_TWT_STATUS_NO_ACK:
	case WMI_HOST_DEL_TWT_STATUS_UNKNOWN_ERROR:
		return true;
	default:
		return false;
	}

	return false;
}

static void
sme_process_sta_twt_del_dialog_event(
		struct mac_context *mac,
		struct wmi_twt_del_dialog_complete_event_param *param)
{
	twt_del_dialog_cb callback;
	bool is_evt_allowed, usr_cfg_ps_enable;
	enum wlan_twt_commands active_cmd = WLAN_TWT_NONE;

	is_evt_allowed = mlme_twt_is_command_in_progress(
					mac->psoc, (struct qdf_mac_addr *)
					param->peer_macaddr, param->dialog_id,
					WLAN_TWT_TERMINATE, &active_cmd);

	if (!is_evt_allowed &&
	    param->dialog_id != WLAN_ALL_SESSIONS_DIALOG_ID &&
	    param->status != WMI_HOST_DEL_TWT_STATUS_ROAMING &&
	    param->status != WMI_HOST_DEL_TWT_STATUS_PEER_INIT_TEARDOWN &&
	    param->status != WMI_HOST_DEL_TWT_STATUS_CONCURRENCY) {
		sme_debug("Drop TWT Del dialog event for dialog_id:%d status:%d active_cmd:%d",
			  param->dialog_id, param->status, active_cmd);

		return;
	}

	usr_cfg_ps_enable = mlme_get_user_ps(mac->psoc, param->vdev_id);
	if (!usr_cfg_ps_enable &&
	    param->status == WMI_HOST_DEL_TWT_STATUS_OK)
		param->status = WMI_HOST_DEL_TWT_STATUS_PS_DISABLE_TEARDOWN;

	callback = mac->sme.twt_del_dialog_cb;
	if (callback)
		callback(mac->psoc, param);

	if (param->status == WMI_HOST_DEL_TWT_STATUS_ROAMING ||
	    param->status == WMI_HOST_DEL_TWT_STATUS_CONCURRENCY)
		mlme_twt_set_wait_for_notify(mac->psoc, param->vdev_id, true);

	/* Reset the active TWT command to none */
	mlme_set_twt_command_in_progress(mac->psoc, (struct qdf_mac_addr *)
					 param->peer_macaddr, param->dialog_id,
					 WLAN_TWT_NONE);

	if (sme_is_twt_teardown_failed(param->status))
		return;

	ucfg_mlme_set_twt_setup_done(mac->psoc, (struct qdf_mac_addr *)
				     param->peer_macaddr, param->dialog_id,
				     false);

	ucfg_mlme_set_twt_session_state(mac->psoc, (struct qdf_mac_addr *)
					param->peer_macaddr, param->dialog_id,
					WLAN_TWT_SETUP_STATE_NOT_ESTABLISHED);

	mlme_init_twt_context(mac->psoc, (struct qdf_mac_addr *)
			      param->peer_macaddr, param->dialog_id);
}

/**
 * sme_process_twt_del_dialog_event() - Process twt del dialog event
 * response from firmware
 * @mac: Global MAC pointer
 * @param: pointer to wmi_twt_del_dialog_complete_event_param buffer
 *
 * Return: None
 */
static void
sme_process_twt_del_dialog_event(
	struct mac_context *mac,
	struct wmi_twt_del_dialog_complete_event_param *param)
{
	twt_del_dialog_cb callback;
	enum QDF_OPMODE opmode;
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;

	opmode = wlan_get_opmode_from_vdev_id(mac->pdev, param->vdev_id);

	qdf_status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		return;

	switch (opmode) {
	case QDF_SAP_MODE:
		callback = mac->sme.twt_del_dialog_cb;
		if (callback)
			callback(mac->psoc, param);

		/*
		 * If this is an unsolicited TWT del event initiated from the
		 * peer, then no need to clear the active command in progress
		 */
		if (param->status !=
		    WMI_HOST_DEL_TWT_STATUS_PEER_INIT_TEARDOWN) {
			/* Reset the active TWT command to none */
			sme_sap_set_twt_command_in_progress(mac->psoc,
				param->vdev_id,
				(struct qdf_mac_addr *)param->peer_macaddr,
				param->dialog_id, WLAN_TWT_NONE);
			sme_sap_init_twt_context(mac->psoc, param->vdev_id,
				      (struct qdf_mac_addr *)
				      param->peer_macaddr, param->dialog_id);
		}
		break;
	case QDF_STA_MODE:
		sme_process_sta_twt_del_dialog_event(mac, param);
		break;
	default:
		sme_debug("TWT Teardown is not supported on %s",
			  qdf_opmode_str(opmode));
	}

	sme_release_global_lock(&mac->sme);
	return;
}

/**
 * sme_process_twt_pause_dialog_event() - Process twt pause dialog event
 * response from firmware
 * @mac: Global MAC pointer
 * @param: pointer to wmi_twt_pause_dialog_complete_event_param buffer
 *
 * Return: None
 */
static void
sme_process_twt_pause_dialog_event(
		struct mac_context *mac,
		struct wmi_twt_pause_dialog_complete_event_param *param)
{
	twt_pause_dialog_cb callback;
	enum QDF_OPMODE opmode;
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;

	opmode = wlan_get_opmode_from_vdev_id(mac->pdev, param->vdev_id);

	qdf_status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		return;

	switch (opmode) {
	case QDF_SAP_MODE:
		callback = mac->sme.twt_pause_dialog_cb;
		if (callback)
			callback(mac->psoc, param);
		break;
	case QDF_STA_MODE:
		callback = mac->sme.twt_pause_dialog_cb;
		if (callback)
			callback(mac->psoc, param);

		ucfg_mlme_set_twt_session_state(
					mac->psoc, (struct qdf_mac_addr *)
					param->peer_macaddr, param->dialog_id,
					WLAN_TWT_SETUP_STATE_SUSPEND);

		/*Reset the active TWT command to none */
		mlme_set_twt_command_in_progress(
					mac->psoc, (struct qdf_mac_addr *)
					param->peer_macaddr, param->dialog_id,
					WLAN_TWT_NONE);
		break;
	default:
		sme_debug("TWT Pause is not supported on %s",
			  qdf_opmode_str(opmode));
	}

	sme_release_global_lock(&mac->sme);
	return;
}

/**
 * sme_process_twt_nudge_dialog_event() - Process twt nudge dialog event
 * response from firmware
 * @mac: Global MAC pointer
 * @param: pointer to wmi_twt_nudge_dialog_complete_event_param buffer
 *
 * Return: None
 */
static void
sme_process_twt_nudge_dialog_event(struct mac_context *mac,
			struct wmi_twt_nudge_dialog_complete_event_param *param)
{
	twt_nudge_dialog_cb callback;
	bool is_evt_allowed;
	enum wlan_twt_commands active_cmd = WLAN_TWT_NONE;
	enum QDF_OPMODE opmode;
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;

	opmode = wlan_get_opmode_from_vdev_id(mac->pdev, param->vdev_id);

	qdf_status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		return;

	switch (opmode) {
	case QDF_SAP_MODE:
		callback = mac->sme.twt_nudge_dialog_cb;
		if (callback)
			callback(mac->psoc, param);
		break;
	case QDF_STA_MODE:
		is_evt_allowed = mlme_twt_is_command_in_progress(
					mac->psoc, (struct qdf_mac_addr *)
					param->peer_macaddr, param->dialog_id,
					WLAN_TWT_NUDGE, &active_cmd);
		if (!is_evt_allowed &&
		    param->dialog_id != WLAN_ALL_SESSIONS_DIALOG_ID) {
			sme_debug("Nudge event dropped active_cmd:%d",
				  active_cmd);
			goto fail;
		}

		callback = mac->sme.twt_nudge_dialog_cb;
		if (callback)
			callback(mac->psoc, param);
		/* Reset the active TWT command to none */
		mlme_set_twt_command_in_progress(
					mac->psoc, (struct qdf_mac_addr *)
					param->peer_macaddr, param->dialog_id,
					WLAN_TWT_NONE);
		break;
	default:
		sme_debug("TWT Nudge is not supported on %s",
			  qdf_opmode_str(opmode));
	}

fail:
	sme_release_global_lock(&mac->sme);
	return;
}

/**
 * sme_process_twt_resume_dialog_event() - Process twt resume dialog event
 * response from firmware
 * @mac: Global MAC pointer
 * @param: pointer to wmi_twt_resume_dialog_complete_event_param buffer
 *
 * Return: None
 */
static void
sme_process_twt_resume_dialog_event(
		struct mac_context *mac,
		struct wmi_twt_resume_dialog_complete_event_param *param)
{
	twt_resume_dialog_cb callback;
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	enum QDF_OPMODE opmode;

	opmode = wlan_get_opmode_from_vdev_id(mac->pdev, param->vdev_id);

	qdf_status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		return;

	switch (opmode) {
	case QDF_SAP_MODE:
		callback = mac->sme.twt_resume_dialog_cb;
		if (callback)
			callback(mac->psoc, param);
		break;
	case QDF_STA_MODE:
		callback = mac->sme.twt_resume_dialog_cb;
		if (callback)
			callback(mac->psoc, param);

		ucfg_mlme_set_twt_session_state(
					mac->psoc, (struct qdf_mac_addr *)
					param->peer_macaddr, param->dialog_id,
					WLAN_TWT_SETUP_STATE_ACTIVE);

		/* Reset the active TWT command to none */
		mlme_set_twt_command_in_progress(
					mac->psoc, (struct qdf_mac_addr *)
					param->peer_macaddr, param->dialog_id,
					WLAN_TWT_NONE);
		break;
	default:
		sme_debug("TWT Resume is not supported on %s",
			  qdf_opmode_str(opmode));
	}

	sme_release_global_lock(&mac->sme);
	return;
}

/**
 * sme_process_twt_notify_event() - Process twt ready for setup notification
 * event from firmware
 * @mac: Global MAC pointer
 * @twt_notify_event: pointer to event buf containing twt notify parameters
 *
 * Return: None
 */
static void
sme_process_twt_notify_event(struct mac_context *mac,
			     struct wmi_twt_notify_event_param *notify_event)
{
	twt_notify_cb callback;

	mlme_twt_set_wait_for_notify(mac->psoc, notify_event->vdev_id, false);
	callback = mac->sme.twt_notify_cb;
	if (callback)
		callback(mac->psoc, notify_event);
}

/**
 * sme_twt_update_beacon_template() - API to send beacon update to fw
 * @mac: Global MAC pointer
 *
 * Return: None
 */
void sme_twt_update_beacon_template(mac_handle_t mac_handle)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	csr_update_beacon(mac);
}

#else
static void
sme_process_twt_add_dialog_event(struct mac_context *mac,
				 struct twt_add_dialog_complete_event *add_dialog_event)
{
}

static void
sme_process_twt_del_dialog_event(
		struct mac_context *mac,
		struct wmi_twt_del_dialog_complete_event_param *param)
{
}

static void
sme_process_twt_pause_dialog_event(struct mac_context *mac,
				   struct wmi_twt_pause_dialog_complete_event_param *param)
{
}

static void
sme_process_twt_resume_dialog_event(struct mac_context *mac,
				    struct wmi_twt_resume_dialog_complete_event_param *param)
{
}

static void
sme_process_twt_nudge_dialog_event(struct mac_context *mac,
				   struct wmi_twt_nudge_dialog_complete_event_param *param)
{
}

static void
sme_process_twt_notify_event(struct mac_context *mac,
			     struct wmi_twt_notify_event_param *notify_event)
{
}
#endif

QDF_STATUS sme_process_msg(struct mac_context *mac, struct scheduler_msg *pMsg)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!pMsg) {
		sme_err("Empty message for SME");
		return status;
	}
	status = sme_acquire_global_lock(&mac->sme);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		if (pMsg->bodyptr)
			qdf_mem_free(pMsg->bodyptr);
		return status;
	}
	if (!SME_IS_START(mac)) {
		sme_debug("message type %d in stop state ignored", pMsg->type);
		if (pMsg->bodyptr)
			qdf_mem_free(pMsg->bodyptr);
		goto release_lock;
	}
	switch (pMsg->type) {
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	case eWNI_SME_HO_FAIL_IND:
		csr_process_ho_fail_ind(mac, pMsg->bodyptr);
		qdf_mem_free(pMsg->bodyptr);
		break;
#endif
	case eWNI_SME_ADDTS_RSP:
	case eWNI_SME_DELTS_RSP:
	case eWNI_SME_DELTS_IND:
	case eWNI_SME_FT_AGGR_QOS_RSP:
		/* QoS */
		if (pMsg->bodyptr) {
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
			status = sme_qos_msg_processor(mac, pMsg->type,
						       pMsg->bodyptr);
			qdf_mem_free(pMsg->bodyptr);
#endif
		} else {
			sme_err("Empty message for: %d", pMsg->type);
		}
		break;
	case eWNI_SME_NEIGHBOR_REPORT_IND:
	case eWNI_SME_BEACON_REPORT_REQ_IND:
		if (pMsg->bodyptr) {
			status = sme_rrm_msg_processor(mac, pMsg->type,
						       pMsg->bodyptr);
			qdf_mem_free(pMsg->bodyptr);
		} else {
			sme_err("Empty message for: %d", pMsg->type);
		}
		break;
	case eWNI_SME_VDEV_DELETE_RSP:
		if (pMsg->bodyptr)
			sme_vdev_self_peer_delete_resp(pMsg->bodyptr);
		else
			sme_err("Empty message for: %d", pMsg->type);
		break;
	case eWNI_SME_GENERIC_CHANGE_COUNTRY_CODE:
		if (pMsg->bodyptr) {
			status = sme_handle_generic_change_country_code(
						(void *)mac, pMsg->bodyptr);
			qdf_mem_free(pMsg->bodyptr);
		} else {
			sme_err("Empty message for: %d", pMsg->type);
		}
		break;
#ifdef WLAN_FEATURE_11W
	case eWNI_SME_UNPROT_MGMT_FRM_IND:
		if (pMsg->bodyptr) {
			sme_unprotected_mgmt_frm_ind(mac, pMsg->bodyptr);
			qdf_mem_free(pMsg->bodyptr);
		} else {
			sme_err("Empty message for: %d", pMsg->type);
		}
		break;
#endif
#ifdef FEATURE_WLAN_ESE
	case eWNI_SME_TSM_IE_IND:
		if (pMsg->bodyptr) {
			sme_tsm_ie_ind(mac, pMsg->bodyptr);
			qdf_mem_free(pMsg->bodyptr);
		} else {
			sme_err("Empty message for: %d", pMsg->type);
		}
		break;
#endif /* FEATURE_WLAN_ESE */
	case eWNI_SME_ROAM_SCAN_OFFLOAD_RSP:
		status = csr_roam_offload_scan_rsp_hdlr((void *)mac,
							pMsg->bodyptr);
		qdf_mem_free(pMsg->bodyptr);
		break;
#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
	case eWNI_SME_READY_TO_EXTWOW_IND:
		if (pMsg->bodyptr) {
			sme_process_ready_to_ext_wow(mac, pMsg->bodyptr);
			qdf_mem_free(pMsg->bodyptr);
		} else {
			sme_err("Empty message for: %d", pMsg->type);
		}
		break;
#endif
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
	case eWNI_SME_AUTO_SHUTDOWN_IND:
		if (mac->sme.auto_shutdown_cb) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
				  FL("Auto shutdown notification"));
			mac->sme.auto_shutdown_cb();
		}
		break;
#endif
	case eWNI_SME_DFS_RADAR_FOUND:
	case eWNI_SME_DFS_CAC_COMPLETE:
	case eWNI_SME_DFS_CSAIE_TX_COMPLETE_IND:
	case eWNI_SME_CSA_RESTART_RSP:
		status = dfs_msg_processor(mac, pMsg);
		qdf_mem_free(pMsg->bodyptr);
		break;
	case eWNI_SME_CHANNEL_CHANGE_RSP:
		if (pMsg->bodyptr) {
			status = sme_process_channel_change_resp(mac,
								 pMsg->type,
								 pMsg->bodyptr);
			qdf_mem_free(pMsg->bodyptr);
		} else {
			sme_err("Empty message for: %d", pMsg->type);
		}
		break;
	case eWNI_SME_STATS_EXT_EVENT:
		status = sme_stats_ext_event(mac, pMsg->bodyptr);
		qdf_mem_free(pMsg->bodyptr);
		break;
	case eWNI_SME_FW_STATUS_IND:
		status = sme_fw_state_resp(mac);
		break;
	case eWNI_SME_TSF_EVENT:
		if (mac->sme.get_tsf_cb) {
			mac->sme.get_tsf_cb(mac->sme.get_tsf_cxt,
					(struct stsf *)pMsg->bodyptr);
		}
		if (pMsg->bodyptr)
			qdf_mem_free(pMsg->bodyptr);
		break;
	case eWNI_SME_LINK_STATUS_IND:
	{
		tAniGetLinkStatus *pLinkStatus =
			(tAniGetLinkStatus *) pMsg->bodyptr;
		if (pLinkStatus) {
			if (mac->sme.link_status_callback)
				mac->sme.link_status_callback(
					pLinkStatus->linkStatus,
					mac->sme.link_status_context);

			mac->sme.link_status_callback = NULL;
			mac->sme.link_status_context = NULL;
			qdf_mem_free(pLinkStatus);
		}
		break;
	}
	case eWNI_SME_MSG_GET_TEMPERATURE_IND:
		if (mac->sme.temperature_cb)
			mac->sme.temperature_cb(pMsg->bodyval,
					mac->sme.temperature_cb_context);
		break;
	case eWNI_SME_SNR_IND:
	{
		tAniGetSnrReq *pSnrReq = (tAniGetSnrReq *) pMsg->bodyptr;

		if (pSnrReq) {
			if (pSnrReq->snrCallback) {
				((tCsrSnrCallback)
				 (pSnrReq->snrCallback))(pSnrReq->snr,
							 pSnrReq->pDevContext);
			}
			qdf_mem_free(pSnrReq);
		}
		break;
	}
#ifdef FEATURE_WLAN_EXTSCAN
	case eWNI_SME_EXTSCAN_FULL_SCAN_RESULT_IND:
		if (mac->sme.ext_scan_ind_cb)
			mac->sme.ext_scan_ind_cb(mac->hdd_handle,
					eSIR_EXTSCAN_FULL_SCAN_RESULT_IND,
					pMsg->bodyptr);
		else
			sme_err("callback not registered to process: %d",
				pMsg->type);

		qdf_mem_free(pMsg->bodyptr);
		break;
	case eWNI_SME_EPNO_NETWORK_FOUND_IND:
		if (mac->sme.ext_scan_ind_cb)
			mac->sme.ext_scan_ind_cb(mac->hdd_handle,
					eSIR_EPNO_NETWORK_FOUND_IND,
					pMsg->bodyptr);
		else
			sme_err("callback not registered to process: %d",
				pMsg->type);

		qdf_mem_free(pMsg->bodyptr);
		break;
#endif
	case eWNI_SME_SET_HW_MODE_RESP:
		if (pMsg->bodyptr) {
			status = sme_process_set_hw_mode_resp(mac,
								pMsg->bodyptr);
			qdf_mem_free(pMsg->bodyptr);
		} else {
			sme_err("Empty message for: %d", pMsg->type);
		}
		break;
	case eWNI_SME_HW_MODE_TRANS_IND:
		if (pMsg->bodyptr) {
			status = sme_process_hw_mode_trans_ind(mac,
								pMsg->bodyptr);
			qdf_mem_free(pMsg->bodyptr);
		} else {
			sme_err("Empty message for: %d", pMsg->type);
		}
		break;
	case eWNI_SME_NSS_UPDATE_RSP:
		if (pMsg->bodyptr) {
			status = sme_process_nss_update_resp(mac,
								pMsg->bodyptr);
			qdf_mem_free(pMsg->bodyptr);
		} else {
			sme_err("Empty message for: %d", pMsg->type);
		}
		break;
	case eWNI_SME_SET_DUAL_MAC_CFG_RESP:
		if (pMsg->bodyptr) {
			status = sme_process_dual_mac_config_resp(mac,
					pMsg->bodyptr);
			qdf_mem_free(pMsg->bodyptr);
		} else {
			sme_err("Empty message for: %d", pMsg->type);
		}
		break;
	case eWNI_SME_SET_THERMAL_LEVEL_IND:
		if (mac->sme.set_thermal_level_cb)
			mac->sme.set_thermal_level_cb(mac->hdd_handle,
						       pMsg->bodyval);
		break;
	case eWNI_SME_EXT_CHANGE_CHANNEL_IND:
		 status = sme_extended_change_channel_ind(mac, pMsg->bodyptr);
		 qdf_mem_free(pMsg->bodyptr);
		break;
	case eWNI_SME_SET_ANTENNA_MODE_RESP:
		if (pMsg->bodyptr) {
			status = sme_process_antenna_mode_resp(mac,
					pMsg->bodyptr);
			qdf_mem_free(pMsg->bodyptr);
		} else {
			sme_err("Empty message for: %d", pMsg->type);
		}
		break;
	case eWNI_SME_LOST_LINK_INFO_IND:
		if (mac->sme.lost_link_info_cb)
			mac->sme.lost_link_info_cb(mac->hdd_handle,
				(struct sir_lost_link_info *)pMsg->bodyptr);
		qdf_mem_free(pMsg->bodyptr);
		break;
	case eWNI_SME_RSO_CMD_STATUS_IND:
		if (mac->sme.rso_cmd_status_cb)
			mac->sme.rso_cmd_status_cb(mac->hdd_handle,
						    pMsg->bodyptr);
		qdf_mem_free(pMsg->bodyptr);
		break;
	case eWMI_SME_LL_STATS_IND:
		if (mac->sme.link_layer_stats_ext_cb)
			mac->sme.link_layer_stats_ext_cb(mac->hdd_handle,
							  pMsg->bodyptr);
		qdf_mem_free(pMsg->bodyptr);
		break;
	case eWNI_SME_BT_ACTIVITY_INFO_IND:
		if (mac->sme.bt_activity_info_cb)
			mac->sme.bt_activity_info_cb(mac->hdd_handle,
						      pMsg->bodyval);
		break;
	case eWNI_SME_HIDDEN_SSID_RESTART_RSP:
		if (mac->sme.hidden_ssid_cb)
			mac->sme.hidden_ssid_cb(mac->hdd_handle, pMsg->bodyval);
		else
			sme_err("callback is NULL");
		break;
	case eWNI_SME_ANTENNA_ISOLATION_RSP:
		if (pMsg->bodyptr) {
			if (mac->sme.get_isolation_cb)
				mac->sme.get_isolation_cb(
				  (struct sir_isolation_resp *)pMsg->bodyptr,
				  mac->sme.get_isolation_cb_context);
			qdf_mem_free(pMsg->bodyptr);
		} else {
			sme_err("Empty message for: %d", pMsg->type);
		}
		break;
	case eWNI_SME_GET_ROAM_SCAN_CH_LIST_EVENT:
		sme_process_roam_scan_ch_list_resp(mac, pMsg->bodyptr);
		qdf_mem_free(pMsg->bodyptr);
		break;
	case eWNI_SME_MONITOR_MODE_VDEV_UP:
		status = sme_process_monitor_mode_vdev_up_evt(pMsg->bodyval);
		break;
	case eWNI_SME_TWT_ADD_DIALOG_EVENT:
		sme_process_twt_add_dialog_event(mac, pMsg->bodyptr);
		qdf_mem_free(pMsg->bodyptr);
		break;
	case eWNI_SME_TWT_DEL_DIALOG_EVENT:
		sme_process_twt_del_dialog_event(mac, pMsg->bodyptr);
		qdf_mem_free(pMsg->bodyptr);
		break;
	case eWNI_SME_TWT_PAUSE_DIALOG_EVENT:
		sme_process_twt_pause_dialog_event(mac, pMsg->bodyptr);
		qdf_mem_free(pMsg->bodyptr);
		break;
	case eWNI_SME_TWT_RESUME_DIALOG_EVENT:
		sme_process_twt_resume_dialog_event(mac, pMsg->bodyptr);
		qdf_mem_free(pMsg->bodyptr);
		break;
	case eWNI_SME_TWT_NUDGE_DIALOG_EVENT:
		sme_process_twt_nudge_dialog_event(mac, pMsg->bodyptr);
		qdf_mem_free(pMsg->bodyptr);
		break;
	case eWNI_SME_TWT_NOTIFY_EVENT:
		sme_process_twt_notify_event(mac, pMsg->bodyptr);
		qdf_mem_free(pMsg->bodyptr);
		break;
	default:

		if ((pMsg->type >= eWNI_SME_MSG_TYPES_BEGIN)
		    && (pMsg->type <= eWNI_SME_MSG_TYPES_END)) {
			/* CSR */
			if (pMsg->bodyptr) {
				status = csr_msg_processor(mac, pMsg->bodyptr);
				qdf_mem_free(pMsg->bodyptr);
			} else
				sme_err("Empty message for: %d", pMsg->type);
		} else {
			sme_warn("Unknown message type: %d", pMsg->type);
			if (pMsg->bodyptr)
				qdf_mem_free(pMsg->bodyptr);
		}
	} /* switch */
release_lock:
	sme_release_global_lock(&mac->sme);
	return status;
}

QDF_STATUS sme_mc_process_handler(struct scheduler_msg *msg)
{
	struct mac_context *mac_ctx = cds_get_context(QDF_MODULE_ID_SME);

	if (!mac_ctx) {
		QDF_ASSERT(0);
		return QDF_STATUS_E_FAILURE;
	}

	return sme_process_msg(mac_ctx, msg);
}

/**
 * sme_process_nss_update_resp() - Process nss update response
 * @mac: Global MAC pointer
 * @msg: nss update response
 *
 * Processes the nss update response and invokes the HDD
 * callback to process further
 */
static QDF_STATUS sme_process_nss_update_resp(struct mac_context *mac, uint8_t *msg)
{
	tListElem *entry = NULL;
	tSmeCmd *command = NULL;
	bool found;
	policy_mgr_nss_update_cback callback = NULL;
	struct sir_bcn_update_rsp *param;

	param = (struct sir_bcn_update_rsp *)msg;
	if (!param)
		sme_err("nss update resp param is NULL");
		/* Not returning. Need to check if active command list
		 * needs to be freed
		 */

	if (param && param->reason != REASON_NSS_UPDATE) {
		sme_err("reason not NSS update");
		return QDF_STATUS_E_INVAL;
	}
	entry = csr_nonscan_active_ll_peek_head(mac, LL_ACCESS_LOCK);
	if (!entry) {
		sme_err("No cmd found in active list");
		return QDF_STATUS_E_FAILURE;
	}

	command = GET_BASE_ADDR(entry, tSmeCmd, Link);
	if (!command) {
		sme_err("Base address is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (e_sme_command_nss_update != command->command) {
		sme_err("Command mismatch!");
		return QDF_STATUS_E_FAILURE;
	}

	callback = command->u.nss_update_cmd.nss_update_cb;
	if (callback) {
		if (!param)
			sme_err("Callback failed since nss update params is NULL");
		else
			callback(command->u.nss_update_cmd.context,
				param->status,
				param->vdev_id,
				command->u.nss_update_cmd.next_action,
				command->u.nss_update_cmd.reason,
				command->u.nss_update_cmd.original_vdev_id);
	} else {
		sme_err("Callback does not exisit");
	}

	found = csr_nonscan_active_ll_remove_entry(mac, entry, LL_ACCESS_LOCK);
	if (found) {
		/* Now put this command back on the avilable command list */
		csr_release_command(mac, command);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sme_stop(mac_handle_t mac_handle)
{
	QDF_STATUS status;
	QDF_STATUS ret_status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = rrm_stop(mac);
	if (QDF_IS_STATUS_ERROR(status)) {
		ret_status = status;
		sme_err("rrm_stop failed with status: %d", status);
	}

	status = csr_stop(mac);
	if (QDF_IS_STATUS_ERROR(status)) {
		ret_status = status;
		sme_err("csr_stop failed with status: %d", status);
	}

	mac->sme.state = SME_STATE_STOP;

	return ret_status;
}

/*
 * sme_close() - Release all SME modules and their resources.
 * The function release each module in SME, PMC, CSR, etc. . Upon
 *  return, all modules are at closed state.
 *
 *  No SME APIs can be involved after smeClose except smeOpen.
 *  smeClose must be called before mac_close.
 *  This is a synchronous call
 *
 * mac_handle - The handle returned by mac_open
 * Return QDF_STATUS_SUCCESS - SME is successfully close.
 *
 *  Other status means SME is failed to be closed but caller still cannot
 *  call any other SME functions except smeOpen.
 */
QDF_STATUS sme_close(mac_handle_t mac_handle)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	QDF_STATUS fail_status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	if (!mac)
		return QDF_STATUS_E_FAILURE;

	sme_unregister_power_debug_stats_cb(mac);

	status = csr_close(mac);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("csr_close failed with status: %d", status);
		fail_status = status;
	}
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
	status = sme_qos_close(mac);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Qos close failed with status: %d", status);
		fail_status = status;
	}
#endif
	status = sme_ps_close(mac_handle);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("sme_ps_close failed status: %d", status);
		fail_status = status;
	}

	status = rrm_close(mac);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("RRM close failed with status: %d", status);
		fail_status = status;
	}

	free_sme_cmd_list(mac);

	status = qdf_mutex_destroy(&mac->sme.sme_global_lock);
	if (!QDF_IS_STATUS_SUCCESS(status))
		fail_status = QDF_STATUS_E_FAILURE;

	mac->sme.state = SME_STATE_STOP;

	return fail_status;
}

/**
 * sme_remove_bssid_from_scan_list() - wrapper to remove the bssid from
 * scan list
 * @mac_handle: Opaque handle to the global MAC context.
 * @bssid: bssid to be removed
 *
 * This function remove the given bssid from scan list.
 *
 * Return: QDF status.
 */
QDF_STATUS sme_remove_bssid_from_scan_list(mac_handle_t mac_handle,
					   tSirMacAddr bssid)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		csr_remove_bssid_from_scan_list(mac_ctx, bssid);
		sme_release_global_lock(&mac_ctx->sme);
	}

	return status;
}

QDF_STATUS sme_scan_get_result(mac_handle_t mac_handle, uint8_t vdev_id,
			       struct scan_filter *filter,
			       tScanResultHandle *phResult)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_MSG_SCAN_GET_RESULTS, vdev_id,
			 0));
	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = csr_scan_get_result(mac, filter, phResult, false);
		sme_release_global_lock(&mac->sme);
	}

	return status;
}

QDF_STATUS sme_scan_get_result_for_bssid(mac_handle_t mac_handle,
					 struct qdf_mac_addr *bssid,
					 tCsrScanResultInfo *res)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	QDF_STATUS status;

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = csr_scan_get_result_for_bssid(mac_ctx, bssid, res);
		sme_release_global_lock(&mac_ctx->sme);
	}

	return status;
}

QDF_STATUS sme_get_ap_channel_from_scan(void *profile,
					tScanResultHandle *scan_cache,
					uint32_t *ap_ch_freq,
					uint8_t vdev_id)
{
	QDF_STATUS status;

	status = sme_get_ap_channel_from_scan_cache((struct csr_roam_profile *)
						  profile,
						  scan_cache,
						  ap_ch_freq, vdev_id);
	return status;
}

QDF_STATUS sme_get_ap_channel_from_scan_cache(
	struct csr_roam_profile *profile, tScanResultHandle *scan_cache,
	uint32_t *ap_ch_freq, uint8_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac_ctx = sme_get_mac_context();
	struct scan_filter *scan_filter;
	tScanResultHandle filtered_scan_result = NULL;
	struct bss_description first_ap_profile;

	if (!mac_ctx) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				FL("mac_ctx is NULL"));
		return QDF_STATUS_E_FAILURE;
	}
	scan_filter = qdf_mem_malloc(sizeof(*scan_filter));
	if (!scan_filter)
		return QDF_STATUS_E_FAILURE;

	qdf_mem_zero(&first_ap_profile, sizeof(struct bss_description));
	if (!profile) {
		csr_set_open_mode_in_scan_filter(scan_filter);
	} else {
		/* Here is the profile we need to connect to */
		status = csr_roam_get_scan_filter_from_profile(mac_ctx, profile,
							       scan_filter,
							       false, vdev_id);
	}

	if (QDF_IS_STATUS_ERROR(status)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			FL("Preparing the profile filter failed"));
		qdf_mem_free(scan_filter);
		return QDF_STATUS_E_FAILURE;
	}
	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_STATUS_SUCCESS == status) {
		status = csr_scan_get_result(mac_ctx, scan_filter,
					  &filtered_scan_result, false);
		if (QDF_STATUS_SUCCESS == status) {
			csr_get_bssdescr_from_scan_handle(filtered_scan_result,
					&first_ap_profile);
			*scan_cache = filtered_scan_result;
			if (first_ap_profile.chan_freq) {
				*ap_ch_freq = first_ap_profile.chan_freq;
				QDF_TRACE(QDF_MODULE_ID_SME,
					  QDF_TRACE_LEVEL_DEBUG,
					  FL("Found best AP & its on freq[%d]"),
					  first_ap_profile.chan_freq);
			} else {
				/*
				 * This means scan result is empty
				 * so set the channel to zero, caller should
				 * take of zero channel id case.
				 */
				*ap_ch_freq = 0;
				QDF_TRACE(QDF_MODULE_ID_SME,
					  QDF_TRACE_LEVEL_ERROR,
					  FL("Scan is empty, set chnl to 0"));
				status = QDF_STATUS_E_FAILURE;
			}
		} else {
			sme_err("Failed to get scan get result");
			status = QDF_STATUS_E_FAILURE;
		}
		sme_release_global_lock(&mac_ctx->sme);
	} else {
		status = QDF_STATUS_E_FAILURE;
	}
	qdf_mem_free(scan_filter);

	return status;
}

/*
 * sme_scan_result_get_first() -
 * A wrapper function to request CSR to returns the first element of
 * scan result.
 * This is a synchronous call
 *
 * hScanResult - returned from csr_scan_get_result
 * Return tCsrScanResultInfo * - NULL if no result
 */
tCsrScanResultInfo *sme_scan_result_get_first(mac_handle_t mac_handle,
					      tScanResultHandle hScanResult)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	tCsrScanResultInfo *pRet = NULL;

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_MSG_SCAN_RESULT_GETFIRST,
			 NO_SESSION, 0));
	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		pRet = csr_scan_result_get_first(mac, hScanResult);
		sme_release_global_lock(&mac->sme);
	}

	return pRet;
}

/*
 * sme_scan_result_get_next() -
 * A wrapper function to request CSR to returns the next element of
 * scan result. It can be called without calling csr_scan_result_get_first first
 *   This is a synchronous call
 *
 * hScanResult - returned from csr_scan_get_result
 * Return Null if no result or reach the end
 */
tCsrScanResultInfo *sme_scan_result_get_next(mac_handle_t mac_handle,
					     tScanResultHandle hScanResult)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	tCsrScanResultInfo *pRet = NULL;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		pRet = csr_scan_result_get_next(mac, hScanResult);
		sme_release_global_lock(&mac->sme);
	}

	return pRet;
}

/*
 * sme_scan_result_purge() -
 * A wrapper function to request CSR to remove all items(tCsrScanResult)
 * in the list and free memory for each item
 *   This is a synchronous call
 *
 * hScanResult - returned from csr_scan_get_result. hScanResult is
 *	considered gone by
 *  calling this function and even before this function reutrns.
 * Return QDF_STATUS
 */
QDF_STATUS sme_scan_result_purge(tScanResultHandle hScanResult)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac_ctx = sme_get_mac_context();

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_MSG_SCAN_RESULT_PURGE,
			 NO_SESSION, 0));
	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = csr_scan_result_purge(mac_ctx, hScanResult);
		sme_release_global_lock(&mac_ctx->sme);
	}

	return status;
}

eCsrPhyMode sme_get_phy_mode(mac_handle_t mac_handle)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	return mac->roam.configParam.phyMode;
}

/*
 * sme_roam_connect() -
 * A wrapper function to request CSR to inititiate an association
 *   This is an asynchronous call.
 *
 * sessionId - the sessionId returned by sme_open_session.
 * pProfile - description of the network to which to connect
 * hBssListIn - a list of BSS descriptor to roam to. It is returned
 *			from csr_scan_get_result
 * pRoamId - to get back the request ID
 * Return QDF_STATUS
 */
QDF_STATUS sme_roam_connect(mac_handle_t mac_handle, uint8_t sessionId,
			    struct csr_roam_profile *pProfile,
			    uint32_t *pRoamId)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	if (!mac)
		return QDF_STATUS_E_FAILURE;

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_MSG_CONNECT, sessionId, 0));
	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (CSR_IS_SESSION_VALID(mac, sessionId)) {
			status =
				csr_roam_connect(mac, sessionId, pProfile,
						 pRoamId);
		} else {
			sme_err("Invalid sessionID: %d", sessionId);
			status = QDF_STATUS_E_INVAL;
		}
		sme_release_global_lock(&mac->sme);
	}

	return status;
}

/*
 * sme_set_phy_mode() -
 * Changes the PhyMode.
 *
 * mac_handle - The handle returned by mac_open.
 * phyMode new phyMode which is to set
 * Return QDF_STATUS  SUCCESS.
 */
QDF_STATUS sme_set_phy_mode(mac_handle_t mac_handle, eCsrPhyMode phyMode)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	mac->roam.configParam.phyMode = phyMode;
	mac->roam.configParam.uCfgDot11Mode =
		csr_get_cfg_dot11_mode_from_csr_phy_mode(NULL,
						mac->roam.configParam.phyMode,
						mac->roam.configParam.
						ProprietaryRatesEnabled);

	return QDF_STATUS_SUCCESS;
}

/*
 * sme_roam_reassoc() -
 * A wrapper function to request CSR to inititiate a re-association
 *
 * pProfile - can be NULL to join the currently connected AP. In that
 * case modProfileFields should carry the modified field(s) which could trigger
 * reassoc
 * modProfileFields - fields which are part of tCsrRoamConnectedProfile
 *   that might need modification dynamically once STA is up & running and this
 *   could trigger a reassoc
 * pRoamId - to get back the request ID
 * Return QDF_STATUS
 */
QDF_STATUS sme_roam_reassoc(mac_handle_t mac_handle, uint8_t sessionId,
			    struct csr_roam_profile *pProfile,
			    tCsrRoamModifyProfileFields modProfileFields,
			    uint32_t *pRoamId, bool fForce)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_ROAM_REASSOC, sessionId, 0));
	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (CSR_IS_SESSION_VALID(mac, sessionId)) {
			if ((!pProfile) && (fForce == 1))
				status = csr_reassoc(mac, sessionId,
						     &modProfileFields, pRoamId,
						     fForce);
			else
				status = csr_roam_reassoc(mac, sessionId,
						pProfile,
						modProfileFields, pRoamId);
		} else {
			status = QDF_STATUS_E_INVAL;
		}
		sme_release_global_lock(&mac->sme);
	}

	return status;
}

QDF_STATUS sme_roam_disconnect(mac_handle_t mac_handle, uint8_t session_id,
			       eCsrRoamDisconnectReason reason,
			       enum wlan_reason_code mac_reason)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_ROAM_DISCONNECT, session_id,
			 reason));
	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (CSR_IS_SESSION_VALID(mac_ctx, session_id))
			status = csr_roam_disconnect(mac_ctx, session_id,
						     reason,
						     mac_reason);
		else
			status = QDF_STATUS_E_INVAL;
		sme_release_global_lock(&mac_ctx->sme);
	}

	return status;
}

/* sme_dhcp_done_ind() - send dhcp done ind
 * @mac_handle: Opaque handle to the global MAC context
 * @session_id: session id
 *
 * Return: void.
 */
void sme_dhcp_done_ind(mac_handle_t mac_handle, uint8_t session_id)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session;

	if (!mac_ctx)
		return;

	session = CSR_GET_SESSION(mac_ctx, session_id);
	if (!session) {
		sme_err("Session: %d not found", session_id);
		return;
	}
	session->dhcp_done = true;
}

/*
 * sme_roam_stop_bss() -
 * To stop BSS for Soft AP. This is an asynchronous API.
 *
 * mac_handle - Global structure
 * sessionId - sessionId of SoftAP
 * Return QDF_STATUS  SUCCESS  Roam callback will be called to indicate
 * actual results
 */
QDF_STATUS sme_roam_stop_bss(mac_handle_t mac_handle, uint8_t sessionId)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (CSR_IS_SESSION_VALID(mac, sessionId))
			status = csr_roam_issue_stop_bss_cmd(mac, sessionId,
							false);
		else
			status = QDF_STATUS_E_INVAL;
		sme_release_global_lock(&mac->sme);
	}

	return status;
}

/**
 * sme_roam_disconnect_sta() - disassociate a station
 * @mac_handle:          Global structure
 * @sessionId:     SessionId of SoftAP
 * @p_del_sta_params: Pointer to parameters of the station to disassoc
 *
 * To disassociate a station. This is an asynchronous API.
 *
 * Return: QDF_STATUS_SUCCESS on success.Roam callback will
 *         be called to indicate actual result.
 */
QDF_STATUS sme_roam_disconnect_sta(mac_handle_t mac_handle, uint8_t sessionId,
				   struct csr_del_sta_params *p_del_sta_params)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	if (!mac) {
		QDF_ASSERT(0);
		return status;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (CSR_IS_SESSION_VALID(mac, sessionId))
			status = csr_roam_issue_disassociate_sta_cmd(mac,
					sessionId, p_del_sta_params);
		else
			status = QDF_STATUS_E_INVAL;
		sme_release_global_lock(&mac->sme);
	}

	return status;
}

/**
 * sme_roam_deauth_sta() - deauthenticate a station
 * @mac_handle:          Global structure
 * @sessionId:     SessionId of SoftAP
 * @pDelStaParams: Pointer to parameters of the station to deauthenticate
 *
 * To disassociate a station. This is an asynchronous API.
 *
 * Return: QDF_STATUS_SUCCESS on success or another QDF_STATUS error
 *         code on error. Roam callback will be called to indicate actual
 *         result
 */
QDF_STATUS sme_roam_deauth_sta(mac_handle_t mac_handle, uint8_t sessionId,
			       struct csr_del_sta_params *pDelStaParams)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	if (!mac) {
		QDF_ASSERT(0);
		return status;
	}

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_MSG_DEAUTH_STA,
			 sessionId, pDelStaParams->reason_code));
	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (CSR_IS_SESSION_VALID(mac, sessionId))
			status =
				csr_roam_issue_deauth_sta_cmd(mac, sessionId,
							      pDelStaParams);
		else
			status = QDF_STATUS_E_INVAL;
		sme_release_global_lock(&mac->sme);
	}

	return status;
}

/*
 * sme_roam_get_connect_profile() -
 * A wrapper function to request CSR to return the current connect
 * profile. Caller must call csr_roam_free_connect_profile after it is done
 * and before reuse for another csr_roam_get_connect_profile call.
 * This is a synchronous call.
 *
 * pProfile - pointer to a caller allocated structure
 *		      tCsrRoamConnectedProfile
 * eturn QDF_STATUS. Failure if not connected
 */
QDF_STATUS sme_roam_get_connect_profile(mac_handle_t mac_handle,
					uint8_t sessionId,
					tCsrRoamConnectedProfile *pProfile)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_ROAM_GET_CONNECTPROFILE,
			 sessionId, 0));
	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (CSR_IS_SESSION_VALID(mac, sessionId))
			status = csr_roam_get_connect_profile(mac, sessionId,
							pProfile);
		else
			status = QDF_STATUS_E_INVAL;
		sme_release_global_lock(&mac->sme);
	}

	return status;
}

/**
 * sme_roam_free_connect_profile - a wrapper function to request CSR to free and
 * reinitialize the profile returned previously by csr_roam_get_connect_profile.
 *
 * @profile - pointer to a caller allocated structure tCsrRoamConnectedProfile
 *
 * Return: none
 */
void sme_roam_free_connect_profile(tCsrRoamConnectedProfile *profile)
{
	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_ROAM_FREE_CONNECTPROFILE,
			 NO_SESSION, 0));
	csr_roam_free_connect_profile(profile);
}

QDF_STATUS sme_roam_del_pmkid_from_cache(mac_handle_t mac_handle,
					 uint8_t vdev_id,
					 struct wlan_crypto_pmksa *pmksa,
					 bool set_pmk)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct wlan_objmgr_vdev *vdev;

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_ROAM_DEL_PMKIDCACHE,
			 vdev_id, set_pmk));

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("Vdev[%d] is NULL!", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	status = wlan_crypto_set_del_pmksa(vdev, pmksa, set_pmk);
	if (QDF_IS_STATUS_ERROR(status))
		sme_err("Delete PMK entry failed");

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
	return status;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
void sme_get_pmk_info(mac_handle_t mac_handle, uint8_t session_id,
		      tPmkidCacheInfo *pmk_cache)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = sme_acquire_global_lock(&mac_ctx->sme);

	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (CSR_IS_SESSION_VALID(mac_ctx, session_id))
			csr_get_pmk_info(mac_ctx, session_id, pmk_cache);
		sme_release_global_lock(&mac_ctx->sme);
	}
}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
QDF_STATUS sme_roam_set_psk_pmk(mac_handle_t mac_handle,
				struct wlan_crypto_pmksa *pmksa,
				uint8_t vdev_id, bool update_to_fw)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (CSR_IS_SESSION_VALID(mac, vdev_id))
			status = csr_roam_set_psk_pmk(mac, pmksa, vdev_id,
						      update_to_fw);
		else
			status = QDF_STATUS_E_INVAL;
		sme_release_global_lock(&mac->sme);
	}
	return status;
}

QDF_STATUS sme_set_pmk_cache_ft(mac_handle_t mac_handle, uint8_t session_id,
				tPmkidCacheInfo *pmk_cache)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = csr_set_pmk_cache_ft(mac, session_id, pmk_cache);
		sme_release_global_lock(&mac->sme);
	}
	return status;
}
#endif

QDF_STATUS sme_roam_get_wpa_rsn_req_ie(mac_handle_t mac_handle,
				       uint8_t session_id,
				       uint32_t *len, uint8_t *buf)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (CSR_IS_SESSION_VALID(mac, session_id))
			status = csr_roam_get_wpa_rsn_req_ie(mac, session_id,
							     len, buf);
		else
			status = QDF_STATUS_E_INVAL;
		sme_release_global_lock(&mac->sme);
	}

	return status;
}

/*
 * sme_get_config_param() -
 * A wrapper function that HDD calls to get the global settings
 *	currently maintained by CSR.
 * This is a synchronous call.
 *
 * pParam - caller allocated memory
 * Return QDF_STATUS
 */
QDF_STATUS sme_get_config_param(mac_handle_t mac_handle,
				struct sme_config_params *pParam)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_GET_CONFIGPARAM, NO_SESSION, 0));
	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = csr_get_config_param(mac, &pParam->csr_config);
		if (status != QDF_STATUS_SUCCESS) {
			sme_err("csr_get_config_param failed");
			sme_release_global_lock(&mac->sme);
			return status;
		}
		sme_release_global_lock(&mac->sme);
	}

	return status;
}

uint32_t sme_get_vht_ch_width(void)
{
	return wma_get_vht_ch_width();
}

/*
 * sme_get_modify_profile_fields() -
 * HDD or SME - QOS calls this function to get the current values of
 * connected profile fields, changing which can cause reassoc.
 * This function must be called after CFG is downloaded and STA is in connected
 * state. Also, make sure to call this function to get the current profile
 * fields before calling the reassoc. So that pModifyProfileFields will have
 * all the latest values plus the one(s) has been updated as part of reassoc
 * request.
 *
 * pModifyProfileFields - pointer to the connected profile fields
 *   changing which can cause reassoc
 * Return QDF_STATUS
 */
QDF_STATUS sme_get_modify_profile_fields(mac_handle_t mac_handle,
					 uint8_t sessionId,
					 tCsrRoamModifyProfileFields *
					 pModifyProfileFields)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_GET_MODPROFFIELDS, sessionId,
			 0));
	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (CSR_IS_SESSION_VALID(mac, sessionId))
			status = csr_get_modify_profile_fields(mac, sessionId,
							  pModifyProfileFields);
		else
			status = QDF_STATUS_E_INVAL;
		sme_release_global_lock(&mac->sme);
	}

	return status;
}

#ifdef FEATURE_OEM_DATA_SUPPORT
/**
 * sme_register_oem_data_rsp_callback() - Register a routine of
 *                                        type send_oem_data_rsp_msg
 * @mac_handle:                                Handle returned by mac_open.
 * @callback:                             Callback to send response
 *                                        to oem application.
 *
 * sme_oem_data_rsp_callback is used to register sme_send_oem_data_rsp_msg
 * callback function.
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS sme_register_oem_data_rsp_callback(mac_handle_t mac_handle,
				sme_send_oem_data_rsp_msg callback)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *pmac = MAC_CONTEXT(mac_handle);

	pmac->sme.oem_data_rsp_callback = callback;

	return status;

}

/**
 * sme_deregister_oem_data_rsp_callback() - De-register OEM datarsp callback
 * @mac_handle: Handler return by mac_open
 * This function De-registers the OEM data response callback  to SME
 *
 * Return: None
 */
void  sme_deregister_oem_data_rsp_callback(mac_handle_t mac_handle)
{
	struct mac_context *pmac;

	if (!mac_handle) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				  FL("mac_handle is not valid"));
		return;
	}
	pmac = MAC_CONTEXT(mac_handle);

	pmac->sme.oem_data_rsp_callback = NULL;
}

/**
 * sme_oem_update_capability() - update UMAC's oem related capability.
 * @mac_handle: Handle returned by mac_open
 * @oem_cap: pointer to oem_capability
 *
 * This function updates OEM capability to UMAC. Currently RTT
 * related capabilities are updated. More capabilities can be
 * added in future.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_oem_update_capability(mac_handle_t mac_handle,
				     struct sme_oem_capability *cap)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *pmac = MAC_CONTEXT(mac_handle);
	uint8_t *bytes;

	bytes = pmac->rrm.rrmConfig.rm_capability;

	if (cap->ftm_rr)
		bytes[4] |= RM_CAP_FTM_RANGE_REPORT;
	if (cap->lci_capability)
		bytes[4] |= RM_CAP_CIVIC_LOC_MEASUREMENT;

	return status;
}

/**
 * sme_oem_get_capability() - get oem capability
 * @mac_handle: Handle returned by mac_open
 * @oem_cap: pointer to oem_capability
 *
 * This function is used to get the OEM capability from UMAC.
 * Currently RTT related capabilities are received. More
 * capabilities can be added in future.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_oem_get_capability(mac_handle_t mac_handle,
				  struct sme_oem_capability *cap)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *pmac = MAC_CONTEXT(mac_handle);
	uint8_t *bytes;

	bytes = pmac->rrm.rrmConfig.rm_capability;

	cap->ftm_rr = bytes[4] & RM_CAP_FTM_RANGE_REPORT;
	cap->lci_capability = bytes[4] & RM_CAP_CIVIC_LOC_MEASUREMENT;

	return status;
}
#endif

/*
 * sme_get_snr() -
 * A wrapper function that client calls to register a callback to get SNR
 *
 * callback - SME sends back the requested stats using the callback
 * staId - The station ID for which the stats is requested for
 * pContext - user context to be passed back along with the callback
 * p_cds_context - cds context
 *   \return QDF_STATUS
 */
QDF_STATUS sme_get_snr(mac_handle_t mac_handle,
		       tCsrSnrCallback callback,
		       struct qdf_mac_addr bssId, void *pContext)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = csr_get_snr(mac, callback, bssId, pContext);
		sme_release_global_lock(&mac->sme);
	}
	return status;
}

QDF_STATUS sme_get_link_status(mac_handle_t mac_handle,
			       csr_link_status_callback callback,
			       void *context, uint8_t session_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	tAniGetLinkStatus *msg;
	struct scheduler_msg message = {0};

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		msg = qdf_mem_malloc(sizeof(*msg));
		if (!msg) {
			sme_release_global_lock(&mac->sme);
			return QDF_STATUS_E_NOMEM;
		}

		msg->msgType = WMA_LINK_STATUS_GET_REQ;
		msg->msgLen = sizeof(*msg);
		msg->sessionId = session_id;
		mac->sme.link_status_context = context;
		mac->sme.link_status_callback = callback;

		message.type = WMA_LINK_STATUS_GET_REQ;
		message.bodyptr = msg;
		message.reserved = 0;

		status = scheduler_post_message(QDF_MODULE_ID_SME,
						QDF_MODULE_ID_WMA,
						QDF_MODULE_ID_WMA, &message);
		if (QDF_IS_STATUS_ERROR(status)) {
			sme_err("post msg failed, %d", status);
			qdf_mem_free(msg);
			mac->sme.link_status_context = NULL;
			mac->sme.link_status_callback = NULL;
		}

		sme_release_global_lock(&mac->sme);
	}

	return status;
}

/*
 * sme_generic_change_country_code() -
 * Change Country code from upperlayer during WLAN driver operation.
 * This is a synchronous API.
 *
 * mac_handle - The handle returned by mac_open.
 * pCountry New Country Code String
 * reg_domain regulatory domain
 * Return QDF_STATUS  SUCCESS.
 * FAILURE or RESOURCES  The API finished and failed.
 */
QDF_STATUS sme_generic_change_country_code(mac_handle_t mac_handle,
					   uint8_t *pCountry)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg msg = {0};
	tAniGenericChangeCountryCodeReq *pMsg;

	if (!mac) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_FATAL,
			  "%s: mac is null", __func__);
		return status;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		pMsg = qdf_mem_malloc(sizeof(tAniGenericChangeCountryCodeReq));
		if (!pMsg) {
			sme_release_global_lock(&mac->sme);
			return QDF_STATUS_E_NOMEM;
		}

		pMsg->msgType = eWNI_SME_GENERIC_CHANGE_COUNTRY_CODE;
		pMsg->msgLen =
			(uint16_t) sizeof(tAniGenericChangeCountryCodeReq);
		qdf_mem_copy(pMsg->countryCode, pCountry, 2);
		pMsg->countryCode[2] = ' ';

		msg.type = eWNI_SME_GENERIC_CHANGE_COUNTRY_CODE;
		msg.bodyptr = pMsg;
		msg.reserved = 0;

		if (QDF_STATUS_SUCCESS !=
		    scheduler_post_message(QDF_MODULE_ID_SME,
					   QDF_MODULE_ID_SME,
					   QDF_MODULE_ID_SME, &msg)) {
			qdf_mem_free(pMsg);
			status = QDF_STATUS_E_FAILURE;
		}
		sme_release_global_lock(&mac->sme);
	}

	return status;
}

/*
 * sme_dhcp_start_ind() -
 * API to signal the FW about the DHCP Start event.
 *
 * mac_handle: Opaque handle to the global MAC context.
 * device_mode - mode(AP,SAP etc) of the device.
 * macAddr - MAC address of the adapter.
 * sessionId - session ID.
 * Return QDF_STATUS  SUCCESS.
 * FAILURE or RESOURCES  The API finished and failed.
 */
QDF_STATUS sme_dhcp_start_ind(mac_handle_t mac_handle,
			      uint8_t device_mode,
			      uint8_t *macAddr, uint8_t sessionId)
{
	QDF_STATUS status;
	QDF_STATUS qdf_status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};
	tAniDHCPInd *pMsg;
	struct csr_roam_session *pSession;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_STATUS_SUCCESS == status) {
		pSession = CSR_GET_SESSION(mac, sessionId);

		if (!pSession) {
			sme_err("Session: %d not found", sessionId);
			sme_release_global_lock(&mac->sme);
			return QDF_STATUS_E_FAILURE;
		}
		pSession->dhcp_done = false;

		pMsg = qdf_mem_malloc(sizeof(tAniDHCPInd));
		if (!pMsg) {
			sme_release_global_lock(&mac->sme);
			return QDF_STATUS_E_NOMEM;
		}
		pMsg->msgType = WMA_DHCP_START_IND;
		pMsg->msgLen = (uint16_t) sizeof(tAniDHCPInd);
		pMsg->device_mode = device_mode;
		qdf_mem_copy(pMsg->adapterMacAddr.bytes, macAddr,
			     QDF_MAC_ADDR_SIZE);
		qdf_copy_macaddr(&pMsg->peerMacAddr,
				 &pSession->connectedProfile.bssid);

		message.type = WMA_DHCP_START_IND;
		message.bodyptr = pMsg;
		message.reserved = 0;
		MTRACE(qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
				 sessionId, message.type));
		qdf_status = scheduler_post_message(QDF_MODULE_ID_SME,
						    QDF_MODULE_ID_WMA,
						    QDF_MODULE_ID_WMA,
						    &message);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				  "%s: Post DHCP Start MSG fail", __func__);
			qdf_mem_free(pMsg);
			status = QDF_STATUS_E_FAILURE;
		}
		sme_release_global_lock(&mac->sme);
	}
	return status;
}

/*
 * sme_dhcp_stop_ind() -
 * API to signal the FW about the DHCP complete event.
 *
 * mac_handle: Opaque handle to the global MAC context.
 * device_mode - mode(AP, SAP etc) of the device.
 * macAddr - MAC address of the adapter.
 * sessionId - session ID.
 * Return QDF_STATUS  SUCCESS.
 *			FAILURE or RESOURCES  The API finished and failed.
 */
QDF_STATUS sme_dhcp_stop_ind(mac_handle_t mac_handle,
			     uint8_t device_mode,
			     uint8_t *macAddr, uint8_t sessionId)
{
	QDF_STATUS status;
	QDF_STATUS qdf_status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};
	tAniDHCPInd *pMsg;
	struct csr_roam_session *pSession;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_STATUS_SUCCESS == status) {
		pSession = CSR_GET_SESSION(mac, sessionId);

		if (!pSession) {
			sme_err("Session: %d not found", sessionId);
			sme_release_global_lock(&mac->sme);
			return QDF_STATUS_E_FAILURE;
		}
		pSession->dhcp_done = true;

		pMsg = qdf_mem_malloc(sizeof(tAniDHCPInd));
		if (!pMsg) {
			sme_release_global_lock(&mac->sme);
			return QDF_STATUS_E_NOMEM;
		}

		pMsg->msgType = WMA_DHCP_STOP_IND;
		pMsg->msgLen = (uint16_t) sizeof(tAniDHCPInd);
		pMsg->device_mode = device_mode;
		qdf_mem_copy(pMsg->adapterMacAddr.bytes, macAddr,
			     QDF_MAC_ADDR_SIZE);
		qdf_copy_macaddr(&pMsg->peerMacAddr,
				 &pSession->connectedProfile.bssid);

		message.type = WMA_DHCP_STOP_IND;
		message.bodyptr = pMsg;
		message.reserved = 0;
		MTRACE(qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
				 sessionId, message.type));
		qdf_status = scheduler_post_message(QDF_MODULE_ID_SME,
						    QDF_MODULE_ID_WMA,
						    QDF_MODULE_ID_WMA,
						    &message);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				  "%s: Post DHCP Stop MSG fail", __func__);
			qdf_mem_free(pMsg);
			status = QDF_STATUS_E_FAILURE;
		}

		sme_release_global_lock(&mac->sme);
	}
	return status;
}

/*
 * sme_neighbor_report_request() -
 * API to request neighbor report.
 *
 * mac_handle - The handle returned by mac_open.
 * pRrmNeighborReq - Pointer to a caller allocated object of type
 *			      tRrmNeighborReq. Caller owns the memory and is
 *			      responsible for freeing it.
 * Return QDF_STATUS
 *	    QDF_STATUS_E_FAILURE - failure
 *	    QDF_STATUS_SUCCESS  success
 */
QDF_STATUS sme_neighbor_report_request(
				mac_handle_t mac_handle,
				uint8_t sessionId,
				tpRrmNeighborReq pRrmNeighborReq,
				tpRrmNeighborRspCallbackInfo callbackInfo)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_NEIGHBOR_REPORTREQ, NO_SESSION,
			 0));

	if (pRrmNeighborReq->neighbor_report_offload) {
		status = csr_invoke_neighbor_report_request(sessionId,
							    pRrmNeighborReq,
							    false);
		return status;
	}

	if (QDF_STATUS_SUCCESS == sme_acquire_global_lock(&mac->sme)) {
		status =
			sme_rrm_neighbor_report_request(mac, sessionId,
						pRrmNeighborReq, callbackInfo);
		sme_release_global_lock(&mac->sme);
	}

	return status;
}

#ifdef FEATURE_OEM_DATA_SUPPORT
QDF_STATUS sme_oem_req_cmd(mac_handle_t mac_handle,
			   struct oem_data_req *oem_req)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct oem_data_req *oem_data_req;
	void *wma_handle;

	SME_ENTER();
	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		sme_err("wma_handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	oem_data_req = qdf_mem_malloc(sizeof(*oem_data_req));
	if (!oem_data_req)
		return QDF_STATUS_E_NOMEM;

	oem_data_req->data_len = oem_req->data_len;
	oem_data_req->data = qdf_mem_malloc(oem_data_req->data_len);
	if (!oem_data_req->data) {
		qdf_mem_free(oem_data_req);
		return QDF_STATUS_E_NOMEM;
	}

	qdf_mem_copy(oem_data_req->data, oem_req->data,
		     oem_data_req->data_len);

	status = wma_start_oem_req_cmd(wma_handle, oem_data_req);

	if (!QDF_IS_STATUS_SUCCESS(status))
		sme_err("Post oem data request msg fail");
	else
		sme_debug("OEM request(length: %d) sent to WMA",
			  oem_data_req->data_len);

	if (oem_data_req->data_len)
		qdf_mem_free(oem_data_req->data);
	qdf_mem_free(oem_data_req);

	SME_EXIT();
	return status;
}
#endif /*FEATURE_OEM_DATA_SUPPORT */

#ifdef FEATURE_OEM_DATA
QDF_STATUS sme_oem_data_cmd(mac_handle_t mac_handle,
			    void (*oem_data_event_handler_cb)
			    (const struct oem_data *oem_event_data,
			     uint8_t vdev_id),
			     struct oem_data *oem_data,
			     uint8_t vdev_id)
{
	QDF_STATUS status;
	void *wma_handle;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	SME_ENTER();
	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		sme_err("wma_handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		mac->sme.oem_data_event_handler_cb = oem_data_event_handler_cb;
		mac->sme.oem_data_vdev_id = vdev_id;
		sme_release_global_lock(&mac->sme);
	}
	status = wma_start_oem_data_cmd(wma_handle, oem_data);
	if (!QDF_IS_STATUS_SUCCESS(status))
		sme_err("fail to call wma_start_oem_data_cmd.");

	SME_EXIT();
	return status;
}
#endif

#define STA_NSS_CHAINS_SHIFT               0
#define SAP_NSS_CHAINS_SHIFT               3
#define P2P_GO_NSS_CHAINS_SHIFT            6
#define P2P_CLI_CHAINS_SHIFT               9
#define TDLS_NSS_CHAINS_SHIFT              12
#define IBSS_NSS_CHAINS_SHIFT              15
#define P2P_DEV_NSS_CHAINS_SHIFT           18
#define OCB_NSS_CHAINS_SHIFT               21
#define NAN_NSS_CHAIN_SHIFT                24
#define NSS_CHAIN_MASK                     0x7
#define GET_VDEV_NSS_CHAIN(x, y)         (((x) >> (y)) & NSS_CHAIN_MASK)

static uint8_t sme_get_nss_chain_shift(enum QDF_OPMODE device_mode)
{
	switch (device_mode) {
	case QDF_STA_MODE:
		return STA_NSS_CHAINS_SHIFT;
	case QDF_SAP_MODE:
		return SAP_NSS_CHAINS_SHIFT;
	case QDF_P2P_GO_MODE:
		return P2P_GO_NSS_CHAINS_SHIFT;
	case QDF_P2P_CLIENT_MODE:
		return P2P_CLI_CHAINS_SHIFT;
	case QDF_IBSS_MODE:
		return IBSS_NSS_CHAINS_SHIFT;
	case QDF_P2P_DEVICE_MODE:
		return P2P_DEV_NSS_CHAINS_SHIFT;
	case QDF_OCB_MODE:
		return OCB_NSS_CHAINS_SHIFT;
	case QDF_TDLS_MODE:
		return TDLS_NSS_CHAINS_SHIFT;

	default:
		sme_err("Device mode %d invalid", device_mode);
		return STA_NSS_CHAINS_SHIFT;
	}
}

static void
sme_check_nss_chain_ini_param(struct wlan_mlme_nss_chains *vdev_ini_cfg,
			      uint8_t rf_chains_supported,
			      enum nss_chains_band_info band)
{
	vdev_ini_cfg->rx_nss[band] = QDF_MIN(vdev_ini_cfg->rx_nss[band],
					     rf_chains_supported);
	vdev_ini_cfg->tx_nss[band] = QDF_MIN(vdev_ini_cfg->tx_nss[band],
					     rf_chains_supported);
}

static void
sme_fill_nss_chain_params(struct mac_context *mac_ctx,
			  struct wlan_mlme_nss_chains *vdev_ini_cfg,
			  enum QDF_OPMODE device_mode,
			  enum nss_chains_band_info band,
			  uint8_t rf_chains_supported)
{
	uint8_t nss_chain_shift, btc_chain_mode;
	uint8_t max_supported_nss;
	struct wlan_mlme_nss_chains *nss_chains_ini_cfg =
					&mac_ctx->mlme_cfg->nss_chains_ini_cfg;
	QDF_STATUS status;

	nss_chain_shift = sme_get_nss_chain_shift(device_mode);
	max_supported_nss = mac_ctx->mlme_cfg->vht_caps.vht_cap_info.enable2x2 ?
			    MAX_VDEV_NSS : 1;

	/*
	 * If target supports Antenna sharing, set NSS to 1 for 2.4GHz band for
	 * NDI vdev.
	 */
	if (device_mode == QDF_NDI_MODE && mac_ctx->mlme_cfg->gen.as_enabled &&
	    band == NSS_CHAINS_BAND_2GHZ)
		max_supported_nss = NSS_1x1_MODE;

	status = ucfg_coex_psoc_get_btc_chain_mode(mac_ctx->psoc,
						   &btc_chain_mode);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Failed to get BT coex chain mode");
		btc_chain_mode = WLAN_COEX_BTC_CHAIN_MODE_UNSETTLED;
	}

	if (band == NSS_CHAINS_BAND_2GHZ &&
	    btc_chain_mode == QCA_BTC_CHAIN_SEPARATED)
		max_supported_nss = NSS_1x1_MODE;

	/* If the fw doesn't support two chains, num rf chains can max be 1 */
	vdev_ini_cfg->num_rx_chains[band] =
		QDF_MIN(GET_VDEV_NSS_CHAIN(
				nss_chains_ini_cfg->num_rx_chains[band],
				nss_chain_shift), rf_chains_supported);

	vdev_ini_cfg->num_tx_chains[band] =
		QDF_MIN(GET_VDEV_NSS_CHAIN(
				nss_chains_ini_cfg->num_tx_chains[band],
				nss_chain_shift), rf_chains_supported);

	/* If 2x2 mode is disabled, then max rx, tx nss can be 1 */
	vdev_ini_cfg->rx_nss[band] =
		QDF_MIN(GET_VDEV_NSS_CHAIN(
				nss_chains_ini_cfg->rx_nss[band],
				nss_chain_shift), max_supported_nss);

	vdev_ini_cfg->tx_nss[band] =
		QDF_MIN(GET_VDEV_NSS_CHAIN(
				nss_chains_ini_cfg->tx_nss[band],
				nss_chain_shift), max_supported_nss);

	vdev_ini_cfg->num_tx_chains_11a =
		QDF_MIN(GET_VDEV_NSS_CHAIN(
				nss_chains_ini_cfg->num_tx_chains_11a,
				nss_chain_shift), rf_chains_supported);

	/* If the fw doesn't support two chains, num rf chains can max be 1 */
	vdev_ini_cfg->num_tx_chains_11b =
		QDF_MIN(GET_VDEV_NSS_CHAIN(
				nss_chains_ini_cfg->num_tx_chains_11b,
				nss_chain_shift), rf_chains_supported);

	vdev_ini_cfg->num_tx_chains_11g =
		QDF_MIN(GET_VDEV_NSS_CHAIN(
				nss_chains_ini_cfg->num_tx_chains_11g,
				nss_chain_shift), rf_chains_supported);

	vdev_ini_cfg->disable_rx_mrc[band] =
				nss_chains_ini_cfg->disable_rx_mrc[band];

	vdev_ini_cfg->disable_tx_mrc[band] =
				nss_chains_ini_cfg->disable_tx_mrc[band];
	/*
	 * Check whether the rx/tx nss is greater than the number of rf chains
	 * supported by FW, if so downgrade the nss to the number of chains
	 * supported, as higher nss cannot be supported with less chains.
	 */
	sme_check_nss_chain_ini_param(vdev_ini_cfg, rf_chains_supported,
				      band);

}

void sme_populate_nss_chain_params(mac_handle_t mac_handle,
				   struct wlan_mlme_nss_chains *vdev_ini_cfg,
				   enum QDF_OPMODE device_mode,
				   uint8_t rf_chains_supported)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	enum nss_chains_band_info band;

	for (band = NSS_CHAINS_BAND_2GHZ; band < NSS_CHAINS_BAND_MAX; band++)
		sme_fill_nss_chain_params(mac_ctx, vdev_ini_cfg,
					  device_mode, band,
					  rf_chains_supported);
}

void
sme_store_nss_chains_cfg_in_vdev(struct wlan_objmgr_vdev *vdev,
				 struct wlan_mlme_nss_chains *vdev_ini_cfg)
{
	struct wlan_mlme_nss_chains *ini_cfg;
	struct wlan_mlme_nss_chains *dynamic_cfg;

	if (!vdev) {
		sme_err("Invalid vdev");
		return;
	}

	ini_cfg = mlme_get_ini_vdev_config(vdev);
	dynamic_cfg = mlme_get_dynamic_vdev_config(vdev);

	if (!ini_cfg || !dynamic_cfg) {
		sme_err("Nss chains ini/dynamic config NULL vdev_id %d",
					     vdev->vdev_objmgr.vdev_id);
		return;
	}

	*ini_cfg = *vdev_ini_cfg;
	*dynamic_cfg = *vdev_ini_cfg;
}

static void
sme_populate_user_config(struct wlan_mlme_nss_chains *dynamic_cfg,
			 struct wlan_mlme_nss_chains *user_cfg,
			 enum nss_chains_band_info band)
{
	if (!user_cfg->num_rx_chains[band])
		user_cfg->num_rx_chains[band] =
			dynamic_cfg->num_rx_chains[band];

	if (!user_cfg->num_tx_chains[band])
		user_cfg->num_tx_chains[band] =
			dynamic_cfg->num_tx_chains[band];

	if (!user_cfg->rx_nss[band])
		user_cfg->rx_nss[band] =
			dynamic_cfg->rx_nss[band];

	if (!user_cfg->tx_nss[band])
		user_cfg->tx_nss[band] =
			dynamic_cfg->tx_nss[band];

	if (!user_cfg->num_tx_chains_11a)
		user_cfg->num_tx_chains_11a =
			dynamic_cfg->num_tx_chains_11a;

	if (!user_cfg->num_tx_chains_11b)
		user_cfg->num_tx_chains_11b =
			dynamic_cfg->num_tx_chains_11b;

	if (!user_cfg->num_tx_chains_11g)
		user_cfg->num_tx_chains_11g =
			dynamic_cfg->num_tx_chains_11g;

	if (!user_cfg->disable_rx_mrc[band])
		user_cfg->disable_rx_mrc[band] =
			dynamic_cfg->disable_rx_mrc[band];

	if (!user_cfg->disable_tx_mrc[band])
		user_cfg->disable_tx_mrc[band] =
			dynamic_cfg->disable_tx_mrc[band];
}

static QDF_STATUS
sme_validate_from_ini_config(struct wlan_mlme_nss_chains *user_cfg,
			     struct wlan_mlme_nss_chains *ini_cfg,
			     enum nss_chains_band_info band)
{
	if (user_cfg->num_rx_chains[band] >
	    ini_cfg->num_rx_chains[band])
		return QDF_STATUS_E_FAILURE;

	if (user_cfg->num_tx_chains[band] >
	    ini_cfg->num_tx_chains[band])
		return QDF_STATUS_E_FAILURE;

	if (user_cfg->rx_nss[band] >
	    ini_cfg->rx_nss[band])
		return QDF_STATUS_E_FAILURE;

	if (user_cfg->tx_nss[band] >
	    ini_cfg->tx_nss[band])
		return QDF_STATUS_E_FAILURE;

	if (user_cfg->num_tx_chains_11a >
	    ini_cfg->num_tx_chains_11a)
		return QDF_STATUS_E_FAILURE;

	if (user_cfg->num_tx_chains_11b >
	    ini_cfg->num_tx_chains_11b)
		return QDF_STATUS_E_FAILURE;

	if (user_cfg->num_tx_chains_11g >
	    ini_cfg->num_tx_chains_11g)
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
sme_validate_user_nss_chain_params(
				    struct wlan_mlme_nss_chains *user_cfg,
				    enum nss_chains_band_info band)
{
	/* Reject as 2x1 modes are not supported in chains yet */

	if (user_cfg->num_tx_chains[band] >
	    user_cfg->num_rx_chains[band])
		return QDF_STATUS_E_FAILURE;

	/* Also if mode is 2x2, we cant have chains as 1x1, or 1x2, or 2x1 */

	if (user_cfg->tx_nss[band] >
	    user_cfg->num_tx_chains[band])
		user_cfg->num_tx_chains[band] =
			user_cfg->tx_nss[band];

	if (user_cfg->rx_nss[band] >
	    user_cfg->num_rx_chains[band])
		user_cfg->num_rx_chains[band] =
			user_cfg->rx_nss[band];

	/*
	 * It may happen that already chains are in 1x1 mode and nss too
	 * is in 1x1 mode, but the tx 11a/b/g chains in user config comes
	 * as 2x1, or 1x2 which cannot support respective mode, as tx chains
	 * for respective band have max of 1x1 only, so these cannot exceed
	 * respective band num tx chains.
	 */

	if (user_cfg->num_tx_chains_11a >
	    user_cfg->num_tx_chains[NSS_CHAINS_BAND_5GHZ])
		user_cfg->num_tx_chains_11a =
			user_cfg->num_tx_chains[NSS_CHAINS_BAND_5GHZ];

	if (user_cfg->num_tx_chains_11b >
	    user_cfg->num_tx_chains[NSS_CHAINS_BAND_2GHZ])
		user_cfg->num_tx_chains_11b =
			user_cfg->num_tx_chains[NSS_CHAINS_BAND_2GHZ];

	if (user_cfg->num_tx_chains_11g >
	    user_cfg->num_tx_chains[NSS_CHAINS_BAND_2GHZ])
		user_cfg->num_tx_chains_11g =
			user_cfg->num_tx_chains[NSS_CHAINS_BAND_2GHZ];

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
sme_validate_nss_chains_config(struct wlan_objmgr_vdev *vdev,
			       struct wlan_mlme_nss_chains *user_cfg,
			       struct wlan_mlme_nss_chains *dynamic_cfg)
{
	enum nss_chains_band_info band;
	struct wlan_mlme_nss_chains *ini_cfg;
	QDF_STATUS status;

	ini_cfg = mlme_get_ini_vdev_config(vdev);
	if (!ini_cfg) {
		sme_err("nss chain ini config NULL");
		return QDF_STATUS_E_FAILURE;
	}

	for (band = NSS_CHAINS_BAND_2GHZ; band < NSS_CHAINS_BAND_MAX; band++) {
		sme_populate_user_config(dynamic_cfg,
					 user_cfg, band);
		status = sme_validate_from_ini_config(user_cfg,
						      ini_cfg,
						      band);
		if (QDF_IS_STATUS_ERROR(status)) {
			sme_err("Validation from ini config failed");
			return QDF_STATUS_E_FAILURE;
		}
		status = sme_validate_user_nss_chain_params(user_cfg,
							    band);
		if (QDF_IS_STATUS_ERROR(status)) {
			sme_err("User cfg validation failed");
			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_SUCCESS;
}

static bool
sme_is_nss_update_allowed(struct wlan_mlme_chain_cfg chain_cfg,
			  uint8_t rx_nss, uint8_t tx_nss,
			  enum nss_chains_band_info band)
{
	switch (band) {
	case NSS_CHAINS_BAND_2GHZ:
		if (rx_nss > chain_cfg.max_rx_chains_2g)
			return false;
		if (tx_nss > chain_cfg.max_tx_chains_2g)
			return false;
		break;
	case NSS_CHAINS_BAND_5GHZ:
		if (rx_nss > chain_cfg.max_rx_chains_5g)
			return false;
		if (tx_nss > chain_cfg.max_tx_chains_5g)
			return false;
		break;
	default:
		sme_err("Unknown Band nss change not allowed");
		return false;
	}
	return true;
}

static void sme_modify_chains_in_mlme_cfg(mac_handle_t mac_handle,
					  uint8_t rx_chains,
					  uint8_t tx_chains,
					  enum QDF_OPMODE vdev_op_mode,
					  enum nss_chains_band_info band)
{
	uint8_t nss_shift;
	uint32_t nss_mask = 0x7;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	nss_shift = sme_get_nss_chain_shift(vdev_op_mode);

	mac_ctx->mlme_cfg->nss_chains_ini_cfg.num_rx_chains[band] &=
						~(nss_mask << nss_shift);
	mac_ctx->mlme_cfg->nss_chains_ini_cfg.num_rx_chains[band] |=
						 (rx_chains << nss_shift);
	mac_ctx->mlme_cfg->nss_chains_ini_cfg.num_tx_chains[band] &=
						~(nss_mask << nss_shift);
	mac_ctx->mlme_cfg->nss_chains_ini_cfg.num_tx_chains[band] |=
						 (tx_chains << nss_shift);
	sme_debug("rx chains %d tx chains %d changed for vdev mode %d for band %d",
		  rx_chains, tx_chains, vdev_op_mode, band);
}

static void
sme_modify_nss_in_mlme_cfg(mac_handle_t mac_handle,
			   uint8_t rx_nss, uint8_t tx_nss,
			   enum QDF_OPMODE vdev_op_mode,
			   enum nss_chains_band_info band)
{
	uint8_t nss_shift;
	uint32_t nss_mask = 0x7;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	if (!sme_is_nss_update_allowed(mac_ctx->fw_chain_cfg, rx_nss, tx_nss,
				       band)) {
		sme_debug("Nss modification failed, fw doesn't support this nss %d",
			  rx_nss);
		return;
	}

	nss_shift = sme_get_nss_chain_shift(vdev_op_mode);

	mac_ctx->mlme_cfg->nss_chains_ini_cfg.rx_nss[band] &=
						~(nss_mask << nss_shift);
	mac_ctx->mlme_cfg->nss_chains_ini_cfg.rx_nss[band] |=
						 (rx_nss << nss_shift);
	mac_ctx->mlme_cfg->nss_chains_ini_cfg.tx_nss[band] &=
						~(nss_mask << nss_shift);
	mac_ctx->mlme_cfg->nss_chains_ini_cfg.tx_nss[band] |=
						 (tx_nss << nss_shift);
	sme_debug("rx nss %d tx nss %d changed for vdev mode %d for band %d",
		  rx_nss, tx_nss, vdev_op_mode, band);
}

void
sme_modify_nss_chains_tgt_cfg(mac_handle_t mac_handle,
			      enum QDF_OPMODE vdev_op_mode,
			      enum nss_chains_band_info band)
{
	uint8_t ini_rx_nss;
	uint8_t ini_tx_nss;
	uint8_t max_supported_rx_nss = MAX_VDEV_NSS;
	uint8_t max_supported_tx_nss = MAX_VDEV_NSS;
	uint8_t ini_rx_chains;
	uint8_t ini_tx_chains;
	uint8_t max_supported_rx_chains = MAX_VDEV_CHAINS;
	uint8_t max_supported_tx_chains = MAX_VDEV_CHAINS;

	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct wlan_mlme_nss_chains *nss_chains_ini_cfg =
					&mac_ctx->mlme_cfg->nss_chains_ini_cfg;
	uint8_t nss_shift = sme_get_nss_chain_shift(vdev_op_mode);
	struct wlan_mlme_chain_cfg chain_cfg = mac_ctx->fw_chain_cfg;

	ini_rx_nss = GET_VDEV_NSS_CHAIN(nss_chains_ini_cfg->rx_nss[band],
					nss_shift);
	ini_tx_nss = GET_VDEV_NSS_CHAIN(nss_chains_ini_cfg->tx_nss[band],
					nss_shift);

	if (band == NSS_CHAINS_BAND_2GHZ) {
		max_supported_rx_nss = chain_cfg.max_rx_chains_2g;
		max_supported_tx_nss = chain_cfg.max_tx_chains_2g;
	} else if (band == NSS_CHAINS_BAND_5GHZ) {
		max_supported_rx_nss = chain_cfg.max_rx_chains_5g;
		max_supported_tx_nss = chain_cfg.max_tx_chains_5g;
	}

	max_supported_rx_nss = QDF_MIN(ini_rx_nss, max_supported_rx_nss);
	if (!max_supported_rx_nss)
		return;

	max_supported_tx_nss = QDF_MIN(ini_tx_nss, max_supported_tx_nss);
	if (!max_supported_tx_nss)
		return;

	ini_rx_chains = GET_VDEV_NSS_CHAIN(nss_chains_ini_cfg->
						num_rx_chains[band],
					   nss_shift);
	ini_tx_chains = GET_VDEV_NSS_CHAIN(nss_chains_ini_cfg->
						num_tx_chains[band],
					   nss_shift);

	if (band == NSS_CHAINS_BAND_2GHZ) {
		max_supported_rx_chains = chain_cfg.max_rx_chains_2g;
		max_supported_tx_chains = chain_cfg.max_tx_chains_2g;
	} else if (band == NSS_CHAINS_BAND_5GHZ) {
		max_supported_rx_chains = chain_cfg.max_rx_chains_5g;
		max_supported_tx_chains = chain_cfg.max_tx_chains_5g;
	}

	max_supported_rx_chains = QDF_MIN(ini_rx_chains,
					  max_supported_rx_chains);
	if (!max_supported_rx_chains)
		return;

	max_supported_tx_chains = QDF_MIN(ini_tx_chains,
					  max_supported_tx_chains);
	if (!max_supported_tx_chains)
		return;

	sme_modify_chains_in_mlme_cfg(mac_handle, max_supported_rx_chains,
				      max_supported_tx_chains, vdev_op_mode,
				      band);
	sme_modify_nss_in_mlme_cfg(mac_handle, max_supported_rx_nss,
				   max_supported_tx_nss, vdev_op_mode, band);
}

void
sme_update_nss_in_mlme_cfg(mac_handle_t mac_handle,
			   uint8_t rx_nss, uint8_t tx_nss,
			   enum QDF_OPMODE vdev_op_mode,
			   enum nss_chains_band_info band)
{
	/*
	 * If device mode is P2P-DEVICE, then we want P2P to come in that
	 * particular nss, then we should change the nss of P@P-CLI, and GO
	 * and we are unaware that for what will be the device mode after
	 * negotiation yet.
	 */

	if (vdev_op_mode == QDF_P2P_DEVICE_MODE ||
	    vdev_op_mode == QDF_P2P_CLIENT_MODE ||
	    vdev_op_mode == QDF_P2P_GO_MODE) {
		sme_modify_nss_in_mlme_cfg(mac_handle, rx_nss, tx_nss,
					   QDF_P2P_CLIENT_MODE, band);
		sme_modify_nss_in_mlme_cfg(mac_handle, rx_nss, tx_nss,
					   QDF_P2P_GO_MODE, band);
		sme_modify_nss_in_mlme_cfg(mac_handle, rx_nss, tx_nss,
					   QDF_P2P_DEVICE_MODE, band);
	} else
		sme_modify_nss_in_mlme_cfg(mac_handle, rx_nss, tx_nss,
					   vdev_op_mode, band);
}

QDF_STATUS
sme_nss_chains_update(mac_handle_t mac_handle,
		      struct wlan_mlme_nss_chains *user_cfg,
		      uint8_t vdev_id)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	QDF_STATUS status;
	struct wlan_mlme_nss_chains *dynamic_cfg;
	struct wlan_objmgr_vdev *vdev =
		       wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
							    vdev_id,
							    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("Got NULL vdev obj, returning");
		return QDF_STATUS_E_FAILURE;
	}

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_IS_STATUS_ERROR(status))
		goto release_ref;

	dynamic_cfg = mlme_get_dynamic_vdev_config(vdev);
	if (!dynamic_cfg) {
		sme_err("nss chain dynamic config NULL");
		status = QDF_STATUS_E_FAILURE;
		goto release_lock;
	}

	status = sme_validate_nss_chains_config(vdev, user_cfg,
						dynamic_cfg);
	if (QDF_IS_STATUS_ERROR(status))
		goto release_lock;

	if (!qdf_mem_cmp(dynamic_cfg, user_cfg,
			 sizeof(struct wlan_mlme_nss_chains))) {
		sme_debug("current config same as user config");
		status = QDF_STATUS_SUCCESS;
		goto release_lock;
	}
	sme_debug("User params verified, sending to fw vdev id %d", vdev_id);

	status = wma_vdev_nss_chain_params_send(vdev->vdev_objmgr.vdev_id,
						user_cfg);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("params sent failed to fw vdev id %d", vdev_id);
		goto release_lock;
	}

	*dynamic_cfg = *user_cfg;

release_lock:
	sme_release_global_lock(&mac_ctx->sme);

release_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
	return status;
}

#ifdef WLAN_FEATURE_11AX
static void sme_update_bfer_he_cap(struct wma_tgt_cfg *cfg)
{
	cfg->he_cap_5g.su_beamformer = 0;
}
#else
static void sme_update_bfer_he_cap(struct wma_tgt_cfg *cfg)
{
}
#endif

#ifdef WLAN_FEATURE_11BE
void sme_update_bfer_eht_cap(struct wma_tgt_cfg *cfg)
{
	cfg->eht_cap_5g.su_beamformer = 0;
}
#else
static void sme_update_bfer_eht_cap(struct wma_tgt_cfg *cfg)
{
}
#endif

void sme_update_bfer_caps_as_per_nss_chains(mac_handle_t mac_handle,
					    struct wma_tgt_cfg *cfg)
{
	uint8_t max_supported_tx_chains = MAX_VDEV_CHAINS;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct wlan_mlme_nss_chains *nss_chains_ini_cfg =
					&mac_ctx->mlme_cfg->nss_chains_ini_cfg;
	uint8_t ini_tx_chains;

	ini_tx_chains = GET_VDEV_NSS_CHAIN(
			nss_chains_ini_cfg->num_tx_chains[NSS_CHAINS_BAND_5GHZ],
			SAP_NSS_CHAINS_SHIFT);

	max_supported_tx_chains =
			mac_ctx->fw_chain_cfg.max_tx_chains_5g;

	max_supported_tx_chains = QDF_MIN(ini_tx_chains,
					  max_supported_tx_chains);
	if (!max_supported_tx_chains)
		return;

	if (max_supported_tx_chains == 1) {
		sme_debug("ini support %d and firmware support %d",
			  ini_tx_chains,
			  mac_ctx->fw_chain_cfg.max_tx_chains_5g);
		if (mac_ctx->fw_chain_cfg.max_tx_chains_5g == 1) {
			cfg->vht_cap.vht_su_bformer = 0;
			sme_update_bfer_he_cap(cfg);
			sme_update_bfer_eht_cap(cfg);
		}
		mac_ctx->mlme_cfg->vht_caps.vht_cap_info.su_bformer = 0;
		mac_ctx->mlme_cfg->vht_caps.vht_cap_info.num_soundingdim = 0;
		mac_ctx->mlme_cfg->vht_caps.vht_cap_info.mu_bformer = 0;
	}
}

QDF_STATUS sme_vdev_post_vdev_create_setup(mac_handle_t mac_handle,
					   struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	uint8_t vdev_id;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme) {
		sme_err("Failed to get vdev mlme obj!");
		QDF_BUG(0);
		return status;
	}

	vdev_id = wlan_vdev_get_id(vdev);
	status = wma_post_vdev_create_setup(vdev);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Failed to setup wma for vdev: %d", vdev_id);
		return status;
	}

	status = csr_setup_vdev_session(vdev_mlme);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Failed to setup CSR layer for vdev: %d", vdev_id);
		goto cleanup_wma;
	}

	status = mlme_vdev_self_peer_create(vdev);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Failed to create vdev selfpeer for vdev:%d", vdev_id);
		goto csr_cleanup_vdev_session;
	}

	return status;

csr_cleanup_vdev_session:
	csr_cleanup_vdev_session(mac_ctx, vdev_id);
cleanup_wma:
	wma_cleanup_vdev(vdev);
	return status;
}

struct wlan_objmgr_vdev
*sme_vdev_create(mac_handle_t mac_handle,
		 struct wlan_vdev_create_params *vdev_params)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct wlan_objmgr_vdev *vdev;

	sme_debug("addr:"QDF_MAC_ADDR_FMT" opmode:%d",
		  QDF_MAC_ADDR_REF(vdev_params->macaddr),
		  vdev_params->opmode);

	vdev = wlan_objmgr_vdev_obj_create(mac_ctx->pdev, vdev_params);
	if (!vdev) {
		sme_err("Failed to create vdev object");
		return NULL;
	}

	if (wlan_objmgr_vdev_try_get_ref(vdev, WLAN_LEGACY_SME_ID) !=
	    QDF_STATUS_SUCCESS) {
		wlan_objmgr_vdev_obj_delete(vdev);
		return NULL;
	}

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_OPEN_SESSION,
			 wlan_vdev_get_id(vdev), 0));

	return vdev;
}

void sme_vdev_del_resp(uint8_t vdev_id)
{
	mac_handle_t mac_handle;
	struct mac_context *mac;

	mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	if (!mac_handle) {
		QDF_ASSERT(0);
		return;
	}

	mac = MAC_CONTEXT(mac_handle);
	csr_cleanup_vdev_session(mac, vdev_id);

	if (mac->session_close_cb)
		mac->session_close_cb(vdev_id);
}

QDF_STATUS sme_vdev_self_peer_delete_resp(struct del_vdev_params *del_vdev_req)
{
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status;

	vdev = del_vdev_req->vdev;
	if (!vdev) {
		qdf_mem_free(del_vdev_req);
		return QDF_STATUS_E_INVAL;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	status = del_vdev_req->status;
	qdf_mem_free(del_vdev_req);
	return status;
}

QDF_STATUS sme_vdev_delete(mac_handle_t mac_handle,
			   struct wlan_objmgr_vdev *vdev)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	struct scheduler_msg self_peer_delete_msg = {0};
	struct del_vdev_params *del_self_peer;

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_CLOSE_SESSION, vdev_id, 0));

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = csr_prepare_vdev_delete(mac, vdev_id, false);
		sme_release_global_lock(&mac->sme);
	}

	/*
	 * While this vdev delete is invoked we will still have following
	 * references held:
	 * WLAN_LEGACY_WMA_ID -- 1
	 * WLAN_LEGACY_SME_ID -- 1
	 * WLAN_OBJMGR_ID -- 2
	 * Following message will release the self and delete the self peer
	 * and release the wma references so the objmgr and wma_legacy will be
	 * released because of this.
	 *
	 * In the message callback the legacy_sme reference will be released
	 * resulting in the last reference of vdev object and sending the
	 * vdev_delete to firmware.
	 */
	status = wlan_objmgr_vdev_obj_delete(vdev);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_nofl_err("Failed to mark vdev as logical delete %d",
			     status);
		return status;
	}

	del_self_peer = qdf_mem_malloc(sizeof(*del_self_peer));
	if (!del_self_peer)
		return QDF_STATUS_E_NOMEM;

	del_self_peer->vdev = vdev;
	del_self_peer->vdev_id = wlan_vdev_get_id(vdev);
	qdf_mem_copy(del_self_peer->self_mac_addr,
		     wlan_vdev_mlme_get_macaddr(vdev), sizeof(tSirMacAddr));

	self_peer_delete_msg.bodyptr = del_self_peer;
	self_peer_delete_msg.callback = mlme_vdev_self_peer_delete;
	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_MLME,
					QDF_MODULE_ID_TARGET_IF,
					&self_peer_delete_msg);

	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Failed to post vdev selfpeer for vdev:%d", vdev_id);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		qdf_mem_free(del_self_peer);
	}

	return status;
}

void sme_cleanup_session(mac_handle_t mac_handle, uint8_t vdev_id)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status))
		return;
	csr_cleanup_vdev_session(mac, vdev_id);
	sme_release_global_lock(&mac->sme);
}

/*
 * sme_change_mcc_beacon_interval() -
 * To update P2P-GO beaconInterval. This function should be called after
 *    disassociating all the station is done
 *   This is an asynchronous API.
 *
 * @sessionId: Session Identifier
 * Return QDF_STATUS  SUCCESS
 *			FAILURE or RESOURCES
 *			The API finished and failed.
 */
QDF_STATUS sme_change_mcc_beacon_interval(uint8_t sessionId)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac_ctx = sme_get_mac_context();

	if (!mac_ctx) {
		sme_err("mac_ctx is NULL");
		return status;
	}
	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = csr_send_chng_mcc_beacon_interval(mac_ctx,
							   sessionId);
		sme_release_global_lock(&mac_ctx->sme);
	}
	return status;
}

/**
 * sme_set_host_offload(): API to set the host offload feature.
 * @mac_handle: The handle returned by mac_open.
 * @sessionId: Session Identifier
 * @request: Pointer to the offload request.
 *
 * Return QDF_STATUS
 */
QDF_STATUS sme_set_host_offload(mac_handle_t mac_handle, uint8_t sessionId,
				struct sir_host_offload_req *request)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_SET_HOSTOFFLOAD, sessionId, 0));
	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
#ifdef WLAN_NS_OFFLOAD
		if (SIR_IPV6_NS_OFFLOAD == request->offloadType) {
			status = sme_set_ps_ns_offload(mac_handle, request,
					sessionId);
		} else
#endif /* WLAN_NS_OFFLOAD */
		{
			status = sme_set_ps_host_offload(mac_handle, request,
					sessionId);
		}
		sme_release_global_lock(&mac->sme);
	}

	return status;
}

/*
 * sme_set_keep_alive() -
 * API to set the Keep Alive feature.
 *
 * mac_handle - The handle returned by mac_open.
 * request -  Pointer to the Keep Alive request.
 * Return QDF_STATUS
 */
QDF_STATUS sme_set_keep_alive(mac_handle_t mac_handle, uint8_t session_id,
			      struct keep_alive_req *request)
{
	struct keep_alive_req *request_buf;
	struct scheduler_msg msg = {0};
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, session_id);

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			FL("WMA_SET_KEEP_ALIVE message"));

	if (!pSession) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				FL("Session not Found"));
		return QDF_STATUS_E_FAILURE;
	}
	request_buf = qdf_mem_malloc(sizeof(*request_buf));
	if (!request_buf)
		return QDF_STATUS_E_NOMEM;

	qdf_copy_macaddr(&request->bssid, &pSession->connectedProfile.bssid);
	qdf_mem_copy(request_buf, request, sizeof(*request_buf));

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			"buff TP %d input TP %d ", request_buf->timePeriod,
		  request->timePeriod);
	request_buf->sessionId = session_id;

	msg.type = WMA_SET_KEEP_ALIVE;
	msg.reserved = 0;
	msg.bodyptr = request_buf;
	MTRACE(qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
			 session_id, msg.type));
	if (QDF_STATUS_SUCCESS !=
			scheduler_post_message(QDF_MODULE_ID_SME,
					       QDF_MODULE_ID_WMA,
					       QDF_MODULE_ID_WMA, &msg)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			"Not able to post WMA_SET_KEEP_ALIVE message to WMA");
		qdf_mem_free(request_buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/*
 * sme_get_operation_channel() -
 * API to get current channel on which STA is parked his function gives
 * channel information only of infra station
 *
 * mac_handle, pointer to memory location and sessionId
 * Returns QDF_STATUS_SUCCESS
 *	     QDF_STATUS_E_FAILURE
 */
QDF_STATUS sme_get_operation_channel(mac_handle_t mac_handle,
				     uint32_t *chan_freq,
				     uint8_t sessionId)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *pSession;

	if (CSR_IS_SESSION_VALID(mac, sessionId)) {
		pSession = CSR_GET_SESSION(mac, sessionId);

		if ((pSession->connectedProfile.BSSType ==
		     eCSR_BSS_TYPE_INFRASTRUCTURE)
		    || (pSession->connectedProfile.BSSType ==
			eCSR_BSS_TYPE_INFRA_AP)) {
			*chan_freq = pSession->connectedProfile.op_freq;
			return QDF_STATUS_SUCCESS;
		}
	}
	return QDF_STATUS_E_FAILURE;
} /* sme_get_operation_channel ends here */

/**
 * sme_register_mgmt_frame_ind_callback() - Register a callback for
 * management frame indication to PE.
 *
 * @mac_handle: Opaque handle to the global MAC context
 * @callback: callback pointer to be registered
 *
 * This function is used to register a callback for management
 * frame indication to PE.
 *
 * Return: Success if msg is posted to PE else Failure.
 */
QDF_STATUS sme_register_mgmt_frame_ind_callback(mac_handle_t mac_handle,
				sir_mgmt_frame_ind_callback callback)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct sir_sme_mgmt_frame_cb_req *msg;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (QDF_STATUS_SUCCESS ==
			sme_acquire_global_lock(&mac_ctx->sme)) {
		msg = qdf_mem_malloc(sizeof(*msg));
		if (!msg) {
			sme_release_global_lock(&mac_ctx->sme);
			return QDF_STATUS_E_NOMEM;
		}
		msg->message_type = eWNI_SME_REGISTER_MGMT_FRAME_CB;
		msg->length          = sizeof(*msg);

		msg->callback = callback;
		status = umac_send_mb_message_to_mac(msg);
		sme_release_global_lock(&mac_ctx->sme);
		return status;
	}
	return QDF_STATUS_E_FAILURE;
}

/*
 * sme_RegisterMgtFrame() -
 * To register management frame of specified type and subtype.
 *
 * frameType - type of the frame that needs to be passed to HDD.
 * matchData - data which needs to be matched before passing frame
 *		       to HDD.
 * matchDataLen - Length of matched data.
 * Return QDF_STATUS
 */
QDF_STATUS sme_register_mgmt_frame(mac_handle_t mac_handle, uint8_t sessionId,
				   uint16_t frameType, uint8_t *matchData,
				   uint16_t matchLen)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		struct register_mgmt_frame *pMsg;
		uint16_t len;
		struct csr_roam_session *pSession = CSR_GET_SESSION(mac,
							sessionId);

		if (!CSR_IS_SESSION_ANY(sessionId) && !pSession) {
			sme_err("Session %d not found",	sessionId);
			sme_release_global_lock(&mac->sme);
			return QDF_STATUS_E_FAILURE;
		}

		if (!CSR_IS_SESSION_ANY(sessionId) &&
						!pSession->sessionActive) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				  "%s Invalid Sessionid", __func__);
			sme_release_global_lock(&mac->sme);
			return QDF_STATUS_E_FAILURE;
		}

		len = sizeof(*pMsg) + matchLen;

		pMsg = qdf_mem_malloc(len);
		if (!pMsg)
			status = QDF_STATUS_E_NOMEM;
		else {
			pMsg->messageType = eWNI_SME_REGISTER_MGMT_FRAME_REQ;
			pMsg->length = len;
			pMsg->sessionId = sessionId;
			pMsg->registerFrame = true;
			pMsg->frameType = frameType;
			pMsg->matchLen = matchLen;
			qdf_mem_copy(pMsg->matchData, matchData, matchLen);
			status = umac_send_mb_message_to_mac(pMsg);
		}
		sme_release_global_lock(&mac->sme);
	}
	return status;
}

/*
 * sme_DeregisterMgtFrame() -
 * To De-register management frame of specified type and subtype.
 *
 * frameType - type of the frame that needs to be passed to HDD.
 * matchData - data which needs to be matched before passing frame
 *		       to HDD.
 * matchDataLen - Length of matched data.
 * Return QDF_STATUS
 */
QDF_STATUS sme_deregister_mgmt_frame(mac_handle_t mac_handle, uint8_t sessionId,
				     uint16_t frameType, uint8_t *matchData,
				     uint16_t matchLen)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_DEREGISTER_MGMTFR, sessionId,
			 0));
	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		struct register_mgmt_frame *pMsg;
		uint16_t len;
		struct csr_roam_session *pSession = CSR_GET_SESSION(mac,
							sessionId);

		if (!CSR_IS_SESSION_ANY(sessionId) && !pSession) {
			sme_err("Session %d not found",	sessionId);
			sme_release_global_lock(&mac->sme);
			return QDF_STATUS_E_FAILURE;
		}

		if (!CSR_IS_SESSION_ANY(sessionId) &&
						!pSession->sessionActive) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				  "%s Invalid Sessionid", __func__);
			sme_release_global_lock(&mac->sme);
			return QDF_STATUS_E_FAILURE;
		}

		len = sizeof(*pMsg) + matchLen;

		pMsg = qdf_mem_malloc(len);
		if (!pMsg)
			status = QDF_STATUS_E_NOMEM;
		else {
			pMsg->messageType = eWNI_SME_REGISTER_MGMT_FRAME_REQ;
			pMsg->length = len;
			pMsg->registerFrame = false;
			pMsg->frameType = frameType;
			pMsg->matchLen = matchLen;
			qdf_mem_copy(pMsg->matchData, matchData, matchLen);
			status = umac_send_mb_message_to_mac(pMsg);
		}
		sme_release_global_lock(&mac->sme);
	}
	return status;
}

/**
 * sme_prepare_mgmt_tx() - Prepares mgmt frame
 * @mac_handle: The handle returned by mac_open
 * @vdev_id: vdev id
 * @buf: pointer to frame
 * @len: frame length
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sme_prepare_mgmt_tx(mac_handle_t mac_handle,
				      uint8_t vdev_id,
				      const uint8_t *buf, uint32_t len)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct sir_mgmt_msg *msg;
	uint16_t msg_len;
	struct scheduler_msg sch_msg = {0};

	sme_debug("prepares auth frame");

	msg_len = sizeof(*msg) + len;
	msg = qdf_mem_malloc(msg_len);
	if (!msg) {
		status = QDF_STATUS_E_NOMEM;
	} else {
		msg->type = eWNI_SME_SEND_MGMT_FRAME_TX;
		msg->msg_len = msg_len;
		msg->vdev_id = vdev_id;
		msg->data = (uint8_t *)msg + sizeof(*msg);
		qdf_mem_copy(msg->data, buf, len);

		sch_msg.type = eWNI_SME_SEND_MGMT_FRAME_TX;
		sch_msg.bodyptr = msg;
		status = scheduler_post_message(QDF_MODULE_ID_SME,
						QDF_MODULE_ID_PE,
						QDF_MODULE_ID_PE, &sch_msg);
	}
	return status;
}

QDF_STATUS sme_send_mgmt_tx(mac_handle_t mac_handle, uint8_t session_id,
			    const uint8_t *buf, uint32_t len)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = sme_prepare_mgmt_tx(mac_handle, session_id, buf, len);
		sme_release_global_lock(&mac->sme);
	}

	return status;
}

#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
/**
 * sme_configure_ext_wow() - configure Extr WoW
 * @mac_handle - The handle returned by mac_open.
 * @wlanExtParams - Depicts the wlan Ext params.
 * @callback - ext_wow callback to be registered.
 * @callback_context - ext_wow callback context
 *
 * SME will pass this request to lower mac to configure Extr WoW
 * Return: QDF_STATUS
 */
QDF_STATUS sme_configure_ext_wow(mac_handle_t mac_handle,
				 tpSirExtWoWParams wlanExtParams,
				 csr_readyToExtWoWCallback callback,
				 void *callback_context)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};
	tpSirExtWoWParams MsgPtr = qdf_mem_malloc(sizeof(*MsgPtr));

	if (!MsgPtr)
		return QDF_STATUS_E_NOMEM;

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_CONFIG_EXTWOW, NO_SESSION, 0));

	mac->readyToExtWoWCallback = callback;
	mac->readyToExtWoWContext = callback_context;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {

		/* serialize the req through MC thread */
		qdf_mem_copy(MsgPtr, wlanExtParams, sizeof(*MsgPtr));
		message.bodyptr = MsgPtr;
		message.type = WMA_WLAN_EXT_WOW;
		qdf_status = scheduler_post_message(QDF_MODULE_ID_SME,
						    QDF_MODULE_ID_WMA,
						    QDF_MODULE_ID_WMA,
						    &message);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			mac->readyToExtWoWCallback = NULL;
			mac->readyToExtWoWContext = NULL;
			qdf_mem_free(MsgPtr);
			status = QDF_STATUS_E_FAILURE;
		}
		sme_release_global_lock(&mac->sme);
	} else {
		mac->readyToExtWoWCallback = NULL;
		mac->readyToExtWoWContext = NULL;
		qdf_mem_free(MsgPtr);
	}

	return status;
}

/*
 * sme_configure_app_type1_params() -
 * SME will pass this request to lower mac to configure Indoor WoW parameters.
 *
 * mac_handle - The handle returned by mac_open.
 * wlanAppType1Params- Depicts the wlan App Type 1(Indoor) params
 * Return QDF_STATUS
 */
QDF_STATUS sme_configure_app_type1_params(mac_handle_t mac_handle,
					  tpSirAppType1Params wlanAppType1Params)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};
	tpSirAppType1Params MsgPtr = qdf_mem_malloc(sizeof(*MsgPtr));

	if (!MsgPtr)
		return QDF_STATUS_E_NOMEM;

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_CONFIG_APP_TYPE1, NO_SESSION,
			 0));

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		/* serialize the req through MC thread */
		qdf_mem_copy(MsgPtr, wlanAppType1Params, sizeof(*MsgPtr));
		message.bodyptr = MsgPtr;
		message.type = WMA_WLAN_SET_APP_TYPE1_PARAMS;
		qdf_status = scheduler_post_message(QDF_MODULE_ID_SME,
						    QDF_MODULE_ID_WMA,
						    QDF_MODULE_ID_WMA,
						    &message);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			qdf_mem_free(MsgPtr);
			status = QDF_STATUS_E_FAILURE;
		}
		sme_release_global_lock(&mac->sme);
	} else {
		qdf_mem_free(MsgPtr);
	}

	return status;
}

/*
 * sme_configure_app_type2_params() -
 * SME will pass this request to lower mac to configure Indoor WoW parameters.
 *
 * mac_handle - The handle returned by mac_open.
 * wlanAppType2Params- Depicts the wlan App Type 2 (Outdoor) params
 * Return QDF_STATUS
 */
QDF_STATUS sme_configure_app_type2_params(mac_handle_t mac_handle,
					 tpSirAppType2Params wlanAppType2Params)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};
	tpSirAppType2Params MsgPtr = qdf_mem_malloc(sizeof(*MsgPtr));

	if (!MsgPtr)
		return QDF_STATUS_E_NOMEM;

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_CONFIG_APP_TYPE2, NO_SESSION,
			 0));

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		/* serialize the req through MC thread */
		qdf_mem_copy(MsgPtr, wlanAppType2Params, sizeof(*MsgPtr));
		message.bodyptr = MsgPtr;
		message.type = WMA_WLAN_SET_APP_TYPE2_PARAMS;
		qdf_status = scheduler_post_message(QDF_MODULE_ID_SME,
						    QDF_MODULE_ID_WMA,
						    QDF_MODULE_ID_WMA,
						    &message);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			qdf_mem_free(MsgPtr);
			status = QDF_STATUS_E_FAILURE;
		}
		sme_release_global_lock(&mac->sme);
	} else {
		qdf_mem_free(MsgPtr);
	}

	return status;
}
#endif

uint32_t sme_get_beaconing_concurrent_operation_channel(mac_handle_t mac_handle,
							uint8_t vdev_id_to_skip)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	uint32_t chan_freq = 0;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {

		chan_freq = csr_get_beaconing_concurrent_channel(mac,
							       vdev_id_to_skip);
		sme_debug("Other Concurrent Chan_freq: %d skipped vdev_id %d",
			  chan_freq, vdev_id_to_skip);
		sme_release_global_lock(&mac->sme);
	}

	return chan_freq;
}

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
uint16_t sme_check_concurrent_channel_overlap(mac_handle_t mac_handle,
					      uint16_t sap_ch_freq,
					      eCsrPhyMode sapPhyMode,
					      uint8_t cc_switch_mode)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	uint16_t intf_ch_freq = 0;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		intf_ch_freq = csr_check_concurrent_channel_overlap(
			mac, sap_ch_freq, sapPhyMode, cc_switch_mode);
		sme_release_global_lock(&mac->sme);
	}

	return intf_ch_freq;
}
#endif

/**
 * sme_set_tsfcb() - Set callback for TSF capture
 * @mac_handle: Handler return by mac_open
 * @cb_fn: Callback function pointer
 * @db_ctx: Callback data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_set_tsfcb(mac_handle_t mac_handle,
	int (*cb_fn)(void *cb_ctx, struct stsf *ptsf), void *cb_ctx)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		mac->sme.get_tsf_cb = cb_fn;
		mac->sme.get_tsf_cxt = cb_ctx;
		sme_release_global_lock(&mac->sme);
	}
	return status;
}

/**
 * sme_reset_tsfcb() - Reset callback for TSF capture
 * @mac_handle: Handler return by mac_open
 *
 * This function reset the tsf capture callback to SME
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_reset_tsfcb(mac_handle_t mac_handle)
{
	struct mac_context *mac;
	QDF_STATUS status;

	if (!mac_handle) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				  FL("mac_handle is not valid"));
		return QDF_STATUS_E_INVAL;
	}
	mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		mac->sme.get_tsf_cb = NULL;
		mac->sme.get_tsf_cxt = NULL;
		sme_release_global_lock(&mac->sme);
	}
	return status;
}

#if defined(WLAN_FEATURE_TSF) && !defined(WLAN_FEATURE_TSF_PLUS_NOIRQ)
/*
 * sme_set_tsf_gpio() - set gpio pin that be toggled when capture tsf
 * @mac_handle: Handler return by mac_open
 * @pinvalue: gpio pin id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_set_tsf_gpio(mac_handle_t mac_handle, uint32_t pinvalue)
{
	QDF_STATUS status;
	struct scheduler_msg tsf_msg = {0};
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		tsf_msg.type = WMA_TSF_GPIO_PIN;
		tsf_msg.reserved = 0;
		tsf_msg.bodyval = pinvalue;

		status = scheduler_post_message(QDF_MODULE_ID_SME,
						QDF_MODULE_ID_WMA,
						QDF_MODULE_ID_WMA, &tsf_msg);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("Unable to post WMA_TSF_GPIO_PIN");
			status = QDF_STATUS_E_FAILURE;
		}
		sme_release_global_lock(&mac->sme);
	}
	return status;
}
#endif

QDF_STATUS sme_get_cfg_valid_channels(uint32_t *valid_ch_freq, uint32_t *len)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac_ctx = sme_get_mac_context();
	uint32_t *valid_ch_freq_list;
	uint32_t i;

	if (!mac_ctx) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
		FL("Invalid MAC context"));
		return QDF_STATUS_E_FAILURE;
	}

	valid_ch_freq_list = qdf_mem_malloc(CFG_VALID_CHANNEL_LIST_LEN *
					    sizeof(uint32_t));
	if (!valid_ch_freq_list)
		return QDF_STATUS_E_NOMEM;

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = csr_get_cfg_valid_channels(mac_ctx,
			valid_ch_freq_list, len);
		sme_release_global_lock(&mac_ctx->sme);
	}

	for (i = 0; i < *len; i++)
		valid_ch_freq[i] = valid_ch_freq_list[i];

	qdf_mem_free(valid_ch_freq_list);

	return status;
}

static uint8_t *sme_reg_hint_to_str(const enum country_src src)
{
	switch (src) {
	case SOURCE_CORE:
		return "WORLD MODE";

	case SOURCE_DRIVER:
		return "BDF file";

	case SOURCE_USERSPACE:
		return "user-space";

	case SOURCE_11D:
		return "802.11D IEs in beacons";

	default:
		return "unknown";
	}
}

void sme_set_cc_src(mac_handle_t mac_handle, enum country_src cc_src)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	mac_ctx->reg_hint_src = cc_src;

	sme_debug("Country source is %s",
		  sme_reg_hint_to_str(cc_src));
}

/**
 * sme_handle_generic_change_country_code() - handles country ch req
 * @mac_ctx:    mac global context
 * @msg:        request msg packet
 *
 * If Supplicant country code is priority than 11d is disabled.
 * If 11D is enabled, we update the country code after every scan.
 * Hence when Supplicant country code is priority, we don't need 11D info.
 * Country code from Supplicant is set as current country code.
 *
 * Return: status of operation
 */
static QDF_STATUS
sme_handle_generic_change_country_code(struct mac_context *mac_ctx,
				       void *msg_buf)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	/* get the channels based on new cc */
	status = csr_get_channel_and_power_list(mac_ctx);

	if (status != QDF_STATUS_SUCCESS) {
		sme_err("fail to get Channels");
		return status;
	}

	/* reset info based on new cc, and we are done */
	csr_apply_channel_power_info_wrapper(mac_ctx);
	csr_update_beacon(mac_ctx);

	csr_scan_filter_results(mac_ctx);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sme_update_channel_list(mac_handle_t mac_handle)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	QDF_STATUS status;

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		/* Update umac channel (enable/disable) from cds channels */
		status = csr_get_channel_and_power_list(mac_ctx);
		if (status != QDF_STATUS_SUCCESS) {
			sme_err("fail to get Channels");
			sme_release_global_lock(&mac_ctx->sme);
			return status;
		}

		csr_apply_channel_power_info_wrapper(mac_ctx);
		csr_scan_filter_results(mac_ctx);
		sme_disconnect_connected_sessions(mac_ctx,
					REASON_OPER_CHANNEL_USER_DISABLED);
		sme_release_global_lock(&mac_ctx->sme);
	}

	return status;
}

/**
 * sme_search_in_base_ch_freq_lst() - is given ch_freq in base ch freq
 * @mac_ctx: mac global context
 * @chan_freq: current channel freq
 *
 * Return: true if given ch_freq is in base ch freq
 */
static bool sme_search_in_base_ch_freq_lst(
	struct mac_context *mac_ctx, uint32_t chan_freq)
{
	uint8_t i;
	struct csr_channel *ch_lst_info;

	ch_lst_info = &mac_ctx->scan.base_channels;
	for (i = 0; i < ch_lst_info->numChannels; i++) {
		if (ch_lst_info->channel_freq_list[i] == chan_freq)
			return true;
	}

	return false;
}

/**
 * sme_disconnect_connected_sessions() - Disconnect STA and P2P client session
 * if channel is not supported
 * @mac_ctx: mac global context
 * @reason: Mac Disconnect reason code as per @enum wlan_reason_code
 *
 * If new country code does not support the channel on which STA/P2P client
 * is connetced, it sends the disconnect to the AP/P2P GO
 *
 * Return: void
 */
static void sme_disconnect_connected_sessions(struct mac_context *mac_ctx,
					      enum wlan_reason_code reason)
{
	uint8_t session_id, found = false;
	uint32_t chan_freq;

	for (session_id = 0; session_id < WLAN_MAX_VDEVS; session_id++) {
		if (!csr_is_session_client_and_connected(mac_ctx, session_id))
			continue;
		found = false;
		/* Session is connected.Check the channel */
		chan_freq = csr_get_infra_operation_chan_freq(
					mac_ctx, session_id);
		sme_debug("Current Operating channel : %d, session :%d",
			  chan_freq, session_id);
		found = sme_search_in_base_ch_freq_lst(mac_ctx, chan_freq);
		if (!found) {
			sme_debug("Disconnect Session: %d", session_id);
			csr_roam_disconnect(mac_ctx, session_id,
					    eCSR_DISCONNECT_REASON_UNSPECIFIED,
					    reason);
		}
	}
}

#ifdef WLAN_FEATURE_PACKET_FILTERING
QDF_STATUS sme_8023_multicast_list(mac_handle_t mac_handle, uint8_t sessionId,
				   tpSirRcvFltMcAddrList pMulticastAddrs)
{
	tpSirRcvFltMcAddrList request_buf;
	struct scheduler_msg msg = {0};
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *pSession = NULL;

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		"%s: ulMulticastAddrCnt: %d, multicastAddr[0]: %pK", __func__,
		  pMulticastAddrs->ulMulticastAddrCnt,
		  pMulticastAddrs->multicastAddr[0].bytes);

	/* Find the connected Infra / P2P_client connected session */
	pSession = CSR_GET_SESSION(mac, sessionId);
	if (!CSR_IS_SESSION_VALID(mac, sessionId) ||
			(!csr_is_conn_state_infra(mac, sessionId) &&
			 !csr_is_ndi_started(mac, sessionId))) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Unable to find the session Id: %d", __func__,
			  sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	request_buf = qdf_mem_malloc(sizeof(tSirRcvFltMcAddrList));
	if (!request_buf)
		return QDF_STATUS_E_NOMEM;

	if (!csr_is_conn_state_connected_infra(mac, sessionId) &&
			!csr_is_ndi_started(mac, sessionId)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			"%s: Request ignored, session %d is not connected or started",
			__func__, sessionId);
		qdf_mem_free(request_buf);
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_copy(request_buf, pMulticastAddrs,
		     sizeof(tSirRcvFltMcAddrList));

	qdf_copy_macaddr(&request_buf->self_macaddr, &pSession->self_mac_addr);
	qdf_copy_macaddr(&request_buf->bssid,
			 &pSession->connectedProfile.bssid);

	msg.type = WMA_8023_MULTICAST_LIST_REQ;
	msg.reserved = 0;
	msg.bodyptr = request_buf;
	MTRACE(qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
			 sessionId, msg.type));
	if (QDF_STATUS_SUCCESS != scheduler_post_message(QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_WMA,
							 QDF_MODULE_ID_WMA,
							 &msg)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Not able to post WMA_8023_MULTICAST_LIST message to WMA",
			  __func__);
		qdf_mem_free(request_buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_FEATURE_PACKET_FILTERING */

bool sme_is_channel_valid(mac_handle_t mac_handle, uint32_t chan_freq)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	bool valid = false;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {

		valid = csr_roam_is_channel_valid(mac, chan_freq);

		sme_release_global_lock(&mac->sme);
	}

	return valid;
}

/*
 * sme_set_max_tx_power_per_band() -
 * Set the Maximum Transmit Power specific to band dynamically.
 *   Note: this setting will not persist over reboots.
 *
 * band
 * power to set in dB
 * Return QDF_STATUS
 */
QDF_STATUS sme_set_max_tx_power_per_band(enum band_info band, int8_t dB)
{
	struct scheduler_msg msg = {0};
	tpMaxTxPowerPerBandParams pMaxTxPowerPerBandParams = NULL;

	pMaxTxPowerPerBandParams =
		qdf_mem_malloc(sizeof(tMaxTxPowerPerBandParams));
	if (!pMaxTxPowerPerBandParams)
		return QDF_STATUS_E_NOMEM;

	pMaxTxPowerPerBandParams->power = dB;
	pMaxTxPowerPerBandParams->bandInfo = band;

	msg.type = WMA_SET_MAX_TX_POWER_PER_BAND_REQ;
	msg.reserved = 0;
	msg.bodyptr = pMaxTxPowerPerBandParams;
	MTRACE(qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
			 NO_SESSION, msg.type));
	if (QDF_STATUS_SUCCESS != scheduler_post_message(QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_WMA,
							 QDF_MODULE_ID_WMA,
							 &msg)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s:Not able to post WMA_SET_MAX_TX_POWER_PER_BAND_REQ",
			  __func__);
		qdf_mem_free(pMaxTxPowerPerBandParams);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/*
 * sme_set_max_tx_power() -
 * Set the Maximum Transmit Power dynamically. Note: this setting will
 *   not persist over reboots.
 *
 * mac_handle
 * pBssid  BSSID to set the power cap for
 * pBssid  pSelfMacAddress self MAC Address
 * pBssid  power to set in dB
 * Return QDF_STATUS
 */
QDF_STATUS sme_set_max_tx_power(mac_handle_t mac_handle,
				struct qdf_mac_addr pBssid,
				struct qdf_mac_addr pSelfMacAddress, int8_t dB)
{
	struct scheduler_msg msg = {0};
	tpMaxTxPowerParams pMaxTxParams = NULL;

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_SET_MAXTXPOW, NO_SESSION, 0));
	pMaxTxParams = qdf_mem_malloc(sizeof(tMaxTxPowerParams));
	if (!pMaxTxParams)
		return QDF_STATUS_E_NOMEM;

	qdf_copy_macaddr(&pMaxTxParams->bssId, &pBssid);
	qdf_copy_macaddr(&pMaxTxParams->selfStaMacAddr, &pSelfMacAddress);
	pMaxTxParams->power = dB;

	msg.type = WMA_SET_MAX_TX_POWER_REQ;
	msg.reserved = 0;
	msg.bodyptr = pMaxTxParams;

	if (QDF_STATUS_SUCCESS != scheduler_post_message(QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_WMA,
							 QDF_MODULE_ID_WMA,
							 &msg)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Not able to post WMA_SET_MAX_TX_POWER_REQ message to WMA",
			  __func__);
		qdf_mem_free(pMaxTxParams);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/*
 * sme_set_custom_mac_addr() -
 * Set the customer Mac Address.
 *
 * customMacAddr  customer MAC Address
 * Return QDF_STATUS
 */
QDF_STATUS sme_set_custom_mac_addr(tSirMacAddr customMacAddr)
{
	struct scheduler_msg msg = {0};
	tSirMacAddr *pBaseMacAddr;

	pBaseMacAddr = qdf_mem_malloc(sizeof(tSirMacAddr));
	if (!pBaseMacAddr)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_copy(*pBaseMacAddr, customMacAddr, sizeof(tSirMacAddr));

	msg.type = SIR_HAL_SET_BASE_MACADDR_IND;
	msg.reserved = 0;
	msg.bodyptr = pBaseMacAddr;

	if (QDF_STATUS_SUCCESS != scheduler_post_message(QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_WMA,
							 QDF_MODULE_ID_WMA,
							 &msg)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			"Not able to post SIR_HAL_SET_BASE_MACADDR_IND message to WMA");
		qdf_mem_free(pBaseMacAddr);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/*
 * sme_set_tx_power() -
 * Set Transmit Power dynamically.
 *
 * mac_handle
 * sessionId  Target Session ID
 * BSSID
 * dev_mode dev_mode such as station, P2PGO, SAP
 * dBm  power to set
 * Return QDF_STATUS
 */
QDF_STATUS sme_set_tx_power(mac_handle_t mac_handle, uint8_t sessionId,
			   struct qdf_mac_addr pBSSId,
			   enum QDF_OPMODE dev_mode, int dBm)
{
	struct scheduler_msg msg = {0};
	tpMaxTxPowerParams pTxParams = NULL;
	int8_t power = (int8_t) dBm;

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_SET_TXPOW, sessionId, 0));

	/* make sure there is no overflow */
	if ((int)power != dBm) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: error, invalid power = %d", __func__, dBm);
		return QDF_STATUS_E_FAILURE;
	}

	pTxParams = qdf_mem_malloc(sizeof(tMaxTxPowerParams));
	if (!pTxParams)
		return QDF_STATUS_E_NOMEM;

	qdf_copy_macaddr(&pTxParams->bssId, &pBSSId);
	pTxParams->power = power;       /* unit is dBm */
	pTxParams->dev_mode = dev_mode;
	msg.type = WMA_SET_TX_POWER_REQ;
	msg.reserved = 0;
	msg.bodyptr = pTxParams;

	if (QDF_STATUS_SUCCESS != scheduler_post_message(QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_WMA,
							 QDF_MODULE_ID_WMA,
							 &msg)) {
		qdf_mem_free(pTxParams);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sme_set_check_assoc_disallowed(mac_handle_t mac_handle,
					  bool check_assoc_disallowed)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Failed to acquire sme lock; status: %d", status);
		return status;
	}
	wlan_cm_set_check_assoc_disallowed(mac->psoc, check_assoc_disallowed);
	sme_release_global_lock(&mac->sme);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sme_update_session_param(mac_handle_t mac_handle, uint8_t session_id,
				    uint32_t param_type, uint32_t param_val)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	uint16_t len;

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		struct sir_update_session_param *msg;
		struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx,
							session_id);

		if (!session) {
			sme_err("Session: %d not found", session_id);
			sme_release_global_lock(&mac_ctx->sme);
			return status;
		}

		if (!session->sessionActive)
			QDF_ASSERT(0);

		len = sizeof(*msg);
		msg = qdf_mem_malloc(len);
		if (!msg)
			status = QDF_STATUS_E_NOMEM;
		else {
			msg->message_type = eWNI_SME_SESSION_UPDATE_PARAM;
			msg->length = len;
			msg->vdev_id = session_id;
			msg->param_type = param_type;
			msg->param_val = param_val;
			status = umac_send_mb_message_to_mac(msg);
		}
		sme_release_global_lock(&mac_ctx->sme);
	}
	return status;
}

QDF_STATUS sme_update_roam_scan_n_probes(mac_handle_t mac_handle,
					 uint8_t vdev_id,
					 const uint8_t probes)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status;
	tCsrNeighborRoamControlInfo *neighbor_roam_info;

	if (vdev_id >= WLAN_MAX_VDEVS) {
		sme_err("Invalid vdev_id: %d", vdev_id);
		return QDF_STATUS_E_INVAL;
	}
	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_UPDATE_ROAM_SCAN_N_PROBES,
			 vdev_id, 0));
	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Failed to acquire sme lock; status: %d", status);
		return status;
	}
	neighbor_roam_info = &mac->roam.neighborRoamInfo[vdev_id];
	sme_debug("gRoamScanNProbes is changed from %u to %u",
		  neighbor_roam_info->cfgParams.roam_scan_n_probes, probes);
	neighbor_roam_info->cfgParams.roam_scan_n_probes = probes;

	if (mac->mlme_cfg->lfr.roam_scan_offload_enabled)
		csr_roam_update_cfg(mac, vdev_id, REASON_NPROBES_CHANGED);
	sme_release_global_lock(&mac->sme);

	return status;
}

QDF_STATUS
sme_update_roam_scan_home_away_time(mac_handle_t mac_handle, uint8_t vdev_id,
				    const uint16_t roam_scan_home_away_time,
				    const bool send_offload_cmd)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status;
	tCsrNeighborRoamControlInfo *neighbor_roam_info;

	if (vdev_id >= WLAN_MAX_VDEVS) {
		sme_err("Invalid vdev_id: %d", vdev_id);
		return QDF_STATUS_E_INVAL;
	}
	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_UPDATE_ROAM_SCAN_HOME_AWAY_TIME,
			 vdev_id, 0));

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Failed to acquire sme lock; status: %d", status);
		return status;
	}
	neighbor_roam_info = &mac->roam.neighborRoamInfo[vdev_id];

	if (neighbor_roam_info->cfgParams.roam_scan_home_away_time ==
	    roam_scan_home_away_time) {
		sme_debug("Not updated as current value is :%u",
			  roam_scan_home_away_time);
		sme_release_global_lock(&mac->sme);
		return QDF_STATUS_SUCCESS;
	}

	sme_debug("gRoamScanHomeAwayTime is changed from %d to %d",
		  neighbor_roam_info->cfgParams.roam_scan_home_away_time,
		  roam_scan_home_away_time);
	neighbor_roam_info->cfgParams.roam_scan_home_away_time =
		roam_scan_home_away_time;
	if (mac->mlme_cfg->lfr.roam_scan_offload_enabled && send_offload_cmd)
		csr_roam_update_cfg(mac, vdev_id,
				    REASON_HOME_AWAY_TIME_CHANGED);
	sme_release_global_lock(&mac->sme);

	return QDF_STATUS_SUCCESS;
}

/**
 * sme_ext_change_channel()- function to post send ECSA
 * action frame to csr.
 * @mac_handle: Opaque handle to the global MAC context
 * @channel freq: new channel freq to switch
 * @session_id: senssion it should be sent on.
 *
 * This function is called to post ECSA frame to csr.
 *
 * Return: success if msg is sent else return failure
 */
QDF_STATUS sme_ext_change_channel(mac_handle_t mac_handle, uint32_t channel,
						uint8_t session_id)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac_ctx  = MAC_CONTEXT(mac_handle);
	uint8_t channel_state;

	sme_err("Set Channel: %d", channel);
	channel_state =
		wlan_reg_get_channel_state(mac_ctx->pdev, channel);

	if (CHANNEL_STATE_DISABLE == channel_state) {
		sme_err("Invalid channel: %d", channel);
		return QDF_STATUS_E_INVAL;
	}

	status = sme_acquire_global_lock(&mac_ctx->sme);

	if (QDF_STATUS_SUCCESS == status) {
		/* update the channel list to the firmware */
		status = csr_send_ext_change_channel(mac_ctx,
						channel, session_id);
		sme_release_global_lock(&mac_ctx->sme);
	}

	return status;
}

/*
 * sme_get_roam_intra_band() -
 * get Intra band roaming
 *
 * mac_handle: Opaque handle to the global MAC context
 * Return Success or failure
 */
bool sme_get_roam_intra_band(mac_handle_t mac_handle)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_GET_ROAMIBAND, NO_SESSION, 0));

	return mac->mlme_cfg->lfr.roam_intra_band;
}

QDF_STATUS sme_get_roam_scan_n_probes(mac_handle_t mac_handle, uint8_t vdev_id,
				      uint8_t *roam_scan_n_probes)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status;
	tCsrNeighborRoamControlInfo *neighbor_roam_info;

	if (vdev_id >= WLAN_MAX_VDEVS) {
		sme_err("Invalid vdev_id: %d", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Failed to acquire sme lock; status: %d", status);
		return status;
	}
	neighbor_roam_info = &mac->roam.neighborRoamInfo[vdev_id];
	*roam_scan_n_probes = neighbor_roam_info->cfgParams.roam_scan_n_probes;
	sme_release_global_lock(&mac->sme);

	return status;
}

QDF_STATUS sme_get_roam_scan_home_away_time(mac_handle_t mac_handle,
					    uint8_t vdev_id,
					    uint16_t *roam_scan_home_away_time)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status;
	tCsrNeighborRoamControlInfo *neighbor_roam_info;

	if (vdev_id >= WLAN_MAX_VDEVS) {
		sme_err("Invalid vdev_id: %d", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Failed to acquire sme lock; status: %d", status);
		return status;
	}
	neighbor_roam_info = &mac->roam.neighborRoamInfo[vdev_id];
	*roam_scan_home_away_time =
		neighbor_roam_info->cfgParams.roam_scan_home_away_time;
	sme_release_global_lock(&mac->sme);

	return status;
}

QDF_STATUS sme_update_roam_rssi_diff(mac_handle_t mac_handle, uint8_t vdev_id,
				     uint8_t roam_rssi_diff)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status;
	tCsrNeighborRoamControlInfo *neighbor_roam_info;

	if (vdev_id >= WLAN_MAX_VDEVS) {
		sme_err("Invalid sme vdev id: %d", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status))
		return status;
	neighbor_roam_info = &mac->roam.neighborRoamInfo[vdev_id];
	sme_debug("LFR runtime successfully set roam rssi diff to %d - old value is %d - roam state is %s",
		  roam_rssi_diff,
		  neighbor_roam_info->cfgParams.roam_rssi_diff,
		  mac_trace_get_neighbour_roam_state(
		  neighbor_roam_info->neighborRoamState));

	neighbor_roam_info->cfgParams.roam_rssi_diff = roam_rssi_diff;
	if (mac->mlme_cfg->lfr.roam_scan_offload_enabled)
		csr_roam_update_cfg(mac, vdev_id,
				    REASON_RSSI_DIFF_CHANGED);

	sme_release_global_lock(&mac->sme);
	return status;
}

void sme_update_session_assoc_ie(mac_handle_t mac_handle,
				 uint8_t vdev_id,
				 struct csr_roam_profile *src_profile)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session = CSR_GET_SESSION(mac, vdev_id);

	if (!session) {
		sme_err("Session: %d not found", vdev_id);
		return;
	}

	qdf_mem_free(session->pAddIEAssoc);
	session->pAddIEAssoc = NULL;
	session->nAddIEAssocLength = 0;

	if (!src_profile->nAddIEAssocLength) {
		sme_debug("Assoc IE len 0");
		return;
	}

	session->pAddIEAssoc = qdf_mem_malloc(src_profile->nAddIEAssocLength);
	if (!session->pAddIEAssoc)
		return;

	session->nAddIEAssocLength = src_profile->nAddIEAssocLength;
	qdf_mem_copy(session->pAddIEAssoc, src_profile->pAddIEAssoc,
		     src_profile->nAddIEAssocLength);
}

QDF_STATUS sme_send_rso_connect_params(mac_handle_t mac_handle,
				       uint8_t vdev_id,
				       struct csr_roam_profile *src_profile)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tpCsrNeighborRoamControlInfo neighbor_roam_info =
			&mac->roam.neighborRoamInfo[vdev_id];

	if (vdev_id >= WLAN_MAX_VDEVS) {
		sme_err("Invalid sme vdev id: %d", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	if (!src_profile) {
		sme_err("src roam profile NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (!mac->mlme_cfg->lfr.lfr_enabled ||
	    (neighbor_roam_info->neighborRoamState !=
	     eCSR_NEIGHBOR_ROAM_STATE_CONNECTED)) {
		sme_debug("Fast roam is disabled or not connected(%d)",
			  neighbor_roam_info->neighborRoamState);
		return QDF_STATUS_E_PERM;
	}

	if (csr_is_roam_offload_enabled(mac)) {
		status = sme_acquire_global_lock(&mac->sme);
		if (QDF_IS_STATUS_SUCCESS(status)) {
			sme_debug("Updating fils config to fw");
			csr_roam_update_cfg(mac, vdev_id,
					    REASON_FILS_PARAMS_CHANGED);
			sme_release_global_lock(&mac->sme);
		} else {
			sme_err("Failed to acquire SME lock");
		}
	} else {
		sme_debug("LFR3 not enabled");
		return QDF_STATUS_E_INVAL;
	}

	return status;
}

#ifdef WLAN_FEATURE_FILS_SK
QDF_STATUS sme_update_fils_config(mac_handle_t mac_handle, uint8_t vdev_id,
				  struct csr_roam_profile *src_profile)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	csr_update_fils_config(mac, vdev_id, src_profile);

	return status;
}

void sme_send_hlp_ie_info(mac_handle_t mac_handle, uint8_t vdev_id,
			  struct csr_roam_profile *profile, uint32_t if_addr)
{
	int i;
	struct scheduler_msg msg;
	QDF_STATUS status;
	struct hlp_params *params;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session = CSR_GET_SESSION(mac, vdev_id);
	tpCsrNeighborRoamControlInfo neighbor_roam_info =
				&mac->roam.neighborRoamInfo[vdev_id];

	if (!session) {
		sme_err("session NULL");
		return;
	}

	if (!mac->mlme_cfg->lfr.lfr_enabled ||
	    (neighbor_roam_info->neighborRoamState !=
	     eCSR_NEIGHBOR_ROAM_STATE_CONNECTED)) {
		sme_debug("Fast roam is disabled or not connected(%d)",
				neighbor_roam_info->neighborRoamState);
		return;
	}

	params = qdf_mem_malloc(sizeof(*params));
	if (!params)
		return;

	if ((profile->hlp_ie_len +
	     QDF_IPV4_ADDR_SIZE) > FILS_MAX_HLP_DATA_LEN) {
		sme_err("HLP IE len exceeds %d",
				profile->hlp_ie_len);
		qdf_mem_free(params);
		return;
	}

	params->vdev_id = vdev_id;
	params->hlp_ie_len = profile->hlp_ie_len + QDF_IPV4_ADDR_SIZE;

	for (i = 0; i < QDF_IPV4_ADDR_SIZE; i++)
		params->hlp_ie[i] = (if_addr >> (i * 8)) & 0xFF;

	qdf_mem_copy(params->hlp_ie + QDF_IPV4_ADDR_SIZE,
		     profile->hlp_ie, profile->hlp_ie_len);

	msg.type = SIR_HAL_HLP_IE_INFO;
	msg.reserved = 0;
	msg.bodyptr = params;
	status = sme_acquire_global_lock(&mac->sme);
	if (status != QDF_STATUS_SUCCESS) {
		sme_err("sme lock acquire fails");
		qdf_mem_free(params);
		return;
	}

	if (!QDF_IS_STATUS_SUCCESS
			(scheduler_post_message(QDF_MODULE_ID_SME,
						QDF_MODULE_ID_WMA,
						QDF_MODULE_ID_WMA, &msg))) {
		sme_err("Not able to post WMA_HLP_IE_INFO message to HAL");
		sme_release_global_lock(&mac->sme);
		qdf_mem_free(params);
		return;
	}

	sme_release_global_lock(&mac->sme);
}

void sme_free_join_rsp_fils_params(struct csr_roam_info *roam_info)
{
	struct fils_join_rsp_params *roam_fils_params;

	if (!roam_info)
		return;

	roam_fils_params = roam_info->fils_join_rsp;
	if (!roam_fils_params)
		return;

	if (roam_fils_params->fils_pmk)
		qdf_mem_free(roam_fils_params->fils_pmk);

	qdf_mem_free(roam_fils_params);

	roam_info->fils_join_rsp = NULL;
}

#else
inline void sme_send_hlp_ie_info(mac_handle_t mac_handle, uint8_t vdev_id,
			  struct csr_roam_profile *profile, uint32_t if_addr)
{}
#endif

/*
 * sme_update_wes_mode() -
 * Update WES Mode
 *	    This function is called through dynamic setConfig callback function
 *	    to configure isWESModeEnabled
 *
 * mac_handle: Opaque handle to the global MAC context
 * isWESModeEnabled - WES mode
 * sessionId - Session Identifier
 * Return QDF_STATUS_SUCCESS - SME update isWESModeEnabled config successfully.
 *	    Other status means SME is failed to update isWESModeEnabled.
 */

QDF_STATUS sme_update_wes_mode(mac_handle_t mac_handle, bool isWESModeEnabled,
			       uint8_t sessionId)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (sessionId >= WLAN_MAX_VDEVS) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Invalid sme session id: %d"), sessionId);
		return QDF_STATUS_E_INVAL;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "LFR runtime successfully set WES Mode to %d - old value is %d - roam state is %s",
			  isWESModeEnabled,
			  mac->mlme_cfg->lfr.wes_mode_enabled,
			  mac_trace_get_neighbour_roam_state(mac->roam.
							     neighborRoamInfo
							     [sessionId].
							    neighborRoamState));
		mac->mlme_cfg->lfr.wes_mode_enabled = isWESModeEnabled;
		sme_release_global_lock(&mac->sme);
	}

	return status;
}

/*
 * sme_set_roam_scan_control() -
 * Set roam scan control
 *	    This function is called to set roam scan control
 *	    if roam scan control is set to 0, roaming scan cache is cleared
 *	    any value other than 0 is treated as invalid value
 * mac_handle: Opaque handle to the global MAC context
 * sessionId - Session Identifier
 * Return QDF_STATUS_SUCCESS - SME update config successfully.
 *	    Other status means SME failure to update
 */
QDF_STATUS sme_set_roam_scan_control(mac_handle_t mac_handle, uint8_t sessionId,
				     bool roamScanControl)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tCsrChannelInfo *specific_channel_info;
	tCsrNeighborRoamControlInfo *neighbor_roam_info;

	MTRACE(qdf_trace(QDF_MODULE_ID_SME,
			 TRACE_CODE_SME_RX_HDD_SET_SCANCTRL, NO_SESSION, 0));

	if (sessionId >= WLAN_MAX_VDEVS) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Invalid sme session id: %d"), sessionId);
		return QDF_STATUS_E_INVAL;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status))
		return status;
	neighbor_roam_info = &mac->roam.neighborRoamInfo[sessionId];
	sme_debug("LFR runtime successfully set roam scan control to %d - old value is %d - roam state is %s",
		  roamScanControl,
		  mac->roam.configParam.nRoamScanControl,
		  mac_trace_get_neighbour_roam_state(
			neighbor_roam_info->neighborRoamState));
	if (!roamScanControl && mac->roam.configParam.nRoamScanControl) {
		/**
		 * Clear the specific channel info cache when roamScanControl
		 * is set to 0. If any preffered channel list is configured,
		 * that will be sent to firmware for further roam scans.
		 */
		sme_debug("LFR runtime successfully cleared roam scan cache");
		specific_channel_info =
			&neighbor_roam_info->cfgParams.specific_chan_info;
		csr_flush_cfg_bg_scan_roam_channel_list(specific_channel_info);
		if (mac->mlme_cfg->lfr.roam_scan_offload_enabled) {
		/** Clear the static channel in FW by REASON_FLUSH_CHANNEL_LIST
		 *  and then append channel list with dynamic channels in the FW
		 *  using REASON_CHANNEL_LIST_CHANGED.
		 */
			csr_roam_update_cfg(mac, sessionId,
					    REASON_FLUSH_CHANNEL_LIST);

			csr_roam_update_cfg(mac, sessionId,
					    REASON_CHANNEL_LIST_CHANGED);
		}
	}
	mac->roam.configParam.nRoamScanControl = roamScanControl;
	sme_release_global_lock(&mac->sme);

	return status;
}

/*
 * sme_update_is_fast_roam_ini_feature_enabled() - enable/disable LFR
 *	support at runtime
 * It is used at in the REG_DYNAMIC_VARIABLE macro definition of
 * isFastRoamIniFeatureEnabled.
 * This is a synchronous call
 *
 * mac_handle - The handle returned by mac_open.
 * sessionId - Session Identifier
 * Return QDF_STATUS_SUCCESS - SME update isFastRoamIniFeatureEnabled config
 *	successfully.
 * Other status means SME is failed to update isFastRoamIniFeatureEnabled.
 */
QDF_STATUS sme_update_is_fast_roam_ini_feature_enabled(mac_handle_t mac_handle,
		uint8_t sessionId, const bool isFastRoamIniFeatureEnabled)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	if (mac->mlme_cfg->lfr.lfr_enabled ==
	    isFastRoamIniFeatureEnabled) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "%s: FastRoam is already enabled or disabled, nothing to do (returning) old(%d) new(%d)",
			  __func__,
			  mac->mlme_cfg->lfr.lfr_enabled,
			  isFastRoamIniFeatureEnabled);
		return QDF_STATUS_SUCCESS;
	}

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		  "%s: FastRoamEnabled is changed from %d to %d", __func__,
		  mac->mlme_cfg->lfr.lfr_enabled,
		  isFastRoamIniFeatureEnabled);
	mac->mlme_cfg->lfr.lfr_enabled = isFastRoamIniFeatureEnabled;
	csr_neighbor_roam_update_fast_roaming_enabled(mac, sessionId,
						   isFastRoamIniFeatureEnabled);

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_WLAN_ESE
int sme_add_key_krk(mac_handle_t mac_handle, uint8_t session_id,
		    const uint8_t *key, const int key_len)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session;

	if (key_len < SIR_KRK_KEY_LEN) {
		sme_warn("Invalid KRK keylength [= %d]", key_len);
		return -EINVAL;
	}

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("incorrect session/vdev ID");
		return -EINVAL;
	}

	session = CSR_GET_SESSION(mac_ctx, session_id);

	qdf_mem_copy(session->eseCckmInfo.krk, key, SIR_KRK_KEY_LEN);
	session->eseCckmInfo.reassoc_req_num = 1;
	session->eseCckmInfo.krk_plumbed = true;

	return 0;
}
#endif

#if defined(FEATURE_WLAN_ESE) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
int sme_add_key_btk(mac_handle_t mac_handle, uint8_t session_id,
		    const uint8_t *key, const int key_len)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session;

	if (key_len < SIR_BTK_KEY_LEN) {
		sme_warn("Invalid BTK keylength [= %d]", key_len);
		return -EINVAL;
	}

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("incorrect session/vdev ID");
		return -EINVAL;
	}

	session = CSR_GET_SESSION(mac_ctx, session_id);

	qdf_mem_copy(session->eseCckmInfo.btk, key, SIR_BTK_KEY_LEN);
	/*
	 * KRK and BTK are updated by upper layer back to back. Send
	 * updated KRK and BTK together to FW here.
	 */
	csr_roam_update_cfg(mac_ctx, session_id, REASON_ROAM_PSK_PMK_CHANGED);

	return 0;
}
#endif

QDF_STATUS sme_stop_roaming(mac_handle_t mac_handle, uint8_t vdev_id,
			    uint8_t reason,
			    enum wlan_cm_rso_control_requestor requestor)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session;

	session = CSR_GET_SESSION(mac, vdev_id);
	if (!session) {
		sme_err("ROAM: incorrect vdev ID %d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	return wlan_cm_disable_rso(mac->pdev, vdev_id, requestor, reason);
}

QDF_STATUS sme_start_roaming(mac_handle_t mac_handle, uint8_t vdev_id,
			     uint8_t reason,
			     enum wlan_cm_rso_control_requestor requestor)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	return wlan_cm_enable_rso(mac->pdev, vdev_id, requestor, reason);
}

QDF_STATUS sme_abort_roaming(mac_handle_t mac_handle, uint8_t vdev_id)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session;

	session = CSR_GET_SESSION(mac, vdev_id);
	if (!session) {
		sme_err("ROAM: incorrect vdev ID %d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	return wlan_cm_abort_rso(mac->pdev, vdev_id);
}

bool sme_roaming_in_progress(mac_handle_t mac_handle, uint8_t vdev_id)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session;

	session = CSR_GET_SESSION(mac, vdev_id);
	if (!session) {
		sme_err("ROAM: incorrect vdev ID %d", vdev_id);
		return false;
	}

	return wlan_cm_roaming_in_progress(mac->pdev, vdev_id);
}

/*
 * sme_set_roam_opportunistic_scan_threshold_diff() -
 * Update Opportunistic Scan threshold diff
 *	This function is called through dynamic setConfig callback function
 *	to configure  nOpportunisticThresholdDiff
 *
 * mac_handle: Opaque handle to the global MAC context
 * sessionId - Session Identifier
 * nOpportunisticThresholdDiff - Opportunistic Scan threshold diff
 * Return QDF_STATUS_SUCCESS - SME update nOpportunisticThresholdDiff config
 *	    successfully.
 *	    else SME is failed to update nOpportunisticThresholdDiff.
 */
QDF_STATUS sme_set_roam_opportunistic_scan_threshold_diff(
				mac_handle_t mac_handle,
				uint8_t sessionId,
				const uint8_t nOpportunisticThresholdDiff)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = csr_neighbor_roam_update_config(mac, sessionId,
				nOpportunisticThresholdDiff,
				REASON_OPPORTUNISTIC_THRESH_DIFF_CHANGED);
		if (QDF_IS_STATUS_SUCCESS(status)) {
			mac->mlme_cfg->lfr.opportunistic_scan_threshold_diff =
				nOpportunisticThresholdDiff;
		}
		sme_release_global_lock(&mac->sme);
	}
	return status;
}

/*
 * sme_get_roam_opportunistic_scan_threshold_diff()
 * gets Opportunistic Scan threshold diff
 * This is a synchronous call
 *
 * mac_handle - The handle returned by mac_open
 * Return uint8_t - nOpportunisticThresholdDiff
 */
uint8_t sme_get_roam_opportunistic_scan_threshold_diff(mac_handle_t mac_handle)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	return mac->mlme_cfg->lfr.opportunistic_scan_threshold_diff;
}

/*
 * sme_set_roam_rescan_rssi_diff() - Update roam rescan rssi diff
 * This function is called through dynamic setConfig callback function
 * to configure  nRoamRescanRssiDiff
 *
 * mac_handle: Opaque handle to the global MAC context
 * sessionId - Session Identifier
 * nRoamRescanRssiDiff - roam rescan rssi diff
 * Return QDF_STATUS_SUCCESS - SME update nRoamRescanRssiDiff config
 *	    successfully.
 * else SME is failed to update nRoamRescanRssiDiff.
 */
QDF_STATUS sme_set_roam_rescan_rssi_diff(mac_handle_t mac_handle,
					 uint8_t sessionId,
					 const uint8_t nRoamRescanRssiDiff)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = csr_neighbor_roam_update_config(mac, sessionId,
				nRoamRescanRssiDiff,
				REASON_ROAM_RESCAN_RSSI_DIFF_CHANGED);
		if (QDF_IS_STATUS_SUCCESS(status))
			mac->mlme_cfg->lfr.roam_rescan_rssi_diff =
				nRoamRescanRssiDiff;

		sme_release_global_lock(&mac->sme);
	}
	return status;
}

/*
 * sme_get_roam_rescan_rssi_diff()
 * gets roam rescan rssi diff
 *	  This is a synchronous call
 *
 * mac_handle - The handle returned by mac_open
 * Return int8_t - nRoamRescanRssiDiff
 */
uint8_t sme_get_roam_rescan_rssi_diff(mac_handle_t mac_handle)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	return mac->mlme_cfg->lfr.roam_rescan_rssi_diff;
}

/*
 * sme_set_roam_bmiss_first_bcnt() -
 * Update Roam count for first beacon miss
 *	    This function is called through dynamic setConfig callback function
 *	    to configure nRoamBmissFirstBcnt
 * mac_handle: Opaque handle to the global MAC context
 * sessionId - Session Identifier
 * nRoamBmissFirstBcnt - Roam first bmiss count
 * Return QDF_STATUS_SUCCESS - SME update nRoamBmissFirstBcnt
 *	    successfully.
 * else SME is failed to update nRoamBmissFirstBcnt
 */
QDF_STATUS sme_set_roam_bmiss_first_bcnt(mac_handle_t mac_handle,
					 uint8_t sessionId,
					 const uint8_t nRoamBmissFirstBcnt)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = csr_neighbor_roam_update_config(mac, sessionId,
				nRoamBmissFirstBcnt,
				REASON_ROAM_BMISS_FIRST_BCNT_CHANGED);
		if (QDF_IS_STATUS_SUCCESS(status)) {
			mac->mlme_cfg->lfr.roam_bmiss_first_bcnt =
							nRoamBmissFirstBcnt;
		}
		sme_release_global_lock(&mac->sme);
	}
	return status;
}

/*
 * sme_set_roam_bmiss_final_bcnt() -
 * Update Roam count for final beacon miss
 *	    This function is called through dynamic setConfig callback function
 *	    to configure nRoamBmissFinalBcnt
 * mac_handle: Opaque handle to the global MAC context
 * sessionId - Session Identifier
 * nRoamBmissFinalBcnt - Roam final bmiss count
 * Return QDF_STATUS_SUCCESS - SME update nRoamBmissFinalBcnt
 *	    successfully.
 * else SME is failed to update nRoamBmissFinalBcnt
 */
QDF_STATUS sme_set_roam_bmiss_final_bcnt(mac_handle_t mac_handle,
					 uint8_t sessionId,
					 const uint8_t nRoamBmissFinalBcnt)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = csr_neighbor_roam_update_config(mac, sessionId,
				nRoamBmissFinalBcnt,
				REASON_ROAM_BMISS_FINAL_BCNT_CHANGED);
		if (QDF_IS_STATUS_SUCCESS(status)) {
			mac->mlme_cfg->lfr.roam_bmiss_final_bcnt =
							nRoamBmissFinalBcnt;
		}
		sme_release_global_lock(&mac->sme);
	}
	return status;
}

QDF_STATUS
sme_set_neighbor_lookup_rssi_threshold(mac_handle_t mac_handle,
				       uint8_t vdev_id,
				       uint8_t neighbor_lookup_rssi_threshold)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status;

	if (vdev_id >= WLAN_MAX_VDEVS) {
		sme_err("Invalid vdev_id: %u", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status))
		return status;
	csr_neighbor_roam_update_config(mac, vdev_id,
					neighbor_lookup_rssi_threshold,
					REASON_LOOKUP_THRESH_CHANGED);
	sme_release_global_lock(&mac->sme);
	return status;
}

QDF_STATUS sme_get_neighbor_lookup_rssi_threshold(mac_handle_t mac_handle,
						  uint8_t vdev_id,
						  uint8_t *lookup_threshold)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	tCsrNeighborRoamControlInfo *neighbor_roam_info;

	if (vdev_id >= WLAN_MAX_VDEVS) {
		sme_err("Invalid vdev_id: %d", vdev_id);
		return QDF_STATUS_E_INVAL;
	}
	neighbor_roam_info = &mac->roam.neighborRoamInfo[vdev_id];

	*lookup_threshold =
		neighbor_roam_info->cfgParams.neighborLookupThreshold;

	return QDF_STATUS_SUCCESS;
}

/*
 * sme_set_neighbor_scan_refresh_period() - set neighbor scan results
 *	refresh period
 *  This is a synchronous call
 *
 * mac_handle - The handle returned by mac_open.
 * sessionId - Session Identifier
 * Return QDF_STATUS_SUCCESS - SME update config successful.
 *	   Other status means SME is failed to update
 */
QDF_STATUS sme_set_neighbor_scan_refresh_period(mac_handle_t mac_handle,
		uint8_t sessionId, uint16_t neighborScanResultsRefreshPeriod)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo = NULL;

	if (sessionId >= WLAN_MAX_VDEVS) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Invalid sme session id: %d"), sessionId);
		return QDF_STATUS_E_INVAL;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		pNeighborRoamInfo = &mac->roam.neighborRoamInfo[sessionId];
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "LFR runtime successfully set roam scan refresh period to %d- old value is %d - roam state is %s",
			  neighborScanResultsRefreshPeriod,
			  mac->mlme_cfg->lfr.
			  neighbor_scan_results_refresh_period,
			  mac_trace_get_neighbour_roam_state(mac->roam.
							     neighborRoamInfo
							     [sessionId].
							    neighborRoamState));
		mac->mlme_cfg->lfr.neighbor_scan_results_refresh_period =
			neighborScanResultsRefreshPeriod;
		pNeighborRoamInfo->cfgParams.neighborResultsRefreshPeriod =
			neighborScanResultsRefreshPeriod;

		if (mac->mlme_cfg->lfr.roam_scan_offload_enabled)
			csr_roam_update_cfg(mac, sessionId,
				   REASON_NEIGHBOR_SCAN_REFRESH_PERIOD_CHANGED);

		sme_release_global_lock(&mac->sme);
	}

	return status;
}

/*
 * sme_get_neighbor_scan_refresh_period() - get neighbor scan results
 *	refresh period
 *  This is a synchronous call
 *
 *  \param mac_handle - The handle returned by mac_open.
 *  \return uint16_t - Neighbor scan results refresh period value
 */
uint16_t sme_get_neighbor_scan_refresh_period(mac_handle_t mac_handle)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	return mac->mlme_cfg->lfr.neighbor_scan_results_refresh_period;
}

uint16_t sme_get_empty_scan_refresh_period_global(mac_handle_t mac_handle)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	return mac->mlme_cfg->lfr.empty_scan_refresh_period;
}

QDF_STATUS sme_get_empty_scan_refresh_period(mac_handle_t mac_handle,
					     uint8_t vdev_id,
					     uint16_t *refresh_threshold)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	tCsrNeighborRoamControlInfo *neighbor_roam_info;
	QDF_STATUS status;

	if (vdev_id >= WLAN_MAX_VDEVS) {
		sme_err("Invalid vdev_id: %d", vdev_id);
		return QDF_STATUS_E_INVAL;
	}
	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status))
		return status;
	neighbor_roam_info = &mac->roam.neighborRoamInfo[vdev_id];

	*refresh_threshold =
		neighbor_roam_info->cfgParams.emptyScanRefreshPeriod;
	sme_release_global_lock(&mac->sme);

	return QDF_STATUS_SUCCESS;
}

/*
 * sme_update_empty_scan_refresh_period
 * Update empty_scan_refresh_period
 *	    This function is called through dynamic setConfig callback function
 *	    to configure empty_scan_refresh_period
 *	    Usage: adb shell iwpriv wlan0 setConfig
 *			empty_scan_refresh_period=[0 .. 60]
 *
 * mac_handle: Opaque handle to the global MAC context
 * sessionId - Session Identifier
 * empty_scan_refresh_period - scan period following empty scan results.
 * Return Success or failure
 */

QDF_STATUS sme_update_empty_scan_refresh_period(mac_handle_t mac_handle,
						uint8_t sessionId, uint16_t
						empty_scan_refresh_period)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo = NULL;

	if (sessionId >= WLAN_MAX_VDEVS) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Invalid sme session id: %d"), sessionId);
		return QDF_STATUS_E_INVAL;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		pNeighborRoamInfo = &mac->roam.neighborRoamInfo[sessionId];
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "LFR runtime successfully set roam scan period to %d -old value is %d - roam state is %s",
			  empty_scan_refresh_period,
			  pNeighborRoamInfo->cfgParams.emptyScanRefreshPeriod,
			  mac_trace_get_neighbour_roam_state(mac->roam.
							     neighborRoamInfo
							     [sessionId].
							    neighborRoamState));
		pNeighborRoamInfo->cfgParams.emptyScanRefreshPeriod =
			empty_scan_refresh_period;

		if (mac->mlme_cfg->lfr.roam_scan_offload_enabled)
			csr_roam_update_cfg(mac, sessionId,
					  REASON_EMPTY_SCAN_REF_PERIOD_CHANGED);

		sme_release_global_lock(&mac->sme);
	}

	return status;
}

QDF_STATUS sme_update_full_roam_scan_period(mac_handle_t mac_handle,
					    uint8_t vdev_id,
					    uint32_t full_roam_scan_period)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status;
	tpCsrNeighborRoamControlInfo neighbor_roam_info;

	if (vdev_id >= WLAN_MAX_VDEVS) {
		sme_err("Invalid vdev_id: %d", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	neighbor_roam_info = &mac->roam.neighborRoamInfo[vdev_id];
	sme_debug("LFR runtime successfully set full roam scan period to %d -old value is %d - roam state is %s",
		  full_roam_scan_period,
		  neighbor_roam_info->cfgParams.full_roam_scan_period,
		  mac_trace_get_neighbour_roam_state(
		  neighbor_roam_info->neighborRoamState));
	neighbor_roam_info->cfgParams.full_roam_scan_period =
						full_roam_scan_period;
	if (mac->mlme_cfg->lfr.roam_scan_offload_enabled)
		csr_roam_update_cfg(mac, vdev_id,
				    REASON_ROAM_FULL_SCAN_PERIOD_CHANGED);

	sme_release_global_lock(&mac->sme);

	return status;
}

QDF_STATUS
sme_modify_roam_cand_sel_criteria(mac_handle_t mac_handle,
				  uint8_t vdev_id,
				  bool enable_scoring_for_roam)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status;
	tpCsrNeighborRoamControlInfo neighbor_roam_info;

	if (vdev_id >= WLAN_MAX_VDEVS) {
		sme_err("Invalid vdev_id: %d", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	if (!mac->mlme_cfg->lfr.roam_scan_offload_enabled) {
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	neighbor_roam_info = &mac->roam.neighborRoamInfo[vdev_id];
	neighbor_roam_info->cfgParams.enable_scoring_for_roam =
					enable_scoring_for_roam;
	status = csr_roam_update_cfg(mac, vdev_id,
				     REASON_SCORING_CRITERIA_CHANGED);
out:
	sme_release_global_lock(&mac->sme);

	return status;
}

/**
 * sme_restore_default_roaming_params() - Restore neighbor roam config
 * @mac: mac context
 * @roam_info: Neighbor roam info pointer to be populated
 *
 * Restore neighbor roam info request params with lfr config params
 *
 * Return: None
 */
static void
sme_restore_default_roaming_params(struct mac_context *mac,
				   tCsrNeighborRoamControlInfo *roam_info)
{
	roam_info->cfgParams.enable_scoring_for_roam =
			mac->mlme_cfg->roam_scoring.enable_scoring_for_roam;
	roam_info->cfgParams.emptyScanRefreshPeriod =
			mac->mlme_cfg->lfr.empty_scan_refresh_period;
	roam_info->cfgParams.full_roam_scan_period =
			mac->mlme_cfg->lfr.roam_full_scan_period;
	roam_info->cfgParams.maxChannelScanTime =
			mac->mlme_cfg->lfr.neighbor_scan_max_chan_time;
	roam_info->cfgParams.neighborScanPeriod =
			mac->mlme_cfg->lfr.neighbor_scan_timer_period;
	roam_info->cfgParams.neighborLookupThreshold =
			mac->mlme_cfg->lfr.neighbor_lookup_rssi_threshold;
	roam_info->cfgParams.roam_rssi_diff =
			mac->mlme_cfg->lfr.roam_rssi_diff;
	roam_info->cfgParams.bg_rssi_threshold =
			mac->mlme_cfg->lfr.bg_rssi_threshold;

	roam_info->cfgParams.maxChannelScanTime =
			mac->mlme_cfg->lfr.neighbor_scan_max_chan_time;
	roam_info->cfgParams.roam_scan_home_away_time =
			mac->mlme_cfg->lfr.roam_scan_home_away_time;
	roam_info->cfgParams.roam_scan_n_probes =
			mac->mlme_cfg->lfr.roam_scan_n_probes;
	roam_info->cfgParams.roam_scan_inactivity_time =
			mac->mlme_cfg->lfr.roam_scan_inactivity_time;
	roam_info->cfgParams.roam_inactive_data_packet_count =
			mac->mlme_cfg->lfr.roam_inactive_data_packet_count;
	roam_info->cfgParams.roam_scan_period_after_inactivity =
			mac->mlme_cfg->lfr.roam_scan_period_after_inactivity;
}

void sme_roam_reset_configs(mac_handle_t mac_handle, uint8_t vdev_id)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	tCsrNeighborRoamControlInfo *neighbor_roam_info;

	neighbor_roam_info = &mac->roam.neighborRoamInfo[vdev_id];
	sme_restore_default_roaming_params(mac, neighbor_roam_info);
}

QDF_STATUS sme_roam_control_restore_default_config(mac_handle_t mac_handle,
						   uint8_t vdev_id)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status;
	tpCsrNeighborRoamControlInfo neighbor_roam_info;
	tCsrChannelInfo *chan_info;

	if (vdev_id >= WLAN_MAX_VDEVS) {
		sme_err("Invalid vdev_id: %d", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	if (!mac->mlme_cfg->lfr.roam_scan_offload_enabled) {
		sme_err("roam_scan_offload_enabled is not supported");
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	mac->roam.configParam.nRoamScanControl = false;

	neighbor_roam_info = &mac->roam.neighborRoamInfo[vdev_id];

	chan_info = &neighbor_roam_info->cfgParams.pref_chan_info;
	csr_flush_cfg_bg_scan_roam_channel_list(chan_info);

	chan_info = &neighbor_roam_info->cfgParams.specific_chan_info;
	csr_flush_cfg_bg_scan_roam_channel_list(chan_info);

	mlme_reinit_control_config_lfr_params(mac->psoc, &mac->mlme_cfg->lfr);

	sme_restore_default_roaming_params(mac, neighbor_roam_info);

	/* Flush static and dynamic channels in ROAM scan list in firmware */
	csr_roam_update_cfg(mac, vdev_id, REASON_FLUSH_CHANNEL_LIST);
	csr_roam_update_cfg(mac, vdev_id, REASON_SCORING_CRITERIA_CHANGED);

out:
	sme_release_global_lock(&mac->sme);

	return status;
}

/*
 * sme_set_neighbor_scan_min_chan_time() -
 * Update nNeighborScanMinChanTime
 *	    This function is called through dynamic setConfig callback function
 *	    to configure gNeighborScanChannelMinTime
 *	    Usage: adb shell iwpriv wlan0 setConfig
 *			gNeighborScanChannelMinTime=[0 .. 60]
 *
 * mac_handle: Opaque handle to the global MAC context
 * nNeighborScanMinChanTime - Channel minimum dwell time
 * sessionId - Session Identifier
 * Return Success or failure
 */
QDF_STATUS sme_set_neighbor_scan_min_chan_time(mac_handle_t mac_handle,
					       const uint16_t
					       nNeighborScanMinChanTime,
					       uint8_t sessionId)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (sessionId >= WLAN_MAX_VDEVS) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Invalid sme session id: %d"), sessionId);
		return QDF_STATUS_E_INVAL;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "LFR runtime successfully set channel min dwell time to %d - old value is %d - roam state is %s",
			  nNeighborScanMinChanTime,
			  mac->mlme_cfg->lfr.neighbor_scan_min_chan_time,
			  mac_trace_get_neighbour_roam_state(mac->roam.
							     neighborRoamInfo
							     [sessionId].
							    neighborRoamState));

		mac->mlme_cfg->lfr.neighbor_scan_min_chan_time =
						nNeighborScanMinChanTime;
		mac->roam.neighborRoamInfo[sessionId].cfgParams.
		minChannelScanTime = nNeighborScanMinChanTime;
		sme_release_global_lock(&mac->sme);
	}

	return status;
}

/*
 * sme_set_neighbor_scan_max_chan_time() -
 * Update nNeighborScanMaxChanTime
 *	    This function is called through dynamic setConfig callback function
 *	    to configure gNeighborScanChannelMaxTime
 *	    Usage: adb shell iwpriv wlan0 setConfig
 *			gNeighborScanChannelMaxTime=[0 .. 60]
 *
 * mac_handle: Opaque handle to the global MAC context
 * sessionId - Session Identifier
 * nNeighborScanMinChanTime - Channel maximum dwell time
 * Return Success or failure
 */
QDF_STATUS sme_set_neighbor_scan_max_chan_time(mac_handle_t mac_handle,
					       uint8_t sessionId,
					       const uint16_t
					       nNeighborScanMaxChanTime)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo = NULL;

	if (sessionId >= WLAN_MAX_VDEVS) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Invalid sme session id: %d"), sessionId);
		return QDF_STATUS_E_INVAL;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		pNeighborRoamInfo = &mac->roam.neighborRoamInfo[sessionId];
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "LFR runtime successfully set channel max dwell time to %d - old value is %d - roam state is %s",
			  nNeighborScanMaxChanTime,
			  pNeighborRoamInfo->cfgParams.maxChannelScanTime,
			  mac_trace_get_neighbour_roam_state(mac->roam.
							     neighborRoamInfo
							     [sessionId].
							    neighborRoamState));
		pNeighborRoamInfo->cfgParams.maxChannelScanTime =
			nNeighborScanMaxChanTime;
		if (mac->mlme_cfg->lfr.roam_scan_offload_enabled)
			csr_roam_update_cfg(mac, sessionId,
					    REASON_SCAN_CH_TIME_CHANGED);

		sme_release_global_lock(&mac->sme);
	}

	return status;
}

/*
 * sme_get_neighbor_scan_min_chan_time() -
 * get neighbor scan min channel time
 *
 * mac_handle - The handle returned by mac_open.
 * sessionId - Session Identifier
 * Return uint16_t - channel min time value
 */
uint16_t sme_get_neighbor_scan_min_chan_time(mac_handle_t mac_handle,
					     uint8_t sessionId)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	if (sessionId >= WLAN_MAX_VDEVS) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Invalid sme session id: %d"), sessionId);
		return 0;
	}

	return mac->roam.neighborRoamInfo[sessionId].cfgParams.
	       minChannelScanTime;
}

/*
 * sme_get_neighbor_roam_state() -
 * get neighbor roam state
 *
 * mac_handle - The handle returned by mac_open.
 * sessionId - Session Identifier
 * Return uint32_t - neighbor roam state
 */
uint32_t sme_get_neighbor_roam_state(mac_handle_t mac_handle, uint8_t sessionId)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	if (sessionId >= WLAN_MAX_VDEVS) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Invalid sme session id: %d"), sessionId);
		return 0;
	}

	return mac->roam.neighborRoamInfo[sessionId].neighborRoamState;
}

/*
 * sme_get_current_roam_state() -
 * get current roam state
 *
 * mac_handle - The handle returned by mac_open.
 * sessionId - Session Identifier
 * Return uint32_t - current roam state
 */
uint32_t sme_get_current_roam_state(mac_handle_t mac_handle, uint8_t sessionId)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	return mac->roam.curState[sessionId];
}

/*
 * sme_get_current_roam_sub_state() -
 *   \brief  get neighbor roam sub state
 *
 * mac_handle - The handle returned by mac_open.
 * sessionId - Session Identifier
 * Return uint32_t - current roam sub state
 */
uint32_t sme_get_current_roam_sub_state(mac_handle_t mac_handle,
					uint8_t sessionId)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	return mac->roam.curSubState[sessionId];
}

/*
 * sme_get_lim_sme_state() -
 *   get Lim Sme state
 *
 * mac_handle - The handle returned by mac_open.
 * Return uint32_t - Lim Sme state
 */
uint32_t sme_get_lim_sme_state(mac_handle_t mac_handle)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	return mac->lim.gLimSmeState;
}

/*
 * sme_get_lim_mlm_state() -
 *   get Lim Mlm state
 *
 * mac_handle - The handle returned by mac_open.
 * Return uint32_t - Lim Mlm state
 */
uint32_t sme_get_lim_mlm_state(mac_handle_t mac_handle)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	return mac->lim.gLimMlmState;
}

/*
 * sme_is_lim_session_valid() -
 *  is Lim session valid
 *
 * mac_handle - The handle returned by mac_open.
 * sessionId - Session Identifier
 * Return bool - true or false
 */
bool sme_is_lim_session_valid(mac_handle_t mac_handle, uint8_t sessionId)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	if (sessionId > mac->lim.maxBssId)
		return false;

	return mac->lim.gpSession[sessionId].valid;
}

/*
 * sme_get_lim_sme_session_state() -
 *   get Lim Sme session state
 *
 * mac_handle - The handle returned by mac_open.
 * sessionId - Session Identifier
 * Return uint32_t - Lim Sme session state
 */
uint32_t sme_get_lim_sme_session_state(mac_handle_t mac_handle,
				       uint8_t sessionId)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	return mac->lim.gpSession[sessionId].limSmeState;
}

/*
 * sme_get_lim_mlm_session_state() -
 *   \brief  get Lim Mlm session state
 *
 * mac_handle - The handle returned by mac_open.
 * sessionId - Session Identifier
 * Return uint32_t - Lim Mlm session state
 */
uint32_t sme_get_lim_mlm_session_state(mac_handle_t mac_handle,
				       uint8_t sessionId)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	return mac->lim.gpSession[sessionId].limMlmState;
}

/*
 * sme_get_neighbor_scan_max_chan_time() -
 *   get neighbor scan max channel time
 *
 * mac_handle - The handle returned by mac_open.
 * sessionId - Session Identifier
 * Return uint16_t - channel max time value
 */
uint16_t sme_get_neighbor_scan_max_chan_time(mac_handle_t mac_handle,
					     uint8_t sessionId)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	if (sessionId >= WLAN_MAX_VDEVS) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Invalid sme session id: %d"), sessionId);
		return 0;
	}

	return mac->roam.neighborRoamInfo[sessionId].cfgParams.
	       maxChannelScanTime;
}

/*
 * sme_set_neighbor_scan_period() -
 *  Update nNeighborScanPeriod
 *	    This function is called through dynamic setConfig callback function
 *	    to configure nNeighborScanPeriod
 *	    Usage: adb shell iwpriv wlan0 setConfig
 *			nNeighborScanPeriod=[0 .. 1000]
 *
 * mac_handle: Opaque handle to the global MAC context
 * sessionId - Session Identifier
 * nNeighborScanPeriod - neighbor scan period
 * Return Success or failure
 */
QDF_STATUS sme_set_neighbor_scan_period(mac_handle_t mac_handle,
					uint8_t sessionId,
					const uint16_t nNeighborScanPeriod)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo = NULL;

	if (sessionId >= WLAN_MAX_VDEVS) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Invalid sme session id: %d"), sessionId);
		return QDF_STATUS_E_INVAL;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		pNeighborRoamInfo = &mac->roam.neighborRoamInfo[sessionId];
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "LFR runtime successfully set neighbor scan period to %d - old value is %d - roam state is %s",
			  nNeighborScanPeriod,
			  pNeighborRoamInfo->cfgParams.neighborScanPeriod,
			  mac_trace_get_neighbour_roam_state(mac->roam.
							     neighborRoamInfo
							     [sessionId].
							    neighborRoamState));
		pNeighborRoamInfo->cfgParams.neighborScanPeriod =
			nNeighborScanPeriod;

		if (mac->mlme_cfg->lfr.roam_scan_offload_enabled)
			csr_roam_update_cfg(mac, sessionId,
					    REASON_SCAN_HOME_TIME_CHANGED);

		sme_release_global_lock(&mac->sme);
	}

	return status;
}

/*
 * sme_get_neighbor_scan_period() -
 *   get neighbor scan period
 *
 * mac_handle - The handle returned by mac_open.
 * sessionId - Session Identifier
 * Return uint16_t - neighbor scan period
 */
uint16_t sme_get_neighbor_scan_period(mac_handle_t mac_handle,
				      uint8_t sessionId)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	if (sessionId >= WLAN_MAX_VDEVS) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Invalid sme session id: %d"), sessionId);
		return 0;
	}

	return mac->roam.neighborRoamInfo[sessionId].cfgParams.
	       neighborScanPeriod;
}

QDF_STATUS sme_get_roam_rssi_diff(mac_handle_t mac_handle, uint8_t vdev_id,
				  uint8_t *rssi_diff)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	tCsrNeighborRoamControlInfo *neighbor_roam_info;
	QDF_STATUS status;

	if (vdev_id >= WLAN_MAX_VDEVS) {
		sme_err("Invalid vdev_id: %d", vdev_id);
		return QDF_STATUS_E_INVAL;
	}
	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status))
		return status;
	neighbor_roam_info = &mac->roam.neighborRoamInfo[vdev_id];
	*rssi_diff = neighbor_roam_info->cfgParams.roam_rssi_diff;
	sme_release_global_lock(&mac->sme);

	return QDF_STATUS_SUCCESS;
}

void sme_dump_freq_list(tCsrChannelInfo *chan_info)
{
	uint8_t *channel_list;
	uint8_t i = 0, j = 0;
	uint32_t buflen = CFG_VALID_CHANNEL_LIST_LEN * 4;

	channel_list = qdf_mem_malloc(buflen);
	if (!channel_list)
		return;

	if (chan_info->freq_list) {
		for (i = 0; i < chan_info->numOfChannels; i++) {
			if (j < buflen)
				j += snprintf(channel_list + j, buflen - j,
					      "%d ", chan_info->freq_list[i]);
			else
				break;
		}
	}

	sme_debug("frequency list [%u]: %s", i, channel_list);
	qdf_mem_free(channel_list);
}

static bool sme_validate_freq_list(mac_handle_t mac_handle,
				   uint32_t *freq_list,
				   uint8_t num_channels)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	uint8_t i = 0, j;
	bool found;
	struct csr_channel *ch_lst_info = &mac_ctx->scan.base_channels;

	if (!freq_list || !num_channels) {
		sme_err("Freq list empty %pK or num_channels is 0", freq_list);
		return false;
	}

	while (i < num_channels) {
		found = false;
		for (j = 0; j < ch_lst_info->numChannels; j++) {
			if (ch_lst_info->channel_freq_list[j] == freq_list[i]) {
				found = true;
				break;
			}
		}

		if (!found) {
			sme_debug("Invalid frequency %u", freq_list[i]);
			return false;
		}

		i++;
	}

	return true;
}

static uint8_t
csr_append_pref_chan_list(tCsrChannelInfo *chan_info, uint32_t *freq_list,
			  uint8_t num_chan)
{
	uint8_t i = 0, j = 0;

	for (i = 0; i < chan_info->numOfChannels; i++) {
		for (j = 0; j < num_chan; j++)
			if (chan_info->freq_list[i] == freq_list[j])
				break;

		if (j < num_chan)
			continue;
		if (num_chan == SIR_ROAM_MAX_CHANNELS)
			break;
		freq_list[num_chan++] = chan_info->freq_list[i];
	}

	return num_chan;
}

/**
 * sme_update_roam_scan_channel_list() - to update scan channel list
 * @mac_handle: Opaque handle to the global MAC context
 * @vdev_id: vdev identifier
 * @chan_info: Channel information to be updated to.
 * @freq_list: Frequency list to be updated from.
 * @num_chan: Number of channels
 *
 * Updates the chan_info by flushing the current frequency list and update
 * the same with freq_list.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
sme_update_roam_scan_channel_list(mac_handle_t mac_handle, uint8_t vdev_id,
				  tCsrChannelInfo *chan_info,
				  uint32_t *freq_list, uint8_t num_chan)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	uint16_t pref_chan_cnt = 0;

	if (chan_info->numOfChannels) {
		sme_debug("Current channels:");
		sme_dump_freq_list(chan_info);
	}

	pref_chan_cnt = csr_append_pref_chan_list(chan_info, freq_list,
						  num_chan);
	num_chan = pref_chan_cnt;

	csr_flush_cfg_bg_scan_roam_channel_list(chan_info);
	csr_create_bg_scan_roam_channel_list(mac, chan_info, freq_list,
					     num_chan);
	sme_debug("New channels:");
	sme_dump_freq_list(chan_info);
	sme_debug("Updated roam scan channels - roam state is %d",
		  mac->roam.neighborRoamInfo[vdev_id].neighborRoamState);

	if (mac->mlme_cfg->lfr.roam_scan_offload_enabled)
		status = csr_roam_update_cfg(mac, vdev_id,
					     REASON_CHANNEL_LIST_CHANGED);

	return status;
}

/**
 * sme_change_roam_scan_channel_list() - to change scan channel list
 * @mac_handle: Opaque handle to the global MAC context
 * @sessionId: sme session id
 * @channel_freq_list: Output channel list
 * @numChannels: Output number of channels
 *
 * This routine is called to Change roam scan channel list.
 * This is a synchronous call
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_change_roam_scan_channel_list(mac_handle_t mac_handle,
					     uint8_t sessionId,
					     uint32_t *channel_freq_list,
					     uint8_t numChannels)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo = NULL;
	uint8_t oldChannelList[CFG_VALID_CHANNEL_LIST_LEN * 5] = { 0 };
	uint8_t newChannelList[CFG_VALID_CHANNEL_LIST_LEN * 5] = { 0 };
	uint8_t i = 0, j = 0;
	tCsrChannelInfo *chan_info;

	if (sessionId >= WLAN_MAX_VDEVS) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Invalid sme session id: %d"), sessionId);
		return QDF_STATUS_E_INVAL;
	}

	pNeighborRoamInfo = &mac->roam.neighborRoamInfo[sessionId];
	status = sme_acquire_global_lock(&mac->sme);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Failed to acquire SME lock");
		return status;
	}
	chan_info = &pNeighborRoamInfo->cfgParams.specific_chan_info;

	if (chan_info->freq_list) {
		for (i = 0; i < chan_info->numOfChannels; i++) {
			if (j < sizeof(oldChannelList))
				j += snprintf(oldChannelList + j,
					sizeof(oldChannelList) -
					j, " %d",
					chan_info->freq_list[i]);
			else
				break;
		}
	}
	csr_flush_cfg_bg_scan_roam_channel_list(chan_info);
	csr_create_bg_scan_roam_channel_list(mac, chan_info, channel_freq_list,
					     numChannels);
	sme_set_roam_scan_control(mac_handle, sessionId, 1);
	if (chan_info->freq_list) {
		j = 0;
		for (i = 0; i < chan_info->numOfChannels; i++) {
			if (j < sizeof(newChannelList))
				j += snprintf(newChannelList + j,
					sizeof(newChannelList) -
					j, " %d", chan_info->freq_list[i]);
			else
				break;
		}
	}
	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		"LFR runtime successfully set roam scan channels to %s - old value is %s - roam state is %d",
			newChannelList, oldChannelList,
		mac->roam.neighborRoamInfo[sessionId].neighborRoamState);

	if (mac->mlme_cfg->lfr.roam_scan_offload_enabled)
		csr_roam_update_cfg(mac, sessionId,
				    REASON_CHANNEL_LIST_CHANGED);

	sme_release_global_lock(&mac->sme);
	return status;
}

QDF_STATUS
sme_update_roam_scan_freq_list(mac_handle_t mac_handle, uint8_t vdev_id,
			       uint32_t *freq_list, uint8_t num_chan,
			       uint32_t freq_list_type)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status;
	tpCsrNeighborRoamControlInfo neighbor_roam_info;
	tCsrChannelInfo *channel_info;

	if (vdev_id >= WLAN_MAX_VDEVS) {
		sme_err("Invalid vdev_id: %d", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	if (!sme_validate_freq_list(mac_handle, freq_list, num_chan)) {
		sme_err("List contains invalid channel(s)");
		return QDF_STATUS_E_INVAL;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Failed to acquire SME lock");
		return status;
	}

	neighbor_roam_info = &mac->roam.neighborRoamInfo[vdev_id];
	if (neighbor_roam_info->cfgParams.specific_chan_info.numOfChannels &&
	    freq_list_type == QCA_PREFERRED_SCAN_FREQ_LIST) {
		sme_err("Specific channel list is already configured");
		sme_release_global_lock(&mac->sme);
		return QDF_STATUS_E_INVAL;
	}

	sme_debug("frequency list type %d", freq_list_type);
	if (freq_list_type == QCA_PREFERRED_SCAN_FREQ_LIST) {
		channel_info = &neighbor_roam_info->cfgParams.pref_chan_info;
		status = sme_update_roam_scan_channel_list(
					mac_handle, vdev_id, channel_info,
					freq_list, num_chan);
	} else {
		status = sme_change_roam_scan_channel_list(mac_handle, vdev_id,
							   freq_list, num_chan);
	}
	sme_release_global_lock(&mac->sme);

	return status;
}

/**
 * sme_get_roam_scan_channel_list() - To get roam scan channel list
 * @mac_handle: Opaque handle to the global MAC context
 * @freq_list: Output channel freq list
 * @pNumChannels: Output number of channels
 * @sessionId: Session Identifier
 *
 * To get roam scan channel list This is a synchronous call
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_get_roam_scan_channel_list(mac_handle_t mac_handle,
			uint32_t *freq_list, uint8_t *pNumChannels,
			uint8_t sessionId)
{
	int i = 0, chan_cnt = 0;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tCsrChannelInfo *chan_info;
	struct csr_channel *occupied_ch_lst =
		&mac->scan.occupiedChannels[sessionId];

	if (sessionId >= WLAN_MAX_VDEVS) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Invalid sme session id: %d"), sessionId);
		return QDF_STATUS_E_INVAL;
	}

	pNeighborRoamInfo = &mac->roam.neighborRoamInfo[sessionId];
	status = sme_acquire_global_lock(&mac->sme);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;

	chan_info = &pNeighborRoamInfo->cfgParams.specific_chan_info;
	if (chan_info->numOfChannels) {
		*pNumChannels = chan_info->numOfChannels;
		for (i = 0; i < (*pNumChannels) &&
		     i < WNI_CFG_VALID_CHANNEL_LIST_LEN; i++)
			freq_list[i] = chan_info->freq_list[i];

		*pNumChannels = i;
	} else {
		chan_info = &pNeighborRoamInfo->cfgParams.pref_chan_info;
		*pNumChannels = chan_info->numOfChannels;
		if (chan_info->numOfChannels) {
			for (chan_cnt = 0; chan_cnt < (*pNumChannels) &&
			     chan_cnt < WNI_CFG_VALID_CHANNEL_LIST_LEN;
			     chan_cnt++)
				freq_list[chan_cnt] =
					chan_info->freq_list[chan_cnt];
		}

		if (occupied_ch_lst->numChannels) {
			for (i = 0; i < occupied_ch_lst->numChannels &&
			     chan_cnt < WNI_CFG_VALID_CHANNEL_LIST_LEN; i++)
				freq_list[chan_cnt++] =
					occupied_ch_lst->channel_freq_list[i];
		}

		*pNumChannels = chan_cnt;
		if (!(chan_info->numOfChannels ||
		      occupied_ch_lst->numChannels)) {
			sme_err("Roam Scan channel list is NOT yet initialized");
			*pNumChannels = 0;
			status = QDF_STATUS_E_INVAL;
		}
	}

	sme_release_global_lock(&mac->sme);
	return status;
}

/*
 * sme_get_is_ese_feature_enabled() - get ESE feature enabled or not
 *  This is a synchronuous call
 *
 * mac_handle - The handle returned by mac_open.
 * Return true (1) - if the ESE feature is enabled
 *	  false (0) - if feature is disabled (compile or runtime)
 */
bool sme_get_is_ese_feature_enabled(mac_handle_t mac_handle)
{
#ifdef FEATURE_WLAN_ESE
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	return csr_roam_is_ese_ini_feature_enabled(mac);
#else
	return false;
#endif
}

/*
 * sme_get_wes_mode() - get WES Mode
 *  This is a synchronous call
 *
 * mac_handle - The handle returned by mac_open
 * Return uint8_t - WES Mode Enabled(1)/Disabled(0)
 */
bool sme_get_wes_mode(mac_handle_t mac_handle)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	return mac->mlme_cfg->lfr.wes_mode_enabled;
}

/*
 * sme_get_roam_scan_control() - get scan control
 *  This is a synchronous call
 *
 * mac_handle - The handle returned by mac_open.
 * Return bool - Enabled(1)/Disabled(0)
 */
bool sme_get_roam_scan_control(mac_handle_t mac_handle)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	return mac->roam.configParam.nRoamScanControl;
}

/*
 * sme_get_is_lfr_feature_enabled() - get LFR feature enabled or not
 *  This is a synchronuous call
 * mac_handle - The handle returned by mac_open.
 * Return true (1) - if the feature is enabled
 *	  false (0) - if feature is disabled (compile or runtime)
 */
bool sme_get_is_lfr_feature_enabled(mac_handle_t mac_handle)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	return mac->mlme_cfg->lfr.lfr_enabled;
}

/*
 * sme_get_is_ft_feature_enabled() - get FT feature enabled or not
 *  This is a synchronuous call
 *
 * mac_handle - The handle returned by mac_open.
 * Return true (1) - if the feature is enabled
 *	   false (0) - if feature is disabled (compile or runtime)
 */
bool sme_get_is_ft_feature_enabled(mac_handle_t mac_handle)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	return mac->mlme_cfg->lfr.fast_transition_enabled;
}

/**
 * sme_is_feature_supported_by_fw() - check if feature is supported by FW
 * @feature: enum value of requested feature.
 *
 * Retrun: 1 if supported; 0 otherwise
 */
bool sme_is_feature_supported_by_fw(enum cap_bitmap feature)
{
	return IS_FEATURE_SUPPORTED_BY_FW(feature);
}

QDF_STATUS sme_get_link_speed(mac_handle_t mac_handle,
			      struct link_speed_info *req,
			      void *context,
			      sme_link_speed_cb cb)
{
	QDF_STATUS status;
	struct mac_context *mac;
	void *wma_handle;

	if (!mac_handle || !cb || !req) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Invalid parameter"));
		return QDF_STATUS_E_FAILURE;
	}

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				"wma handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mac = MAC_CONTEXT(mac_handle);
	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_STATUS_SUCCESS != status) {
		sme_err("Failed to acquire global lock");
		return QDF_STATUS_E_FAILURE;
	}

	mac->sme.link_speed_context = context;
	mac->sme.link_speed_cb = cb;
	status = wma_get_link_speed(wma_handle, req);
	sme_release_global_lock(&mac->sme);
	return status;
}

QDF_STATUS sme_get_isolation(mac_handle_t mac_handle, void *context,
			     sme_get_isolation_cb callbackfn)
{
	QDF_STATUS status;
	struct mac_context  *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};

	if (!callbackfn) {
		sme_err("Indication Call back is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status))
		return status;
	mac->sme.get_isolation_cb = callbackfn;
	mac->sme.get_isolation_cb_context = context;
	message.bodyptr = NULL;
	message.type    = WMA_GET_ISOLATION;
	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA,
					&message);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("failed to post WMA_GET_ISOLATION");
		status = QDF_STATUS_E_FAILURE;
	}
	sme_release_global_lock(&mac->sme);
	return status;
}

/*convert the ini value to the ENUM used in csr and MAC for CB state*/
ePhyChanBondState sme_get_cb_phy_state_from_cb_ini_value(uint32_t cb_ini_value)
{
	return csr_convert_cb_ini_value_to_phy_cb_state(cb_ini_value);
}

void sme_set_curr_device_mode(mac_handle_t mac_handle,
			      enum QDF_OPMODE curr_device_mode)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	mac->sme.curr_device_mode = curr_device_mode;
}

/*
 * sme_handoff_request() - a wrapper function to Request a handoff from CSR.
 *   This is a synchronous call
 *
 * mac_handle - The handle returned by mac_open
 * sessionId - Session Identifier
 * pHandoffInfo - info provided by HDD with the handoff request (namely:
 * BSSID, channel etc.)
 * Return QDF_STATUS_SUCCESS - SME passed the request to CSR successfully.
 *	   Other status means SME is failed to send the request.
 */

QDF_STATUS sme_handoff_request(mac_handle_t mac_handle,
			       uint8_t sessionId,
			       tCsrHandoffRequest *pHandoffInfo)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "%s: invoked", __func__);
		status = csr_handoff_request(mac, sessionId, pHandoffInfo);
		sme_release_global_lock(&mac->sme);
	}

	return status;
}

/**
 * sme_add_periodic_tx_ptrn() - Add Periodic TX Pattern
 * @mac_handle: Opaque handle to the global MAC context
 * @addPeriodicTxPtrnParams: request message
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS
sme_add_periodic_tx_ptrn(mac_handle_t mac_handle,
			 struct sSirAddPeriodicTxPtrn *addPeriodicTxPtrnParams)
{
	QDF_STATUS status   = QDF_STATUS_SUCCESS;
	struct mac_context *mac  = MAC_CONTEXT(mac_handle);
	struct sSirAddPeriodicTxPtrn *req_msg;
	struct scheduler_msg msg = {0};

	SME_ENTER();

	req_msg = qdf_mem_malloc(sizeof(*req_msg));
	if (!req_msg)
		return QDF_STATUS_E_NOMEM;

	*req_msg = *addPeriodicTxPtrnParams;

	status = sme_acquire_global_lock(&mac->sme);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("sme_acquire_global_lock failed!(status=%d)",
			status);
		qdf_mem_free(req_msg);
		return status;
	}

	/* Serialize the req through MC thread */
	msg.bodyptr = req_msg;
	msg.type    = WMA_ADD_PERIODIC_TX_PTRN_IND;
	MTRACE(qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
			 NO_SESSION, msg.type));
	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &msg);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("scheduler_post_msg failed!(err=%d)",
			status);
		qdf_mem_free(req_msg);
	}
	sme_release_global_lock(&mac->sme);
	return status;
}

/**
 * sme_del_periodic_tx_ptrn() - Delete Periodic TX Pattern
 * @mac_handle: Opaque handle to the global MAC context
 * @delPeriodicTxPtrnParams: request message
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS
sme_del_periodic_tx_ptrn(mac_handle_t mac_handle,
			 struct sSirDelPeriodicTxPtrn *delPeriodicTxPtrnParams)
{
	QDF_STATUS status    = QDF_STATUS_SUCCESS;
	struct mac_context *mac   = MAC_CONTEXT(mac_handle);
	struct sSirDelPeriodicTxPtrn *req_msg;
	struct scheduler_msg msg = {0};

	SME_ENTER();

	req_msg = qdf_mem_malloc(sizeof(*req_msg));
	if (!req_msg)
		return QDF_STATUS_E_NOMEM;

	*req_msg = *delPeriodicTxPtrnParams;

	status = sme_acquire_global_lock(&mac->sme);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("sme_acquire_global_lock failed!(status=%d)",
			status);
		qdf_mem_free(req_msg);
		return status;
	}

	/* Serialize the req through MC thread */
	msg.bodyptr = req_msg;
	msg.type    = WMA_DEL_PERIODIC_TX_PTRN_IND;
	MTRACE(qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
			 NO_SESSION, msg.type));
	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &msg);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("scheduler_post_msg failed!(err=%d)",
			status);
		qdf_mem_free(req_msg);
	}
	sme_release_global_lock(&mac->sme);
	return status;
}

QDF_STATUS sme_set_wlm_latency_level(mac_handle_t mac_handle,
				     uint16_t session_id,
				     uint16_t latency_level)
{
	QDF_STATUS status;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct wlm_latency_level_param params;
	void *wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma)
		return QDF_STATUS_E_FAILURE;

	if (!mac_ctx->mlme_cfg->wlm_config.latency_enable) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: WLM latency level setting is disabled",
			   __func__);
		return QDF_STATUS_E_FAILURE;
	}
	if (!wma) {
		sme_err("wma is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	params.wlm_latency_level = latency_level;
	params.wlm_latency_flags =
		mac_ctx->mlme_cfg->wlm_config.latency_flags[latency_level];
	params.vdev_id = session_id;

	status = wma_set_wlm_latency_level(wma, &params);

	return status;
}

void sme_get_command_q_status(mac_handle_t mac_handle)
{
	tSmeCmd *pTempCmd = NULL;
	struct mac_context *mac;
	struct wlan_serialization_command *cmd;

	if (!mac_handle)
		return;

	mac = MAC_CONTEXT(mac_handle);

	sme_debug("smeCmdPendingList has %d commands",
		  wlan_serialization_get_pending_list_count(mac->psoc, false));
	cmd = wlan_serialization_peek_head_active_cmd_using_psoc(mac->psoc,
								 false);
	if (cmd)
		sme_debug("Active commaned is %d cmd id %d source %d",
			  cmd->cmd_type, cmd->cmd_id, cmd->source);
	if (!cmd || cmd->source != WLAN_UMAC_COMP_MLME)
		return;

	pTempCmd = cmd->umac_cmd;
	if (pTempCmd) {
		if (eSmeCsrCommandMask & pTempCmd->command)
			/* CSR command is stuck. See what the reason code is
			 * for that command
			 */
			dump_csr_command_info(mac, pTempCmd);
	}
}

#ifdef WLAN_FEATURE_DSRC
/**
 * sme_ocb_gen_timing_advert_frame() - generate TA frame and populate the buffer
 * @mac_handle: Opaque handle to the global MAC context
 * @self_addr: the self MAC address
 * @buf: the buffer that will contain the frame
 * @timestamp_offset: return for the offset of the timestamp field
 * @time_value_offset: return for the time_value field in the TA IE
 *
 * Return: the length of the buffer.
 */
int sme_ocb_gen_timing_advert_frame(mac_handle_t mac_handle,
				    tSirMacAddr self_addr, uint8_t **buf,
				    uint32_t *timestamp_offset,
				    uint32_t *time_value_offset)
{
	int template_length;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	template_length = sch_gen_timing_advert_frame(mac_ctx, self_addr, buf,
						  timestamp_offset,
						  time_value_offset);
	return template_length;
}
#endif

QDF_STATUS sme_notify_modem_power_state(mac_handle_t mac_handle, uint32_t value)
{
	struct scheduler_msg msg = {0};
	tpSirModemPowerStateInd request_buf;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	if (!mac)
		return QDF_STATUS_E_FAILURE;

	request_buf = qdf_mem_malloc(sizeof(tSirModemPowerStateInd));
	if (!request_buf)
		return QDF_STATUS_E_NOMEM;

	request_buf->param = value;

	msg.type = WMA_MODEM_POWER_STATE_IND;
	msg.reserved = 0;
	msg.bodyptr = request_buf;
	if (!QDF_IS_STATUS_SUCCESS
		    (scheduler_post_message(QDF_MODULE_ID_SME,
					    QDF_MODULE_ID_WMA,
					    QDF_MODULE_ID_WMA, &msg))) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Not able to post WMA_MODEM_POWER_STATE_IND message to WMA",
			__func__);
		qdf_mem_free(request_buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef QCA_HT_2040_COEX
QDF_STATUS sme_notify_ht2040_mode(mac_handle_t mac_handle,
				  struct qdf_mac_addr macAddrSTA,
				  uint8_t sessionId,
				  uint8_t channel_type)
{
	struct scheduler_msg msg = {0};
	tUpdateVHTOpMode *pHtOpMode = NULL;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	if (!mac)
		return QDF_STATUS_E_FAILURE;

	pHtOpMode = qdf_mem_malloc(sizeof(tUpdateVHTOpMode));
	if (!pHtOpMode)
		return QDF_STATUS_E_NOMEM;

	switch (channel_type) {
	case eHT_CHAN_HT20:
		pHtOpMode->opMode = eHT_CHANNEL_WIDTH_20MHZ;
		break;

	case eHT_CHAN_HT40MINUS:
	case eHT_CHAN_HT40PLUS:
		pHtOpMode->opMode = eHT_CHANNEL_WIDTH_40MHZ;
		break;

	default:
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Invalid OP mode", __func__);
		qdf_mem_free(pHtOpMode);
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_copy(pHtOpMode->peer_mac, macAddrSTA.bytes,
		     sizeof(tSirMacAddr));
	pHtOpMode->smesessionId = sessionId;

	msg.type = WMA_UPDATE_OP_MODE;
	msg.reserved = 0;
	msg.bodyptr = pHtOpMode;
	if (!QDF_IS_STATUS_SUCCESS
		    (scheduler_post_message(QDF_MODULE_ID_SME,
					    QDF_MODULE_ID_WMA,
					    QDF_MODULE_ID_WMA, &msg))) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Not able to post WMA_UPDATE_OP_MODE message to WMA",
			__func__);
		qdf_mem_free(pHtOpMode);
		return QDF_STATUS_E_FAILURE;
	}

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		  "%s: Notified FW about OP mode: %d",
		  __func__, pHtOpMode->opMode);

	return QDF_STATUS_SUCCESS;
}

/*
 * sme_set_ht2040_mode() -
 *  To update HT Operation beacon IE.
 *
 * Return QDF_STATUS  SUCCESS
 *			FAILURE or RESOURCES
 *			The API finished and failed.
 */
QDF_STATUS sme_set_ht2040_mode(mac_handle_t mac_handle, uint8_t sessionId,
			       uint8_t channel_type, bool obssEnabled)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	ePhyChanBondState cbMode;
	struct csr_roam_session *session = CSR_GET_SESSION(mac, sessionId);

	if (!CSR_IS_SESSION_VALID(mac, sessionId)) {
		sme_err("Session not valid for session id %d", sessionId);
		return QDF_STATUS_E_INVAL;
	}
	session = CSR_GET_SESSION(mac, sessionId);
	sme_debug("Update HT operation beacon IE, channel_type=%d cur cbmode %d",
		channel_type, session->bssParams.cbMode);

	switch (channel_type) {
	case eHT_CHAN_HT20:
		if (!session->bssParams.cbMode)
			return QDF_STATUS_SUCCESS;
		cbMode = PHY_SINGLE_CHANNEL_CENTERED;
		break;
	case eHT_CHAN_HT40MINUS:
		if (session->bssParams.cbMode)
			return QDF_STATUS_SUCCESS;
		cbMode = PHY_DOUBLE_CHANNEL_HIGH_PRIMARY;
		break;
	case eHT_CHAN_HT40PLUS:
		if (session->bssParams.cbMode)
			return QDF_STATUS_SUCCESS;
		cbMode = PHY_DOUBLE_CHANNEL_LOW_PRIMARY;
		break;
	default:
		sme_err("Error!!! Invalid HT20/40 mode !");
		return QDF_STATUS_E_FAILURE;
	}
	session->bssParams.cbMode = cbMode;
	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = csr_set_ht2040_mode(mac, sessionId,
					     cbMode, obssEnabled);
		sme_release_global_lock(&mac->sme);
	}
	return status;
}

#endif

/*
 * SME API to enable/disable idle mode powersave
 * This should be called only if powersave offload
 * is enabled
 */
QDF_STATUS sme_set_idle_powersave_config(bool value)
{
	void *wmaContext = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wmaContext) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: wmaContext is NULL", __func__);
		return QDF_STATUS_E_FAILURE;
	}
	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		  " Idle Ps Set Value %d", value);

	if (QDF_STATUS_SUCCESS != wma_set_idle_ps_config(wmaContext, value)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  " Failed to Set Idle Ps Value %d", value);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

int16_t sme_get_ht_config(mac_handle_t mac_handle, uint8_t session_id,
			  uint16_t ht_capab)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, session_id);

	if (!pSession) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: pSession is NULL", __func__);
		return -EIO;
	}
	switch (ht_capab) {
	case WNI_CFG_HT_CAP_INFO_ADVANCE_CODING:
		return pSession->ht_config.ht_rx_ldpc;
	case WNI_CFG_HT_CAP_INFO_TX_STBC:
		return pSession->ht_config.ht_tx_stbc;
	case WNI_CFG_HT_CAP_INFO_RX_STBC:
		return pSession->ht_config.ht_rx_stbc;
	case WNI_CFG_HT_CAP_INFO_SHORT_GI_20MHZ:
		return pSession->ht_config.ht_sgi20;
	case WNI_CFG_HT_CAP_INFO_SHORT_GI_40MHZ:
		return pSession->ht_config.ht_sgi40;
	default:
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "invalid ht capability");
		return -EIO;
	}
}

int sme_update_ht_config(mac_handle_t mac_handle, uint8_t sessionId,
			 uint16_t htCapab, int value)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: pSession is NULL", __func__);
		return -EIO;
	}

	if (QDF_STATUS_SUCCESS != wma_set_htconfig(sessionId, htCapab, value)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "Failed to set ht capability in target");
		return -EIO;
	}

	switch (htCapab) {
	case WNI_CFG_HT_CAP_INFO_ADVANCE_CODING:
		pSession->ht_config.ht_rx_ldpc = value;
		break;
	case WNI_CFG_HT_CAP_INFO_TX_STBC:
		pSession->ht_config.ht_tx_stbc = value;
		break;
	case WNI_CFG_HT_CAP_INFO_RX_STBC:
		pSession->ht_config.ht_rx_stbc = value;
		break;
	case WNI_CFG_HT_CAP_INFO_SHORT_GI_20MHZ:
		value = value ? 1 : 0; /* HT SGI can be only 1 or 0 */
		pSession->ht_config.ht_sgi20 = value;
		break;
	case WNI_CFG_HT_CAP_INFO_SHORT_GI_40MHZ:
		value = value ? 1 : 0; /* HT SGI can be only 1 or 0 */
		pSession->ht_config.ht_sgi40 = value;
		break;
	}

	csr_roam_update_config(mac, sessionId, htCapab, value);
	return 0;
}

int sme_set_addba_accept(mac_handle_t mac_handle, uint8_t session_id, int value)
{
	struct sme_addba_accept *addba_accept;
	struct scheduler_msg msg = {0};
	QDF_STATUS status;

	addba_accept = qdf_mem_malloc(sizeof(*addba_accept));
	if (!addba_accept)
		return -EIO;

	addba_accept->session_id = session_id;
	addba_accept->addba_accept = value;
	qdf_mem_zero(&msg, sizeof(msg));
	msg.type = eWNI_SME_SET_ADDBA_ACCEPT;
	msg.reserved = 0;
	msg.bodyptr = addba_accept;
	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE, &msg);
	if (status != QDF_STATUS_SUCCESS) {
		sme_err("Not able to post addba reject");
		qdf_mem_free(addba_accept);
		return -EIO;
	}
	return 0;
}

int sme_set_ba_buff_size(mac_handle_t mac_handle, uint8_t session_id,
			 uint16_t buff_size)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	if (!buff_size) {
		sme_err("invalid buff size %d", buff_size);
		return -EINVAL;
	}
	mac_ctx->usr_cfg_ba_buff_size = buff_size;
	sme_debug("addba buff size is set to %d",
			mac_ctx->usr_cfg_ba_buff_size);

	return 0;
}

#define DEFAULT_BA_BUFF_SIZE 64
int sme_send_addba_req(mac_handle_t mac_handle, uint8_t session_id, uint8_t tid,
		       uint16_t buff_size)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	uint16_t ba_buff = 0;
	QDF_STATUS status;
	struct scheduler_msg msg = {0};
	struct send_add_ba_req *send_ba_req;
	struct csr_roam_session *csr_session = NULL;

	if (!csr_is_conn_state_connected_infra(mac_ctx, session_id)) {
		sme_err("STA not infra/connected state session_id: %d",
				session_id);
		return -EINVAL;
	}
	csr_session = CSR_GET_SESSION(mac_ctx, session_id);
	if (!csr_session) {
		sme_err("CSR session is NULL");
		return -EINVAL;
	}
	send_ba_req = qdf_mem_malloc(sizeof(*send_ba_req));
	if (!send_ba_req)
		return -EIO;

	qdf_mem_copy(send_ba_req->mac_addr,
			csr_session->connectedProfile.bssid.bytes,
			QDF_MAC_ADDR_SIZE);
	ba_buff = buff_size;
	if (!buff_size) {
		if (mac_ctx->usr_cfg_ba_buff_size)
			ba_buff = mac_ctx->usr_cfg_ba_buff_size;
		else
			ba_buff = DEFAULT_BA_BUFF_SIZE;
	}
	send_ba_req->param.vdev_id = session_id;
	send_ba_req->param.tidno = tid;
	send_ba_req->param.buffersize = ba_buff;
	msg.type = WMA_SEND_ADDBA_REQ;
	msg.bodyptr = send_ba_req;
	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &msg);
	if (QDF_STATUS_SUCCESS != status) {
		qdf_mem_free(send_ba_req);
		return -EIO;
	}
	sme_debug("ADDBA_REQ sent to FW: tid %d buff_size %d", tid, ba_buff);

	return 0;
}

int sme_set_no_ack_policy(mac_handle_t mac_handle, uint8_t session_id,
			  uint8_t val, uint8_t ac)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	uint8_t i, set_val;
	struct scheduler_msg msg = {0};
	QDF_STATUS status;

	if (ac > QCA_WLAN_AC_ALL) {
		sme_err("invalid ac val %d", ac);
		return -EINVAL;
	}
	if (val)
		set_val = 1;
	else
		set_val = 0;
	if (ac == QCA_WLAN_AC_ALL) {
		for (i = 0; i < ac; i++)
			mac_ctx->no_ack_policy_cfg[i] = set_val;
	} else {
		mac_ctx->no_ack_policy_cfg[ac] = set_val;
	}
	sme_debug("no ack is set to %d for ac %d", set_val, ac);
	qdf_mem_zero(&msg, sizeof(msg));
	msg.type = eWNI_SME_UPDATE_EDCA_PROFILE;
	msg.reserved = 0;
	msg.bodyval = session_id;
	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE, &msg);
	if (status != QDF_STATUS_SUCCESS) {
		sme_err("Not able to post update edca profile");
		return -EIO;
	}

	return 0;
}

int sme_set_auto_rate_he_ltf(mac_handle_t mac_handle, uint8_t session_id,
			     uint8_t cfg_val)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	uint32_t set_val;
	uint32_t bit_mask = 0;
	int status;

	if (cfg_val > QCA_WLAN_HE_LTF_4X) {
		sme_err("invalid HE LTF cfg %d", cfg_val);
		return -EINVAL;
	}

	/*set the corresponding HE LTF cfg BIT*/
	if (cfg_val == QCA_WLAN_HE_LTF_AUTO)
		bit_mask = HE_LTF_ALL;
	else
		bit_mask = (1 << (cfg_val - 1));

	set_val = mac_ctx->he_sgi_ltf_cfg_bit_mask;

	SET_AUTO_RATE_HE_LTF_VAL(set_val, bit_mask);

	mac_ctx->he_sgi_ltf_cfg_bit_mask = set_val;
	status = wma_cli_set_command(session_id,
			WMI_VDEV_PARAM_AUTORATE_MISC_CFG,
			set_val, VDEV_CMD);
	if (status) {
		sme_err("failed to set he_ltf_sgi");
		return status;
	}

	sme_debug("HE SGI_LTF is set to 0x%08X",
			mac_ctx->he_sgi_ltf_cfg_bit_mask);

	return 0;
}

int sme_set_auto_rate_he_sgi(mac_handle_t mac_handle, uint8_t session_id,
			     uint8_t cfg_val)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	uint32_t set_val;
	uint32_t sgi_bit_mask = 0;
	int status;

	if ((cfg_val > AUTO_RATE_GI_3200NS) ||
			(cfg_val < AUTO_RATE_GI_400NS)) {
		sme_err("invalid auto rate GI cfg %d", cfg_val);
		return -EINVAL;
	}

	sgi_bit_mask = (1 << cfg_val);

	set_val = mac_ctx->he_sgi_ltf_cfg_bit_mask;
	SET_AUTO_RATE_SGI_VAL(set_val, sgi_bit_mask);

	mac_ctx->he_sgi_ltf_cfg_bit_mask = set_val;
	status = wma_cli_set_command(session_id,
				     WMI_VDEV_PARAM_AUTORATE_MISC_CFG,
				     set_val, VDEV_CMD);
	if (status) {
		sme_err("failed to set he_ltf_sgi");
		return status;
	}

	sme_debug("auto rate HE SGI_LTF is set to 0x%08X",
			mac_ctx->he_sgi_ltf_cfg_bit_mask);

	return 0;
}

int sme_set_auto_rate_ldpc(mac_handle_t mac_handle, uint8_t session_id,
			   uint8_t ldpc_disable)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	uint32_t set_val;
	int status;

	set_val = mac_ctx->he_sgi_ltf_cfg_bit_mask;

	set_val |= (ldpc_disable << AUTO_RATE_LDPC_DIS_BIT);

	status = wma_cli_set_command(session_id,
				     WMI_VDEV_PARAM_AUTORATE_MISC_CFG,
				     set_val, VDEV_CMD);
	if (status) {
		sme_err("failed to set auto rate LDPC cfg");
		return status;
	}

	sme_debug("auto rate misc cfg set to 0x%08X", set_val);

	return 0;
}

#define HT20_SHORT_GI_MCS7_RATE 722
/*
 * sme_send_rate_update_ind() -
 *  API to Update rate
 *
 * mac_handle - The handle returned by mac_open
 * rateUpdateParams - Pointer to rate update params
 * Return QDF_STATUS
 */
QDF_STATUS sme_send_rate_update_ind(mac_handle_t mac_handle,
				    tSirRateUpdateInd *rateUpdateParams)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status;
	struct scheduler_msg msg = {0};
	tSirRateUpdateInd *rate_upd = qdf_mem_malloc(sizeof(tSirRateUpdateInd));

	if (!rate_upd)
		return QDF_STATUS_E_FAILURE;

	*rate_upd = *rateUpdateParams;

	if (rate_upd->mcastDataRate24GHz == HT20_SHORT_GI_MCS7_RATE)
		rate_upd->mcastDataRate24GHzTxFlag =
			TX_RATE_HT20 | TX_RATE_SGI;
	else if (rate_upd->reliableMcastDataRate ==
		 HT20_SHORT_GI_MCS7_RATE)
		rate_upd->reliableMcastDataRateTxFlag =
			TX_RATE_HT20 | TX_RATE_SGI;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(rate_upd);
		return status;
	}

	msg.type = WMA_RATE_UPDATE_IND;
	msg.bodyptr = rate_upd;
	MTRACE(qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
			 NO_SESSION, msg.type));

	status = scheduler_post_message(QDF_MODULE_ID_SME, QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Not able to post WMA_RATE_UPDATE_IND to WMA!",
			  __func__);
		qdf_mem_free(rate_upd);
		status = QDF_STATUS_E_FAILURE;
	}

	sme_release_global_lock(&mac->sme);

	return status;
}

/**
 * sme_update_access_policy_vendor_ie() - update vendor ie and access policy.
 * @mac_handle: Pointer to the mac context
 * @vdev_id: vdev_id
 * @vendor_ie: vendor ie
 * @access_policy: vendor ie access policy
 *
 * This function updates the vendor ie and access policy to lim.
 *
 * Return: success or failure.
 */
QDF_STATUS sme_update_access_policy_vendor_ie(mac_handle_t mac_handle,
					      uint8_t vdev_id,
					      uint8_t *vendor_ie,
					      int access_policy)
{
	struct sme_update_access_policy_vendor_ie *msg;
	uint16_t msg_len;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	msg_len  = sizeof(*msg);

	msg = qdf_mem_malloc(msg_len);
	if (!msg) {
		return QDF_STATUS_E_NOMEM;
	}

	msg->msg_type = (uint16_t)eWNI_SME_UPDATE_ACCESS_POLICY_VENDOR_IE;
	msg->length = (uint16_t)msg_len;

	qdf_mem_copy(&msg->ie[0], vendor_ie, sizeof(msg->ie));

	msg->vdev_id = vdev_id;
	msg->access_policy = access_policy;

	sme_debug("vdev_id: %hu, access_policy: %d", vdev_id, access_policy);
	status = umac_send_mb_message_to_mac(msg);

	return status;
}

/**
 * sme_update_sta_inactivity_timeout(): Update sta_inactivity_timeout to FW
 * @mac_handle: Handle returned by mac_open
 * @session_id: Session ID on which sta_inactivity_timeout needs
 * to be updated to FW
 * @sta_inactivity_timeout: sta inactivity timeout.
 *
 * If a station does not send anything in sta_inactivity_timeout seconds, an
 * empty data frame is sent to it in order to verify whether it is
 * still in range. If this frame is not ACKed, the station will be
 * disassociated and then deauthenticated.
 *
 * Return: QDF_STATUS_SUCCESS or non-zero on failure.
 */
QDF_STATUS sme_update_sta_inactivity_timeout(mac_handle_t mac_handle,
		 struct sme_sta_inactivity_timeout  *sta_inactivity_timer)
{
	struct sme_sta_inactivity_timeout *inactivity_time;
	void *wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	inactivity_time = qdf_mem_malloc(sizeof(*inactivity_time));
	if (!inactivity_time)
		return QDF_STATUS_E_FAILURE;

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			FL("sta_inactivity_timeout: %d"),
			sta_inactivity_timer->sta_inactivity_timeout);
	inactivity_time->session_id = sta_inactivity_timer->session_id;
	inactivity_time->sta_inactivity_timeout =
		sta_inactivity_timer->sta_inactivity_timeout;

	wma_update_sta_inactivity_timeout(wma_handle, inactivity_time);
	qdf_mem_free(inactivity_time);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sme_get_reg_info(mac_handle_t mac_handle, uint32_t chan_freq,
			    uint32_t *regInfo1, uint32_t *regInfo2)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status;
	uint8_t i;
	bool found = false;

	status = sme_acquire_global_lock(&mac->sme);
	*regInfo1 = 0;
	*regInfo2 = 0;
	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;

	for (i = 0; i < CFG_VALID_CHANNEL_LIST_LEN; i++) {
		if (mac->scan.defaultPowerTable[i].center_freq == chan_freq) {
			SME_SET_CHANNEL_REG_POWER(*regInfo1,
				mac->scan.defaultPowerTable[i].tx_power);

			SME_SET_CHANNEL_MAX_TX_POWER(*regInfo2,
				mac->scan.defaultPowerTable[i].tx_power);
			found = true;
			break;
		}
	}
	if (!found)
		status = QDF_STATUS_E_FAILURE;

	sme_release_global_lock(&mac->sme);
	return status;
}

#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
QDF_STATUS sme_set_auto_shutdown_cb(mac_handle_t mac_handle,
				    void (*callback_fn)(void))
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
		  "%s: Plug in Auto shutdown event callback", __func__);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_STATUS_SUCCESS == status) {
		if (callback_fn)
			mac->sme.auto_shutdown_cb = callback_fn;
		sme_release_global_lock(&mac->sme);
	}

	return status;
}

/*
 * sme_set_auto_shutdown_timer() -
 *  API to set auto shutdown timer value in FW.
 *
 * mac_handle - The handle returned by mac_open
 * timer_val - The auto shutdown timer value to be set
 * Return Configuration message posting status, SUCCESS or Fail
 */
QDF_STATUS sme_set_auto_shutdown_timer(mac_handle_t mac_handle,
				       uint32_t timer_val)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct auto_shutdown_cmd *auto_sh_cmd;
	struct scheduler_msg message = {0};

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_STATUS_SUCCESS == status) {
		auto_sh_cmd = qdf_mem_malloc(sizeof(*auto_sh_cmd));
		if (!auto_sh_cmd) {
			sme_release_global_lock(&mac->sme);
			return QDF_STATUS_E_NOMEM;
		}

		auto_sh_cmd->timer_val = timer_val;

		/* serialize the req through MC thread */
		message.bodyptr = auto_sh_cmd;
		message.type = WMA_SET_AUTO_SHUTDOWN_TIMER_REQ;
		qdf_status = scheduler_post_message(QDF_MODULE_ID_SME,
						    QDF_MODULE_ID_WMA,
						    QDF_MODULE_ID_WMA,
						    &message);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				  "%s: Post Auto shutdown MSG fail", __func__);
			qdf_mem_free(auto_sh_cmd);
			sme_release_global_lock(&mac->sme);
			return QDF_STATUS_E_FAILURE;
		}
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Posted Auto shutdown MSG", __func__);
		sme_release_global_lock(&mac->sme);
	}

	return status;
}
#endif

#ifdef FEATURE_WLAN_CH_AVOID
/*
 * sme_ch_avoid_update_req() -
 *   API to request channel avoidance update from FW.
 *
 * mac_handle - The handle returned by mac_open
 * update_type - The update_type parameter of this request call
 * Return Configuration message posting status, SUCCESS or Fail
 */
QDF_STATUS sme_ch_avoid_update_req(mac_handle_t mac_handle)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	tSirChAvoidUpdateReq *cauReq;
	struct scheduler_msg message = {0};

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_STATUS_SUCCESS == status) {
		cauReq = qdf_mem_malloc(sizeof(tSirChAvoidUpdateReq));
		if (!cauReq) {
			sme_release_global_lock(&mac->sme);
			return QDF_STATUS_E_NOMEM;
		}

		cauReq->reserved_param = 0;

		/* serialize the req through MC thread */
		message.bodyptr = cauReq;
		message.type = WMA_CH_AVOID_UPDATE_REQ;
		qdf_status = scheduler_post_message(QDF_MODULE_ID_SME,
						    QDF_MODULE_ID_WMA,
						    QDF_MODULE_ID_WMA,
						    &message);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				  "%s: Post Ch Avoid Update MSG fail",
				  __func__);
			qdf_mem_free(cauReq);
			sme_release_global_lock(&mac->sme);
			return QDF_STATUS_E_FAILURE;
		}
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Posted Ch Avoid Update MSG", __func__);
		sme_release_global_lock(&mac->sme);
	}

	return status;
}
#endif

/**
 * sme_set_miracast() - Function to set miracast value to UMAC
 * @mac_handle:                Handle returned by macOpen
 * @filter_type:        0-Disabled, 1-Source, 2-sink
 *
 * This function passes down the value of miracast set by
 * framework to UMAC
 *
 * Return: Configuration message posting status, SUCCESS or Fail
 *
 */
QDF_STATUS sme_set_miracast(mac_handle_t mac_handle, uint8_t filter_type)
{
	struct scheduler_msg msg = {0};
	uint32_t *val;
	struct mac_context *mac_ptr = MAC_CONTEXT(mac_handle);

	if (!mac_ptr) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				"%s: Invalid pointer", __func__);
		return QDF_STATUS_E_INVAL;
	}

	val = qdf_mem_malloc(sizeof(*val));
	if (!val)
		return QDF_STATUS_E_NOMEM;

	*val = filter_type;

	msg.type = SIR_HAL_SET_MIRACAST;
	msg.reserved = 0;
	msg.bodyptr = val;

	if (!QDF_IS_STATUS_SUCCESS(
				scheduler_post_message(QDF_MODULE_ID_SME,
						       QDF_MODULE_ID_WMA,
						       QDF_MODULE_ID_WMA,
						       &msg))) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
		    "%s: Not able to post WDA_SET_MAS_ENABLE_DISABLE to WMA!",
		    __func__);
		qdf_mem_free(val);
		return QDF_STATUS_E_FAILURE;
	}

	mac_ptr->sme.miracast_value = filter_type;
	return QDF_STATUS_SUCCESS;
}

/**
 * sme_set_mas() - Function to set MAS value to UMAC
 * @val:	1-Enable, 0-Disable
 *
 * This function passes down the value of MAS to the UMAC. A
 * value of 1 will enable MAS and a value of 0 will disable MAS
 *
 * Return: Configuration message posting status, SUCCESS or Fail
 *
 */
QDF_STATUS sme_set_mas(uint32_t val)
{
	struct scheduler_msg msg = {0};
	uint32_t *ptr_val;

	ptr_val = qdf_mem_malloc(sizeof(*ptr_val));
	if (!ptr_val)
		return QDF_STATUS_E_NOMEM;

	*ptr_val = val;

	msg.type = SIR_HAL_SET_MAS;
	msg.reserved = 0;
	msg.bodyptr = ptr_val;

	if (!QDF_IS_STATUS_SUCCESS(
				scheduler_post_message(QDF_MODULE_ID_SME,
						       QDF_MODULE_ID_WMA,
						       QDF_MODULE_ID_WMA,
						       &msg))) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
		    "%s: Not able to post WDA_SET_MAS_ENABLE_DISABLE to WMA!",
		    __func__);
		qdf_mem_free(ptr_val);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * sme_roam_channel_change_req() - Channel change to new target channel
 * @mac_handle: handle returned by mac_open
 * @bssid: mac address of BSS
 * @ch_params: target channel information
 * @profile: CSR profile
 *
 * API to Indicate Channel change to new target channel
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_roam_channel_change_req(mac_handle_t mac_handle,
				       struct qdf_mac_addr bssid,
				       struct ch_params *ch_params,
				       struct csr_roam_profile *profile)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {

		status = csr_roam_channel_change_req(mac, bssid, ch_params,
				profile);
		sme_release_global_lock(&mac->sme);
	}
	return status;
}

/*
 * sme_process_channel_change_resp() -
 * API to Indicate Channel change response message to SAP.
 *
 * Return QDF_STATUS
 */
static QDF_STATUS sme_process_channel_change_resp(struct mac_context *mac,
					   uint16_t msg_type, void *msg_buf)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_info *roam_info;
	eCsrRoamResult roamResult;
	uint8_t session_id;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_NOMEM;

	roam_info->channelChangeRespEvent =
		(struct sSirChanChangeResponse *)msg_buf;

	session_id = roam_info->channelChangeRespEvent->sessionId;

	if (roam_info->channelChangeRespEvent->channelChangeStatus ==
	    QDF_STATUS_SUCCESS) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "sapdfs: Received success eWNI_SME_CHANNEL_CHANGE_RSP for sessionId[%d]",
			  session_id);
		roamResult = eCSR_ROAM_RESULT_CHANNEL_CHANGE_SUCCESS;
	} else {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "sapdfs: Received failure eWNI_SME_CHANNEL_CHANGE_RSP for sessionId[%d]",
			  session_id);
		roamResult = eCSR_ROAM_RESULT_CHANNEL_CHANGE_FAILURE;
	}

	csr_roam_call_callback(mac, session_id, roam_info, 0,
			       eCSR_ROAM_SET_CHANNEL_RSP, roamResult);

	qdf_mem_free(roam_info);

	return status;
}

/*
 * sme_roam_start_beacon_req() -
 * API to Indicate LIM to start Beacon Tx after SAP CAC Wait is completed.
 *
 * mac_handle - The handle returned by mac_open
 * sessionId - session ID
 * dfsCacWaitStatus - CAC WAIT status flag
 * Return QDF_STATUS
 */
QDF_STATUS sme_roam_start_beacon_req(mac_handle_t mac_handle,
				     struct qdf_mac_addr bssid,
				     uint8_t dfsCacWaitStatus)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);

	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = csr_roam_start_beacon_req(mac, bssid,
						dfsCacWaitStatus);
		sme_release_global_lock(&mac->sme);
	}
	return status;
}

QDF_STATUS sme_csa_restart(struct mac_context *mac_ctx, uint8_t session_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = csr_csa_restart(mac_ctx, session_id);
		sme_release_global_lock(&mac_ctx->sme);
	}

	return status;
}

QDF_STATUS sme_roam_csa_ie_request(mac_handle_t mac_handle,
				   struct qdf_mac_addr bssid,
				   uint32_t target_chan_freq, uint8_t csaIeReqd,
				   struct ch_params *ch_params)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = csr_roam_send_chan_sw_ie_request(mac, bssid,
				target_chan_freq, csaIeReqd, ch_params);
		sme_release_global_lock(&mac->sme);
	}
	return status;
}

/*
 * sme_init_thermal_info() -
 * SME API to initialize the thermal mitigation parameters
 *
 * mac_handle
 * thermalParam : thermal mitigation parameters
 * Return QDF_STATUS
 */
QDF_STATUS sme_init_thermal_info(mac_handle_t mac_handle)
{
	t_thermal_mgmt *pWmaParam;
	struct scheduler_msg msg = {0};
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct wlan_fwol_thermal_temp thermal_temp = {0};
	QDF_STATUS status;

	pWmaParam = qdf_mem_malloc(sizeof(t_thermal_mgmt));
	if (!pWmaParam)
		return QDF_STATUS_E_NOMEM;

	status = ucfg_fwol_get_thermal_temp(mac->psoc, &thermal_temp);
	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	pWmaParam->thermalMgmtEnabled = thermal_temp.thermal_mitigation_enable;
	pWmaParam->throttlePeriod = thermal_temp.throttle_period;

	pWmaParam->throttle_duty_cycle_tbl[0] =
				thermal_temp.throttle_dutycycle_level[0];
	pWmaParam->throttle_duty_cycle_tbl[1] =
				thermal_temp.throttle_dutycycle_level[1];
	pWmaParam->throttle_duty_cycle_tbl[2] =
				thermal_temp.throttle_dutycycle_level[2];
	pWmaParam->throttle_duty_cycle_tbl[3] =
				thermal_temp.throttle_dutycycle_level[3];

	pWmaParam->thermalLevels[0].minTempThreshold =
				thermal_temp.thermal_temp_min_level[0];
	pWmaParam->thermalLevels[0].maxTempThreshold =
				thermal_temp.thermal_temp_max_level[0];
	pWmaParam->thermalLevels[1].minTempThreshold =
				thermal_temp.thermal_temp_min_level[1];
	pWmaParam->thermalLevels[1].maxTempThreshold =
				thermal_temp.thermal_temp_max_level[1];
	pWmaParam->thermalLevels[2].minTempThreshold =
				thermal_temp.thermal_temp_min_level[2];
	pWmaParam->thermalLevels[2].maxTempThreshold =
				thermal_temp.thermal_temp_max_level[2];
	pWmaParam->thermalLevels[3].minTempThreshold =
				thermal_temp.thermal_temp_min_level[3];
	pWmaParam->thermalLevels[3].maxTempThreshold =
				thermal_temp.thermal_temp_max_level[3];
	pWmaParam->thermal_action = thermal_temp.thermal_action;
	if (QDF_STATUS_SUCCESS == sme_acquire_global_lock(&mac->sme)) {
		msg.type = WMA_INIT_THERMAL_INFO_CMD;
		msg.bodyptr = pWmaParam;

		if (!QDF_IS_STATUS_SUCCESS
			    (scheduler_post_message(QDF_MODULE_ID_SME,
						    QDF_MODULE_ID_WMA,
						    QDF_MODULE_ID_WMA,
						    &msg))) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				  "%s: Not able to post WMA_SET_THERMAL_INFO_CMD to WMA!",
				  __func__);
			qdf_mem_free(pWmaParam);
			sme_release_global_lock(&mac->sme);
			return QDF_STATUS_E_FAILURE;
		}
		sme_release_global_lock(&mac->sme);
		return QDF_STATUS_SUCCESS;
	}
	qdf_mem_free(pWmaParam);
	return QDF_STATUS_E_FAILURE;
}

/**
 * sme_add_set_thermal_level_callback() - Plug in set thermal level callback
 * @mac_handle:	Handle returned by macOpen
 * @callback:	sme_set_thermal_level_callback
 *
 * Plug in set thermal level callback
 *
 * Return: none
 */
void sme_add_set_thermal_level_callback(mac_handle_t mac_handle,
		sme_set_thermal_level_callback callback)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	mac->sme.set_thermal_level_cb = callback;
}

/**
 * sme_set_thermal_level() - SME API to set the thermal mitigation level
 * @mac_handle: Opaque handle to the global MAC context
 * @level:       Thermal mitigation level
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_set_thermal_level(mac_handle_t mac_handle, uint8_t level)
{
	struct scheduler_msg msg = {0};
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	if (QDF_STATUS_SUCCESS == sme_acquire_global_lock(&mac->sme)) {
		qdf_mem_zero(&msg, sizeof(msg));
		msg.type = WMA_SET_THERMAL_LEVEL;
		msg.bodyval = level;

		qdf_status =  scheduler_post_message(QDF_MODULE_ID_SME,
						     QDF_MODULE_ID_WMA,
						     QDF_MODULE_ID_WMA, &msg);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				   "%s: Not able to post WMA_SET_THERMAL_LEVEL to WMA!",
				   __func__);
			sme_release_global_lock(&mac->sme);
			return QDF_STATUS_E_FAILURE;
		}
		sme_release_global_lock(&mac->sme);
		return QDF_STATUS_SUCCESS;
	}
	return QDF_STATUS_E_FAILURE;
}

/*
 * sme_txpower_limit() -
 * SME API to set txpower limits
 *
 * mac_handle
 * psmetx : power limits for 2g/5g
 * Return QDF_STATUS
 */
QDF_STATUS sme_txpower_limit(mac_handle_t mac_handle,
			     struct tx_power_limit *psmetx)
{
	QDF_STATUS status;
	struct scheduler_msg message = {0};
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct tx_power_limit *tx_power_limit;

	tx_power_limit = qdf_mem_malloc(sizeof(*tx_power_limit));
	if (!tx_power_limit)
		return QDF_STATUS_E_FAILURE;

	*tx_power_limit = *psmetx;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(tx_power_limit);
		return status;
	}

	message.type = WMA_TX_POWER_LIMIT;
	message.bodyptr = tx_power_limit;
	status = scheduler_post_message(QDF_MODULE_ID_SME, QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &message);
	if (QDF_IS_STATUS_ERROR(status)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: not able to post WMA_TX_POWER_LIMIT",
			  __func__);
		status = QDF_STATUS_E_FAILURE;
		qdf_mem_free(tx_power_limit);
	}

	sme_release_global_lock(&mac->sme);

	return status;
}

QDF_STATUS sme_update_connect_debug(mac_handle_t mac_handle, uint32_t set_value)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	mac->mlme_cfg->gen.debug_packet_log = set_value;
	return status;
}

/*
 * sme_ap_disable_intra_bss_fwd() -
 * SME will send message to WMA to set Intra BSS in txrx
 *
 * mac_handle - The handle returned by mac_open
 * sessionId - session id ( vdev id)
 * disablefwd - bool value that indicate disable intrabss fwd disable
 * Return QDF_STATUS
 */
QDF_STATUS sme_ap_disable_intra_bss_fwd(mac_handle_t mac_handle,
					uint8_t sessionId,
					bool disablefwd)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	int status = QDF_STATUS_SUCCESS;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct scheduler_msg message = {0};
	tpDisableIntraBssFwd pSapDisableIntraFwd = NULL;

	/* Prepare the request to send to SME. */
	pSapDisableIntraFwd = qdf_mem_malloc(sizeof(tDisableIntraBssFwd));
	if (!pSapDisableIntraFwd)
		return QDF_STATUS_E_NOMEM;

	pSapDisableIntraFwd->sessionId = sessionId;
	pSapDisableIntraFwd->disableintrabssfwd = disablefwd;

	status = sme_acquire_global_lock(&mac->sme);

	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(pSapDisableIntraFwd);
		return QDF_STATUS_E_FAILURE;
	}
	/* serialize the req through MC thread */
	message.bodyptr = pSapDisableIntraFwd;
	message.type = WMA_SET_SAP_INTRABSS_DIS;
	qdf_status = scheduler_post_message(QDF_MODULE_ID_SME,
					    QDF_MODULE_ID_WMA,
					    QDF_MODULE_ID_WMA,
					    &message);
	if (QDF_IS_STATUS_ERROR(status)) {
		status = QDF_STATUS_E_FAILURE;
		qdf_mem_free(pSapDisableIntraFwd);
	}
	sme_release_global_lock(&mac->sme);

	return status;
}

#ifdef WLAN_FEATURE_STATS_EXT

void sme_stats_ext_register_callback(mac_handle_t mac_handle,
				     stats_ext_cb callback)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	if (!mac) {
		sme_err("Invalid mac context");
		return;
	}

	mac->sme.stats_ext_cb = callback;
}

void sme_stats_ext_deregister_callback(mac_handle_t mac_handle)
{
	sme_stats_ext_register_callback(mac_handle, NULL);
}

void sme_stats_ext2_register_callback(mac_handle_t mac_handle,
				      stats_ext2_cb callback)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	if (!mac) {
		sme_err("Invalid mac context");
		return;
	}

	mac->sme.stats_ext2_cb = callback;
}

/*
 * sme_stats_ext_request() -
 * Function called when HDD receives STATS EXT vendor command from userspace
 *
 * sessionID - vdevID for the stats ext request
 * input - Stats Ext Request structure ptr
 * Return QDF_STATUS
 */
QDF_STATUS sme_stats_ext_request(uint8_t session_id, tpStatsExtRequestReq input)
{
	struct scheduler_msg msg = {0};
	tpStatsExtRequest data;
	size_t data_len;

	data_len = sizeof(tStatsExtRequest) + input->request_data_len;
	data = qdf_mem_malloc(data_len);
	if (!data)
		return QDF_STATUS_E_NOMEM;

	data->vdev_id = session_id;
	data->request_data_len = input->request_data_len;
	if (input->request_data_len)
		qdf_mem_copy(data->request_data,
			     input->request_data, input->request_data_len);

	msg.type = WMA_STATS_EXT_REQUEST;
	msg.reserved = 0;
	msg.bodyptr = data;

	if (QDF_STATUS_SUCCESS != scheduler_post_message(QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_WMA,
							 QDF_MODULE_ID_WMA,
							 &msg)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Not able to post WMA_STATS_EXT_REQUEST message to WMA",
			  __func__);
		qdf_mem_free(data);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * sme_stats_ext_event() - eWNI_SME_STATS_EXT_EVENT processor
 * @mac: Global MAC context
 * @msg: "stats ext" message

 * This callback function called when SME received eWNI_SME_STATS_EXT_EVENT
 * response from WMA
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sme_stats_ext_event(struct mac_context *mac,
				      struct stats_ext_event *msg)
{
	if (!msg) {
		sme_err("Null msg");
		return QDF_STATUS_E_FAILURE;
	}

	if (mac->sme.stats_ext_cb)
		mac->sme.stats_ext_cb(mac->hdd_handle, msg);

	return QDF_STATUS_SUCCESS;
}

#else

static QDF_STATUS sme_stats_ext_event(struct mac_context *mac,
				      struct stats_ext_event *msg)
{
	return QDF_STATUS_SUCCESS;
}

#endif

#ifdef FEATURE_FW_STATE
QDF_STATUS sme_get_fw_state(mac_handle_t mac_handle,
			    fw_state_callback callback,
			    void *context)
{
	QDF_STATUS status;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	tp_wma_handle wma_handle;

	SME_ENTER();

	mac_ctx->sme.fw_state_cb = callback;
	mac_ctx->sme.fw_state_context = context;
	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	status = wma_get_fw_state(wma_handle);

	SME_EXIT();
	return status;
}

/**
 * sme_fw_state_resp() - eWNI_SME_FW_STATUS_IND processor
 * @mac: Global MAC context

 * This callback function called when SME received eWNI_SME_FW_STATUS_IND
 * response from WMA
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sme_fw_state_resp(struct mac_context *mac)
{
	if (mac->sme.fw_state_cb)
		mac->sme.fw_state_cb(mac->sme.fw_state_context);
	mac->sme.fw_state_cb = NULL;
	mac->sme.fw_state_context = NULL;

	return QDF_STATUS_SUCCESS;
}

#else /* FEATURE_FW_STATE */
static QDF_STATUS sme_fw_state_resp(struct mac_context *mac)
{
	return QDF_STATUS_SUCCESS;
}

#endif /* FEATURE_FW_STATE */

/*
 * sme_update_dfs_scan_mode() -
 * Update DFS roam scan mode
 *	    This function is called through dynamic setConfig callback function
 *	    to configure allowDFSChannelRoam.
 * mac_handle: Opaque handle to the global MAC context
 * sessionId - Session Identifier
 * allowDFSChannelRoam - DFS roaming scan mode 0 (disable),
 *	    1 (passive), 2 (active)
 * Return QDF_STATUS_SUCCESS - SME update DFS roaming scan config
 *	    successfully.
 *	  Other status means SME failed to update DFS roaming scan config.
 */
QDF_STATUS sme_update_dfs_scan_mode(mac_handle_t mac_handle, uint8_t sessionId,
				    uint8_t allowDFSChannelRoam)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (sessionId >= WLAN_MAX_VDEVS) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Invalid sme session id: %d"), sessionId);
		return QDF_STATUS_E_INVAL;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "LFR runtime successfully set AllowDFSChannelRoam Mode to %d - old value is %d - roam state is %s",
			  allowDFSChannelRoam,
			  mac->mlme_cfg->lfr.roaming_dfs_channel,
			  mac_trace_get_neighbour_roam_state(mac->roam.
							     neighborRoamInfo
							     [sessionId].
							    neighborRoamState));
		mac->mlme_cfg->lfr.roaming_dfs_channel =
			allowDFSChannelRoam;
		if (mac->mlme_cfg->lfr.roam_scan_offload_enabled)
			csr_roam_update_cfg(mac, sessionId,
					    REASON_ROAM_DFS_SCAN_MODE_CHANGED);

		sme_release_global_lock(&mac->sme);
	}


	return status;
}

/*
 * sme_get_dfs_scan_mode() - get DFS roam scan mode
 *	  This is a synchronous call
 *
 * mac_handle - The handle returned by mac_open.
 * Return DFS roaming scan mode 0 (disable), 1 (passive), 2 (active)
 */
uint8_t sme_get_dfs_scan_mode(mac_handle_t mac_handle)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	return mac->mlme_cfg->lfr.roaming_dfs_channel;
}

/*
 * sme_modify_add_ie() -
 * This function sends msg to updates the additional IE buffers in PE
 *
 * mac_handle - global structure
 * pModifyIE - pointer to tModifyIE structure
 * updateType - type of buffer
 * Return Success or failure
 */
QDF_STATUS sme_modify_add_ie(mac_handle_t mac_handle,
			     tSirModifyIE *pModifyIE, eUpdateIEsType updateType)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);

	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = csr_roam_modify_add_ies(mac, pModifyIE, updateType);
		sme_release_global_lock(&mac->sme);
	}
	return status;
}

/*
 * sme_update_add_ie() -
 * This function sends msg to updates the additional IE buffers in PE
 *
 * mac_handle - global structure
 * pUpdateIE - pointer to structure tUpdateIE
 * updateType - type of buffer
 * Return Success or failure
 */
QDF_STATUS sme_update_add_ie(mac_handle_t mac_handle,
			     tSirUpdateIE *pUpdateIE, eUpdateIEsType updateType)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);

	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = csr_roam_update_add_ies(mac, pUpdateIE, updateType);
		sme_release_global_lock(&mac->sme);
	}
	return status;
}

/**
 * sme_update_dsc_pto_up_mapping()
 * @mac_handle: Opaque handle to the global MAC context
 * @dscpmapping: pointer to DSCP mapping structure
 * @sessionId: SME session id
 *
 * This routine is called to update dscp mapping
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_update_dsc_pto_up_mapping(mac_handle_t mac_handle,
					 enum sme_qos_wmmuptype *dscpmapping,
					 uint8_t sessionId)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t i, j, peSessionId;
	struct csr_roam_session *pCsrSession = NULL;
	struct pe_session *pSession = NULL;

	status = sme_acquire_global_lock(&mac->sme);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;
	pCsrSession = CSR_GET_SESSION(mac, sessionId);
	if (!pCsrSession) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				FL("Session lookup fails for CSR session"));
		sme_release_global_lock(&mac->sme);
		return QDF_STATUS_E_FAILURE;
	}
	if (!CSR_IS_SESSION_VALID(mac, sessionId)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				FL("Invalid session Id %u"), sessionId);
		sme_release_global_lock(&mac->sme);
		return QDF_STATUS_E_FAILURE;
	}

	pSession = pe_find_session_by_bssid(mac,
				pCsrSession->connectedProfile.bssid.bytes,
				&peSessionId);

	if (!pSession) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				FL(" Session lookup fails for BSSID"));
		sme_release_global_lock(&mac->sme);
		return QDF_STATUS_E_FAILURE;
	}

	if (!pSession->QosMapSet.present) {
		sme_debug("QOS Mapping IE not present");
		sme_release_global_lock(&mac->sme);
		return QDF_STATUS_E_FAILURE;
	}

	for (i = 0; i < SME_QOS_WMM_UP_MAX; i++) {
		for (j = pSession->QosMapSet.dscp_range[i][0];
			j <= pSession->QosMapSet.dscp_range[i][1] &&
			j <= WLAN_MAX_DSCP; j++)
				dscpmapping[j] = i;
	}
	for (i = 0; i < pSession->QosMapSet.num_dscp_exceptions; i++)
		if (pSession->QosMapSet.dscp_exceptions[i][0] <= WLAN_MAX_DSCP)
			dscpmapping[pSession->QosMapSet.dscp_exceptions[i][0]] =
				pSession->QosMapSet.dscp_exceptions[i][1];

	sme_release_global_lock(&mac->sme);
	return status;
}

QDF_STATUS sme_get_valid_channels_by_band(mac_handle_t mac_handle,
					  uint8_t wifi_band,
					  uint32_t *valid_chan_list,
					  uint8_t *valid_chan_len)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t chan_freq_list[CFG_VALID_CHANNEL_LIST_LEN] = { 0 };
	uint8_t num_channels = 0;
	uint8_t i = 0;
	uint32_t valid_channels = CFG_VALID_CHANNEL_LIST_LEN;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	if (!valid_chan_list || !valid_chan_len) {
		sme_err("Output channel list/NumChannels is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (wifi_band >= WIFI_BAND_MAX) {
		sme_err("Invalid wifi Band: %d", wifi_band);
		return QDF_STATUS_E_INVAL;
	}

	status = sme_get_cfg_valid_channels(&chan_freq_list[0],
					    &valid_channels);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Fail to get valid channel list (err=%d)", status);
		return status;
	}

	switch (wifi_band) {
	case WIFI_BAND_UNSPECIFIED:
		sme_debug("Unspec Band, return all %d valid channels",
			  valid_channels);
		num_channels = valid_channels;
		for (i = 0; i < valid_channels; i++)
			valid_chan_list[i] = chan_freq_list[i];
		break;

	case WIFI_BAND_BG:
		sme_debug("WIFI_BAND_BG (2.4 GHz)");
		for (i = 0; i < valid_channels; i++) {
			if (WLAN_REG_IS_24GHZ_CH_FREQ(chan_freq_list[i]))
				valid_chan_list[num_channels++] =
					chan_freq_list[i];
		}
		break;

	case WIFI_BAND_A:
		sme_debug("WIFI_BAND_A (5 GHz without DFS)");
		for (i = 0; i < valid_channels; i++) {
			if (WLAN_REG_IS_5GHZ_CH_FREQ(chan_freq_list[i]) &&
			    !wlan_reg_is_dfs_for_freq(mac_ctx->pdev,
						      chan_freq_list[i]))
				valid_chan_list[num_channels++] =
					chan_freq_list[i];
		}
		break;

	case WIFI_BAND_ABG:
		sme_debug("WIFI_BAND_ABG (2.4 GHz + 5 GHz; no DFS)");
		for (i = 0; i < valid_channels; i++) {
			if ((WLAN_REG_IS_24GHZ_CH_FREQ(chan_freq_list[i]) ||
			     WLAN_REG_IS_5GHZ_CH_FREQ(chan_freq_list[i])) &&
			    !wlan_reg_is_dfs_for_freq(mac_ctx->pdev,
						      chan_freq_list[i]))
				valid_chan_list[num_channels++] =
					chan_freq_list[i];
		}
		break;

	case WIFI_BAND_A_DFS_ONLY:
		sme_debug("WIFI_BAND_A_DFS (5 GHz DFS only)");
		for (i = 0; i < valid_channels; i++) {
			if (WLAN_REG_IS_5GHZ_CH_FREQ(chan_freq_list[i]) &&
			    wlan_reg_is_dfs_for_freq(mac_ctx->pdev,
						     chan_freq_list[i]))
				valid_chan_list[num_channels++] =
					chan_freq_list[i];
		}
		break;

	case WIFI_BAND_A_WITH_DFS:
		sme_debug("WIFI_BAND_A_WITH_DFS (5 GHz with DFS)");
		for (i = 0; i < valid_channels; i++) {
			if (WLAN_REG_IS_5GHZ_CH_FREQ(chan_freq_list[i]))
				valid_chan_list[num_channels++] =
					chan_freq_list[i];
		}
		break;

	case WIFI_BAND_ABG_WITH_DFS:
		sme_debug("WIFI_BAND_ABG_WITH_DFS (2.4 GHz+5 GHz with DFS)");
		for (i = 0; i < valid_channels; i++) {
			if (WLAN_REG_IS_24GHZ_CH_FREQ(chan_freq_list[i]) ||
			    WLAN_REG_IS_5GHZ_CH_FREQ(chan_freq_list[i]))
				valid_chan_list[num_channels++] =
					chan_freq_list[i];
		}
		break;

	default:
		sme_err("Unknown wifi Band: %d", wifi_band);
		return QDF_STATUS_E_INVAL;
	}
	*valid_chan_len = num_channels;

	return status;
}

#ifdef FEATURE_WLAN_EXTSCAN

QDF_STATUS
sme_ext_scan_get_capabilities(mac_handle_t mac_handle,
			      struct extscan_capabilities_params *params)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};
	struct extscan_capabilities_params *bodyptr;

	/* per contract must make a copy of the params when messaging */
	bodyptr = qdf_mem_malloc(sizeof(*bodyptr));
	if (!bodyptr)
		return QDF_STATUS_E_NOMEM;

	*bodyptr = *params;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		/* Serialize the req through MC thread */
		message.bodyptr = bodyptr;
		message.type = WMA_EXTSCAN_GET_CAPABILITIES_REQ;
		qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
			  NO_SESSION, message.type);
		status = scheduler_post_message(QDF_MODULE_ID_SME,
						QDF_MODULE_ID_WMA,
						QDF_MODULE_ID_WMA,
						&message);
		sme_release_global_lock(&mac->sme);
	}

	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("failure: %d", status);
		qdf_mem_free(bodyptr);
	}
	return status;
}

QDF_STATUS
sme_ext_scan_start(mac_handle_t mac_handle,
		   struct wifi_scan_cmd_req_params *params)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};
	struct wifi_scan_cmd_req_params *bodyptr;

	/* per contract must make a copy of the params when messaging */
	bodyptr = qdf_mem_malloc(sizeof(*bodyptr));
	if (!bodyptr)
		return QDF_STATUS_E_NOMEM;

	*bodyptr = *params;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		/* Serialize the req through MC thread */
		message.bodyptr = bodyptr;
		message.type = WMA_EXTSCAN_START_REQ;
		qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
			  NO_SESSION, message.type);
		status = scheduler_post_message(QDF_MODULE_ID_SME,
						QDF_MODULE_ID_WMA,
						QDF_MODULE_ID_WMA,
						&message);
		sme_release_global_lock(&mac->sme);
	}

	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("failure: %d", status);
		qdf_mem_free(bodyptr);
	}
	return status;
}

QDF_STATUS sme_ext_scan_stop(mac_handle_t mac_handle,
			     struct extscan_stop_req_params *params)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};
	struct extscan_stop_req_params *bodyptr;

	/* per contract must make a copy of the params when messaging */
	bodyptr = qdf_mem_malloc(sizeof(*bodyptr));
	if (!bodyptr)
		return QDF_STATUS_E_NOMEM;

	*bodyptr = *params;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		/* Serialize the req through MC thread */
		message.bodyptr = bodyptr;
		message.type = WMA_EXTSCAN_STOP_REQ;
		qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
			  NO_SESSION, message.type);
		status = scheduler_post_message(QDF_MODULE_ID_SME,
						QDF_MODULE_ID_WMA,
						QDF_MODULE_ID_WMA,
						&message);
		sme_release_global_lock(&mac->sme);
	}

	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("failure: %d", status);
		qdf_mem_free(bodyptr);
	}
	return status;
}

QDF_STATUS
sme_set_bss_hotlist(mac_handle_t mac_handle,
		    struct extscan_bssid_hotlist_set_params *params)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};
	struct extscan_bssid_hotlist_set_params *bodyptr;

	/* per contract must make a copy of the params when messaging */
	bodyptr = qdf_mem_malloc(sizeof(*bodyptr));
	if (!bodyptr)
		return QDF_STATUS_E_NOMEM;

	*bodyptr = *params;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		/* Serialize the req through MC thread */
		message.bodyptr = bodyptr;
		message.type = WMA_EXTSCAN_SET_BSSID_HOTLIST_REQ;
		qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
			  NO_SESSION, message.type);
		status = scheduler_post_message(QDF_MODULE_ID_SME,
						QDF_MODULE_ID_WMA,
						QDF_MODULE_ID_WMA, &message);
		sme_release_global_lock(&mac->sme);
	}

	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("failure: %d", status);
		qdf_mem_free(bodyptr);
	}
	return status;
}

QDF_STATUS
sme_reset_bss_hotlist(mac_handle_t mac_handle,
		      struct extscan_bssid_hotlist_reset_params *params)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};
	struct extscan_bssid_hotlist_reset_params *bodyptr;

	/* per contract must make a copy of the params when messaging */
	bodyptr = qdf_mem_malloc(sizeof(*bodyptr));
	if (!bodyptr)
		return QDF_STATUS_E_NOMEM;

	*bodyptr = *params;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		/* Serialize the req through MC thread */
		message.bodyptr = bodyptr;
		message.type = WMA_EXTSCAN_RESET_BSSID_HOTLIST_REQ;
		MTRACE(qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
				 NO_SESSION, message.type));
		status = scheduler_post_message(QDF_MODULE_ID_SME,
						QDF_MODULE_ID_WMA,
						QDF_MODULE_ID_WMA, &message);
		sme_release_global_lock(&mac->sme);
	}

	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("failure: %d", status);
		qdf_mem_free(bodyptr);
	}
	return status;
}

QDF_STATUS
sme_set_significant_change(mac_handle_t mac_handle,
			   struct extscan_set_sig_changereq_params *params)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};
	struct extscan_set_sig_changereq_params *bodyptr;

	/* per contract must make a copy of the params when messaging */
	bodyptr = qdf_mem_malloc(sizeof(*bodyptr));
	if (!bodyptr)
		return QDF_STATUS_E_NOMEM;

	*bodyptr = *params;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		/* Serialize the req through MC thread */
		message.bodyptr = bodyptr;
		message.type = WMA_EXTSCAN_SET_SIGNF_CHANGE_REQ;
		status = scheduler_post_message(QDF_MODULE_ID_SME,
						QDF_MODULE_ID_WMA,
						QDF_MODULE_ID_WMA,
						&message);
		sme_release_global_lock(&mac->sme);
	}
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("failure: %d", status);
		qdf_mem_free(bodyptr);
	}
	return status;
}

QDF_STATUS
sme_reset_significant_change(mac_handle_t mac_handle,
			     struct extscan_capabilities_reset_params *params)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};
	struct extscan_capabilities_reset_params *bodyptr;

	/* per contract must make a copy of the params when messaging */
	bodyptr = qdf_mem_malloc(sizeof(*bodyptr));
	if (!bodyptr)
		return QDF_STATUS_E_NOMEM;

	*bodyptr = *params;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		/* Serialize the req through MC thread */
		message.bodyptr = bodyptr;
		message.type = WMA_EXTSCAN_RESET_SIGNF_CHANGE_REQ;
		MTRACE(qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
				 NO_SESSION, message.type));
		status = scheduler_post_message(QDF_MODULE_ID_SME,
						QDF_MODULE_ID_WMA,
						QDF_MODULE_ID_WMA,
						&message);
		sme_release_global_lock(&mac->sme);
	}
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("failure: %d", status);
		qdf_mem_free(bodyptr);
	}

	return status;
}

QDF_STATUS
sme_get_cached_results(mac_handle_t mac_handle,
		       struct extscan_cached_result_params *params)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};
	struct extscan_cached_result_params *bodyptr;

	/* per contract must make a copy of the params when messaging */
	bodyptr = qdf_mem_malloc(sizeof(*bodyptr));
	if (!bodyptr)
		return QDF_STATUS_E_NOMEM;

	*bodyptr = *params;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		/* Serialize the req through MC thread */
		message.bodyptr = bodyptr;
		message.type = WMA_EXTSCAN_GET_CACHED_RESULTS_REQ;
		qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
			  NO_SESSION, message.type);
		status = scheduler_post_message(QDF_MODULE_ID_SME,
						QDF_MODULE_ID_WMA,
						QDF_MODULE_ID_WMA,
						&message);
		sme_release_global_lock(&mac->sme);
	}

	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("failure: %d", status);
		qdf_mem_free(bodyptr);
	}
	return status;
}

QDF_STATUS sme_set_epno_list(mac_handle_t mac_handle,
			     struct wifi_enhanced_pno_params *params)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};
	struct wifi_enhanced_pno_params *req_msg;
	int len;

	SME_ENTER();

	/* per contract must make a copy of the params when messaging */
	len = sizeof(*req_msg) +
		(params->num_networks * sizeof(req_msg->networks[0]));

	req_msg = qdf_mem_malloc(len);
	if (!req_msg)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_copy(req_msg, params, len);

	status = sme_acquire_global_lock(&mac->sme);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("sme_acquire_global_lock failed!(status=%d)",
			status);
		qdf_mem_free(req_msg);
		return status;
	}

	/* Serialize the req through MC thread */
	message.bodyptr = req_msg;
	message.type    = WMA_SET_EPNO_LIST_REQ;
	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &message);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("scheduler_post_msg failed!(err=%d)", status);
		qdf_mem_free(req_msg);
	}
	sme_release_global_lock(&mac->sme);

	return status;
}

QDF_STATUS sme_set_passpoint_list(mac_handle_t mac_handle,
				  struct wifi_passpoint_req_param *params)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};
	struct wifi_passpoint_req_param *req_msg;
	int len;

	SME_ENTER();

	len = sizeof(*req_msg) +
		(params->num_networks * sizeof(params->networks[0]));
	req_msg = qdf_mem_malloc(len);
	if (!req_msg)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_copy(req_msg, params, len);

	status = sme_acquire_global_lock(&mac->sme);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("sme_acquire_global_lock failed!(status=%d)",
			status);
		qdf_mem_free(req_msg);
		return status;
	}

	/* Serialize the req through MC thread */
	message.bodyptr = req_msg;
	message.type    = WMA_SET_PASSPOINT_LIST_REQ;
	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &message);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("scheduler_post_msg failed!(err=%d)",
			status);
		qdf_mem_free(req_msg);
	}
	sme_release_global_lock(&mac->sme);
	return status;
}

QDF_STATUS sme_reset_passpoint_list(mac_handle_t mac_handle,
				    struct wifi_passpoint_req_param *params)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};
	struct wifi_passpoint_req_param *req_msg;

	SME_ENTER();

	req_msg = qdf_mem_malloc(sizeof(*req_msg));
	if (!req_msg)
		return QDF_STATUS_E_NOMEM;

	*req_msg = *params;

	status = sme_acquire_global_lock(&mac->sme);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("sme_acquire_global_lock failed!(status=%d)",
			status);
		qdf_mem_free(req_msg);
		return status;
	}

	/* Serialize the req through MC thread */
	message.bodyptr = req_msg;
	message.type    = WMA_RESET_PASSPOINT_LIST_REQ;
	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &message);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("scheduler_post_msg failed!(err=%d)",
			status);
		qdf_mem_free(req_msg);
	}
	sme_release_global_lock(&mac->sme);
	return status;
}

QDF_STATUS sme_ext_scan_register_callback(mac_handle_t mac_handle,
					  ext_scan_ind_cb ext_scan_ind_cb)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		mac->sme.ext_scan_ind_cb = ext_scan_ind_cb;
		sme_release_global_lock(&mac->sme);
	}
	return status;
}
#endif /* FEATURE_WLAN_EXTSCAN */

/**
 * sme_send_wisa_params(): Pass WISA mode to WMA
 * @mac_handle: Opaque handle to the global MAC context
 * @wisa_params: pointer to WISA params struct
 * @sessionId: SME session id
 *
 * Pass WISA params to WMA
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_set_wisa_params(mac_handle_t mac_handle,
			       struct sir_wisa_params *wisa_params)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};
	struct sir_wisa_params *cds_msg_wisa_params;

	cds_msg_wisa_params = qdf_mem_malloc(sizeof(struct sir_wisa_params));
	if (!cds_msg_wisa_params)
		return QDF_STATUS_E_NOMEM;

	*cds_msg_wisa_params = *wisa_params;
	status = sme_acquire_global_lock(&mac->sme);

	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(cds_msg_wisa_params);
		return QDF_STATUS_E_FAILURE;
	}
	message.bodyptr = cds_msg_wisa_params;
	message.type = WMA_SET_WISA_PARAMS;
	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &message);
	if (QDF_IS_STATUS_ERROR(status))
		qdf_mem_free(cds_msg_wisa_params);
	sme_release_global_lock(&mac->sme);
	return status;
}

#ifdef WLAN_FEATURE_LINK_LAYER_STATS

/*
 * sme_ll_stats_clear_req() -
 * SME API to clear Link Layer Statistics
 *
 * mac_handle
 * pclearStatsReq: Link Layer clear stats request params structure
 * Return QDF_STATUS
 */
QDF_STATUS sme_ll_stats_clear_req(mac_handle_t mac_handle,
				  tSirLLStatsClearReq *pclearStatsReq)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};
	tSirLLStatsClearReq *clear_stats_req;

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		  "staId = %u", pclearStatsReq->staId);
	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		  "statsClearReqMask = 0x%X",
		  pclearStatsReq->statsClearReqMask);
	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		  "stopReq = %u", pclearStatsReq->stopReq);
	if (!sme_is_session_id_valid(mac_handle, pclearStatsReq->staId)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: invalid staId %d",
			  __func__, pclearStatsReq->staId);
		return QDF_STATUS_E_INVAL;
	}

	clear_stats_req = qdf_mem_malloc(sizeof(*clear_stats_req));
	if (!clear_stats_req)
		return QDF_STATUS_E_NOMEM;

	*clear_stats_req = *pclearStatsReq;

	if (QDF_STATUS_SUCCESS == sme_acquire_global_lock(&mac->sme)) {
		/* Serialize the req through MC thread */
		message.bodyptr = clear_stats_req;
		message.type = WMA_LINK_LAYER_STATS_CLEAR_REQ;
		MTRACE(qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
				 NO_SESSION, message.type));
		qdf_status = scheduler_post_message(QDF_MODULE_ID_SME,
						    QDF_MODULE_ID_WMA,
						    QDF_MODULE_ID_WMA,
						    &message);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				  "%s: not able to post WMA_LL_STATS_CLEAR_REQ",
				  __func__);
			qdf_mem_free(clear_stats_req);
			status = QDF_STATUS_E_FAILURE;
		}
		sme_release_global_lock(&mac->sme);
	} else {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			"%s: sme_acquire_global_lock error", __func__);
		qdf_mem_free(clear_stats_req);
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}

/*
 * sme_ll_stats_set_req() -
 * SME API to set the Link Layer Statistics
 *
 * mac_handle
 * psetStatsReq: Link Layer set stats request params structure
 * Return QDF_STATUS
 */
QDF_STATUS sme_ll_stats_set_req(mac_handle_t mac_handle, tSirLLStatsSetReq
				*psetStatsReq)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};
	tSirLLStatsSetReq *set_stats_req;

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		  "%s:  MPDU Size = %u", __func__,
		  psetStatsReq->mpduSizeThreshold);
	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		  " Aggressive Stats Collections = %u",
		  psetStatsReq->aggressiveStatisticsGathering);

	set_stats_req = qdf_mem_malloc(sizeof(*set_stats_req));
	if (!set_stats_req)
		return QDF_STATUS_E_NOMEM;

	*set_stats_req = *psetStatsReq;

	if (QDF_STATUS_SUCCESS == sme_acquire_global_lock(&mac->sme)) {
		/* Serialize the req through MC thread */
		message.bodyptr = set_stats_req;
		message.type = WMA_LINK_LAYER_STATS_SET_REQ;
		MTRACE(qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
				 NO_SESSION, message.type));
		qdf_status = scheduler_post_message(QDF_MODULE_ID_SME,
						    QDF_MODULE_ID_WMA,
						    QDF_MODULE_ID_WMA,
						    &message);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				  "%s: not able to post WMA_LL_STATS_SET_REQ",
				  __func__);
			qdf_mem_free(set_stats_req);
			status = QDF_STATUS_E_FAILURE;
		}
		sme_release_global_lock(&mac->sme);
	} else {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			"%s: sme_acquire_global_lock error", __func__);
		qdf_mem_free(set_stats_req);
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}

QDF_STATUS sme_ll_stats_get_req(mac_handle_t mac_handle,
				tSirLLStatsGetReq *get_stats_req,
				void *context)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};
	tSirLLStatsGetReq *ll_stats_get_req;

	ll_stats_get_req = qdf_mem_malloc(sizeof(*ll_stats_get_req));
	if (!ll_stats_get_req)
		return QDF_STATUS_E_NOMEM;

	*ll_stats_get_req = *get_stats_req;

	mac->sme.ll_stats_context = context;
	if (sme_acquire_global_lock(&mac->sme) == QDF_STATUS_SUCCESS) {
		/* Serialize the req through MC thread */
		message.bodyptr = ll_stats_get_req;
		message.type = WMA_LINK_LAYER_STATS_GET_REQ;
		qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
			  NO_SESSION, message.type);
		status = scheduler_post_message(QDF_MODULE_ID_SME,
						QDF_MODULE_ID_WMA,
						QDF_MODULE_ID_WMA, &message);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("Not able to post WMA_LL_STATS_GET_REQ");
			qdf_mem_free(ll_stats_get_req);
		}
		sme_release_global_lock(&mac->sme);
	} else {
		sme_err("sme_acquire_global_lock error");
		qdf_mem_free(ll_stats_get_req);
	}

	return status;
}

QDF_STATUS sme_set_link_layer_stats_ind_cb(mac_handle_t mac_handle,
					   link_layer_stats_cb callback)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		mac->sme.link_layer_stats_cb = callback;
		sme_release_global_lock(&mac->sme);
	} else {
		sme_err("sme_acquire_global_lock error");
	}

	return status;
}

/**
 * sme_set_link_layer_ext_cb() - Register callback for link layer statistics
 * @mac_handle: Mac global handle
 * @ll_stats_ext_cb: HDD callback which needs to be invoked after getting
 *                   status notification from FW
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
sme_set_link_layer_ext_cb(mac_handle_t mac_handle,
			  void (*ll_stats_ext_cb)(hdd_handle_t callback_ctx,
						  tSirLLStatsResults *rsp))
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (status == QDF_STATUS_SUCCESS) {
		mac->sme.link_layer_stats_ext_cb = ll_stats_ext_cb;
		sme_release_global_lock(&mac->sme);
	} else
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: sme_qcquire_global_lock error", __func__);
	return status;
}

/**
 * sme_reset_link_layer_stats_ind_cb() - SME API to reset link layer stats
 *					 indication
 * @mac_handle: Opaque handle to the global MAC context
 *
 * This function reset's the link layer stats indication
 *
 * Return: QDF_STATUS Enumeration
 */

QDF_STATUS sme_reset_link_layer_stats_ind_cb(mac_handle_t mac_handle)
{
	QDF_STATUS status;
	struct mac_context *pmac;

	if (!mac_handle) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				  FL("mac_handle is not valid"));
		return QDF_STATUS_E_INVAL;
	}
	pmac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&pmac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		pmac->sme.link_layer_stats_cb = NULL;
		sme_release_global_lock(&pmac->sme);
	} else
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			"%s: sme_acquire_global_lock error", __func__);

	return status;
}

/**
 * sme_ll_stats_set_thresh - set threshold for mac counters
 * @mac_handle: Opaque handle to the global MAC context
 * @threshold, threshold for mac counters
 *
 * Return: QDF_STATUS Enumeration
 */
QDF_STATUS sme_ll_stats_set_thresh(mac_handle_t mac_handle,
				   struct sir_ll_ext_stats_threshold *threshold)
{
	QDF_STATUS status;
	struct mac_context *mac;
	struct scheduler_msg message = {0};
	struct sir_ll_ext_stats_threshold *thresh;

	if (!threshold) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("threshold is not valid"));
		return QDF_STATUS_E_INVAL;
	}

	if (!mac_handle) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("mac_handle is not valid"));
		return QDF_STATUS_E_INVAL;
	}
	mac = MAC_CONTEXT(mac_handle);

	thresh = qdf_mem_malloc(sizeof(*thresh));
	if (!thresh)
		return QDF_STATUS_E_NOMEM;

	*thresh = *threshold;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		/* Serialize the req through MC thread */
		message.bodyptr = thresh;
		message.type    = WDA_LINK_LAYER_STATS_SET_THRESHOLD;
		MTRACE(qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
				 NO_SESSION, message.type));
		status = scheduler_post_message(QDF_MODULE_ID_SME,
						QDF_MODULE_ID_WMA,
						QDF_MODULE_ID_WMA, &message);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				  "%s: not able to post WDA_LL_STATS_GET_REQ",
				  __func__);
			qdf_mem_free(thresh);
		}
		sme_release_global_lock(&mac->sme);
	} else {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("sme_acquire_global_lock error"));
		qdf_mem_free(thresh);
	}
	return status;
}

#endif /* WLAN_FEATURE_LINK_LAYER_STATS */

#ifdef WLAN_POWER_DEBUG
void sme_reset_power_debug_stats_cb(mac_handle_t mac_handle)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		mac_ctx->sme.power_debug_stats_context = NULL;
		mac_ctx->sme.power_stats_resp_callback = NULL;
		sme_release_global_lock(&mac_ctx->sme);
	}
}

QDF_STATUS sme_power_debug_stats_req(
		mac_handle_t mac_handle,
		void (*callback_fn)(struct power_stats_response *response,
				    void *context),
		void *power_stats_context)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct scheduler_msg msg = {0};

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (!callback_fn) {
			sme_err("Indication callback did not registered");
			sme_release_global_lock(&mac_ctx->sme);
			return QDF_STATUS_E_FAILURE;
		}

		if (mac_ctx->sme.power_debug_stats_context ||
		    mac_ctx->sme.power_stats_resp_callback) {
			sme_err("Already one power stats req in progress");
			sme_release_global_lock(&mac_ctx->sme);
			return QDF_STATUS_E_ALREADY;
		}
		mac_ctx->sme.power_debug_stats_context = power_stats_context;
		mac_ctx->sme.power_stats_resp_callback = callback_fn;
		msg.bodyptr = NULL;
		msg.type = WMA_POWER_DEBUG_STATS_REQ;
		status = scheduler_post_message(QDF_MODULE_ID_SME,
						QDF_MODULE_ID_WMA,
						QDF_MODULE_ID_WMA, &msg);
		if (!QDF_IS_STATUS_SUCCESS(status))
			sme_err("not able to post WDA_POWER_DEBUG_STATS_REQ");
		sme_release_global_lock(&mac_ctx->sme);
	}
	return status;
}
#endif

#ifdef WLAN_FEATURE_BEACON_RECEPTION_STATS
QDF_STATUS sme_beacon_debug_stats_req(
		mac_handle_t mac_handle, uint32_t vdev_id,
		void (*callback_fn)(struct bcn_reception_stats_rsp
				    *response, void *context),
		void *beacon_stats_context)
{
	QDF_STATUS status;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	uint32_t *val;
	struct scheduler_msg msg = {0};

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (!callback_fn) {
			sme_err("Indication callback did not registered");
			sme_release_global_lock(&mac_ctx->sme);
			return QDF_STATUS_E_FAILURE;
		}

		if (!mac_ctx->bcn_reception_stats &&
		    !mac_ctx->mlme_cfg->gen.enable_beacon_reception_stats) {
			sme_err("Beacon Reception stats not supported");
			sme_release_global_lock(&mac_ctx->sme);
			return QDF_STATUS_E_NOSUPPORT;
		}

		val = qdf_mem_malloc(sizeof(*val));
		if (!val) {
			sme_release_global_lock(&mac_ctx->sme);
			return QDF_STATUS_E_NOMEM;
		}

		*val = vdev_id;
		mac_ctx->sme.beacon_stats_context = beacon_stats_context;
		mac_ctx->sme.beacon_stats_resp_callback = callback_fn;
		msg.bodyptr = val;
		msg.type = WMA_BEACON_DEBUG_STATS_REQ;
		status = scheduler_post_message(QDF_MODULE_ID_SME,
						QDF_MODULE_ID_WMA,
						QDF_MODULE_ID_WMA, &msg);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("not able to post WMA_BEACON_DEBUG_STATS_REQ");
			qdf_mem_free(val);
		}
		sme_release_global_lock(&mac_ctx->sme);
	}
	return status;
}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * sme_update_roam_key_mgmt_offload_enabled() - enable/disable key mgmt offload
 * This is a synchronous call
 * @mac_handle: The handle returned by mac_open.
 * @session_id: Session Identifier
 * @pmkid_modes: PMKID modes of PMKSA caching and OKC
 * Return: QDF_STATUS_SUCCESS - SME updated config successfully.
 * Other status means SME is failed to update.
 */

QDF_STATUS sme_update_roam_key_mgmt_offload_enabled(mac_handle_t mac_handle,
					uint8_t session_id,
					struct pmkid_mode_bits *pmkid_modes)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
			status = csr_roam_set_key_mgmt_offload(mac_ctx,
						session_id,
						pmkid_modes);
		} else
			status = QDF_STATUS_E_INVAL;
		sme_release_global_lock(&mac_ctx->sme);
	}

	return status;
}
#endif

/**
 * sme_get_temperature() - SME API to get the pdev temperature
 * @mac_handle: Handle to global MAC context
 * @cb_context: temperature callback context
 * @cb: callback function with response (temperature)
 * Return: QDF_STATUS
 */
QDF_STATUS sme_get_temperature(mac_handle_t mac_handle,
			       void *cb_context,
			       void (*cb)(int temperature,
					  void *context))
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_STATUS_SUCCESS == status) {
		if ((!cb) &&
		    (!mac->sme.temperature_cb)) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				"Indication Call back did not registered");
			sme_release_global_lock(&mac->sme);
			return QDF_STATUS_E_FAILURE;
		} else if (cb) {
			mac->sme.temperature_cb_context = cb_context;
			mac->sme.temperature_cb = cb;
		}
		/* serialize the req through MC thread */
		message.bodyptr = NULL;
		message.type = WMA_GET_TEMPERATURE_REQ;
		qdf_status = scheduler_post_message(QDF_MODULE_ID_SME,
						    QDF_MODULE_ID_WMA,
						    QDF_MODULE_ID_WMA,
						    &message);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				  FL("Post Get Temperature msg fail"));
			status = QDF_STATUS_E_FAILURE;
		}
		sme_release_global_lock(&mac->sme);
	}
	return status;
}

QDF_STATUS sme_set_scanning_mac_oui(mac_handle_t mac_handle,
				    struct scan_mac_oui *scan_mac_oui)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};
	struct scan_mac_oui *bodyptr;

	/* per contract must make a copy of the params when messaging */
	bodyptr = qdf_mem_malloc(sizeof(*bodyptr));
	if (!bodyptr)
		return QDF_STATUS_E_NOMEM;
	*bodyptr = *scan_mac_oui;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_STATUS_SUCCESS == status) {
		/* Serialize the req through MC thread */
		message.bodyptr = bodyptr;
		message.type = WMA_SET_SCAN_MAC_OUI_REQ;
		status = scheduler_post_message(QDF_MODULE_ID_SME,
						QDF_MODULE_ID_WMA,
						QDF_MODULE_ID_WMA,
						&message);
		sme_release_global_lock(&mac->sme);
	}

	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("failure: %d", status);
		qdf_mem_free(bodyptr);
	}

	return status;
}

#ifdef DHCP_SERVER_OFFLOAD
QDF_STATUS
sme_set_dhcp_srv_offload(mac_handle_t mac_handle,
			 struct dhcp_offload_info_params *dhcp_srv_info)
{
	struct scheduler_msg message = {0};
	struct dhcp_offload_info_params *payload;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	payload = qdf_mem_malloc(sizeof(*payload));
	if (!payload)
		return QDF_STATUS_E_NOMEM;

	*payload = *dhcp_srv_info;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_STATUS_SUCCESS == status) {
		/* serialize the req through MC thread */
		message.type = WMA_SET_DHCP_SERVER_OFFLOAD_CMD;
		message.bodyptr = payload;

		if (!QDF_IS_STATUS_SUCCESS
			    (scheduler_post_message(QDF_MODULE_ID_SME,
						    QDF_MODULE_ID_WMA,
						    QDF_MODULE_ID_WMA,
						    &message))) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				  "%s:WMA_SET_DHCP_SERVER_OFFLOAD_CMD failed",
				  __func__);
			qdf_mem_free(payload);
			status = QDF_STATUS_E_FAILURE;
		}
		sme_release_global_lock(&mac->sme);
	} else {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: sme_acquire_global_lock error!", __func__);
		qdf_mem_free(payload);
	}

	return status;
}
#endif /* DHCP_SERVER_OFFLOAD */

QDF_STATUS sme_send_unit_test_cmd(uint32_t vdev_id, uint32_t module_id,
				  uint32_t arg_count, uint32_t *arg)
{
	return wma_form_unit_test_cmd_and_send(vdev_id, module_id,
					       arg_count, arg);
}

#ifdef WLAN_FEATURE_GPIO_LED_FLASHING
/*
 * sme_set_led_flashing() -
 * API to set the Led flashing parameters.
 *
 * mac_handle - The handle returned by mac_open.
 * x0, x1 -  led flashing parameters
 * Return QDF_STATUS
 */
QDF_STATUS sme_set_led_flashing(mac_handle_t mac_handle, uint8_t type,
				uint32_t x0, uint32_t x1)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};
	struct flashing_req_params *ledflashing;

	ledflashing = qdf_mem_malloc(sizeof(*ledflashing));
	if (!ledflashing)
		return QDF_STATUS_E_NOMEM;

	ledflashing->req_id = 0;
	ledflashing->pattern_id = type;
	ledflashing->led_x0 = x0;
	ledflashing->led_x1 = x1;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		/* Serialize the req through MC thread */
		message.bodyptr = ledflashing;
		message.type = WMA_LED_FLASHING_REQ;
		status = scheduler_post_message(QDF_MODULE_ID_SME,
						QDF_MODULE_ID_WMA,
						QDF_MODULE_ID_WMA, &message);
		sme_release_global_lock(&mac->sme);
	}
	if (!QDF_IS_STATUS_SUCCESS(status))
		qdf_mem_free(ledflashing);

	return status;
}
#endif

/**
 *  sme_enable_dfS_chan_scan() - set DFS channel scan enable/disable
 *  @mac_handle:         corestack handler
 *  @dfs_flag:      flag indicating dfs channel enable/disable
 *  Return:         QDF_STATUS
 */
QDF_STATUS sme_enable_dfs_chan_scan(mac_handle_t mac_handle, uint8_t dfs_flag)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac;

	if (!mac_handle) {
		sme_err("mac_handle is NULL");
		return QDF_STATUS_E_INVAL;
	}

	mac = MAC_CONTEXT(mac_handle);

	mac->scan.fEnableDFSChnlScan = dfs_flag;

	return status;
}

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
/**
 * sme_validate_sap_channel_switch() - validate target channel switch w.r.t
 *      concurreny rules set to avoid channel interference.
 * @mac_handle: Opaque handle to the global MAC context
 * @sap_ch - channel to switch
 * @sap_phy_mode - phy mode of SAP
 * @cc_switch_mode - concurreny switch mode
 * @session_id - sme session id.
 *
 * Return: true if there is no channel interference else return false
 */
bool sme_validate_sap_channel_switch(mac_handle_t mac_handle,
				     uint32_t sap_ch_freq,
				     eCsrPhyMode sap_phy_mode,
				     uint8_t cc_switch_mode,
				     uint8_t session_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session = CSR_GET_SESSION(mac, session_id);
	uint16_t intf_channel_freq = 0;

	if (!session)
		return false;

	session->ch_switch_in_progress = true;
	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		intf_channel_freq = csr_check_concurrent_channel_overlap(
			mac, sap_ch_freq, sap_phy_mode, cc_switch_mode);
		sme_release_global_lock(&mac->sme);
	} else {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				FL("sme_acquire_global_lock error!"));
		session->ch_switch_in_progress = false;
		return false;
	}

	session->ch_switch_in_progress = false;
	return (intf_channel_freq == 0) ? true : false;
}
#endif

/**
 * sme_configure_stats_avg_factor() - function to config avg. stats factor
 * @mac_handle: Opaque handle to the global MAC context
 * @session_id: session ID
 * @stats_avg_factor: average stats factor
 *
 * This function configures the stats avg factor in firmware
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_configure_stats_avg_factor(mac_handle_t mac_handle,
					  uint8_t session_id,
					  uint16_t stats_avg_factor)
{
	struct scheduler_msg msg = {0};
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac  = MAC_CONTEXT(mac_handle);
	struct sir_stats_avg_factor *stats_factor;

	stats_factor = qdf_mem_malloc(sizeof(*stats_factor));
	if (!stats_factor)
		return QDF_STATUS_E_NOMEM;

	status = sme_acquire_global_lock(&mac->sme);

	if (QDF_STATUS_SUCCESS == status) {

		stats_factor->vdev_id = session_id;
		stats_factor->stats_avg_factor = stats_avg_factor;

		/* serialize the req through MC thread */
		msg.type     = SIR_HAL_CONFIG_STATS_FACTOR;
		msg.bodyptr  = stats_factor;

		if (!QDF_IS_STATUS_SUCCESS(
			    scheduler_post_message(QDF_MODULE_ID_SME,
						   QDF_MODULE_ID_WMA,
						   QDF_MODULE_ID_WMA, &msg))) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				  "%s: Not able to post SIR_HAL_CONFIG_STATS_FACTOR to WMA!",
				  __func__);
			qdf_mem_free(stats_factor);
			status = QDF_STATUS_E_FAILURE;
		}
		sme_release_global_lock(&mac->sme);
	} else {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: sme_acquire_global_lock error!",
			  __func__);
		qdf_mem_free(stats_factor);
	}

	return status;
}

/**
 * sme_configure_guard_time() - function to configure guard time
 * @mac_handle: Opaque handle to the global MAC context
 * @session_id: session id
 * @guard_time: guard time
 *
 * This function configures the guard time in firmware
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_configure_guard_time(mac_handle_t mac_handle, uint8_t session_id,
				    uint32_t guard_time)
{
	struct scheduler_msg msg = {0};
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac  = MAC_CONTEXT(mac_handle);
	struct sir_guard_time_request *g_time;

	g_time = qdf_mem_malloc(sizeof(*g_time));
	if (!g_time)
		return QDF_STATUS_E_NOMEM;

	status = sme_acquire_global_lock(&mac->sme);

	if (QDF_STATUS_SUCCESS == status) {

		g_time->vdev_id = session_id;
		g_time->guard_time = guard_time;

		/* serialize the req through MC thread */
		msg.type     = SIR_HAL_CONFIG_GUARD_TIME;
		msg.bodyptr  = g_time;

		if (!QDF_IS_STATUS_SUCCESS(
			    scheduler_post_message(QDF_MODULE_ID_SME,
						   QDF_MODULE_ID_WMA,
						   QDF_MODULE_ID_WMA, &msg))) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				  "%s: Not able to post SIR_HAL_CONFIG_GUARD_TIME to WMA!",
				  __func__);
			qdf_mem_free(g_time);
			status = QDF_STATUS_E_FAILURE;
		}
		sme_release_global_lock(&mac->sme);
	} else {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: sme_acquire_global_lock error!",
			  __func__);
		qdf_mem_free(g_time);
	}

	return status;
}

/*
 * sme_wifi_start_logger() - Send the start/stop logging command to WMA
 * to either start/stop logging
 * @mac_handle: Opaque handle to the global MAC context
 * @start_log: Structure containing the wifi start logger params
 *
 * This function sends the start/stop logging command to WMA
 *
 * Return: QDF_STATUS_SUCCESS on successful posting
 */
QDF_STATUS sme_wifi_start_logger(mac_handle_t mac_handle,
				 struct sir_wifi_start_log start_log)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct scheduler_msg message = {0};
	struct sir_wifi_start_log *req_msg;
	uint32_t len;

	len = sizeof(*req_msg);
	req_msg = qdf_mem_malloc(len);
	if (!req_msg)
		return QDF_STATUS_E_NOMEM;

	req_msg->verbose_level = start_log.verbose_level;
	req_msg->is_iwpriv_command = start_log.is_iwpriv_command;
	req_msg->ring_id = start_log.ring_id;
	req_msg->ini_triggered = start_log.ini_triggered;
	req_msg->user_triggered = start_log.user_triggered;
	req_msg->size = start_log.size;
	req_msg->is_pktlog_buff_clear = start_log.is_pktlog_buff_clear;

	message.bodyptr = req_msg;
	message.type    = SIR_HAL_START_STOP_LOGGING;
	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &message);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("scheduler_post_msg failed!(err=%d)", status);
		qdf_mem_free(req_msg);
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}

/**
 * sme_neighbor_middle_of_roaming() - Function to know if
 * STA is in the middle of roaming states
 * @mac_handle:                Handle returned by macOpen
 * @sessionId: sessionId of the STA session
 *
 * This function is a wrapper to call
 * csr_neighbor_middle_of_roaming to know STA is in the
 * middle of roaming states
 *
 * Return: True or False
 *
 */
bool sme_neighbor_middle_of_roaming(mac_handle_t mac_handle, uint8_t sessionId)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	bool val = false;

	if (CSR_IS_SESSION_VALID(mac_ctx, sessionId))
		val = csr_neighbor_middle_of_roaming(mac_ctx, sessionId);
	else
		sme_debug("Invalid Session: %d", sessionId);

	return val;
}

bool sme_is_any_session_in_middle_of_roaming(mac_handle_t mac_handle)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	uint8_t session_id;

	for (session_id = 0; session_id < WLAN_MAX_VDEVS; session_id++) {
		if (CSR_IS_SESSION_VALID(mac_ctx, session_id) &&
		    csr_neighbor_middle_of_roaming(mac_ctx, session_id))
			return true;
	}

	return false;
}

/*
 * sme_send_flush_logs_cmd_to_fw() - Flush FW logs
 *
 * This function is used to send the command that will
 * be used to flush the logs in the firmware
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_send_flush_logs_cmd_to_fw(void)
{
	QDF_STATUS status;
	struct scheduler_msg message = {0};

	/* Serialize the req through MC thread */
	message.bodyptr = NULL;
	message.type    = SIR_HAL_FLUSH_LOG_TO_FW;
	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &message);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("scheduler_post_msg failed!(err=%d)", status);
		status = QDF_STATUS_E_FAILURE;
	}
	return status;
}

QDF_STATUS sme_enable_uapsd_for_ac(sme_ac_enum_type ac, uint8_t tid,
				   uint8_t pri, uint32_t srvc_int,
				   uint32_t sus_int,
				   enum sme_qos_wmm_dir_type dir,
				   uint8_t psb, uint32_t sessionId,
				   uint32_t delay_interval)
{
	void *wma_handle;
	t_wma_trigger_uapsd_params uapsd_params;
	enum uapsd_ac access_category;

	if (!psb) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			"No need to configure auto trigger:psb is 0");
		return QDF_STATUS_SUCCESS;
	}

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
					"wma_handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	switch (ac) {
	case SME_AC_BK:
		access_category = UAPSD_BK;
		break;
	case SME_AC_BE:
		access_category = UAPSD_BE;
		break;
	case SME_AC_VI:
		access_category = UAPSD_VI;
		break;
	case SME_AC_VO:
		access_category = UAPSD_VO;
		break;
	default:
		return QDF_STATUS_E_FAILURE;
	}

	uapsd_params.wmm_ac = access_category;
	uapsd_params.user_priority = pri;
	uapsd_params.service_interval = srvc_int;
	uapsd_params.delay_interval = delay_interval;
	uapsd_params.suspend_interval = sus_int;

	if (QDF_STATUS_SUCCESS !=
	    wma_trigger_uapsd_params(wma_handle, sessionId, &uapsd_params)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			"Failed to Trigger Uapsd params for sessionId %d",
			    sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sme_disable_uapsd_for_ac(sme_ac_enum_type ac, uint32_t sessionId)
{
	void *wma_handle;
	enum uapsd_ac access_category;

	switch (ac) {
	case SME_AC_BK:
		access_category = UAPSD_BK;
		break;
	case SME_AC_BE:
		access_category = UAPSD_BE;
		break;
	case SME_AC_VI:
		access_category = UAPSD_VI;
		break;
	case SME_AC_VO:
		access_category = UAPSD_VO;
		break;
	default:
		return QDF_STATUS_E_FAILURE;
	}

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				"wma handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	if (QDF_STATUS_SUCCESS !=
	    wma_disable_uapsd_per_ac(wma_handle, sessionId, access_category)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			"Failed to disable uapsd for ac %d for sessionId %d",
			    ac, sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * sme_update_nss() - SME API to change the number for spatial streams
 * (1 or 2)
 * @mac_handle: Handle returned by mac open
 * @nss: Number of spatial streams
 *
 * This function is used to update the number of spatial streams supported.
 *
 * Return: Success upon successfully changing nss else failure
 *
 */
QDF_STATUS sme_update_nss(mac_handle_t mac_handle, uint8_t nss)
{
	QDF_STATUS status;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	uint32_t i;
	struct mlme_ht_capabilities_info *ht_cap_info;
	struct csr_roam_session *csr_session;
	struct mlme_vht_capabilities_info *vht_cap_info;

	vht_cap_info = &mac_ctx->mlme_cfg->vht_caps.vht_cap_info;

	status = sme_acquire_global_lock(&mac_ctx->sme);

	if (QDF_STATUS_SUCCESS == status) {
		vht_cap_info->enable2x2 = (nss == 1) ? 0 : 1;

		/* get the HT capability info*/
		ht_cap_info = &mac_ctx->mlme_cfg->ht_caps.ht_cap_info;

		for (i = 0; i < WLAN_MAX_VDEVS; i++) {
			if (CSR_IS_SESSION_VALID(mac_ctx, i)) {
				csr_session = &mac_ctx->roam.roamSession[i];
				csr_session->ht_config.ht_tx_stbc =
					ht_cap_info->tx_stbc;
			}
		}

		sme_release_global_lock(&mac_ctx->sme);
	}
	return status;
}

/**
 * sme_update_user_configured_nss() - sets the nss based on user request
 * @mac_handle: Opaque handle to the global MAC context
 * @nss: number of streams
 *
 * Return: None
 */
void sme_update_user_configured_nss(mac_handle_t mac_handle, uint8_t nss)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	mac_ctx->user_configured_nss = nss;
}

int sme_update_tx_bfee_supp(mac_handle_t mac_handle, uint8_t session_id,
			    uint8_t cfg_val)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	mac_ctx->mlme_cfg->vht_caps.vht_cap_info.su_bformee = cfg_val;

	return sme_update_he_tx_bfee_supp(mac_handle, session_id, cfg_val);
}

int sme_update_tx_bfee_nsts(mac_handle_t mac_handle, uint8_t session_id,
			    uint8_t usr_cfg_val, uint8_t nsts_val)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	uint8_t nsts_set_val;
	struct mlme_vht_capabilities_info *vht_cap_info;

	vht_cap_info = &mac_ctx->mlme_cfg->vht_caps.vht_cap_info;
	mac_ctx->usr_cfg_tx_bfee_nsts = usr_cfg_val;
	if (usr_cfg_val)
		nsts_set_val = usr_cfg_val;
	else
		nsts_set_val = nsts_val;

	vht_cap_info->tx_bfee_ant_supp = nsts_set_val;

	if (usr_cfg_val)
		sme_set_he_tx_bf_cbf_rates(session_id);

	return sme_update_he_tx_bfee_nsts(mac_handle, session_id, nsts_set_val);
}
#ifdef WLAN_FEATURE_11AX
void sme_update_tgt_he_cap(mac_handle_t mac_handle,
			   struct wma_tgt_cfg *cfg,
			   tDot11fIEhe_cap *he_cap_ini)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	qdf_mem_copy(&mac_ctx->he_cap_2g,
		     &cfg->he_cap_2g,
		     sizeof(tDot11fIEhe_cap));

	qdf_mem_copy(&mac_ctx->he_cap_5g,
		     &cfg->he_cap_5g,
		     sizeof(tDot11fIEhe_cap));

	/* modify HE Caps field according to INI setting */
	mac_ctx->he_cap_2g.bfee_sts_lt_80 =
			QDF_MIN(cfg->he_cap_2g.bfee_sts_lt_80,
				he_cap_ini->bfee_sts_lt_80);

	mac_ctx->he_cap_5g.bfee_sts_lt_80 =
			QDF_MIN(cfg->he_cap_5g.bfee_sts_lt_80,
				he_cap_ini->bfee_sts_lt_80);
}

void sme_update_he_cap_nss(mac_handle_t mac_handle, uint8_t session_id,
			   uint8_t nss)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *csr_session;
	uint32_t tx_mcs_map = 0;
	uint32_t rx_mcs_map = 0;
	uint32_t mcs_map = 0;

	if (!nss || (nss > 2)) {
		sme_err("invalid Nss value %d", nss);
		return;
	}
	csr_session = CSR_GET_SESSION(mac_ctx, session_id);
	if (!csr_session) {
		sme_err("No session for id %d", session_id);
		return;
	}
	rx_mcs_map =
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.rx_he_mcs_map_lt_80;
	tx_mcs_map =
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.tx_he_mcs_map_lt_80;
	mcs_map = rx_mcs_map & 0x3;

	if (nss == 1) {
		tx_mcs_map = HE_SET_MCS_4_NSS(tx_mcs_map, HE_MCS_DISABLE, 2);
		rx_mcs_map = HE_SET_MCS_4_NSS(rx_mcs_map, HE_MCS_DISABLE, 2);
	} else {
		tx_mcs_map = HE_SET_MCS_4_NSS(tx_mcs_map, mcs_map, 2);
		rx_mcs_map = HE_SET_MCS_4_NSS(rx_mcs_map, mcs_map, 2);
	}
	sme_debug("new HE Nss MCS MAP: Rx 0x%0X, Tx: 0x%0X",
		  rx_mcs_map, tx_mcs_map);
	if (cfg_in_range(CFG_HE_RX_MCS_MAP_LT_80, rx_mcs_map))
		mac_ctx->mlme_cfg->he_caps.dot11_he_cap.rx_he_mcs_map_lt_80 =
		rx_mcs_map;
	if (cfg_in_range(CFG_HE_TX_MCS_MAP_LT_80, tx_mcs_map))
		mac_ctx->mlme_cfg->he_caps.dot11_he_cap.tx_he_mcs_map_lt_80 =
		tx_mcs_map;
	csr_update_session_he_cap(mac_ctx, csr_session);

}

int sme_update_he_mcs(mac_handle_t mac_handle, uint8_t session_id,
		      uint16_t he_mcs)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *csr_session;
	uint16_t mcs_val = 0;
	uint16_t mcs_map = HE_MCS_ALL_DISABLED;

	csr_session = CSR_GET_SESSION(mac_ctx, session_id);
	if (!csr_session) {
		sme_err("No session for id %d", session_id);
		return -EINVAL;
	}
	if ((he_mcs & 0x3) == HE_MCS_DISABLE) {
		sme_err("Invalid HE MCS 0x%0x, can't disable 0-7 for 1ss",
			he_mcs);
		return -EINVAL;
	}
	mcs_val = he_mcs & 0x3;
	switch (he_mcs) {
	case HE_80_MCS0_7:
	case HE_80_MCS0_9:
	case HE_80_MCS0_11:
		if (mac_ctx->mlme_cfg->vht_caps.vht_cap_info.enable2x2) {
			mcs_map = HE_SET_MCS_4_NSS(mcs_map, mcs_val, 1);
			mcs_map = HE_SET_MCS_4_NSS(mcs_map, mcs_val, 2);
		} else {
			mcs_map = HE_SET_MCS_4_NSS(mcs_map, mcs_val, 1);
		}
		if (cfg_in_range(CFG_HE_TX_MCS_MAP_LT_80, mcs_map))
			mac_ctx->mlme_cfg->he_caps.dot11_he_cap.
			tx_he_mcs_map_lt_80 = mcs_map;
		if (cfg_in_range(CFG_HE_RX_MCS_MAP_LT_80, mcs_map))
			mac_ctx->mlme_cfg->he_caps.dot11_he_cap.
			rx_he_mcs_map_lt_80 = mcs_map;
		break;

	case HE_160_MCS0_7:
	case HE_160_MCS0_9:
	case HE_160_MCS0_11:
		mcs_map = HE_SET_MCS_4_NSS(mcs_map, mcs_val, 1);
		if (cfg_in_range(CFG_HE_TX_MCS_MAP_160, mcs_map))
			qdf_mem_copy(mac_ctx->mlme_cfg->he_caps.dot11_he_cap.
				     tx_he_mcs_map_160, &mcs_map,
				     sizeof(uint16_t));
		if (cfg_in_range(CFG_HE_RX_MCS_MAP_160, mcs_map))
			qdf_mem_copy(mac_ctx->mlme_cfg->he_caps.dot11_he_cap.
				     rx_he_mcs_map_160, &mcs_map,
				     sizeof(uint16_t));
		break;

	case HE_80p80_MCS0_7:
	case HE_80p80_MCS0_9:
	case HE_80p80_MCS0_11:
		mcs_map = HE_SET_MCS_4_NSS(mcs_map, mcs_val, 1);
		if (cfg_in_range(CFG_HE_TX_MCS_MAP_80_80, mcs_map))
			qdf_mem_copy(mac_ctx->mlme_cfg->he_caps.dot11_he_cap.
				     tx_he_mcs_map_80_80, &mcs_map,
				     sizeof(uint16_t));
		if (cfg_in_range(CFG_HE_RX_MCS_MAP_80_80, mcs_map))
			qdf_mem_copy(mac_ctx->mlme_cfg->he_caps.dot11_he_cap.
				     rx_he_mcs_map_80_80, &mcs_map,
				     sizeof(uint16_t));
		break;

	default:
		sme_err("Invalid HE MCS 0x%0x", he_mcs);
		return -EINVAL;
	}
	sme_debug("new HE MCS 0x%0x", mcs_map);
	csr_update_session_he_cap(mac_ctx, csr_session);

	return 0;
}

void sme_set_usr_cfg_mu_edca(mac_handle_t mac_handle, bool val)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	mac_ctx->usr_cfg_mu_edca_params = val;
}

int sme_update_mu_edca_params(mac_handle_t mac_handle, uint8_t session_id)
{
	struct scheduler_msg msg = {0};
	QDF_STATUS status;

	qdf_mem_zero(&msg, sizeof(msg));
	msg.type = WNI_SME_UPDATE_MU_EDCA_PARAMS;
	msg.reserved = 0;
	msg.bodyval = session_id;
	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE, &msg);
	if (status != QDF_STATUS_SUCCESS) {
		sme_err("Not able to post update edca profile");
		return -EIO;
	}

	return 0;
}

void sme_set_he_mu_edca_def_cfg(mac_handle_t mac_handle)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	uint8_t i;

	sme_debug("Set MU EDCA params to default");
	for (i = 0; i < QCA_WLAN_AC_ALL; i++) {
		mac_ctx->usr_mu_edca_params[i].aci.aifsn = MU_EDCA_DEF_AIFSN;
		mac_ctx->usr_mu_edca_params[i].aci.aci = i;
		mac_ctx->usr_mu_edca_params[i].cw.max = MU_EDCA_DEF_CW_MAX;
		mac_ctx->usr_mu_edca_params[i].cw.min = MU_EDCA_DEF_CW_MIN;
		mac_ctx->usr_mu_edca_params[i].mu_edca_timer =
							MU_EDCA_DEF_TIMER;
	}
}

int sme_update_he_tx_bfee_supp(mac_handle_t mac_handle, uint8_t session_id,
			       uint8_t cfg_val)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session;

	session = CSR_GET_SESSION(mac_ctx, session_id);

	if (!session) {
		sme_err("No session for id %d", session_id);
		return -EINVAL;
	}
	if (cfg_val <= 1)
		mac_ctx->mlme_cfg->he_caps.dot11_he_cap.su_beamformee = cfg_val;
	else
		return -EINVAL;

	csr_update_session_he_cap(mac_ctx, session);
	return 0;
}

int sme_update_he_trigger_frm_mac_pad(mac_handle_t mac_handle,
				      uint8_t session_id,
				      uint8_t cfg_val)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session;

	session = CSR_GET_SESSION(mac_ctx, session_id);

	if (!session) {
		sme_err("No session for id %d", session_id);
		return -EINVAL;
	}
	if (cfg_in_range(CFG_HE_TRIG_PAD, cfg_val))
		mac_ctx->mlme_cfg->he_caps.dot11_he_cap.trigger_frm_mac_pad =
		cfg_val;
	else
		return -EINVAL;

	csr_update_session_he_cap(mac_ctx, session);
	return 0;

}

int sme_update_he_om_ctrl_supp(mac_handle_t mac_handle, uint8_t session_id,
			       uint8_t cfg_val)
{

	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session;

	session = CSR_GET_SESSION(mac_ctx, session_id);

	if (!session) {
		sme_err("No session for id %d", session_id);
		return -EINVAL;
	}
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.omi_a_ctrl = cfg_val;

	csr_update_session_he_cap(mac_ctx, session);
	return 0;
}

int sme_update_he_htc_he_supp(mac_handle_t mac_handle, uint8_t session_id,
			      bool cfg_val)
{

	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session;

	session = CSR_GET_SESSION(mac_ctx, session_id);

	if (!session) {
		sme_err("No session for id %d", session_id);
		return -EINVAL;
	}

	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.htc_he = cfg_val;
	csr_update_session_he_cap(mac_ctx, session);

	return 0;
}

static QDF_STATUS
sme_validate_session_for_cap_update(struct mac_context *mac_ctx,
				    uint8_t session_id,
				    struct csr_roam_session *session)
{
	if (!session) {
		sme_err("Session does not exist, Session_id: %d", session_id);
		return QDF_STATUS_E_INVAL;
	}
	if (!csr_is_conn_state_connected_infra(mac_ctx, session_id)) {
		sme_debug("STA is not connected, Session_id: %d", session_id);
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

int sme_send_he_om_ctrl_update(mac_handle_t mac_handle, uint8_t session_id)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct omi_ctrl_tx omi_data = {0};
	void *wma_handle;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);
	uint32_t param_val = 0;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		sme_err("wma handle is NULL");
		return -EIO;
	}

	status = sme_validate_session_for_cap_update(mac_ctx, session_id,
						     session);
	if (QDF_IS_STATUS_ERROR(status))
		return -EINVAL;

	omi_data.a_ctrl_id = A_CTRL_ID_OMI;

	if (mac_ctx->he_om_ctrl_cfg_nss_set)
		omi_data.rx_nss = mac_ctx->he_om_ctrl_cfg_nss;
	else
		omi_data.rx_nss = session->nss - 1;

	if (mac_ctx->he_om_ctrl_cfg_tx_nsts_set)
		omi_data.tx_nsts = mac_ctx->he_om_ctrl_cfg_tx_nsts;
	else
		omi_data.tx_nsts = session->nss - 1;

	if (mac_ctx->he_om_ctrl_cfg_bw_set)
		omi_data.ch_bw = mac_ctx->he_om_ctrl_cfg_bw;
	else
		omi_data.ch_bw = session->connectedProfile.vht_channel_width;

	omi_data.ul_mu_dis = mac_ctx->he_om_ctrl_cfg_ul_mu_dis;
	omi_data.ul_mu_data_dis = mac_ctx->he_om_ctrl_ul_mu_data_dis;
	omi_data.omi_in_vht = 0x1;
	omi_data.omi_in_he = 0x1;

	sme_debug("OMI: BW %d TxNSTS %d RxNSS %d ULMU %d, OMI_VHT %d, OMI_HE %d",
		  omi_data.ch_bw, omi_data.tx_nsts, omi_data.rx_nss,
		  omi_data.ul_mu_dis, omi_data.omi_in_vht, omi_data.omi_in_he);
	qdf_mem_copy(&param_val, &omi_data, sizeof(omi_data));
	sme_debug("param val %08X, bssid:"QDF_MAC_ADDR_FMT, param_val,
		  QDF_MAC_ADDR_REF(session->connectedProfile.bssid.bytes));
	status = wma_set_peer_param(wma_handle,
				    session->connectedProfile.bssid.bytes,
				    WMI_PEER_PARAM_XMIT_OMI,
				    param_val, session_id);
	if (QDF_STATUS_SUCCESS != status) {
		sme_err("set_peer_param_cmd returned %d", status);
		return -EIO;
	}

	return 0;
}

int sme_set_he_om_ctrl_param(mac_handle_t mac_handle, uint8_t session_id,
			     enum qca_wlan_vendor_attr_he_omi_tx param,
			     uint8_t cfg_val)
{
	QDF_STATUS status;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);

	status = sme_validate_session_for_cap_update(mac_ctx, session_id,
						     session);
	if (QDF_IS_STATUS_ERROR(status))
		return -EINVAL;

	switch(param) {
		case QCA_WLAN_VENDOR_ATTR_HE_OMI_ULMU_DISABLE:
			sme_debug("Set OM ctrl UL MU dis to %d", cfg_val);
			mac_ctx->he_om_ctrl_cfg_ul_mu_dis = cfg_val;
			break;
		case QCA_WLAN_VENDOR_ATTR_HE_OMI_RX_NSS:
			if ((cfg_val + 1)  > session->nss) {
				sme_debug("OMI Nss %d is > connected Nss %d",
					  cfg_val, session->nss);
				mac_ctx->he_om_ctrl_cfg_nss_set = false;
				return 0;
			}
			sme_debug("Set OM ctrl Rx Nss cfg to %d", cfg_val);
			mac_ctx->he_om_ctrl_cfg_nss_set = true;
			mac_ctx->he_om_ctrl_cfg_nss = cfg_val;
			break;
		case QCA_WLAN_VENDOR_ATTR_HE_OMI_CH_BW:
			if (cfg_val >
			    session->connectedProfile.vht_channel_width) {
				sme_debug
				  ("OMI BW %d is > connected BW %d",
				   cfg_val,
				   session->connectedProfile.vht_channel_width);
				mac_ctx->he_om_ctrl_cfg_bw_set = false;
				return 0;
			}
			sme_debug("Set OM ctrl BW cfg to %d", cfg_val);
			mac_ctx->he_om_ctrl_cfg_bw_set = true;
			mac_ctx->he_om_ctrl_cfg_bw = cfg_val;
			break;
		case QCA_WLAN_VENDOR_ATTR_HE_OMI_TX_NSTS:
			if ((cfg_val + 1) > session->nss) {
				sme_debug("OMI NSTS %d is > connected Nss %d",
					  cfg_val, session->nss);
				mac_ctx->he_om_ctrl_cfg_tx_nsts_set = false;
				return 0;
			}
			sme_debug("Set OM ctrl tx nsts cfg to %d", cfg_val);
			mac_ctx->he_om_ctrl_cfg_tx_nsts_set = true;
			mac_ctx->he_om_ctrl_cfg_tx_nsts = cfg_val;
			break;
		case QCA_WLAN_VENDOR_ATTR_HE_OMI_ULMU_DATA_DISABLE:
			sme_debug("Set OM ctrl UL MU data dis to %d", cfg_val);
			mac_ctx->he_om_ctrl_ul_mu_data_dis = cfg_val;
			break;
		default:
			sme_debug("Invalid OMI param %d", param);
			return -EINVAL;
	}

	return 0;
}

void sme_reset_he_om_ctrl(mac_handle_t mac_handle)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	mac_ctx->he_om_ctrl_cfg_bw_set = false;
	mac_ctx->he_om_ctrl_cfg_nss_set = false;
	mac_ctx->he_om_ctrl_cfg_bw = 0;
	mac_ctx->he_om_ctrl_cfg_nss = 0;
	mac_ctx->he_om_ctrl_cfg_ul_mu_dis = false;
	mac_ctx->he_om_ctrl_cfg_tx_nsts_set = false;
	mac_ctx->he_om_ctrl_cfg_tx_nsts = 0;
	mac_ctx->he_om_ctrl_ul_mu_data_dis = false;
}

int sme_config_action_tx_in_tb_ppdu(mac_handle_t mac_handle, uint8_t session_id,
				    uint8_t cfg_val)
{
	QDF_STATUS status;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct scheduler_msg msg = {0};
	struct sir_cfg_action_frm_tb_ppdu *cfg_msg;

	if (!csr_is_conn_state_connected_infra(mac_ctx, session_id)) {
		sme_debug("STA not in connected state Session_id: %d",
			  session_id);
		return -EINVAL;
	}

	cfg_msg = qdf_mem_malloc(sizeof(*cfg_msg));
	if (!cfg_msg)
		return -EIO;

	cfg_msg->type = WNI_SME_CFG_ACTION_FRM_HE_TB_PPDU;
	cfg_msg->vdev_id = session_id;
	cfg_msg->cfg = cfg_val;

	msg.bodyptr = cfg_msg;
	msg.type = WNI_SME_CFG_ACTION_FRM_HE_TB_PPDU;
	status = scheduler_post_message(QDF_MODULE_ID_SME, QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE, &msg);
	if (QDF_STATUS_SUCCESS != status) {
		sme_err("Failed to send CFG_ACTION_FRAME_IN_TB_PPDU to PE %d",
			status);
		qdf_mem_free(cfg_msg);
		return -EIO;
	}

	return 0;
}

int sme_update_he_tx_bfee_nsts(mac_handle_t mac_handle, uint8_t session_id,
			       uint8_t cfg_val)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session;

	session = CSR_GET_SESSION(mac_ctx, session_id);

	if (!session) {
		sme_err("No session for id %d", session_id);
		return -EINVAL;
	}
	if (cfg_in_range(CFG_HE_BFEE_STS_LT80, cfg_val)) {
		mac_ctx->mlme_cfg->he_caps.dot11_he_cap.bfee_sts_lt_80 =
		cfg_val;
		mac_ctx->mlme_cfg->he_caps.dot11_he_cap.bfee_sts_gt_80 =
		cfg_val;
	} else {
		return -EINVAL;
	}


	csr_update_session_he_cap(mac_ctx, session);
	return 0;
}

void sme_set_he_tx_bf_cbf_rates(uint8_t session_id)
{
	uint32_t tx_bf_cbf_rates_5g[] = {91, 1, 0, 3, 2, 4, 0};
	uint32_t tx_bf_cbf_rates_2g[] = {91, 1, 1, 3, 1, 3, 0};
	QDF_STATUS status;

	status = wma_form_unit_test_cmd_and_send(session_id, 0x48, 7,
						 tx_bf_cbf_rates_5g);
	if (QDF_STATUS_SUCCESS != status)
		sme_err("send_unit_test_cmd returned %d", status);

	status = wma_form_unit_test_cmd_and_send(session_id, 0x48, 7,
						 tx_bf_cbf_rates_2g);
	if (QDF_STATUS_SUCCESS != status)
		sme_err("send_unit_test_cmd returned %d", status);
}

void sme_config_su_ppdu_queue(uint8_t session_id, bool enable)
{
	uint32_t su_ppdu_enable[] = {69, 1, 1, 1};
	uint32_t su_ppdu_disable[] = {69, 1, 1, 0};
	QDF_STATUS status;

	if (enable) {
		sme_debug("Send Tx SU PPDU queue ENABLE cmd to FW");
		status = wma_form_unit_test_cmd_and_send(session_id, 0x48, 4,
							 su_ppdu_enable);
	} else {
		sme_debug("Send Tx SU PPDU queue DISABLE cmd to FW");
		status = wma_form_unit_test_cmd_and_send(session_id, 0x48, 4,
							 su_ppdu_disable);
	}
	if (QDF_STATUS_SUCCESS != status)
		sme_err("send_unit_test_cmd returned %d", status);
}

int sme_update_he_tx_stbc_cap(mac_handle_t mac_handle, uint8_t session_id,
			      int value)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session;
	uint32_t he_cap_val = 0;

	he_cap_val = value ? 1 : 0;
	session = CSR_GET_SESSION(mac_ctx, session_id);

	if (!session) {
		sme_err("No session for id %d", session_id);
		return -EINVAL;
	}
	if (he_cap_val <= 1)
		mac_ctx->mlme_cfg->he_caps.dot11_he_cap.tb_ppdu_tx_stbc_lt_80mhz
			= he_cap_val;
	else
		return -EINVAL;
	if (he_cap_val <= 1)
		mac_ctx->mlme_cfg->he_caps.dot11_he_cap.tb_ppdu_tx_stbc_gt_80mhz
			= he_cap_val;
	else
		return -EINVAL;
	csr_update_session_he_cap(mac_ctx, session);
	return 0;
}

int sme_update_he_rx_stbc_cap(mac_handle_t mac_handle, uint8_t session_id,
			      int value)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session;
	uint32_t he_cap_val = 0;

	he_cap_val = value ? 1 : 0;
	session = CSR_GET_SESSION(mac_ctx, session_id);

	if (!session) {
		sme_err("No session for id %d", session_id);
		return -EINVAL;
	}
	if (he_cap_val <= 1)
		mac_ctx->mlme_cfg->he_caps.dot11_he_cap.rx_stbc_lt_80mhz =
		he_cap_val;
	else
		return -EINVAL;
	if (he_cap_val <= 1)
		mac_ctx->mlme_cfg->he_caps.dot11_he_cap.rx_stbc_gt_80mhz =
		he_cap_val;
	else
		return -EINVAL;
	csr_update_session_he_cap(mac_ctx, session);
	return 0;
}

int sme_update_he_frag_supp(mac_handle_t mac_handle, uint8_t session_id,
			    uint16_t he_frag)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session;

	session = CSR_GET_SESSION(mac_ctx, session_id);

	if (!session) {
		sme_err("No session for id %d", session_id);
		return -EINVAL;
	}
	if (cfg_in_range(CFG_HE_FRAGMENTATION, he_frag))
		mac_ctx->mlme_cfg->he_caps.dot11_he_cap.fragmentation = he_frag;
	else
		return -EINVAL;

	csr_update_session_he_cap(mac_ctx, session);
	return 0;

}

int sme_update_he_ldpc_supp(mac_handle_t mac_handle, uint8_t session_id,
			    uint16_t he_ldpc)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session;

	session = CSR_GET_SESSION(mac_ctx, session_id);

	if (!session) {
		sme_err("No session for id %d", session_id);
		return -EINVAL;
	}
	if (he_ldpc <= 1)
		mac_ctx->mlme_cfg->he_caps.dot11_he_cap.ldpc_coding = he_ldpc;
	else
		return -EINVAL;

	csr_update_session_he_cap(mac_ctx, session);
	return 0;

}

int sme_update_he_twt_req_support(mac_handle_t mac_handle, uint8_t session_id,
				  uint8_t cfg_val)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session;

	session = CSR_GET_SESSION(mac_ctx, session_id);

	if (!session) {
		sme_err("No session for id %d", session_id);
		return -EINVAL;
	}
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.twt_request = cfg_val;

	csr_update_session_he_cap(mac_ctx, session);

	return 0;
}
#endif

QDF_STATUS
sme_update_session_txq_edca_params(mac_handle_t mac_handle,
				   uint8_t session_id,
				   tSirMacEdcaParamRecord *txq_edca_params)
{
	QDF_STATUS status;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct sir_update_session_txq_edca_param *msg;

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_E_AGAIN;

	msg = qdf_mem_malloc(sizeof(*msg));
	if (!msg)
		return QDF_STATUS_E_NOMEM;

	msg->message_type = eWNI_SME_UPDATE_SESSION_EDCA_TXQ_PARAMS;
	msg->vdev_id = session_id;
	qdf_mem_copy(&msg->txq_edca_params, txq_edca_params,
		     sizeof(tSirMacEdcaParamRecord));
	msg->length = sizeof(*msg);

	status = umac_send_mb_message_to_mac(msg);

	sme_release_global_lock(&mac_ctx->sme);
	if (status != QDF_STATUS_SUCCESS)
		return QDF_STATUS_E_IO;

	return QDF_STATUS_SUCCESS;
}

/**
 * sme_set_nud_debug_stats_cb() - set nud debug stats callback
 * @mac_handle: Opaque handle to the global MAC context
 * @cb: callback function pointer
 * @context: callback context
 *
 * This function stores nud debug stats callback function.
 *
 * Return: QDF_STATUS enumeration.
 */
QDF_STATUS sme_set_nud_debug_stats_cb(mac_handle_t mac_handle,
				void (*cb)(void *, struct rsp_stats *, void *),
				void *context)
{
	QDF_STATUS status  = QDF_STATUS_SUCCESS;
	struct mac_context *mac;

	if (!mac_handle) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				  FL("mac_handle is not valid"));
		return QDF_STATUS_E_INVAL;
	}
	mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			FL("sme_acquire_global_lock failed!(status=%d)"),
			status);
		return status;
	}

	mac->sme.get_arp_stats_cb = cb;
	mac->sme.get_arp_stats_context = context;
	sme_release_global_lock(&mac->sme);
	return status;
}

/**
 * sme_is_any_session_in_connected_state() - SME wrapper API to
 * check if any session is in connected state or not.
 *
 * @mac_handle: Handle returned by mac open
 *
 * This function is used to check if any valid sme session is in
 * connected state or not.
 *
 * Return: true if any session is connected, else false.
 *
 */
bool sme_is_any_session_in_connected_state(mac_handle_t mac_handle)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	QDF_STATUS status;
	bool ret = false;

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_STATUS_SUCCESS == status) {
		ret = csr_is_any_session_in_connect_state(mac_ctx);
		sme_release_global_lock(&mac_ctx->sme);
	}
	return ret;
}

QDF_STATUS sme_set_chip_pwr_save_fail_cb(mac_handle_t mac_handle,
					 pwr_save_fail_cb cb)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (status != QDF_STATUS_SUCCESS) {
		sme_err("sme_AcquireGlobalLock failed!(status=%d)", status);
		return status;
	}
	mac->sme.chip_power_save_fail_cb = cb;
	sme_release_global_lock(&mac->sme);
	return status;
}

#ifdef FEATURE_RSSI_MONITOR
/**
 * sme_set_rssi_monitoring() - set rssi monitoring
 * @mac_handle: Opaque handle to the global MAC context
 * @input: request message
 *
 * This function constructs the vos message and fill in message type,
 * bodyptr with @input and posts it to WDA queue.
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS sme_set_rssi_monitoring(mac_handle_t mac_handle,
				   struct rssi_monitor_param *input)
{
	QDF_STATUS status     = QDF_STATUS_SUCCESS;
	struct mac_context *mac    = MAC_CONTEXT(mac_handle);
	struct scheduler_msg message = {0};
	struct rssi_monitor_param *req_msg;

	SME_ENTER();
	req_msg = qdf_mem_malloc(sizeof(*req_msg));
	if (!req_msg)
		return QDF_STATUS_E_NOMEM;

	*req_msg = *input;

	status = sme_acquire_global_lock(&mac->sme);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("sme_acquire_global_lock failed!(status=%d)", status);
		qdf_mem_free(req_msg);
		return status;
	}

	/* Serialize the req through MC thread */
	message.bodyptr = req_msg;
	message.type    = WMA_SET_RSSI_MONITOR_REQ;
	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &message);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("scheduler_post_msg failed!(err=%d)", status);
		qdf_mem_free(req_msg);
	}
	sme_release_global_lock(&mac->sme);

	return status;
}

QDF_STATUS sme_set_rssi_threshold_breached_cb(mac_handle_t mac_handle,
					      rssi_threshold_breached_cb cb)
{
	QDF_STATUS status;
	struct mac_context *mac;

	mac = MAC_CONTEXT(mac_handle);
	if (!mac) {
		sme_err("Invalid mac context");
		return QDF_STATUS_E_INVAL;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("sme_acquire_global_lock failed!(status=%d)",
			status);
		return status;
	}

	mac->sme.rssi_threshold_breached_cb = cb;
	sme_release_global_lock(&mac->sme);
	return status;
}
#endif /* FEATURE_RSSI_MONITOR */

QDF_STATUS sme_reset_rssi_threshold_breached_cb(mac_handle_t mac_handle)
{
	return sme_set_rssi_threshold_breached_cb(mac_handle, NULL);
}

/*
 * sme_pdev_set_hw_mode() - Send WMI_PDEV_SET_HW_MODE_CMDID to the WMA
 * @mac_handle: Handle returned by macOpen
 * @msg: HW mode structure containing hw mode and callback details
 *
 * Sends the command to CSR to send WMI_PDEV_SET_HW_MODE_CMDID to FW
 * Return: QDF_STATUS_SUCCESS on successful posting
 */
QDF_STATUS sme_pdev_set_hw_mode(struct policy_mgr_hw_mode msg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = sme_get_mac_context();
	tSmeCmd *cmd = NULL;

	if (!mac) {
		sme_err("mac is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	status = sme_acquire_global_lock(&mac->sme);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Failed to acquire lock");
		return QDF_STATUS_E_RESOURCES;
	}

	cmd = csr_get_command_buffer(mac);
	if (!cmd) {
		sme_err("Get command buffer failed");
		sme_release_global_lock(&mac->sme);
		return QDF_STATUS_E_NULL_VALUE;
	}

	cmd->command = e_sme_command_set_hw_mode;
	cmd->vdev_id = msg.session_id;
	cmd->u.set_hw_mode_cmd.hw_mode_index = msg.hw_mode_index;
	cmd->u.set_hw_mode_cmd.set_hw_mode_cb = msg.set_hw_mode_cb;
	cmd->u.set_hw_mode_cmd.reason = msg.reason;
	cmd->u.set_hw_mode_cmd.session_id = msg.session_id;
	cmd->u.set_hw_mode_cmd.next_action = msg.next_action;
	cmd->u.set_hw_mode_cmd.action = msg.action;
	cmd->u.set_hw_mode_cmd.context = msg.context;
	cmd->u.set_hw_mode_cmd.request_id = msg.request_id;

	sme_debug("Queuing set hw mode to CSR, session: %d reason: %d request_id: %d",
		  cmd->u.set_hw_mode_cmd.session_id,
		  cmd->u.set_hw_mode_cmd.reason,
		  cmd->u.set_hw_mode_cmd.request_id);
	csr_queue_sme_command(mac, cmd, false);

	sme_release_global_lock(&mac->sme);
	return QDF_STATUS_SUCCESS;
}

/**
 * sme_nss_update_request() - Send beacon templete update to FW with new
 * nss value
 * @mac_handle: Handle returned by macOpen
 * @vdev_id: the session id
 * @new_nss: the new nss value
 * @ch_width: channel width, optional value
 * @cback: hdd callback
 * @next_action: next action to happen at policy mgr after beacon update
 * @original_vdev_id: original request hwmode change vdev id
 *
 * Sends the command to CSR to send to PE
 * Return: QDF_STATUS_SUCCESS on successful posting
 */
QDF_STATUS sme_nss_update_request(uint32_t vdev_id,
				uint8_t  new_nss, uint8_t ch_width,
				policy_mgr_nss_update_cback cback,
				uint8_t next_action, struct wlan_objmgr_psoc *psoc,
				enum policy_mgr_conn_update_reason reason,
				uint32_t original_vdev_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = sme_get_mac_context();
	tSmeCmd *cmd = NULL;

	if (!mac) {
		sme_err("mac is null");
		return status;
	}
	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		cmd = csr_get_command_buffer(mac);
		if (!cmd) {
			sme_err("Get command buffer failed");
			sme_release_global_lock(&mac->sme);
			return QDF_STATUS_E_NULL_VALUE;
		}
		cmd->command = e_sme_command_nss_update;
		/* Sessionized modules may require this info */
		cmd->vdev_id = vdev_id;
		cmd->u.nss_update_cmd.new_nss = new_nss;
		cmd->u.nss_update_cmd.ch_width = ch_width;
		cmd->u.nss_update_cmd.session_id = vdev_id;
		cmd->u.nss_update_cmd.nss_update_cb = cback;
		cmd->u.nss_update_cmd.context = psoc;
		cmd->u.nss_update_cmd.next_action = next_action;
		cmd->u.nss_update_cmd.reason = reason;
		cmd->u.nss_update_cmd.original_vdev_id = original_vdev_id;

		sme_debug("Queuing e_sme_command_nss_update to CSR:vdev (%d %d) ss %d r %d",
			  vdev_id, original_vdev_id, new_nss, reason);
		csr_queue_sme_command(mac, cmd, false);
		sme_release_global_lock(&mac->sme);
	}
	return status;
}

/**
 * sme_soc_set_dual_mac_config() - Set dual mac configurations
 * @mac_handle: Handle returned by macOpen
 * @msg: Structure containing the dual mac config parameters
 *
 * Queues configuration information to CSR to configure
 * WLAN firmware for the dual MAC features
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_soc_set_dual_mac_config(struct policy_mgr_dual_mac_config msg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = sme_get_mac_context();
	tSmeCmd *cmd;

	if (!mac) {
		sme_err("mac is null");
		return QDF_STATUS_E_FAILURE;
	}
	status = sme_acquire_global_lock(&mac->sme);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Failed to acquire lock");
		return QDF_STATUS_E_RESOURCES;
	}

	cmd = csr_get_command_buffer(mac);
	if (!cmd) {
		sme_err("Get command buffer failed");
		sme_release_global_lock(&mac->sme);
		return QDF_STATUS_E_NULL_VALUE;
	}

	cmd->command = e_sme_command_set_dual_mac_config;
	cmd->u.set_dual_mac_cmd.scan_config = msg.scan_config;
	cmd->u.set_dual_mac_cmd.fw_mode_config = msg.fw_mode_config;
	cmd->u.set_dual_mac_cmd.set_dual_mac_cb = msg.set_dual_mac_cb;

	sme_debug("set_dual_mac_config scan_config: %x fw_mode_config: %x",
		cmd->u.set_dual_mac_cmd.scan_config,
		cmd->u.set_dual_mac_cmd.fw_mode_config);
	status = csr_queue_sme_command(mac, cmd, false);

	sme_release_global_lock(&mac->sme);
	return status;
}

#ifdef FEATURE_LFR_SUBNET_DETECTION
/**
 * sme_gateway_param_update() - to update gateway parameters with WMA
 * @mac_handle: Opaque handle to the global MAC context
 * @gw_params: request parameters from HDD
 *
 * Return: QDF_STATUS
 *
 * This routine will update gateway parameters to WMA
 */
QDF_STATUS sme_gateway_param_update(mac_handle_t mac_handle,
				    struct gateway_update_req_param *gw_params)
{
	QDF_STATUS qdf_status;
	struct scheduler_msg message = {0};
	struct gateway_update_req_param *request_buf;

	request_buf = qdf_mem_malloc(sizeof(*request_buf));
	if (!request_buf)
		return QDF_STATUS_E_NOMEM;

	*request_buf = *gw_params;

	message.type = WMA_GW_PARAM_UPDATE_REQ;
	message.reserved = 0;
	message.bodyptr = request_buf;
	qdf_status = scheduler_post_message(QDF_MODULE_ID_SME,
					    QDF_MODULE_ID_WMA,
					    QDF_MODULE_ID_WMA, &message);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			"Not able to post WMA_GW_PARAM_UPDATE_REQ message to HAL");
		qdf_mem_free(request_buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}
#endif /* FEATURE_LFR_SUBNET_DETECTION */

/**
 * sme_soc_set_antenna_mode() - set antenna mode
 * @mac_handle: Handle returned by macOpen
 * @msg: Structure containing the antenna mode parameters
 *
 * Send the command to CSR to send
 * WMI_SOC_SET_ANTENNA_MODE_CMDID to FW
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_soc_set_antenna_mode(mac_handle_t mac_handle,
				    struct sir_antenna_mode_param *msg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	tSmeCmd *cmd;

	if (!msg) {
		sme_err("antenna mode mesg is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Failed to acquire lock");
		return QDF_STATUS_E_RESOURCES;
	}

	cmd = csr_get_command_buffer(mac);
	if (!cmd) {
		sme_release_global_lock(&mac->sme);
		sme_err("Get command buffer failed");
		return QDF_STATUS_E_NULL_VALUE;
	}

	cmd->command = e_sme_command_set_antenna_mode;
	cmd->u.set_antenna_mode_cmd = *msg;

	sme_debug("Antenna mode rx_chains: %d tx_chains: %d",
		cmd->u.set_antenna_mode_cmd.num_rx_chains,
		cmd->u.set_antenna_mode_cmd.num_tx_chains);

	csr_queue_sme_command(mac, cmd, false);
	sme_release_global_lock(&mac->sme);

	return QDF_STATUS_SUCCESS;
}

/**
 * sme_set_peer_authorized() - call peer authorized callback
 * @peer_addr: peer mac address
 * @auth_cb: auth callback
 * @vdev_id: vdev id
 *
 * Return: QDF Status
 */
QDF_STATUS sme_set_peer_authorized(uint8_t *peer_addr,
				   sme_peer_authorized_fp auth_cb,
				   uint32_t vdev_id)
{
	void *wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				"wma handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	wma_set_peer_authorized_cb(wma_handle, auth_cb);
	return wma_set_peer_param(wma_handle, peer_addr, WMI_PEER_AUTHORIZE,
				  1, vdev_id);
}

/**
 * sme_setdef_dot11mode() - Updates mac with default dot11mode
 * @mac_handle: Global MAC pointer
 *
 * Return: NULL.
 */
void sme_setdef_dot11mode(mac_handle_t mac_handle)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	csr_set_default_dot11_mode(mac_ctx);
}

/**
 * sme_update_tgt_services() - update the target services config.
 * @mac_handle: Opaque handle to the global MAC context.
 * @cfg: wma_tgt_services parameters.
 *
 * update the target services config.
 *
 * Return: None.
 */
void sme_update_tgt_services(mac_handle_t mac_handle,
			     struct wma_tgt_services *cfg)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	mac_ctx->obss_scan_offload = cfg->obss_scan_offload;
	sme_debug("obss_scan_offload: %d", mac_ctx->obss_scan_offload);
	mac_ctx->mlme_cfg->gen.as_enabled = cfg->lte_coex_ant_share;
	mac_ctx->beacon_offload = cfg->beacon_offload;
	mac_ctx->pmf_offload = cfg->pmf_offload;
	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		FL("mac_ctx->pmf_offload: %d"), mac_ctx->pmf_offload);
	mac_ctx->is_fils_roaming_supported =
				cfg->is_fils_roaming_supported;
	mac_ctx->is_11k_offload_supported =
				cfg->is_11k_offload_supported;
	sme_debug("pmf_offload: %d fils_roam support %d 11k_offload %d",
		  mac_ctx->pmf_offload, mac_ctx->is_fils_roaming_supported,
		  mac_ctx->is_11k_offload_supported);
	mac_ctx->bcn_reception_stats = cfg->bcn_reception_stats;
}

/**
 * sme_is_session_id_valid() - Check if the session id is valid
 * @mac_handle: Opaque handle to the global MAC context
 * @session_id: Session id
 *
 * Checks if the session id is valid or not
 *
 * Return: True is the session id is valid, false otherwise
 */
bool sme_is_session_id_valid(mac_handle_t mac_handle, uint32_t session_id)
{
	struct mac_context *mac;

	if (mac_handle) {
		mac = MAC_CONTEXT(mac_handle);
	} else {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			"%s: null mac pointer", __func__);
		return false;
	}

	if (CSR_IS_SESSION_VALID(mac, session_id))
		return true;

	return false;
}

#ifdef FEATURE_WLAN_TDLS

/**
 * sme_get_opclass() - determine operating class
 * @mac_handle: Opaque handle to the global MAC context
 * @channel: channel id
 * @bw_offset: bandwidth offset
 * @opclass: pointer to operating class
 *
 * Function will determine operating class from regdm_get_opclass_from_channel
 *
 * Return: none
 */
void sme_get_opclass(mac_handle_t mac_handle, uint8_t channel,
		     uint8_t bw_offset, uint8_t *opclass)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	/* redgm opclass table contains opclass for 40MHz low primary,
	 * 40MHz high primary and 20MHz. No support for 80MHz yet. So
	 * first we will check if bit for 40MHz is set and if so find
	 * matching opclass either with low primary or high primary
	 * (a channel would never be in both) and then search for opclass
	 * matching 20MHz, else for any BW.
	 */
	if (bw_offset & (1 << BW_40_OFFSET_BIT)) {
		*opclass = wlan_reg_dmn_get_opclass_from_channel(
				mac_ctx->scan.countryCodeCurrent,
				channel, BW40_LOW_PRIMARY);
		if (!(*opclass)) {
			*opclass = wlan_reg_dmn_get_opclass_from_channel(
					mac_ctx->scan.countryCodeCurrent,
					channel, BW40_HIGH_PRIMARY);
		}
	} else if (bw_offset & (1 << BW_20_OFFSET_BIT)) {
		*opclass = wlan_reg_dmn_get_opclass_from_channel(
				mac_ctx->scan.countryCodeCurrent,
				channel, BW20);
	} else {
		*opclass = wlan_reg_dmn_get_opclass_from_channel(
				mac_ctx->scan.countryCodeCurrent,
				channel, BWALL);
	}
}
#endif

/**
 * sme_set_fw_test() - set fw test
 * @fw_test: fw test param
 *
 * Return: Return QDF_STATUS, otherwise appropriate failure code
 */
QDF_STATUS sme_set_fw_test(struct set_fwtest_params *fw_test)
{
	void *wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				"wma handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wma_process_fw_test_cmd(wma_handle, fw_test);
}

/**
 * sme_ht40_stop_obss_scan() - ht40 obss stop scan
 * @mac_handle: mac handel
 * @vdev_id: vdev identifier
 *
 * Return: Return QDF_STATUS, otherwise appropriate failure code
 */
QDF_STATUS sme_ht40_stop_obss_scan(mac_handle_t mac_handle, uint32_t vdev_id)
{
	void *wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				"wma handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	wma_ht40_stop_obss_scan(wma_handle, vdev_id);
	return QDF_STATUS_SUCCESS;
}

/**
 * sme_update_mimo_power_save() - Update MIMO power save
 * configuration
 * @mac_handle: The handle returned by macOpen
 * @is_ht_smps_enabled: enable/disable ht smps
 * @ht_smps_mode: smps mode disabled/static/dynamic
 * @send_smps_action: flag to send smps force mode command
 * to FW
 *
 * Return: QDF_STATUS if SME update mimo power save
 * configuration success else failure status
 */
QDF_STATUS sme_update_mimo_power_save(mac_handle_t mac_handle,
				      uint8_t is_ht_smps_enabled,
				      uint8_t ht_smps_mode,
				      bool send_smps_action)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	sme_debug("SMPS enable: %d mode: %d send action: %d",
		is_ht_smps_enabled, ht_smps_mode,
		send_smps_action);
	mac_ctx->mlme_cfg->ht_caps.enable_smps =
		is_ht_smps_enabled;
	mac_ctx->mlme_cfg->ht_caps.smps = ht_smps_mode;
	mac_ctx->roam.configParam.send_smps_action =
		send_smps_action;

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_BCN_RECV_FEATURE
QDF_STATUS sme_handle_bcn_recv_start(mac_handle_t mac_handle,
				     uint32_t vdev_id, uint32_t nth_value,
				     bool do_not_resume)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session;
	QDF_STATUS status;
	int ret;

	session = CSR_GET_SESSION(mac_ctx, vdev_id);
	if (!session) {
		sme_err("vdev_id %d not found", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (!CSR_IS_SESSION_VALID(mac_ctx, vdev_id)) {
		sme_err("CSR session not valid: %d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (session->is_bcn_recv_start) {
			sme_release_global_lock(&mac_ctx->sme);
			sme_err("Beacon receive already started");
			return QDF_STATUS_SUCCESS;
		}
		session->is_bcn_recv_start = true;
		session->beacon_report_do_not_resume = do_not_resume;
		sme_release_global_lock(&mac_ctx->sme);
	}

	/*
	 * Allows fw to send beacons of connected AP to driver.
	 * MSB set : means fw do not wakeup host in wow mode
	 * LSB set: Value of beacon report period (say n), Means fw sends nth
	 * beacons of connected AP to HOST
	 */
	ret = sme_cli_set_command(vdev_id,
				  WMI_VDEV_PARAM_NTH_BEACON_TO_HOST,
				  nth_value, VDEV_CMD);
	if (ret) {
		status = sme_acquire_global_lock(&mac_ctx->sme);
		if (QDF_IS_STATUS_SUCCESS(status)) {
			session->is_bcn_recv_start = false;
			session->beacon_report_do_not_resume = false;
			sme_release_global_lock(&mac_ctx->sme);
		}
		sme_err("WMI_VDEV_PARAM_NTH_BEACON_TO_HOST %d", ret);
		status = qdf_status_from_os_return(ret);
	}

	return status;
}

void sme_stop_beacon_report(mac_handle_t mac_handle, uint32_t session_id)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session;
	QDF_STATUS status;
	int ret;

	session = CSR_GET_SESSION(mac_ctx, session_id);
	if (!session) {
		sme_err("vdev_id %d not found", session_id);
		return;
	}

	ret = sme_cli_set_command(session_id,
				  WMI_VDEV_PARAM_NTH_BEACON_TO_HOST, 0,
				  VDEV_CMD);
	if (ret)
		sme_err("WMI_VDEV_PARAM_NTH_BEACON_TO_HOST command failed to FW");
	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		session->is_bcn_recv_start = false;
		session->beacon_report_do_not_resume = false;
		sme_release_global_lock(&mac_ctx->sme);
	}
}

bool sme_is_beacon_report_started(mac_handle_t mac_handle, uint32_t session_id)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session;

	session = CSR_GET_SESSION(mac_ctx, session_id);
	if (!session) {
		sme_err("vdev_id %d not found", session_id);
		return false;
	}

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("CSR session not valid: %d", session_id);
		return false;
	}

	return session->is_bcn_recv_start;
}

bool sme_is_beacon_reporting_do_not_resume(mac_handle_t mac_handle,
					   uint32_t session_id)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session;

	session = CSR_GET_SESSION(mac_ctx, session_id);
	if (!session) {
		sme_err("vdev_id %d not found", session_id);
		return false;
	}

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("CSR session not valid: %d", session_id);
		return false;
	}

	return session->beacon_report_do_not_resume;
}
#endif

/**
 * sme_add_beacon_filter() - set the beacon filter configuration
 * @mac_handle: The handle returned by macOpen
 * @session_id: session id
 * @ie_map: bitwise array of IEs
 *
 * Return: Return QDF_STATUS, otherwise appropriate failure code
 */
QDF_STATUS sme_add_beacon_filter(mac_handle_t mac_handle,
				 uint32_t session_id,
				 uint32_t *ie_map)
{
	struct scheduler_msg message = {0};
	QDF_STATUS qdf_status;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct beacon_filter_param *filter_param;

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("CSR session not valid: %d", session_id);
		return QDF_STATUS_E_FAILURE;
	}

	filter_param = qdf_mem_malloc(sizeof(*filter_param));
	if (!filter_param)
		return QDF_STATUS_E_FAILURE;

	filter_param->vdev_id = session_id;

	qdf_mem_copy(filter_param->ie_map, ie_map,
			BCN_FLT_MAX_ELEMS_IE_LIST * sizeof(uint32_t));

	message.type = WMA_ADD_BCN_FILTER_CMDID;
	message.bodyptr = filter_param;
	qdf_status = scheduler_post_message(QDF_MODULE_ID_SME,
					    QDF_MODULE_ID_WMA,
					    QDF_MODULE_ID_WMA,
					    &message);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			"%s: Not able to post msg to WDA!",
			__func__);

		qdf_mem_free(filter_param);
	}
	return qdf_status;
}

/**
 * sme_remove_beacon_filter() - set the beacon filter configuration
 * @mac_handle: The handle returned by macOpen
 * @session_id: session id
 *
 * Return: Return QDF_STATUS, otherwise appropriate failure code
 */
QDF_STATUS sme_remove_beacon_filter(mac_handle_t mac_handle,
				    uint32_t session_id)
{
	struct scheduler_msg message = {0};
	QDF_STATUS qdf_status;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct beacon_filter_param *filter_param;

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("CSR session not valid: %d", session_id);
		return QDF_STATUS_E_FAILURE;
	}

	filter_param = qdf_mem_malloc(sizeof(*filter_param));
	if (!filter_param)
		return QDF_STATUS_E_FAILURE;

	filter_param->vdev_id = session_id;

	message.type = WMA_REMOVE_BCN_FILTER_CMDID;
	message.bodyptr = filter_param;
	qdf_status = scheduler_post_message(QDF_MODULE_ID_SME,
					    QDF_MODULE_ID_WMA,
					    QDF_MODULE_ID_WMA,
					    &message);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			"%s: Not able to post msg to WDA!",
			__func__);

		qdf_mem_free(filter_param);
	}
	return qdf_status;
}

/**
 * sme_send_disassoc_req_frame - send disassoc req
 * @mac_handle: Opaque handle to the global MAC context
 * @session_id: session id
 * @peer_mac: peer mac address
 * @reason: reason for disassociation
 * wait_for_ack: wait for acknowledgment
 *
 * function to send disassoc request to lim
 *
 * return: none
 */
void sme_send_disassoc_req_frame(mac_handle_t mac_handle, uint8_t session_id,
				 uint8_t *peer_mac, uint16_t reason,
				 uint8_t wait_for_ack)
{
	struct sme_send_disassoc_frm_req *msg;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	msg = qdf_mem_malloc(sizeof(*msg));
	if (!msg)
		return;

	msg->msg_type = eWNI_SME_SEND_DISASSOC_FRAME;
	msg->length = sizeof(*msg);
	msg->vdev_id = session_id;
	qdf_mem_copy(msg->peer_mac, peer_mac, QDF_MAC_ADDR_SIZE);
	msg->reason = reason;
	msg->wait_for_ack = wait_for_ack;

	qdf_status = umac_send_mb_message_to_mac(msg);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		sme_err("umac_send_mb_message_to_mac failed, %d",
			qdf_status);
}

#ifdef FEATURE_WLAN_APF
QDF_STATUS sme_get_apf_capabilities(mac_handle_t mac_handle,
				    apf_get_offload_cb callback,
				    void *context)
{
	QDF_STATUS          status     = QDF_STATUS_SUCCESS;
	struct mac_context *     mac_ctx      = MAC_CONTEXT(mac_handle);
	struct scheduler_msg           cds_msg = {0};

	SME_ENTER();

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_STATUS_SUCCESS == status) {
		/* Serialize the req through MC thread */
		mac_ctx->sme.apf_get_offload_cb = callback;
		mac_ctx->sme.apf_get_offload_context = context;
		cds_msg.bodyptr = NULL;
		cds_msg.type = WDA_APF_GET_CAPABILITIES_REQ;
		status = scheduler_post_message(QDF_MODULE_ID_SME,
						QDF_MODULE_ID_WMA,
						QDF_MODULE_ID_WMA, &cds_msg);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
					FL("Post apf get offload msg fail"));
			status = QDF_STATUS_E_FAILURE;
		}
		sme_release_global_lock(&mac_ctx->sme);
	}

	SME_EXIT();
	return status;
}

QDF_STATUS sme_set_apf_instructions(mac_handle_t mac_handle,
				    struct sir_apf_set_offload *req)
{
	void *wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				"wma handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wma_set_apf_instructions(wma_handle, req);
}

QDF_STATUS sme_set_apf_enable_disable(mac_handle_t mac_handle, uint8_t vdev_id,
				      bool apf_enable)
{
	void *wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				"wma handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wma_send_apf_enable_cmd(wma_handle, vdev_id, apf_enable);
}

QDF_STATUS
sme_apf_write_work_memory(mac_handle_t mac_handle,
			struct wmi_apf_write_memory_params *write_params)
{
	void *wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				"wma handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wma_send_apf_write_work_memory_cmd(wma_handle, write_params);
}

QDF_STATUS
sme_apf_read_work_memory(mac_handle_t mac_handle,
			 struct wmi_apf_read_memory_params *read_params,
			 apf_read_mem_cb callback)
{
	QDF_STATUS status   = QDF_STATUS_SUCCESS;
	struct mac_context *mac  = MAC_CONTEXT(mac_handle);
	void *wma_handle;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		mac->sme.apf_read_mem_cb = callback;
		sme_release_global_lock(&mac->sme);
	} else {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			FL("sme_acquire_global_lock failed"));
	}

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				"wma handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wma_send_apf_read_work_memory_cmd(wma_handle, read_params);
}
#endif /* FEATURE_WLAN_APF */

/**
 * sme_get_wni_dot11_mode() - return configured wni dot11mode
 * @mac_handle: Opaque handle to the global MAC context
 *
 * Return: wni dot11 mode.
 */
uint32_t sme_get_wni_dot11_mode(mac_handle_t mac_handle)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	return csr_translate_to_wni_cfg_dot11_mode(mac_ctx,
		mac_ctx->roam.configParam.uCfgDot11Mode);
}

/**
 * sme_create_mon_session() - post message to create PE session for monitormode
 * operation
 * @mac_handle: Opaque handle to the global MAC context
 * @bssid: pointer to bssid
 * @vdev_id: sme session id
 *
 * Return: QDF_STATUS_SUCCESS on success, non-zero error code on failure.
 */
QDF_STATUS sme_create_mon_session(mac_handle_t mac_handle, tSirMacAddr bss_id,
				  uint8_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct sir_create_session *msg;

	msg = qdf_mem_malloc(sizeof(*msg));
	if (msg) {
		msg->type = eWNI_SME_MON_INIT_SESSION;
		msg->vdev_id = vdev_id;
		msg->msg_len = sizeof(*msg);
		qdf_mem_copy(msg->bss_id.bytes, bss_id, QDF_MAC_ADDR_SIZE);
		status = umac_send_mb_message_to_mac(msg);
	}
	return status;
}

QDF_STATUS sme_delete_mon_session(mac_handle_t mac_handle, uint8_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct sir_delete_session *msg;

	msg = qdf_mem_malloc(sizeof(*msg));
	if (msg) {
		msg->type = eWNI_SME_MON_DEINIT_SESSION;
		msg->vdev_id = vdev_id;
		msg->msg_len = sizeof(*msg);
		status = umac_send_mb_message_to_mac(msg);
	}

	return status;
}

void
sme_set_del_peers_ind_callback(mac_handle_t mac_handle,
			       void (*callback)(struct wlan_objmgr_psoc *psoc,
						uint8_t vdev_id))
{
	struct mac_context *mac;

	if (!mac_handle) {
		QDF_ASSERT(0);
		return;
	}
	mac = MAC_CONTEXT(mac_handle);
	mac->del_peers_ind_cb = callback;
}

void sme_set_chan_info_callback(mac_handle_t mac_handle,
			void (*callback)(struct scan_chan_info *chan_info))
{
	struct mac_context *mac;

	if (!mac_handle) {
		QDF_ASSERT(0);
		return;
	}
	mac = MAC_CONTEXT(mac_handle);
	mac->chan_info_cb = callback;
}

#ifdef WLAN_FEATURE_CAL_FAILURE_TRIGGER
void sme_set_cal_failure_event_cb(
			mac_handle_t mac_handle,
			void (*callback)(uint8_t cal_type, uint8_t reason))
{
	struct mac_context *mac;
	QDF_STATUS status   = QDF_STATUS_SUCCESS;

	if (!mac_handle) {
		QDF_ASSERT(0);
		return;
	}
	mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		mac->cal_failure_event_cb = callback;
		sme_release_global_lock(&mac->sme);
	} else {
		sme_err("sme_acquire_global_lock failed");
	}
}
#endif

void sme_set_vdev_ies_per_band(mac_handle_t mac_handle, uint8_t vdev_id,
			       enum QDF_OPMODE device_mode)
{
	struct sir_set_vdev_ies_per_band *p_msg;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	enum csr_cfgdot11mode curr_dot11_mode =
				mac_ctx->roam.configParam.uCfgDot11Mode;

	p_msg = qdf_mem_malloc(sizeof(*p_msg));
	if (!p_msg)
		return;


	p_msg->vdev_id = vdev_id;
	p_msg->device_mode = device_mode;
	p_msg->dot11_mode = csr_get_vdev_dot11_mode(mac_ctx, device_mode,
						    curr_dot11_mode);
	p_msg->msg_type = eWNI_SME_SET_VDEV_IES_PER_BAND;
	p_msg->len = sizeof(*p_msg);
	sme_debug("SET_VDEV_IES_PER_BAND: vdev_id %d dot11mode %d dev_mode %d",
		  vdev_id, p_msg->dot11_mode, device_mode);
	status = umac_send_mb_message_to_mac(p_msg);
	if (QDF_STATUS_SUCCESS != status)
		sme_err("Send eWNI_SME_SET_VDEV_IES_PER_BAND fail");
}

/**
 * sme_set_pdev_ht_vht_ies() - sends the set pdev IE req
 * @mac_handle: Opaque handle to the global MAC context
 * @enable2x2: 1x1 or 2x2 mode.
 *
 * Sends the set pdev IE req with Nss value.
 *
 * Return: None
 */
void sme_set_pdev_ht_vht_ies(mac_handle_t mac_handle, bool enable2x2)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct sir_set_ht_vht_cfg *ht_vht_cfg;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!((mac_ctx->roam.configParam.uCfgDot11Mode ==
					eCSR_CFG_DOT11_MODE_AUTO) ||
				(mac_ctx->roam.configParam.uCfgDot11Mode ==
				 eCSR_CFG_DOT11_MODE_11N) ||
				(mac_ctx->roam.configParam.uCfgDot11Mode ==
				 eCSR_CFG_DOT11_MODE_11N_ONLY) ||
				(mac_ctx->roam.configParam.uCfgDot11Mode ==
				 eCSR_CFG_DOT11_MODE_11AC) ||
				(mac_ctx->roam.configParam.uCfgDot11Mode ==
				 eCSR_CFG_DOT11_MODE_11AC_ONLY)))
		return;

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_STATUS_SUCCESS == status) {
		ht_vht_cfg = qdf_mem_malloc(sizeof(*ht_vht_cfg));
		if (!ht_vht_cfg) {
			sme_release_global_lock(&mac_ctx->sme);
			return;
		}

		ht_vht_cfg->pdev_id = 0;
		if (enable2x2)
			ht_vht_cfg->nss = 2;
		else
			ht_vht_cfg->nss = 1;
		ht_vht_cfg->dot11mode =
			(uint8_t)csr_translate_to_wni_cfg_dot11_mode(mac_ctx,
				mac_ctx->roam.configParam.uCfgDot11Mode);

		ht_vht_cfg->msg_type = eWNI_SME_PDEV_SET_HT_VHT_IE;
		ht_vht_cfg->len = sizeof(*ht_vht_cfg);
		sme_debug("SET_HT_VHT_IE with nss: %d, dot11mode: %d",
			  ht_vht_cfg->nss,
			  ht_vht_cfg->dot11mode);
		status = umac_send_mb_message_to_mac(ht_vht_cfg);
		if (QDF_STATUS_SUCCESS != status)
			sme_err("Send SME_PDEV_SET_HT_VHT_IE fail");

		sme_release_global_lock(&mac_ctx->sme);
	}
}

void sme_get_sap_vdev_type_nss(mac_handle_t mac_handle, uint8_t *vdev_nss,
			       enum band_info band)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	if (band == BAND_5G)
		*vdev_nss = mac_ctx->vdev_type_nss_5g.sap;
	else
		*vdev_nss = mac_ctx->vdev_type_nss_2g.sap;
}

void sme_update_vdev_type_nss(mac_handle_t mac_handle, uint8_t max_supp_nss,
			      enum nss_chains_band_info band)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct vdev_type_nss *vdev_nss;

	struct wlan_mlme_nss_chains *nss_chains_ini_cfg =
					&mac_ctx->mlme_cfg->nss_chains_ini_cfg;

	if (band == NSS_CHAINS_BAND_5GHZ)
		vdev_nss = &mac_ctx->vdev_type_nss_5g;
	else
		vdev_nss = &mac_ctx->vdev_type_nss_2g;

	vdev_nss->sta = QDF_MIN(max_supp_nss, GET_VDEV_NSS_CHAIN(
						nss_chains_ini_cfg->
							rx_nss[band],
						STA_NSS_CHAINS_SHIFT));
	vdev_nss->sap = QDF_MIN(max_supp_nss, GET_VDEV_NSS_CHAIN(
						nss_chains_ini_cfg->
							rx_nss[band],
						SAP_NSS_CHAINS_SHIFT));
	vdev_nss->p2p_go = QDF_MIN(max_supp_nss, GET_VDEV_NSS_CHAIN(
						nss_chains_ini_cfg->
							rx_nss[band],
						P2P_GO_NSS_CHAINS_SHIFT));
	vdev_nss->p2p_cli = QDF_MIN(max_supp_nss, GET_VDEV_NSS_CHAIN(
						nss_chains_ini_cfg->
							rx_nss[band],
						P2P_CLI_CHAINS_SHIFT));
	vdev_nss->p2p_dev = QDF_MIN(max_supp_nss, GET_VDEV_NSS_CHAIN(
						nss_chains_ini_cfg->
							rx_nss[band],
						P2P_DEV_NSS_CHAINS_SHIFT));
	vdev_nss->ibss = QDF_MIN(max_supp_nss, GET_VDEV_NSS_CHAIN(
						nss_chains_ini_cfg->
							rx_nss[band],
						IBSS_NSS_CHAINS_SHIFT));
	vdev_nss->tdls = QDF_MIN(max_supp_nss, GET_VDEV_NSS_CHAIN(
						nss_chains_ini_cfg->
							rx_nss[band],
						TDLS_NSS_CHAINS_SHIFT));
	vdev_nss->ocb = QDF_MIN(max_supp_nss, GET_VDEV_NSS_CHAIN(
						nss_chains_ini_cfg->
							rx_nss[band],
						OCB_NSS_CHAINS_SHIFT));
	vdev_nss->nan = QDF_MIN(max_supp_nss, GET_VDEV_NSS_CHAIN(
						nss_chains_ini_cfg->
							rx_nss[band],
						NAN_NSS_CHAIN_SHIFT));
	vdev_nss->ndi = QDF_MIN(max_supp_nss, GET_VDEV_NSS_CHAIN(
						nss_chains_ini_cfg->
							rx_nss[band],
						NAN_NSS_CHAIN_SHIFT));

	sme_debug("band %d NSS:sta %d sap %d cli %d go %d dev %d ibss %d tdls %d ocb %d nan %d",
		  band, vdev_nss->sta, vdev_nss->sap, vdev_nss->p2p_cli,
		  vdev_nss->p2p_go, vdev_nss->p2p_dev, vdev_nss->ibss,
		  vdev_nss->tdls, vdev_nss->ocb, vdev_nss->nan);
}

#ifdef WLAN_FEATURE_11AX_BSS_COLOR
#define MAX_BSS_COLOR_VAL 63
#define MIN_BSS_COLOR_VAL 1

QDF_STATUS sme_set_he_bss_color(mac_handle_t mac_handle, uint8_t session_id,
				uint8_t bss_color)

{
	struct sir_set_he_bss_color *bss_color_msg;
	uint8_t len;

	if (!mac_handle) {
		sme_err("Invalid mac_handle pointer");
		return QDF_STATUS_E_FAULT;
	}

	sme_debug("Set HE bss_color  %d", bss_color);

	if (bss_color < MIN_BSS_COLOR_VAL || bss_color > MAX_BSS_COLOR_VAL) {
		sme_debug("Invalid HE bss_color  %d", bss_color);
		return QDF_STATUS_E_INVAL;
	}
	len = sizeof(*bss_color_msg);
	bss_color_msg = qdf_mem_malloc(len);
	if (!bss_color_msg)
		return QDF_STATUS_E_NOMEM;

	bss_color_msg->message_type = eWNI_SME_SET_HE_BSS_COLOR;
	bss_color_msg->length = len;
	bss_color_msg->vdev_id = session_id;
	bss_color_msg->bss_color = bss_color;
	return umac_send_mb_message_to_mac(bss_color_msg);
}
#endif

#ifdef FEATURE_P2P_LISTEN_OFFLOAD
/**
 * sme_register_p2p_lo_event() - Register for the p2p lo event
 * @mac_handle: Opaque handle to the global MAC context
 * @context: the context of the call
 * @callback: the callback to hdd
 *
 * This function registers the callback function for P2P listen
 * offload stop event.
 *
 * Return: none
 */
void sme_register_p2p_lo_event(mac_handle_t mac_handle, void *context,
			       p2p_lo_callback callback)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	status = sme_acquire_global_lock(&mac->sme);
	mac->sme.p2p_lo_event_callback = callback;
	mac->sme.p2p_lo_event_context = context;
	sme_release_global_lock(&mac->sme);
}
#endif

/**
 * sme_process_mac_pwr_dbg_cmd() - enable mac pwr debugging
 * @mac_handle: The handle returned by macOpen
 * @session_id: session id
 * @dbg_args: args for mac pwr debug command
 * Return: Return QDF_STATUS, otherwise appropriate failure code
 */
QDF_STATUS sme_process_mac_pwr_dbg_cmd(mac_handle_t mac_handle,
				       uint32_t session_id,
				       struct sir_mac_pwr_dbg_cmd *dbg_args)
{
	struct scheduler_msg message = {0};
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct sir_mac_pwr_dbg_cmd *req;
	int i;

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("CSR session not valid: %d", session_id);
		return QDF_STATUS_E_FAILURE;
	}

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return QDF_STATUS_E_FAILURE;

	req->module_id = dbg_args->module_id;
	req->pdev_id = dbg_args->pdev_id;
	req->num_args = dbg_args->num_args;
	for (i = 0; i < req->num_args; i++)
		req->args[i] = dbg_args->args[i];

	message.type = SIR_HAL_POWER_DBG_CMD;
	message.bodyptr = req;

	if (!QDF_IS_STATUS_SUCCESS(scheduler_post_message(QDF_MODULE_ID_SME,
							  QDF_MODULE_ID_WMA,
							  QDF_MODULE_ID_WMA,
							  &message))) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Not able to post msg to WDA!",
			  __func__);
		qdf_mem_free(req);
	}
	return QDF_STATUS_SUCCESS;
}
/**
 * sme_get_vdev_type_nss() - gets the nss per vdev type
 * @dev_mode: connection type.
 * @nss2g: Pointer to the 2G Nss parameter.
 * @nss5g: Pointer to the 5G Nss parameter.
 *
 * Fills the 2G and 5G Nss values based on connection type.
 *
 * Return: None
 */
void sme_get_vdev_type_nss(enum QDF_OPMODE dev_mode,
			   uint8_t *nss_2g, uint8_t *nss_5g)
{
	csr_get_vdev_type_nss(dev_mode, nss_2g, nss_5g);
}

/**
 * sme_update_sta_roam_policy() - update sta roam policy for
 * unsafe and DFS channels.
 * @mac_handle: Opaque handle to the global MAC context
 * @dfs_mode: dfs mode which tell if dfs channel needs to be
 * skipped or not
 * @skip_unsafe_channels: Param to tell if driver needs to
 * skip unsafe channels or not.
 * @vdev_id: vdev_id
 * @sap_operating_band: Band on which SAP is operating
 *
 * sme_update_sta_roam_policy update sta rome policies to csr
 * this function will call csrUpdateChannelList as well
 * to include/exclude DFS channels and unsafe channels.
 *
 * Return: eHAL_STATUS_SUCCESS or non-zero on failure.
 */
QDF_STATUS sme_update_sta_roam_policy(mac_handle_t mac_handle,
				      enum sta_roam_policy_dfs_mode dfs_mode,
				      bool skip_unsafe_channels,
				      uint8_t vdev_id,
				      uint8_t sap_operating_band)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct sme_config_params *sme_config;

	if (!mac_ctx) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_FATAL,
				"%s: mac_ctx is null", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	sme_config = qdf_mem_malloc(sizeof(*sme_config));
	if (!sme_config)
		return QDF_STATUS_E_FAILURE;

	qdf_mem_zero(sme_config, sizeof(*sme_config));
	sme_get_config_param(mac_handle, sme_config);

	sme_config->csr_config.sta_roam_policy_params.dfs_mode =
		dfs_mode;
	sme_config->csr_config.sta_roam_policy_params.skip_unsafe_channels =
		skip_unsafe_channels;
	sme_config->csr_config.sta_roam_policy_params.sap_operating_band =
		sap_operating_band;

	sme_update_config(mac_handle, sme_config);

	status = csr_update_channel_list(mac_ctx);
	if (QDF_STATUS_SUCCESS != status) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			FL("failed to update the supported channel list"));
	}

	if (mac_ctx->mlme_cfg->lfr.roam_scan_offload_enabled) {
		status = sme_acquire_global_lock(&mac_ctx->sme);
		if (QDF_IS_STATUS_SUCCESS(status)) {
			csr_roam_update_cfg(mac_ctx, vdev_id,
				      REASON_ROAM_SCAN_STA_ROAM_POLICY_CHANGED);
			sme_release_global_lock(&mac_ctx->sme);
		}
	}
	qdf_mem_free(sme_config);
	return status;
}

/**
 * sme_enable_disable_chanavoidind_event - configure ca event ind
 * @mac_handle: Opaque handle to the global MAC context
 * @set_value: enable/disable
 *
 * function to enable/disable chan avoidance indication
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_enable_disable_chanavoidind_event(mac_handle_t mac_handle,
						 uint8_t set_value)
{
	QDF_STATUS status;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct scheduler_msg msg = {0};

	if (!mac_ctx->mlme_cfg->gen.optimize_ca_event) {
		sme_debug("optimize_ca_event not enabled in ini");
		return QDF_STATUS_E_NOSUPPORT;
	}

	sme_debug("set_value: %d", set_value);
	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_STATUS_SUCCESS == status) {
		qdf_mem_zero(&msg, sizeof(struct scheduler_msg));
		msg.type = WMA_SEND_FREQ_RANGE_CONTROL_IND;
		msg.bodyval = set_value;
		status = scheduler_post_message(QDF_MODULE_ID_SME,
						QDF_MODULE_ID_WMA,
						QDF_MODULE_ID_WMA, &msg);
		sme_release_global_lock(&mac_ctx->sme);
		return status;
	}
	return status;
}

/*
 * sme_set_default_scan_ie() - API to send default scan IE to LIM
 * @mac_handle: Opaque handle to the global MAC context
 * @session_id: current session ID
 * @ie_data: Pointer to Scan IE data
 * @ie_len: Length of @ie_data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_set_default_scan_ie(mac_handle_t mac_handle, uint16_t session_id,
				   uint8_t *ie_data, uint16_t ie_len)
{
	QDF_STATUS status;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct hdd_default_scan_ie *set_ie_params;

	if (!ie_data)
		return QDF_STATUS_E_INVAL;

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		set_ie_params = qdf_mem_malloc(sizeof(*set_ie_params));
		if (!set_ie_params)
			status = QDF_STATUS_E_NOMEM;
		else {
			set_ie_params->message_type = eWNI_SME_DEFAULT_SCAN_IE;
			set_ie_params->length = sizeof(*set_ie_params);
			set_ie_params->vdev_id = session_id;
			set_ie_params->ie_len = ie_len;
			qdf_mem_copy(set_ie_params->ie_data, ie_data, ie_len);
			status = umac_send_mb_message_to_mac(set_ie_params);
		}
		sme_release_global_lock(&mac_ctx->sme);
	}
	return status;
}

QDF_STATUS sme_get_sar_power_limits(mac_handle_t mac_handle,
				    wma_sar_cb callback, void *context)
{
	void *wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				"wma handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wma_get_sar_limit(wma_handle, callback, context);
}

QDF_STATUS sme_set_sar_power_limits(mac_handle_t mac_handle,
				    struct sar_limit_cmd_params *sar_limit_cmd)
{
	void *wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				"wma handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wma_set_sar_limit(wma_handle, sar_limit_cmd);
}

QDF_STATUS sme_send_coex_config_cmd(struct coex_config_params *coex_cfg_params)
{
	void *wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		sme_err("wma handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	return wma_send_coex_config_cmd(wma_handle, coex_cfg_params);
}

#ifdef WLAN_FEATURE_FIPS
QDF_STATUS sme_fips_request(mac_handle_t mac_handle, struct fips_params *param,
			    wma_fips_cb callback, void *context)
{
	void *wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		sme_err("wma handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wma_fips_request(wma_handle, param, callback, context);
}
#endif

QDF_STATUS sme_set_cts2self_for_p2p_go(mac_handle_t mac_handle)
{
	void *wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				"wma_handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	if (QDF_STATUS_SUCCESS !=
		wma_set_cts2self_for_p2p_go(wma_handle, true)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			"%s: Failed to set cts2self for p2p GO to firmware",
			__func__);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * sme_update_tx_fail_cnt_threshold() - update tx fail count Threshold
 * @mac_handle: Handle returned by mac_open
 * @session_id: Session ID on which tx fail count needs to be updated to FW
 * @tx_fail_count: Count for tx fail threshold after which FW will disconnect
 *
 * This function is used to set tx fail count threshold to firmware.
 * firmware will issue disocnnect with peer device once this threshold is
 * reached.
 *
 * Return: Return QDF_STATUS, otherwise appropriate failure code
 */
QDF_STATUS sme_update_tx_fail_cnt_threshold(mac_handle_t mac_handle,
					    uint8_t session_id,
					    uint32_t tx_fail_count)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct sme_tx_fail_cnt_threshold *tx_fail_cnt;
	struct scheduler_msg msg = {0};

	tx_fail_cnt = qdf_mem_malloc(sizeof(*tx_fail_cnt));
	if (!tx_fail_cnt)
		return QDF_STATUS_E_FAILURE;

	sme_debug("session_id: %d tx_fail_count: %d",
		  session_id, tx_fail_count);
	tx_fail_cnt->session_id = session_id;
	tx_fail_cnt->tx_fail_cnt_threshold = tx_fail_count;

	qdf_mem_zero(&msg, sizeof(struct scheduler_msg));
	msg.type = SIR_HAL_UPDATE_TX_FAIL_CNT_TH;
	msg.reserved = 0;
	msg.bodyptr = tx_fail_cnt;
	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &msg);

	if (!QDF_IS_STATUS_SUCCESS(status)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			FL("Not able to post Tx fail count message to WDA"));
		qdf_mem_free(tx_fail_cnt);
	}
	return status;
}

QDF_STATUS sme_set_lost_link_info_cb(mac_handle_t mac_handle,
				     lost_link_info_cb cb)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		mac->sme.lost_link_info_cb = cb;
		sme_release_global_lock(&mac->sme);
	}

	return status;
}

#ifdef FEATURE_WLAN_ESE
bool sme_roam_is_ese_assoc(struct csr_roam_info *roam_info)
{
	return roam_info->isESEAssoc;
}
#endif

bool sme_neighbor_roam_is11r_assoc(mac_handle_t mac_handle, uint8_t session_id)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	return csr_neighbor_roam_is11r_assoc(mac_ctx, session_id);
}

#ifdef WLAN_FEATURE_WOW_PULSE
/**
 * sme_set_wow_pulse() - set wow pulse info
 * @wow_pulse_set_info: wow_pulse_mode structure pointer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_set_wow_pulse(struct wow_pulse_mode *wow_pulse_set_info)
{
	struct scheduler_msg message = {0};
	QDF_STATUS status;
	struct wow_pulse_mode *wow_pulse_set_cmd;

	if (!wow_pulse_set_info) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			"%s: invalid wow_pulse_set_info pointer", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	wow_pulse_set_cmd = qdf_mem_malloc(sizeof(*wow_pulse_set_cmd));
	if (!wow_pulse_set_cmd)
		return QDF_STATUS_E_NOMEM;

	*wow_pulse_set_cmd = *wow_pulse_set_info;

	message.type = WMA_SET_WOW_PULSE_CMD;
	message.bodyptr = wow_pulse_set_cmd;
	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA,
					&message);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			"%s: Not able to post msg to WDA!",
			__func__);
		qdf_mem_free(wow_pulse_set_cmd);
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}
#endif

/**
 * sme_prepare_beacon_from_bss_descp() - prepares beacon frame by populating
 * different fields and IEs from bss descriptor.
 * @frame_buf: frame buffer to populate
 * @bss_descp: bss descriptor
 * @bssid: bssid of the beacon frame to populate
 * @ie_len: length of IE fields
 *
 * Return: None
 */
static void sme_prepare_beacon_from_bss_descp(uint8_t *frame_buf,
					      struct bss_description *bss_descp,
					      const tSirMacAddr bssid,
					      uint32_t ie_len)
{
	tDot11fBeacon1 *bcn_fixed;
	tpSirMacMgmtHdr mac_hdr = (tpSirMacMgmtHdr)frame_buf;

	/* populate mac header first to indicate beacon */
	mac_hdr->fc.protVer = SIR_MAC_PROTOCOL_VERSION;
	mac_hdr->fc.type = SIR_MAC_MGMT_FRAME;
	mac_hdr->fc.subType = SIR_MAC_MGMT_BEACON;
	qdf_mem_copy((uint8_t *) mac_hdr->da,
		     (uint8_t *) "\xFF\xFF\xFF\xFF\xFF\xFF",
		     sizeof(struct qdf_mac_addr));
	qdf_mem_copy((uint8_t *) mac_hdr->sa, bssid,
		     sizeof(struct qdf_mac_addr));
	qdf_mem_copy((uint8_t *) mac_hdr->bssId, bssid,
		     sizeof(struct qdf_mac_addr));

	/* now populate fixed params */
	bcn_fixed = (tDot11fBeacon1 *)(frame_buf + SIR_MAC_HDR_LEN_3A);
	/* populate timestamp */
	qdf_mem_copy(&bcn_fixed->TimeStamp.timestamp, &bss_descp->timeStamp,
			sizeof(bss_descp->timeStamp));
	/* populate beacon interval */
	bcn_fixed->BeaconInterval.interval = bss_descp->beaconInterval;
	/* populate capability */
	qdf_mem_copy(&bcn_fixed->Capabilities, &bss_descp->capabilityInfo,
			sizeof(bss_descp->capabilityInfo));

	/* copy IEs now */
	qdf_mem_copy(frame_buf + SIR_MAC_HDR_LEN_3A
		     + SIR_MAC_B_PR_SSID_OFFSET,
		     &bss_descp->ieFields, ie_len);
}

QDF_STATUS sme_get_rssi_snr_by_bssid(mac_handle_t mac_handle,
				     struct csr_roam_profile *profile,
				     const uint8_t *bssid,
				     int8_t *rssi, int8_t *snr,
				     uint8_t vdev_id)
{
	struct bss_description *bss_descp;
	struct scan_filter *scan_filter;
	struct scan_result_list *bss_list;
	tScanResultHandle result_handle = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	scan_filter = qdf_mem_malloc(sizeof(*scan_filter));
	if (!scan_filter) {
		status = QDF_STATUS_E_NOMEM;
		goto exit;
	}

	/* update filter to get scan result with just target BSSID */
	scan_filter->num_of_bssid = 1;
	qdf_mem_copy(scan_filter->bssid_list[0].bytes,
		     bssid, sizeof(struct qdf_mac_addr));
	scan_filter->ignore_auth_enc_type = true;

	status = csr_scan_get_result(mac_ctx, scan_filter, &result_handle,
				     false);
	qdf_mem_free(scan_filter);
	if (QDF_STATUS_SUCCESS != status) {
		sme_debug("parse_scan_result failed");
		goto exit;
	}

	bss_list = (struct scan_result_list *)result_handle;
	bss_descp = csr_get_fst_bssdescr_ptr(bss_list);
	if (!bss_descp) {
		sme_err("unable to fetch bss descriptor");
		status = QDF_STATUS_E_FAULT;
		goto exit;
	}

	sme_debug("snr: %d, rssi: %d, raw_rssi: %d",
		bss_descp->sinr, bss_descp->rssi, bss_descp->rssi_raw);

	if (rssi)
		*rssi = bss_descp->rssi;
	if (snr)
		*snr = bss_descp->sinr;

exit:
	if (result_handle)
		csr_scan_result_purge(mac_ctx, result_handle);

	return status;
}

QDF_STATUS sme_get_beacon_frm(mac_handle_t mac_handle,
			      struct csr_roam_profile *profile,
			      const tSirMacAddr bssid,
			      uint8_t **frame_buf, uint32_t *frame_len,
			      uint32_t *ch_freq, uint8_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tScanResultHandle result_handle = NULL;
	struct scan_filter *scan_filter;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct bss_description *bss_descp;
	struct scan_result_list *bss_list;
	uint32_t ie_len;

	scan_filter = qdf_mem_malloc(sizeof(*scan_filter));
	if (!scan_filter) {
		status = QDF_STATUS_E_NOMEM;
		goto exit;
	}
	status = csr_roam_get_scan_filter_from_profile(mac_ctx,
						       profile, scan_filter,
						       false, vdev_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("prepare_filter failed");
		status = QDF_STATUS_E_FAULT;
		qdf_mem_free(scan_filter);
		goto exit;
	}

	/* update filter to get scan result with just target BSSID */
	scan_filter->num_of_bssid = 1;
	qdf_mem_copy(scan_filter->bssid_list[0].bytes,
		     bssid, sizeof(struct qdf_mac_addr));

	status = csr_scan_get_result(mac_ctx, scan_filter, &result_handle,
				     false);
	qdf_mem_free(scan_filter);
	if (QDF_STATUS_SUCCESS != status) {
		sme_err("parse_scan_result failed");
		status = QDF_STATUS_E_FAULT;
		goto exit;
	}

	bss_list = (struct scan_result_list *)result_handle;
	bss_descp = csr_get_fst_bssdescr_ptr(bss_list);
	if (!bss_descp) {
		sme_err("unable to fetch bss descriptor");
		status = QDF_STATUS_E_FAULT;
		goto exit;
	}

	/**
	 * Length of BSS descriptor is without length of
	 * length itself and length of pointer that holds ieFields.
	 *
	 * struct bss_description
	 * +--------+---------------------------------+---------------+
	 * | length | other fields                    | pointer to IEs|
	 * +--------+---------------------------------+---------------+
	 *                                            ^
	 *                                            ieFields
	 */
	ie_len = bss_descp->length + sizeof(bss_descp->length)
		- (uint16_t)(offsetof(struct bss_description, ieFields[0]));
	sme_debug("found bss_descriptor ie_len: %d frequency %d",
		  ie_len, bss_descp->chan_freq);

	/* include mac header and fixed params along with IEs in frame */
	*frame_len = SIR_MAC_HDR_LEN_3A + SIR_MAC_B_PR_SSID_OFFSET + ie_len;
	*frame_buf = qdf_mem_malloc(*frame_len);
	if (!*frame_buf) {
		status = QDF_STATUS_E_NOMEM;
		goto exit;
	}

	sme_prepare_beacon_from_bss_descp(*frame_buf, bss_descp, bssid, ie_len);

	if (!*ch_freq)
		*ch_freq = bss_descp->chan_freq;
exit:
	if (result_handle)
		csr_scan_result_purge(mac_ctx, result_handle);

	return status;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
QDF_STATUS sme_roam_invoke_nud_fail(mac_handle_t mac_handle, uint8_t vdev_id)
{
	struct wma_roam_invoke_cmd *roam_invoke_params;
	struct scheduler_msg msg = {0};
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct mlme_roam_after_data_stall *vdev_roam_params;
	struct csr_roam_session *session;
	bool control_bitmap;

	if (!mac_ctx->mlme_cfg->gen.data_stall_recovery_fw_support) {
		sme_debug("FW does not support data stall recovery, aborting roam invoke");
		return QDF_STATUS_E_NOSUPPORT;
	}

	session = CSR_GET_SESSION(mac_ctx, vdev_id);
	if (!session) {
		sme_err("session %d not found", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}
	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;

	control_bitmap = mlme_get_operations_bitmap(mac_ctx->psoc, vdev_id);
	if (control_bitmap ||
	    !MLME_IS_ROAM_INITIALIZED(mac_ctx->psoc, vdev_id)) {
		sme_debug("ROAM: RSO Disabled internaly: vdev[%d] bitmap[0x%x]",
			  vdev_id, control_bitmap);
		sme_release_global_lock(&mac_ctx->sme);
		return QDF_STATUS_E_FAILURE;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);

	if (!vdev) {
		sme_err("vdev is NULL, aborting roam invoke");
		sme_release_global_lock(&mac_ctx->sme);
		return QDF_STATUS_E_NULL_VALUE;
	}

	vdev_roam_params = mlme_get_roam_invoke_params(vdev);

	if (!vdev_roam_params) {
		sme_err("Invalid vdev roam params, aborting roam invoke");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		sme_release_global_lock(&mac_ctx->sme);
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (vdev_roam_params->roam_invoke_in_progress) {
		sme_debug("Roaming already initiated by %d source",
			  vdev_roam_params->source);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		sme_release_global_lock(&mac_ctx->sme);
		return QDF_STATUS_E_BUSY;
	}

	roam_invoke_params = qdf_mem_malloc(sizeof(*roam_invoke_params));
	if (!roam_invoke_params) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		sme_release_global_lock(&mac_ctx->sme);
		return QDF_STATUS_E_NOMEM;
	}
	roam_invoke_params->vdev_id = vdev_id;
	/* Set forced roaming as true so that FW scans all ch, and connect */
	roam_invoke_params->forced_roaming = true;

	msg.type = eWNI_SME_ROAM_INVOKE;
	msg.reserved = 0;
	msg.bodyptr = roam_invoke_params;
	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Not able to post ROAM_INVOKE_CMD message to PE");
		qdf_mem_free(roam_invoke_params);
	} else {
		vdev_roam_params->roam_invoke_in_progress = true;
		vdev_roam_params->source = CONNECTION_MGR_INITIATED;
		session->roam_invoke_timer_info.mac = mac_ctx;
		session->roam_invoke_timer_info.vdev_id = vdev_id;
		qdf_mc_timer_start(&session->roam_invoke_timer,
				   QDF_ROAM_INVOKE_TIMEOUT);
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
	sme_release_global_lock(&mac_ctx->sme);

	return status;
}

QDF_STATUS sme_fast_reassoc(mac_handle_t mac_handle,
			    struct csr_roam_profile *profile,
			    const tSirMacAddr bssid, uint32_t ch_freq,
			    uint8_t vdev_id,
			    const tSirMacAddr connected_bssid)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	if (!mac)
		return QDF_STATUS_E_FAILURE;

	if (!CSR_IS_SESSION_VALID(mac, vdev_id)) {
		sme_err("Invalid vdev_id: %d", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	if (QDF_IS_STATUS_ERROR(sme_acquire_global_lock(&mac->sme)))
		return QDF_STATUS_E_FAILURE;

	status = csr_fast_reassoc(mac_handle, profile, bssid, ch_freq, vdev_id,
				  connected_bssid);

	sme_release_global_lock(&mac->sme);

	return status;
}

#endif

void sme_clear_sae_single_pmk_info(struct wlan_objmgr_psoc *psoc,
				   uint8_t session_id,
				   tPmkidCacheInfo *pmk_cache_info)
{
	struct wlan_crypto_pmksa *pmksa;
	struct wlan_objmgr_vdev *vdev;
	tPmkidCacheInfo pmk_to_del;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, session_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("Invalid vdev");
		return;
	}
	pmksa = wlan_crypto_get_pmksa(vdev, &pmk_cache_info->BSSID);
	if (!pmksa) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}
	qdf_mem_copy(&pmk_to_del.pmk, pmksa->pmk, pmksa->pmk_len);
	pmk_to_del.pmk_len = pmksa->pmk_len;
	csr_clear_sae_single_pmk(psoc, session_id, &pmk_to_del);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}

QDF_STATUS sme_set_del_pmkid_cache(struct wlan_objmgr_psoc *psoc,
				   uint8_t session_id,
				   tPmkidCacheInfo *pmk_cache_info,
				   bool is_add)
{
	struct wmi_unified_pmk_cache *pmk_cache;
	struct scheduler_msg msg;

	pmk_cache = qdf_mem_malloc(sizeof(*pmk_cache));
	if (!pmk_cache)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_zero(pmk_cache, sizeof(*pmk_cache));

	pmk_cache->vdev_id = session_id;

	if (!pmk_cache_info) {
		pmk_cache->is_flush_all = true;
		csr_clear_sae_single_pmk(psoc, session_id, NULL);
		goto send_flush_cmd;
	}

	if (!pmk_cache_info->ssid_len) {
		pmk_cache->cat_flag = WMI_PMK_CACHE_CAT_FLAG_BSSID;
		WMI_CHAR_ARRAY_TO_MAC_ADDR(pmk_cache_info->BSSID.bytes,
				&pmk_cache->bssid);
	} else {
		pmk_cache->cat_flag = WMI_PMK_CACHE_CAT_FLAG_SSID_CACHE_ID;
		pmk_cache->ssid.length = pmk_cache_info->ssid_len;
		qdf_mem_copy(pmk_cache->ssid.ssid,
			     pmk_cache_info->ssid,
			     pmk_cache->ssid.length);
	}
	pmk_cache->cache_id = (uint32_t) (pmk_cache_info->cache_id[0] << 8 |
					pmk_cache_info->cache_id[1]);

	if (is_add)
		pmk_cache->action_flag = WMI_PMK_CACHE_ACTION_FLAG_ADD_ENTRY;
	else
		pmk_cache->action_flag = WMI_PMK_CACHE_ACTION_FLAG_DEL_ENTRY;

	pmk_cache->pmkid_len = PMKID_LEN;
	qdf_mem_copy(pmk_cache->pmkid, pmk_cache_info->PMKID,
		     PMKID_LEN);

	pmk_cache->pmk_len = pmk_cache_info->pmk_len;
	qdf_mem_copy(pmk_cache->pmk, pmk_cache_info->pmk,
		     pmk_cache->pmk_len);

send_flush_cmd:
	msg.type = SIR_HAL_SET_DEL_PMKID_CACHE;
	msg.reserved = 0;
	msg.bodyptr = pmk_cache;
	if (QDF_STATUS_SUCCESS !=
	    scheduler_post_message(QDF_MODULE_ID_SME,
				   QDF_MODULE_ID_WMA,
				   QDF_MODULE_ID_WMA, &msg)) {
		sme_err("Not able to post message to WDA");
		if (pmk_cache) {
			qdf_mem_zero(pmk_cache, sizeof(*pmk_cache));
			qdf_mem_free(pmk_cache);
		}
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/* ARP DEBUG STATS */

/**
 * sme_set_nud_debug_stats() - sme api to set nud debug stats
 * @mac_handle: Opaque handle to the global MAC context
 * @set_stats_param: pointer to set stats param
 *
 * Return: Return QDF_STATUS.
 */
QDF_STATUS sme_set_nud_debug_stats(mac_handle_t mac_handle,
				   struct set_arp_stats_params
				   *set_stats_param)
{
	struct set_arp_stats_params *arp_set_param;
	struct scheduler_msg msg;

	arp_set_param = qdf_mem_malloc(sizeof(*arp_set_param));
	if (!arp_set_param)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_copy(arp_set_param, set_stats_param, sizeof(*arp_set_param));

	msg.type = WMA_SET_ARP_STATS_REQ;
	msg.reserved = 0;
	msg.bodyptr = arp_set_param;

	if (QDF_STATUS_SUCCESS !=
	    scheduler_post_message(QDF_MODULE_ID_SME,
				   QDF_MODULE_ID_WMA,
				   QDF_MODULE_ID_WMA, &msg)) {
		sme_err("Not able to post message to WDA");
		qdf_mem_free(arp_set_param);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * sme_get_nud_debug_stats() - sme api to get nud debug stats
 * @mac_handle: Opaque handle to the global MAC context
 * @get_stats_param: pointer to set stats param
 *
 * Return: Return QDF_STATUS.
 */
QDF_STATUS sme_get_nud_debug_stats(mac_handle_t mac_handle,
				   struct get_arp_stats_params
				   *get_stats_param)
{
	struct get_arp_stats_params *arp_get_param;
	struct scheduler_msg msg;

	arp_get_param = qdf_mem_malloc(sizeof(*arp_get_param));
	if (!arp_get_param)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_copy(arp_get_param, get_stats_param, sizeof(*arp_get_param));

	msg.type = WMA_GET_ARP_STATS_REQ;
	msg.reserved = 0;
	msg.bodyptr = arp_get_param;

	if (QDF_STATUS_SUCCESS !=
	    scheduler_post_message(QDF_MODULE_ID_SME,
				   QDF_MODULE_ID_WMA,
				   QDF_MODULE_ID_WMA, &msg)) {
		sme_err("Not able to post message to WDA");
		qdf_mem_free(arp_get_param);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sme_set_peer_param(uint8_t *peer_addr, uint32_t param_id,
			      uint32_t param_value, uint32_t vdev_id)
{
	void *wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		sme_err("wma handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wma_set_peer_param(wma_handle, peer_addr, param_id,
				  param_value, vdev_id);
}

QDF_STATUS sme_register_set_connection_info_cb(mac_handle_t mac_handle,
				bool (*set_connection_info_cb)(bool),
				bool (*get_connection_info_cb)(uint8_t *session_id,
				enum scan_reject_states *reason))
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		mac->sme.set_connection_info_cb = set_connection_info_cb;
		mac->sme.get_connection_info_cb = get_connection_info_cb;
		sme_release_global_lock(&mac->sme);
	}
	return status;
}

QDF_STATUS sme_rso_cmd_status_cb(mac_handle_t mac_handle,
				 rso_cmd_status_cb cb)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	mac->sme.rso_cmd_status_cb = cb;
	sme_debug("Registered RSO command status callback");
	return status;
}

QDF_STATUS sme_set_dbs_scan_selection_config(mac_handle_t mac_handle,
		struct wmi_dbs_scan_sel_params *params)
{
	struct scheduler_msg message = {0};
	QDF_STATUS status;
	struct wmi_dbs_scan_sel_params *dbs_scan_params;
	uint32_t i;

	if (0 == params->num_clients) {
		sme_err("Num of clients is 0");
		return QDF_STATUS_E_FAILURE;
	}

	dbs_scan_params = qdf_mem_malloc(sizeof(*dbs_scan_params));
	if (!dbs_scan_params)
		return QDF_STATUS_E_NOMEM;

	dbs_scan_params->num_clients = params->num_clients;
	dbs_scan_params->pdev_id = params->pdev_id;
	for (i = 0; i < params->num_clients; i++) {
		dbs_scan_params->module_id[i] = params->module_id[i];
		dbs_scan_params->num_dbs_scans[i] = params->num_dbs_scans[i];
		dbs_scan_params->num_non_dbs_scans[i] =
			params->num_non_dbs_scans[i];
	}
	message.type = WMA_SET_DBS_SCAN_SEL_CONF_PARAMS;
	message.bodyptr = dbs_scan_params;
	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &message);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Not able to post msg to WMA!");
		qdf_mem_free(dbs_scan_params);
	}

	return status;
}

QDF_STATUS sme_get_rcpi(mac_handle_t mac_handle, struct sme_rcpi_req *rcpi)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg msg = {0};
	struct sme_rcpi_req *rcpi_req;

	rcpi_req = qdf_mem_malloc(sizeof(*rcpi_req));
	if (!rcpi_req)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_copy(rcpi_req, rcpi, sizeof(*rcpi_req));

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		msg.bodyptr = rcpi_req;
		msg.type = WMA_GET_RCPI_REQ;
		status = scheduler_post_message(QDF_MODULE_ID_SME,
						QDF_MODULE_ID_WMA,
						QDF_MODULE_ID_WMA, &msg);
		sme_release_global_lock(&mac->sme);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				  FL("post get rcpi req failed"));
			status = QDF_STATUS_E_FAILURE;
			qdf_mem_free(rcpi_req);
		}
	} else {
		qdf_mem_free(rcpi_req);
	}

	return status;
}

void sme_store_pdev(mac_handle_t mac_handle, struct wlan_objmgr_pdev *pdev)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	void *wma_handle;
	QDF_STATUS status;

	status = wlan_objmgr_pdev_try_get_ref(pdev, WLAN_LEGACY_MAC_ID);
	if (QDF_STATUS_SUCCESS != status) {
		mac_ctx->pdev = NULL;
		return;
	}
	mac_ctx->pdev = pdev;
	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				FL("wma handle is NULL"));
		return;
	}
	wma_store_pdev(wma_handle, pdev);
	pdev->pdev_nif.pdev_fw_caps |= SUPPORTED_CRYPTO_CAPS;
}

QDF_STATUS sme_register_tx_queue_cb(mac_handle_t mac_handle,
				    tx_queue_cb tx_queue_cb)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		mac->sme.tx_queue_cb = tx_queue_cb;
		sme_release_global_lock(&mac->sme);
		sme_debug("Tx queue callback set");
	}

	return status;
}

QDF_STATUS sme_deregister_tx_queue_cb(mac_handle_t mac_handle)
{
	return sme_register_tx_queue_cb(mac_handle, NULL);
}

#ifdef WLAN_SUPPORT_TWT

QDF_STATUS sme_test_config_twt_setup(struct wmi_twt_add_dialog_param *params)
{
	t_wma_handle *wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "wma handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wma_twt_process_add_dialog(wma_handle, params);
}

QDF_STATUS
sme_test_config_twt_terminate(struct wmi_twt_del_dialog_param *params)
{
	t_wma_handle *wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "wma handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wma_twt_process_del_dialog(wma_handle, params);
}

QDF_STATUS sme_clear_twt_complete_cb(mac_handle_t mac_handle)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		mac->sme.twt_add_dialog_cb = NULL;
		mac->sme.twt_del_dialog_cb = NULL;
		mac->sme.twt_pause_dialog_cb = NULL;
		mac->sme.twt_resume_dialog_cb = NULL;
		mac->sme.twt_notify_cb = NULL;
		mac->sme.twt_nudge_dialog_cb = NULL;
		mac->sme.twt_ack_comp_cb = NULL;
		sme_release_global_lock(&mac->sme);

		sme_debug("TWT: callbacks Initialized");
	}

	return status;
}

QDF_STATUS sme_register_twt_callbacks(mac_handle_t mac_handle,
				      struct twt_callbacks *twt_cb)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		mac->sme.twt_enable_cb = twt_cb->twt_enable_cb;
		mac->sme.twt_add_dialog_cb = twt_cb->twt_add_dialog_cb;
		mac->sme.twt_del_dialog_cb = twt_cb->twt_del_dialog_cb;
		mac->sme.twt_pause_dialog_cb = twt_cb->twt_pause_dialog_cb;
		mac->sme.twt_resume_dialog_cb = twt_cb->twt_resume_dialog_cb;
		mac->sme.twt_disable_cb = twt_cb->twt_disable_cb;
		mac->sme.twt_notify_cb = twt_cb->twt_notify_cb;
		mac->sme.twt_nudge_dialog_cb = twt_cb->twt_nudge_dialog_cb;
		mac->sme.twt_ack_comp_cb = twt_cb->twt_ack_comp_cb;
		sme_release_global_lock(&mac->sme);
		sme_debug("TWT: callbacks registered");
	}

	return status;
}

QDF_STATUS sme_add_dialog_cmd(mac_handle_t mac_handle,
			      twt_add_dialog_cb twt_add_dialog_cb,
			      struct wmi_twt_add_dialog_param *twt_params,
			      void *context)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg twt_msg = {0};
	bool is_twt_cmd_in_progress, is_twt_notify_in_progress;
	bool usr_cfg_ps_enable;
	QDF_STATUS status;
	void *wma_handle;
	struct wmi_twt_add_dialog_param *cmd_params;
	enum wlan_twt_commands active_cmd = WLAN_TWT_NONE;

	SME_ENTER();

	usr_cfg_ps_enable = mlme_get_user_ps(mac->psoc, twt_params->vdev_id);
	if (!usr_cfg_ps_enable) {
		sme_debug("Power save mode disable");
		return QDF_STATUS_E_INVAL;
	}

	is_twt_notify_in_progress = mlme_is_twt_notify_in_progress(
			mac->psoc, twt_params->vdev_id);

	if (is_twt_notify_in_progress) {
		sme_debug("Waiting for TWT Notify");
		return QDF_STATUS_E_BUSY;
	}

	is_twt_cmd_in_progress = mlme_twt_is_command_in_progress(
			mac->psoc,
			(struct qdf_mac_addr *)twt_params->peer_macaddr,
			twt_params->dialog_id, WLAN_TWT_ANY, &active_cmd);
	if (is_twt_cmd_in_progress) {
		sme_debug("Already TWT command:%d is in progress", active_cmd);
		return QDF_STATUS_E_PENDING;
	}

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		sme_err("wma_handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	/*bodyptr should be freeable*/
	cmd_params = qdf_mem_malloc(sizeof(*cmd_params));
	if (!cmd_params)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_copy(cmd_params, twt_params, sizeof(*cmd_params));

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("failed to register add dialog callback");
		qdf_mem_free(cmd_params);
		return status;
	}

	/*
	 * Add the dialog id to TWT context to drop back to back
	 * commands
	 */
	mlme_add_twt_session(mac->psoc,
			     (struct qdf_mac_addr *)twt_params->peer_macaddr,
			     twt_params->dialog_id);

	mlme_set_twt_command_in_progress(mac->psoc,
				(struct qdf_mac_addr *)twt_params->peer_macaddr,
				twt_params->dialog_id, WLAN_TWT_SETUP);

	/* Serialize the req through MC thread */
	mac->sme.twt_add_dialog_cb = twt_add_dialog_cb;
	mac->sme.twt_ack_context_cb = context;
	twt_msg.bodyptr = cmd_params;
	twt_msg.type = WMA_TWT_ADD_DIALOG_REQUEST;
	sme_release_global_lock(&mac->sme);

	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &twt_msg);

	if (QDF_IS_STATUS_ERROR(status)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Post twt add dialog msg fail"));
		mlme_set_twt_command_in_progress(mac->psoc,
				(struct qdf_mac_addr *)twt_params->peer_macaddr,
				twt_params->dialog_id, WLAN_TWT_NONE);
		mlme_init_twt_context(mac->psoc,
				(struct qdf_mac_addr *)twt_params->peer_macaddr,
				twt_params->dialog_id);
		qdf_mem_free(cmd_params);
	}

	SME_EXIT();
	return status;
}

QDF_STATUS sme_del_dialog_cmd(mac_handle_t mac_handle,
			      twt_del_dialog_cb del_dialog_cb,
			      struct wmi_twt_del_dialog_param *twt_params,
			      void *context)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg twt_msg = {0};
	bool is_twt_cmd_in_progress;
	QDF_STATUS status;
	void *wma_handle;
	struct wmi_twt_del_dialog_param *cmd_params;
	enum wlan_twt_commands active_cmd = WLAN_TWT_NONE;

	SME_ENTER();

	is_twt_cmd_in_progress =
		mlme_twt_is_command_in_progress(
			mac->psoc,
			(struct qdf_mac_addr *)twt_params->peer_macaddr,
			twt_params->dialog_id, WLAN_TWT_SETUP, &active_cmd) ||
		mlme_twt_is_command_in_progress(
			mac->psoc,
			(struct qdf_mac_addr *)twt_params->peer_macaddr,
			twt_params->dialog_id, WLAN_TWT_TERMINATE,
			&active_cmd);
	if (is_twt_cmd_in_progress) {
		sme_debug("Already TWT command:%d is in progress", active_cmd);
		return QDF_STATUS_E_PENDING;
	}

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		sme_err("wma_handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	/*bodyptr should be freeable*/
	cmd_params = qdf_mem_malloc(sizeof(*cmd_params));
	if (!cmd_params)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_copy(cmd_params, twt_params, sizeof(*cmd_params));

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Failed to acquire SME global lock");
		qdf_mem_free(cmd_params);
		return status;
	}

	mlme_set_twt_command_in_progress(mac->psoc,
				(struct qdf_mac_addr *)twt_params->peer_macaddr,
				twt_params->dialog_id, WLAN_TWT_TERMINATE);

	/* Serialize the req through MC thread */
	mac->sme.twt_del_dialog_cb = del_dialog_cb;
	mac->sme.twt_ack_context_cb = context;
	twt_msg.bodyptr = cmd_params;
	twt_msg.type = WMA_TWT_DEL_DIALOG_REQUEST;
	sme_release_global_lock(&mac->sme);

	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &twt_msg);

	if (QDF_IS_STATUS_ERROR(status)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Post twt del dialog msg fail"));
		mlme_set_twt_command_in_progress(
				mac->psoc,
				(struct qdf_mac_addr *)twt_params->peer_macaddr,
				twt_params->dialog_id, WLAN_TWT_NONE);
		qdf_mem_free(cmd_params);
	}

	SME_EXIT();
	return status;
}

QDF_STATUS sme_sap_del_dialog_cmd(mac_handle_t mac_handle,
				  twt_del_dialog_cb del_dialog_cb,
				  struct wmi_twt_del_dialog_param *twt_params)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg twt_msg = {0};
	bool is_twt_cmd_in_progress;
	QDF_STATUS status;
	void *wma_handle;
	struct wmi_twt_del_dialog_param *cmd_params;

	SME_ENTER();

	is_twt_cmd_in_progress =
		sme_sap_twt_is_command_in_progress(mac->psoc,
			twt_params->vdev_id,
			(struct qdf_mac_addr *)twt_params->peer_macaddr,
			twt_params->dialog_id, WLAN_TWT_TERMINATE);

	if (is_twt_cmd_in_progress) {
		sme_debug("Already TWT teardown command is in progress");
		return QDF_STATUS_E_PENDING;
	}

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle)
		return QDF_STATUS_E_FAILURE;

	/* bodyptr should be freeable */
	cmd_params = qdf_mem_malloc(sizeof(*cmd_params));
	if (!cmd_params)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_copy(cmd_params, twt_params, sizeof(*cmd_params));

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(cmd_params);
		sme_err("Failed to acquire SME global lock");
		return status;
	}

	/*
	 * Add the dialog id to TWT context to drop back to back
	 * commands
	 */
	sme_sap_add_twt_session(mac->psoc, twt_params->vdev_id,
			       (struct qdf_mac_addr *)twt_params->peer_macaddr,
			       twt_params->dialog_id);

	sme_sap_set_twt_command_in_progress(mac->psoc, twt_params->vdev_id,
				(struct qdf_mac_addr *)twt_params->peer_macaddr,
				twt_params->dialog_id, WLAN_TWT_TERMINATE);

	/* Serialize the req through MC thread */
	mac->sme.twt_del_dialog_cb = del_dialog_cb;
	twt_msg.bodyptr = cmd_params;
	twt_msg.type = WMA_TWT_DEL_DIALOG_REQUEST;
	sme_release_global_lock(&mac->sme);

	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &twt_msg);

	if (QDF_IS_STATUS_ERROR(status)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Post twt del dialog msg fail"));
		sme_sap_set_twt_command_in_progress(mac->psoc,
				twt_params->vdev_id,
				(struct qdf_mac_addr *)twt_params->peer_macaddr,
				twt_params->dialog_id, WLAN_TWT_NONE);
		sme_sap_init_twt_context(mac->psoc,
				twt_params->vdev_id,
				(struct qdf_mac_addr *)twt_params->peer_macaddr,
				twt_params->dialog_id);
		qdf_mem_free(cmd_params);
	}

	SME_EXIT();
	return status;
}

QDF_STATUS
sme_pause_dialog_cmd(mac_handle_t mac_handle,
		     struct wmi_twt_pause_dialog_cmd_param *twt_params,
		     void *context)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct wmi_twt_pause_dialog_cmd_param *cmd_params;
	struct scheduler_msg twt_msg = {0};
	bool is_twt_cmd_in_progress;
	QDF_STATUS status;
	void *wma_handle;
	enum wlan_twt_commands active_cmd = WLAN_TWT_NONE;

	SME_ENTER();

	is_twt_cmd_in_progress = mlme_twt_is_command_in_progress(
			mac->psoc,
			(struct qdf_mac_addr *)twt_params->peer_macaddr,
			twt_params->dialog_id, WLAN_TWT_ANY, &active_cmd);
	if (is_twt_cmd_in_progress) {
		sme_debug("Already TWT command:%d is in progress", active_cmd);
		return QDF_STATUS_E_PENDING;
	}

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		sme_err("wma_handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	/*bodyptr should be freeable*/
	cmd_params = qdf_mem_malloc(sizeof(*cmd_params));
	if (!cmd_params)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_copy(cmd_params, twt_params, sizeof(*cmd_params));

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("failed to register pause dialog callback");
		qdf_mem_free(cmd_params);
		return status;
	}

	mlme_set_twt_command_in_progress(mac->psoc,
				(struct qdf_mac_addr *)twt_params->peer_macaddr,
				twt_params->dialog_id, WLAN_TWT_SUSPEND);

	/* Serialize the req through MC thread */
	mac->sme.twt_ack_context_cb = context;
	twt_msg.bodyptr = cmd_params;
	twt_msg.type = WMA_TWT_PAUSE_DIALOG_REQUEST;
	sme_release_global_lock(&mac->sme);

	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &twt_msg);

	if (QDF_IS_STATUS_ERROR(status)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Post twt pause dialog msg fail"));
		mlme_set_twt_command_in_progress(
				mac->psoc,
				(struct qdf_mac_addr *)twt_params->peer_macaddr,
				twt_params->dialog_id, WLAN_TWT_NONE);
		qdf_mem_free(cmd_params);
	}

	SME_EXIT();
	return status;
}

QDF_STATUS
sme_nudge_dialog_cmd(mac_handle_t mac_handle,
		     struct wmi_twt_nudge_dialog_cmd_param *twt_params,
		     void *context)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct wmi_twt_nudge_dialog_cmd_param *cmd_params;
	struct scheduler_msg twt_msg = {0};
	bool is_twt_cmd_in_progress;
	QDF_STATUS status;
	void *wma_handle;
	enum wlan_twt_commands active_cmd = WLAN_TWT_NONE;

	SME_ENTER();

	is_twt_cmd_in_progress = mlme_twt_is_command_in_progress(
			mac->psoc,
			(struct qdf_mac_addr *)twt_params->peer_macaddr,
			twt_params->dialog_id, WLAN_TWT_ANY, &active_cmd);
	if (is_twt_cmd_in_progress) {
		sme_debug("Already TWT command:%d is in progress", active_cmd);
		return QDF_STATUS_E_PENDING;
	}

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		sme_err("wma_handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	/*bodyptr should be freeable*/
	cmd_params = qdf_mem_malloc(sizeof(*cmd_params));
	if (!cmd_params)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_copy(cmd_params, twt_params, sizeof(*cmd_params));

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("failed to register nudge dialog callback");
		qdf_mem_free(cmd_params);
		return status;
	}

	mlme_set_twt_command_in_progress(mac->psoc,
				(struct qdf_mac_addr *)twt_params->peer_macaddr,
				twt_params->dialog_id, WLAN_TWT_NUDGE);

	/* Serialize the req through MC thread */
	mac->sme.twt_ack_context_cb = context;
	twt_msg.bodyptr = cmd_params;
	twt_msg.type = WMA_TWT_NUDGE_DIALOG_REQUEST;
	sme_release_global_lock(&mac->sme);

	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &twt_msg);

	if (QDF_IS_STATUS_ERROR(status)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Post twt nudge dialog msg fail"));
		mlme_set_twt_command_in_progress(
			mac->psoc,
			(struct qdf_mac_addr *)twt_params->peer_macaddr,
			twt_params->dialog_id, WLAN_TWT_NONE);
		qdf_mem_free(cmd_params);
	}

	SME_EXIT();
	return status;
}

QDF_STATUS
sme_resume_dialog_cmd(mac_handle_t mac_handle,
		      struct wmi_twt_resume_dialog_cmd_param *twt_params,
		      void *context)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct wmi_twt_resume_dialog_cmd_param *cmd_params;
	struct scheduler_msg twt_msg = {0};
	bool is_twt_cmd_in_progress;
	QDF_STATUS status;
	void *wma_handle;
	enum wlan_twt_commands active_cmd = WLAN_TWT_NONE;

	SME_ENTER();

	is_twt_cmd_in_progress = mlme_twt_is_command_in_progress(
			mac->psoc,
			(struct qdf_mac_addr *)twt_params->peer_macaddr,
			twt_params->dialog_id, WLAN_TWT_ANY, &active_cmd);
	if (is_twt_cmd_in_progress) {
		sme_debug("Already TWT command:%d is in progress", active_cmd);
		return QDF_STATUS_E_PENDING;
	}

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		sme_err("wma_handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	/*bodyptr should be freeable*/
	cmd_params = qdf_mem_malloc(sizeof(*cmd_params));
	if (!cmd_params)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_copy(cmd_params, twt_params, sizeof(*cmd_params));

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("failed to register resume dialog callback");
		qdf_mem_free(cmd_params);
		return status;
	}

	mlme_set_twt_command_in_progress(mac->psoc,
				(struct qdf_mac_addr *)twt_params->peer_macaddr,
				twt_params->dialog_id, WLAN_TWT_RESUME);

	/* Serialize the req through MC thread */
	mac->sme.twt_ack_context_cb = context;
	twt_msg.bodyptr = cmd_params;
	twt_msg.type = WMA_TWT_RESUME_DIALOG_REQUEST;
	sme_release_global_lock(&mac->sme);

	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &twt_msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Post twt resume dialog msg fail");
		mlme_set_twt_command_in_progress(
				mac->psoc,
				(struct qdf_mac_addr *)twt_params->peer_macaddr,
				twt_params->dialog_id, WLAN_TWT_NONE);
		qdf_mem_free(cmd_params);
	}

	SME_EXIT();
	return status;
}
#endif

QDF_STATUS sme_set_smps_cfg(uint32_t vdev_id, uint32_t param_id,
						uint32_t param_val)
{
	return wma_configure_smps_params(vdev_id, param_id, param_val);
}

QDF_STATUS sme_set_reorder_timeout(mac_handle_t mac_handle,
				   struct sir_set_rx_reorder_timeout_val *req)
{
	QDF_STATUS status;
	tp_wma_handle wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	status = wma_set_rx_reorder_timeout_val(wma_handle, req);

	return status;
}

QDF_STATUS sme_set_rx_set_blocksize(mac_handle_t mac_handle,
				    struct sir_peer_set_rx_blocksize *req)
{
	QDF_STATUS status;
	tp_wma_handle wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	status = wma_set_rx_blocksize(wma_handle, req);

	return status;
}

int sme_cli_set_command(int vdev_id, int param_id, int sval, int vpdev)
{
	return wma_cli_set_command(vdev_id, param_id, sval, vpdev);
}

int sme_set_enable_mem_deep_sleep(mac_handle_t mac_handle, int vdev_id)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	return wma_cli_set_command(vdev_id, WMI_PDEV_PARAM_HYST_EN,
				   mac_ctx->mlme_cfg->gen.memory_deep_sleep,
				   PDEV_CMD);
}

int sme_set_cck_tx_fir_override(mac_handle_t mac_handle, int vdev_id)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	return wma_cli_set_command(vdev_id,
				   WMI_PDEV_PARAM_ENABLE_CCK_TXFIR_OVERRIDE,
				   mac_ctx->mlme_cfg->gen.cck_tx_fir_override,
				   PDEV_CMD);
}

QDF_STATUS sme_set_bt_activity_info_cb(mac_handle_t mac_handle,
				       bt_activity_info_cb cb)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		mac->sme.bt_activity_info_cb = cb;
		sme_release_global_lock(&mac->sme);
		sme_debug("bt activity info callback set");
	}

	return status;
}

QDF_STATUS sme_get_chain_rssi(mac_handle_t mac_handle,
			      struct get_chain_rssi_req_params *input,
			      get_chain_rssi_callback callback,
			      void *context)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	tp_wma_handle wma_handle;

	SME_ENTER();

	if (!input) {
		sme_err("Invalid req params");
		return QDF_STATUS_E_INVAL;
	}

	mac_ctx->sme.get_chain_rssi_cb = callback;
	mac_ctx->sme.get_chain_rssi_context = context;
	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	wma_get_chain_rssi(wma_handle, input);

	SME_EXIT();
	return status;
}

QDF_STATUS sme_process_msg_callback(struct mac_context *mac,
				    struct scheduler_msg *msg)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!msg) {
		sme_err("Empty message for SME Msg callback");
		return status;
	}
	status = sme_process_msg(mac, msg);
	return status;
}

void sme_display_disconnect_stats(mac_handle_t mac_handle, uint8_t session_id)
{
	struct csr_roam_session *session;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("%s Invalid session id: %d", __func__, session_id);
		return;
	}

	session = CSR_GET_SESSION(mac_ctx, session_id);
	if (!session) {
		sme_err("%s Failed to get session for id: %d",
			__func__, session_id);
		return;
	}

	sme_nofl_info("Total No. of Disconnections: %d",
		      session->disconnect_stats.disconnection_cnt);

	sme_nofl_info("No. of Diconnects Triggered by Application: %d",
		      session->disconnect_stats.disconnection_by_app);

	sme_nofl_info("No. of Disassoc Sent by Peer: %d",
		      session->disconnect_stats.disassoc_by_peer);

	sme_nofl_info("No. of Deauth Sent by Peer: %d",
		      session->disconnect_stats.deauth_by_peer);

	sme_nofl_info("No. of Disconnections due to Beacon Miss: %d",
		      session->disconnect_stats.bmiss);

	sme_nofl_info("No. of Disconnections due to Peer Kickout: %d",
		      session->disconnect_stats.peer_kickout);
}

#ifdef FEATURE_WLAN_DYNAMIC_CVM
 /**
 * sme_set_vc_mode_config() - Set voltage corner config to FW
 * @bitmap:	Bitmap that referes to voltage corner config with
 * different phymode and bw configuration
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_set_vc_mode_config(uint32_t vc_bitmap)
{
	void *wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				"wma_handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	if (QDF_STATUS_SUCCESS !=
		wma_set_vc_mode_config(wma_handle, vc_bitmap)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			"%s: Failed to set Voltage Control config to FW",
			__func__);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * sme_set_bmiss_bcnt() - set bmiss config parameters
 * @vdev_id: virtual device for the command
 * @first_cnt: bmiss first value
 * @final_cnt: bmiss final value
 *
 * Return: QDF_STATUS_SUCCESS or non-zero on failure
 */
QDF_STATUS sme_set_bmiss_bcnt(uint32_t vdev_id, uint32_t first_cnt,
		uint32_t final_cnt)
{
	return wma_config_bmiss_bcnt_params(vdev_id, first_cnt, final_cnt);
}

QDF_STATUS sme_send_limit_off_channel_params(mac_handle_t mac_handle,
					     uint8_t vdev_id,
					     bool is_tos_active,
					     uint32_t max_off_chan_time,
					     uint32_t rest_time,
					     bool skip_dfs_chan)
{
	struct sir_limit_off_chan *cmd;
	struct scheduler_msg msg = {0};

	cmd = qdf_mem_malloc(sizeof(*cmd));
	if (!cmd)
		return QDF_STATUS_E_NOMEM;

	cmd->vdev_id = vdev_id;
	cmd->is_tos_active = is_tos_active;
	cmd->max_off_chan_time = max_off_chan_time;
	cmd->rest_time = rest_time;
	cmd->skip_dfs_chans = skip_dfs_chan;

	msg.type = WMA_SET_LIMIT_OFF_CHAN;
	msg.reserved = 0;
	msg.bodyptr = cmd;

	if (!QDF_IS_STATUS_SUCCESS(scheduler_post_message(QDF_MODULE_ID_SME,
							  QDF_MODULE_ID_WMA,
							  QDF_MODULE_ID_WMA,
							  &msg))) {
		sme_err("Not able to post WMA_SET_LIMIT_OFF_CHAN to WMA");
		qdf_mem_free(cmd);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

uint32_t sme_unpack_rsn_ie(mac_handle_t mac_handle, uint8_t *buf,
			   uint8_t buf_len, tDot11fIERSN *rsn_ie,
			   bool append_ie)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	return dot11f_unpack_ie_rsn(mac_ctx, buf, buf_len, rsn_ie, append_ie);
}

void sme_add_qcn_ie(mac_handle_t mac_handle, uint8_t *ie_data,
		    uint16_t *ie_len)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	static const uint8_t qcn_ie[] = {WLAN_ELEMID_VENDOR, 8,
					 0x8C, 0xFD, 0xF0, 0x1,
					 QCN_IE_VERSION_SUBATTR_ID,
					 QCN_IE_VERSION_SUBATTR_DATA_LEN,
					 QCN_IE_VERSION_SUPPORTED,
					 QCN_IE_SUBVERSION_SUPPORTED};

	if (!mac_ctx->mlme_cfg->sta.qcn_ie_support) {
		sme_debug("QCN IE is not supported");
		return;
	}

	if (((*ie_len) + sizeof(qcn_ie)) > MAX_DEFAULT_SCAN_IE_LEN) {
		sme_err("IE buffer not enough for QCN IE");
		return;
	}

	qdf_mem_copy(ie_data + (*ie_len), qcn_ie, sizeof(qcn_ie));
	(*ie_len) += sizeof(qcn_ie);
}

#ifdef FEATURE_BSS_TRANSITION
/**
 * sme_get_status_for_candidate() - Get bss transition status for candidate
 * @mac_handle: Opaque handle to the global MAC context
 * @conn_bss_desc: connected bss descriptor
 * @bss_desc: candidate bss descriptor
 * @info: candiadate bss information
 * @trans_reason: transition reason code
 * @is_bt_in_progress: bt activity indicator
 *
 * Return : true if candidate is rejected and reject reason is filled
 * @info->status. Otherwise returns false.
 */
static bool sme_get_status_for_candidate(mac_handle_t mac_handle,
					 struct bss_description *conn_bss_desc,
					 struct bss_description *bss_desc,
					 struct bss_candidate_info *info,
					 uint8_t trans_reason,
					 bool is_bt_in_progress)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct wlan_mlme_mbo *mbo_cfg;
	int8_t current_rssi_mcc_thres;
	uint32_t bss_chan_freq, conn_bss_chan_freq;
	bool bss_chan_safe, conn_bss_chan_safe;

	if (!(mac_ctx->mlme_cfg)) {
		pe_err("mlme cfg is NULL");
		return false;
	}
	mbo_cfg = &mac_ctx->mlme_cfg->mbo_cfg;

	/*
	 * Low RSSI based rejection
	 * If candidate rssi is less than mbo_candidate_rssi_thres and connected
	 * bss rssi is greater than mbo_current_rssi_thres, then reject the
	 * candidate with MBO reason code 4.
	 */
	if ((bss_desc->rssi < mbo_cfg->mbo_candidate_rssi_thres) &&
	    (conn_bss_desc->rssi > mbo_cfg->mbo_current_rssi_thres)) {
		sme_err("Candidate BSS "QDF_MAC_ADDR_FMT" has LOW RSSI(%d), hence reject",
			QDF_MAC_ADDR_REF(bss_desc->bssId), bss_desc->rssi);
		info->status = QCA_STATUS_REJECT_LOW_RSSI;
		return true;
	}

	if (trans_reason == MBO_TRANSITION_REASON_LOAD_BALANCING ||
	    trans_reason == MBO_TRANSITION_REASON_TRANSITIONING_TO_PREMIUM_AP) {
		bss_chan_freq = bss_desc->chan_freq;
		conn_bss_chan_freq = conn_bss_desc->chan_freq;
		/*
		 * MCC rejection
		 * If moving to candidate's channel will result in MCC scenario
		 * and the rssi of connected bss is greater than
		 * mbo_current_rssi_mss_thres, then reject the candidate with
		 * MBO reason code 3.
		 */
		current_rssi_mcc_thres = mbo_cfg->mbo_current_rssi_mcc_thres;
		if ((conn_bss_desc->rssi > current_rssi_mcc_thres) &&
		    csr_is_mcc_channel(mac_ctx, bss_chan_freq)) {
			sme_err("Candidate BSS "QDF_MAC_ADDR_FMT" causes MCC, hence reject",
				QDF_MAC_ADDR_REF(bss_desc->bssId));
			info->status =
				QCA_STATUS_REJECT_INSUFFICIENT_QOS_CAPACITY;
			return true;
		}

		/*
		 * BT coex rejection
		 * If AP is trying to move the client from 5G to 2.4G and moving
		 * to 2.4G will result in BT coex and candidate channel rssi is
		 * less than mbo_candidate_rssi_btc_thres, then reject the
		 * candidate with MBO reason code 2.
		 */
		if (WLAN_REG_IS_5GHZ_CH_FREQ(conn_bss_chan_freq) &&
		    WLAN_REG_IS_24GHZ_CH_FREQ(bss_chan_freq) &&
		    is_bt_in_progress &&
		    (bss_desc->rssi < mbo_cfg->mbo_candidate_rssi_btc_thres)) {
			sme_err("Candidate BSS "QDF_MAC_ADDR_FMT" causes BT coex, hence reject",
				QDF_MAC_ADDR_REF(bss_desc->bssId));
			info->status =
				QCA_STATUS_REJECT_EXCESSIVE_DELAY_EXPECTED;
			return true;
		}

		/*
		 * LTE coex rejection
		 * If moving to candidate's channel can cause LTE coex, then
		 * reject the candidate with MBO reason code 5.
		 */
		conn_bss_chan_safe = policy_mgr_is_safe_channel(
			mac_ctx->psoc, conn_bss_chan_freq);
		bss_chan_safe = policy_mgr_is_safe_channel(
			mac_ctx->psoc, bss_chan_freq);

		if (conn_bss_chan_safe && !bss_chan_safe) {
			sme_err("High interference expected if transitioned to BSS "
				QDF_MAC_ADDR_FMT" hence reject",
				QDF_MAC_ADDR_REF(bss_desc->bssId));
			info->status =
				QCA_STATUS_REJECT_HIGH_INTERFERENCE;
			return true;
		}
	}

	return false;
}

QDF_STATUS sme_get_bss_transition_status(mac_handle_t mac_handle,
					 uint8_t transition_reason,
					 struct qdf_mac_addr *bssid,
					 struct bss_candidate_info *info,
					 uint16_t n_candidates,
					 bool is_bt_in_progress)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct bss_description *bss_desc, *conn_bss_desc;
	tCsrScanResultInfo *res, *conn_res;
	uint16_t i;

	if (!n_candidates || !info) {
		sme_err("No candidate info available");
		return QDF_STATUS_E_INVAL;
	}

	conn_res = qdf_mem_malloc(sizeof(tCsrScanResultInfo));
	if (!conn_res)
		return QDF_STATUS_E_NOMEM;

	res = qdf_mem_malloc(sizeof(tCsrScanResultInfo));
	if (!res) {
		status = QDF_STATUS_E_NOMEM;
		goto free;
	}

	/* Get the connected BSS descriptor */
	status = sme_scan_get_result_for_bssid(mac_handle, bssid, conn_res);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Failed to find connected BSS in scan list");
		goto free;
	}
	conn_bss_desc = &conn_res->BssDescriptor;

	for (i = 0; i < n_candidates; i++) {
		/* Get candidate BSS descriptors */
		status = sme_scan_get_result_for_bssid(mac_handle,
						       &info[i].bssid,
						       res);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("BSS "QDF_MAC_ADDR_FMT" not present in scan list",
				QDF_MAC_ADDR_REF(info[i].bssid.bytes));
			info[i].status = QCA_STATUS_REJECT_UNKNOWN;
			continue;
		}

		bss_desc = &res->BssDescriptor;
		if (!sme_get_status_for_candidate(mac_handle, conn_bss_desc,
						  bss_desc, &info[i],
						  transition_reason,
						  is_bt_in_progress)) {
			/*
			 * If status is not over written, it means it is a
			 * candidate for accept.
			 */
			info[i].status = QCA_STATUS_ACCEPT;
		}
	}

	/* success */
	status = QDF_STATUS_SUCCESS;

free:
	/* free allocated memory */
	if (conn_res)
		qdf_mem_free(conn_res);
	if (res)
		qdf_mem_free(res);

	return status;
}
#endif /* FEATURE_BSS_TRANSITION */

bool sme_is_conn_state_connected(mac_handle_t mac_handle, uint8_t session_id)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	return csr_is_conn_state_connected(mac_ctx, session_id);
}

void sme_enable_roaming_on_connected_sta(mac_handle_t mac_handle,
					 uint8_t vdev_id)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	QDF_STATUS status;

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_IS_STATUS_ERROR(status))
		return;

	csr_enable_roaming_on_connected_sta(mac_ctx, vdev_id);
	sme_release_global_lock(&mac_ctx->sme);
}

int16_t sme_get_oper_chan_freq(struct wlan_objmgr_vdev *vdev)
{
	uint8_t vdev_id;
	struct csr_roam_session *session;
	struct mac_context *mac_ctx;
	mac_handle_t mac_handle;

	if (!vdev) {
		sme_err("Invalid vdev id is passed");
		return 0;
	}

	mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	if (!mac_handle) {
		sme_err("mac_handle is null");
		return 0;
	}
	mac_ctx = MAC_CONTEXT(mac_handle);
	vdev_id = wlan_vdev_get_id(vdev);
	if (!CSR_IS_SESSION_VALID(mac_ctx, vdev_id)) {
		sme_err("Invalid vdev id is passed");
		return 0;
	}

	session = CSR_GET_SESSION(mac_ctx, vdev_id);

	return csr_get_infra_operation_chan_freq(mac_ctx, vdev_id);
}

enum phy_ch_width sme_get_oper_ch_width(struct wlan_objmgr_vdev *vdev)
{
	uint8_t vdev_id;
	struct csr_roam_session *session;
	struct mac_context *mac_ctx;
	mac_handle_t mac_handle;
	enum phy_ch_width ch_width = CH_WIDTH_20MHZ;

	if (!vdev) {
		sme_err("Invalid vdev id is passed");
		return CH_WIDTH_INVALID;
	}

	mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	if (!mac_handle) {
		sme_err("mac_handle is null");
		return CH_WIDTH_INVALID;
	}
	mac_ctx = MAC_CONTEXT(mac_handle);
	vdev_id = wlan_vdev_get_id(vdev);
	if (!CSR_IS_SESSION_VALID(mac_ctx, vdev_id)) {
		sme_err("Invalid vdev id is passed");
		return CH_WIDTH_INVALID;
	}

	session = CSR_GET_SESSION(mac_ctx, vdev_id);

	if (csr_is_conn_state_connected(mac_ctx, vdev_id))
		ch_width = session->connectedProfile.vht_channel_width;

	return ch_width;
}

int sme_get_sec20chan_freq_mhz(struct wlan_objmgr_vdev *vdev,
						uint16_t *sec20chan_freq)
{
	uint8_t vdev_id;

	vdev_id = wlan_vdev_get_id(vdev);
	/* Need to extend */
	return 0;
}

#ifdef WLAN_FEATURE_SAE
QDF_STATUS sme_handle_sae_msg(mac_handle_t mac_handle,
			      uint8_t session_id,
			      uint8_t sae_status,
			      struct qdf_mac_addr peer_mac_addr,
			      const uint8_t *pmkid)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct sir_sae_msg *sae_msg;
	struct scheduler_msg sch_msg = {0};
	struct wmi_roam_auth_status_params *params;
	struct csr_roam_session *csr_session;

	qdf_status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		return qdf_status;

	csr_session = CSR_GET_SESSION(mac, session_id);
	if (!csr_session) {
		sme_err("session %d not found", session_id);
		qdf_status = QDF_STATUS_E_FAILURE;
		goto error;
	}

	/* Update the status to SME in below cases
	 * 1. SAP mode: Always
	 * 2. STA mode: When the device is not in joined state
	 *
	 * If the device is in joined state, send the status to WMA which
	 * is meant for roaming.
	 */
	if ((csr_session->pCurRoamProfile &&
	     (csr_session->pCurRoamProfile->csrPersona == QDF_SAP_MODE ||
	      csr_session->pCurRoamProfile->csrPersona == QDF_P2P_GO_MODE)) ||
	    !CSR_IS_ROAM_JOINED(mac, session_id)) {
		sae_msg = qdf_mem_malloc(sizeof(*sae_msg));
		if (!sae_msg) {
			qdf_status = QDF_STATUS_E_NOMEM;
			goto error;
		}

		sae_msg->message_type = eWNI_SME_SEND_SAE_MSG;
		sae_msg->length = sizeof(*sae_msg);
		sae_msg->vdev_id = session_id;
		sae_msg->sae_status = sae_status;
		qdf_mem_copy(sae_msg->peer_mac_addr,
			     peer_mac_addr.bytes,
			     QDF_MAC_ADDR_SIZE);
		sme_debug("SAE: sae_status %d vdev_id %d Peer: "
			  QDF_MAC_ADDR_FMT, sae_msg->sae_status,
			  sae_msg->vdev_id,
			  QDF_MAC_ADDR_REF(sae_msg->peer_mac_addr));

		sch_msg.type = eWNI_SME_SEND_SAE_MSG;
		sch_msg.bodyptr = sae_msg;

		qdf_status = scheduler_post_message(QDF_MODULE_ID_SME,
						    QDF_MODULE_ID_PE,
						    QDF_MODULE_ID_PE,
						    &sch_msg);
		if (QDF_IS_STATUS_ERROR(qdf_status)) {
			qdf_mem_free(sae_msg);
			goto error;
		}
	} else {
		/*
		 * For WPA3 SAE roaming, external auth offload is enabled. The
		 * firmware will send preauth start event after candidate
		 * selection. The supplicant will perform the SAE authentication
		 * and will send the auth status, PMKID in the external auth
		 * cmd.
		 *
		 * csr roam state is CSR_ROAM_STATE_JOINED. So this SAE
		 * external auth event is for wpa3 roam pre-auth offload.
		 *
		 * Post the preauth status to WMA.
		 */
		params = qdf_mem_malloc(sizeof(*params));
		if (!params) {
			qdf_status = QDF_STATUS_E_NOMEM;
			goto error;
		}

		params->vdev_id = session_id;
		params->preauth_status = sae_status;
		qdf_copy_macaddr(&params->bssid, &peer_mac_addr);

		qdf_mem_zero(params->pmkid, PMKID_LEN);
		if (pmkid)
			qdf_mem_copy(params->pmkid, pmkid, PMKID_LEN);

		sch_msg.type = WMA_ROAM_PRE_AUTH_STATUS;
		sch_msg.bodyptr = params;

		qdf_status = scheduler_post_message(QDF_MODULE_ID_SME,
						    QDF_MODULE_ID_WMA,
						    QDF_MODULE_ID_WMA,
						    &sch_msg);
		if (QDF_IS_STATUS_ERROR(qdf_status)) {
			sme_err("WMA_ROAM_PRE_AUTH_STATUS cmd posting failed");
			qdf_mem_free(params);
		}
	}
error:
	sme_release_global_lock(&mac->sme);

	return qdf_status;
}
#endif

bool sme_is_sta_key_exchange_in_progress(mac_handle_t mac_handle,
					 uint8_t session_id)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("Invalid session id: %d", session_id);
		return false;
	}

	return CSR_IS_WAIT_FOR_KEY(mac_ctx, session_id);
}

bool sme_validate_channel_list(mac_handle_t mac_handle,
			       uint32_t *chan_freq_list,
			       uint8_t num_channels)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	uint8_t i = 0;
	uint8_t j;
	bool found;
	struct csr_channel *ch_lst_info = &mac_ctx->scan.base_channels;

	if (!chan_freq_list || !num_channels) {
		sme_err("Chan list empty %pK or num_channels is 0",
			chan_freq_list);
		return false;
	}

	while (i < num_channels) {
		found = false;
		for (j = 0; j < ch_lst_info->numChannels; j++) {
			if (ch_lst_info->channel_freq_list[j] ==
					chan_freq_list[i]) {
				found = true;
				break;
			}
		}

		if (!found) {
			sme_debug("Invalid channel %d", chan_freq_list[i]);
			return false;
		}

		i++;
	}

	return true;
}

void sme_set_pmf_wep_cfg(mac_handle_t mac_handle, uint8_t pmf_wep)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	mac_ctx->is_usr_cfg_pmf_wep = pmf_wep;
}

void sme_set_amsdu(mac_handle_t mac_handle, bool enable)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	mac_ctx->is_usr_cfg_amsdu_enabled = enable;
}

#ifdef WLAN_FEATURE_11AX
void sme_set_he_testbed_def(mac_handle_t mac_handle, uint8_t vdev_id)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session;
	QDF_STATUS status;

	session = CSR_GET_SESSION(mac_ctx, vdev_id);

	if (!session) {
		sme_debug("No session for id %d", vdev_id);
		return;
	}
	sme_debug("set HE testbed defaults");
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.amsdu_in_ampdu = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.twt_request = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.omi_a_ctrl = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.he_ppdu_20_in_160_80p80Mhz = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.he_ppdu_20_in_40Mhz_2G = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.he_ppdu_80_in_160_80p80Mhz = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.dcm_enc_tx = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.dcm_enc_rx = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.ul_mu = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.max_nc = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.trigger_frm_mac_pad =
					QCA_WLAN_HE_16US_OF_PROCESS_TIME;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.flex_twt_sched = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.ofdma_ra = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.he_4x_ltf_3200_gi_ndp = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.qtp = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.bsrp_ampdu_aggr = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.a_bqr = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.he_sub_ch_sel_tx_supp = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.ndp_feedback_supp = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.ops_supp = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.srp = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.power_boost = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.num_sounding_lt_80 = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.num_sounding_gt_80 = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.dl_mu_mimo_part_bw = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.non_trig_cqi_feedback = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.tx_1024_qam_lt_242_tone_ru = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.rx_1024_qam_lt_242_tone_ru = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.rx_full_bw_su_he_mu_compress_sigb = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.rx_full_bw_su_he_mu_non_cmpr_sigb = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.su_beamformer = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.multi_tid_aggr_rx_supp = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.multi_tid_aggr_tx_supp = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.he_dynamic_smps = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.punctured_sounding_supp = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.ht_vht_trg_frm_rx_supp = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.su_feedback_tone16 = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.mu_feedback_tone16 = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.codebook_su = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.codebook_mu = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.ul_2x996_tone_ru_supp = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.beamforming_feedback = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.he_er_su_ppdu = 0;
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap.dl_mu_mimo_part_bw = 0;
	csr_update_session_he_cap(mac_ctx, session);

	mac_ctx->mlme_cfg->he_caps.enable_6g_sec_check = true;
	status = ucfg_mlme_set_enable_bcast_probe_rsp(mac_ctx->psoc, false);
	if (QDF_IS_STATUS_ERROR(status))
		sme_err("Failed not set enable bcast probe resp info, %d",
			status);

	status = wma_cli_set_command(vdev_id,
				     WMI_VDEV_PARAM_ENABLE_BCAST_PROBE_RESPONSE,
				     0, VDEV_CMD);
	if (QDF_IS_STATUS_ERROR(status))
		sme_err("Failed to set enable bcast probe resp in FW, %d",
			status);

	mac_ctx->mlme_cfg->sta.usr_disabled_roaming = true;
}

void sme_reset_he_caps(mac_handle_t mac_handle, uint8_t vdev_id)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session;
	QDF_STATUS status;

	session = CSR_GET_SESSION(mac_ctx, vdev_id);

	if (!session) {
		sme_err("No session for id %d", vdev_id);
		return;
	}
	sme_debug("reset HE caps");
	mac_ctx->mlme_cfg->he_caps.dot11_he_cap =
		mac_ctx->mlme_cfg->he_caps.he_cap_orig;
	csr_update_session_he_cap(mac_ctx, session);

	mac_ctx->mlme_cfg->he_caps.enable_6g_sec_check = true;
	status = ucfg_mlme_set_enable_bcast_probe_rsp(mac_ctx->psoc, true);
	if (QDF_IS_STATUS_ERROR(status))
		sme_err("Failed not set enable bcast probe resp info, %d",
			status);

	status = wma_cli_set_command(vdev_id,
				     WMI_VDEV_PARAM_ENABLE_BCAST_PROBE_RESPONSE,
				     1, VDEV_CMD);
	if (QDF_IS_STATUS_ERROR(status))
		sme_err("Failed to set enable bcast probe resp in FW, %d",
			status);
	mac_ctx->is_usr_cfg_pmf_wep = PMF_CORRECT_KEY;
}
#endif

uint8_t sme_get_mcs_idx(uint16_t raw_rate, enum tx_rate_info rate_flags,
			bool is_he_mcs_12_13_supported,
			uint8_t *nss, uint8_t *dcm,
			enum txrate_gi *guard_interval,
			enum tx_rate_info *mcs_rate_flags)
{
	return wma_get_mcs_idx(raw_rate, rate_flags, is_he_mcs_12_13_supported,
			       nss, dcm, guard_interval, mcs_rate_flags);
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
QDF_STATUS sme_get_sta_cxn_info(mac_handle_t mac_handle, uint32_t session_id,
				char *buf, uint32_t buf_sz)
{
	QDF_STATUS status;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct tagCsrRoamConnectedProfile *conn_profile;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;
	if (!session || !session->pCurRoamProfile) {
		status = QDF_STATUS_E_FAILURE;
		goto end;
	}
	conn_profile = &session->connectedProfile;
	if (!conn_profile) {
		status = QDF_STATUS_E_FAILURE;
		goto end;
	}
	csr_get_sta_cxn_info(mac_ctx, session, conn_profile, buf, buf_sz);
end:
	sme_release_global_lock(&mac_ctx->sme);

	return status;
}
#endif
QDF_STATUS
sme_get_roam_scan_stats(mac_handle_t mac_handle,
			roam_scan_stats_cb cb, void *context,
			uint32_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg msg = {0};
	struct sir_roam_scan_stats *req;

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return QDF_STATUS_E_NOMEM;

	req->vdev_id = vdev_id;
	req->cb = cb;
	req->context = context;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		msg.bodyptr = req;
		msg.type = WMA_GET_ROAM_SCAN_STATS;
		msg.reserved = 0;
		status = scheduler_post_message(QDF_MODULE_ID_SME,
						QDF_MODULE_ID_WMA,
						QDF_MODULE_ID_WMA,
						&msg);
		sme_release_global_lock(&mac->sme);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				  FL("post roam scan stats req failed"));
			status = QDF_STATUS_E_FAILURE;
			qdf_mem_free(req);
		}
	} else {
		qdf_mem_free(req);
	}

	return status;
}

void sme_update_score_config(mac_handle_t mac_handle, eCsrPhyMode phy_mode,
			     uint8_t num_rf_chains)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct wlan_mlme_nss_chains vdev_ini_cfg;
	bool bval = false;
	uint32_t channel_bonding_mode;
	QDF_STATUS status;
	struct psoc_phy_config config = {0};

	qdf_mem_zero(&vdev_ini_cfg, sizeof(struct wlan_mlme_nss_chains));
	/* Populate the nss chain params from ini for this vdev type */
	sme_populate_nss_chain_params(mac_handle, &vdev_ini_cfg,
				      QDF_STA_MODE, num_rf_chains);

	config.vdev_nss_24g = vdev_ini_cfg.rx_nss[NSS_CHAINS_BAND_2GHZ];
	config.vdev_nss_5g = vdev_ini_cfg.rx_nss[NSS_CHAINS_BAND_5GHZ];

	if (phy_mode == eCSR_DOT11_MODE_AUTO ||
	    phy_mode == eCSR_DOT11_MODE_11ax ||
	    phy_mode == eCSR_DOT11_MODE_11ax_ONLY)
		config.he_cap = 1;

	if (config.he_cap ||
	    phy_mode == eCSR_DOT11_MODE_11ac ||
	    phy_mode == eCSR_DOT11_MODE_11ac_ONLY)
		config.vht_cap = 1;

	if (config.vht_cap || phy_mode == eCSR_DOT11_MODE_11n ||
	    phy_mode == eCSR_DOT11_MODE_11n_ONLY)
		config.ht_cap = 1;

	if (!IS_FEATURE_SUPPORTED_BY_FW(DOT11AX))
		config.he_cap = 0;

	if (!IS_FEATURE_SUPPORTED_BY_FW(DOT11AC))
		config.vht_cap = 0;

	status = wlan_mlme_get_vht_for_24ghz(mac_ctx->psoc, &bval);
	if (!QDF_IS_STATUS_SUCCESS(status))
		sme_err("Failed to get vht_for_24ghz");
	if (config.vht_cap && bval)
		config.vht_24G_cap = 1;

	status = wlan_mlme_get_vht_enable_tx_bf(mac_ctx->psoc,
						&bval);
	if (!QDF_IS_STATUS_SUCCESS(status))
		sme_err("unable to get vht_enable_tx_bf");

	if (bval)
		config.beamformee_cap = 1;

	ucfg_mlme_get_channel_bonding_24ghz(mac_ctx->psoc,
					    &channel_bonding_mode);
	config.bw_above_20_24ghz = channel_bonding_mode;
	ucfg_mlme_get_channel_bonding_5ghz(mac_ctx->psoc,
					   &channel_bonding_mode);
	config.bw_above_20_5ghz = channel_bonding_mode;

	wlan_psoc_set_phy_config(mac_ctx->psoc, &config);
}

static void
__sme_enable_fw_module_log_level(uint8_t *enable_fw_module_log_level,
				 uint8_t enable_fw_module_log_level_num,
				 int vdev_id, int param_id)
{
	uint8_t count = 0;
	uint32_t value = 0;
	int ret;

	while (count < enable_fw_module_log_level_num) {
		/*
		 * FW module log level input array looks like
		 * below:
		 * enable_fw_module_log_level = {<FW Module ID>,
		 * <Log Level>,...}
		 * For example:
		 * enable_fw_module_log_level=
		 * {1,0,2,1,3,2,4,3,5,4,6,5,7,6}
		 * Above input array means :
		 * For FW module ID 1 enable log level 0
		 * For FW module ID 2 enable log level 1
		 * For FW module ID 3 enable log level 2
		 * For FW module ID 4 enable log level 3
		 * For FW module ID 5 enable log level 4
		 * For FW module ID 6 enable log level 5
		 * For FW module ID 7 enable log level 6
		 */

		if ((enable_fw_module_log_level[count] > WLAN_MODULE_ID_MAX) ||
		    (enable_fw_module_log_level[count + 1] > DBGLOG_LVL_MAX)) {
			sme_err("Module id %d or dbglog level %d input value is more than max",
				enable_fw_module_log_level[count],
				enable_fw_module_log_level[count + 1]);
			count += 2;
			continue;
		}

		value = enable_fw_module_log_level[count] << 16;
		value |= enable_fw_module_log_level[count + 1];
		ret = sme_cli_set_command(vdev_id, param_id, value, DBG_CMD);
		if (ret != 0)
			sme_err("Failed to enable FW module log level %d ret %d",
				value, ret);

		count += 2;
	}
}

void sme_enable_fw_module_log_level(mac_handle_t mac_handle, int vdev_id)
{
	QDF_STATUS status;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	uint8_t *enable_fw_module_log_level;
	uint8_t enable_fw_module_log_level_num;

	status = ucfg_fwol_get_enable_fw_module_log_level(
			mac_ctx->psoc, &enable_fw_module_log_level,
			&enable_fw_module_log_level_num);
	if (QDF_IS_STATUS_ERROR(status))
		return;
	__sme_enable_fw_module_log_level(enable_fw_module_log_level,
					 enable_fw_module_log_level_num,
					 vdev_id,
					 WMI_DBGLOG_MOD_LOG_LEVEL);

	enable_fw_module_log_level_num = 0;
	status = ucfg_fwol_wow_get_enable_fw_module_log_level(
			mac_ctx->psoc, &enable_fw_module_log_level,
			&enable_fw_module_log_level_num);
	if (QDF_IS_STATUS_ERROR(status))
		return;
	__sme_enable_fw_module_log_level(enable_fw_module_log_level,
					 enable_fw_module_log_level_num,
					 vdev_id,
					 WMI_DBGLOG_MOD_WOW_LOG_LEVEL);
}

#ifdef WLAN_FEATURE_MOTION_DETECTION
/**
 * sme_set_md_bl_evt_cb - Register/set motion detection baseline callback
 * @mac_handle: mac handle
 * @callback_fn: callback function pointer
 * @hdd_ctx: hdd context
 *
 * Return: QDF_STATUS_SUCCESS or non-zero on failure
 */
QDF_STATUS sme_set_md_bl_evt_cb(
	mac_handle_t mac_handle,
	QDF_STATUS (*callback_fn)(void *ctx, struct sir_md_bl_evt *event),
	void *hdd_ctx
)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	qdf_status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(qdf_status)) {
		mac->sme.md_bl_evt_cb = callback_fn;
		mac->sme.md_ctx = hdd_ctx;
		sme_release_global_lock(&mac->sme);
	}
	return qdf_status;
}

/**
 * sme_set_md_host_evt_cb - Register/set motion detection callback
 * @mac_handle: mac handle
 * @callback_fn: motion detection callback function pointer
 * @hdd_ctx: hdd context
 *
 * Return: QDF_STATUS_SUCCESS or non-zero on failure
 */
QDF_STATUS sme_set_md_host_evt_cb(
	mac_handle_t mac_handle,
	QDF_STATUS (*callback_fn)(void *ctx, struct sir_md_evt *event),
	void *hdd_ctx
)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	qdf_status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(qdf_status)) {
		mac->sme.md_host_evt_cb = callback_fn;
		mac->sme.md_ctx = hdd_ctx;
		sme_release_global_lock(&mac->sme);
	}
	return qdf_status;
}

/**
 * sme_motion_det_config - Post motion detection configuration msg to scheduler
 * @mac_handle: mac handle
 * @motion_det_config: motion detection configuration
 *
 * Return: QDF_STATUS_SUCCESS or non-zero on failure
 */
QDF_STATUS sme_motion_det_config(mac_handle_t mac_handle,
				 struct sme_motion_det_cfg *motion_det_config)
{
	struct scheduler_msg msg;
	struct sme_motion_det_cfg *motion_det_cfg;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	qdf_status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(qdf_status)) {
		motion_det_cfg =
				qdf_mem_malloc(sizeof(*motion_det_cfg));
		if (!motion_det_cfg) {
			sme_release_global_lock(&mac->sme);
			return QDF_STATUS_E_NOMEM;
		}

		*motion_det_cfg = *motion_det_config;

		qdf_mem_set(&msg, sizeof(msg), 0);
		msg.type = WMA_SET_MOTION_DET_CONFIG;
		msg.bodyptr = motion_det_cfg;

		qdf_status = scheduler_post_message(QDF_MODULE_ID_SME,
						    QDF_MODULE_ID_WMA,
						    QDF_MODULE_ID_WMA,
						    &msg);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			qdf_mem_free(motion_det_cfg);
			qdf_status = QDF_STATUS_E_FAILURE;
		}
		sme_release_global_lock(&mac->sme);
	}
	return qdf_status;
}

/**
 * sme_motion_det_enable - Post motion detection start/stop msg to scheduler
 * @mac_handle: mac handle
 * @motion_det_enable: motion detection start/stop
 *
 * Return: QDF_STATUS_SUCCESS or non-zero on failure
 */
QDF_STATUS sme_motion_det_enable(mac_handle_t mac_handle,
				 struct sme_motion_det_en *motion_det_enable)
{
	struct scheduler_msg msg;
	struct sme_motion_det_en *motion_det_en;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	qdf_status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(qdf_status)) {
		motion_det_en = qdf_mem_malloc(sizeof(*motion_det_en));
		if (!motion_det_en) {
			sme_release_global_lock(&mac->sme);
			return QDF_STATUS_E_NOMEM;
		}

		*motion_det_en = *motion_det_enable;

		qdf_mem_set(&msg, sizeof(msg), 0);
		msg.type = WMA_SET_MOTION_DET_ENABLE;
		msg.bodyptr = motion_det_en;

		qdf_status = scheduler_post_message(QDF_MODULE_ID_SME,
						    QDF_MODULE_ID_WMA,
						    QDF_MODULE_ID_WMA,
						    &msg);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			qdf_mem_free(motion_det_en);
			qdf_status = QDF_STATUS_E_FAILURE;
		}
		sme_release_global_lock(&mac->sme);
	}
	return qdf_status;
}

/**
 * sme_motion_det_base_line_config - Post md baselining cfg msg to scheduler
 * @mac_handle: mac handle
 * @motion_det_base_line_config: motion detection baselining configuration
 *
 * Return: QDF_STATUS_SUCCESS or non-zero on failure
 */
QDF_STATUS sme_motion_det_base_line_config(
	mac_handle_t mac_handle,
	struct sme_motion_det_base_line_cfg *motion_det_base_line_config)
{
	struct scheduler_msg msg;
	struct sme_motion_det_base_line_cfg *motion_det_base_line_cfg;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	qdf_status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(qdf_status)) {
		motion_det_base_line_cfg =
			qdf_mem_malloc(sizeof(*motion_det_base_line_cfg));

		if (!motion_det_base_line_cfg) {
			sme_release_global_lock(&mac->sme);
			return QDF_STATUS_E_NOMEM;
		}

		*motion_det_base_line_cfg = *motion_det_base_line_config;

		qdf_mem_set(&msg, sizeof(msg), 0);
		msg.type = WMA_SET_MOTION_DET_BASE_LINE_CONFIG;
		msg.bodyptr = motion_det_base_line_cfg;

		qdf_status = scheduler_post_message(QDF_MODULE_ID_SME,
						    QDF_MODULE_ID_WMA,
						    QDF_MODULE_ID_WMA,
						    &msg);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			qdf_mem_free(motion_det_base_line_cfg);
			qdf_status = QDF_STATUS_E_FAILURE;
		}
		sme_release_global_lock(&mac->sme);
	}
	return qdf_status;
}

/**
 * sme_motion_det_base_line_enable - Post md baselining enable msg to scheduler
 * @mac_handle: mac handle
 * @motion_det_base_line_enable: motion detection baselining start/stop
 *
 * Return: QDF_STATUS_SUCCESS or non-zero on failure
 */
QDF_STATUS sme_motion_det_base_line_enable(
	mac_handle_t mac_handle,
	struct sme_motion_det_base_line_en *motion_det_base_line_enable)
{
	struct scheduler_msg msg;
	struct sme_motion_det_base_line_en *motion_det_base_line_en;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	qdf_status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(qdf_status)) {
		motion_det_base_line_en =
			qdf_mem_malloc(sizeof(*motion_det_base_line_en));

		if (!motion_det_base_line_en) {
			sme_release_global_lock(&mac->sme);
			return QDF_STATUS_E_NOMEM;
		}

		*motion_det_base_line_en = *motion_det_base_line_enable;

		qdf_mem_set(&msg, sizeof(msg), 0);
		msg.type = WMA_SET_MOTION_DET_BASE_LINE_ENABLE;
		msg.bodyptr = motion_det_base_line_en;

		qdf_status = scheduler_post_message(QDF_MODULE_ID_SME,
						    QDF_MODULE_ID_WMA,
						    QDF_MODULE_ID_WMA,
						    &msg);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			qdf_mem_free(motion_det_base_line_en);
			qdf_status = QDF_STATUS_E_FAILURE;
		}
		sme_release_global_lock(&mac->sme);
	}
	return qdf_status;
}
#endif /* WLAN_FEATURE_MOTION_DETECTION */

#ifdef FW_THERMAL_THROTTLE_SUPPORT

/**
 * sme_set_thermal_throttle_cfg() - SME API to set the thermal throttle
 * configuration parameters
 * @mac_handle: Opaque handle to the global MAC context
 * @therm_params: structure of thermal configuration parameters
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_set_thermal_throttle_cfg(mac_handle_t mac_handle,
			struct thermal_mitigation_params *therm_params)

{
	struct scheduler_msg msg;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct thermal_mitigation_params *therm_cfg_params;

	therm_cfg_params = qdf_mem_malloc(sizeof(*therm_cfg_params));
	if (!therm_cfg_params)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_set(therm_cfg_params, sizeof(*therm_cfg_params), 0);
	qdf_mem_copy(therm_cfg_params, therm_params, sizeof(*therm_cfg_params));

	qdf_mem_set(&msg, sizeof(msg), 0);
	msg.type = WMA_SET_THERMAL_THROTTLE_CFG;
	msg.reserved = 0;
	msg.bodyptr = therm_cfg_params;

	qdf_status = sme_acquire_global_lock(&mac->sme);
	qdf_status =  scheduler_post_message(QDF_MODULE_ID_SME,
					     QDF_MODULE_ID_WMA,
					     QDF_MODULE_ID_WMA, &msg);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		sme_err("failed to schedule throttle config req %d",
			qdf_status);
		qdf_mem_free(therm_cfg_params);
		qdf_status = QDF_STATUS_E_FAILURE;
	}
	sme_release_global_lock(&mac->sme);
	return qdf_status;
}

/**
 * sme_set_thermal_mgmt() - SME API to set the thermal management params
 * @mac_handle: Opaque handle to the global MAC context
 * @lower_thresh_deg: Lower threshold value of Temperature
 * @higher_thresh_deg: Higher threshold value of Temperature
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_set_thermal_mgmt(mac_handle_t mac_handle,
				uint16_t lower_thresh_deg,
				uint16_t higher_thresh_deg)
{
	struct scheduler_msg msg;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	t_thermal_cmd_params *therm_mgmt_cmd;

	qdf_status = sme_acquire_global_lock(&mac->sme);
	if (QDF_STATUS_SUCCESS == qdf_status) {
		therm_mgmt_cmd = qdf_mem_malloc(sizeof(*therm_mgmt_cmd));
		if (!therm_mgmt_cmd) {
			sme_release_global_lock(&mac->sme);
			return QDF_STATUS_E_NOMEM;
		}

		therm_mgmt_cmd->minTemp = lower_thresh_deg;
		therm_mgmt_cmd->maxTemp = higher_thresh_deg;
		therm_mgmt_cmd->thermalEnable = 1;

		qdf_mem_set(&msg, sizeof(msg), 0);
		msg.type = WMA_SET_THERMAL_MGMT;
		msg.reserved = 0;
		msg.bodyptr = therm_mgmt_cmd;

		qdf_status =  scheduler_post_message(QDF_MODULE_ID_SME,
						     QDF_MODULE_ID_WMA,
						     QDF_MODULE_ID_WMA, &msg);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			qdf_mem_free(therm_mgmt_cmd);
			qdf_status = QDF_STATUS_E_FAILURE;
		}
		sme_release_global_lock(&mac->sme);
	}
	return qdf_status;
}
#endif /* FW_THERMAL_THROTTLE_SUPPORT */

QDF_STATUS sme_update_hidden_ssid_status_cb(mac_handle_t mac_handle,
					    hidden_ssid_cb cb)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		mac->sme.hidden_ssid_cb = cb;
		sme_release_global_lock(&mac->sme);
	}

	return status;
}

QDF_STATUS sme_update_owe_info(struct mac_context *mac,
			       struct assoc_ind *assoc_ind)
{
	QDF_STATUS status;

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = csr_update_owe_info(mac, assoc_ind);
		sme_release_global_lock(&mac->sme);
	}

	return status;
}

#ifdef WLAN_MWS_INFO_DEBUGFS
QDF_STATUS
sme_get_mws_coex_info(mac_handle_t mac_handle, uint32_t vdev_id,
		      uint32_t cmd_id, void (*callback_fn)(void *coex_info_data,
							   void *context,
							   wmi_mws_coex_cmd_id
							   cmd_id),
		      void *context)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct scheduler_msg msg = {0};
	struct sir_get_mws_coex_info *req;

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return QDF_STATUS_E_NOMEM;

	req->vdev_id = vdev_id;
	req->cmd_id  = cmd_id;
	mac->sme.mws_coex_info_state_resp_callback = callback_fn;
	mac->sme.mws_coex_info_ctx = context;
	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		msg.bodyptr = req;
		msg.type = WMA_GET_MWS_COEX_INFO_REQ;
		status = scheduler_post_message(QDF_MODULE_ID_SME,
						QDF_MODULE_ID_WMA,
						QDF_MODULE_ID_WMA,
						&msg);
		sme_release_global_lock(&mac->sme);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("post MWS coex info req failed");
			status = QDF_STATUS_E_FAILURE;
			qdf_mem_free(req);
		}
	} else {
		sme_err("sme_acquire_global_lock failed");
		qdf_mem_free(req);
	}

	return status;
}
#endif /* WLAN_MWS_INFO_DEBUGFS */

#ifdef WLAN_BCN_RECV_FEATURE
/**
 * sme_scan_event_handler() - Scan complete event handler
 * @vdev: vdev obj manager
 * @event: scan event
 * @arg: arg of scan event
 *
 * This function is getting called after Host receive scan start
 *
 * Return: None
 */
static void sme_scan_event_handler(struct wlan_objmgr_vdev *vdev,
				   struct scan_event *event,
				   void *arg)
{
	struct mac_context *mac = arg;
	uint8_t vdev_id;

	if (!mac) {
		sme_err("Invalid mac context");
		return;
	}

	if (!mac->sme.beacon_pause_cb)
		return;

	if (event->type != SCAN_EVENT_TYPE_STARTED)
		return;

	for (vdev_id = 0 ; vdev_id < WLAN_MAX_VDEVS ; vdev_id++) {
		if (CSR_IS_SESSION_VALID(mac, vdev_id) &&
		    sme_is_beacon_report_started(MAC_HANDLE(mac), vdev_id)) {
			sme_debug("Send pause ind for vdev_id : %d", vdev_id);
			mac->sme.beacon_pause_cb(mac->hdd_handle, vdev_id,
						 event->type, false);
		}
	}
}

QDF_STATUS sme_register_bcn_recv_pause_ind_cb(mac_handle_t mac_handle,
					      beacon_pause_cb cb)
{
	QDF_STATUS status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	if (!mac) {
		sme_err("Invalid mac context");
		return QDF_STATUS_E_NOMEM;
	}

	/* scan event de-registration */
	if (!cb) {
		ucfg_scan_unregister_event_handler(mac->pdev,
						   sme_scan_event_handler, mac);
		return QDF_STATUS_SUCCESS;
	}
	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		mac->sme.beacon_pause_cb = cb;
		sme_release_global_lock(&mac->sme);
	}

	/* scan event registration */
	status = ucfg_scan_register_event_handler(mac->pdev,
						  sme_scan_event_handler, mac);
	if (QDF_IS_STATUS_ERROR(status))
		sme_err("scan event register failed ");

	return status;
}
#endif

QDF_STATUS sme_set_vdev_sw_retry(uint8_t vdev_id, uint8_t sw_retry_count,
				 wmi_vdev_custom_sw_retry_type_t sw_retry_type)
{
	QDF_STATUS status;

	status = wma_set_vdev_sw_retry_th(vdev_id, sw_retry_count,
					  sw_retry_type);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Failed to set retry count for vdev: %d", vdev_id);
		return status;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sme_set_disconnect_ies(mac_handle_t mac_handle, uint8_t vdev_id,
				  uint8_t *ie_data, uint16_t ie_len)
{
	struct mac_context *mac_ctx;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_ies ie;

	if (!ie_data || !ie_len) {
		sme_debug("Got NULL disconnect IEs");
		return QDF_STATUS_E_INVAL;
	}

	mac_ctx = MAC_CONTEXT(mac_handle);
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
						    vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("Got NULL vdev obj, returning");
		return QDF_STATUS_E_FAILURE;
	}

	ie.data = ie_data;
	ie.len = ie_len;

	mlme_set_self_disconnect_ies(vdev, &ie);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
	return QDF_STATUS_SUCCESS;
}

void sme_chan_to_freq_list(
			struct wlan_objmgr_pdev *pdev,
			uint32_t *freq_list,
			const uint8_t *chan_list,
			uint32_t chan_list_len)
{
	uint32_t count;

	for (count = 0; count < chan_list_len; count++)
		freq_list[count] =
			wlan_reg_chan_to_freq(pdev, (uint32_t)chan_list[count]);
}

QDF_STATUS
sme_send_vendor_btm_params(mac_handle_t mac_handle, uint8_t vdev_id)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (vdev_id >= WLAN_MAX_VDEVS) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Invalid sme session id: %d"), vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (mac->mlme_cfg->lfr.roam_scan_offload_enabled)
			csr_roam_update_cfg(mac, vdev_id,
					    REASON_ROAM_CONTROL_CONFIG_CHANGED);
		sme_release_global_lock(&mac->sme);
	}

	return status;
}

QDF_STATUS sme_set_roam_config_enable(mac_handle_t mac_handle,
				      uint8_t vdev_id,
				      uint8_t roam_control_enable)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	tCsrNeighborRoamControlInfo *neighbor_roam_info;
	tCsrNeighborRoamCfgParams *cfg_params;
	QDF_STATUS status;

	if (!mac->mlme_cfg->lfr.roam_scan_offload_enabled)
		return QDF_STATUS_E_INVAL;

	if (vdev_id >= WLAN_MAX_VDEVS) {
		sme_err("Invalid vdev_id: %d", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Failed to acquire sme lock; status: %d", status);
		return status;
	}
	neighbor_roam_info = &mac->roam.neighborRoamInfo[vdev_id];

	neighbor_roam_info->roam_control_enable = !!roam_control_enable;
	if (roam_control_enable) {
		cfg_params = &neighbor_roam_info->cfgParams;
		cfg_params->roam_scan_period_after_inactivity = 0;
		cfg_params->roam_inactive_data_packet_count = 0;
		cfg_params->roam_scan_inactivity_time = 0;

		csr_roam_update_cfg(mac, vdev_id,
				    REASON_ROAM_CONTROL_CONFIG_ENABLED);
	}
	sme_release_global_lock(&mac->sme);

	return status;
}

QDF_STATUS sme_get_roam_config_status(mac_handle_t mac_handle,
				      uint8_t vdev_id,
				      uint8_t *config_status)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status;

	if (vdev_id >= WLAN_MAX_VDEVS) {
		sme_err("Invalid vdev_id: %d", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Failed to acquire sme lock; status: %d", status);
		return status;
	}
	*config_status =
		mac->roam.neighborRoamInfo[vdev_id].roam_control_enable;
	sme_release_global_lock(&mac->sme);

	return status;
}

uint16_t sme_get_full_roam_scan_period_global(mac_handle_t mac_handle)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	return mac->mlme_cfg->lfr.roam_full_scan_period;
}

QDF_STATUS
sme_get_full_roam_scan_period(mac_handle_t mac_handle, uint8_t vdev_id,
			      uint32_t *full_roam_scan_period)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS status;
	tpCsrNeighborRoamControlInfo neighbor_roam_info;

	if (vdev_id >= WLAN_MAX_VDEVS) {
		sme_err("Invalid vdev_id: %d", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Failed to acquire sme lock; status: %d", status);
		return status;
	}
	neighbor_roam_info = &mac->roam.neighborRoamInfo[vdev_id];
	*full_roam_scan_period =
		neighbor_roam_info->cfgParams.full_roam_scan_period;
	sme_release_global_lock(&mac->sme);

	return status;
}

QDF_STATUS sme_check_for_duplicate_session(mac_handle_t mac_handle,
					   uint8_t *peer_addr)
{
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	bool peer_exist = false;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	if (!soc) {
		sme_err("Failed to get soc handle");
		return QDF_STATUS_E_INVAL;
	}

	if (QDF_STATUS_SUCCESS != sme_acquire_global_lock(&mac_ctx->sme))
		return status;

	peer_exist = cdp_find_peer_exist(soc, OL_TXRX_PDEV_ID, peer_addr);
	if (peer_exist) {
		sme_err("Peer exists with same MAC");
		status = QDF_STATUS_E_EXISTS;
	} else {
		status = QDF_STATUS_SUCCESS;
	}
	sme_release_global_lock(&mac_ctx->sme);

	return status;
}

#ifdef FEATURE_ANI_LEVEL_REQUEST
QDF_STATUS sme_get_ani_level(mac_handle_t mac_handle, uint32_t *freqs,
			     uint8_t num_freqs, void (*callback)(
			     struct wmi_host_ani_level_event *ani, uint8_t num,
			     void *context), void *context)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	void *wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		sme_err("wma handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mac->ani_params.ani_level_cb = callback;
	mac->ani_params.context = context;

	status = wma_send_ani_level_request(wma_handle, freqs, num_freqs);
	return status;
}
#endif /* FEATURE_ANI_LEVEL_REQUEST */

QDF_STATUS sme_get_prev_connected_bss_ies(mac_handle_t mac_handle,
					  uint8_t vdev_id,
					  uint8_t **ies, uint32_t *ie_len)
{
	struct csr_roam_session *session;
	struct mac_context *mac;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t len;
	uint8_t *beacon_ie;

	if (!mac_handle) {
		sme_err("mac_handle is not valid");
		return QDF_STATUS_E_INVAL;
	}
	mac = MAC_CONTEXT(mac_handle);
	session = CSR_GET_SESSION(mac, vdev_id);
	if (!session) {
		sme_err("session not found");
		return QDF_STATUS_E_INVAL;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Failed to acquire sme lock; status: %d", status);
		return status;
	}

	len = session->prev_assoc_ap_info.nBeaconLength;
	if (!len) {
		sme_debug("No IEs to return");
		status = QDF_STATUS_E_INVAL;
		goto end;
	}
	beacon_ie = qdf_mem_malloc(len);
	if (!beacon_ie) {
		status = QDF_STATUS_E_NOMEM;
		goto end;
	}
	qdf_mem_copy(beacon_ie, session->prev_assoc_ap_info.pbFrames, len);

	*ie_len = len;
	*ies = beacon_ie;
end:
	sme_release_global_lock(&mac->sme);
	return status;
}

#ifdef FEATURE_MONITOR_MODE_SUPPORT

QDF_STATUS sme_set_monitor_mode_cb(mac_handle_t mac_handle,
				   void (*monitor_mode_cb)(uint8_t vdev_id))
{
	QDF_STATUS qdf_status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	qdf_status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		sme_err("Failed to acquire sme lock; status: %d", qdf_status);
		return qdf_status;
	}
	mac->sme.monitor_mode_cb = monitor_mode_cb;
	sme_release_global_lock(&mac->sme);

	return qdf_status;
}

QDF_STATUS sme_process_monitor_mode_vdev_up_evt(uint8_t vdev_id)
{
	mac_handle_t mac_handle;
	struct mac_context *mac;

	mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	if (!mac_handle) {
		sme_err("mac_handle is not valid");
		return QDF_STATUS_E_INVAL;
	}

	mac = MAC_CONTEXT(mac_handle);

	if (mac->sme.monitor_mode_cb)
		mac->sme.monitor_mode_cb(vdev_id);
	else {
		sme_warn_rl("monitor_mode_cb is not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}
#endif

#if defined(CLD_PM_QOS) && defined(WLAN_FEATURE_LL_MODE)
QDF_STATUS
sme_set_beacon_latency_event_cb(mac_handle_t mac_handle,
				void (*beacon_latency_event_cb)
				(uint32_t latency_level))
{
	QDF_STATUS qdf_status;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	qdf_status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		sme_err("Failed to acquire sme lock; status: %d", qdf_status);
		return qdf_status;
	}
	mac->sme.beacon_latency_event_cb = beacon_latency_event_cb;
	sme_release_global_lock(&mac->sme);

	return qdf_status;
}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
void sme_roam_events_register_callback(mac_handle_t mac_handle,
				       void (*roam_rt_stats_cb)(
				hdd_handle_t hdd_handle,
				struct mlme_roam_debug_info *roam_stats))
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	if (!mac) {
		sme_err("Invalid mac context");
		return;
	}

	mac->sme.roam_rt_stats_cb = roam_rt_stats_cb;
}

void sme_roam_events_deregister_callback(mac_handle_t mac_handle)
{
	sme_roam_events_register_callback(mac_handle, NULL);
}
#endif
