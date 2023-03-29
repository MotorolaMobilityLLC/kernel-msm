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

/**
 * ===========================================================================
 *                     sap_module.c
 *  OVERVIEW:
 *  This software unit holds the implementation of the WLAN SAP modules
 *  functions providing EXTERNAL APIs. It is also where the global SAP module
 *  context gets initialised
 *  DEPENDENCIES:
 *  Are listed for each API below.
 * ===========================================================================
 */

/* $Header$ */

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "qdf_trace.h"
#include "qdf_util.h"
#include "qdf_atomic.h"
/* Pick up the sme callback registration API */
#include "sme_api.h"

/* SAP API header file */

#include "sap_internal.h"
#include "sme_inside.h"
#include "cds_ieee80211_common_i.h"
#include "cds_regdomain.h"
#include "wlan_policy_mgr_api.h"
#include <wlan_scan_ucfg_api.h>
#include "wlan_reg_services_api.h"
#include <wlan_dfs_utils_api.h>
#include <wlan_reg_ucfg_api.h>
#include <wlan_cfg80211_crypto.h>
#include <wlan_crypto_global_api.h>
#include "cfg_ucfg_api.h"
#include "wlan_mlme_ucfg_api.h"
#include "wlan_mlme_vdev_mgr_interface.h"

#define SAP_DEBUG
static struct sap_context *gp_sap_ctx[SAP_MAX_NUM_SESSION];
static qdf_atomic_t sap_ctx_ref_count[SAP_MAX_NUM_SESSION];
static qdf_mutex_t sap_context_lock;

/**
 * wlansap_global_init() - Initialize SAP globals
 *
 * Initializes the SAP global data structures
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlansap_global_init(void)
{
	uint32_t i;

	if (QDF_IS_STATUS_ERROR(qdf_mutex_create(&sap_context_lock))) {
		sap_err("failed to init sap_context_lock");
		return QDF_STATUS_E_FAULT;
	}

	for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
		gp_sap_ctx[i] = NULL;
		qdf_atomic_init(&sap_ctx_ref_count[i]);
	}

	sap_debug("sap global context initialized");

	return QDF_STATUS_SUCCESS;
}

/**
 * wlansap_global_deinit() - De-initialize SAP globals
 *
 * De-initializes the SAP global data structures
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlansap_global_deinit(void)
{
	uint32_t i;

	for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
		if (gp_sap_ctx[i]) {
			sap_err("we could be leaking context:%d", i);
		}
		gp_sap_ctx[i] = NULL;
		qdf_atomic_init(&sap_ctx_ref_count[i]);
	}

	if (QDF_IS_STATUS_ERROR(qdf_mutex_destroy(&sap_context_lock))) {
		sap_err("failed to destroy sap_context_lock");
		return QDF_STATUS_E_FAULT;
	}

	sap_debug("sap global context deinitialized");

	return QDF_STATUS_SUCCESS;
}

/**
 * wlansap_save_context() - Save the context in global SAP context
 * @ctx: SAP context to be stored
 *
 * Stores the given SAP context in the global SAP context array
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wlansap_save_context(struct sap_context *ctx)
{
	uint32_t i;

	qdf_mutex_acquire(&sap_context_lock);
	for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
		if (!gp_sap_ctx[i]) {
			gp_sap_ctx[i] = ctx;
			qdf_atomic_inc(&sap_ctx_ref_count[i]);
			qdf_mutex_release(&sap_context_lock);
			sap_debug("sap context saved at index: %d", i);
			return QDF_STATUS_SUCCESS;
		}
	}
	qdf_mutex_release(&sap_context_lock);

	sap_err("failed to save sap context");

	return QDF_STATUS_E_FAILURE;
}

/**
 * wlansap_context_get() - Verify SAP context and increment ref count
 * @ctx: Context to be checked
 *
 * Verifies the SAP context and increments the reference count maintained for
 * the corresponding SAP context.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlansap_context_get(struct sap_context *ctx)
{
	uint32_t i;

	qdf_mutex_acquire(&sap_context_lock);
	for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
		if (ctx && (gp_sap_ctx[i] == ctx)) {
			qdf_atomic_inc(&sap_ctx_ref_count[i]);
			qdf_mutex_release(&sap_context_lock);
			return QDF_STATUS_SUCCESS;
		}
	}
	qdf_mutex_release(&sap_context_lock);

	sap_debug("sap session is not valid");
	return QDF_STATUS_E_FAILURE;
}

/**
 * wlansap_context_put() - Check the reference count and free SAP context
 * @ctx: SAP context to be checked and freed
 *
 * Checks the reference count and frees the SAP context
 *
 * Return: None
 */
void wlansap_context_put(struct sap_context *ctx)
{
	uint32_t i;

	if (!ctx)
		return;

	qdf_mutex_acquire(&sap_context_lock);
	for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
		if (gp_sap_ctx[i] == ctx) {
			if (qdf_atomic_dec_and_test(&sap_ctx_ref_count[i])) {
				if (ctx->freq_list) {
					qdf_mem_free(ctx->freq_list);
					ctx->freq_list = NULL;
					ctx->num_of_channel = 0;
				}
				qdf_mem_free(ctx);
				gp_sap_ctx[i] = NULL;
				sap_debug("sap session freed: %d", i);
			}
			qdf_mutex_release(&sap_context_lock);
			return;
		}
	}
	qdf_mutex_release(&sap_context_lock);
}

struct sap_context *sap_create_ctx(void)
{
	struct sap_context *sap_ctx;
	QDF_STATUS status;

	sap_ctx = qdf_mem_malloc(sizeof(*sap_ctx));
	if (!sap_ctx)
		return NULL;

	/* Clean up SAP control block, initialize all values */
	/* Save the SAP context pointer */
	status = wlansap_save_context(sap_ctx);
	if (QDF_IS_STATUS_ERROR(status)) {
		sap_err("failed to save SAP context");
		qdf_mem_free(sap_ctx);
		return NULL;
	}
	sap_debug("Exit");

	return sap_ctx;
} /* sap_create_ctx */

static QDF_STATUS wlansap_owe_init(struct sap_context *sap_ctx)
{
	qdf_list_create(&sap_ctx->owe_pending_assoc_ind_list, 0);

	return QDF_STATUS_SUCCESS;
}

static void wlansap_owe_cleanup(struct sap_context *sap_ctx)
{
	struct mac_context *mac;
	struct owe_assoc_ind *owe_assoc_ind;
	struct assoc_ind *assoc_ind = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	QDF_STATUS status;

	if (!sap_ctx) {
		sap_err("Invalid SAP context");
		return;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return;
	}

	if (QDF_STATUS_SUCCESS !=
	    qdf_list_peek_front(&sap_ctx->owe_pending_assoc_ind_list,
				&node)) {
		sap_debug("Failed to find assoc ind list");
		return;
	}

	while (node) {
		qdf_list_peek_next(&sap_ctx->owe_pending_assoc_ind_list,
				   node, &next_node);
		owe_assoc_ind = qdf_container_of(node, struct owe_assoc_ind,
						 node);
		status = qdf_list_remove_node(
					   &sap_ctx->owe_pending_assoc_ind_list,
					   node);
		if (status == QDF_STATUS_SUCCESS) {
			assoc_ind = owe_assoc_ind->assoc_ind;
			qdf_mem_free(owe_assoc_ind);
			assoc_ind->owe_ie = NULL;
			assoc_ind->owe_ie_len = 0;
			assoc_ind->owe_status = STATUS_UNSPECIFIED_FAILURE;
			status = sme_update_owe_info(mac, assoc_ind);
			qdf_mem_free(assoc_ind);
		} else {
			sap_err("Failed to remove assoc ind");
		}
		node = next_node;
		next_node = NULL;
	}
}

static void wlansap_owe_deinit(struct sap_context *sap_ctx)
{
	qdf_list_destroy(&sap_ctx->owe_pending_assoc_ind_list);
}

QDF_STATUS sap_init_ctx(struct sap_context *sap_ctx,
			 enum QDF_OPMODE mode,
			 uint8_t *addr, uint32_t session_id, bool reinit)
{
	QDF_STATUS status;
	struct mac_context *mac;

	sap_debug("wlansap_start invoked successfully");

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	/* Now configure the roaming profile links. To SSID and bssid.*/
	/* We have room for two SSIDs. */
	sap_ctx->csr_roamProfile.SSIDs.numOfSSIDs = 1;   /* This is true for now. */
	sap_ctx->csr_roamProfile.SSIDs.SSIDList = sap_ctx->SSIDList;     /* Array of two */
	sap_ctx->csr_roamProfile.SSIDs.SSIDList[0].SSID.length = 0;
	sap_ctx->csr_roamProfile.SSIDs.SSIDList[0].handoffPermitted = false;
	sap_ctx->csr_roamProfile.SSIDs.SSIDList[0].ssidHidden =
		sap_ctx->SSIDList[0].ssidHidden;

	sap_ctx->csr_roamProfile.BSSIDs.numOfBSSIDs = 1; /* This is true for now. */
	sap_ctx->csa_reason = CSA_REASON_UNKNOWN;
	sap_ctx->csr_roamProfile.BSSIDs.bssid = &sap_ctx->bssid;
	sap_ctx->csr_roamProfile.csrPersona = mode;
	qdf_mem_copy(sap_ctx->self_mac_addr, addr, QDF_MAC_ADDR_SIZE);

	/* Now configure the auth type in the roaming profile. To open. */
	sap_ctx->csr_roamProfile.negotiatedAuthType = eCSR_AUTH_TYPE_OPEN_SYSTEM;        /* open is the default */

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_INVAL;
	}

	status = sap_set_session_param(MAC_HANDLE(mac), sap_ctx, session_id);
	if (QDF_STATUS_SUCCESS != status) {
		sap_err("Calling sap_set_session_param status = %d", status);
		return QDF_STATUS_E_FAILURE;
	}
	/* Register with scan component only during init */
	if (!reinit)
		sap_ctx->req_id =
			ucfg_scan_register_requester(mac->psoc, "SAP",
					sap_scan_event_callback, sap_ctx);

	if (!reinit) {
		status = wlansap_owe_init(sap_ctx);
		if (QDF_STATUS_SUCCESS != status) {
			sap_err("OWE init failed");
			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sap_deinit_ctx(struct sap_context *sap_ctx)
{
	struct mac_context *mac;

	/* Sanity check - Extract SAP control block */
	sap_debug("wlansap_stop invoked successfully ");

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	wlansap_owe_cleanup(sap_ctx);
	wlansap_owe_deinit(sap_ctx);

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}
	ucfg_scan_unregister_requester(mac->psoc, sap_ctx->req_id);

	if (sap_ctx->freq_list) {
		qdf_mem_free(sap_ctx->freq_list);
		sap_ctx->freq_list = NULL;
		sap_ctx->num_of_channel = 0;
	}
	sap_free_roam_profile(&sap_ctx->csr_roamProfile);
	if (sap_ctx->sessionId != WLAN_UMAC_VDEV_ID_MAX) {
		/* empty queues/lists/pkts if any */
		sap_clear_session_param(MAC_HANDLE(mac), sap_ctx,
					sap_ctx->sessionId);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sap_destroy_ctx(struct sap_context *sap_ctx)
{
	sap_debug("Enter");

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}
	/* Cleanup SAP control block */
	/*
	 * wlansap_context_put will release actual sap_ctx memory
	 * allocated during sap_create_ctx
	 */
	wlansap_context_put(sap_ctx);

	return QDF_STATUS_SUCCESS;
} /* sap_destroy_ctx */

bool wlansap_is_channel_in_nol_list(struct sap_context *sap_ctx,
				    uint8_t channelNumber,
				    ePhyChanBondState chanBondState)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer from pCtx");
		return QDF_STATUS_E_FAULT;
	}

	return sap_dfs_is_channel_in_nol_list(sap_ctx, channelNumber,
					      chanBondState);
}

static QDF_STATUS wlansap_mark_leaking_channel(struct wlan_objmgr_pdev *pdev,
		uint8_t *leakage_adjusted_lst,
		uint8_t chan_bw)
{

	return utils_dfs_mark_leaking_ch(pdev, chan_bw, 1,
			leakage_adjusted_lst);
}

bool wlansap_is_channel_leaking_in_nol(struct sap_context *sap_ctx,
				       uint8_t channel,
				       uint8_t chan_bw)
{
	struct mac_context *mac_ctx;
	uint8_t leakage_adjusted_lst[1];

	leakage_adjusted_lst[0] = channel;
	mac_ctx = sap_get_mac_context();
	if (!mac_ctx) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}
	if (QDF_IS_STATUS_ERROR(wlansap_mark_leaking_channel(mac_ctx->pdev,
			leakage_adjusted_lst, chan_bw)))
		return true;

	if (!leakage_adjusted_lst[0])
		return true;

	return false;
}

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
uint16_t wlansap_check_cc_intf(struct sap_context *sap_ctx)
{
	struct mac_context *mac;
	uint16_t intf_ch_freq;
	eCsrPhyMode phy_mode;

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return 0;
	}
	phy_mode = sap_ctx->csr_roamProfile.phyMode;
	intf_ch_freq = sme_check_concurrent_channel_overlap(
						MAC_HANDLE(mac),
						sap_ctx->chan_freq,
						phy_mode,
						sap_ctx->cc_switch_mode);
	return intf_ch_freq;
}
#endif

 /**
  * wlansap_set_scan_acs_channel_params() - Config scan and channel parameters.
  * config:                                Pointer to the SAP config
  * psap_ctx:                               Pointer to the SAP Context.
  * pusr_context:                           Parameter that will be passed
  *                                         back in all the SAP callback events.
  *
  * This api function is used to copy Scan and Channel parameters from sap
  * config to sap context.
  *
  * Return:                                 The result code associated with
  *                                         performing the operation
  */
