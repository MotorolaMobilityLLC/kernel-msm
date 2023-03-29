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
 * This file lim_api.h contains the definitions exported by
 * LIM module.
 * Author:        Chandra Modumudi
 * Date:          02/11/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#ifndef __LIM_API_H
#define __LIM_API_H
#include "wni_api.h"
#include "sir_api.h"
#include "ani_global.h"
#include "sir_mac_prot_def.h"
#include "sir_common.h"
#include "sir_debug.h"
#include "sch_global.h"
#include "utils_api.h"
#include "lim_global.h"
#include "wma_if.h"
#include "wma_types.h"
#include "scheduler_api.h"

/* Macro to count heartbeat */
#define limResetHBPktCount(pe_session)   (pe_session->LimRxedBeaconCntDuringHB = 0)

/* Useful macros for fetching various states in mac->lim */
/* gLimSystemRole */
#define GET_LIM_SYSTEM_ROLE(pe_session)      (pe_session->limSystemRole)
#define LIM_IS_AP_ROLE(pe_session)           (GET_LIM_SYSTEM_ROLE(pe_session) == eLIM_AP_ROLE)
#define LIM_IS_STA_ROLE(pe_session)          (GET_LIM_SYSTEM_ROLE(pe_session) == eLIM_STA_ROLE)
#define LIM_IS_UNKNOWN_ROLE(pe_session)      (GET_LIM_SYSTEM_ROLE(pe_session) == eLIM_UNKNOWN_ROLE)
#define LIM_IS_P2P_DEVICE_ROLE(pe_session)   (GET_LIM_SYSTEM_ROLE(pe_session) == eLIM_P2P_DEVICE_ROLE)
#define LIM_IS_P2P_DEVICE_GO(pe_session)     (GET_LIM_SYSTEM_ROLE(pe_session) == eLIM_P2P_DEVICE_GO)
#define LIM_IS_NDI_ROLE(pe_session) \
		(GET_LIM_SYSTEM_ROLE(pe_session) == eLIM_NDI_ROLE)
/* gLimSmeState */
#define GET_LIM_SME_STATE(mac)                 (mac->lim.gLimSmeState)
#define SET_LIM_SME_STATE(mac, state)          (mac->lim.gLimSmeState = state)
/* gLimMlmState */
#define GET_LIM_MLM_STATE(mac)                 (mac->lim.gLimMlmState)
#define SET_LIM_MLM_STATE(mac, state)          (mac->lim.gLimMlmState = state)
/*tpdphHashNode mlmStaContext*/
#define GET_LIM_STA_CONTEXT_MLM_STATE(sta)   (sta->mlmStaContext.mlmState)
#define SET_LIM_STA_CONTEXT_MLM_STATE(sta, state)  (sta->mlmStaContext.mlmState = state)
#define LIM_IS_CONNECTION_ACTIVE(pe_session)  (pe_session->LimRxedBeaconCntDuringHB)
/*mac->lim.gLimProcessDefdMsgs*/
#define GET_LIM_PROCESS_DEFD_MESGS(mac) (mac->lim.gLimProcessDefdMsgs)

/**
 * lim_post_msg_api() - post normal priority PE message
 * @mac: mac context
 * @msg: message to be posted
 *
 * This function is called to post a message to the tail of the PE
 * message queue to be processed in the MC Thread with normal
 * priority.
 *
 * Return: QDF_STATUS_SUCCESS on success, other QDF_STATUS on error
 */
QDF_STATUS lim_post_msg_api(struct mac_context *mac, struct scheduler_msg *msg);

static inline void
lim_post_msg_to_process_deferred_queue(struct mac_context *mac)
{
	struct scheduler_msg msg = {0};
	QDF_STATUS status;

	if (!mac->lim.gLimProcessDefdMsgs || !mac->lim.gLimDeferredMsgQ.size)
		return;

	msg.type = SIR_LIM_PROCESS_DEFERRED_QUEUE;
	msg.bodyptr = NULL;
	msg.bodyval = 0;

	status = lim_post_msg_api(mac, &msg);
	if (QDF_IS_STATUS_ERROR(status))
		pe_err("Failed to post lim msg:0x%x", msg.type);
}

