/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_policy_mgr_pcl.c
 *
 * WLAN Concurrenct Connection Management APIs
 *
 */

/* Include files */

#include "wlan_policy_mgr_api.h"
#include "wlan_policy_mgr_i.h"
#include "qdf_types.h"
#include "qdf_trace.h"
#include "qdf_str.h"
#include "wlan_objmgr_global_obj.h"
#include "wlan_utility.h"
#include "wlan_mlme_ucfg_api.h"
#include "csr_neighbor_roam.h"

/**
 * first_connection_pcl_table - table which provides PCL for the
 * very first connection in the system
 */
const enum policy_mgr_pcl_type
first_connection_pcl_table[PM_MAX_NUM_OF_MODE]
			[PM_MAX_CONC_PRIORITY_MODE] = {
	[PM_STA_MODE] = {PM_NONE, PM_NONE, PM_NONE},
	[PM_SAP_MODE] = {PM_5G,   PM_5G,   PM_5G  },
	[PM_P2P_CLIENT_MODE] = {PM_5G,   PM_5G,   PM_5G  },
	[PM_P2P_GO_MODE] = {PM_5G,   PM_5G,   PM_5G  },
	[PM_NAN_DISC_MODE] = {PM_5G, PM_5G, PM_5G},
};

enum policy_mgr_pcl_type
	(*second_connection_pcl_dbs_table)[PM_MAX_ONE_CONNECTION_MODE]
			[PM_MAX_NUM_OF_MODE][PM_MAX_CONC_PRIORITY_MODE];
enum policy_mgr_pcl_type const
	(*second_connection_pcl_non_dbs_table)[PM_MAX_ONE_CONNECTION_MODE]
			[PM_MAX_NUM_OF_MODE][PM_MAX_CONC_PRIORITY_MODE];
pm_dbs_pcl_third_connection_table_type
		*third_connection_pcl_dbs_table;
enum policy_mgr_pcl_type const
	(*third_connection_pcl_non_dbs_table)[PM_MAX_TWO_CONNECTION_MODE]
			[PM_MAX_NUM_OF_MODE][PM_MAX_CONC_PRIORITY_MODE];
policy_mgr_next_action_two_connection_table_type
		*next_action_two_connection_table;
policy_mgr_next_action_three_connection_table_type
		*next_action_three_connection_table;
policy_mgr_next_action_two_connection_table_type
		*next_action_two_connection_2x2_2g_1x1_5g_table;
policy_mgr_next_action_three_connection_table_type
		*next_action_three_connection_2x2_2g_1x1_5g_table;

QDF_STATUS policy_mgr_get_pcl_for_existing_conn(
		struct wlan_objmgr_psoc *psoc,
		enum policy_mgr_con_mode mode,
		uint32_t *pcl_ch, uint32_t *len,
		uint8_t *pcl_weight, uint32_t weight_len,
		bool all_matching_cxn_to_del)
{
	struct policy_mgr_conc_connection_info
			info[MAX_NUMBER_OF_CONC_CONNECTIONS] = { {0} };
	uint8_t num_cxn_del = 0;

	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	policy_mgr_debug("get pcl for existing conn:%d", mode);
	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}
	*len = 0;
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	if (policy_mgr_mode_specific_connection_count(psoc, mode, NULL) > 0) {
		/* Check, store and temp delete the mode's parameter */
		policy_mgr_store_and_del_conn_info(psoc, mode,
				all_matching_cxn_to_del, info, &num_cxn_del);
		/* Get the PCL */
		status = policy_mgr_get_pcl(psoc, mode, pcl_ch, len,
					    pcl_weight, weight_len);
		policy_mgr_debug("Get PCL to FW for mode:%d", mode);
		/* Restore the connection info */
		policy_mgr_restore_deleted_conn_info(psoc, info, num_cxn_del);
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return status;
}

QDF_STATUS policy_mgr_get_pcl_for_vdev_id(struct wlan_objmgr_psoc *psoc,
					  enum policy_mgr_con_mode mode,
					  uint32_t *pcl_ch, uint32_t *len,
					  uint8_t *pcl_weight,
					  uint32_t weight_len,
					  uint8_t vdev_id)
{
	struct policy_mgr_conc_connection_info
			info[MAX_NUMBER_OF_CONC_CONNECTIONS] = { {0} };
	uint8_t num_cxn_del = 0;

	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	policy_mgr_debug("get pcl for existing conn:%d", mode);
	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}
	*len = 0;
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	if (policy_mgr_mode_specific_connection_count(psoc, mode, NULL) > 0) {
		/* Check, store and temp delete the mode's parameter */
		policy_mgr_store_and_del_conn_info_by_vdev_id(psoc,
							      vdev_id,
							      info,
							      &num_cxn_del);
		/* Get the PCL */
		status = policy_mgr_get_pcl(psoc, mode, pcl_ch, len,
					    pcl_weight, weight_len);
		policy_mgr_debug("Get PCL to FW for mode:%d", mode);
		/* Restore the connection info */
		policy_mgr_restore_deleted_conn_info(psoc, info, num_cxn_del);
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return status;
}

void policy_mgr_decr_session_set_pcl(struct wlan_objmgr_psoc *psoc,
						enum QDF_OPMODE mode,
						uint8_t session_id)
{
	QDF_STATUS qdf_status;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	mac_handle_t mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	uint32_t conn_idx = 0;
	uint8_t vdev_id = WLAN_INVALID_VDEV_ID;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	qdf_status = policy_mgr_decr_active_session(psoc, mode, session_id);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		policy_mgr_debug("Invalid active session");
		return;
	}

	/*
	 * After the removal of this connection, we need to check if
	 * a STA connection still exists. The reason for this is that
	 * if one or more STA exists, we need to provide the updated
	 * PCL to the FW for cases like LFR.
	 *
	 * Since policy_mgr_get_pcl provides PCL list based on the new
	 * connection that is going to come up, we will find the
	 * existing STA entry, save it and delete it temporarily.
	 * After this we will get PCL as though as new STA connection
	 * is coming up. This will give the exact PCL that needs to be
	 * given to the FW. After setting the PCL, we need to restore
	 * the entry that we have saved before.
	 */

	if ((policy_mgr_mode_specific_connection_count(
		psoc, PM_STA_MODE, NULL) > 0) && mode != QDF_STA_MODE) {
		for (conn_idx = 0; conn_idx < MAX_NUMBER_OF_CONC_CONNECTIONS;
		     conn_idx++) {
			qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
			if (!(pm_conc_connection_list[conn_idx].mode ==
			      PM_STA_MODE &&
			      pm_conc_connection_list[conn_idx].in_use)) {
				qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
				continue;
			}

			vdev_id = pm_conc_connection_list[conn_idx].vdev_id;
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

			/* Send RSO stop before sending set pcl command */
			pm_ctx->sme_cbacks.sme_rso_stop_cb(
						mac_handle, session_id,
						REASON_DRIVER_DISABLED,
						RSO_SET_PCL);

			policy_mgr_set_pcl_for_existing_combo(psoc, PM_STA_MODE,
							      session_id);

			pm_ctx->sme_cbacks.sme_rso_start_cb(
					mac_handle, session_id,
					REASON_DRIVER_ENABLED,
					RSO_SET_PCL);
		}
	}

	/* do we need to change the HW mode */
	if (policy_mgr_is_hw_dbs_capable(psoc))
		policy_mgr_check_n_start_opportunistic_timer(psoc);
	return;
}

/**
 * policy_mgr_update_valid_ch_freq_list() - Update policy manager valid ch list
 * @pm_ctx: policy manager context data
 * @ch_list: Regulatory channel list
 * @is_client: true if caller is a client, false if it is a beaconing entity
 *
 * When regulatory component channel list is updated this internal function is
 * called to update policy manager copy of valid channel list.
 *
 * Return: QDF_STATUS_SUCCESS on success other qdf error status code
 */
static void
policy_mgr_update_valid_ch_freq_list(struct policy_mgr_psoc_priv_obj *pm_ctx,
				     struct regulatory_channel *reg_ch_list,
				     bool is_client)
{
	uint32_t i, j = 0, ch_freq;
	enum channel_state state;

	for (i = 0; i < NUM_CHANNELS; i++) {
		ch_freq = reg_ch_list[i].center_freq;
		if (is_client)
			state = wlan_reg_get_channel_state_for_freq(
							pm_ctx->pdev, ch_freq);
		else
			state =
			wlan_reg_get_channel_state_from_secondary_list_for_freq(
							pm_ctx->pdev, ch_freq);

		if (state != CHANNEL_STATE_DISABLE &&
		    state != CHANNEL_STATE_INVALID) {
			pm_ctx->valid_ch_freq_list[j] =
				reg_ch_list[i].center_freq;
			j++;
		}
	}
	pm_ctx->valid_ch_freq_list_count = j;
}

void
policy_mgr_reg_chan_change_callback(struct wlan_objmgr_psoc *psoc,
				    struct wlan_objmgr_pdev *pdev,
				    struct regulatory_channel *chan_list,
				    struct avoid_freq_ind_data *avoid_freq_ind,
				    void *arg)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t i;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	wlan_reg_decide_6g_ap_pwr_type(pdev);
	policy_mgr_update_valid_ch_freq_list(pm_ctx, chan_list, false);

	if (!avoid_freq_ind) {
		policy_mgr_debug("avoid_freq_ind NULL");
		return;
	}

	/*
	 * The ch_list buffer can accomadate a maximum of
	 * NUM_CHANNELS and hence the ch_cnt should also not
	 * exceed NUM_CHANNELS.
	 */
	pm_ctx->unsafe_channel_count = avoid_freq_ind->chan_list.chan_cnt >=
			NUM_CHANNELS ?
			NUM_CHANNELS : avoid_freq_ind->chan_list.chan_cnt;

	for (i = 0; i < pm_ctx->unsafe_channel_count; i++)
		pm_ctx->unsafe_channel_list[i] =
			avoid_freq_ind->chan_list.chan_freq_list[i];

	policy_mgr_debug("Channel list update, received %d avoided channels",
			 pm_ctx->unsafe_channel_count);
}

QDF_STATUS policy_mgr_init_chan_avoidance(struct wlan_objmgr_psoc *psoc,
					  qdf_freq_t *chan_freq_list,
					  uint16_t chan_cnt)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t i;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	pm_ctx->unsafe_channel_count = chan_cnt >= NUM_CHANNELS ?
			NUM_CHANNELS : chan_cnt;

	for (i = 0; i < pm_ctx->unsafe_channel_count; i++)
		pm_ctx->unsafe_channel_list[i] = chan_freq_list[i];

	policy_mgr_debug("Channel list init, received %d avoided channels",
			 pm_ctx->unsafe_channel_count);

	return QDF_STATUS_SUCCESS;
}