static QDF_STATUS
wlansap_set_scan_acs_channel_params(struct sap_config *config,
				    struct sap_context *psap_ctx,
				    void *pusr_context)
{
	struct mac_context *mac;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t auto_channel_select_weight;

	if (!config) {
		sap_err("Invalid config passed ");
		return QDF_STATUS_E_FAULT;
	}

	if (!psap_ctx) {
		sap_err("Invalid config passed ");
		return QDF_STATUS_E_FAULT;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_INVAL;
	}

	/* Channel selection is auto or configured */
	wlansap_set_acs_ch_freq(psap_ctx, config->chan_freq);
	psap_ctx->dfs_mode = config->acs_dfs_mode;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	psap_ctx->cc_switch_mode = config->cc_switch_mode;
#endif
	status = ucfg_mlme_get_auto_channel_weight(
					mac->psoc,
					&auto_channel_select_weight);

	if (!QDF_IS_STATUS_SUCCESS(status))
		sap_err("get_auto_channel_weight failed");

	psap_ctx->auto_channel_select_weight = auto_channel_select_weight;
	sap_debug("auto_channel_select_weight %d",
		  psap_ctx->auto_channel_select_weight);

	psap_ctx->user_context = pusr_context;
	psap_ctx->enableOverLapCh = config->enOverLapCh;
	psap_ctx->acs_cfg = &config->acs_cfg;
	psap_ctx->ch_width_orig = config->acs_cfg.ch_width;
	psap_ctx->sec_ch_freq = config->sec_ch_freq;

	/*
	 * Set the BSSID to your "self MAC Addr" read
	 * the mac address from Configuation ITEM received
	 * from HDD
	 */
	psap_ctx->csr_roamProfile.BSSIDs.numOfBSSIDs = 1;

	/* Save a copy to SAP context */
	qdf_mem_copy(psap_ctx->csr_roamProfile.BSSIDs.bssid,
		config->self_macaddr.bytes, QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(psap_ctx->self_mac_addr,
		config->self_macaddr.bytes, QDF_MAC_ADDR_SIZE);

	return status;
}

/**
 * wlan_sap_get_roam_profile() - Returns sap roam profile.
 * @sap_ctx:	Pointer to Sap Context.
 *
 * This function provides the SAP roam profile.
 *
 * Return: SAP RoamProfile
 */
struct csr_roam_profile *wlan_sap_get_roam_profile(struct sap_context *sap_ctx)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer from ctx");
		return NULL;
	}
	return &sap_ctx->csr_roamProfile;
}

eCsrPhyMode wlan_sap_get_phymode(struct sap_context *sap_ctx)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer from ctx");
		return 0;
	}
	return sap_ctx->csr_roamProfile.phyMode;
}

enum phy_ch_width wlan_sap_get_concurrent_bw(struct wlan_objmgr_pdev *pdev,
					     struct wlan_objmgr_psoc *psoc,
					     qdf_freq_t con_ch_freq,
					     enum phy_ch_width channel_width)
{
	enum hw_mode_bandwidth sta_ch_width;
	enum phy_ch_width sta_chan_width;
	bool sta_present, is_con_chan_dfs = false;
	uint8_t sta_vdev_id;
	uint8_t sta_sap_scc_on_dfs_chnl;

	if (WLAN_REG_IS_24GHZ_CH_FREQ(con_ch_freq))
		return CH_WIDTH_20MHZ;

	if (wlan_reg_is_6ghz_chan_freq(con_ch_freq))
		return channel_width;

	sta_present = policy_mgr_is_sta_present_on_freq(psoc,
							&sta_vdev_id,
							con_ch_freq,
							&sta_ch_width);
	if (policy_mgr_is_hw_dbs_capable(psoc)) {
		policy_mgr_get_sta_sap_scc_on_dfs_chnl(
					psoc, &sta_sap_scc_on_dfs_chnl);
		if (sta_sap_scc_on_dfs_chnl ==
					PM_STA_SAP_ON_DFS_MASTER_MODE_FLEX) {
			return channel_width;
		} else if (sta_sap_scc_on_dfs_chnl ==
				PM_STA_SAP_ON_DFS_MASTER_MODE_DISABLED) {
			if (sta_present)
				return channel_width;
			/*
			 * sta_sap_scc_on_dfs_chnl = 1, DFS master is disabled.
			 * If STA not present (SAP single), the SAP (160Mhz) is
			 * not allowed on DFS, so limit SAP to 80Mhz.
			 */
			return QDF_MIN(channel_width, CH_WIDTH_80MHZ);
		}
		/*
		 * sta_sap_scc_on_dfs_chnl = 0, not allow STA+SAP SCC
		 * on DFS. Limit SAP to 80Mhz if STA present.
		 */
		if (sta_present)
			return QDF_MIN(channel_width, CH_WIDTH_80MHZ);

		return channel_width;
	}

	/* if no STA present return max of BW and 80MHZ */
	if (!sta_present)
		return CH_WIDTH_80MHZ;

	sta_chan_width = policy_mgr_get_ch_width(sta_ch_width);
	if (wlan_reg_is_dfs_for_freq(pdev, con_ch_freq) ||
	    sta_chan_width == CH_WIDTH_160MHZ)
		is_con_chan_dfs = true;

	/* if STA not on DFS return max of BW and 80MHZ */
	if (!is_con_chan_dfs)
		return  QDF_MAX(sta_chan_width, CH_WIDTH_80MHZ);

	/* If sta channel is DFS return min of 80 and STA BW */
	return QDF_MIN(sta_chan_width, CH_WIDTH_80MHZ);
}

uint32_t wlan_sap_get_vht_ch_width(struct sap_context *sap_ctx)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return 0;
	}

	return sap_ctx->ch_params.ch_width;
}

bool wlan_sap_get_ch_params(struct sap_context *sap_ctx,
			    struct ch_params *ch_params)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return false;
	}

	*ch_params = sap_ctx->ch_params;
	return true;
}

/**
 * wlan_sap_validate_channel_switch() - validate target channel switch w.r.t
 *      concurreny rules set to avoid channel interference.
 * @mac_handle: Opaque handle to the global MAC context
 * @sap_ch: channel to switch
 * @sap_context: sap session context
 *
 * Return: true if there is no channel interference else return false
 */
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
static bool wlan_sap_validate_channel_switch(mac_handle_t mac_handle,
					     uint32_t sap_ch_freq,
					     struct sap_context *sap_context)
{
	return sme_validate_sap_channel_switch(
			mac_handle,
			sap_ch_freq,
			sap_context->csr_roamProfile.phyMode,
			sap_context->cc_switch_mode,
			sap_context->sessionId);
}
#else
static bool wlan_sap_validate_channel_switch(mac_handle_t mac_handle,
					     uint32_t sap_ch_freq,
					     struct sap_context *sap_context)
{
	return true;
}
#endif

void wlan_sap_set_sap_ctx_acs_cfg(struct sap_context *sap_ctx,
				  struct sap_config *sap_config)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return;
	}

	sap_ctx->acs_cfg = &sap_config->acs_cfg;
}

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
static inline QDF_STATUS
wlansap_set_vdev_crypto_prarams_from_ie(struct wlan_objmgr_vdev *vdev,
					uint8_t *ie_ptr,
					uint16_t ie_len)
{
	return wlan_set_vdev_crypto_prarams_from_ie(vdev, ie_ptr, ie_len);
}
#else
static inline QDF_STATUS
wlansap_set_vdev_crypto_prarams_from_ie(struct wlan_objmgr_vdev *vdev,
					uint8_t *ie_ptr,
					uint16_t ie_len)
{
	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS wlansap_start_bss(struct sap_context *sap_ctx,
			     sap_event_cb sap_event_cb,
			     struct sap_config *config, void *user_context)
{
	struct sap_sm_event sap_event;        /* State machine event */
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	uint32_t auto_channel_select_weight =
			cfg_default(CFG_AUTO_CHANNEL_SELECT_WEIGHT);
	int reduced_beacon_interval;
	struct mac_context *pmac = NULL;
	int sap_chanswitch_beacon_cnt;
	bool sap_chanswitch_mode;

	if (!sap_ctx) {
		sap_info("Invalid SAP context");
		return QDF_STATUS_E_FAULT;
	}

	pmac = sap_get_mac_context();
	if (!pmac) {
		sap_err("Invalid sap MAC context");
		qdf_status = QDF_STATUS_E_INVAL;
		goto fail;
	}

	sap_ctx->fsm_state = SAP_INIT;

	qdf_status = wlansap_set_vdev_crypto_prarams_from_ie(
			sap_ctx->vdev,
			config->RSNWPAReqIE,
			config->RSNWPAReqIELength);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		sap_debug("Failed to set crypto params from IE");

	/* Channel selection is auto or configured */
	sap_ctx->chan_freq = config->chan_freq;
	sap_ctx->dfs_mode = config->acs_dfs_mode;
	sap_ctx->ch_params.ch_width = config->ch_params.ch_width;
	sap_ctx->ch_params.center_freq_seg0 =
		config->ch_params.center_freq_seg0;
	sap_ctx->ch_params.center_freq_seg1 =
		config->ch_params.center_freq_seg1;
	sap_ctx->ch_params.sec_ch_offset =
		config->ch_params.sec_ch_offset;
	sap_ctx->ch_width_orig = config->ch_width_orig;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	sap_ctx->cc_switch_mode = config->cc_switch_mode;
#endif

	qdf_status = ucfg_mlme_get_auto_channel_weight(
					pmac->psoc,
					&auto_channel_select_weight);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		sap_err("get_auto_channel_weight failed");

	sap_ctx->auto_channel_select_weight = auto_channel_select_weight;

	sap_ctx->user_context = user_context;
	sap_ctx->enableOverLapCh = config->enOverLapCh;
	sap_ctx->acs_cfg = &config->acs_cfg;
	sap_ctx->sec_ch_freq = config->sec_ch_freq;
	sap_ctx->dfs_cac_offload = config->dfs_cac_offload;
	sap_ctx->isCacEndNotified = false;
	sap_ctx->is_chan_change_inprogress = false;

	/* Set the BSSID to your "self MAC Addr" read the mac address
		from Configuation ITEM received from HDD */
	sap_ctx->csr_roamProfile.BSSIDs.numOfBSSIDs = 1;
	qdf_mem_copy(sap_ctx->csr_roamProfile.BSSIDs.bssid,
		     sap_ctx->self_mac_addr, sizeof(struct qdf_mac_addr));

	/* Save a copy to SAP context */
	qdf_mem_copy(sap_ctx->csr_roamProfile.BSSIDs.bssid,
		     config->self_macaddr.bytes, QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(sap_ctx->self_mac_addr,
		     config->self_macaddr.bytes, QDF_MAC_ADDR_SIZE);

	/* copy the configuration items to csrProfile */
	sapconvert_to_csr_profile(config, eCSR_BSS_TYPE_INFRA_AP,
			       &sap_ctx->csr_roamProfile);

	/*
	 * Set the DFS Test Mode setting
	 * Set beacon channel count before chanel switch
	 */
	qdf_status = ucfg_mlme_get_sap_chn_switch_bcn_count(
						pmac->psoc,
						&sap_chanswitch_beacon_cnt);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		sap_err("ucfg_mlme_get_sap_chn_switch_bcn_count fail, set def");

	pmac->sap.SapDfsInfo.sap_ch_switch_beacon_cnt =
				sap_chanswitch_beacon_cnt;
	pmac->sap.SapDfsInfo.sap_ch_switch_mode =
				sap_chanswitch_beacon_cnt;

	qdf_status = ucfg_mlme_get_sap_channel_switch_mode(
						pmac->psoc,
						&sap_chanswitch_mode);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		sap_err("ucfg_mlme_get_sap_channel_switch_mode, set def");

	pmac->sap.SapDfsInfo.sap_ch_switch_mode = sap_chanswitch_mode;
	pmac->sap.sapCtxList[sap_ctx->sessionId].sap_context = sap_ctx;
	pmac->sap.sapCtxList[sap_ctx->sessionId].sapPersona =
		sap_ctx->csr_roamProfile.csrPersona;

	qdf_status = ucfg_mlme_get_sap_reduces_beacon_interval(
						pmac->psoc,
						&reduced_beacon_interval);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		sap_err("ucfg_mlme_get_sap_reduces_beacon_interval fail");

	pmac->sap.SapDfsInfo.reduced_beacon_interval =
					reduced_beacon_interval;
	sap_debug("SAP: auth ch select weight:%d chswitch bcn cnt:%d chswitch mode:%d reduced bcn intv:%d",
		  sap_ctx->auto_channel_select_weight,
		  sap_chanswitch_beacon_cnt,
		  pmac->sap.SapDfsInfo.sap_ch_switch_mode,
		  pmac->sap.SapDfsInfo.reduced_beacon_interval);

	/* Copy MAC filtering settings to sap context */
	sap_ctx->eSapMacAddrAclMode = config->SapMacaddr_acl;
	qdf_mem_copy(sap_ctx->acceptMacList, config->accept_mac,
		     sizeof(config->accept_mac));
	sap_ctx->nAcceptMac = config->num_accept_mac;
	sap_sort_mac_list(sap_ctx->acceptMacList, sap_ctx->nAcceptMac);
	qdf_mem_copy(sap_ctx->denyMacList, config->deny_mac,
		     sizeof(config->deny_mac));
	sap_ctx->nDenyMac = config->num_deny_mac;
	sap_sort_mac_list(sap_ctx->denyMacList, sap_ctx->nDenyMac);
	sap_ctx->beacon_tx_rate = config->beacon_tx_rate;

	/* Fill in the event structure for FSM */
	sap_event.event = eSAP_HDD_START_INFRA_BSS;
	sap_event.params = 0;    /* pSapPhysLinkCreate */

	/* Store the HDD callback in SAP context */
	sap_ctx->sap_event_cb = sap_event_cb;

	/* Handle event */
	qdf_status = sap_fsm(sap_ctx, &sap_event);
fail:
	if (QDF_IS_STATUS_ERROR(qdf_status))
		sap_free_roam_profile(&sap_ctx->csr_roamProfile);

	return qdf_status;
} /* wlansap_start_bss */

QDF_STATUS wlansap_set_mac_acl(struct sap_context *sap_ctx,
			       struct sap_config *config)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	sap_debug("wlansap_set_mac_acl");

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}
	/* Copy MAC filtering settings to sap context */
	sap_ctx->eSapMacAddrAclMode = config->SapMacaddr_acl;

	if (eSAP_DENY_UNLESS_ACCEPTED == sap_ctx->eSapMacAddrAclMode) {
		qdf_mem_copy(sap_ctx->acceptMacList,
			     config->accept_mac,
			     sizeof(config->accept_mac));
		sap_ctx->nAcceptMac = config->num_accept_mac;
		sap_sort_mac_list(sap_ctx->acceptMacList,
			       sap_ctx->nAcceptMac);
	} else if (eSAP_ACCEPT_UNLESS_DENIED == sap_ctx->eSapMacAddrAclMode) {
		qdf_mem_copy(sap_ctx->denyMacList, config->deny_mac,
			     sizeof(config->deny_mac));
		sap_ctx->nDenyMac = config->num_deny_mac;
		sap_sort_mac_list(sap_ctx->denyMacList, sap_ctx->nDenyMac);
	}

	return qdf_status;
} /* wlansap_set_mac_acl */