#define SET_LIM_PROCESS_DEFD_MESGS(mac, val) \
	mac->lim.gLimProcessDefdMsgs = val; \
	pe_debug("Defer LIM msg %d", val); \
	lim_post_msg_to_process_deferred_queue(mac);

/* LIM exported function templates */
#define LIM_MIN_BCN_PR_LENGTH  12
#define LIM_BCN_PR_CAPABILITY_OFFSET 10
#define LIM_ASSOC_REQ_IE_OFFSET 4

/**
 * enum lim_vendor_ie_access_policy - vendor ie access policy
 * @LIM_ACCESS_POLICY_NONE: access policy not valid
 * @LIM_ACCESS_POLICY_RESPOND_IF_IE_IS_PRESENT: respond only if vendor ie
 *         is present in probe request and assoc request frames
 * @LIM_ACCESS_POLICY_DONOT_RESPOND_IF_IE_IS_PRESENT: do not respond if vendor
 *         ie is present in probe request or assoc request frames
 */
enum lim_vendor_ie_access_policy {
	LIM_ACCESS_POLICY_NONE,
	LIM_ACCESS_POLICY_RESPOND_IF_IE_IS_PRESENT,
	LIM_ACCESS_POLICY_DONOT_RESPOND_IF_IE_IS_PRESENT,
};

typedef enum eMgmtFrmDropReason {
	eMGMT_DROP_NO_DROP,
	eMGMT_DROP_NOT_LAST_IBSS_BCN,
	eMGMT_DROP_INFRA_BCN_IN_IBSS,
	eMGMT_DROP_SCAN_MODE_FRAME,
	eMGMT_DROP_NON_SCAN_MODE_FRAME,
	eMGMT_DROP_INVALID_SIZE,
	eMGMT_DROP_SPURIOUS_FRAME,
	eMGMT_DROP_DUPLICATE_AUTH_FRAME,
	eMGMT_DROP_EXCESSIVE_MGMT_FRAME,
} tMgmtFrmDropReason;

/**
 * Function to initialize LIM state machines.
 * This called upon LIM thread creation.
 */
QDF_STATUS lim_initialize(struct mac_context *);
QDF_STATUS pe_open(struct mac_context *mac, struct cds_config_info *cds_cfg);
QDF_STATUS pe_close(struct mac_context *mac);
QDF_STATUS lim_start(struct mac_context *mac);
QDF_STATUS pe_start(struct mac_context *mac);
void pe_stop(struct mac_context *mac);

#ifdef WLAN_FEATURE_11W
/**
 * lim_stop_pmfcomeback_timer() - stop pmf comeback timer
 * @session: Pointer to PE session
 *
 * Return: None
 */
void lim_stop_pmfcomeback_timer(struct pe_session *session);
#else
static inline void lim_stop_pmfcomeback_timer(struct pe_session *session)
{
}
#endif

/**
 * pe_register_mgmt_rx_frm_callback() - registers callback for receiving
 *                                      mgmt rx frames
 * @mac_ctx: mac global ctx
 *
 * This function registers a PE function to mgmt txrx component and a WMA
 * function to WMI layer as event handler for receiving mgmt frames.
 *
 * Return: None
 */
void pe_register_mgmt_rx_frm_callback(struct mac_context *mac_ctx);

/**
 * pe_deregister_mgmt_rx_frm_callback() - degisters callback for receiving
 *                                        mgmt rx frames
 * @mac_ctx: mac global ctx
 *
 * This function deregisters the PE function registered to mgmt txrx component
 * and the WMA function registered to WMI layer as event handler for receiving
 * mgmt frames.
 *
 * Return: None
 */
void pe_deregister_mgmt_rx_frm_callback(struct mac_context *mac_ctx);