void policy_mgr_update_with_safe_channel_list(struct wlan_objmgr_psoc *psoc,
					      uint32_t *pcl_channels,
					      uint32_t *len,
					      uint8_t *weight_list,
					      uint32_t weight_len)
{
	uint32_t current_channel_list[NUM_CHANNELS];
	uint8_t org_weight_list[NUM_CHANNELS];
	uint8_t is_unsafe = 1;
	uint8_t i, j;
	uint32_t safe_channel_count = 0, current_channel_count = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint8_t scc_on_lte_coex = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	if (len) {
		current_channel_count = QDF_MIN(*len, NUM_CHANNELS);
	} else {
		policy_mgr_err("invalid number of channel length");
		return;
	}

	if (pm_ctx->unsafe_channel_count == 0) {
		policy_mgr_debug("There are no unsafe channels");
		return;
	}

	qdf_mem_copy(current_channel_list, pcl_channels,
		     current_channel_count * sizeof(*current_channel_list));
	qdf_mem_zero(pcl_channels,
		     current_channel_count * sizeof(*pcl_channels));

	qdf_mem_copy(org_weight_list, weight_list, NUM_CHANNELS);
	qdf_mem_zero(weight_list, weight_len);

	policy_mgr_get_sta_sap_scc_lte_coex_chnl(psoc, &scc_on_lte_coex);
	for (i = 0; i < current_channel_count; i++) {
		is_unsafe = 0;
		for (j = 0; j < pm_ctx->unsafe_channel_count; j++) {
			if (current_channel_list[i] ==
				pm_ctx->unsafe_channel_list[j]) {
				/* Found unsafe channel, update it */
				is_unsafe = 1;
				policy_mgr_debug("CH %d is not safe",
					current_channel_list[i]);
				break;
			}
		}
		if (is_unsafe && scc_on_lte_coex &&
		    policy_mgr_is_sta_sap_scc(psoc, current_channel_list[i])) {
			policy_mgr_debug("CH %d unsafe ingored when STA present on it",
					 current_channel_list[i]);
			is_unsafe = 0;
		}

		if (!is_unsafe) {
			pcl_channels[safe_channel_count] =
				current_channel_list[i];
			if (safe_channel_count < weight_len)
				weight_list[safe_channel_count] =
					org_weight_list[i];
			safe_channel_count++;
		}
	}
	*len = safe_channel_count;

	return;
}

static QDF_STATUS policy_mgr_modify_pcl_based_on_enabled_channels(
					struct policy_mgr_psoc_priv_obj *pm_ctx,
					uint32_t *pcl_list_org,
					uint8_t *weight_list_org,
					uint32_t *pcl_len_org)
{
	uint32_t i, pcl_len = 0;

	for (i = 0; i < *pcl_len_org; i++) {
		if (!wlan_reg_is_passive_or_disable_for_freq(
			pm_ctx->pdev, pcl_list_org[i])) {
			pcl_list_org[pcl_len] = pcl_list_org[i];
			weight_list_org[pcl_len++] = weight_list_org[i];
		}
	}
	*pcl_len_org = pcl_len;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS policy_mgr_modify_pcl_based_on_dnbs(
						struct wlan_objmgr_psoc *psoc,
						uint32_t *pcl_list_org,
						uint8_t *weight_list_org,
						uint32_t *pcl_len_org)
{
	uint32_t i, pcl_len = 0;
	uint32_t pcl_list[NUM_CHANNELS];
	uint8_t weight_list[NUM_CHANNELS];
	bool ok;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (*pcl_len_org > NUM_CHANNELS) {
		policy_mgr_err("Invalid PCL List Length %d", *pcl_len_org);
		return status;
	}
	for (i = 0; i < *pcl_len_org; i++) {
		status = policy_mgr_is_chan_ok_for_dnbs(psoc, pcl_list_org[i],
							&ok);

		if (QDF_IS_STATUS_ERROR(status)) {
			policy_mgr_err("Not able to check DNBS eligibility");
			return status;
		}
		if (ok) {
			pcl_list[pcl_len] = pcl_list_org[i];
			weight_list[pcl_len++] = weight_list_org[i];
		}
	}

	qdf_mem_zero(pcl_list_org, *pcl_len_org * sizeof(*pcl_list_org));
	qdf_mem_zero(weight_list_org, *pcl_len_org);
	qdf_mem_copy(pcl_list_org, pcl_list, pcl_len * sizeof(*pcl_list_org));
	qdf_mem_copy(weight_list_org, weight_list, pcl_len);
	*pcl_len_org = pcl_len;

	return QDF_STATUS_SUCCESS;
}

uint32_t policy_mgr_get_channel(struct wlan_objmgr_psoc *psoc,
				enum policy_mgr_con_mode mode,
				uint32_t *vdev_id)
{
	uint32_t idx = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return 0;
	}

	if (mode >= PM_MAX_NUM_OF_MODE) {
		policy_mgr_err("incorrect mode");
		return 0;
	}

	for (idx = 0; idx < MAX_NUMBER_OF_CONC_CONNECTIONS; idx++) {
		qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
		if ((pm_conc_connection_list[idx].mode == mode) &&
				(!vdev_id || (*vdev_id ==
					pm_conc_connection_list[idx].vdev_id))
				&& pm_conc_connection_list[idx].in_use) {
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			return pm_conc_connection_list[idx].freq;
		}
		qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	}

	return 0;
}

QDF_STATUS policy_mgr_skip_dfs_ch(struct wlan_objmgr_psoc *psoc,
				  bool *skip_dfs_channel)
{
	bool sta_sap_scc_on_dfs_chan;
	bool dfs_master_capable;
	QDF_STATUS status;

	status = ucfg_mlme_get_dfs_master_capability(psoc,
						     &dfs_master_capable);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get dfs master capable");
		return status;
	}

	*skip_dfs_channel = false;
	if (!dfs_master_capable) {
		policy_mgr_debug("skip DFS ch for SAP/Go dfs master cap %d",
				 dfs_master_capable);
		*skip_dfs_channel = true;
		return QDF_STATUS_SUCCESS;
	}

	sta_sap_scc_on_dfs_chan =
		policy_mgr_is_sta_sap_scc_allowed_on_dfs_chan(psoc);

	if (policy_mgr_is_hw_dbs_capable(psoc)) {
		if ((policy_mgr_is_special_mode_active_5g(psoc,
							  PM_P2P_CLIENT_MODE) ||
		     policy_mgr_is_special_mode_active_5g(psoc, PM_STA_MODE)) &&
		    !sta_sap_scc_on_dfs_chan) {
			policy_mgr_debug("skip DFS ch from pcl for DBS SAP/Go");
			*skip_dfs_channel = true;
		}
	} else {
		if ((policy_mgr_mode_specific_connection_count(psoc,
							       PM_STA_MODE,
							       NULL) > 0) &&
		    !sta_sap_scc_on_dfs_chan) {
			policy_mgr_debug("skip DFS ch from pcl for non-DBS SAP/Go");
			*skip_dfs_channel = true;
		}
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * policy_mgr_modify_sap_pcl_based_on_dfs() - filter out DFS channel if needed
 * @psoc: pointer to soc
 * @pcl_list_org: channel list to filter out
 * @weight_list_org: weight of channel list
 * @pcl_len_org: length of channel list
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS policy_mgr_modify_sap_pcl_based_on_dfs(
		struct wlan_objmgr_psoc *psoc,
		uint32_t *pcl_list_org,
		uint8_t *weight_list_org,
		uint32_t *pcl_len_org)
{
	size_t i, pcl_len = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool skip_dfs_channel = false;
	QDF_STATUS status;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}
	if (*pcl_len_org > NUM_CHANNELS) {
		policy_mgr_err("Invalid PCL List Length %d", *pcl_len_org);
		return QDF_STATUS_E_FAILURE;
	}

	status = policy_mgr_skip_dfs_ch(psoc, &skip_dfs_channel);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get dfs channel skip info");
		return status;
	}

	if (!skip_dfs_channel) {
		policy_mgr_debug("No more operation on DFS channel");
		return QDF_STATUS_SUCCESS;
	}

	for (i = 0; i < *pcl_len_org; i++) {
		if (!wlan_reg_is_dfs_in_secondary_list_for_freq(
							pm_ctx->pdev,
							pcl_list_org[i])) {
			pcl_list_org[pcl_len] = pcl_list_org[i];
			weight_list_org[pcl_len++] = weight_list_org[i];
		}
	}

	*pcl_len_org = pcl_len;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS policy_mgr_modify_sap_pcl_based_on_nol(
		struct wlan_objmgr_psoc *psoc,
		uint32_t *pcl_list_org,
		uint8_t *weight_list_org,
		uint32_t *pcl_len_org)
{
	uint32_t i, pcl_len = 0;
	uint32_t pcl_list[NUM_CHANNELS];
	uint8_t weight_list[NUM_CHANNELS];
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}
	if (*pcl_len_org > NUM_CHANNELS) {
		policy_mgr_err("Invalid PCL List Length %d", *pcl_len_org);
		return QDF_STATUS_E_FAILURE;
	}

	for (i = 0; i < *pcl_len_org; i++) {
		if (!wlan_reg_is_disable_in_secondary_list_for_freq(
		    pm_ctx->pdev, pcl_list_org[i])) {
			pcl_list[pcl_len] = pcl_list_org[i];
			weight_list[pcl_len++] = weight_list_org[i];
		}
	}

	qdf_mem_zero(pcl_list_org, *pcl_len_org * sizeof(*pcl_list_org));
	qdf_mem_zero(weight_list_org, *pcl_len_org);
	qdf_mem_copy(pcl_list_org, pcl_list, pcl_len * sizeof(*pcl_list_org));
	qdf_mem_copy(weight_list_org, weight_list, pcl_len);
	*pcl_len_org = pcl_len;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
policy_mgr_modify_pcl_based_on_srd(struct wlan_objmgr_psoc *psoc,
				   uint32_t *pcl_list_org,
				   uint8_t *weight_list_org,
				   uint32_t *pcl_len_org)
{
	uint32_t i, pcl_len = 0;
	uint32_t pcl_list[NUM_CHANNELS];
	uint8_t weight_list[NUM_CHANNELS];
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	if (*pcl_len_org > NUM_CHANNELS) {
		policy_mgr_err("Invalid PCL List Length %d", *pcl_len_org);
		return QDF_STATUS_E_FAILURE;
	}
	for (i = 0; i < *pcl_len_org; i++) {
		if (wlan_reg_is_etsi13_srd_chan_for_freq(
		    pm_ctx->pdev, pcl_list_org[i]))
			continue;
		pcl_list[pcl_len] = pcl_list_org[i];
		weight_list[pcl_len++] = weight_list_org[i];
	}

	qdf_mem_zero(pcl_list_org, *pcl_len_org * sizeof(*pcl_list_org));
	qdf_mem_zero(weight_list_org, *pcl_len_org);
	qdf_mem_copy(pcl_list_org, pcl_list, pcl_len * sizeof(*pcl_list_org));
	qdf_mem_copy(weight_list_org, weight_list, pcl_len);
	*pcl_len_org = pcl_len;

	return QDF_STATUS_SUCCESS;
}