QDF_STATUS wlansap_stop_bss(struct sap_context *sap_ctx)
{
	struct sap_sm_event sap_event;        /* State machine event */
	QDF_STATUS qdf_status;

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	/* Fill in the event structure for FSM */
	sap_event.event = eSAP_HDD_STOP_INFRA_BSS;
	sap_event.params = 0;

	/* Handle event */
	qdf_status = sap_fsm(sap_ctx, &sap_event);

	return qdf_status;
}

/* This routine will set the mode of operation for ACL dynamically*/
QDF_STATUS wlansap_set_acl_mode(struct sap_context *sap_ctx,
				eSapMacAddrACL mode)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	sap_ctx->eSapMacAddrAclMode = mode;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_get_acl_mode(struct sap_context *sap_ctx,
				eSapMacAddrACL *mode)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	*mode = sap_ctx->eSapMacAddrAclMode;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_get_acl_accept_list(struct sap_context *sap_ctx,
				       struct qdf_mac_addr *pAcceptList,
				       uint8_t *nAcceptList)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	memcpy(pAcceptList, sap_ctx->acceptMacList,
	       (sap_ctx->nAcceptMac * QDF_MAC_ADDR_SIZE));
	*nAcceptList = sap_ctx->nAcceptMac;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_get_acl_deny_list(struct sap_context *sap_ctx,
				     struct qdf_mac_addr *pDenyList,
				     uint8_t *nDenyList)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer from p_cds_gctx");
		return QDF_STATUS_E_FAULT;
	}

	memcpy(pDenyList, sap_ctx->denyMacList,
	       (sap_ctx->nDenyMac * QDF_MAC_ADDR_SIZE));
	*nDenyList = sap_ctx->nDenyMac;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_clear_acl(struct sap_context *sap_ctx)
{
	uint8_t i;

	if (!sap_ctx) {
		return QDF_STATUS_E_RESOURCES;
	}

	for (i = 0; i < (sap_ctx->nDenyMac - 1); i++) {
		qdf_mem_zero((sap_ctx->denyMacList + i)->bytes,
			     QDF_MAC_ADDR_SIZE);
	}

	sap_print_acl(sap_ctx->denyMacList, sap_ctx->nDenyMac);
	sap_ctx->nDenyMac = 0;

	for (i = 0; i < (sap_ctx->nAcceptMac - 1); i++) {
		qdf_mem_zero((sap_ctx->acceptMacList + i)->bytes,
			     QDF_MAC_ADDR_SIZE);
	}

	sap_print_acl(sap_ctx->acceptMacList, sap_ctx->nAcceptMac);
	sap_ctx->nAcceptMac = 0;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_modify_acl(struct sap_context *sap_ctx,
			      uint8_t *peer_sta_mac,
			      eSapACLType list_type, eSapACLCmdType cmd)
{
	bool sta_white_list = false, sta_black_list = false;
	uint8_t staWLIndex, staBLIndex;

	if (!sap_ctx) {
		sap_err("Invalid SAP Context");
		return QDF_STATUS_E_FAULT;
	}
	if (qdf_mem_cmp(sap_ctx->bssid.bytes, peer_sta_mac,
			QDF_MAC_ADDR_SIZE) == 0) {
		sap_err("requested peer mac is "QDF_MAC_ADDR_FMT
			"our own SAP BSSID. Do not blacklist or whitelist this BSSID",
			QDF_MAC_ADDR_REF(peer_sta_mac));
		return QDF_STATUS_E_FAULT;
	}
	sap_debug("Modify ACL entered\n" "Before modification of ACL\n"
		  "size of accept and deny lists %d %d", sap_ctx->nAcceptMac,
		  sap_ctx->nDenyMac);
	sap_debug("*** WHITE LIST ***");
	sap_print_acl(sap_ctx->acceptMacList, sap_ctx->nAcceptMac);
	sap_debug("*** BLACK LIST ***");
	sap_print_acl(sap_ctx->denyMacList, sap_ctx->nDenyMac);

	/* the expectation is a mac addr will not be in both the lists
	 * at the same time. It is the responsiblity of userspace to
	 * ensure this
	 */
	sta_white_list =
		sap_search_mac_list(sap_ctx->acceptMacList, sap_ctx->nAcceptMac,
				 peer_sta_mac, &staWLIndex);
	sta_black_list =
		sap_search_mac_list(sap_ctx->denyMacList, sap_ctx->nDenyMac,
				 peer_sta_mac, &staBLIndex);

	if (sta_white_list && sta_black_list) {
		sap_err("Peer mac " QDF_MAC_ADDR_FMT
			" found in white and black lists."
			"Initial lists passed incorrect. Cannot execute this command.",
			QDF_MAC_ADDR_REF(peer_sta_mac));
		return QDF_STATUS_E_FAILURE;

	}
	sap_debug("cmd %d", cmd);

	switch (list_type) {
	case eSAP_WHITE_LIST:
		if (cmd == ADD_STA_TO_ACL) {
			/* error check */
			/* if list is already at max, return failure */
			if (sap_ctx->nAcceptMac == MAX_ACL_MAC_ADDRESS) {
				sap_err("White list is already maxed out. Cannot accept "
					  QDF_MAC_ADDR_FMT,
					  QDF_MAC_ADDR_REF(peer_sta_mac));
				return QDF_STATUS_E_FAILURE;
			}
			if (sta_white_list) {
				/* Do nothing if already present in white list. Just print a warning */
				sap_warn("MAC address already present in white list "
					 QDF_MAC_ADDR_FMT,
					 QDF_MAC_ADDR_REF(peer_sta_mac));
				return QDF_STATUS_SUCCESS;
			}
			if (sta_black_list) {
				/* remove it from black list before adding to the white list */
				sap_warn("STA present in black list so first remove from it");
				sap_remove_mac_from_acl(sap_ctx->denyMacList,
						    &sap_ctx->nDenyMac,
						    staBLIndex);
			}
			sap_debug("... Now add to the white list");
			sap_add_mac_to_acl(sap_ctx->acceptMacList,
					       &sap_ctx->nAcceptMac,
			       peer_sta_mac);
				sap_debug("size of accept and deny lists %d %d",
					  sap_ctx->nAcceptMac,
					  sap_ctx->nDenyMac);
		} else if (cmd == DELETE_STA_FROM_ACL) {
			if (sta_white_list) {

				struct csr_del_sta_params delStaParams;

				sap_info("Delete from white list");
				sap_remove_mac_from_acl(sap_ctx->acceptMacList,
						    &sap_ctx->nAcceptMac,
						    staWLIndex);
				/* If a client is deleted from white list and it is connected, send deauth */
				wlansap_populate_del_sta_params(peer_sta_mac,
					eCsrForcedDeauthSta,
					SIR_MAC_MGMT_DEAUTH,
					&delStaParams);
				wlansap_deauth_sta(sap_ctx, &delStaParams);
				sap_debug("size of accept and deny lists %d %d",
					  sap_ctx->nAcceptMac,
					  sap_ctx->nDenyMac);
			} else {
				sap_warn("MAC address to be deleted is not present in the white list "
					 QDF_MAC_ADDR_FMT,
					 QDF_MAC_ADDR_REF(peer_sta_mac));
				return QDF_STATUS_E_FAILURE;
			}
		} else {
			sap_err("Invalid cmd type passed");
			return QDF_STATUS_E_FAILURE;
		}
		break;

	case eSAP_BLACK_LIST:

		if (cmd == ADD_STA_TO_ACL) {
			struct csr_del_sta_params delStaParams;
			/* error check */
			/* if list is already at max, return failure */
			if (sap_ctx->nDenyMac == MAX_ACL_MAC_ADDRESS) {
				sap_err("Black list is already maxed out. Cannot accept "
					QDF_MAC_ADDR_FMT,
					QDF_MAC_ADDR_REF(peer_sta_mac));
				return QDF_STATUS_E_FAILURE;
			}
			if (sta_black_list) {
				/* Do nothing if already present in white list */
				sap_warn("MAC address already present in black list "
					 QDF_MAC_ADDR_FMT,
					 QDF_MAC_ADDR_REF(peer_sta_mac));
				return QDF_STATUS_SUCCESS;
			}
			if (sta_white_list) {
				/* remove it from white list before adding to the black list */
				sap_warn("Present in white list so first remove from it");
				sap_remove_mac_from_acl(sap_ctx->acceptMacList,
						    &sap_ctx->nAcceptMac,
						    staWLIndex);
			}
			/* If we are adding a client to the black list; if its connected, send deauth */
			wlansap_populate_del_sta_params(peer_sta_mac,
				eCsrForcedDeauthSta,
				SIR_MAC_MGMT_DEAUTH,
				&delStaParams);
			wlansap_deauth_sta(sap_ctx, &delStaParams);
			sap_info("... Now add to black list");
			sap_add_mac_to_acl(sap_ctx->denyMacList,
				       &sap_ctx->nDenyMac, peer_sta_mac);
			sap_debug("size of accept and deny lists %d %d",
				  sap_ctx->nAcceptMac,
				  sap_ctx->nDenyMac);
		} else if (cmd == DELETE_STA_FROM_ACL) {
			if (sta_black_list) {
				sap_info("Delete from black list");
				sap_remove_mac_from_acl(sap_ctx->denyMacList,
						    &sap_ctx->nDenyMac,
						    staBLIndex);
				sap_debug("no accept and deny mac %d %d",
					  sap_ctx->nAcceptMac,
					  sap_ctx->nDenyMac);
			} else {
				sap_warn("MAC address to be deleted is not present in the black list "
					  QDF_MAC_ADDR_FMT,
					  QDF_MAC_ADDR_REF(peer_sta_mac));
				return QDF_STATUS_E_FAILURE;
			}
		} else {
			sap_err("Invalid cmd type passed");
			return QDF_STATUS_E_FAILURE;
		}
		break;

	default:
	{
		sap_err("Invalid list type passed %d", list_type);
		return QDF_STATUS_E_FAILURE;
	}
	}
	sap_debug("After modification of ACL");
	sap_debug("*** WHITE LIST ***");
	sap_print_acl(sap_ctx->acceptMacList, sap_ctx->nAcceptMac);
	sap_debug("*** BLACK LIST ***");
	sap_print_acl(sap_ctx->denyMacList, sap_ctx->nDenyMac);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_disassoc_sta(struct sap_context *sap_ctx,
				struct csr_del_sta_params *params)
{
	struct mac_context *mac;

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}

	return sme_roam_disconnect_sta(MAC_HANDLE(mac), sap_ctx->sessionId,
				       params);
}

QDF_STATUS wlansap_deauth_sta(struct sap_context *sap_ctx,
			      struct csr_del_sta_params *params)
{
	struct mac_context *mac;

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}

	return sme_roam_deauth_sta(MAC_HANDLE(mac), sap_ctx->sessionId,
				   params);
}

enum phy_ch_width
wlansap_get_csa_chanwidth_from_phymode(struct sap_context *sap_context,
				       uint32_t chan_freq,
				       struct ch_params *tgt_ch_params)
{
	enum phy_ch_width ch_width, concurrent_bw = 0;
	struct mac_context *mac;
	struct ch_params ch_params = {0};

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return CH_WIDTH_20MHZ;
	}

	if (WLAN_REG_IS_24GHZ_CH_FREQ(chan_freq)) {
		/*
		 * currently OBSS scan is done in hostapd, so to avoid
		 * SAP coming up in HT40 on channel switch we are
		 * disabling channel bonding in 2.4Ghz.
		 */
		ch_width = CH_WIDTH_20MHZ;
	} else {
		ch_width = wlansap_get_max_bw_by_phymode(sap_context);
		concurrent_bw = wlan_sap_get_concurrent_bw(
				mac->pdev, mac->psoc, chan_freq,
				ch_width);
		ch_width = QDF_MIN(ch_width, concurrent_bw);
		if (tgt_ch_params)
			ch_width = QDF_MIN(ch_width, tgt_ch_params->ch_width);
	}
	ch_params.ch_width = ch_width;
	wlan_reg_set_channel_params_for_freq(mac->pdev, chan_freq, 0,
					     &ch_params);
	ch_width = ch_params.ch_width;
	if (tgt_ch_params)
		*tgt_ch_params = ch_params;
	sap_nofl_debug("freq %d bw %d (phymode %d, con bw %d, tgt bw %d)",
		       chan_freq, ch_width,
		       sap_context->csr_roamProfile.phyMode,
		       concurrent_bw,
		       tgt_ch_params ? tgt_ch_params->ch_width : CH_WIDTH_MAX);

	return ch_width;
}

/**
 * sap_start_csa_restart() - send csa start event
 * @mac_ctx: mac ctx
 * @sap_ctx: SAP context
 *
 * Return: QDF_STATUS
 */
static inline void sap_start_csa_restart(struct mac_context *mac,
					 struct sap_context *sap_ctx)
{
	sme_csa_restart(mac, sap_ctx->sessionId);
}

/**
 * sap_get_csa_reason_str() - Get csa reason in string
 * @reason: sap reason enum value
 *
 * Return: string reason
 */