/**
 * pe_register_callbacks_with_wma() - register SME and PE callback functions to
 * WMA.
 * @mac: mac global ctx
 * @ready_req: Ready request parameters, containing callback pointers
 *
 * Return: None
 */
void pe_register_callbacks_with_wma(struct mac_context *mac,
				    struct sme_ready_req *ready_req);

/**
 * Function to cleanup LIM state.
 * This called upon reset/persona change etc
 */
void lim_cleanup(struct mac_context *);

/**
 * lim_post_msg_high_priority() - post high priority PE message
 * @mac: mac context
 * @msg: message to be posted
 *
 * This function is called to post a message to the head of the PE
 * message queue to be processed in the MC Thread with expedited
 * priority.
 *
 * Return: QDF_STATUS_SUCCESS on success, other QDF_STATUS on error
 */
QDF_STATUS lim_post_msg_high_priority(struct mac_context *mac,
				      struct scheduler_msg *msg);

/**
 * Function to process messages posted to LIM thread
 * and dispatch to various sub modules within LIM module.
 */
void lim_message_processor(struct mac_context *, struct scheduler_msg *);

/* / Function used by other Sirius modules to read global SME state */
static inline tLimSmeStates lim_get_sme_state(struct mac_context *mac)
{
	return mac->lim.gLimSmeState;
}

/**
 * lim_received_hb_handler() - This function is called by
 * sch_beacon_process() upon receiving a Beacon on STA. This
 * also gets called upon receiving Probe Response after heat
 * beat failure is detected.
 *
 * @mac - global mac structure
 * @chan_freq - channel frequency indicated in Beacon, Probe
 *
 * Response return - none
 */
void lim_received_hb_handler(struct mac_context *, uint32_t,
			     struct pe_session *);

/* / Function that triggers STA context deletion */
void lim_trigger_sta_deletion(struct mac_context *mac, tpDphHashNode sta,
			      struct pe_session *pe_session);

#ifdef FEATURE_WLAN_TDLS
/* Function that sends TDLS Del Sta indication to SME */
void lim_send_sme_tdls_del_sta_ind(struct mac_context *mac, tpDphHashNode sta,
				   struct pe_session *pe_session,
				   uint16_t reasonCode);

/**
 * lim_set_tdls_flags() - update tdls flags based on newer STA connection
 * information
 * @roam_sync_ind_ptr: pointer to roam offload structure
 * @ft_session_ptr: pointer to PE session
 *
 * Set TDLS flags as per new STA connection capabilities.
 *
 * Return: None
 */
void lim_set_tdls_flags(struct roam_offload_synch_ind *roam_sync_ind_ptr,
			struct pe_session *ft_session_ptr);
#else
static inline
void lim_set_tdls_flags(struct roam_offload_synch_ind *roam_sync_ind_ptr,
			struct pe_session *ft_session_ptr)
{
}
#endif

/* / Function that checks for change in AP's capabilties on STA */
void lim_detect_change_in_ap_capabilities(struct mac_context *,
					  tpSirProbeRespBeacon,
					  struct pe_session *);

QDF_STATUS lim_update_short_slot(struct mac_context *mac,
				    tpSirProbeRespBeacon pBeacon,
				    tpUpdateBeaconParams pBeaconParams,
				    struct pe_session *);

/**
 * lim_ps_offload_handle_missed_beacon_ind() - handle missed beacon indication
 * @mac: global mac context
 * @msg: message
 *
 * This function process the SIR_HAL_MISSED_BEACON_IND
 * message from HAL, to do active AP probing.
 *
 * Return: void
 */
void lim_ps_offload_handle_missed_beacon_ind(struct mac_context *mac,
					     struct scheduler_msg *msg);

