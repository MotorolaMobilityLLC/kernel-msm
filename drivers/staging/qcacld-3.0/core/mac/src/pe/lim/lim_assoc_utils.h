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
 * This file lim_assoc_utils.h contains the utility definitions
 * LIM uses while processing Re/Association messages.
 * Author:        Chandra Modumudi
 * Date:          02/13/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 * 05/26/10       js             WPA handling in (Re)Assoc frames
 *
 */
#ifndef __LIM_ASSOC_UTILS_H
#define __LIM_ASSOC_UTILS_H

#include "sir_api.h"
#include "sir_debug.h"

#include "lim_types.h"

#define SIZE_OF_NOA_DESCRIPTOR 13
#define MAX_NOA_PERIOD_IN_MICROSECS 3000000

uint32_t lim_cmp_ssid(tSirMacSSid *, struct pe_session *);
uint8_t lim_compare_capabilities(struct mac_context *,
				 tSirAssocReq *,
				 tSirMacCapabilityInfo *, struct pe_session *);
uint8_t lim_check_rx_basic_rates(struct mac_context *, tSirMacRateSet, struct pe_session *);
uint8_t lim_check_mcs_set(struct mac_context *mac, uint8_t *supportedMCSSet);

/**
 * lim_cleanup_rx_path() - Called to cleanup STA state at SP & RFP.
 * @mac: Pointer to Global MAC structure
 * @sta: Pointer to the per STA data structure initialized by LIM
 *	 and maintained at DPH
 * @pe_session: pointer to pe session
 * @delete_peer: is peer delete allowed
 *
 * To circumvent RFP's handling of dummy packet when it does not
 * have an incomplete packet for the STA to be deleted, a packet
 * with 'more framgents' bit set will be queued to RFP's WQ before
 * queuing 'dummy packet'.
 * A 'dummy' BD is pushed into RFP's WQ with type=00, subtype=1010
 * (Disassociation frame) and routing flags in BD set to eCPU's
 * Low Priority WQ.
 * RFP cleans up its local context for the STA id mentioned in the
 * BD and then pushes BD to eCPU's low priority WQ.
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_E_FAILURE.
 */
QDF_STATUS lim_cleanup_rx_path(struct mac_context *, tpDphHashNode,
			       struct pe_session *, bool delete_peer);

void lim_reject_association(struct mac_context *, tSirMacAddr, uint8_t,
			    uint8_t, tAniAuthType, uint16_t, uint8_t,
			    enum wlan_status_code, struct pe_session *);

QDF_STATUS lim_populate_peer_rate_set(struct mac_context *mac,
				      struct supported_rates *pRates,
				      uint8_t *pSupportedMCSSet,
				      uint8_t basicOnly,
				      struct pe_session *pe_session,
				      tDot11fIEVHTCaps *pVHTCaps,
				      tDot11fIEhe_cap *he_caps,
				      struct sDphHashNode *sta_ds,
				      struct bss_description *bss_desc);

/**
 * lim_populate_own_rate_set() - comprises the basic and extended rates read
 *                                from CFG
 * @mac_ctx: pointer to global mac structure
 * @rates: pointer to supported rates
 * @supported_mcs_set: pointer to supported mcs rates
 * @basic_only: update only basic rates if set true
 * @session_entry: pe session entry
 * @vht_caps: pointer to vht capability
 * @he_caps: pointer to HE capability
 *
 * This function is called by limProcessAssocRsp() or
 * lim_add_staInIBSS()
 * - It creates a combined rate set of 12 rates max which
 *   comprises the basic and extended rates read from CFG
 * - It sorts the combined rate Set and copy it in the
 *   rate array of the pSTA descriptor
 * - It sets the erpEnabled bit of the STA descriptor
 * ERP bit is set iff the dph PHY mode is 11G and there is at least
 * an A rate in the supported or extended rate sets
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_E_FAILURE.
 */
QDF_STATUS lim_populate_own_rate_set(struct mac_context *mac_ctx,
				     struct supported_rates *rates,
				     uint8_t *supported_mcs_set,
				     uint8_t basic_only,
				     struct pe_session *session_entry,
				     struct sDot11fIEVHTCaps *vht_caps,
				     struct sDot11fIEhe_cap *he_caps);