static char *sap_get_csa_reason_str(enum sap_csa_reason_code reason)
{
	switch (reason) {
	case CSA_REASON_UNKNOWN:
		return "UNKNOWN";
	case CSA_REASON_STA_CONNECT_DFS_TO_NON_DFS:
		return "STA_CONNECT_DFS_TO_NON_DFS";
	case CSA_REASON_USER_INITIATED:
		return "USER_INITIATED";
	case CSA_REASON_PEER_ACTION_FRAME:
		return "PEER_ACTION_FRAME";
	case CSA_REASON_PRE_CAC_SUCCESS:
		return "PRE_CAC_SUCCESS";
	case CSA_REASON_CONCURRENT_STA_CHANGED_CHANNEL:
		return "CONCURRENT_STA_CHANGED_CHANNEL";
	case CSA_REASON_UNSAFE_CHANNEL:
		return "UNSAFE_CHANNEL";
	case CSA_REASON_LTE_COEX:
		return "LTE_COEX";
	case CSA_REASON_CONCURRENT_NAN_EVENT:
		return "CONCURRENT_NAN_EVENT";
	case CSA_REASON_BAND_RESTRICTED:
		return "BAND_RESTRICTED";
	case CSA_REASON_DCS:
		return "DCS";
	case CSA_REASON_CHAN_DISABLED:
		return "DISABLED";
	case CSA_REASON_CHAN_PASSIVE:
		return "PASSIVE";
	default:
		return "UNKNOWN";
	}
}

/**
 * wlansap_set_chan_params_for_csa() - Update sap channel parameters
 *    for channel switch
 * @mac: mac ctx
 * @sap_ctx: sap context
 * @target_chan_freq: target channel frequency in MHz
 * @target_bw: target bandwidth
 *
 * Return: QDF_STATUS_SUCCESS for success.
 */
static QDF_STATUS
wlansap_set_chan_params_for_csa(struct mac_context *mac,
				struct sap_context *sap_ctx,
				uint32_t target_chan_freq,
				enum phy_ch_width target_bw)
{
	struct ch_params tmp_ch_params = {0};

	tmp_ch_params.ch_width = target_bw;
	mac->sap.SapDfsInfo.new_chanWidth =
		wlansap_get_csa_chanwidth_from_phymode(sap_ctx,
						       target_chan_freq,
						       &tmp_ch_params);
	/*
	 * Copy the requested target channel
	 * to sap context.
	 */
	mac->sap.SapDfsInfo.target_chan_freq = target_chan_freq;
	mac->sap.SapDfsInfo.new_ch_params.ch_width =
		mac->sap.SapDfsInfo.new_chanWidth;

	/* By this time, the best bandwidth is calculated for
	 * the given target channel. Now, if there was a
	 * request from user to move to a selected bandwidth,
	 * we can see if it can be honored.
	 *
	 * Ex1: BW80 was selected for the target channel and
	 * user wants BW40, it can be allowed
	 * Ex2: BW40 was selected for the target channel and
	 * user wants BW80, it cannot be allowed for the given
	 * target channel.
	 *
	 * So, the MIN of the selected channel bandwidth and
	 * user input is used for the bandwidth
	 */
	if (target_bw != CH_WIDTH_MAX) {
		sap_nofl_debug("SAP CSA: target bw:%d new width:%d",
			       target_bw,
			       mac->sap.SapDfsInfo.new_ch_params.ch_width);
		mac->sap.SapDfsInfo.new_ch_params.ch_width =
			mac->sap.SapDfsInfo.new_chanWidth =
			QDF_MIN(mac->sap.SapDfsInfo.new_ch_params.ch_width,
				target_bw);
	}
	wlan_reg_set_channel_params_for_freq(
		mac->pdev, target_chan_freq, 0,
		&mac->sap.SapDfsInfo.new_ch_params);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_set_channel_change_with_csa(struct sap_context *sap_ctx,
					       uint32_t target_chan_freq,
					       enum phy_ch_width target_bw,
					       bool strict)
{
	struct mac_context *mac;
	mac_handle_t mac_handle;
	bool valid;
	QDF_STATUS status, hw_mode_status;
	bool sta_sap_scc_on_dfs_chan;
	bool is_dfs;
	struct ch_params tmp_ch_params = {0};
	enum channel_state state;

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");

		return QDF_STATUS_E_FAULT;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}
	mac_handle = MAC_HANDLE(mac);

	if (!policy_mgr_is_sap_freq_allowed(mac->psoc, target_chan_freq)) {
		sap_err("%u is unsafe channel freq", target_chan_freq);
		return QDF_STATUS_E_FAULT;
	}
	sap_nofl_debug("SAP CSA: %d ---> %d BW %d conn on 5GHz:%d, csa_reason:%s(%d) strict %d vdev %d",
		       sap_ctx->chan_freq, target_chan_freq, target_bw,
		       policy_mgr_is_any_mode_active_on_band_along_with_session(
		       mac->psoc, sap_ctx->sessionId, POLICY_MGR_BAND_5),
		       sap_get_csa_reason_str(sap_ctx->csa_reason),
		       sap_ctx->csa_reason, strict, sap_ctx->sessionId);
	if (sap_ctx->chan_freq == target_chan_freq)
		return QDF_STATUS_E_FAULT;

	state = wlan_reg_get_channel_state_for_freq(mac->pdev,
						    target_chan_freq);
	if (state == CHANNEL_STATE_DISABLE || state == CHANNEL_STATE_INVALID) {
		sap_nofl_debug("invalid target freq %d state %d",
			       target_chan_freq, state);
		return QDF_STATUS_E_INVAL;
	}

	sta_sap_scc_on_dfs_chan =
		policy_mgr_is_sta_sap_scc_allowed_on_dfs_chan(mac->psoc);

	tmp_ch_params.ch_width = target_bw;
	wlansap_get_csa_chanwidth_from_phymode(sap_ctx,
					       target_chan_freq,
					       &tmp_ch_params);
	if (target_bw != CH_WIDTH_MAX) {
		tmp_ch_params.ch_width =
			QDF_MIN(tmp_ch_params.ch_width, target_bw);
		sap_nofl_debug("target ch_width %d to %d ", target_bw,
			       tmp_ch_params.ch_width);
	}

	wlan_reg_set_channel_params_for_freq(mac->pdev, target_chan_freq, 0,
					     &tmp_ch_params);
	is_dfs = wlan_mlme_check_chan_param_has_dfs(
			mac->pdev, &tmp_ch_params,
			target_chan_freq);
	/*
	 * Now, validate if the passed channel is valid in the
	 * current regulatory domain.
	 */
	if (!is_dfs ||
	    (!policy_mgr_is_any_mode_active_on_band_along_with_session(
			mac->psoc, sap_ctx->sessionId,
			POLICY_MGR_BAND_5) ||
			sta_sap_scc_on_dfs_chan ||
			sap_ctx->csa_reason == CSA_REASON_DCS)) {
		/*
		 * validate target channel switch w.r.t various concurrency
		 * rules set.
		 */
		if (!strict) {
			valid = wlan_sap_validate_channel_switch(mac_handle,
								 target_chan_freq,
								 sap_ctx);
			if (!valid) {
				sap_err("Channel freq switch to %u is not allowed due to concurrent channel interference",
					target_chan_freq);
				return QDF_STATUS_E_FAULT;
			}
		}
		/*
		 * Post a CSA IE request to SAP state machine with
		 * target channel information and also CSA IE required
		 * flag set in sap_ctx only, if SAP is in SAP_STARTED
		 * state.
		 */
		if (sap_ctx->fsm_state == SAP_STARTED) {
			status = wlansap_set_chan_params_for_csa(
					mac, sap_ctx, target_chan_freq,
					target_bw);
			if (QDF_IS_STATUS_ERROR(status))
				return status;

			hw_mode_status =
			  policy_mgr_check_and_set_hw_mode_for_channel_switch(
				   mac->psoc, sap_ctx->sessionId,
				   target_chan_freq,
				   POLICY_MGR_UPDATE_REASON_CHANNEL_SWITCH_SAP);

			/*
			 * If hw_mode_status is QDF_STATUS_E_FAILURE, mean HW
			 * mode change was required but driver failed to set HW
			 * mode so ignore CSA for the channel.
			 */
			if (hw_mode_status == QDF_STATUS_E_FAILURE) {
				sap_err("HW change required but failed to set hw mode");
				return hw_mode_status;
			}

			status = policy_mgr_reset_chan_switch_complete_evt(
								mac->psoc);
			if (QDF_IS_STATUS_ERROR(status)) {
				policy_mgr_check_n_start_opportunistic_timer(
								mac->psoc);
				return status;
			}

			/*
			 * Set the CSA IE required flag.
			 */
			mac->sap.SapDfsInfo.csaIERequired = true;

			/*
			 * Set the radar found status to allow the channel
			 * change to happen same as in the case of a radar
			 * detection. Since, this will allow SAP to be in
			 * correct state and also resume the netif queues
			 * that were suspended in HDD before the channel
			 * request was issued.
			 */
			mac->sap.SapDfsInfo.sap_radar_found_status = true;
			mac->sap.SapDfsInfo.cac_state =
					eSAP_DFS_DO_NOT_SKIP_CAC;
			sap_cac_reset_notify(mac_handle);

			/*
			 * If hw_mode_status is QDF_STATUS_SUCCESS mean HW mode
			 * change was required and was successfully requested so
			 * the channel switch will continue after HW mode change
			 * completion.
			 */
			if (QDF_IS_STATUS_SUCCESS(hw_mode_status)) {
				sap_info("Channel change will continue after HW mode change");
				return QDF_STATUS_SUCCESS;
			}
			/*
			 * If hw_mode_status is QDF_STATUS_E_NOSUPPORT or
			 * QDF_STATUS_E_ALREADY (not QDF_STATUS_E_FAILURE and
			 * not QDF_STATUS_SUCCESS), mean DBS is not supported or
			 * required HW mode is already set, So contunue with
			 * CSA from here.
			 */
			sap_start_csa_restart(mac, sap_ctx);
		} else {
			sap_err("Failed to request Channel Change, since SAP is not in SAP_STARTED state");
			return QDF_STATUS_E_FAULT;
		}

	} else {
		sap_err("Channel freq = %d is not valid in the current"
			"regulatory domain, is_dfs %d", target_chan_freq,
			is_dfs);

		return QDF_STATUS_E_FAULT;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_sap_getstation_ie_information(struct sap_context *sap_ctx,
					      uint32_t *len, uint8_t *buf)
{
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	uint32_t ie_len = 0;

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	if (len) {
		ie_len = *len;
		*len = sap_ctx->nStaWPARSnReqIeLength;
		sap_info("WPAIE len : %x", *len);
		if ((buf) && (ie_len >= sap_ctx->nStaWPARSnReqIeLength)) {
			qdf_mem_copy(buf,
				sap_ctx->pStaWpaRsnReqIE,
				sap_ctx->nStaWPARSnReqIeLength);
			sap_info("WPAIE: "QDF_MAC_ADDR_FMT,
				 QDF_MAC_ADDR_REF(buf));
			qdf_status = QDF_STATUS_SUCCESS;
		}
	}
	return qdf_status;
}

QDF_STATUS wlan_sap_update_next_channel(struct sap_context *sap_ctx,
					uint8_t channel,
					enum phy_ch_width chan_bw)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	sap_ctx->dfs_vendor_channel = channel;
	sap_ctx->dfs_vendor_chan_bw = chan_bw;

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_SAP_COND_CHAN_SWITCH
QDF_STATUS wlan_sap_set_pre_cac_status(struct sap_context *sap_ctx,
				       bool status)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	sap_ctx->is_pre_cac_on = status;
	sap_debug("is_pre_cac_on:%d", sap_ctx->is_pre_cac_on);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_sap_set_chan_freq_before_pre_cac(struct sap_context *sap_ctx,
				      qdf_freq_t freq_before_pre_cac)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	sap_ctx->freq_before_pre_cac = freq_before_pre_cac;
	return QDF_STATUS_SUCCESS;
}
#endif /* FEATURE_SAP_COND_CHAN_SWITCH */

QDF_STATUS wlan_sap_set_pre_cac_complete_status(struct sap_context *sap_ctx,
						bool status)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	sap_ctx->pre_cac_complete = status;

	sap_debug("pre cac complete status:%d session:%d",
		  status, sap_ctx->sessionId);

	return QDF_STATUS_SUCCESS;
}

bool wlan_sap_is_pre_cac_context(struct sap_context *context)
{
	return context && context->is_pre_cac_on;
}

/**
 * wlan_sap_is_pre_cac_active() - Checks if pre cac in in progress
 * @handle: Global MAC handle
 *
 * Checks if pre cac is in progress in any of the SAP contexts
 *
 * Return: True is pre cac is active, false otherwise
 */
bool wlan_sap_is_pre_cac_active(mac_handle_t handle)
{
	struct mac_context *mac = NULL;
	struct sap_ctx_list *ctx_list;
	int i;

	mac = MAC_CONTEXT(handle);
	if (!mac) {
		sap_err("Invalid mac context");
		return false;
	}

	ctx_list = mac->sap.sapCtxList;
	for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
		if (wlan_sap_is_pre_cac_context(ctx_list[i].sap_context))
			return true;
	}

	return false;
}