/**
 * policy_mgr_modify_pcl_based_on_indoor() - filter out indoor channel if needed
 * @psoc: pointer to soc
 * @pcl_list_org: channel list to filter out
 * @weight_list_org: weight of channel list
 * @pcl_len_org: length of channel list
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
policy_mgr_modify_pcl_based_on_indoor(struct wlan_objmgr_psoc *psoc,
				      uint32_t *pcl_list_org,
				      uint8_t *weight_list_org,
				      uint32_t *pcl_len_org)
{
	uint32_t i, pcl_len = 0;
	uint32_t pcl_list[NUM_CHANNELS];
	uint8_t weight_list[NUM_CHANNELS];
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool include_indoor_channel;
	QDF_STATUS status;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	if (*pcl_len_org > NUM_CHANNELS) {
		policy_mgr_err("Invalid PCL List Length %d", *pcl_len_org);
		return QDF_STATUS_E_FAILURE;
	}

	status = ucfg_mlme_get_indoor_channel_support(psoc,
						      &include_indoor_channel);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get indoor channel skip info");
		return status;
	}

	if (include_indoor_channel) {
		policy_mgr_debug("No more operation on indoor channel");
		return QDF_STATUS_SUCCESS;
	}

	for (i = 0; i < *pcl_len_org; i++) {
		if (wlan_reg_is_freq_indoor_in_secondary_list(pm_ctx->pdev,
							      pcl_list_org[i]))
			continue;
		pcl_list[pcl_len] = pcl_list_org[i];
		weight_list[pcl_len++] = weight_list_org[i];
	}

	qdf_mem_zero(pcl_list_org, *pcl_len_org * sizeof(*pcl_list_org));
	qdf_mem_zero(weight_list_org, *pcl_len_org);
	qdf_mem_copy(pcl_list_org, pcl_list, pcl_len * sizeof(*pcl_list_org));
	qdf_mem_copy(weight_list_org, weight_list, pcl_len);
	*pcl_len_org = pcl_len;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS policy_mgr_pcl_modification_for_sap(
			struct wlan_objmgr_psoc *psoc,
			uint32_t *pcl_channels, uint8_t *pcl_weight,
			uint32_t *len)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	bool mandatory_modified_pcl = false;
	bool nol_modified_pcl = false;
	bool dfs_modified_pcl = false;
	bool indoor_modified_pcl = false;
	bool modified_final_pcl = false;
	bool srd_chan_enabled;

	if (policy_mgr_is_sap_mandatory_channel_set(psoc)) {
		status = policy_mgr_modify_sap_pcl_based_on_mandatory_channel(
				psoc, pcl_channels, pcl_weight, len);
		if (QDF_IS_STATUS_ERROR(status)) {
			policy_mgr_err(
				"failed to get mandatory modified pcl for SAP");
			return status;
		}
		mandatory_modified_pcl = true;
	}

	status = policy_mgr_modify_sap_pcl_based_on_nol(
			psoc, pcl_channels, pcl_weight, len);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get nol modified pcl for SAP");
		return status;
	}
	nol_modified_pcl = true;

	status = policy_mgr_modify_sap_pcl_based_on_dfs(
			psoc, pcl_channels, pcl_weight, len);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get dfs modified pcl for SAP");
		return status;
	}
	dfs_modified_pcl = true;

	wlan_mlme_get_srd_master_mode_for_vdev(psoc, QDF_SAP_MODE,
					       &srd_chan_enabled);

	if (!srd_chan_enabled) {
		status = policy_mgr_modify_pcl_based_on_srd
				(psoc, pcl_channels, pcl_weight, len);
		if (QDF_IS_STATUS_ERROR(status)) {
			policy_mgr_err("Failed to modify SRD in pcl for SAP");
			return status;
		}
	}

	status = policy_mgr_modify_pcl_based_on_indoor(psoc, pcl_channels,
						       pcl_weight, len);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get indoor modified pcl for SAP");
		return status;
	}
	indoor_modified_pcl = true;

	modified_final_pcl = true;
	policy_mgr_debug(" %d %d %d %d %d",
			 mandatory_modified_pcl,
			 nol_modified_pcl,
			 dfs_modified_pcl,
			 indoor_modified_pcl,
			 modified_final_pcl);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS policy_mgr_pcl_modification_for_p2p_go(
			struct wlan_objmgr_psoc *psoc,
			uint32_t *pcl_channels, uint8_t *pcl_weight,
			uint32_t *len)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool srd_chan_enabled;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("context is NULL");
		return status;
	}

	status = policy_mgr_modify_pcl_based_on_enabled_channels(
			pm_ctx, pcl_channels, pcl_weight, len);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get modified pcl for GO");
		return status;
	}

	wlan_mlme_get_srd_master_mode_for_vdev(psoc, QDF_P2P_GO_MODE,
					       &srd_chan_enabled);

	if (!srd_chan_enabled) {
		status = policy_mgr_modify_pcl_based_on_srd
				(psoc, pcl_channels, pcl_weight, len);
		if (QDF_IS_STATUS_ERROR(status)) {
			policy_mgr_err("Failed to modify SRD in pcl for GO");
			return status;
		}
	}

	policy_mgr_dump_channel_list(*len, pcl_channels, pcl_weight);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS policy_mgr_mode_specific_modification_on_pcl(
			struct wlan_objmgr_psoc *psoc,
			uint32_t *pcl_channels, uint8_t *pcl_weight,
			uint32_t *len, enum policy_mgr_con_mode mode)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	switch (mode) {
	case PM_SAP_MODE:
		status = policy_mgr_pcl_modification_for_sap(
			psoc, pcl_channels, pcl_weight, len);
		break;
	case PM_P2P_GO_MODE:
		status = policy_mgr_pcl_modification_for_p2p_go(
			psoc, pcl_channels, pcl_weight, len);
		break;
	case PM_STA_MODE:
	case PM_P2P_CLIENT_MODE:
	case PM_NAN_DISC_MODE:
		status = QDF_STATUS_SUCCESS;
		break;
	default:
		policy_mgr_err("unexpected mode %d", mode);
		break;
	}

	return status;
}

#ifdef FEATURE_FOURTH_CONNECTION
static enum policy_mgr_pcl_type policy_mgr_get_pcl_4_port(
				struct wlan_objmgr_psoc *psoc,
				enum policy_mgr_con_mode mode,
				enum policy_mgr_conc_priority_mode pref)
{
	enum policy_mgr_three_connection_mode fourth_index = 0;
	enum policy_mgr_pcl_type pcl;

	/* Will be enhanced for other types of 4 port conc (NaN etc.)
	 * in future.
	 */
	if (!policy_mgr_is_hw_dbs_capable(psoc)) {
		policy_mgr_err("Can't find index for 4th port pcl table for non dbs capable");
		return PM_MAX_PCL_TYPE;
	}

	/* SAP and P2P Go have same result in 4th port pcl table */
	if (mode == PM_SAP_MODE || mode == PM_P2P_GO_MODE) {
		mode = PM_SAP_MODE;
	}

	if (mode != PM_STA_MODE && mode != PM_SAP_MODE &&
	    mode != PM_NDI_MODE) {
		policy_mgr_err("Can't start 4th port if not STA, SAP, NDI");
		return PM_MAX_PCL_TYPE;
	}

	fourth_index =
		policy_mgr_get_fourth_connection_pcl_table_index(psoc);
	if (PM_MAX_THREE_CONNECTION_MODE == fourth_index) {
		policy_mgr_err("Can't find index for 4th port pcl table");
		return PM_MAX_PCL_TYPE;
	}
	policy_mgr_debug("Index for 4th port pcl table: %d", fourth_index);

	pcl = fourth_connection_pcl_dbs_table[fourth_index][mode][pref];

	return pcl;
}
#else
static inline enum policy_mgr_pcl_type policy_mgr_get_pcl_4_port(
				struct wlan_objmgr_psoc *psoc,
				enum policy_mgr_con_mode mode,
				enum policy_mgr_conc_priority_mode pref)
{return PM_MAX_PCL_TYPE; }
#endif

QDF_STATUS policy_mgr_get_pcl(struct wlan_objmgr_psoc *psoc,
			      enum policy_mgr_con_mode mode,
			      uint32_t *pcl_channels, uint32_t *len,
			      uint8_t *pcl_weight, uint32_t weight_len)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint32_t num_connections = 0;
	enum policy_mgr_conc_priority_mode first_index = 0;
	enum policy_mgr_one_connection_mode second_index = 0;
	enum policy_mgr_two_connection_mode third_index = 0;
	enum policy_mgr_pcl_type pcl = PM_NONE;
	enum policy_mgr_conc_priority_mode conc_system_pref = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	enum QDF_OPMODE qdf_mode;
	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("context is NULL");
		return status;
	}

	if ((mode < 0) || (mode >= PM_MAX_NUM_OF_MODE)) {
		policy_mgr_err("Invalid connection mode %d received", mode);
		return status;
	}

	/* find the current connection state from pm_conc_connection_list*/
	num_connections = policy_mgr_get_connection_count(psoc);
	policy_mgr_debug("connections:%d pref:%d requested mode:%d",
		num_connections, pm_ctx->cur_conc_system_pref, mode);

	switch (pm_ctx->cur_conc_system_pref) {
	case 0:
		conc_system_pref = PM_THROUGHPUT;
		break;
	case 1:
		conc_system_pref = PM_POWERSAVE;
		break;
	case 2:
		conc_system_pref = PM_LATENCY;
		break;
	default:
		policy_mgr_err("unknown cur_conc_system_pref value %d",
			pm_ctx->cur_conc_system_pref);
		break;
	}

	switch (num_connections) {
	case 0:
		first_index =
			policy_mgr_get_first_connection_pcl_table_index(psoc);
		pcl = first_connection_pcl_table[mode][first_index];
		break;
	case 1:
		second_index =
			policy_mgr_get_second_connection_pcl_table_index(psoc);
		if (PM_MAX_ONE_CONNECTION_MODE == second_index) {
			policy_mgr_err("couldn't find index for 2nd connection pcl table");
			return status;
		}
		qdf_mode = policy_mgr_get_qdf_mode_from_pm(mode);
		if (qdf_mode == QDF_MAX_NO_OF_MODE)
			return status;

		if (policy_mgr_is_hw_dbs_capable(psoc) == true &&
		    policy_mgr_is_dbs_allowed_for_concurrency(
							psoc, qdf_mode)) {
			pcl = (*second_connection_pcl_dbs_table)
				[second_index][mode][conc_system_pref];
		} else {
			pcl = (*second_connection_pcl_non_dbs_table)
				[second_index][mode][conc_system_pref];
		}

		break;
	case 2:
		third_index =
			policy_mgr_get_third_connection_pcl_table_index(psoc);
		if (PM_MAX_TWO_CONNECTION_MODE == third_index) {
			policy_mgr_err(
				"couldn't find index for 3rd connection pcl table");
			return status;
		}
		if (policy_mgr_is_hw_dbs_capable(psoc) == true) {
			pcl = (*third_connection_pcl_dbs_table)
				[third_index][mode][conc_system_pref];
		} else {
			pcl = (*third_connection_pcl_non_dbs_table)
				[third_index][mode][conc_system_pref];
		}
		break;
	case 3:
		pcl = policy_mgr_get_pcl_4_port(psoc, mode, conc_system_pref);
		break;
	default:
		policy_mgr_err("unexpected num_connections value %d",
			num_connections);
		break;
	}

	/* once the PCL enum is obtained find out the exact channel list with
	 * help from sme_get_cfg_valid_channels
	 */
	status = policy_mgr_get_channel_list(psoc, pcl, pcl_channels, len, mode,
					pcl_weight, weight_len);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get channel list:%d", status);
		return status;
	}

	policy_mgr_dump_channel_list(*len, pcl_channels, pcl_weight);

	policy_mgr_mode_specific_modification_on_pcl(
		psoc, pcl_channels, pcl_weight, len, mode);

	status = policy_mgr_modify_pcl_based_on_dnbs(psoc, pcl_channels,
						pcl_weight, len);

	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get modified pcl based on DNBS");
		return status;
	}
	return QDF_STATUS_SUCCESS;
}