QDF_STATUS lim_populate_matching_rate_set(struct mac_context *mac_ctx,
					  tpDphHashNode sta_ds,
					  tSirMacRateSet *oper_rate_set,
					  tSirMacRateSet *ext_rate_set,
					  uint8_t *supported_mcs_set,
					  struct pe_session *session_entry,
					  tDot11fIEVHTCaps *vht_caps,
					  tDot11fIEhe_cap *he_caps);

QDF_STATUS lim_add_sta(struct mac_context *, tpDphHashNode, uint8_t, struct pe_session *);
QDF_STATUS lim_del_bss(struct mac_context *, tpDphHashNode, uint16_t, struct pe_session *);
QDF_STATUS lim_del_sta(struct mac_context *, tpDphHashNode, bool, struct pe_session *);
QDF_STATUS lim_add_sta_self(struct mac_context *, uint8_t, struct pe_session *);

/**
 *lim_del_peer_info() - remove all peer information from host driver and fw
 * @mac:    Pointer to Global MAC structure
 * @pe_session: Pointer to PE Session entry
 *
 * @Return: QDF_STATUS
 */
QDF_STATUS lim_del_peer_info(struct mac_context *mac,
			     struct pe_session *pe_session);

/**
 * lim_del_sta_all() - Cleanup all peers associated with VDEV
 * @mac:    Pointer to Global MAC structure
 * @pe_session: Pointer to PE Session entry
 *
 * @Return: QDF Status of operation.
 */
QDF_STATUS lim_del_sta_all(struct mac_context *mac,
			   struct pe_session *pe_session);

#ifdef WLAN_FEATURE_HOST_ROAM
void lim_restore_pre_reassoc_state(struct mac_context *,
				   tSirResultCodes, uint16_t, struct pe_session *);
void lim_post_reassoc_failure(struct mac_context *,
			      tSirResultCodes, uint16_t, struct pe_session *);
bool lim_is_reassoc_in_progress(struct mac_context *, struct pe_session *);

void lim_handle_add_bss_in_re_assoc_context(struct mac_context *mac,
		tpDphHashNode sta, struct pe_session *pe_session);
void lim_handle_del_bss_in_re_assoc_context(struct mac_context *mac,
		   tpDphHashNode sta, struct pe_session *pe_session);
void lim_send_retry_reassoc_req_frame(struct mac_context *mac,
	      tLimMlmReassocReq *pMlmReassocReq, struct pe_session *pe_session);
QDF_STATUS lim_add_ft_sta_self(struct mac_context *mac, uint16_t assocId,
				  struct pe_session *pe_session);
#else
static inline void lim_restore_pre_reassoc_state(struct mac_context *mac_ctx,
			tSirResultCodes res_code, uint16_t prot_status,
			struct pe_session *pe_session)
{}
static inline void lim_post_reassoc_failure(struct mac_context *mac_ctx,
			      tSirResultCodes res_code, uint16_t prot_status,
			      struct pe_session *pe_session)
{}
static inline void lim_handle_add_bss_in_re_assoc_context(struct mac_context *mac,
		tpDphHashNode sta, struct pe_session *pe_session)
{}
static inline void lim_handle_del_bss_in_re_assoc_context(struct mac_context *mac,
		   tpDphHashNode sta, struct pe_session *pe_session)
{}
static inline void lim_send_retry_reassoc_req_frame(struct mac_context *mac,
	      tLimMlmReassocReq *pMlmReassocReq, struct pe_session *pe_session)
{}
static inline bool lim_is_reassoc_in_progress(struct mac_context *mac_ctx,
		struct pe_session *pe_session)
{
	return false;
}
static inline QDF_STATUS lim_add_ft_sta_self(struct mac_context *mac,
		uint16_t assocId, struct pe_session *pe_session)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static inline bool lim_is_roam_synch_in_progress(struct wlan_objmgr_psoc *psoc,
						 struct pe_session *pe_session)
{
	return MLME_IS_ROAM_SYNCH_IN_PROGRESS(psoc, pe_session->vdev_id);
}
#else
static inline bool lim_is_roam_synch_in_progress(struct wlan_objmgr_psoc *psoc,
						 struct pe_session *pe_session)
{
	return false;
}
#endif