void lim_send_heart_beat_timeout_ind(struct mac_context *mac, struct pe_session *pe_session);
tMgmtFrmDropReason lim_is_pkt_candidate_for_drop(struct mac_context *mac,
						 uint8_t *pRxPacketInfo,
						 uint32_t subType);
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * pe_roam_synch_callback() - Callback registered at wma, gets invoked when
 * ROAM SYNCH event is received from firmware
 * @mac_ctx: global mac context
 * @roam_sync_ind_ptr: Structure with roam synch parameters
 * @bss_desc_ptr: bss_description pointer for new bss to which the firmware has
 * started roaming
 * @reason: Operation to be done by the callback
 *
 * This is a PE level callback called from WMA to complete the roam synch
 * propagation at PE level and also fill the BSS descriptor which will be
 * helpful further to complete the roam synch propagation.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
pe_roam_synch_callback(struct mac_context *mac_ctx,
		       struct roam_offload_synch_ind *roam_sync_ind_ptr,
		       struct bss_description *bss_desc_ptr,
		       enum sir_roam_op_code reason);

void
lim_check_ft_initial_im_association(struct roam_offload_synch_ind *roam_synch,
				    struct pe_session *session_entry);

/**
 * pe_disconnect_callback() - Callback to handle deauth event is received
 * from firmware
 * @mac: pointer to global mac context
 * @vdev_id: VDEV in which the event was received
 * @deauth_disassoc_frame: Deauth/disassoc frame received from firmware
 * @deauth_disassoc_frame_len: Length of @deauth_disassoc_frame
 * @reason_code: Fw sent reason code if disassoc/deauth frame is not
 * available
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
pe_disconnect_callback(struct mac_context *mac, uint8_t vdev_id,
		       uint8_t *deauth_disassoc_frame,
		       uint16_t deauth_disassoc_frame_len,
		       uint16_t reason_code);

#else
static inline QDF_STATUS
pe_roam_synch_callback(struct mac_context *mac_ctx,
		       struct roam_offload_synch_ind *roam_sync_ind_ptr,
		       struct bss_description *bss_desc_ptr,
		       enum sir_roam_op_code reason)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
pe_disconnect_callback(struct mac_context *mac, uint8_t vdev_id,
		       uint8_t *deauth_disassoc_frame,
		       uint16_t deauth_disassoc_frame_len,
		       uint16_t reason_code)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

/**
 * lim_update_lost_link_info() - update lost link information to SME
 * @mac: global MAC handle
 * @session: PE session
 * @rssi: rssi value from the received frame
 *
 * Return: None
 */
void lim_update_lost_link_info(struct mac_context *mac, struct pe_session *session,
				int32_t rssi);

/**
 * lim_mon_init_session() - create PE session for monitor mode operation
 * @mac_ptr: mac pointer
 * @msg: Pointer to struct sir_create_session type.
 *
 * Return: NONE
 */
void lim_mon_init_session(struct mac_context *mac_ptr,
			  struct sir_create_session *msg);

/**
 * lim_mon_deinit_session() - delete PE session for monitor mode operation
 * @mac_ptr: mac pointer
 * @msg: Pointer to struct sir_delete_session type.
 *
 * Return: NONE
 */
void lim_mon_deinit_session(struct mac_context *mac_ptr,
			    struct sir_delete_session *msg);

#define limGetQosMode(pe_session, pVal) (*(pVal) = (pe_session)->limQosEnabled)
#define limGetWmeMode(pe_session, pVal) (*(pVal) = (pe_session)->limWmeEnabled)
#define limGetWsmMode(pe_session, pVal) (*(pVal) = (pe_session)->limWsmEnabled)
/* ----------------------------------------------------------------------- */
static inline void lim_get_phy_mode(struct mac_context *mac, uint32_t *phyMode,
				    struct pe_session *pe_session)
{
	*phyMode =
		pe_session ? pe_session->gLimPhyMode : mac->lim.gLimPhyMode;
}

/* ----------------------------------------------------------------------- */
static inline void lim_get_rf_band_new(struct mac_context *mac,
				       enum reg_wifi_band *band,
				       struct pe_session *pe_session)
{
	*band = pe_session ? pe_session->limRFBand : REG_BAND_UNKNOWN;
}