enum policy_mgr_conc_priority_mode
		policy_mgr_get_first_connection_pcl_table_index(
		struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("context is NULL");
		return PM_THROUGHPUT;
	}

	if (pm_ctx->cur_conc_system_pref >= PM_MAX_CONC_PRIORITY_MODE)
		return PM_THROUGHPUT;

	return pm_ctx->cur_conc_system_pref;
}

enum policy_mgr_one_connection_mode
		policy_mgr_get_second_connection_pcl_table_index(
		struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_one_connection_mode index = PM_MAX_ONE_CONNECTION_MODE;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return index;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	if (PM_STA_MODE == pm_conc_connection_list[0].mode) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq)) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_STA_24_1x1;
			else
				index = PM_STA_24_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_STA_5_1x1;
			else
				index = PM_STA_5_2x2;
		}
	} else if (PM_SAP_MODE == pm_conc_connection_list[0].mode) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq)) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_SAP_24_1x1;
			else
				index = PM_SAP_24_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_SAP_5_1x1;
			else
				index = PM_SAP_5_2x2;
		}
	} else if (PM_P2P_CLIENT_MODE == pm_conc_connection_list[0].mode) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq)) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_CLI_24_1x1;
			else
				index = PM_P2P_CLI_24_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_CLI_5_1x1;
			else
				index = PM_P2P_CLI_5_2x2;
		}
	} else if (PM_P2P_GO_MODE == pm_conc_connection_list[0].mode) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq)) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_24_1x1;
			else
				index = PM_P2P_GO_24_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_5_1x1;
			else
				index = PM_P2P_GO_5_2x2;
		}
	} else if (PM_NAN_DISC_MODE == pm_conc_connection_list[0].mode) {
		if (POLICY_MGR_ONE_ONE == pm_conc_connection_list[0].chain_mask)
			index = PM_NAN_DISC_24_1x1;
		else
			index = PM_NAN_DISC_24_2x2;
	}

	policy_mgr_debug("mode:%d freq:%d chain:%d index:%d",
			 pm_conc_connection_list[0].mode,
			 pm_conc_connection_list[0].freq,
			 pm_conc_connection_list[0].chain_mask, index);

	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return index;
}

static enum policy_mgr_two_connection_mode
		policy_mgr_get_third_connection_pcl_table_index_cli_sap(void)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;
	/* SCC */
	if (pm_conc_connection_list[0].freq ==
		pm_conc_connection_list[1].freq) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[0].freq)) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_CLI_SAP_SCC_24_1x1;
			else
				index = PM_P2P_CLI_SAP_SCC_24_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_CLI_SAP_SCC_5_1x1;
			else
				index = PM_P2P_CLI_SAP_SCC_5_2x2;
		}
	/* MCC */
	} else if (pm_conc_connection_list[0].mac ==
		pm_conc_connection_list[1].mac) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq) &&
		    WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[1].freq)) {
			if (POLICY_MGR_ONE_ONE ==
			pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_CLI_SAP_MCC_24_1x1;
			else
				index = PM_P2P_CLI_SAP_MCC_24_2x2;
		} else if ((WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[0].freq)) &&
			(WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[1].freq))) {
			if (POLICY_MGR_ONE_ONE ==
			pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_CLI_SAP_MCC_5_1x1;
			else
				index = PM_P2P_CLI_SAP_MCC_5_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
			pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_CLI_SAP_MCC_24_5_1x1;
			else
				index = PM_P2P_CLI_SAP_MCC_24_5_2x2;
		}
	/* SBS or DBS */
	} else if (pm_conc_connection_list[0].mac !=
			pm_conc_connection_list[1].mac) {
		/* SBS */
		if ((WLAN_REG_IS_5GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq)) &&
		    (WLAN_REG_IS_5GHZ_CH_FREQ(
		    pm_conc_connection_list[1].freq))) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_CLI_SAP_SBS_5_1x1;
		} else {
		/* DBS */
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_CLI_SAP_DBS_1x1;
			else
				index = PM_P2P_CLI_SAP_DBS_2x2;
		}
	}
	return index;
}

static enum policy_mgr_two_connection_mode
		policy_mgr_get_third_connection_pcl_table_index_sta_sap(void)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;
	/* SCC */
	if (pm_conc_connection_list[0].freq ==
		pm_conc_connection_list[1].freq) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[0].freq)) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_STA_SAP_SCC_24_1x1;
			else
				index = PM_STA_SAP_SCC_24_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_STA_SAP_SCC_5_1x1;
			else
				index = PM_STA_SAP_SCC_5_2x2;
		}
	/* MCC */
	} else if (pm_conc_connection_list[0].mac ==
		pm_conc_connection_list[1].mac) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq) &&
		    WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[1].freq)) {
			if (POLICY_MGR_ONE_ONE ==
			pm_conc_connection_list[0].chain_mask)
				index = PM_STA_SAP_MCC_24_1x1;
			else
				index = PM_STA_SAP_MCC_24_2x2;
		} else if (WLAN_REG_IS_5GHZ_CH_FREQ(
			   pm_conc_connection_list[0].freq) &&
			   WLAN_REG_IS_5GHZ_CH_FREQ(
			   pm_conc_connection_list[1].freq)) {
			if (POLICY_MGR_ONE_ONE ==
			pm_conc_connection_list[0].chain_mask)
				index = PM_STA_SAP_MCC_5_1x1;
			else
				index = PM_STA_SAP_MCC_5_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
			pm_conc_connection_list[0].chain_mask)
				index = PM_STA_SAP_MCC_24_5_1x1;
			else
				index = PM_STA_SAP_MCC_24_5_2x2;
		}
	/* SBS or DBS */
	} else if (pm_conc_connection_list[0].mac !=
			pm_conc_connection_list[1].mac) {
		/* SBS */
		if ((WLAN_REG_IS_5GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq)) &&
		    (WLAN_REG_IS_5GHZ_CH_FREQ(
		    pm_conc_connection_list[1].freq))) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_STA_SAP_SBS_5_1x1;
		} else {
		/* DBS */
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_STA_SAP_DBS_1x1;
			else
				index = PM_STA_SAP_DBS_2x2;
		}
	}
	return index;
}

static enum policy_mgr_two_connection_mode
		policy_mgr_get_third_connection_pcl_table_index_sap_sap(void)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;
	/* SCC */
	if (pm_conc_connection_list[0].freq ==
		pm_conc_connection_list[1].freq) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[0].freq)) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_SAP_SAP_SCC_24_1x1;
			else
				index = PM_SAP_SAP_SCC_24_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_SAP_SAP_SCC_5_1x1;
			else
				index = PM_SAP_SAP_SCC_5_2x2;
		}
	/* MCC */
	} else if (pm_conc_connection_list[0].mac ==
		pm_conc_connection_list[1].mac) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq) &&
		    WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[1].freq)) {
			if (POLICY_MGR_ONE_ONE ==
			pm_conc_connection_list[0].chain_mask)
				index = PM_SAP_SAP_MCC_24_1x1;
			else
				index = PM_SAP_SAP_MCC_24_2x2;
		} else if (WLAN_REG_IS_5GHZ_CH_FREQ(
			   pm_conc_connection_list[0].freq) &&
			   WLAN_REG_IS_5GHZ_CH_FREQ(
			   pm_conc_connection_list[1].freq)) {
			if (POLICY_MGR_ONE_ONE ==
			pm_conc_connection_list[0].chain_mask)
				index = PM_SAP_SAP_MCC_5_1x1;
			else
				index = PM_SAP_SAP_MCC_5_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
			pm_conc_connection_list[0].chain_mask)
				index = PM_SAP_SAP_MCC_24_5_1x1;
			else
				index = PM_SAP_SAP_MCC_24_5_2x2;
		}
	/* SBS or DBS */
	} else if (pm_conc_connection_list[0].mac !=
			pm_conc_connection_list[1].mac) {
		/* SBS */
		if (WLAN_REG_IS_5GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq) &&
		    WLAN_REG_IS_5GHZ_CH_FREQ(
		    pm_conc_connection_list[1].freq)) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_SAP_SAP_SBS_5_1x1;
		} else {
		/* DBS */
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_SAP_SAP_DBS_1x1;
			else
				index = PM_SAP_SAP_DBS_2x2;
		}
	}
	return index;
}

static enum policy_mgr_two_connection_mode
		policy_mgr_get_third_connection_pcl_table_index_sta_go(void)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;
	/* SCC */
	if (pm_conc_connection_list[0].freq ==
	    pm_conc_connection_list[1].freq) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq)) {
			if (POLICY_MGR_ONE_ONE ==
			    pm_conc_connection_list[0].chain_mask)
				index = PM_STA_P2P_GO_SCC_24_1x1;
			else
				index = PM_STA_P2P_GO_SCC_24_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
			    pm_conc_connection_list[0].chain_mask)
				index = PM_STA_P2P_GO_SCC_5_1x1;
			else
				index = PM_STA_P2P_GO_SCC_5_2x2;
		}
	/* MCC */
	} else if (pm_conc_connection_list[0].mac ==
		pm_conc_connection_list[1].mac) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq) &&
		    WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[1].freq)) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_STA_P2P_GO_MCC_24_1x1;
			else
				index = PM_STA_P2P_GO_MCC_24_2x2;
		} else if (WLAN_REG_IS_5GHZ_CH_FREQ(
			   pm_conc_connection_list[0].freq) &&
			   WLAN_REG_IS_5GHZ_CH_FREQ(
			   pm_conc_connection_list[1].freq)) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_STA_P2P_GO_MCC_5_1x1;
			else
				index = PM_STA_P2P_GO_MCC_5_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
			pm_conc_connection_list[0].chain_mask)
				index = PM_STA_P2P_GO_MCC_24_5_1x1;
			else
				index = PM_STA_P2P_GO_MCC_24_5_2x2;
		}
	/* SBS or DBS */
	} else if (pm_conc_connection_list[0].mac !=
			pm_conc_connection_list[1].mac) {
		/* SBS */
		if ((WLAN_REG_IS_5GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq)) &&
		    (WLAN_REG_IS_5GHZ_CH_FREQ(
		    pm_conc_connection_list[1].freq))) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_STA_P2P_GO_SBS_5_1x1;
		} else {
		/* DBS */
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_STA_P2P_GO_DBS_1x1;
			else
				index = PM_STA_P2P_GO_DBS_2x2;
		}
	}
	return index;
}