void
lim_send_del_sta_cnf(struct mac_context *mac, struct qdf_mac_addr sta_dsaddr,
		     uint16_t staDsAssocId,
		     struct lim_sta_context mlmStaContext,
		     tSirResultCodes status_code,
		     struct pe_session *pe_session);

void lim_handle_cnf_wait_timeout(struct mac_context *mac, uint16_t staId);
void lim_delete_dph_hash_entry(struct mac_context *, tSirMacAddr, uint16_t, struct pe_session *);
void lim_check_and_announce_join_success(struct mac_context *,
					 tSirProbeRespBeacon *,
					 tpSirMacMgmtHdr, struct pe_session *);
void lim_update_re_assoc_globals(struct mac_context *mac,
				 tpSirAssocRsp pAssocRsp,
				 struct pe_session *pe_session);

void lim_update_assoc_sta_datas(struct mac_context *mac,
				tpDphHashNode sta, tpSirAssocRsp pAssocRsp,
				struct pe_session *pe_session,
				tSchBeaconStruct *beacon);

/**
 * lim_sta_add_bss_update_ht_parameter() - function to update ht related
 *                                         parameters when add bss request
 * @bss_chan_freq: operating frequency of bss
 * @ht_cap: ht capability extract from beacon/assoc response
 * @ht_inf: ht information extract from beacon/assoc response
 * @chan_width_support: local wide bandwith support capability
 * @add_bss: add bss request struct to be updated
 *
 * Return: none
 */
void lim_sta_add_bss_update_ht_parameter(uint32_t bss_chan_freq,
					 tDot11fIEHTCaps* ht_cap,
					 tDot11fIEHTInfo* ht_inf,
					 bool chan_width_support,
					 struct bss_params *add_bss);

/**
 * lim_sta_send_add_bss() - add bss and send peer assoc after receive assoc
 * rsp in sta mode
 *.@mac: pointer to Global MAC structure
 * @pAssocRsp: contains the structured assoc/reassoc Response got from AP
 * @beaconstruct: the ProbeRsp/Beacon structured details
 * @bssDescription: bss description passed to PE from the SME
 * @updateEntry: bool flag of whether update bss and sta
 * @pe_session: pointer to pe session
 *
 * Return: none
 */
QDF_STATUS lim_sta_send_add_bss(struct mac_context *mac,
				tpSirAssocRsp pAssocRsp,
				tpSchBeaconStruct pBeaconStruct,
				struct bss_description *bssDescription,
				uint8_t updateEntry,
				struct pe_session *pe_session);

/**
 * lim_sta_send_add_bss_pre_assoc() - add bss after channel switch and before
 * associate req in sta mode
 *.@mac: pointer to Global MAC structure
 * @pe_session: pointer to pe session
 *
 * Return: none
 */
QDF_STATUS lim_sta_send_add_bss_pre_assoc(struct mac_context *mac,
					  struct pe_session *pe_session);

void lim_prepare_and_send_del_all_sta_cnf(struct mac_context *mac,
					  tSirResultCodes status_code,
					  struct pe_session *pe_session);

void lim_prepare_and_send_del_sta_cnf(struct mac_context *mac,
				      tpDphHashNode sta,
				      tSirResultCodes status_code,
				      struct pe_session *pe_session);

QDF_STATUS lim_extract_ap_capabilities(struct mac_context *mac, uint8_t *pIE,
					  uint16_t ieLen,
					  tpSirProbeRespBeacon beaconStruct);
void lim_init_pre_auth_timer_table(struct mac_context *mac,
				   tpLimPreAuthTable pPreAuthTimerTable);
tpLimPreAuthNode lim_acquire_free_pre_auth_node(struct mac_context *mac,
						tpLimPreAuthTable
						pPreAuthTimerTable);
tpLimPreAuthNode lim_get_pre_auth_node_from_index(struct mac_context *mac,
						  tpLimPreAuthTable pAuthTable,
						  uint32_t authNodeIdx);

/* Util API to check if the channels supported by STA is within range */
QDF_STATUS lim_is_dot11h_supported_channels_valid(struct mac_context *mac,
						     tSirAssocReq *assoc);

