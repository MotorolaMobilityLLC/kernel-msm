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

#ifndef WLAN_QCT_WLANSAP_INTERNAL_H
#define WLAN_QCT_WLANSAP_INTERNAL_H

/*
 * This file contains the internal API exposed by the wlan SAP PAL layer
 * module.
 */

#include "cds_api.h"
#include "cds_packet.h"

/* Pick up the CSR API definitions */
#include "csr_api.h"
#include "sap_api.h"
#include "sap_fsm_ext.h"
#include "sap_ch_select.h"
#include <wlan_scan_public_structs.h>
#include <wlan_objmgr_pdev_obj.h>
#include "wlan_vdev_mlme_main.h"
#include "wlan_vdev_mlme_api.h"

/* DFS Non Occupancy Period =30 minutes, in microseconds */
#define SAP_DFS_NON_OCCUPANCY_PERIOD      (30 * 60 * 1000 * 1000)

#define SAP_DEBUG

#define IS_ETSI_WEATHER_CH(_ch)   ((_ch >= 120) && (_ch <= 130))
#define IS_CH_BONDING_WITH_WEATHER_CH(_ch)   (_ch == 116)
#define IS_CHAN_JAPAN_INDOOR(_ch) ((_ch >= 36)  && (_ch <= 64))
#define IS_CHAN_JAPAN_OUTDOOR(_ch)((_ch >= 100) && (_ch <= 140))
#define DEFAULT_CAC_TIMEOUT (60 * 1000) /* msecs - 1 min */
#define ETSI_WEATHER_CH_CAC_TIMEOUT (10 * 60 * 1000)    /* msecs - 10 min */
#define SAP_CHAN_PREFERRED_INDOOR  1
#define SAP_CHAN_PREFERRED_OUTDOOR 2

/*SAP Specific logging*/

#define sap_alert(params...) QDF_TRACE_FATAL(QDF_MODULE_ID_SAP, params)
#define sap_err(params...) QDF_TRACE_ERROR(QDF_MODULE_ID_SAP, params)
#define sap_warn(params...) QDF_TRACE_WARN(QDF_MODULE_ID_SAP, params)
#define sap_info(params...) QDF_TRACE_INFO(QDF_MODULE_ID_SAP, params)
#define sap_debug(params...) QDF_TRACE_DEBUG(QDF_MODULE_ID_SAP, params)

#define sap_nofl_alert(params...) \
	QDF_TRACE_FATAL_NO_FL(QDF_MODULE_ID_SAP, params)
#define sap_nofl_err(params...) \
	QDF_TRACE_ERROR_NO_FL(QDF_MODULE_ID_SAP, params)
#define sap_nofl_warn(params...) \
	QDF_TRACE_WARN_NO_FL(QDF_MODULE_ID_SAP, params)
#define sap_nofl_info(params...) \
	QDF_TRACE_INFO_NO_FL(QDF_MODULE_ID_SAP, params)
#define sap_nofl_debug(params...) \
	QDF_TRACE_DEBUG_NO_FL(QDF_MODULE_ID_SAP, params)

#define sap_alert_rl(params...) QDF_TRACE_FATAL_RL(QDF_MODULE_ID_SAP, params)
#define sap_err_rl(params...) QDF_TRACE_ERROR_RL(QDF_MODULE_ID_SAP, params)
#define sap_warn_rl(params...) QDF_TRACE_WARN_RL(QDF_MODULE_ID_SAP, params)
#define sap_info_rl(params...) QDF_TRACE_INFO_RL(QDF_MODULE_ID_SAP, params)
#define sap_debug_rl(params...) QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_SAP, params)

/*----------------------------------------------------------------------------
 *  Typedefs
 * -------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------
 *  Type Declarations - For internal SAP context information
 * -------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------
 *  Opaque SAP context Type Declaration
 * -------------------------------------------------------------------------*/
/* We were only using this syntax, when this was truly opaque. */
/* (I.E., it was defined in a different file.) */

/**
 * enum sap_fsm_state - SAP FSM states for Access Point role
 * @SAP_INIT: init state
 * @SAP_STARTING: starting phase
 * @SAP_STARTED: up and running
 * @SAP_STOPPING: about to stop and transitions to init
 */