static enum policy_mgr_two_connection_mode
		policy_mgr_get_third_connection_pcl_table_index_sta_cli(void)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;
	/* SCC */
	if (pm_conc_connection_list[0].freq ==
	    pm_conc_connection_list[1].freq) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq)) {
			if (POLICY_MGR_ONE_ONE ==
			    pm_conc_connection_list[0].chain_mask)
				index = PM_STA_P2P_CLI_SCC_24_1x1;
			else
				index = PM_STA_P2P_CLI_SCC_24_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
			    pm_conc_connection_list[0].chain_mask)
				index = PM_STA_P2P_CLI_SCC_5_1x1;
			else
				index = PM_STA_P2P_CLI_SCC_5_2x2;
		}
	/* MCC */
	} else if (pm_conc_connection_list[0].mac ==
		pm_conc_connection_list[1].mac) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq) &&
		    WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[1].freq)) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_STA_P2P_CLI_MCC_24_1x1;
			else
				index = PM_STA_P2P_CLI_MCC_24_2x2;
		} else if ((WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[0].freq)) &&
			(WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[1].freq))) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_STA_P2P_CLI_MCC_5_1x1;
			else
				index = PM_STA_P2P_CLI_MCC_5_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_STA_P2P_CLI_MCC_24_5_1x1;
			else
				index = PM_STA_P2P_CLI_MCC_24_5_2x2;
		}
	/* SBS or DBS */
	} else if (pm_conc_connection_list[0].mac !=
			pm_conc_connection_list[1].mac) {
		/* SBS */
		if ((WLAN_REG_IS_5GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq)) &&
		    (WLAN_REG_IS_5GHZ_CH_FREQ(
		    pm_conc_connection_list[1].freq))) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_STA_P2P_CLI_SBS_5_1x1;
		} else {
		/* DBS */
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_STA_P2P_CLI_DBS_1x1;
			else
				index = PM_STA_P2P_CLI_DBS_2x2;
		}
	}
	return index;
}

static enum policy_mgr_two_connection_mode
		policy_mgr_get_third_connection_pcl_table_index_go_cli(void)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;
	/* SCC */
	if (pm_conc_connection_list[0].freq ==
		pm_conc_connection_list[1].freq) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[0].freq)) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_P2P_CLI_SCC_24_1x1;
			else
				index = PM_P2P_GO_P2P_CLI_SCC_24_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_P2P_CLI_SCC_5_1x1;
			else
				index = PM_P2P_GO_P2P_CLI_SCC_5_2x2;
		}
	/* MCC */
	} else if (pm_conc_connection_list[0].mac ==
		pm_conc_connection_list[1].mac) {
		if ((WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[0].freq)) &&
			(WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[1].freq))) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_P2P_CLI_MCC_24_1x1;
			else
				index = PM_P2P_GO_P2P_CLI_MCC_24_2x2;
		} else if ((WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[0].freq)) &&
			(WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[1].freq))) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_P2P_CLI_MCC_5_1x1;
			else
				index = PM_P2P_GO_P2P_CLI_MCC_5_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_P2P_CLI_MCC_24_5_1x1;
			else
				index = PM_P2P_GO_P2P_CLI_MCC_24_5_2x2;
		}
	/* SBS or DBS */
	} else if (pm_conc_connection_list[0].mac !=
			pm_conc_connection_list[1].mac) {
		/* SBS */
		if ((WLAN_REG_IS_5GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq)) &&
		    (WLAN_REG_IS_5GHZ_CH_FREQ(
		    pm_conc_connection_list[1].freq))) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_P2P_CLI_SBS_5_1x1;
		} else {
		/* DBS */
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_P2P_CLI_DBS_1x1;
			else
				index = PM_P2P_GO_P2P_CLI_DBS_2x2;
		}
	}
	return index;
}

static enum policy_mgr_two_connection_mode
		policy_mgr_get_third_connection_pcl_table_index_go_sap(void)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;
	/* SCC */
	if (pm_conc_connection_list[0].freq ==
		pm_conc_connection_list[1].freq) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[0].freq)) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_SAP_SCC_24_1x1;
			else
				index = PM_P2P_GO_SAP_SCC_24_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_SAP_SCC_5_1x1;
			else
				index = PM_P2P_GO_SAP_SCC_5_2x2;
		}
	/* MCC */
	} else if (pm_conc_connection_list[0].mac ==
		pm_conc_connection_list[1].mac) {
		if ((WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[0].freq)) &&
			(WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[1].freq))) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_SAP_MCC_24_1x1;
			else
				index = PM_P2P_GO_SAP_MCC_24_2x2;
		} else if ((WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[0].freq)) &&
			(WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[1].freq))) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_SAP_MCC_5_1x1;
			else
				index = PM_P2P_GO_SAP_MCC_5_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_SAP_MCC_24_5_1x1;
			else
				index = PM_P2P_GO_SAP_MCC_24_5_2x2;
		}
	/* SBS or DBS */
	} else if (pm_conc_connection_list[0].mac !=
			pm_conc_connection_list[1].mac) {
		/* SBS */
		if ((WLAN_REG_IS_5GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq)) &&
		    (WLAN_REG_IS_5GHZ_CH_FREQ(
		    pm_conc_connection_list[1].freq))) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_SAP_SBS_5_1x1;
		} else {
		/* DBS */
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_SAP_DBS_1x1;
			else
				index = PM_P2P_GO_SAP_DBS_2x2;
		}
	}
	return index;
}

static enum policy_mgr_two_connection_mode
		policy_mgr_get_third_connection_pcl_table_index_sta_sta(void)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;
	/* SCC */
	if (pm_conc_connection_list[0].freq ==
	    pm_conc_connection_list[1].freq) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq)) {
			if (POLICY_MGR_ONE_ONE ==
			    pm_conc_connection_list[0].chain_mask)
				index = PM_STA_STA_SCC_24_1x1;
			else
				index = PM_STA_STA_SCC_24_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
			    pm_conc_connection_list[0].chain_mask)
				index = PM_STA_STA_SCC_5_1x1;
			else
				index = PM_STA_STA_SCC_5_2x2;
		}
	/* MCC */
	} else if (pm_conc_connection_list[0].mac ==
		   pm_conc_connection_list[1].mac) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq) &&
		    WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[1].freq)) {
			if (POLICY_MGR_ONE_ONE ==
			    pm_conc_connection_list[0].chain_mask)
				index = PM_STA_STA_MCC_24_1x1;
			else
				index = PM_STA_STA_MCC_24_2x2;
		} else if (WLAN_REG_IS_5GHZ_CH_FREQ(
			   pm_conc_connection_list[0].freq) &&
			   WLAN_REG_IS_5GHZ_CH_FREQ(
			   pm_conc_connection_list[1].freq)) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_STA_STA_MCC_5_1x1;
			else
				index = PM_STA_STA_MCC_5_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_STA_STA_MCC_24_5_1x1;
			else
				index = PM_STA_STA_MCC_24_5_2x2;
		}
	/* SBS or DBS */
	} else if (pm_conc_connection_list[0].mac !=
			pm_conc_connection_list[1].mac) {
		/* SBS */
		if ((WLAN_REG_IS_5GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq)) &&
		    (WLAN_REG_IS_5GHZ_CH_FREQ(
		    pm_conc_connection_list[1].freq))) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_STA_STA_SBS_5_1x1;
		} else {
		/* DBS */
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_STA_STA_DBS_1x1;
			else
				index = PM_STA_STA_DBS_2x2;
		}
	}
	return index;
}

static enum policy_mgr_two_connection_mode
		policy_mgr_get_third_connection_pcl_table_index_go_go(void)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;
	/* SCC */
	if (pm_conc_connection_list[0].freq ==
		pm_conc_connection_list[1].freq) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[0].freq)) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_P2P_GO_SCC_24_1x1;
			else
				index = PM_P2P_GO_P2P_GO_SCC_24_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_P2P_GO_SCC_5_1x1;
			else
				index = PM_P2P_GO_P2P_GO_SCC_5_2x2;
		}
	/* MCC */
	} else if (pm_conc_connection_list[0].mac ==
		pm_conc_connection_list[1].mac) {
		if ((WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[0].freq)) &&
			(WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[1].freq))) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_P2P_GO_MCC_24_1x1;
			else
				index = PM_P2P_GO_P2P_GO_MCC_24_2x2;
		} else if ((WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[0].freq)) &&
			(WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[1].freq))) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_P2P_GO_MCC_5_1x1;
			else
				index = PM_P2P_GO_P2P_GO_MCC_5_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_P2P_GO_MCC_24_5_1x1;
			else
				index = PM_P2P_GO_P2P_GO_MCC_24_5_2x2;
		}
	/* SBS or DBS */
	} else if (pm_conc_connection_list[0].mac !=
			pm_conc_connection_list[1].mac) {
		/* SBS */
		if ((WLAN_REG_IS_5GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq)) &&
		    (WLAN_REG_IS_5GHZ_CH_FREQ(
		    pm_conc_connection_list[1].freq))) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_P2P_GO_SBS_5_1x1;
		} else {
		/* DBS */
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_P2P_GO_DBS_1x1;
			else
				index = PM_P2P_GO_P2P_GO_DBS_2x2;
		}
	}
	return index;
}

static enum policy_mgr_two_connection_mode
		policy_mgr_get_third_connection_pcl_table_index_nan_ndi(void)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;
	/* SCC */
	if (pm_conc_connection_list[0].freq ==
		pm_conc_connection_list[1].freq) {
		/* Policy mgr only considers NAN Disc ch in 2.4GHz */
		if (POLICY_MGR_ONE_ONE == pm_conc_connection_list[0].chain_mask)
			index = PM_NAN_DISC_NDI_SCC_24_1x1;
		else
			index = PM_NAN_DISC_NDI_SCC_24_2x2;
	/* MCC */
	} else if (pm_conc_connection_list[0].mac ==
		pm_conc_connection_list[1].mac) {
		/* Policy mgr only considers NAN Disc ch in 2.4GHz */
		if (POLICY_MGR_ONE_ONE == pm_conc_connection_list[0].chain_mask)
			index = PM_NAN_DISC_NDI_MCC_24_1x1;
		else
			index = PM_NAN_DISC_NDI_MCC_24_2x2;
	/* DBS */
	} else if (pm_conc_connection_list[0].mac !=
			pm_conc_connection_list[1].mac) {
		if (POLICY_MGR_ONE_ONE == pm_conc_connection_list[0].chain_mask)
			index = PM_NAN_DISC_NDI_DBS_1x1;
		else
			index = PM_NAN_DISC_NDI_DBS_2x2;
	}
	return index;
}