/**
 * wlan_sap_get_pre_cac_vdev_id() - Get vdev id of the pre cac interface
 * @handle: Global handle
 * @vdev_id: vdev id
 *
 * Fetches the vdev id of the pre cac interface
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_sap_get_pre_cac_vdev_id(mac_handle_t handle, uint8_t *vdev_id)
{
	struct mac_context *mac = NULL;
	uint8_t i;

	mac = MAC_CONTEXT(handle);
	if (!mac) {
		sap_err("Invalid mac context");
		return QDF_STATUS_E_FAULT;
	}

	for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
		struct sap_context *context =
			mac->sap.sapCtxList[i].sap_context;
		if (context && context->is_pre_cac_on) {
			*vdev_id = i;
			return QDF_STATUS_SUCCESS;
		}
	}
	return QDF_STATUS_E_FAILURE;
}

void wlansap_get_sec_channel(uint8_t sec_ch_offset,
			     uint32_t op_chan_freq,
			     uint32_t *sec_chan_freq)
{
	switch (sec_ch_offset) {
	case LOW_PRIMARY_CH:
		*sec_chan_freq = op_chan_freq + 20;
		break;
	case HIGH_PRIMARY_CH:
		*sec_chan_freq = op_chan_freq - 20;
		break;
	default:
		*sec_chan_freq = 0;
	}
}

static void
wlansap_set_cac_required_for_chan(struct mac_context *mac_ctx,
				  struct sap_context *sap_ctx)
{
	bool is_ch_dfs = false;
	bool cac_required;
	uint32_t chan_freq;
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t sta_cnt, i;

	chan_freq = sap_ctx->chan_freq;
	if (sap_ctx->ch_params.ch_width == CH_WIDTH_160MHZ) {
		is_ch_dfs = true;
	} else if (sap_ctx->ch_params.ch_width == CH_WIDTH_80P80MHZ) {
		if (wlan_reg_get_channel_state_for_freq(
						mac_ctx->pdev,
						sap_ctx->chan_freq) ==
		    CHANNEL_STATE_DFS ||
		    wlan_reg_get_channel_state_for_freq(
					mac_ctx->pdev,
					sap_ctx->ch_params.mhz_freq_seg1) ==
				CHANNEL_STATE_DFS)
			is_ch_dfs = true;
	} else {
		if (wlan_reg_get_channel_state_for_freq(
						mac_ctx->pdev,
						sap_ctx->chan_freq) ==
		    CHANNEL_STATE_DFS)
			is_ch_dfs = true;
	}
	if (WLAN_REG_IS_6GHZ_CHAN_FREQ(sap_ctx->chan_freq))
		is_ch_dfs = false;

	sap_debug("vdev id %d chan %d is_ch_dfs %d pre_cac_complete %d ignore_cac %d cac_state %d",
		  sap_ctx->sessionId, chan_freq, is_ch_dfs,
		  sap_ctx->pre_cac_complete, mac_ctx->sap.SapDfsInfo.ignore_cac,
		  mac_ctx->sap.SapDfsInfo.cac_state);

	if (!is_ch_dfs || sap_ctx->pre_cac_complete ||
	    mac_ctx->sap.SapDfsInfo.ignore_cac ||
	    (mac_ctx->sap.SapDfsInfo.cac_state == eSAP_DFS_SKIP_CAC))
		cac_required = false;
	else
		cac_required = true;

	if (cac_required) {
		sta_cnt =
		  policy_mgr_get_mode_specific_conn_info(mac_ctx->psoc,
							 freq_list,
							 vdev_id_list,
							 PM_STA_MODE);

		for (i = 0; i < sta_cnt; i++) {
			if (sap_ctx->chan_freq == freq_list[i]) {
				sap_debug("STA vdev id %d exists, ignore CAC",
					  vdev_id_list[i]);
				cac_required = false;
			}
		}
	}

	mlme_set_cac_required(sap_ctx->vdev, cac_required);
}

QDF_STATUS wlansap_channel_change_request(struct sap_context *sap_ctx,
					  uint32_t target_chan_freq)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac_ctx;
	eCsrPhyMode phy_mode;
	struct ch_params *ch_params;

	if (!target_chan_freq) {
		sap_err("channel 0 requested");
		return QDF_STATUS_E_FAULT;
	}

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	mac_ctx = sap_get_mac_context();
	if (!mac_ctx) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}

	phy_mode = sap_ctx->csr_roamProfile.phyMode;

	/* Update phy_mode if the target channel is in the other band */
	if (WLAN_REG_IS_5GHZ_CH_FREQ(target_chan_freq) &&
	    ((phy_mode == eCSR_DOT11_MODE_11g) ||
	    (phy_mode == eCSR_DOT11_MODE_11g_ONLY)))
		phy_mode = eCSR_DOT11_MODE_11a;
	else if (WLAN_REG_IS_24GHZ_CH_FREQ(target_chan_freq) &&
		 (phy_mode == eCSR_DOT11_MODE_11a))
		phy_mode = eCSR_DOT11_MODE_11g;

	sap_ctx->csr_roamProfile.phyMode = phy_mode;

	if (sap_ctx->csr_roamProfile.ChannelInfo.numOfChannels == 0 ||
	    !sap_ctx->csr_roamProfile.ChannelInfo.freq_list) {
		sap_err("Invalid channel list");
		return QDF_STATUS_E_FAULT;
	}
	sap_ctx->csr_roamProfile.ChannelInfo.freq_list[0] = target_chan_freq;

	/*
	 * We are getting channel bonding mode from sapDfsInfor structure
	 * because we've implemented channel width fallback mechanism for DFS
	 * which will result in channel width changing dynamically.
	 */
	ch_params = &mac_ctx->sap.SapDfsInfo.new_ch_params;
	wlan_reg_set_channel_params_for_freq(mac_ctx->pdev, target_chan_freq,
			0, ch_params);
	sap_ctx->ch_params = *ch_params;
	sap_ctx->freq_before_ch_switch = sap_ctx->chan_freq;
	/* Update the channel as this will be used to
	 * send event to supplicant
	 */
	sap_ctx->chan_freq = target_chan_freq;
	wlansap_get_sec_channel(ch_params->sec_ch_offset, sap_ctx->chan_freq,
				&sap_ctx->sec_ch_freq);
	sap_ctx->csr_roamProfile.ch_params = *ch_params;
	sap_dfs_set_current_channel(sap_ctx);
	wlansap_set_cac_required_for_chan(mac_ctx, sap_ctx);

	status = sme_roam_channel_change_req(MAC_HANDLE(mac_ctx),
					     sap_ctx->bssid,
					     ch_params,
					     &sap_ctx->csr_roamProfile);

	sap_debug("chan_freq:%d phy_mode %d width:%d offset:%d seg0:%d seg1:%d",
		  sap_ctx->chan_freq, phy_mode, ch_params->ch_width,
		  ch_params->sec_ch_offset, ch_params->center_freq_seg0,
		  ch_params->center_freq_seg1);

	return status;
}

QDF_STATUS wlansap_start_beacon_req(struct sap_context *sap_ctx)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t dfs_cac_wait_status;
	struct mac_context *mac;

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}

	/* No Radar was found during CAC WAIT, So start Beaconing */
	if (mac->sap.SapDfsInfo.sap_radar_found_status == false) {
		/* CAC Wait done without any Radar Detection */
		dfs_cac_wait_status = true;
		sap_ctx->pre_cac_complete = false;
		status = sme_roam_start_beacon_req(MAC_HANDLE(mac),
						   sap_ctx->bssid,
						   dfs_cac_wait_status);
	}

	return status;
}

QDF_STATUS wlansap_dfs_send_csa_ie_request(struct sap_context *sap_ctx)
{
	struct mac_context *mac;

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}

	mac->sap.SapDfsInfo.new_ch_params.ch_width =
				mac->sap.SapDfsInfo.new_chanWidth;
	wlan_reg_set_channel_params_for_freq(mac->pdev,
			mac->sap.SapDfsInfo.target_chan_freq,
			0, &mac->sap.SapDfsInfo.new_ch_params);

	sap_debug("chan freq:%d req:%d width:%d off:%d",
		  mac->sap.SapDfsInfo.target_chan_freq,
		  mac->sap.SapDfsInfo.csaIERequired,
		  mac->sap.SapDfsInfo.new_ch_params.ch_width,
		  mac->sap.SapDfsInfo.new_ch_params.sec_ch_offset);

	return sme_roam_csa_ie_request(MAC_HANDLE(mac),
				       sap_ctx->bssid,
				       mac->sap.SapDfsInfo.target_chan_freq,
				       mac->sap.SapDfsInfo.csaIERequired,
				       &mac->sap.SapDfsInfo.new_ch_params);
}