enum sap_fsm_state {
	SAP_INIT,
	SAP_STARTING,
	SAP_STARTED,
	SAP_STOPPING
};

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
/*
 * In a setup having two MDM both operating in AP+AP MCC scenario
 * if both the AP decides to use same or close channel set, CTS to
 * self, mechanism is causing issues with connectivity. For this, its
 * proposed that 2nd MDM devices which comes up later should detect
 * presence of first MDM device via special Q2Q IE present in becon
 * and avoid those channels mentioned in IE.
 *
 * Following struct will keep this info in sapCtx struct, and will be used
 * to avoid such channels in Random Channel Select in case of radar ind.
 */
struct sap_avoid_channels_info {
	bool       present;
	uint8_t    channels[CFG_VALID_CHANNEL_LIST_LEN];
};
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

struct sap_context {

	/* Include the current channel frequency of AP */
	uint32_t chan_freq;
	uint32_t sec_ch_freq;

#ifdef DCS_INTERFERENCE_DETECTION
	qdf_freq_t dcs_ch_freq;
#endif

	/* Include the SME(CSR) sessionId here */
	uint8_t sessionId;

	/* vdev object corresponding to sessionId */
	struct wlan_objmgr_vdev *vdev;

	/* Include the associations MAC addresses */
	uint8_t self_mac_addr[CDS_MAC_ADDRESS_LEN];

	/* Include the SME(CSR) context here */
	struct csr_roam_profile csr_roamProfile;
	uint32_t csr_roamId;

	/* SAP event Callback to hdd */
	sap_event_cb sap_event_cb;

	/*
	 * Include the state machine structure here, state var that keeps
	 * track of state machine
	 */
	enum sap_fsm_state fsm_state;
	enum sap_csa_reason_code csa_reason;

	/* Actual storage for AP and self (STA) SSID */
	tCsrSSIDInfo SSIDList[2];

	/* Actual storage for AP bssid */
	struct qdf_mac_addr bssid;

	/* Mac filtering settings */
	eSapMacAddrACL eSapMacAddrAclMode;
	struct qdf_mac_addr acceptMacList[MAX_ACL_MAC_ADDRESS];
	uint8_t nAcceptMac;
	struct qdf_mac_addr denyMacList[MAX_ACL_MAC_ADDRESS];
	uint8_t nDenyMac;

	void *user_context;

	uint32_t nStaWPARSnReqIeLength;
	uint8_t pStaWpaRsnReqIE[MAX_ASSOC_IND_IE_LEN];

	uint32_t *freq_list;
	uint8_t num_of_channel;
	uint16_t ch_width_orig;
	struct ch_params ch_params;
	uint32_t chan_freq_before_switch_band;
	enum phy_ch_width chan_width_before_switch_band;
	uint32_t auto_channel_select_weight;
	bool enableOverLapCh;
	struct sap_acs_cfg *acs_cfg;

	qdf_time_t acs_req_timestamp;

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	uint8_t cc_switch_mode;
#endif

#if defined(FEATURE_WLAN_STA_AP_MODE_DFS_DISABLE)
	bool dfs_ch_disable;
#endif
	bool isCacEndNotified;
	bool isCacStartNotified;
	bool is_sap_ready_for_chnl_chng;

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
	/*
	 * In a setup having two MDM both operating in AP+AP MCC scenario
	 * if both the AP decides to use same or close channel set, CTS to
	 * self, mechanism is causing issues with connectivity. For this, its
	 * proposed that 2nd MDM devices which comes up later should detect
	 * presence of first MDM device via special Q2Q IE present in becon
	 * and avoid those channels mentioned in IE.
	 *
	 * this struct contains the list of channels on which another MDM AP
	 * in MCC mode were detected.
	 */
	struct sap_avoid_channels_info sap_detected_avoid_ch_ie;
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
	/*
	 * sap_state, sap_status are created
	 * to inform upper layers about ACS scan status.
	 * Don't use these members for anyother purposes.
	 */
	eSapHddEvent sap_state;
	eSapStatus sap_status;
	uint32_t roc_ind_scan_id;
	bool is_pre_cac_on;
	bool pre_cac_complete;
	bool vendor_acs_dfs_lte_enabled;
	uint8_t dfs_vendor_channel;
	uint8_t dfs_vendor_chan_bw;
	qdf_freq_t freq_before_pre_cac;
	uint16_t beacon_tx_rate;
	enum sap_acs_dfs_mode dfs_mode;
	wlan_scan_requester req_id;
	uint8_t sap_sta_id;
	bool dfs_cac_offload;
	bool is_chan_change_inprogress;
	qdf_list_t owe_pending_assoc_ind_list;
	uint32_t freq_before_ch_switch;
};