static enum policy_mgr_two_connection_mode
		policy_mgr_get_third_connection_pcl_table_index_sta_nan(void)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;
	/* SCC */
	if (pm_conc_connection_list[0].freq ==
		pm_conc_connection_list[1].freq) {
		/* Policy mgr only considers NAN Disc ch in 2.4GHz */
		if (POLICY_MGR_ONE_ONE == pm_conc_connection_list[0].chain_mask)
			index = PM_STA_NAN_DISC_SCC_24_1x1;
		else
			index = PM_STA_NAN_DISC_SCC_24_2x2;
	/* MCC */
	} else if (pm_conc_connection_list[0].mac ==
		pm_conc_connection_list[1].mac) {
		/* Policy mgr only considers NAN Disc ch in 2.4 GHz */
		if (POLICY_MGR_ONE_ONE == pm_conc_connection_list[0].chain_mask)
			index = PM_STA_NAN_DISC_MCC_24_1x1;
		else
			index = PM_STA_NAN_DISC_MCC_24_2x2;
	/* DBS */
	} else if (pm_conc_connection_list[0].mac !=
			pm_conc_connection_list[1].mac) {
		if (POLICY_MGR_ONE_ONE == pm_conc_connection_list[0].chain_mask)
			index = PM_STA_NAN_DISC_DBS_1x1;
		else
			index = PM_STA_NAN_DISC_DBS_2x2;
	}
	return index;
}

static enum policy_mgr_two_connection_mode
		policy_mgr_get_third_connection_pcl_table_index_sap_nan(void)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;
	/* SCC */
	if (pm_conc_connection_list[0].freq ==
		pm_conc_connection_list[1].freq) {
		/* Policy mgr only considers NAN Disc ch in 2.4 GHz */
		if (POLICY_MGR_ONE_ONE == pm_conc_connection_list[0].chain_mask)
			index = PM_SAP_NAN_DISC_SCC_24_1x1;
		else
			index = PM_SAP_NAN_DISC_SCC_24_2x2;
	/* MCC */
	} else if (pm_conc_connection_list[0].mac ==
		pm_conc_connection_list[1].mac) {
		/* Policy mgr only considers NAN Disc ch in 2.4GHz */
		if (POLICY_MGR_ONE_ONE == pm_conc_connection_list[0].chain_mask)
			index = PM_SAP_NAN_DISC_MCC_24_1x1;
		else
			index = PM_SAP_NAN_DISC_MCC_24_2x2;
	/* DBS */
	} else if (pm_conc_connection_list[0].mac !=
			pm_conc_connection_list[1].mac) {
		if (POLICY_MGR_ONE_ONE == pm_conc_connection_list[0].chain_mask)
			index = PM_SAP_NAN_DISC_DBS_1x1;
		else
			index = PM_SAP_NAN_DISC_DBS_2x2;
	}
	return index;
}

enum policy_mgr_two_connection_mode
		policy_mgr_get_third_connection_pcl_table_index(
		struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return index;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	if (((PM_P2P_CLIENT_MODE == pm_conc_connection_list[0].mode) &&
		(PM_SAP_MODE == pm_conc_connection_list[1].mode)) ||
		((PM_SAP_MODE == pm_conc_connection_list[0].mode) &&
		(PM_P2P_CLIENT_MODE == pm_conc_connection_list[1].mode)))
		index =
		policy_mgr_get_third_connection_pcl_table_index_cli_sap();
	else if (((PM_STA_MODE == pm_conc_connection_list[0].mode) &&
		(PM_SAP_MODE == pm_conc_connection_list[1].mode)) ||
		((PM_SAP_MODE == pm_conc_connection_list[0].mode) &&
		(PM_STA_MODE == pm_conc_connection_list[1].mode)))
		index =
		policy_mgr_get_third_connection_pcl_table_index_sta_sap();
	else if ((PM_SAP_MODE == pm_conc_connection_list[0].mode) &&
		(PM_SAP_MODE == pm_conc_connection_list[1].mode))
		index =
		policy_mgr_get_third_connection_pcl_table_index_sap_sap();
	else if (((PM_STA_MODE == pm_conc_connection_list[0].mode) &&
		(PM_P2P_GO_MODE == pm_conc_connection_list[1].mode)) ||
		((PM_P2P_GO_MODE == pm_conc_connection_list[0].mode) &&
		(PM_STA_MODE == pm_conc_connection_list[1].mode)))
		index =
		policy_mgr_get_third_connection_pcl_table_index_sta_go();
	else if (((PM_STA_MODE == pm_conc_connection_list[0].mode) &&
		(PM_P2P_CLIENT_MODE == pm_conc_connection_list[1].mode)) ||
		((PM_P2P_CLIENT_MODE == pm_conc_connection_list[0].mode) &&
		(PM_STA_MODE == pm_conc_connection_list[1].mode)))
		index =
		policy_mgr_get_third_connection_pcl_table_index_sta_cli();
	else if (((PM_P2P_GO_MODE == pm_conc_connection_list[0].mode) &&
		(PM_P2P_CLIENT_MODE == pm_conc_connection_list[1].mode)) ||
		((PM_P2P_CLIENT_MODE == pm_conc_connection_list[0].mode) &&
		(PM_P2P_GO_MODE == pm_conc_connection_list[1].mode)))
		index =
		policy_mgr_get_third_connection_pcl_table_index_go_cli();
	else if (((PM_SAP_MODE == pm_conc_connection_list[0].mode) &&
		(PM_P2P_GO_MODE == pm_conc_connection_list[1].mode)) ||
		((PM_P2P_GO_MODE == pm_conc_connection_list[0].mode) &&
		(PM_SAP_MODE == pm_conc_connection_list[1].mode)))
		index =
		policy_mgr_get_third_connection_pcl_table_index_go_sap();
	else if (((PM_STA_MODE == pm_conc_connection_list[0].mode) &&
		(PM_STA_MODE == pm_conc_connection_list[1].mode)) ||
		((PM_STA_MODE == pm_conc_connection_list[0].mode) &&
		(PM_STA_MODE == pm_conc_connection_list[1].mode)))
		index =
		policy_mgr_get_third_connection_pcl_table_index_sta_sta();
	else if (((PM_NAN_DISC_MODE == pm_conc_connection_list[0].mode) &&
		(PM_STA_MODE == pm_conc_connection_list[1].mode)) ||
		((PM_STA_MODE == pm_conc_connection_list[0].mode) &&
		(PM_NAN_DISC_MODE == pm_conc_connection_list[1].mode)))
		index =
		policy_mgr_get_third_connection_pcl_table_index_sta_nan();
	else if (((PM_NAN_DISC_MODE == pm_conc_connection_list[0].mode) &&
		(PM_NDI_MODE == pm_conc_connection_list[1].mode)) ||
		((PM_NDI_MODE == pm_conc_connection_list[0].mode) &&
		(PM_NAN_DISC_MODE == pm_conc_connection_list[1].mode)))
		index =
		policy_mgr_get_third_connection_pcl_table_index_nan_ndi();
	else if (((PM_SAP_MODE == pm_conc_connection_list[0].mode) &&
		  (PM_NAN_DISC_MODE == pm_conc_connection_list[1].mode)) ||
		((PM_NAN_DISC_MODE == pm_conc_connection_list[0].mode) &&
		 (PM_SAP_MODE == pm_conc_connection_list[1].mode)))
		index =
		policy_mgr_get_third_connection_pcl_table_index_sap_nan();
	else if ((pm_conc_connection_list[0].mode == PM_P2P_GO_MODE) &&
		 (pm_conc_connection_list[1].mode == PM_P2P_GO_MODE))
		index =
		policy_mgr_get_third_connection_pcl_table_index_go_go();

	policy_mgr_debug("mode0:%d mode1:%d freq0:%d freq1:%d chain:%d index:%d",
			 pm_conc_connection_list[0].mode,
			 pm_conc_connection_list[1].mode,
			 pm_conc_connection_list[0].freq,
			 pm_conc_connection_list[1].freq,
			 pm_conc_connection_list[0].chain_mask, index);

	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return index;
}