QDF_STATUS wlansap_get_dfs_ignore_cac(mac_handle_t mac_handle,
				      uint8_t *ignore_cac)
{
	struct mac_context *mac = NULL;

	if (mac_handle) {
		mac = MAC_CONTEXT(mac_handle);
	} else {
		sap_err("Invalid mac_handle pointer");
		return QDF_STATUS_E_FAULT;
	}

	*ignore_cac = mac->sap.SapDfsInfo.ignore_cac;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_set_dfs_ignore_cac(mac_handle_t mac_handle,
				      uint8_t ignore_cac)
{
	struct mac_context *mac = NULL;

	if (mac_handle) {
		mac = MAC_CONTEXT(mac_handle);
	} else {
		sap_err("Invalid mac_handle pointer");
		return QDF_STATUS_E_FAULT;
	}

	mac->sap.SapDfsInfo.ignore_cac = (ignore_cac >= true) ?
					  true : false;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_get_dfs_cac_state(mac_handle_t mac_handle,
				     eSapDfsCACState_t *cac_state)
{
	struct mac_context *mac = NULL;

	if (mac_handle) {
		mac = MAC_CONTEXT(mac_handle);
	} else {
		sap_err("Invalid mac_handle pointer");
		return QDF_STATUS_E_FAULT;
	}

	*cac_state = mac->sap.SapDfsInfo.cac_state;
	return QDF_STATUS_SUCCESS;
}

bool sap_is_auto_channel_select(struct sap_context *sapcontext)
{
	if (!sapcontext) {
		sap_err("Invalid SAP pointer");
		return false;
	}
	return sapcontext->chan_freq == AUTO_CHANNEL_SELECT;
}

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
/**
 * wlan_sap_set_channel_avoidance() - sets sap mcc channel avoidance ini param
 * @mac_handle: Opaque handle to the global MAC context
 * @sap_channel_avoidance: ini parameter value
 *
 * sets sap mcc channel avoidance ini param, to be called in sap_start
 *
 * Return: success of failure of operation
 */
QDF_STATUS
wlan_sap_set_channel_avoidance(mac_handle_t mac_handle,
			       bool sap_channel_avoidance)
{
	struct mac_context *mac_ctx = NULL;

	if (mac_handle) {
		mac_ctx = MAC_CONTEXT(mac_handle);
	} else {
		sap_err("mac_handle or mac_ctx pointer NULL");
		return QDF_STATUS_E_FAULT;
	}
	mac_ctx->sap.sap_channel_avoidance = sap_channel_avoidance;
	return QDF_STATUS_SUCCESS;
}
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

QDF_STATUS
wlan_sap_set_acs_with_more_param(mac_handle_t mac_handle,
				 bool acs_with_more_param)
{
	struct mac_context *mac_ctx;

	if (mac_handle) {
		mac_ctx = MAC_CONTEXT(mac_handle);
	} else {
		sap_err("mac_handle or mac_ctx pointer NULL");
		return QDF_STATUS_E_FAULT;
	}
	mac_ctx->sap.acs_with_more_param = acs_with_more_param;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlansap_set_dfs_preferred_channel_location(mac_handle_t mac_handle)
{
	struct mac_context *mac = NULL;
	QDF_STATUS status;
	enum dfs_reg dfs_region;
	uint8_t dfs_preferred_channels_location = 0;

	if (mac_handle) {
		mac = MAC_CONTEXT(mac_handle);
	} else {
		sap_err("Invalid mac_handle pointer");
		return QDF_STATUS_E_FAULT;
	}

	wlan_reg_get_dfs_region(mac->pdev, &dfs_region);

	/*
	 * The Indoor/Outdoor only random channel selection
	 * restriction is currently enforeced only for
	 * JAPAN regulatory domain.
	 */
	ucfg_mlme_get_pref_chan_location(mac->psoc,
					 &dfs_preferred_channels_location);
	sap_debug("dfs_preferred_channels_location %d",
		  dfs_preferred_channels_location);

	if (DFS_MKK_REGION == dfs_region) {
		mac->sap.SapDfsInfo.sap_operating_chan_preferred_location =
			dfs_preferred_channels_location;
		sap_debug("sapdfs:Set Preferred Operating Channel location=%d",
			  mac->sap.SapDfsInfo.
			  sap_operating_chan_preferred_location);

		status = QDF_STATUS_SUCCESS;
	} else {
		sap_debug("sapdfs:NOT JAPAN REG, Invalid Set preferred chans location");

		status = QDF_STATUS_E_FAULT;
	}

	return status;
}

QDF_STATUS wlansap_set_dfs_target_chnl(mac_handle_t mac_handle,
				       uint32_t target_chan_freq)
{
	struct mac_context *mac = NULL;

	if (mac_handle) {
		mac = MAC_CONTEXT(mac_handle);
	} else {
		sap_err("Invalid mac_handle pointer");
		return QDF_STATUS_E_FAULT;
	}
	if (target_chan_freq > 0) {
		mac->sap.SapDfsInfo.user_provided_target_chan_freq =
			target_chan_freq;
	} else {
		mac->sap.SapDfsInfo.user_provided_target_chan_freq = 0;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlansap_update_sap_config_add_ie(struct sap_config *config,
				 const uint8_t *pAdditionIEBuffer,
				 uint16_t additionIELength,
				 eUpdateIEsType updateType)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t bufferValid = false;
	uint16_t bufferLength = 0;
	uint8_t *pBuffer = NULL;

	if (!config) {
		return QDF_STATUS_E_FAULT;
	}

	if ((pAdditionIEBuffer) && (additionIELength != 0)) {
		/* initialize the buffer pointer so that pe can copy */
		if (additionIELength > 0) {
			bufferLength = additionIELength;
			pBuffer = qdf_mem_malloc(bufferLength);
			if (!pBuffer)
				return QDF_STATUS_E_NOMEM;

			qdf_mem_copy(pBuffer, pAdditionIEBuffer, bufferLength);
			bufferValid = true;
			sap_debug("update_type: %d", updateType);
			qdf_trace_hex_dump(QDF_MODULE_ID_SAP,
				QDF_TRACE_LEVEL_DEBUG, pBuffer, bufferLength);
		}
	}

	switch (updateType) {
	case eUPDATE_IE_PROBE_BCN:
		if (config->pProbeRespBcnIEsBuffer)
			qdf_mem_free(config->pProbeRespBcnIEsBuffer);
		if (bufferValid) {
			config->probeRespBcnIEsLen = bufferLength;
			config->pProbeRespBcnIEsBuffer = pBuffer;
		} else {
			config->probeRespBcnIEsLen = 0;
			config->pProbeRespBcnIEsBuffer = NULL;
		}
		break;
	case eUPDATE_IE_PROBE_RESP:
		if (config->pProbeRespIEsBuffer)
			qdf_mem_free(config->pProbeRespIEsBuffer);
		if (bufferValid) {
			config->probeRespIEsBufferLen = bufferLength;
			config->pProbeRespIEsBuffer = pBuffer;
		} else {
			config->probeRespIEsBufferLen = 0;
			config->pProbeRespIEsBuffer = NULL;
		}
		break;
	case eUPDATE_IE_ASSOC_RESP:
		if (config->pAssocRespIEsBuffer)
			qdf_mem_free(config->pAssocRespIEsBuffer);
		if (bufferValid) {
			config->assocRespIEsLen = bufferLength;
			config->pAssocRespIEsBuffer = pBuffer;
		} else {
			config->assocRespIEsLen = 0;
			config->pAssocRespIEsBuffer = NULL;
		}
		break;
	default:
		sap_debug("No matching buffer type %d", updateType);
		if (pBuffer)
			qdf_mem_free(pBuffer);
		break;
	}

	return status;
}

QDF_STATUS
wlansap_reset_sap_config_add_ie(struct sap_config *config,
				eUpdateIEsType updateType)
{
	if (!config) {
		sap_err("Invalid Config pointer");
		return QDF_STATUS_E_FAULT;
	}

	switch (updateType) {
	case eUPDATE_IE_ALL:    /*only used to reset */
	case eUPDATE_IE_PROBE_RESP:
		if (config->pProbeRespIEsBuffer) {
			qdf_mem_free(config->pProbeRespIEsBuffer);
			config->probeRespIEsBufferLen = 0;
			config->pProbeRespIEsBuffer = NULL;
		}
		if (eUPDATE_IE_ALL != updateType)
			break;
		/* fallthrough */
	case eUPDATE_IE_ASSOC_RESP:
		if (config->pAssocRespIEsBuffer) {
			qdf_mem_free(config->pAssocRespIEsBuffer);
			config->assocRespIEsLen = 0;
			config->pAssocRespIEsBuffer = NULL;
		}
		if (eUPDATE_IE_ALL != updateType)
			break;
		/* fallthrough */
	case eUPDATE_IE_PROBE_BCN:
		if (config->pProbeRespBcnIEsBuffer) {
			qdf_mem_free(config->pProbeRespBcnIEsBuffer);
			config->probeRespBcnIEsLen = 0;
			config->pProbeRespBcnIEsBuffer = NULL;
		}
		if (eUPDATE_IE_ALL != updateType)
			break;
		/* fallthrough */
	default:
		if (eUPDATE_IE_ALL != updateType)
			sap_err("Invalid buffer type %d", updateType);
		break;
	}
	return QDF_STATUS_SUCCESS;
}

#define ACS_WLAN_20M_CH_INC 20
#define ACS_2G_EXTEND ACS_WLAN_20M_CH_INC
#define ACS_5G_EXTEND (ACS_WLAN_20M_CH_INC * 3)

#ifdef CONFIG_BAND_6GHZ
static void wlansap_update_start_range_6ghz(
	uint32_t *start_ch_freq, uint32_t *bandStartChannel)
{
	*bandStartChannel = MIN_6GHZ_CHANNEL;
	*start_ch_freq = (*start_ch_freq - ACS_5G_EXTEND) >
				wlan_reg_ch_to_freq(MIN_6GHZ_CHANNEL) ?
			   (*start_ch_freq - ACS_5G_EXTEND) :
				wlan_reg_ch_to_freq(MIN_6GHZ_CHANNEL);
}

static void wlansap_update_end_range_6ghz(
	uint32_t *end_ch_freq, uint32_t *bandEndChannel)
{
	*bandEndChannel = MAX_6GHZ_CHANNEL;
	*end_ch_freq = (*end_ch_freq + ACS_5G_EXTEND) <=
			     wlan_reg_ch_to_freq(MAX_6GHZ_CHANNEL) ?
			     (*end_ch_freq + ACS_5G_EXTEND) :
			     wlan_reg_ch_to_freq(MAX_6GHZ_CHANNEL);
}
#else
static void wlansap_update_start_range_6ghz(
	uint32_t *start_ch_freq, uint32_t *bandStartChannel)
{
}

static void wlansap_update_end_range_6ghz(
	uint32_t *end_ch_freq, uint32_t *bandEndChannel)
{
}
#endif

/*==========================================================================
   FUNCTION  wlansap_extend_to_acs_range

   DESCRIPTION Function extends give channel range to consider ACS chan bonding

   DEPENDENCIES PARAMETERS

   IN /OUT
   * start_ch_freq : ACS extend start ch
   * end_ch_freq   : ACS extended End ch
   * bandStartChannel: Band start ch
   * bandEndChannel  : Band end ch

   RETURN VALUE NONE

   SIDE EFFECTS
   ============================================================================*/
void wlansap_extend_to_acs_range(mac_handle_t mac_handle,
				 uint32_t *start_ch_freq,
				 uint32_t *end_ch_freq,
				 uint32_t *bandStartChannel,
				 uint32_t *bandEndChannel)
{
	uint32_t tmp_start_ch_freq = 0, tmp_end_ch_freq = 0;
	struct mac_context *mac_ctx;

	mac_ctx = MAC_CONTEXT(mac_handle);
	if (!mac_ctx) {
		sap_err("Invalid mac_ctx");
		return;
	}
	if (*start_ch_freq <= wlan_reg_ch_to_freq(CHAN_ENUM_2484)) {
		*bandStartChannel = CHAN_ENUM_2412;
		tmp_start_ch_freq = *start_ch_freq >
					wlan_reg_ch_to_freq(CHAN_ENUM_2432) ?
					(*start_ch_freq - ACS_2G_EXTEND) :
					wlan_reg_ch_to_freq(CHAN_ENUM_2412);
	} else if (*start_ch_freq <= wlan_reg_ch_to_freq(CHAN_ENUM_5885)) {
		*bandStartChannel = CHAN_ENUM_5180;
		tmp_start_ch_freq = (*start_ch_freq - ACS_5G_EXTEND) >
					wlan_reg_ch_to_freq(CHAN_ENUM_5180) ?
				   (*start_ch_freq - ACS_5G_EXTEND) :
					wlan_reg_ch_to_freq(CHAN_ENUM_5180);
	} else if (WLAN_REG_IS_6GHZ_CHAN_FREQ(*start_ch_freq)) {
		tmp_start_ch_freq = *start_ch_freq;
		wlansap_update_start_range_6ghz(&tmp_start_ch_freq,
						bandStartChannel);
	} else {
		*bandStartChannel = CHAN_ENUM_2412;
		tmp_start_ch_freq = *start_ch_freq >
					wlan_reg_ch_to_freq(CHAN_ENUM_2432) ?
					(*start_ch_freq - ACS_2G_EXTEND) :
					wlan_reg_ch_to_freq(CHAN_ENUM_2412);
		sap_err("unexpected start freq %d",
			*start_ch_freq);
	}

	if (*end_ch_freq <= wlan_reg_ch_to_freq(CHAN_ENUM_2484)) {
		*bandEndChannel = CHAN_ENUM_2472;
		tmp_end_ch_freq = (*end_ch_freq + ACS_2G_EXTEND) <=
					wlan_reg_ch_to_freq(CHAN_ENUM_2472) ?
					(*end_ch_freq + ACS_2G_EXTEND) :
					wlan_reg_ch_to_freq(CHAN_ENUM_2484);
	} else if (*end_ch_freq <= wlan_reg_ch_to_freq(CHAN_ENUM_5885)) {
		*bandEndChannel = CHAN_ENUM_5885;
		tmp_end_ch_freq = (*end_ch_freq + ACS_5G_EXTEND) <=
				     wlan_reg_ch_to_freq(CHAN_ENUM_5885) ?
				     (*end_ch_freq + ACS_5G_EXTEND) :
				     wlan_reg_ch_to_freq(CHAN_ENUM_5885);
	} else if (WLAN_REG_IS_6GHZ_CHAN_FREQ(*end_ch_freq)) {
		tmp_end_ch_freq = *end_ch_freq;
		wlansap_update_end_range_6ghz(&tmp_end_ch_freq,
					      bandEndChannel);
	} else {
		*bandEndChannel = CHAN_ENUM_5885;
		tmp_end_ch_freq = (*end_ch_freq + ACS_5G_EXTEND) <=
				     wlan_reg_ch_to_freq(CHAN_ENUM_5885) ?
				     (*end_ch_freq + ACS_5G_EXTEND) :
				     wlan_reg_ch_to_freq(CHAN_ENUM_5885);

		sap_err("unexpected end freq %d", *end_ch_freq);
	}
	*start_ch_freq = tmp_start_ch_freq;
	*end_ch_freq = tmp_end_ch_freq;
	/* Note if the ACS range include only DFS channels, do not cross range
	* Active scanning in adjacent non DFS channels results in transmission
	* spikes in DFS specturm channels which is due to emission spill.
	* Remove the active channels from extend ACS range for DFS only range
	*/
	if (wlan_reg_is_dfs_for_freq(mac_ctx->pdev, *start_ch_freq)) {
		while (!wlan_reg_is_dfs_for_freq(
				mac_ctx->pdev,
				tmp_start_ch_freq) &&
		       tmp_start_ch_freq < *start_ch_freq)
			tmp_start_ch_freq += ACS_WLAN_20M_CH_INC;

		*start_ch_freq = tmp_start_ch_freq;
	}
	if (wlan_reg_is_dfs_for_freq(mac_ctx->pdev, *end_ch_freq)) {
		while (!wlan_reg_is_dfs_for_freq(
				mac_ctx->pdev,
				tmp_end_ch_freq) &&
		       tmp_end_ch_freq > *end_ch_freq)
			tmp_end_ch_freq -= ACS_WLAN_20M_CH_INC;

		*end_ch_freq = tmp_end_ch_freq;
	}
}

QDF_STATUS wlan_sap_set_vendor_acs(struct sap_context *sap_context,
				   bool is_vendor_acs)
{
	if (!sap_context) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}
	sap_context->vendor_acs_dfs_lte_enabled = is_vendor_acs;

	return QDF_STATUS_SUCCESS;
}

#ifdef DFS_COMPONENT_ENABLE
QDF_STATUS wlansap_set_dfs_nol(struct sap_context *sap_ctx,
			       eSapDfsNolType conf)
{
	struct mac_context *mac;

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}

	if (conf == eSAP_DFS_NOL_CLEAR) {
		struct wlan_objmgr_pdev *pdev;

		sap_err("clear the DFS NOL");

		pdev = mac->pdev;
		if (!pdev) {
			sap_err("null pdev");
			return QDF_STATUS_E_FAULT;
		}
		utils_dfs_clear_nol_channels(pdev);
	} else if (conf == eSAP_DFS_NOL_RANDOMIZE) {
		sap_err("Randomize the DFS NOL");

	} else {
		sap_err("unsupport type %d", conf);
	}

	return QDF_STATUS_SUCCESS;
}
#endif

void wlansap_populate_del_sta_params(const uint8_t *mac,
				     uint16_t reason_code,
				     uint8_t subtype,
				     struct csr_del_sta_params *params)
{
	if (!mac)
		qdf_set_macaddr_broadcast(&params->peerMacAddr);
	else
		qdf_mem_copy(params->peerMacAddr.bytes, mac,
			     QDF_MAC_ADDR_SIZE);

	if (reason_code == 0)
		params->reason_code = REASON_DEAUTH_NETWORK_LEAVING;
	else
		params->reason_code = reason_code;

	if (subtype == SIR_MAC_MGMT_DEAUTH || subtype == SIR_MAC_MGMT_DISASSOC)
		params->subtype = subtype;
	else
		params->subtype = SIR_MAC_MGMT_DEAUTH;

	sap_debug("Delete STA with RC:%hu subtype:%hhu MAC::" QDF_MAC_ADDR_FMT,
		  params->reason_code, params->subtype,
		  QDF_MAC_ADDR_REF(params->peerMacAddr.bytes));
}

void sap_undo_acs(struct sap_context *sap_ctx, struct sap_config *sap_cfg)
{
	struct sap_acs_cfg *acs_cfg;

	if (!sap_ctx)
		return;

	acs_cfg = &sap_cfg->acs_cfg;
	if (!acs_cfg)
		return;

	if (acs_cfg->freq_list) {
		sap_debug("Clearing ACS cfg ch freq list");
		qdf_mem_free(acs_cfg->freq_list);
		acs_cfg->freq_list = NULL;
	}
	if (acs_cfg->master_freq_list) {
		sap_debug("Clearing master ACS cfg chan freq list");
		qdf_mem_free(acs_cfg->master_freq_list);
		acs_cfg->master_freq_list = NULL;
	}
	if (sap_ctx->freq_list) {
		sap_debug("Clearing sap context ch freq list");
		qdf_mem_free(sap_ctx->freq_list);
		sap_ctx->freq_list = NULL;
	}
	acs_cfg->ch_list_count = 0;
	acs_cfg->master_ch_list_count = 0;
	acs_cfg->acs_mode = false;
	sap_ctx->num_of_channel = 0;
	wlansap_dcs_set_vdev_wlan_interference_mitigation(sap_ctx, false);
}

QDF_STATUS wlansap_acs_chselect(struct sap_context *sap_context,
				sap_event_cb acs_event_callback,
				struct sap_config *config,
				void *pusr_context)
{
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac;

	if (!sap_context) {
		sap_err("Invalid SAP pointer");

		return QDF_STATUS_E_FAULT;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}

	sap_context->acs_cfg = &config->acs_cfg;
	sap_context->ch_width_orig = config->acs_cfg.ch_width;
	sap_context->csr_roamProfile.phyMode = config->acs_cfg.hw_mode;

	/*
	 * Now, configure the scan and ACS channel params
	 * to issue a scan request.
	 */
	wlansap_set_scan_acs_channel_params(config, sap_context,
					    pusr_context);

	/*
	 * Copy the HDD callback function to report the
	 * ACS result after scan in SAP context callback function.
	 */
	sap_context->sap_event_cb = acs_event_callback;

	/*
	 * Issue the scan request. This scan request is
	 * issued before the start BSS is done so
	 *
	 * 1. No need to pass the second parameter
	 * as the SAP state machine is not started yet
	 * and there is no need for any event posting.
	 *
	 * 2. Set third parameter to TRUE to indicate the
	 * channel selection function to register a
	 * different scan callback function to process
	 * the results pre start BSS.
	 */
	qdf_status = sap_channel_sel(sap_context);

	if (QDF_STATUS_E_ABORTED == qdf_status) {
		sap_err("DFS not supported in the current operating mode");
		return QDF_STATUS_E_FAILURE;
	} else if (QDF_STATUS_E_CANCELED == qdf_status) {
		/*
		* ERROR is returned when either the SME scan request
		* failed or ACS is overridden due to other constrainst
		* So send selected channel to HDD
		*/
		sap_err("Scan Req Failed/ACS Overridden");
		sap_err("Selected channel frequency = %d",
			sap_context->chan_freq);

		return sap_signal_hdd_event(sap_context, NULL,
				eSAP_ACS_CHANNEL_SELECTED,
				(void *) eSAP_STATUS_SUCCESS);
	}

	return qdf_status;
}

/**
 * wlan_sap_enable_phy_error_logs() - Enable DFS phy error logs
 * @mac_handle: Opaque handle to the global MAC context
 * @enable_log: value to set
 *
 * Since the frequency of DFS phy error is very high, enabling logs for them
 * all the times can cause crash and will also create lot of useless logs
 * causing difficulties in debugging other issue. This function will be called
 * from iwpriv cmd to enable such logs temporarily.
 *
 * Return: void
 */
void wlan_sap_enable_phy_error_logs(mac_handle_t mac_handle,
				    uint32_t enable_log)
{
	int error;

	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	mac_ctx->sap.enable_dfs_phy_error_logs = !!enable_log;
	tgt_dfs_control(mac_ctx->pdev, DFS_SET_DEBUG_LEVEL, &enable_log,
			sizeof(uint32_t), NULL, NULL, &error);
}

#ifdef DFS_PRI_MULTIPLIER
void wlan_sap_set_dfs_pri_multiplier(mac_handle_t mac_handle)
{
	int error;

	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	tgt_dfs_control(mac_ctx->pdev, DFS_SET_PRI_MULTIPILER,
			&mac_ctx->mlme_cfg->dfs_cfg.dfs_pri_multiplier,
			sizeof(uint32_t), NULL, NULL, &error);
}
#endif

uint32_t wlansap_get_chan_width(struct sap_context *sap_ctx)
{
	return wlan_sap_get_vht_ch_width(sap_ctx);
}

enum phy_ch_width
wlansap_get_max_bw_by_phymode(struct sap_context *sap_ctx)
{
	uint32_t max_fw_bw;
	enum phy_ch_width ch_width;

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return CH_WIDTH_20MHZ;
	}

	if (sap_ctx->csr_roamProfile.phyMode ==
	    eCSR_DOT11_MODE_11ac ||
	    sap_ctx->csr_roamProfile.phyMode ==
	    eCSR_DOT11_MODE_11ac_ONLY ||
	    sap_ctx->csr_roamProfile.phyMode ==
	    eCSR_DOT11_MODE_11ax ||
	    sap_ctx->csr_roamProfile.phyMode ==
	    eCSR_DOT11_MODE_11ax_ONLY) {
		max_fw_bw = sme_get_vht_ch_width();
		if (max_fw_bw >= WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ)
			ch_width = CH_WIDTH_160MHZ;
		else
			ch_width = CH_WIDTH_80MHZ;
	} else if (sap_ctx->csr_roamProfile.phyMode ==
		   eCSR_DOT11_MODE_11n ||
		   sap_ctx->csr_roamProfile.phyMode ==
		   eCSR_DOT11_MODE_11n_ONLY) {
		ch_width = CH_WIDTH_40MHZ;
	} else {
		/* For legacy 11a mode return 20MHz */
		ch_width = CH_WIDTH_20MHZ;
	}

	return ch_width;
}

QDF_STATUS wlansap_set_invalid_session(struct sap_context *sap_ctx)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAILURE;
	}

	sap_ctx->sessionId = WLAN_UMAC_VDEV_ID_MAX;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_release_vdev_ref(struct sap_context *sap_ctx)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAILURE;
	}

	sap_release_vdev_ref(sap_ctx);

	return QDF_STATUS_SUCCESS;
}