/*----------------------------------------------------------------------------
 *  External declarations for global context
 * -------------------------------------------------------------------------*/

/**
 * struct sap_sm_event - SAP state machine event definition
 * @params: A VOID pointer type for all possible inputs
 * @event: State machine input event message
 * @u1: Introduced to handle csr_roam_complete_cb roamStatus
 * @u2: Introduced to handle csr_roam_complete_cb roamResult
 */
struct sap_sm_event {
	void *params;
	uint32_t event;
	uint32_t u1;
	uint32_t u2;
};

/*----------------------------------------------------------------------------
 * Function Declarations and Documentation
 * -------------------------------------------------------------------------*/

/**
 * sap_get_mac_context() - Get a pointer to the global MAC context
 *
 * Return: pointer to the global MAC context, or NULL if the MAC
 *         context is no longer registered
 */
static inline struct mac_context *sap_get_mac_context(void)
{
	return cds_get_context(QDF_MODULE_ID_PE);
}

QDF_STATUS wlansap_context_get(struct sap_context *ctx);
void wlansap_context_put(struct sap_context *ctx);

/**
 * wlansap_pre_start_bss_acs_scan_callback() - callback for scan results
 * @mac_handle:    the mac_handle passed in with the scan request
 * @sap_ctx:       the SAP context pointer.
 * @scanid:        scan id passed
 * @sessionid:     session identifier
 * @scan_status:        status of scan -success, failure or abort
 *
 * Api for scan callback. This function is invoked as a result of scan
 * completion and reports the scan results.
 *
 * Return: The QDF_STATUS code associated with performing the operation
 */
QDF_STATUS wlansap_pre_start_bss_acs_scan_callback(mac_handle_t mac_handle,
						   struct sap_context *sap_ctx,
						   uint8_t sessionid,
						   uint32_t scanid,
						   eCsrScanStatus scan_status);

/**
 * sap_select_channel() - select SAP channel
 * @mac_handle: Opaque handle to the global MAC context
 * @sap_ctx: Sap context
 * @scan_list: scan entry list
 *
 * Runs a algorithm to select the best channel to operate in based on BSS
 * rssi and bss count on each channel
 *
 * Returns: channel frequency if success, 0 otherwise
 */
uint32_t sap_select_channel(mac_handle_t mac_handle, struct sap_context *sap_ctx,
			   qdf_list_t *scan_list);

QDF_STATUS
sap_signal_hdd_event(struct sap_context *sap_ctx,
		  struct csr_roam_info *pCsrRoamInfo,
		  eSapHddEvent sapHddevent, void *);

QDF_STATUS sap_fsm(struct sap_context *sap_ctx, struct sap_sm_event *sap_event);

eSapStatus
sapconvert_to_csr_profile(struct sap_config *config,
			  eCsrRoamBssType bssType,
			  struct csr_roam_profile *profile);

void sap_free_roam_profile(struct csr_roam_profile *profile);

QDF_STATUS
sap_is_peer_mac_allowed(struct sap_context *sap_ctx, uint8_t *peerMac);

void
sap_sort_mac_list(struct qdf_mac_addr *macList, uint8_t size);

void
sap_add_mac_to_acl(struct qdf_mac_addr *macList, uint8_t *size,
	       uint8_t *peerMac);

void
sap_remove_mac_from_acl(struct qdf_mac_addr *macList, uint8_t *size,
		    uint8_t index);

void
sap_print_acl(struct qdf_mac_addr *macList, uint8_t size);

bool
sap_search_mac_list(struct qdf_mac_addr *macList, uint8_t num_mac,
		 uint8_t *peerMac, uint8_t *index);

QDF_STATUS sap_init_dfs_channel_nol_list(struct sap_context *sap_ctx);

bool sap_dfs_is_channel_in_nol_list(struct sap_context *sap_ctx,
				    uint8_t channelNumber,
				    ePhyChanBondState chanBondState);
void sap_dfs_cac_timer_callback(void *data);

/**
 * sap_cac_reset_notify() - BSS cleanup notification handler
 * @mac_handle: Opaque handle to the global MAC context
 *
 * This function should be called upon stop bss indication to clean up
 * DFS global structure.
 */
void sap_cac_reset_notify(mac_handle_t mac_handle);

bool is_concurrent_sap_ready_for_channel_change(mac_handle_t mac_handle,
						struct sap_context *sap_ctx);