#ifdef FEATURE_FOURTH_CONNECTION
enum policy_mgr_three_connection_mode
		policy_mgr_get_fourth_connection_pcl_table_index(
		struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_three_connection_mode index =
			PM_MAX_THREE_CONNECTION_MODE;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t count_sap = 0;
	uint32_t count_sta = 0;
	uint32_t count_ndi = 0;
	uint32_t count_nan_disc = 0;
	uint32_t list_sap[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t list_sta[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t list_ndi[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t list_nan_disc[MAX_NUMBER_OF_CONC_CONNECTIONS];

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return index;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);

	/* For 4 port concurrency case,
	 * 1st step: (SAP+STA)(2.4G MAC SCC) + (SAP+STA)(5G MAC SCC)
	 * 2nd step: (AGO+STA)(2.4G MAC SCC) + (AGO+STA)(5G MAC SCC)
	 */
	count_sap += policy_mgr_mode_specific_connection_count(
				psoc, PM_SAP_MODE, &list_sap[count_sap]);
	count_sap += policy_mgr_mode_specific_connection_count(
				psoc, PM_P2P_GO_MODE, &list_sap[count_sap]);
	count_sta = policy_mgr_mode_specific_connection_count(
				psoc, PM_STA_MODE, list_sta);
	count_ndi = policy_mgr_mode_specific_connection_count(
				psoc, PM_NDI_MODE, list_ndi);
	count_nan_disc = policy_mgr_mode_specific_connection_count(
				psoc, PM_NAN_DISC_MODE, list_nan_disc);
	policy_mgr_debug("sap/ago %d, sta %d, ndi %d nan disc %d",
			 count_sap, count_sta, count_ndi, count_nan_disc);
	if (count_sap == 2 && count_sta == 1) {
		policy_mgr_debug(
			"channel: sap0: %d, sap1: %d, sta0: %d",
			pm_conc_connection_list[list_sap[0]].freq,
			pm_conc_connection_list[list_sap[1]].freq,
			pm_conc_connection_list[list_sta[0]].freq);
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[0]].freq) &&
		     WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[0]].freq) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[1]].freq)) {
			index = PM_STA_SAP_SCC_24_SAP_5_DBS;
		} else if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[1]].freq) &&
		     WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[0]].freq) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[0]].freq)) {
			index = PM_STA_SAP_SCC_24_SAP_5_DBS;
		} else if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[0]].freq) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[0]].freq) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[1]].freq)) {
			index = PM_STA_SAP_SCC_5_SAP_24_DBS;
		} else if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[1]].freq) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[0]].freq) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[0]].freq)) {
			index = PM_STA_SAP_SCC_5_SAP_24_DBS;
		} else {
			index =  PM_MAX_THREE_CONNECTION_MODE;
		}
	} else if (count_sap == 1 && count_sta == 2) {
		policy_mgr_debug(
			"channel: sap0: %d, sta0: %d, sta1: %d",
			pm_conc_connection_list[list_sap[0]].freq,
			pm_conc_connection_list[list_sta[0]].freq,
			pm_conc_connection_list[list_sta[1]].freq);
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[0]].freq) &&
		     WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[0]].freq) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[1]].freq)) {
			index = PM_STA_SAP_SCC_24_STA_5_DBS;
		} else if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[1]].freq) &&
		     WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[0]].freq) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[0]].freq)) {
			index = PM_STA_SAP_SCC_24_STA_5_DBS;
		} else if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[0]].freq) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[0]].freq) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[1]].freq)) {
			index = PM_STA_SAP_SCC_5_STA_24_DBS;
		} else if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[1]].freq) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[0]].freq) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[0]].freq)) {
			index = PM_STA_SAP_SCC_5_STA_24_DBS;
		} else {
			index =  PM_MAX_THREE_CONNECTION_MODE;
		}
	} else if (count_nan_disc == 1 && count_ndi == 1 && count_sap == 1) {
		/* Policy mgr only considers NAN Disc ch in 2.4GHz */
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[0]].freq) &&
		    WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq)) {
			index = PM_NAN_DISC_SAP_SCC_24_NDI_5_DBS;
		} else if (WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[0]].freq) &&
			   WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq)) {
			index = PM_NAN_DISC_NDI_SCC_24_SAP_5_DBS;
		} else if (WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[0]].freq) &&
			  WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq)) {
			index = PM_SAP_NDI_SCC_5_NAN_DISC_24_DBS;
		} else {
			index = PM_MAX_THREE_CONNECTION_MODE;
		}
	} else if (count_nan_disc == 1 && count_ndi == 1 && count_sta == 1) {
		/* Policy mgr only considers NAN Disc ch in 2.4GHz */
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[0]].freq) &&
		    WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq)) {
			index = PM_NAN_DISC_STA_24_NDI_5_DBS;
		} else if (WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[0]].freq) &&
			   WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq)) {
			index = PM_NAN_DISC_NDI_24_STA_5_DBS;
		} else if (WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[0]].freq) &&
			  WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq)) {
			index = PM_STA_NDI_5_NAN_DISC_24_DBS;
		} else if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[0]].freq) &&
			  WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq)) {
			index = PM_STA_NDI_NAN_DISC_24_SMM;
		}
	} else if (count_nan_disc == 1 && count_ndi == 2) {
		/* Policy mgr only considers NAN Disc ch in 2.4GHz */
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq) &&
		    WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[1]].freq)) {
			index = PM_NAN_DISC_NDI_24_NDI_5_DBS;
		} else if (WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq) &&
			   WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq)) {
			index = PM_NAN_DISC_NDI_24_NDI_5_DBS;
		} else if (WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq) &&
			  WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq)) {
			index = PM_NDI_NDI_5_NAN_DISC_24_DBS;
		} else if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq) &&
			  WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq)) {
			index = PM_NDI_NDI_NAN_DISC_24_SMM;
		}
	}

	policy_mgr_debug(
		"mode0:%d mode1:%d mode2:%d chan0:%d chan1:%d chan2:%d chain:%d index:%d",
		pm_conc_connection_list[0].mode,
		pm_conc_connection_list[1].mode,
		pm_conc_connection_list[2].mode,
		pm_conc_connection_list[0].freq,
		pm_conc_connection_list[1].freq,
		pm_conc_connection_list[2].freq,
		pm_conc_connection_list[0].chain_mask, index);

	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return index;
}
#endif

uint32_t
policy_mgr_get_nondfs_preferred_channel(struct wlan_objmgr_psoc *psoc,
					enum policy_mgr_con_mode mode,
					bool for_existing_conn)
{
	uint32_t pcl_channels[NUM_CHANNELS];
	uint8_t pcl_weight[NUM_CHANNELS];
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	/*
	 * in worst case if we can't find any channel at all
	 * then return 2.4G channel, so atleast we won't fall
	 * under 5G MCC scenario
	 */
	uint32_t i, pcl_len = 0, non_dfs_freq, freq;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return PM_24_GHZ_CH_FREQ_6;
	}

	freq = PM_24_GHZ_CH_FREQ_6;
	if (true == for_existing_conn) {
		/*
		 * First try to see if there is any non-dfs channel already
		 * present in current connection table. If yes then return
		 * that channel
		 */
		if (true == policy_mgr_is_any_nondfs_chnl_present(
			psoc, &non_dfs_freq))
			return non_dfs_freq;

		if (QDF_STATUS_SUCCESS !=
				policy_mgr_get_pcl_for_existing_conn(
					psoc, mode,
					pcl_channels, &pcl_len,
					pcl_weight, QDF_ARRAY_SIZE(pcl_weight),
					false))
			return freq;
	} else {
		if (QDF_STATUS_SUCCESS != policy_mgr_get_pcl(
		    psoc, mode, pcl_channels, &pcl_len, pcl_weight,
		    QDF_ARRAY_SIZE(pcl_weight)))
			return freq;
	}

	for (i = 0; i < pcl_len; i++) {
		if (wlan_reg_is_dfs_for_freq(pm_ctx->pdev, pcl_channels[i]) ||
		    !policy_mgr_is_safe_channel(psoc, pcl_channels[i])) {
			continue;
		} else {
			freq = pcl_channels[i];
			break;
		}
	}

	return freq;
}

static void policy_mgr_remove_dsrc_channels(uint32_t *ch_freq_list,
					    uint32_t *num_channels,
					    struct wlan_objmgr_pdev *pdev)
{
	uint32_t num_chan_temp = 0;
	int i;

	for (i = 0; i < *num_channels; i++) {
		if (!wlan_reg_is_dsrc_freq(ch_freq_list[i])) {
			ch_freq_list[num_chan_temp] = ch_freq_list[i];
			num_chan_temp++;
		}
	}

	*num_channels = num_chan_temp;
}

QDF_STATUS policy_mgr_get_valid_chans_from_range(
		struct wlan_objmgr_psoc *psoc, uint32_t *ch_freq_list,
		uint32_t *ch_cnt, enum policy_mgr_con_mode mode)
{
	uint8_t ch_weight_list[NUM_CHANNELS] = {0};
	uint32_t ch_weight_len;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	size_t chan_index = 0;

	if (!ch_freq_list || !ch_cnt) {
		policy_mgr_err("NULL parameters");
		return QDF_STATUS_E_FAILURE;
	}

	for (chan_index = 0; chan_index < *ch_cnt; chan_index++)
		ch_weight_list[chan_index] = WEIGHT_OF_GROUP1_PCL_CHANNELS;

	ch_weight_len = *ch_cnt;

	/* check the channel avoidance list for beaconing entities */
	if (mode == PM_SAP_MODE || mode == PM_P2P_GO_MODE)
		policy_mgr_update_with_safe_channel_list(
			psoc, ch_freq_list, ch_cnt, ch_weight_list,
			ch_weight_len);

	status = policy_mgr_mode_specific_modification_on_pcl(
			psoc, ch_freq_list, ch_weight_list, ch_cnt, mode);

	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get modified pcl for mode %d", mode);
		return status;
	}

	status = policy_mgr_modify_pcl_based_on_dnbs(psoc, ch_freq_list,
						     ch_weight_list, ch_cnt);

	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get modified pcl based on DNBS");
		return status;
	}

	return status;
}

QDF_STATUS policy_mgr_get_valid_chans(struct wlan_objmgr_psoc *psoc,
				      uint32_t *ch_freq_list,
				      uint32_t *list_len)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	*list_len = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}
	if (!pm_ctx->valid_ch_freq_list_count) {
		policy_mgr_err("Invalid PM valid channel list");
		return QDF_STATUS_E_INVAL;
	}

	*list_len = pm_ctx->valid_ch_freq_list_count;
	qdf_mem_copy(ch_freq_list, pm_ctx->valid_ch_freq_list,
		     pm_ctx->valid_ch_freq_list_count *
		     sizeof(pm_ctx->valid_ch_freq_list[0]));

	policy_mgr_remove_dsrc_channels(ch_freq_list, list_len, pm_ctx->pdev);

	return QDF_STATUS_SUCCESS;
}

bool policy_mgr_list_has_24GHz_channel(uint32_t *ch_freq_list,
				       uint32_t list_len)
{
	uint32_t i;

	for (i = 0; i < list_len; i++) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(ch_freq_list[i]))
			return true;
	}

	return false;
}

QDF_STATUS
policy_mgr_set_sap_mandatory_channels(struct wlan_objmgr_psoc *psoc,
				      uint32_t *ch_freq_list, uint32_t len)
{
	uint32_t i;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	if (!len) {
		policy_mgr_err("No mandatory freq/chan configured");
		return QDF_STATUS_E_FAILURE;
	}

	if (!policy_mgr_list_has_24GHz_channel(ch_freq_list, len)) {
		policy_mgr_err("2.4GHz channels missing, this is not expected");
		return QDF_STATUS_E_FAILURE;
	}

	policy_mgr_debug("mandatory chan length:%d",
			pm_ctx->sap_mandatory_channels_len);

	for (i = 0; i < len; i++) {
		pm_ctx->sap_mandatory_channels[i] = ch_freq_list[i];
		policy_mgr_debug("chan:%d", pm_ctx->sap_mandatory_channels[i]);
	}

	pm_ctx->sap_mandatory_channels_len = len;

	return QDF_STATUS_SUCCESS;
}

bool policy_mgr_is_sap_mandatory_channel_set(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	if (pm_ctx->sap_mandatory_channels_len)
		return true;
	else
		return false;
}