void wlansap_cleanup_cac_timer(struct sap_context *sap_ctx)
{
	struct mac_context *mac;

	if (!sap_ctx) {
		sap_debug("Invalid SAP context");
		return;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return;
	}

	if (mac->sap.SapDfsInfo.is_dfs_cac_timer_running) {
		mac->sap.SapDfsInfo.is_dfs_cac_timer_running = 0;
		if (!sap_ctx->dfs_cac_offload) {
			qdf_mc_timer_stop(
				&mac->sap.SapDfsInfo.sap_dfs_cac_timer);
			qdf_mc_timer_destroy(
				&mac->sap.SapDfsInfo.sap_dfs_cac_timer);
		}
		sap_err("sapdfs, force cleanup running dfs cac timer");
	}
}

#define DH_OUI_TYPE	(0x20)
/**
 * wlansap_validate_owe_ie() - validate OWE IE
 * @ie: IE buffer
 * @remaining_ie_len: remaining IE length
 *
 * Return: validated IE length, negative for failure
 */
static int wlansap_validate_owe_ie(const uint8_t *ie, uint32_t remaining_ie_len)
{
	uint8_t ie_id, ie_len, ie_ext_id = 0;

	if (remaining_ie_len < 2) {
		sap_err("IE too short");
		return -EINVAL;
	}

	ie_id = ie[0];
	ie_len = ie[1];

	/* IEs that we are expecting in OWE IEs
	 * - RSN IE
	 * - DH IE
	 */
	switch (ie_id) {
	case DOT11F_EID_RSN:
		if (ie_len < DOT11F_IE_RSN_MIN_LEN ||
		    ie_len > DOT11F_IE_RSN_MAX_LEN) {
			sap_err("Invalid RSN IE len %d", ie_len);
			return -EINVAL;
		}
		ie_len += 2;
		break;
	case DOT11F_EID_DH_PARAMETER_ELEMENT:
		ie_ext_id = ie[2];
		if (ie_ext_id != DH_OUI_TYPE) {
			sap_err("Invalid DH IE ID %d", ie_ext_id);
			return -EINVAL;
		}
		if (ie_len < DOT11F_IE_DH_PARAMETER_ELEMENT_MIN_LEN ||
		    ie_len > DOT11F_IE_DH_PARAMETER_ELEMENT_MAX_LEN) {
			sap_err("Invalid DH IE len %d", ie_len);
			return -EINVAL;
		}
		ie_len += 2;
		break;
	default:
		sap_err("Invalid IE %d", ie_id);
		return -EINVAL;
	}

	if (ie_len > remaining_ie_len) {
		sap_err("Invalid IE len");
		return -EINVAL;
	}

	return ie_len;
}

/**
 * wlansap_validate_owe_ies() - validate OWE IEs
 * @ie: IE buffer
 * @ie_len: IE length
 *
 * Return: true if validated
 */
static bool wlansap_validate_owe_ies(const uint8_t *ie, uint32_t ie_len)
{
	const uint8_t *remaining_ie = ie;
	uint32_t remaining_ie_len = ie_len;
	int validated_len;
	bool validated = true;

	while (remaining_ie_len) {
		validated_len = wlansap_validate_owe_ie(remaining_ie,
							remaining_ie_len);
		if (validated_len < 0) {
			validated = false;
			break;
		}
		remaining_ie += validated_len;
		remaining_ie_len -= validated_len;
	}

	return validated;
}

QDF_STATUS wlansap_update_owe_info(struct sap_context *sap_ctx,
				   uint8_t *peer, const uint8_t *ie,
				   uint32_t ie_len, uint16_t owe_status)
{
	struct mac_context *mac;
	struct owe_assoc_ind *owe_assoc_ind;
	struct assoc_ind *assoc_ind = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!wlansap_validate_owe_ies(ie, ie_len)) {
		sap_err("Invalid OWE IE");
		return QDF_STATUS_E_FAULT;
	}

	if (!sap_ctx) {
		sap_err("Invalid SAP context");
		return QDF_STATUS_E_FAULT;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}

	if (QDF_STATUS_SUCCESS !=
		qdf_list_peek_front(&sap_ctx->owe_pending_assoc_ind_list,
				    &next_node)) {
		sap_err("Failed to find assoc ind list");
		return QDF_STATUS_E_FAILURE;
	}

	do {
		node = next_node;
		owe_assoc_ind = qdf_container_of(node, struct owe_assoc_ind,
						 node);
		if (qdf_mem_cmp(peer,
				owe_assoc_ind->assoc_ind->peerMacAddr,
				QDF_MAC_ADDR_SIZE) == 0) {
			status = qdf_list_remove_node(
					   &sap_ctx->owe_pending_assoc_ind_list,
					   node);
			if (status != QDF_STATUS_SUCCESS) {
				sap_err("Failed to remove assoc ind");
				return status;
			}
			assoc_ind = owe_assoc_ind->assoc_ind;
			qdf_mem_free(owe_assoc_ind);
			break;
		}
	} while (QDF_STATUS_SUCCESS ==
		 qdf_list_peek_next(&sap_ctx->owe_pending_assoc_ind_list,
				    node, &next_node));

	if (assoc_ind) {
		assoc_ind->owe_ie = ie;
		assoc_ind->owe_ie_len = ie_len;
		assoc_ind->owe_status = owe_status;
		status = sme_update_owe_info(mac, assoc_ind);
		qdf_mem_free(assoc_ind);
	}

	return status;
}

bool wlansap_is_channel_present_in_acs_list(uint32_t freq,
					    uint32_t *ch_freq_list,
					    uint8_t ch_count)
{
	uint8_t i;

	for(i = 0; i < ch_count; i++)
		if (ch_freq_list[i] == freq)
			return true;

	return false;
}

QDF_STATUS wlansap_filter_ch_based_acs(struct sap_context *sap_ctx,
				       uint32_t *ch_freq_list,
				       uint32_t *ch_cnt)
{
	size_t ch_index;
	size_t target_ch_cnt = 0;

	if (!sap_ctx || !ch_freq_list || !ch_cnt ||
	    !sap_ctx->acs_cfg->master_freq_list ||
	    !sap_ctx->acs_cfg->master_ch_list_count) {
		sap_err("NULL parameters");
		return QDF_STATUS_E_FAULT;
	}

	for (ch_index = 0; ch_index < *ch_cnt; ch_index++) {
		if (wlansap_is_channel_present_in_acs_list(
					ch_freq_list[ch_index],
					sap_ctx->acs_cfg->master_freq_list,
					sap_ctx->acs_cfg->master_ch_list_count))
			ch_freq_list[target_ch_cnt++] = ch_freq_list[ch_index];
	}

	*ch_cnt = target_ch_cnt;

	return QDF_STATUS_SUCCESS;
}

bool wlansap_is_6ghz_included_in_acs_range(struct sap_context *sap_ctx)
{
	uint32_t i;
	uint32_t *ch_freq_list;

	if (!sap_ctx || !sap_ctx->acs_cfg ||
	    !sap_ctx->acs_cfg->master_freq_list ||
	    !sap_ctx->acs_cfg->master_ch_list_count) {
		sap_err("NULL parameters");
		return false;
	}
	ch_freq_list = sap_ctx->acs_cfg->master_freq_list;
	for (i = 0; i < sap_ctx->acs_cfg->master_ch_list_count; i++) {
		if (WLAN_REG_IS_6GHZ_CHAN_FREQ(ch_freq_list[i]))
			return true;
	}
	return false;
}

#if defined(FEATURE_WLAN_CH_AVOID)
/**
 * wlansap_get_safe_channel() - Get safe channel from current regulatory
 * @sap_ctx: Pointer to SAP context
 *
 * This function is used to get safe channel from current regulatory valid
 * channels to restart SAP if failed to get safe channel from PCL.
 *
 * Return: Chan freq num to restart SAP in case of success. In case of any
 * failure, the channel number returned is zero.
 */
static uint32_t
wlansap_get_safe_channel(struct sap_context *sap_ctx)
{
	struct mac_context *mac;
	uint32_t pcl_freqs[NUM_CHANNELS];
	QDF_STATUS status;
	mac_handle_t mac_handle;
	uint32_t pcl_len = 0, i;

	if (!sap_ctx) {
		sap_err("NULL parameter");
		return INVALID_CHANNEL_ID;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return INVALID_CHANNEL_ID;
	}
	mac_handle = MAC_HANDLE(mac);

	/* get the channel list for current domain */
	status = policy_mgr_get_valid_chans(mac->psoc, pcl_freqs, &pcl_len);
	if (QDF_IS_STATUS_ERROR(status)) {
		sap_err("Error in getting valid channels");
		return INVALID_CHANNEL_ID;
	}

	status = wlansap_filter_ch_based_acs(sap_ctx, pcl_freqs, &pcl_len);

	if (QDF_IS_STATUS_ERROR(status)) {
		sap_err("failed to filter ch from acs %d", status);
		return INVALID_CHANNEL_ID;
	}

	if (pcl_len) {
		status = policy_mgr_get_valid_chans_from_range(mac->psoc,
							       pcl_freqs,
							       &pcl_len,
							       PM_SAP_MODE);
		if (QDF_IS_STATUS_ERROR(status) || !pcl_len) {
			sap_err("failed to get valid channel: %d len %d",
				status, pcl_len);
			return INVALID_CHANNEL_ID;
		}

		for (i = 0; i < pcl_len; i++) {
			if (WLAN_REG_IS_SAME_BAND_FREQS(sap_ctx->chan_freq,
							pcl_freqs[i])) {
				sap_debug("select %d from valid channel list",
					  pcl_freqs[i]);
				return pcl_freqs[i];
			}
		}
		sap_debug("select first %d from valid channel list",
			  pcl_freqs[0]);
		return pcl_freqs[0];
	}

	return INVALID_CHANNEL_ID;
}
#else
/**
 * wlansap_get_safe_channel() - Get safe channel from current regulatory
 * @sap_ctx: Pointer to SAP context
 *
 * This function is used to get safe channel from current regulatory valid
 * channels to restart SAP if failed to get safe channel from PCL.
 *
 * Return: Channel number to restart SAP in case of success. In case of any
 * failure, the channel number returned is zero.
 */