/**
 * pe_mc_process_handler() - Message Processor for PE
 * @msg: Pointer to the message structure
 *
 * Verifies the system is in a mode where messages are expected to be
 * processed, and if so, routes the message to the appropriate handler
 * based upon message type.
 *
 * Return: QDF_STATUS_SUCCESS if the message was handled, otherwise an
 *         appropriate QDF_STATUS error code
 */
QDF_STATUS pe_mc_process_handler(struct scheduler_msg *msg);

/** -------------------------------------------------------------
   \fn pe_free_msg
   \brief Called by CDS scheduler (function cds_sched_flush_mc_mqs)
 \      to free a given PE message on the TX and MC thread.
 \      This happens when there are messages pending in the PE
 \      queue when system is being stopped and reset.
   \param   struct mac_context *mac
   \param   struct scheduler_msg       pMsg
   \return none
   -----------------------------------------------------------------*/
void pe_free_msg(struct mac_context *mac, struct scheduler_msg *pMsg);

/**
 * lim_process_abort_scan_ind() - abort the scan which is presently being run
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @vdev_id: vdev_id
 * @scan_id: Scan ID from the scan request
 * @scan_requesor_id: Entity requesting the scan
 *
 * @return: None
 */
void lim_process_abort_scan_ind(struct mac_context *mac, uint8_t vdev_id,
	uint32_t scan_id, uint32_t scan_requestor_id);

void __lim_process_sme_assoc_cnf_new(struct mac_context *, uint32_t, uint32_t *);

/**
 * lim_process_sme_addts_rsp_timeout(): Send addts rsp timeout to SME
 * @mac: Pointer to Global MAC structure
 * @param: Addts rsp timer count
 *
 * This function is used to reset the addts sent flag and
 * send addts rsp timeout to SME
 *
 * Return: None
 */
void lim_process_sme_addts_rsp_timeout(struct mac_context *mac, uint32_t param);
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
void lim_fill_join_rsp_ht_caps(struct pe_session *session,
			       struct join_rsp *rsp);
#else
static inline
void lim_fill_join_rsp_ht_caps(struct pe_session *session,
			       struct join_rsp *rsp)
{}
#endif
QDF_STATUS lim_update_ext_cap_ie(struct mac_context *mac_ctx, uint8_t *ie_data,
				 uint8_t *local_ie_buf, uint16_t *local_ie_len,
				 struct pe_session *session);

/**
 * lim_handle_sap_beacon(): Handle the beacon received from scan module for SAP
 * @pdev: pointer to the pdev object
 * @scan_entry: pointer to the scan cache entry for the beacon
 *
 * Registered as callback to the scan module for handling beacon frames.
 * This API filters the and allows beacons for SAP protection mechanisms
 * if there are active SAP sessions and the received beacon's channel
 * matches the SAP active channel
 *
 * Return: None
 */
void lim_handle_sap_beacon(struct wlan_objmgr_pdev *pdev,
					struct scan_cache_entry *scan_entry);

/**
 * lim_translate_rsn_oui_to_akm_type() - translate RSN OUI to AKM type
 * @auth_suite: auth suite
 *
 * Return: AKM type
 */
enum ani_akm_type lim_translate_rsn_oui_to_akm_type(uint8_t auth_suite[4]);

#ifdef WLAN_SUPPORT_TWT
/**
 * lim_fill_roamed_peer_twt_caps() - Update Peer TWT capabilities
 * @mac_ctx: Pointer to mac context
 * @vdev_id: vdev id
 * @roam_synch: Pointer to roam synch indication
 *
 * Return: None
 */
void lim_fill_roamed_peer_twt_caps(struct mac_context *mac_ctx, uint8_t vdev_id,
				   struct roam_offload_synch_ind *roam_synch);
#else
static inline
void lim_fill_roamed_peer_twt_caps(struct mac_context *mac_ctx, uint8_t vdev_id,
				   struct roam_offload_synch_ind *roam_synch)
{}
#endif

/************************************************************/
#endif /* __LIM_API_H */