bool sap_is_conc_sap_doing_scc_dfs(mac_handle_t mac_handle,
				   struct sap_context *given_sapctx);

uint8_t sap_get_total_number_sap_intf(mac_handle_t mac_handle);

/**
 * sap_channel_sel - Function for initiating scan request for ACS
 * @sap_context: Sap Context value.
 *
 * Initiates Scan for ACS to pick a channel.
 *
 * Return: The QDF_STATUS code associated with performing the operation.
 */
QDF_STATUS sap_channel_sel(struct sap_context *sap_ctx);

/**
 * sap_validate_chan - Function validate the channel and forces SCC
 * @sap_context: Sap Context value.
 * @pre_start_bss: if its called pre start BSS with valid channel.
 * @check_for_connection_update: true, check and wait for connection update
 *				 false, do not perform connection update
 *
 * validate and update the channel in case of force SCC.
 *
 * Return: The QDF_STATUS code associated with performing the operation.
 */
QDF_STATUS
sap_validate_chan(struct sap_context *sap_context,
		  bool pre_start_bss,
		  bool check_for_connection_update);

/**
 * sap_check_in_avoid_ch_list() - checks if given channel present is channel
 * avoidance list
 * avoid_channels_info struct
 * @sap_ctx:        sap context.
 * @channel:        channel to be checked in sap_ctx's avoid ch list
 *
 * sap_ctx contains sap_avoid_ch_info strcut containing the list of channels on
 * which MDM device's AP with MCC was detected. This function checks if given
 * channel is present in that list.
 *
 * Return: true, if channel was present, false othersie.
 */
bool
sap_check_in_avoid_ch_list(struct sap_context *sap_ctx, uint8_t channel);

/**
 * sap_set_session_param() - set sap related param to sap context and global var
 * @mac_handle: Opaque handle to the global MAC context
 * @sapctx: pointer to sapctx
 * @session_id: session id for sap
 *
 * This API will set appropriate softap parameters to sap context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sap_set_session_param(mac_handle_t mac_handle,
				 struct sap_context *sapctx,
				 uint32_t session_id);

/**
 * sap_clear_session_param() - clear sap related param from sap context
 * @mac_handle: Opaque handle to the global MAC context
 * @sapctx: pointer to sapctx
 * @session_id: session id for sap
 *
 * This API will clear appropriate softap parameters from sap context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sap_clear_session_param(mac_handle_t mac_handle,
				   struct sap_context *sapctx,
				   uint32_t session_id);

void sap_scan_event_callback(struct wlan_objmgr_vdev *vdev,
			struct scan_event *event, void *arg);

#ifdef DFS_COMPONENT_ENABLE
/**
 * sap_indicate_radar() - Process radar indication
 * @sap_ctx: pointer to sap context
 *
 * process radar indication.
 *
 * Return: frequency to which sap wishes to switch.
 */
qdf_freq_t sap_indicate_radar(struct sap_context *sap_ctx);
#else
static inline qdf_freq_t sap_indicate_radar(struct sap_context *sap_ctx)
{
	return 0;
}
#endif

/**
 * sap_select_default_oper_chan() - Select AP mode default operating channel
 * @mac_ctx: mac context
 * @acs_cfg: pointer to ACS config info
 *
 * Select AP mode default operating channel based on ACS hw mode and channel
 * range configuration when ACS scan fails due to some reasons, such as scan
 * timeout, etc.
 *
 * Return: Selected operating channel frequency
 */
uint32_t sap_select_default_oper_chan(struct mac_context *mac_ctx,
				      struct sap_acs_cfg *acs_cfg);

/*
 * sap_is_dfs_cac_wait_state() - check if sap is in cac wait state
 * @sap_ctx: sap context to check
 *
 * Return: true if sap is in cac wait state
 */
bool sap_is_dfs_cac_wait_state(struct sap_context *sap_ctx);

/**
 * sap_chan_bond_dfs_sub_chan - check bonded channel includes dfs sub chan
 * @sap_context: Handle to SAP context.
 * @channel_number: chan whose bonded chan will be checked
 * @bond_state: The channel bonding mode of the passed channel.
 *
 * This function checks if a given bonded channel includes dfs sub chan.
 *
 * Return: true if at least one dfs sub chan is bonded, otherwise false
 */
bool
sap_chan_bond_dfs_sub_chan(struct sap_context *sap_context,
			   uint8_t channel_number,
			   ePhyChanBondState bond_state);
#endif