QDF_STATUS policy_mgr_modify_sap_pcl_based_on_mandatory_channel(
		struct wlan_objmgr_psoc *psoc, uint32_t *pcl_list_org,
		uint8_t *weight_list_org, uint32_t *pcl_len_org)
{
	uint32_t i, j, pcl_len = 0;
	bool found;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	if (!pm_ctx->sap_mandatory_channels_len)
		return QDF_STATUS_SUCCESS;

	if (!policy_mgr_list_has_24GHz_channel(pm_ctx->sap_mandatory_channels,
			pm_ctx->sap_mandatory_channels_len)) {
		policy_mgr_err("fav channel list is missing 2.4GHz channels");
		return QDF_STATUS_E_FAILURE;
	}

	for (i = 0; i < pm_ctx->sap_mandatory_channels_len; i++)
		policy_mgr_debug("fav chan:%d",
			pm_ctx->sap_mandatory_channels[i]);

	for (i = 0; i < *pcl_len_org; i++) {
		found = false;
		if (i >= NUM_CHANNELS) {
			policy_mgr_debug("index is exceeding NUM_CHANNELS");
			break;
		}
		for (j = 0; j < pm_ctx->sap_mandatory_channels_len; j++) {
			if (pcl_list_org[i] ==
			    pm_ctx->sap_mandatory_channels[j]) {
				found = true;
				break;
			}
		}
		if (found && (pcl_len < NUM_CHANNELS)) {
			pcl_list_org[pcl_len] = pcl_list_org[i];
			weight_list_org[pcl_len++] = weight_list_org[i];
		}
	}
	*pcl_len_org = pcl_len;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
policy_mgr_get_sap_mandatory_channel(struct wlan_objmgr_psoc *psoc,
				     uint32_t sap_ch_freq,
				     uint32_t *intf_ch_freq)
{
	QDF_STATUS status;
	struct policy_mgr_pcl_list pcl;
	uint32_t i;
	uint32_t sap_new_freq;

	qdf_mem_zero(&pcl, sizeof(pcl));

	status = policy_mgr_get_pcl_for_existing_conn(
			psoc, PM_SAP_MODE, pcl.pcl_list, &pcl.pcl_len,
			pcl.weight_list, QDF_ARRAY_SIZE(pcl.weight_list),
			false);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("Unable to get PCL for SAP");
		return status;
	}

	/*
	 * Get inside below loop if no existing SAP connection and hence a new
	 * SAP connection might be coming up. pcl.pcl_len can be 0 if no common
	 * channel between PCL & mandatory channel list as well
	 */
	if (!pcl.pcl_len && !policy_mgr_mode_specific_connection_count(psoc,
	    PM_SAP_MODE, NULL)) {
		policy_mgr_debug("policy_mgr_get_pcl_for_existing_conn returned no pcl");
		status = policy_mgr_get_pcl(
				psoc, PM_SAP_MODE,
				pcl.pcl_list, &pcl.pcl_len,
				pcl.weight_list,
				QDF_ARRAY_SIZE(pcl.weight_list));
		if (QDF_IS_STATUS_ERROR(status)) {
			policy_mgr_err("Unable to get PCL for SAP: policy_mgr_get_pcl");
			return status;
		}
	}

	status = policy_mgr_modify_sap_pcl_based_on_mandatory_channel(
							psoc, pcl.pcl_list,
							pcl.weight_list,
							&pcl.pcl_len);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("Unable to modify SAP PCL");
		return status;
	}

	if (!pcl.pcl_len) {
		policy_mgr_err("No common channel between mandatory list & PCL");
		return QDF_STATUS_E_FAILURE;
	}

	sap_new_freq = pcl.pcl_list[0];
	if (WLAN_REG_IS_6GHZ_CHAN_FREQ(sap_ch_freq)) {
		for (i = 0; i < pcl.pcl_len; i++) {
			if (pcl.pcl_list[i] == *intf_ch_freq) {
				sap_new_freq = pcl.pcl_list[i];
				break;
			}
		}
	}

	*intf_ch_freq = sap_new_freq;
	policy_mgr_debug("mandatory channel:%d org sap ch %d", *intf_ch_freq,
			 sap_ch_freq);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_valid_chan_weights(struct wlan_objmgr_psoc *psoc,
		struct policy_mgr_pcl_chan_weights *weight,
		enum policy_mgr_con_mode mode)
{
	uint32_t i, j;
	struct policy_mgr_conc_connection_info
			info[MAX_NUMBER_OF_CONC_CONNECTIONS] = { {0} };
	uint8_t num_cxn_del = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_set(weight->weighed_valid_list, NUM_CHANNELS,
		    WEIGHT_OF_DISALLOWED_CHANNELS);
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	if ((mode == PM_P2P_GO_MODE || mode == PM_P2P_CLIENT_MODE) ||
	    (mode == PM_STA_MODE &&
	     policy_mgr_mode_specific_connection_count(psoc, PM_STA_MODE,
						       NULL) > 0)) {
		/*
		 * Store the STA mode's parameter and temporarily delete it
		 * from the concurrency table. This way the allow concurrency
		 * check can be used as though a new connection is coming up,
		 * allowing to detect the disallowed channels.
		 */
		if (mode == PM_STA_MODE)
			policy_mgr_store_and_del_conn_info(psoc, mode,
							   false, info,
							   &num_cxn_del);
		/*
		 * There is a small window between releasing the above lock
		 * and acquiring the same in policy_mgr_allow_concurrency,
		 * below!
		 */
		for (i = 0; i < weight->saved_num_chan; i++) {
			if (policy_mgr_is_concurrency_allowed
				(psoc, mode, weight->saved_chan_list[i],
				HW_MODE_20_MHZ)) {
				weight->weighed_valid_list[i] =
					WEIGHT_OF_NON_PCL_CHANNELS;
			}
		}
		/* Restore the connection info */
		if (mode == PM_STA_MODE)
			policy_mgr_restore_deleted_conn_info(psoc, info,
							     num_cxn_del);
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	for (i = 0; i < weight->saved_num_chan; i++) {
		for (j = 0; j < weight->pcl_len; j++) {
			if (weight->saved_chan_list[i] == weight->pcl_list[j]) {
				weight->weighed_valid_list[i] =
					weight->weight_list[j];
				break;
			}
		}
	}

	return QDF_STATUS_SUCCESS;
}

uint32_t policy_mgr_mode_specific_get_channel(
	struct wlan_objmgr_psoc *psoc, enum policy_mgr_con_mode mode)
{
	uint32_t conn_index;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t freq = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return 0;
	}
	/* provides the channel for the first matching mode type */
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
		conn_index++) {
		if ((pm_conc_connection_list[conn_index].mode == mode) &&
		    pm_conc_connection_list[conn_index].in_use) {
			freq = pm_conc_connection_list[conn_index].freq;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return freq;
}

uint32_t policy_mgr_get_alternate_channel_for_sap(
	struct wlan_objmgr_psoc *psoc, uint8_t sap_vdev_id,
	uint32_t sap_ch_freq)
{
	uint32_t pcl_channels[NUM_CHANNELS];
	uint8_t pcl_weight[NUM_CHANNELS];
	uint32_t ch_freq = 0;
	uint32_t pcl_len = 0;
	struct policy_mgr_conc_connection_info info;
	uint8_t num_cxn_del = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint8_t i;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return 0;
	}

	/*
	 * Store the connection's parameter and temporarily delete it
	 * from the concurrency table. This way the get pcl can be used as a
	 * new connection is coming up, after check, restore the connection to
	 * concurrency table.
	 */
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	policy_mgr_store_and_del_conn_info_by_vdev_id(psoc, sap_vdev_id,
						      &info, &num_cxn_del);

	if (QDF_STATUS_SUCCESS == policy_mgr_get_pcl(
	    psoc, PM_SAP_MODE, pcl_channels, &pcl_len,
	    pcl_weight, QDF_ARRAY_SIZE(pcl_weight))) {
		for (i = 0; i < pcl_len; i++) {
			if (pcl_channels[i] == sap_ch_freq)
				continue;
			ch_freq = pcl_channels[i];
			break;
		}
	}
	/* Restore the connection entry */
	if (num_cxn_del > 0)
		policy_mgr_restore_deleted_conn_info(psoc, &info, num_cxn_del);
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return ch_freq;
}

/*
 * Buffer len size to consider the 4 char freq, 3 char weight, 2 char
 * for open close brackets and space and a space, Total 10
 */
#define CHAN_WEIGHT_CHAR_LEN 10
#define MAX_CHAN_TO_PRINT 39

bool policy_mgr_dump_channel_list(uint32_t len, uint32_t *pcl_channels,
				  uint8_t *pcl_weight)
{
	uint32_t idx, buff_len, num = 0, count = 0, count_6G = 0;
	char *chan_buff = NULL;

	buff_len = (QDF_MIN(len, MAX_CHAN_TO_PRINT) * CHAN_WEIGHT_CHAR_LEN) + 1;
	chan_buff = qdf_mem_malloc(buff_len);
	if (!chan_buff)
		return false;

	policymgr_nofl_debug("Total PCL Chan Freq %d", len);
	for (idx = 0; (idx < len) && (idx < NUM_CHANNELS); idx++) {
		if (!WLAN_REG_IS_6GHZ_CHAN_FREQ(pcl_channels[idx])) {
			num += qdf_scnprintf(chan_buff + num, buff_len - num,
					     " %d[%d]", pcl_channels[idx],
					     pcl_weight[idx]);
			count++;
			if (count >= MAX_CHAN_TO_PRINT) {
				/* Print the MAX_CHAN_TO_PRINT channels */
				policymgr_nofl_debug("2G+5G Freq[weight]:%s",
						     chan_buff);
				count = 0;
				num = 0;
			}
		} else {
			count_6G++;
		}
	}
	/* Print any pending channels */
	if (num)
		policymgr_nofl_debug("2G+5G Freq[weight]:%s", chan_buff);

	if (!count_6G)
		goto free;

	count = 0;
	num = 0;
	for (idx = 0; (idx < len) && (idx < NUM_CHANNELS); idx++) {
		if (WLAN_REG_IS_6GHZ_CHAN_FREQ(pcl_channels[idx])) {
			num += qdf_scnprintf(chan_buff + num, buff_len - num,
					     " %d[%d]", pcl_channels[idx],
					     pcl_weight[idx]);
			count++;
			if (count >= MAX_CHAN_TO_PRINT) {
				/* Print the MAX_CHAN_TO_PRINT channels */
				policymgr_nofl_debug("6G Freq[weight]:%s",
						     chan_buff);
				count = 0;
				num = 0;
			}
		}
	}
	/* Print any pending channels */
	if (num)
		policymgr_nofl_debug("6G Freq[weight]:%s", chan_buff);

free:
	qdf_mem_free(chan_buff);

	return true;
}

QDF_STATUS policy_mgr_filter_passive_ch(struct wlan_objmgr_pdev *pdev,
					uint32_t *ch_freq_list,
					uint32_t *ch_cnt)
{
	size_t ch_index;
	size_t target_ch_cnt = 0;

	if (!pdev || !ch_freq_list || !ch_cnt) {
		policy_mgr_err("NULL parameters");
		return QDF_STATUS_E_FAULT;
	}

	for (ch_index = 0; ch_index < *ch_cnt; ch_index++) {
		if (!wlan_reg_is_passive_for_freq(pdev, ch_freq_list[ch_index]))
			ch_freq_list[target_ch_cnt++] = ch_freq_list[ch_index];
	}

	*ch_cnt = target_ch_cnt;

	return QDF_STATUS_SUCCESS;
}