/* Util API to check if the txpower supported by STA is within range */
QDF_STATUS lim_is_dot11h_power_capabilities_in_range(struct mac_context *mac,
							tSirAssocReq *assoc,
							struct pe_session *);
/**
 * lim_fill_rx_highest_supported_rate() - Fill highest rx rate
 * @mac: Global MAC context
 * @rxHighestRate: location to store the highest rate
 * @pSupportedMCSSet: location of the 'supported MCS set' field in HT
 *                    capability element
 *
 * Fills in the Rx Highest Supported Data Rate field from
 * the 'supported MCS set' field in HT capability element.
 *
 * Return: void
 */
void lim_fill_rx_highest_supported_rate(struct mac_context *mac,
					uint16_t *rxHighestRate,
					uint8_t *pSupportedMCSSet);
#ifdef WLAN_FEATURE_11W
void lim_send_sme_unprotected_mgmt_frame_ind(struct mac_context *mac, uint8_t frameType,
					     uint8_t *frame, uint32_t frameLen,
					     uint16_t sessionId,
					     struct pe_session *pe_session);
#endif

/**
 * lim_send_sme_tsm_ie_ind() - Send TSM IE information to SME
 * @mac: Global MAC context
 * @pe_session: PE session context
 * @tid: traffic id
 * @state: tsm state (enabled/disabled)
 * @measurement_interval: measurement interval
 *
 * Return: void
 */
#ifdef FEATURE_WLAN_ESE
void lim_send_sme_tsm_ie_ind(struct mac_context *mac,
			     struct pe_session *pe_session,
			     uint8_t tid, uint8_t state,
			     uint16_t measurement_interval);
#else
static inline
void lim_send_sme_tsm_ie_ind(struct mac_context *mac,
			     struct pe_session *pe_session,
			     uint8_t tid, uint8_t state,
			     uint16_t measurement_interval)
{}
#endif /* FEATURE_WLAN_ESE */

/**
 * lim_populate_vht_mcs_set - function to populate vht mcs rate set
 * @mac_ctx: pointer to global mac structure
 * @rates: pointer to supported rate set
 * @peer_vht_caps: pointer to peer vht capabilities
 * @session_entry: pe session entry
 * @nss: number of spatial streams
 * @sta_ds: pointer to peer sta data structure
 *
 * Populates vht mcs rate set based on peer and self capabilities
 *
 * Return: QDF_STATUS_SUCCESS on success else QDF_STATUS_E_FAILURE
 */
QDF_STATUS lim_populate_vht_mcs_set(struct mac_context *mac_ctx,
				    struct supported_rates *rates,
				    tDot11fIEVHTCaps *peer_vht_caps,
				    struct pe_session *session_entry,
				    uint8_t nss,
				    struct sDphHashNode *sta_ds);

/**
 * lim_extract_ies_from_deauth_disassoc() - Extract IEs from deauth/disassoc
 *
 * @session: PE session entry
 * @deauth_disassoc_frame: A pointer to the deauth/disconnect frame buffer
 *			   received from WMA.
 * @deauth_disassoc_frame_leni: Length of the deauth/disconnect frame.
 *
 * This function receives deauth/disassoc frame from header. It extracts
 * the IEs(tagged params) from the frame and caches in vdev object.
 *
 * Return: None
 */
void
lim_extract_ies_from_deauth_disassoc(struct pe_session *session,
				     uint8_t *deauth_disassoc_frame,
				     uint16_t deauth_disassoc_frame_len);

/**
 * lim_update_vhtcaps_assoc_resp : Update VHT caps in assoc response.
 * @mac_ctx Pointer to Global MAC structure
 * @pAddBssParams: parameters required for add bss params.
 * @vht_caps: VHT capabilities.
 * @pe_session : session entry.
 *
 * Return : void
 */
void lim_update_vhtcaps_assoc_resp(struct mac_context *mac_ctx,
				   struct bss_params *pAddBssParams,
				   tDot11fIEVHTCaps *vht_caps,
				   struct pe_session *pe_session);
#endif /* __LIM_ASSOC_UTILS_H */