static uint8_t
wlansap_get_safe_channel(struct sap_context *sap_ctx)
{
	return 0;
}
#endif

uint32_t
wlansap_get_safe_channel_from_pcl_and_acs_range(struct sap_context *sap_ctx)
{
	struct mac_context *mac;
	struct sir_pcl_list pcl = {0};
	uint32_t pcl_freqs[NUM_CHANNELS] = {0};
	QDF_STATUS status;
	mac_handle_t mac_handle;
	uint32_t pcl_len = 0;

	if (!sap_ctx) {
		sap_err("NULL parameter");
		return INVALID_CHANNEL_ID;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return INVALID_CHANNEL_ID;
	}
	mac_handle = MAC_HANDLE(mac);

	if (policy_mgr_get_connection_count(mac->psoc) == 1) {
		sap_debug("only SAP present return best channel from ACS list");
		return wlansap_get_safe_channel(sap_ctx);
	}

	status =
		policy_mgr_get_pcl_for_vdev_id(mac->psoc, PM_SAP_MODE,
					       pcl_freqs, &pcl_len,
					       pcl.weight_list,
					       QDF_ARRAY_SIZE(pcl.weight_list),
					       sap_ctx->sessionId);

	if (QDF_IS_STATUS_ERROR(status)) {
		sap_err("Get PCL failed");
		return INVALID_CHANNEL_ID;
	}

	if (pcl_len) {
		status = wlansap_filter_ch_based_acs(sap_ctx, pcl_freqs,
						     &pcl_len);
		if (QDF_IS_STATUS_ERROR(status)) {
			sap_err("failed to filter ch from acs %d", status);
			return INVALID_CHANNEL_ID;
		}

		if (pcl_len) {
			sap_debug("select %d from valid ch freq list",
				  pcl_freqs[0]);
			return pcl_freqs[0];
		}
		sap_debug("no safe channel from PCL found in ACS range");
	} else {
		sap_debug("pcl length is zero!");
	}

	/*
	 * In some scenarios, like hw dbs disabled, sap+sap case, if operating
	 * channel is unsafe channel, the pcl may be empty, instead of return,
	 * try to choose a safe channel from acs range.
	 */
	return wlansap_get_safe_channel(sap_ctx);
}

static uint32_t wlansap_get_2g_first_safe_chan_freq(struct sap_context *sap_ctx)
{
	uint32_t i;
	uint32_t freq;
	enum channel_state state;
	struct regulatory_channel *cur_chan_list;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc;
	uint32_t *acs_freq_list;
	uint8_t acs_list_count;

	pdev = sap_ctx->vdev->vdev_objmgr.wlan_pdev;
	psoc = pdev->pdev_objmgr.wlan_psoc;

	cur_chan_list = qdf_mem_malloc(NUM_CHANNELS *
			sizeof(struct regulatory_channel));
	if (!cur_chan_list)
		return TWOG_CHAN_6_IN_MHZ;

	if (wlan_reg_get_current_chan_list(pdev, cur_chan_list) !=
					   QDF_STATUS_SUCCESS) {
		freq = TWOG_CHAN_6_IN_MHZ;
		goto err;
	}

	acs_freq_list = sap_ctx->acs_cfg->master_freq_list;
	acs_list_count = sap_ctx->acs_cfg->master_ch_list_count;
	for (i = 0; i < NUM_CHANNELS; i++) {
		freq = cur_chan_list[i].center_freq;
		state = wlan_reg_get_channel_state_for_freq(pdev, freq);
		if (state != CHANNEL_STATE_DISABLE &&
		    state != CHANNEL_STATE_PASSIVE &&
		    state != CHANNEL_STATE_INVALID &&
		    wlan_reg_is_24ghz_ch_freq(freq) &&
		    policy_mgr_is_safe_channel(psoc, freq) &&
		    wlansap_is_channel_present_in_acs_list(freq,
							   acs_freq_list,
							   acs_list_count)) {
			sap_debug("found a 2g channel: %d", freq);
			goto err;
		}
	}

	freq = TWOG_CHAN_6_IN_MHZ;
err:
	qdf_mem_free(cur_chan_list);
	return freq;
}

uint32_t wlansap_get_safe_channel_from_pcl_for_sap(struct sap_context *sap_ctx)
{
	struct wlan_objmgr_pdev *pdev;
	struct mac_context *mac;
	struct sir_pcl_list pcl = {0};
	uint32_t pcl_freqs[NUM_CHANNELS] = {0};
	QDF_STATUS status;
	uint32_t pcl_len = 0;

	if (!sap_ctx) {
		sap_err("NULL parameter");
		return INVALID_CHANNEL_ID;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return INVALID_CHANNEL_ID;
	}

	pdev = sap_ctx->vdev->vdev_objmgr.wlan_pdev;
	if (!pdev) {
		sap_err("NULL pdev");
	}

	status = policy_mgr_get_pcl_for_vdev_id(mac->psoc, PM_SAP_MODE,
						pcl_freqs, &pcl_len,
						pcl.weight_list,
						QDF_ARRAY_SIZE(pcl.weight_list),
						sap_ctx->sessionId);

	if (QDF_IS_STATUS_ERROR(status)) {
		sap_err("Get PCL failed");
		return INVALID_CHANNEL_ID;
	}

	if (pcl_len) {
		status = policy_mgr_filter_passive_ch(pdev, pcl_freqs,
						      &pcl_len);
		if (QDF_IS_STATUS_ERROR(status)) {
			sap_err("failed to filter passive channels");
			return INVALID_CHANNEL_ID;
		}

		if (pcl_len) {
			sap_debug("select %d from valid ch freq list",
				  pcl_freqs[0]);
			return pcl_freqs[0];
		}
		sap_debug("no active channels found in PCL");
	} else {
		sap_debug("pcl length is zero!");
	}

	return wlansap_get_2g_first_safe_chan_freq(sap_ctx);
}

qdf_freq_t wlansap_get_chan_band_restrict(struct sap_context *sap_ctx,
					  enum sap_csa_reason_code *csa_reason)
{
	uint32_t restart_freq;
	enum phy_ch_width restart_ch_width;
	uint16_t intf_ch_freq;
	uint32_t phy_mode;
	struct mac_context *mac;
	uint8_t cc_mode;
	uint8_t vdev_id;
	enum reg_wifi_band sap_band;
	enum band_info band;

	if (!sap_ctx) {
		sap_err("sap_ctx NULL parameter");
		return 0;
	}

	if (!csa_reason) {
		sap_err("csa_reason is NULL");
		return 0;
	}

	if (cds_is_driver_recovering())
		return 0;

	mac = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac) {
		sap_err("Invalid MAC context");
		return 0;
	}

	if (ucfg_reg_get_band(mac->pdev, &band) != QDF_STATUS_SUCCESS) {
		sap_err("Failed to get current band config");
		return 0;
	}
	sap_band = wlan_reg_freq_to_band(sap_ctx->chan_freq);
	sap_debug("SAP/Go current band: %d, pdev band capability: %d",
		  sap_band, band);
	if (sap_band == REG_BAND_5G && band == BIT(REG_BAND_2G)) {
		sap_ctx->chan_freq_before_switch_band = sap_ctx->chan_freq;
		sap_ctx->chan_width_before_switch_band =
			sap_ctx->ch_params.ch_width;
		sap_debug("Save chan info before switch: %d, width: %d",
			  sap_ctx->chan_freq, sap_ctx->ch_params.ch_width);
		restart_freq = wlansap_get_2g_first_safe_chan_freq(sap_ctx);
		if (restart_freq == 0) {
			sap_debug("use default chan 6");
			restart_freq = TWOG_CHAN_6_IN_MHZ;
		}
		restart_ch_width = sap_ctx->ch_params.ch_width;
		if (restart_ch_width > CH_WIDTH_40MHZ) {
			sap_debug("set 40M when switch SAP to 2G");
			restart_ch_width = CH_WIDTH_40MHZ;
		}
	} else if (sap_band == REG_BAND_2G && (band & BIT(REG_BAND_5G)) &&
		   sap_ctx->chan_freq_before_switch_band) {
		restart_freq = sap_ctx->chan_freq_before_switch_band;
		restart_ch_width = sap_ctx->chan_width_before_switch_band;
		sap_debug("Restore chan freq: %d, width: %d",
			  restart_freq, restart_ch_width);
		*csa_reason = CSA_REASON_BAND_RESTRICTED;
	} else if (wlan_reg_is_disable_for_freq(mac->pdev,
						sap_ctx->chan_freq)) {
		sap_debug("channel is disabled");
		*csa_reason = CSA_REASON_CHAN_DISABLED;
		return wlansap_get_safe_channel_from_pcl_and_acs_range(sap_ctx);
	} else if (wlan_reg_is_passive_for_freq(mac->pdev,
						sap_ctx->chan_freq)) {
		sap_ctx->chan_freq_before_switch_band = sap_ctx->chan_freq;
		sap_ctx->chan_width_before_switch_band =
			sap_ctx->ch_params.ch_width;
		sap_debug("Save chan info before switch: %d, width: %d",
			  sap_ctx->chan_freq, sap_ctx->ch_params.ch_width);
		sap_debug("channel is passive");
		*csa_reason = CSA_REASON_CHAN_PASSIVE;
		return wlansap_get_safe_channel_from_pcl_for_sap(sap_ctx);
	} else {
		sap_debug("No need switch SAP/Go channel");
		return 0;
	}

	cc_mode = sap_ctx->cc_switch_mode;
	phy_mode = sap_ctx->csr_roamProfile.phyMode;
	intf_ch_freq = sme_check_concurrent_channel_overlap(
						       MAC_HANDLE(mac),
						       restart_freq,
						       phy_mode,
						       cc_mode);
	if (intf_ch_freq)
		restart_freq = intf_ch_freq;
	vdev_id = sap_ctx->vdev->vdev_objmgr.vdev_id;
	sap_debug("vdev: %d, CSA target freq: %d", vdev_id, restart_freq);

	return restart_freq;
}

#ifdef DCS_INTERFERENCE_DETECTION
QDF_STATUS wlansap_dcs_set_vdev_wlan_interference_mitigation(
				struct sap_context *sap_context,
				bool wlan_interference_mitigation_enable)
{
	struct mac_context *mac;

	if (!sap_context) {
		sap_err("Invalid SAP context pointer");
		return QDF_STATUS_E_FAULT;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}

	mac->sap.dcs_info.
		wlan_interference_mitigation_enable[sap_context->sessionId] =
					wlan_interference_mitigation_enable;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_dcs_set_wlan_interference_mitigation_on_band(
					struct sap_context *sap_context,
					struct sap_config *sap_cfg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	bool wlan_interference_mitigation_enable = false;

	if (WLAN_REG_IS_5GHZ_CH_FREQ(sap_cfg->acs_cfg.pri_ch_freq))
		wlan_interference_mitigation_enable = true;

	status = wlansap_dcs_set_vdev_wlan_interference_mitigation(
					sap_context,
					wlan_interference_mitigation_enable);
	return status;
}

QDF_STATUS wlansap_dcs_set_vdev_starting(struct sap_context *sap_context,
					 bool vdev_starting)
{
	struct mac_context *mac;

	if (!sap_context) {
		sap_err("Invalid SAP context pointer");
		return QDF_STATUS_E_FAULT;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}

	mac->sap.dcs_info.is_vdev_starting[sap_context->sessionId] =
							vdev_starting;

	return QDF_STATUS_SUCCESS;
}

bool wlansap_dcs_is_wlan_interference_mitigation_enabled(
					struct sap_context *sap_context)
{
	struct mac_context *mac;

	if (!sap_context) {
		sap_err("Invalid SAP context pointer");
		return false;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return false;
	}

	return mac->sap.dcs_info.
		wlan_interference_mitigation_enable[sap_context->sessionId];
}

qdf_freq_t wlansap_dcs_get_freq(struct sap_context *sap_context)
{
	if (!sap_context) {
		sap_err("Invalid SAP context pointer");
		return false;
	}

	return sap_context->dcs_ch_freq;
}

void wlansap_dump_acs_ch_freq(struct sap_context *sap_context)
{
	if (!sap_context) {
		sap_err("Invalid sap_debug");
		return;
	}

	if (sap_context->fsm_state == SAP_STARTED)
		sap_info("ACS dump DCS freq=%d", sap_context->dcs_ch_freq);
	else
		sap_info("ACS dump ch_freq=%d", sap_context->chan_freq);
}

void wlansap_set_acs_ch_freq(struct sap_context *sap_context,
			     qdf_freq_t ch_freq)
{
	if (!sap_context) {
		sap_err("Invalid sap_debug");
		return;
	}

	if (sap_context->fsm_state == SAP_STARTED) {
		sap_context->dcs_ch_freq = ch_freq;
		sap_debug("ACS configuring dcs_ch_freq=%d",
			  sap_context->dcs_ch_freq);
	} else {
		sap_context->chan_freq = ch_freq;
		sap_debug("ACS configuring ch_freq=%d",
			  sap_context->chan_freq);
	}
}
#else
void wlansap_dump_acs_ch_freq(struct sap_context *sap_context)
{
	if (!sap_context) {
		sap_err("Invalid sap_debug");
		return;
	}

	sap_info("ACS dump ch_freq=%d", sap_context->chan_freq);
}

void wlansap_set_acs_ch_freq(struct sap_context *sap_context,
			     qdf_freq_t ch_freq)
{
	if (!sap_context) {
		sap_err("Invalid sap_debug");
		return;
	}

	sap_context->chan_freq = ch_freq;
	sap_debug("ACS configuring ch_freq=%d", sap_context->chan_freq);
}
#endif
